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
  msg = (char*) malloc(len);
  if (!msg)
  {
    perror("Message creation failed! ");
    exit(errno);
  }

  rep()->len = len;
  rep()->tag = tag;
}

Message::~Message(void)
{
  free(msg);
}
