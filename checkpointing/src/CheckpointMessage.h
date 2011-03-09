/*
 * CheckpointMessage.h
 *
 * What is a checkpoint message
 */

#ifndef CHECKPOINTMESSAGE_H_
#define CHECKPOINTMESSAGE_H_

#include <stdlib.h>
#include <stdint.h>

#ifdef IPC_MSG_QUEUE
#include "comm_mech/ipc_interface.h"
#endif

const int Max_message_size = 128;

struct checkpoint_message
{
  CheckpointMessageTag tag;
  size_t len; // total length of the message, including this header
}__attribute__((__packed__, __aligned__(64)));

class CheckpointMessage
{
public:
  CheckpointMessage(void);
  CheckpointMessage(CheckpointMessageTag tag);

#ifdef ULM
  CheckpointMessage(size_t len, CheckpointMessageTag tag, int cid);
#endif

  CheckpointMessage(size_t len, CheckpointMessageTag tag);
  ~CheckpointMessage(void);

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
  CheckpointMessageTag tag(void) const;

  // return the length of this message
  size_t length(void) const;

  // initialize the message
  void init_message(size_t len, CheckpointMessageTag tag, bool ulm_alloc = false, int cid = 0);

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
inline struct ipc_message* CheckpointMessage::content(void)
{
  return &ipc_msg;
}
#else
inline char* CheckpointMessage::content(void) const
{
  return msg;
}
#endif

// return the tag of this message
inline CheckpointMessageTag CheckpointMessage::tag(void) const
{
  return rep()->tag;
}

// return the length of this message
inline size_t CheckpointMessage::length(void) const
{
  return rep()->len;
}

// cast content to a struct message_header*
inline struct message_header *CheckpointMessage::rep(void) const
{
  return (struct message_header *) msg;
}

#ifdef ULM
inline int CheckpointMessage::get_msg_pos(void)
{
  return msg_pos_in_ring_buffer;
}
#endif

#endif /* CHECKPOINTMESSAGE_H_ */
