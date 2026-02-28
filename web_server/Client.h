#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "CircularBuffer.h"
#include "NetworkProtocol.h"

typedef struct {
    uint64_t timestamp;
    char name[256];
    
    circularBuffer buffer;

    pthread_mutex_t buffer_lock;
    pthread_cond_t data_available;
    pthread_cond_t space_available;
    
} ClientSession;