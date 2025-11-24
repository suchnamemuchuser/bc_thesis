#ifndef ARGPARSER_C
#define ARGPARSER_C

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>


#include "ArgParser.h"

#define BUFFER_SIZE_DEF 2000
#define DATA_RATE_DEF 100

extern char* optarg;

void printArgs(argParser args);

argParser optargArguments(int argc, char* argv[])
{
    char *colon;

    // default values
    argParser args = {  
                        .bufferSize = BUFFER_SIZE_DEF,
        
                        .dataSource = DATA_TYPE_ZEROS,
                        .dataRate = DATA_RATE_DEF,
                        .dataDestination = DATA_TYPE_ZEROS,

                        .dataSourceString = NULL,
                        .dataSourceFilename = NULL,
                        .dataDestString = NULL,
                        .dataDestFilename = NULL
                    };

    int opt;

    while((opt = getopt(argc, argv, "hs:d:r:b:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            printf("Heres ur help lmao\n"); // TODO: print help
            exit(0);
            break;
        
        // source - domain:port or filename
        case 's':
            colon = strchr(optarg, ':');
            
            // contains ':' -> is address
            if (colon != NULL)
            {
                // Temporarily split the string by replacing ':' with a null terminator
                *colon = '\0'; 
                
                // The part before the colon is the domain (even if empty)
                args.dataSourceString = strdup(optarg);
                
                // The part after the colon is the port
                args.srcPort = atoi(colon + 1); 
                
                args.dataSource = DATA_TYPE_NETWORK;

                // You can optionally restore the colon, but it's not needed here
                // *colon = ':'; 
            }
            // is filename
            else
            {
                args.dataSourceFilename = strdup(optarg);
                args.dataSource = DATA_TYPE_FILE;
            }
            break;

        // destination - domain:port or filename
        case 'd':
            colon = strchr(optarg, ':');

            // contains ':' -> is address
            if (colon != NULL)
            {
                // Temporarily split the string
                *colon = '\0';

                // Part before colon (e.g., "" from ":55555")
                args.dataDestString = strdup(optarg);

                // Part after colon (e.g., "55555" from ":55555")
                args.destPort = atoi(colon + 1);

                args.dataDestination = DATA_TYPE_NETWORK;
            }
            // is filename
            else
            {
                args.dataDestFilename = strdup(optarg);
                args.dataDestination = DATA_TYPE_FILE;
            }
            break;

        // Data rate in B/s
        case 'r':
            if (atoi(optarg) == 0)
            {
                printf("Invalid data rate: %s\n", optarg);
                exit(1);
            }

            args.dataRate = atoi(optarg);
            break;

        // buffer size
        case 'b':
            if (atoi(optarg) == 0)
            {
                printf("Invalid buffer size: %s\n", optarg);
                exit(1);
            }

            args.bufferSize = atoi(optarg);
            break;

        default:
            break;
        }
    }

    if (args.dataSource == DATA_TYPE_ZEROS)
    {
        args.dataSourceFilename = strdup("/dev/zero");

        args.dataSource = DATA_TYPE_FILE;
    }

    if (args.dataDestination == DATA_TYPE_ZEROS)
    {
        args.dataDestFilename = strdup("/dev/zero"); // TODO: change to /dev/zero

        args.dataDestination = DATA_TYPE_FILE;
    }

    

    printArgs(args);

    return args;
}

void printAllArgs(int argc, char* argv[])
{
    printf("Arg : Value\n");

    for (int i = 0 ; i < argc ; i++)
    {
        printf("%d.  : %s\n", i, argv[i]);
    }
}

void printArgs(argParser args)
{
    printf("Args:\n");

    printf("Buffersize: %d\n", args.bufferSize);
    printf("Data rate: %d\n", args.dataRate);
    
    printf("\nSource: \n");
    if (args.dataSource == DATA_TYPE_NETWORK)
    {
        printf("Network source: %s:%d\n", args.dataSourceString, args.srcPort);
    }
    else
    {
        printf("File source: %s\n", args.dataSourceFilename);
    }
    

    printf("\nDestination: \n");
    if (args.dataDestination == DATA_TYPE_NETWORK)
    {
        printf("Network destination: %s:%d\n", args.dataDestString, args.destPort);
    }
    else
    {
        printf("File destination: %s\n", args.dataDestFilename);
    }
}

void freeArgs(argParser args)
{
    if (args.dataSourceString != NULL)
    {
        free(args.dataSourceString);
        args.dataSourceString = NULL;
    }

    if (args.dataSourceFilename != NULL)
    {
        free(args.dataSourceFilename);
        args.dataSourceFilename = NULL;
    }

    if (args.dataDestString != NULL)
    {
        free(args.dataDestString);
        args.dataDestString = NULL;
    }

    if (args.dataDestFilename != NULL)
    {
        free(args.dataDestFilename);
        args.dataDestFilename = NULL;
    }
}

#endif