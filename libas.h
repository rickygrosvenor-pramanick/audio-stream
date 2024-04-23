#ifndef LIBAS_H_
#define LIBAS_H_
/*****************************************************************************/
/*                       CSC209-24s A4 Audio Stream                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/
#include <unistd.h>

// General stuff
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// Network stuff
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>         /* gethostname */
#include <sys/socket.h>

// File and directory stuff
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

// system stuff
#include <errno.h>
#include <sys/wait.h>

#define ERR_PRINT(...) fprintf(stderr, "ERROR: ");\
    fprintf(stderr, __VA_ARGS__);

// stringification macro functions
#define STR(v) #v
#define XSTR(v) STR(v)

/*
** Constants
** ---------
*/
#define MAX_PATH 256
#define MAX_FILE_NAME 256
// This parameter is a sort of back-up to make sure you finish
// there doesn't need to be a maximum number of files
#define MAX_FILES 1024

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 3456
#endif

// Add to this list to experiment with different file types
// The server will only serve files with these extensions
// This and a lot of the other parameters would be better as
// a configuration file or command line arguments.
#define SUPPORTED_FILE_EXTS {".wav", ".mp3", ".flac", ".ogg", ".m4a"}

#define REQUEST_BUFFER_SIZE 128
#define REQUEST_LIST "LIST"
#define REQUEST_STREAM "STREAM"

#define RESPONSE_BUFFER_SIZE 4 * MAX_FILE_NAME

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define END_OF_MESSAGE_TOKEN "\r\n"


/*
** Library structure
** -----------------
** name: name of the library, arbitrary, may be the name of the directory.
** path: path to the library, absolute or relative, with a trailing slash.
**       Note: this string should not heap-allocated.
** files: List of files in the library as a dynamic array of strings.
**        Each string is a path to a file in the file library. The path is
**        relative to the library's path without a leading slash (heap-allocated).
**        (e.g. "file1.wav", "artist/file2.wav", "artist/album/file3.wav", etc)
** num_files: number of files in the library, and the size of the files array.
 */
typedef struct library {
    char *name;
    const char *path;
    char **files;
    uint32_t num_files;
} Library;


void _free_library(Library *library);


/*
** Joins two paths together, adding a / between them if necessary.
**
** Returns a heap-allocated string with the joined path, or NULL on error.
*/
char *_join_path(const char *path1, const char *path2);

/*
** Finds the first \r\n in the buffer and returns a heap allocated string
** with the data before the \r\n. NULL is returned if no \r\n is found.
**
**
** The bytes through the \r\n are removed from the buffer when found.
*/
char *find_network_newline(char *buf, int *inbuf);

/*
** Blocking read from the file descriptor *exactly* count bytes into the buffer.
** Using as many calls to read as necessary, only returns when count bytes have
** been read, EOF is reached, or an error occurs.
**
** Returns the number of bytes actually read, or -1 on error.
*/
int read_precisely(int fd, void *buf, size_t count);

/*
** Blocking write to the file descriptor *exactly* count bytes from the buffer.
** Using as many calls to write as necessary, only returns when count bytes have
** been written, or an error occurs.
**
** Returns the number of bytes actually written, or -1 on error.
*/
int write_precisely(int fd, const void *buf, size_t count);

#endif // LIBAS_H_
