#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdlib.h>

typedef struct
{
    int readerID;

    char* readerTail;
} cb_readerInfo;

typedef struct
{
    char* data_ptr; // pointer to data

    size_t data_len; // length of data in bytes
    
    char* data_head; // The position within the buffer, where new data is to be written

    char* data_end; // pointer to the end of data - writing here would cause memory error
    
    cb_readerInfo reader_data[16]; // Array for reader data

    int reader_cnt; // number of registered readers


} cb_CircularBuffer;



int circularBufferInit(cb_CircularBuffer* buffer, int bufferSize);

void circularBufferFree(cb_CircularBuffer* buffer);

int circularBufferWrite(cb_CircularBuffer* buffer, char* data, size_t writeLen);

#endif // CIRCULAR_BUFFER_H