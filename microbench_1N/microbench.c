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
#include "list.h"

// debug macro
//#define DEBUG
#undef DEBUG

// number of threads per core. Set it to 2 when having a hyperthreaded CPU
#define NB_THREADS_PER_CORE 1

// name of the file which will contain statistics
#define STATISTICS_FILE_PREFIX "./statistics_"
#define STATISTICS_FILE_SUFFIX ".log"

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static long nb_messages_warmup;
static long nb_messages_logging;
static long nb_messages; // sum of the 2 previous values
static int message_size; // messages size in bytes

uint64_t do_producer(void)
{
  long nb_msg;
  uint64_t xp_start_time = 0;

#ifdef DEBUG
  printf("[core %i] I am a producer and I am doing my job\n", core_id);
#endif

  for (nb_msg = 0; nb_msg < nb_messages; nb_msg++)
  {
#ifdef DEBUG
    printf("[core %i] Sending message %li\n", core_id, nb_msg);
#endif

    if (nb_msg == nb_messages_warmup)
    {
      // get current time, which is the start time of this experiment (we consider only the logging phase)
      xp_start_time = get_current_time();
    }

    IPC_sendToAll(message_size, nb_msg);
  }

  return xp_start_time;
}

void do_consumer(void)
{
  long nb_msg;
  long msg_id;

  // for computing the throughput
  uint64_t total_payload;

  uint64_t lat_start, lat_stop;

  double current_thr;
  double current_lat;
  struct my_list_node *list_of_thr;
  struct my_list_node *list_of_lat;

  list_of_thr = NULL;
  list_of_lat = NULL;

#ifdef DEBUG
  printf("[core %i] I am a consumer and I am doing my job\n", core_id);
#endif

  nb_msg = 0;
  while (nb_msg < nb_messages)
  {
    rdtsc(lat_start);
    uint64_t ret = IPC_receive(message_size, &msg_id);
    rdtsc(lat_stop);

    if (ret)
    {
#ifdef DEBUG
      printf("[core %i] Receiving valid message %li\n", core_id, nb_msg);
#endif

      // are we in the logging phase?
      if (nb_msg >= nb_messages_warmup)
      {
        total_payload += ret;

        // compute latency in usec
        current_lat = diffTime(lat_stop, lat_start);
        list_of_lat = list_add(list_of_lat, current_lat);
      }

      nb_msg++;
    }
    else
    {
#ifdef DEBUG
      printf("[core %i] Receiving unvalid message %li\n", core_id, nb_msg);
#endif
    }

    // compute the throughput periodically
    //TODO
    //current_thr = 0.0;
    //list_of_thr = list_add(list_of_thr, current_thr);

  }

  // compute throughput one last time
  //TODO
  current_thr = 0.0;
  list_of_thr = list_add(list_of_thr, current_thr);

  // compute mean and stddev for throughput and latency
  double avg_thr = list_compute_avg(list_of_thr);
  double thr_stddev = list_compute_stddev(list_of_thr, avg_thr);
  double avg_lat = list_compute_avg(list_of_lat);
  double lat_stddev = list_compute_stddev(list_of_lat, avg_lat);

  //TODO: output statistics in the file
  // STATISTICS_FILE_PREFIX + consumer_<core_id> + STATISTICS_FILE_SUFFIX
  printf("[consumer %i] thr = %f +/- %f, lat = %f +/- %f\n", core_id, avg_thr,
      thr_stddev, avg_lat, lat_stddev);
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
      "Usage: %s -n nb_receivers -w nb_messages_warmup_phase -l nb_messages_logging_phase -m messages_size_in_B\n",
      program_name);
  exit(-1);
}

int main(int argc, char **argv)
{
  nb_receivers = -1;
  nb_messages_warmup = -1;
  nb_messages_logging = -1;
  nb_messages = -1;
  message_size = -1;

  // process command line options
  int opt;
  while ((opt = getopt(argc, argv, "n:w:l:m:")) != EOF)
  {
    switch (opt)
    {
    case 'n':
      nb_receivers = atoi(optarg);
      break;

    case 'w':
      nb_messages_warmup = atol(optarg);
      break;

    case 'l':
      nb_messages_logging = atol(optarg);
      break;

    case 'm':
      message_size = atoi(optarg);
      break;

    default:
      print_help_and_exit(argv[0]);
    }
  }

  nb_messages = nb_messages_warmup + nb_messages_logging;
  if (nb_receivers <= 0 || nb_messages <= 0 || message_size <= 0)
  {
    print_help_and_exit(argv[0]);
  }

  init_clock_mhz();

  // initialize the mechanism
  IPC_initialize(nb_receivers, message_size);

  // fork in order to create the children
  core_id = 0;
  int i;
  for (i = 1; i <= nb_receivers; i++)
  {
    if (!fork())
    {
      core_id = i * NB_THREADS_PER_CORE;
      break; // i'm a child, so I exit the loop
    }
  }

  // set affinity to 1 core
  cpu_set_t mask;

  CPU_ZERO(&mask);
  CPU_SET(core_id, &mask);

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
    uint64_t xp_start_time = do_producer();

    // wait for children to terminate
    wait_for_receivers();

    uint64_t xp_end_time = get_current_time();

    //TODO: output statistics in the file
    // STATISTICS_FILE_PREFIX + producer + STATISTICS_FILE_SUFFIX
    printf(
        "[producer] start_time=%qu usec, end_time=%qu usec, elapsed_time=%qu usec\n",
        (long long unsigned int) xp_start_time,
        (long long unsigned int) xp_end_time,
        (long long unsigned int) (xp_end_time - xp_start_time));

    // release mechanism resources
    uint64_t cycles_in_send;
    IPC_clean_producer(&cycles_in_send);
    printf("[producer] nb cycles in send = %qd\n",
        (long long unsigned int) cycles_in_send);

    IPC_clean();
  }
  else
  {
    IPC_initialize_consumer(core_id);
    do_consumer();

    uint64_t cycles_in_recv;
    IPC_clean_consumer(&cycles_in_recv);
    printf("[consumer %i] nb cycles in recv = %qd\n", core_id,
        (long long unsigned int) cycles_in_recv);
  }

  return 0;
}
