#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <asm-generic/socket.h>

#define MAX_NR_TASKS 10
#define MAX_NR_AGENTS 10
#define MAX_NR_CLIENTS 10
#define PORT 8080
#define BUFFER_SIZE 1024


typedef struct Agent
{
    int idAgent;
    int isBusy;
    int taskType;  // 0-usor  1-mediu  2-dificil
    // de completat
}Agent;

typedef struct Client
{
    int idClient;
    int isWaiting;
    //de completat
}Client;

typedef struct Task
{
    int taskType;
    char* fileName;
    int* args; // premiza initiala este doar argumente nr intregi
    size_t dimFile; // pentru verificarea integritatii fisierului
    Client* client;
    Agent* agent; // agentul care va executa task-ul -> pentru evidenta unor log-uri
    //de completat
}Task;


typedef struct TaskQueue
{
    Task taskQueue[MAX_NR_TASKS];
    int taskCount;  
}TaskQueue;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

// functie creare socket

// functie handle client

// functie handle agent

// mecanism threadpool pentru clienti
                                            // o structura ce va avea 2 instante
                                            // (una pt clienti,alta pt agenti)
                                            // rezulta, structura sa fie usor generica
// mecanism threadpool pentru agenti

int main(int argc,char* argv[])
{
    
    return 0;
}