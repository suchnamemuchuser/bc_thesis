#include "server.h"

AppConfig* appConfig;

int main(){    
    DbItem currentRecording;

    appConfig = loadConfig("serverconfig.json");

    printConfig(appConfig);


    BufferSession* bufferSessions = malloc(sizeof(BufferSession) * appConfig->deviceCount);

    for (int i = 0 ; i < appConfig->deviceCount ; i++)
    {
        circularBufferInit(&bufferSessions[i].buffer, appConfig->bufferSize);
        bufferSessions[i].deviceInfo = appConfig->devices[i];

        pthread_mutex_init(&bufferSessions[i].buffer_lock, NULL);
        pthread_cond_init(&bufferSessions[i].data_available, NULL);

        pthread_t producer_tid;
        if (pthread_create(&producer_tid, NULL, bufferProducerThread, (void*)&bufferSessions[i]) != 0)
        {
            fprintf(stderr, "Failed to create producer thread.\n");
            return 1;
        }

        pthread_t fileConsumer_tid;
        if (pthread_create(&producer_tid, NULL, bufferFileConsumerThread, (void*)&bufferSessions[i]) != 0)
        {
            fprintf(stderr, "Failed to create file consumer thread.\n");
            return 1;
        }

        printf("Init CB\n");
        printf("Add to buffer array\n");
        printf("Start writer and readers\n");
    }

    // time_t now = time(NULL);

    // DbItem testItem = {
    //     .id = 1,
    //     .object_name = "PSR1999+TEST",
    //     .is_interstellar = 1,
    //     .obs_start_time = (int)now,
    //     .rec_start_time = (int)(now + 30),
    //     .end_time = (int)(now + 30)
    // };

    // printf("Starting test!\n");

    // printf("Starting observation of %s\n", testItem.object_name);

    // sleep(30);

    // printf("Starting recording of %s\n", testItem.object_name);

    // for (int i = 0 ; i < appConfig->deviceCount ; i++)
    // {
    //     pthread_mutex_lock(&bufferSessions[i].buffer_lock);
    //     bufferSessions[i].recordingInfo = testItem;
    //     bufferSessions[i].buffer.recordingActive = true;
    //     pthread_mutex_unlock(&bufferSessions[i].buffer_lock);
    // }

    // sleep(30);

    // printf("Ending recording.\n");

    // for (int i = 0 ; i < appConfig->deviceCount ; i++)
    // {
    //     pthread_mutex_lock(&bufferSessions[i].buffer_lock);
    //     bufferSessions[i].buffer.recordingActive = false;
    //     pthread_mutex_unlock(&bufferSessions[i].buffer_lock);
    // }

    // sleep(60);

    // printf("Ending test.\n");

    // exit(0);

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