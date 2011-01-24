/* a client - get requets and send back ack */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "multicore_multicast.h"

/* these functions depend on the communication mean
 * we want to use and are linked at compile time
 */
extern void initialize_client(void);
extern int mmcast_receive(struct ipc_message *ipc_msg, int s);
extern void mmcast_send(struct ipc_message ipc_msg, int s, int c);


/* Return 1 if the request in buffer buf of size size is valid,
 * 0 otherwise
 */
int is_valid(struct message msg, int size) {
  /*
  printf("msize = %i, mtype = %i, mid = %i\n", size, msg.type, msg.id);
  printf("\ttype = %i, id = %i\n", MSG_REQ, core_id);
  */

  return ( (size > 0 && size <= MESSAGE_MAX_SIZE)
	   && (msg.type == MSG_REQ && msg.id == core_id) );
}


void send_ack(void) {
   struct ipc_message ipc_msg;
   struct message *msg;

   ipc_msg.mtype = 1;
   msg = (struct message*)(ipc_msg.mtext);
   msg->type = MSG_ACK;
   msg->id = core_id;
   msg->len = 0;
   mmcast_send(ipc_msg, MSG_HEADER_SIZE, 0);
}


int main(int argc, char **argv) {
  int size;
  unsigned long timestamp, ack_laps;
  int req_size;
  struct ipc_message ipc_msg;

  core_id = -1;
  ack_laps = 1;
  req_size = 0;

  // process command line options
  int opt;
  while ((opt = getopt(argc, argv, "c:s:a:")) != EOF) {
    switch (opt) {
    case 'c':
      core_id = atoi(optarg);
      break;

    case 's':
      req_size = atoi(optarg);
      break;

    case 'a':
      ack_laps = atoi(optarg);
      break;

    default:
      fprintf(stderr, "Usage: %s -c core_id -s req_size -a ack_laps\n", argv[0]);
      exit(-1);
    }
  }

  if (core_id < 1 || core_id >= MAX_NB_CORES) {
    fprintf(stderr, "Usage: %s -c core_id -s req_size -a ack_laps\n", argv[0]);
    fprintf(stderr, "\tcore_id shall be greater than 0 and less than %i\n", MAX_NB_CORES);
    exit(-1);
  }

  if (req_size + MSG_HEADER_SIZE > MESSAGE_MAX_SIZE) {
    fprintf(stderr, "req_size is too big: %i < %i\n", req_size, MESSAGE_MAX_SIZE - MSG_HEADER_SIZE);
    exit(-1);
  }

  initialize_client();

  printf("Core %i is ready with requests of size %i and acks every %lu requests\n", core_id, req_size, ack_laps);

  timestamp = 0;
  while (1) {
     // receive request
     size = mmcast_receive(&ipc_msg, MSG_HEADER_SIZE + req_size);

     // if the request is valid then send an ack
     if (!is_valid(*((struct message*)(ipc_msg.mtext)), size))
        ; //fprintf(stderr, "Core %i received a non-valid request.\n", core_id);
     else {
        //printf("Core %i received a request.\n", core_id);
        timestamp++;
     }

     if (ack_laps > 0 && timestamp == ack_laps) {
        timestamp = 0;
        //printf("core %i is sending an ack\n", core_id);
        send_ack();
     }
  }

  return 0;
}

