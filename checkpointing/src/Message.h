/*
 * Message.h
 *
 * What is a message
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdio.h>
#include <stdint.h>

#include "MessageTag.h"

#define CACHE_LINE_SIZE 64

// Effects: Increases sz to the least multiple of ALIGNMENT greater
// than size.
#define ALIGNED_SIZE(sz) ((sz)-(sz)%CACHE_LINE_SIZE+CACHE_LINE_SIZE)

const int Max_message_size = MESSAGE_MAX_SIZE;

#define MESSAGE_HEADER_SIZE (sizeof(int) * 2 + sizeof(uint64_t))
struct message_header
{
  MessageTag tag; // tag of the message
  size_t len; // length of the message
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

class Message
{
public:
  Message(void);
  Message(MessageTag tag);
  Message(size_t len, MessageTag tag);
  ~Message(void);

  // return a pointer to the message
  char* content(void) const;

  // return the length of this message
  size_t length(void) const;

  // return the tag of this message
  MessageTag tag(void) const;

protected:
  char *msg;

private:
  // cast content to a struct message*
  struct message_header *rep(void) const;

  void init_message(size_t len, MessageTag tag);
};

// return a pointer to the message
inline char* Message::content(void) const
{
  return msg;
}

// return the length of this message
inline size_t Message::length(void) const
{
  return rep()->len;
}

// return the tag of this message
inline MessageTag Message::tag(void) const
{
  return rep()->tag;
}

// cast content to a struct message*
inline struct message_header *Message::rep(void) const
{
  return (struct message_header *) msg;
}

#endif /* MESSAGE_H_ */
