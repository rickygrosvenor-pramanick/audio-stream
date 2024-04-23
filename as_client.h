#ifndef AS_CLIENT_H_
#define AS_CLIENT_H_
/*****************************************************************************/
/*                       CSC209-24s A4 Audio Stream                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/
#include "libas.h"

/*
** The following constants are used to define a separate process that
** will be used to playback audio data. The process will be started
** with the AUDIO_PLAYER command and the AUDIO_PLAYER_ARGS arguments.
*/

// Keep the last char * NULL as the last arg to use with exec
// uncomment to use mpv
#define AUDIO_PLAYER "mpv"
#define AUDIO_PLAYER_ARGS {AUDIO_PLAYER, "-", NULL}

// uncomment to use the debug streamer
//#define AUDIO_PLAYER "stream_debugger"
//#define AUDIO_PLAYER_ARGS {AUDIO_PLAYER, "-c", "1024", "-f", "stream_dump.wav", NULL}

// takes a while for mpv to start, make sure its ready
#define AUDIO_PLAYER_BOOT_DELAY 2

#define SELECT_TIMEOUT_SEC 1
#define SELECT_TIMEOUT_USEC 0

// Buffer size to receive network data
// before the dynamically changing one
#define NETWORK_PRE_DYNAMIC_BUFF_SIZE 8192

// Student's don't need to change this
#define BUFFER_BLEED_OFF 1

/*
** Client shell commands and constants**
** -----------------------------------
*/
#define CMD_LIST "list"
#define CMD_GET "get"
#define CMD_STREAM "stream"
#define CMD_STREAM_AND_GET "stream+"
#define CMD_QUIT "quit"
#define CMD_HELP "help"


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
int list_request(int sockfd, Library *library);

/*
** Sends a stream request to the server and simply saves the file received
** from the server to the local library directory. The AUDIO_PLAYER is
** not started.
**
** See send_and_process_stream_request for more information. This function
** should invoke send_and_process_stream_request such that the file data
** received is saved to an identical file in the local library_directory.
**
** returns 0 on success, -1 on error
*/
int get_file_request(int sockfd, uint32_t file_index, const Library * library);

/*
** Starts the audio player process and returns the file descriptor of
** the write end of a pipe connected to the audio player's stdin.
**
** The audio player process is started with the AUDIO_PLAYER command and the
** AUDIO_PLAYER_ARGS arguments. The file descriptor to write the audio stream to
** is returned in the audio_out_fd parameter.
**
** returns PID of AUDIO_PLAYER (returns in parent process on success), -1 on error
** child process does not return.
*/
int start_audio_player_process(int *audio_out_fd);

/*
** Sends a stream request to the server and starts the audio player process.
**
** This function leverages send_and_process_stream_request to send the request and
** receive the audio stream from the server.
**
** It also leverages the start_audio_player_process function to start the audio player.
**
** returns 0 on success, -1 on error
*/
int stream_request(int sockfd, uint32_t file_index);

/*
** Sends a stream request to the server, starts the audio player process and creates
** a file to store the incoming audio stream.
**
** This function leverages send_and_process_stream_request to send the request and
** receive the audio stream from the server.
**
** It also leverages the start_audio_player_process function to start the audio player.
**
** returns 0 on success, -1 on error
*/
int stream_and_get_request(int sockfd, uint32_t file_index, const Library * library);

/*
** Sends a stream request for the particular file_index to the server and sends the audio
** stream to the audio_out_fd and file_dest_fd file descriptors
** -- provided that they are not < 0.
**
** The select system call should be used to simultaneously wait for data to be available
** to read from the server connection/socket, as well as for when audio_out_fd and file_dest_fd
** (if applicable) are ready to be written to. Differing numbers of bytes may be written to
** at each time (do no use write_precisely for this purpose -- you will nor receive full marks)
** audio_out_fd and file_dest_fd, and this must be handled.
**
** One of audio_out_fd or file_dest_fd can be -1, but not both. File descriptors >= 0
** should be closed before the function returns.
**
** This function will leverage a dynamic circular buffer with two output streams
** and one input stream. The input stream is the server connection/socket, and the output
** streams are audio_out_fd and file_dest_fd. The buffer should be dynamically sized using
** realloc. See the assignment handout for more information, and notice how realloc is used
** to manage the library.files in this client and the server.
**
** Phrased differently, this uses a FIFO with two independent out streams and one in stream,
** but because it is as long as needed, we call it circular, and just point to three different
** parts of it.
**
** returns 0 on success, -1 on error
*/
int send_and_process_stream_request(int sockfd, uint32_t file_index,
                                    int audio_out_fd, int file_dest_fd);

#endif // AS_CLIENT_H_
