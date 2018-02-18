#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static unsigned char *escape(unsigned char *data, int *data_size)
{
    int esc_pos, data_pos;
    ssize_t esc_size = 4096;
    unsigned char *esc = malloc(esc_size);
    esc[0] = 0;

    for(esc_pos = data_pos = 0; data_pos < *data_size; data_pos++) {
        if((esc_pos + 16) >= esc_size) {
            esc_size += 4096;
            esc = realloc(esc, esc_size);
        }
        if(data[data_pos] == '\r') {
            sprintf((char *)&esc[esc_pos], "\\r");
            esc_pos += 2;
        } else if(data[data_pos] == '\n') {
            sprintf((char *)&esc[esc_pos], "\\n");
            esc_pos += 2;
        } else if((data[data_pos] < 32) || (data[data_pos] == '\\')) {
            sprintf((char *)&esc[esc_pos], "\\u%04x", (unsigned char)data[data_pos]);
            esc_pos += 6;
        } else {
            esc[esc_pos] = data[data_pos];
            esc_pos++;
        }
    }
    *data_size = esc_pos;
    return esc;
}

static FILE *open_file(const char *dir, const char *file)
{
    FILE *fd;
    char *fullname;

    asprintf(&fullname, "%s/%s", dir, file);

    if((fd = fopen(fullname, "r")) == NULL) {
        perror(file);
        free(fullname);
        exit(EXIT_FAILURE);
    }
    free(fullname);
    return fd;
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
        fprintf(stderr, "Syntax error\n\n");
        fprintf(stderr, "sudo2asciinema <dir>\n");
        exit(EXIT_FAILURE);
    }

    char *basedir = argv[1];
    FILE *fdlog = open_file(basedir, "log");
    FILE *fdstderr = open_file(basedir, "stderr");
    FILE *fdstdout = open_file(basedir, "stdout");
    FILE *fdtiming = open_file(basedir, "timing");
    FILE *fdttyout = open_file(basedir, "ttyout");

    char timestamp[64], user[64], group[64], terminal[64];
    char home[64];
    char command[64];
    char *line;
    size_t size;

    // get timestamp, user, group and terminal from log
    line = NULL, size = 0;
    getline(&line, &size, fdlog);
    sscanf(line, "%[^:]:%[^:]:%[^:]::%[^\n]\n", (char *)&timestamp, (char *)&user, (char *)&group, (char *)&terminal);
    free(line);

    // get home from log
    line = NULL, size = 0;
    getline(&line, &size, fdlog);
    sscanf(line, "%[^\n]", (char *)&home);
    free(line);

    // get command from log
    line = NULL, size = 0;
    getline(&line, &size, fdlog);
    sscanf(line, "%[^\n]", (char *)&command);
    free(line);

    // print header
    printf("{\n");
    printf("  \"version\": %u,\n", 1);
    printf("  \"width\": %u,\n", 89);
    printf("  \"height\": %u,\n", 26);
    printf("  \"duration\": %f,\n", 27.221634);
    printf("  \"command\": null,\n");
    printf("  \"title\": null,\n");
    printf("  \"start\": %s,\n", timestamp);
    printf("  \"user\": \"%s\",\n", user);
    printf("  \"group\": \"%s\",\n", group);
    printf("  \"terminal\": \"%s\",\n", terminal);
    printf("  \"home\": \"%s\",\n", home);
    printf("  \"command\": \"%s\",\n", command);
    printf("  \"env\": {\n");
    printf("    \"TERM\": \"xterm-256color\",\n");
    printf("    \"SHELL\": \"/bin/bash\"\n");
    printf("  },\n");
    printf("  \"stdout\": [\n");

    // get stream chunk
    int op;
    float duration;
    int bytes;
    unsigned char *escape_data;
    unsigned char *data;
    int first = 1;
    for(;;) {
        line = NULL, size = 0;
        if(getline(&line, &size, fdtiming) == -1) {
            free(line);
            break;
        }
        sscanf(line, "%u %f %u", &op, &duration, &bytes);
        free(line);
        if(first) first = 0;
        else printf("    ],\n");
        printf("    [\n");
        printf("      %f,\n", duration);

        data = malloc(bytes);
        if(fread(data, bytes, 1, fdttyout) != 1) {
            fprintf(stderr, "Missing data in fdttyout\n");
            exit(EXIT_FAILURE);
        }
        escape_data = escape(data, &bytes);
        free(data);
        printf("      \"%*s\"\n", bytes, escape_data);

        free(escape_data);
    }
    // Last item
    printf("    ]\n");

    printf("  ]\n");
    printf("}\n");

    fclose(fdlog);
    fclose(fdstderr);
    fclose(fdstdout);
    fclose(fdtiming);
    fclose(fdttyout);
}
