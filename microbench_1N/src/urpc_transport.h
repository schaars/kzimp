/**
 * Our implementation of Barrelfish URPC on Linux
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

#ifndef URPC_TRANSPORT_H_
#define URPC_TRANSPORT_H_

#include "urpc.h"

/// PRIVATE data of URPC transport instance
struct urpc_connection
{
  struct urpc_channel *in, out;
  urpc_t sent_id, seq_id;
  //XXX volatile urpc_t ack_id;
  urpc_t ack_id;
  urpc_t max_msgs;
  size_t chanlength_bytes;
  uintptr_t monitor_id;
};

// initialize the urpc_connection structure.
// If called the first time, create must be true: the other end of the connection
// will call this method with false (but with the same piece shared memory)
void urpc_transport_create(int mon_id, void *buf, size_t buffer_size,
    size_t channel_length, struct urpc_connection *c, bool create);

// send a message
// return true if the sending has suceeded, false otherwise.
// For now, busy waiting
bool urpc_transport_send(struct urpc_connection *c, void *msg, size_t msg_len);

// receive a message
// Return the length of the read message or 0 if there is no message
// busy waiting (with a small sleep)
size_t urpc_transport_recv_nonblocking(struct urpc_connection *c, void *msg,
    size_t msg_len);

// receive a message
// Return the length of the read message
// busy waiting (with a small sleep)
size_t
    urpc_transport_recv(struct urpc_connection *c, void *msg, size_t msg_len);

static inline bool urpc_poll_abstract(struct urpc_channel *c, uint64_t *msg)
{
#ifdef OPTIMIZE_THROUGHPUT
  return urpc_poll_throughput(c, msg);
#else
  return urpc_poll(c, msg);
#endif
}

static inline bool urpc_send_abstract(struct urpc_channel *c, uint64_t *msg)
{
#ifdef OPTIMIZE_THROUGHPUT
  return urpc_send_throughput(c, msg);
#else
  return urpc_send(c, msg);
#endif

}

#endif /* URPC_TRANSPORT_H_ */
