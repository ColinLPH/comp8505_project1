#ifndef NETWORK_H
#define NETWORK_H

#include "context.h"
#include "commands.h"

struct Blob {
    struct Blob *next;
    struct Blob *prev;
    uint8_t data1;
    uint8_t data2;
    uint16_t seq_num;
    int marked;
};

struct List {
    struct Blob *head;
    struct Blob *tail;
    enum command_type type;
    size_t list_size;
};

int setup_covert_fd(void);

int perform_knock(const struct Context *ctx);

unsigned short checksum(void *b, int len);
int send_packet(struct Context *ctx, const char *ip_src_addr);

int recv_file_data(struct Context *ctx, struct List *list);

#endif //NETWORK_H
