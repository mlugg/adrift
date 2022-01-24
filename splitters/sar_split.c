#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

/*
 * 16 byte "SAR_TIMER_START\0"
 * int total
 * float interval_per_tick
 * int action
 * 14 byte end "SAR_TIMER_END\0"
 */

struct addr_range {
	struct addr_range *next;
	void *start;
	size_t len;
};

/* Read and parse /proc/<pid>/maps and return a linked list of all the
 * address ranges in the program. If heap_only is true, only return
 * address ranges labelled '[heap]'. */
static struct addr_range *get_ranges(pid_t pid, bool heap_only) {
	if (pid <= 0) return NULL;

	char pathbuf[32]; // sufficient for pid INT_MAX
	snprintf(pathbuf, sizeof pathbuf, "/proc/%d/maps", pid);

	FILE *f = fopen(pathbuf, "r");
	if (!f) return NULL;

	struct addr_range *lst = NULL;

	char *line = NULL;
	size_t len = 0;
	while (getline(&line, &len, f) > 0) {
		uint64_t start, end;
		int name_start;
		if (sscanf(line, "%lx-%lx %*7s %*x %*u:%*u %*u %n", &start, &end, &name_start) != 2) continue;

		if (heap_only) {
			if (strcmp("[heap]\n", line + name_start)) continue;
		}

		struct addr_range *tmp = malloc(sizeof *tmp);
		tmp->next = lst;
		tmp->start = (void *)start;
		tmp->len = end - start;
		lst = tmp;
	}

	free(line);
	fclose(f);

	return lst;
}

/* Try to find the SAR timer structure in the memory of the given
 * process, and return the address in its memory where the timer was
 * found, or NULL if it could not be found. */
static void *scan_for_timer(pid_t pid) {
	for (struct addr_range *r = get_ranges(pid, true); r; r = r->next) {
		fprintf(stderr, "[LOG] --- Scan range %lx-%lx ---\n", (uintptr_t)r->start, (uintptr_t)r->start + r->len);

		void *buf = malloc(r->len);

		if (!buf) {
			fprintf(stderr, "[WARN] Failed to allocate buffer of size %lx. Skipping address range\n", r->len);
			continue;
		}

		struct iovec local = {buf, r->len}, remote = {r->start, r->len};

		size_t len_read = syscall(SYS_process_vm_readv, pid, &local, 1, &remote, 1, 0);

		if (len_read == -1) {
			fprintf(stderr, "[WARN] Failed to read address range with errno %d. Skipping\n", errno);
			free(buf);
			continue;
		}

		if (len_read != r->len) {
			fprintf(stderr, "[WARN] Failed to read full address range; range had size %lx, but only read %lx. Skipping\n", r->len, len_read);
			free(buf);
			continue;
		}

		for (size_t i = 0; i <= r->len - 42; ++i) {
			if (strcmp("SAR_TIMER_START", buf + i) != 0) continue;
			if (strcmp("SAR_TIMER_END", buf + i + 28) != 0) continue;
			fputs("[LOG] Found SAR timer location!\n", stderr);
			free(buf);
			return (char *)r->start + i;
		}

		free(buf);
	}

	return NULL;
}

enum timer_action {
	NOTHING,
	START,
	RESTART,
	SPLIT,
	END,
	RESET,
	PAUSE,
	RESUME,
};

struct timer_info {
	int total;
	float ipt;
	enum timer_action action;
};

/* Poll the SAR timer that exists at the given address for the given
 * process, and put the retrieved information in *info. Returns 0 on
 * success, any other value on failure. */
static int poll_timer(pid_t pid, void *addr, struct timer_info *info) {
	struct iovec local = {info, sizeof *info}, remote = {(char *)addr + 16, sizeof *info};
	size_t len_read = syscall(SYS_process_vm_readv, pid, &local, 1, &remote, 1, 0);

	if (len_read != sizeof *info) {
		return 1;
	}

	return 0;
}

static pid_t find_process(void) {
	DIR *d = opendir("/proc");
	if (!d) return -1;

	fputs("[LOG] Enumerating processes...\n", stderr);

	struct dirent *de;
	while ((de = readdir(d))) {
		pid_t pid = atoi(de->d_name);
		if (pid > 0) {
			char pathbuf[32]; // sufficient for pid INT_MAX
			snprintf(pathbuf, sizeof pathbuf, "/proc/%d/cmdline", pid);

			FILE *f = fopen(pathbuf, "r");
			if (!f) continue;

			char *cmdline = NULL;
			size_t cmdlen = 0;
			if (getline(&cmdline, &cmdlen, f) > 0) {
				char *base = basename(cmdline);
				if (!strcmp(base, "portal2_linux")) {
					fputs("[LOG] Found portal2_linux process!\n", stderr);
					free(cmdline);
					fclose(f);
					closedir(d);
					return pid;
				}
			}

			free(cmdline);
			fclose(f);
		}
	}

	closedir(d);
	return -1;
}

struct state {
	pid_t pid;
	void *addr;
	enum timer_action last_action;
};

struct state *splitter_init(int fd, bool initial_connect) {
	pid_t pid = find_process();

	if (pid < 0) {
		fputs("[ERR] Could not find portal2_linux process\n", stderr);
		return NULL;
	}

	fprintf(stderr, "[LOG] Using process %d\n", pid);

	void *addr = scan_for_timer(pid);

	if (!addr) {
		fputs("[ERR] Could not find timer in memory! Is SAR loaded?\n", stderr);
		return NULL;
	}

	struct state *s = malloc(sizeof *s);
	s->pid = pid;
	s->addr = addr;
	s->last_action = NOTHING;

	fputs("[LOG] Initialization completed!\n", stderr);

	// Reset to the current time

	struct timer_info info;

	if (poll_timer(s->pid, s->addr, &info)) {
		fputs("[ERR] Failed to poll timer!\n", stderr);
		return NULL;
	}

	if (initial_connect) {
		uint64_t usec = (double)info.ipt * (double)info.total * 1e6;
		dprintf(fd, "%lu RESET\n", usec);
	}

	return s;
}

int splitter_update(int fd, struct state *st) {
	struct timer_info info;

	if (poll_timer(st->pid, st->addr, &info)) {
		fputs("[ERR] Failed to poll timer!\n", stderr);
		return 1;
	}

	uint64_t usec = (double)info.ipt * (double)info.total * 1e6;

	enum timer_action new_act = info.action != st->last_action ? info.action : NOTHING;
	st->last_action = info.action;

	switch (new_act) {
		case START:
			dprintf(fd, "0 BEGIN\n");
			dprintf(fd, "%lu\n", usec);
			break;
		case SPLIT:
		case END:
			dprintf(fd, "%lu SPLIT\n", usec);
			break;
		case RESET:
			dprintf(fd, "%lu RESET\n", usec);
			break;
		case RESTART:
			dprintf(fd, "%lu RESET\n", usec);
			dprintf(fd, "0 BEGIN\n");
			dprintf(fd, "%lu\n", usec);
			break;
		default:
			dprintf(fd, "%lu\n", usec);
			break;
	}

	return 0;
}

int fd;
char *fifo_path;

void cleanup(int sig) {
	if (fifo_path) {
		close(fd);
		unlink(fifo_path);
	}
	exit(0);
}

int usage(char *argv0) {
	fprintf(stderr, "Usage: %s [fifo path]\n", argv0);
	return 1;
}

int main(int argc, char **argv) {
	if (argc > 2) {
		return usage(argv[0]);
	}

	if (argc == 2 && !strcmp(argv[1], "-h")) {
		return usage(argv[0]);
	}

	fd = STDOUT_FILENO;
	fifo_path = argc == 2 ? argv[1] : NULL;

	if (fifo_path) {
		if (mkfifo(fifo_path, 0644) == -1) {
			fprintf(stderr, "[ERR] Failed to create FIFO '%s': error %d\n", fifo_path, errno);
			return 1;
		}

		fd = open(fifo_path, O_WRONLY);
		if (fd == -1) {
			fprintf(stderr, "[ERR] Failed to open '%s': error %d\n", fifo_path, errno);
			unlink(fifo_path);
			return 1;
		}
	}

	struct sigaction act = {
		.sa_handler = cleanup,
	};
	sigaction(SIGINT, &act, NULL);

	struct state *st;
	bool last_failed = false;
	bool initial_connect = true;

	while (true) {
		do {
			st = splitter_init(fd, initial_connect);
			if (!st) sleep(2);
		} while (!st);

		initial_connect = false;

		if (st == NULL) {
			if (fifo_path) unlink(fifo_path);
			return 1;
		}

		while (true) {
			if (splitter_update(fd, st)) {
				if (last_failed) {
					if (fifo_path) {
						close(fd);
						unlink(fifo_path);
					}
					return 1;
				} else {
					last_failed = true;
					break; // re-init
				}
			}

			last_failed = false;

			struct timespec sleep_tv = {
				.tv_sec = 0,
				.tv_nsec = 15000000, // 15ms
			};

			nanosleep(&sleep_tv, NULL);
		}
	}

	return 0;
}
