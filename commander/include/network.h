#ifndef NETWORK_H
#define NETWORK_H

#include "context.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define KNOCK1 8505
#define KNOCK2 1999
#define KNOCK3 1201

#define BUF_SIZE 2048

int perform_knock(struct Context *ctx) {
    //do knock sequence
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(KNOCK1);
    if (inet_pton(AF_INET, ctx->runner_ip, &dest.sin_addr) != 1) {
        perror("inet_pton");
        close(fd);
        return -1;
    }
    socklen_t dest_len = sizeof(dest);

    const char knock_msg[] = "This is a knock";

    ssize_t ret;
    ret = sendto(fd, knock_msg, strlen(knock_msg), 0, (struct sockaddr *)&dest, dest_len);
    if (ret == -1) {
        perror("sendto knock1");
        return -1;
    }

    sleep(1);
    dest.sin_port = htons(KNOCK2);
    ret = sendto(fd, knock_msg, strlen(knock_msg), 0, (struct sockaddr *)&dest, dest_len);
    if (ret == -1) {
        perror("sendto knock2");
        return -1;
    }

    sleep(1);
    dest.sin_port = htons(KNOCK3);
    ret = sendto(fd, knock_msg, strlen(knock_msg), 0, (struct sockaddr *)&dest, dest_len);
    if (ret == -1) {
        perror("sendto knock3");
        return -1;
    }

    close(fd);

    return 1;
}

#endif //NETWORK_H
