#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "CircularBuffer.h"
#include "NetworkProtocol.h"

typedef struct {
    char server_host[256];
    int server_port;

    uint64_t timestamp;
    char name[256];
    
    circularBuffer buffer;

    pthread_mutex_t buffer_lock;
    pthread_cond_t data_available;
    pthread_cond_t space_available;
    
} ClientSession;

void* networkProducerThread(void* arg);
void* fileConsumerThread(void* arg);

#endif // CLIENT_H