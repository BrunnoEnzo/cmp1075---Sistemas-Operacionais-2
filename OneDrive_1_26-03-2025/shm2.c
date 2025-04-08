/* O segundo programa é o produtor e permite inserir dados para os consumidores.
    Ele é muito semelhante ao shm1.c. */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/shm.h>  // Para memória compartilhada
#include <sys/sem.h>  // Para semáforos

#include "shm_com.h"  // Arquivo de cabeçalho para memória compartilhada
#include "semun.h"    // Arquivo de cabeçalho para manipulação de semáforos

// Declaração de funções auxiliares para manipulação de semáforos
static int set_semvalue(int);
static void del_semvalue(int);
static void del_mutexvalue(int);
static int down(int);
static int up(int);

// IDs dos semáforos
static int sem_idVazio;  // Semáforo para controlar espaços vazios
static int sem_idCheio;  // Semáforo para controlar espaços cheios
static int sem_idMutex;  // Semáforo para controle de exclusão mútua

int main()
{
     int running = 1;  // Variável para controlar o loop principal
     void *shared_memory = (void *)0;  // Ponteiro para a memória compartilhada
     struct shared_use_st {
          int pos_p;  // Posição do produtor
          int pos_c;  // Posição do consumidor
          char vetor[10][BUFSIZ];  // Buffer circular com 10 posições
     };
     struct shared_use_st *shared_stuff;  // Estrutura para acessar a memória compartilhada
     char buffer[BUFSIZ];  // Buffer para entrada de dados
     int shmid;  // ID da memória compartilhada

     // Criação dos semáforos
     sem_idVazio = semget((key_t)123, 1, 0666 | IPC_CREAT);
     sem_idCheio = semget((key_t)321, 1, 0666 | IPC_CREAT);
     sem_idMutex = semget((key_t)456, 1, 0666 | IPC_CREAT);

     // Verifica se os semáforos foram criados corretamente
     if (sem_idVazio == -1) {
          fprintf(stderr, "Falha ao criar o semáforo vazio\n");
          exit(EXIT_FAILURE);
     }
     if (sem_idCheio == -1) {
          fprintf(stderr, "Falha ao criar o semáforo cheio\n");
          exit(EXIT_FAILURE);
     }
     if (!set_semvalue(sem_idCheio)) {  // Inicializa o semáforo cheio
          fprintf(stderr, "Falha ao inicializar o semáforo cheio\n");
          exit(EXIT_FAILURE);
     }
     sleep(2);  // Pausa para garantir a inicialização

     // Criação da memória compartilhada
     shmid = shmget((key_t)123456, sizeof(struct shared_use_st), 0666 | IPC_CREAT);

     // Anexa a memória compartilhada ao espaço de endereçamento do processo
     shared_memory = shmat(shmid, (void *)0, 0);

     printf("Memória anexada em %X\n", (int)shared_memory);

     shared_stuff = (struct shared_use_st *)shared_memory;  // Cast para acessar a estrutura
     for (int i = 0; i < 10; i++) {
          strncpy(shared_stuff->vetor[i], "", BUFSIZ);  // Inicializa o buffer com strings vazias
     }
     shared_stuff->pos_c = 0;  // Inicializa a posição do consumidor
     shared_stuff->pos_p = 0;  // Inicializa a posição do produtor

     // Loop principal do produtor
     while (running) {
          // Aguarda até que haja espaço vazio no buffer
          while (!down(sem_idVazio)) {
                sleep(1);
                printf("Aguardando cliente...\n");
          }

          // Solicita entrada do usuário
          printf("Digite um texto: ");
          fgets(buffer, BUFSIZ, stdin);

          // Copia o texto para o buffer compartilhado
          strncpy(shared_stuff->vetor[shared_stuff->pos_p], buffer, BUFSIZ);
          shared_stuff->pos_p = (shared_stuff->pos_p + 1) % 10;  // Atualiza a posição do produtor

          up(sem_idCheio);  // Incrementa o semáforo cheio

          // Verifica se o texto digitado é "end" para encerrar
          if (strncmp(buffer, "end", 3) == 0) {
                running = 0;
          }
     }

     // Remove os semáforos antes de encerrar
     del_semvalue(sem_idVazio);
     del_semvalue(sem_idCheio);
     del_mutexvalue(sem_idMutex);

     // Desanexa a memória compartilhada
     shmdt(shared_memory);

     exit(EXIT_SUCCESS);
}

// Função para inicializar o valor de um semáforo
static int set_semvalue(int sem_id)
{
     union semun sem_union;

     sem_union.val = 0;  // Inicializa o semáforo com valor 0
     if (semctl(sem_id, 0, SETVAL, sem_union) == -1) return (0);
     return (1);
}

// Função para remover um semáforo
static void del_semvalue(int sem_id)
{
     union semun sem_union;

     if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1)
          fprintf(stderr, "Falha ao deletar o semáforo\n");
}

// Função para remover um semáforo de exclusão mútua
static void del_mutexvalue(int sem_id)
{
     if (semctl(sem_id, 0, IPC_RMID) == -1) {
          fprintf(stderr, "Falha ao deletar o semáforo\n");
     }
}

// Função para incrementar o valor de um semáforo (operação V)
static int up(int sem_id)
{
     struct sembuf sem_b;

     sem_b.sem_num = 0;
     sem_b.sem_op = 1;  // Incrementa o semáforo
     sem_b.sem_flg = SEM_UNDO;
     if (semop(sem_id, &sem_b, 1) == -1) {
          fprintf(stderr, "Falha ao incrementar o semáforo\n");
          return (0);
     }
     return (1);
}

// Função para decrementar o valor de um semáforo (operação P)
static int down(int sem_id)
{
     struct sembuf sem_b;

     sem_b.sem_num = 0;
     sem_b.sem_op = -1;  // Decrementa o semáforo
     sem_b.sem_flg = SEM_UNDO;
     if (semop(sem_id, &sem_b, 1) == -1) {
          fprintf(stderr, "Falha ao decrementar o semáforo\n");
          return (0);
     }
     return (1);
}