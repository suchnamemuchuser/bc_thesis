#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>

#define MAX 256
#define PORT 11111 
#define SA struct sockaddr 

void chat(int connfd);

int main()
{
    int sockfd, connfd;
    uint len;
    struct sockaddr_in servaddr, cli; 

    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1)
    {
        printf("socket creation failed...\n"); 
        exit(0); 
    }
    else
    {
        printf("Socket successfully created..\n"); 
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0)
    { 
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
    else
    {
        printf("Socket successfully binded..\n"); 
    }

    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0)
    { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
    {
        printf("Server listening..\n"); 
    }
    len = sizeof(cli);

    // Accept the data packet from client and verification 
    connfd = accept(sockfd, (SA*)&cli, &len); 
    if (connfd < 0)
    { 
        printf("server accept failed...\n"); 
        exit(0); 
    } 
    else
    {
        printf("server accept the client...\n"); 
    }


    chat(connfd); // chat with the client

    close(sockfd); // close socket
   
    return 0;
}


void chat(int connfd)
{
    // buffer for received message
    char buff[MAX];

    // pointer for getline buffer
    char *line = NULL;
    size_t size = 0;

    // infinite loop for server listen
    for(;;)
    {
        // ensure receive buffer empty
        bzero(buff, MAX);

        // read from network
        read(connfd, buff, sizeof(buff));

        // print client message
        printf("From client: %s", buff);

        // check if message is 'exit', quit
        if (strncmp("exit", buff, 4) == 0)
        {
            printf("Server exit.\n");
            break;
        }

        printf("To client: ");

        // get message on server side
        if(getline(&line, &size, stdin) == -1)
        {
            printf("No line.\n");
        }

        // send message
        write(connfd, line, sizeof(line));

        // clear line buffer
        free(line);
        line = NULL;
    }
}