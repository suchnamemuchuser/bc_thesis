#ifndef CLIENT_C
#define CLIENT_C

#include "client.h"

AppConfig* appConfig;

void writeBufferStatusToFile(const char* filepath, BufferSession* sessions, int deviceCount);

int main(){
    appConfig = loadConfig("clientconfig.json");

    printConfig(appConfig);

    BufferSession* bufferSessions = malloc(sizeof(BufferSession) * appConfig->deviceCount);

    pthread_t producer_tid;
    pthread_t fileConsumer_tid;

    for (int i = 0 ; i < appConfig->deviceCount ; i++)
    {
        circularBufferInit(&bufferSessions[i].buffer, appConfig->bufferSize);
        bufferSessions[i].deviceInfo = appConfig->devices[i];

        pthread_mutex_init(&bufferSessions[i].buffer_lock, NULL);
        pthread_cond_init(&bufferSessions[i].data_available, NULL);

        printf("Starting producer for buffer %d.\n", i+1);
        
        if (pthread_create(&producer_tid, NULL, clientProducerThread, (void*)&bufferSessions[i]) != 0)
        {
            fprintf(stderr, "Failed to create producer thread.\n");
            exit(1);
        }

        printf("Starting file consumer for buffer %d.\n", i+1);
        
        if (pthread_create(&fileConsumer_tid, NULL, bufferFileConsumerThread, (void*)&bufferSessions[i]) != 0)
        {
            fprintf(stderr, "Failed to create file consumer thread.\n");
            exit(1);
        }
    }

    while(true)
    {
        writeBufferStatusToFile(appConfig->bufferStatusFile, bufferSessions, appConfig->deviceCount);
        sleep(60);
    }

    pthread_join(producer_tid, NULL);

    return 0;
}

void writeBufferStatusToFile(const char* filepath, BufferSession* sessions, int deviceCount) 
{
    // "w" mode overwrites the file entirely, ensuring only the latest data is kept
    FILE* file = fopen(filepath, "w");
    if (file == NULL) 
    {
        fprintf(stderr, "Failed to open buffer status file for writing: %s\n", filepath);
        return;
    }

    fprintf(file, "Circular Buffer Status\n\n");

    for (int i = 0; i < deviceCount; i++) 
    {
        BufferSession* session = &sessions[i];
        
        // Device info is accessed outside the lock, as per your comments
        fprintf(file, "Device [%d]: %s (Source: %s, Port: %d)\n", 
                i + 1, 
                session->deviceInfo.name ? session->deviceInfo.name : "Unknown",
                session->deviceInfo.source ? session->deviceInfo.source : "N/A",
                session->deviceInfo.port);

        // Lock the session before checking buffer state and recording info
        pthread_mutex_lock(&session->buffer_lock);

        // 1. Report Recording Status
        if (session->buffer.recordingActive) 
        {
            fprintf(file, "  Status: RECORDING\n");
            fprintf(file, "  Target: %s (ID: %d)\n", 
                    session->recordingInfo.object_name, 
                    session->recordingInfo.id);
            fprintf(file, "  Type:   %s\n", 
                    session->recordingInfo.is_interstellar ? "Interstellar" : "Local");
        } 
        else 
        {
            fprintf(file, "  Status: IDLE\n");
        }

        // 2. Report Buffer Fullness
        size_t capacity = session->buffer.data_len;
        fprintf(file, "  Capacity: %zu bytes\n", capacity);
        
        int active_readers = 0;
        for (int r = 0; r < session->buffer.reader_cnt; r++) 
        {
            // Skip unused readers (unused fields are set to buffer length)
            if (session->buffer.readerOffset[r] >= capacity) 
            {
                continue;
            }
            active_readers++;

            size_t head = session->buffer.data_head_offset;
            size_t tail = session->buffer.readerOffset[r];
            
            // Handle circular wrap-around logic
            size_t unread_bytes;
            if (head >= tail) 
            {
                unread_bytes = head - tail;
            } 
            else 
            {
                unread_bytes = capacity - tail + head;
            }

            float fill_percentage = ((float)unread_bytes / capacity) * 100.0f;

            fprintf(file, "    Reader %d: %zu bytes unread (%.2f%% full)\n", 
                    r, unread_bytes, fill_percentage);
        }
        
        if (active_readers == 0) 
        {
            fprintf(file, "    No active readers registered.\n");
        }

        fprintf(file, "\n");

        // Always unlock before moving to the next buffer
        pthread_mutex_unlock(&session->buffer_lock);
    }
    fclose(file);
}

#endif // CLIENT_C