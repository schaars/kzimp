#include <stdio.h>
#include <stdlib.h>

#include "time_for_latency.h"

static uint64_t basetime;
static uint64_t *time_for_latency;
static int nb_messages_logging = 100000;

// initialize everything for logging latencies
void time_for_latency_init(uint64_t _basetime)
{
  basetime = _basetime;

  // create the structure for latency
  time_for_latency = (uint64_t*) malloc(sizeof(uint64_t) * nb_messages_logging);
  if (!time_for_latency)
  {
    perror("Allocation error of time_for_latency");
    exit(-1);
  }
}

// add a new tuple
void time_for_latency_add(long n, uint64_t time)
{
  if (n >= 0 && n < nb_messages_logging)
  {
    time_for_latency[n] = time;
  }
}

// output the tuple we have in the FILE *F
void time_for_latency_output(FILE *F)
{
  long m;
  fprintf(F, "message_id\ttime_for_latency\n");
  for (m = 0; m < nb_messages_logging; m++)
  {
    fprintf(F, "%li\t%qd\n", m, (unsigned long long) (time_for_latency[m]
        - basetime));
  }
}

// destroy the allocated structures
void time_for_latency_destroy(void)
{
  free(time_for_latency);
}
