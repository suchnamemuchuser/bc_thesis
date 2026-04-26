#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

#define READER_MAX_CAP 128

typedef struct circularBuffer
{
    uint8_t* data_ptr; // pointer to data

    size_t data_len; // length of data in bytes

    size_t data_head_offset;

    size_t readerOffset[READER_MAX_CAP]; // For each reader, tail position relative to start of buffer - buffer length for unused fields

    int reader_cnt; // number of registered readers

    bool recordingActive;

} CircularBuffer;

int circularBufferInit(CircularBuffer* buffer, int bufferSize);

void circularBufferFree(CircularBuffer* buffer);

int circularBufferMemWrite(CircularBuffer* cb, const uint8_t* src, size_t len);

int circularBufferConfirmWrite(CircularBuffer* buffer, size_t writeLen);

size_t circularBufferWriterSpace(CircularBuffer* buffer);

int circularBufferConfirmRead(CircularBuffer* buffer, int readerId, size_t readLen);

int circularBufferAvailableData(CircularBuffer* buffer, int readerId);

int circularBufferReadData(CircularBuffer* cb, int readerId, size_t readLen, uint8_t** read_ptr);

void circularBufferPrintStatus(CircularBuffer* cb);