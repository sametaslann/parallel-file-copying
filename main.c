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

#include "file_transfer.h"
#include "common.h"


Buffer* buffer;


int done = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;


void* producer(void *arg){

    char** directories = (char**)arg;
    char *source_dir_path = directories[0];
    char *destination_dir_path = directories[1];


    DIR *source_dir = opendir(source_dir_path);
    if (!source_dir) {
        printf("Failed to open source directory.\n");
        exit(EXIT_FAILURE);
    }

    DIR *destination_dir = opendir(destination_dir_path);
    if (!destination_dir) {
        printf("Failed to open destination directory.\n");
        closedir(source_dir);
        exit(EXIT_FAILURE);
    }

    struct dirent* dir_entry;
    while ((dir_entry = readdir(source_dir)) != NULL)
    {
        if (dir_entry->d_type == DT_REG) // (REGULAR) don't forget to add fifo and otherssssssssssss
        {
            char* filename = dir_entry->d_name;

            char source_file_path[256];
            snprintf(source_file_path, sizeof(source_file_path), "%s/%s", source_dir_path, filename);

            int source_file = open(source_file_path, O_RDONLY, 0666);
            if (!source_file) {
                printf("Failed to open source file: %s\n", source_file_path);
                continue;
            }

            char destination_file_path[256];
            snprintf(destination_file_path, sizeof(destination_file_path), "%s/%s", destination_dir_path, filename);

            // Open the destination file for writing
            int destination_file = open(destination_file_path, O_WRONLY | O_CREAT, 0666);
            if (!destination_file) {
                printf("Failed to create destination file: %s\n", destination_file_path);
                close(source_file);
                continue;
            }

            pthread_mutex_lock(&mutex);


            //Wait the buffer has a empty place
            while (isFull(buffer))
                pthread_cond_wait(&empty, &mutex);
            

            //Insert item to buffer
            enqueue(buffer, source_file, destination_file, filename);


            // Signal that the buffer is not empty
            pthread_cond_broadcast(&full); // broadcast olabilir burası 
            
            // Release the lock
            pthread_mutex_unlock(&mutex);

        }
        
    }

    done = 1;
    return NULL;
} 



void* consumer(void *arg){


    while (!done) //Done flagını bekle
    {
        pthread_mutex_lock(&mutex);

        //Wait the buffer has an item
        while (isEmpty(buffer))
            pthread_cond_wait(&full, &mutex);

        FileInformations fileInfos = dequeue(buffer);

        int source_fd = fileInfos.source_fd;
        int destination_fd = fileInfos.destination_fd;
        char *filename = fileInfos.filename;

        // Signal that the buffer is not full
        pthread_cond_signal(&empty);

        // Release the lock
        pthread_mutex_unlock(&mutex);


        // Copy the contents of the source file to the dest file using chunks
        char buffer[4096];
        size_t bytes_read;
        size_t bytes_written;

        while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0)
        {
            bytes_written = write(destination_fd, buffer, bytes_read);
            if (bytes_written != bytes_read)
            {
                printf("Failed to write to destination file \n");
                break;
            }
        }
        printf("%s copied to destionation path \n", filename);

        close(source_fd);
        close(destination_fd);    
    }
    
    return NULL;
} 


int main(int argc, char *argv[]) {

    char* directories[2];

    if (argc != 5) {
        printf("Usage: %s buffer_size num_consumers source_dir destination_dir\n", argv[0]);
        return 1;
    }

    //Parse the given command line
    int buffer_size = atoi(argv[1]);
    int num_consumers = atoi(argv[2]);
    char *source_dir_path = argv[3];
    char *destination_dir_path = argv[4];

    directories[0] = source_dir_path;
    directories[1] = destination_dir_path;

    buffer = createQueue(buffer_size);

     //Create consumer threads
    pthread_t consumer_threads[num_consumers];
    for (int i = 0; i < num_consumers; i++)
    {
       if (pthread_create(&consumer_threads[i], NULL, consumer, NULL) != 0) {
            printf("Failed to create consumer thread %d.\n", i);
            return -1;
        }        
    }

    // Create producer thread
    pthread_t producer_thread;
    if (pthread_create(&producer_thread, NULL, producer, (void*)directories) != 0) {
        printf("Failed to create producer thread.\n");
        return -1;
    }


    // Wait for consumer threads to finish
    for (int i = 0; i < num_consumers; i++) {
        if (pthread_join(consumer_threads[i], NULL) != 0) {
            printf("Failed to join consumer thread %d.\n", i);
            return -1;
        }
    }

    // Wait for producer thread to finish
    if (pthread_join(producer_thread, NULL) != 0) {
        printf("Failed to join producer thread.\n");
        return -1;
    }


    return 0;
}
