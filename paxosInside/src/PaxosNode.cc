/*
 * PaxosNode.cc
 *
 */

#include <stdio.h>

#include "PaxosNode.h"
#include "Proposer.h"
#include "Acceptor.h"
#include "Learner.h"
#include "Message.h"

PaxosNode::PaxosNode(int _node_id, bool _iAmLeader)
{
  nid = _node_id;
  iAmLeader = _iAmLeader;

  proposer = NULL;
  acceptor = NULL;
  learner = new Learner(this);

  switch (nid)
  {
  case 0:
    proposer = new Proposer(this);
    break;

  case 1:
    acceptor = new Acceptor(this);
    break;

  case 2:
    // there is no special additional role for this node
    break;

  default:
    printf("[PaxosNode::new()] There is no node of id %i\n", nid);
    break;
  }

  printf("PaxosNode %i has been created. Is this node a leader? %s\n", nid,
      (iAmLeader ? "true" : "false"));
}

PaxosNode::~PaxosNode()
{
  delete proposer;
  delete acceptor;
  delete learner;
}

// main loop. Receives messages
void PaxosNode::recv(void)
{
  // the leader starts the run by proposing a value
  if (iAmLeader)
  {
    //todo
  }

  while (1)
  {
    Message *msg = new Message();

    delete msg;

    //todo
    // receive and handle messages
    break;
  }
}
