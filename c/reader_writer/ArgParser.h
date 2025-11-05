#ifndef ARGPARSER_H
#define ARGPARSER_H

typedef enum datatype {
    DATA_TYPE_ZEROS, // does this as destination mean write into /dev/null or just discard data?
    DATA_TYPE_FILE,
    DATA_TYPE_NETWORK
} dataType;

typedef struct argparser {

    // buffer info
    int bufferSize; // -b || --buffer-size       ?? in (k/m)bytes?

    // desired data rate in B/s
    int dataRate; // -r



    // Writer info
    dataType dataSource; // -s || --source : z, f, n

    char* dataSourceString; // -i || --input : string

    char* dataSourceFilename;

    int srcPort;
    
    // socket pointer thing for address?

    

    // Reader
    dataType dataDestination; // ZEROS means /dev/null // -d || --destination : z, f, n

    char* dataDestString; // -o || --output : string

    char* dataDestFilename;

    int destPort;

    // socket pointer thing again?
} argParser;

argParser parseArguments(int argc, char* argv[]);

argParser optargArguments(int argc, char* argv[]);

void freeArgs(argParser args);


#endif