#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9090
#define BUFFER_SIZE 1024
#define MAX_ARGS 5

int ID=0;

typedef struct Task
{
    char fileName[BUFFER_SIZE];
    int args[MAX_ARGS]; // premiza initiala este doar argumente nr intregi
    size_t dimFile; // pentru verificarea integritatii fisierului
    struct Task *next;
}Task;



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

int sendTaskType(int socket_fd, char ** argv)
{
    int rc=send(socket_fd, argv[1], strlen(argv[1]), 0);
    if(rc <0)
    {
        printf("Agent %d: Error send task type.\n", socket_fd);
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
void connectToServer(int socket_fd, struct sockaddr_in* server_addr, char **argv)
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
            sendTaskType(socket_fd, argv);
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }
}

void parseBuffer(Task* task,char* buffer)
{
    char* p = strtok(buffer," ");

    p = strtok(NULL," ");
    strcpy(task->fileName,p);
    task->fileName[strlen(p)]='\0';

    p = strtok(NULL," ");
    int argument = 0;
    int contor=0;
    while(p)
    {
        argument = atoi(p);
        task->args[contor++] = argument;
        p = strtok(NULL," ");
    }
}

void receiveArgs(int socket_fd, Task* task)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[bytes_received] = '\0'; 
        printf("Agent %d: Args received.\n", socket_fd);

    }

    if (bytes_received < 0) {
        perror("recv");
    }

    parseBuffer(task, buffer);
}


//functie primire task
int receiveDataFile(int socket,char* filename,int id, Task * task)
{
    int rc;
    char strID[16];
    sprintf(strID,"agent%d",id);
    
    char fileLocal[100];
    strcpy(fileLocal, filename);
    strcat(fileLocal,strID);
    int fd = open(fileLocal,O_CREAT | O_RDWR,0644);
    if(fd == -1)
    {
        perror("open in receiveDataFIle");
        exit(EXIT_FAILURE);
    }
    char buffer[BUFFER_SIZE];
    int dimFile = 0;
    int bytes_recv;
    rc = send(socket,"Ready?",6,0);
    if(rc == -1)
    {
        perror("READY?");
        exit(EXIT_FAILURE);
    }

    while((bytes_recv = recv(socket,buffer,BUFFER_SIZE,0)) > 0)
    {
        dimFile += bytes_recv;
        rc = write(fd,buffer,bytes_recv);
        if(rc == -1)
        {
            perror("write in receiveDataFIle");
             exit(EXIT_FAILURE);
        }
    }

    strcpy(task->fileName, fileLocal);
    task->dimFile=dimFile;

    close(fd);
}

//functie executare task
void executeTask(Task * task)
{
    if (task == NULL) {
        fprintf(stderr, "Eroare: Task-ul este NULL.\n");
        return;
   }

    char *argList[MAX_ARGS + 2];
    argList[0] = task->fileName; 
    for (int i = 0; i < MAX_ARGS && task->args[i] != 0; i++) {
        static char argBuffer[MAX_ARGS][BUFFER_SIZE];
        snprintf(argBuffer[i], BUFFER_SIZE, "%d", task->args[i]);
        argList[i + 1] = argBuffer[i]; 
    }
    argList[MAX_ARGS + 1] = NULL; 

    pid_t pid = fork();
    if (pid < 0) {
        perror("Eroare la fork");
        return;
    }

    if (pid == 0) {
        execvp(task->fileName, argList);
        perror("Eroare la execvp");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0); 
        if (WIFEXITED(status)) {
            printf("Procesul s-a terminat cu codul de ieșire %d\n", WEXITSTATUS(status));
        } else {
            fprintf(stderr, "Procesul s-a terminat anormal.\n");
        }
    }
}

void waitForMessage(int socket_fd)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;
    char* args;
    Task * task;

    printf("Agent %d: Waiting for message from server...\n", socket_fd);

     while ((bytes_received = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; 
        printf("Agent %d: Message received: %s\n", socket_fd, buffer);
        task= (Task*)malloc(sizeof(Task*));
        if (strcmp(buffer, "ARGS") == 0) {
            printf("Agent %d: Receiving arguments...\n", socket_fd);
            receiveArgs(socket_fd, task);
        } else if (strcmp(buffer, "DATA_FILE") == 0) {
            printf("Agent %d: Receiving data file...\n", socket_fd);
            receiveDataFile(socket_fd, "task_file_", ID, task);
            ID++;
            printf("Agent %d: Received data file.\n", socket_fd);
        } else if (strcmp(buffer, "EXIT") == 0) {
            printf("Agent %d: Server requested exit. Closing connection.\n", socket_fd);
            break;
        } else {
            printf("Agent %d: Unknown message: %s\n", socket_fd, buffer);
        }
    }

    if (bytes_received == 0) {
        printf("Agent %d: Server closed the connection.\n", socket_fd);
    } else if (bytes_received < 0) {
        perror("recv");
    }

    executeTask(task);

}


//functie trimitere task

//functie trimitere raspuns catre server

int main(int argc, char* argv[]) 
{
    int socket_fd=createSocket();
    struct sockaddr_in *server_addr;
    connectToServer(socket_fd, server_addr, argv);
    waitForMessage(socket_fd);

    close(socket_fd);
    return 0;
}