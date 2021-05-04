#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "draw.h"
#include "common.h"
#include "io.h"
#include "timer.h"

static atomic_bool _g_should_exit;

static vtk_window _g_win;

static void int_handler(int signal) {
	vtk_window_close(_g_win);
}

static void close_handler(vtk_event ev, void *u) {
	vtk_window_close(_g_win);
}

static int input_main(void *u) {
	struct state *s = u;

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		fputs("Failed to create pipe\n", stderr);
		exit(1);
	}

	pid_t pid = 0;

	pid = fork();
	if (pid == 0) {
		// Child
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		execlp("./splitter", "./splitter", NULL);
		fputs("Failed to exec splitter\n", stderr);
		exit(1);
	} else if (pid == -1) {
		fputs("Failed to fork\n", stderr);
		exit(1);
	}

	// Parent

	close(pipefd[1]);

	int fl = fcntl(pipefd[0], F_GETFL);
	if (fl == -1) fl = 0;
	fcntl(pipefd[0], F_SETFL, fl | O_NONBLOCK);

	FILE *f = fdopen(pipefd[0], "r");

	struct pollfd fds[] = {
		{ pipefd[0], POLLIN },
	};

	while (!_g_should_exit) {
		// Use poll rather than getline directly; that way, we can routinely
		// check if we should exit
		if (poll(fds, sizeof fds / sizeof fds[0], 500) > 0) {
			char *line = NULL;
			size_t n = 0;
			if (getline(&line, &n, f) == -1) {
				free(line);
				break;
			}
			line[strlen(line) - 1] = 0; // Remove newline
			timer_parse(s, line);
			free(line);
			vtk_window_trigger_update(s->win);
		}
	}

	close(pipefd[0]);

	if (pid) {
		kill(pid, SIGINT);
	}

	return 0;
}

void update_handler(vtk_event ev, void *u) {
	struct state *s = u;
	vtk_window_redraw(s->win);
}

int main(int argc, char **argv) {
	if (argc > 2 || (argc == 2 && !strcmp(argv[1], "-h"))) {
		fprintf(stderr, "Usage: %s [path]\n", argv[0]);
		return argc > 2;
	}

	if (argc == 2) {
		if (chdir(argv[1])) {
			fprintf(stderr, "Failed to chdir to %s\n", argv[1]);
			return 1;
		}
	}

	enum widget_type widgets[] = {
		WIDGET_GAME_NAME,
		WIDGET_CATEGORY_NAME,
		WIDGET_SUM_OF_BEST,
		WIDGET_BEST_POSSIBLE_TIME,
		WIDGET_TIMER,
		WIDGET_SPLIT_TIMER,
		WIDGET_SPLITS,
	};

	struct split *splits;
	ssize_t nsplits = read_splits_file("splits", &splits);

	if (nsplits == -1) {
		return 1;
	}

	if (!read_times(splits, nsplits, "pb", offsetof(struct times, pb))) {
		fputs("Warning: could not read PB\n", stderr);
	}

	if (!read_times(splits, nsplits, "golds", offsetof(struct times, best))) {
		fputs("Warning: could not read golds\n", stderr);
	}

	struct cfgdict *cfg = cfgdict_new();
	if (!read_config("config", cfg)) {
		fputs("Warning: could not read config\n", stderr);
	}

	int err;

	vtk vtk;
	err = vtk_new(&vtk);
	if (err) {
		fprintf(stderr, "Error initializing vtk: %s\n", vtk_strerr(err));
		return 1;
	}	

	vtk_window win;
	err = vtk_window_new(&win, vtk, "Adrift", 0, 0, 315, 650);
	if (err) {
		fprintf(stderr, "Error initializing vtk window: %s\n", vtk_strerr(err));
		vtk_destroy(vtk);
		return 1;
	}

	cairo_t *cr = vtk_window_get_cairo(win);

	struct state s = {
		.win = win,
		.cr = cr,

		.game_name = config_get_str(cfg, "game", "Portal 2"),
		.category_name = config_get_str(cfg, "category", "Inbounds NoSLA"),

		.nwidgets = sizeof widgets / sizeof widgets[0],
		.widgets = widgets,

		.nsplits = nsplits,
		.splits = splits,

		.active_split = -1,

		.timer = 0,
		.split_time = 0,

		.cfg = cfg,
	};

	_g_win = win;

	thrd_t inp_thrd;
	if (thrd_create(&inp_thrd, &input_main, &s) != thrd_success) {
		fputs("Error creating thread\n", stderr);
		vtk_window_destroy(win);
		vtk_destroy(vtk);
		return 1;
	}

	vtk_window_set_event_handler(win, VTK_EV_DRAW, draw_handler, &s);
	vtk_window_set_event_handler(win, VTK_EV_CLOSE, close_handler, &s);
	vtk_window_set_event_handler(win, VTK_EV_UPDATE, update_handler, &s);

	signal(SIGINT, &int_handler);

	vtk_window_mainloop(win);

	vtk_window_destroy(win);
	vtk_destroy(vtk);

	_g_should_exit = true;

	thrd_join(inp_thrd, NULL);

	save_times(splits, nsplits, "golds", offsetof(struct times, best));

	cfgdict_free(cfg);

	return 0;
}
