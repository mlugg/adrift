#include "common.h"
#include <stdlib.h>

static struct split *_get_split_by_id(struct split *splits, size_t nsplits, unsigned id) {
	for (size_t i = 0; i < nsplits; ++i) {
		if (splits[i].is_group) {
			struct split *sp = _get_split_by_id(splits[i].group.splits, splits[i].group.nsplits, id);
			if (sp) return sp;
		} else if (splits[i].split.id == id) {
			return &splits[i];
		}
	}

	return NULL;
}

struct split *get_split_by_id(struct client *cl, unsigned id) {
	return _get_split_by_id(cl->splits, cl->nsplits, id);
}

static struct split *_get_final_split(struct split *splits, size_t nsplits) {
	struct split *sp = &splits[nsplits - 1];
	if (sp->is_group) {
		return _get_final_split(sp->group.splits, sp->group.nsplits);
	} else {
		return sp;
	}
}

struct split *get_final_split(struct client *cl) {
	return _get_final_split(cl->splits, cl->nsplits);
}

struct times get_split_times(struct split *sp) {
	while (sp->is_group) {
		sp = &sp->group.splits[sp->group.nsplits - 1];
	}

	return sp->split.times;
}

uint64_t get_comparison(struct state *s, struct times t) {
	return t.pb;
}

void free_splits(struct split *splits, size_t nsplits) {
	for (size_t i = 0; i < nsplits; ++i) {
		vs_free(splits[i].name);
		if (splits[i].is_group) {
			free_splits(splits[i].group.splits, splits[i].group.nsplits);
		}
	}
}
