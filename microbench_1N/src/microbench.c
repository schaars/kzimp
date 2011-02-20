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

// stop the experiment after XP_MAX_DURATION seconds
#define XP_MAX_DURATION (60*10)

// name of the file which will contain statistics
#define STATISTICS_FILE_PREFIX "./statistics"
#define STATISTICS_FILE_SUFFIX ".log"

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static long nb_messages; // total number of messages sent/received during this experiment
static long xp_duration; // duration of the experiment, in seconds
static int message_size; // messages size in bytes

// start and end of the experiment, in cycles
uint64_t cycle_start_xp, cycle_stop_xp;

// return the throughput, in MB/s
double do_producer(void)
{
  long nb_msg;
  uint64_t thr_start_time, thr_stop_time, thr_elapsed_time, thr_current_time;
  uint64_t total_payload;
  double throughput;

  nb_msg = 0;
  thr_elapsed_time = 0;
  rdtsc(thr_start_time);

  while (thr_elapsed_time < xp_duration * 1000000)
  {
#ifdef DEBUG2
    printf("[producer] Sending message %i\n", nb_msg);
#endif

    IPC_sendToAll(message_size, 0);

    nb_msg++;
    rdtsc(thr_current_time);
    thr_elapsed_time = diffTime(thr_current_time, thr_start_time);
  }

  IPC_sendToAll(message_size, -2);
  nb_msg++;

  rdtsc(thr_stop_time);
  thr_elapsed_time = diffTime(thr_stop_time, thr_start_time);

  total_payload = nb_msg * message_size;
  nb_messages = nb_msg;

  // total_payload is in bytes
  // thr_elapsed_time is in usec
  throughput = ((double) total_payload / 1000000.0)
      / ((double) thr_elapsed_time / 1000000.0);

#ifdef DEBUG
  printf(
      "[producer] Throughput = %f MB/s\n", throughput);
#endif

  cycle_start_xp = thr_start_time;

  return throughput;
}

// return the throughput, in MB/s
double do_consumer(void)
{
  long nb_msg;
  char msg_id;
  uint64_t thr_start_time, thr_stop_time, thr_elapsed_time;
  uint64_t total_payload;
  double throughput;

  IPC_receive(message_size, &msg_id);

  thr_start_time = get_current_time();
  nb_msg = 0;

  while (1)
  {
    IPC_receive(message_size, &msg_id);

#ifdef DEBUG2
    printf("[consumer %i] Receiving message %i\n", core_id, nb_msg);
#endif

    nb_msg++;
    if (msg_id == -2)
    {
      break;
    }
  }

  rdtsc(cycle_stop_xp);

  thr_stop_time = get_current_time();
  thr_elapsed_time = thr_stop_time - thr_start_time;

  total_payload = nb_msg * message_size;
  nb_messages = nb_msg;

  // total_payload is in bytes
  // thr_elapsed_time is in usec
  throughput = ((double) total_payload / 1000000.0)
      / ((double) thr_elapsed_time / 1000000.0);

#ifdef DEBUG
  printf("[consumer %i] Throughput = %f MB/s\n", core_id, throughput);
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
  fprintf(
      stderr,
      "Usage: %s -r nb_receivers -t xp_duration_in_sec -s messages_size_in_B\n",
      program_name);
  exit(-1);
}

int main(int argc, char **argv)
{
  nb_receivers = -1;
  message_size = -1;

  // process command line options
  int opt;
  while ((opt = getopt(argc, argv, "r:t:s:")) != EOF)
  {
    switch (opt)
    {
    case 'r':
      nb_receivers = atoi(optarg);
      break;

    case 't':
      xp_duration = atol(optarg);
      break;

    case 's':
      message_size = atoi(optarg);
      break;

    default:
      print_help_and_exit(argv[0]);
    }
  }

  if (nb_receivers <= 0 || xp_duration <= 0 || message_size <= 0)
  {
    print_help_and_exit(argv[0]);
  }

  cycle_start_xp = cycle_stop_xp = 0;

  init_clock_mhz();

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

#ifdef CORE_EXPERIMENT
  if (core_id != 0)
  {
    CPU_SET(CORE_EXPERIMENT_CORE_ID, &mask);
  }
  else
  {
    CPU_SET(core_id, &mask);
  }
#else
  CPU_SET(core_id * NB_THREADS_PER_CORE, &mask);
#endif

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

    float nb_cycles_per_byte_send = (float) get_cycles_send() / (nb_messages
        * message_size);
    float nb_cycles_per_byte_recv = (float) get_cycles_recv() / ((nb_messages
        - 1) * message_size);

    // When receiving we do not take into account the first message since the receive is blocking and does not start necesarily with
    // a message in the mechanism buffer for reception
    fprintf(
        F,
        "core_id= %i\nnb_receivers= %i\nnb_messages= %li\nmessages_size= %i\nthr= %f\nnb_cycles_send= %f\nnb_cycles_recv= %f\ncycle_start_xp= %lu\n",
        core_id, nb_receivers, nb_messages, message_size, throughput,
        nb_cycles_per_byte_send, nb_cycles_per_byte_recv,
        (unsigned long) cycle_start_xp);

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
      perror("Error while creating the file for the consumer");
    }

    float nb_cycles_per_byte_send = (float) get_cycles_send() / (nb_messages
        * message_size);
    float nb_cycles_per_byte_recv = (float) get_cycles_recv() / ((nb_messages
        - 1) * message_size);

    // When receiving we do not take into account the first message since the receive is blocking and does not start necesarily with
    // a message in the mechanism buffer for reception
    fprintf(
        F,
        "core_id= %i\nnb_receivers= %i\nnb_messages= %li\nmessages_size= %i\nthr= %f\nnb_cycles_send= %f\nnb_cycles_recv= %f\ncycle_stop_xp= %lu\n",
        core_id, nb_receivers, nb_messages, message_size, throughput,
        nb_cycles_per_byte_send, nb_cycles_per_byte_recv,
        (unsigned long) cycle_stop_xp);

    fclose(F);
  }

  return 0;
}
