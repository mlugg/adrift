#ifndef TIMER_H
#define TIMER_H

#include "common.h"

void timer_begin(struct state *s);
void timer_reset(struct state *s);
void timer_split(struct state *s);
void timer_parse(struct state *s, const char *str);

#endif
