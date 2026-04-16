#ifndef CONFIG_H
#define CONFIG_H

typedef struct device {
    char* source;
    char* name;
    int port;
} Device;

typedef struct appConfig {
    int bufferSize;
    char* webURL;
    char* database;
    char* dataRootDir;
    char* dataBaseDir;
    char* bufferStatusFile;
    Device* devices;
    int deviceCount;
} AppConfig;

AppConfig* loadConfig(const char *filename);

int printConfig(AppConfig* config);

#endif // CONFIG_H