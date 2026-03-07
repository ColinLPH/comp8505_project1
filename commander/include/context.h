#ifndef CONTEXT_H
#define CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#define IP_ADDR_LEN 16

#define POOL_SIZE 8192
#define MAX_CHUNK 1200

struct Context {
    size_t pool_pos;
    size_t pool_rem;
    int covert_fd;
    uint16_t cmd_seq_num;
    uint16_t rep_seq_num;
    char runner_ip[IP_ADDR_LEN];
    uint8_t random_pool[POOL_SIZE];
};

void refill_pool(struct Context *ctx);
uint8_t rand_u8(struct Context *ctx);
uint8_t rand_ip_octet(struct Context *ctx);
uint16_t rand_u16(struct Context *ctx);
uint16_t rand_payload_size(struct Context *ctx);
uint16_t rand_payload(struct Context *ctx, uint8_t *buffer);

#endif //CONTEXT_H
