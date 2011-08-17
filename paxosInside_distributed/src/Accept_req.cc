/*
 * Accept_req.cc
 *
 * What a accept_req message is
 */

#include <stdio.h>
#include <stdint.h>

#include "Message.h"
#include "MessageTag.h"
#include "Accept_req.h"

Accept_req::Accept_req(void) :
  Message(sizeof(struct message_accept_req), ACCEPT_REQ)
{
}

Accept_req::Accept_req(int cid, uint64_t pn, uint64_t in, uint64_t v) :
  Message(sizeof(struct message_accept_req), ACCEPT_REQ)
{
  rep()->cid = cid;
  rep()->proposal_number = pn;
  rep()->instance_number = in;
  rep()->value = v;
}

void Accept_req::init_accept_req(int cid, uint64_t pn, uint64_t in, uint64_t v) {
  rep()->cid = cid;
  rep()->proposal_number = pn;
  rep()->instance_number = in;
  rep()->value = v;
}

Accept_req::~Accept_req(void)
{
}
