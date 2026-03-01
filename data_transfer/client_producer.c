#define _XOPEN_SOURCE 700 // Fix incomplete struct addrinfo
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <endian.h>    // For be64toh (Network to Host for 64-bit)

#include "client.h"

#define RECONNECT_DELAY_SECONDS 5
#define DATA_PACKET_SIZE 3 * 256 * 1000

ssize_t recv_exact(int sockfd, void *buf, size_t len)
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

void* networkProducerThread(void* arg)
{
    ClientSession* session = (ClientSession*)arg;

    pthread_t consumer_tid;
    
    int sockfd = -1;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%d", session->server_port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Allocate read buffer
    size_t read_buffer_size = DATA_PACKET_SIZE;
    uint8_t *read_buffer = malloc(read_buffer_size);
    if (read_buffer == NULL)
    {
        fprintf(stderr, "Writer: Failed to allocate read buffer\n");
        return NULL;
    }

    bool connected;

    // connect
    while (true)
    {
        connected = false;

        // resolve and connect
        printf("Writer: Attempting to connect to %s:%s\n", session->server_host, port_str);
               
        servinfo = NULL; 
        sockfd = -1; // reset socket descriptor

        printf("Resolving dn\n");
        // resolve domain name
        if ((rv = getaddrinfo(session->server_host, port_str, &hints, &servinfo)) != 0)
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
                fprintf(stderr, "writer: failed to connect to %s\n", session->server_host);
            }

            printf("Connected?\n");
        }
        
        if (connected)
        {
            printf("Writer: Connected. Waiting for packets...\n");
            ssize_t bytes_received;

            while (connected) 
            {
                ProtocolHeader header;

                bytes_received = recv_exact(sockfd, &header, sizeof(ProtocolHeader));

                if (bytes_received <= 0) 
                {
                    if (bytes_received < 0) perror("Writer: recv error on header");
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

                uint16_t payload_length = ntohl(header.length);

                if (header.type == PACKET_TYPE_START) 
                {
                    StartPayload start_data;
                    
                    // Read the payload
                    bytes_received = recv_exact(sockfd, &start_data, sizeof(StartPayload));
                    if (bytes_received <= 0) {
                        connected = false; break;
                    }

                    printf("Writer: START packet received. Name: %s, Time: %lu\n", start_data.name, start_data.timestamp);
                    
                    pthread_mutex_lock(&session->buffer_lock);
                    session->buffer.writerFinished = false; // Reset for the new recording
                    pthread_mutex_unlock(&session->buffer_lock);

                    memset(session->name, 0, 256);
                    strcpy(session->name, start_data.name);
                    session->timestamp = be64toh(start_data.timestamp);

                    printf("Producer: Spawning Consumer thread...\n");
                    if (pthread_create(&consumer_tid, NULL, fileConsumerThread, (void*)session) != 0)
                    {
                        perror("Producer: Failed to create Consumer thread");
                        connected = false; 
                        break; // Drop connection to resync, we can't record without a consumer!
                    }

                }
                else if (header.type == PACKET_TYPE_DATA) 
                {
                    // Ensure we don't read more than our buffer can hold
                    size_t bytes_to_read = payload_length;
                    if (bytes_to_read > read_buffer_size) {
                        fprintf(stderr, "Writer: Warning! Payload (%u) larger than read buffer (%zu)\n", payload_length, read_buffer_size);
                        bytes_to_read = read_buffer_size; // Cap it to avoid overflow
                    }

                    // Read exactly 'bytes_to_read' bytes of data
                    bytes_received = recv_exact(sockfd, read_buffer, bytes_to_read);
                    if (bytes_received <= 0) {
                        connected = false; break;
                    }

                    // --- Your Existing Buffer Writing Logic ---
                    pthread_mutex_lock(&session->buffer_lock);
                    size_t space = circularBufferWriterSpace(&session->buffer);

                    if (space >= (size_t)bytes_received)
                    {
                        circularBufferMemWrite(&session->buffer, read_buffer, bytes_received);
                    }
                    else
                    {
                        fprintf(stderr, "Writer: Buffer full! Dropping %zd bytes.\n", bytes_received);
                    }

                    // Wake up any waiting reader threads
                    circularBufferConfirmWrite(&session->buffer, bytes_received);
                    pthread_cond_broadcast(&session->data_available);
                    pthread_mutex_unlock(&session->buffer_lock);
                }
                else if (header.type == PACKET_TYPE_END) 
                {
                    printf("Writer: END packet received. Signaling readers to finish.\n");
                    
                    // No payload to read for END packets (assuming length is 0)
                    
                    pthread_mutex_lock(&session->buffer_lock);
                    session->buffer.writerFinished = true;
                    pthread_cond_broadcast(&session->data_available); 
                    pthread_mutex_unlock(&session->buffer_lock);

                    printf("Producer: Waiting for Consumer to flush to disk...\n");
                    pthread_join(consumer_tid, NULL);
                    printf("Producer: Consumer successfully finished. Ready for next START.\n");
                }
                else 
                {
                    fprintf(stderr, "Writer: Unknown packet type 0x%02X received!\n", header.type);
                    // You might want to just read and discard `payload_length` bytes here 
                    // so you don't lose your place in the TCP stream.
                }

            } // End of Read loop
            
            // Connection was lost, close the bad socket
            if (sockfd != -1) {
                close(sockfd);
                sockfd = -1; 
            }
        }

        sleep(RECONNECT_DELAY_SECONDS);
    }
    
    return NULL;
}