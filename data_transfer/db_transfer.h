#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <sqlite3.h>
#include "cJSON.h"
#include <string.h>
#include <unistd.h>

int checkAndUpdateDb(char* dbFileName, char* baseURL);