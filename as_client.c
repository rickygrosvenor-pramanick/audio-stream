/*****************************************************************************/
/*                       CSC209-24s A4 Audio Stream                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/
#include "as_client.h"


static int connect_to_server(int port, const char *hostname) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("connect_to_server");
        return -1;
    }

    struct sockaddr_in addr;

    // Allow sockets across machines.
    addr.sin_family = AF_INET;
    // The port the server will be listening on.
    // htons() converts the port number to network byte order.
    // This is the same as the byte order of the big-endian architecture.
    addr.sin_port = htons(port);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(addr.sin_zero), 0, 8);

    // Lookup host IP address.
    struct hostent *hp = gethostbyname(hostname);
    if (hp == NULL) {
        ERR_PRINT("Unknown host: %s\n", hostname);
        return -1;
    }

    addr.sin_addr = *((struct in_addr *) hp->h_addr);

    // Request connection to server.
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return -1;
    }

    return sockfd;
}


/*
** Helper for: list_request
** This function reads from the socket until it finds a network newline.
** This is processed as a list response for a single library file,
** of the form:
**                   <index>:<filename>\r\n
**
** returns index on success, -1 on error
** filename is a heap allocated string pointing to the parsed filename
*/
static int get_next_filename(int sockfd, char **filename) {
    static int bytes_in_buffer = 0;
    static char buf[RESPONSE_BUFFER_SIZE];

    while((*filename = find_network_newline(buf, &bytes_in_buffer)) == NULL) {
        int num = read(sockfd, buf + bytes_in_buffer,
                       RESPONSE_BUFFER_SIZE - bytes_in_buffer);
        if (num < 0) {
            perror("list_request");
            return -1;
        }
        bytes_in_buffer += num;
        if (bytes_in_buffer == RESPONSE_BUFFER_SIZE) {
            ERR_PRINT("Response buffer filled without finding file\n");
            ERR_PRINT("Bleeding data, this shouldn't happen, but not giving up\n");
            memmove(buf, buf + BUFFER_BLEED_OFF, RESPONSE_BUFFER_SIZE - BUFFER_BLEED_OFF);
        }
    }

    char *parse_ptr = strtok(*filename, ":");
    int index = strtol(parse_ptr, NULL, 10);
    parse_ptr = strtok(NULL, ":");
    // moves the filename to the start of the string (overwriting the index)
    memmove(*filename, parse_ptr, strlen(parse_ptr) + 1);

    return index;
}

/*
** Sends a list request to the server and prints the list of files in the
** library. Also parses the list of files and stores it in the list parameter.
**
** The list of files is stored as a dynamic array of strings. Each string is
** a path to a file in the file library. The indexes of the array correspond
** to the file indexes that can be used to request each file from the server.
**
** You may free and malloc or realloc the library->files array as preferred.
**
** returns the length of the new library on success, -1 on error
*/
// https://piazza.com/class/lr04m5y3web1yr/post/3004
int list_request(int sockfd, Library *library) {
    // Send list request to the server
    const char *list_request_msg = "LIST\r\n";
    if (write_precisely(sockfd, list_request_msg, 6 * sizeof(char)) != 6) {
        perror("list_request: write");
        return -1;
    }

    // Initialize the library
    _free_library(library);
    library->num_files = 0;
    library->files = NULL;

    int num_files = 0;
    // Receive and process response from the server
    while (1) {
        char *filename;
        int index = get_next_filename(sockfd, &filename);
        if (index == -1) {
            perror("list_request: get_next_filename");
            break;
        }
        // Store filename in library object
        if (num_files == 0) {
            library->files = malloc((index + 1) * sizeof(char *));
            if (library->files == NULL) {
                perror("list_request: realloc");
                return -1;
            }
        }
        library->files[index] = strdup(filename);
        if (library->files[index] == NULL) {
            perror("list_request: strdup");
            return -1;
        }
        num_files++;
        if (index == 0) {
            break;
        }
    }
    library->num_files = num_files;
    for (int i = 0; i < num_files; i++) {
        printf("%d: %s\n", i, library->files[i]);
    }
    return library->num_files;
}

/*
** Get the permission of the library directory. If the library
** directory does not exist, this function shall create it.
**
** library_dir: the path of the directory storing the audio files
** perpt:       an output parameter for storing the permission of the
**              library directory.
**
** returns 0 on success, -1 on error
**
** References:
 * https://stackoverflow.com/questions/12510874/how-can-i-check-if-a-directory-exists
 * https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c
*/
static int get_library_dir_permission(const char *library_dir, mode_t * perpt) {
    struct stat dir_stat;
    if (stat(library_dir, &dir_stat) == 0) {
        // Directory exists, store its permission
        *perpt = dir_stat.st_mode;
        return 0;
    } else {
        // Directory doesn't exist, create it with permission 0700
        if (mkdir(library_dir, 0700) == 0) {
            // Directory created successfully, set permission to 0700
            *perpt = 0700;
            return 0;
        } else {
            // Error creating directory
            perror("mkdir");
            return -1;
        }
    }
}

/*
** Creates any directories needed within the library dir so that the file can be
** written to the correct destination. All directories will inherit the permissions
** of the library_dir.
**
** This function is recursive, and will create all directories needed to reach the
** file in destination.
**
** Destination shall be a path without a leading /
**
** library_dir can be an absolute or relative path, and can optionally end with a '/'
**
*/
static void create_missing_directories(const char *destination, const char *library_dir) {
    // get the permissions of the library dir
    mode_t permissions;
    if (get_library_dir_permission(library_dir, &permissions) == -1) {
        exit(1);
    }

    char *str_de_tokville = strdup(destination);
    if (str_de_tokville == NULL) {
        perror("create_missing_directories");
        return;
    }

    char *before_filename = strrchr(str_de_tokville, '/');
    if (!before_filename){
        goto free_tokville;
    }

    char *path = malloc(strlen(library_dir) + strlen(destination) + 2);
    if (path == NULL) {
        goto free_tokville;
    } *path = '\0';

    char *dir = strtok(str_de_tokville, "/");
    if (dir == NULL){
        goto free_path;
    }
    strcpy(path, library_dir);
    if (path[strlen(path) - 1] != '/') {
        strcat(path, "/");
    }
    strcat(path, dir);

    while (dir != NULL && dir != before_filename + 1) {
#ifdef DEBUG
        printf("Creating directory %s\n", path);
#endif
        if (mkdir(path, permissions) == -1) {
            if (errno != EEXIST) {
                perror("create_missing_directories");
                goto free_path;
            }
        }
        dir = strtok(NULL, "/");
        if (dir != NULL) {
            strcat(path, "/");
            strcat(path, dir);
        }
    }
    free_path:
    free(path);
    free_tokville:
    free(str_de_tokville);
}


/*
** Helper for: get_file_request
*/
static int file_index_to_fd(uint32_t file_index, const Library * library){
    create_missing_directories(library->files[file_index], library->path);

    char *filepath = _join_path(library->path, library->files[file_index]);
    if (filepath == NULL) {
        return -1;
    }

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
#ifdef DEBUG
    printf("Opened file %s\n", filepath);
#endif
    free(filepath);
    if (fd < 0 ) {
        perror("file_index_to_fd");
        return -1;
    }

    return fd;
}


int get_file_request(int sockfd, uint32_t file_index, const Library * library){
#ifdef DEBUG
    printf("Getting file %s\n", library->files[file_index]);
#endif

    int file_dest_fd = file_index_to_fd(file_index, library);
    if (file_dest_fd == -1) {
        return -1;
    }

    int result = send_and_process_stream_request(sockfd, file_index, -1, file_dest_fd);
    if (result == -1) {
        return -1;
    }

    return 0;
}


int start_audio_player_process(int *audio_out_fd) {
    // Create pipe for communication with the child process
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) { // Child process
        // Close the write end of the pipe
        close(pipefd[1]);

        // Redirect stdin to the read end of the pipe
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
        }
        close(pipefd[0]);

        // Execute the audio player process
        char *args[] = AUDIO_PLAYER_ARGS;
        execvp(AUDIO_PLAYER, args);

        // execvp() returns only if an error occurs
        perror("execvp");
        exit(EXIT_FAILURE);
    } else { // Parent process
        // Close the read end of the pipe
        close(pipefd[0]);

        // Pass the file descriptor of the write end of the pipe to the caller
        *audio_out_fd = pipefd[1];

        // Sleep for a while to allow the audio player process to boot up
        sleep(AUDIO_PLAYER_BOOT_DELAY);

        return pid; // Return the PID of the child process to the parent
    }
}


static void _wait_on_audio_player(int audio_player_pid) {
    int status;
    if (waitpid(audio_player_pid, &status, 0) == -1) {
        perror("_wait_on_audio_player");
        return;
    }
    if (WIFEXITED(status)) {
        fprintf(stderr, "Audio player exited with status %d\n", WEXITSTATUS(status));
    } else {
        printf("Audio player exited abnormally\n");
    }
}


int stream_request(int sockfd, uint32_t file_index) {
    int audio_out_fd;
    int audio_player_pid = start_audio_player_process(&audio_out_fd);

    int result = send_and_process_stream_request(sockfd, file_index, audio_out_fd, -1);
    if (result == -1) {
        ERR_PRINT("stream_request: send_and_process_stream_request failed\n");
        return -1;
    }

    _wait_on_audio_player(audio_player_pid);

    return 0;
}


int stream_and_get_request(int sockfd, uint32_t file_index, const Library * library) {
    int audio_out_fd;
    int audio_player_pid = start_audio_player_process(&audio_out_fd);

#ifdef DEBUG
    printf("Getting file %s\n", library->files[file_index]);
#endif

    int file_dest_fd = file_index_to_fd(file_index, library);
    if (file_dest_fd == -1) {
        ERR_PRINT("stream_and_get_request: file_index_to_fd failed\n");
        return -1;
    }

    int result = send_and_process_stream_request(sockfd, file_index,
                                                 audio_out_fd, file_dest_fd);
    if (result == -1) {
        ERR_PRINT("stream_and_get_request: send_and_process_stream_request failed\n");
        return -1;
    }

    _wait_on_audio_player(audio_player_pid);

    return 0;
}

int send_and_process_stream_request(int sockfd, uint32_t file_index,
                                    int audio_out_fd, int file_dest_fd) {
    if (audio_out_fd < 0 && file_dest_fd < 0) {
        fprintf(stderr, "Invalid file descriptors\n");
        return -1;
    }

    // Write Stream Request to Socket
    char stream_request_msg[12];
    memcpy(stream_request_msg, "STREAM\r\n", 8);
    uint32_t network_file_index = htonl(file_index);
    memcpy(stream_request_msg + 8, &network_file_index, sizeof(uint32_t));

    if (write_precisely(sockfd, stream_request_msg, 12) != 12) {
        return -1;
    }

    // Read In the File Size
    int file_size;
    if ((read_precisely(sockfd, &file_size, sizeof(int))) < 0) {
        return -1;
    }
    file_size = ntohl(file_size);

    // Create fixed-size buffer to read from the socket
    int bytes_to_read = file_size;
    u_int8_t fixed_buffer[NETWORK_PRE_DYNAMIC_BUFF_SIZE];
    u_int8_t *dynamic_buffer = NULL;
    size_t dynamic_buffer_size = 0;

    int audio_read_start_idx = 0;
    int file_read_start_idx = 0;

    int audio_out_written_count = 0;
    int file_dest_written_count = 0;

    // Set Parameters for select syscall
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = SELECT_TIMEOUT_SEC;
    timeout.tv_usec = SELECT_TIMEOUT_USEC;

    // numfd parameter in the select syscall is the value of the highest file descriptor in the set + 1.
    int numfd;
    if (sockfd > audio_out_fd && sockfd > file_dest_fd) {
        numfd = sockfd + 1;
    } else if (audio_out_fd > sockfd && audio_out_fd > file_dest_fd) {
        numfd = audio_out_fd + 1;
    } else {
        numfd = file_dest_fd + 1;
    }

    while (1) {
        // Set up a copy of read_fds as select mutates the read_fds set.
        // This ensures only a copy is being mutated and not the actual set
        fd_set curr_read_fd_sets = read_fds;

        // Our select call does not use write_fds as write calls are not blocked as there is always something to read
        // from the fixed buffer (as we select on the read_fds to ensure that read is never blocked.
        if ((select(numfd, &curr_read_fd_sets, NULL, NULL, &timeout)) < 0) {
            perror("select");
            exit(1);
        }

        if (FD_ISSET(sockfd, &curr_read_fd_sets)) {
            int r;
            if ((r = read(sockfd, fixed_buffer, sizeof(u_int8_t) * NETWORK_PRE_DYNAMIC_BUFF_SIZE)) < 0) {
                // Read from socket to the fixed_buffer
                // Using read instead of read_precisely is better in this case as it does not block
                // until it has read NETWORK_PRE_DYNAMIC_BUFF_SIZE bytes (it reads as many as available)
                perror("read");
                exit(1);
            }

            dynamic_buffer = realloc(dynamic_buffer, dynamic_buffer_size + r);
            u_int8_t *write_to_dynamic_buffer_from = dynamic_buffer + dynamic_buffer_size;
            dynamic_buffer_size += r;
            memcpy(write_to_dynamic_buffer_from, fixed_buffer, r);
            bytes_to_read = bytes_to_read - r;
        }

        int audio_bytes_written = 0;
        int audio_bytes_to_write;
        int file_bytes_written = 0;
        int file_bytes_to_write;

        if (audio_out_fd > -1 && file_dest_fd > -1 && dynamic_buffer != NULL) {
            int written_differential_audio = audio_out_written_count - file_dest_written_count;
            int written_differential_file = file_dest_written_count - audio_out_written_count;
            if (written_differential_audio > 0) {
                audio_read_start_idx = written_differential_audio;
            }
            if (written_differential_file > 0) {
                file_read_start_idx = written_differential_file;
            }
            audio_bytes_to_write = dynamic_buffer_size - audio_read_start_idx;
            if ((audio_bytes_written = write(audio_out_fd, dynamic_buffer + audio_read_start_idx, audio_bytes_to_write)) < 0) {
                perror("write");
                exit(1);
            }
            audio_out_written_count += audio_bytes_written;
            file_bytes_to_write = dynamic_buffer_size - file_read_start_idx;
            if ((file_bytes_written = write(file_dest_fd, dynamic_buffer + file_read_start_idx, file_bytes_to_write)) < 0) {
                perror("write");
                exit(1);
            }
            file_dest_written_count += file_bytes_written;
        } else if (audio_out_fd > -1 && dynamic_buffer != NULL) {
            audio_bytes_to_write = dynamic_buffer_size - audio_read_start_idx;
            if ((audio_bytes_written = write(audio_out_fd, dynamic_buffer + audio_read_start_idx, audio_bytes_to_write)) < 0) {
                perror("write");
                exit(1);
            }
            audio_out_written_count += audio_bytes_written;
        } else if (file_dest_fd > -1  && dynamic_buffer != NULL) {
            file_bytes_to_write = dynamic_buffer_size - file_read_start_idx;
            if ((file_bytes_written = write(file_dest_fd, dynamic_buffer + file_read_start_idx, file_bytes_to_write)) < 0) {
                perror("write");
                exit(1);
            }
            file_dest_written_count += file_bytes_written;
        }

        int lowest_bytes_written = 0;
        if (file_dest_fd > -1 && audio_out_fd > -1 && dynamic_buffer != NULL) {
            if (file_read_start_idx + file_bytes_written < audio_read_start_idx + audio_bytes_written) {
                lowest_bytes_written = file_read_start_idx + file_bytes_written;
            } else {
                lowest_bytes_written = audio_read_start_idx + audio_bytes_written;
            }
        } else if (file_dest_fd < 0 && dynamic_buffer != NULL) {
            lowest_bytes_written = audio_bytes_written;
        } else if (audio_out_fd < 0 && dynamic_buffer != NULL) {
            lowest_bytes_written = file_bytes_written;
        }

        if (dynamic_buffer != NULL) {
            dynamic_buffer_size = dynamic_buffer_size - lowest_bytes_written;
            u_int8_t dynamic_buffer_stack[dynamic_buffer_size];
            memcpy(dynamic_buffer_stack, dynamic_buffer + lowest_bytes_written, dynamic_buffer_size);
            dynamic_buffer = realloc(dynamic_buffer, sizeof(u_int8_t) * dynamic_buffer_size);
            memset(dynamic_buffer, '\0', dynamic_buffer_size);
            memcpy(dynamic_buffer, dynamic_buffer_stack, dynamic_buffer_size);
        }

        if (bytes_to_read == 0 || file_dest_written_count >= file_size || audio_out_written_count >= file_size) {
            break;
        }
    }

    if (bytes_to_read == 0) {
        free(dynamic_buffer);
        close(audio_out_fd);
        close(file_dest_fd);
    }

    return 0;
}


static void _print_shell_help(){
    printf("Commands:\n");
    printf("  list: List the files in the library\n");
    printf("  get <file_index>: Get a file from the library\n");
    printf("  stream <file_index>: Stream a file from the library (without saving it)\n");
    printf("  stream+ <file_index>: Stream a file from the library\n");
    printf("                        and save it to the local library\n");
    printf("  help: Display this help message\n");
    printf("  quit: Quit the client\n");
}


/*
** Shell to handle the client options
** ----------------------------------
** This function is a mini shell to handle the client options. It prompts the
** user for a command and then calls the appropriate function to handle the
** command. The user can enter the following commands:
** - "list" to list the files in the library
** - "get <file_index>" to get a file from the library
** - "stream <file_index>" to stream a file from the library (without saving it)
** - "stream+ <file_index>" to stream a file from the library and save it to the local library
** - "help" to display the help message
** - "quit" to quit the client
*/
static int client_shell(int sockfd, const char *library_directory) {
    char buffer[REQUEST_BUFFER_SIZE];
    char *command;
    int file_index;

    Library library = {"client", library_directory, NULL, 0};

    while (1) {
        if (library.files == 0) {
            printf("Server library is empty or not retrieved yet\n");
        }

        printf("Enter a command: ");
        if (fgets(buffer, REQUEST_BUFFER_SIZE, stdin) == NULL) {
            perror("client_shell");
            goto error;
        }

        command = strtok(buffer, " \n");
        if (command == NULL) {
            continue;
        }

        // List Request -- list the files in the library
        if (strcmp(command, CMD_LIST) == 0) {
            if (list_request(sockfd, &library) == -1) {
                goto error;
            }


            // Get Request -- get a file from the library
        } else if (strcmp(command, CMD_GET) == 0) {
            char *file_index_str = strtok(NULL, " \n");
            if (file_index_str == NULL) {
                printf("Usage: get <file_index>\n");
                continue;
            }
            file_index = strtol(file_index_str, NULL, 10);
            if (file_index < 0 || file_index >= library.num_files) {
                printf("Invalid file index\n");
                continue;
            }

            if (get_file_request(sockfd, file_index, &library) == -1) {
                goto error;
            }

            // Stream Request -- stream a file from the library (without saving it)
        } else if (strcmp(command, CMD_STREAM) == 0) {
            char *file_index_str = strtok(NULL, " \n");
            if (file_index_str == NULL) {
                printf("Usage: stream <file_index>\n");
                continue;
            }
            file_index = strtol(file_index_str, NULL, 10);
            if (file_index < 0 || file_index >= library.num_files) {
                printf("Invalid file index\n");
                continue;
            }

            if (stream_request(sockfd, file_index) == -1) {
                goto error;
            }

            // Stream and Get Request -- stream a file from the library and save it to the local library
        } else if (strcmp(command, CMD_STREAM_AND_GET) == 0) {
            char *file_index_str = strtok(NULL, " \n");
            if (file_index_str == NULL) {
                printf("Usage: stream+ <file_index>\n");
                continue;
            }
            file_index = strtol(file_index_str, NULL, 10);
            if (file_index < 0 || file_index >= library.num_files) {
                printf("Invalid file index\n");
                continue;
            }

            if (stream_and_get_request(sockfd, file_index, &library) == -1) {
                goto error;
            }

        } else if (strcmp(command, CMD_HELP) == 0) {
            _print_shell_help();

        } else if (strcmp(command, CMD_QUIT) == 0) {
            printf("Quitting shell\n");
            break;

        } else {
            printf("Invalid command\n");
        }
    }

    _free_library(&library);
    return 0;
    error:
    _free_library(&library);
    return -1;
}


static void print_usage() {
    printf("Usage: as_client [-h] [-a NETWORK_ADDRESS] [-p PORT] [-l LIBRARY_DIRECTORY]\n");
    printf("  -h: Print this help message\n");
    printf("  -a NETWORK_ADDRESS: Connect to server at NETWORK_ADDRESS (default 'localhost')\n");
    printf("  -p  Port to listen on (default: " XSTR(DEFAULT_PORT) ")\n");
    printf("  -l LIBRARY_DIRECTORY: Use LIBRARY_DIRECTORY as the library directory (default 'as-library')\n");
}


int main(int argc, char * const *argv) {
    int opt;
    int port = DEFAULT_PORT;
    const char *hostname = "localhost";
    const char *library_directory = "saved";

    while ((opt = getopt(argc, argv, "ha:p:l:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'a':
                hostname = optarg;
                break;
            case 'p':
                port = strtol(optarg, NULL, 10);
                if (port < 0 || port > 65535) {
                    ERR_PRINT("Invalid port number %d\n", port);
                    return 1;
                }
                break;
            case 'l':
                library_directory = optarg;
                break;
            default:
                print_usage();
                return 1;
        }
    }

    printf("Connecting to server at %s:%d, using library in %s\n",
           hostname, port, library_directory);

    int sockfd = connect_to_server(port, hostname);
    if (sockfd == -1) {
        return -1;
    }

    int result = client_shell(sockfd, library_directory);
    if (result == -1) {
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}
