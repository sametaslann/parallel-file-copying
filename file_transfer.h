#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H
#define _POSIX_C_SOURCE 200809L

#include "common.h"



void* producer(void *arg);
void* consumer(void *arg);

#endif
