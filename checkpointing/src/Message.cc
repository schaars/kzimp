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
#include "comm_mech/ipc_interface.h"

// to get some debug printf
#define MSG_DEBUG
#undef MSG_DEBUG

Message::Message(void)
{
#ifdef KZIMP_READ_SPLICE
  init_message(Max_message_size, UNKNOWN, false, -2, 1);
#else
  init_message(Max_message_size, UNKNOWN);
#endif
}

Message::Message(MessageTag tag)
{
  init_message(Max_message_size, tag);
}

Message::Message(size_t len, MessageTag tag)
{
  init_message(len, tag);
}

#ifdef ULM
Message::Message(size_t len, MessageTag tag, int cid)
{
  init_message(len, tag, true, cid);
}
#endif

Message::~Message(void)
{
#ifdef KZIMP_READ_SPLICE

  if (!kzimp_reader_splice_do_not_free)
  {
    free(msg);
  }

#else

#ifndef IPC_MSG_QUEUE
#ifndef ULM
  free(msg);
#endif
#endif /* IPC_MSG_QUEUE */

#endif /* KZIMP_READ_SPLICE */
}

// initialize the message
// +ulm_alloc is considered only when using ULM. If set to true, then this message is allocated
//  directly in shared memory.
// +nid is relevant only when using ULM. If set to -1, then this message will be multicast,
//  otherwise it is sent to node nid.
void Message::init_message(size_t len, MessageTag tag, bool ulm_alloc, int nid,
    int doNotFree)
{
#if defined(IPC_MSG_QUEUE)

  msg = ipc_msg.mtext;

#elif defined(ULM)

  if (ulm_alloc)
  {
    msg = (char*) IPC_ulm_alloc(len, &msg_pos_in_ring_buffer, nid);
  }
  else
  {
    msg = (char*) malloc(len);
    if (!msg)
    {
      perror("Message creation failed! ");
      exit(errno);
    }

    msg_pos_in_ring_buffer = -1;
  }

#elif defined(KZIMP_READ_SPLICE)

  kzimp_reader_splice_do_not_free = doNotFree;
  if (!kzimp_reader_splice_do_not_free)
  {
    msg = (char*) malloc(len);
    if (!msg)
    {
      perror("Message creation failed! ");
      exit(errno);
    }
  }

#else

  msg = (char*) malloc(len);
  if (!msg)
  {
    perror("Message creation failed! ");
    exit(errno);
  }

#endif

#ifdef KZIMP_READ_SPLICE
  if (!kzimp_reader_splice_do_not_free)
  {
#endif
    rep()->tag = tag;
    rep()->len = len;
#ifdef KZIMP_READ_SPLICE
  }
#endif

#ifdef MSG_DEBUG
  printf("Creating a new Message %i of size %lu\n", rep()->tag, rep()->len);
#endif
}
