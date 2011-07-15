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

  ump_index_t max_send_msgs; ///< Number of messages that fit in the send channel
  ump_index_t max_recv_msgs; ///< Number of messages that fit in the recv channel

  uintptr_t sendid; ///< id for tracing
  uintptr_t recvid; ///< id for tracing

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
 * Return the size of the received message.
 */
int recv_msg(struct ump_channel *chan, char *msg, size_t len);

/*
 * receive a message and place it in msg of size len.
 * Is not blocking.
 * Return the size of the received message or 0 if there is no message
 */
int recv_msg_nonblocking(struct ump_channel *chan, char *msg, size_t len);

// select on n channels that can be found in chans.
// Note that you give an array of channel pointers.
// Return NULL if there is no message, or a pointer to a channel on which a message is available.
struct ump_channel* select(struct ump_channel** chans, int n);

/********************* inline "private" methods *********************/
//todo if needed

#endif
