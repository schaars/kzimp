/*
 * Checkpointer.h
 *
 * The main class: the checkpointer
 */

#ifndef CHECKPOINTER_H_
#define CHECKPOINTER_H_

class CheckpointMessage;
class Checkpoint;

class Checkpointer
{
public:
  Checkpointer(int _node_id);
  ~Checkpointer(void);

  // return the id of this node
  int node_id(void) const;

  // main loop. Receives messages
  void recv(void);

private:
  int nid;
  Checkpoint chkpt; // the current checkpoint of this node
};

inline int Checkpointer::node_id(void) const
{
  return nid;
}

#endif /* CHECKPOINTER_H_ */
