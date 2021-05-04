#define VDICT_IMPL

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

bool config_get_color(struct cfgdict *cfg, const char *k, float *r, float *g, float *b, float *a) {
	char *v;
	if (!cfgdict_get(cfg, (char *)k, &v)) {
		return false;
	}

	if (v[0] == '#') ++v;

	size_t len = strlen(v);
	int ri, gi, bi, ai = 255;
	int p;

	if (len == 6 && sscanf(v, "%2x%2x%2x%n", &ri, &gi, &bi, &p) == 3 && p == 6) {
		goto done;
	}

	if (len == 8 && sscanf(v, "%2x%2x%2x%2x%n", &ri, &gi, &bi, &ai, &p) == 4 && p == 8) {
		goto done;
	}

	return false;

done:
	*r = ri / 255.0f;
	*g = gi / 255.0f;
	*b = bi / 255.0f;
	*a = ai / 255.0f;
	return true;
}

const char *config_get_str(struct cfgdict *cfg, const char *k, const char *def) {
	char *ret = (char *)def;
	cfgdict_get(cfg, (char *)k, &ret);
	return ret;
}
