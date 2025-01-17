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
    char fileNameOut[BUFFER_SIZE];
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
    // int rc=send(socket_fd, "2", 1, 0);
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
    //leak aici
    server_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
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

void f()
{
    int pid = fork();
    if (pid == 0)
    {
        // Înlocuim procesul curent cu gcc
        execl("/usr/bin/gcc", "gcc", "task_file_agent0.c", "-o", "task_file_agent0_exec", NULL);
        
        // Dacă exec eșuează, afișăm o eroare
        perror("Eroare la exec");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        // Procesul părinte așteaptă procesul copil
        int status;
        wait(&status);

        if (WIFEXITED(status))
        {
            printf("Procesul copil s-a terminat cu codul de ieșire: %d\n", WEXITSTATUS(status));
        }
        else
        {
            printf("Procesul copil nu s-a terminat corect.\n");
        }
    }
    else
    {
        perror("Eroare la fork");
        exit(EXIT_FAILURE);
    }
}

void parseBuffer(Task* task,char* buffer)
{
    
    char* p = strtok(buffer," ");
    
    

    //f();
    task->args[0]=0;
    task->args[1]=0;
    task->args[2]=0;
    task->args[3]=0;
    task->args[4]=0;
    
    

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
    
    int bytes_received = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0);
    buffer[bytes_received] = '\0'; 
    if(bytes_received)
        printf("Agent %d: Args received.\n", socket_fd);

    int rc = send(socket_fd, "ACK", 3, 0);
    if(rc < 0)
    {
        printf("Error send ACK in function waitForMessage:");
        exit(EXIT_FAILURE);
    }

    printf("Argumente: %s\n", buffer);

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
    strcat(fileLocal, ".c");
    strcpy(task->fileName, fileLocal);
    int fd = open(fileLocal,O_CREAT | O_RDWR ,0755);
    if(fd == -1)
    {
        perror("open in receiveDataFIle");
        exit(EXIT_FAILURE);
    }
    char buffer[BUFFER_SIZE];
    int dimFile = 0;
    int bytes_recv=0;
    

    while((bytes_recv = recv(socket,buffer,BUFFER_SIZE,MSG_DONTWAIT)) > 0)
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
    strcpy(buffer, "");

    close(fd);
}

void sendOutToServer(Task* task, int socket_fd)
{
    char buffer[BUFFER_SIZE];
    int fd = open(task->fileNameOut,O_RDONLY);
    if(fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    int bytes_read, rc2;
    strcpy(buffer,"\0");
    while((bytes_read = read(fd,buffer,BUFFER_SIZE)) > 0)
    {
        rc2 = send(socket_fd,buffer,bytes_read,0);
        if(rc2 == -1)
        {
            perror("send in sendFileToAgent.\n");
            exit(EXIT_FAILURE);
        }
    }
    
    rc2 = send(socket_fd,"OK",3,0);
}

//functie executare task
void executeTask(Task *task, int socket_fd)
{
    if (task == NULL || task->fileName == NULL) {
        fprintf(stderr, "Eroare: Task-ul sau numele fișierului este NULL.\n");
        return;
    }

    char execName[100];
    char cpy[100];
    strcpy(cpy, task->fileName);
    char* p=strtok(cpy, ".");
    strcpy(execName, cpy);
    strcat(execName, "_exec");


    int status;

    int pid = fork();
    if (pid < 0) 
    {
        perror("Eroare la fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    { 
        execl("/usr/bin/gcc", "gcc", task->fileName, "-o", execName, NULL);

        perror("Eroare la execvp pentru gcc");
        exit(EXIT_FAILURE);
    }
    else
    {
        wait(&status);

        if (WIFEXITED(status)) {
            printf("Procesul gcc s-a terminat normal.\n");
            if (WEXITSTATUS(status) == 0) {
                printf("Compilare reușită!\n");
            } else {
                fprintf(stderr, "Eroare la compilare, cod de ieșire gcc: %d\n", WEXITSTATUS(status));
            }
        } else if (WIFSIGNALED(status)) {
            printf("Procesul gcc a fost terminat de semnalul: %d\n", WTERMSIG(status));
        } else {
            printf("Procesul gcc s-a terminat într-un mod neașteptat.\n");
        }
    }

    char *argList[MAX_ARGS];

    argList[0] = malloc(strlen(execName) + 1);
    if (argList[0] == NULL) {
        fprintf(stderr, "Eroare la alocarea memoriei pentru execName.\n");
        return;
    }
    strcpy(argList[0], execName);

    int i;

    for (i = 1; i < MAX_ARGS && task->args[i-1] != 0; i++) {
        argList[i] = malloc(BUFFER_SIZE);
        if (argList[i] == NULL) {
            fprintf(stderr, "Eroare la alocarea memoriei pentru argumente.\n");
            for (int j = 0; j < i; j++) {
                free(argList[j]);
            }
            return;
        }
        snprintf(argList[i], BUFFER_SIZE, "%d", task->args[i-1]);
    }
    
    argList[i]=NULL;

    char fileNameLocal[100];


    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) 
    {
        printf("Compilare reușită!\n");

        pid_t pid2=fork();

        if(pid2<0)
        {
            perror("Eroare la fork.\n");
            exit(EXIT_FAILURE);
        }

        if(pid2==0)
        {

            strcpy(fileNameLocal, execName);
            strcat(fileNameLocal, "_out");

            int fd = open(fileNameLocal, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            
            if (fd < 0) 
            {
                perror("Eroare la deschiderea fișierului de ieșire");
                exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
                perror("Eroare la redirecționarea ieșirii");
                close(fd);
                exit(EXIT_FAILURE);
            }

            char fileExec[100];
            strcpy(fileExec, "./");\
            strcat(fileExec, execName);

            execvp(fileExec, argList);

            perror("Eroare la execvp");
            exit(EXIT_FAILURE); 
        }
        else 
        {
            int status2;
            waitpid(pid2, &status2, 0);

            if (WIFEXITED(status)) 
            {
                printf("Procesul executabil s-a terminat normal.\n");
                if (WEXITSTATUS(status) == 0) {
                    printf("Executabil reușit!\n");
                } else {
                    fprintf(stderr, "Eroare la executie, cod de ieșire: %d\n", WEXITSTATUS(status));
                }
            } 
            else if (WIFSIGNALED(status)) 
            {
                printf("Procesul executabil a fost terminat de semnalul: %d\n", WTERMSIG(status));
            } else 
            {
                printf("Procesul executabil s-a terminat într-un mod neașteptat.\n");
            }   
        }
    }

    strcpy(task->fileNameOut, execName);
    strcat(task->fileNameOut, "_out");
}

void waitForMessage(int socket_fd)
{
    char buffer[BUFFER_SIZE];
    int bytes_received = 0;
    char* args;
    //leak aici
    Task * task = (Task* )malloc(sizeof(Task));
    int ok=0;

    printf("Agent %d: Waiting for message from server...\n", socket_fd);

     while(ok==0)
        { 
            bytes_received = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0);
            buffer[bytes_received] = '\0'; 
            // printf("Agent %d: Message received: %s\n", socket_fd, buffer);
            
            if (strcmp(buffer, "ARGS") == 0) 
            {
                printf("Agent %d: Receiving arguments...\n", socket_fd);
                int rc = send(socket_fd, "ACK", 3, 0);
                if(rc < 0)
                {
                    printf("Error send ACK in function waitForMessage:");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    receiveArgs(socket_fd, task);
                }
            }
            else if (strcmp(buffer, "DATA_FILE") == 0) 
            {
                printf("Agent %d: Receiving data file...\n", socket_fd);
                int rc = send(socket_fd, "ACK", 3, 0);
                if(rc < 0)
                {
                    printf("Error send ACK in function waitForMessage: DATA_FILE.");
                    exit(EXIT_FAILURE);
                }
                else
                { 
                    receiveDataFile(socket_fd, "task_file_", ID, task);
                    ok=1;
                }
                ID++;
                printf("Agent %d: Received data file.\n", socket_fd);
            } 
        }
    
    if (bytes_received == 0) {
        printf("Agent %d: Server closed the connection.\n", socket_fd);
    } else if (bytes_received < 0) {
        perror("recv");
    }

    
    executeTask(task, socket_fd);
    sendOutToServer(task, socket_fd);
    free(task);
    strcpy(buffer, "\0");
    ok=0;
}

int main(int argc, char* argv[]) 
{
    int socket_fd=createSocket();
    struct sockaddr_in *server_addr = NULL;
    connectToServer(socket_fd, server_addr, argv);
     while(1)
     {
        waitForMessage(socket_fd);
     }

    close(socket_fd);
    free(server_addr);
    return 0;
}