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

#ifdef IPC_MSG_QUEUE
#include "comm_mech/ipc_interface.h"
#endif

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

#ifdef ULM
  Message(size_t len, MessageTag tag, int cid);
#endif

  ~Message(void);

  // return a pointer to the content of this message
#ifdef IPC_MSG_QUEUE
  struct ipc_message* content(void);
#else
  char* content(void) const;
#endif

  // return the length of this message
  size_t length(void) const;

  // return the tag of this message
  MessageTag tag(void) const;

#ifdef ULM
  // return the position of the message in the shared buffer of ULM
  int get_msg_pos(void);
#endif

protected:

#ifdef IPC_MSG_QUEUE
  struct ipc_message ipc_msg;
#endif

  char *msg;

#ifdef ULM
  int msg_pos_in_ring_buffer;
#endif

private:
  // cast content to a struct message*
  struct message_header *rep(void) const;

  // initialize the message
  // +ulm_alloc is considered only when using ULM. If set to true, then this message is allocated
  //  directly in shared memory.
  // +nid is relevant only when using ULM. If set to -1, then this message will be multicast,
  //  otherwise it is sent to node nid.
  void init_message(size_t len, MessageTag tag, bool ulm_alloc = false, int nid = -2);
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

#ifdef ULM
inline int Message::get_msg_pos(void)
{
  return msg_pos_in_ring_buffer;
}
#endif

#endif /* MESSAGE_H_ */
