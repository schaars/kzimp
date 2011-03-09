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
//#undef MSG_DEBUG

// does the leader sends the response directly, without involving the other nodes?
#define LEADER_ONLY
#undef LEADER_ONLY

// current proposal number. Since we do not change the leader
// this value is fixed
#define PROPOSAL_NUMBER 1

PaxosNode::PaxosNode(int _node_id, bool _iAmLeader)
{
  nid = _node_id;
  iAmLeader = _iAmLeader;
  last_instance_number = 0;

  last_proposal.instance_number = 0;
  last_proposal.proposal_number = 0;
  last_proposal.value = 0;

  printf("PaxosNode %i has been created. Is this node a leader? %s\n", nid,
      (iAmLeader ? "true" : "false"));
}

PaxosNode::~PaxosNode(void)
{
}

// main loop. Receives messages
void PaxosNode::recv(void)
{
  Message m;

  /* Display the size of the different messages
   if (node_id() == 0)
   {
   Message *ms = new Message();
   Request *rs = new Request();
   Accept_req *as = new Accept_req();
   Learn *ls = new Learn();

   printf("Size of Message (%i) = %lu, addr%%64 = %lu\n", ms->tag(),
   ms->length(), (unsigned long) ms->content() % 64);
   printf("Size of Request (%i) = %lu, addr%%64 = %lu\n", rs->tag(),
   rs->length(), (unsigned long) rs->content() % 64);
   printf("Size of Accept_req (%i) = %lu, addr%%64 = %lu\n", as->tag(),
   as->length(), (unsigned long) as->content() % 64);
   printf("Size of Learn (%i) = %lu, addr%%64 = %lu\n", ls->tag(),
   ls->length(), (unsigned long) ls->content() % 64);

   delete ls;
   delete as;
   delete rs;
   delete ms;
   }

   return;
   */

  while (1)
  {
    size_t s = IPC_receive(m.content(), m.length());

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

#ifdef LEADER_ONLY

  Response r(value);

#ifdef ULM
  IPC_send_node_to_client(r.content(), r.length(), cid, r.get_msg_pos());
#else
  IPC_send_node_to_client(r.content(), r.length(), cid);
#endif

#else

  uint64_t in = next_instance_number();
  uint64_t pn = PROPOSAL_NUMBER;

  Accept_req ar(cid, pn, in, value);

#ifdef ULM
  IPC_send_node_unicast(ar.content(), ar.length(), ar.get_msg_pos());
#else
  IPC_send_node_unicast(ar.content(), ar.length());
#endif

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

  Learn learn(cid, pn, in, value);

#ifdef ULM
  IPC_send_node_multicast(learn.content(), learn.length(), learn.get_msg_pos());
  handle_learn(cid, pn, in, value);
#else
  IPC_send_node_multicast(learn.content(), learn.length());
  handle_learn(&learn);
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

  // node 1 is the acceptor. It has already accepted and saved the last proposal
  if (node_id() != 1)
  {
    last_proposal.instance_number = in;
    last_proposal.proposal_number = pn;
    last_proposal.value = value;
  }

  if (iAmLeader)
  {
    Response r(value);

#ifdef ULM
    printf("Leader going to send message to client %i\n", cid);
    IPC_send_node_to_client(r.content(), r.length(), cid, r.get_msg_pos());
#else
    IPC_send_node_to_client(r.content(), r.length(), cid);
#endif
  }
}

#ifdef ULM
void PaxosNode::handle_learn(int cid, uint64_t value, uint64_t in, uint64_t pn)
{
#ifdef MSG_DEBUG
  printf("PaxosNode %i handles a learn (%i, %lu, %lu, %lu)\n", nid, cid, in,
      pn, value);
#endif

  // node 1 is the acceptor. It has already accepted and saved the last proposal
  if (node_id() != 1)
  {
    last_proposal.instance_number = in;
    last_proposal.proposal_number = pn;
    last_proposal.value = value;
  }
}
#endif
