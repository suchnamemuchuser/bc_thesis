#ifndef CLIENT_H
#define CLIENT_H

#include "CircularBuffer.h"
#include "db_transfer.h"
#include "config.h"
#include <pthread.h>

typedef struct bufferSession {
    circularBuffer buffer;

    // ALL ACCESS TO BUFFERSESSION UNDER MUTEX!
    pthread_mutex_t buffer_lock;

    pthread_cond_t data_available;

    Device deviceInfo;

    DbItem recordingInfo;

} BufferSession;

void* clientProducerThread(void* arg);

void* bufferFileConsumerThread(void* arg);

#endif // CLIENT_H