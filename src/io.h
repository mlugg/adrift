#ifndef IO_H
#define IO_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include "common.h"
#include "config.h"

ssize_t read_splits_file(const char *path, struct split **out);
bool read_times(struct split *splits, size_t nsplits, const char *path, size_t off);
bool save_times(struct split *splits, size_t nsplits, const char *path, size_t off);
bool read_config(const char *path, struct cfgdict *cfg);

#endif
