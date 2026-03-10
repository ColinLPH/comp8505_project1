#ifndef KEYLOG_H
#define KEYLOG_H

#include "context.h"
#include "runner.h"

// Tracks modifiers for external access if needed
typedef struct {
    int shift;
    int ctrl;
    int alt;
    int meta;
    int capslock;
} modifier_state_t;

void start_key_logging(int keyboard_fd, struct Context *ctx, struct List *list);

#endif // KEYLOG_H
