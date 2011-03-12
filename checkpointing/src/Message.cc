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

// to get some debug printf
#define MSG_DEBUG
//#undef MSG_DEBUG

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

Message::~Message(void)
{
  free(msg);
}

void Message::init_message(size_t len, MessageTag tag)
{
  msg = (char*) malloc(len);
  if (!msg)
  {
    perror("Message creation failed! ");
    exit(errno);
  }

  rep()->tag = tag;
  rep()->len = len;

#ifdef MSG_DEBUG
  printf("Creating a new Message %i of size %lu\n", rep()->tag, rep()->len);
#endif
}
