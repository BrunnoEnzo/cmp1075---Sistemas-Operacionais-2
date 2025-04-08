/* O segundo programa é o consumidor e permite consumir dados de uma memória compartilhada. */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/shm.h> // Para manipulação de memória compartilhada

#include "shm_com.h" // Cabeçalho personalizado (não fornecido)

#include <sys/sem.h> // Para manipulação de semáforos
#include "semun.h"   // Definição da união semun (não fornecida)

// Declaração de funções auxiliares para manipulação de semáforos
static int set_semvalue(int);
static void del_semvalue(int);
static int set_mutexvalue(int);
static void del_mutexvalue(int);
static int up(int);
static int down(int);

// IDs dos semáforos
static int sem_idVazio;
static int sem_idCheio;
static int sem_idMutex;

int main(int argc, char *argv[])
{
    int running = 1; // Variável para controlar o loop principal
    void *shared_memory = (void *)0; // Ponteiro para a memória compartilhada
    struct shared_use_st {
        int pos_p; // Posição do produtor
        int pos_c; // Posição do consumidor
        char vetor[10][BUFSIZ]; // Buffer circular para armazenar dados
    };
    struct shared_use_st *shared_stuff; // Estrutura para acessar a memória compartilhada
    int shmid; // ID da memória compartilhada

    srand((unsigned int)getpid()); // Inicializa o gerador de números aleatórios com o PID do processo

    // Criação dos semáforos
    sem_idVazio = semget((key_t)123, 1, 0666 | IPC_CREAT);
    sem_idCheio = semget((key_t)321, 1, 0666 | IPC_CREAT);
    sem_idMutex = semget((key_t)456, 1, 0666 | IPC_CREAT);

    // Verifica se os semáforos foram criados com sucesso
    if (sem_idVazio == -1) {
        fprintf(stderr, "Falha ao criar o semáforo vazio\n");
        exit(EXIT_FAILURE);
    }
    if (sem_idCheio == -1) {
        fprintf(stderr, "Falha ao criar o semáforo cheio\n");
        exit(EXIT_FAILURE);
    }

    // Inicializa os semáforos
    if (!set_semvalue(sem_idVazio)) {
        fprintf(stderr, "Falha ao inicializar o semáforo vazio\n");
        exit(EXIT_FAILURE);
    }
    sleep(2); // Pausa para garantir a inicialização
    if (!set_mutexvalue(sem_idMutex)) {
        fprintf(stderr, "Falha ao inicializar o semáforo mutex\n");
        exit(EXIT_FAILURE);
    }

    // Criação da memória compartilhada
    shmid = shmget((key_t)123456, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
    if (shmid == -1) {
        fprintf(stderr, "Falha ao criar a memória compartilhada\n");
        exit(EXIT_FAILURE);
    }

    // Anexa a memória compartilhada ao espaço de endereçamento do processo
    shared_memory = shmat(shmid, (void *)0, 0);
    if (shared_memory == (void *)-1) {
        fprintf(stderr, "Falha ao anexar a memória compartilhada\n");
        exit(EXIT_FAILURE);
    }

    printf("Memória anexada em %X\n", (int)shared_memory);

    shared_stuff = (struct shared_use_st *)shared_memory; // Associa a estrutura à memória compartilhada

    // Loop principal para consumir dados
    while (running) {
        if (down(sem_idCheio)) { // Aguarda até que haja dados disponíveis
            down(sem_idMutex); // Entra na seção crítica
            printf("Você escreveu: %s", shared_stuff->vetor[shared_stuff->pos_c]);

            sleep(rand() % 4); // Simula o tempo de processamento

            shared_stuff->pos_c = (shared_stuff->pos_c + 1) % 10; // Atualiza a posição do consumidor no buffer circular

            up(sem_idMutex); // Sai da seção crítica

            up(sem_idVazio); // Incrementa o semáforo vazio

            // Verifica se a mensagem "end" foi recebida para encerrar o programa
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

// Função para inicializar o valor de um semáforo
static int set_semvalue(int sem_id)
{
    union semun sem_union;

    sem_union.val = 10; // Define o valor inicial do semáforo
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) return(0);
    return(1);
}

// Função para inicializar o valor do semáforo mutex
static int set_mutexvalue(int sem_id)
{
    union semun sem_union;

    sem_union.val = 1; // Define o valor inicial do mutex
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) return(0);
    return(1);
}

// Função para remover um semáforo
static void del_semvalue(int sem_id)
{
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        fprintf(stderr, "Falha ao deletar o semáforo\n");
    }
}

// Função para remover o semáforo mutex
static void del_mutexvalue(int sem_id)
{
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        fprintf(stderr, "Falha ao deletar o semáforo mutex\n");
    }
}

// Função para incrementar o valor de um semáforo (operação V)
static int up(int sem_id)
{
    struct sembuf sem_b;
    
    sem_b.sem_num = 0;
    sem_b.sem_op = 1; // Incrementa o semáforo
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "Falha na operação V do semáforo\n");
        return(0);
    }
    return(1);
}

// Função para decrementar o valor de um semáforo (operação P)
static int down(int sem_id)
{
    struct sembuf sem_b;
    
    sem_b.sem_num = 0;
    sem_b.sem_op = -1; // Decrementa o semáforo
    sem_b.sem_flg = SEM_UNDO;
    if (semop(sem_id, &sem_b, 1) == -1) {
        fprintf(stderr, "Falha na operação P do semáforo\n");
        return(0);
    }
    return(1);
}