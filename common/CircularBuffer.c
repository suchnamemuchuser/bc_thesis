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

    buffer->data_head_offset = 0;

    memset(buffer->data_ptr, 48, buffer->data_len); // init buffer with 0s

    buffer->reader_cnt = 0;

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

    buffer->data_len = 0;
}

int circularBufferMemWrite(circularBuffer* cb, const uint8_t* src, size_t len)
{
    if (len == 0)
    {
        return 0;
    }
    
    uint8_t* write_ptr = cb->data_ptr + cb->data_head_offset;

    // Calculate space before end of buffer
    size_t space_to_end = cb->data_len - cb->data_head_offset;

    if (len <= space_to_end)
    {
        // single copy
        memcpy(write_ptr, src, len);
    }
    else
    {
        // two copies.
        size_t first_chunk_len = space_to_end;
        size_t second_chunk_len = len - first_chunk_len;

        memcpy(write_ptr, src, first_chunk_len);

        memcpy(cb->data_ptr, src + first_chunk_len, second_chunk_len);
    }

    return len;
}

int circularBufferConfirmWrite(circularBuffer* cb, size_t writeLen)
{
    cb->data_head_offset = (cb->data_head_offset + writeLen) % cb->data_len;

    return 0;
}

size_t circularBufferWriterSpace(circularBuffer* cb)
{
    size_t minSpace = cb->data_len - 1;

    for (int i = 0 ; i < cb->reader_cnt ; i++)
    {
        size_t tail = cb->readerOffset[i];

        size_t rawWriterSpace = (tail - cb->data_head_offset + cb->data_len) % cb->data_len;

        size_t writerSpace;
    
        if (rawWriterSpace == 0)
        {
            // head == tail: buffer is empty for this reader.
            // writer can write (buffer_size - 1) bytes.
            writerSpace = cb->data_len - 1;
        }
        else
        {
            // number of empty slots.
            // subtract 1 for the guard slot.
            writerSpace = rawWriterSpace - 1;
        }

        if (writerSpace < minSpace)
        {
            minSpace = writerSpace;
        }
    }

    return minSpace;
}

int circularBufferConfirmRead(circularBuffer* cb, int readerId, size_t readLen)
{
    cb->readerOffset[readerId] = (cb->readerOffset[readerId] + readLen) % cb->data_len; // update offset

    return readLen;
}

int circularBufferAvailableData(circularBuffer* cb, int readerId)
{
    int spaceLeft = 0;

    if (cb->readerOffset[readerId] < cb->data_head_offset)
    {
        spaceLeft = cb->data_head_offset - cb->readerOffset[readerId];
    }
    else if (cb->readerOffset[readerId] > cb->data_head_offset)
    {
        spaceLeft = (cb->data_len - cb->readerOffset[readerId]) + cb->data_head_offset;
    }

    return spaceLeft;
}

int circularBufferReadData(circularBuffer* cb, int readerId, size_t readLen, uint8_t** read_ptr)
{
    *read_ptr = cb->data_ptr + cb->readerOffset[readerId];

    if (cb->readerOffset[readerId] < cb->data_head_offset)
    {
        return readLen;
    }

    if ((cb->data_len - cb->readerOffset[readerId]) >= readLen)
    {
        return readLen;
    }

    int spaceToEnd = cb->data_len - cb->readerOffset[readerId];

    return spaceToEnd;
}

void circularBufferPrintStatus(circularBuffer* cb)
{
    if (cb->reader_cnt < 1)
    {
        printf("No readers\n");
        return;
    }
    
    size_t head = cb->data_head_offset;
    size_t tail = cb->readerOffset[0];
    size_t used_bytes = 0;

    if (head >= tail)
    {
        used_bytes = head - tail;
    }
    else
    {
        used_bytes = (cb->data_len - tail) + head;
    }

    size_t free_bytes = cb->data_len - used_bytes;

    float free_percent = ((float)free_bytes / (float)cb->data_len) * 100.0f;

    printf("Buffer free space: %.2f%%\n", free_percent);
}