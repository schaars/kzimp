/*
 * Message.h
 *
 * What is a message
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include "MessageTag.h"

const int Max_message_size = 9000;

struct message_header
{
  MessageTag tag;
  int len; // total length of the message, including this header
};

class Message
{
public:
  Message(int len = Max_message_size, MessageTag tag = UNKNOWN);
  ~Message();

  // return the tag of this message
  MessageTag tag();

  // return the length of this message
  int length();

  // return a pointer to the header of this message
  struct message_header* header();

  // return a pointer to the content of this message
  char* content();

private:
  struct message_header* msg;
};

// return the tag of this message
inline MessageTag Message::tag()
{
  return msg->tag;
}

// return the length of this message
inline int Message::length()
{
  return msg->len;
}

// return a pointer to the header of this message
inline struct message_header* Message::header()
{
  return msg;
}

// return a pointer to the content of this message
inline char* Message::content()
{
  return (char*) (msg + sizeof(struct message_header));
}

#endif /* MESSAGE_H_ */
