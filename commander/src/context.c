#include "context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

void refill_pool(struct Context *ctx) {
    const ssize_t n = getrandom(ctx->random_pool, POOL_SIZE, 0);
    if (n < 0) {
        fprintf(stderr, "getrandom failed\n");
        exit(EXIT_FAILURE);
    }
    ctx->pool_pos = 0;
    ctx->pool_rem = n;
}

uint8_t rand_u8(struct Context *ctx) {
    if (ctx->pool_rem == 0) {
        refill_pool(ctx);
    }
    const uint8_t val = ctx->random_pool[ctx->pool_pos];
    ctx->pool_pos++;
    ctx->pool_rem--;
    return val;
}

uint8_t rand_ip_octet(struct Context *ctx) {
    static const uint8_t start[4] = {23, 104, 170, 193};
    static const uint8_t size[4]  = {77, 65, 22, 31};

    uint8_t r = rand_u8(ctx);

    uint8_t range = r & 3;
    uint8_t offset = rand_u8(ctx) % size[range];

    return start[range] + offset;
}

uint16_t rand_u16(struct Context *ctx) {
    const uint16_t val = ((uint16_t)rand_u8(ctx) << 8) | rand_u8(ctx);
    return val;
}

uint16_t rand_payload_size(struct Context *ctx) {
    const uint16_t val = ((uint16_t)rand_u8(ctx) << 8) | rand_u8(ctx);
    return 1 + val % MAX_CHUNK;
}

uint16_t rand_payload(struct Context *ctx, uint8_t *buffer) {
    const uint16_t out_size = rand_payload_size(ctx);

    uint16_t remaining = out_size;
    while (remaining > 0) {
        if (ctx->pool_rem == 0) {
            refill_pool(ctx);
        }

        size_t chunk = ctx->pool_rem;
        if (chunk > remaining) {
            chunk = remaining;
        }

        memcpy(buffer, ctx->random_pool + ctx->pool_pos, chunk);

        buffer += chunk;
        ctx->pool_pos += chunk;
        ctx->pool_rem -= chunk;
        remaining -= chunk;
    }

    return out_size;
}
