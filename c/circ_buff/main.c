#include <stdlib.h>
#include <stdio.h>

#include "CircularBuffer.h"

int main()
{
    cb_CircularBuffer buffer;

    int retVal = circularBufferInit(&buffer, 8);

    if (retVal != 0)
    {
        fprintf(stderr, "Failed to allocate circular buffer, exiting.\n");
        
        return 1;
    }

    printf("Data ptr: %p\n", (void*) buffer.data_ptr);
    printf("Data head: %p\n", (void*) buffer.data_head);
    printf("^ should be the same\n");
    printf("Data length: %zu\n", buffer.data_len);
    printf("Data end: %p\n", (void*) buffer.data_end);

    printf("Data length from pointers = %p\n", (void*) (buffer.data_end - buffer.data_ptr));

    circularBufferWrite(&buffer, "ABCDE", 5);

    printf("Buffer: %s\n", buffer.data_ptr);

    circularBufferWrite(&buffer, "FGHI", 4);

    printf("Buffer: %s\n", buffer.data_ptr);

    circularBufferWrite(&buffer, "Kokosovina123", 13);

    printf("Buffer: %s\n", buffer.data_ptr);


    circularBufferFree(&buffer);

    return 0;
}