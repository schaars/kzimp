/*
 * PaxosNode.h
 *
 * A PaxosInside node, which is a (Proposer, Acceptor, Learner)
 */

#ifndef PAXOS_NODE_H_
#define PAXOS_NODE_H_

#include <stdint.h>
#include <map>

using namespace std;

const int checkpoint_frequency = 10000;

class Message;
class Request;
class Accept_req;
class Learn;

struct proposal
{
  uint64_t value;
  uint64_t proposal_number;
};

class PaxosNode
{
public:
  PaxosNode(int _node_id, bool _iAmLeader);
  ~PaxosNode(void);

  // return the id of this node
  int node_id(void) const;

  // main loop. Receives messages
  void recv(void);

private:
  void handle_request(Request *request);
  void handle_accept_req(Accept_req *accept_req);
  void handle_learn(Learn *learn);
  uint64_t next_instance_number(void);

  int nid;
  bool iAmLeader;
  uint64_t last_instance_number;
  map<uint64_t, struct proposal> ap;
  int nb_iter_for_checkpoint;
};

inline int PaxosNode::node_id(void) const
{
  return nid;
}

inline uint64_t PaxosNode::next_instance_number(void)
{
  return ++last_instance_number;
}

#endif /* PAXOS_NODE_H_ */
