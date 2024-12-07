#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024


// functie creare socket
int createSocket() 
{
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0) 
    {
        perror("Socket");
        exit(EXIT_FAILURE);
    }
    return socketfd;
}

int sendType(int socket_fd)
{
    char type[2];
    strcpy(type, "C");
    type[sizeof(type) - 1] = '\0';

    int rc = send(socket_fd, type, strlen(type), 0);
    if (rc < 0) 
    {
        printf("Client %d: Error sending type.\n", socket_fd);
        close(socket_fd);
        return 0;
    }

    char receivBufffer[4];
    int bytesReceived = recv(socket_fd, receivBufffer, 3, 0);
    if (bytesReceived <= 0) 
    {
        printf("Client %d: Error receiving ACK or connection closed.\n", socket_fd);
        close(socket_fd);
        return 0;
    }
    receivBufffer[bytesReceived] = '\0';

    if (strcmp(receivBufffer, "ACK") != 0) 
    {
        printf("Client %d: Unexpected response: %s\n", socket_fd, receivBufffer);
        close(socket_fd);
        return 0;
    }

    return 1;
}


// functie cerere conexiune la server
void connectToServer(int socketfd, struct sockaddr_in *server_addr) 
{
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr->sin_addr) <= 0) 
    {
        perror("Invalid address");
        close(socketfd);
        exit(EXIT_FAILURE);
    }

    if (connect(socketfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) 
    {
        perror("Connection to server failed");
        close(socketfd);
        exit(EXIT_FAILURE);
    }
    else
    {
        int verify=sendType(socketfd);
        if(verify)
        {
            printf("Client %d: Connected to server.\n", socketfd);
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }
}
// functie trimitere task

void sendTask(int socketfd,int argc,char* argv[])
{
    char buffer[BUFFER_SIZE];
    strcpy(buffer,argv[1]);

    for(int i=2;i<argc;i++)
    {
        strcat(buffer," ");
        strcat(buffer,argv[i]);
    }

    int rc=send(socketfd, buffer, strlen(buffer), 0);
    if(rc<0)
    {
        printf("Client: %d Error send args.", socketfd);
        exit(EXIT_FAILURE);
    }

    char receivBufffer[4]; // msj ACK
    recv(socketfd, receivBufffer, 3, 0);
    if(strcmp(receivBufffer, "ACK")!=0)
    {
        printf("Agent %d: Error receive ACK in function sendTask.\n", socketfd);
        exit(EXIT_FAILURE);
    }
}

void sendFile(int socketfd, char* argv[])
{
    char buffer[BUFFER_SIZE];

    char fileName[30];
    strcpy(fileName, argv[2]);
    int fd = open(argv[2],O_RDONLY);
    if(fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    int bytes_read,rc;
    strcpy(buffer,"\0");

    //hardcodat 6 => abstractizare dimensiune buffer trimis de server
    char buff_ready[7];
    recv(socketfd,buff_ready,6,0);
    if(strcmp(buff_ready,"Ready?") == 0)
    {
        while((bytes_read = read(fd,buffer,BUFFER_SIZE)) > 0)
        {
            //sleep(5);
            rc = send(socketfd,buffer,bytes_read,0);
            if(rc == -1)
            {
                perror("send in sendFile");
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        printf("%s\n",buffer);
        printf("Server didn't send the ready\n");
    }
}


// functie primire raspuns 
void receiveResponseFromServer()
{
    int rc;
    char buffer[BUFFER_SIZE];
    int bytes_recv;
    

    while((bytes_recv = recv(socket,buffer,BUFFER_SIZE,0)) > 0)
    {
    
        rc = write(STDOUT_FILENO,buffer,bytes_recv);
        if(rc == -1)
        {
            perror("write in receiveResponseFromServer");
            exit(EXIT_FAILURE);
        }
    }
}


int main(int argc,char* argv[])
{
    int socketfd = createSocket();
    struct sockaddr_in *server_addr;
    connectToServer(socketfd,server_addr);
    printf("%s\n",argv[2]);
    sendTask(socketfd,argc,argv);
    sendFile(socketfd, argv);
    
    close(socketfd);

    return 0;
}