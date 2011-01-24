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

/* recv buffer */
char *recv_buf;

/* these functions depend on the communication mean
 * we want to use and are linked at compile time
 */
extern void initialize_server(void);
extern int mmcast_receive(void *buf, int s);
extern int mmcast_receive_timeout(void *buf, int s);
extern void mmcast_send(void *buf, int s, int c);


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
  char *buffer;
  struct message *m;
  int msize;

  msize = MSG_HEADER_SIZE + req_size;
  buffer = (char*)malloc(sizeof(char)*msize);
  if (!buffer) exit(-1);
  m = (struct message*)buffer;

  m->type = MSG_REQ;
  m->len = req_size;

#ifdef LOCAL_MULTICAST
  m->id = 0;
  mmcast_send((void*)m, msize, -1); /* -1 for multicast */
#else
  int i;

  for (i=1; i<nb_cores; i++) {
    m->id = i;
    mmcast_send((void*)m, msize, i);
  }
#endif

  free(buffer);
}


/* function to send ONE request to the one that needs it after a timeout
 * unicast send
 */
void send_request_timeout(int req_size, int gottenack, unsigned long ack_laps) {
  char *buffer;
  struct message *m;
  int msize;

  msize = MSG_HEADER_SIZE + req_size;
  buffer = (char*)malloc(sizeof(char)*msize);
  if (!buffer) exit(-1);
  m = (struct message*)buffer;

  m->type = MSG_REQ;
  m->len = req_size;

#ifdef LOCAL_MULTICAST
  int i;

  m->id = 0;
  for (i=1; i<nb_cores; i++) {
    if (!(gottenack & (1 << i))) {
      //printf("sending again to core %i\n", i);
      mmcast_send((void*)m, msize, i);
    }
  }
#else
  int i, j;

  for (i=1; i<nb_cores; i++) {
    if (!(gottenack & (1 << i))) {
      for (j=0; j<ack_laps; j++) {
        m->id = i;
        mmcast_send((void*)m, msize, i);
      }
    }
  }
#endif

  free(buffer);
}


/* wait for acks, retransmit message if needed */
void wait_for_acks(unsigned long ack_laps, int req_size) {
  int i, size, gottenack;
  struct message *m;

  i=0;
  gottenack = 0;
  while (i < nb_cores-1) {
    size = mmcast_receive_timeout((void*)recv_buf, MSG_HEADER_SIZE);
    //size = mmcast_receive((void*)recv_buf, MSG_HEADER_SIZE);

    m = (struct message*)recv_buf;

    // size = -2 means timeout, so retransmit
    if (size == -2) {
      send_request_timeout(req_size, gottenack, ack_laps);
      nb_retransmit++;
    } else if (is_valid(*m, size)) {
      // check if I received it already
      if ( !(gottenack & (1 << m->id)) ) {
        //printf("Ack from %i received\n", ((struct message*)buf)->id);
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

  recv_buf = (char*)malloc(sizeof(char)*MESSAGE_MAX_SIZE);
  if (!recv_buf) exit(-1);

  initialize_server();
  
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
//#ifdef LOCAL_MULTICAST
//      // otherwise core 0's list of indexes grows up indefinitely
//      else if (ack_laps == 0) {
//         mmcast_receive((void*)recv_buf, MSG_HEADER_SIZE + req_size);
//      }
//#endif
    }

    // timer off
    timer_off(&T);
    
    // output "nbreqs avgthr stddev"
    total_nb_req += nb_req;
    L = list_add(L, (float)nb_req/timer_elapsed(T));
    timer_reset(&T);
    print_stats(total_nb_req, L);
  }

  free(recv_buf);

  return 0;
}
