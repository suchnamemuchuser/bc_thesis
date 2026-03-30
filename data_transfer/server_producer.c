#ifndef SERVER_PRODUCER_C
#define SERVER_PRODUCER_C

#define _DEFAULT_SOURCE // fix usleep warning

#include "server.h"

#define PROD_BUF_SIZ 1000

void* bufferProducerThread(void* arg)
{
    BufferSession* bufferSession = (BufferSession*)arg; 


    uint8_t* dataBuffer = malloc(PROD_BUF_SIZ);

    if (dataBuffer == NULL)
    {
        fprintf(stderr, "Producer data buffer malloc fail!\n");
        exit(1);
    }

    const char* pattern = "ABCD";
    size_t patternLen = 4;

    size_t i = 0;
    while (i + patternLen <= PROD_BUF_SIZ) {
        memcpy(dataBuffer + i, pattern, patternLen);
        i += patternLen;
    }

    if (i < PROD_BUF_SIZ) {
        memcpy(dataBuffer + i, pattern, PROD_BUF_SIZ - i);
    }

    //printf("dataBuffer: %.1000s\n", dataBuffer);


    while(true)
    {
        pthread_mutex_lock(&bufferSession->buffer_lock);

        if (bufferSession->buffer.recordingActive == true)
        {
            size_t space = circularBufferWriterSpace(&bufferSession->buffer);

            if (space >= (size_t)PROD_BUF_SIZ)
            {
                pthread_mutex_unlock(&bufferSession->buffer_lock);
                circularBufferMemWrite(&bufferSession->buffer, dataBuffer, PROD_BUF_SIZ);
                pthread_mutex_lock(&bufferSession->buffer_lock);

                circularBufferConfirmWrite(&bufferSession->buffer, PROD_BUF_SIZ);
                pthread_cond_broadcast(&bufferSession->data_available);
            }
            // TODO: print or something for data thrown away
        }

        pthread_mutex_unlock(&bufferSession->buffer_lock);

        usleep(1000);
    }

    return NULL;
}

#endif //SERVER_PRODUCER_C