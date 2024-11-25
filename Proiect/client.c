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
    printf("Connected to server.\n");
}
// functie trimitere task
void sendTask(int socketfd,int argc,char* argv[])
{
    char type = 'C';
    send(socketfd, &type, sizeof(type), 0);

    char buffer[BUFFER_SIZE];
    strcpy(buffer,argv[1]);

    for(int i=2;i<argc;i++)
    {
        strcat(buffer," ");
        strcat(buffer,argv[i]);
    }

    send(socketfd, buffer, strlen(buffer), 0);
    printf("File request sent: %s\n", argv[2]);

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
                perror("write in receiveDataFIle");
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

int main(int argc,char* argv[])
{
    int socketfd = createSocket();
    struct sockaddr_in *server_addr;
    connectToServer(socketfd,server_addr);
    printf("%s\n",argv[2]);
    sendTask(socketfd,argc,argv);

    close(socketfd);

    return 0;
}