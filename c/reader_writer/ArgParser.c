#include <string.h>

#define SOURCE_ZEROS 1
#define SOURCE_GEN 2
#define SOURCE_FILE 3

#define READER_MAX_CAP 128
#define BUFFER_SIZE_DEF 512000000 // 512 MB
#define DATA_RATE_DEF 3*1024 // 3MB/s

typedef struct {
    // data creation rate
    int dataRate;
    // data source
    int dataSource;
    // data destination
    int dataDestination;
    // data destination - ipv4 or dname
    char destName[256];
    // port
    int port;


    // Need to know data input and output:
    // Input:
    //      1. Network
    //      2. File
    //      3. Zeros
    //
    // Output:
    //      1. Network
    //      2. File


    // Writer reads zeroes, from file, or from network
    // reader writes into file or network
} argParser;

argParser parseArguments(int argc, char* argv[])
{
    argParser args = { .dataRate = DATA_RATE_DEF, .dataSource = SOURCE_ZEROS};

    for (int i = 0 ; i < argc ; i++)
    {
        if (strcmp(argv[i], "-i"))
        {
            printf("Got -i arg\n");
        }
    }
}