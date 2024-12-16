#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
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

    return 0;
}
