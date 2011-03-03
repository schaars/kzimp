/*
 * paxosInside.cc
 *
 *  Created on: Mar 2, 2011
 *      Author: pl
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "PaxosNode.h"

// number of threads per core. Set it to 2 when having a hyperthreaded CPU
#define NB_THREADS_PER_CORE 2

int main(int argc, char **argv)
{
  // get the list of nodes to create
  int nb_nodes = 3;
  int leader_core_id = 0;
  //todo

  // create them (with fork)
  int core_id = 0;
  int i;
  for (i = 1; i < nb_nodes; i++)
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

  // create the node
  PaxosNode *paxosNode;
  if (core_id == leader_core_id)
  {
    paxosNode = new PaxosNode(core_id, true);
  }
  else
  {
    paxosNode = new PaxosNode(core_id, false);
  }

  // main loop
  paxosNode->recv();

  delete paxosNode;

  // wait for my children
  if (core_id == 0)
  {
    int status;

    for (i = 0; i < nb_nodes; i++)
    {
      wait(&status);
    }
  }

  return 0;
}
