/*
 * Acceptor.cc
 *
 */

#include <stdio.h>

#include "Acceptor.h"
#include "PaxosNode.h"

Acceptor::Acceptor(PaxosNode * _paxosNode)
{
  paxosNode = _paxosNode;

  printf("New acceptor for PaxosNode %i\n", paxosNode->node_id());
}

Acceptor::~Acceptor()
{
}
