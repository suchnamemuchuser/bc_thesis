#include "CircularBuffer.h"
#include <stdio.h>
#include <string.h> // for memset, strdup

int circularBufferInit(circularBuffer* buffer, int bufferSize)
{
    buffer->data_ptr = malloc((size_t) bufferSize);

    if(buffer->data_ptr == NULL) // check for malloc fail
    {
        exit(1);
    }

    buffer->data_len = bufferSize; // set size

    //buffer->data_head = buffer->data_ptr; // set head

    buffer->data_head_offset = 0;

    //buffer->data_end = buffer->data_ptr + buffer->data_len; // set end

    memset(buffer->data_ptr, 48, buffer->data_len); // init buffer with 0s

    buffer->reader_cnt = 0;

    // for (int i = 0 ; i < READER_MAX_CAP ; i++) // NULLify all reader tails
    // {
    //     buffer->readerTails[i] = NULL;
    // }

    for (int i = 0 ; i < READER_MAX_CAP ; i++) // NULLify all reader tails
    {
        buffer->readerOffset[i] = bufferSize;
    }

    buffer->writerFinished = false;

    return 0;
};

void circularBufferFree(circularBuffer* buffer)
{
    if(buffer->data_ptr != NULL)
    {
        free(buffer->data_ptr);

        buffer->data_ptr = NULL;
    }

    //buffer->data_head = NULL;

    //buffer->data_end = NULL;

    buffer->data_len = 0;
}

int circularBufferWrite(circularBuffer* buffer, size_t writeLen)
{
    //uint8_t* minTail = buffer->readerTails[0]; // find minimal tail (tail closest in front of data_head)

    size_t minTail = buffer->readerOffset[0];

    size_t availableSpace = buffer->data_len; // default is empty buffer

    if (minTail < buffer->data_head_offset) // calculate space left to minimal tail
    {
        availableSpace = (buffer->data_len - buffer->data_head_offset) + (minTail);
    }
    else if (minTail > buffer->data_head_offset)
    {
        availableSpace = minTail - buffer->data_head_offset;
    }
    
    if (availableSpace < writeLen) // not enough space to write because of a reader
    {
        return -1;
    }

    buffer->data_head_offset = (buffer->data_head_offset + writeLen) % buffer->data_len;

    return 0;
}

int circularBufferConfirmRead(circularBuffer* buffer, int readerId, size_t readLen)
{
    buffer->readerOffset[readerId] = (buffer->readerOffset[readerId] + readLen) % buffer->data_len; // update offset

    //buffer->readerTails[readerId] = buffer->data_ptr + ((buffer->readerOffset[readerId] + readLen) % buffer->data_len); // update ptr

    return readLen;
}

int circularBufferAvailableData(circularBuffer* buffer, int readerId)
{
    int spaceLeft = 0;

    if (buffer->readerOffset[readerId] < buffer->data_head_offset)
    {
        spaceLeft = buffer->data_head_offset - buffer->readerOffset[readerId];
    }
    else if (buffer->readerOffset[readerId] > buffer->data_head_offset)
    {
        spaceLeft = (buffer->data_len - buffer->readerOffset[readerId]) + buffer->data_head_offset;
    }

    return spaceLeft;
}

int circularBufferReadData(circularBuffer* buffer, int readerId, size_t readLen, uint8_t* readerBuffer)
{
    if (buffer->data_head_offset > buffer->readerOffset[readerId])
    {
        memcpy(readerBuffer, buffer->data_ptr+buffer->readerOffset[readerId], readLen);

        return readLen;
    }

    if ((buffer->data_len - buffer->readerOffset[readerId]) >= readLen)
    {
        memcpy(readerBuffer, buffer->data_ptr+buffer->readerOffset[readerId], readLen);

        return readLen;
    }

    int spaceToEnd = buffer->data_len - buffer->readerOffset[readerId];

    memcpy(readerBuffer, (buffer->data_ptr + buffer->readerOffset[readerId]), spaceToEnd);
    memcpy(readerBuffer + spaceToEnd, buffer->data_ptr, readLen - spaceToEnd);

    return readLen;
}