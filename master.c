#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <pthread.h>
#include "master.h"


page_info* page_table;
int socket_fd;
int slaves[MAX_SLAVE];                      //contient les sockets de connexion avec tous les esclaves
pthread_t thread_connexions[MAX_SLAVE];
struct sockaddr_in addr_slaves[MAX_SLAVE];  //contient les sockaddr_in de connexion de tous les escles dans le même ordre 
int cpt = 0;    //pour savoir combien il y a de slaves

int size_shm;   //taille de la mémoire partagée en octets
int nb_pages;   //taille de la mémoire partagée en nombre de pages
void* shm_addr;
unsigned page_shift = 12;

//size = nb de pages
void *InitMaster(int size){
    nb_pages = size;
    size_shm = size * PAGE_SIZE;
    shm_addr = mmap(NULL, size_shm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shm_addr == MAP_FAILED){
        perror("mmap");
        exit(1);
    }

    if(mprotect(shm_addr, size_shm, PROT_READ | PROT_WRITE) == -1){
        perror("mprotect");
        munmap(shm_addr, size);
        exit(1);
    }

    // int nb_pages = size / PAGE_SIZE;
    // if(size % PAGE_SIZE != 0){
    //     nb_pages++;
    // }

    //Allocation d'une table des pages
    page_table = (page_info*) malloc(nb_pages * sizeof(page_info));
    for(int i =0; i < nb_pages; i++){
        page_table[i].owner = -1; //par défaut propriétaire = -1 ce qui signifie que c'est le maître qui possède la page
        //page_table[i].copies = NULL;
        page_table[i].lecture = 0;
        page_table[i].ecriture = 0;
        page_table[i].dernier = 0;
    }

    return shm_addr;
}

void LoopMaster(){
    //Création de la socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        perror("socket");
        exit(1);
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET; 
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(SERVER_PORT);

    if (bind(socket_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        perror("bind");
        close(socket_fd);
        exit(1);
    }

    if(listen(socket_fd, 50) < 0){
        perror("listen");
        close(socket_fd);
        exit(1);
    }


    struct sockaddr_in exp;
    socklen_t len = sizeof(exp);


    for(int i = 0; i < MAX_SLAVE; i++){

        int scom = accept(socket_fd, (struct sockaddr *) &exp, &len);
        if (scom < 0) { 
            perror("accept");
            exit(1);
        }
        
        if(cpt < MAX_SLAVE){
            slaves[cpt] = scom;
            addr_slaves[cpt] = exp;
            cpt++;

            //envoie de la taille de la mémoire partagée
            if(write(scom, &nb_pages, sizeof(nb_pages)) == -1){
                perror("write size");
                exit(1);
            }

            pthread_create(&thread_connexions[i], NULL, job_thread, scom);
            i++;
        }

    }

    for (int i = 0; i < MAX_SLAVE; i++){
      pthread_join (thread_connexions[i], NULL);
    }

}


void job_thread(int scom){
    int msg[2];
    int type;
    int numpage;
    while(1){
        //on attend une requête sur la socket
        read(scom, msg, sizeof(msg));
        type = msg[0];
        numpage = msg[1];
        if(type == LOCK_READ){
            lock_read(scom, numpage);
        }
        else if (type == LOCK_WRITE){
            lock_write(scom, numpage);
        }
        else if(type == GET_PAGE){

        }
    }
}

void accord_read(int slave, int numpage){
    //le maître envoie un message pour que l'esclave sache que le verrou lui a été accordé
    int ack = LOCK_OK;    //peut importe la valeur juste si recoit une valeur cela veut dire que le verrou lui a été donné
    page_table[numpage].lecture++;
    page_table[numpage].copies[page_table[numpage].dernier] = slave;
    page_table[numpage].dernier++;
    write(slave, &ack, sizeof(ack));
}

//permet d'accorder l'accés à une page en lecture à un esclave
void lock_read(int slave, int numpage){
    //si on a au moins une écriture en cours ou en attente on ajoute la demande dans la liste d'attente
    if(page_table[numpage].ecriture > 0){
        push(&page_table[numpage].liste_attente, slave, 'r');
    }

    //aucune ecriture en cours ou en attente (il y a seulement 0 ou plusieurs lecteurs)
    else{
        accord_read(slave,numpage);
    }
}

void accord_write(int slave, int numpage){
    //invalide toutes les copies

    int dernier = page_table[numpage].dernier;

    for(int i = 0; i < dernier; i++){
        write(page_table[numpage].copies[i], INVALIDATION, sizeof(int));
    }

    page_table[numpage].dernier = 0;
    page_table[numpage].owner = slave;

    //le maître envoie un message pour que l'esclave sache que le verrou lui a été accordé
    int ack = LOCK_OK;    //peut importe la valeur juste si recoit une valeur cela veut dire que le verrou lui a été donné
    write(slave, &ack, sizeof(ack));    
}


//permet d'accorder l'accés à une page en ecriture à un esclave
void lock_write(int slave, int numpage){

    page_table[numpage].ecriture++;
    //si on a au moins une écriture en cours ou en attente et/ou des lectures en cours
    //on ajoute la demande dans la liste d'attente
    if(page_table[numpage].ecriture > 0 || page_table[numpage].lecture > 0){
        push(&page_table[numpage].liste_attente, slave, 'w');
    }

    else{
        accord_write(slave, numpage);
    }
}

void unlock_read(int slave, int numpage){
    page_table[numpage].lecture--;
    
    if((page_table[numpage].lecture == 0) && (page_table[numpage].ecriture > 0)){
        //il y a nécessairement un écrivain qui attend en début de file d'attente (si ça avait été un lecteur il serait pas dans la file)
        attente* tete = pop(&page_table[numpage].liste_attente);
        
        accord_write(tete->id, numpage);

        free(tete);
    }
}

void unlock_write(int slave, int numpage){
    page_table[numpage].ecriture--;
    //si la liste d'attente n'est pas vide
    if(!isEmpty(page_table[numpage].liste_attente)){
        //on récupère la tête
        attente* tete = pop(&page_table[numpage].liste_attente);

        //si c'est un ecrivain on lui accorde le verrou exclusivement

        if(tete->mode == 'w'){
            accord_write(tete->id, numpage);
        }
        
        else{
            //si c'est un lecteur en tete de liste on doit laisser tous les lecteurs à la suite avoir accès au verrou jusqu'au prochain ecrivain ou que la liste soit vide
            while((tete != NULL) && (tete->mode == 'r')){
                accord_read(tete->id, numpage);
                tete = pop(&page_table[numpage].liste_attente);
            }

        }
    }

}