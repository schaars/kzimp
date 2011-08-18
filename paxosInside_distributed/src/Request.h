/*
 * Request.h
 *
 * What a request is
 */

#ifndef REQUEST_H_
#define REQUEST_H_

#include <stdint.h>

#include "Message.h"

#define MESSAGE_REQUEST_SIZE (MESSAGE_HEADER_SIZE + sizeof(int) + sizeof(uint64_t))
struct message_request
{
  MessageTag tag;
  size_t len; // total length of the message, including this header
  int cid; // client id
  uint64_t value;

  char __p[MESSAGE_MAX_SIZE - MESSAGE_REQUEST_SIZE];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

class Request: public Message
{
public:
  Request(void);
  Request(int cid, uint64_t value);
  ~Request(void);

  void init_request(int cid, uint64_t value);

  // client id
  int cid(void) const;

  // value
  uint64_t value(void) const;

private:
  // cast content to a struct message_request*
  struct message_request *rep(void) const;
};

inline int Request::cid(void) const
{
  return rep()->cid;
}

inline uint64_t Request::value(void) const
{
  return rep()->value;
}

// cast content to a struct message_request*
inline struct message_request *Request::rep(void) const
{
  return (struct message_request *) msg;
}

#endif /* REQUEST_H_ */
