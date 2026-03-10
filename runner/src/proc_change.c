#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "proc_change.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/prctl.h>

#define PROC_NAME_LEN 256

typedef struct {
    char name[PROC_NAME_LEN];
    int count;
} name_count;

static int is_digits(const char *s)
{
    if (!s || *s == '\0')
        return 0;

    for (; *s; s++)
        if (!isdigit((unsigned char)*s))
            return 0;

    return 1;
}

static char *read_comm(pid_t pid)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);

    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    char buf[PROC_NAME_LEN];

    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return NULL;
    }

    fclose(f);

    buf[strcspn(buf, "\n")] = '\0';

    return strdup(buf);
}

static void add_name(name_count **arr, size_t *len, const char *name)
{
    for (size_t i = 0; i < *len; i++) {
        if (strcmp((*arr)[i].name, name) == 0) {
            (*arr)[i].count++;
            return;
        }
    }

    name_count *tmp = realloc(*arr, (*len + 1) * sizeof(name_count));
    if (!tmp)
        return;

    *arr = tmp;

    strncpy((*arr)[*len].name, name, PROC_NAME_LEN - 1);
    (*arr)[*len].name[PROC_NAME_LEN - 1] = '\0';

    (*arr)[*len].count = 1;
    (*len)++;
}

static const char *most_common(name_count *arr, size_t len)
{
    if (len == 0)
        return NULL;

    size_t best = 0;

    for (size_t i = 1; i < len; i++) {
        if (arr[i].count > arr[best].count)
            best = i;
    }

    return arr[best].name;
}

static void rename_process(int argc, char **argv, const char *name)
{
    if (!name)
        return;

    /* update kernel name */
    prctl(PR_SET_NAME, name, 0, 0, 0);

    /* overwrite argv[0] */
    size_t old_len = strlen(argv[0]);
    size_t new_len = strlen(name);

    if (new_len > old_len)
        new_len = old_len;

    memcpy(argv[0], name, new_len);

    if (new_len < old_len)
        memset(argv[0] + new_len, '\0', old_len - new_len);
}

int rename_to_most_common_process(int argc, char **argv)
{
    DIR *proc = opendir("/proc");
    if (!proc)
        return -1;

    struct dirent *entry;

    name_count *names = NULL;
    size_t name_len = 0;

    while ((entry = readdir(proc)) != NULL) {

        if (!is_digits(entry->d_name))
            continue;

        pid_t pid = (pid_t)atoi(entry->d_name);

        char *comm = read_comm(pid);

        if (!comm)
            continue;

        add_name(&names, &name_len, comm);

        free(comm);
    }

    closedir(proc);

    const char *target = most_common(names, name_len);

    if (!target) {
        free(names);
        return -1;
    }

    printf("Renaming to: %s\n", target);
    rename_process(argc, argv, target);

    free(names);

    return 0;
}