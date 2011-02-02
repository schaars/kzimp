/* Inter-process communication mechanisms evaluation -- micro-benchmark
 * 1 core sends messages of size M to N other cores
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
#include <stdint.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ipc_interface.h"
#include "time.h"

// debug macro
#define DEBUG
#undef DEBUG

// more verbose
#define DEBUG2
#undef DEBUG2

// number of threads per core. Set it to 2 when having a hyperthreaded CPU
#define NB_THREADS_PER_CORE 2

// name of the file which will contain statistics
#define STATISTICS_FILE_PREFIX "./statistics"
#define STATISTICS_FILE_SUFFIX ".log"

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static long nb_messages;
static int message_size; // messages size in bytes


// return the throughput, in MB/s
double do_producer(void)
{
  long nb_msg;
  uint64_t thr_start_time, thr_stop_time, thr_elapsed_time;
  uint64_t total_payload;
  double throughput;

  thr_start_time = get_current_time();

  for (nb_msg = 0; nb_msg < nb_messages; nb_msg++)
  {
#ifdef DEBUG2
    printf("[producer] Sending message %li\n", nb_msg);
#endif

    total_payload += IPC_sendToAll(message_size, nb_msg);
  }

  thr_stop_time = get_current_time();
  thr_elapsed_time = thr_stop_time - thr_start_time;

  // total_payload is in bytes
  // thr_elapsed_time is in usec
  throughput = ((double) total_payload / 1000000.0)
      / ((double) thr_elapsed_time / 1000000.0);

#ifdef DEBUG
  printf(
      "[producer] Throughput = %f MB/s\n", throughput);
#endif

  return throughput;
}

// return the throughput, in MB/s
double do_consumer(void)
{
  long nb_msg;
  long msg_id;
  uint64_t thr_start_time, thr_stop_time, thr_elapsed_time;
  uint64_t total_payload;
  double throughput;

  IPC_receive(message_size, &msg_id);
  total_payload = 0;
  thr_start_time = get_current_time();

  for (nb_msg = 0; nb_msg < nb_messages - 1; nb_msg++)
  {
    total_payload += IPC_receive(message_size, &msg_id);

#ifdef DEBUG2
    printf("[consumer %i] Receiving message %li\n", core_id, nb_msg);
#endif
  }

  thr_stop_time = get_current_time();
  thr_elapsed_time = thr_stop_time - thr_start_time;

  // total_payload is in bytes
  // thr_elapsed_time is in usec
  throughput = ((double) total_payload / 1000000.0)
      / ((double) thr_elapsed_time / 1000000.0);

#ifdef DEBUG
  printf("[consumer %i] Throughput = %f MB/s\n", throughput);
#endif

  return throughput;
}

void wait_for_receivers(void)
{
  int i;
  int status;

  for (i = 0; i < nb_receivers; i++)
  {
    wait(&status);
  }
}

void print_help_and_exit(char *program_name)
{
  fprintf(stderr,
      "Usage: %s -r nb_receivers -n nb_messages -s messages_size_in_B\n",
      program_name);
  exit(-1);
}

int main(int argc, char **argv)
{
  nb_receivers = -1;
  nb_messages = -1;
  message_size = -1;

  // process command line options
  int opt;
  while ((opt = getopt(argc, argv, "n:r:s:")) != EOF)
  {
    switch (opt)
    {
    case 'r':
      nb_receivers = atoi(optarg);
      break;

    case 'n':
      nb_messages = atol(optarg);
      break;

    case 's':
      message_size = atoi(optarg);
      break;

    default:
      print_help_and_exit(argv[0]);
    }
  }

  if (nb_receivers <= 0 || nb_messages <= 0 || message_size <= 0)
  {
    print_help_and_exit(argv[0]);
  }

  // initialize the mechanism
  IPC_initialize(nb_receivers, message_size);

  fflush(NULL);
  sync();

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
  CPU_SET(core_id * NB_THREADS_PER_CORE, &mask);

  if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
  {
    printf("[core %i] Error while calling sched_setaffinity()\n", core_id);
    perror("");
  }

  // if I am the father, then do_producer()
  // else, do_consumer()
  if (core_id == 0)
  {
    IPC_initialize_producer(core_id);
    double throughput = do_producer();

    // wait for children to terminate
    wait_for_receivers();

    // release mechanism resources
    IPC_clean_producer();
    IPC_clean();

    char filename[256];

    sprintf(filename, "%s_producer%s", STATISTICS_FILE_PREFIX,
        STATISTICS_FILE_SUFFIX);

    FILE *F;
    F = fopen(filename, "w");
    if (!F)
    {
      perror("Error while creating the file for the producer");
    }

    fprintf(
        F,
        "core_id= %i\nnb_receivers= %i\nnb_messages= %li\nmessages_size= %i\nthr= %f\n",
        core_id, nb_receivers, nb_messages, message_size, throughput);

    fclose(F);
  }
  else
  {
    IPC_initialize_consumer(core_id);
    double throughput = do_consumer();
    IPC_clean_consumer();

    char filename[256];

    FILE *F;
    sprintf(filename, "%s_consumer_%i%s", STATISTICS_FILE_PREFIX, core_id,
        STATISTICS_FILE_SUFFIX);
    F = fopen(filename, "w");
    if (!F)
    {
      perror("Error while creating the file for the producer");
    }

    fprintf(
        F,
        "core_id= %i\nnb_receivers= %i\nnb_messages= %li\nmessages_size= %i\nthr= %f\n",
        core_id, nb_receivers, nb_messages, message_size, throughput);

    fclose(F);
  }

  return 0;
}
