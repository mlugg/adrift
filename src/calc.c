#include "calc.h"

static uint64_t _sob(struct split *splits, size_t nsplits) {
	uint64_t sum = 0;

	for (size_t i = 0; i < nsplits; ++i) {
		uint64_t best;
		if (splits[i].is_group) {
			best = _sob(splits[i].group.splits, splits[i].group.nsplits);
		} else {
			best = splits[i].split.times.best;
		}

		if (best == UINT64_MAX) {
			return UINT64_MAX;
		}

		sum += best;
	}

	return sum;
}

uint64_t calc_sum_of_best(struct client *cl) {
	return _sob(cl->splits, cl->nsplits);
}

static uint64_t _bpt(struct client *cl, struct split *splits, size_t nsplits) {
	uint64_t sum = 0;

	for (size_t i = 0; i < nsplits; ++i) {
		uint64_t best;
		if (splits[i].is_group) {
			best = _bpt(cl, splits[i].group.splits, splits[i].group.nsplits);
		} else if (splits[i].split.id == cl->active_split) {
			best = splits[i].split.times.best;
			if (cl->split_time > best) best = cl->split_time;
		} else if (splits[i].split.times.cur != UINT64_MAX) {
			uint64_t prev = splits[i].split.id == 0 ? 0 : get_split_by_id(cl, splits[i].split.id - 1)->split.times.cur;
			best = splits[i].split.times.cur - prev;
		} else {
			best = splits[i].split.times.best;
		}

		if (best == UINT64_MAX) {
			return UINT64_MAX;
		}

		sum += best;
	}

	return sum;
}

uint64_t calc_best_possible_time(struct client *cl) {
	return _bpt(cl, cl->splits, cl->nsplits);
}
