/*
 * Checkpointer.h
 *
 * The main class: the checkpointer
 */

#ifndef CHECKPOINTER_H_
#define CHECKPOINTER_H_

class Checkpointer
{
public:
  Checkpointer(int _node_id, int _nb_nodes, int _periodic_chkpt);
  ~Checkpointer(void);

  // return the id of this node
  int node_id(void) const;

  // main loop. Receives messages
  void recv(void);

private:
  int nid;
  int nb_nodes;
  int periodic_chkpt;
};

inline int Checkpointer::node_id(void) const
{
  return nid;
}

#endif /* CHECKPOINTER_H_ */
