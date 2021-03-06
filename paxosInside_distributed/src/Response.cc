/*
 * Response.cc
 *
 * What a response is
 */

#include <stdio.h>
#include <stdint.h>

#include "Message.h"
#include "MessageTag.h"
#include "Response.h"

Response::Response(void) :
  Message(sizeof(struct message_response), RESPONSE)
{
}

Response::Response(uint64_t value) :
  Message(sizeof(struct message_response), RESPONSE)
{
  rep()->value = value;
}

#ifdef ULM
Response::Response(uint64_t value, int cid) :
  Message(sizeof(struct message_response), RESPONSE, cid)
{
  rep()->value = value;
}
#endif

Response::~Response(void)
{
}
