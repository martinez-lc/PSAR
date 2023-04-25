#include <stdlib.h>
#include <stdio.h>
#include "outils.h"

void push(attente** liste, int id, char mode){
    attente* elem = malloc(sizeof(attente));
    if(!elem) exit(EXIT_FAILURE);
    elem->id = id;
    elem->mode = mode;
    attente* prev = (*liste)->prev;
    elem->prev = prev;
    prev->next = elem;
    elem->next = (*liste);
    (*liste)->prev = elem;
    *liste = elem;
}

int isEmpty(attente* liste){
    if(liste == NULL){
        return 1;
    }

    return 0;
}

attente* pop(attente** liste){
    attente* tmp;

    if(isEmpty(*liste)){
        return NULL;
    }

    tmp = (*liste);
    *liste = (*liste)->next;
    (*liste)->prev = tmp->prev;

    return tmp;
}