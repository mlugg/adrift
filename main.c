#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"
#include "common.h"
#include "io.h"
#include "timer.h"

static bool stop_input = false;

void close_handler(vtk_event ev, void *u) {
	struct state *s = u;
	save_times(s->splits, s->nsplits, "golds", offsetof(struct times, best));
	vtk_window_close(s->win);
}

int input_main(void *u) {
	struct state *s = u;

	while (!stop_input) {
		char *line = NULL;
		size_t n = 0;
		if (getline(&line, &n, stdin) == -1) {
			free(line);
			break;
		}
		line[strlen(line) - 1] = 0; // Remove newline
		timer_parse(s, line);
		free(line);
		vtk_window_trigger_update(s->win);
	}

	return 0;
}

void update_handler(vtk_event ev, void *u) {
	struct state *s = u;
	vtk_window_redraw(s->win);
}

int main(int argc, char **argv) {
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

	int err;

	vtk vtk;
	err = vtk_new(&vtk);
	if (err) {
		fprintf(stderr, "Error initializing vtk: %s\n", vtk_strerr(err));
		return 1;
	}	

	vtk_window win;
	err = vtk_window_new(&win, vtk, "Adrift", 0, 0, 300, 650);
	if (err) {
		fprintf(stderr, "Error initializing vtk window: %s\n", vtk_strerr(err));
		vtk_destroy(vtk);
		return 1;
	}

	cairo_t *cr = vtk_window_get_cairo(win);

	struct state s = {
		.win = win,
		.cr = cr,

		.game_name = "Portal 2",
		.category_name = "Inbounds NoSLA",

		.nwidgets = sizeof widgets / sizeof widgets[0],
		.widgets = widgets,

		.nsplits = nsplits,
		.splits = splits,

		.active_split = -1,

		.timer = 0,
		.split_time = 0,
	};

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

	vtk_window_mainloop(win);

	vtk_window_destroy(win);
	vtk_destroy(vtk);

	stop_input = true; // FIXME: unsafe

	thrd_join(inp_thrd, NULL);

	return 0;
}
