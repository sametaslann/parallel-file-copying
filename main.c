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


#include "file_transfer.h"
#include "common.h"

#define MAX_FD_LIMIT 1000


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t new_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t new_fd_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

int done = 0;
int signal_received = 0;
int total_bytes = 0;
int created_dir = 0;
int copied_reg_file = 0;
int copied_fifo_file = 0;
unsigned int fd_limit;
unsigned int num_of_opened_fd = 0;



Buffer* buffer;

void handle_signal(int signal) {

    switch (signal)
    {
    case SIGINT:
        printf("\t Received SIGINT signal. Terminating...\n");
        break;
    case SIGTSTP:
        printf("\t Received SIGSTP signal. Terminating...\n");
        break;
    }

    done = 1;
    signal_received = 1;
    pthread_cond_broadcast(&full);
    pthread_cond_broadcast(&empty); 

}


// void* signal_handler_thread(void *arg){

//     sigset_t signalSet;
//     sigemptyset(&signalSet);
//     sigaddSet(signalSet, SIGINT);
//     sigaddSet(signalSet, SIGTSTP);

//     while (!stop)
//     {
//         int sig;
//         siginfo_t info;

//         if (sigwaitinfo(&signalSet, &info) != 1)
//         {
            
//         }
        
//     }
    

// }


void store_file_descriptors(const char* source_file_path, const char* destination_file_path, char *filename) {



    pthread_mutex_lock(&new_fd_mutex);
    while(num_of_opened_fd >= MAX_FD_LIMIT){
        pthread_cond_wait(&new_fd_cond, &new_fd_mutex);
    }
    pthread_mutex_unlock(&new_fd_mutex);



    int source_file = open(source_file_path, O_RDONLY, 0777);
    if (source_file < 1) {
        pthread_mutex_lock(&stdout_mutex);
        perror("open");
        pthread_mutex_unlock(&stdout_mutex);
        return;
    }

    // Open the destination file for writing
    int destination_file = open(destination_file_path, O_WRONLY | O_CREAT, 0777);
    if (destination_file < 1) {
        pthread_mutex_lock(&stdout_mutex);
        perror("open");
        pthread_mutex_unlock(&stdout_mutex);
        close(source_file);
        return;
    }

    pthread_mutex_lock(&mutex);

    num_of_opened_fd += 2; 
    //Wait the buffer has a empty place
    while (isFull(buffer) && !done)
        pthread_cond_wait(&empty, &mutex);
    
    if (isEmpty(buffer) && done)
    {
        pthread_mutex_unlock(&mutex);
        return;
    }
    
    //Insert item to buffer
    printf("source: %d\n", source_file);
    printf("dest: %d\n", destination_file);

    enqueue(buffer, source_file, destination_file, filename);

    // Signal that the buffer is not empty
    pthread_cond_signal(&full); 
    
    // Release the lock
    pthread_mutex_unlock(&mutex);


}

void copy_directory(char* source_dir_path, char* destination_dir_path){

    DIR *source_dir = opendir(source_dir_path);
    if (!source_dir) {
        pthread_mutex_lock(&stdout_mutex);
        printf("Failed to open source directory!!\n");
        pthread_mutex_unlock(&stdout_mutex);
        done = 1;
        return;
    }


    mkdir(destination_dir_path, 0777);

    struct dirent* dir_entry;
    while ((dir_entry = readdir(source_dir)) != NULL && !done)
    {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        if (dir_entry->d_type == DT_DIR ) // DIRECTORY
        {
            char destination_subdir[512];
            char source_subdir[512];

            snprintf(destination_subdir, sizeof(destination_subdir), "%s/%s", destination_dir_path, (dir_entry->d_name));
            snprintf(source_subdir, sizeof(source_subdir), "%s/%s", source_dir_path, dir_entry->d_name);

            ++created_dir; 


            pthread_mutex_lock(&stdout_mutex);
            printf("|\t\033[1;31m# %-42s  directory created  #\033[0m\n", destination_subdir);
            pthread_mutex_unlock(&stdout_mutex);

            copy_directory(source_subdir, destination_subdir);

        }

        else if (dir_entry->d_type == DT_FIFO ) // FIFO
        {
            ++copied_fifo_file;
            mkfifo(destination_dir_path, 0777);

            pthread_mutex_lock(&stdout_mutex);
            printf("|\t\033[1;31m# %-30s  FIFO copied        #\033[0m\t\t|\n", destination_dir_path);
            pthread_mutex_unlock(&stdout_mutex);

        }

        else if (dir_entry->d_type == DT_REG ) // REGULAR
        {
            ++copied_reg_file;
            char destination_file_path[256];
            char source_file_path[256];
            char* filename = dir_entry->d_name;

            snprintf(destination_file_path, sizeof(destination_file_path), "%s/%s", destination_dir_path, filename);
            snprintf(source_file_path, sizeof(source_file_path), "%s/%s", source_dir_path, filename);

            store_file_descriptors(source_file_path, destination_file_path, filename);
        }
    }
    closedir(source_dir);

}

void* producer(void *arg){

    char** directories = (char**)arg;
    char *source_dir_path = directories[0];
    char *destination_dir_path = directories[1];

    pthread_mutex_lock(&stdout_mutex);
    printf(" \n\033[1;32m --- Directory copying process started between '%s' to '%s' ---\033[0m\n\n", directories[0], directories[1]);
    pthread_mutex_unlock(&stdout_mutex);


    struct stat st;
    if (stat(destination_dir_path, &st) == -1) {
        // Destination directory does not exist, create it
        mkdir(destination_dir_path, 0777);
    }

    else {
        char *lastSlash = strrchr(source_dir_path, '/');
        char destination_subdir[512];
        snprintf(destination_subdir, sizeof(destination_subdir), "%s%s", destination_dir_path, lastSlash);
        destination_dir_path = destination_subdir;
    }


    copy_directory(source_dir_path, destination_dir_path);
    
    done = 1;
    pthread_cond_broadcast(&full); 
    return NULL;
} 



void* consumer(void *arg){


    while (1) // Waits done flag
    {
        pthread_mutex_lock(&mutex);

        //Wait the buffer has an item
        while (isEmpty(buffer) && !done)
            pthread_cond_wait(&full, &mutex);


        if (done)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        

        FileInformations *fileInfos = dequeue(buffer);

        // Signal that the buffer is not full
        pthread_cond_signal(&empty);

        // Release the lock
        pthread_mutex_unlock(&mutex);

        
        int source_fd = fileInfos->source_fd;
        int destination_fd = fileInfos->destination_fd;
        char *filename = fileInfos->filename;


        // Copy the contents of the source file to the dest file using chunks
        char buff[4096];
        size_t bytes_read;
        size_t bytes_written;
        size_t total_bytes_for_a_file = 0;

        while ((bytes_read = read(source_fd, buff, sizeof(buff))) > 0 && !signal_received)
        {
            bytes_written = write(destination_fd, buff, bytes_read);
            if (bytes_written != bytes_read)
            {
                printf("Failed to write to destination file \n");
                break;
            }
            total_bytes += bytes_written;
            total_bytes_for_a_file += bytes_written; 
        }

        pthread_mutex_lock(&stdout_mutex);
        printf("|\t\033[1;36m#  %-30s  %10ld bytes copied succesfully #\033[0m\t\t\n", filename, total_bytes_for_a_file);
        pthread_mutex_unlock(&stdout_mutex);




        pthread_mutex_lock(&new_fd_mutex);
        close(source_fd);
        close(destination_fd);    
        num_of_opened_fd -= 2; 
        pthread_cond_signal(&new_fd_cond);
        pthread_mutex_unlock(&new_fd_mutex);
    }

    return NULL;
} 


int main(int argc, char *argv[]) {
      printf("zort");

    char* directories[2];
    struct timeval start,end;
    struct sigaction sa;
    struct rlimit limit;


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
    

    //Parse the given command line
    int buffer_size = atoi(argv[1]);
    long num_consumers = atoi(argv[2]);
    char *source_dir_path = argv[3];
    char *destination_dir_path = argv[4];

    directories[0] = source_dir_path;
    directories[1] = destination_dir_path;

    printf("size: %d\n", buffer_size);
    buffer = createQueue(buffer_size);
    

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("getrlimit");
        return 1;
    }
    fd_limit = limit.rlim_cur;


    if (gettimeofday(&start, NULL) == -1)
    {
        perror("gettimeofday");
        return -1;
    }

    //Create consumer threads
    pthread_t consumer_threads[num_consumers];
    for (int i = 0; i < num_consumers; i++)
    {
        printf("**************************\n");
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
    
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&stdout_mutex);

    pthread_cond_destroy(&full);
    pthread_cond_destroy(&empty);



    if (gettimeofday(&end, NULL) == -1)
    {
        perror("gettimeofday");
        return -1;
    }
    

    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    double elapsed = seconds + microseconds / 1000000.0;


    printf("\n\t\033[1;32m--%d Directory created \033[0m\n\n", created_dir);
    printf("\n\t\033[1;32m--%d Regular file copied \033[0m\n\n", copied_reg_file);
    printf("\n\t\033[1;32m--%d Fifo copied \033[0m\n\n", copied_fifo_file);

    printf("\n\t\033[1;32m--Total Time to copy files : %f ms\033[0m\n\n", elapsed);

    printf("\n\t\033[1;32m--Total %d bytes copied \033[0m\n\n", total_bytes);

    for (int i = 0; i < buffer->size; i++)
    {
        FileInformations *fds = dequeue(buffer);
        close(fds->source_fd);
        close(fds->destination_fd);
    }
    

    free(buffer->array);
    free(buffer);

    return 0;
}
