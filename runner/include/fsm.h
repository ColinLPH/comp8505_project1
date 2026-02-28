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
#include <sys/time.h>
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

typedef struct Context {
    int knock_alarm_fd;
    int raw_sock_fd;
    State state;
    char commander_ip[INET_ADDRSTRLEN];
} Context;

static void transition(Context *ctx, State next){
    ctx->state = next;
}

void state_init(Context *ctx, Event e){
    switch (e){
        case EV_START:
            printf("Setting up...\n");
            ctx->knock_alarm_fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (ctx->knock_alarm_fd < 0){
                // handle error, fsm transition to error 
                perror("knock alarm socket");
                break;
            }
            break;
        case EV_SETUP_FIN:
            printf("Setting done...\n");
            transition(ctx, state_sleep);
            break;
        default:
            break;
    }
}

void state_sleep(Context *ctx, Event e){
    switch (e)
    {
        case EV_KNOCKED:
            printf("Knocked..\n");
            transition(ctx, state_running);
            break;

        case EV_NONE:
            printf("Nothing happened\n");
            break;

        case EV_ERROR:
            transition(ctx, state_init); // probably shouldn't transition to state_init for better error handling
            break;

        default:
            break;
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

int listen_knock(Context *ctx, int port, int timeout_sec) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(fd, (struct sockaddr *)&addr, sizeof(addr));

    struct timeval tv = {
        .tv_sec = timeout_sec,
        .tv_usec = 0
    };

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    char buffer[BUF_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    ssize_t ret = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender, &sender_len);

    if (ret < 0) {
        close(fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0; // timeout
        return -1;    // error
    }

    if (port == KNOCK3) {
        inet_ntop(AF_INET, &sender.sin_addr, &ctx->commander_ip, INET_ADDRSTRLEN);
        printf("Commander ip: %s\n", ctx->commander_ip);
    }

    close(fd);

    return 1;

}

Event knock_poll(Context *ctx){

    printf("Waiting for knock1\n");
    int ret = listen_knock(ctx, KNOCK1, 0);
    if (ret < 0) {
        return EV_ERROR;
    } else if (ret == 0) {
        return EV_NONE;
    }

    printf("Waiting for knock2\n");
    ret = listen_knock(ctx, KNOCK2, KNOCK_TIMEOUT);
    if (ret < 0) {
        return EV_ERROR;
    } else if (ret == 0) {
        return EV_NONE;
    }

    printf("Waiting for knock3\n");
    ret = listen_knock(ctx, KNOCK3, KNOCK_TIMEOUT);
    if (ret < 0) {
        return EV_ERROR;
    } else if (ret == 0) {
        return EV_NONE;
    }

    return EV_KNOCKED;

}

Event wait_next_event(Context *ctx){
    if (ctx->state == state_init){
        return EV_SETUP_FIN;
    }

    if (ctx->state == state_sleep){
        return knock_poll(ctx);
    }

    if (ctx->state == state_running){
        return EV_SHUTDOWN;
    }

}

#endif // FSM_H
