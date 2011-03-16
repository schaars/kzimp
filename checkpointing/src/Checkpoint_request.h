/*
 * Checkpoint_request.h
 *
 * What is a checkpoint request
 */

#ifndef CHECKPOINT_REQUEST_H
#define CHECKPOINT_REQUEST_H

#include <stdint.h>

#include "Message.h"
#include "MessageTag.h"

#define CHECKPOINT_REQUEST_SIZE (MESSAGE_HEADER_SIZE + sizeof(int) + sizeof(uint64_t))
struct checkpoint_request
{
  MessageTag tag; // tag of the message
  size_t len; // length of the message
  int caller; // the node which is asking for a checkpoint
  uint64_t cn; // requested checkpoint number

  char __p[MESSAGE_MAX_SIZE_CHKPT_REQ - CHECKPOINT_REQUEST_SIZE];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

class Checkpoint_request: public Message
{
public:
  Checkpoint_request(void);
  Checkpoint_request(int _caller, uint64_t _cn);
  ~Checkpoint_request(void);

  int caller(void) const;
  uint64_t cn(void) const;

private:
  // cast content to a struct checkpoint_request*
  struct checkpoint_request *rep(void) const;
};

inline int Checkpoint_request::caller(void) const
{
  return rep()->caller;
}

inline uint64_t Checkpoint_request::cn(void) const
{
  return rep()->cn;
}

// cast content to a struct checkpoint_request*
inline struct checkpoint_request *Checkpoint_request::rep(void) const
{
  return (struct checkpoint_request *) msg;
}

#endif /* CHECKPOINT_REQUEST_H */
