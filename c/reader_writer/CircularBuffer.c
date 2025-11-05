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
    buffer->data_head_offset = (buffer->data_head_offset + writeLen) % buffer->data_len;

    return 0;
}

size_t circularBufferWriterSpace(circularBuffer* buffer)
{
    size_t minSpace = buffer->data_len - 1;

    for (int i = 0 ; i < buffer->reader_cnt ; i++)
    {
        size_t tail = buffer->readerOffset[i];

        size_t rawWriterSpace = (tail - buffer->data_head_offset + buffer->data_len) % buffer->data_len;

        size_t writerSpace;
    
        if (rawWriterSpace == 0)
        {
            // head == tail. This means the buffer is EMPTY for this reader.
            // The writer can write (buffer_size - 1) bytes.
            writerSpace = buffer->data_len - 1;
        }
        else
        {
            // This is the number of empty slots.
            // We must subtract 1 for the guard slot.
            writerSpace = rawWriterSpace - 1;
        }

        if (writerSpace < minSpace)
        {
            minSpace = writerSpace;
        }
    }

    return minSpace;
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