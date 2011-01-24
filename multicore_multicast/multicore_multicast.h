/* header file for multicore_multicast programs */

#ifndef _MULTICORE_MULTICAST_
#define _MULTICORE_MULTICAST_

#define MAX_NB_CORES 128
#ifndef MESSAGE_MAX_SIZE
#define MESSAGE_MAX_SIZE 1048576
//#define MESSAGE_MAX_SIZE 16384
#endif
#define MSG_REQ 0
#define MSG_ACK 1

#define MSG_HEADER_SIZE (3*sizeof(int))

/* timeout in ms */
#define TIMEOUT 100

/* a message */
struct message {
  int type;
  int id; /* type == MSG_REQ => id = dest id
	   * type == MSG_ACK => id = src id */
  int len; // len of data
};

/* a message for IPC message queue */
struct ipc_message {
   long mtype;
   char mtext[MESSAGE_MAX_SIZE];
};

/* core_id is between 0 and nb_cores -1
 * nb_cores is between 0 and MAX_NB_CORES
 */
int core_id;
int nb_cores;

#endif
