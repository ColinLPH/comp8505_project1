#ifndef CONTEXT_H
#define CONTEXT_H

#include <arpa/inet.h>

struct Context {
    int covert_fd;
    uint16_t cmd_seq_num;
    uint16_t rep_seq_num;
    char commander_ip[INET_ADDRSTRLEN];
};


#endif // CONTEXT_H
