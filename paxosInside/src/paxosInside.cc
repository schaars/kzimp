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
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "PaxosNode.h"
#include "Client.h"
#include "comm_mech/ipc_interface.h"

// number of threads per core. Set it to 2 when having a hyperthreaded CPU
#define NB_THREADS_PER_CORE 2

// do not modify this value, otherwise you need to modify
// who is the leader in the communication mechanism
const int leader_node_id = 0;

int main(int argc, char **argv)
{
  // get the list of nodes to create
  int nb_nodes = 3; // this counts the number of paxos nodes
  int nb_clients = 1; // this counts the number of clients
  uint64_t nb_iter = 1; // number of requests sent by each client before terminating

  //todo
  //we should have an array T[node_id] = core_on_which_to_run_this_node

  IPC_initialize(nb_nodes, nb_clients);

  // create them (with fork)
  int core_id = 0;
  int i;
  for (i = 1; i < nb_nodes + nb_clients; i++)
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

  if (core_id < nb_nodes)
  {
    IPC_initialize_node(core_id);

    // create the node
    PaxosNode *paxosNode;
    if (core_id == leader_node_id)
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

    IPC_clean_node();
  }
  else
  {
    IPC_initialize_client(core_id);

    Client *c = new Client(core_id, nb_iter);

    c->run();

    delete c;

    IPC_clean_client();
  }

  // wait for my children
  if (core_id == 0)
  {
    int status;

    for (i = 0; i < nb_nodes; i++)
    {
      wait(&status);
    }

    IPC_clean();
  }

  return 0;
}
