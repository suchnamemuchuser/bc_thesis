#include <stdio.h>
//#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <string.h> // For memset
#include <errno.h>

// networking includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "CircularBuffer.h"
#include "ArgParser.h"

pthread_mutex_t buffer_lock;

pthread_cond_t data_available;

pthread_cond_t space_available;

circularBuffer buffer;

argParser arguments;

void* writerBehaviour(void* arg);

void* writerWriteFromFile(void* arg);

void* readerBehaviour(void* arg);

void* readerWriteToFile(void* arg);

void* readerWriteToNetwork(void* arg);

int sendall(int sockfd, const uint8_t *buf, size_t len);

int main(int argc, char* argv[]){

    arguments = optargArguments(argc, argv);

    srand(time(NULL));

    // init mutex, conds
    pthread_mutex_init(&buffer_lock, NULL);
    pthread_cond_init(&data_available, NULL);
    pthread_cond_init(&space_available, NULL);

    circularBufferInit(&buffer, arguments.bufferSize);

    printf("Buffer init:\n");
    printf("Pointer: %p\n", buffer.data_ptr);
    printf("Length:  %ld\n", buffer.data_len);
    printf("Offset:  %ld\n", buffer.data_head_offset);

    FILE *reader_fptr = NULL;
    FILE *writer_fptr = NULL;


    // open files if needed (should always be)

    if (arguments.dataSource == DATA_TYPE_FILE)
    {
        reader_fptr = fopen(arguments.dataSourceFilename, "r");

        if (reader_fptr == NULL)
        {
            perror("Error opening file");
            return 1;
        }
    }
    
    if (arguments.dataDestination == DATA_TYPE_FILE)
    {
        writer_fptr = fopen(arguments.dataDestFilename, "w");
        if (writer_fptr == NULL)
        {
            perror("Error opening output file");
            if (reader_fptr) fclose(reader_fptr);
            return 1;
        }
    }


    // create thread identifiers

    pthread_t writer_thread;
    pthread_t reader_thread1;

    printf("Starting threads...\n");

    // start reader thread first

    if (arguments.dataDestination == DATA_TYPE_FILE)
    {
        if (pthread_create(&reader_thread1, NULL, readerWriteToFile, (void*) writer_fptr))
        {
            fprintf(stderr, "Error creating reader thread\n");
            return 1;
        }
    }
    else if (arguments.dataDestination == DATA_TYPE_NETWORK)
    {
        if (pthread_create(&reader_thread1, NULL, readerWriteToNetwork, NULL))
        {
            fprintf(stderr, "Error creating reader thread\n");
            return 1;
        }
    }

    // then writer thread

    if (pthread_create(&writer_thread, NULL, writerWriteFromFile, (void*) reader_fptr)) 
    {
        fprintf(stderr, "Error creating writer thread\n");

        pthread_cancel(reader_thread1);
        return 1;
    }
    
    // wait for threads

    pthread_join(writer_thread, NULL);

    printf("Writer finished. Cleaning up.\n");

    fclose(reader_fptr);

    pthread_join(reader_thread1, NULL);

    if (writer_fptr != NULL) fclose(writer_fptr);

    // destroy mutex, conds
    pthread_mutex_destroy(&buffer_lock);
    pthread_cond_destroy(&data_available);
    pthread_cond_destroy(&space_available);

    circularBufferFree(&buffer);

    freeArgs(arguments);

    return 0;
}

void* writerBehaviour(void* arg) // will be in a while loop?
{
    time_t start = time(NULL);
    time_t end = start + 10;
    size_t dataSize = 30;

    while (end > time(NULL))
    {
        sleep(1); // writing 30 bytes every second

        pthread_mutex_lock(&buffer_lock);

        // if no space, print error and discard data
        size_t space = circularBufferWriterSpace(&buffer);

        if (space < dataSize)
        {
            printf("Not enough space for data\n");
            continue;
        }

        circularBufferConfirmWrite(&buffer, 30);

        pthread_cond_broadcast(&data_available);

        pthread_mutex_unlock(&buffer_lock);

        printf("Wrote %d of data.\n", 30);
    }


    // Writer is finished, set flag
    pthread_mutex_lock(&buffer_lock);

    buffer.writerFinished = true;

    pthread_mutex_unlock(&buffer_lock);

    return NULL;
}

void* writerWriteFromFile(void* arg)
{
    time_t start = time(NULL);
    time_t end = start + 10;

    uint8_t *read_buffer = malloc(arguments.dataRate); // maximum read size

    FILE* file = (FILE*) arg;

    int rnd;

    size_t rndcount;

    while (end > time(NULL))
    {
        rnd = rand() % 1000; // get range from 0 to 999 - 

        usleep(rnd * 1000); // in microseconds

        // calculate appropriate amound of data
        rndcount = (rnd * arguments.dataRate + 500) / 1000; // + 500 rounds up

        if (rndcount > arguments.dataRate)
        {
            rndcount = arguments.dataRate;
        }

        if (rndcount == 0)
        {
            continue;
        }

        printf("reading %d data\n", rndcount);

        int read_count = fread(read_buffer, sizeof(uint8_t), rndcount, file);

        pthread_mutex_lock(&buffer_lock);

        // if no space, print error and discard data
        size_t space = circularBufferWriterSpace(&buffer);

        pthread_mutex_unlock(&buffer_lock);

        if (space >= read_count)
        {
            circularBufferMemWrite(&buffer, read_buffer, read_count);
        }

        // check EOF
        if (feof(file))
        {
            break; // Exit the loop
        }

        pthread_mutex_lock(&buffer_lock);

        pthread_cond_broadcast(&data_available);

        pthread_mutex_unlock(&buffer_lock);

        printf("Wrote %d bytes of data.\n", read_count);
    }

    pthread_mutex_lock(&buffer_lock);

    buffer.writerFinished = true;

    pthread_cond_broadcast(&data_available);

    pthread_mutex_unlock(&buffer_lock);

    return NULL;
}

void* readerBehaviour(void* arg) // example function
{

    size_t data_to_read = 100;
    int my_reader_id = 0;

    size_t availableData;

    pthread_mutex_lock(&buffer_lock);

    //buffer.readerTails[my_reader_id] = buffer.data_head;
    buffer.readerOffset[my_reader_id] = buffer.data_head_offset;
    buffer.reader_cnt = 1;
    
    pthread_mutex_unlock(&buffer_lock);

    while (1)
    {
        pthread_mutex_lock(&buffer_lock);

        while (((availableData = circularBufferAvailableData(&buffer, my_reader_id)) < data_to_read) && (buffer.writerFinished == false))
        {
            printf("Available data: %ld\n", availableData);

            pthread_cond_wait(&data_available, &buffer_lock);
        }
        
        printf("Writer finished: %s\n", buffer.writerFinished ? "true" : "false");

        pthread_mutex_unlock(&buffer_lock);

        // This is where data is processed

        // mutes lock again

        // update tail (maybe atomic)

        // mutex unlock

        pthread_mutex_lock(&buffer_lock);

        int readCnt = circularBufferConfirmRead(&buffer, my_reader_id, data_to_read);

        pthread_mutex_unlock(&buffer_lock);

        printf("Read %d of data.\n", readCnt);

        sleep(1);
    }

    return NULL;
}

void* readerWriteToFile(void* arg)
{
    size_t availableData;
    size_t read_len; 

    FILE *fptr = (FILE*) arg;

    // set values
    size_t data_to_read = 100;
    

    // get reader buffer
    //uint8_t* readerBuffer = malloc(data_to_read); // no reader buffer, get pointer to data

    uint8_t* read_ptr;

    // init buffer reader info
    pthread_mutex_lock(&buffer_lock);

    //buffer.readerTails[my_reader_id] = buffer.data_head;
    int my_reader_id = buffer.reader_cnt;
    buffer.readerOffset[my_reader_id] = buffer.data_head_offset;
    buffer.reader_cnt++;

    pthread_mutex_unlock(&buffer_lock);

    while(1)
    {
        pthread_mutex_lock(&buffer_lock);
        // find if enough data or writer finished
        while (((availableData = circularBufferAvailableData(&buffer, my_reader_id)) < data_to_read) && (buffer.writerFinished == false))
        {
            printf("Available data: %ld\n", availableData);

            pthread_cond_wait(&data_available, &buffer_lock);
        }
        
        // enough data or writer finished

        if (availableData == 0) // writer finished and no data to read
        {
            pthread_mutex_unlock(&buffer_lock);

            break;
        }

        if (availableData < data_to_read)
        {
            read_len = availableData;
        }
        else
        {
            read_len = data_to_read;
        }

        // get data
        read_len = circularBufferReadData(&buffer, my_reader_id, read_len, &read_ptr);

        pthread_mutex_unlock(&buffer_lock);



        fwrite(read_ptr, sizeof(uint8_t), read_len, fptr);



        pthread_mutex_lock(&buffer_lock);

        circularBufferConfirmRead(&buffer, my_reader_id, read_len);

        pthread_mutex_unlock(&buffer_lock);

        printf("Read %ld of data.\n", read_len);
    }

    printf("Reader thread finished.\n");

    return NULL;
}

void* readerWriteToNetwork(void* arg)
{
    // network stuff
    int sockfd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;
    int rv;
    char port_str[6];

    // convert port to string
    snprintf(port_str, sizeof(port_str), "%d", arguments.destPort);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // Use IPv4 as requested
    hints.ai_socktype = SOCK_STREAM; // Use TCP

    printf("Reader: Resolving %s:%s\n", arguments.dataDestString, port_str);
    if ((rv = getaddrinfo(arguments.dataDestString, port_str, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "reader getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        // Create a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("reader: socket");
            continue;
        }

        // Connect to the server
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd); // Close this socket, try the next
            perror("reader: connect");
            continue;
        }

        break; // If we get here, we successfully connected
    }

    if (p == NULL)
    {
        fprintf(stderr, "reader: failed to connect to %s\n", arguments.dataDestString);
        freeaddrinfo(servinfo); // Free the address info
        return NULL;
    }

    freeaddrinfo(servinfo);

    printf("Reader connected to %s:%d\n", arguments.dataDestString, arguments.destPort);

    size_t availableData;
    size_t read_len; 
    uint8_t* read_ptr;
    bool network_error = false;

    size_t data_to_read = 10; // How much to try to read at once

    pthread_mutex_lock(&buffer_lock);

    int my_reader_id = buffer.reader_cnt;
    buffer.readerOffset[my_reader_id] = buffer.data_head_offset;
    buffer.reader_cnt++;

    pthread_mutex_unlock(&buffer_lock);

    while(1)
    {
        pthread_mutex_lock(&buffer_lock);
        // find if enough data or writer finished
        while (((availableData = circularBufferAvailableData(&buffer, my_reader_id)) < data_to_read) && (buffer.writerFinished == false))
        {
            // printf("Available data: %ld\n", availableData); // This can be noisy
            pthread_cond_wait(&data_available, &buffer_lock);
        }
        
        // enough data or writer finished
        if (availableData == 0 && buffer.writerFinished) // writer finished and no data to read
        {
            pthread_mutex_unlock(&buffer_lock);
            break; // Exit main loop
        }

        // If writer is finished, read all remaining data.
        // Otherwise, read in chunks of 'data_to_read'.
        if (buffer.writerFinished)
        {
            read_len = availableData;
        }
        else if (availableData < data_to_read)
        {
            read_len = availableData;
        }
        else
        {
            read_len = data_to_read;
        }

        if (read_len == 0) // No data to read, but writer not finished
        {
             pthread_mutex_unlock(&buffer_lock);
             continue; // Go back to wait
        }

        // get data pointer and actual readable length (handles wrap-around)
        read_len = circularBufferReadData(&buffer, my_reader_id, read_len, &read_ptr);

        pthread_mutex_unlock(&buffer_lock);


        // --- This is the changed part ---
        // Instead of fwrite, send data over the network
        if (sendall(sockfd, read_ptr, read_len) == -1)
        {
            fprintf(stderr, "Reader: network send failed.\n");
            network_error = true;
            // We must still confirm the read to advance the buffer
            // otherwise we might deadlock.
        }
        // --- End of changed part ---


        pthread_mutex_lock(&buffer_lock);

        circularBufferConfirmRead(&buffer, my_reader_id, read_len);

        pthread_mutex_unlock(&buffer_lock);

        if (network_error)
        {
            break; // Exit loop on network error
        }
        
        // printf("Sent %ld bytes of data.\n", read_len); // This can be noisy
    }

    close(sockfd); // Close the network socket
    printf("Network reader thread finished.\n");

    return NULL;
}

int sendall(int sockfd, const uint8_t *buf, size_t len)
{
    size_t total_sent = 0;
    while (total_sent < len)
    {
        // Use MSG_NOSIGNAL to prevent SIGPIPE on broken pipe,
        // we'll handle the error return instead.
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