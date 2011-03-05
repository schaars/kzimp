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

#ifdef IPC_MSG_QUEUE
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

Message::Message(size_t len)
{
  init_message(len, UNKNOWN);
}

Message::Message(size_t len, MessageTag tag)
{
  init_message(len, tag);
}

void Message::init_message(size_t len, MessageTag tag)
{
#ifdef IPC_MSG_QUEUE

  msg = ipc_msg.mtext;

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
  free(msg);
#endif
}
