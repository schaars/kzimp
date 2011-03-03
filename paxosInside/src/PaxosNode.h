/*
 * PaxosNode.h
 *
 * A PaxosInside node, which is a (Proposer, Acceptor, Learner)
 */

#ifndef PAXOS_NODE_H_
#define PAXOS_NODE_H_

#include "Role.h"

class Proposer;
class Acceptor;
class Learner;

class PaxosNode
{
public:
  PaxosNode(int _node_id, bool _iAmLeader);
  ~PaxosNode();

  // return the id of this node
  inline int node_id(void) const;

  // main loop. Receives messages
  void recv(void);

private:
  int nid;
  bool iAmLeader;

  Proposer *proposer;
  Acceptor *acceptor;
  Learner *learner;
};

inline int PaxosNode::node_id(void) const
{
  return nid;
}

#endif /* PAXOS_NODE_H_ */
