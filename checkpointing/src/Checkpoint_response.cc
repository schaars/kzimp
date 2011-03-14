/*
 * Checkpoint_response.cc
 *
 * What is a checkpoint response
 */

#include <stdint.h>

#include "Message.h"
#include "MessageTag.h"
#include "Checkpoint_response.h"

Checkpoint_response::Checkpoint_response(void) :
  Message(sizeof(struct checkpoint_response), CHECKPOINT_RESPONSE)
{
  rep()->sender = -1;
  rep()->cn = ~0;
  rep()->value = ~0;
}

Checkpoint_response::Checkpoint_response(int _sender, uint64_t _cn,
    uint64_t _value) :
  Message(sizeof(struct checkpoint_response), CHECKPOINT_RESPONSE)
{
  rep()->sender = _sender;
  rep()->cn = _cn;
  rep()->value = _value;
}

#ifdef ULM
Checkpoint_response::Checkpoint_response(int _sender, uint64_t _cn,
    uint64_t _value, int caller) :
  Message(sizeof(struct checkpoint_response), CHECKPOINT_RESPONSE, caller)
{
  rep()->sender = _sender;
  rep()->cn = _cn;
  rep()->value = _value;
}
#endif

Checkpoint_response::~Checkpoint_response(void)
{
}
