/* Credits:
 *  -The basic lines come from Linux Device Drivers 3rd edition
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
#include <linux/mm.h>           /* about vma_struct */

#define DRIVER_AUTHOR "Pierre Louis Aublin <pierre-louis.aublin@inria.fr>"
#define DRIVER_DESC   "Kernel module which offers protected memory for the Barrelfish Message Passing (UMP) communication mechanism"

// even if all the machines do not necessarily have lines of 64B, we don't really care
#define CACHE_LINE_SIZE 64

// Used to define the size of the pad member
// The last modulo is to prevent the padding to add CACHE_LINE_SIZE bytes to the structure
#define PADDING_SIZE(S) ((CACHE_LINE_SIZE - S % CACHE_LINE_SIZE) % CACHE_LINE_SIZE)

// Round up S to a multiple of the page size
#define ROUND_UP_SIZE(S) (S + (PAGE_SIZE - S % PAGE_SIZE) % PAGE_SIZE)

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
#define DEVICE_NAME "kbfishmem"

/* file /proc/<procfs_name> */
#define procfs_name "kbfishmem"

// FILE OPERATIONS
static int kbfishmem_open(struct inode *, struct file *);
static int kbfishmem_release(struct inode *, struct file *);
static int kbfishmem_mmap(struct file *, struct vm_area_struct *);

// an open file is associated with a set of functions
static struct file_operations kbfishmem_fops =
{
    .owner = THIS_MODULE,
    .open = kbfishmem_open,
    .release = kbfishmem_release,
    .mmap = kbfishmem_mmap,
};


// VMA OPERATIONS
static int kbfishmem_vma_fault(struct vm_area_struct *, struct vm_fault *);

// operations for mmap on the vmas
static struct vm_operations_struct kbfishmem_vm_ops = {
    .fault = kbfishmem_vma_fault,
};


// what is a channel (for this module)
struct kbfishmem_channel {
  int chan_id;                 /* id of this channel */
  pid_t sender;                /* pid of the sender */
  pid_t receiver;              /* pid of the receiver */
  int channel_size;            /* max number of messages */
  unsigned long size_in_bytes; /* channel size in bytes. Is a multiple of the page size */
  int max_msg_size;            /* max message size */;
  spinlock_t bcl;              /* the Big Channel Lock :) */
  char* sender_to_receiver;    /* shared area used by the sender to send messages */
  char* receiver_to_sender;    /* shared area used by the receiver to send messages */

  struct cdev cdev;            /* char device structure */
};

// Each process has a control structure.
struct kbfishmem_ctrl {
  pid_t pid;                   /* pid of this structure's owner */
  struct kbfishmem_channel *chan; /* pointer to the channel */
  int is_sender;               /* is this process a sender? */
};


MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

#endif
