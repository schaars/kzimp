/*
 * Checkpoint_request.cc
 *
 * What is a checkpoint request
 */

#include <stdint.h>

#include "Checkpoint_request.h"

Checkpoint_request::Checkpoint_request(void) :
#ifdef ULM
      Message(sizeof(struct checkpoint_request), CHECKPOINT_REQUEST, -1)
#else
      Message(sizeof(struct checkpoint_request), CHECKPOINT_REQUEST)
#endif
{
  rep()->caller = -1;
  rep()->cn = ~0;
}

Checkpoint_request::Checkpoint_request(int _caller, uint64_t _cn) :
#ifdef ULM
      Message(sizeof(struct checkpoint_request), CHECKPOINT_REQUEST, -1)
#else
      Message(sizeof(struct checkpoint_request), CHECKPOINT_REQUEST)
#endif
{
  rep()->caller = _caller;
  rep()->cn = _cn;
}

Checkpoint_request::~Checkpoint_request(void)
{
}
