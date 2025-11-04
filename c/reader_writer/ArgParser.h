#ifndef ARGPARSER_H
#define ARGPARSER_H

typedef enum {
    DATA_TYPE_ZEROS, // does this as destination mean write into /dev/null or just discard data?
    DATA_TYPE_FILE,
    DATA_TYPE_NETWORK
} dataType;

typedef struct {

    // buffer info
    int bufferSize; // -b || --buffer-size       ?? in (k/m)bytes?



    // Writer info
    dataType dataSource; // -s || --source : z, f, n

    char* dataSourceString; // -i || --input : string

    char* dataSourceFilename;

    // socket pointer thing for address?

    // desired data rate in B/s
    int dataRate; // -r || --rate



    // Reader
    dataType dataDestination; // ZEROS means /dev/null // -d || --destination : z, f, n

    char* dataDestString; // -o || --output : string

    char* dataDestFilename;

    int port;

    // socket pointer thing again?
} argParser;

argParser parseArguments(int argc, char* argv[]);

#endif