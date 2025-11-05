#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

#define READER_MAX_CAP 128

typedef struct
{
    uint8_t* data_ptr; // pointer to data

    size_t data_len; // length of data in bytes
    
    //uint8_t* data_head; // The position within the buffer, where new data is to be written

    size_t data_head_offset;

    //uint8_t* data_end; // pointer to the end of data - writing here would cause memory error
    
    //uint8_t* readerTails[READER_MAX_CAP]; // For each reader, either NULL or pointer to where reader will read next

    size_t readerOffset[READER_MAX_CAP]; // For each reader, tail position relative to start of buffer - buffer length for unused fields

    int reader_cnt; // number of registered readers

    bool writerFinished;

} circularBuffer;

int circularBufferInit(circularBuffer* buffer, int bufferSize);

void circularBufferFree(circularBuffer* buffer);

int circularBufferWrite(circularBuffer* buffer, size_t writeLen);

size_t circularBufferWriterSpace(circularBuffer* buffer);

int circularBufferConfirmRead(circularBuffer* buffer, int readerId, size_t readLen);

int circularBufferAvailableData(circularBuffer* buffer, int readerId);

int circularBufferReadData(circularBuffer* buffer, int readerId, size_t readLen, uint8_t* readerBuffer);