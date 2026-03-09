#ifndef RUNNER_H
#define RUNNER_H

#include "context.h"

struct Blob {
    struct Blob *next;
    struct Blob *prev;
    uint8_t byte1;
    uint8_t byte2;
    uint16_t seq_num;
};

struct List {
    struct Blob *head;
    struct Blob *tail;

};

int runner(struct Context *ctx);

int runner_recv(struct Context *ctx);

int knock_poll(struct Context *ctx);
int listen_knock(struct Context *ctx);

#endif // RUNNER_H
