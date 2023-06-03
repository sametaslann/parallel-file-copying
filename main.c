#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "include/common.h"
#include "../include/file_transfer.h"



int main(int argc, char *argv[]) {

    char* directories[2];
    struct sigaction sa;


    // Register the signal handler
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    if (argc != 5) {
        printf("Usage: %s buffer_size num_consumers source_dir destination_dir\n", argv[0]);
        return 1;
    }

    if (atoi(argv[1]) < 1 || atoi(argv[2]) < 1 )
    {
        printf("Buffer size and Number of consumer must greater than 0 \n");
        return 1;
    }
    
    directories[0] = argv[3];
    directories[1] = argv[4];
    int buffer_size = atoi(argv[1]);
    int num_consumers = atoi(argv[2]);


    if(directory_checking(directories[0], directories[1]) == 0)
    {
        printf("Cannot copy a directory\n");
        exit(EXIT_FAILURE);
    }

    if(!initialize(buffer_size, num_consumers, directories)){
        exit(EXIT_FAILURE);
    }

    freeResources(num_consumers);
    printStatistics();

    return 0;
}
