/*
 * Message.h
 *
 * What is a message
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdlib.h>
#include <stdint.h>

#include "MessageTag.h"

const int Max_message_size = 1024;

struct message_header
{
  MessageTag tag;
  size_t len; // total length of the message, including this header
}__attribute__((__packed__, __aligned__(64)));

class Message
{
public:
  Message(void);
  Message(MessageTag tag);
  Message(size_t len);
  Message(size_t len, MessageTag tag);
  ~Message(void);

  // return a pointer to the content of this message
  char* content(void) const;

  // return the tag of this message
  MessageTag tag(void) const;

  // return the length of this message
  size_t length(void) const;

protected:
  // initialize the message
  void init_message(size_t len, MessageTag tag);

  char* msg;

private:
  // cast content to a struct message_header*
  struct message_header *rep(void) const;
};

// return a pointer to the header of this message
inline char* Message::content(void) const
{
  return msg;
}

// return the tag of this message
inline MessageTag Message::tag(void) const
{
  return rep()->tag;
}

// return the length of this message
inline size_t Message::length(void) const
{
  return rep()->len;
}

// cast content to a struct message_header*
inline struct message_header *Message::rep(void) const
{
  return (struct message_header *) msg;
}

#endif /* MESSAGE_H_ */
