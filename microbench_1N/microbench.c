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
#define DEBUG 1

// number of threads per core. Set it to 2 when having a hyperthreaded CPU
#define NB_THREADS_PER_CORE 1

// name of the file which will contain statistics
#define STATISTICS_FILE_PREFIX "./statistics_consumer_"
#define STATISTICS_FILE_SUFFIX ".log"

static int core_id; // 0 is the producer. The others are children
static int nb_receivers;
static long nb_requests; // do not forget the warmup phase which lasts nb_requests/2
static int request_size; // requests size in bytes

void do_producer(void)
{
  long nb_req;

#ifdef DEBUG
  printf("[core %i] I am a producer and I am doing my job\n", core_id);
#endif

  for (nb_req = 0; nb_req < nb_requests; nb_req++)
  {
    printf("[core %i] Sending message %li\n", core_id, nb_req);
    IPC_sendToAll(request_size);
  }
}

void do_consumer(void)
{
  long nb_req;
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

  //TODO: compute throughput + latency

  nb_req = 0;
  while (nb_req < nb_requests)
  {
    rdtsc(lat_start);
    int ret = IPC_receive(request_size);
    rdtsc(lat_stop);

    if (ret)
    {
#ifdef DEBUG
      printf("[core %i] Receiving valid message %li\n", core_id, nb_req);
#endif

      // are we in the logging phase?
      if (nb_req >= nb_requests / 2)
      {
        // compute latency in usec
        current_lat = diffTime(lat_stop, lat_start);
        list_of_lat = list_add(list_of_lat, current_lat);
      }

      nb_req++;
    }
    else
    {
#ifdef DEBUG
      printf("[core %i] Receiving unvalid message %li\n", core_id, nb_req);
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
  // STATISTICS_FILE_PREFIX + core_id + STATISTICS_FILE_SUFFIX
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
      "Usage: %s -n nb_receivers -r nb_requests -m requests_size_in_B\nThere is a warmup phase which lasts nb_requests/2\n",
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

  if (nb_receivers <= 0 || nb_requests <= 0 || request_size <= 0)
  {
    print_help_and_exit(argv[0]);
  }

  init_clock_mhz();

  // initialize the mechanism
  IPC_initialize(nb_receivers, request_size);

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
    do_producer();

    // wait for children to terminate
    wait_for_receivers();

    // release mechanism resources
    IPC_clean_producer();
    IPC_clean();
  }
  else
  {
    IPC_initialize_consumer(core_id);
    do_consumer();
    IPC_clean_consumer();
  }

  return 0;
}
