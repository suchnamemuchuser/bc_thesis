#include <stdio.h>
//#include <stdlib.h>
#include <pthread.h>
#include<unistd.h>

#include "CircularBuffer.h"
#include "ArgParser.h"

pthread_mutex_t buffer_lock;

pthread_cond_t data_available;

pthread_cond_t space_available;

circularBuffer buffer;

void* writerBehaviour(void* arg);

void* readerBehaviour(void* arg);

int main(int argc, char* argv[]){

    argParser arguments = parseArguments(argc, argv);

    return 0;

    // init mutex, conds
    pthread_mutex_init(&buffer_lock, NULL);
    pthread_cond_init(&data_available, NULL);
    pthread_cond_init(&space_available, NULL);

    circularBufferInit(&buffer, 2000);

    pthread_t writer_thread;
    pthread_t reader_thread;

    printf("Starting threads...\n");

    if (pthread_create(&writer_thread, NULL, writerBehaviour, NULL)) 
    {
        fprintf(stderr, "Error creating writer thread\n");
        return 1;
    }
    
    if (pthread_create(&reader_thread, NULL, readerBehaviour, NULL))
    {
        fprintf(stderr, "Error creating reader thread\n");
        return 1;
    }

    pthread_join(writer_thread, NULL);

    printf("Writer finished. Cleaning up.\n");

    pthread_cancel(reader_thread);

    // destroy mutex, conds
    pthread_mutex_destroy(&buffer_lock);
    pthread_cond_destroy(&data_available);
    pthread_cond_destroy(&space_available);

    circularBufferFree(&buffer);

    return 0;
}

void* writerBehaviour(void* arg) // will be in a while loop?
{
    time_t start = time(NULL);
    time_t end = start + 60;

    while (end > time(NULL))
    {
        sleep(1); // writing 30 bytes every second

        pthread_mutex_lock(&buffer_lock);

        // if no space, print error and discard data

        int ret = circularBufferWrite(&buffer, 30);

        if (ret < 0)
        {
            printf("Writer has no space to write!\n");
            return NULL;
        }

        pthread_cond_broadcast(&data_available);

        pthread_mutex_unlock(&buffer_lock);

        printf("Wrote %d of data.\n", 30);
    }

    return NULL;
}

void* readerBehaviour(void* arg) // will be in a while loop?
{

    size_t data_to_read = 100;
    int my_reader_id = 0;

    pthread_mutex_lock(&buffer_lock);
    if (buffer.readerTails[my_reader_id] == NULL)
    {
        buffer.readerTails[my_reader_id] = buffer.data_ptr; 
        buffer.reader_cnt = 1;
    }
    pthread_mutex_unlock(&buffer_lock);

    while (1)
    {
        pthread_mutex_lock(&buffer_lock);

        while (circularBufferAvailableData(&buffer, my_reader_id) < (int) data_to_read)
        {
            pthread_cond_wait(&data_available, &buffer_lock);
        }

        int readCnt = circularBufferRead(&buffer, my_reader_id, data_to_read);

        pthread_mutex_unlock(&buffer_lock);

        // This is where data is processed

        // mutes lock again

        // update tail (maybe atomic)

        // mutex unlock

        printf("Read %d of data.\n", readCnt);
    }

    return NULL;
}