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
#include "list_of_double.h"
#include "time_for_latency.h"

// debug macro
#define DEBUG
//#undef DEBUG

// more verbose
#define DEBUG2
//#undef DEBUG2

// number of threads per core. Set it to 2 when having a hyperthreaded CPU
#define NB_THREADS_PER_CORE 1

// period at which we compute the current throughput, in sec
#define PERIODIC_THROUGHPUT_COMPUTATION_SEC 10

// period at which we compute the current throughput, in usec
#define PERIODIC_THROUGHPUT_COMPUTATION (PERIODIC_THROUGHPUT_COMPUTATION_SEC*1000*1000)

// name of the file which will contain statistics
#define LATENCIES_FILE_PREFIX "./latencies"
#define STATISTICS_FILE_PREFIX "./statistics"
#define STATISTICS_FILE_SUFFIX ".log"

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static long nb_messages_warmup;
static long nb_messages_logging;
static long nb_messages; // sum of the 2 previous values
static int message_size; // messages size in bytes

// in order to measure the time spent in send() and recv() operations
static uint64_t cycles_in_send, cycles_in_recv;

// for the throughput
double avg_thr, thr_stddev;

uint64_t do_producer(void)
{
  long nb_msg;
  uint64_t logging_phase_start_time;

  // for computing the throughput
  uint64_t thr_start_time, thr_current_time, thr_elapsed_time;
  uint64_t total_payload;
  double current_thr; // in MB/s
  struct my_list_node* list_of_thr;

#ifdef DEBUG2
  printf("[core %i] I am a producer and I am doing my job\n", core_id);
#endif

  logging_phase_start_time = 0;
  total_payload = 0;
  thr_start_time = 0;
  list_of_thr = NULL;
  nb_msg = 0;

  for (nb_msg = 0; nb_msg < nb_messages; nb_msg++)
  {
#ifdef DEBUG2
    printf("[core %i] Sending message %li\n", core_id, nb_msg);
#endif

    if (nb_msg == nb_messages_warmup)
    {
      // get current time, which is the start time of this experiment (we consider only the logging phase)
      logging_phase_start_time = get_current_time();
      thr_start_time = get_current_time();
    }

    if (nb_msg >= nb_messages_warmup)
    {
      // get current time for latency
      time_for_latency_add(nb_msg - nb_messages_warmup, get_current_time());

      total_payload += IPC_sendToAll(message_size, nb_msg, &cycles_in_send);

      // compute the current throughput
      thr_current_time = get_current_time();
      thr_elapsed_time = thr_current_time - thr_start_time;

      // periodically, compute and add thr to the list of throughputs
      if (thr_elapsed_time >= PERIODIC_THROUGHPUT_COMPUTATION)
      {
        // total_payload is in bytes
        // xp_elapsed_time is in usec
        current_thr = ((double) total_payload / 1000000.0)
            / ((double) thr_elapsed_time / 1000000.0);

#ifdef DEBUG
        printf(
            "[producer] Current throughput = %f MB/s, nb_msg in logging = %ld\n",
            current_thr, nb_msg - nb_messages_warmup);
#endif

        list_of_thr = list_add(list_of_thr, current_thr);
        thr_start_time = thr_current_time;
        total_payload = 0;
      }
    }
    else
    {
      IPC_sendToAll(message_size, nb_msg, NULL);
    }
  }

  // compute throughput one last time
  thr_current_time = get_current_time();
  thr_elapsed_time = thr_current_time - thr_start_time;

  // total_payload is in bytes
  // xp_elapsed_time is in usec
  current_thr = ((double) total_payload / 1000000.0)
      / ((double) thr_elapsed_time / 1000000.0);

#ifdef DEBUG
        printf(
            "[producer] Final throughput = %f MB/s, nb_msg in logging = %ld\n",
            current_thr, nb_msg - nb_messages_warmup);
#endif

  list_of_thr = list_add(list_of_thr, current_thr);

  // compute mean and stddev for throughput and latency
  avg_thr = list_compute_avg(list_of_thr);
  thr_stddev = list_compute_stddev(list_of_thr, avg_thr);

  return logging_phase_start_time;
}

void do_consumer(void)
{
  long nb_msg;
  long msg_id;

  // for computing the throughput
  uint64_t thr_start_time, thr_current_time, thr_elapsed_time;
  uint64_t total_payload;
  double current_thr; // in MB/s
  struct my_list_node* list_of_thr;

#ifdef DEBUG2
  printf("[core %i] I am a consumer and I am doing my job\n", core_id);
#endif

  nb_msg = 0;
  total_payload = 0;
  list_of_thr = NULL;
  thr_start_time = 0;
  while (nb_msg < nb_messages)
  {
    uint64_t ret = 0;

    if (nb_msg >= nb_messages_warmup)
    {
      ret = IPC_receive(message_size, &msg_id, &cycles_in_recv);

      // get current time for latency
      time_for_latency_add(nb_msg - nb_messages_warmup, get_current_time());
    }
    else
    {
      ret = IPC_receive(message_size, &msg_id, NULL);
    }

    if (ret)
    {
#ifdef DEBUG2
      printf("[core %i] Receiving valid message %li\n", core_id, msg_id);
#endif

      if (nb_msg == nb_messages_warmup)
      {
        thr_start_time = get_current_time();
      }

      // are we in the logging phase?
      if (nb_msg >= nb_messages_warmup)
      {
        // compute the current throughput
        thr_current_time = get_current_time();
        thr_elapsed_time = thr_current_time - thr_start_time;
        total_payload += ret;

        // periodically, compute and add thr to the list of throughputs
        if (thr_elapsed_time >= PERIODIC_THROUGHPUT_COMPUTATION)
        {
          // total_payload is in bytes
          // xp_elapsed_time is in usec
          current_thr = ((double) total_payload / 1000000.0)
              / ((double) thr_elapsed_time / 1000000.0);

          printf(
              "[consumer %i] Current throughput = %f MB/s, nb_msg in logging = %ld\n",
              core_id, current_thr, nb_msg - nb_messages_warmup + 1);

          list_of_thr = list_add(list_of_thr, current_thr);
          thr_start_time = thr_current_time;
          total_payload = 0;
        }

      }

      nb_msg++;
    }
    else
    {
#ifdef DEBUG2
      printf("[core %i] Receiving unvalid message %li\n", core_id, nb_msg);
#endif
    }
  }

  // compute throughput one last time
  thr_current_time = get_current_time();
  thr_elapsed_time = thr_current_time - thr_start_time;

  // total_payload is in bytes
  // xp_elapsed_time is in usec
  current_thr = ((double) total_payload / 1000000.0)
      / ((double) thr_elapsed_time / 1000000.0);

#ifdef DEBUG
  printf("[consumer %i] Final throughput = %f MB/s, nb_msg in logging = %ld\n",
      core_id, current_thr, nb_msg - nb_messages_warmup);
#endif

  list_of_thr = list_add(list_of_thr, current_thr);

  // compute mean and stddev for throughput and latency
  avg_thr = list_compute_avg(list_of_thr);
  thr_stddev = list_compute_stddev(list_of_thr, avg_thr);
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
  cycles_in_send = cycles_in_recv = 0;

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

  time_for_latency_init(get_current_time(), nb_messages_logging);

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
    uint64_t xp_start_time = do_producer();

    // wait for children to terminate
    wait_for_receivers();

    uint64_t xp_end_time = get_current_time();

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

    fprintf(F, "[producer] thr= %f +/- %f\n", avg_thr, thr_stddev);
    fprintf(
        F,
        "[producer] start_time= %qu usec, end_time= %qu usec, elapsed_time= %qu usec\n",
        (long long unsigned int) xp_start_time,
        (long long unsigned int) xp_end_time,
        (long long unsigned int) (xp_end_time - xp_start_time));
    fprintf(F, "[producer] nb_cycles_in_send= %qd\n",
        (long long unsigned int) cycles_in_send);

    fclose(F);

    // output time_for_latency
    sprintf(filename, "%s_producer%s", LATENCIES_FILE_PREFIX,
        STATISTICS_FILE_SUFFIX);
    time_for_latency_output(filename);
  }
  else
  {
    IPC_initialize_consumer(core_id);
    do_consumer();
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

    fprintf(F, "[consumer %i] thr= %f +/- %f\n", core_id, avg_thr, thr_stddev);
    fprintf(F, "[consumer %i] nb_cycles_in_recv = %qd\n", core_id,
        (long long unsigned int) cycles_in_recv);

    fclose(F);

    // output time_for_latency
    sprintf(filename, "./%s_consumer_%i%s", LATENCIES_FILE_PREFIX, core_id,
        STATISTICS_FILE_SUFFIX);
    time_for_latency_output(filename);
  }

  time_for_latency_destroy();

  return 0;
}
