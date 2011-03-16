/**
 * Our implementation of Barrelfish URPC on Linux
 *
 * Pierre Louis Aublin <pierre-louis.aublin@inria.fr>
 * January 2011
 */

/**
 * \file
 * \brief User-Space Remote Procedure Call (URPC).
 *
 * URPC message size is fixed to cache-line size (64 bytes).
 */

/*
 * Copyright (c) 2007, 2008, 2009, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef URPC_H
#define URPC_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#define CACHE_LINE_SIZE 64

typedef uint16_t urpc_t;
#define URPC_TYPE_SIZE          (sizeof(urpc_t) * 8)
#define URPC_TYPE_REMAINDER     ((sizeof(uint64_t) * 8) - URPC_TYPE_SIZE)

// URPC_MSG_WORDS * sizeof(uint64_t) = size of a message, in bytes
// sizeof(uint64_t) = 8.
// Must be a multiple of the size of a cache line
// This is the macro we need to modify for having bigger messages
//#define URPC_MSG_WORDS      8
#define URPC_PAYLOAD_WORDS  (URPC_MSG_WORDS - 1)

/**
 * URPC channel type.
 */
enum urpc_type
{
  URPC_OUTGOING, URPC_INCOMING
};

/**
 * A URPC channel.
 */
struct urpc_channel
{
  uint64_t pos; ///< Current position
  uint64_t *buf; ///< Ring buffer
  size_t size; ///< Buffer size IN WORDS
  enum urpc_type type; ///< Channel type
  urpc_t epoch; ///< Next Message epoch
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

/// Round up n to the next multiple of size
#define ROUND_UP(n, size)           ((((n) + (size) - 1)) & (~((size) - 1)))

/// Cache-aligned size of a #urpc_channel struct
#define URPC_CHANNEL_SIZE \
    ROUND_UP(sizeof(struct urpc_channel), URPC_MSG_WORDS * sizeof(uintptr_t))

/**
 * \brief Initialize a URPC channel.
 *
 * The channel structure and buffer must already be allocated.
 *
 * \param       c       Pointer to channel structure to initialize.
 * \param       buf     Pointer to ring buffer for the channel.
 * \param       size    Size (in bytes) of buffer. Must be multiple of 64.
 * \param       type    Channel type.
 */
static inline void urpc_new(struct urpc_channel *c, void *buf, size_t size,
    enum urpc_type type)
{
  assert(size % (URPC_MSG_WORDS * sizeof(uint64_t)) == 0);
  assert((uintptr_t) buf % sizeof(uint64_t) == 0);
  //assert(size < 256 * URPC_MSG_WORDS * sizeof(uint64_t));

  c->pos = 0;
  c->buf = (uint64_t *) buf;
  c->type = type;
  c->size = size / sizeof(uintptr_t);
  c->epoch = 1;

  if (type == URPC_OUTGOING)
  {
    size_t i;
    for (i = URPC_MSG_WORDS - 1; i < size / URPC_MSG_WORDS; i += URPC_MSG_WORDS)
    {
      c->buf[i] = 0;
    }
  }
}

/**
 * \brief Return whether a message is outstanding on 'c'.
 *
 * \param c     Pointer to URPC message structure.
 *
 * \return true if message outstanding, false otherwise.
 */
static inline bool urpc_havemessage(struct urpc_channel *c, size_t msg_len)
{
  assert(c->type == URPC_INCOMING);

  return (c->buf[c->pos + msg_len - 1] >> URPC_TYPE_REMAINDER) == c->epoch;
}

/**
 * \brief Peek URPC channel.
 *
 * Peeks a msg on the channel, without consuming it.
 *
 * \param c     The URPC channel.
 * \param msg   Pointer to message, if any.
 *
 * \return true if message in 'msg', false if no message.
 */
static inline bool urpc_peek(struct urpc_channel *c, uint64_t *msg,
    size_t msg_len)
{
  assert(c->type == URPC_INCOMING);

  if (urpc_havemessage(c, msg_len))
  {
    memcpy(msg, (void*) &(c->buf[c->pos]), msg_len * sizeof(uint64_t));

    return true;
  }
  else
  {
    return false;
  }
}

/**
 * \brief Consume a msg from the URPC channel.
 *
 * Consumes a msg from the channel, discarding it.
 *
 * \param c     The URPC channel.
 */
static inline void urpc_consume(struct urpc_channel *c, size_t msg_len)
{
  assert(c->type == URPC_INCOMING);
  assert(urpc_havemessage(c, msg_len));

  c->pos = (c->pos + URPC_MSG_WORDS) % c->size;
  c->epoch++;
}

/**
 * \brief Poll URPC channel. Optimized for latency
 *
 * \param c     The URPC channel.
 * \param msg   Pointer to message, if any.
 *
 * \return true if message in 'msg', false if no message.
 */
static inline bool urpc_poll(struct urpc_channel *c, uint64_t *msg,
    size_t msg_len)
{
  if (urpc_peek(c, msg, msg_len))
  {
    urpc_consume(c, msg_len);
    return true;
  }
  return false;
}

/**
 * \brief Poll URPC channel. Optimized for throughput
 *
 * \param c     The URPC channel.
 * \param msg   Pointer to message, if any.
 *
 * \return true if message in 'msg', false if no message.
 */
static inline bool urpc_poll_throughput(struct urpc_channel *c, uint64_t *msg,
    size_t msg_len)
{
  if (urpc_peek(c, msg, msg_len))
  {
    urpc_consume(c, msg_len);
    __asm volatile("prefetch %[addr]" :: [addr] "m" (c->buf[c->pos]));
    return true;
  }
  return false;
}

/**
 * \brief Try to send a message on URPC channel. Optimized for latency
 *
 * \param c     The URPC channel.
 * \param msg   Pointer to message to send.
 *
 * \return true if message sent, false if buffer full.
 */
static inline bool urpc_send(struct urpc_channel *c, uint64_t *msg,
    size_t msg_len)
{
  assert(c->type == URPC_OUTGOING);

  memcpy((void*) &(c->buf[c->pos]), msg, msg_len * sizeof(uint64_t));

#ifndef __ARMEL__
  c->buf[c->pos + msg_len - 1] = (msg[msg_len - 1] & ((((uint64_t) 1)
      << URPC_TYPE_REMAINDER) - 1)) | ((uint64_t) c->epoch
      << URPC_TYPE_REMAINDER);
#endif // __ARMEL__
  c->pos = (c->pos + URPC_MSG_WORDS) % c->size;
  c->epoch++;

  return true;
}

/**
 * \brief Try to send a message on URPC channel. Optimized for throughput.
 *
 * \param c     The URPC channel.
 * \param msg   Pointer to message to send.
 *
 * \return true if message sent, false if buffer full.
 */
static inline bool urpc_send_throughput(struct urpc_channel *c, uint64_t *msg,
    size_t msg_len)
{
  urpc_send(c, msg, msg_len);
  __asm volatile("prefetchw %[addr]" :: [addr] "m" (c->buf[c->pos]));

  return true;
}

#endif
