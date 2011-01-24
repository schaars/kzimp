/* a client - get requets and send back ack */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "multicore_multicast.h"

/* these functions depend on the communication mean
 * we want to use and are linked at compile time
 */
extern void initialize_client(void);
extern int mmcast_receive(void *buf, int s);


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


int main(int argc, char **argv) {
  int size;
  int req_size;
  char *buf;
  struct message *m;

  core_id = -1;
  req_size = 0;

  // process command line options
  int opt;
  while ((opt = getopt(argc, argv, "c:s:")) != EOF) {
    switch (opt) {
    case 'c':
      core_id = atoi(optarg);
      break;

    case 's':
      req_size = atoi(optarg);
      break;

    default:
      fprintf(stderr, "Usage: %s -c core_id -s req_size\n", argv[0]);
      exit(-1);
    }
  }

  if (core_id < 1 || core_id >= MAX_NB_CORES) {
      fprintf(stderr, "Usage: %s -c core_id -s req_size\n", argv[0]);
    fprintf(stderr, "\tcore_id shall be greater than 0 and less than %i\n", MAX_NB_CORES);
    exit(-1);
  }

  if (req_size + MSG_HEADER_SIZE > MESSAGE_MAX_SIZE) {
    fprintf(stderr, "req_size is too big: %i < %i\n", req_size, MESSAGE_MAX_SIZE - MSG_HEADER_SIZE);
    exit(-1);
  }

  signal(SIGINT, sig_fn);

  buf = (char*)malloc(sizeof(char)*MESSAGE_MAX_SIZE);
  if (!buf) {
    fprintf(stderr, "Error while allocating memory\n");
    exit(-1);
  }
  m = (struct message*)buf;

  initialize_client();
  
  //printf("Core %i is ready with requests of size %i\n", core_id, req_size);

  total_recv = 0;
  while (1) {
    // receive request
    size = mmcast_receive((void*)m, MSG_HEADER_SIZE + req_size);

    if (m->len == req_size) {
       total_recv++;
    }
  }

  return 0;
}
