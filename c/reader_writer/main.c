#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>

#define CYCLICAL_BUFFER_SIZE 10000

sem_t readLightSwitch;
sem_t turnstile;
sem_t readerCountMutex;

u_int32_t readerCount;

int PulsarValuesCBuffer[CYCLICAL_BUFFER_SIZE];

void writerBehaviour();

void readerBehaviour();

void readerWaitLightswitch();

void readerPostLightswitch();

int main(){
    // locked when one writer or atleast one reader present in critical section
    sem_init(&readLightSwitch, 0, 1);

    // locked when one writer present  in critical section
    sem_init(&turnstile, 0, 1);

    // to access reader count
    sem_init(&readerCountMutex, 0, 1);

    /*
    TODO: behaviour

    TODO: create writer and reader
    */

    pid_t processId = fork();

    // am child process
    if (processId == 0)
    {
        printf("I am writer process\n");
        // create writer
        // make sure writer ends, does not continue past here
        exit(0);
    }

    // TODO: add writer process id to list for later kill?
    
    printf("I am parent process\n");

    sem_destroy(&readLightSwitch);
    sem_destroy(&turnstile);
    sem_destroy(&readerCountMutex);

    return 0;
}

void writerBehaviour()
{
    int pulsarValues[1000];

    for (int i = 0 ; i < 1000 ; i++){
        pulsarValues[i] = rand();
    }

    sem_wait(&turnstile);
    sem_wait(&readLightSwitch);
    // Critical section here
    sem_post(&readLightSwitch);
    sem_post(&turnstile);
}

void readerBehaviour()
{
    // check writer is not in critical section
    sem_wait(&turnstile);
    sem_post(&turnstile);

    readerWaitLightswitch();
    // critical section
    readerPostLightswitch();
}

void readerWaitLightswitch()
{
    // lock mutex
    sem_wait(&readerCountMutex);

    readerCount++;

    // check if first reader in
    if (readerCount == 1)
    {
        sem_wait(&readLightSwitch);
    }

    // unlock mutex
    sem_post(&readerCountMutex);
}

void readerPostLightswitch()
{
    // lock mutex
    sem_wait(&readerCountMutex);

    readerCount--;

    // check if last reader out
    if (readerCount == 0)
    {
        sem_post(&readLightSwitch);
    }

    // unlock mutex
    sem_post(&readerCountMutex);
}