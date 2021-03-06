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
#include <linux/mm.h>           /* about vma_struct */

#include "mem_wrapper.h"

#define DRIVER_AUTHOR "Pierre Louis Aublin <pierre-louis.aublin@inria.fr>"
#define DRIVER_DESC   "Kernel module of the ZIMP communication mechanism"

// even if all the machines do not necessarily have lines of 64B, we don't really care
#define CACHE_LINE_SIZE 64

// Used to define the size of the pad member
// The last modulo is to prevent the padding to add CACHE_LINE_SIZE bytes to the structure
#define PADDING_SIZE(S) ((CACHE_LINE_SIZE - ((S) % CACHE_LINE_SIZE)) % CACHE_LINE_SIZE)

// round up to a page size
#define ROUND_UP_PAGE_SIZE(S) ((S) + ((PAGE_SIZE - ((S) % PAGE_SIZE)) % PAGE_SIZE))

// IOCTL commands
#define KZIMP_IOCTL_SPLICE_WRITE 0x1

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
MODULE_PARM_DESC(default_compute_checksum, " By default, for the new channels, do we compute the checksum on messages. If 0 then no; if 1 then yes; if 2 then on header only");

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
static int kzimp_mmap(struct file *, struct vm_area_struct *);
static long kzimp_ioctl(struct file *, unsigned int, unsigned long);

// an open file is associated with a set of functions
static struct file_operations kzimp_fops =
{
    .owner = THIS_MODULE,
    .open = kzimp_open,
    .release = kzimp_release,
    .read = kzimp_read,
    .write = kzimp_write,
    .poll = kzimp_poll,
    .mmap = kzimp_mmap,
    .unlocked_ioctl = kzimp_ioctl,
};


// VMA OPERATIONS
static int kzimp_vma_fault(struct vm_area_struct *, struct vm_fault *);

// operations for mmap on the vmas
static struct vm_operations_struct kzimp_vm_ops = {
    .fault = kzimp_vma_fault,
};

#define KZIMP_HEADER_SIZE (sizeof(unsigned long)+sizeof(int)+sizeof(short))

// what is a message
// it must be packed so that we can compute the checksum
struct kzimp_message
{
  unsigned long bitmap;       /* the bitmap, alone on  */
  int len;                    /* length of the message */
  short checksum;             /* the checksum */

  short __p1;                 /* to align properly the char* */

  char *data;                 /* the message content */
  char *big_msg_data;         /* the message content, for a big message */
  struct task_struct *writer; /* task_struct of the writer */
  struct vm_area_struct *vma; /* pointer to the vma of the writer */
  atomic_t waking_up_writer;  /* 1 if a process is currently waking up the writers, 0 otherwise */

  // padding (to avoid false sharing)
  char __p2[PADDING_SIZE(KZIMP_HEADER_SIZE + sizeof(short) + sizeof(char*)*2 + sizeof(struct task_struct *) + sizeof(struct vm_area_struct *) + sizeof(atomic_t))];
}__attribute__((__packed__, __aligned__(CACHE_LINE_SIZE)));

// kzimp communication channel
struct kzimp_comm_chan
{
  int channel_size;                 /* max number of messages in the channel */
  int compute_checksum;             /* do we compute the checksum? 0: no, 1: yes, 2: partial */
  unsigned long multicast_mask;     /* the multicast mask, used for the bitmap */
  struct kzimp_message* msgs;       /* the messages of the channel */
  char *messages_area;              /* pointer to the big allocated area of messages */
  long timeout_in_ms;               /* writer's timeout in miliseconds */
  wait_queue_head_t rq, wq;         /* the wait queues */

  // these variables are used by the writers only.
  int next_write_idx;               /* position of the next written message */
  spinlock_t bcl;                   /* the Big Channel Lock :) */

  struct list_head writers_big_msg; /* List of pointers to the writers' big messages areas */
  int max_msg_size;                 /* max message size */
  int max_msg_size_page_rounded;    /* max message size, rounded up to a multiple of the page size */
  int nb_readers;                   /* number of readers */
  struct list_head readers;         /* List of pointers to the readers' control structure */
  int chan_id;                      /* id of this channel */
  struct cdev cdev;                 /* char device structure */
}__attribute__((__aligned__(CACHE_LINE_SIZE)));

// Each process that uses the channel to read has a control structure.
struct kzimp_ctrl
{
  int next_read_idx;               /* index of the next read in the channel */
  int bitmap_bit;                  /* position of the bit in the multicast mask modified by this reader */
  pid_t pid;                       /* pid of this reader */
  int online;                      /* is this reader still active or not? */
  char *big_msg_area;              /* pointer to the big area that will be mmapped, for big messages */
  size_t big_msg_area_len;         /* length of the big messages area */
  struct list_head next;           /* pointer to the next reader on this channel */
  struct kzimp_comm_chan *channel; /* pointer to the channel */
}__attribute__((__aligned__(CACHE_LINE_SIZE)));

// element that contains an address and a pointer to the next element.
// It is used by the channel to store the list of memory areas allocated by the writers,
// to free them when removing the channel
struct big_mem_area_elt {
  char *addr;
  size_t len;
  struct list_head next;
};

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
