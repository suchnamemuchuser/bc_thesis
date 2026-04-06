#ifndef CLIENT_C
#define CLIENT_C

#include "client.h"

AppConfig* appConfig;

int main(){
    appConfig = loadConfig("clientconfig.json");

    printConfig(appConfig);

    BufferSession* bufferSessions = malloc(sizeof(BufferSession) * appConfig->deviceCount);

    for (int i = 0 ; i < appConfig->deviceCount ; i++)
    {
        circularBufferInit(&bufferSessions[i].buffer, appConfig->bufferSize);
        bufferSessions[i].deviceInfo = appConfig->devices[i];

        pthread_mutex_init(&bufferSessions[i].buffer_lock, NULL);
        pthread_cond_init(&bufferSessions[i].data_available, NULL);

        printf("Starting producer for buffer %d.\n", i+1);
        pthread_t producer_tid;
        if (pthread_create(&producer_tid, NULL, clientProducerThread, (void*)&bufferSessions[i]) != 0)
        {
            fprintf(stderr, "Failed to create producer thread.\n");
            exit(1);
        }

        printf("Starting file consumer for buffer %d.\n", i+1);
        pthread_t fileConsumer_tid;
        if (pthread_create(&fileConsumer_tid, NULL, bufferFileConsumerThread, (void*)&bufferSessions[i]) != 0)
        {
            fprintf(stderr, "Failed to create file consumer thread.\n");
            exit(1);
        }
    }

    return 0;
}

#endif // CLIENT_C