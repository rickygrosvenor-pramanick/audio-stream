#ifndef AS_SERVER_H_
#define AS_SERVER_H_
/*****************************************************************************/
/*                       CSC209-24s A4 Audio Stream                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/
#include "libas.h"

/*
** Constants
** ---------
*/
#define MAX_PENDING 10
#define STREAM_CHUNK_SIZE 1024

#define SELECT_TIMEOUT_SEC 1
#define SELECT_TIMEOUT_USEC 0
#define SELECT_TIMEOUT {SELECT_TIMEOUT_SEC, SELECT_TIMEOUT_USEC}

#define LIBRARY_FILENAME_MAX 256
#define LIBRARY_SCAN_INTERVAL 60


/*
** Design
** ------
** The server will be a simple command-line program that listens for incoming
** connections on a specified port. When a connection is made, the server will
** fork a child process to handle the client. The child process will exist solely
** to handle this client, and will terminate when the client disconnects.
**
** The server will maintain a library of audio files. The library will be a
** directory on the server's file system. The server will scan the library
** directory at regular intervals to keep the library up to date.
**
** Once a client connects, it can make requests.
** The server will respond to the following requests:
** 1) "LIST" to list the files in the library
**    - The string REQUEST_LIST will be sent to the server, followed by the
**      network newline "\r\n" (2 chars).
**      - The server will respond with a list of files in the library.
**      - see list_request_response for more information
**
** 2) "STREAM" to stream a file from the library
**   - The string REQUEST_STREAM will be sent to the server, followed by the
**     network newline "\r\n" (2 chars).
**   - This will be followed by the index of the file in the library to stream.
**     as a 32-bit integer in network byte order.
**   - The server will respond with:
**     - the file's size followed by the file's data.
**       - see stream_request_response for more information
**
** The client-server connection code is nearly identical to T10, so be sure to take a crack
** at that lab before starting this assignment.
*/


// Convenience struct for clients
typedef struct client_socket {
    int socket;
    struct sockaddr_in addr;
} ClientSocket;


#define SET_SERVER_FD_SET(fd, conn_soc) do { \
    FD_ZERO(&fd); \
    FD_SET(conn_soc, &fd); \
    FD_SET(STDIN_FILENO, &fd); \
} while (0)


// Network Connection functions
/*
** Initialize a sockaddr_in structure for the server to listen on.
** The server will listen on all network interfaces on the specified port.
**
** return 0 on success, -1 on error
*/
int init_server_addr(int port, struct sockaddr_in *server_options);


/*
** Create and set up a socket for this server process to listen on. Does not
** accept any connections yet.
**
** The server will listen on all network interfaces on the specified port. This
** function will use provided sockaddr_in structure to set up the socket, then call
** bind and listen on the socket. If any of these steps fail, the program will
** terminate with an error message.
**
** Return the socket file descriptor, -1 on error
*/
int set_up_server_socket(const struct sockaddr_in *self, int num_queue);


/*
** Wait for and accept a new connection. Return the socket file descriptor for
** the new connection.
**
** If the accept call fails, return -1.
*/
ClientSocket accept_connection(int listenfd);


// Request response functions
/*
** List the files in the library. The list is returned as a single string
** with each file starting with an integer corresponding to it's index in
** the library, in reverse order. A colon seperates the index and the file's
** name. Each entry is separated by a network newline "\r\n" (2 chars).
** Filenames must not contain newline characters.
**
** For example, if the library contains the files "file1.wav",
** "artist/file2.wav", and "artist/album/file3.wav" in this order,
** the data sent to the client will be the following characters:
** "2:artist/album/file3.wav\r\n1:artist/file2.wav\r\n0:file1.wav\r\n"
**
** Notes:
**   -- the null character is not included in the message sent to the client.
**
** return 0 on success, -1 on error
*/
int list_request_response(const ClientSocket * client, const Library *library);


/*
** Stream a file from the library to the client. The file is streamed in chunks
** of a maximum of STREAM_CHUNK_SIZE bytes. The client will be able to request
** a specific file by its index in the library.
**
** The 32-bit unsigned network byte-order integer file_index will be read
** from the client_socket, but will consider num_pr_bytes (must be <= uint32_t)
** from post_req first, then:
**   The stream will be sent in the following format:
**     - the first 4 bytes (32-bits) will be the file size in network byte-order
**     - the rest of the stream will be the file's data written in chunks of
**       STREAM_CHUNK_SIZE bytes, or less if write returns less than STREAM_CHUNK_SIZE,
**       the file is less than STREAM_CHUNK_SIZE bytes, or the last remaining chunk is
**       less than STREAM_CHUNK_SIZE bytes.
**
** If the file is successfully transported to the client over the client_socket,
** return 0. Otherwise, return -1.
 */
int stream_request_response(const ClientSocket * client, const Library *library,
                            uint8_t *post_req, int num_pr_bytes);


// Library functions
/*
** Scan the library directory and (re-)populate the library structure. The library
** structure will be populated with the name of the library, the path to the library,
** and a list of files in the library.
**
** Only SUPPORTED_FILE_EXTS files will be added to the library.
**
** If the library is successfully populated, return 0. Otherwise, return -1.
*/
int scan_library(Library *library);


// Server operation functions
// These leverage all above functions
/*
** Handle a client connection. This function will only be used by child processes
** of the server. It will continue to manage a connected client, for the duration
** that it is connected.
**
** When the client's socket is closed/receives EOF, this process must exit with a
** value of 0. If any errors occur, the process must exit with a non-zero status.
*/
int handle_client(const ClientSocket * client, Library *library);


/*
** Run the server using the specified port and library directory. The server
** will listen for incoming connections and respond to requests from clients in
** an infinite loop. The loop will terminate if an error occurs or the user types
** q + enter in the server's terminal.
**
** All new connections will be accepted and handled in a child process that will
** exclusively run the handle_client function. The server will continue to listen
** for new connections in the parent process.
**
** If the server is successfully set up and running, this function will never
** return. If any errors occur, the server will terminate with an error message.
*/
int run_server(int port, const char *library_directory);

#endif // AS_SERVER_H_
