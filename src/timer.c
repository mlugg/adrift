#include "timer.h"
#include "client.h"
#include "io.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define RUNS_DIR "runs"

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

static void _run_finish(struct client *cl) {
	struct split *final = get_final_split(cl);
	bool pb = final->split.times.cur < final->split.times.pb;
	client_save_run(cl, pb);
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

void update_expanded(struct client *cl) {
	_update_expanded(cl->active_split, cl->splits, cl->nsplits);
}

void timer_start(struct client *cl) {
	cl->active_split = 0;
	cl->run_started = time(NULL);
	update_expanded(cl);
}

void timer_reset(struct client *cl) {
	if (cl->active_split == -1) {
		struct split *final = get_final_split(cl);
		if (final->split.times.cur < final->split.times.pb) {
			_commit_pb(cl->splits, cl->nsplits);
		}
	}
	_clear_cur(cl->splits, cl->nsplits);
	cl->active_split = -1;
	update_expanded(cl);
}

void timer_split(struct client *cl) {
	if (cl->active_split == -1) return;

	struct split *sp = get_split_by_id(cl, cl->active_split);
	sp->split.times.cur = cl->timer;

	if (cl->split_time < sp->split.times.best) {
		sp->split.times.best = cl->split_time;
		sp->split.times.golded_this_run = true;
	}

	if (sp == get_final_split(cl)) {
		cl->active_split = -1;
		_run_finish(cl);
	} else {
		cl->active_split++;
	}

	update_expanded(cl);
}

void timer_update(struct client *cl, uint64_t time) {
	uint64_t prev = 0;
	if (cl->active_split > 0) {
		prev = get_split_by_id(cl, cl->active_split - 1)->split.times.cur;
	}

	cl->timer = time;
	cl->split_time = time - prev;
}
