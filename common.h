#ifndef COMMON_H
#define COMMON_H

#include <vtk.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <threads.h>
#include <time.h>
#include "config.h"

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
	char *name;
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

struct state {
	vtk_window win;
	cairo_t *cr;

	char *game_name;
	char *category_name;

	size_t nwidgets;
	enum widget_type *widgets;

	size_t nsplits;
	struct split *splits;

	int active_split;

	uint64_t timer;
	uint64_t split_time;

	time_t run_started;

	struct cfgdict *cfg;
};

struct split *get_split_by_id(struct state *s, unsigned id);
unsigned get_split_id(struct split *sp);
struct split *get_final_split(struct state *s);
struct times get_split_times(struct split *sp);
uint64_t get_comparison(struct state *s, struct times t);
void free_splits(struct split *splits, size_t nsplits);

#endif
