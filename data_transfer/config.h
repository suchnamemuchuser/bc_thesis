typedef struct {
    char* source;
    char* name;
    int port;
} Device;

typedef struct {
    int bufferSize;
    char* webURL;
    char* database;
    char* dataRootDir;
    char* dataBaseDir;
    Device* devices;
    int deviceCount;
} AppConfig;

AppConfig* loadConfig(const char *filename);

int printConfig(AppConfig* config);