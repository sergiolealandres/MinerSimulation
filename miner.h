#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>


#define PRIME 99997669
#define BIG_X 435679812
#define BIG_Y 100001819
#include "block.h"
#define QUEUE_NAME "/mq_queue"

#define OK 0
#define ERR -1
#define ZERO 0
#define SECS 5
#define SECS3 3

#define BREAK 12
#define MAX_WORKERS 10

#define SHM_NAME_NET "/netdata"
#define SHM_NAME_BLOCK "/block"

#define MAX_MINERS 200


typedef struct _NetData {
    pid_t miners_pid[MAX_MINERS];
    char voting_pool[MAX_MINERS];
    int last_miner;
    int active_miners;
    int total_miners;
    pid_t monitor_pid;
    pid_t last_winner;
    int count;
    int count_activos;
    int quorum;
    int votos;
    sem_t barrera;
    sem_t mutex;
    sem_t barrera_activos;
    sem_t barrera_ganador;
} NetData;

typedef struct _TrabajadorInfo {
    long int target;
    long int inicio;
    long int fin;
    long int salida;
} TrabajadorInfo;



long int simple_hash(long int number);

void print_blocks(Block * plast_block, int num_wallets);
