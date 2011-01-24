/* header file for multicore_multicast programs */

#ifndef _MULTICORE_MULTICAST_
#define _MULTICORE_MULTICAST_

#define MAX_NB_CORES 128

#ifndef MESSAGE_MAX_SIZE
#define MESSAGE_MAX_SIZE 1048576
#endif

#define MSG_HEADER_SIZE (sizeof(int))

/* a message */
struct message {
   int len; // len of data
};

/* core_id is between 0 and nb_cores -1
 * nb_cores is between 0 and MAX_NB_CORES
 */
int core_id;
int nb_cores;

#ifdef IPC_ZERO_COPY
/* a message for IPC message queue */
struct ipc_message {
   long mtype;
   char mtext[MESSAGE_MAX_SIZE];
};
#endif

#endif
