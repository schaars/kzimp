/*
 * Request.cc
 *
 * What a request is
 */

#include <stdio.h>
#include <stdint.h>

#include "Message.h"
#include "MessageTag.h"
#include "Request.h"

Request::Request(void) :
  Message(sizeof(struct message_request), REQUEST)
{
}

Request::Request(int cid, uint64_t value) :
  Message(sizeof(struct message_request), REQUEST)
{
  rep()->cid = cid;
  rep()->value = value;
}

Request::~Request(void)
{
}
