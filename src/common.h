#ifndef COMMON_H
#define COMMON_H

#include <vtk.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <threads.h>
#include <time.h>
#include "config.h"
#include "vstring.h"

enum widget_type {
	WIDGET_GAME_NAME,
	WIDGET_CATEGORY_NAME,
	WIDGET_TIMER,
	WIDGET_SPLIT_TIMER,
	WIDGET_SPLITS,
	WIDGET_SUM_OF_BEST,
	WIDGET_BEST_POSSIBLE_TIME,
};

struct times {
	// UINT64_MAX if not present
	
	// Cumulative
	uint64_t cur;
	uint64_t pb;

	// Per-split
	uint64_t best;
	bool golded_this_run;
};

struct split {
	vstring name;
	bool is_group;

	union {
		struct {
			size_t nsplits;
			struct split *splits;
			bool expanded;
		} group;

		struct {
			struct times times;
			int id;
		} split;
	};
};

struct client {
	int fd;

	vstring game_name;
	vstring game_name_hr;

	vstring cat_name;
	vstring cat_name_hr;

	struct split *splits;
	size_t nsplits;

	int active_split;
	uint64_t timer;
	uint64_t split_time;
	time_t run_started;

	void *recovery;
	size_t recovery_len;

	bool graceful_term;
};

struct state {
	vtk_window win;
	cairo_t *cr;

	size_t nwidgets;
	enum widget_type *widgets;

	struct cfgdict *cfg;

	struct client *clients;
	size_t nclients;

	struct client *cur_client;
};

struct split *get_split_by_id(struct client *cl, unsigned id);
struct split *get_final_split(struct client *cl);
struct times get_split_times(struct split *sp);
uint64_t get_comparison(struct state *s, struct times t);
void free_splits(struct split *splits, size_t nsplits);

#endif
