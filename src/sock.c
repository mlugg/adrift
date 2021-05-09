#include "proto.h"
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define POLL_TIMEOUT 300
#define COMMAND_MAX 20000000  // If we get 20MiB of data at once, something is probably wrong
#define DEFAULT_RUNTIME "/tmp/runtime-root"

static struct c2s_cmd *_get_c2s_by_name(const char *cmd) {
	for (size_t i = 0; i < sizeof g_c2s_cmds / sizeof g_c2s_cmds[0]; ++i) {
		if (!strcmp(g_c2s_cmds[i].name, cmd)) return &g_c2s_cmds[i];
	}
	return NULL;
}

static void _client_command(struct client *cl, const char *cmd) {
	size_t nargs = 0;
	size_t alloc = 8;
	char **args = malloc(alloc * sizeof args[0]);

	struct c2s_cmd *decoded = NULL;

	cl->graceful_term = false;

	while (cmd[0] && (!decoded || nargs - 1 < decoded->normal_args)) {
		size_t i = 0;
		while (cmd[i] && cmd[i] != ' ') ++i;

		if (nargs == alloc) {
			alloc *= 2;
			args = realloc(args, alloc * sizeof args[0]);
		}

		args[nargs++] = strndup(cmd, i);

		cmd += i;
		if (cmd[0] == ' ') ++cmd;

		if (!decoded) {
			decoded = _get_c2s_by_name(args[0]);
			if (!decoded) {
				break;
			}
		}
	}

	if (!decoded) {
		for (size_t i = 0; i < nargs; ++i) {
			free(args[i]);
		}
		free(args);
		return;
		// TODO: disconnect?
	}

	if (decoded->normal_args != SIZE_MAX && nargs != decoded->normal_args + 1) {
		for (size_t i = 0; i < nargs; ++i) {
			free(args[i]);
		}
		free(args);
		return;
		// TODO: disconnect?
	}

	if (decoded->has_long_arg) {
		if (nargs == alloc) {
			alloc++;
			args = realloc(args, alloc * sizeof args[0]);
		}

		args[nargs++] = strdup(cmd);
	}

	decoded->callback(cl, nargs, (const char *const *)args);

	for (size_t i = 0; i < nargs; ++i) {
		free(args[i]);
	}
	free(args);
}

static void _client_data(struct client *cl) {
	size_t len = 64;
	char *buf = malloc(len);
	ssize_t rlen = 0;

	do {
		if (rlen == len) {
			len *= 2;
			buf = realloc(buf, len);
		}

		struct pollfd fds[] = {
			{cl->fd, POLLIN},
		};

		if (len > COMMAND_MAX || poll(fds, 1, POLL_TIMEOUT) != 1) {
			// Something went wrong
			// TODO: error and disconnect?
			return;
		}

		rlen += read(cl->fd, buf + rlen, len - rlen);
	} while (buf[rlen - 1] != '\n');

	// buf now contains one or more packets

	char *cmd = buf;
	size_t remaining = len;
	for (size_t i = 0; i < remaining; ++i) {
		if (cmd[i] == '\n') {
			cmd[i] = 0;
			_client_command(cl, cmd);
			cmd += i + 1;
			i = 0;
			remaining -= i + 1;
		}
	}

	free(buf);
}

static void _client_remove(struct state *s, struct client *cl) {
	if (s->cur_client == cl) {
		if (!cl->graceful_term) {
			// TODO: recovery?
		}
		s->cur_client = NULL;
	}
	vs_free(cl->game_name);
	vs_free(cl->game_name_hr);
	vs_free(cl->cat_name);
	vs_free(cl->cat_name_hr);
	free_splits(cl->splits, cl->nsplits);
}

static void _client_new(struct state *s, struct client *cl) {
	if (!s->cur_client) {
		s->cur_client = cl;
	}
}

int sock_main(void *_s) {
	struct state *s = _s;

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sockfd == -1) {
		return 0;
	}

	char *sock_path = getenv("ADRIFT_SOCK_PATH");
	if (!sock_path || sock_path[0] == 0) {
		char *runtime_dir = getenv("XDG_RUNTIME_DIR");
		if (!runtime_dir || runtime_dir[0] == 0) {
			runtime_dir = DEFAULT_RUNTIME;
		}
		sock_path = malloc(strlen(runtime_dir) + 16);
		strcpy(sock_path, runtime_dir);
		strcat(sock_path, "/adrift.sock");
	}

	struct sockaddr_un bind_addr;
	bind_addr.sun_family = AF_UNIX;
	strncpy(bind_addr.sun_path, sock_path, sizeof bind_addr.sun_path - 1);
	bind_addr.sun_path[sizeof bind_addr.sun_path - 1] = 0;

	if (bind(sockfd, (struct sockaddr *)&bind_addr, sizeof bind_addr) == -1) {
		// TODO: error
		return 0;
	}

	if (!getenv("ADRIFT_SOCK_PATH")) {
		// sock_path was malloc'd
		free(sock_path);
	}

	size_t clients_alloc = 8;
	s->nclients = 0;
	s->clients = malloc(clients_alloc * sizeof s->clients[0]);
	struct pollfd *fds = malloc((clients_alloc + 1) * sizeof fds[0]);

	fds[0] = (struct pollfd){
		.fd = sockfd,
		.events = POLLIN,
	};

	if (listen(sockfd, 4) == -1) {
		// TODO: error
		return 0;
	}

	while (1) {
		int to_handle = poll(fds, s->nclients + 1, -1);
		if (to_handle == -1) {
			// Error
			break;
		}

		if (fds[0].revents) {
			--to_handle;
			if (fds[0].revents & POLLIN) {
				int fd = accept(sockfd, NULL, NULL);

				if (fd != -1) {
					if (s->nclients == clients_alloc) {
						clients_alloc *= 2;
						s->clients = realloc(s->clients, clients_alloc * sizeof s->clients[0]);
						fds = malloc((clients_alloc + 1) * sizeof fds[0]);
					}

					s->clients[s->nclients] = (struct client){
						.fd = fd,
						.active_split = -1,
					};

					fds[s->nclients + 1] = (struct pollfd){
						.fd = fd,
						.events = POLLIN,
					};

					_client_new(s, &s->clients[s->nclients]);

					++s->nclients;
				}
			} else {
				// Error
				break;
			}
		}

		for (size_t i = 0; i < s->nclients && to_handle > 0; ++i) {
			if (fds[i + 1].revents) {
				--to_handle;

				if (fds[i + 1].revents & POLLERR) {
					--s->nclients;
					_client_remove(s, &s->clients[i]);
					memmove(s->clients + i, s->clients + i + 1, s->nclients - i);
					memmove(fds + i, fds + i + 1, s->nclients - i);
					--i;
				} else {
					_client_data(&s->clients[i]);
				}
			}
		}
	}

	void *clients = s->clients;

	s->nclients = 0;
	s->clients = NULL;

	free(clients);
	free(fds);

	return 0;
}
