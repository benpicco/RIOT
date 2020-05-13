#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINE_BUFFER 128

static int _getline(char* buffer, size_t size, int fd) {

    int in = -1;
    while (read(fd, buffer, 1) > 0 && --size) {
        ++in;

        if (*buffer == '\n') {
            *buffer = 0;
            break;
        }

        ++buffer;
    }

    return in;
}

static char* concat_args(int argc, char** argv) {
    char *concat = argv[0];

    char *arg = concat;
    while (--argc) {
        arg += strlen(arg);
        *arg = ' ';
    }

    return concat;
}

static bool insert_line(int fd, const char* text, size_t len, int line) {
    static const char nl = '\n';

    printf("%d\t%s\n", line, text);

    if (fd < 0) { // hack for append
        return true;
    }

    if (write(fd, text, len) != len) {
        return false;
    }

    if (write(fd, &nl, 1) != 1) {
        return false;
    }

    return true;
}

static int copy_file(int fd_old, int fd_new, char mode, int line, const char* text) {
    int current_line = -1;

    char buffer[LINE_BUFFER];
    int len = 0;
    while (len >= 0) {

        len = _getline(buffer, sizeof(buffer), fd_old);
        ++current_line;

        // delete - skip line
        if (mode == 'd' && current_line == line) {
            continue;
        }

        // insert line
        if (mode == 'i' && current_line == line) {
            if (!insert_line(fd_new, text, strlen(text), current_line++))
                return -1;
        }

        // replace line
        if (mode == 'r' && current_line == line) {
            if (!insert_line(fd_new, text, strlen(text), current_line))
                return -1;

            continue;
        }

        if (len >= 0) {
            if (!insert_line(fd_new, buffer, len, current_line))
                return -1;
        }
    }

    return current_line;
}

static int file_append(const char* file, const char* text) {

    int fd;

    if (text) {
        fd = open(file, O_RDWR | O_CREAT, 0644);
    } else {
        fd = open(file, O_RDONLY);
    }

    if (fd < 0) {
        return -1;
    }

    int lines = copy_file(fd, -1, 'p', 0, 0);

    if (text != NULL) {
        insert_line(fd, text, strlen(text), lines);
    }

    close(fd);

    return 0;
}

static int file_edit(const char* file, char mode, int line, const char* text) {

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        printf("can't open %s\n", file);
        return -1;
    }

    char file_tmp[32];
    snprintf(file_tmp, sizeof(file_tmp), "%s.t", file);

    int fd_new = open(file_tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_new < 0) {
        printf("can't open tmp file %s\n", file_tmp);

        close(fd);
        return -1;
    }

    int ret = copy_file(fd, fd_new, mode, line, text);

    close(fd);
    close(fd_new);

    if (ret < 0) {
        printf("error writing file\n");
        return ret;
    }

    rename(file_tmp, file);

    return 0;
}

int _lined(int argc, char** argv) {

    if (argc < 3) {
        printf("usage: %s [file] [cmd] [text]\n", argv[0]);
        return -1;
    }

    int res;
    char* file = argv[1];
    char* cmd = argv[2];
    char* text = argc > 3 ? concat_args(argc - 3, &argv[3]) : NULL;
    int line = atoi(cmd + 1);
    char mode = cmd[0];

    if (text == NULL && (mode == 'i' || mode == 'r' || mode == 'a')) {
        return -1;
    }

    switch (mode) {
    case 'a':  // append
        res = file_append(file, text);
        break;
    case 'p':  // print
        res = file_append(file, NULL);
        break;
    default:
        res = file_edit(file, mode, line, text);
        break;
    }

    if (text) {
        free(text);
    }

    return 0;
}
