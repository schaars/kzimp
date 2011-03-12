/*
 * Checkpointer.cc
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "Checkpointer.h"
#include "Checkpoint.h"
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
#undef MSG_DEBUG

Checkpointer::Checkpointer(int _node_id, int _nb_nodes, int _periodic_chkpt)
{
  nid = _node_id;
  nb_nodes = _nb_nodes;
  periodic_chkpt = _periodic_chkpt;
  cn = 0;

  printf("Checkpoint.new(%i, %i, %i)\n", node_id(), nb_nodes, periodic_chkpt);
}

Checkpointer::~Checkpointer(void)
{
}

// main loop. Receives messages
void Checkpointer::recv(void)
{
  //todo
  //todo: measure latency
  //        -between send/recv
  //        -between new/recv in order to take into account the allocation time (because ULM allocates messages in shared memory)

  while (1)
  {
    // for receiving a message
    Checkpoint *c = new Checkpoint();

    delete c;

    //todo
break;
  }
}
