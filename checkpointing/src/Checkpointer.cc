/*
 * Checkpointer.cc
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "Checkpointer.h"
#include "MessageTag.h"
#include "Message.h"
#include "Checkpoint_request.h"
#include "Checkpoint_response.h"
#include "time.h"
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
//#undef MSG_DEBUG

Checkpointer::Checkpointer(int _node_id, int _nb_nodes, uint64_t _nb_iter,
    uint64_t _periodic_chkpt, uint64_t _periodic_snapshot)
{
  nid = _node_id;
  nb_nodes = _nb_nodes;
  nb_iter = _nb_iter;
  iter = 0;
  snapshot_in_progress = false;

  chkpt.cn = 0;
  chkpt.value = node_id();

  awaited_responses_mask = 0;
  for (int i = 0; i < nb_nodes; i++)
  {
    if (i != node_id())
    {
      awaited_responses_mask |= (1 << i);
    }
  }

  snapshot = (struct checkpoint*) malloc(sizeof(struct checkpoint) * nb_nodes);
  if (!snapshot)
  {
    perror("Allocation error! ");
    exit(-1);
  }

  for (int i = 0; i < nb_nodes; i++)
  {
    snapshot[i].cn = 0;
    snapshot[i].value = 0;
  }

  int *a = (int*) malloc(sizeof(int));
  srand(1 + node_id() * nb_nodes + (unsigned long int) a);
  free(a);

  periodic_chkpt = _periodic_chkpt + rand() % 10000; // add at most 10 ms
  periodic_snapshot = _periodic_snapshot + rand() % 100000; // add at most 100 ms

  init_clock_mhz();

#ifdef MSG_DEBUG
  printf("Checkpointer.new(%i, %i, %lu, %lu, %lu, <%lu, %lu>, %lx)\n",
      node_id(), nb_nodes, nb_iter, periodic_chkpt, periodic_snapshot,
      chkpt.cn, chkpt.value, awaited_responses_mask);
#endif
}

Checkpointer::~Checkpointer(void)
{
  free(snapshot);
}

// main loop
void Checkpointer::run(void)
{
  Message m;
  uint64_t periodic_chkpt_start_time, periodic_snapshot_start_time;
  uint64_t current_time, elapsed_time;

  rdtsc(periodic_chkpt_start_time);
  periodic_snapshot_start_time = periodic_chkpt_start_time;

  while (iter < nb_iter)
  {
    rdtsc(current_time);

    elapsed_time = diffTime(current_time, periodic_chkpt_start_time);
    if (elapsed_time >= periodic_chkpt)
    {
      // take a checkpoint
      if (!snapshot_in_progress)
      {
        take_checkpoint(chkpt.cn + 1);
      }

      rdtsc(periodic_chkpt_start_time);
    }

    elapsed_time = diffTime(current_time, periodic_snapshot_start_time);
    if (elapsed_time >= periodic_snapshot)
    {
      // take a snapshot
      if (!snapshot_in_progress)
      {
        take_snapshot();
      }

      rdtsc(periodic_snapshot_start_time);
    }

    // receive a message
    recv(&m);
  }

  //todo: print average latency
  //todo: create a file to announce that this client has finished
  printf("Node %i has finished its %lu iterations.\n", node_id(), nb_iter);

  while (1)
  {
    recv(&m);
  }
}

void Checkpointer::recv(Message *m)
{
  size_t recv = IPC_receive(m->content(), m->length());

  if (recv > 0 && m->length() >= sizeof(struct message_header))
  {
    switch (m->tag())
    {
    case CHECKPOINT_REQUEST:
      handle((Checkpoint_request*) m);
      break;

    case CHECKPOINT_RESPONSE:
      handle((Checkpoint_response *) m);
      break;

    default:
#ifdef MSG_DEBUG
      printf(
          "Node %i has received a non-valid message of size %lu (%lu received) and tag %i\n",
          node_id(), m->length(), recv, m->tag());
#endif
      break;
    }
  }
}

void Checkpointer::handle(Checkpoint_request *req)
{
  int caller = req->caller();
  uint64_t requested_cn = req->cn();

#ifdef MSG_DEBUG
  printf(
      "Node %i is handling a checkpoint_request from %i for checkpoint %lu (current is %lu)\n",
      node_id(), caller, requested_cn, chkpt.cn);
#endif

  if (requested_cn > chkpt.cn)
  {
    take_checkpoint(requested_cn);
  }

  // send a Checkpoint_response with the current checkpoint
#ifdef ULM
  Checkpoint_response resp(node_id(), chkpt.cn, chkpt.value, caller);
  IPC_send_unicast(resp.content(), resp.length(), caller, resp.get_msg_pos());
#else
  Checkpoint_response resp(node_id(), chkpt.cn, chkpt.value);
  IPC_send_unicast(resp.content(), resp.length(), caller);
#endif
}

void Checkpointer::handle(Checkpoint_response *resp)
{
  if (!snapshot_in_progress)
  {
    return;
  }

  int sender = resp->sender();
  uint64_t sent_cn = resp->cn();
  uint64_t value = resp->value();

#ifdef MSG_DEBUG
  printf(
      "Node %i is handling a checkpoint_response from %i with checkpoint %lu (current is %lu)\n",
      node_id(), sender, sent_cn, chkpt.cn);
#endif

  snapshot[sender].cn = sent_cn;
  snapshot[sender].value = value;

  // wait for the checkpoint_reponses (1 per node but this one)
  awaited_responses &= ~(1 << sender);

  if (awaited_responses == 0)
  {
    snapshot_in_progress = false;
    iter++;
  }
}

void Checkpointer::take_checkpoint(uint64_t new_checkpoint_number)
{
  chkpt.cn = new_checkpoint_number;
  chkpt.value = node_id();

#ifdef MSG_DEBUG
  printf("Node %i is creating a periodic checkpoint <%lu, %lu>\n", node_id(),
      chkpt.cn, chkpt.value);
#endif
}

void Checkpointer::take_snapshot(void)
{
#ifdef MSG_DEBUG
  printf("Node %i is taking a snapshot\n", node_id());
#endif

  snapshot_in_progress = true;
  awaited_responses = awaited_responses_mask;

  //todo: measure latency
  //        -between send/recv
  //        -between new/recv in order to take into account the allocation time (because ULM allocates messages in shared memory)

  Checkpoint_request cr(node_id(), chkpt.cn);

#ifdef ULM
  IPC_send_multicast(cr.content(), cr.length(), cr.get_msg_pos());
#else
  IPC_send_multicast(cr.content(), cr.length());
#endif
}
