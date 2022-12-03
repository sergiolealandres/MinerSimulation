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
#include "miner.h"

#define PRIME 99997669
#define BIG_X 435679812
#define BIG_Y 100001819
static volatile sig_atomic_t found_solution = 0;
static volatile sig_atomic_t got_sigusr2 = 0;
static volatile sig_atomic_t got_sigint =0;
static volatile sig_atomic_t got_sigalarm =0;
static volatile sig_atomic_t got_sigusr1 =0;

void manejadorSigUsr1(int signal){
    got_sigusr1=1;
}

void manejadorSigAlarm(int signal){
    got_sigalarm=1;
}

void manejadorSigInt(int signal){
    got_sigint=1;
}

//Vamos a tener que arreglar problemas de concurrencia
void manejadorSolucion(int signal){
    found_solution = 1;
    got_sigusr2=1;
    printf("HE RECIBIDO LA PUTA SEÑAL\n");
}

//hola soy un piratilla
long int simple_hash(long int number) {
    long int result = (number * BIG_X + BIG_Y) % PRIME;
    return result;
}

void trabajador(void *arg){
    long int i;
    
    TrabajadorInfo *ti;
    if(!arg)return;
    //printf("soy un currante\n");

    ti = (TrabajadorInfo*) arg;
    printf("inicio: %ld, fin: %ld, target: %ld\n", ti->inicio, ti->fin, ti->target);
    for (i = ti->inicio; i < ti->fin && found_solution==0; i++) {
        //fprintf(stdout, "Searching... %6.2f%%\r", 100.0 * i / PRIME);
        if (ti->target == simple_hash(i)) {
            fprintf(stdout, "\nSolution: %ld\n", i);
            found_solution=1;
            ti->salida = i;
            //printf("la salida es %ld\n",i);
            return;
        }
    }
    //printf("no lo he encontardo\n");
    return;
}

void liberar_recursos(NetData *net,Block *block,Block **lista_block){
    Block *current,*next;
    if(net) munmap(net , sizeof(NetData));
    if(block) munmap(block, sizeof(Block));
    if(lista_block){
        if(*lista_block){
            current = *lista_block;
            while(current){
                next = current->next;
                blockFree(current);
                current = next;
            }
            free(lista_block);
        }
    }
}

int NetDataInitialize(NetData **net,int* miner_id){
    int fd_shm,flag_create = 0;
    fd_shm = shm_open(SHM_NAME_NET, O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR);
    if(fd_shm == -1){
        if(errno == EEXIST) {
            fd_shm = shm_open (SHM_NAME_NET, O_RDWR, 0) ;
            if (fd_shm == -1) {
                perror ("Error opening the shared memory segment") ;
                return -1;
            }
        }
        else {
            perror ("Error creating the shared memory segment \n") ;
            return -1;
        }
    }
    else {
        flag_create = 1;
        /* Resize of the memory segment. */
        if (ftruncate(fd_shm, sizeof(NetData)) == -1) {
            perror("ftruncate");
            shm_unlink(SHM_NAME_NET);
            return -1;
        }
    }
    *net = mmap(NULL, sizeof(NetData), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (*net == MAP_FAILED) {
        perror("mmap");
        //shm_unlink(SHM_NAME_NET);
        return -1;
    }
    
    if(flag_create){
        sem_init(&((*net)->mutex),1,1);
        sem_init(&((*net)->barrera),1,0);
        sem_init(&((*net)->barrera_activos),1,0);
        sem_init(&((*net)->barrera_ganador),1,0);
        (*net)->count = 0;
        (*net)->count_activos = 0;
        (*net)->miners_pid[0] = getpid();
        (*net)->voting_pool[0] = '3';
        (*net)->last_miner = 0;
        (*net)->total_miners = 1;
        (*net)->monitor_pid=-1;
       
    }
    else{
       
        while(sem_wait(&((*net)->mutex))){
            if(errno != EINTR){
            perror("sem_wait");
            return -1;
            }
        }
        (*net)->miners_pid[(*net)->last_miner + 1] = getpid();
        (*net)->voting_pool[(*net)->last_miner + 1] = '3';
        ((*net)->last_miner)++;
        ((*net)->total_miners)++;
        sem_post(&((*net)->mutex));
    }

    (*miner_id) = (*net)->last_miner;

    return 0;
}

int wait_mutex(NetData *net){

    struct timespec t;
    
    if (clock_gettime(CLOCK_REALTIME, &t) == -1) {
        perror("clock_gettime");
        return ERR;
    }
    t.tv_sec += 5;

    while((sem_timedwait(&net->mutex,&t))){
        
         if(errno==ETIMEDOUT){
            perror("time out mutex");
            fprintf(stderr, "Por errores graves en la estructura vamos a finalizar la ejecución del programa\n");
            return ERR;
        }
        if(errno != EINTR){
            perror("mutex");
            return ERR;
        }
    }
}

int BlockInitialize(Block **block){
    int fd_shm;
    int created=0;
    fd_shm = shm_open(SHM_NAME_BLOCK, O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR);
    if(fd_shm == -1){
        if(errno == EEXIST) {
            fd_shm = shm_open (SHM_NAME_BLOCK, O_RDWR, 0) ;
            if (fd_shm == -1) {
                perror ("Error opening the shared memory segment") ;
                return -1;
            }
        }
        else {
            perror ("Error creating the shared memory segment \n") ;
            return -1;
        }
    }
    else {
        /* Resize of the memory segment. */
        if (ftruncate(fd_shm, sizeof(Block)) == -1) {
            perror("ftruncate");
            shm_unlink(SHM_NAME_NET);
            return -1;
        }
        created = 1;
    }
    
    *block = mmap(NULL, sizeof(Block), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (*block == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME_NET);
        return -1;
    }
    if(created){
        srand(getpid());
        (*block)->id=1;
        (*block)->target= (long int)((((double)rand())/RAND_MAX)*(PRIME-1));
        //printf("el id es %d\n",(*block)->id);
    } 
    return 0;
}

int manejadorInitialize(){
    struct sigaction act;

    sigemptyset(&(act.sa_mask));
    act.sa_flags=0;
    act.sa_handler=manejadorSolucion;

    /*Asignamos a todos los procesos la rutina manejadorusr1 ante la señal SIGUSR1*/
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        return -1;
    }

    act.sa_handler=manejadorSigInt;

   if (sigaction(SIGINT, &act, NULL) < 0) {
       return -1;
    }

    act.sa_handler=manejadorSigAlarm;

    if (sigaction(SIGALRM, &act, NULL) < 0) {
       return -1;
    }

    act.sa_handler=manejadorSigUsr1;

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
       return -1;
    }
}

int crear_hilos(int nTrabajador,TrabajadorInfo *ti,pthread_t *trabajadores,Block *block,NetData *net,Block ** lista_block){
    int i;
    for(i=0; i<nTrabajador;i++){
        ti[i].salida = -1;
        //mirar esto, hay que detenerles
        ti[i].target = block->target;
        ti[i].inicio = i*(PRIME/nTrabajador);
        if(i==nTrabajador-1) ti[i].fin=PRIME;

        else ti[i].fin = (i+1)*(PRIME/nTrabajador);
        if(pthread_create(&trabajadores[i],NULL,(void*)trabajador,&ti[i])){
            liberar_recursos(net,block,lista_block);
            return -1;
        }
    }
    return 0;
}

int recoger_hilos(int nTrabajador, TrabajadorInfo *ti,pthread_t *trabajadores,Block *block,NetData *net,Block ** lista_block, int miner_id){
    int i,k;
    for(i=0;i<nTrabajador;i++){
        pthread_join(trabajadores[i],NULL);
        if(ti[i].salida!=-1){
            //utilizar un semaforo nuevo, si no se ha recibido sigUSR2 aun se manda a todos
            if(wait_mutex(net)==ERR){
                liberar_recursos(net, block, lista_block);
                return ERR;
            }
           
            if(got_sigusr2==0){
                printf("soy el puto campeon y actualizaré el target\n");
                net->last_winner=getpid();
                for(k=0;k<=net->last_miner;k++){
                    if(net->miners_pid[k]){//COMPROBAR SI ESTAN ACTIVOS PARA QUORUM
                        kill(net->miners_pid[k],SIGUSR2);
                    }
                }  
                
                
                block->is_valid=1;
                block->solution=ti[i].salida;
                block->wallets[miner_id]++;
            }
            sem_post(&net->mutex);
        }
    }
}

void barrera_start(NetData *net){
    int i;
    if(net->total_miners == net->count){
        net->active_miners = net->total_miners;
        for(i=0;i<=net->last_miner;i++){
            if(net->miners_pid[i]!=0){
                net->voting_pool[i] = '2';
            }
        }
        for(i=0;i<net->count;i++){
            sem_post(&net->barrera);
        }
        net->count = 0;
    }
}

int comprobacionAlarm(NetData *net){
    int k;
    if(wait_mutex(net)==ERR)return ERR;
    if(got_sigusr1==0){
        printf("bahe entrado aquí1\n");
        for (k=0;k<=net->last_miner;k++){
            if(net->miners_pid[k]!=getpid()&&net->miners_pid[k]){
                //REVISAR
                if(kill(net->miners_pid[k],SIGUSR1)==-1){
                    if(errno==ESRCH){
                        printf("deberia de entrar\n");
                        net->miners_pid[k]=0;
                        net->voting_pool[k] = '3';
                        net->total_miners--;
                    }
                    else{
                        perror("kill");
                        return -1;
                    }
                }
            }
            
        }
        net->count_activos=0;
        
    }
    sem_post(&net->mutex);
}

int barrera(NetData *net){
    int i;
    got_sigusr1=0;
    //printf("hasta aquí llego, total miners es %d y count %d:\n", net->total_miners, net->count);
    if(wait_mutex(net)==ERR)return ERR;

    net->count++;
    printf("El count es %d\n",net->count);
    barrera_start(net);
    
    sem_post(&net->mutex);
    //printf("hasta aquí llego\n");
    alarm(SECS);
    while(sem_wait(&(net->barrera))){
        if(errno != EINTR){
            perror("sem_wait");
            return -1;
        }

        if(got_sigalarm==1){
            if(comprobacionAlarm(net)==-1) return -1;
            break;
        }
        
    }
    alarm(ZERO);
    got_sigalarm=0;
    return 0;

}

int barrera_activos(NetData *net){
    int i;
    got_sigusr1=0;
    //printf("hasta aquí llego, total miners es %d y count %d:\n", net->total_miners, net->count);
    if(wait_mutex(net)==ERR)return ERR;
    printf("active es %d y total es %d\n", net->active_miners, net->total_miners);
    net->count_activos++;
    if(net->count_activos == net->active_miners){
        for(i=0;i<net->count_activos;i++){
            sem_post(&net->barrera_activos);
        }
        net->count_activos = 0;
    }
    sem_post(&net->mutex);

    alarm(SECS);
    while(sem_wait(&(net->barrera_activos))){
        if(errno != EINTR){
            perror("sem_wait");
            return -1;
        }

        if(got_sigalarm==1){
            if(comprobacionAlarm(net)==-1) return -1;
            break;
        }
    }
    printf("aquí llego\n");
    alarm(ZERO);
    got_sigalarm=0;
    return 0;
}

int wait_winner(NetData *net){
    int i;
    got_sigusr1=0;
    if(wait_mutex(net)==ERR)return ERR;
    if(net->last_winner == getpid()){
        for(i=0;i<net->active_miners;i++){
            sem_post(&net->barrera_ganador);
        }
    }
    sem_post(&net->mutex);
    alarm(SECS);
    while(sem_wait(&(net->barrera_ganador))){
        if(errno != EINTR){
            perror("sem_wait");
            return -1;
        }

        if(got_sigalarm==1){
            printf("whe entrado aquí\n");
            while(sem_wait(&(net->mutex))){
                if(errno != EINTR){
                    perror("sem_wait");
                    return -1;
                }
            }
            net->last_winner=0;
            sem_post(&net->mutex);
            break;
        }
    }
    alarm(ZERO);
    got_sigalarm=0;
    return 0;
}

int votacion_loser(NetData *net,Block *block,int miner_id){
    int i;
    sigset_t mask, oldmask;
    got_sigalarm = 0;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGALRM);

    alarm(SECS);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while(got_sigusr2 == 0 && got_sigalarm == 0){
        
        sigsuspend(&oldmask);
        
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    alarm(ZERO);
    if(got_sigalarm==1){
        return 1;
    }

    if(wait_mutex(net)==ERR)return ERR;

    net->voting_pool[miner_id] = '0';
    
    net->votos++;
    if(net->votos == net->quorum){
        if(kill(net->last_winner,SIGUSR2)==-1){
            if(errno==ESRCH){
                perror("The winner is dead");
            }
            else{
                sem_post(&net->mutex);
                return -1;
            }

        }
    }
    sem_post(&net->mutex);
    
    
    return 0;
}




int votacion_winner(NetData *net,Block *block,int miner_id){
    int i,votes=0;
    int active_id[MAX_MINERS];
    sigset_t mask, oldmask;
    got_sigalarm = 0;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGALRM);
    printf("empezando la votacion soy el winner\n");
    if(wait_mutex(net)==ERR)return ERR;
    
    net->votos = 0;
    for(i=0,net->quorum = 0;i<=net->last_miner;i++){
        printf("LA i es %d, minerid es %d, net vpool i es %c\n", i, miner_id, net->voting_pool[i]);
        if(i != miner_id &&net->miners_pid[i] != 0 && net->voting_pool[i] == '2'){
            printf("ESTOY EN WINNER VOY A MANDAR LA SEÑAL\n");
            if(kill(net->miners_pid[i],SIGUSR1)==-1){
                if(errno !=ESRCH){
                    perror("kill");
                    return -1;
                }
            }
            else{
                active_id[net->quorum] = i;
                net->quorum++;
            }
        }
    }
    sem_post(&net->mutex);

    for(i=0;i<net->quorum;i++){
        if(kill(net->miners_pid[active_id[i]],SIGUSR2)==-1){
            perror("kill");
            return -1;
        }
    }
    printf("EL quorum es %d\n",net->quorum);
    if(net->quorum == 0){
        block->is_valid = 1;
        return 0;
    } 
    alarm(SECS);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while(got_sigusr2 == 0 && got_sigalarm == 0){
        
        sigsuspend(&oldmask);
        
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    alarm(ZERO);
    if(got_sigalarm==1){
        return 1;
    }

    for(i=0;i<net->quorum;i++){
        if(net->voting_pool[active_id[i]] == '1'){
            votes++;
        }
    }
    if(2*votes > net->quorum){
        block->is_valid = 1;
    }
    else{
        block->is_valid = 0;
    }
    return 0;
}

int sendAdd(NetData *net, Block **lista_block,Block *block,mqd_t queue, int prio){
    if(wait_mutex(net)==-1){
        return -1;
        
    }
    if(net->monitor_pid!=-1) {
        printf("HAY MINERO\n");
        mq_send(queue, (const char*)block, sizeof(Block), prio);
    }
    sem_post(&net->mutex);
    addBlock(lista_block,block);
    return 0;
}

int esperarYActualizar(NetData *net, Block *block){
    if(barrera_activos(net)==-1){
        return -1;
    }
    printf("paso la barrera\n");
    if(getpid()==net->last_winner){
        block->id++;
        block->target=block->solution;
    }
    return 0;
}

void salirYLiberar(NetData *net,Block *block,Block ** lista_block,int miner_id, mqd_t queue){
    if(wait_mutex(net)==-1){
        liberar_recursos(net, block, lista_block);
        exit(EXIT_FAILURE);
    }
    print_blocks(*lista_block,net->last_miner);
    net->miners_pid[miner_id] = 0;
    net->voting_pool[miner_id] = '3';
    net->total_miners--;
    
    sem_post(&net->mutex);
    mq_close(queue);
    liberar_recursos(net, block,lista_block);
}

int main(int argc, char *argv[]) {
    long int i, target, nTrabajador, solucion;
    long int * thread_return;
    int fd_shm, nRondas, j,k, indef=0;
    int miner_id, valor;
    NetData *net=NULL;
    Block *block=NULL;
    TrabajadorInfo ti[MAX_WORKERS];
    pthread_t trabajadores[MAX_WORKERS];
    Block **lista_block=NULL;
    struct sigaction act;
    struct  mq_attr  attributes = {.mq_flags = 0,.mq_maxmsg = 10,.mq_curmsgs = 0,.mq_msgsize = sizeof(Block)};
    mqd_t  queue;
    int prio=0;

    if (argc != 3) {
        fprintf(stderr, "Parámetros erróneos\n");
        exit(EXIT_FAILURE);
    }
        
    nRondas = atoi(argv[2]);
    if(nRondas<=0)indef=1;
    nTrabajador = atol(argv[1]);

    if(!(lista_block = malloc(sizeof(Block*)))){
        fprintf(stderr, "Error de malloc\n");
        exit(EXIT_FAILURE);
    }
    (*lista_block) = NULL;

    if(NetDataInitialize(&net,&miner_id)==-1){
        liberar_recursos(net,block,lista_block);
        exit(EXIT_FAILURE);
    } 
   
    if(BlockInitialize(&block)==-1){
        liberar_recursos(net,block,lista_block);
        exit(EXIT_FAILURE);
    }
   
    if(manejadorInitialize() == -1){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    queue=mq_open(QUEUE_NAME, O_CREAT| O_WRONLY ,S_IRUSR | S_IWUSR , &attributes);
    if(queue== (mqd_t) -1){
        perror("queue create 222");
        exit(EXIT_FAILURE);
    }


    for(j=0;(j<nRondas||indef==1) && got_sigint == 0;j++){
        prio=1;
        printf("an la barrera1\n");
        
        if(barrera(net)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }
        printf("paso la barrera1\n");
        found_solution=0;
        got_sigusr2 = 0;
        got_sigusr1=0;

        printf("La ronda es %d\n", j);
        
        if(crear_hilos(nTrabajador,ti,trabajadores,block,net,lista_block) == -1){
            perror("Thread creation");
            exit(EXIT_FAILURE);
        }

        if(recoger_hilos(nTrabajador,ti,trabajadores,block,net,lista_block,miner_id) == -1){   
            perror("Thread creation");
            exit(EXIT_FAILURE);
        }
        
        got_sigusr2=0;
        if(barrera_activos(net)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }

        if(getpid()==net->last_winner) {
            
            if(votacion_winner(net, block, miner_id)==ERR){
                liberar_recursos(net, block, lista_block);
                exit(EXIT_FAILURE);
            }
            prio=2;
        }
        else{
            if(votacion_loser(net, block, miner_id)==ERR){
                liberar_recursos(net, block, lista_block);
                exit(EXIT_FAILURE);
            }
            prio = 1;
        }
        printf("Fin de la votacion\n");

        if(wait_winner(net)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }

        if(block->is_valid == 0){
            continue;
        }
        
        if(sendAdd(net,lista_block,block,queue,prio)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
            
        }

        printf("antes de la barrera\n");
        if(esperarYActualizar(net,block)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }
    }

    salirYLiberar(net,block,lista_block,miner_id,queue);
    
    exit(EXIT_FAILURE);
}
