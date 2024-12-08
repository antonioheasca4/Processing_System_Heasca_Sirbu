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
    struct Agent * next;
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
    Agent* front, *rear;
} AgentQueue;

AgentQueue agentQueue;

typedef struct
{
    Task *front, *rear;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} TaskQueue;

static TaskQueue taskQueue;

void initializeQueueAgent(AgentQueue queue) 
{
    queue.front = NULL;
    queue.rear = NULL;
}

void initQueue(TaskQueue *queue)
{
    queue->front = queue->rear = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void enqueue(TaskQueue *queue, Task* task)
{
    pthread_mutex_lock(&queue->lock);
    pthread_t tid = pthread_self();
    if (queue->rear == NULL)
    {
        queue->front = queue->rear = task;
    }
    else
    {
        queue->rear->next = task;
        queue->rear = task;
    }
     printf("Task pentru clientul %d a fost adăugat în coadă. TID = %lu\n", task->client->idClient,tid);
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

Task *dequeue(TaskQueue *queue)
{
    pthread_mutex_lock(&queue->lock);
    pthread_t tid = pthread_self();

    if (queue->front == NULL) 
    {
        printf("TaskQueue este goală. TID = %lu\n",tid);
    }

    while (queue->front == NULL)
    {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }
    Task *task = queue->front;
    printf("Task pentru clientul %d a fost extras din coadă de TID = %lu.\n", task->client->idClient,tid);
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
} ThreadPool;

void *worker(void *p)
{
    ThreadPool* pool = (ThreadPool*) p;
    while(1)
    {
        Task *task = dequeue(pool->queue);
        pool->processTask(task);
    }
        
    
    return NULL;
}




void initThreadPool(ThreadPool *pool, TaskQueue* queue, void (*process_task)(Task *))
{
    pool->queue = queue;
    pool->processTask = process_task;
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&pool->threads[i], NULL, worker, pool);
    }
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

void processClientTask(Task *task)
{
    // primim un buffer cu argumente
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(task->client->socketfd, &buffer, BUFFER_SIZE, 0); // primim argumentele
    int rc=send(task->client->socketfd, "ACK", 3, 0);
    if(rc<0)
    {
        printf("Client: %d Error send ACK in function processClientTask.", task->client->socketfd);
        exit(EXIT_FAILURE);
    }
    else
    if(bytes_received > 0)
    {
        task = createTask(task,buffer);
        printf("Processed client %d request for file: %s \n", task->client->idClient, task->fileName);
        enqueue(&taskQueue,task);
        printf("enqueued\n");
        //dequeue(&taskQueue);
    }
}

void processAgentTask(Task *task)
{
    Agent* currentAgent=agentQueue.front;
    

    while(currentAgent)
    {
        if(currentAgent->isBusy==0)
            break;
        currentAgent=currentAgent->next;

    }

    int rc;
    char strID[5];
    sprintf(strID,"_%d",task->agent->idAgent);
    char fileLocal[100];
    strcpy(fileLocal, task->fileName);
    strcat(fileLocal,strID);
    int fd = open(fileLocal, O_RDONLY,0644);
    if(fd == -1)
    {
        perror("open in processAgentTask");
        exit(EXIT_FAILURE);
    }   

    char buffer[BUFFER_SIZE];

    int bytes_read;
    strcpy(buffer,"\0");

    //hardcodat 6 => abstractizare dimensiune buffer trimis de server
    char buff_ready[7];
    recv(currentAgent->socketfd,buff_ready,6,0);
    if(strcmp(buff_ready,"Ready?") == 0)
    {
        while((bytes_read = read(fd,buffer,BUFFER_SIZE)) > 0)
        {
            //sleep(5);
            rc = send(currentAgent->socketfd,buffer,bytes_read,0);
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

Agent* createAgent(int socket_fd)
{
    char p[2];
    recv(socket_fd, p, strlen(p), 0);
    int tasktype = atoi(p);
    int rc=send(socket_fd, "ACK", 3, 0);
    if(rc<0)
    {
        printf("Server: Error send ACK.");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    Agent* agent=(Agent*)malloc(sizeof(Agent));
    agent->idAgent=idAgent++;
    agent->isBusy=0;
    agent->socketfd=socket_fd;
    agent->taskType=tasktype;

    return agent;
}

void addAgentInQueue(Agent* newAgent, AgentQueue queue)
{
    newAgent->next = NULL;

    if (queue.rear == NULL) {
        queue.front = newAgent;
        queue.rear = newAgent;
    } else {
        
        queue.rear->next = newAgent;
        queue.rear = newAgent;
    }

}


void debugTaskQueue(TaskQueue *queue) {
    pthread_mutex_lock(&queue->lock); // Protejează accesul cu mutex
    Task *current = queue->front;
    if (!current) {
        printf("TaskQueue este goală.\n");
    } else {
        printf("Conținutul TaskQueue:\n");
        while (current) {
            printf("Task: clientId=%d\n", current->client->idClient);
            current = current->next;
        }
    }
    pthread_mutex_unlock(&queue->lock);
}


int main(int argc,char* argv[])
{
   int serverSocket = createSocket(PORT);

    // initializare task queue-uri
    initQueue(&taskQueue);
    initQueue(&taskQueue);
    initializeQueueAgent(agentQueue);

    // initializare threadpool-uri
    ThreadPool clientPool, agentPool;
    initThreadPool(&clientPool, &taskQueue, processClientTask);
    //initThreadPool(&agentPool, &agentQueue, processAgentTask);

    printf("Server is listening on port %d. TID = %lu\n", PORT,pthread_self());

    struct sockaddr_in* client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (1)
    {
        //debugTaskQueue(&taskQueue);
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
            int rc=send(new_socket, "ACK", 3, 0);
            
            printf("Agent connected.\n");

            Agent* agent=createAgent(new_socket);  
            addAgentInQueue(agent, agentQueue);
        }
    }

    close(serverSocket);
    return 0;
}