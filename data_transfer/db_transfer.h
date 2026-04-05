#ifndef DB_TRANSFER_H
#define DB_TRANSFER_H

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <sqlite3.h>
#include "cJSON.h"
#include <string.h>
#include <unistd.h>

typedef struct {
    int id;
    char object_name[256];
    int is_interstellar;
    int obs_start_time;
    int rec_start_time;
    int end_time;
} DbItem;

int checkAndUpdateDb(char* dbFileName, char* baseURL);

DbItem getDbItem(char* dbFileName, int timestamp);

void printDbItem(DbItem item);

DbItem getDbItemById(char* dbFileName, int id);

#endif