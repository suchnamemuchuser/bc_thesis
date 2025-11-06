#include <stdio.h>
//#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

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

    FILE *rdfptr = fopen(arguments.dataSourceFilename, "r");

    if (rdfptr == NULL)
    {
       perror("Error opening file");
       return 1;
    }



    FILE *fptr1 = fopen(arguments.dataDestFilename, "w");

    pthread_t writer_thread;
    pthread_t reader_thread1;

    printf("Starting threads...\n");

    if (pthread_create(&writer_thread, NULL, writerWriteFromFile, (void*) rdfptr)) 
    {
        fprintf(stderr, "Error creating writer thread\n");
        return 1;
    }
    
    if (pthread_create(&reader_thread1, NULL, readerWriteToFile, (void*) fptr1))
    {
        fprintf(stderr, "Error creating reader thread\n");
        return 1;
    }

    pthread_join(writer_thread, NULL);

    printf("Writer finished. Cleaning up.\n");

    fclose(rdfptr);

    pthread_join(reader_thread1, NULL);

    fclose(fptr1);

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

        if (space >= read_count)
        {
            circularBufferMemWrite(&buffer, read_buffer, read_count);
        }

        // check EOF
        if (feof(file))
        {
            break; // Exit the loop
        }

        pthread_cond_broadcast(&data_available);

        pthread_mutex_unlock(&buffer_lock);

        printf("Wrote %d bytes of data.\n", read_count);
    }

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
    size_t readLen; 

    FILE *fptr = (FILE*) arg;

    // set values
    size_t data_to_read = 100;
    

    // get reader buffer
    uint8_t* readerBuffer = malloc(data_to_read);

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
        pthread_mutex_unlock(&buffer_lock);

        if (availableData == 0) // writer finished and no data to read
        {
            break;
        }

        if (availableData < data_to_read)
        {
            readLen = availableData;
        }
        else
        {
            readLen = data_to_read;
        }

        // get data
        circularBufferReadData(&buffer, my_reader_id, readLen, readerBuffer);

        fwrite(readerBuffer, sizeof(uint8_t), readLen, fptr);

        pthread_mutex_lock(&buffer_lock);

        circularBufferConfirmRead(&buffer, my_reader_id, readLen);

        pthread_mutex_unlock(&buffer_lock);

        printf("Read %ld of data.\n", readLen);
    }

    if (readerBuffer != NULL)
    {
        free(readerBuffer);
    }

    printf("Reader thread finished.\n");

    return NULL;
}