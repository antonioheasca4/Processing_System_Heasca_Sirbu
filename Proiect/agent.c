#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

// functie creare socket
int createSocket()
{
    int socket_fd=socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd<0)
    {
        printf("Agent %d: Socket faied.\n", socket_fd);
        exit(EXIT_FAILURE);
    }
}

int sendType(int socket_fd)
{
    char type[2];
    strcpy(type, "A");
    int rc=send(socket_fd, type, strlen(type), 0);
    if(rc <0)
    {
        printf("Agent %d: Error send type.\n", socket_fd);
        close(socket_fd);
        return 0;
    }
    char receivBufffer[3]; // msj ACK
    recv(socket_fd, receivBufffer, 3, 0);
    if(strcmp(receivBufffer, "ACK"))
    {
        printf("Agent %d: Error receive ACK.\n", socket_fd);
        close(socket_fd);
        return 0;
    }

    return 1;
}


// functie cerere conexiune la server
void connectToServer(int socket_fd, struct sockaddr_in* server_addr)
{
    server_addr->sin_family=AF_INET;
    server_addr->sin_port=htons(SERVER_PORT);

    if(inet_pton(AF_INET, SERVER_IP, &server_addr->sin_addr)<=0)
    {
        printf("Agent %d: Ivalid adress.\n", socket_fd);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    if(connect(socket_fd, (struct sockaddr*)server_addr, sizeof(*server_addr))<0)
    {
        printf("Agent %d: Connection to server failed.\n", socket_fd);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    else
    {
        int verify=sendType(socket_fd);
        if(verify)
        {
            printf("Agent %d: Connected to server.\n", socket_fd);
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }
}

//functie primire task
void recvTask()
{
    
}
//functie executare task

//functie trimitere task

//functie trimitere raspuns catre server

int main() 
{
    int socket_fd=createSocket();
    struct sockaddr_in *server_addr;
    connectToServer(socket_fd, server_addr);


    close(socket_fd);
    return 0;
}