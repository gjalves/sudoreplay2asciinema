#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

static unsigned char *escape(unsigned char *data, int *data_size)
{
    int esc_pos, data_pos;
    ssize_t esc_size = 4096;
    unsigned char *esc = malloc(esc_size);
    u_int32_t unicode;
    esc[0] = 0;

    for(esc_pos = data_pos = 0; data_pos < *data_size; data_pos++) {
        if((esc_pos + 16) > esc_size) {
            esc_size += 4096;
            esc = realloc(esc, esc_size);
        }
        if(data[data_pos] == '\r') {
            sprintf((char *)&esc[esc_pos], "\\r");
            esc_pos += 2;
        } else if(data[data_pos] == '\n') {
            sprintf((char *)&esc[esc_pos], "\\n");
            esc_pos += 2;
        } else if(data[data_pos] == '\"') {
            sprintf((char *)&esc[esc_pos], "\\\"");
            esc_pos += 2;
        } else if((data[data_pos] < 32) || (data[data_pos] == '\\')) {
            sprintf((char *)&esc[esc_pos], "\\u%04x", (unsigned char)data[data_pos]);
            esc_pos += 6;
        } else if((data[data_pos] & 248 == 240) && ((*data_size - data_pos) > 4)) {
            // Unicode 4 bytes
            unicode = ((data[data_pos] & 0x07) << 18) + ((data[data_pos + 2] & 0x3F) << 12) + ((data[data_pos + 2] & 0x3F) << 6) + (data[data_pos + 3] & 0x3F);
            sprintf((char *)&esc[esc_pos], "\\u%04x", unicode);
            data_pos += 3;
            esc_pos += 6;
        } else if((data[data_pos] & 248 == 224) && ((*data_size - data_pos) > 3)) {
            // Unicode 3 bytes
            unicode = ((data[data_pos] & 0x0F) << 12) + ((data[data_pos + 1] & 0x3F) << 6) + (data[data_pos + 2] & 0x3F);
            sprintf((char *)&esc[esc_pos], "\\u%04x", unicode);
            data_pos += 2;
            esc_pos += 6;
        } else if((data[data_pos] & 248 == 192) && ((*data_size - data_pos) > 2)) {
            // Unicode 2 bytes
            unicode = ((data[data_pos] & 0x1F) << 6) + (data[data_pos + 1] & 0x3F);
            sprintf((char *)&esc[esc_pos], "\\u%04x", unicode);
            data_pos += 1;
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

static gzFile gz_open_file(const char *dir, const char *file)
{
    gzFile fd;
    char *fullname;

    asprintf(&fullname, "%s/%s", dir, file);

    if((fd = gzopen(fullname, "r")) == NULL) {
        perror(file);
        free(fullname);
        exit(EXIT_FAILURE);
    }
    free(fullname);
    return fd;
}

int main(int argc, char *argv[])
{
    FILE *output;
    int gz = 1;

    if(argc < 2) {
        fprintf(stderr, "Syntax error\n\n");
        fprintf(stderr, "sudo2asciinema <dir> [output]\n");
        exit(EXIT_FAILURE);
    }

    if(argc < 3) {
        output = stdout;
    } else {
        if((output = fopen(argv[2], "w+")) == NULL) {
            perror(argv[2]);
            exit(EXIT_FAILURE);
        }
    }

    char *basedir = argv[1];
    FILE *fdlog = open_file(basedir, "log");
    FILE *fdstderr = open_file(basedir, "stderr");
    FILE *fdstdout = open_file(basedir, "stdout");
    FILE *fdtiming;
    FILE *fdttyout;

    gzFile fdgzttyout;
    gzFile fdgztiming;

    if(gz) {
        fdgzttyout = gz_open_file(basedir, "ttyout");
        fdgztiming = gz_open_file(basedir, "timing");
    } else {
        fdttyout = open_file(basedir, "ttyout");
        fdtiming = open_file(basedir, "timing");
    }

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
    fprintf(output, "{\n");
    fprintf(output, "  \"version\": %u,\n", 1);
    fprintf(output, "  \"width\": %u,\n", 89);
    fprintf(output, "  \"height\": %u,\n", 26);
    fprintf(output, "  \"duration\": %f,\n", 27.221634);
    fprintf(output, "  \"command\": null,\n");
    fprintf(output, "  \"title\": null,\n");
    fprintf(output, "  \"start\": %s,\n", timestamp);
    fprintf(output, "  \"user\": \"%s\",\n", user);
    fprintf(output, "  \"group\": \"%s\",\n", group);
    fprintf(output, "  \"terminal\": \"%s\",\n", terminal);
    fprintf(output, "  \"home\": \"%s\",\n", home);
    fprintf(output, "  \"command\": \"%s\",\n", command);
    fprintf(output, "  \"env\": {\n");
    fprintf(output, "    \"TERM\": \"xterm-256color\",\n");
    fprintf(output, "    \"SHELL\": \"/bin/bash\"\n");
    fprintf(output, "  },\n");
    fprintf(output, "  \"stdout\": [\n");

    // get stream chunk
    int op;
    float duration;
    int bytes;
    unsigned char *escape_data;
    unsigned char *data;
    int first = 1;
    for(;;) {
        line = NULL, size = 0;

        if(gz) {
            line = malloc(1024);
            if(gzgets(fdgztiming, line, 1024) == NULL) {
                free(line);
                break;
            }
        } else {
            if(getline(&line, &size, fdtiming) == -1) {
                free(line);
                break;
            }
        }
        sscanf(line, "%u %f %u", &op, &duration, &bytes);
        free(line);
        if(first) first = 0;
        else fprintf(output, "    ],\n");
        fprintf(output, "    [\n");
        fprintf(output, "      %f,\n", duration);

        // Read data from ttyout
        data = malloc(bytes);
        if(gz) {
            if(gzfread(data, bytes, 1, fdgzttyout) != 1) {
                fprintf(stderr, "Missing data in fdttyout\n");
                exit(EXIT_FAILURE);
            }
        } else {
            if(fread(data, bytes, 1, fdttyout) != 1) {
                fprintf(stderr, "Missing data in fdttyout\n");
                exit(EXIT_FAILURE);
            }
        }
        escape_data = escape(data, &bytes);
        free(data);
        fprintf(output, "      \"");
        fwrite(escape_data, bytes, 1, output);
        fprintf(output, "\"\n");

        free(escape_data);
    }
    // Last item
    fprintf(output, "    ]\n");

    fprintf(output, "  ]\n");
    fprintf(output, "}\n");

    fclose(fdlog);
    fclose(fdstderr);
    fclose(fdstdout);
    if(gz) {
        gzclose(fdgztiming);
        gzclose(fdgzttyout);
    } else {
        fclose(fdtiming);
        fclose(fdttyout);
    }
    fclose(output);
}
