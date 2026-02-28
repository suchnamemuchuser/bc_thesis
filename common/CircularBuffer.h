#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

#define READER_MAX_CAP 128

typedef struct
{
    uint8_t* data_ptr; // pointer to data

    size_t data_len; // length of data in bytes

    size_t data_head_offset;

    size_t readerOffset[READER_MAX_CAP]; // For each reader, tail position relative to start of buffer - buffer length for unused fields

    int reader_cnt; // number of registered readers

    bool writerFinished;

} circularBuffer;

int circularBufferInit(circularBuffer* buffer, int bufferSize);

void circularBufferFree(circularBuffer* buffer);

int circularBufferMemWrite(circularBuffer* cb, const uint8_t* src, size_t len);

int circularBufferConfirmWrite(circularBuffer* buffer, size_t writeLen);

size_t circularBufferWriterSpace(circularBuffer* buffer);

int circularBufferConfirmRead(circularBuffer* buffer, int readerId, size_t readLen);

int circularBufferAvailableData(circularBuffer* buffer, int readerId);

int circularBufferReadData(circularBuffer* cb, int readerId, size_t readLen, uint8_t** read_ptr);

void circularBufferPrintStatus(circularBuffer* cb);