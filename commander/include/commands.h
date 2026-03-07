#ifndef COMMANDS_H
#define COMMANDS_H

#include <context.h>

int prompt_stdin(char *buffer, int buf_size);
int cmd_send_file(struct Context *ctx);

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2,
    uint8_t byte3, uint8_t byte4);


/**
 * sending a file:
 *
 *  - function to encode 2 bytes into an ip address                                         done
 *  - sth to retrieve the next byte from a file
 *  - the function to encode doesn't know if the file or whatever its sending
 *      has an odd length, just pass all zero's to the 2nd byte and it will encode that
 *  - after each encoding, the packet is sent,
 *      properly updating seq num, then the program randomly sleeps
 */

#endif //COMMANDS_H
