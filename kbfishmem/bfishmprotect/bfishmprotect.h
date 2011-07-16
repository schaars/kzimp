/* Barrelfish communication mechanism
 * Uses the module kbfishmem, which offers the required level of memory protection
 */

#ifndef BFISH_MEM_PROTECT
#define BFISH_MEM_PROTECT

#include <stdint.h>

/********************* types & structures *********************/

// Define MESSAGE_BYTES as the size of a message in bytes at compile time.
//FIXME: remove this define since it is set at compile time
#define MESSAGE_BYTES 64

#define CACHELINE_BYTES 64
#define MESSAGE_BYTES_PLUS_ONE (MESSAGE_BYTES + sizeof(uintptr_t))

#define UMP_PAYLOAD_WORDS  (CACHELINE_BYTES / sizeof(uintptr_t) - 1)
#define UMP_MSG_WORDS      (UMP_PAYLOAD_WORDS + 1)
#define UMP_MSG_BYTES      (UMP_MSG_WORDS * sizeof(uintptr_t))

/// Default size of a unidirectional UMP message buffer, in bytes
#define DEFAULT_UMP_BUFLEN  (BASE_PAGE_SIZE / 2 / UMP_MSG_BYTES * UMP_MSG_BYTES)

// control word is 32-bit, because it must be possible to atomically write it
typedef uint32_t ump_control_t;
#define UMP_EPOCH_BITS  1
#define UMP_HEADER_BITS 31

struct ump_control
{
  ump_control_t epoch :UMP_EPOCH_BITS;
  ump_control_t header :UMP_HEADER_BITS;
};

struct ump_message
{
  uintptr_t data[UMP_PAYLOAD_WORDS] __attribute__((aligned (CACHELINE_BYTES)));
  union
  {
    struct ump_control control;
    uintptr_t raw;
  } header;
};

/// Type used for indices of UMP message slots
typedef uint16_t ump_index_t;
#define NBBY         (8)
#define UMP_INDEX_BITS         (sizeof(ump_index_t) * NBBY)
#define UMP_INDEX_MASK         ((((uintptr_t)1) << UMP_INDEX_BITS) - 1)

/**
 * UMP direction
 */
enum ump_direction
{
  UMP_OUTGOING, UMP_INCOMING
};

/**
 * \brief State of a (one-way) UMP channel
 */
struct ump_chan_state
{
  struct ump_message *buf; ///< Ring buffer
  ump_index_t pos; ///< Current position
  ump_index_t bufmsgs; ///< Buffer size in messages
  int epoch; ///< Next Message epoch
  enum ump_direction dir; ///< Channel direction
};

/// Round up n to the next multiple of size
#define ROUND_UP(n, size)           ((((n) + (size) - 1)) & (~((size) - 1)))

/// Cache-aligned size of a #ump_chan_state struct
#define UMP_CHAN_STATE_SIZE ROUND_UP(sizeof(struct ump_chan_state), CACHELINE_BYTES)

struct ump_channel
{
  struct ump_chan_state send_chan; ///< Outgoing UMP channel state
  struct ump_chan_state recv_chan; ///< Incoming UMP channel state

  ump_index_t sent_id; ///< Sequence number of next message to be sent
  ump_index_t seq_id; ///< Last sequence number received from remote
  ump_index_t ack_id; ///< Last sequence number acknowledged by remote
  ump_index_t last_ack; ///< Last acknowledgement we sent to remote

  ump_index_t max_send_msgs; ///< Number of messages that fit in the send channel
  ump_index_t max_recv_msgs; ///< Number of messages that fit in the recv channel

  int fd; // file descriptor associated with the protected memory areas

  size_t inchanlen, outchanlen;
};

/********************* exported interface *********************/
/*
 * open a channel using the special file mprotectfile for memory protection.
 * The channel size is (for both direction) nb_messages * message_size.
 * is_receiver must be set to 1 if called by the receiver end, 0 otherwise.
 * Return the new channel if ok.
 */
struct ump_channel open_channel(char *mprotectfile, int nb_messages,
    int message_size, int is_receiver);

/*
 * close the channel whose structure is *chan.
 */
void close_channel(struct ump_channel *chan);

/*
 * send the message msg of size len through the channel *chan.
 * Is blocking.
 * Return the size of the sent message or an error code.
 */
int send_msg(struct ump_channel *chan, char *msg, size_t len);

/*
 * receive a message and place it in msg of size len.
 * Is blocking.
 * Return the size of the received message or -1 if an error has occured (cannot send the ack)
 */
int recv_msg(struct ump_channel *chan, char *msg, size_t len);

/*
 * receive a message and place it in msg of size len.
 * Is not blocking.
 * Return the size of the received message or 0 if there is no message or -1 if an error has occured (cannot send the ack)
 */
int recv_msg_nonblocking(struct ump_channel *chan, char *msg, size_t len);

// select on n channels that can be found in chans.
// Note that you give an array of channel pointers.
// Return NULL if there is no message, or a pointer to a channel on which a message is available.
struct ump_channel* bfish_mprotect_select(struct ump_channel** chans, int n);

/********************* inline "private" methods *********************/
/**
 * \brief Determine next position for an outgoing message on a channel, and
 *   advance send pointer.
 *
 * \param c     Pointer to UMP channel-state structure.
 * \param ctrl  Pointer to storage for control word for next message, to be filled in
 *
 * \return Pointer to message if outstanding, or NULL.
 */
static inline struct ump_message *ump_impl_get_next(struct ump_chan_state *c,
    struct ump_control *ctrl)
{
  // construct header
  ctrl->epoch = c->epoch;

  struct ump_message *msg = &c->buf[c->pos];

  // update pos
  if (++c->pos == c->bufmsgs)
  {
    c->pos = 0;
    c->epoch = !c->epoch;
  }

  return msg;
}

/// Prepare a "control" word (header for each UMP message fragment)
static inline void ump_control_fill(struct ump_channel *s,
    struct ump_control *ctrl, int msgtype)
{
  ctrl->header = ((uintptr_t) msgtype << UMP_INDEX_BITS)
      | (uintptr_t) s->seq_id;
  s->last_ack = s->seq_id;
  s->sent_id++;
}

/// Process a "control" word
static inline int ump_control_process(struct ump_channel *s,
    struct ump_control ctrl)
{
  s->ack_id = ctrl.header & UMP_INDEX_MASK;
  s->seq_id++;
  return ctrl.header >> UMP_INDEX_BITS;
}

/// Computes (from seq/ack numbers) whether we can currently send on the channel
static inline int ump_can_send(struct ump_channel *s)
{
  return (ump_index_t) (s->sent_id - s->ack_id) < s->max_send_msgs;
}

// return 1 if an ack is needed (to be sent), 0 otherwise
static inline int ump_send_ack_is_needed(struct ump_channel *s)
{
  return (ump_index_t) (s->seq_id - s->last_ack) >= s->max_send_msgs - 1;
}

/**
 * \brief Return pointer to a message if outstanding on 'c'.
 *
 * \param c     Pointer to UMP channel-state structure.
 *
 * \return Pointer to message if outstanding, or NULL.
 */
static inline struct ump_message *ump_impl_poll(struct ump_chan_state *c)
{
  struct ump_control ctrl = c->buf[c->pos].header.control;
  if (ctrl.epoch == c->epoch)
  {
    return &c->buf[c->pos];
  }
  else
  {
    return NULL;
  }
}

/**
 * \brief Return pointer to a message if outstanding on 'c' and
 * advance pointer.
 *
 * \param c     Pointer to UMP channel-state structure.
 *
 * \return Pointer to message if outstanding, or NULL.
 */
static inline struct ump_message *ump_impl_recv(struct ump_chan_state *c)
{
  struct ump_message *msg = ump_impl_poll(c);

  if (msg != NULL)
  {
    if (++c->pos == c->bufmsgs)
    {
      c->pos = 0;
      c->epoch = !c->epoch;
    }
    return msg;
  }
  else
  {
    return NULL;
  }
}

/**
 * \brief Returns true iff there is a message pending on the given UMP channel
 */
static inline int ump_endpoint_can_recv(struct ump_chan_state *chan)
{
  return ump_impl_poll(chan) != NULL;
}

/**
 * \brief Retrieve a message from the given UMP channel, if possible
 *
 * Non-blocking, may fail if there are no messages available.
 *
 * Return 0 if there is a message, 1 otherwise
 */
static inline int ump_endpoint_recv(struct ump_chan_state *chan,
    volatile struct ump_message **msg)
{
  *msg = ump_impl_recv(chan);

  if (*msg != NULL)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

#endif
