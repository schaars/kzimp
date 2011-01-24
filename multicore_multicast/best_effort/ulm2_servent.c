/* Best Effort Throughput Server */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>

#include "timer.h"
#include "multicore_multicast.h"
#include "mpsoc2.h"


// the signal
unsigned long total_recv;
void sig_fn(int sig) {
   if (sig == SIGINT) {
      //printf("Core %i has received %lu requests\n", core_id, total_recv);
      printf("%i %lu\n", core_id, total_recv);
      fflush(NULL); sync();
      exit(0);
   }
}


void send_request(int req_size) {
   char *buf;
   struct message *m;
   int msize, nw;

   msize = MSG_HEADER_SIZE + req_size;
   buf = mpsoc_alloc(sizeof(char)*msize, &nw);
   if (!buf) exit(-1);
   m = (struct message*)buf;
   bzero((void*)m, req_size);
   m->len = req_size;

   mpsoc_sendto((void*)m, msize, nw, -1); /* -1 for multicast */
}


void do_server(float duration, int thr_limit, int req_size, int sleep_time) {
   float thr_cur, elapsed;
   unsigned long should_have_sent, total_sent;
   struct timer T;

   //printf("Core 0 is ready with %i cores, requests of size %i, a limit throughput of %i req/s and a sleep_time of %i usec\n", nb_cores, req_size, thr_limit, sleep_time);

   // launch exp
   total_sent = 0;
   timer_init(&T);
   timer_on(&T);
   while ( (elapsed = timer_elapsed(T)) <= duration ) {
      should_have_sent = thr_limit * elapsed;

      if (thr_limit > 0 && total_sent >= should_have_sent) {
         //usleep(50000);
      } else {
         send_request(req_size);
         total_sent++;
         thr_cur = (float)total_sent / elapsed;

         /* debug 
            if (total_sent % 10000 == 0)
            printf("%lu req at %f req/s\n", total_sent, thr_cur);
            */

         usleep(sleep_time);
      }
   }

   thr_cur = (float)total_sent/duration;
   //printf("Core 0 has sent %lu requests of size %i at %i req/s\n", total_sent, req_size+MSG_HEADER_SIZE, final_thr);
   printf("%i %lu %f\n", 0, total_sent, thr_cur);
}


void do_client(int req_size, int sleep_time) {
   int size;
   char *buf;
   struct message *m;

   signal(SIGINT, sig_fn);

   //printf("Core %i is ready with requests of size %i and a sleep time of %i sec\n", core_id, req_size, sleep_time);

   total_recv = 0;
   while (1) {
      // receive request
      size = mpsoc_recvfrom((void**)&buf, MSG_HEADER_SIZE + req_size);
      m = (struct message*)buf;

      if (size > 0 && m->len == req_size) {
         total_recv++;
      }

      usleep(sleep_time);
   }

}


int main(int argc, char **argv) {
   int req_size, thr_limit, nb_msg, sleep_time, i;
   float duration;

   // process command line args
   int opt;
   while ((opt = getopt(argc, argv, "n:s:l:t:m:d:")) != EOF) {
      switch (opt) {
         case 'n':
            nb_cores = atoi(optarg);
            break;

         case 's':
            req_size = atoi(optarg);
            break;

         case 'l':
            thr_limit = atoi(optarg);
            break;

         case 't':
            duration = atoi(optarg);
            break;

         case 'm':
            nb_msg = atoi(optarg);
            break;

         case 'd':
            sleep_time = atoi(optarg);
            break;

         default:
            fprintf(stderr, "Usage: %s -n nb_cores -s req_size -l thr_limit (req/s) -t duration -m nb_msg -d sleep_time (usec)\nOption %c has been passed\n", argv[0], opt);
            exit(-1);
      }
   }


   if (nb_cores < 1 || nb_cores > MAX_NB_CORES) {
      fprintf(stderr, "Usage: %s -n nb_cores -s req_size -l thr_limit (req/s) -t duration -m nb_msg -d sleep_time (usec)\n", argv[0]);
      fprintf(stderr, "\tnb_cores must be greater than 1 and less than %i\n", MAX_NB_CORES);
      exit(-1);
   }

   if (req_size + MSG_HEADER_SIZE > MESSAGE_MAX_SIZE) {
      fprintf(stderr, "req_size is too big: %i < %i\n", req_size, MESSAGE_MAX_SIZE - MSG_HEADER_SIZE);
      exit(-1);
   }


   mpsoc_init(argv[0], nb_cores, nb_msg);
   fflush(NULL); sync();

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
      // to wait for all clients
      sleep(2);

      do_server(duration, thr_limit, req_size, sleep_time);
   } else {
      do_client(req_size, sleep_time);
   }


   return 0;
}
