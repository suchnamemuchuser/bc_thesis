#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET; // ipv4
    addr.sin_port = htons(11111); // port 11111
    addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));

    listen(server_fd, 1);

    int client_fd = accept(server_fd, NULL, NULL);

    char buffer[1024] = {0};
    read(client_fd, buffer, sizeof(buffer));
    printf("Client says: %s\n", buffer);

    char *reply = "Hello from server!";
    send(client_fd, reply, strlen(reply), 0);

    close(client_fd);
    close(server_fd);

    return 0;
}

