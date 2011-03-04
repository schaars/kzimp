/*
 * Learn.h
 *
 * What a Learn message is
 */

#ifndef LEARN_H_
#define LEARN_H_

#include <stdint.h>

#include "Message.h"

struct message_learn: public message_header
{
  int cid; // client id
  uint64_t proposal_number;
  uint64_t instance_number;
  uint64_t value;
};

class Learn: public Message
{
public:
  Learn(void);
  Learn(int cid, uint64_t pn, uint64_t in, uint64_t v);
  ~Learn(void);

  // client id
  int cid(void);

  // proposal number
  uint64_t proposal_number(void);

  // instance number
  uint64_t instance_number(void);

  // value
  uint64_t value(void);

private:
  // cast content to a struct message_learn*
  struct message_learn *rep(void);
};

inline int Learn::cid(void)
{
  return rep()->cid;
}

// proposal number
inline uint64_t Learn::proposal_number(void)
{
  return rep()->proposal_number;
}

// instance number
inline uint64_t Learn::instance_number(void)
{
  return rep()->instance_number;
}

// value
inline uint64_t Learn::value(void)
{
  return rep()->value;
}

// cast content to a struct message_request*
inline struct message_learn *Learn::rep(void)
{
  return (struct message_learn *) msg;
}

#endif /* LEARN_H_ */
