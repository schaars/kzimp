/*
 * Message.cc
 *
 * What is a message
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "MessageTag.h"
#include "Message.h"

#if defined(IPC_MSG_QUEUE) || defined(ULM) || defined(KZIMP_SPLICE)
#include "comm_mech/ipc_interface.h"
#endif

Message::Message(void)
{
  init_message(Max_message_size, UNKNOWN);
}

Message::Message(MessageTag tag)
{
  init_message(Max_message_size, tag);
}

#ifdef ULM
Message::Message(size_t len, MessageTag tag, int cid)
{
  init_message(Max_message_size, tag, true, cid);
}
#endif

Message::Message(size_t len, MessageTag tag)
{
#if defined(ULM) || defined(KZIMP_SPLICE)
  init_message(len, tag, true);
#else
  init_message(len, tag);
#endif
}

void Message::init_message(size_t len, MessageTag tag, bool ulm_alloc, int cid)
{
#if defined(IPC_MSG_QUEUE)

  msg = ipc_msg.mtext;

#elif defined(ULM)

  if (ulm_alloc)
  {
    if (tag == ACCEPT_REQ)
    {
      msg = (char*) IPC_ulm_alloc(len, &msg_pos_in_ring_buffer, 1);
    }
    else
    {
      msg = (char*) IPC_ulm_alloc(len, &msg_pos_in_ring_buffer, cid);
    }
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

#elif defined(KZIMP_SPLICE)

  if (ulm_alloc)
  {
    msg = get_next_message();
    msg_pos_in_ring_buffer = 0; // only used when freeing the message, to know that it has been allocated or not
  }
  else
  {
    msg = (char*) malloc(len);
    if (!msg)
    {
      perror("Message creation failed! ");
      exit(errno);
    }

    msg_pos_in_ring_buffer = -1; // only used when freeing the message, to know that it has been allocated or not
  }

#else

  msg = (char*) malloc(len);
  if (!msg)
  {
    perror("Message creation failed! ");
    exit(errno);
  }

#endif

  rep()->len = len;
  rep()->tag = tag;
}

Message::~Message(void)
{
#ifndef IPC_MSG_QUEUE
#if defined(ULM) || defined(KZIMP_SPLICE)
  if (msg_pos_in_ring_buffer == -1)
  {
#endif
    free(msg);
#if defined(ULM) || defined(KZIMP_SPLICE)
  }
#endif
#endif
}
