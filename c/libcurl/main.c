#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <sqlite3.h>
#include "cJSON.h"
#include <string.h>
#include <unistd.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0; // out of memory!

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0; // Null-terminate the string

    return realsize;
}

void update_database(const char *db_filename, const char *json_string, int currentVersion) {
    sqlite3 *db;
    if (sqlite3_open(db_filename, &db) != SQLITE_OK) return;


    cJSON *root = cJSON_Parse(json_string);
    cJSON *data_array = cJSON_GetObjectItem(root, "data");
    cJSON *new_version = cJSON_GetObjectItem(root, "version");

    // no update
    if (new_version->valueint == currentVersion) return;

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    sqlite3_exec(db, "DELETE FROM plan;", NULL, NULL, NULL);

    // Prepare the statement once for efficiency
    sqlite3_stmt *res;
    const char *sql = "INSERT INTO plan (id, object_name, is_interstellar, obs_start_time, rec_start_time, end_time) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db, sql, -1, &res, NULL);

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, data_array) {
        // Bind JSON values to the SQL statement parameters (?)
        sqlite3_bind_int(res, 1, cJSON_GetObjectItem(item, "id")->valueint);
        sqlite3_bind_text(res, 2, cJSON_GetObjectItem(item, "object_name")->valuestring, -1, SQLITE_STATIC);
        sqlite3_bind_int(res, 3, cJSON_GetObjectItem(item, "is_interstellar")->valueint);
        sqlite3_bind_int64(res, 4, cJSON_GetObjectItem(item, "obs_start_time")->valueint);
        sqlite3_bind_int64(res, 5, cJSON_GetObjectItem(item, "rec_start_time")->valueint);
        sqlite3_bind_int64(res, 6, cJSON_GetObjectItem(item, "end_time")->valueint);

        sqlite3_step(res);      // Execute the insert
        sqlite3_reset(res);     // Clear bindings for next row
    }

    sqlite3_finalize(res);

    // Manually set the version to match the server exactly
    // (This overrides the trigger increments)
    char ver_sql[64];
    sprintf(ver_sql, "UPDATE db_metadata SET version = %d WHERE id = 1;", new_version->valueint);
    sqlite3_exec(db, ver_sql, NULL, NULL, NULL);

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    cJSON_Delete(root);
}

int get_local_version(const char *db_filename)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int version = 0; // default 0

    if (sqlite3_open(db_filename, &db) != SQLITE_OK) return version;

    const char *query = "SELECT version FROM db_metadata WHERE id = 1;";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            version = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);

    return version;
}

int main(void)
{
    const char *dbFile = "plan.db";

    while (1)
    {
        printf("Checking for db update\n");

        int currenVersion = get_local_version(dbFile);
        printf("local db version: %d\n", currenVersion);

        CURL *curl_handle;
        CURLcode res;

        struct MemoryStruct chunk;
        chunk.memory = malloc(1);
        chunk.size = 0;

        curl_global_init(CURL_GLOBAL_ALL);
        curl_handle = curl_easy_init();

        char url[256];
        snprintf(url, sizeof(url), "http://bogano/rt2/get_database.php?v=%d", currenVersion);

        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl_handle);

        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            long response_code;
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

            if (response_code == 200)
            {
                printf("Update received!\n");
                
                update_database(dbFile, chunk.memory, currenVersion);
            }
            else
            {
                printf("Response code: %ld. Skipping update!\n", response_code);
            }
        }
        
        curl_easy_cleanup(curl_handle);
        free(chunk.memory);
        curl_global_cleanup();

        sleep(10);
    }
    
    return 0;
}