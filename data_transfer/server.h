#ifndef SERVER_H
#define SERVER_H

#include "CircularBuffer.h"
#include "db_transfer.h"
#include "config.h"
#include <pthread.h>

#define MILLISECOND 1000

typedef struct bufferSession {
    CircularBuffer buffer;

    // Access to buffer session except device info under mutex!
    pthread_mutex_t buffer_lock;

    pthread_cond_t data_available;

    Device deviceInfo;

    DbItem recordingInfo;

} BufferSession;

void* bufferProducerThread(void* arg);

void* bufferFileConsumerThread(void* arg);

void* bufferNetworkConsumerThread(void* arg);

#endif //SERVER_H