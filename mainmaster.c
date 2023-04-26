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

//#include "master.h"
//#include "master.c"

#define INPUT_PAGS (10)

int main(int argc, char  *argv[])
{
    int num_pages;
    char command[INPUT_PAGS];
    printf("numero de paginas:");

    if (!fgets(command, 10, stdin)){

		perror("Enumero de paginas:");
	}
    
    void * shm_addr;
	num_pages =  atoi(command);
    shm_addr = (void *) InitMaster(num_pages);
    
    LoopMaster();
    return 0;
}