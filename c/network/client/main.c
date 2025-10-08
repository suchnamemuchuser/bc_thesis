#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX 256
#define PORT 11111
#define SA struct sockaddr

void chat(int connfd);

int main()
{
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("192.168.0.79");
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    // function for chat
    chat(sockfd);

    // close the socket
    close(sockfd);
}

void chat(int sockfd)
{
    char buff[MAX];

    // pointer for getline buffer
    char *line = NULL;
    size_t size = 0;

    for (;;) {
        printf("From client: ");

        // get message on server side
        if(getline(&line, &size, stdin) == -1)
        {
            printf("No line.\n");
        }

        write(sockfd, line, sizeof(line));

        if(strncmp("exit", line, 4) == 0)
        {
            free(line);
            line = NULL;
            printf("Client exit.\n");
            break;
        }

        // clear line buffer
        free(line);
        line = NULL;

        bzero(buff, sizeof(buff));
        read(sockfd, buff, sizeof(buff));

        printf("From Server : %s", buff);

        if ((strncmp("exit", buff, 4)) == 0)
        {
            printf("Client Exit...\n");
            break;
        }
    }
}