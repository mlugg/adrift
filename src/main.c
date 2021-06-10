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
#include "sock.h"

#ifdef _WIN32
#define DEFAULT_RUNTIME "C:/Windows/TMP/runtime"
#else
#define DEFAULT_RUNTIME "/tmp/runtime-root"
#endif

static atomic_bool _g_should_exit;

static vtk_window _g_win;

static void int_handler(int signal) {
	vtk_window_close(_g_win);
}

static void close_handler(vtk_event ev, void *u) {
	vtk_window_close(_g_win);
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

		.nwidgets = sizeof widgets / sizeof widgets[0],
		.widgets = widgets,

		.cfg = cfg,

		.nclients = 0,
		.clients = NULL,

		.cur_client = NULL,
	};

	_g_win = win;

	thrd_t inp_thrd;
	if (thrd_create(&inp_thrd, &sock_main, &s) != thrd_success) {
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

	cfgdict_free(cfg);

	return 0;
}
