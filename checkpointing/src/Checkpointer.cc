/*
 * Checkpointer.cc
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "Checkpointer.h"
//#include "CheckpointMessage.h"
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
#undef MSG_DEBUG

Checkpointer::Checkpointer(int _node_id, int _nb_nodes, int _periodic_chkpt)
{
  nid = _node_id;
  nb_nodes = _nb_nodes;
  periodic_chkpt = _periodic_chkpt;

  printf("Checkpoint.new(%i, %i, %i)\n", node_id(), nb_nodes, periodic_chkpt);
}

Checkpointer::~Checkpointer(void)
{
}

// main loop. Receives messages
void Checkpointer::recv(void)
{
  //todo
}
