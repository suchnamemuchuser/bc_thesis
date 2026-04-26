#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#ifndef CLIENT_BEHAVIOURS_C
#define CLIENT_BEHAVIOURS_C

#include "client_behaviours.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "NetworkProtocol.h"

#define RECONNECT_DELAY_SECONDS 5

#define FILE_DEFAULT_READ_SIZE 15000

ssize_t recvExact(int sockfd, void *buf, size_t len);

void recursiveMkdir(const char *path);

void* networkProducerThread(void* arg){
    ClientContext* ctx = (ClientContext*) arg;
    NetworkProducerArgs* args = (NetworkProducerArgs*) ctx->customArgs;
    BufferSession* bufferSession = ctx->outputBuffer;
    
    int sockfd = -1;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%d", args->port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    size_t readBufferSize = DATA_PACKET_DEFAULT_SIZE;
    uint8_t *readBuffer = malloc(DATA_PACKET_DEFAULT_SIZE);
    if (readBuffer == NULL)
    {
        fprintf(stderr, "Writer: Failed to allocate read buffer\n");
        exit(1);
    }

    bool connected;

    while (true)
    {
        connected = false;

        // resolve and connect
        printf("Writer: Attempting to connect to %s:%s\n", args->source, port_str);
               
        servinfo = NULL; 
        sockfd = -1; // reset socket descriptor

        printf("Resolving dn\n");
        // resolve domain name
        if ((rv = getaddrinfo(args->source, port_str, &hints, &servinfo)) != 0)
        {
            fprintf(stderr, "writer getaddrinfo: %s\n", gai_strerror(rv));
            // Resolution failed, skip connection attempt
        }
        else 
        {
            // connect loop
            for (p = servinfo; p != NULL; p = p->ai_next)
            {
                if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
                {
                    perror("writer: socket");
                    continue;
                }
                if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
                {
                    close(sockfd);
                    sockfd = -1;
                    perror("writer: connect");
                    continue;
                }
                connected = true; // Connection successful
                break;
            }

            freeaddrinfo(servinfo);
            
            if (!connected)
            {
                fprintf(stderr, "writer: failed to connect to %s\n", args->source);
            }
        }

        if (connected)
        {
            printf("Writer: Connected. Waiting for packets...\n");
            ssize_t bytesReceived;

            while (connected)
            {
                ProtocolHeader header;

                bytesReceived = recvExact(sockfd, &header, sizeof(ProtocolHeader));
                
                header.value = ntohl(header.value);

                if (bytesReceived <= 0) 
                {
                    if (bytesReceived < 0) perror("Writer: recv error on header");
                    else printf("Writer: Connection closed by server.\n");
                    connected = false; 
                    break;
                }

                if (header.magic != PROTOCOL_MAGIC) 
                {
                    fprintf(stderr, "Writer: PROTOCOL DESYNC! Expected magic byte 0xAA, got 0x%02X. Dropping connection to resync.\n", header.magic);
                    connected = false;
                    break;
                }

                if (header.type == PACKET_TYPE_START) // start - set the buffer target info
                {
                    DbItem newRecording = getDbItemById(args->database, header.value);

                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    bufferSession->recordingInfo = newRecording;
                    bufferSession->buffer.recordingActive = true;
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
                else if (header.type == PACKET_TYPE_DATA) // write data into buffer
                {
                    size_t dataToRead = header.value;
                    while (dataToRead > 0)
                    {
                        if (dataToRead > readBufferSize)
                        {
                            bytesReceived = recvExact(sockfd, readBuffer, readBufferSize);
                        }
                        else
                        {
                            bytesReceived = recvExact(sockfd, readBuffer, dataToRead);
                        }

                        pthread_mutex_lock(&bufferSession->buffer_lock);
                        size_t space = circularBufferWriterSpace(&bufferSession->buffer);
                        pthread_mutex_unlock(&bufferSession->buffer_lock);

                        if (space >= (size_t)bytesReceived)
                        {
                            circularBufferMemWrite(&bufferSession->buffer, readBuffer, bytesReceived);
                            pthread_mutex_lock(&bufferSession->buffer_lock);
                            circularBufferConfirmWrite(&bufferSession->buffer, bytesReceived);
                            pthread_cond_broadcast(&bufferSession->data_available);
                            pthread_mutex_unlock(&bufferSession->buffer_lock);
                        }

                        dataToRead -= bytesReceived;
                    }
                }
                else if (header.type == PACKET_TYPE_END) // set end flag
                {
                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    bufferSession->buffer.recordingActive = false;
                    bufferSession->recordingInfo = (DbItem){0};
                    pthread_cond_broadcast(&bufferSession->data_available);
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
                else if (header.type == PACKET_TYPE_INACTIVE) // wait for next read
                {
                    pthread_cond_broadcast(&bufferSession->data_available); // ensure consumers wake
                }
            }
        }

        sleep(RECONNECT_DELAY_SECONDS);
    }

    return NULL;
}


void* bufferFileConsumerThread(void* arg)
{
    ClientContext* ctx = (ClientContext*) arg;
    FileConsumerArgs* args = (FileConsumerArgs*) ctx->customArgs;
    BufferSession* bufferSession = ctx->inputBuffers[0];

    char dirPath[1024];
    char filePath[2048];
    FILE* file = NULL;

    char timeStr[20];
    struct tm timeInfo;

    size_t readLen; 
    uint8_t* readPtr;

    pthread_mutex_lock(&bufferSession->buffer_lock);
    int consumerId = bufferSession->buffer.reader_cnt;
    bufferSession->buffer.readerOffset[consumerId] = bufferSession->buffer.data_head_offset;
    bufferSession->buffer.reader_cnt++;
    pthread_mutex_unlock(&bufferSession->buffer_lock);

    bool threadRecordingActive = false;
    bool bufferRecordingActive;

    while (true)
    {
        readLen = FILE_DEFAULT_READ_SIZE;

        pthread_mutex_lock(&bufferSession->buffer_lock);
        bufferRecordingActive = bufferSession->buffer.recordingActive;
        pthread_mutex_unlock(&bufferSession->buffer_lock);

        if (!threadRecordingActive && !bufferRecordingActive)  // no recording - sleep
        {
            usleep(10000);
        }
        else if (!threadRecordingActive && bufferRecordingActive) // start recording - open file if record flag is set
        {
            pthread_mutex_lock(&bufferSession->buffer_lock);

            // Check record flag under mutex
            bool shouldRecordToFile = bufferSession->recordingInfo.record;

            if (shouldRecordToFile) {
                time_t rawTime = (time_t)bufferSession->recordingInfo.rec_start_time;
                localtime_r(&rawTime, &timeInfo);
                strftime(timeStr, sizeof(timeStr), "%Y.%m.%d-%H:%M", &timeInfo);

                snprintf(dirPath, sizeof(dirPath), "%s/%s",
                         args->dataDir,
                         bufferSession->recordingInfo.object_name);
                        
                recursiveMkdir(dirPath);

                snprintf(filePath, sizeof(filePath), "%s/%s.%s",
                         dirPath,
                         timeStr,
                         args->ext);
                
                file = fopen(filePath, "wb");

                if (file == NULL) {
                    perror("Failed to open file for writing");
                    fprintf(stderr, "Dir: %s\n", dirPath);
                    fprintf(stderr, "File: %s\n", filePath);
                    exit(1);
                }

                printf("File opened successfully: %s\n", filePath);
            }

            pthread_mutex_unlock(&bufferSession->buffer_lock);
        }
        else if (threadRecordingActive && bufferRecordingActive) // recording - write into file if opened
        {
            pthread_mutex_lock(&bufferSession->buffer_lock);
            int availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
            while (availableData < readLen && bufferSession->buffer.recordingActive)
            {
                pthread_cond_wait(&bufferSession->data_available, &bufferSession->buffer_lock);
                availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
            }

            if (!bufferSession->buffer.recordingActive) {
                pthread_mutex_unlock(&bufferSession->buffer_lock);
                continue; 
            }

            readLen = circularBufferReadData(&bufferSession->buffer, consumerId, readLen, &readPtr);

            pthread_mutex_unlock(&bufferSession->buffer_lock);

            // Only write to file if it was opened (record flag was set)
            if (file != NULL) {
                fwrite(readPtr, sizeof(uint8_t), readLen, file);
            }
            // If file is NULL (record flag was 0), skip writing but still consume the data

            pthread_mutex_lock(&bufferSession->buffer_lock);
            circularBufferConfirmRead(&bufferSession->buffer, consumerId, readLen);
            pthread_mutex_unlock(&bufferSession->buffer_lock);
        }
        else if (threadRecordingActive && !bufferRecordingActive) // end recording - close file if opened, align tail to writer
        {
            pthread_mutex_lock(&bufferSession->buffer_lock);
            int availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);

            while (availableData > 0)
            {
                size_t chunkToRead = (availableData > 1000) ? 1000 : availableData;
                
                readLen = circularBufferReadData(&bufferSession->buffer, consumerId, chunkToRead, &readPtr);
                
                pthread_mutex_unlock(&bufferSession->buffer_lock);
                
                // Only write to file if it was opened (record flag was set)
                if (file != NULL && readLen > 0) {
                    fwrite(readPtr, sizeof(uint8_t), readLen, file);
                }
                
                pthread_mutex_lock(&bufferSession->buffer_lock);
                circularBufferConfirmRead(&bufferSession->buffer, consumerId, readLen);
                
                availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
            }

            bufferSession->buffer.readerOffset[consumerId] = bufferSession->buffer.data_head_offset;
            pthread_mutex_unlock(&bufferSession->buffer_lock);

            if (file != NULL) {
                fclose(file);
                file = NULL;
            }
        }

        threadRecordingActive = bufferRecordingActive;
    }

    return NULL;
}


ssize_t recvExact(int sockfd, void *buf, size_t len)
{
    size_t total_received = 0;
    uint8_t *ptr = (uint8_t *)buf;

    while (total_received < len)
    {
        ssize_t bytes = recv(sockfd, ptr + total_received, len - total_received, 0);

        if (bytes <= 0)
        {
            return bytes; // 0 means disconnected, <0 means error
        }

        total_received += bytes;
    }

    return total_received;
}

void recursiveMkdir(const char *path) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU); // Create parent level
            *p = '/';
        }
    }
    mkdir(tmp, S_IRWXU); // Create final level
}

#endif