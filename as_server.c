/*****************************************************************************/
/*                       CSC209-24s A4 Audio Stream                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/
#include "as_server.h"


int init_server_addr(int port, struct sockaddr_in *addr){
    // Allow sockets across machines.
    addr->sin_family = AF_INET;
    // The port the process will listen on.
    addr->sin_port = htons(port);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(addr->sin_zero), 0, 8);

    // Listen on all network interfaces.
    addr->sin_addr.s_addr = INADDR_ANY;

    return 0;
}

// For set_up_server_socket, use MAX_PENDING for the num_queue argument.
int set_up_server_socket(const struct sockaddr_in *server_options, int num_queue) {
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc < 0) {
        perror("socket");
        exit(1);
    }

    printf("Listen socket created\n");

    // Make sure we can reuse the port immediately after the
    // server terminates. Avoids the "address in use" error
    int on = 1;
    int status = setsockopt(soc, SOL_SOCKET, SO_REUSEADDR,
                            (const char *) &on, sizeof(on));
    if (status < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Associate the process with the address and a port
    if (bind(soc, (struct sockaddr *)server_options, sizeof(*server_options)) < 0) {
        // bind failed; could be because port is in use.
        perror("bind");
        exit(1);
    }

    printf("Socket bound to port %d\n", ntohs(server_options->sin_port));

    // Set up a queue in the kernel to hold pending connections.
    if (listen(soc, num_queue) < 0) {
        // listen failed
        perror("listen");
        exit(1);
    }

    printf("Socket listening for connections\n");

    return soc;
}


ClientSocket accept_connection(int listenfd) {
    ClientSocket client;
    socklen_t addr_size = sizeof(client.addr);
    client.socket = accept(listenfd, (struct sockaddr *)&client.addr,
                               &addr_size);
    if (client.socket < 0) {
        perror("accept_connection: accept");
        exit(-1);
    }

    // print out a message that we got the connection
    printf("Server got a connection from %s, port %d\n",
           inet_ntoa(client.addr.sin_addr), ntohs(client.addr.sin_port));

    return client;
}

// HELPER FOR list_request_response
/**
 * @brief Counts the number of digits in an integer.
 *
 * This function calculates the number of digits in the given integer number.
 * It counts the digits by repeatedly dividing the number by 10 until it reaches 0.
 *
 * @param number The integer number for which digits are to be counted.
 * @return The number of digits in the input integer. Returns 1 if the input number is 0.
 */
int countDigits(int number) {
    int count = 0;

    // Handle the case when the number is 0 separately
    if (number == 0)
        return 1;

    // Count the digits by repeatedly dividing by 10
    while (number != 0) {
        number = number / 10;
        count++;
    }

    return count;
}

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
** References:
* - https://stackoverflow.com/questions/8257714/how-can-i-convert-an-int-to-a-string-in-c
*/
int list_request_response(const ClientSocket * client, const Library *library) {
    int total_len = 0;
    int num_items = library->num_files;
    for (int i = 0; i < num_items - 1; i++) {
        // Adding length for index
        total_len += countDigits(i);
        // Adding length for colon
        total_len++;
        // Adding length for file name
        total_len += LIBRARY_FILENAME_MAX;
        // Adding length for network newline
        total_len += 2;
    }

    int offset = 0;

    char *response = malloc(sizeof(char) * total_len);
    if (response == NULL) {
        perror("Memory allocation error");
        return -1; // Return failure
    }

    for (int i = library->num_files - 1; i >= 0; i--) {
        // Convert index to string
        char index_str[countDigits(i)]; // Assuming index will not exceed 10 digits
        sprintf(index_str, "%d", i);
        strcpy(response + offset, index_str); // Append index
        offset += countDigits(i);
        strcpy(response + offset, ":"); // Append colon
        offset++;
        strcpy(response + offset, library->files[i]); // Append file name
        offset += strlen(library->files[i]);
        strcpy(response + offset, "\r\n"); // Append newline
        offset += 2;
    }
    // Send the response to the client
    if (write_precisely(client->socket, response, offset * sizeof(char)) < 0) {
        perror("write");
        free(response); // Free allocated memory before returning
        return -1; // Return failure
    }
    // Free allocated memory
    free(response);

    return 0;
}


static int _load_file_size_into_buffer(FILE *file, uint8_t *buffer) {
    if (fseek(file, 0, SEEK_END) < 0) {
        ERR_PRINT("Error seeking to end of file\n");
        return -1;
    }
    uint32_t file_size = ftell(file);
    if (fseek(file, 0, SEEK_SET) < 0) {
        ERR_PRINT("Error seeking to start of file\n");
        return -1;
    }
    buffer[0] = (file_size >> 24) & 0xFF;
    buffer[1] = (file_size >> 16) & 0xFF;
    buffer[2] = (file_size >> 8) & 0xFF;
    buffer[3] = file_size & 0xFF;
    return 0;
}

// Function to convert a 4-byte buffer to an integer
/**
 * @brief Converts a 4-byte buffer to a 32-bit unsigned integer.
 *
 * This function takes a 4-byte buffer containing the bytes of a 32-bit unsigned integer
 * and converts it into a 32-bit unsigned integer.
 * The bytes in the buffer are combined into an integer in Big Endian format.
 *
 * @param buffer An array of 4 bytes containing the bytes of the integer to be converted.
 * @return The 32-bit unsigned integer value converted from the byte buffer.
 */
uint32_t convert_buffer_to_int(uint8_t buffer[4]) {
    // Combine the individual bytes into an int (Big Endian - takes care of the ntohl)
    int result = ((int)buffer[0] << 24) | ((int)buffer[1] << 16) | ((int)buffer[2] << 8) | (int)buffer[3];
    return result;
}

/**
 * @brief Returns the minimum of two integers.
 *
 * This function compares two integers and returns the smaller of the two.
 *
 * @param a The first integer to compare.
 * @param b The second integer to compare.
 * @return The smaller of the two input integers. If both are equal, returns either one.
 */
int min(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * @brief Retrieves the size of a file.
 *
 * This function takes a file pointer and retrieves the size of the file associated with it.
 * It achieves this by seeking to the end of the file and then using ftell() to determine the current position,
 * which represents the size of the file. After retrieving the file size, it seeks back to the beginning of the file.
 *
 * @param file A pointer to the FILE structure representing the file for which the size is to be retrieved.
 * @return The size of the file in bytes if successful, or -1 if an error occurs.
 */
int file_size_retrieve(FILE *file) {
    // Retrieve the file size
    if (fseek(file, 0, SEEK_END) < 0) {
        ERR_PRINT("Error seeking to end of file\n");
        return -1;
    }

    int curr_size = ftell(file);
    if (fseek(file, 0, SEEK_SET) < 0) {
        ERR_PRINT("Error seeking to start of file\n");
        return -1;
    }

    return curr_size;

}

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
                            uint8_t *post_req, int num_pr_bytes) {
    if (num_pr_bytes > 4){
        fprintf(stderr, "Error: Invalid number of num_pr_bytes\n");
        return -1;
    }

    // Extract the file index from the next 4 bytes
    uint8_t file_index_buffer[4];
    if (num_pr_bytes == 4) {
        memcpy(&file_index_buffer, post_req, 4);
    } else {
        // if (num_pr_bytes < 4)
        // If not all bytes of file_index are available, read from the socket
        memcpy(&file_index_buffer, post_req, num_pr_bytes);
        int remaining_bytes = 4 - num_pr_bytes;
        int bytes_read = read_precisely(client->socket, &file_index_buffer + num_pr_bytes, remaining_bytes);
        if (bytes_read != remaining_bytes) {
            perror("read");
            return -1;
        }
    }
    // Convert from network byte order to host byte order
    // Use ntohl instead of ntohs as we are converting a 32-bit int
    // Note that the file index needs to be in network byte order, i.e., big endian byte order.

    int file_index = convert_buffer_to_int(file_index_buffer);
//    file_index = ntohl(file_index);

    // Validate File Index
    if (file_index >= library->num_files) {
        fprintf(stderr, "Invalid file index\n");
        return -1;
    }

    // Open the requested file
    char *file_path = _join_path(library->path, library->files[file_index]);
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    // Calculate the file size
    uint8_t file_size_buffer[4];
    if (_load_file_size_into_buffer(file, file_size_buffer) < 0) {
        fclose(file);
        return -1;
    }
    // Send file size to client
    if ((write_precisely(client->socket, file_size_buffer, 4)) < 0) {
        perror("write");
        fclose(file);
        return -1;
    }

    int curr_size = file_size_retrieve(file);
    uint8_t data_chunk_buffer[STREAM_CHUNK_SIZE];
    int curr_read_size = STREAM_CHUNK_SIZE;
    int bytes_read;
    while ((bytes_read = fread(data_chunk_buffer, sizeof(uint8_t), curr_read_size, file)) > 0) {
        if (write_precisely(client->socket, data_chunk_buffer, bytes_read) < 0) {
            perror("write");
            fclose(file);
            return -1;
        }
        curr_size = curr_size - curr_read_size;
        if (curr_size <= 0) {
            break;
        }
        curr_read_size = min(STREAM_CHUNK_SIZE, curr_size);
    }
    // Close the file and return success
    fclose(file);
    free(file_path);
    return 0;
}


static Library make_library(const char *path){
    Library library;
    library.path = path;
    library.num_files = 0;
    library.files = NULL;
    library.name = "server";

    printf("Initializing library\n");
    printf("Library path: %s\n", library.path);

    return library;
}


static void _wait_for_children(pid_t **client_conn_pids, int *num_connected_clients, uint8_t immediate) {
    int status;
    for (int i = 0; i < *num_connected_clients; i++) {
        int options = immediate ? WNOHANG : 0;
        if (waitpid((*client_conn_pids)[i], &status, options) > 0) {
            if (WIFEXITED(status)) {
                printf("Client process %d terminated\n", (*client_conn_pids)[i]);
                if (WEXITSTATUS(status) != 0) {
                    fprintf(stderr, "Client process %d exited with status %d\n",
                            (*client_conn_pids)[i], WEXITSTATUS(status));
                }
            } else {
                fprintf(stderr, "Client process %d terminated abnormally\n",
                        (*client_conn_pids)[i]);
            }

            for (int j = i; j < *num_connected_clients - 1; j++) {
                (*client_conn_pids)[j] = (*client_conn_pids)[j + 1];
            }

            (*num_connected_clients)--;
            *client_conn_pids = (pid_t *)realloc(*client_conn_pids,
                                                 (*num_connected_clients)
                                                 * sizeof(pid_t));
        }
    }
}

/*
** Create a server socket and listen for connections
**
** port: the port number to listen on.
** 
** On success, returns the file descriptor of the socket.
** On failure, return -1.
*/
static int initialize_server_socket(int port) {
    struct sockaddr_in server;
    init_server_addr(port, &server);
    int listen_soc = set_up_server_socket(&server, MAX_PENDING);
    return listen_soc;

}

int run_server(int port, const char *library_directory){
    Library library = make_library(library_directory);
    if (scan_library(&library) < 0) {
        ERR_PRINT("Error scanning library\n");
        return -1;
    }

    int num_connected_clients = 0;
    pid_t *client_conn_pids = NULL;

	int incoming_connections = initialize_server_socket(port);
	if (incoming_connections == -1) {
		return -1;	
	}
	
    int maxfd = incoming_connections;
    fd_set incoming;
    SET_SERVER_FD_SET(incoming, incoming_connections);
    int num_intervals_without_scan = 0;

    while(1) {
        if (num_intervals_without_scan >= LIBRARY_SCAN_INTERVAL) {
            if (scan_library(&library) < 0) {
                fprintf(stderr, "Error scanning library\n");
                return 1;
            }
            num_intervals_without_scan = 0;
        }

        struct timeval select_timeout = SELECT_TIMEOUT;
        if(select(maxfd + 1, &incoming, NULL, NULL, &select_timeout) < 0){
            perror("run_server");
            exit(1);
        }

        if (FD_ISSET(incoming_connections, &incoming)) {
            ClientSocket client_socket = accept_connection(incoming_connections);

            pid_t pid = fork();
            if(pid == -1){
                perror("run_server");
                exit(-1);
            }
            // child process
            if(pid == 0){
                close(incoming_connections);
                free(client_conn_pids);
                int result = handle_client(&client_socket, &library);
                _free_library(&library);
                close(client_socket.socket);
                return result;
            }
            close(client_socket.socket);
            num_connected_clients++;
            client_conn_pids = (pid_t *)realloc(client_conn_pids,
                                               (num_connected_clients)
                                               * sizeof(pid_t));
            client_conn_pids[num_connected_clients - 1] = pid;
        }
        if (FD_ISSET(STDIN_FILENO, &incoming)) {
            if (getchar() == 'q') break;
        }

        num_intervals_without_scan++;
        SET_SERVER_FD_SET(incoming, incoming_connections);

        // Immediate return wait for client processes
        _wait_for_children(&client_conn_pids, &num_connected_clients, 1);
    }

    printf("Quitting server\n");
    close(incoming_connections);
    _wait_for_children(&client_conn_pids, &num_connected_clients, 0);
    _free_library(&library);
    return 0;
}


static uint8_t _is_file_extension_supported(const char *filename){
    static const char *supported_file_exts[] = SUPPORTED_FILE_EXTS;

    for (int i = 0; i < sizeof(supported_file_exts)/sizeof(char *); i++) {
        char *files_ext = strrchr(filename, '.');
        if (files_ext != NULL && strcmp(files_ext, supported_file_exts[i]) == 0) {
            return 1;
        }
    }

    return 0;
}


static int _depth_scan_library(Library *library, char *current_path){

    char *path_in_lib = _join_path(library->path, current_path);
    if (path_in_lib == NULL) {
        return -1;
    }

    DIR *dir = opendir(path_in_lib);
    if (dir == NULL) {
        perror("scan_library");
        return -1;
    }
    free(path_in_lib);

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if ((entry->d_type == DT_REG) &&
            _is_file_extension_supported(entry->d_name)) {
            library->files = (char **)realloc(library->files,
                                              (library->num_files + 1)
                                              * sizeof(char *));
            if (library->files == NULL) {
                perror("_depth_scan_library");
                return -1;
            }

            library->files[library->num_files] = _join_path(current_path, entry->d_name);
            if (library->files[library->num_files] == NULL) {
                perror("scan_library");
                return -1;
            }
            #ifdef DEBUG
            printf("Found file: %s\n", library->files[library->num_files]);
            #endif
            library->num_files++;

        } else if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char *new_path = _join_path(current_path, entry->d_name);
            if (new_path == NULL) {
                return -1;
            }

            #ifdef DEBUG
            printf("Library scan descending into directory: %s\n", new_path);
            #endif

            int ret_code = _depth_scan_library(library, new_path);
            free(new_path);
            if (ret_code < 0) {
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}


// This function is implemented recursively and uses realloc to grow the files array
// as it finds more files in the library. It ignores MAX_FILES.
int scan_library(Library *library) {
    // Maximal flexibility, free the old strings and start again
    // A hash table leveraging inode number would be a better way to do this
    #ifdef DEBUG
    printf("^^^^ ----------------------------------- ^^^^\n");
    printf("Freeing library\n");
    #endif
    _free_library(library);

    #ifdef DEBUG
    printf("Scanning library\n");
    #endif
    int result = _depth_scan_library(library, "");
    #ifdef DEBUG
    printf("vvvv ----------------------------------- vvvv\n");
    #endif
    return result;
}


int handle_client(const ClientSocket * client, Library *library) {
    char *request = NULL;
    uint8_t *request_buffer = (uint8_t *)malloc(REQUEST_BUFFER_SIZE);
    if (request_buffer == NULL) {
        perror("handle_client");
        return 1;
    }
    uint8_t *buff_end = request_buffer;

    int bytes_read = 0;
    int bytes_in_buf = 0;
    while((bytes_read = read(client->socket, buff_end, REQUEST_BUFFER_SIZE - bytes_in_buf)) > 0){
        #ifdef DEBUG
        printf("Read %d bytes from client\n", bytes_read);
        #endif

        bytes_in_buf += bytes_read;

        request = find_network_newline((char *)request_buffer, &bytes_in_buf);

        if (request && strcmp(request, REQUEST_LIST) == 0) {
            if (list_request_response(client, library) < 0) {
                ERR_PRINT("Error handling LIST request\n");
                goto client_error;
            }

        } else if (request && strcmp(request, REQUEST_STREAM) == 0) {
            int num_pr_bytes = MIN(sizeof(uint32_t), (unsigned long)bytes_in_buf);
            if (stream_request_response(client, library, request_buffer, num_pr_bytes) < 0) {
                ERR_PRINT("Error handling STREAM request\n");
                goto client_error;
            }
            bytes_in_buf -= num_pr_bytes;
            memmove(request_buffer, request_buffer + num_pr_bytes, bytes_in_buf);

        } else if (request) {
            ERR_PRINT("Unknown request: %s\n", request);
        }

        free(request); request = NULL;
        buff_end = request_buffer + bytes_in_buf;

    }
    if (bytes_read < 0) {
        perror("handle_client");
        goto client_error;
    }

    printf("Client on %s:%d disconnected\n",
           inet_ntoa(client->addr.sin_addr),
           ntohs(client->addr.sin_port));

    free(request_buffer);
    if (request != NULL) {
        free(request);
    }
    return 0;
client_error:
    free(request_buffer);
    if (request != NULL) {
        free(request);
    }
    return -1;
}


static void print_usage(){
    printf("Usage: as_server [-h] [-p port] [-l library_directory]\n");
    printf("  -h  Print this message\n");
    printf("  -p  Port to listen on (default: " XSTR(DEFAULT_PORT) ")\n");
    printf("  -l  Directory containing the library (default: ./library/)\n");
}


int main(int argc, char * const *argv){
    int opt;
    int port = DEFAULT_PORT;
    const char *library_directory = "library";

    // Check out man 3 getopt for how to use this function
    // The short version: it parses command line options
    // Note that optarg is a global variable declared in getopt.h
    while ((opt = getopt(argc, argv, "hp:l:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                library_directory = optarg;
                break;
            default:
                print_usage();
                return 1;
        }
    }

    printf("Starting server on port %d, serving library in %s\n",
           port, library_directory);

    return run_server(port, library_directory);
}
