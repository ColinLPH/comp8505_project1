#ifndef RUNNER_H
#define RUNNER_H

#include "context.h"

int runner(struct Context *ctx);
int runner_loop(struct Context *ctx);

int knock_poll(struct Context *ctx);
int listen_knock(struct Context *ctx);

#endif // RUNNER_H
