#ifndef FSM_H
#define FSM_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define KNOCK1 8505
#define KNOCK2 1999
#define KNOCK3 1201
#define KNOCK_TIMEOUT 30

#define BUF_SIZE 2048

typedef enum {
    EV_NONE,
    EV_START,
    EV_SETUP_FIN,
    EV_KNOCKED,
    EV_ERROR,
    EV_SHUTDOWN,
} Event;

struct Context;
typedef void (*State)(struct Context *ctx, Event e);

void state_init(struct Context *ctx, Event e);
void state_sleep(struct Context *ctx, Event e);
void state_running(struct Context *ctx, Event e);
void state_uninstall(struct Context *ctx, Event e);

struct KnockAlarm {
    int progress;
    time_t last_knock;
};

typedef struct Context {
    int raw_sock_fd;
    struct KnockAlarm knock_alarm;
    State state;
    char *commander_ip;
} Context;

static void transition(Context *ctx, State next){
    ctx->state = next;
}

void state_init(Context *ctx, Event e){
    switch (e){
        case EV_START:
            printf("Setting up...\n");
            ctx->raw_sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
            if (ctx->raw_sock_fd < 0){
                // handle error, fsm transition to error 
                perror("socket");
                return;
            }
            ctx->knock_alarm.progress = 0;
            ctx->knock_alarm.last_knock = 0;
            return;
        case EV_SETUP_FIN:
            printf("Setting done...\n");
            transition(ctx, state_sleep);
            return;
        default:
            return;
    }
}

void state_sleep(Context *ctx, Event e){
    switch (e)
    {
        case EV_KNOCKED:
            printf("Knocked..\n");
            transition(ctx, state_running);
            return;

        case EV_ERROR:
            transition(ctx, state_init);
            return;

        default:
            return;
    }
}

void state_running(Context *ctx, Event e){
    switch (e)
    {
        case EV_SHUTDOWN:
            printf("Shutting down...\n");
            transition(ctx, state_sleep);
            return;

        case EV_ERROR:
            transition(ctx, state_init);
            return;

        default:
            return;
    }
}

Event knock_poll(Context *ctx){
    struct KnockAlarm *ka = &ctx->knock_alarm;

    uint8_t buffer[BUF_SIZE];

    ssize_t ret = recv(ctx->raw_sock_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    if(ret < 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            return EV_NONE;
        }
        perror("recv");
        return EV_ERROR;
    }

    struct iphdr *ip = (struct iphdr *)buffer;
    if(ip->protocol != IPPROTO_UDP){
        return EV_NONE;
    }

    unsigned short iphdrlen = ip->ihl * 4;
    struct udphdr *udp = (struct udphdr *)(buffer + iphdrlen);

    int dest_port = ntohs(udp->dest);
    time_t now = time(NULL);

    if (ka->progress > 0 &&
        now - ka->last_knock > KNOCK_TIMEOUT)
    {
        ka->progress = 0;
    }

    switch (ka->progress) {

        case 0:
            if (dest_port == KNOCK1) {
                ka->progress = 1;
                ka->last_knock = now;
            }
            break;

        case 1:
            if (dest_port == KNOCK2) {
                ka->progress = 2;
                ka->last_knock = now;
            } else {
                ka->progress = 0;
            }
            break;

        case 2:
            if (dest_port == KNOCK3) {
                ka->progress = 0;
                // add commander ip to context
                return EV_KNOCKED;
            } else {
                ka->progress = 0;
            }
            break;
    }

    return EV_NONE;
}

Event wait_next_event(Context *ctx){
    if (ctx->state == state_init){
        return EV_SETUP_FIN;

    }
    if (ctx->state == state_sleep){
        sleep(5);
        return EV_KNOCKED;
    }
    if (ctx->state == state_running){
        return EV_SHUTDOWN;
    }

}

#endif // FSM_H
