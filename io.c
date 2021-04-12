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

		char *name;
		uint64_t best = UINT64_MAX;
		uint64_t pb = UINT64_MAX;
		
		int match = sscanf(line + ntabs, "\"%m[^\n\"]\" %"PRIu64" %"PRIu64, &name, &best, &pb);

		if (match < 1) {
			free(line);
			free_splits(splits, nsplits);
			return -1;
		}

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
				.pb = pb,
				.best = best,
			};
		} else {
			// We have subsplits, so it's a group
			if (match != 1) {
				// Having times doesn't make sense in a group
				if (next) free(next);
				free(line);
				free_splits(subsplits, nsubsplits);
				return -1;
			}
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
