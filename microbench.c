/* Inter-process communication mechanisms evaluation -- micro-benchmark
 *
 * Pierre Louis Aublin    <pierre-louis.aublin@inria.fr>
 * January 2011
 *
 * Basic structure taken from:
 *   Patricia K. Immich, Ravi S. Bhagavatula, and Ravi Pendse.
 *   Performance analysis of five interprocess communication mechanisms across unix operating systems.
 *   J. Syst. Softw., 68:27--43, October 2003.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

extern void IPC_initialize(void);
extern void IPC_initialize_producer(void);
extern void IPC_initialize_consumer(void);

extern void IPC_clean(void);
extern void IPC_clean_producer(void);
extern void IPC_clean_consumer(void);

extern void IPC_sendToAll(void);

extern void IPC_receive(void);

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static long nb_requests;
static int request_size; // requests size in bytes

void do_producer(void)
{
  IPC_initialize_producer();

  //TODO
  printf("[core %i] I am a producer and I am doing my job\n", core_id);
}

void do_consumer(void)
{
  IPC_initialize_consumer();

  //TODO
  printf("[core %i] I am a consumer and I am doing my job\n", core_id);

  IPC_clean_consumer();
}

void wait_for_receivers(void)
{
  //TODO

  IPC_clean_producer();
}

void print_help_and_exit(char *program_name)
{
  fprintf(stderr,
      "Usage: %s -n nb_receivers -r nb_requests -m requests_size_in_B\n",
      program_name);
  exit(-1);
}

int main(int argc, char **argv)
{
  nb_receivers = -1;
  nb_requests = -1;
  request_size = -1;

  // process command line options
  int opt;
  while ((opt = getopt(argc, argv, "n:r:m:")) != EOF)
  {
    switch (opt)
    {
    case 'n':
      nb_receivers = atoi(optarg);
      break;

    case 'r':
      nb_requests = atol(optarg);
      break;

    case 'm':
      request_size = atoi(optarg);
      break;

    default:
      print_help_and_exit(argv[0]);
    }
  }

  if (nb_receivers < 0 || nb_requests < 0 || request_size < 0)
  {
    print_help_and_exit(argv[0]);
  }

  // initialize the mechanism
  IPC_initialize();

  // fork in order to create the children
  core_id = 0;
  int i;
  for (i = 1; i <= nb_receivers; i++)
  {
    if (!fork())
    {
      core_id = i;
      break; // i'm a child, so I exit the loop
    }
  }

  // set affinity to 1 core
  cpu_set_t mask;

  CPU_ZERO(&mask);
  CPU_SET(core_id, &mask);

  if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
  {
    perror("Error while callind sched_setaffinity()\n");
  }

  // if I am the father, then do_producer()
  // else, do_consumer()
  if (core_id == 0)
  {
    do_producer();
  }
  else
  {
    do_consumer();
  }

  if (core_id == 0)
  {
    // wait for children to terminate
    wait_for_receivers();

    // release mechanism ressources
    IPC_clean();
  }

  return 0;
}
