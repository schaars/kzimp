/* Best Effort Throughput Server */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "timer.h"
#include "multicore_multicast.h"


/* these functions depend on the communication mean
 * we want to use and are linked at compile time
 */
extern void initialize_server(void);
extern void mmcast_send(void *buf, int s, int c);


void send_request(int req_size) {
   char *buf;
   struct message *m;
   int msize;

   msize = MSG_HEADER_SIZE + req_size;
   buf = (char*)malloc(sizeof(char)*msize);
   if (!buf) exit(-1);
   m = (struct message*)buf;

   m->len = req_size;

#ifdef LOCAL_MULTICAST
  mmcast_send((void*)m, msize, -1); /* -1 for multicast */
#else
  int i;

  for (i=1; i<nb_cores; i++) {
    mmcast_send((void*)m, msize, i);
  }
#endif

  free(buf);
}


int main(int argc, char **argv) {
   int req_size, thr_limit;
   float thr_cur, elapsed, duration;
   unsigned long should_have_sent, total_sent;
   struct timer T;

   // process command line args
   int opt;
   while ((opt = getopt(argc, argv, "n:s:l:t:")) != EOF) {
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

         default:
            fprintf(stderr, "Usage: %s -n nb_cores -s req_size -l thr_limit (req/s) -t duration\n", argv[0]);
            exit(-1);
      }
   }

   if (nb_cores < 1 || nb_cores > MAX_NB_CORES) {
      fprintf(stderr, "Usage: %s -n nb_cores -s req_size -l thr_limit (req/s) -t duration\n", argv[0]);
      fprintf(stderr, "\tnb_cores must be greater than 0 and less than %i\n", MAX_NB_CORES);
      exit(-1);
   }

   if (req_size + MSG_HEADER_SIZE > MESSAGE_MAX_SIZE) {
      fprintf(stderr, "req_size is too big: %i < %i\n", req_size, MESSAGE_MAX_SIZE - MSG_HEADER_SIZE);
      exit(-1);
   }

   core_id = 0;
   initialize_server();

   // to wait for all clients
   sleep(2);

   //printf("Core 0 is ready with %i cores, requests of size %i and a limit throughput of %i req/s\n", nb_cores, req_size, thr_limit);

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
      }
   }

   //int final_thr = ( (thr_limit == 0) ? total_sent/duration : thr_limit);
   thr_cur = (float)total_sent/duration;
   //printf("Core 0 has sent %lu requests of size %i at %i req/s\n", total_sent, req_size+MSG_HEADER_SIZE, final_thr);
   printf("%i %lu %f\n", 0, total_sent, thr_cur);
   fflush(NULL); sync();
   
   return 0;
}
