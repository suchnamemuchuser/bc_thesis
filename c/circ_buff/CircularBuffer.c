#ifndef CIRCULAR_BUFFER_C
#define CIRCULAR_BUFFER_C

#include "CircularBuffer.h"
#include <stdio.h>
#include <string.h>

int circularBufferInit(cb_CircularBuffer* buffer, int bufferSize)
{
    buffer->data_ptr = (char*) malloc((size_t) bufferSize);

    if(buffer->data_ptr == NULL) // check for malloc fail
    {
        return 1;
    }

    buffer->data_len = bufferSize; // set size

    buffer->data_head = buffer->data_ptr; // set head

    buffer->data_end = buffer->data_ptr + buffer->data_len; // set end

    memset(buffer->data_ptr, 0, buffer->data_len); // init buffer with 0s

    return 0;
};

void circularBufferFree(cb_CircularBuffer* buffer)
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

int circularBufferWrite(cb_CircularBuffer* buffer, char* data, size_t writeLen)
{
    if (writeLen > buffer->data_len) // insufficient buffer size
    {
        fprintf(stderr, "Failed to write data, insufficient buffer size.\n");

        return 1;
    }

    size_t spaceLeft = (size_t) (buffer->data_end - buffer->data_head);

    if(spaceLeft >= writeLen) // enough space before end of buffer
    {
        memcpy((void*) buffer->data_head, (void*) data, writeLen); // write data

        buffer->data_head = buffer->data_head + writeLen; // move data head

        if(buffer->data_head == buffer->data_end) // data fit exactly to the end
        {
            buffer->data_head = buffer->data_ptr; // head is now at the beginning
        }
    }
    else // write data has to be split
    {
        memcpy((void*) buffer->data_head, (void*) data, spaceLeft); // copy untill end of buffer

        memcpy((void*) buffer->data_ptr, (void*) (data + spaceLeft), (writeLen - spaceLeft)); // copy rest of data to beginning of buffer

        buffer->data_head = buffer->data_ptr + (writeLen - spaceLeft); // move head
    }

    printf("Wrote %zu bytes, head now at offset %ld\n", writeLen, buffer->data_head - buffer->data_ptr);

    return 0;
}

#endif // CIRCULAR_BUFFER_C