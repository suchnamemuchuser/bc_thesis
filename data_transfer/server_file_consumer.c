#ifndef SERVER_FILE_CONSUMER_C
#define SERVER_FILE_CONSUMER_C

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "server.h"

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
        readLen = 1000;

        pthread_mutex_lock(&bufferSession->buffer_lock);
        bufferRecordingActive = bufferSession->buffer.recordingActive;
        pthread_mutex_unlock(&bufferSession->buffer_lock);

        if (!threadRecordingActive && !bufferRecordingActive)  // no recording - sleep
        {
            usleep(10000);
        }
        else if (!threadRecordingActive && bufferRecordingActive) // start recording - open file
        {
            pthread_mutex_lock(&bufferSession->buffer_lock);

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
            pthread_mutex_unlock(&bufferSession->buffer_lock);
        }
        else if (threadRecordingActive && bufferRecordingActive) // recording - write into file
        {
            pthread_mutex_lock(&bufferSession->buffer_lock);
            int availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
            while (availableData < readLen && bufferSession->buffer.recordingActive)
            {
                pthread_cond_wait(&bufferSession->data_available, &bufferSession->buffer_lock);
                availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
            }

            readLen = circularBufferReadData(&bufferSession->buffer, consumerId, readLen, &readPtr);

            pthread_mutex_unlock(&bufferSession->buffer_lock);

            fwrite(readPtr, sizeof(uint8_t), readLen, file);

            pthread_mutex_lock(&bufferSession->buffer_lock);
            circularBufferConfirmRead(&bufferSession->buffer, consumerId, readLen);
            pthread_mutex_unlock(&bufferSession->buffer_lock);
        }
        else if (threadRecordingActive && !bufferRecordingActive) // end recording - close file, align tail to writer
        {
            fclose(file);

            pthread_mutex_lock(&bufferSession->buffer_lock);
            bufferSession->buffer.readerOffset[consumerId] = bufferSession->buffer.data_head_offset;
            pthread_mutex_unlock(&bufferSession->buffer_lock);
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