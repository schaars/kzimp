/*
 * Message.h
 *
 * What is a message
 */

#ifndef REQUEST_H_
#define REQUEST_H_

#include <stdint.h>

#include "Message.h"

struct message_request: public message_header
{
  int cid; // client id
  uint64_t value;
};

class Request: public Message
{
public:
  Request(void);
  Request(int cid, uint64_t value);
  ~Request();

  int cid();

  uint64_t value();

private:
};

inline int Request::cid() {
  //todo
  return 0;
}

uint64_t Request::value() {

}

#endif /* REQUEST_H_ */
