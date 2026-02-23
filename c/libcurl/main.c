#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <sqlite3.h>

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

int get_local_version(const char *db_filename)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int version = 0; // default 0

    if (sqlite3_open(db_filename, &db) == SQLITE_OK)
    {
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
    }

    return version;
}

int main(void)
{
    CURL *curl;
    CURLcode res;
    FILE *fp;
    
    const char *temp_filename = "temp_plan.db";
    const char *final_filename = "plan.db";
    char url[256];

    // get version
    int current_version = get_local_version(final_filename);
    printf("local db version: %d\n", current_version);

    // make url
    snprintf(url, sizeof(url), "http://bogano/bc_thesis/web_server/get_database.php?v=%d", current_version);
    printf("Requesting URL: %s\n", url);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl)
    {
        fp = fopen(temp_filename, "wb");

        if (!fp)
        {
            perror("Failed to open temp file");
            return 1;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        
        res = curl_easy_perform(curl);
        fclose(fp);

        if (res == CURLE_OK)
        {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (response_code == 200)
            {
                if (rename(temp_filename, final_filename) != 0)
                {
                    perror("Error replacing the database file");
                }

            }
            else
            {
                remove(temp_filename);
                printf("No update needed. Server returned: %ld\n", response_code);
            }
        }
        else
        {
            fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
            remove(temp_filename);
        }

        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    return 0;
}