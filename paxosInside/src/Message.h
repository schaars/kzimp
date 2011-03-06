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

#ifdef IPC_MSG_QUEUE
#include "comm_mech/ipc_interface.h"
#endif

const int Max_message_size = 128;

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
  Message(size_t len, MessageTag tag);
  ~Message(void);

  // return a pointer to the content of this message
#ifdef IPC_MSG_QUEUE
  struct ipc_message* content(void);
#else
  char* content(void) const;
#endif

#ifdef ULM
  // return the position of the message in the shared buffer of ULM
  int get_msg_pos(void);
#endif

  // return the tag of this message
  MessageTag tag(void) const;

  // return the length of this message
  size_t length(void) const;

  // initialize the message
  void init_message(size_t len, MessageTag tag, bool ulm_alloc = false);

protected:

#ifdef IPC_MSG_QUEUE
  struct ipc_message ipc_msg;
#endif

  char* msg;

#ifdef ULM
  int msg_pos_in_ring_buffer;
#endif

private:
  // cast content to a struct message_header*
  struct message_header *rep(void) const;
};

// return a pointer to the header of this message
#ifdef IPC_MSG_QUEUE
inline struct ipc_message* Message::content(void)
{
  return &ipc_msg;
}
#else
inline char* Message::content(void) const
{
  return msg;
}
#endif

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

#ifdef ULM
inline int Message::get_msg_pos(void)
{
  return msg_pos_in_ring_buffer;
}
#endif

#endif /* MESSAGE_H_ */
