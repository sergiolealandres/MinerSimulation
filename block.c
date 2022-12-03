#include "block.h"
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>

/***********************************************************/
/* Función: addBlock
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param lista: lista enlazada de bloques
/* @param block: bloque a añadir
/* @return int: OK en caso de que todo vaya bien y ERR en caso de error
/*
/* Descripción:
/* Función para añadir un bloque a la lista
/***********************************************************/
int addBlock(Block** lista,Block *block){
    Block* newblock;
    
    if(!lista || !block) return -1;
    
    newblock=block_init();
    if(!newblock)return -1;
   
    changeBlock(newblock, block);
     
    if((*lista)){
        newblock->next=*lista;
        (*lista)->prev = newblock;
    }
    
    newblock->prev=NULL;
    
    (*lista)=newblock;

    return 0;
}

/***********************************************************/
/* Función: blockFree
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param block: bloque a liberar
/*
/* Descripción:
/* Función para liberar un bloque
/***********************************************************/
void blockFree(Block * block){
    if(block){
        free(block);
    }
}

/***********************************************************/
/* Función: block_init
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param lista: lista enlazada de bloques
/* @param block: bloque a añadir
/*
/* @return b: bloque para el que se había reservado memoria
/*
/* Descripción:
/* Función para añadir un bloque a la lista
/***********************************************************/
Block * block_init(){
    Block *b;
    b=(Block*)calloc( 1,sizeof(Block));
    if(b==NULL)return NULL;
    return b;
}

/***********************************************************/
/* Función: changeBlock
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param newblock: nuevo bloque
/* @param block: antiguo bloque
/*
/* Descripción:
/* Función para añadir un bloque a la lista
/***********************************************************/
void changeBlock(Block* newblock, Block*block){
    int i;
    newblock->solution = block->solution;
    newblock->target = block->target;
    newblock->id=block->id;
    newblock->is_valid=block->is_valid;

    for(i=0;i<MAX_MINERS;i++){
        newblock->wallets[i] = block->wallets[i];
    }
}

/***********************************************************/
/* Función: print_blocks
/* @author: Pablo Soto Martín (pablo.soto@estudiante.uam.es)
/*          Sergio Leal Andrés (sergio.leala@estudiante.uam.es)                    
/*
/* @param plast_block: lista de bloques
/* @param num_wallets: numero de carteras de los mineros a imprimir
/*
/* @return b: bloque para el que se había reservado memoria
/*
/* Descripción:
/* Función para imprimir la cadena de bloques y las carteras de los mineros
/***********************************************************/
void print_blocks(Block *plast_block, int num_wallets) {
    Block *block = NULL;
    int i, j;
    char str[256];
    FILE *f;
    sprintf(str, "%d", getpid());
    f=fopen(str, "w");

    for(i = 0, block = plast_block; block != NULL; block = block->next, i++) {
        fprintf(f,"Block number: %d; Target: %ld;    Solution: %ld\n", block->id, block->target, block->solution);
        for(j = 0; j < num_wallets; j++) {
            fprintf(f,"%d: %d;         ", j, block->wallets[j]);
        }
        fprintf(f,"\n\n\n");
    }
    fprintf(f,"A total of %d blocks were printed\n", i);

    fclose(f);
}