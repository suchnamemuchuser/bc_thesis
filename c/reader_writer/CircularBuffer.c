#include "CircularBuffer.h"
#include <stdio.h>
#include <string.h> // for memset, strdup
#include <stdbool.h>

int circularBufferInit(circularBuffer* buffer, int bufferSize)
{
    buffer->data_ptr = (char*) malloc((size_t) bufferSize);

    if(buffer->data_ptr == NULL) // check for malloc fail
    {
        exit(1);
    }

    buffer->data_len = bufferSize; // set size

    buffer->data_head = buffer->data_ptr; // set head

    buffer->data_end = buffer->data_ptr + buffer->data_len; // set end

    memset(buffer->data_ptr, 0, buffer->data_len); // init buffer with 0s

    buffer->reader_cnt = 0;

    for (int i = 0 ; i < READER_MAX_CAP ; i++) // NULLify all reader tails
    {
        buffer->readerTails[i] = NULL;
    }

    return 0;
};

void circularBufferFree(circularBuffer* buffer)
{
    if(buffer->data_ptr != NULL)
    {
        free(buffer->data_ptr);

        buffer->data_ptr = NULL;
    }

    buffer->data_head = NULL;

    buffer->data_end = NULL;

    buffer->data_len = 0;
}

int circularBufferWrite(circularBuffer* buffer, size_t writeLen)
{
    char* minTail = buffer->readerTails[0]; // find minimal tail (tail closest in front of data_head)

    size_t availableSpace = buffer->data_len; // default is empty buffer

    if (minTail < buffer->data_head) // calculate space left to minimal tail
    {
        availableSpace = (buffer->data_end - buffer->data_head) + (minTail - buffer->data_ptr);
    }
    else if (minTail > buffer->data_head)
    {
        availableSpace = minTail - buffer->data_head;
    }
    
    if (availableSpace < writeLen) // not enough space to write because of a reader
    {
        return -1;
    }

    if (buffer->data_head + writeLen <= buffer->data_end) // data fits until end of the buffer
    {
        buffer->data_head += writeLen;

        if (buffer->data_head == buffer->data_end)
        {
            buffer->data_head = buffer->data_ptr;
        }
    }
    else // data has to be split
    {
        size_t spaceToEndOfBuffer = buffer->data_end - buffer->data_head;

        buffer->data_head = buffer->data_ptr + (writeLen - spaceToEndOfBuffer);
    }

    return 0;
}

int circularBufferRead(circularBuffer* buffer, int readerId, size_t readLen)
{
    if (buffer->data_head > buffer->readerTails[readerId])
    {
        buffer->readerTails[readerId] += readLen;
        return readLen;
    }
    else
    {
        int toEndOfBuffer = (buffer->data_end - buffer->readerTails[readerId]);

        buffer->readerTails[readerId] = buffer->data_ptr + (readLen - toEndOfBuffer);
        return readLen;
    }
}

int circularBufferAvailableData(circularBuffer* buffer, int readerId)
{
    int spaceLeft = 0;

    if (buffer->readerTails[readerId] == NULL)
    {
        return -1;
    }

    if (buffer->readerTails[readerId] < buffer->data_head)
    {
        spaceLeft = buffer->data_head - buffer->readerTails[readerId];
    }
    else if (buffer->readerTails[readerId] > buffer->data_head)
    {
        spaceLeft = (buffer->data_end - buffer->readerTails[readerId]) + (buffer->data_head - buffer->data_ptr);
    }

    return spaceLeft;
}