/* a server - multicast requests, wait for acks and compute stats */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "list.h"
#include "timer.h"
#include "multicore_multicast.h"

#define OUTFILE "/tmp/res.txt"


/* number of times we have retransmitted a message */
unsigned long nb_retransmit;

/* these functions depend on the communication mean
 * we want to use and are linked at compile time
 */
extern void initialize_server(void);
extern int mmcast_receive(struct ipc_message *ipc_msg, int s);
extern int mmcast_receive_timeout(struct ipc_message *ipc_msg, int s);
extern void mmcast_send(struct ipc_message ipc_msg, int s, int c);

/* Return 1 if the request in buffer buf of size size is valid,
 * 0 otherwise
 */
int is_valid(struct message msg, int size) {
  return ( (size > 0 && size <= MESSAGE_MAX_SIZE)
	   && (msg.type == MSG_ACK && msg.id > 0
	       && msg.id < MAX_NB_CORES) );
}


/* function to send ONE request to ALL cores except me */
void send_request(int req_size) {
   struct ipc_message ipc_msg;
   struct message *msg;
   int msize;

   ipc_msg.mtype = 1;
   msize = MSG_HEADER_SIZE + req_size;
   msg = (struct message*)(ipc_msg.mtext);
   msg->type = MSG_REQ;
   msg->len = req_size;

   int i;
   for (i=1; i<nb_cores; i++) {
      msg->id = i;
      mmcast_send(ipc_msg, msize, i);
   }
}


/* function to send ONE request to the one that needs it after a timeout
 * unicast send
 */
void send_request_timeout(int req_size, int gottenack, unsigned long ack_laps) {
   struct ipc_message ipc_msg;
   struct message *msg;
   int i, msize;
   long j;

   //printf("In send_request_timeout\n");

   ipc_msg.mtype = 1;
   msize = MSG_HEADER_SIZE + req_size;
   msg = (struct message*)(ipc_msg.mtext);
   msg->type = MSG_REQ;
   msg->len = req_size;

   for (i=1; i<nb_cores; i++) {
      if (!(gottenack & (1 << i))) {
         for (j=0; j<ack_laps; j++) {
            msg->id = i;
            mmcast_send(ipc_msg, msize, i);
         }
      }
   }
}


/* wait for acks, retransmit message if needed */
void wait_for_acks(unsigned long ack_laps, int req_size) {
   int i, size, gottenack;
   struct message *m;
   struct ipc_message ipc_msg;

   i=0;
   gottenack = 0;
   while (i < nb_cores-1) {
      size = mmcast_receive_timeout(&ipc_msg, MSG_HEADER_SIZE);
      //size = mmcast_receive(&ipc_msg, MSG_HEADER_SIZE);

      m = (struct message*)(ipc_msg.mtext);

      // size = -2 means timeout, so retransmit
      if (size == -2) {
         send_request_timeout(req_size, gottenack, ack_laps);
         nb_retransmit++;
      } else if (is_valid(*m, size)) {
         // check if I received it already
         if ( !(gottenack & (1 << m->id)) ) {
            //printf("Ack from %i received\n", m->id);
            gottenack |= (1 << m->id);
            i++;
         }
      }
   }
}

void print_stats(unsigned long total_nb_req, struct node *L) {
   float avg_thr, stddev;
   FILE *F;

   avg_thr = list_compute_avg(L);
   stddev = list_compute_stddev(L, avg_thr);

   if ((F = fopen(OUTFILE, "a")) != NULL) {
      fprintf(F, "%lu\t%f\t%f\t%lu\n", total_nb_req, avg_thr, stddev, nb_retransmit);
      fclose(F);
   }
   else
      printf("%lu\t%f\t%f\t%lu\n", total_nb_req, avg_thr, stddev, nb_retransmit);
}


int main(int argc, char **argv) {
   int i, req_size;
   unsigned long nb_req, total_nb_req, ack_laps;
   unsigned long timestamp; // max val = 4 294 967 295
   struct node *L;
   struct timer T;

   nb_retransmit = 0;
   ack_laps = 1;
   nb_req = 1000;
   total_nb_req = 0;
   core_id = 0;
   nb_cores = -1;
   req_size = 0;
   L = NULL;
   timer_init(&T);

   // get command line options
   int opt;
   while ((opt = getopt(argc, argv, "n:r:s:a:")) != EOF) {
      switch (opt) {
         case 'n':
            nb_cores = atoi(optarg);
            break;

         case 'r':
            nb_req = atoi(optarg);
            break;

         case 's':
            req_size = atoi(optarg);
            break;

         case 'a':
            ack_laps = atoi(optarg);
            break;

         default:
            fprintf(stderr, "Usage: %s -n nb_cores -r nb_req -s req_size -a ack_laps\n", argv[0]);
            exit(-1);
      }
   }

   if (nb_cores < 1 || nb_cores > MAX_NB_CORES) {
      fprintf(stderr, "Usage: %s -n nb_cores -r nb_req -s req_size -a ack_laps\n", argv[0]);
      fprintf(stderr, "\tnb_cores must be greater than 0 and less than %i\n", MAX_NB_CORES);
      exit(-1);
   }

   if (req_size + MSG_HEADER_SIZE > MESSAGE_MAX_SIZE) {
      fprintf(stderr, "req_size is too big: %i < %i\n", req_size, MESSAGE_MAX_SIZE - MSG_HEADER_SIZE);
      exit(-1);
   }

   initialize_server();

   sleep(2);

   printf("Core 0 is ready with %i cores, %lu requests per step, requests of size %i and acks every %lu requests\n", nb_cores, nb_req, req_size, ack_laps);

   timestamp = 0;
   while (1) {
      // timer on
      timer_on(&T);

      for (i=0; i<nb_req; i++) {
         send_request(req_size);
         timestamp++;      

         if (ack_laps > 0 && timestamp == ack_laps) {
            //printf("timestamp=%lu, ack_laps=%lu, waiting for ack\n", timestamp, ack_laps);
            timestamp = 0;
            wait_for_acks(ack_laps, req_size);
         }
      }

      // timer off
      timer_off(&T);

      // output "nbreqs avgthr stddev"
      total_nb_req += nb_req;
      L = list_add(L, (float)nb_req/timer_elapsed(T));
      timer_reset(&T);
      print_stats(total_nb_req, L);
   }

   /* need to call ipcrm in order to destroy message queues,
    * as these programs never finish
    */

   return 0;
}

