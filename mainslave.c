#include <linux/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <stdbool.h>

#include "slave.h"



int main(int argc, char  *argv[])
{
    printf("connection to server: %s:\n",argv[1]);
    void * shm_addr;
    shm_addr = (void *) InitSlave(argv[1]);

    printf("local address: %p:\n",shm_addr);

    return 0;
}