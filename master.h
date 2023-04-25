#ifndef MASTER_H
#define MASTER_H

#define PAGE_SIZE       sysconf(_SC_PAGE_SIZE)
#define SERVER_PORT     4242
#define MAX_SLAVE       5

#include "outils.h"


typedef struct {
    int owner;                  //identitfiant (socket) du propriétaire de la derniere version
    int copies[MAX_SLAVE];      //sites possédant une copie de la page
    int dernier;                //indice du dernier dans le tableau copies
    int lecture;                //= nb d'accès en lecture en cours
    int ecriture;               //= nb d'écriture en cours et/ou en attente
    attente* liste_attente;     //liste d'attente des esclaves voulant accéder à la page

} page_info;

// typedef struct sockets {
//     int scom;
//     struct sockets* next;
// } sockets;

void* InitMaster(int size);
void LoopMaster();
void lock_read(int slave, int numpage);
void lock_write(int slave, int numpage);
void unlock_read(int slave, int numpage);
void unlock_write(int slave, int num_page);

#endif

