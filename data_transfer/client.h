#ifndef CLIENT_H
#define CLIENT_H

#include "CircularBuffer.h"
#include "db_transfer.h"
#include "config.h"
#include <pthread.h>

typedef struct bufferSession {
    CircularBuffer buffer;

    int bufferId;

    char description[256];

    // Access to buffer session except device info under mutex!
    pthread_mutex_t buffer_lock;

    pthread_cond_t data_available;

    DbItem recordingInfo;

} BufferSession;

typedef struct {
    BufferSession** sessions;
    int count;
} BufferRegistry;

void* clientProducerThread(void* arg);

void* bufferFileConsumerThread(void* arg);

#endif // CLIENT_H