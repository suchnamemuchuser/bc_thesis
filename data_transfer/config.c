#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

AppConfig* loadConfig(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return NULL;

    // Read file into memory
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, fp);
    buffer[size] = '\0';
    fclose(fp);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);
    if (!json) return NULL;

    AppConfig *config = malloc(sizeof(AppConfig));

    cJSON *bufsiz = cJSON_GetObjectItemCaseSensitive(json, "buffer_size");
    config->bufferSize = cJSON_IsNumber(bufsiz) ? bufsiz->valueint : 0;

    // 1. Get URL
    cJSON *url = cJSON_GetObjectItemCaseSensitive(json, "web_url");
    config->webURL = cJSON_IsString(url) ? strdup(url->valuestring) : NULL;

    // 2. Get DB Connection
    cJSON *db = cJSON_GetObjectItemCaseSensitive(json, "database");
    config->database = cJSON_IsString(db) ? strdup(db->valuestring) : NULL;

    cJSON *drd = cJSON_GetObjectItemCaseSensitive(json, "data_root_dir");
    config->dataRootDir = cJSON_IsString(drd) ? strdup(drd->valuestring) : NULL;

    cJSON *dbd = cJSON_GetObjectItemCaseSensitive(json, "data_base_dir");
    config->dataBaseDir = cJSON_IsString(dbd) ? strdup(dbd->valuestring) : NULL;

    // 3. Get Devices Array
    cJSON *devices_array = cJSON_GetObjectItemCaseSensitive(json, "devices");
    config->deviceCount = cJSON_GetArraySize(devices_array);
    config->devices = malloc(sizeof(Device) * config->deviceCount);

    int i = 0;
    cJSON *device_item;
    cJSON_ArrayForEach(device_item, devices_array) {
        cJSON *source = cJSON_GetObjectItemCaseSensitive(device_item, "source");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(device_item, "name");
        cJSON *port = cJSON_GetObjectItemCaseSensitive(device_item, "port");

        config->devices[i].source = cJSON_IsString(source) ? strdup(source->valuestring) : strdup("");
        config->devices[i].name = cJSON_IsString(name) ? strdup(name->valuestring) : strdup("");
        config->devices[i].port = cJSON_IsNumber(port) ? port->valueint : 0;
        i++;
    }

    cJSON_Delete(json);
    return config;
}

int printConfig(AppConfig* config) {
    if (config == NULL) {
        fprintf(stderr, "Error: Cannot print NULL config.\n");
        return -1;
    }

    printf("App configuration:\n");
    printf("Buffer size: %d\n", config->bufferSize);
    printf("Web URL: %s\n", config->webURL ? config->webURL : "NULL");
    printf("Database: %s\n", config->database ? config->database : "NULL");
    printf("Data Root Dir: %s\n", config->dataRootDir ? config->dataRootDir : "NULL");
    printf("Data Base Dir: %s\n", config->dataBaseDir ? config->dataBaseDir : "NULL");
    printf("Device Count: %d\n", config->deviceCount);

    if (config->devices == NULL || config->deviceCount == 0) {
        printf("No devices found.\n");
    } else {
        for (int i = 0; i < config->deviceCount; i++) {
            Device *d = &config->devices[i];
            printf("Device #%d:\n", i + 1);
            printf("  Name:   %s\n", d->name ? d->name : "N/A");
            
            // Handle the case where source might be an empty string
            if (d->source && d->source[0] != '\0') {
                printf("  Source: %s\n", d->source);
            } else {
                printf("  Source: (empty)\n");
            }
            
            printf("  Port:   %d\n", d->port);
            printf("\n");
        }
    }

    return 0;
}