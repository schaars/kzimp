/* using IPC message queue */

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "multicore_multicast.h"
#include "timer.h"

#define MIN(a, b) ((a < b) ? a : b)


/* a message for IPC message queue */
struct ipc_message {
   long mtype;
   char mtext[MESSAGE_MAX_SIZE];
};


// use core_addr[i] to communicate between core 0 and core i
static int core_addr[MAX_NB_CORES];


/* Initialize client message queue */
void initialize_client(void) {
   key_t key;

   // coordinator -> replica 
   if ((key = ftok("ipcmsgqueue.c", 'A' + core_id)) == -1) {
      perror("ftok");
      exit(1);
   }

   if ((core_addr[core_id] = msgget(key, 0600)) == -1) {
      perror("msgget");
      exit(1);
   }

   // replica -> coordinator
   if ((key = ftok("ipcmsgqueue.c", 'A')) == -1) {
      perror("ftok");
      exit(1);
   }

   if ((core_addr[0] = msgget(key, 0600)) == -1) {
      perror("msgget");
      exit(1);
   }

   //printf("Core %i key is %x\n", core_id, key);
}


/* Initialize server message queues */
void initialize_server(void) {
   key_t key;
   int i;

   for (i=0; i<nb_cores; i++) {
      if ((key = ftok("ipcmsgqueue.c", 'A' + i)) == -1) {
         perror("ftok");
         exit(1);
      }

      if ((core_addr[i] = msgget(key, 0600 | IPC_CREAT)) == -1) {
         perror("msgget");
         exit(1);
      }
   }
}


/* receive a message and return its size */
int mmcast_receive(void *buf, int s) {
   struct ipc_message ipc_msg;
   int recv_size;

   recv_size = 0;
   while (recv_size != s) {
      recv_size = msgrcv(core_addr[core_id], &ipc_msg, sizeof(ipc_msg.mtext), 0, 0);
      
      /*
      if (recv_size == -1) {
         perror("msgrcv");
         exit(1);
      }
      */
   }

   memcpy(buf, ipc_msg.mtext, recv_size);

   return recv_size;
}


/* send buffer buf of size s to core c */
void mmcast_send(void *buf, int s, int c) {
   struct ipc_message ipc_msg;
   int r;

   ipc_msg.mtype = 1;
   memcpy(ipc_msg.mtext, buf, s);

   r = msgsnd(core_addr[c], &ipc_msg, s, 0);

   /*
   if (r == -1) {
      switch (errno) {
         case E2BIG:
            printf("errno = E2BIG\n");
            break;
         case EACCES:
            printf("errno = EACCES\n");
            break;
         case EIDRM:
            printf("errno = EIDRM\n");
            break;
         case EINTR:
            printf("errno = EINTR\n");
            break;
         case EINVAL:
            printf("errno = EINVAL\n");
            break;
         case ENOMSG:
            printf("errno = ENOMSG\n");
            break;
         default:
            printf("errno = %i\n", errno);
            break;
      }
   }
   */
}

