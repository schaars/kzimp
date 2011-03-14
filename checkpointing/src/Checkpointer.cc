/*
 * Checkpointer.cc
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Checkpointer.h"
#include "MessageTag.h"
#include "Message.h"
#include "Checkpoint_request.h"
#include "Checkpoint_response.h"
#include "time.h"
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
#undef MSG_DEBUG

Checkpointer::Checkpointer(int _node_id, int _nb_nodes, uint64_t _nb_iter)
{
  nid = _node_id;
  nb_nodes = _nb_nodes;
  nb_iter = _nb_iter;
  iter = 0;
  snapshot_in_progress = false;

  chkpt.cn = 0;
  chkpt.value = node_id();

  sum_of_latencies_new = 0;
  sum_of_latencies_send = 0;

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

  init_clock_mhz();

#ifdef MSG_DEBUG
  printf("Checkpointer.new(%i, %i, %lu, <%lu, %lu>, %lx)\n",
      node_id(), nb_nodes, nb_iter,
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
  uint64_t thr_start_time, thr_stop_time;

  rdtsc(thr_start_time);

  while (iter < nb_iter)
  {
    // take a snapshot
    if (node_id() == 0 && !snapshot_in_progress)
    {
      take_snapshot();
    }

    // receive a message
    recv(&m);
  }

  rdtsc(thr_stop_time);

#ifdef MSG_DEBUG
  printf("Node %i has finished its %lu iterations.\n", node_id(), nb_iter);
#endif

  // thr_elapsed_time is in usec
  uint64_t thr_elapsed_time = diffTime(thr_stop_time, thr_start_time);
  double elapsed_time_sec = (double) thr_elapsed_time / 1000000.0;
  double throughput = ((double) nb_iter) / elapsed_time_sec;

  FILE *results_file = fopen(LOG_FILE, "a");
  if (!results_file)
  {
    fprintf(stderr, "[Node %i] Unable to open %s in append mode.\n", node_id(),
        LOG_FILE);
  }

  printf("Node= %i\tnb_iter= %lu\tthr= %f snap/s\n", node_id(), nb_iter,
      throughput);

  if (results_file)
  {
    fprintf(results_file, "Node= %i\tnb_iter= %lu\tthr= %f snap/s\n",
        node_id(), nb_iter, throughput);
  }

  fclose(results_file);

  // create an empty file to announce that this node has finished
  char filename[256];
  sprintf(filename, "/tmp/checkpointing_node_%i_finished", node_id());
  int fd = open(filename, O_WRONLY | O_CREAT, 0666);
  close(fd);

  while (1)
  {
    sleep(1);
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

  Checkpoint_request cr(node_id(), chkpt.cn);

#ifdef ULM
  IPC_send_multicast(cr.content(), cr.length(), cr.get_msg_pos());
#else
  IPC_send_multicast(cr.content(), cr.length());
#endif
}
