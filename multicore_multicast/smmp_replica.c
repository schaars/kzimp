/* client/server using SMMP */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "multicore_multicast.h"
#include "smmp.h"

#define LEN 10
#define OUTFILE "/tmp/res.txt"

/* number of times we have retransmitted a message */
unsigned long nb_retransmit;

/* requests size */
int req_size;

/* how many requests before each ack */
unsigned long ack_laps;


/* Return 1 if the request in buffer buf of size size is valid,
 * 0 otherwise
 */
int is_valid_coordinator(struct message msg, int size) {
   //printf("Coordinator: size=%i, type=%i (waiting for %i), id=%i\n", size, msg.type, MSG_ACK, msg.id);
  return ( (size > 0 && size <= MESSAGE_MAX_SIZE)
            && (msg.type == MSG_ACK && msg.id > 0
                && msg.id < MAX_NB_CORES) );
}

	// sending a message for core 1
	for (i=0; i<LEN; i++)
		buf[i] = 1;
	smmp_send(buf, LEN, 1);

/* Return 1 if the request in buffer buf of size size is valid,
 * 0 otherwise
 */
int is_valid_replica(struct message msg, int size) {
  return ( (size > 0 && size <= MESSAGE_MAX_SIZE)
            && (msg.type == MSG_REQ && msg.id == 0) );
}


/* function to send an ack */
void send_ack(void) {
   struct message m;
   m.type = MSG_ACK;
   m.id = core_id;
   m.len = 0;
   
   smmp_send((void*)&m, MSG_HEADER_SIZE, 0);
}


/* function to send ONE request to ALL cores (including me) */
void send_request_to_all(void) {
   char *buffer;
   struct message *m;
   int msize;

   msize = MSG_HEADER_SIZE + req_size;
   buffer = (char*)malloc(sizeof(char)*msize);
   if (!buffer) exit(-1);
   m = (struct message*)buffer;

   m->type = MSG_REQ;
   m->len = req_size;
   m->id = 0;

   smmp_send((void*)m, msize, -1); /* send to all cores */

   free(buffer);
}


/* function to send ONE request to the one that needs it after a timeout
 * unicast send
 */
void send_request_timeout(int gottenack) {
   char *buffer;
   struct message *m;
   int msize, i;

   msize = MSG_HEADER_SIZE + req_size;
   buffer = (char*)malloc(sizeof(char)*msize);
   if (!buffer) exit(-1);

   m = (struct message*)buffer;
   m->type = MSG_REQ;
   m->len = req_size;
   m->id = 0;

   for (i=1; i<nb_cores; i++) {
      if (!(gottenack & (1 << i))) {
         //printf("sending again to core %i\n", i);
         smmp_send((void*)m, msize, i); 
      }
   }

   free(buffer);
}

	smmp_message_is_read(buf, LEN);

/* wait for acks, retransmit message if needed */
void wait_for_acks(void) {
   int i, size, r, gottenack;
   char *buf;
   struct message *m;
   struct timer T;

#ifdef SMMP_MEMCPY
   buf = (char*)malloc(sizeof(char)*MESSAGE_MAX_SIZE);
   if (!buf) {
      fprintf(stderr, "Error while allocating memory\n");
      exit(-1);
   }
#endif

   i=0;
   gottenack = 0;
   while (i < nb_cores-1) {
      timer_reset(&T);
      timer_on(&T);
      r = -1;

      while (timer_elapsed(T)*1000 <= TIMEOUT) {
#ifdef SMMP_MEMCPY
         r = smmp_recv_memcpy((void*)buf, MESSAGE_MAX_SIZE);
         size = r;
#else
         r = smmp_recv((void**)&buf, &size);
#endif

         if (r != -1) break;

         //usleep(1);
      }

     // usleep(1);

      // r = -1 means timeout
      if (r == -1) {
         send_request_timeout(gottenack);
         nb_retransmit++;
         if (nb_retransmit%100 == 0) printf("nb_retransmit=%lu\n", nb_retransmit);
      } else {
         m = (struct message*)buf;

         if (is_valid_coordinator(*m, size)) {
            // check if I received it already
            if ( !(gottenack & (1 << m->id)) ) {
               //printf("Ack from %i received\n", m->id);
               gottenack |= (1 << m->id);
               i++;
            }

         }

#ifndef SMMP_MEMCPY
         smmp_message_is_read(buf, size);
#endif
      }
   }

#ifdef SMMP_MEMCPY
   free(buf);
#endif
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
   //else
   printf("%lu\t%f\t%f\t%lu\n", total_nb_req, avg_thr, stddev, nb_retransmit);
}


void launch_coordinator(unsigned long nb_req) {
   int i;
   unsigned long total_nb_req;
   unsigned long timestamp; // max val = 4 294 967 295
   struct node *L;
   struct timer T;

   total_nb_req = 0;
   L = NULL;
   timer_init(&T);

   printf("Core 0 is ready with %i cores, %lu requests per step, requests of size %i and acks every %lu requests\n", nb_cores, nb_req, req_size, ack_laps);

   timestamp = 0;
   while (1) {
      // timer on
      timer_on(&T);

      for (i=0; i<nb_req; i++) {
         send_request_to_all();
         timestamp++;      

         if (ack_laps > 0 && timestamp == ack_laps) {
            //printf("timestamp=%lu, ack_laps=%lu, waiting for ack\n", timestamp, ack_laps);
            timestamp = 0;
            wait_for_acks();
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
}


void launch_replica(void) {
   int size, ret;
   unsigned long timestamp;
   char *buf;


#ifdef SMMP_MEMCPY
   buf = (char*)malloc(sizeof(char)*MESSAGE_MAX_SIZE);
   if (!buf) {
      fprintf(stderr, "Error while allocating memory\n");
      exit(-1);
   }
#endif

   printf("Core %i is ready with requests of size %i and acks every %lu requests\n", core_id, req_size, ack_laps);

   timestamp = 0;
   while (1) {
      // receive request
#ifdef SMMP_MEMCPY
      ret = 0;
      size = smmp_recv_memcpy((void*)buf, MESSAGE_MAX_SIZE);
#else
      ret = smmp_recv((void**)&buf, &size);
#endif

      if (ret == -1)
         continue;

      // if the request is valid then send an ack
      if (!is_valid_replica(*((struct message*)buf), size))
         ; //fprintf(stderr, "Core %i received a non-valid request.\n", core_id);
      else {
         //printf("Core %i received a request and is sending an ack.\n", core_id);
         timestamp++;
      }

#ifndef SMMP_MEMCPY
         smmp_message_is_read(buf, size);
#endif

      if (ack_laps > 0 && timestamp == ack_laps) {
         timestamp = 0;
         //printf("core %i is sending an ack\n", core_id);
         send_ack();
      }

   //   usleep(1);
   }

#ifdef SMMP_MEMCPY
   free(buf);
#endif
}


int main(int argc, char **argv) {
   int ret, i;
   unsigned long nb_req;
   int smmp_nb_msg;

   nb_retransmit = 0;
   req_size = 0;
   ack_laps = 1;
   nb_cores = -1;
   smmp_nb_msg = 10;
   nb_req = 1000;


   // initialize
   int opt;
   while ((opt = getopt(argc, argv, "n:r:s:a:m:")) != EOF) {
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

         case 'm':
            smmp_nb_msg = atoi(optarg);
            break;

         default:
            fprintf(stderr, "Usage: %s -n nb_cores -r nb_req -s req_size -a ack_laps -m smmp_nb_msg\n", argv[0]);
            exit(-1);
      }
   }

   if (nb_cores < 1 || nb_cores > MAX_NB_CORES) {
      fprintf(stderr, "Usage: %s -n nb_cores -r nb_req -s req_size -a ack_laps -m smmp_nb_msg\n", argv[0]);
      fprintf(stderr, "\tnb_cores must be greater than 0 and less than %i\n", MAX_NB_CORES);
      exit(-1);
   }

   if (req_size + MSG_HEADER_SIZE > MESSAGE_MAX_SIZE) {
      fprintf(stderr, "req_size is too big: %i < %i\n", req_size, MESSAGE_MAX_SIZE - MSG_HEADER_SIZE);
      exit(-1);
   }


   //   recv_buf = (char*)malloc(sizeof(char)*MESSAGE_MAX_SIZE);
   //   if (!recv_buf) exit(-1);

   ret = smmp_init(argv[0], smmp_nb_msg);
   if (ret == -1) {
      printf("Error in smmp_init! Aborting.\n");
      return -1;
   }


   // fork
   core_id = 0;
   for (i=1; i<nb_cores; i++) {
      if (!fork()) {
         core_id = i;
         break; // i'm a child, so I exit the loop
      }
   }


   /*********** executed by everyone ***********/


   if (core_id == 0) {
      sleep(2);
      launch_coordinator(nb_req);
   } else { 
      launch_replica();
   }

   return 0;
}

