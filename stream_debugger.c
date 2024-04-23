#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void print_usage(){
    printf("Usage: stream_debugger [-h] [-f DEBUG_FILE] [-c READ_CHUNK]\n");
    printf("  -h: Print this help message\n");
    printf("  -f  debug_file: Use DEBUG_FILE as the file to dump the\n");
    printf("      stream into. Compare this file to the original using\n");
    printf("      diff tool to see if the stream is correct. The file\n");
    printf("      will be overwritten if it already exists.\n");
    printf("       -- If not specified, the stream will not be dumped.\n");
    printf("  -c  read_chunk: Use READ_CHUNK as the number of bytes to\n");
    printf("      read from stdin at a time. This may be useful for\n");
    printf("      debugging purposes.\n");
}

/*
** This function reads from stdin and writes to a file if the debug_file
** is specified.
*/
void stream_debugger(int read_chunk, char *debug_file){
    FILE *file = NULL;
    if(debug_file){
        file = fopen(debug_file, "w");
        if(!file){
            perror("stream_debugger: fopen");
            return;
        }
    }
    char buffer[read_chunk];
    int bytes_read;
    while((bytes_read = fread(buffer, 1, read_chunk, stdin)) > 0){
        printf("SD: Read %d bytes from stdin\n", bytes_read);
        if(debug_file){
            int num = bytes_read;
            while(num > 0){
                int written = fwrite(buffer, 1, bytes_read, file);
                if(written < 0){
                    perror("stream_debugger: fwrite");
                    return;
                }
                printf("SD: Wrote %d bytes to file\n", written);
                num -= written;
            }
        }
    }
    if(debug_file){
        fclose(file);
    }
}

int main(int argc, char *argv[]){
    char *debug_file = NULL;
    int read_chunk = 1024;
    int i;
    for(i = 1; i < argc; i++){
        if(strcmp(argv[i], "-h") == 0){
            print_usage();
            return 0;
        }
        else if(strcmp(argv[i], "-f") == 0){
            if(i + 1 < argc){
                debug_file = argv[i + 1];
                i++;
            }
        }
        else if(strcmp(argv[i], "-c") == 0){
            if(i + 1 < argc){
                read_chunk = atoi(argv[i + 1]);
                if(read_chunk <= 0){
                    fprintf(stderr, "stream_debugger: -c requires a positive integer argument\n");
                    return 1;
                }
                i++;
            }
        }
        else{
            fprintf(stderr, "stream_debugger: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }
    stream_debugger(read_chunk, debug_file);
    return 0;
}
