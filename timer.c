#include "timer.h"
#include "io.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define RUNS_DIR "runs"
// Microseconds you have to beat gold by for it to actually register - prevents rounding issues
#define GOLD_EPSILON 10

static bool _update_expanded(int active_split, struct split *splits, size_t nsplits) {
	bool expand = false;

	for (size_t i = 0; i < nsplits; ++i) {
		if (splits[i].is_group) {
			bool e = _update_expanded(active_split, splits[i].group.splits, splits[i].group.nsplits);
			splits[i].group.expanded = e;
			expand = expand || e;
		} else {
			expand = expand || active_split == splits[i].split.id;
		}
	}

	return expand;
}

static void _commit_pb(struct split *splits, size_t nsplits) {
	for (size_t i = 0; i < nsplits; ++i) {
		if (splits[i].is_group) {
			_commit_pb(splits[i].group.splits, splits[i].group.nsplits);
		} else {
			splits[i].split.times.pb = splits[i].split.times.cur;
		}
	}
}

static void _run_finish(struct state *s) {
	struct split *final = get_final_split(s);
	mkdir(RUNS_DIR, 0777);
	char run_name[64];
	strftime(run_name, sizeof run_name, RUNS_DIR "/%Y-%m-%d_%H.%M.%S", localtime(&s->run_started));
	save_times(s->splits, s->nsplits, run_name, offsetof(struct times, cur));
	if (final->split.times.cur < final->split.times.pb) {
		unlink("pb");
		symlink(run_name, "pb");
	}
}

static void _clear_cur(struct split *splits, size_t nsplits) {
	for (size_t i = 0; i < nsplits; ++i) {
		if (splits[i].is_group) {
			_clear_cur(splits[i].group.splits, splits[i].group.nsplits);
		} else {
			splits[i].split.times.cur = UINT64_MAX;
			splits[i].split.times.golded_this_run = false;
		}
	}
}

void update_expanded(struct state *s) {
	_update_expanded(s->active_split, s->splits, s->nsplits);
}

void timer_begin(struct state *s) {
	s->active_split = 0;
	s->run_started = time(NULL);
	update_expanded(s);
}

void timer_reset(struct state *s) {
	if (s->active_split == -1) {
		struct split *final = get_final_split(s);
		if (final->split.times.cur < final->split.times.pb) {
			_commit_pb(s->splits, s->nsplits);
		}
	}
	_clear_cur(s->splits, s->nsplits);
	s->active_split = -1;
	update_expanded(s);
}

void timer_split(struct state *s) {
	if (s->active_split == -1) return;

	struct split *sp = get_split_by_id(s, s->active_split);
	sp->split.times.cur = s->timer;

	if (s->split_time < sp->split.times.best - GOLD_EPSILON) {
		sp->split.times.best = s->split_time;
		sp->split.times.golded_this_run = true;
		save_times(s->splits, s->nsplits, "golds", offsetof(struct times, best));
	}

	if (sp == get_final_split(s)) {
		s->active_split = -1;
		_run_finish(s);
	} else {
		s->active_split++;
	}

	update_expanded(s);
}

static void update_time(struct state *s, uint64_t time) {
	uint64_t prev = 0;
	if (s->active_split > 0) {
		prev = get_split_by_id(s, s->active_split - 1)->split.times.cur;
	}

	s->timer = time;
	s->split_time = time - prev;
}

void timer_parse(struct state *s, const char *str) {
	char *end;
	long us = strtol(str, &end, 10);

	if (end == str) {
		goto err;
	}

	bool updated = false;

	if (end[0] == ' ') {
		++end;
		if (!strcmp(end, "BEGIN")) {
			timer_reset(s);
			update_time(s, us);
			timer_begin(s);
			updated = true;
		} else if (!strcmp(end, "RESET")) {
			timer_reset(s);
			update_time(s, us);
			updated = true;
		} else if (!strcmp(end, "SPLIT")) {
			if (s->active_split != -1) {
				update_time(s, us);
				timer_split(s);
				updated = true;
			}
		} else if (end[0] != '\0') {
			goto err;
		}
	} else if (end[0] != '\0') {
		goto err;
	}

	if (!updated && s->active_split != -1) {
		update_time(s, us);
	}

	return;

err:
	fprintf(stderr, "Warning: bad splitter data! Got line '%s'\n", str);
	return;
}
