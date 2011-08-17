/*
 * Learn.h
 *
 * What a Learn message is
 */

#ifndef LEARN_H_
#define LEARN_H_

#include <stdint.h>

#include "Message.h"

#define MESSAGE_LEARN_SIZE (MESSAGE_HEADER_SIZE + sizeof(int) + 3*sizeof(uint64_t))
struct message_learn
{
  MessageTag tag;
  size_t len; // total length of the message, including this header
  int cid; // client id
  uint64_t proposal_number;
  uint64_t instance_number;
  uint64_t value;

  char __p[MESSAGE_MAX_SIZE - MESSAGE_LEARN_SIZE];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

class Learn: public Message
{
public:
  Learn(void);
  Learn(int cid, uint64_t pn, uint64_t in, uint64_t v);
  ~Learn(void);

  void init_learn(int cid, uint64_t pn, uint64_t in, uint64_t v);

  // client id
  int cid(void) const;

  // proposal number
  uint64_t proposal_number(void) const;

  // instance number
  uint64_t instance_number(void) const;

  // value
  uint64_t value(void) const;

private:
  // cast content to a struct message_learn*
  struct message_learn *rep(void) const;
};

inline int Learn::cid(void) const
{
  return rep()->cid;
}

// proposal number
inline uint64_t Learn::proposal_number(void) const
{
  return rep()->proposal_number;
}

// instance number
inline uint64_t Learn::instance_number(void) const
{
  return rep()->instance_number;
}

// value
inline uint64_t Learn::value(void) const
{
  return rep()->value;
}

// cast content to a struct message_request*
inline struct message_learn *Learn::rep(void) const
{
  return (struct message_learn *) msg;
}

#endif /* LEARN_H_ */
