#ifndef SLAVE_H
#define SLAVE_H

// typedef struct {
//     char access; //r = read , w = read/write, n = null
//     int lock; // 0 = not locked , 1 = locked
// }page_info;

#include "outils.h"


void *InitSlave(char *HostMaster);
void lock_read(void *adr, int s);
void unlock_read(void *adr, int s);
void lock_write(void *adr, int s);
void unlock_write(void *adr, int s);


#endif