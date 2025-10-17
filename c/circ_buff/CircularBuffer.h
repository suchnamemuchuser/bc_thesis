#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdlib.h>
#include <semaphore.h>

#define SOURCE_ZEROS 1
#define SOURCE_GEN 2
#define SOURCE_FILE 3

#define READER_MAX_CAP 128
#define BUFFER_DEF_SIZ 512000000 // 512 MB

typedef struct 
{
    int source;     // store which source for data from SOURCE_ defines
    char* filename; // For -f <filename>
    char* hostname; // For -h <hostname/ip>
    int port;       // For -p <port>
    
} cb_config;

typedef struct
{
    int readerID;

    char* readerTail;

    int dataAmountAvailable;

    sem_t dataUnavailableAvailableLock; // this probably doesnt need to be here, could maybe be solved with a wait or something
} cb_readerInfo;

typedef struct
{
    char* data_ptr; // pointer to data

    size_t data_len; // length of data in bytes
    
    char* data_head; // The position within the buffer, where new data is to be written

    char* data_end; // pointer to the end of data - writing here would cause memory error
    
    cb_readerInfo reader_data[READER_MAX_CAP]; // Array for reader data

    int reader_cnt; // number of registered readers


} cb_CircularBuffer;

int parseArguments(int argc, char* argv[], cb_config* cfg);

int circularBufferInit(cb_CircularBuffer* buffer, int bufferSize);

void circularBufferFree(cb_CircularBuffer* buffer);

int circularBufferWrite(cb_CircularBuffer* buffer, char* data, size_t writeLen);

int circularBufferRead(cb_CircularBuffer* buffer, cb_readerInfo* readerInfo);

#endif // CIRCULAR_BUFFER_H