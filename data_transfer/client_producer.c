#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#ifndef CLIENT_PRODUCER_C
#define CLIENT_PRODUCER_C

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "NetworkProtocol.h"

#include "client.h"

#define RECONNECT_DELAY_SECONDS 5

extern AppConfig* appConfig;

ssize_t recvExact(int sockfd, void *buf, size_t len);

void* clientProducerThread(void* arg){
    BufferSession* bufferSession = (BufferSession*)arg; 
    
    int sockfd = -1;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%d", bufferSession->deviceInfo.port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    size_t readBufferSize = DATA_PACKET_DEFAULT_SIZE;
    uint8_t *readBuffer = malloc(DATA_PACKET_DEFAULT_SIZE);
    if (readBuffer == NULL)
    {
        fprintf(stderr, "Writer: Failed to allocate read buffer\n");
        exit(1);
    }

    bool connected;

    while (true)
    {
        connected = false;

        // resolve and connect
        printf("Writer: Attempting to connect to %s:%s\n", bufferSession->deviceInfo.source, port_str);
               
        servinfo = NULL; 
        sockfd = -1; // reset socket descriptor

        printf("Resolving dn\n");
        // resolve domain name
        if ((rv = getaddrinfo(bufferSession->deviceInfo.source, port_str, &hints, &servinfo)) != 0)
        {
            fprintf(stderr, "writer getaddrinfo: %s\n", gai_strerror(rv));
            // Resolution failed, skip connection attempt
        }
        else 
        {
            // connect loop
            for (p = servinfo; p != NULL; p = p->ai_next)
            {
                if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
                {
                    perror("writer: socket");
                    continue;
                }
                if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
                {
                    close(sockfd);
                    sockfd = -1;
                    perror("writer: connect");
                    continue;
                }
                connected = true; // Connection successful
                break;
            }

            freeaddrinfo(servinfo);
            
            if (!connected)
            {
                fprintf(stderr, "writer: failed to connect to %s\n", bufferSession->deviceInfo.source);
            }
        }

        if (connected)
        {
            printf("Writer: Connected. Waiting for packets...\n");
            ssize_t bytesReceived;

            while (connected)
            {
                ProtocolHeader header;

                bytesReceived = recvExact(sockfd, &header, sizeof(ProtocolHeader));
                
                header.value = ntohl(header.value);

                if (bytesReceived <= 0) 
                {
                    if (bytesReceived < 0) perror("Writer: recv error on header");
                    else printf("Writer: Connection closed by server.\n");
                    connected = false; 
                    break;
                }

                if (header.magic != PROTOCOL_MAGIC) 
                {
                    fprintf(stderr, "Writer: PROTOCOL DESYNC! Expected magic byte 0xAA, got 0x%02X. Dropping connection to resync.\n", header.magic);
                    connected = false;
                    break;
                }

                if (header.type == PACKET_TYPE_START) // start - set the buffer target info
                {
                    DbItem newRecording = getDbItemById(appConfig->database, header.value);

                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    bufferSession->recordingInfo = newRecording;
                    bufferSession->buffer.recordingActive = true;
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
                else if (header.type == PACKET_TYPE_DATA) // write data into buffer
                {
                    size_t dataToRead = header.value;
                    while (dataToRead > 0)
                    {
                        if (dataToRead > readBufferSize)
                        {
                            bytesReceived = recvExact(sockfd, readBuffer, readBufferSize);
                        }
                        else
                        {
                            bytesReceived = recvExact(sockfd, readBuffer, dataToRead);
                        }

                        pthread_mutex_lock(&bufferSession->buffer_lock);
                        size_t space = circularBufferWriterSpace(&bufferSession->buffer);
                        pthread_mutex_unlock(&bufferSession->buffer_lock);

                        if (space >= (size_t)bytesReceived)
                        {
                            circularBufferMemWrite(&bufferSession->buffer, readBuffer, bytesReceived);
                            pthread_mutex_lock(&bufferSession->buffer_lock);
                            circularBufferConfirmWrite(&bufferSession->buffer, bytesReceived);
                            pthread_cond_broadcast(&bufferSession->data_available);
                            pthread_mutex_unlock(&bufferSession->buffer_lock);
                        }

                        dataToRead -= bytesReceived;
                    }
                }
                else if (header.type == PACKET_TYPE_END) // set end flag
                {
                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    bufferSession->buffer.recordingActive = false;
                    pthread_cond_broadcast(&bufferSession->data_available);
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
                else if (header.type == PACKET_TYPE_INACTIVE) // wait for next read
                {
                    pthread_cond_broadcast(&bufferSession->data_available); // ensure consumers wake
                }
            }
        }

        sleep(RECONNECT_DELAY_SECONDS);
    }







    return NULL;
}


ssize_t recvExact(int sockfd, void *buf, size_t len)
{
    size_t total_received = 0;
    uint8_t *ptr = (uint8_t *)buf;

    while (total_received < len)
    {
        ssize_t bytes = recv(sockfd, ptr + total_received, len - total_received, 0);

        if (bytes <= 0)
        {
            return bytes; // 0 means disconnected, <0 means error
        }

        total_received += bytes;
    }

    return total_received;
}

#endif // CLIENT_PRODUCER_C