#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "client.h"

#define BUFFER_SIZE (60 * 3 * 1024 * 1024) // 1 minute of data

extern char* optarg;

void print_usage(const char* prog_name) {
    printf("Usage: %s [-s server_address] [-p port]\n", prog_name);
    printf("  -s  Server address or domain (default: 127.0.0.1)\n");
    printf("  -p  Port number (default: 55555)\n");
}

int main(int argc, char *argv[]) {
    ClientSession session;
    
    // default
    strncpy(session.server_host, "127.0.0.1", sizeof(session.server_host) - 1);
    session.server_host[sizeof(session.server_host) - 1] = '\0';
    session.server_port = 55555;

    int opt;
    while ((opt = getopt(argc, argv, "s:p:h")) != -1)
    {
        switch (opt)
        {
            case 's': // host name
                strncpy(session.server_host, optarg, sizeof(session.server_host) - 1);
                session.server_host[sizeof(session.server_host) - 1] = '\0';
                break;

            case 'p': // port
                session.server_port = atoi(optarg);
                if (session.server_port <= 0 || session.server_port > 65535)
                {
                    fprintf(stderr, "Error: Invalid port number.\n");
                    return 1;
                }
                break;

            case 'h':
                print_usage(argv[0]);
                return 0;

            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    printf("Starting Telescope Client...\n");
    printf("Target Server: %s:%d\n", session.server_host, session.server_port);

    // init
    pthread_mutex_init(&session.buffer_lock, NULL);
    pthread_cond_init(&session.data_available, NULL);
    pthread_cond_init(&session.space_available, NULL);

    if (circularBufferInit(&session.buffer, BUFFER_SIZE) != 0)
    {
        fprintf(stderr, "Failed to initialize circular buffer.\n");
        return 1;
    }

    // start producer
    pthread_t producer_tid;
    if (pthread_create(&producer_tid, NULL, networkProducerThread, (void*)&session) != 0)
    {
        fprintf(stderr, "Failed to create producer thread.\n");
        circularBufferFree(&session.buffer);
        return 1;
    }

    pthread_join(producer_tid, NULL);

    // clean
    printf("Cleaning up and exiting.\n");
    circularBufferFree(&session.buffer);
    pthread_mutex_destroy(&session.buffer_lock);
    pthread_cond_destroy(&session.data_available);
    pthread_cond_destroy(&session.space_available);

    return 0;
}