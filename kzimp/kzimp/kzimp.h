/* Credits:
 *  -The basic lines come from Linux Device Drivers 3rd edition
 *  -some code from kl-shcom (linux_kernel_local_multicast) and zimp (in user-space)
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
#include <linux/spinlock.h>     /* spinlock */
#include <linux/wait.h>         /* wait queues */
#include <linux/list.h>         /* linked list */
#include <linux/poll.h>         /* poll_table structure */

#include "mem_wrapper.h"

#define DRIVER_AUTHOR "Pierre Louis Aublin <pierre-louis.aublin@inria.fr>"
#define DRIVER_DESC   "Kernel module of the ZIMP communication mechanism"

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

static long default_timeout_in_ms = 5000;
module_param(default_timeout_in_ms, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(default_timeout_in_ms, " The default timeout (in miliseconds) of the new channels.");

static int default_compute_checksum = 1;
module_param(default_compute_checksum, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(default_compute_checksum, " By default, for the new channels, do we compute the checksum on messages.");

// file /dev/<DEVICE_NAME>
#define DEVICE_NAME "kzimp"

/* file /proc/<procfs_name> */
#define procfs_name "kzimp"

// FILE OPERATIONS
static int kzimp_open(struct inode *, struct file *);
static int kzimp_release(struct inode *, struct file *);
static ssize_t kzimp_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t kzimp_write(struct file *, const char __user *, size_t, loff_t *);
static unsigned int kzimp_poll(struct file *filp, poll_table *wait);

// an open file is associated with a set of functions
static struct file_operations kzimp_fops =
{
   .owner = THIS_MODULE,
   .open = kzimp_open,
   .release = kzimp_release,
   .read = kzimp_read,
   .write = kzimp_write,
   .poll = kzimp_poll,
};

#define KZIMP_HEADER_SIZE (sizeof(unsigned long)+sizeof(int)+sizeof(short))

// what is a message
// it must be packed so that we can compute the checksum
struct kzimp_message
{
   unsigned long bitmap; /* the bitmap, alone on  */
   int len;              /* length of the message */
   short checksum;       /* the checksum */
   char *data;           /* the message content */

   // padding (to avoid false sharing)
   char __p[PADDING_SIZE(KZIMP_HEADER_SIZE + sizeof(char*))];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

#define KZIMP_COMM_CHAN_SIZE1 (sizeof(int)+sizeof(int)+sizeof(long)+sizeof(unsigned long)+sizeof(wait_queue_head_t)*2+sizeof(struct kzimp_message*))
#define KZIMP_COMM_CHAN_SIZE2 (sizeof(int))
#define KZIMP_COMM_CHAN_SIZE3 (sizeof(spinlock_t))
#define KZIMP_COMM_CHAN_SIZE4 (sizeof(struct list_head)+sizeof(int)+sizeof(int)+sizeof(int)+sizeof(struct cdev))

// kzimp communication channel
struct kzimp_comm_chan
{
  int channel_size;                 /* max number of messages in the channel */
  int compute_checksum;             /* do we compute the checksum? */
  long timeout_in_ms;               /* writer's timeout in miliseconds */
  unsigned long multicast_mask;     /* the multicast mask, used for the bitmap */
  wait_queue_head_t rq, wq;         /* the wait queues */
  struct kzimp_message* msgs;       /* the messages of the channel */

  char __p1[PADDING_SIZE(KZIMP_COMM_CHAN_SIZE1)];

  // these variables are used by the readers only.
  // We use padding to ensure they do not cause cache misses at the readers
  // And also to ensure there are no cache misses when the writer changes next_write_idx but not bcl (or the contrary)
  int next_write_idx;               /* position of the next written message */
  char __p2[PADDING_SIZE(KZIMP_COMM_CHAN_SIZE2)];
  spinlock_t bcl;                   /* the Big Channel Lock :) */

  char __p3[PADDING_SIZE(KZIMP_COMM_CHAN_SIZE3)];

  struct list_head readers;         /* List of pointers to the readers' control structure */
  int chan_id;                      /* id of this channel */
  int max_msg_size;                 /* max message size */
  int nb_readers;                   /* number of readers */
  struct cdev cdev;                 /* char device structure */

  char __p4[PADDING_SIZE(KZIMP_COMM_CHAN_SIZE4)];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

// Each process that uses the channel to read has a control structure.
struct kzimp_ctrl
{
   int next_read_idx;      /* index of the next read in the channel */
   int bitmap_bit;         /* position of the bit in the multicast mask modified by this reader */
   pid_t pid;              /* pid of this reader */
   int online;             /* is this reader still active or not? */
   struct list_head next;  /* pointer to the next reader on this channel */
}__attribute__((__aligned__(CACHE_LINE_SIZE)));

// This structure pointer is found at filp->private_data.
// It allows a process to have local data + access to the global channel
struct kzimp_file_private_data
{
   struct kzimp_comm_chan *channel;
   struct kzimp_ctrl *ctrl;
}__attribute__((__aligned__(CACHE_LINE_SIZE)));

// return 1 if the writer can writeits message, 0 otherwise
static inline int writer_can_write(unsigned long bitmap)
{
   return (bitmap == 0);
}

// return 1 if the reader has a message to read, 0 otherwise
static inline int reader_can_read(unsigned long bitmap, int bit)
{
   return (bitmap & (1 << bit));
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

#endif