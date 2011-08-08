/*
 * Checkpointer.h
 *
 * The main class: the checkpointer
 */

#ifndef CHECKPOINTER_H_
#define CHECKPOINTER_H_

#define LOG_FILE "results.txt"

struct checkpoint
{
  uint64_t cn;
  uint64_t value;
};

class Message;
class Checkpoint_request;
class Checkpoint_response;

class Checkpointer
{
public:
  Checkpointer(int _node_id, int _nb_nodes, uint64_t _nb_iter);
  ~Checkpointer(void);

  // return the id of this node
  int node_id(void) const;

  // main loop
  void run(void);

private:
  int nid;
  int nb_nodes;
  uint64_t nb_iter, iter;
  bool snapshot_in_progress;
  uint64_t awaited_responses; // each bit represents a node. If set to 1, then we are waiting for a response from this node
  uint64_t awaited_responses_mask; // The initial mask, when we have not yet received any response
  uint64_t cn; // current checkpoint number
  struct checkpoint chkpt; // current checkpoint
  struct checkpoint *snapshot; // current snapshot

  // in order to compute the average latency of the snapshots
  uint64_t latency_new_start, latency_send_start, latency_stop;
  uint64_t sum_of_latencies_new, sum_of_latencies_send;

#ifdef KZIMP_READ_SPLICE
  char chkpt_buf[MESSAGE_MAX_SIZE];
#endif

  void recv(Message *m);

  void handle(Checkpoint_request *req);
  void handle(Checkpoint_response *resp);

  void take_checkpoint(uint64_t new_checkpoint_number);
  void take_snapshot(void);
};

inline int Checkpointer::node_id(void) const
{
  return nid;
}

#endif /* CHECKPOINTER_H_ */
