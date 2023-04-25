#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <inttypes.h>
#include <sys/types.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <poll.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#include "slave.h"

#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#define SERVER_PORT 4242

int socket_fd;
int userfaultfd;

void* shm_addr;
struct sockaddr_in sin;

long uffd;          /* userfaultfd file descriptor */
void *addr;         /* Start of region handled by userfaultfd */
uint64_t len;       /* Length of region handled by userfaultfd */
pthread_t thread;      /* ID of thread that handles page faults */
struct uffdio_api uffdio_api;
struct uffdio_register uffdio_register;

//page_info* page_table; //table qui maintient des infos sur les pages
int nb_pages;

static void *fault_handler_thread(){
    static struct uffd_msg msg;
    struct uffdio_copy uffdio_copy;


    while(1){
        struct pollfd pollfd;
        int nready;
        int nread;
        pollfd.fd = uffd;
        pollfd.events = POLLIN;

        nready = poll(&pollfd, 1, -1);
        if (nready == -1){
            perror("poll");
            exit(1);
        }

        nread = read(uffd, &msg, sizeof(msg));
        if (nread == 0) {
            printf("EOF on userfaultfd!\n");
            exit(EXIT_FAILURE);
        }

        if(nread == -1){
            perror("read");
            exit(1);
        }

        if (msg.event != UFFD_EVENT_PAGEFAULT) {
            fprintf(stderr, "Unexpected event on userfaultfd\n");
            exit(EXIT_FAILURE);
        }

        long long addr = msg.arg.pagefault.address;

        if (connect(socket_fd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
            perror("connect");
            exit(1);
        }

        int numpage = (addr - (long long) shm_addr) % PAGE_SIZE;

        int message[2];
        message[0] = GET_PAGE;
        message[1] = numpage;
        char page[PAGE_SIZE];
        if(write(socket_fd, message, sizeof(message)) == -1){
            perror("write");
            exit(1);
        };
        
        if(read(socket_fd, &page, sizeof(page)) == -1){
            perror("write");
            exit(1);
        };

        uffdio_copy.src = (unsigned long) page;
        uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address;
        uffdio_copy.len = PAGE_SIZE;
        uffdio_copy.mode = 0;
        uffdio_copy.copy = 0;
        if(ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1){
            perror("ioctl UFFDIO_COPY");
            exit(1);
        }

    }
    

}


void *InitSlave(char *HostMaster){

    struct hostent *hostinfo = gethostbyname(HostMaster);

    if (hostinfo == NULL){ /* l'hôte n'existe pas */
        fprintf (stderr, "Unknown host %s.\n", HostMaster);
        exit(EXIT_FAILURE);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1){
        perror("socket");
        exit(1);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET; 
    sin.sin_addr = *(struct in_addr*)hostinfo->h_addr_list;
    sin.sin_port = htons(SERVER_PORT); 


    if (connect(socket_fd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
        perror("connect");
        exit(1);
    }

    if(read(socket_fd, &nb_pages, sizeof(nb_pages)) == -1){
        perror("read len");
        exit(1);
    }

    len = nb_pages * PAGE_SIZE;

    //initialisation userfaultfd

    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1){
        perror("syscall userfaultfd");
        exit(1);
    }

    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1){
        perror("ioctl UFFDIO_API");
        exit(1);
    }


    //initialisation de la zone de mémoire partagée
    shm_addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(shm_addr == MAP_FAILED){
        perror("mmap");
        exit(1);
    }

    //on retire les droits pour les esclaves sur la zone de mémoire partagée initialement
    if(mprotect(shm_addr, len, PROT_NONE) == -1){
        perror("mprotect");
        munmap(shm_addr, len);
        exit(1);
    }

    //relie la zone de mémoire partagée avec le userfaultfd pour qu'il puisse gérer les défauts de pages
    uffdio_register.range.start = (unsigned long) shm_addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1){
        perror("ioctl UFFDIO_REGISTER");
        exit(1);
    }

    //initialisation de la table des pages local à un esclave

    // nb_pages = len / PAGE_SIZE;
    // if(len % PAGE_SIZE != 0){
    //     nb_pages++;
    // }

    // page_info* page_table = (page_info*) malloc(nb_pages * sizeof(page_info));
    // for(int i =0; i < nb_pages; i++){
    //     page_table[i].access = 'n';
    //     page_table[i].lock = 0;
    // }

    return shm_addr;
}



/* Demande un verrou pour l'accès en lecture pour la zone du segment de taille s située à
l'adresse adr. Si la ou les pages correspondantes sont verrouillées en écriture par un autre
site cette primitive est bloquante.*/
void lock_read(void *adr, int s){

    int page_deb = (int)(adr - shm_addr) % PAGE_SIZE;
    int page_fin = (int)((adr+s) - shm_addr) % PAGE_SIZE;




}

/* Libère l'accès en lecture pour la zone du segment de taille s située à l'adresse adr. */
void unlock_read(void *adr, int s);

/* Demande un verrou pour l'accès en écriture pour la zone du segment de taille s située à
l'adresse adr. Si la ou les pages correspondantes sont verrouillées en lecture ou en écriture
par un autre site cette primitive est bloquante. */
void lock_write(void *adr, int s);


/* Libère l'accès en écriture pour la zone du segment de taille s située à l'adresse adr. */
void unlock_write(void *adr, int s);