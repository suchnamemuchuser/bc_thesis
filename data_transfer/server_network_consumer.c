#ifndef SERVER_NETWORK_CONSUMER_C
#define SERVER_NETWORK_CONSUMER_C

#define _XOPEN_SOURCE 700 // fix incomplete struct addrinfo

// networking includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "NetworkProtocol.h"
#include "server.h"

int sendall(int sockfd, const uint8_t *buf, size_t len);

void* bufferNetworkConsumerThread(void* arg)
{
    BufferSession* bufferSession = (BufferSession*)arg; 

    // set up server, wait for connection

    int listen_fd, client_fd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;
    int rv;
    char port_str[6];
    int yes = 1; // For setsockopt

    struct sockaddr_storage their_addr;
    socklen_t sin_size;

    pthread_mutex_lock(&bufferSession->buffer_lock);
    snprintf(port_str, sizeof(port_str), "%d", bufferSession->deviceInfo.port);
    pthread_mutex_unlock(&bufferSession->buffer_lock);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    printf("Server: Setting up listener on port %s\n", port_str);

    if ((rv = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "server getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            close(listen_fd);
            continue;
        }

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listen_fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return NULL;
    }

    if (listen(listen_fd, 1) == -1)
    {
        perror("listen");
        return NULL;
    }

    printf("Server: Waiting for a connection on port %s...\n", port_str);

    // Main loop: accept connections and handle clients
    // This allows reconnection after a client disconnects
    while (true)
    {
        sin_size = sizeof their_addr;
        client_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &sin_size);
        if (client_fd == -1)
        {
            perror("accept");
            continue;
        }

        printf("Server: Got connection!\n");

        // register with buffer

        pthread_mutex_lock(&bufferSession->buffer_lock);
        int consumerId = bufferSession->buffer.reader_cnt;
        bufferSession->buffer.readerOffset[consumerId] = bufferSession->buffer.data_head_offset;
        bufferSession->buffer.reader_cnt++;
        pthread_mutex_unlock(&bufferSession->buffer_lock);

        // life cycle similar to file consumer

        bool threadRecordingActive = false;
        bool bufferRecordingActive;
        size_t readLen;
        uint8_t* readPtr;
        bool clientConnected = true;

        ProtocolHeader header = {0};
        header.magic = PROTOCOL_MAGIC;

        // Client handling loop: send data until client disconnects
        while (clientConnected)
    {
        readLen = DATA_PACKET_DEFAULT_SIZE;

        pthread_mutex_lock(&bufferSession->buffer_lock);
        bufferRecordingActive = bufferSession->buffer.recordingActive;
        pthread_mutex_unlock(&bufferSession->buffer_lock);

        if (!threadRecordingActive && !bufferRecordingActive) // no recording - sleep
        {
            header.type = PACKET_TYPE_INACTIVE;
            header.value = htonl(0);
                if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1)
                {
                    clientConnected = false;
                }
                else
                {
                    sleep(5);
                }
            }
            else if (!threadRecordingActive && bufferRecordingActive) // start recording - send start packet with object id
            {
                header.type = PACKET_TYPE_START;

                pthread_mutex_lock(&bufferSession->buffer_lock);
                printf("Starting send for %s\n", bufferSession->recordingInfo.object_name);
                header.value = htonl(bufferSession->recordingInfo.id);
                pthread_mutex_unlock(&bufferSession->buffer_lock);

                if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1)
                {
                    clientConnected = false;
                }
            }
            else if (threadRecordingActive && bufferRecordingActive) // recording - send data
            {
                pthread_mutex_lock(&bufferSession->buffer_lock);
                int availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
                while (availableData < readLen && bufferSession->buffer.recordingActive)
                {
                    pthread_cond_wait(&bufferSession->data_available, &bufferSession->buffer_lock);
                    availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
                }

                if (!bufferSession->buffer.recordingActive) {
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                    continue; 
                }

                header.type = PACKET_TYPE_DATA;

                readLen = circularBufferReadData(&bufferSession->buffer, consumerId, readLen, &readPtr);

                pthread_mutex_unlock(&bufferSession->buffer_lock);

                header.value = htonl(readLen);
                if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1 ||
                    sendall(client_fd, readPtr, readLen) == -1)
                {
                    clientConnected = false;
                }
                else
                {
                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    circularBufferConfirmRead(&bufferSession->buffer, consumerId, readLen);
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
            }
            else if (threadRecordingActive && !bufferRecordingActive) // end recording - drain buffer, send end packet
            {
                pthread_mutex_lock(&bufferSession->buffer_lock);
                int availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);

                // 1. Drain the remaining data in the buffer
                while (availableData > 0)
                {
                    size_t chunkToRead = (availableData > 1000) ? 1000 : availableData;
                    
                    readLen = circularBufferReadData(&bufferSession->buffer, consumerId, chunkToRead, &readPtr);
                    
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                    
                    if (readLen > 0)
                    {
                        header.type = PACKET_TYPE_DATA;
                        header.value = htonl(readLen);
                        if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1 ||
                            sendall(client_fd, readPtr, readLen) == -1)
                        {
                            clientConnected = false;
                            break;
                        }
                    }
                    
                    pthread_mutex_lock(&bufferSession->buffer_lock);
                    circularBufferConfirmRead(&bufferSession->buffer, consumerId, readLen);
                    
                    availableData = circularBufferAvailableData(&bufferSession->buffer, consumerId);
                }

                if (clientConnected)
                {
                    // 2. Align the tail to the writer
                    bufferSession->buffer.readerOffset[consumerId] = bufferSession->buffer.data_head_offset;
                    pthread_mutex_unlock(&bufferSession->buffer_lock);

                    // 3. Send the end packet
                    header.type = PACKET_TYPE_END;
                    header.value = htonl(0);
                    if (sendall(client_fd, (const uint8_t*)&header, sizeof(header)) == -1)
                    {
                        clientConnected = false;
                    }
                }
                else
                {
                    pthread_mutex_unlock(&bufferSession->buffer_lock);
                }
                
                // printf("Ending send for %s\n", bufferSession->recordingInfo.object_name);
            }

            threadRecordingActive = bufferRecordingActive;
        }

        // Client disconnected - clean up and wait for new connection
        close(client_fd);
        printf("Server: Client disconnected. Waiting for new connection...\n");
    }

    return NULL;
}

int sendall(int sockfd, const uint8_t *buf, size_t len)
{
    size_t total_sent = 0;

    while (total_sent < len)
    {
        ssize_t sent_now = send(sockfd, buf + total_sent, len - total_sent, MSG_NOSIGNAL);

        if (sent_now == -1)
        {
            if (errno == EPIPE || errno == ECONNRESET)
            {
                fprintf(stderr, "sendall: Connection closed by peer.\n");
            }
            else
            {
                perror("sendall");
            }
            return -1; // Error occurred
        }

        total_sent += sent_now;
    }

    return 0; // Success
}


#endif