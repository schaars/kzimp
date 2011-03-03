/*
 * Proposer.cc
 *
 */

#include <stdio.h>

#include "Proposer.h"
#include "PaxosNode.h"

Proposer::Proposer(PaxosNode * _paxosNode)
{
  paxosNode = _paxosNode;

  printf("New proposer for PaxosNode %i\n", paxosNode->node_id());
}

Proposer::~Proposer()
{
}
