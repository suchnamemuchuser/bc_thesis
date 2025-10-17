#ifndef CIRCULAR_BUFFER_C
#define CIRCULAR_BUFFER_C

#include "CircularBuffer.h"
#include <stdio.h>
#include <string.h> // for memset, strdup
#include <unistd.h> // for arg parsing
#include <stdbool.h>

extern char *optarg;

void printArgError(int errNum);

void printUsage();

int parseArguments(int argc, char* argv[], cb_config* cfg)
{
    int opt;

    memset(cfg, 0, sizeof(cb_config));

    // z = generate zeros
    // g = generate an array of values, write from that
    // f = filename from which to read
    // H = hostname or ip
    // p = port
    // 
    while ((opt = getopt(argc, argv, "zgf:H:hp:")) != -1)
    {
        switch(opt)
        {
            case 'f':
                if (cfg->source != 0)
                {
                    printArgError(1);
                    return 1;
                }
                cfg->source = SOURCE_FILE;
                cfg->filename = strdup(optarg);
                break;

            case 'z':
                cfg->source = SOURCE_ZEROS;
                break;

            case 'g':
                cfg->source = SOURCE_GEN;
                break;

            case 'H':
                cfg->hostname = strdup(optarg);
                break;

            case 'h':
                printUsage();
                exit(0);
                break;
        }
    }

    return 0;
}

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

int circularBufferRead(cb_CircularBuffer* buffer, cb_readerInfo* readerInfo, int dataAmountRequested)
{
    // Lightswitch lock to access reader data

    // Find whether enough data is available

    // If not, return 0 -> upon receiving zero, the parent program must sleep for a bit

    // If there is not enough data to satisfy, but writer has seized writing and there will be no more available, 
    // return amount of data remaining (less than requested, but all thats left)

    // If there is no more data (0 data remaining, writer no longer writing), return -1

    // If enough data available, lightswitch to access data

    // copy data somewhere

    // Leave lightswitch

    // return amount of data read
}

void printArgError(int errNum)
{
    if (errNum == 1)
    {
        fprintf(stderr, "Argument error: must specify exactly one data source (z, g, f)");
    }

    printUsage();
}

void printUsage()
{
    char* helpString = "Usage:\n"
                       "./thing <-h> \n"
                       "";

    printf("%s", helpString);
}

#endif // CIRCULAR_BUFFER_C