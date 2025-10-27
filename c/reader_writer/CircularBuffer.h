#include <stdlib.h>

#define READER_MAX_CAP 128
#define BUFFER_DEF_SIZ 512000000 // 512 MB

typedef struct
{
    char* data_ptr; // pointer to data

    size_t data_len; // length of data in bytes
    
    char* data_head; // The position within the buffer, where new data is to be written

    char* data_end; // pointer to the end of data - writing here would cause memory error
    
    char* readerTails[READER_MAX_CAP]; // For each reader, either NULL or pointer to where reader will read next

    int reader_cnt; // number of registered readers


} circularBuffer;

int circularBufferInit(circularBuffer* buffer, int bufferSize);

void circularBufferFree(circularBuffer* buffer);

int circularBufferWrite(circularBuffer* buffer, size_t writeLen);

int circularBufferRead(circularBuffer* buffer, int readerId, size_t readLen);

int circularBufferAvailableData(circularBuffer* buffer, int readerId);