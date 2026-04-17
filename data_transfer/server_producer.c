#ifndef SERVER_PRODUCER_C
#define SERVER_PRODUCER_C

#define _DEFAULT_SOURCE // fix usleep warning

#include "server.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <libftdi1/ftdi.h>

#define PROD_BUF_SIZ 900
#define FTDI_BUF_LEN 16384
#define FTDI_VENDOR 0x0403
#define FTDI_PRODUCT 0x6014

void run_dummy_producer(BufferSession* bufferSession)
{
    uint8_t* dataBuffer = malloc(PROD_BUF_SIZ);
    if (dataBuffer == NULL)
    {
        fprintf(stderr, "Producer data buffer malloc fail!\n");
        exit(1);
    }

    const char* pattern = "ABCD";
    size_t patternLen = 4;
    size_t i = 0;

    while (i + patternLen <= PROD_BUF_SIZ)
    {
        memcpy(dataBuffer + i, pattern, patternLen);
        i += patternLen;
    }
    if (i < PROD_BUF_SIZ)
    {
        memcpy(dataBuffer + i, pattern, PROD_BUF_SIZ - i);
    }

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
        }
        else
        {
            pthread_cond_broadcast(&bufferSession->data_available);
        }

        pthread_mutex_unlock(&bufferSession->buffer_lock);
        usleep(1000);
    }
    
    free(dataBuffer);
}

void run_ftdi_producer(BufferSession* bufferSession)
{
    // source is index?
    int spec_idx = atoi(bufferSession->deviceInfo.source);
    
    struct ftdi_context *ftdi;
    struct ftdi_device_list *devlist = NULL, *curdev = NULL, *used_dev = NULL;
    int ret, i;

    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed for %s\n", bufferSession->deviceInfo.name);
        return;
    }

    if ((ret = ftdi_usb_find_all(ftdi, &devlist, FTDI_VENDOR, FTDI_PRODUCT)) < 0) {
        fprintf(stderr, "No FTDI devices found for %s\n", bufferSession->deviceInfo.name);
        ftdi_free(ftdi);
        return;
    }

    if (spec_idx >= ret)
    {
        fprintf(stderr, "Requested FTDI index %d, but only found %d devices!\n", spec_idx, ret);
        ftdi_list_free(&devlist);
        ftdi_free(ftdi);
        return;
    }

    for (curdev = devlist, i = 0; curdev != NULL; i++, curdev = curdev->next)
    {
        if (i == spec_idx)
        {
            used_dev = curdev; break;
        }
    }

    if (used_dev == NULL || ftdi_usb_open_dev(ftdi, used_dev->dev) < 0)
    {
        fprintf(stderr, "Unable to open FTDI device %d\n", spec_idx);
        ftdi_list_free(&devlist);
        ftdi_free(ftdi);
        return;
    }
    ftdi_list_free(&devlist);

    // FTDI Hardware Initialization Pipeline
    ftdi_usb_reset(ftdi);
    ftdi_tcioflush(ftdi);
    ftdi_set_bitmode(ftdi, 0xFF, BITMODE_RESET);
    ftdi_set_latency_timer(ftdi, 1);
    ftdi_set_bitmode(ftdi, 0xFF, BITMODE_SYNCFF);

    uint8_t* dataBuffer = malloc(FTDI_BUF_LEN);
    if (dataBuffer == NULL)
    {
        fprintf(stderr, "FTDI data buffer malloc fail!\n");
        return;
    }

    printf("Started FTDI producer for %s on device index %d\n", bufferSession->deviceInfo.name, spec_idx);

    while(true)
    {
        // read up to FTDI_BUF_LEN bytes
        int bytes_read = ftdi_read_data(ftdi, dataBuffer, FTDI_BUF_LEN);
        
        if (bytes_read < 0)
        {
            fprintf(stderr, "FTDI read error on %s\n", bufferSession->deviceInfo.name);
            break; 
        }
        if (bytes_read == 0)
        {
            usleep(100); // sleep a bit if empty
            continue;
        }

        pthread_mutex_lock(&bufferSession->buffer_lock);

        if (bufferSession->buffer.recordingActive == true)
        {
            size_t space = circularBufferWriterSpace(&bufferSession->buffer);

            if (space >= (size_t)bytes_read)
            {
                pthread_mutex_unlock(&bufferSession->buffer_lock);
                circularBufferMemWrite(&bufferSession->buffer, dataBuffer, bytes_read);
                pthread_mutex_lock(&bufferSession->buffer_lock);

                circularBufferConfirmWrite(&bufferSession->buffer, bytes_read);
                pthread_cond_broadcast(&bufferSession->data_available);
            }
            else
            {
                pthread_cond_broadcast(&bufferSession->data_available);
            }
        }
        else
        {
            pthread_cond_broadcast(&bufferSession->data_available);
        }

        pthread_mutex_unlock(&bufferSession->buffer_lock);
    }

    // Teardown if loop breaks
    free(dataBuffer);
    ftdi_disable_bitbang(ftdi);
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
}

void* bufferProducerThread(void* arg)
{
    BufferSession* bufferSession = (BufferSession*)arg; 

    // Check if the source string exists and is not empty
    bool use_dummy_data = true;
    if (bufferSession->deviceInfo.source != NULL && strlen(bufferSession->deviceInfo.source) > 0)
    {
        use_dummy_data = false;
    }

    if (use_dummy_data)
    {
        printf("Source is empty. Starting dummy data producer for: %s\n", bufferSession->deviceInfo.name);
        run_dummy_producer(bufferSession);
    }
    else
    {
        printf("Source is '%s'. Starting FTDI hardware producer for: %s\n", 
                bufferSession->deviceInfo.source, bufferSession->deviceInfo.name);
        run_ftdi_producer(bufferSession);
    }

    return NULL;
}

#endif //SERVER_PRODUCER_C