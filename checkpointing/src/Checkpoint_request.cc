/*
 * Checkpoint_request.cc
 *
 * What is a checkpoint request
 */

#include <stdint.h>

#include "Checkpoint_request.h"

Checkpoint_request::Checkpoint_request(void) :
  Message(sizeof(struct checkpoint_request), CHECKPOINT_REQUEST)
{
  rep()->caller = -1;
  rep()->cn = ~0;
}

Checkpoint_request::Checkpoint_request(int _caller, uint64_t _cn) :
  Message(sizeof(struct checkpoint_request), CHECKPOINT_REQUEST)
{
  rep()->caller = _caller;
  rep()->cn = _cn;
}

Checkpoint_request::~Checkpoint_request(void)
{
}
