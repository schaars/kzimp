/**
 * Our implementation of Barrelfish URPC on Linux
 * The code comes from the 2nd release of Barrelfish, 2009
 *
 * Pierre Louis Aublin <pierre-louis.aublin@inria.fr>
 * January 2011
 */

/** \file
 * \brief
 */

/*
 * Copyright (c) 2008, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "urpc_transport.h"
#include "urpc.h"

// Debug macro
#define URPC_TRANSPORT_DEBUG
#undef URPC_TRANSPORT_DEBUG

#define SEQ_ID_MASK ((1UL<<URPC_TYPE_SIZE)-1)

#define MIN(a, b) ( (a) < (b) ? (a) : (b) )

// initialize the urpc_connection structure.
// If called the first time, create must be true: the other end of the connection
// will call this method with false (but with the same piece of shared memory)
void urpc_transport_create(int mon_id, void *buf, size_t buffer_size,
    size_t channel_length, struct urpc_connection *c, bool create)
{
  assert(channel_length * 2 + 2 * URPC_CHANNEL_SIZE <= buffer_size);

  /* initialise our local instance data */
  c->sent_id = c->ack_id = c->seq_id = 0;

  c->monitor_id = mon_id;

  size_t tmp_max_msgs = channel_length / (URPC_MSG_WORDS * sizeof(uint64_t));
  assert(tmp_max_msgs < (1UL << URPC_TYPE_SIZE));
  assert(tmp_max_msgs > 0);
  c->max_msgs = tmp_max_msgs;

  char *buf_as_char_ptr = (char*) buf;

#ifdef URPC_TRANSPORT_DEBUG
  printf("[urpc_transport_create][%u] create=%s, @buf=%p\n",
      (unsigned int) c->monitor_id, (create ? "true" : "false"),
      buf_as_char_ptr);
#endif

  if (create)
  {
    c->in = (struct urpc_channel*) buf_as_char_ptr;
    buf_as_char_ptr += 2 * URPC_CHANNEL_SIZE;
    urpc_new(c->in, buf_as_char_ptr, channel_length, URPC_INCOMING);
    buf_as_char_ptr += channel_length;
    urpc_new(&(c->out), buf_as_char_ptr, channel_length, URPC_OUTGOING);
  }
  else
  {
    buf_as_char_ptr += URPC_CHANNEL_SIZE;
    c->in = (struct urpc_channel*) buf_as_char_ptr;
    buf_as_char_ptr += URPC_CHANNEL_SIZE;
    urpc_new(&(c->out), buf_as_char_ptr, channel_length, URPC_OUTGOING);
    buf_as_char_ptr += channel_length;
    urpc_new(c->in, buf_as_char_ptr, channel_length, URPC_INCOMING);
  }

#ifdef URPC_TRANSPORT_DEBUG
  printf(
      "[urpc_transport_create][%u] create=%s, @buf=%p, @c->in=%p, @c.out=%p, @c->in->buf=%p, @(c->out).buf=%p\n",
      (unsigned int) c->monitor_id, (create ? "true" : "false"),
      buf_as_char_ptr, c->in, &c->out, c->in->buf, (c->out).buf);
#endif
}

static bool cansend(struct urpc_connection *c)
{
  return (urpc_t) (c->sent_id - c->ack_id) <= c->max_msgs;
}

// send a message
// return true if the sending has succeeded, false otherwise.
// For now, busy waiting
bool urpc_transport_send(struct urpc_connection *c, void *msg, size_t msg_len)
{
  uint64_t* msg_as_uint64_t = (uint64_t*) msg;

  while (!cansend(c))
  {
#ifdef URPC_TRANSPORT_DEBUG
    //printf("[urpc_transport_send][%u] Trying again\n",
    //    (unsigned int) c->monitor_id);
    fflush(NULL);
#endif

    // sleep
#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }

#ifdef URPC_TRANSPORT_DEBUG
  printf("[%u] Sending a message with seq_id=%u, ack_id=%u and sent_id=%u\n",
      (unsigned int) c->monitor_id, (unsigned int) c->seq_id,
      (unsigned int) c->ack_id, (unsigned int) c->sent_id);
#endif

  msg_as_uint64_t[URPC_PAYLOAD_WORDS]
      = ((uint64_t) c->seq_id << URPC_TYPE_SIZE) | c->sent_id;
  c->sent_id++;
  urpc_send_abstract(&c->out, msg_as_uint64_t);

  return true;
}

// Get the message that is available.
size_t get_the_message(struct urpc_connection *c, uint64_t *msg)
{
  c->ack_id = (msg[URPC_PAYLOAD_WORDS] >> URPC_TYPE_SIZE) & SEQ_ID_MASK;
  c->seq_id = msg[URPC_PAYLOAD_WORDS] & SEQ_ID_MASK;

#ifdef URPC_TRANSPORT_DEBUG
  printf("[%u] Receiving a message with seq_id=%u, ack_id=%i and sent_id=%u\n",
      (unsigned int) c->monitor_id, (unsigned int) c->seq_id,
      (unsigned int) c->ack_id, (unsigned int) c->sent_id);
#endif

  return URPC_MSG_WORDS;
}

// receive a message
// Return the length of the read message (in number of uint64_t) or 0 if there is no message
size_t urpc_transport_recv_nonblocking(struct urpc_connection *c, void *msg,
    size_t msg_len)
{
  uint64_t* msg_as_uint64_t = (uint64_t*) msg;

  if (urpc_poll_abstract(c->in, msg_as_uint64_t))
  {
    // there is a message
    return get_the_message(c, msg_as_uint64_t);
  }
  else
  {
    // there is no message
    return 0;
  }
}

// receive a message.
// Return the length of the read message
// busy waiting
size_t urpc_transport_recv(struct urpc_connection *c, void *msg, size_t msg_len)
{
  uint64_t* msg_as_uint64_t = (uint64_t*) msg;

  while (!urpc_poll_abstract(c->in, msg_as_uint64_t))
  {
#ifdef URPC_TRANSPORT_DEBUG
    //printf("[urpc_transport_recv][%u] Trying again\n",
    //    (unsigned int) c->monitor_id);
#endif

    // sleep
#ifdef USLEEP
    usleep(1);
#endif
#ifdef NOP
    __asm__ __volatile__("nop");
#endif
  }

  // we have received a message in msg_as_uint64_t
  return get_the_message(c, msg_as_uint64_t);
}
