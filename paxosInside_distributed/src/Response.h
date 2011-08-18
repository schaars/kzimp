/*
 * Response.h
 *
 * What a response is
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include <stdint.h>

#include "Message.h"

#define MESSAGE_RESPONSE_SIZE (MESSAGE_HEADER_SIZE + sizeof(uint64_t))
struct message_response
{
  MessageTag tag;
  size_t len; // total length of the message, including this header
  uint64_t value;

  char __p[MESSAGE_MAX_SIZE - MESSAGE_RESPONSE_SIZE];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

class Response: public Message
{
public:
  Response(void);
  Response(uint64_t value);

  void init_response(uint64_t value);

#ifdef ULM
  Response(uint64_t value, int cid);
#endif

  ~Response(void);

  // value
  uint64_t value(void) const;

private:
  // cast content to a struct message_response*
  struct message_response *rep(void) const;
};

inline uint64_t Response::value(void) const
{
  return rep()->value;
}

// cast content to a struct message_response*
inline struct message_response *Response::rep(void) const
{
  return (struct message_response *) msg;
}

#endif /* RESPONSE_H_ */
