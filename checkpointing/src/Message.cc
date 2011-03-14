/*
 * Message.cc
 *
 * What is a message
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#include "Message.h"
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
#undef MSG_DEBUG

Message::Message(void)
{
  init_message(Max_message_size, UNKNOWN);
}

Message::Message(MessageTag tag)
{
  init_message(Max_message_size, tag);
}

Message::Message(size_t len, MessageTag tag)
{
  init_message(len, tag);
}

#ifdef ULM
Message::Message(size_t len, MessageTag tag, int cid)
{
  init_message(len, tag, true, cid);
}
#endif

Message::~Message(void)
{
#ifndef ULM
  free(msg);
#endif
}

// initialize the message
// +ulm_alloc is considered only when using ULM. If set to true, then this message is allocated
//  directly in shared memory.
// +nid is relevant only when using ULM. If set to -1, then this message will be multicast,
//  otherwise it is sent to node nid.
void Message::init_message(size_t len, MessageTag tag, bool ulm_alloc, int nid)
{
#ifdef ULM

  if (ulm_alloc)
  {
    msg = (char*) IPC_ulm_alloc(len, &msg_pos_in_ring_buffer, nid);
  }
  else
  {
    msg = (char*) malloc(len);
    if (!msg)
    {
      perror("Message creation failed! ");
      exit(errno);
    }

    msg_pos_in_ring_buffer = -1;
  }

#else

  msg = (char*) malloc(len);
  if (!msg)
  {
    perror("Message creation failed! ");
    exit(errno);
  }

#endif

  rep()->tag = tag;
  rep()->len = len;

#ifdef MSG_DEBUG
  printf("Creating a new Message %i of size %lu\n", rep()->tag, rep()->len);
#endif
}
