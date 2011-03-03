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

Message::Message(int len, MessageTag tag)
{
  //XXX: when usign ULM we need not to call malloc for sending a message.
  // Instead we call mpsoc_alloc().
  // We call malloc for receiving a message.
  msg = (struct message_header*) malloc(len);
  if (!msg)
  {
    perror("Message creation failed! ");
    exit( errno);
  }

  msg->len = len;
  msg->tag = tag;

  printf("New message of size %i and tag %i has been created.\n", msg->len,
      msg->tag);
}

Message::~Message()
{
  free(msg);
}
