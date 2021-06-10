#include <unistd.h>
#include <sys/stat.h>
#include "common.h"
#include "vstring.h"
#include "io.h"

#define RUNS_DIR "runs"

static vstring _get_base_path(void) {
	// FIXME: possible buffer overflow!

	static vstring path;

	if (!path) {
		const char *conf_dir = getenv("XDG_CONFIG_DIR");
		if (!conf_dir || !conf_dir[0]) {
			const char *home = getenv("HOME");
			if (!home || !home[0]) abort();
			path = vs_new(home);
			vs_append_c(path, "/.config");
		} else {
			path = vs_new(conf_dir);
		}

		vs_append_c(path, "/adrift");
	}

	return path;
}

bool client_load_splits(struct client *cl) {
	free_splits(cl->splits, cl->nsplits);

	cl->nsplits = 0;
	cl->splits = NULL;

	if (!cl->game_name || !cl->game_name[0]) return false;
	if (!cl->cat_name || !cl->cat_name[0]) return false;

	vstring base = _get_base_path();
	base = vs_append_c(base, "/");
	base = vs_append(base, cl->game_name);
	base = vs_append_c(base, "/");
	base = vs_append(base, cl->cat_name);
	base = vs_append_c(base, "/");

	vstring path = vs_dup(base);
	path = vs_append(path, "splits");

	struct split *splits;
	ssize_t nsplits = read_splits_file(path, &splits);

	vs_free(path);

	if (nsplits == -1) {
		vs_free(base);
		return false;
	}

	cl->nsplits = nsplits;
	cl->splits = splits;

	// FIXME: We need this to be a fuck of a lot more versatile. Probably store a
	// vdict of run name (e.g. "pb", "golds" etc) -> splits associations, although
	// if we do that we should definitely switch to always storing times for
	// individual splits for all saved times (never cumulative).

	path = vs_dup(base);
	path = vs_append(path, "pb");
	read_times(splits, nsplits, path, offsetof(struct times, pb));
	vs_free(path);

	path = vs_dup(base);
	path = vs_append(path, "golds");
	read_times(splits, nsplits, path, offsetof(struct times, best));
	vs_free(path);

	vs_free(base);

	return nsplits != -1;
}

bool client_save_golds(struct client *cl) {
	if (!cl->game_name || !cl->game_name[0]) return false;
	if (!cl->cat_name || !cl->cat_name[0]) return false;

	vstring path = _get_base_path();
	path = vs_append_c(path, "/");
	path = vs_append(path, cl->game_name);
	path = vs_append_c(path, "/");
	path = vs_append(path, cl->cat_name);
	path = vs_append_c(path, "/golds");

	bool ret = save_times(cl->splits, cl->nsplits, path, offsetof(struct times, best));

	vs_free(path);

	return ret;
}

bool client_save_run(struct client *cl, bool pb) {
	if (!cl->game_name || !cl->game_name[0]) return false;
	if (!cl->cat_name || !cl->cat_name[0]) return false;

	vstring base = _get_base_path();
	base = vs_append_c(base, "/");
	base = vs_append(base, cl->game_name);
	base = vs_append_c(base, "/");
	base = vs_append(base, cl->cat_name);
	base = vs_append_c(base, "/");

	vstring run_path = vs_dup(base);
	run_path = vs_append_c(run_path, RUNS_DIR "/");

	mkdir(run_path, 0777);

	{
		char run_name[64];
		size_t len = strftime(run_name, sizeof run_name, "%Y-%m-%d_%H-%M-%S", localtime(&cl->run_started));
		if (len == 0) unreachable;
		run_path = vs_append_n(run_path, run_name, len);
	}

	if (!save_times(cl->splits, cl->nsplits, run_path, offsetof(struct times, best))) {
		vs_free(base);
		vs_free(run_path);
		return false;
	}

	if (pb) {
		vstring pb_path = vs_dup(base);
		pb_path = vs_append_c(pb_path, "pb");
		unlink(pb_path);
		symlink(run_path, pb_path);
		vs_free(pb_path);
	}

	vs_free(base);
	vs_free(run_path);

	return true;
}
