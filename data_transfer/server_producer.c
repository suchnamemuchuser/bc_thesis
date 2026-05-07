#ifndef SERVER_PRODUCER_C
#define SERVER_PRODUCER_C

#define _DEFAULT_SOURCE // fix usleep warning

#include "server.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <libftdi1/ftdi.h>

#define PROD_BUF_SIZ 3075
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

    // make payload
    for (int i = 0; i < 1024; i++)
    {
        // gain changes every 256 samples: 0, 1, 2, 3
        uint8_t gain = i / 256; 
        
        // values go 10000, 10001, 10002...
        uint32_t raw_val = 10000 + i; 

        size_t offset = 3 + (i * 3);
        
        dataBuffer[offset] = ((gain & 0x03) << 6) | ((raw_val >> 16) & 0x3F);
        dataBuffer[offset + 1] = (raw_val >> 8) & 0xFF;
        dataBuffer[offset + 2] = raw_val & 0xFF;
    }

    while(true)
    {
        // get current time
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint16_t current_ms = tv.tv_usec / 1000;

        // write Header
        dataBuffer[0] = 0xAA;
        dataBuffer[1] = 0x80 | ((current_ms >> 8) & 0x03);
        dataBuffer[2] = current_ms & 0xFF;

        pthread_mutex_lock(&bufferSession->buffer_lock);

        if (bufferSession->buffer.recordingActive == true)
        {
            size_t space = circularBufferWriterSpace(&bufferSession->buffer);

            if (space >= (size_t)PROD_BUF_SIZ)
            {
                // Unlock while copying data into the buffer memory to keep mutex free
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

        // sleep atleas 1.1ms - cannot repeat ms
        usleep(1100); 
    }
    
    free(dataBuffer);
}

void run_ftdi_producer(BufferSession* bufferSession)
{
    const char* target_id = bufferSession->deviceInfo.source;
    
    if (target_id == NULL) 
    {
        fprintf(stderr, "Target FTDI device string is NULL for %s\n", bufferSession->deviceInfo.name);
        return;
    }

    struct ftdi_context *ftdi;
    struct ftdi_device_list *devlist = NULL, *curdev = NULL, *used_dev = NULL;
    int ret;

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

    char manufacturer[128];
    char description[128];
    char serial[128];

    // iterate through devs
    for (curdev = devlist; curdev != NULL; curdev = curdev->next)
    {
        // reset description
        description[0] = '\0';

        if (ftdi_usb_get_strings(ftdi, curdev->dev, manufacturer, sizeof(manufacturer), 
                                 description, sizeof(description), serial, sizeof(serial)) >= 0)
        {
            // check description
            if (strcmp(description, target_id) == 0)
            {
                used_dev = curdev;
                break;
            }
        }
    }

    if (used_dev == NULL) // not found
    {
        fprintf(stderr, "FATAL: Could not find FTDI device with description: '%s'. Exiting.\n", target_id);
        ftdi_list_free(&devlist);
        ftdi_free(ftdi);
        exit(EXIT_FAILURE);
    }

    if (ftdi_usb_open_dev(ftdi, used_dev->dev) < 0)
    {
        fprintf(stderr, "Unable to open FTDI device '%s'\n", target_id);
        ftdi_list_free(&devlist);
        ftdi_free(ftdi);
        exit(EXIT_FAILURE);
    }
    
    ftdi_list_free(&devlist);

    ftdi_usb_reset(ftdi);
    ftdi_tcioflush(ftdi);
    ftdi_set_bitmode(ftdi, 0xFF, BITMODE_RESET);
    ftdi_set_latency_timer(ftdi, 1);
    ftdi_set_bitmode(ftdi, 0xFF, BITMODE_SYNCFF);

    uint8_t* dataBuffer = malloc(FTDI_BUF_LEN);
    if (dataBuffer == NULL)
    {
        fprintf(stderr, "FTDI data buffer malloc fail!\n");
        ftdi_disable_bitbang(ftdi);
        ftdi_usb_close(ftdi);
        ftdi_free(ftdi);
        exit(EXIT_FAILURE);
    }

    printf("Started FTDI producer for %s on device '%s'\n", bufferSession->deviceInfo.name, target_id);

    while(true)
    {
        // read up to FTDI_BUF_LEN bytes
        int bytes_read = ftdi_read_data(ftdi, dataBuffer, FTDI_BUF_LEN);
        
        if (bytes_read < 0)
        {
            fprintf(stderr, "FTDI read error on %s\n", bufferSession->deviceInfo.name);
            break; 
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