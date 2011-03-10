/*
 * Accept_req.h
 *
 * What a Accept_req message is
 */

#ifndef ACCEPT_REQ_H_
#define ACCEPT_REQ_H_

#include <stdint.h>

#include "Message.h"

#define MESSAGE_ACCEPT_REQ_SIZE (MESSAGE_HEADER_SIZE + sizeof(int) + 3*sizeof(uint64_t))
struct message_accept_req
{
  MessageTag tag;
  size_t len; // total length of the message, including this header
  int cid; // client id
  uint64_t proposal_number;
  uint64_t instance_number;
  uint64_t value;

  char __p[MESSAGE_MAX_SIZE - MESSAGE_ACCEPT_REQ_SIZE];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

class Accept_req: public Message
{
public:
  Accept_req(void);
  Accept_req(int cid, uint64_t pn, uint64_t in, uint64_t v);
  ~Accept_req(void);

  // client id
  int cid(void) const;

  // proposal number
  uint64_t proposal_number(void) const;

  // instance number
  uint64_t instance_number(void) const;

  // value
  uint64_t value(void) const;

private:
  // cast content to a struct message_accept_req*
  struct message_accept_req *rep(void) const;
};

inline int Accept_req::cid(void) const
{
  return rep()->cid;
}

// proposal number
inline uint64_t Accept_req::proposal_number(void) const
{
  return rep()->proposal_number;
}

// instance number
inline uint64_t Accept_req::instance_number(void) const
{
  return rep()->instance_number;
}

// value
inline uint64_t Accept_req::value(void) const
{
  return rep()->value;
}

// cast content to a struct message_accept_req*
inline struct message_accept_req *Accept_req::rep(void) const
{
  return (struct message_accept_req *) msg;
}

#endif /* ACCEPT_REQ_H_ */
