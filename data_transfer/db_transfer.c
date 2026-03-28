#ifndef DB_TRANSFER_C
#define DB_TRANSFER_C

#include "db_transfer.h"
#include <sqlite3.h>
#include <string.h>
#include <time.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

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

int checkAndUpdateDb(char* dbFileName, char* baseURL)
{
    int currenVersion = get_local_version(dbFileName);
        printf("local db version: %d\n", currenVersion);

        CURL *curl_handle;
        CURLcode res;

        struct MemoryStruct chunk;
        chunk.memory = malloc(1);
        chunk.size = 0;

        curl_global_init(CURL_GLOBAL_ALL);
        curl_handle = curl_easy_init();

        char url[256];
        snprintf(url, sizeof(url), "%s?v=%d", baseURL, currenVersion);

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
                
                update_database(dbFileName, chunk.memory, currenVersion);
            }
            else
            {
                printf("Response code: %ld. Skipping update!\n", response_code);
            }
        }
        
        curl_easy_cleanup(curl_handle);
        free(chunk.memory);
        curl_global_cleanup();
}

DbItem getDbItem(char* dbFileName, int timestamp) {
    sqlite3 *db;
    sqlite3_stmt *res;
    DbItem item = {0}; // Initialize with zeros (id = 0 often indicates "not found")
    item.id = -1;      // Using -1 as a clearer "not found" sentinel

    // 1. Open database
    if (sqlite3_open(dbFileName, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return item;
    }

    // 2. Prepare SQL Query
    // Updated SQL string
    const char *sql = "SELECT id, object_name, is_interstellar, obs_start_time, "
                    "rec_start_time, end_time FROM plan "
                    "WHERE (obs_start_time >= ? AND obs_start_time <= ? + 300) "
                    "OR (rec_start_time >= ? AND rec_start_time <= ? + 300) "
                    "OR (end_time >= ? AND end_time <= ? + 300) "
                    "ORDER BY obs_start_time ASC LIMIT 1;";




    if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return item;
    }

    sqlite3_bind_int(res, 1, timestamp);
    sqlite3_bind_int(res, 2, timestamp);
    sqlite3_bind_int(res, 3, timestamp);
    sqlite3_bind_int(res, 4, timestamp);
    sqlite3_bind_int(res, 5, timestamp);
    sqlite3_bind_int(res, 6, timestamp);

    // 4. Execute and map to struct
    int step = sqlite3_step(res);
    if (step == SQLITE_ROW) {
        item.id = sqlite3_column_int(res, 0);
        const char* name = (const char*)sqlite3_column_text(res, 1);
        if (name)
        {
        // Copy the string into the fixed-size array safely
            snprintf(item.object_name, sizeof(item.object_name), "%s", name);
        }
        else
        {
            item.object_name[0] = '\0'; // Empty string if NULL
        }
        
        item.is_interstellar = sqlite3_column_int(res, 2);
        item.obs_start_time = sqlite3_column_int(res, 3);
        item.rec_start_time = sqlite3_column_int(res, 4);
        item.end_time = sqlite3_column_int(res, 5);
    }

    // 5. Cleanup
    sqlite3_finalize(res);
    sqlite3_close(db);

    return item;
}

void printLocalTime(int timestamp) {
    // 1. Convert int to time_t
    time_t raw_time = (time_t)timestamp;
    
    // 2. Convert to broken-down time struct (local timezone)
    struct tm *time_info = localtime(&raw_time);

    // 3. Format the time into a string
    char buffer[80];
    // Format: YYYY-MM-DD HH:MM:SS
    strftime(buffer, sizeof(buffer), "%Y.%m.%d-%H:%M", time_info);

    printf("%s", buffer);
}

void printDbItem(DbItem item){
    printf("Id: %d\n", item.id);
    printf("Name: %s\n", item.object_name);
    printf("Is interstellar: %d\n", item.is_interstellar);
    printf("Observation start time:");
    printLocalTime(item.obs_start_time);
    printf("\n");
    printf("Recording start time:");
    printLocalTime(item.rec_start_time);
    printf("\n");
    printf("End time:");
    printLocalTime(item.end_time);
    printf("\n");
}

#endif