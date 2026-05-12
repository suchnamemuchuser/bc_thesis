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
#include <fcntl.h>
#include <sys/sendfile.h>
#include <time.h>

#define RECONNECT_DELAY_SECONDS 5

#define FILE_DEFAULT_READ_SIZE 15000

// defines for data processing
#define HEADER  0xAA
#define H_MASK  0xFC
#define H_MATCH 0x80
#define GAIN_MASK 0xC0
#define DATA_MASK 0x3F

#define SAMPLES_PER_MS 1024
#define BYTES_PER_MS 3075
#define PROCESSED_BYTES_PER_MS (4 * SAMPLES_PER_MS * sizeof(uint64_t) + sizeof(uint64_t)) // + overall ms timestamp

ssize_t recvExact(int sockfd, void *buf, size_t len);

void recursiveMkdir(const char *path);

int sendall(int sockfd, const uint8_t *buf, size_t len);

int getValidMillisecond(BufferSession* session, int consumerId, uint8_t* outBuffer);

static inline uint8_t peekByte(CircularBuffer* buf, int consumerId, size_t offset);

int getEarliestMs(int *streamMs, int streamCount);

void copyImageWithTimestamp(const char* sourcePath, const char* baseDataDir, const char* objectName, time_t eventTime);

static void writeSigprocHeader(FILE *file, const char *object_name, int unix_start_time, const FileConsumerArgs *args);

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
                    printf("NETWORK PROD ENDING RECORDING!!!\n");

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

                writeSigprocHeader(file, bufferSession->recordingInfo.object_name, bufferSession->recordingInfo.rec_start_time, args);
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

void* dataProcessorThread(void* arg)
{
    ClientContext* ctx = (ClientContext*) arg;
    BufferSession* masterBuf = ctx->inputBuffers[0];
    BufferSession* outBuf = ctx->outputBuffer;

    uint64_t msFromStart; // overall counter of milliseconds for each recording
    int currentMs = 10000;

    uint64_t processedData[4 * SAMPLES_PER_MS];
    uint64_t oneStreamSample[SAMPLES_PER_MS];
    int startMilliseconds[4];
    uint8_t streamMilliseconds[4][BYTES_PER_MS];
    int consumerIds[4];

    for (int i = 0 ; i < 4 ; i++) // register for all input buffers
    {
        pthread_mutex_lock(&ctx->inputBuffers[i]->buffer_lock);
        
        consumerIds[i] = ctx->inputBuffers[i]->buffer.reader_cnt;
        ctx->inputBuffers[i]->buffer.readerOffset[consumerIds[i]] = ctx->inputBuffers[i]->buffer.data_head_offset;
        ctx->inputBuffers[i]->buffer.reader_cnt++;
        
        pthread_mutex_unlock(&ctx->inputBuffers[i]->buffer_lock);
    }

    while (true) // master recordings loop
    {
        // 1. Wait for master buffer to get recording info
        pthread_mutex_lock(&masterBuf->buffer_lock);
        while(!masterBuf->buffer.recordingActive)
        {
            pthread_cond_wait(&masterBuf->data_available, &masterBuf->buffer_lock);
        }
        DbItem recording = masterBuf->recordingInfo;
        pthread_mutex_unlock(&masterBuf->buffer_lock);

        // 2. NEW: Wait for ALL streams to be officially recording
        for (int i = 0 ; i < 4 ; i++) 
        {
            pthread_mutex_lock(&ctx->inputBuffers[i]->buffer_lock);
            while(!ctx->inputBuffers[i]->buffer.recordingActive)
            {
                pthread_cond_wait(&ctx->inputBuffers[i]->data_available, &ctx->inputBuffers[i]->buffer_lock);
            }
            pthread_mutex_unlock(&ctx->inputBuffers[i]->buffer_lock);
        }
        
        printf("Processing\n");
        
        // into output
        pthread_mutex_lock(&outBuf->buffer_lock);
        outBuf->recordingInfo = recording;
        outBuf->buffer.recordingActive = true;
        pthread_mutex_unlock(&outBuf->buffer_lock);

        // reset recording ms counter
        msFromStart = 0;

        // get starting milliseconds for each
        for (int i = 0 ; i < ctx->inputBufferCount ; i++)
        {
            getValidMillisecond(ctx->inputBuffers[i], consumerIds[i], streamMilliseconds[i]);
            startMilliseconds[i] = (streamMilliseconds[i][1] & 0x03) << 8 | streamMilliseconds[i][2];
        }

        for (int i = 0 ; i < 4 ; i ++)
        {
            printf("Stream %d has ms %d\n", i, startMilliseconds[i]);
        }

        // get starting ms
        currentMs = getEarliestMs(startMilliseconds, 4);

        printf("Starting with ms %d\n", currentMs);

        bool allBuffersActive = true;

        while (allBuffersActive)
        {
            bzero(processedData, sizeof(processedData));

            for (int i = 0 ; i < 4 ; i++)
            {
                if (((streamMilliseconds[i][1] & 0x03) << 8 | streamMilliseconds[i][2]) == currentMs)
                {
                    // loop through and process, add to output buffer
                    for (int j = 3 ; j < BYTES_PER_MS ; j+=3)
                    {
                        uint8_t gain = (streamMilliseconds[i][j] & (uint8_t)GAIN_MASK) >> 6;
                        uint32_t rawVal = ((uint32_t)(streamMilliseconds[i][j] & (uint8_t)DATA_MASK) << 16) + ((uint32_t)streamMilliseconds[i][j+1] << 8) + (uint32_t)streamMilliseconds[i][j+2];
                        uint64_t realVal = (uint64_t)rawVal << (8*(3-gain));

                        oneStreamSample[j/3 - 1] = realVal; // /3 because j iterates by three, -1 because it skips header bytes
                    }

                    // swap halves
                    memcpy(&processedData[i * SAMPLES_PER_MS], &oneStreamSample[SAMPLES_PER_MS / 2], (SAMPLES_PER_MS / 2) * sizeof(uint64_t));
                    memcpy(&processedData[(i * SAMPLES_PER_MS) + (SAMPLES_PER_MS / 2)], oneStreamSample, (SAMPLES_PER_MS / 2) * sizeof(uint64_t));
                } // else skip, buffer is zeroed out
            }

            // check if space in output buffer and write
            pthread_mutex_lock(&outBuf->buffer_lock);
            if (circularBufferWriterSpace(&outBuf->buffer) > sizeof(processedData) + sizeof(msFromStart)) // space for overall ms
            {
                pthread_mutex_unlock(&outBuf->buffer_lock);
                // write outside of lock
                circularBufferMemWrite(&outBuf->buffer, (uint8_t*)&msFromStart, sizeof(msFromStart)); // write ms first
                pthread_mutex_lock(&outBuf->buffer_lock);
                circularBufferConfirmWrite(&outBuf->buffer, sizeof(msFromStart));
                pthread_mutex_unlock(&outBuf->buffer_lock);

                circularBufferMemWrite(&outBuf->buffer, (uint8_t*)processedData, sizeof(processedData)); // then processed data
                pthread_mutex_lock(&outBuf->buffer_lock);
                circularBufferConfirmWrite(&outBuf->buffer, sizeof(processedData));
                pthread_cond_broadcast(&outBuf->data_available);
            }
            pthread_mutex_unlock(&outBuf->buffer_lock);

            //printf("Wrote ms %d\n", msFromStart);

            for (int i = 0 ; i < 4 ; i++) // update all streams ms, check for end of recording
            {
                int streamMs = (streamMilliseconds[i][1] & 0x03) << 8 | streamMilliseconds[i][2];

                if (streamMs != currentMs) continue; // dont update streams which are ahead

                if(getValidMillisecond(ctx->inputBuffers[i], consumerIds[i], streamMilliseconds[i]) == -1)
                {
                    allBuffersActive = false;
                }
            }

            // update milliseconds
            msFromStart++;
            currentMs = (currentMs + 1) % 1000;
            //printf("Got ms %d\n", msFromStart);
            
            if ((msFromStart % 1000) == 0)
            {
                printf("Seconds from start: %ld\n", msFromStart / 1000);
            }
        } 
        // some buffer ended

        // set output to no recording
        pthread_mutex_lock(&outBuf->buffer_lock);
        outBuf->buffer.recordingActive = false;
        pthread_cond_broadcast(&outBuf->data_available);
        pthread_mutex_unlock(&outBuf->buffer_lock);

        bool someBufferHasData = true;
        while (someBufferHasData)
        {
            someBufferHasData = false;
            for (int i = 0 ; i < 4 ; i++) // drain all buffers
            {
                pthread_mutex_lock(&ctx->inputBuffers[i]->buffer_lock);
                if (ctx->inputBuffers[i]->buffer.recordingActive) someBufferHasData = true;
                int data = circularBufferAvailableData(&ctx->inputBuffers[i]->buffer, consumerIds[i]);
                if (data != 0) circularBufferConfirmRead(&ctx->inputBuffers[i]->buffer, consumerIds[i], data);
                pthread_mutex_unlock(&ctx->inputBuffers[i]->buffer_lock);
            }
        }

    } // end of master recordings loop

    return NULL;
}

void* bufferNetworkConsumerThread(void* arg)
{
    ClientContext* ctx = (ClientContext*) arg;
    NetworkConsumerArgs* args = (NetworkConsumerArgs*) ctx->customArgs;
    BufferSession* bufferSession = ctx->inputBuffers[0];

    // set up server, wait for connection

    int listen_fd, client_fd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;
    int rv;
    char port_str[6];
    int yes = 1; // For setsockopt

    struct sockaddr_storage their_addr;
    socklen_t sin_size;

    printf("Server port: %d\n", args->port);

    snprintf(port_str, sizeof(port_str), "%d", args->port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    printf("Server: Setting up listener on port %s\n", port_str);

    if ((rv = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "server getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            close(listen_fd);
            continue;
        }

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listen_fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return NULL;
    }

    if (listen(listen_fd, 1) == -1)
    {
        perror("listen");
        return NULL;
    }

    printf("Server: Waiting for a connection on port %s...\n", port_str);

    // Main loop: accept connections and handle clients
    // This allows reconnection after a client disconnects
    while (true)
    {
        sin_size = sizeof their_addr;
        client_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &sin_size);
        if (client_fd == -1)
        {
            perror("accept");
            continue;
        }

        printf("Server: Got connection!\n");

        // register with buffer

        pthread_mutex_lock(&bufferSession->buffer_lock);
        int consumerId = bufferSession->buffer.reader_cnt;
        bufferSession->buffer.readerOffset[consumerId] = bufferSession->buffer.data_head_offset;
        bufferSession->buffer.reader_cnt++;
        pthread_mutex_unlock(&bufferSession->buffer_lock);

        // life cycle similar to file consumer

        bool threadRecordingActive = false;
        bool bufferRecordingActive;
        size_t readLen;
        uint8_t* readPtr;
        bool clientConnected = true;

        ProtocolHeader header = {0};
        header.magic = PROTOCOL_MAGIC;

        // Client handling loop: send data until client disconnects
        while (clientConnected)
        {
            readLen = DATA_PACKET_DEFAULT_SIZE;

            pthread_mutex_lock(&bufferSession->buffer_lock);
            bufferRecordingActive = bufferSession->buffer.recordingActive;
            pthread_mutex_unlock(&bufferSession->buffer_lock);

            if (!threadRecordingActive && !bufferRecordingActive) // no recording - sleep
            {
                header.type = PACKET_TYPE_INACTIVE;
                header.value = htonl(0);
                if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1)
                {
                    clientConnected = false;
                }
                else
                {
                    sleep(5);
                }
            }
            else if (!threadRecordingActive && bufferRecordingActive) // start recording - send start packet with object id
            {
                header.type = PACKET_TYPE_START;

                pthread_mutex_lock(&bufferSession->buffer_lock);
                printf("Starting send for %s\n", bufferSession->recordingInfo.object_name);
                header.value = htonl(bufferSession->recordingInfo.id);
                pthread_mutex_unlock(&bufferSession->buffer_lock);

                if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1)
                {
                    clientConnected = false;
                }
            }
            else if (threadRecordingActive && bufferRecordingActive) // recording - send data
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

                header.type = PACKET_TYPE_DATA;

                readLen = circularBufferReadData(&bufferSession->buffer, consumerId, readLen, &readPtr);

                pthread_mutex_unlock(&bufferSession->buffer_lock);

                header.value = htonl(readLen);
                if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1 ||
                    sendall(client_fd, readPtr, readLen) == -1)
                {
                    clientConnected = false;
                }
                else
                {
                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    circularBufferConfirmRead(&bufferSession->buffer, consumerId, readLen);
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
            }
            else if (threadRecordingActive && !bufferRecordingActive) // end recording - drain buffer, send end packet
            {
                pthread_mutex_lock(&bufferSession->buffer_lock);
                int availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);

                // 1. Drain the remaining data in the buffer
                while (availableData > 0)
                {
                    size_t chunkToRead = (availableData > 1000) ? 1000 : availableData;
                    
                    readLen = circularBufferReadData(&bufferSession->buffer, consumerId, chunkToRead, &readPtr);
                    
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                    
                    if (readLen > 0)
                    {
                        header.type = PACKET_TYPE_DATA;
                        header.value = htonl(readLen);
                        if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1 ||
                            sendall(client_fd, readPtr, readLen) == -1)
                        {
                            clientConnected = false;
                            break;
                        }
                    }
                    
                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    circularBufferConfirmRead(&bufferSession->buffer, consumerId, readLen);
                    
                    availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
                }

                if (clientConnected)
                {
                    // 2. Align the tail to the writer
                    bufferSession->buffer.readerOffset[consumerId] = bufferSession->buffer.data_head_offset;
                    pthread_mutex_unlock(&bufferSession->buffer_lock);

                    // 3. Send the end packet
                    header.type = PACKET_TYPE_END;
                    header.value = htonl(0);
                    if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1)
                    {
                        clientConnected = false;
                    }
                }
                else
                {
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
                
                // printf("Ending send for %s\n", bufferSession->recordingInfo.object_name);
            }

            threadRecordingActive = bufferRecordingActive;
        }

        // Client disconnected - clean up and wait for new connection
        close(client_fd);
        printf("Server: Client disconnected. Waiting for new connection...\n");
    }

    return NULL;
}

void* dataAveragerThread(void* arg)
{
    ClientContext* ctx = (ClientContext*) arg;
    DataAveragerArgs* args = ctx->customArgs;
    BufferSession* inBuf = ctx->inputBuffers[0];
    BufferSession* outBuf = ctx->outputBuffer;

    int ratio = args->ratio;

    uint64_t averagedData[4 * SAMPLES_PER_MS];
    uint64_t inputSecond[4 * SAMPLES_PER_MS + 1];

    uint64_t blockTimestampMs; 

    bool threadRecordingActive = false;
    size_t readLen; 
    uint8_t* readPtr;
    uint32_t recordingMs;

    // Register for input buffer
    pthread_mutex_lock(&inBuf->buffer_lock);
    int consumerId = inBuf->buffer.reader_cnt;
    inBuf->buffer.readerOffset[consumerId] = inBuf->buffer.data_head_offset;
    inBuf->buffer.reader_cnt++;
    pthread_mutex_unlock(&inBuf->buffer_lock);

    while (true) // Loop for all recordings
    {
        // Wait for recording to start
        pthread_mutex_lock(&inBuf->buffer_lock);
        while (!inBuf->buffer.recordingActive)
        {
            pthread_cond_wait(&inBuf->data_available, &inBuf->buffer_lock);
        }

        // Copy recording info and set output as active
        DbItem recording = inBuf->recordingInfo;
        pthread_mutex_unlock(&inBuf->buffer_lock);

        printf("Averager started\n");

        pthread_mutex_lock(&outBuf->buffer_lock);
        outBuf->recordingInfo = recording;
        outBuf->buffer.recordingActive = true;
        pthread_mutex_unlock(&outBuf->buffer_lock);

        // Recording started
        threadRecordingActive = true;
        recordingMs = 0;

        int availableData;
        while (threadRecordingActive)
        {
            // Wait for 1 ms of data or end of recording
            pthread_mutex_lock(&inBuf->buffer_lock);
            while (((availableData = circularBufferAvailableData(&inBuf->buffer, consumerId)) < PROCESSED_BYTES_PER_MS) && inBuf->buffer.recordingActive)
            {
                pthread_cond_wait(&inBuf->data_available, &inBuf->buffer_lock);
            }

            if (!inBuf->buffer.recordingActive && availableData < PROCESSED_BYTES_PER_MS)
            {
                // Recording ended and we don't have enough data for a full ms
                pthread_mutex_unlock(&inBuf->buffer_lock);
                threadRecordingActive = false;
                break;
            }

            pthread_mutex_unlock(&inBuf->buffer_lock);

            readLen = circularBufferReadData(&inBuf->buffer, consumerId, PROCESSED_BYTES_PER_MS, &readPtr);

            // Need to copy out because uint64 can be split across buffer bounds
            memcpy(inputSecond, readPtr, readLen);

            if(readLen < PROCESSED_BYTES_PER_MS) // Split read (wrap around)
            {
                int firstPart = readLen;
                int remainingBytes = PROCESSED_BYTES_PER_MS - firstPart;

                pthread_mutex_lock(&inBuf->buffer_lock);
                circularBufferConfirmRead(&inBuf->buffer, consumerId, firstPart);
                pthread_mutex_unlock(&inBuf->buffer_lock);
                
                circularBufferReadData(&inBuf->buffer, consumerId, remainingBytes, &readPtr);
                
                memcpy((uint8_t*)inputSecond + firstPart, readPtr, remainingBytes);

                readLen = remainingBytes; 
            }

            // (If split, this confirms `remainingBytes`. If not, it confirms the full `PROCESSED_BYTES_PER_MS`)
            pthread_mutex_lock(&inBuf->buffer_lock);
            circularBufferConfirmRead(&inBuf->buffer, consumerId, readLen);
            pthread_mutex_unlock(&inBuf->buffer_lock);

            // If ms == 0, zero out our accumulation buffer
            if (recordingMs == 0)
            {
                bzero(averagedData, sizeof(averagedData));

                blockTimestampMs = inputSecond[0] / 1000;
            }

            // Accumulate the 1ms data (offset by 1 because index 0 is the ms timestamp)
            for (int i = 0; i < 4 * SAMPLES_PER_MS; i++)
            {
                averagedData[i] += inputSecond[i + 1];
            }

            recordingMs++;

            // If we have aggregated the target amount of data
            if (recordingMs == ratio)
            {
                // Divide to get the average
                for (int i = 0; i < 4 * SAMPLES_PER_MS; i++)
                {
                    if (ratio > 0) 
                    {
                        averagedData[i] /= ratio;
                    }
                }

                // Write to output buffer
                pthread_mutex_lock(&outBuf->buffer_lock);
                if (circularBufferWriterSpace(&outBuf->buffer) > sizeof(averagedData) + sizeof(blockTimestampMs)) 
                {
                    pthread_mutex_unlock(&outBuf->buffer_lock);
                    
                    circularBufferMemWrite(&outBuf->buffer, (uint8_t*)&blockTimestampMs, sizeof(blockTimestampMs));
                    pthread_mutex_lock(&outBuf->buffer_lock);
                    circularBufferConfirmWrite(&outBuf->buffer, sizeof(blockTimestampMs));
                    pthread_mutex_unlock(&outBuf->buffer_lock);

                    circularBufferMemWrite(&outBuf->buffer, (uint8_t*)averagedData, sizeof(averagedData));
                    pthread_mutex_lock(&outBuf->buffer_lock);
                    circularBufferConfirmWrite(&outBuf->buffer, sizeof(averagedData));
                    pthread_cond_broadcast(&outBuf->data_available);
                    pthread_mutex_unlock(&outBuf->buffer_lock);
                }
                else
                {
                    pthread_mutex_unlock(&outBuf->buffer_lock);
                }

                // Reset for the next block
                recordingMs = 0;
            }
        } // End of recording active loop

        // Recording ended - Set output to no recording
        pthread_mutex_lock(&outBuf->buffer_lock);
        outBuf->buffer.recordingActive = false;
        pthread_cond_broadcast(&outBuf->data_available);
        pthread_mutex_unlock(&outBuf->buffer_lock);

        // Drain any remaining data in the input buffer
        bool inBufHasData = true;
        while (inBufHasData)
        {
            inBufHasData = false;
            pthread_mutex_lock(&inBuf->buffer_lock);
            
            if (inBuf->buffer.recordingActive) inBufHasData = true;
            
            int data = circularBufferAvailableData(&inBuf->buffer, consumerId);
            if (data != 0) 
            {
                circularBufferConfirmRead(&inBuf->buffer, consumerId, data);
            }
            pthread_mutex_unlock(&inBuf->buffer_lock);
        }

    } // End of master recordings loop

    return NULL;
}

void* imageArchiverThread(void* arg)
{
    ClientContext* ctx = (ClientContext*) arg;
    ImageArchiverArgs* args = (ImageArchiverArgs*) ctx->customArgs;
    BufferSession* inBuf = ctx->inputBuffers[0];

    bool wasRecording = false;
    time_t lastHourlySaveTime = 0;
    
    DbItem currentRecInfo; 

    printf("Image archiver thread started. Polling every 1 second.\n");

    while (true)
    {
        pthread_mutex_lock(&inBuf->buffer_lock);
        bool isRecording = inBuf->buffer.recordingActive;
        if (isRecording) {
            currentRecInfo = inBuf->recordingInfo; 
        }
        pthread_mutex_unlock(&inBuf->buffer_lock);

        time_t now = time(NULL);

        if (isRecording && !wasRecording) // recording started
        {
            time_t startTime = (time_t)currentRecInfo.rec_start_time;
            copyImageWithTimestamp(args->sourceImagePath, args->dataDir, currentRecInfo.object_name, startTime);
            
            lastHourlySaveTime = now;
        }
        else if (!isRecording && wasRecording) // recording ended
        {
            copyImageWithTimestamp(args->sourceImagePath, args->dataDir, currentRecInfo.object_name, now);
        }
        else if (isRecording && wasRecording)
        {
            if (now - lastHourlySaveTime >= 3600) // hour passed, save img
            {
                copyImageWithTimestamp(args->sourceImagePath, args->dataDir, currentRecInfo.object_name, now);
                
                lastHourlySaveTime = now;
            }
        }

        wasRecording = isRecording;
        sleep(1);
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

int sendall(int sockfd, const uint8_t *buf, size_t len)
{
    size_t total_sent = 0;

    while (total_sent < len)
    {
        ssize_t sent_now = send(sockfd, buf + total_sent, len - total_sent, MSG_NOSIGNAL);

        if (sent_now == -1)
        {
            if (errno == EPIPE || errno == ECONNRESET)
            {
                fprintf(stderr, "sendall: Connection closed by peer.\n");
            }
            else
            {
                perror("sendall");
            }
            return -1; // Error occurred
        }

        total_sent += sent_now;
    }

    return 0; // Success
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

int getValidMillisecond(BufferSession* session, int consumerId, uint8_t* outBuffer)
{
    while (true)
    {
        pthread_mutex_lock(&session->buffer_lock);

        // wait for 10 ms of data
        int availableData = circularBufferAvailableData(&session->buffer, consumerId);
        while (session->buffer.recordingActive && availableData < (10 * BYTES_PER_MS))
        {
            pthread_cond_wait(&session->data_available, &session->buffer_lock);
            availableData = circularBufferAvailableData(&session->buffer, consumerId);
        }

        // recording stopped, end data processing
        if (!session->buffer.recordingActive)
        {
            pthread_mutex_unlock(&session->buffer_lock);
            return -1; 
        }
        pthread_mutex_unlock(&session->buffer_lock);

        size_t searchPos = 0;
        bool found = false;

        // scan for h1 and h2
        // stop at 8 ms, dont risk going over
        while (searchPos < (8 * BYTES_PER_MS))
        {
            uint8_t h1_b1 = peekByte(&session->buffer, consumerId, searchPos);
            uint8_t h1_b2 = peekByte(&session->buffer, consumerId, searchPos + 1);

            // match first header and second byte mask
            if (h1_b1 == HEADER && (h1_b2 & H_MASK) == H_MATCH)
            {
                
                // match h2 offset by BYTES_PER_MS
                uint8_t h2_b1 = peekByte(&session->buffer, consumerId, searchPos + BYTES_PER_MS);
                uint8_t h2_b2 = peekByte(&session->buffer, consumerId, searchPos + BYTES_PER_MS + 1);

                if (h2_b1 == HEADER && (h2_b2 & H_MASK) == H_MATCH)
                {
                    
                    // found: throw away up to h1
                    if (searchPos > 0)
                    {
                        pthread_mutex_lock(&session->buffer_lock);
                        circularBufferConfirmRead(&session->buffer, consumerId, searchPos);
                        pthread_mutex_unlock(&session->buffer_lock);
                    }

                    // copy
                    uint8_t* ptr;
                    pthread_mutex_lock(&session->buffer_lock);
                    int readLen = circularBufferReadData(&session->buffer, consumerId, BYTES_PER_MS, &ptr);
                    pthread_mutex_unlock(&session->buffer_lock);
                    memcpy(outBuffer, ptr, readLen);

                    // was at end of buffer, split read
                    if (readLen < BYTES_PER_MS)
                    {
                        // confirm first read
                        int firstPart = readLen;
                        pthread_mutex_lock(&session->buffer_lock);
                        circularBufferConfirmRead(&session->buffer, consumerId, firstPart);
                        
                        // read rest
                        circularBufferReadData(&session->buffer, consumerId, BYTES_PER_MS - firstPart, &ptr);
                        pthread_mutex_unlock(&session->buffer_lock);
                        memcpy(outBuffer + firstPart, ptr, BYTES_PER_MS - firstPart);
                        readLen = BYTES_PER_MS - firstPart;
                    }

                    // confirm rest / whole
                    pthread_mutex_lock(&session->buffer_lock);
                    circularBufferConfirmRead(&session->buffer, consumerId, readLen);
                    pthread_mutex_unlock(&session->buffer_lock);
                    
                    return 1;
                }
            }
            searchPos++;
        }

        // no ms found in 8 ms, flush all and continue loop
        pthread_mutex_lock(&session->buffer_lock);
        circularBufferConfirmRead(&session->buffer, consumerId, 8 * BYTES_PER_MS);
        pthread_mutex_unlock(&session->buffer_lock);
    }
}

static inline uint8_t peekByte(CircularBuffer* buf, int consumerId, size_t offset)
{
    return buf->data_ptr[(buf->readerOffset[consumerId] + offset) % buf->data_len];
}

static int compareMs(const void *a, const void *b)
{
    return (*(const int *)a - *(const int *)b);
}

int getEarliestMs(int *streamMs, int streamCount)
{
    if (streamCount <= 0) return -1; 
    if (streamCount == 1) return streamMs[0];

    // sort timestamps
    qsort(streamMs, streamCount, sizeof(int), compareMs);

    int maxGap = -1;
    int startMs = streamMs[0];

    // find largest gap
    for (int i = 0; i < streamCount; i++)
    {
        int gap;
        int candidateStart;

        if (i == streamCount - 1)
        {
            // wrap-around gap (from last to first)
            gap = (1000 - streamMs[i]) + streamMs[0];
            candidateStart = streamMs[0];
        }
        else
        {
            // standart gap
            gap = streamMs[i + 1] - streamMs[i];
            candidateStart = streamMs[i + 1];
        }

        // earliest is following biggest gap
        if (gap > maxGap)
        {
            maxGap = gap;
            startMs = candidateStart;
        }
    }

    return startMs;
}

void copyImageWithTimestamp(const char* sourcePath, const char* baseDataDir, const char* objectName, time_t eventTime)
{
    char dirPath[512];
    char filePath[1024];
    char timeStr[64];
    struct tm timeInfo;

    localtime_r(&eventTime, &timeInfo);
    strftime(timeStr, sizeof(timeStr), "%Y.%m.%d-%H:%M", &timeInfo);

    // Build the directory path and create it
    snprintf(dirPath, sizeof(dirPath), "%s/%s", baseDataDir, objectName);
    recursiveMkdir(dirPath);

    // Build the final file path (e.g., /data/Target_A/2026.03.29-06:24_start.png)
    snprintf(filePath, sizeof(filePath), "%s/%s.png", dirPath, timeStr);

    // Open source
    int source_fd = open(sourcePath, O_RDONLY);
    if (source_fd < 0) {
        perror("Archiver: Failed to open source image");
        return;
    }

    // Open destination
    int dest_fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dest_fd < 0) {
        perror("Archiver: Failed to create destination image");
        close(source_fd);
        return;
    }

    // Get file size and copy via kernel
    struct stat stat_buf;
    fstat(source_fd, &stat_buf);
    sendfile(dest_fd, source_fd, NULL, stat_buf.st_size);

    close(source_fd);
    close(dest_fd);
    printf("Archiver: Saved image %s\n", filePath);
}

static void writeSigprocString(FILE *file, const char *string)
{
    int len = strlen(string);
    fwrite(&len, sizeof(int), 1, file);
    fwrite(string, sizeof(char), len, file);
}

static void writeSigprocInt(FILE *file, const char *name, int value)
{
    writeSigprocString(file, name);
    fwrite(&value, sizeof(int), 1, file);
}

static void writeSigprocDouble(FILE *file, const char *name, double value)
{
    writeSigprocString(file, name);
    fwrite(&value, sizeof(double), 1, file);
}

static void writeSigprocHeader(FILE *file, const char *object_name, int unix_start_time, const FileConsumerArgs *args)
{
    if (file == NULL || args == NULL) return;

    // MJD from timestamp
    double tstart = (unix_start_time / 86400.0) + 40587.0;

    // start header
    writeSigprocString(file, "HEADER_START");

    // ts info
    writeSigprocInt(file, "machine_id", args->machId);
    writeSigprocInt(file, "telescope_id", args->telID);
    writeSigprocInt(file, "data_type", args->dataType);
    
    // name
    writeSigprocString(file, "source_name");
    writeSigprocString(file, object_name);

    // cannot get raj dec.. write zeros
    writeSigprocDouble(file, "src_raj", 0.0);
    writeSigprocDouble(file, "src_dej", 0.0);
    writeSigprocDouble(file, "az_start", 0.0);
    writeSigprocDouble(file, "za_start", 0.0);

    // other params
    writeSigprocInt(file, "barycentric", args->barycentric);
    writeSigprocInt(file, "pulsarcentric", 0);
    writeSigprocDouble(file, "tstart", tstart);
    writeSigprocDouble(file, "tsamp", args->tsamp);
    writeSigprocInt(file, "nbits", args->nbits);
    writeSigprocDouble(file, "fch1", args->fch1);
    writeSigprocDouble(file, "foff", args->foff);
    writeSigprocInt(file, "nchans", args->nchans);
    
    // default ifs and beams to 1
    writeSigprocInt(file, "nifs", 1);
    writeSigprocInt(file, "nbeams", 1);

    // end header
    writeSigprocString(file, "HEADER_END");
}

#endif