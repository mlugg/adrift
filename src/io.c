#include "io.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

static inline size_t _count_tabs(char *line) {
	size_t i = 0;
	while (line[i] == '\t') ++i;
	return i;
};

static ssize_t _read_splits(FILE *f, unsigned *id, unsigned level, struct split **out, char **trailing_line) {
	size_t splits_alloc = 8;
	size_t nsplits = 0;
	struct split *splits = malloc(splits_alloc * sizeof splits[0]);

	char *line = NULL;

	while (true) {
		if (!line) {
			size_t n = 0;
			if (getline(&line, &n, f) == -1) {
				free(line);
				*trailing_line = NULL;
				break;
			}
		}

		if (!strcmp(line, "\n")) {
			line = NULL;
			continue;
		}

		size_t ntabs = _count_tabs(line);

		if (ntabs != level) {
			*trailing_line = line;
			break;
		}

		if (splits_alloc == nsplits) {
			splits_alloc *= 2;
			splits = realloc(splits, splits_alloc * sizeof splits[0]);
		}

		size_t len = strlen(line + ntabs);
		char *name = malloc(len);
		strncpy(name, line + ntabs, len - 1);
		name[len - 1] = 0;

		char *next = NULL;

		splits[nsplits].name = name;

		struct split *subsplits;
		ssize_t nsubsplits = _read_splits(f, id, level + 1, &subsplits, &next);
		if (nsubsplits < 1) {
			// No subsplits, so it's an actual split
			splits[nsplits].is_group = false;
			splits[nsplits].split.id = (*id)++;
			splits[nsplits].split.times = (struct times){
				.cur = UINT64_MAX,
				.pb = UINT64_MAX,
				.best = UINT64_MAX,
				.golded_this_run = false,
			};
		} else {
			// We have subsplits, so it's a group
			splits[nsplits].is_group = true;
			splits[nsplits].group.expanded = false;
			splits[nsplits].group.nsplits = nsubsplits;
			splits[nsplits].group.splits = subsplits;
		}

		free(line);

		line = next;
		++nsplits;
	}

	if (nsplits == 0) {
		free(splits);
	}

	*out = splits;
	return nsplits;
}

ssize_t read_splits_file(const char *path, struct split **out) {
	FILE *f = fopen(path, "r");

	if (!f) {
		return -1;
	}

	unsigned id = 0;

	char *trailing;
	ssize_t nsplits = _read_splits(f, &id, 0, out, &trailing);

	if (nsplits != -1 && trailing) {
		fprintf(stderr, "trailing line in splits file: %s", trailing); // We don't print a newline as the trailing line includes one
		free(trailing);
		free_splits(*out, nsplits);
		return -1;
	}

	fclose(f);

	return nsplits;
}

static bool _read_times(FILE *f, size_t off, struct split *splits, size_t nsplits) {
	for (size_t i = 0; i < nsplits; ++i) {
		if (splits[i].is_group) {
			if (!_read_times(f, off, splits[i].group.splits, splits[i].group.nsplits)) {
				return false;
			}
		} else {
			uint64_t val;
			int dummy = -1;
			if (fscanf(f, "-\n%n", &dummy) == 0 && dummy != -1) {
				*(uint64_t *)((char *)&splits[i].split.times + off) = UINT64_MAX;
			} else if (fscanf(f, "%"PRIu64"\n", &val) == 1) {
				*(uint64_t *)((char *)&splits[i].split.times + off) = val;
			} else {
				return false;
			}
		}
	}

	return true;
}

static void _clear_times(size_t off, struct split *splits, size_t nsplits) {
	for (size_t i = 0; i < nsplits; ++i) {
		if (splits[i].is_group) {
			_clear_times(off, splits[i].group.splits, splits[i].group.nsplits);
		} else {
			*(uint64_t *)((char *)&splits[i].split.times + off) = UINT64_MAX;
		}
	}
}

bool read_times(struct split *splits, size_t nsplits, const char *path, size_t off) {
	FILE *f = fopen(path, "r");

	if (!f) {
		return false;
	}

	bool success = _read_times(f, off, splits, nsplits);

	// TODO: enforce eof

	fclose(f);

	if (!success) {
		_clear_times(off, splits, nsplits);
	}

	return success;
}

bool _save_times(FILE *f, size_t off, struct split *splits, size_t nsplits) {
	for (size_t i = 0; i < nsplits; ++i) {
		if (splits[i].is_group) {
			_save_times(f, off, splits[i].group.splits, splits[i].group.nsplits);
		} else {
			uint64_t time = *(uint64_t *)((char *)&splits[i].split.times + off);
			if (time == UINT64_MAX) {
				if (fputs("-\n", f) == EOF) {
					return false;
				}
			} else {
				if (fprintf(f, "%"PRIu64"\n", time) == EOF) {
					return false;
				}
			}
		}
	}

	return true;
}

bool save_times(struct split *splits, size_t nsplits, const char *path, size_t off) {
	FILE *f = fopen(path, "w");

	if (!f) {
		return false;
	}

	bool success = _save_times(f, off, splits, nsplits);

	fclose(f);

	return success;
}

// TODO: this leaks memory, might be worth not doing that
bool read_config(const char *path, struct cfgdict *cfg) {
	FILE *f = fopen(path, "r");

	if (!f) {
		return false;
	}

	while (true) {
		size_t allocd = 0;
		char *line = NULL;
		size_t n = getline(&line, &allocd, f);

		if (n == -1) {
			break;
		}

		if (n > 0) {
			int p = -1;
			char *k, *v;
			int matched = sscanf(line, "%ms %ms %n", &k, &v, &p);
			if (matched != 2 || p == -1) {
				if (matched >= 2) free(v);
				if (matched >= 1) free(k);
				// TODO: clear cfg
				fclose(f);
				return false;
			}
			int ret = cfgdict_put(cfg, k, v);
			if (ret == -1) {
				free(k);
				free(v);
				// TODO: clear cfg
				fclose(f);
				return false;
			} else if (ret == 1) {
				fprintf(stderr, "Warning: duplicate config key %s\n", k);
			}
		}
	}

	fclose(f);

	return true;
}
