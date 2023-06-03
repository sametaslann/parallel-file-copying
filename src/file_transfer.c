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

#include "../include/common.h"
#include "../include/file_transfer.h"

#define MAX_FD_LIMIT 1000


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t new_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t new_fd_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

pthread_t *consumer_threads;
pthread_t producer_thread;

struct timeval start,end;


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

void printStatistics(){

    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    double elapsed = seconds + microseconds / 1000000.0;


    printf("\n\t\033[1;32m--%d Directory created \033[0m\n\n", created_dir);
    printf("\n\t\033[1;32m--%d Regular file copied \033[0m\n\n", copied_reg_file);
    printf("\n\t\033[1;32m--%d Fifo copied \033[0m\n\n", copied_fifo_file);
    printf("\n\t\033[1;32m--Total Time to copy files : %f ms\033[0m\n\n", elapsed);
    printf("\n\t\033[1;32m--Total %d bytes copied \033[0m\n\n", total_bytes);
}


int initialize(int buffer_size, int num_consumers, char **directories){

    struct rlimit limit;

    buffer = createQueue(buffer_size);
    
    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("getrlimit");
        return 0;
    }
    fd_limit = limit.rlim_cur;


    if (gettimeofday(&start, NULL) == -1){
        perror("gettimeofday");
        return -1;
    }

    //Create consumer threads
    consumer_threads = (pthread_t*)malloc(num_consumers * sizeof(pthread_t));
    for (int i = 0; i < num_consumers; i++)
    {
       if (pthread_create(&consumer_threads[i], NULL, consumer, NULL) != 0) {
            printf("Failed to create consumer thread %d.\n", i);
            return 0;
        }        
    }

    // Create producer thread
    if (pthread_create(&producer_thread, NULL, producer, (void*)directories) != 0) {
        printf("Failed to create producer thread.\n");
        return 0;
    }

    return 1;
}



int destroyThreads(int num_consumers){
    // Wait for consumer threads to finish
    for (int i = 0; i < num_consumers; i++) {
        // printf("i: %d %d\n",i ,num_consumers);
        if (pthread_join(consumer_threads[i], NULL) != 0) {
            perror("Joining consumer thread ");
            return 0;
        }
    }

    // Wait for producer thread to finish
    if (pthread_join(producer_thread, NULL) != 0) {
        perror("Joining producer thread.\n");
        return 0;
    }
    if (gettimeofday(&end, NULL) == -1)
    {
        perror("gettimeofday");
        return 0;
    }
    
    if(pthread_mutex_destroy(&mutex) != 0 
        || pthread_mutex_destroy(&stdout_mutex) != 0 
        || pthread_cond_destroy(&full) != 0
        || pthread_cond_destroy(&empty) != 0)
    {
        perror("Destroy");
        return 0;
    }

    free(consumer_threads);

    return 1;
}

int freeResources(int num_consumers){

    for (int i = 0; i < num_consumers; i++) {
        // printf("i: %d %d\n",i ,num_consumers);
        if (pthread_join(consumer_threads[i], NULL) != 0) {
            perror("Joining consumer thread ");
            return 0;
        }
    }

    // Wait for producer thread to finish
    if (pthread_join(producer_thread, NULL) != 0) {
        perror("Joining producer thread.\n");
        return 0;
    }

    free(consumer_threads);
    free(buffer->array);
    free(buffer);
    
    if(pthread_mutex_destroy(&mutex) != 0 
        || pthread_mutex_destroy(&stdout_mutex) != 0 
        || pthread_cond_destroy(&full) != 0
        || pthread_cond_destroy(&empty) != 0)
    {
        perror("Destroy");
        return 0;
    }

    if (gettimeofday(&end, NULL) == -1)
    {
        perror("gettimeofday");
        return 0;
    }

    return 1;

}

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
    int destination_file = open(destination_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
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
    

    enqueue(buffer, source_file, destination_file, destination_file_path);

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
            char *filename = dir_entry->d_name;

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


        if (isEmpty(buffer) && done)
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
        char *filename = fileInfos->filepath;


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
                pthread_mutex_lock(&stdout_mutex);
                printf("Failed to write to destination file \n");
                pthread_mutex_unlock(&stdout_mutex);

                break;
            }
            total_bytes += bytes_written;
            total_bytes_for_a_file += bytes_written; 
        }

        pthread_mutex_lock(&stdout_mutex);
        printf("|\033[1;36m#  %-30s  %10ld bytes copied succesfully #\033[0m\t\t\n", filename, (long)total_bytes_for_a_file);
        pthread_mutex_unlock(&stdout_mutex);

        pthread_mutex_lock(&new_fd_mutex);
        close(source_fd);
        close(destination_fd);    
        free(fileInfos);
        num_of_opened_fd -= 2; 
        pthread_cond_signal(&new_fd_cond);
        pthread_mutex_unlock(&new_fd_mutex);
    }

    return NULL;
} 

int is_path_exists(const char* path, char* parentDir) {

    const char* lastSlash = strrchr(path, '/'); // Find the last occurrence of '/'
    char parentDir_absolute[4096];
    if (lastSlash != NULL) {
        size_t length = lastSlash - path;

        strncpy(parentDir, path, length);
        parentDir[length] = '\0'; // Null-terminate the string

    } else 
        parentDir[0] = '\0'; // Empty string


    if (realpath(parentDir, parentDir_absolute) == NULL) {
        return 0;
    }

    strcpy(parentDir, parentDir_absolute);

    return 1;
}

int directory_checking(const char* source, const char* destination) {

    char dest_parent[4096];
    char source_absolute[4096];
    char dest_absolute[4096];

    // Get the absolute path of the source file/directory
    if (realpath(source, source_absolute) == NULL) {
        perror("Source Path");
        return 0;
    }

    if (realpath(destination, dest_absolute) == NULL) {

        if (!is_path_exists(destination, dest_parent)){
            perror("Destination Path");
            return 0;
        }
        strcpy(dest_absolute ,dest_parent);
    }

    // Compare the directory paths
    if(strncmp(source_absolute, dest_absolute, strlen(source_absolute)) == 0)
    {
        printf("Cannot copy into itself\n");
        return 0;
    }

    return 1;
}