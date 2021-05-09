#ifndef PROTO_H
#define PROTO_H

#include "common.h"

void proto_main(struct state *s);

struct c2s_cmd {
	const char *name;
	size_t normal_args;
	bool has_long_arg;
	void (*callback)(struct client *cl, size_t argc, const char *const *argv);
};

extern struct c2s_cmd g_c2s_cmds[12];

#endif
