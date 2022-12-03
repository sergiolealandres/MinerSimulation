
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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
#include <unistd.h>
#include "miner.h"

#define QUEUE_NAME "/mq_queue"
#define SHM_NAME_NET "/netdata"
#define ACTIVE_BLOCKS 10
#define SECS 5
static  volatile  sig_atomic_t got_alrm = 0;
static  volatile  sig_atomic_t got_sigint = 0;

/***********************************************************/
/* Función: manejadorSigAlarm            
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/*
/* Descripción:
/* Función utilizada como manejador para SIGALRM
/* y que internamente únicamente pone a 1 una variable volátil estática que se utiliza para indicar que el 
/* monitor ha recibido la señal
/***********************************************************/
void manejadorAlarm(){
    got_alrm=1;
}

/***********************************************************/
/* Función: manejadorSigINT          
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/*
/* Descripción:
/* Función utilizada como manejador para SIGINT
/* y que internamente únicamente pone a 1 una variable volátil estática que se utiliza para indicar que el 
/* monitor ha recibido la señal
/***********************************************************/
void manejadorSigInt(){
    got_sigint=1;
}

/***********************************************************/
/* Función: addToLog         
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* Descripción:
/* Función que usa el proceso hijo para escribir en log.txt cada 5 segundos
/***********************************************************/
int addToLog(Block **list, int file){
    Block *aux=*list;
    int nbytes;
    dprintf(file, "\nNueva cadena:\n\n");
    while(aux!=NULL){
        dprintf(file, "*************************\n");
        dprintf(file, "id: %d\n", aux->id);
        dprintf(file, "solution: %ld\n", aux->solution);
        dprintf(file, "target: %ld\n", aux->target);

        aux=aux->next;

    }
}

/***********************************************************/
/* Función: liberar_recursos_hijo     
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param lista_block: lista enlazada de bloques del hijo
/*
/* Descripción:
/* Función usada por el hijo para liberar la lista enlazada de bloques
/***********************************************************/
void liberar_recursos_hijo(Block **lista_block){

    Block* current, *next;

    if(lista_block){
        if(*lista_block){
            current = *lista_block;
            while(current){
                next = current->next;
                blockFree(current);
                current = next;
            }
        }
        free(lista_block);
    }
}

/***********************************************************/
/* Función: funcion_hijo        
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param fd: descriptor de la tuberia de comunicación entre el proceso padre y el hijo
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función del proceso hijo, consta de un bucle principal que se ejecuta hasta que no reciba sigint.
/* En este bucle lee de la tubería (controlando que no reciba una señal a la mitad y se pierda ese bloque),
/* cada vezz que reciba sigalrm que es cada 5 segundos, abre el fichero y imprime su cadena de bloques completa ahí,
/* sobrescribiendo por supuesto todo lo que pudiera contener el fichero. 
/***********************************************************/
int funcion_hijo(int fd[2]){

    struct sigaction act;
    Block **lista_block;
    Block *aux=block_init();
    int nbytes=1, file;

    //reserva de memori para la lista enlazada de bloques
    if(!(lista_block = malloc(sizeof(Block*)))){
        fprintf(stderr, "Error de malloc\n");
        return ERR;
    }
    (*lista_block) = NULL;
    sigemptyset(&(act.sa_mask));
    act.sa_flags=0;
    act.sa_handler=manejadorAlarm;

    //se establece el manejador de sigalrm
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("sigaction");
        return ERR;
    }

    alarm(SECS);
    close(fd[1]);
    //bucle principal
    while(got_sigint==0){
        //lectura de la tuberia
        while((nbytes=(read(fd[0], aux, sizeof(Block)))==-1)&&errno==EINTR){

            if(got_alrm){
                //si se recibe sigalrm  mientras se estaba esperando en la lectura de la tuberia se actualiza el log.txt
                file=open("log.txt", O_CREAT| O_TRUNC| O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                if(file==-1){
                    perror("open");
                    liberar_recursos_hijo(lista_block);
                    return ERR;
                }
                addToLog(lista_block, file);
                alarm(SECS);
                close(file);
                got_alrm=0;

            }
            if(got_sigint){
                //si se recibe sigint se finaliza correctamente
                blockFree(aux);
                close(fd[0]);
                liberar_recursos_hijo(lista_block);
    
                return ERR;

            }
        }
        
        if(nbytes==-1){
            liberar_recursos_hijo(lista_block);
            return ERR;
        }
        else if(nbytes>=0){
            // se añade el bloque a la cadena
            if(addBlock(lista_block, aux)==-1){
                liberar_recursos_hijo(lista_block);
                return ERR;
            }
          
            if(got_alrm){
               //si se recibe sigalrm se actualiza el log.txt
                file=open("log.txt", O_CREAT| O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                if(file==-1){
                    perror("open");
                    liberar_recursos_hijo(lista_block);
                    return ERR;
                }
                addToLog(lista_block, file);
                alarm(SECS);
                close(file);
                got_alrm=0;
            }
        }
    }
    return OK;
}

/***********************************************************/
/* Función: openAndJoin    
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param net: puntero al segmento de memoria compartido (red de mineros)
/*
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función para que el monitor se de alta en la red, también se usa por si el minero es el primer
/* proceso en lanzarse que inicialice todos los campos correspondientes.
/***********************************************************/
int openAndJoin(NetData **net){
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
        (*net)->last_miner = -1;
        (*net)->total_miners = 0;
        (*net)->active_miners=0;
        (*net)->count_activos=0;
        (*net)->monitor_pid=-1;
    }
    while(sem_wait(&((*net)->mutex))){
            if(errno != EINTR){
            perror("sem_wait");
            return ERR;
            }
        }
    if((*net)->monitor_pid!=-1){
        kill((*net)->monitor_pid, SIGINT);
    }
    ((*net)->monitor_pid)=getpid();
    sem_post(&((*net)->mutex));
    
}

/***********************************************************/
/* Función: liberarRecursos
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param net: puntero al segmento de memoria compartido (red de mineros)
/* @param queue: cola de mensajes
/*
/* Descripción:
/* Función para liberar los recursos del monitor
/***********************************************************/
void liberarRecursos(NetData *net, mqd_t queue){

    mq_close(queue);  
    sem_wait(&net->mutex);
    if(net->monitor_pid==getpid()){
        net->monitor_pid=-1;
    }
    sem_post(&net->mutex);
    munmap(net, sizeof(NetData));
}

/***********************************************************/
/* Función: main           
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* Descripción:
/* Función principal del monitor, se va indicando en cada parte lo que se va haciendo.
/***********************************************************/
int main(int argc, char *argv[]) {
    
    pid_t pid;
    mqd_t  queue;
    Block  auxblock, *auxcopy;
    struct  mq_attr  attributes = {.mq_flags = 0,.mq_maxmsg = 10,.mq_curmsgs = 0,.mq_msgsize = sizeof(Block)};
    Block * lastBlocks[ACTIVE_BLOCKS];
    int flag=0, i, j, id, found, lastBlock=0, fd[2], blockPipe, fd_shm, ret;
    NetData *net;
    struct sigaction act;
    //unirse a la red de mineros
    if(openAndJoin(&net)==ERR){
        exit(EXIT_FAILURE);
    }
   
   //creación de la tuberia
    blockPipe = pipe(fd);
	if (blockPipe == -1)
	{
		perror("pipe");
		exit(EXIT_FAILURE);
	} 

    //establecimiento del manejador para sigint
    sigemptyset(&(act.sa_mask));
    act.sa_flags=0;
    act.sa_handler=manejadorSigInt;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        return ERR;
    }

    //creación del proceso hijo
    pid=fork();
    if(pid<0){
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if(pid==0){
        //función del hijo
       
        ret=funcion_hijo(fd);
        if(ret==ERR){
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    else{
        //FUNCION DEL PADRE

        //se abre la cola de mensajes
        queue=mq_open(QUEUE_NAME,O_CREAT| O_RDONLY ,S_IRUSR | S_IWUSR , &attributes);
        
        if (queue  == (mqd_t) -1) {
            perror("Error  opening  queue\n");
            liberarRecursos(net, queue);
            exit(EXIT_FAILURE);
        }

        //reserva memoria para la cadena de bloques
        for(i=0;i<ACTIVE_BLOCKS;i++){
            lastBlocks[i]=block_init();
            
            if(!lastBlocks[i]){
                for(j=0;j<i;j++){
                    blockFree(lastBlocks[j]);
                }
                liberarRecursos(net, queue);
                exit(EXIT_FAILURE);
            }
        }

        //bucle principal de ejecución mientras no reciba sigint
        while(got_sigint==0){
            i=0;
            found=0;
            //recepción de mensajes por parte de los mineros
            if(mq_receive(queue, (char*)&auxblock, sizeof(Block), NULL)==-1){
                if(errno==EINTR){
                    if(got_sigint==1){
                        break;
                    }
                }
            }

            
            id=auxblock.id;
            //busqueda de el bloque recibido entre los bloques de la cadena
            while(lastBlocks[i]!=NULL&&found==0){
                
                if(lastBlocks[i]->id!=id) {
                    i++;
                    continue;
                }
                
                found=1;
                if(lastBlocks[i]->solution == auxblock.solution  &&  lastBlocks[i]->target == auxblock.target){
                    //si se encuentra y los datos coinciden se imprime que el bloque ha sido verificado
                    fprintf(stdout, "Verified block %d with solution %ld for target %ld\n\n", id, auxblock.solution, auxblock.target);
                    continue;
                }
                
                fprintf(stdout, "Error in block %d with solution %ld for target %ld\n\n", id, auxblock.solution, auxblock.target);
                
                i++;
            }
            
            if(found!=1){
                //en caso de no ser encontrado se mandará al hijo por la tuberia para que lo añada al log.txt
                changeBlock(lastBlocks[lastBlock], &auxblock);
               
                lastBlock=(lastBlock+1)%ACTIVE_BLOCKS;
                close(fd[0]);

                auxcopy=block_init();
                changeBlock(auxcopy, &auxblock);
                //escritura en la tuberia
                if(write(fd[1], auxcopy, sizeof(Block))==-1){
                    perror("write pipe");
                    liberarRecursos(net, queue);
                    exit(EXIT_FAILURE);
                }
                blockFree(auxcopy);
               
            }
        }
        kill(pid, SIGINT);

        //liberacion de memoria
        for(i=0;i<10;i++){
            blockFree(lastBlocks[i]);
        }

        close(fd[1]);

        //espera al hijo
        wait(NULL);
        liberarRecursos(net, queue);
    }

    exit(EXIT_SUCCESS);
}

