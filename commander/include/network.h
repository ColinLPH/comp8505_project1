#ifndef NETWORK_H
#define NETWORK_H

#include "context.h"

enum packet_type {
    CMD,
    DATA,
    TERM
};

int setup_covert_fd(void);

int perform_knock(const struct Context *ctx);

unsigned short checksum(void *b, int len);
int send_packet(struct Context *ctx, const char *ip_src_addr);

#endif //NETWORK_H
