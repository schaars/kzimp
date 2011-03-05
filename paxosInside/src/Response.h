/*
 * Response.h
 *
 * What a response is
 */

#ifndef RESPONSE_H_
#define RESPONSE_H_

#include <stdint.h>

#include "Message.h"

struct message_response: public message_header
{
  uint64_t value;
};

class Response: public Message
{
public:
  Response(void);
  Response(uint64_t value);
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
