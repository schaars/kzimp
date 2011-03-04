/*
 * Learn.cc
 *
 * What a Learn message is
 */

#include <stdio.h>
#include <stdint.h>

#include "Message.h"
#include "MessageTag.h"
#include "Learn.h"

Learn::Learn(void) :
  Message(sizeof(struct message_learn), LEARN)
{
}

Learn::Learn(int cid, uint64_t pn, uint64_t in, uint64_t v) :
  Message(sizeof(struct message_learn), LEARN)
{
  rep()->cid = cid;
  rep()->proposal_number = pn;
  rep()->instance_number = in;
  rep()->value = v;
}

Learn::~Learn(void)
{
}
