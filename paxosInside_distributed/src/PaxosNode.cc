/*
 * PaxosNode.cc
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <map>

#include "PaxosNode.h"
#include "Message.h"
#include "Request.h"
#include "Accept_req.h"
#include "Learn.h"
#include "Response.h"
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
#undef MSG_DEBUG

// current proposal number. Since we do not change the leader
// this value is fixed
#define PROPOSAL_NUMBER 1

/* By convention:
 *   node 0: leader
 *   node 1: acceptor
 *   node >= 2: learner
 */

PaxosNode::PaxosNode(int _node_id)
{
  nid = _node_id;
  last_instance_number = 0;

  last_proposal.instance_number = 0;
  last_proposal.proposal_number = 0;
  last_proposal.value = 0;

  printf("PaxosNode %i has been created.\n", nid);
}

PaxosNode::~PaxosNode(void)
{
}

// main loop. Receives messages
void PaxosNode::recv(void)
{
  Message m;
  size_t s;

  while (1)
  {
#ifdef KZIMP_READ_SPLICE
    s = IPC_receive(m.content_addr());
#else
    s = IPC_receive(m.content(), m.length());
#endif

#ifdef MSG_DEBUG
    printf(
        "PaxosNode %i has received a message of size %lu (%lu received) and tag %i\n",
        nid, (unsigned long) m.length(), (unsigned long) s, m.tag());
#endif

    switch (m.tag())
    {
    case REQUEST:
      handle_request((Request*) &m);
      break;

    case ACCEPT_REQ:
      handle_accept_req((Accept_req*) &m);
      break;

    case LEARN:
      handle_learn((Learn*) &m);
      break;

    default:
      printf(
          "PaxosNode %i has received a non-valid message of size %lu (%lu received) and tag %i\n",
          nid, (unsigned long) m.length(), (unsigned long) s, m.tag());
      break;
    }

#ifdef KZIMP_READ_SPLICE
    IPC_receive_finalize();
#endif
  }
}

void PaxosNode::handle_request(Request *request)
{
  int cid = request->cid();
  uint64_t value = request->value();

#ifdef MSG_DEBUG
  printf("PaxosNode %i handles request %lu from client %i\n", node_id(), value,
      cid);
#endif

  uint64_t in = next_instance_number();
  uint64_t pn = PROPOSAL_NUMBER;

  ar.init_accept_req(cid, pn, in, value);

#ifdef ULM
  IPC_send_node_unicast(ar.content(), ar.length(), ar.get_msg_pos());
#else
  IPC_send_node_unicast(ar.content(), ar.length());
#endif
}

void PaxosNode::handle_accept_req(Accept_req *ar)
{
  int cid = ar->cid();
  uint64_t value = ar->value();
  uint64_t in = ar->instance_number();
  uint64_t pn = ar->proposal_number();

#ifdef MSG_DEBUG
  printf("PaxosNode %i handles a accept_req (%i, %lu, %lu, %lu)\n", nid, cid,
      in, pn, value);
#endif

  if (pn != PROPOSAL_NUMBER)
  {
    return;
  }

  last_proposal.instance_number = in;
  last_proposal.proposal_number = pn;
  last_proposal.value = value;

  learn.init_learn(cid, pn, in, value);

#ifdef ULM
  IPC_send_node_multicast(learn.content(), learn.length(), learn.get_msg_pos());
#else
  IPC_send_node_multicast(learn.content(), learn.length());
#endif

  return;
}

void PaxosNode::handle_learn(Learn *learn)
{
  int cid = learn->cid();
  uint64_t value = learn->value();
  uint64_t in = learn->instance_number();
  uint64_t pn = learn->proposal_number();

#ifdef MSG_DEBUG
  printf("PaxosNode %i handles a learn (%i, %lu, %lu, %lu)\n", nid, cid, in,
      pn, value);
#endif

  last_proposal.instance_number = in;
  last_proposal.proposal_number = pn;
  last_proposal.value = value;

#ifdef ULM
  Response r(value, cid);
  IPC_send_node_to_client(r.content(), r.length(), cid, r.get_msg_pos());
#else
  r.init_response(value);
  IPC_send_node_to_client(r.content(), r.length(), cid);
#endif
}
