#ifndef CLIENT_BEHAVIOURS_H
#define CLIENT_BEHAVIOURS_H

#include <pthread.h>
#include "client.h" // Assuming this contains your BufferSession and BufferRegistry

// --- Specific Client Arguments ---

typedef struct {
    char database[256];
    char source[256];
    int port;
} NetworkProducerArgs;

typedef struct {
    int port;
} NetworkConsumerArgs;

typedef struct {
    char dataDir[256];
    char ext[64];
} FileConsumerArgs;


// This is the single void* passed to EVERY thread
typedef struct {
    int clientId;
    char description[256];

    BufferSession* outputBuffer;     // Will be NULL if output_buffer is -1
    
    BufferSession** inputBuffers;    // Dynamically allocated array of pointers
    int inputBufferCount;

    void* customArgs;                

} ClientContext;

void* networkProducerThread(void* arg);
void* fileConsumerThread(void* arg);
void* dataProcessorThread(void* arg);
void* bufferNetworkConsumerThread(void* arg);
void* dataAveragerThread(void* arg);

#endif // CLIENT_BEHAVIOURS_H