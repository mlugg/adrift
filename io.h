#ifndef IO_H
#define IO_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include "common.h"

ssize_t read_splits_file(const char *path, struct split **out);

#endif
