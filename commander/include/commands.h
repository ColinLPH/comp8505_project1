#ifndef COMMANDS_H
#define COMMANDS_H

#include <context.h>

int prompt_stdin(char *buffer, int buf_size);
int cmd_send_file(struct Context *ctx);

#endif //COMMANDS_H
