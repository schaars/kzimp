/* Credits:
 *  -The basic lines come from Linux Device Drivers 3rd edition
 *  -Parts related to the mechanism come from Barrelfish OS.
 *
 *  Developed for an x86-64: we allocate all the memory in ZONE_NORMAL.
 *  On a x86 machine, ZONE_NORMAL is up to 896MB. On a x86-64 machine, ZONE_NORMAL
 *  represents the whole memory.
 */

#ifndef _KZIMP_MODULE_
#define _KZIMP_MODULE_

#include <linux/module.h>       /* Needed by all modules */
#include <linux/moduleparam.h>  /* This module takes arguments */
#include <linux/fs.h>           /* (un)register the block device - file operations */
#include <linux/cdev.h>         /* char device */
#include <linux/poll.h>         /* poll_table structure */

#define DRIVER_AUTHOR "Pierre Louis Aublin <pierre-louis.aublin@inria.fr>"
#define DRIVER_DESC   "Kernel module of the Barrelfish Message Passing (UMP) communication mechanism"

// even if all the machines do not necessarily have lines of 64B, we don't really care
#define CACHE_LINE_SIZE 64

// Used to define the size of the pad member
// The last modulo is to prevent the padding to add CACHE_LINE_SIZE bytes to the structure
#define PADDING_SIZE(S) ((CACHE_LINE_SIZE - S % CACHE_LINE_SIZE) % CACHE_LINE_SIZE)

// This module takes the following arguments:
static int nb_max_communication_channels = 4;
module_param(nb_max_communication_channels, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(nb_max_communication_channels, " The max number of communication channels.");

static int default_channel_size = 10;
module_param(default_channel_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(default_channel_size, " The default size of the new channels.");

static int default_max_msg_size = 1024;
module_param(default_max_msg_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(default_max_msg_size, " The default max size of the new channels messages.");

// file /dev/<DEVICE_NAME>
#define DEVICE_NAME "kbfish"

/* file /proc/<procfs_name> */
#define procfs_name "kbfish"

// FILE OPERATIONS
static int kbfish_open(struct inode *, struct file *);
static int kbfish_release(struct inode *, struct file *);
static ssize_t kbfish_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t kbfish_write(struct file *, const char __user *, size_t, loff_t *);
static unsigned int kbfish_poll(struct file *, poll_table *);

// an open file is associated with a set of functions
static struct file_operations kbfish_fops =
{
    .owner = THIS_MODULE,
    .open = kbfish_open,
    .release = kbfish_release,
    .read = kbfish_read,
    .write = kbfish_write,
    .poll = kbfish_poll,
};

// Note that you need to define MESSAGE_BYTES at compile time
#define UMP_PAYLOAD_WORDS  (MESSAGE_BYTES / sizeof(uintptr_t))

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
  uintptr_t data[UMP_PAYLOAD_WORDS] __attribute__((aligned (CACHE_LINE_SIZE)));
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

#define BARRIER()   __asm volatile ("" : : : "memory")

/// Special message types
enum ump_msgtype
{
  UMP_MSG = 0, UMP_ACK = 1,
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

// what is a channel (for the communication mechanism)
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

  size_t inchanlen, outchanlen;
};

// what is a channel (for this module)
struct kbfish_channel
{
  int chan_id; /* id of this channel */
  pid_t sender; /* pid of the sender */
  pid_t receiver; /* pid of the receiver */
  int channel_size; /* max number of messages */
  unsigned long size_in_bytes; /* channel size in bytes. Is a multiple of the page size */
  int max_msg_size; /* max message size */
  spinlock_t bcl; /* the Big Channel Lock :) */
  wait_queue_head_t rq, wq; /* the wait queues */
  char* sender_to_receiver; /* shared area used by the sender to send messages */
  char* receiver_to_sender; /* shared area used by the receiver to send messages */

  struct cdev cdev; /* char device structure */
};

// Each process has a control structure.
struct kbfish_ctrl
{
  pid_t pid; /* pid of this structure's owner */
  struct kbfish_channel *chan; /* pointer to the kernel channel */
  struct ump_channel *ump_chan; /* pointer to the UMP channel */
  int is_sender; /* is this process a sender? */
};

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
  struct ump_message *msg;

  // construct header
  ctrl->epoch = c->epoch;

  msg = &c->buf[c->pos];

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

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

#endif
