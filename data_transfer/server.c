#include "config.h"
#include "db_transfer.h"
#include "CircularBuffer.h"
#include <pthread.h>

typedef struct {
    circularBuffer buffer;

    // ALL ACCESS TO BUFFERSESSION UNDER MUTEX!
    pthread_mutex_t buffer_lock;

    pthread_cond_t data_available;

    Device deviceInfo;

    DbItem recordingInfo;

} BufferSession;

int main(){    
    DbItem currentRecording;

    AppConfig* appConfig;

    appConfig = loadConfig("serverconfig.json");

    printConfig(appConfig);

    // load config

    // start all r+w

    BufferSession* bufferSessions = malloc(sizeof(BufferSession) * appConfig->deviceCount);

    for (int i = 0 ; i < appConfig->deviceCount ; i++)
    {
        circularBufferInit(&bufferSessions[i].buffer, appConfig->bufferSize);
        bufferSessions[i].deviceInfo = appConfig->devices[i];

        pthread_mutex_init(&bufferSessions[i].buffer_lock, NULL);
        pthread_cond_init(&bufferSessions[i].data_available, NULL);

        printf("Init CB\n");
        printf("Add to buffer array\n");
        printf("Start writer and readers\n");
    }

    while(1)
    {
        checkAndUpdateDb(appConfig->database, appConfig->webURL);

        // check and update database

        int curTime = (int)time(NULL);

        DbItem nextRecording = getDbItem(appConfig->database, curTime);

        if (nextRecording.id == -1)
        {
            printf("Sleeping for 1 min.\n");
            sleep(60);
            continue;
        }

        printDbItem(nextRecording);

        if (nextRecording.obs_start_time > curTime) // start observing - aim RT
        {
            printf("Sleeping for %d\n", nextRecording.obs_start_time - curTime);
            sleep(nextRecording.obs_start_time - curTime);

            printf("Starting observation of %s\n", nextRecording.object_name);

            // add mutex
            currentRecording = nextRecording;

            continue;
        }
        else if (nextRecording.rec_start_time > curTime) // start recording - update recording info and set flag
        {
            printf("Sleeping for %d\n", nextRecording.rec_start_time - curTime);
            sleep(nextRecording.rec_start_time - curTime);

            printf("Starting recording of %s\n", currentRecording.object_name);

            for (int i = 0 ; i < appConfig->deviceCount ; i++)
            {
                pthread_mutex_lock(&bufferSessions[i].buffer_lock);
                bufferSessions[i].recordingInfo = currentRecording;
                bufferSessions[i].buffer.recordingActive = true;
                pthread_mutex_unlock(&bufferSessions[i].buffer_lock);
            }

            continue;
        }
        else // end recording - set flag
        {
            printf("Sleeping for %d\n", nextRecording.end_time - curTime);
            sleep(nextRecording.end_time - curTime);

            printf("Ending recording of %s\n", currentRecording.object_name);

            for (int i = 0 ; i < appConfig->deviceCount ; i++)
            {
                pthread_mutex_lock(&bufferSessions[i].buffer_lock);
                bufferSessions[i].buffer.recordingActive = false;
                pthread_mutex_unlock(&bufferSessions[i].buffer_lock);
            }

            continue;
        }

        // check for starting / ending / canceled recordings and inform r+w

        // rewrite the status report file
    }

    return 0;
}