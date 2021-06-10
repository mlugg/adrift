#include <stdio.h>
#include "common.h"
#include "client.h"
#include "timer.h"
#define VSTRING_IMPL
#include "vstring.h"
#include "proto.h"
#include <assert.h>
#include <unistd.h>

#define PROTO_VERSION "0.1"

static inline long _parse_uint(const char *str) {
	char *end;
	long x = strtol(str, &end, 10);
	if (*end || end == str) {
		return -1;
	}
	return x;
}

static void _c2s_hello(struct client *cl, size_t argc, const char *const *argv) {
	for (size_t i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], PROTO_VERSION)) {
			write(cl->fd, "HELLO " PROTO_VERSION "\n", sizeof ("HELLO " PROTO_VERSION "\n"));
			write(cl->fd, "NORECOVER\n", sizeof "NORECOVER\n"); // TODO: recovery
			return;
		}
	}

	// TODO: disconnect
}

static void _c2s_game(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 3);
	cl->game_name = vs_new(argv[1]);
	cl->game_name_hr = vs_new(argv[2]);
	client_load_splits(cl);
}

static void _c2s_cat(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 3);
	cl->cat_name = vs_new(argv[1]);
	cl->cat_name_hr = vs_new(argv[2]);
	client_load_splits(cl);
}

static void _c2s_addcat(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 3);
	int n = ++cl->n_avail_cats;
	cl->avail_cats = realloc(cl->avail_cats, n * sizeof cl->avail_cats[0]);
	cl->avail_cats[n - 1] = (struct category){
		.name = vs_new(argv[1]),
		.name_hr = argv[2] && argv[2][0] ? vs_new(argv[2]) : NULL,
		.committed = false,
		.uncommitted = true,
	};
}

static inline void _delete_avail_cat(struct client *cl, size_t i) {
	size_t n = --cl->n_avail_cats;
	vs_free(cl->avail_cats[i].name);
	vs_free(cl->avail_cats[i].name_hr);
	memmove(&cl->avail_cats[i], &cl->avail_cats[i + 1], (n - i) * sizeof cl->avail_cats[0]);
	if (n == 0) {
		free(cl->avail_cats);
		cl->avail_cats = NULL;
	} else {
		cl->avail_cats = realloc(cl->avail_cats, n * sizeof cl->avail_cats[0]);
	}
}

static void _c2s_delcat(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);
	for (size_t i = 0; i < cl->n_avail_cats; ++i) {
		if (!strcmp(argv[1], cl->avail_cats[i].name)) {
			if (!cl->avail_cats[i].committed) {
				_delete_avail_cat(cl, i);
				--i;
				continue;
			}
			cl->avail_cats[i].uncommitted = false;
		}
	}
}

static void _c2s_clearcats(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 1);
	for (size_t i = 0; i < cl->n_avail_cats; ++i) {
		if (!cl->avail_cats[i].committed) {
			_delete_avail_cat(cl, i);
			--i;
			continue;
		}
		cl->avail_cats[i].uncommitted = false;
	}
}

static void _c2s_commitcats(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 1);
	for (size_t i = 0; i < cl->n_avail_cats; ++i) {
		if (!cl->avail_cats[i].uncommitted) {
			_delete_avail_cat(cl, i);
			--i;
			continue;
		}
		cl->avail_cats[i].committed = cl->avail_cats[i].uncommitted;
	}
}

static void _c2s_sync(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);
	long us = _parse_uint(argv[1]);
	if (us == -1) {
		// TODO: disconnect?
		return;
	}
	timer_update(cl, us);
}

static void _c2s_start(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);
	long us = _parse_uint(argv[1]);
	if (us == -1) {
		// TODO: disconnect?
		return;
	}
	timer_update(cl, us);
	timer_start(cl);
}

static void _c2s_split(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);
	long us = _parse_uint(argv[1]);
	if (us == -1) {
		// TODO: disconnect?
		return;
	}
	timer_update(cl, us);
	timer_split(cl);
}

static void _c2s_reset(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);
	long us = _parse_uint(argv[1]);
	if (us == -1) {
		// TODO: disconnect?
		return;
	}
	timer_reset(cl);
	timer_update(cl, us);
}

static void _c2s_nsplits(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);

	size_t count = _parse_uint(argv[1]);
	if (count == -1) {
		// TODO: disconnect?
		return;
	}

	if (cl->splits && count == get_final_split(cl)->split.id + 1) {
		// Our existing splits are fine
		return;
	}

	if (cl->splits) {
		// TODO: backup existing splits
	}

	free_splits(cl->splits, cl->nsplits);

	cl->nsplits = count;
	cl->splits = malloc(count * sizeof cl->splits[0]);

	for (size_t i = 0; i < count; ++i) {
		char name_buf[32];
		snprintf(name_buf, sizeof name_buf, "%u\n", (unsigned)i);
		cl->splits[i].name = vs_new(name_buf);
		cl->splits[i].is_group = false;
		cl->splits[i].split.id = i;
		cl->splits[i].split.times = (struct times){
			.cur = UINT64_MAX,
			.pb = UINT64_MAX,
			.best = UINT64_MAX,
			.golded_this_run = false,
		};
	}
}

static void _c2s_splitname(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 3);

	int id = _parse_uint(argv[1]);
	if (id == -1) {
		// TODO: disconnect?
		return;
	}

	struct split *sp = get_split_by_id(cl, id);
	if (!sp) {
		// TODO: disconnect?
		return;
	}

	vs_free(sp->name);
	sp->name = vs_new(argv[2]);
}

static void _c2s_recoverdata(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);
	free(cl->recovery);
	size_t len = strlen(argv[1]);
	void *buf = malloc(len);
	memcpy(buf, argv[1], len);
	cl->recovery = buf;
	cl->recovery_len = len;
}

static void _c2s_recoverappend(struct client *cl, size_t argc, const char *const *argv) {
	assert(argc == 2);
	size_t old_len = cl->recovery_len;
	size_t new_len = strlen(argv[1]);
	void *buf = realloc(cl->recovery, old_len + new_len);
	memcpy((char *)buf + old_len, argv[1], new_len);
	cl->recovery = buf;
	cl->recovery_len += new_len;
}

static void _c2s_goodbye(struct client *cl, size_t argc, const char *const *argv) {
	cl->graceful_term = true;
}

struct c2s_cmd g_c2s_cmds[] = {
	{ "HELLO", SIZE_MAX, false, &_c2s_hello },

	{ "GAME", 1, true, &_c2s_game },
	{ "CAT", 1, true, &_c2s_cat },

	{ "ADDCAT", 1, true, &_c2s_addcat },
	{ "DELCAT", 1, false, &_c2s_delcat },
	{ "CLEARCATS", 0, false, &_c2s_clearcats },
	{ "COMMITCATS", 0, false, &_c2s_commitcats },

	{ "SYNC", 1, false, &_c2s_sync },
	{ "START", 1, false, &_c2s_start },
	{ "SPLIT", 1, false, &_c2s_split },
	{ "RESET", 1, false, &_c2s_reset },

	{ "NSPLITS", 1, false, &_c2s_nsplits },
	{ "SPLITNAME", 1, true, &_c2s_splitname },

	{ "RECOVERDATA", 0, true, &_c2s_recoverdata },
	{ "RECOVERAPPEND", 0, true, &_c2s_recoverappend },

	{ "GOODBYE", 0, false, &_c2s_goodbye },
};

