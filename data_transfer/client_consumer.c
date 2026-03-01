#include <stdio.h>      // For FILE, fopen, fwrite, printf, perror, snprintf
#include <stdlib.h>     // For malloc, free, size_t
#include <stdint.h>     // For uint8_t, uint32_t, uint64_t
#include <stdbool.h>    // For bool, true, false
#include <pthread.h>
#include <time.h>

#include "client.h"

void* fileConsumerThread(void* arg)
{
    ClientSession* session = (ClientSession*)arg;
    circularBuffer* buffer = &session->buffer;
    
    time_t raw_time = (time_t)session->timestamp;
    struct tm *time_info = localtime(&raw_time);

    char datetime[20];
    strftime(datetime, sizeof(datetime), "%Y-%m-%d-%H-%M", time_info);

    char filename[512];
    snprintf(filename, sizeof(filename), "%s_%s.bin", datetime, session->name);

    FILE *fptr = fopen(filename, "wb");
    if (fptr == NULL)
    {
        perror("Consumer: Failed to open file");
        return NULL;
    }
    printf("Consumer: Started recording to %s\n", filename);

    size_t data_to_read = 10000;
    size_t availableData;
    size_t read_len; 
    uint8_t* read_ptr;

    pthread_mutex_lock(&session->buffer_lock);
    int my_reader_id = buffer->reader_cnt;
    buffer->readerOffset[my_reader_id] = buffer->data_head_offset;
    buffer->reader_cnt++;
    pthread_mutex_unlock(&session->buffer_lock);
    
    while(1)
    {
        pthread_mutex_lock(&session->buffer_lock);
        
        // Wait until we have enough data OR the producer has finished
        while (((availableData = circularBufferAvailableData(buffer, my_reader_id)) < data_to_read) && 
               (buffer->writerFinished == false))
        {
            pthread_cond_wait(&session->data_available, &session->buffer_lock);
        }

        // Exit condition: Producer finished and we drained the buffer
        if (availableData == 0 && buffer->writerFinished) 
        {
            pthread_mutex_unlock(&session->buffer_lock);
            break; 
        }

        // Determine how much we can actually read
        if (availableData < data_to_read) {
            read_len = availableData;
        } else {
            read_len = data_to_read;
        }

        read_len = circularBufferReadData(buffer, my_reader_id, read_len, &read_ptr);

        pthread_mutex_unlock(&session->buffer_lock);

        fwrite(read_ptr, sizeof(uint8_t), read_len, fptr);

        pthread_mutex_lock(&session->buffer_lock);
        circularBufferConfirmRead(buffer, my_reader_id, read_len);
        pthread_mutex_unlock(&session->buffer_lock);
    }

    fclose(fptr);
    printf("Consumer: Thread finished. Saved %s.\n", filename);

    return NULL;
}