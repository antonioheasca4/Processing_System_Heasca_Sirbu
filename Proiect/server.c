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
#define PORT_CLIENT 8080
#define PORT_AGENT 9090
#define BUFFER_SIZE 1024
#define QUEUE_SIZE 10
#define THREAD_POOL_SIZE 5

static int idClient = 0;
static int idAgent = 0;

pthread_mutex_t client_accept_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t agent_accept_lock = PTHREAD_MUTEX_INITIALIZER;



typedef struct Client
{
    int idClient;
    int isWaiting;
    //de completat
    int socketfd;
}Client;

struct Agent;
typedef struct Task
{
    int taskType;
    char fileName[BUFFER_SIZE];
    int args[MAX_ARGS]; // premiza initiala este doar argumente nr intregi
    size_t dimFile; // pentru verificarea integritatii fisierului
    Client* client;
    struct Agent* agent; // agentul care va executa task-ul -> pentru evidenta unor log-uri
    //de completat
    struct Task *next;
    int isReady;
}Task;

typedef struct Agent
{
    int idAgent;
    int isBusy;
    int taskType;  // 0-usor  1-mediu  2-dificil
    // de completat
    int socketfd;
    struct Agent * next;
    struct Task* task;
}Agent;

typedef struct
{
    Agent* front;
    pthread_mutex_t lock;
    pthread_cond_t cond;
}AgentQueue;

static AgentQueue agentQueue;

typedef struct
{
    Task *front;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} TaskQueue;

static TaskQueue taskQueue;

typedef struct
{
    pthread_t threads[THREAD_POOL_SIZE];
    void *queue;
    void (*processTask)(void*);
    int serverSocket;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
} ThreadPool;

void initTaskQueue(TaskQueue *queue)
{
    queue->front =  NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void initAgentQueue(AgentQueue* queue)
{
    queue->front =  NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void enqueue(TaskQueue *queue, Task* task)
{
    pthread_mutex_lock(&queue->lock);
    pthread_t tid = pthread_self();

    task->next = queue->front;
    queue->front = task;

    printf("Task pentru clientul %d a fost adăugat în coadă. TID = %lu\n", task->client->idClient,tid);
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
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
    for(int i=0;i<MAX_ARGS;i++)
    {
        task->args[i] = -1; //adica nu e argument valid
    }
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
    close(fd);
    return dimFile;
}

Task* createTask(Task* task,char* buffer)
{
    parseBuffer(task,buffer);
    task->dimFile = receiveDataFile(task->client->socketfd,task->fileName,task->client->idClient);
    return task;
}

void debugTaskQueue(TaskQueue *queue) 
{
    pthread_mutex_lock(&queue->lock); 
    Task *current = queue->front;
    if (!current) 
    {
        printf("TaskQueue este goală.\n");
    } 
    else 
    {
        printf("Continutul TaskQueue:\n");
        while (current) 
        {
            printf("Task: clientId=%d fileName=%s\t", current->client->idClient,current->fileName);
            printf("Args: ");
            for(int i=0; current->args[i] != -1; i++)
            {
                printf("%d ",current->args[i]);
            }
            printf("\n");
            current = current->next;
        }
    }
    pthread_mutex_unlock(&queue->lock);
}

void freeClient(Client *client) 
{
    if (client != NULL) 
    {
        close(client->socketfd); 
        free(client);            
    }
}

void freeTask(Task *task) 
{
    if (task != NULL) 
    {
        if (task->client != NULL) 
        {
            freeClient(task->client); 
        }
        free(task); 
    }
}


void processClientTask(void *t)
{
    Task* task = (Task*) t;

    // primim un buffer cu argumente
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(task->client->socketfd, &buffer, BUFFER_SIZE, 0); // primim argumentele
    int rc = send(task->client->socketfd, "ACK", 3, 0);
    if(rc < 0)
    {
        printf("Client: %d Error send ACK in function processClientTask.", task->client->socketfd);
        exit(EXIT_FAILURE);
        freeTask(task);
    }
    else
    if(bytes_received > 0)
    {
        task = createTask(task,buffer);
        task->isReady = 1;
        printf("Processed client %d request for file: %s \n", task->client->idClient, task->fileName);
        enqueue(&taskQueue,task);
        debugTaskQueue(&taskQueue);
    }
}

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

Client* createClient(int socket)
{
    Client* client = (Client*)malloc(sizeof(Client));
    client->idClient = idClient++;
    client->socketfd = socket;
    client->isWaiting = 1;

    return client;
}

void *workerClient(void *p)
{
    ThreadPool* pool = (ThreadPool*)p;

    while (1)
    {
        pthread_mutex_lock(&client_accept_lock);
        int new_socket = accept(pool->serverSocket, (struct sockaddr *)&pool->client_addr, &pool->addr_len);
        pthread_mutex_unlock(&client_accept_lock);
        if (new_socket < 0) 
        {
            perror("accept socket");
            continue;
        }

        char type;
        int bytes_received = recv(new_socket, &type, sizeof(type), 0);
        if (bytes_received <= 0) 
        {
            perror("Eroare receptionare C de la client\n");
            close(new_socket);
            continue;
        }
        if (type == 'C')
        {
            int rc = send(new_socket, "ACK", 3, 0);
            if (rc <= 0) 
            {
                perror("Eroare send in workerClient\n");
                close(new_socket);
                continue;
            }
            printf("Client connected.\n");

            Task *task = (Task *)malloc(sizeof(Task));
            task->agent = NULL;
            task->client = createClient(new_socket);
            task->isReady = 0;
            pool->processTask(task);
        }
        else
        {
            close(new_socket);
            continue;
        }
    }
}

void addAgentToQueue(AgentQueue* agentQueue, Agent *agent)
{
    pthread_mutex_lock(&(agentQueue->lock));
    agent->next = agentQueue->front;
    agentQueue->front = agent;
    pthread_cond_broadcast(&(agentQueue->cond));
    pthread_mutex_unlock(&(agentQueue->lock));
}

void *workerAgent(void *p)
{
    ThreadPool *pool = (ThreadPool *)p;

    while (1)
    {
        pthread_mutex_lock(&agent_accept_lock);
        int agent_socket = accept(pool->serverSocket, (struct sockaddr *)&pool->client_addr, &pool->addr_len);
        pthread_mutex_unlock(&agent_accept_lock);
        if (agent_socket < 0)
        {
            perror("accept agent socket");
            continue;
        }

        char type;
        int bytes_received = recv(agent_socket, &type, sizeof(type), 0);
        if (bytes_received <= 0) 
        {
            perror("Eroare receptionare A de la agent\n");
            close(agent_socket);
            continue;
        }

        if(type == 'A')
        {
            int rc = send(agent_socket, "ACK", 3, 0);
            if(rc < 0)
            {
                printf("Client: %d Error send ACK in function workerAgent.", agent_socket);
                close(agent_socket);
                continue;
            }

            printf("Agent connected.\n");

            Agent *agent = (Agent *)malloc(sizeof(Agent));
            agent->idAgent = idAgent++;
            agent->socketfd = agent_socket;
            agent->isBusy = 0;
            agent->task = NULL;
            
            char buff[2];
            bytes_received = recv(agent_socket, buff, 1, 0);
            if (bytes_received <= 0) 
            {
                perror("Eroare taskType al agentului\n");
                close(agent_socket);
                continue;
            }

            agent->taskType = atoi(buff);

            rc = send(agent_socket, "ACK", 3, 0);
            if(rc < 0)
            {
                printf("Client: %d Error send ACK in function workerAgent.", agent_socket);
                close(agent_socket);
                continue;
            }

            addAgentToQueue(&agentQueue,agent);

            pool->processTask(agent);
        } 
        else
        {
            close(agent_socket);
            continue;
        }
    }
}


void assignTaskToAgent(Task *task)
{
    pthread_mutex_lock(&agentQueue.lock);
    Agent *prev = NULL, *curr = agentQueue.front;

    while (curr)
    {
        if (!curr->isBusy && curr->taskType == task->taskType)
        {
            curr->isBusy = 1;
            task->agent = curr;

            // Remove agent from queue temporarily
            if (prev)
                prev->next = curr->next;
            else
                agentQueue.front = curr->next;

            pthread_mutex_unlock(&agentQueue.lock);

            // Send task to agent
            send(curr->socketfd, &task, sizeof(Task), 0);
            printf("Task sent to Agent %d\n", curr->idAgent);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&agentQueue.lock);

    printf("No available agents for task type %d.\n", task->taskType);
}


void processAgentResult(Agent *agent)
{
    char result[BUFFER_SIZE];
    recv(agent->socketfd, result, BUFFER_SIZE, 0);

    // Send result to client
    send(agent->task->client->socketfd, result, strlen(result), 0);

    // Mark agent as free
    pthread_mutex_lock(&agentQueue.lock);
    agent->isBusy = 0;
    agent->next = agentQueue.front;
    agentQueue.front = agent;
    pthread_mutex_unlock(&agentQueue.lock);

    printf("Task completed by Agent %d and result sent to Client %d\n",
           agent->idAgent, agent->task->client->idClient);
}


void initThreadClientPool(ThreadPool *pool, TaskQueue* queue, void (*process_task)(void *),int serverSocket)
{
    pool->queue = queue;
    pool->processTask = process_task;
    pool->serverSocket = serverSocket;
    pool->addr_len = sizeof(pool->client_addr);

    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&pool->threads[i], NULL, workerClient, pool);
    }
}

void debugAgentQueue(AgentQueue *queue) 
{
    pthread_mutex_lock(&queue->lock); 
    Agent *current = queue->front;
    if (!current) 
    {
        printf("AgentQueue este goală.\n");
    } 
    else 
    {
        printf("Continutul AgentQueue:\n");
        while (current) 
        {
            printf("Agent: agentID=%d  taskType=%d\n",current->idAgent,current->taskType);
            current = current->next;
        }
    }
    pthread_mutex_unlock(&queue->lock);
}

void processAgent(void* a)
{
    Agent* agent = (Agent*)a;
    debugAgentQueue(&agentQueue);
}

void initThreadAgentPool(ThreadPool *pool, AgentQueue* queue, void (*process_Agent)(void *),int serverSocket)
{
    pool->queue = queue;
    pool->processTask = process_Agent;
    pool->serverSocket = serverSocket;
    pool->addr_len = sizeof(pool->client_addr);

    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&pool->threads[i], NULL, workerAgent, pool);
    }
}


void sendResponseToClient(Task *task, const char *result) 
{
    int bytes_sent = send(task->client->socketfd, result, strlen(result), 0);
    if (bytes_sent <= 0) 
    {
        perror("Server: Failed to send response to client.");
    } 
    else 
    {
        printf("Server: Response sent to client %d.\n", task->client->idClient);
    }
}

int main(int argc,char* argv[])
{
    int clientSocket = createSocket(PORT_CLIENT);
    int agentSocket = createSocket(PORT_AGENT);

    initTaskQueue(&taskQueue);
    initAgentQueue(&agentQueue);


    ThreadPool clientPool;
    initThreadClientPool(&clientPool, &taskQueue, processClientTask,clientSocket);

    ThreadPool agentPool;
    initThreadAgentPool(&agentPool,&agentQueue,processAgent,agentSocket);

    printf("Server is listening on port %d for CLIENTS. TID = %lu\n", PORT_CLIENT,pthread_self());
    printf("Server is listening on port %d for AGENTS. TID = %lu\n", PORT_AGENT,pthread_self());

    for (int i = 0; i < THREAD_POOL_SIZE; i++) 
    {
        pthread_join(clientPool.threads[i], NULL);
    }

    for (int i = 0; i < THREAD_POOL_SIZE; i++) 
    {
        pthread_join(agentPool.threads[i], NULL);
    }

    close(clientSocket);
    close(agentSocket);
    return 0;
}