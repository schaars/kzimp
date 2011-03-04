/*
 * Accept_req.h
 *
 * What a Accept_req message is
 */

#ifndef ACCEPT_REQ_H_
#define ACCEPT_REQ_H_

#include <stdint.h>

#include "Message.h"

struct message_accept_req: public message_header
{
  int cid; // client id
  uint64_t proposal_number;
  uint64_t instance_number;
  uint64_t value;
};

class Accept_req: public Message
{
public:
  Accept_req(void);
  Accept_req(int cid, uint64_t pn, uint64_t in, uint64_t v);
  ~Accept_req(void);

  // client id
  int cid(void);

  // proposal number
  uint64_t proposal_number(void);

  // instance number
  uint64_t instance_number(void);

  // value
  uint64_t value(void);

private:
  // cast content to a struct message_accept_req*
  struct message_accept_req *rep(void);
};

inline int Accept_req::cid(void)
{
  return rep()->cid;
}

// proposal number
inline uint64_t Accept_req::proposal_number(void)
{
  return rep()->proposal_number;
}

// instance number
inline uint64_t Accept_req::instance_number(void)
{
  return rep()->instance_number;
}

// value
inline uint64_t Accept_req::value(void)
{
  return rep()->value;
}

// cast content to a struct message_accept_req*
inline struct message_accept_req *Accept_req::rep(void)
{
  return (struct message_accept_req *) msg;
}

#endif /* ACCEPT_REQ_H_ */
