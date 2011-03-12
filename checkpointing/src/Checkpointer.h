/*
 * Checkpointer.h
 *
 * The main class: the checkpointer
 */

#ifndef CHECKPOINTER_H_
#define CHECKPOINTER_H_

struct checkpoint
{
  uint64_t cn;
  uint64_t value;
};

class Checkpoint_request;
class Checkpoint_response;

class Checkpointer
{
public:
  Checkpointer(int _node_id, int _nb_nodes, uint64_t _nb_iter,
      uint64_t _periodic_chkpt, uint64_t _periodic_snapshot);
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
  uint64_t periodic_chkpt; // elapsed time in usec between 2 checkpoints
  uint64_t periodic_snapshot; // elapsed time in usec between 2 snapshots
  uint64_t cn; // current checkpoint number
  struct checkpoint chkpt; // current checkpoint
  struct checkpoint *snapshot; // current snapshot

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
