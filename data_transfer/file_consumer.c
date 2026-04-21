#ifndef SERVER_FILE_CONSUMER_C
#define SERVER_FILE_CONSUMER_C

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "server.h"

#define FILE_DEFAULT_READ_SIZE 15000

extern AppConfig* appConfig;

void recursive_mkdir(const char *path);

void* bufferFileConsumerThread(void* arg)
{
    BufferSession* bufferSession = (BufferSession*)arg; 

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

                snprintf(dirPath, sizeof(dirPath), "%s/%s/%s",
                         appConfig->dataRootDir,
                         appConfig->dataBaseDir,
                         bufferSession->recordingInfo.object_name);
                        
                recursive_mkdir(dirPath);

                snprintf(filePath, sizeof(filePath), "%s/%s.%s",
                         dirPath,
                         timeStr,
                         bufferSession->deviceInfo.name);
                
                file = fopen(filePath, "wb");

                if (file == NULL) {
                    perror("Failed to open file for writing");
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

void recursive_mkdir(const char *path) {
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

#endif //SERVER_FILE_CONSUMER_C