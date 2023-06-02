#ifndef COMMON_H
#define COMMON_H



typedef struct {
    int source_fd;
    int destination_fd;
    char filename[512];
} FileInformations;

typedef struct {
    int front, rear, size;
    unsigned capacity;
    FileInformations** array;
} Buffer;

Buffer* createQueue(unsigned capacity);
int isFull(Buffer* queue);
int isEmpty(Buffer* queue);
void enqueue(Buffer* queue, int source_file, int destination_file, const char* file_name);
FileInformations *dequeue(Buffer* queue);
FileInformations front(Buffer* queue);
FileInformations rear(Buffer* queue);

#endif /* COMMON_H */
