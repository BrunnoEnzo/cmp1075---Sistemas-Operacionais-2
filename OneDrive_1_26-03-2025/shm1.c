#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/shm.h>

#include "shm_com.h"

#include <sys/sem.h>
#include "semun.h"

static int set_semvalue(int);
static void del_semvalue(int);
static int set_mutexvalue(int);
static void del_mutexvalue(int);
static int up(int);
static int down(int);

static int sem_idVazio;
static int sem_idCheio;
static int sem_idMutex;

int main(int argc, char *argv[])
{
    int running = 1;
    void *shared_memory = (void *)0;
    struct shared_use_st {
        int pos_p;
        int pos_c;
        char vetor[10][BUFSIZ];
        int written_by_you;
    };
    struct shared_use_st *shared_stuff;
    int shmid;

    srand((unsigned int)getpid());    

    sem_idVazio = semget((key_t)123, 1, 0666 | IPC_CREAT);
    sem_idCheio = semget((key_t)321, 1, 0666 | IPC_CREAT);
    sem_idMutex = semget((key_t)456, 1, 0666 | IPC_CREAT);

    if (sem_idVazio == -1) {
        fprintf(stderr, "Failed to create semaphore vazio\n");
        exit(EXIT_FAILURE);
    }
    if (sem_idCheio == -1) {
        fprintf(stderr, "Failed to create semaphore cheio\n");
        exit(EXIT_FAILURE);
    }

    if (!set_semvalue(sem_idVazio)) {
        fprintf(stderr, "Failed to initialize semaphore cheio  \n");
        exit(EXIT_FAILURE);
    }
    sleep(2);
    if (!set_mutexvalue(sem_idMutex)) {
        fprintf(stderr, "Failed to initialize semaphore mutex\n");
        exit(EXIT_FAILURE);
    }

    shmid = shmget((key_t)123456, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
    if (shmid == -1) {
        fprintf(stderr, "Failed to create shared memory\n");
        exit(EXIT_FAILURE);
    }

    shared_memory = shmat(shmid, (void *)0, 0);
    if (shared_memory == (void *)-1) {
        fprintf(stderr, "Failed to attach shared memory\n");
        exit(EXIT_FAILURE);
    }

    printf("Memory attached at %X\n", (int)shared_memory);

    shared_stuff = (struct shared_use_st *)shared_memory;
    shared_stuff->written_by_you = 0;

    while (running) {
        if (down(sem_idCheio)) {
            down(sem_idMutex);
            printf("You wrote: %s", shared_stuff->vetor[shared_stuff->pos_c]);

            sleep(rand() % 4); 

            shared_stuff->pos_c = (shared_stuff->pos_c + 1) % 10; 

            shared_stuff->written_by_you = 0;
            up(sem_idMutex);

            up(sem_idVazio);

            if (strncmp(shared_stuff->vetor[shared_stuff->pos_c], "end", 3) == 0) {
                running = 0;
            }
        }
    }
    // Remove os semáforos antes de encerrar
    del_semvalue(sem_idVazio);
    del_semvalue(sem_idCheio);

    // Desanexa a memória compartilhada
    shmdt(shared_memory);

    exit(EXIT_SUCCESS);
}

static int set_semvalue(int sem_id)
{
    union semun sem_union;

    sem_union.val = 10;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) return(0);
    return(1);
}
static int set_mutexvalue(int sem_id)
{
    union semun sem_union;

    sem_union.val = 1;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) return(0);
    return(1);
}

/* The del_semvalue function has almost the same form, except the call to semctl uses
 the command IPC_RMID to remove the semaphore's ID. */

 static void del_semvalue(int sem_id)
 {
     if (semctl(sem_id, 0, IPC_RMID) == -1) {
         fprintf(stderr, "Failed to delete semaphore\n");
     }
 }
 static void del_mutexvalue(int sem_id)
 {
     if (semctl(sem_id, 0, IPC_RMID) == -1) {
         fprintf(stderr, "Failed to delete semaphore\n");
     }
 }

static int up(int sem_id)
{
    struct sembuf sem_b;
    
    sem_b.sem_num = 0;
    sem_b.sem_op = 1; /* V() */
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_v failed\n");
        return(0);
    }
    return(1);
}
static int down(int sem_id)
{
    struct sembuf sem_b;
    
    sem_b.sem_num = 0;
    sem_b.sem_op = -1; /* P() */
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "semaphore_p failed\n");
        return(0);
    }
    return(1);
}