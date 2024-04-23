/*****************************************************************************/
/*                       CSC209-24s A4 Audio Stream                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/
#include "libas.h"


void _free_library(Library *library){
    if (library == NULL) return;
    for (int i = 0; i < library->num_files; i++) {
        free(library->files[i]);
    }
    if (library->files != NULL) {
        free(library->files);
    }
    library->files = NULL;
    library->num_files = 0;
}


char *_join_path(const char *path1, const char *path2) {
    int path_len_1 = strlen(path1);
    int path_len_2 = strlen(path2);

    // +2 for null char and optional /
    char *joined = (char *)malloc(path_len_1 + path_len_2 + 2);
    if (joined == NULL) {
        perror("_join_path");
        return NULL;
    }
    strcpy(joined, path1);

    if (path_len_1 && joined[path_len_1 - 1] != '/') {
        strcat(joined, "/");
    }
    strcat(joined, path2);
    return joined;
}


char *find_network_newline(char *buf, int *inbuf) {
    for (int i = 0; i < *inbuf - 1; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            buf[i] = '\0';
            char *ret = strdup(buf);
            if (ret == NULL) {
                perror("find_network_newline: strdup");
                exit(-1);
            }
            *inbuf -= i + 2;
            memmove(buf, buf + i + 2, *inbuf);
            return ret;
        }
    }
    return NULL;
}


int read_precisely(int fd, void *buf, size_t count) {
    int bytes_read = 0;
    while (bytes_read < count) {
        int ret = read(fd, buf + bytes_read, count - bytes_read);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            ERR_PRINT("read_precisely: read");
            return -1;
        }
        if (ret == 0) {
            ERR_PRINT("read_precisely: read: Unexpected EOF");
            return -1;
        }
        bytes_read += ret;
    }
    #ifdef DEBUG
    printf("read_precisely: read %d bytes\n", bytes_read);
    #endif
    return bytes_read;
}


int write_precisely(int fd, const void *buf, size_t count) {
    int bytes_written = 0;
    while (bytes_written < count) {
        int ret = write(fd, buf + bytes_written, count - bytes_written);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            ERR_PRINT("write_precisely: write");
            return -1;
        }
        bytes_written += ret;
    }
    #ifdef DEBUG
    printf("write_precisely: wrote %d bytes\n", bytes_written);
    #endif
    return bytes_written;
}
