/*
 * Learner.cc
 *
 */

#include <stdio.h>

#include "Learner.h"
#include "PaxosNode.h"

Learner::Learner(PaxosNode * _paxosNode)
{
  paxosNode = _paxosNode;

  printf("New learner for PaxosNode %i\n", paxosNode->node_id());
}

Learner::~Learner()
{
}
