#define _XOPEN_SOURCE 700 // fix incomplete struct addrinfo
#define _DEFAULT_SOURCE // fix usleep warning

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

#define RECONNECT_DELAY_SECONDS 5

pthread_mutex_t buffer_lock;

pthread_cond_t data_available;

pthread_cond_t space_available;

circularBuffer buffer;

argParser arguments;

void* writerBehaviour(void* arg);

void* writerWriteFromFile(void* arg);

void* writerReadFromNetwork(void* arg);

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


    // source is file
    if (arguments.dataSource == DATA_TYPE_FILE)
    {
        reader_fptr = fopen(arguments.dataSourceFilename, "r");

        if (reader_fptr == NULL)
        {
            perror("Error opening file");
            return 1;
        }
    }
    
    // destination is file
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
            exit(1);
        }
    }
    else if (arguments.dataDestination == DATA_TYPE_NETWORK)
    {
        if (pthread_create(&reader_thread1, NULL, readerWriteToNetwork, NULL))
        {
            fprintf(stderr, "Error creating reader thread\n");
            exit(1);
        }
    }

    if (arguments.dataSource == DATA_TYPE_FILE)
    {
        if (pthread_create(&writer_thread, NULL, writerWriteFromFile, (void*) reader_fptr)) 
        {
            fprintf(stderr, "Error creating writer thread\n");

            pthread_cancel(reader_thread1);
            exit(1);
        }
    }
    else if (arguments.dataSource == DATA_TYPE_NETWORK)
    {
        if (pthread_create(&writer_thread, NULL, writerReadFromNetwork, NULL)) 
        {
            fprintf(stderr, "Error creating writer thread\n");

            pthread_cancel(reader_thread1);
            exit(1);
        }
    }

    
    // wait for threads

    pthread_join(writer_thread, NULL);

    printf("Writer finished. Cleaning up.\n");

    if (reader_fptr != NULL) fclose(reader_fptr);

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

    int cnt = 0;

    size_t readDataCnt = 0;

    while (true)
    {
        rnd = rand() % 1000; // get range from 0 to 999 - 

        usleep(rnd * 1000); // in microseconds

        // calculate appropriate amound of data
        rndcount = ((long long) rnd * arguments.dataRate + 500) / 1000; // + 500 rounds up

        if (rndcount > arguments.dataRate) // ensure no buffer overflow
        {
            rndcount = arguments.dataRate;
        }

        if (rndcount == 0)
        {
            continue;
        }

        //printf("reading %d data\n", rndcount);

        int read_count = fread(read_buffer, sizeof(uint8_t), rndcount, file);

        readDataCnt += read_count;

        pthread_mutex_lock(&buffer_lock);

        // if no space, print error and discard data
        size_t space = circularBufferWriterSpace(&buffer);

        pthread_mutex_unlock(&buffer_lock);

        if (space >= read_count)
        {
            circularBufferMemWrite(&buffer, read_buffer, read_count);
        }
        else
        {
            printf("Losing data: %d\n", read_count);
        }

        //printf("%.*s\n", read_count, read_buffer);

        pthread_mutex_lock(&buffer_lock);

        // if (cnt == 9)
        // {
        // }
        
        circularBufferPrintStatus(&buffer);

        if (space >= read_count)
        {
            circularBufferConfirmWrite(&buffer, read_count);
        }

        pthread_cond_broadcast(&data_available);

        pthread_mutex_unlock(&buffer_lock);

        //printf("Wrote %d bytes of data.\n", read_count);

        // check EOF
        if (feof(file))
        {
            break; // Exit the loop
        }

        cnt = (cnt + 1) % 10;

        //printf("%d\n", cnt);
    }

    pthread_mutex_lock(&buffer_lock);

    buffer.writerFinished = true;

    pthread_cond_broadcast(&data_available);

    pthread_mutex_unlock(&buffer_lock);

    return NULL;
}

void* writerReadFromNetworkOld(void* arg)
{
    // --- CLIENT NETWORK STUFF ---
    int sockfd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;
    int rv;
    char port_str[6];

    (void)arg; // (arg is unused)

    snprintf(port_str, sizeof(port_str), "%d", arguments.srcPort);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    printf("Writer: Resolving %s:%s\n", arguments.dataSourceString, port_str);
    if ((rv = getaddrinfo(arguments.dataSourceString, port_str, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "writer getaddrinfo: %s\n", gai_strerror(rv));

        // Tell the readers that we are done
        pthread_mutex_lock(&buffer_lock);
        buffer.writerFinished = true;
        pthread_cond_broadcast(&data_available); // Wake up readers one last time
        pthread_mutex_unlock(&buffer_lock);

        return NULL;
    }

    // Connect loop
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
            perror("writer: connect");
            continue;
        }
        break; // Connected
    }

    if (p == NULL)
    {
        fprintf(stderr, "writer: failed to connect to %s\n", arguments.dataSourceString);
        freeaddrinfo(servinfo);

        // Tell the readers that we are done
        pthread_mutex_lock(&buffer_lock);
        buffer.writerFinished = true;
        pthread_cond_broadcast(&data_available); // Wake up readers one last time
        pthread_mutex_unlock(&buffer_lock);

        return NULL;
    }

    freeaddrinfo(servinfo);
    printf("Writer: Connected to %s:%d. Starting to receive data.\n",
           arguments.dataSourceString, arguments.srcPort);

    // --- BUFFER-WRITING LOGIC ---
    
    // Use arguments.dataRate as the *maximum size* of our read buffer
    size_t read_buffer_size = arguments.dataRate;
    uint8_t *read_buffer = malloc(read_buffer_size);
    if (read_buffer == NULL)
    {
        fprintf(stderr, "Writer: Failed to allocate read buffer\n");
        close(sockfd);
        return NULL;
    }

    ssize_t bytes_received;

    // Loop until the server closes the connection
    while (true)
    {
        // Block until data arrives or the connection is closed.
        bytes_received = recv(sockfd, read_buffer, read_buffer_size, 0);

        if (bytes_received == 0)
        {
            printf("Writer: Connection closed by server.\n");
            break; // Exit the loop
        }
        else if (bytes_received < 0)
        {
            perror("Writer: recv error");
            break; // Exit the loop
        }

        // --- This is the locking logic from your writerWriteFromFile ---
        
        pthread_mutex_lock(&buffer_lock);
        // if no space, print error and discard data
        size_t space = circularBufferWriterSpace(&buffer);
        pthread_mutex_unlock(&buffer_lock);

        if (space >= (size_t)bytes_received)
        {
            // Call write function *outside* the lock, as per your design
            circularBufferMemWrite(&buffer, read_buffer, bytes_received);
        }
        else
        {
            // Not enough space, we must drop the data
            fprintf(stderr, "Writer: Buffer full! Dropping %zd bytes.\n", bytes_received);
            // We still continue, and the broadcast will happen
        }

        // Wake up any waiting reader threads
        pthread_mutex_lock(&buffer_lock);

        circularBufferConfirmWrite(&buffer, bytes_received);

        pthread_cond_broadcast(&data_available);
        pthread_mutex_unlock(&buffer_lock);
        // --- End of specific locking logic ---
    }

    // --- Cleanup ---
    printf("Writer: Network loop finished. Cleaning up.\n");
    free(read_buffer);
    close(sockfd);

    // Tell the readers that we are done
    pthread_mutex_lock(&buffer_lock);
    buffer.writerFinished = true;
    pthread_cond_broadcast(&data_available); // Wake up readers one last time
    pthread_mutex_unlock(&buffer_lock);

    return NULL;
}

void* writerReadFromNetwork(void* arg)
{
    int sockfd = -1;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;
    int rv;
    char port_str[6];
    bool connected = false; // Flag to track successful connection

    int cnt = 0;

    (void)arg;

    // --- INITIAL SETUP (Happens once) ---
    snprintf(port_str, sizeof(port_str), "%d", arguments.srcPort);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    // Allocate read buffer
    size_t read_buffer_size = arguments.dataRate;
    uint8_t *read_buffer = malloc(read_buffer_size);
    if (read_buffer == NULL)
    {
        fprintf(stderr, "Writer: Failed to allocate read buffer\n");
        // Signal readers to finish due to permanent error
        pthread_mutex_lock(&buffer_lock);
        buffer.writerFinished = true;
        pthread_cond_broadcast(&data_available);
        pthread_mutex_unlock(&buffer_lock);
        return NULL;
    }


    // --- OUTER INFINITE RECONNECT LOOP ---
    while (true)
    {
        connected = false; // Assume not connected at the start of each iteration

        // 1. Resolve and Connect (Attempt connection)
        printf("Writer: Attempting to connect to %s:%s\n", 
               arguments.dataSourceString, port_str);
               
        servinfo = NULL; 
        sockfd = -1; // Reset socket descriptor

        // 1a. Resolve domain name
        if ((rv = getaddrinfo(arguments.dataSourceString, port_str, &hints, &servinfo)) != 0)
        {
            fprintf(stderr, "writer getaddrinfo: %s\n", gai_strerror(rv));
            // Resolution failed, skip connection attempt
        }
        else 
        {
            // 1b. Connect loop
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
            freeaddrinfo(servinfo); // Always free address info
            
            if (!connected)
            {
                fprintf(stderr, "writer: failed to connect to %s\n", arguments.dataSourceString);
            }
        }
        
        // 2. Data Reception Loop (Only runs if connected)
        if (connected)
        {
            printf("Writer: Connected. Starting to receive data.\n");
            ssize_t bytes_received;

            while (connected) // Read loop continues as long as 'connected' is true
            {
                bytes_received = recv(sockfd, read_buffer, read_buffer_size, 0);

                if (bytes_received <= 0) // Server closed connection (0) or error (< 0)
                {
                    if (bytes_received < 0)
                    {
                        perror("Writer: recv error");
                    }
                    else
                    {
                        printf("Writer: Connection closed by server.\n");
                    }
                    // Connection lost, exit read loop
                    connected = false; 
                    break;
                }

                // --- Buffer Writing Logic ---
                pthread_mutex_lock(&buffer_lock);
                size_t space = circularBufferWriterSpace(&buffer);
                pthread_mutex_unlock(&buffer_lock);

                if (space >= (size_t)bytes_received)
                {
                    circularBufferMemWrite(&buffer, read_buffer, bytes_received);
                }
                else
                {
                    fprintf(stderr, "Writer: Buffer full! Dropping %zd bytes.\n", bytes_received);
                }

                // Wake up any waiting reader threads
                pthread_mutex_lock(&buffer_lock);

                if (cnt == 0)
                {
                    circularBufferPrintStatus(&buffer);
                }

                // confirm write under lock

                circularBufferConfirmWrite(&buffer, bytes_received);
                pthread_cond_broadcast(&data_available);
                pthread_mutex_unlock(&buffer_lock);

                cnt = (cnt + 1) % 100;
            } // End of Read loop
            
            // Connection was lost, close the bad socket
            if (sockfd != -1) {
                close(sockfd);
                sockfd = -1; 
            }
        }

        // 3. Mandatory Delay before Retrying
        fprintf(stderr, "Writer: Retrying connection in %d seconds...\n", RECONNECT_DELAY_SECONDS);
        sleep(RECONNECT_DELAY_SECONDS); 
        
        // Outer loop repeats
    } // End of Outer Reconnect Loop
    
    // --- FINAL CLEANUP (If loop ever exits) ---
    free(read_buffer);
    // No need to signal writerFinished here as the loop is infinite by design
    
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
            //printf("Available data: %ld\n", availableData);

            pthread_cond_wait(&data_available, &buffer_lock);
        }

        //printf("Reader available data: %ld\n", availableData);
        
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

        //fflush(fptr);

        //printf("%.*s\n", read_len, read_ptr);


        pthread_mutex_lock(&buffer_lock);

        circularBufferConfirmRead(&buffer, my_reader_id, read_len);

        pthread_mutex_unlock(&buffer_lock);

        //printf("Read %ld of data.\n", read_len);
    }

    printf("Reader thread finished.\n");

    return NULL;
}

void* readerWriteToNetworkOld(void* arg)
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

void* readerWriteToNetwork(void* arg)
{
    // --- SERVER NETWORK STUFF ---
    int listen_fd, client_fd; // Listener socket, Client socket
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;
    int rv;
    char port_str[6];
    int yes = 1; // For setsockopt

    // Client address info
    struct sockaddr_storage their_addr;
    socklen_t sin_size;

    // Convert port to string (This is the port we will *listen* on)
    snprintf(port_str, sizeof(port_str), "%d", arguments.destPort);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // Use IPv4
    hints.ai_socktype = SOCK_STREAM; // Use TCP
    hints.ai_flags = AI_PASSIVE;     // Use my IP (tell getaddrinfo to prepare for bind())

    printf("Server: Setting up listener on port %s\n", port_str);

    // Get address info for binding. Use NULL for host to bind to all interfaces
    if ((rv = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "server getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        // Create a socket
        if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        // --- NEW: Allow port reuse ---
        // This prevents "Address already in use" errors if you restart quickly
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            close(listen_fd);
            continue; // Try the next address
        }

        // --- REPLACED connect() with bind() ---
        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listen_fd);
            perror("server: bind");
            continue;
        }

        break; // If we get here, we successfully bound
    }

    freeaddrinfo(servinfo); // All done with this structure

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return NULL;
    }

    // --- NEW: listen() ---
    // Mark the socket to listen for incoming connections
    // Using a BACKLOG of 1 because this thread only handles one client.
    if (listen(listen_fd, 1) == -1)
    {
        perror("listen");
        return NULL;
    }

    printf("Server: Waiting for a connection on port %s...\n", port_str);

    // --- NEW: accept() ---
    // This is a blocking call. It waits until a client connects.
    sin_size = sizeof their_addr;
    client_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (client_fd == -1)
    {
        perror("accept");
        close(listen_fd);
        return NULL;
    }

    // A client has connected!
    // We can now close the *listening* socket, since we only want one client.
    close(listen_fd);

    printf("Server: Got connection! Starting data send.\n");

    // --- FROM HERE, YOUR CIRCULAR BUFFER LOGIC IS THE SAME ---
    // --- Just make sure to use 'client_fd' instead of 'sockfd' ---

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

    while (1)
    {
        pthread_mutex_lock(&buffer_lock);
        // find if enough data or writer finished
        while (((availableData = circularBufferAvailableData(&buffer, my_reader_id)) < data_to_read) && (buffer.writerFinished == false))
        {
            pthread_cond_wait(&data_available, &buffer_lock);
        }

        // enough data or writer finished
        if (availableData == 0 && buffer.writerFinished) // writer finished and no data to read
        {
            pthread_mutex_unlock(&buffer_lock);
            break; // Exit main loop
        }
        
        if (availableData < data_to_read)
        {
            read_len = availableData;
        }
        else
        {
            read_len = data_to_read;
        }

        if (read_len == 0)
        {
             pthread_mutex_unlock(&buffer_lock);
             continue; 
        }

        // get data pointer and actual readable length
        read_len = circularBufferReadData(&buffer, my_reader_id, read_len, &read_ptr);

        pthread_mutex_unlock(&buffer_lock);


        // --- Send data over the new 'client_fd' socket ---
        // (Assuming you have a 'sendall' helper function that handles partial sends)
        if (sendall(client_fd, read_ptr, read_len) == -1)
        {
            fprintf(stderr, "Reader: network send failed.\n");
            network_error = true;
        }
        // --- End of changed part ---


        pthread_mutex_lock(&buffer_lock);

        circularBufferConfirmRead(&buffer, my_reader_id, read_len);

        pthread_mutex_unlock(&buffer_lock);

        if (network_error)
        {
            break; // Exit loop on network error
        }
    }

    close(client_fd); // Close the client's socket
    printf("Network reader thread finished (client disconnected).\n");

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