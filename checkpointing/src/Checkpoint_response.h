/*
 * Checkpoint_response.h
 *
 * What is a checkpoint response
 */

#ifndef CHECKPOINT_RESPONSE_H
#define CHECKPOINT_RESPONSE_H

#include <stdint.h>

#include "Message.h"
#include "MessageTag.h"

#define CHECKPOINT_RESPONSE_SIZE (MESSAGE_HEADER_SIZE + sizeof(int) + 2*sizeof(uint64_t))
struct checkpoint_response
{
  MessageTag tag; // tag of the message
  size_t len; // length of the message
  int sender; // the sender of this checkpoint
  uint64_t cn; // checkpoint number
  uint64_t value; // checkpoint value

  char __p[MESSAGE_MAX_SIZE - CHECKPOINT_RESPONSE_SIZE];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

class Checkpoint_response: public Message
{
public:
  Checkpoint_response(void);
  Checkpoint_response(int _sender, uint64_t _cn, uint64_t _value);

#ifdef ULM
  Checkpoint_response(int _sender, uint64_t _cn, uint64_t _value, int caller);
#endif

  ~Checkpoint_response(void);

  int sender(void) const;
  uint64_t cn(void) const;
  uint64_t value(void) const;

private:
  // cast content to a struct checkpoint_response*
  struct checkpoint_response *rep(void) const;
};

inline int Checkpoint_response::sender(void) const
{
  return rep()->sender;
}

inline uint64_t Checkpoint_response::cn(void) const
{
  return rep()->cn;
}

inline uint64_t Checkpoint_response::value(void) const
{
  return rep()->value;
}

// cast content to a struct checkpoint_response*
inline struct checkpoint_response *Checkpoint_response::rep(void) const
{
  return (struct checkpoint_response *) msg;
}

#endif /* CHECKPOINT_RESPONSE_H */
