#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

// TODO: should we move the struct client definition here?

bool client_load_splits(struct client *cl);
bool client_save_golds(struct client *cl);
bool client_save_run(struct client *cl, bool pb);

#endif
