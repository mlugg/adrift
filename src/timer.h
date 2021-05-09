#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "common.h"

void timer_start(struct client *cl);
void timer_reset(struct client *cl);
void timer_split(struct client *cl);
void timer_update(struct client *cl, uint64_t time);

#endif
