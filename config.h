#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define VDICT_NAME cfgdict
#define VDICT_KEY char *
#define VDICT_VAL char *
#define VDICT_HASH vdict_hash_string
#define VDICT_EQUAL vdict_eq_string
#include "vdict.h"

bool config_get_color(struct cfgdict *cfg, const char *k, float *r, float *g, float *b, float *a);
long config_get_int(struct cfgdict *cfg, const char *k, long def);
const char *config_get_str(struct cfgdict *cfg, const char *k, const char *def);

#endif
