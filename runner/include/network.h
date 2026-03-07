#ifndef NETWORK_H
#define NETWORK_H

#include "context.h"

#define KNOCKS 3
#define COVERT_CHAN 8505
#define KNOCK2 1999
#define KNOCK3 1201
#define KNOCK_TO 30

#define BUF_SIZE 4096 //largest ipv4 packet possible is 65,535

#define HEADER_SIZE 28 //mininum ipv4 header + udp header

void print_packet_info(uint8_t *packet);

int setup_covert_fd(void);
unsigned short checksum(void *b, int len);
int send_packet(struct Context *ctx, const char *ip_src_addr);

#endif //NETWORK_H
