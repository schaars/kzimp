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
#include "ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
//#undef MSG_DEBUG

// current proposal number. Since we do not change the leader
// this value is fixed
#define PROPOSAL_NUMBER 1

PaxosNode::PaxosNode(int _node_id, bool _iAmLeader)
{
  nid = _node_id;
  iAmLeader = _iAmLeader;
  last_instance_number = -1;
  nb_iter_for_checkpoint = 0;

  printf("PaxosNode %i has been created. Is this node a leader? %s\n", nid,
      (iAmLeader ? "true" : "false"));
}

PaxosNode::~PaxosNode(void)
{
  ap.clear();
}

//fixme
// will be removed when we will have the clients
int __cid = 42;
uint64_t __v = 1337;

// main loop. Receives messages
void PaxosNode::recv(void)
{
  Message m;

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

  //return;

  //fixme
  // will be removed when we will have the clients
  // the leader starts the run by proposing a value
  if (iAmLeader)
  {
    Request r(__cid, __v);
    handle_request(&r);
  }

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
  printf("PaxosNode %i handles request %i from client %lu\n", node_id(), cid,
      value);
#endif

  uint64_t in = next_instance_number();
  uint64_t pn = PROPOSAL_NUMBER;

  Accept_req ar(cid, pn, in, value);

  IPC_send_node_unicast(ar.content(), ar.length());
}

void PaxosNode::handle_accept_req(Accept_req *ar)
{
  struct proposal prop;
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

  if (ap[in].proposal_number != 0 && ap[in].value != 0)
  {
    // there is already a proposal accepted with this instance number
    prop = ap[in];
  }
  else
  {
    prop.proposal_number = pn;
    prop.value = value;
    ap[in] = prop;
  }

  prop.proposal_number = pn;
  prop.value = value;

  Learn learn(cid, prop.proposal_number, in, prop.value);
  IPC_send_node_multicast(learn.content(), learn.length());

  handle_learn(&learn);

  return;
}

void PaxosNode::handle_learn(Learn *learn)
{
  int cid = learn->cid();
  uint64_t value = learn->value();
  uint64_t in = learn->instance_number();
  uint64_t pn = learn->proposal_number();

  //fixme
  pn = in = value = cid;

  // periodically empty ap, in order to simulate checkpointing on disk
  // and not to use the whole system memory
  nb_iter_for_checkpoint++;
  if (nb_iter_for_checkpoint == checkpoint_frequency)
  {
    ap.clear();
    nb_iter_for_checkpoint = 0;
  }

#ifdef MSG_DEBUG
  printf("PaxosNode %i handles a learn (%i, %lu, %lu, %lu)\n", nid, cid, in,
      pn, value);
#endif

  if (iAmLeader)
  {
    //todo
    //send response to client
    //for now, send a new request

    //sleep(1);

    //fixme
    // will be removed when we will have the clients
    __cid++;
    __v++;
    Request r(__cid, __v);
    handle_request(&r);
  }
}
