#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <asm-generic/socket.h>
#include <fcntl.h>

#define MAX_ARGS 5
#define MAX_NR_TASKS 10
#define MAX_NR_AGENTS 10
#define MAX_NR_CLIENTS 10
#define PORT 8080
#define BUFFER_SIZE 1024
#define QUEUE_SIZE 10
#define THREAD_POOL_SIZE 5

static int idClient = 0;
static int idAgent = 0;

typedef struct Agent
{
    int idAgent;
    int isBusy;
    int taskType;  // 0-usor  1-mediu  2-dificil
    // de completat
    int socketfd;
}Agent;

typedef struct Client
{
    int idClient;
    int isWaiting;
    //de completat
    int socketfd;
}Client;

typedef struct Task
{
    int taskType;
    char fileName[BUFFER_SIZE];
    int args[MAX_ARGS]; // premiza initiala este doar argumente nr intregi
    size_t dimFile; // pentru verificarea integritatii fisierului
    Client* client;
    Agent* agent; // agentul care va executa task-ul -> pentru evidenta unor log-uri
    //de completat
    struct Task *next;
}Task;


typedef struct
{
    Task *front, *rear;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} TaskQueue;

TaskQueue clientQueue, agentQueue;


void initQueue(TaskQueue *queue)
{
    queue->front = queue->rear = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void enqueue(TaskQueue *queue,Task* task)
{
    pthread_mutex_lock(&queue->lock);
    if (queue->rear == NULL)
    {
        queue->front = queue->rear = task;
    }
    else
    {
        queue->rear->next = task;
        queue->rear = task;
    }
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

Task *dequeue(TaskQueue *queue)
{
    pthread_mutex_lock(&queue->lock);
    while (queue->front == NULL)
    {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }
    Task *task = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }
    pthread_mutex_unlock(&queue->lock);
    return task;
}

typedef struct
{
    pthread_t threads[THREAD_POOL_SIZE];
    TaskQueue *queue;
    void (*processTask)(Task*);
    // pthread_mutex_t lock;
    // pthread_cond_t signal;
} ThreadPool;

void *worker(void *p)
{
    ThreadPool* pool = (ThreadPool*) p;
    while (1)
    {
        Task *task = dequeue(pool->queue);
        pool->processTask(task);
    }
    return NULL;
}

void initThreadPool(ThreadPool *pool, TaskQueue *queue, void (*process_task)(Task *))
{
    pool->queue = queue;
    pool->processTask = process_task;
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&pool->threads[i], NULL, worker, pool);
    }
    
    //pthread_mutex_init(&pool->lock,NULL);
}

void parseBuffer(Task* task,char* buffer)
{
    char* p = strtok(buffer," ");

    int tasktype = atoi(p);
    task->taskType = tasktype;

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

int receiveDataFile(int socket,char* filename,int id)
{
    int rc;
    char strID[5];
    sprintf(strID,"_%d",id);
    
    char fileLocal[100];
    strcpy(fileLocal, filename);
    strcat(fileLocal,strID);
    int fd = open(filename,O_CREAT | O_RDWR,0644);
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
        printf("SUnt aici");
        if(rc == -1)
        {
            perror("write in receiveDataFIle");
             exit(EXIT_FAILURE);
        }
    }
    close(fd);
    return dimFile;
}

Task* createTask(Task* task,char* buffer)
{
    //Task* task = (Task*)malloc(sizeof(Task));
    parseBuffer(task,buffer);
    task->dimFile = receiveDataFile(task->client->socketfd,task->fileName,task->client->idClient);
    return task;
}

void processClientTask(Task *task)
{
    // primim un buffer cu argumente
    char buffer[BUFFER_SIZE];
    recv(task->client->socketfd, &buffer, BUFFER_SIZE, 0); // primim argumentele
    int rc=send(task->client->socketfd, "ACK", 3, 0);
    if(rc<0)
    {
        printf("Client: %d Error send ACK in function processClientTask.", task->client->socketfd);
        exit(EXIT_FAILURE);
    }
    else
    {
        task = createTask(task,buffer);
        printf("Processed client %d request for file: %s \n", task->client->idClient, task->fileName);
        enqueue(&agentQueue,task);
        dequeue(&clientQueue);
    }
    //free(task);
}


// functie creare socket
int createSocket(int port)
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, QUEUE_SIZE) < 0)
    {
        perror("Listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    return serverSocket;
}

// functie handle client


// functie handle agent


// mecanism threadpool pentru clienti
                                            // o structura ce va avea 2 instante
                                            // (una pt clienti,alta pt agenti)
                                            // rezulta, structura sa fie usor generica
// mecanism threadpool pentru agenti

Client* createClient(int socket)
{
    Client* client = (Client*)malloc(sizeof(Client));
    client->idClient = idClient++;
    client->socketfd = socket;
    client->isWaiting = 1;

    return client;
}

int main(int argc,char* argv[])
{
   int serverSocket = createSocket(PORT);

    // initializare task queue-uri
    initQueue(&clientQueue);
    initQueue(&agentQueue);

    // initializare threadpool-uri
    ThreadPool clientPool, agentPool;
    initThreadPool(&clientPool, &clientQueue, processClientTask);
    //initThreadPool(&agentPool, &agentQueue, processAgentTask);

    printf("Server is listening on port %d\n", PORT);

    struct sockaddr_in* client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (1)
    {
        int new_socket = accept(serverSocket, (struct sockaddr *)&client_addr, &addr_len);
        if (new_socket < 0) 
        {
            perror("accept socket");
            continue;
        }

        char type;
        recv(new_socket, &type, sizeof(type), 0);
        if (type == 'C')
        {
            int rc=send(new_socket, "ACK", 3, 0);
            printf("Client connected.\n");

            Task *task = (Task *)malloc(sizeof(Task));
            task->client = createClient(new_socket);
            enqueue(clientPool.queue, task);
        }
        else if (type == 'A')
        {
            printf("Agent connected.\n");
            int rc=send(new_socket, "ACK", 3, 0);
        }
    }

    close(serverSocket);
    return 0;
}