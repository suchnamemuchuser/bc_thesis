#define SOURCE_ZEROS 1
#define SOURCE_GEN 2
#define SOURCE_FILE 3

#define READER_MAX_CAP 128
#define BUFFER_DEF_SIZ 512000000 // 512 MB

typedef struct {
    // data creation rate
    int dataRate;
    // data source
    int dataSource;
    // data destination - ipv4 or dname
    char address[256];
    // port
    int port;
} argParser;

int parseArguments(int argc, char* argv[], argParser* args)
{
    
}