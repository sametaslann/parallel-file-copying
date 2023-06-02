// C program for array implementation of queue
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"




// function to create a queue
// of given capacity.
// It initializes size of queue as 0
Buffer* createQueue(unsigned capacity)
{
    Buffer* queue = (Buffer*)malloc(sizeof(Buffer));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (FileInformations**)malloc(queue->capacity * sizeof(FileInformations*));
    return queue;
}

// Queue is full when size becomes
// equal to the capacity
int isFull(Buffer* queue)
{
	return (queue->size == (int)queue->capacity);
}

// Queue is empty when size is 0
int isEmpty(Buffer* queue)
{
	return (queue->size == 0);
}

// Function to add an item to the queue.
// It changes rear and size
void enqueue(Buffer* queue, int source_file, int destination_file, const char* filename)
{
    if (isFull(queue)) {
        printf("Queue is full. Cannot enqueue.\n");
        return;
    }

    FileInformations* data = malloc(sizeof(FileInformations));

    data->source_fd = source_file;
    data->destination_fd = destination_file;

    strncpy(data->filename, filename, sizeof(data->filename - 1));
    data->filename[sizeof(data->filename - 1)] = '\0'; 

    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = data;
    // strncpy(queue->array[queue->rear].filename, filename, sizeof(queue->array[queue->rear].filename) - 1);

    // queue->array[queue->rear].filename[sizeof(queue->array[queue->rear].filename) - 1] = '\0'; // Ensure null-termination
    queue->size = queue->size + 1;
}


// Function to remove an item from queue.
// It changes front and size
FileInformations* dequeue(Buffer* queue)
{
    FileInformations *emptyFileInformations = { 0 };
    if (isEmpty(queue)) {
        printf("Queue is empty. Cannot dequeue.\n");
        return emptyFileInformations;
    }
    FileInformations* item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

// FileInformations front(Buffer* queue)
// {
//     FileInformations emptyFileInformations = { 0 };
//     if (isEmpty(queue)) {
//         printf("Queue is empty.\n");
//         return emptyFileInformations;
//     }
//     return queue->array[queue->front];
// }

// FileInformations rear(Buffer* queue)
// {
//     FileInformations emptyFileInformations = { 0 };
//     if (isEmpty(queue)) {
//         printf("Queue is empty.\n");
//         return emptyFileInformations;
//     }
//     return queue->array[queue->rear];
// }