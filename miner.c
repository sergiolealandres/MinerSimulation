#include "miner.h"

static volatile sig_atomic_t found_solution = 0;
static volatile sig_atomic_t got_sigusr2 = 0;
static volatile sig_atomic_t got_sigint = 0;
static volatile sig_atomic_t got_sigalarm = 0;
static volatile sig_atomic_t got_sigusr1 = 0;

/***********************************************************/
/* Función: manejadorSigUsr1               
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param signal: entero que hace referencia al código asociado a una señal
/*
/* Descripción:
/* Función utilizada como manejador para todos los procesos cuando reciben la señal SIGUSR1
/* y que internamente únicamente pone a 1 una variable volátil estática que se utiliza para indicar que el 
/* proceso en cuestión ha recibido la señal
/***********************************************************/
void manejadorSigUsr1(int signal){
    got_sigusr1=1;
}

/***********************************************************/
/* Función: manejadorSigAlarm           
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param signal: entero que hace referencia al código asociado a una señal
/*
/* Descripción:
/* Se utilizará principalmente para prevenir interbloqueos cuando a algún 
/* proceso le llegue SIGKILL
/***********************************************************/
void manejadorSigAlarm(int signal){
    got_sigalarm=1;
}

/***********************************************************/
/* Función: manejadorSigInt              
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param signal: entero que hace referencia al código asociado a una señal
/*
/* Descripción:
/* Manejador de la señal SIGINT
/***********************************************************/
void manejadorSigInt(int signal){
    got_sigint=1;
}

/***********************************************************/
/* Función: manejadorSigInt              
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param signal: entero que hace referencia al código asociado a una señal
/*
/* Descripción:
/* Manejador de la señal SIGINT
/***********************************************************/
void manejadorSolucion(int signal){
    found_solution = 1;
    got_sigusr2=1;
    
}

/***********************************************************/
/* Función: simple_hash            
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param number: entero con el que se prueba para buscar la solucion hash
/*
/* Descripción:
/* Función hash simple
/***********************************************************/
long int simple_hash(long int number) {
    long int result = (number * BIG_X + BIG_Y) % PRIME;
    return result;
}

/***********************************************************/
/* Función: trabajador         
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param arg: referencia a una esructura de tipo TrabajadorInfo
/*
/* Descripción:
/* Función para el trabajador en la que busca la solución
/***********************************************************/
void trabajador(void *arg){
    long int i;
    
    TrabajadorInfo *ti;
    if(!arg)return;

    ti = (TrabajadorInfo*) arg;
    
    for (i = ti->inicio; i < ti->fin && found_solution==0; i++) {
        
        if (ti->target == simple_hash(i)) {
            
            found_solution=1;
            ti->salida = i;
            return;
        }
    }
    return;
}

/***********************************************************/
/* Función: liberador_recursos     
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param net: puntero a la estructura de memoria compartida
/* @param block: puntero al bloque compartido
/* @param lista_block: lista doblemente enlazada de bloques de cada proceso
/*
/* Descripción:
/* Función para liberar los recursos de cada minero
/***********************************************************/
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

/***********************************************************/
/* Función: NetDataInitialize
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param net: puntero a la estructura de memoria compartida
/* @param miner_id: puntero a un entero que hace referencia al id de un minero
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Uso para conectarse a la red de mineros
/***********************************************************/
int NetDataInitialize(NetData **net,int* miner_id){
    int fd_shm,flag_create = 0;
    fd_shm = shm_open(SHM_NAME_NET, O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR);
    if(fd_shm == -1){
        if(errno == EEXIST) {
            fd_shm = shm_open (SHM_NAME_NET, O_RDWR, 0) ;
            if (fd_shm == -1) {
                perror ("Error opening the shared memory segment") ;
                return ERR;
            }
        }
        else {
            perror ("Error creating the shared memory segment \n") ;
            return ERR;
        }
    }
    else {
        flag_create = 1;
        /* Resize of the memory segment. */
        if (ftruncate(fd_shm, sizeof(NetData)) == -1) {
            perror("ftruncate");
            shm_unlink(SHM_NAME_NET);
            return ERR;
        }
    }
    *net = mmap(NULL, sizeof(NetData), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (*net == MAP_FAILED) {
        perror("mmap");
        
        return ERR;
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
            return ERR;
            }
        }
        (*net)->miners_pid[(*net)->last_miner + 1] = getpid();
        (*net)->voting_pool[(*net)->last_miner + 1] = '3';
        ((*net)->last_miner)++;
        ((*net)->total_miners)++;
        sem_post(&((*net)->mutex));
    }

    (*miner_id) = (*net)->last_miner;

    return OK;
}

/***********************************************************/
/* Función: wait_mutex
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param net: puntero a la estructura de memoria compartida
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* función para esperar el mutex utilizando sem_timedwait
/***********************************************************/
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

/***********************************************************/
/* Función: BlockInitialize
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param block: pointer to the shared block
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función utilizada para inicializar el bloque comun para todos los procesos
/***********************************************************/
int BlockInitialize(Block **block){
    int fd_shm;
    int created=0;
    fd_shm = shm_open(SHM_NAME_BLOCK, O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR);
    if(fd_shm == -1){
        if(errno == EEXIST) {
            fd_shm = shm_open (SHM_NAME_BLOCK, O_RDWR, 0) ;
            if (fd_shm == -1) {
                perror ("Error opening the shared memory segment") ;
                return ERR;
            }
        }
        else {
            perror ("Error creating the shared memory segment \n") ;
            return ERR;
        }
    }
    else {
        /* Resize of the memory segment. */
        if (ftruncate(fd_shm, sizeof(Block)) == -1) {
            perror("ftruncate");
            shm_unlink(SHM_NAME_NET);
            return ERR;
        }
        created = 1;
    }
    
    *block = mmap(NULL, sizeof(Block), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (*block == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME_NET);
        return ERR;
    }
    if(created){
        srand(getpid());
        (*block)->id=1;
        (*block)->target= (long int)((((double)rand())/RAND_MAX)*(PRIME-1));
        
    } 
    return OK;
}

/***********************************************************/
/* Función: manejador Initialize
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función utilizada para establecer los manejadores de sigint, sigalrm, sigusr1 y sigusr2
/***********************************************************/
int manejadorInitialize(){
    struct sigaction act;

    sigemptyset(&(act.sa_mask));
    act.sa_flags=0;
    act.sa_handler=manejadorSolucion;

    /*Asignamos a todos los procesos la rutina manejadorusr1 ante la señal SIGUSR1*/
    if (sigaction(SIGUSR2, &act, NULL) < 0) {
        return ERR;
    }

    act.sa_handler=manejadorSigInt;

   if (sigaction(SIGINT, &act, NULL) < 0) {
       return ERR;
    }

    act.sa_handler=manejadorSigAlarm;

    if (sigaction(SIGALRM, &act, NULL) < 0) {
       return ERR;
    }

    act.sa_handler=manejadorSigUsr1;

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
       return ERR;
    }
}

/***********************************************************/
/* Función: crear_hilos
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param nTrabajador: número de trabajadores
/* @param ti: array de estructuras de los trabajadores
/* @param trabajadores: puntero a un hilo
/* @param block: puntero al bloque compartido
/* @param net: puntero a la red compartida
/* @param lista_block: lista enlazada de bloques del minero
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función para lanzar los trabajadores
/***********************************************************/
int crear_hilos(int nTrabajador,TrabajadorInfo *ti,pthread_t *trabajadores,Block *block,NetData *net,Block ** lista_block){
    int i;
    for(i=0; i<nTrabajador;i++){
        ti[i].salida = -1;
        
        ti[i].target = block->target;
        ti[i].inicio = i*(PRIME/nTrabajador);
        if(i==nTrabajador-1) ti[i].fin=PRIME;

        else ti[i].fin = (i+1)*(PRIME/nTrabajador);
        if(pthread_create(&trabajadores[i],NULL,(void*)trabajador,&ti[i])){
            liberar_recursos(net,block,lista_block);
            return ERR;
        }
    }
    return OK;
}

/***********************************************************/
/* Función: recoger_hilos
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param nTrabajador: número de trabajadores
/* @param ti: array de estructuras de los trabajadores
/* @param trabajadores: puntero a un hilo
/* @param block: puntero al bloque compartido
/* @param net: puntero a la red compartida
/* @param lista_block: lista enlazada de bloques del minero
/* @param miner_id: id del minero actual
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función para recoger la solución en caso de que los trabajadores la encuentren
/* y desde la que se manda SIGUSR2 al resto de mineros de la red
/***********************************************************/
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
                
                net->last_winner=getpid();
                for(k=0;k<=net->last_miner;k++){
                    if(net->miners_pid[k]){//COMPROBAR SI ESTAN ACTIVOS PARA QUORUM
                        kill(net->miners_pid[k],SIGUSR2);
                    }
                }  
                
                
                block->is_valid=1;
                block->solution=ti[i].salida;
                
            }
            sem_post(&net->mutex);
        }
    }
}

/***********************************************************/
/* Función: barrera_start
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/*
/* Descripción:
/* Función que simula una barrera, el último en llegar es el que levanta el semáforo
/* de barrera a todos. (Esta función solo es llamada desde la función barrera barrera)
/***********************************************************/
void barrera_start(NetData *net){
    int i;
    if(net->total_miners == net->count){
        net->active_miners = net->total_miners;
        //En caso de ser el ultimo en llegar levantará la barrera de todos
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

/***********************************************************/
/* Función: comprobacionAlarm
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/* @param activos: entero que indica la barrera desde la que se llama a la función
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función para prevenir de SIGKILL, en caso de que un minero muera y todos queden bloqueados a la espera.
/***********************************************************/
int comprobacionAlarm(NetData *net, int activos){
    int k;
    if(wait_mutex(net)==ERR)return ERR;
    //solo entrará uno de los procesos vivos aquí
    if(got_sigusr1==0){
        
        for (k=0;k<=net->last_miner;k++){
            if(net->miners_pid[k]!=getpid()&&net->miners_pid[k]){
               //manda a todo el resto de procesos que se creía que estaban activos SIGUSR1
                if(kill(net->miners_pid[k],SIGUSR1)==-1){
                    if(errno==ESRCH){
                        //En caso de que no la reciban porque ya habían muertos, se les borra de la red como activos
                        net->miners_pid[k]=0;
                        net->voting_pool[k] = '3';
                        net->total_miners--;
                    }
                    else{
                        perror("kill");
                        return ERR;
                    }
                }
            }
        }
        net->count_activos=0;
        if(activos)net->count=0;
        
    }
    sem_post(&net->mutex);
}

/***********************************************************/
/* Función: barrera
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función de barrera de inicio de ronda. Cuando un minero llega en medio de una ronda se queda esperando.
/* Llama a la función barrera start que es mediante la cual el último minero de la red en llegar deja paso al resto
/***********************************************************/
int barrera(NetData *net){
    int i;
    got_sigusr1=0;
    
    if(wait_mutex(net)==ERR)return ERR;

    net->count++;
    
    barrera_start(net);
    
    sem_post(&net->mutex);
    
    alarm(SECS3);
    while(sem_wait(&(net->barrera))){
        if(errno != EINTR){
            perror("sem_wait");
            return ERR;
        }

        if(got_sigalarm==1){
            if(comprobacionAlarm(net,1)==-1) return ERR;

            break;
        }
        
    }
    alarm(ZERO);
    got_sigalarm=0;
    return OK;

}

/***********************************************************/
/* Función: barrera_activos
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función de barrera utilizada en dos ocasiones. Primero para esperar a todos los mineros antes de empezar la votación y
/* después para esperar a que todos hayan metido en su cadena el bloque correspondiente (invocada desde esperarYactualizar en este caso)
/***********************************************************/
int barrera_activos(NetData *net){
    int i;
    got_sigusr1=0;
    if(wait_mutex(net)==ERR)return ERR;

    net->count_activos++;
    if(net->count_activos == net->active_miners){
        //El último minero en llegar abre la barrera para todos
        for(i=0;i<net->count_activos;i++){
            sem_post(&net->barrera_activos);
        }
        net->count_activos = 0;
    }
    sem_post(&net->mutex);

    alarm(SECS3);
    //uso de la alarma en caso de que algún proceso haya muerto para saberlo
    while(sem_wait(&(net->barrera_activos))){
        if(errno != EINTR){
            perror("sem_wait");
            return ERR;
        }

        if(got_sigalarm==1){
            if(comprobacionAlarm(net,0)==-1) return ERR;
            break;
        }
    }
    //reset de la alarma
    alarm(ZERO);
    got_sigalarm=0;
    return OK;
}

/***********************************************************/
/* Función: wait_winner
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/* @param block: bloque compartido
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función de sincronización utilizada para esperar al ganador de la ronda, justo después de la votación,
/* ya que este será quien actualice el bloque compartido. Tiene una forma similar a las funciones de barrera,
/* pero no este caso no es el último minero en llegar sino el ganador de la ronda el que deja pasar al resto de procesos
/***********************************************************/
int wait_winner(NetData *net, Block *block){
    int i;
    got_sigusr1=0;
    if(wait_mutex(net)==ERR)return ERR;
    if(net->last_winner == getpid()){
        for(i=0;i<net->active_miners;i++){
            sem_post(&net->barrera_ganador);
        }
    }
    sem_post(&net->mutex);
    alarm(SECS3);
    //mismo significado de antes, previsión de que el último ganador no muera
    while(sem_wait(&(net->barrera_ganador))){
        if(errno != EINTR){
            perror("sem_wait");
            return ERR;
        }

        if(got_sigalarm==1){
           
            while(sem_wait(&(net->mutex))){
                if(errno != EINTR){
                    perror("sem_wait");
                    return ERR;
                }
            }
            net->last_winner=0;
            //en caso de que se vaya el ganador de la ronda en medio de la votación o mientras se le espera
            //para que actualice el bloque, se dice que el bloque no es valido y se empieza de nuevo la ronda
            block->is_valid=0;
            sem_post(&net->mutex);
            break;
        }
    }
    //reset de la alarma
    alarm(ZERO);
    got_sigalarm=0;
    return OK;
}

/***********************************************************/
/* Función: votacion_loser
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/* @param block: bloque compartido
/* @param miner_id: id del minero
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función utilizada por los mineros no ganadores de cada ronda para realizar el proceso de votación
/***********************************************************/
int votacion_loser(NetData *net,Block *block,int miner_id){
    int i;
    sigset_t mask, oldmask;
    got_sigalarm = 0;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGALRM);

    alarm(SECS3);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while(got_sigusr2 == 0 && got_sigalarm == 0){
        
        sigsuspend(&oldmask);
        
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    alarm(ZERO);
    if(got_sigalarm==1){
        block->is_valid=0;
        return OK;
    }

    if(wait_mutex(net)==ERR)return ERR;
    if(simple_hash(block->solution) == block->target){
        net->voting_pool[miner_id] = '1';
    }
    else{
        net->voting_pool[miner_id] = '0';
    }
    net->votos++;
    if(net->votos == net->quorum){
        if(kill(net->last_winner,SIGUSR2)==-1){
            if(errno==ESRCH){
                perror("The winner is dead");
            }
            else{
                sem_post(&net->mutex);
                return ERR;
            }

        }
    }
    sem_post(&net->mutex);
    
    return OK;
}

/***********************************************************/
/* Función: votacion_winner
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/* @param block: bloque compartido
/* @param miner_id: id del minero
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función utilizada por el minero ganador de cada ronda para realizar el proceso de votación
/***********************************************************/
int votacion_winner(NetData *net,Block *block,int miner_id){
    int i,votes=0;
    int active_id[MAX_MINERS];
    sigset_t mask, oldmask;
    got_sigalarm = 0;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGALRM);
    if(wait_mutex(net)==ERR)return ERR;
    
    net->votos = 0;
    for(i=0,net->quorum = 0;i<=net->last_miner;i++){
        
        if(i != miner_id &&net->miners_pid[i] != 0 && net->voting_pool[i] == '2'){
           
            if(kill(net->miners_pid[i],SIGUSR1)==-1){
                if(errno !=ESRCH){
                    perror("kill");
                    return ERR;
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
            return ERR;
        }
    }
    if(net->quorum == 0){
        block->is_valid = 1;
        return OK;
    } 
    alarm(SECS3);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while(got_sigusr2 == 0 && got_sigalarm == 0){
        
        sigsuspend(&oldmask);
        
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    alarm(ZERO);
    if(got_sigalarm==1){
        printf("One of the losing miners left, we will repeat the round");
        block->is_valid=0;
        return OK;
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
    return OK;
}

/***********************************************************/
/* Función: sendAdd
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/* @param block: bloque compartido
/* @param prio: prioridad del mensaje que se envía al monitor
/* @param lista_block: lista enlazada de bloques de cada minero
/* @param queue: cola de mensajes
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función para añadir el bloque a la cadena de bloques y en caso de que haya un monitor activo enviárselo
/***********************************************************/
int sendAdd(NetData *net, Block **lista_block,Block *block,mqd_t queue, int prio){
    if(wait_mutex(net)==-1){
        return ERR;
        
    }
    if(net->monitor_pid!=-1) {
        
        got_sigalarm=0;
        alarm(SECS3);
        //gestion en caso de que el monitor mueras
        mq_send(queue, (const char*)block, sizeof(Block), prio);
        if(got_sigalarm){
            net->monitor_pid=-1;
        }
        alarm(ZERO);
    }
    sem_post(&net->mutex);
    addBlock(lista_block,block);
    return OK;
}

/***********************************************************/
/* Función: esperarYActualizar
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/* @param block: bloque compartido
/* @param miner_id: id del minero
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función mediante la que se espera a todos los mineros tras la votación y en caso de ser el winner actualizar el bloque
/***********************************************************/
int esperarYActualizar(NetData *net, Block *block, int miner_id){
    if(barrera_activos(net)==-1){
        return ERR;
    }
    
    if(getpid()==net->last_winner){
        block->id++;
        block->target=block->solution;
        block->wallets[miner_id]++;
    }
    return OK;
}

/***********************************************************/
/* Función: salirYLiberar
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)           
/*
/* @param net: puntero a la red compartida
/* @param block: bloque compartido
/* @param miner_id: id del minero
/* @param lista_block: lista enlazada de bloques de cada minero
/* @param queue: cola de mensajes
/*
/* Descripción:
/* Función para cuando un minero termina ya sea por sigint o por acabar todas las rondas, libera recursos y se da de baja en la red
/***********************************************************/
void salirYLiberar(NetData *net,Block *block,Block ** lista_block,int miner_id, mqd_t queue){
    if(wait_mutex(net)==-1){
        liberar_recursos(net, block, lista_block);
        exit(EXIT_FAILURE);
    }
    print_blocks(*lista_block,net->last_miner);
    net->miners_pid[miner_id] = 0;
    net->voting_pool[miner_id] = '3';
    net->total_miners--;
    barrera_start(net);
    sem_post(&net->mutex);
    mq_close(queue);
    liberar_recursos(net, block,lista_block);
}

/***********************************************************/
/* Función: main           
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param argc: entero que hace referencia al número de parámetros
/* @param argv: array de cadenas de caracteres que incluyen los argumentos de entrada 
/* pasados por el usuario
/*
/* Descripción:
/* Función principal, contiene inicialización, proceso de busqueda de solución, proceso de votación,
/* liberación y múltiples llamadas a funciones de sincronización explicadas anteriormente
/***********************************************************/
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

    //CDE
    if (argc != 3) {
        fprintf(stderr, "Parámetros erróneos\n");
        exit(EXIT_FAILURE);
    }
        
    nRondas = atoi(argv[2]);
    if(nRondas<=0)indef=1;
    nTrabajador = atol(argv[1]);
    //CDE
    if(nTrabajador>10 || nTrabajador<=0){

        fprintf(stderr, "El numero de trabajadores debe de estar entre 0 y 10\n");
        exit(EXIT_FAILURE);

    }
    //reserva de memoria para la lista enlazada de bloques
    if(!(lista_block = malloc(sizeof(Block*)))){
        fprintf(stderr, "Error de malloc\n");
        exit(EXIT_FAILURE);
    }
    (*lista_block) = NULL;
    //inicialización y unión a la red
    if(NetDataInitialize(&net,&miner_id)==-1){
        liberar_recursos(net,block,lista_block);
        exit(EXIT_FAILURE);
    } 
    //inicialización del bloque compartido
    if(BlockInitialize(&block)==-1){
        liberar_recursos(net,block,lista_block);
        exit(EXIT_FAILURE);
    }
    //establecimiento de manejadores para las distintas señales
    if(manejadorInitialize() == -1){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    //apertura de la cola de mensajes
    queue=mq_open(QUEUE_NAME, O_CREAT| O_WRONLY ,S_IRUSR | S_IWUSR , &attributes);
    if(queue== (mqd_t) -1){
        perror("queue create");
        exit(EXIT_FAILURE);
    }

    //bucle principal de ejecución
    for(j=0;(j<nRondas||indef==1) && got_sigint == 0;){
        prio=1;
        //barrera de comienzo de ronda
        if(barrera(net)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }
        printf("**************************\n");
        printf("Comienza la ronda %d\n", j);
        found_solution=0;
        got_sigusr2 = 0;
        got_sigusr1=0;

        //lanzamiento de los trabajadores
        if(crear_hilos(nTrabajador,ti,trabajadores,block,net,lista_block) == -1){
            perror("Thread creation");
            exit(EXIT_FAILURE);
        }
        //recogida de los retornos y establecimiento del ganador, así como envío de la señal al resto de mineros
        if(recoger_hilos(nTrabajador,ti,trabajadores,block,net,lista_block,miner_id) == -1){   
            perror("Thread creation");
            exit(EXIT_FAILURE);
        }
        
        got_sigusr2=0;
        //barrera de sincronización
        if(barrera_activos(net)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }
        //votación si se trata de el minero ganador
        if(getpid()==net->last_winner) {
            
            if(votacion_winner(net, block, miner_id)==ERR){
                liberar_recursos(net, block, lista_block);
                exit(EXIT_FAILURE);
            }
            prio=2;
        }
        //votación si se trata de un minero perdedor
        else{
            if(votacion_loser(net, block, miner_id)==ERR){
                liberar_recursos(net, block, lista_block);
                exit(EXIT_FAILURE);
            }
            prio = 1;
        }
       
        //espera al ganador
        if(wait_winner(net,block)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }
        //En caso de que el bloque no sea válido se empieza de nuevo la misma ronda
        if(block->is_valid == 0){
            printf("El bloque no es valido\n\n");
            continue;
        }
        //si el bloque es válido se añade a la cadena y se envía al monitor en caso de que haya uno activo
        j++;
        printf("El bloque es valido\n\n");
        if(sendAdd(net,lista_block,block,queue,prio)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
            
        }
        
        if(esperarYActualizar(net,block, miner_id)==-1){
            liberar_recursos(net, block, lista_block);
            exit(EXIT_FAILURE);
        }
    }
    //liberación de recursos
    salirYLiberar(net,block,lista_block,miner_id,queue);
    
    exit(EXIT_FAILURE);
}
