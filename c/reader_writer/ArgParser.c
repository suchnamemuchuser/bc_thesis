#ifndef ARGPARSER_C
#define ARGPARSER_C

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>


#include "ArgParser.h"

#define READER_MAX_CAP 128
#define BUFFER_SIZE_DEF 512000000 // 512 MB
#define DATA_RATE_DEF 3*1024*1000 // 3KiB/ms

extern char* optarg;

argParser parseArguments(int argc, char* argv[])
{
    bool inputSet = false;

    printAllArgs(argc, argv);

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

    for (int i = 1 ; i < argc ; i++) // 1st arg is executable name
    {
        // help
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            printf("Heres ur help lmao\n"); // TODO: print help
            exit(0);
        }

        else if ((strcmp(argv[i], "-z") == 0) || (strcmp(argv[i], "--zeros") == 0))
        {
            if (!inputSet) // setting input
            {
                args.dataSource = DATA_TYPE_ZEROS;
                inputSet = true;
            }
            else // setting output
            {
                args.dataDestination = DATA_TYPE_ZEROS;
            }
        }

        else if ((strcmp(argv[i], "-f") == 0) || (strcmp(argv[i], "--filename") == 0))
        {
            if (i == (argc - 1)) // last argument
            {
                printf("Expected argumnent after %s\n", argv[i]);
                exit(1);
            }

            i++;

            if (!inputSet) // setting input
            {
                args.dataSource = DATA_TYPE_FILE;

                args.dataSourceString = strdup(argv[i]);

                inputSet = true;
            }
            else // setting input
            {
                args.dataDestination = DATA_TYPE_FILE;

                args.dataDestString = strdup(argv[i]);
            }
        }

        else if ((strcmp(argv[i], "-n") == 0) || (strcmp(argv[i], "--network") == 0))
        {
            if (i == (argc - 1)) // last argument
            {
                printf("Expected argumnent after %s\n", argv[i]);
                exit(1);
            }

            i++;

            if (!inputSet) // setting input
            {
                args.dataSource = DATA_TYPE_NETWORK;

                args.dataSourceString = strdup(argv[i]);

                inputSet = true;
            }
            else // setting input
            {
                args.dataDestination = DATA_TYPE_NETWORK;

                args.dataDestString = strdup(argv[i]);
            }
        }

        else if ((strcmp(argv[i], "-r") == 0) || (strcmp(argv[i], "--rate") == 0))
        {
            if (i == (argc - 1)) // last argument
            {
                printf("Expected argumnent after %s\n", argv[i]);
                exit(1);
            }

            i++;

            if (!inputSet) // setting input
            {
                args.dataSource = DATA_TYPE_NETWORK;

                args.dataSourceString = strdup(argv[i]);

                inputSet = true;
            }
        }

        else
        {
            printf("Unknown argument: %s\n", argv[i]);
            exit(1);
        }
    }

    if (args.dataDestination == DATA_TYPE_ZEROS)
    {
        args.dataDestFilename = "/dev/null";
    }

    return args;
}

argParser optargArguments(int argc, char* argv[])
{
    printAllArgs(argc, argv);

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

    while((opt = getopt(argc, argv, "hs:d:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            printf("Heres ur help lmao\n"); // TODO: print help
            exit(0);
            break;
        
        case 's':
            if (strchr(optarg, ':') != NULL) // contains ':' -> is address
            {
                strdup(strtok(optarg, ':'), args.dataSourceString);
                strdup(strtok(NULL, ':'), args.port);
            }
            else
            {
                args.dataSourceFilename = strdup(optarg);
            }

        default:
            break;
        }
    }
}

void printAllArgs(int argc, char* argv[])
{
    printf("Arg : Value\n");

    for (int i = 0 ; i < argc ; i++)
    {
        printf("%d.  : %s\n", i, argv[i]);
    }
}

#endif