#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H
#define _POSIX_C_SOURCE 200809L

#include "common.h"

void handle_signal(int signal);
int initialize(int buffer_size, int num_consumer, char **directories);
int freeResources(int num_consumers);
void store_file_descriptors(const char* source_file_path, const char* destination_file_path, char *filename);
void copy_directory(char* source_dir_path, char* destination_dir_path);
int destroyThreads(int num_consumers);


void* producer(void *arg);
void* consumer(void *arg);

void printStatistics();

int is_path_exists(const char* path, char* parentDir);
int directory_checking(const char* source, const char* destination);


#endif
