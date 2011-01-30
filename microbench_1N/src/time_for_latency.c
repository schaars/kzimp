#include <stdio.h>
#include <stdlib.h>

#include "time_for_latency.h"

struct element
{
  long n;
  uint64_t time;

  struct element *next;
};

static struct element *head;
static uint64_t basetime;

// initialize everything for logging latencies
void time_for_latency_init(uint64_t _basetime)
{
  basetime = _basetime;

  head = NULL;
}

// add a new tuple
void time_for_latency_add(long n, uint64_t time)
{
  struct element *elt = (struct element*) malloc(sizeof(struct element));
  if (!elt)
  {
    perror("Allocation error of a new element");
    exit(-1);
  }

  elt->n = n;
  elt->time = time;

  elt->next = head;
  head = elt;
}

// output the tuple we have in the FILE *F
void time_for_latency_output(char *filename)
{
  FILE *F;

  F = fopen(filename, "w");
  if (!F)
  {
    perror("Error while creating the file for latencies");
  }

  struct element *e = head;
  while (e)
  {
    // message_id   time_for_latency_in_usec
    fprintf(F, "%li\t%qd\n", e->n, (unsigned long long) (e->time - basetime));
    e = e->next;
  }

  fclose(F);
}

void time_for_latency_recursive_destroy(struct element *e)
{
  if (e)
  {
    time_for_latency_recursive_destroy(e->next);
    free(e);
  }
}

// destroy the allocated structures
void time_for_latency_destroy(void)
{
  //time_for_latency_recursive_destroy(head);

  struct element *e;
  while (head != NULL)
  {
    e = head;
    head = head->next;
    free(e);
  }
}
