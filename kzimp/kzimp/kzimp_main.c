/* kzimp - character device module */

#include <linux/kernel.h>      /* Needed for KERN_INFO */
#include <linux/init.h>        /* Needed for the macros */
#include <linux/proc_fs.h>     /* proc fs */
#include <asm/uaccess.h>       /* copy_from_user function */
#include <linux/sched.h>       /* current macro */
#include <linux/fcntl.h>       /* some flags */
#include <asm/bitops.h>        /* atomic bitwise ops */
#include <asm/param.h>         /* HZ value */
#include <linux/sched.h>       /* TASK_*INTERRUPTIBLE macros */

#include "kzimp.h"

// debug flag
#undef DEBUG

// character device files need a major and minor number.
// They are set automatically when loading the module
static int kzimp_major, kzimp_minor;

// holds device information
static dev_t kzimp_dev_t;

// pointer to the /proc file
static struct proc_dir_entry *proc_file;

// array of communication channels
static struct kzimp_comm_chan *kzimp_channels;

// Compute the 16 bit one's complement sum of all the 16-bit words in data of size size.
// Use prev as the initial value of the sum.
// The same method is used by TCP and UDP to compute the checksum
// See RFC 793: http://tools.ietf.org/html/rfc793
// Code from Minix3 (http://www.minix3.org)
short oneC_sum(short prev, void *data, size_t size)
{
  char *dptr;
  size_t n;
  short word;
  int sum;
  int swap = 0;

  sum = prev;
  dptr = data;
  n = size;

  swap = ((size_t) dptr & 1);
  if (swap)
  {
    sum = ((sum & 0xFF) << 8) | ((sum & 0xFF00) >> 8);
    if (n > 0)
    {
      ((char *) &word)[0] = 0;
      ((char *) &word)[1] = dptr[0];
      sum += (int) word;
      dptr += 1;
      n -= 1;
    }
  }

  while (n >= 8)
  {
    sum += (int) ((short *) dptr)[0] + (int) ((short *) dptr)[1]
                                                              + (int) ((short *) dptr)[2] + (int) ((short *) dptr)[3];
    dptr += 8;
    n -= 8;
  }

  while (n >= 2)
  {
    sum += (int) ((short *) dptr)[0];
    dptr += 2;
    n -= 2;
  }

  if (n > 0)
  {
    ((char *) &word)[0] = dptr[0];
    ((char *) &word)[1] = 0;
    sum += (int) word;
  }

  sum = (sum & 0xFFFF) + (sum >> 16);
  if (sum > 0xFFFF)
    sum++;

  if (swap)
  {
    sum = ((sum & 0xFF) << 8) | ((sum & 0xFF00) >> 8);
  }

  return sum;
}

// return the bit to modify in the multicast mask for this reader
// given the communication channel chan or -1 if an error has occured
static int get_new_bitmap_bit(struct kzimp_comm_chan *chan)
{
  int bit_pos, nr_bits;

  nr_bits = sizeof(chan->multicast_mask) * 8; // sizeof() returns a number of bytes. We want bits.
  bit_pos = find_first_zero_bit(&chan->multicast_mask, nr_bits);

  if (bit_pos != nr_bits)
  {
    set_bit(bit_pos, &chan->multicast_mask);
  }
  else
  {
    bit_pos = -1;
  }

  return bit_pos;
}

/*
 * kzimp open operation.
 * Returns:
 *  . -ENOMEM if the memory allocations fail
 *  . -1 if the maximum number of readers have been reached
 *  . 0 otherwise
 */
static int kzimp_open(struct inode *inode, struct file *filp)
{
  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  chan = container_of(inode->i_cdev, struct kzimp_comm_chan, cdev);

  ctrl = my_kmalloc(sizeof(*ctrl), GFP_KERNEL);
  if (unlikely(!ctrl))
  {
    printk(KERN_ERR "kzimp: kzimp_ctrl allocation error\n");
    return -ENOMEM;
  }

  ctrl->pid = current->pid;
  ctrl->channel = chan;

  if (filp->f_mode & FMODE_READ)
  {
    spin_lock(&chan->bcl);

    // we set next_read_idx to the next position where the writer is going to write
    // so that it gets the next message
    ctrl->next_read_idx = chan->next_write_idx;
    ctrl->bitmap_bit = get_new_bitmap_bit(chan);
    ctrl->online = 1;

    chan->nb_readers++;
    list_add_tail(&ctrl->next, &chan->readers);

    spin_unlock(&chan->bcl);

    if (ctrl->bitmap_bit == -1)
    {
      printk(KERN_ERR "Maximum number of readers on the channel %i has been reached: %i\n", chan->chan_id, chan->nb_readers);
      return -1;
    }
  }
  else
  {
    ctrl->next_read_idx = -1;
    ctrl->bitmap_bit = -1;
    ctrl->online = -1;
    ctrl->next.prev = ctrl->next.next = NULL;
  }

  filp->private_data = ctrl;

  return 0;
}

/*
 * kzimp release operation.
 * Returns:
 *  . 0: it always succeeds
 */
static int kzimp_release(struct inode *inode, struct file *filp)
{
  int i;

  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  ctrl = filp->private_data;

  // the control structure is only for a reader
  if (filp->f_mode & FMODE_READ)
  {
    chan = ctrl->channel;

    spin_lock(&chan->bcl);

    for (i = 0; i < chan->channel_size; i++)
    {
      clear_bit(ctrl->bitmap_bit, &(chan->msgs[i].bitmap));
    }
    clear_bit(ctrl->bitmap_bit, &(chan->multicast_mask));

    list_del(&ctrl->next);
    chan->nb_readers--;

    spin_unlock(&chan->bcl);

  }

  my_kfree(ctrl);

  return 0;
}

/*
 * kzimp read operation.
 * Blocking by default. May be non blocking (if O_NONBLOCK is set when calling open()).
 * Returns:
 *  . -EFAULT if the copy to buf has failed
 *  . -EAGAIN if the operations are non-blocking and the call would block.
 *  . -EBADF if this reader is no longer online (because the writer has experienced a timeout)
 *  . -EIO if the checksum is incorrect
 *  . -EINTR if the process has been interrupted by a signal while waiting
 *  . 0 if there has been an error when reading (count is <= 0)
 *  . The number of read bytes otherwise
 */
static ssize_t kzimp_read
(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  int retval;
  struct kzimp_message *m, m4chksum;
  DEFINE_WAIT(__wait);

  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  m = &(chan->msgs[ctrl->next_read_idx]);

  // we do not need this test to be atomic
  while (!reader_can_read(m->bitmap, ctrl->bitmap_bit))
  {
    // file is open in no-blocking mode
    if (filp->f_flags & O_NONBLOCK)
    {
      printk(KERN_WARNING "kzimp: process %i in read returns because of non-blocking ops\n", current->pid);
      return -EAGAIN;
    }

    prepare_to_wait(&chan->rq, &__wait, TASK_INTERRUPTIBLE);

    if (unlikely(signal_pending(current)))
    {
      printk(KERN_WARNING "kzimp: process %i in read has been interrupted\n", current->pid);
      return -EINTR;
    }

    schedule();
  }
  finish_wait(&chan->rq, &__wait);

  // check length
  count = (m->len < count ? m->len : count);

  // compute checksum if required
  m4chksum.checksum = 0;
  if (chan->compute_checksum)
  {
    // construct the header
    memset(&m4chksum, 0, KZIMP_HEADER_SIZE);
    m4chksum.len = count;
    m4chksum.bitmap = 0;

    m4chksum.checksum = oneC_sum(oneC_sum(0, &m4chksum, KZIMP_HEADER_SIZE), m->data, count);
  }

  if (m4chksum.checksum == m->checksum)
  {
    if (unlikely(copy_to_user(buf, m->data, count)))
    {
      printk(KERN_ERR "kzimp: copy_to_user failed for process %i in read\n", current->pid);
      return -EFAULT;
    }
    retval = count;
  }
  else
  {
    printk(KERN_WARNING "kzimp: Process %i in read has found an incorrect checksum: %hi != %hi\n", current->pid, m4chksum.checksum, m->checksum);
    retval = -EIO;
  }

  // the timeout at the writer may have expired, and the writer may have started to write
  // a new message at m
  if (likely(ctrl->online))
  {
    clear_bit(ctrl->bitmap_bit, &m->bitmap);
    if (writer_can_write(m->bitmap))
    {
       wake_up(&chan->wq);
    }

    ctrl->next_read_idx = (ctrl->next_read_idx + 1) % chan->channel_size;
  }
  else
  {
    printk(KERN_WARNING "kzimp: Process %i in read is no longer active\n", current->pid);
    retval = -EBADF;
  }

  return retval;
}

// When the timeout expires, the writer removes the bits that are at 1 in this bitmap, for all the messages
// chan is the channel, m is the struct kzimp_message where to write the current message.
static void handle_timeout(struct kzimp_comm_chan *chan,
    struct kzimp_message *m)
{
  int i;
  struct list_head *p;
  struct kzimp_ctrl *ptr;

  unsigned long bitmap = m->bitmap;

  // remove the bits from all the messages
  for (i = 0; i < chan->channel_size; i++)
  {
    chan->msgs[i].bitmap &= ~bitmap;
  }

  // remove the bits from the multicast mask
  chan->multicast_mask &= ~bitmap;

  // set the remaining readers on that bitmap to offline
  list_for_each(p, &chan->readers)
  {
    ptr = list_entry(p, struct kzimp_ctrl, next);
    if (reader_can_read(bitmap, ptr->bitmap_bit))
    {
      ptr->online = 0;
      printk(KERN_DEBUG "kzimp: Process %i in write. Process %i is offline\n", current->pid, ptr->pid);
    }
  }

}

/*
 * kzimp write operation.
 * Blocking call.
 * Sleeps until it can write the message.
 * There can be only one writer at a time.
 * Returns:
 *  . 0 if the size of the user-level buffer is less or equal than 0 or greater than the maximal message size
 *  . -ENOMEM if the memory allocations fail
 *  . -EFAULT if the buffer *buf is not valid
 *  . -EINTR if the process has been interrupted by a signal while waiting
 *  . -EAGAIN if the operations are non-blocking and the call would block. Note that if this is the case, the buffer is copied
 *            from user to kernel space.
 *  . The number of written bytes otherwise
 */
static ssize_t kzimp_write
(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  char *kb;
  long to_expired;
  struct kzimp_message *m, m4chksum;
  DEFINE_WAIT(__wait);

  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  // Check the validity of the arguments
  if (unlikely(count <= 0 || count > chan->max_msg_size))
  {
    printk(KERN_ERR "kzimp: count is not valid: %lu (process %i in write on channel %i)\n", (unsigned long)count, current->pid, chan->chan_id);
    return 0;
  }

  // allocate kernel-space buffer and memcpy
  kb = my_kmalloc(count, GFP_KERNEL);
  if (unlikely(!kb))
  {
    printk(KERN_ERR "kzimp: allocation failed for process %i in write\n", current->pid);
    return -ENOMEM;
  }

  // copy_from_user returns the number of bytes left to copy
  if (unlikely(copy_from_user(kb, buf, count)))
  {
    printk(KERN_ERR "kzimp: copy_from_user failed for process %i in write\n", current->pid);
    my_kfree(kb);
    return -EFAULT;
  }

  // compute checksum if required
  m4chksum.checksum = 0;
  if (chan->compute_checksum)
  {
    // construct the header
    memset(&m4chksum, 0, KZIMP_HEADER_SIZE);
    m4chksum.len = count;
    m4chksum.bitmap = 0;

    m4chksum.checksum = oneC_sum(oneC_sum(0, &m4chksum, KZIMP_HEADER_SIZE), kb, count);
  }

  //TODO: do we keep the lock? I.e.:
  //        -1 writer in the critical section, which may experience the timeout, the other waiting behing.
  //        -all the writers in the critical section, which may experience the timeout.     -> less spinning, better solution?
  spin_lock(&chan->bcl);

  m = &(chan->msgs[chan->next_write_idx]);

  // there is only 1 writer at a time, which will sleep until
  // the bitmap is empty or the timeout expires.
  // It keeps the writer lock while waiting.
  to_expired = 1;
  while (!writer_can_write(m->bitmap) && to_expired)
  {
    // file is open in no-blocking mode
    if (filp->f_flags & O_NONBLOCK)
    {
      spin_unlock(&chan->bcl);
      my_kfree(kb);
      return -EAGAIN;
    }

    prepare_to_wait(&chan->wq, &__wait, TASK_INTERRUPTIBLE);

    if (unlikely(signal_pending(current)))
    {
      spin_unlock(&chan->bcl);
      my_kfree(kb);
      printk(KERN_WARNING "kzimp: process %i in write has been interrupted\n", current->pid);
      return -EINTR;
    }

    to_expired = schedule_timeout(chan->timeout_in_ms * HZ / 1000);
  }
  finish_wait(&chan->wq, &__wait);

  if (unlikely(!to_expired))
  {
    printk(KERN_DEBUG "kzimp: timer has expired for process %i in write\n", current->pid);
    handle_timeout(chan, m);
  }

  if (m->data != NULL)
  {
    my_kfree(m->data);
  }
  m->data = kb;
  m->len = count;
  m->checksum = m4chksum.checksum;
  m->bitmap = chan->multicast_mask;

  chan->next_write_idx = (chan->next_write_idx + 1) % chan->channel_size;

  spin_unlock(&chan->bcl);

  // wake up sleeping readers
  wake_up(&chan->rq);

  return count;
}

// Called by select(), poll() and epoll() syscalls.
// pre-condition: must be called by a reader. The call does not work
// (and does not have sense) for a writer.
static unsigned int kzimp_poll(struct file *filp, poll_table *wait)
{
  unsigned int mask = 0;
  struct kzimp_message *m;

  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  poll_wait(filp, &chan->rq, wait);

  m = &(chan->msgs[ctrl->next_read_idx]);
  if (reader_can_read(m->bitmap, ctrl->bitmap_bit))
  {
    mask |= POLLIN | POLLRDNORM;
  }
  if (!ctrl->online)
  {
    mask |= POLLHUP; // kind of end-of-file
  }

  return mask;
}

static int kzimp_init_channel(struct kzimp_comm_chan *channel, int chan_id,
    int max_msg_size, int channel_size, long to, int compute_checksum,
    int init_lock)
{
  int i;
  unsigned long size;

  channel->chan_id = chan_id;
  channel->max_msg_size = max_msg_size;
  channel->channel_size = channel_size;
  channel->compute_checksum = compute_checksum;
  channel->timeout_in_ms = to;
  channel->multicast_mask = 0;
  channel->nb_readers = 0;
  channel->next_write_idx = 0;
  init_waitqueue_head(&channel->rq);
  init_waitqueue_head(&channel->wq);
  INIT_LIST_HEAD(&channel->readers);

  size = sizeof(*channel->msgs) * channel->channel_size;
  channel->msgs = my_vmalloc(size);
  if (unlikely(!channel->msgs))
  {
    printk(KERN_ERR "kzimp: channel messages allocation of %lu bytes error\n", size);
    return -ENOMEM;
  }

  for (i = 0; i < channel->channel_size; i++)
  {
    channel->msgs[i].data = NULL;
    channel->msgs[i].bitmap = 0;
  }

  if (init_lock)
  {
    channel->bcl = SPIN_LOCK_UNLOCKED;
  }

  return 0;
}

// called when reading file /proc/<procfs_name>
static int kzimp_read_proc_file(char *page, char **start, off_t off, int count,
    int *eof, void *data)
{
  int i;
  int len;

  len = sprintf(page, "kzimp %s @ %s\n\n", __DATE__, __TIME__);
  len += sprintf(page + len, "nb_max_communication_channels = %i\n",
      nb_max_communication_channels);
  len += sprintf(page + len, "default_channel_size = %i\n",
      default_channel_size);
  len += sprintf(page + len, "default_max_msg_size = %i\n",
      default_max_msg_size);
  len += sprintf(page + len, "default_timeout_in_ms = %li\n",
      default_timeout_in_ms);
  len += sprintf(page + len, "default_compute_checksum = %i\n\n",
      default_compute_checksum);

  len
  += sprintf(
      page + len,
      "chan_id\tchan_size\tmax_msg_size\tmulticast_mask\tnb_receivers\ttimeout_in_ms\tcompute_checksum\n");
  for (i = 0; i < nb_max_communication_channels; i++)
  {
    len += sprintf(page + len, "%i\t%i\t%i\t%lx\t%i\t%li\t%i\n",
        kzimp_channels[i].chan_id, kzimp_channels[i].channel_size,
        kzimp_channels[i].max_msg_size, kzimp_channels[i].multicast_mask,
        kzimp_channels[i].nb_readers, kzimp_channels[i].timeout_in_ms,
        kzimp_channels[i].compute_checksum);
  }

  return len;
}

// called when writing to file /proc/<procfs_name>
static int kzimp_write_proc_file(struct file *file, const char *buffer,
    unsigned long count, void *data)
{
  int err = 0;
  int len;
  int chan_id, max_msg_size, channel_size, compute_checksum;
  long to;
  char* kbuff;

  len = count;

  kbuff = (char*) my_kmalloc(sizeof(char) * len, GFP_KERNEL);

  if (copy_from_user(kbuff, buffer, len))
  {
    my_kfree(kbuff);
    return -EFAULT;
  }

  kbuff[len - 1] = '\0';

  sscanf(kbuff, "%i %i %i %li %i", &chan_id, &channel_size, &max_msg_size, &to,
      &compute_checksum);

  my_kfree(kbuff);

  if (chan_id < 0 || chan_id >= nb_max_communication_channels)
  {
    // channel id not valid
    printk(KERN_WARNING "kzimp: channel id not valid: %i <= %i < %i", 0, chan_id, nb_max_communication_channels);
    return len;
  }

  if (channel_size < 0)
  {
    // channel size not valid
    printk(KERN_WARNING "kzimp: channel size not valid: %i < %i", channel_size, 0);
    return len;
  }

  if (max_msg_size <= 0)
  {
    // max message size not valid
    printk(KERN_WARNING "kzimp: max message size not valid: %i <= %i", max_msg_size, 0);
    return len;
  }

  if (to <= 0)
  {
    // timeout not valid
    printk(KERN_WARNING "kzimp: timeout not valid: %li <= %i", to, 0);
    return len;
  }

  spin_lock(&kzimp_channels[chan_id].bcl);

  // we can modify the channel only if there are no readers on it
  if (kzimp_channels[chan_id].nb_readers == 0)
  {
    err = kzimp_init_channel(&kzimp_channels[chan_id], chan_id, max_msg_size,
        channel_size, to, compute_checksum, 0);
  }

  spin_unlock(&kzimp_channels[chan_id].bcl);

  if (unlikely(err))
  {
    printk  (KERN_WARNING "kzimp: Error %i at initialization of channel %i", err, chan_id);
  }

  return len;
}

static int kzimp_init_cdev(struct kzimp_comm_chan *channel, int i)
{
  int err, devno;

  err = kzimp_init_channel(channel, i, default_max_msg_size,
      default_channel_size, default_timeout_in_ms, default_compute_checksum, 1);
  if (unlikely(err))
  {
    printk(KERN_ERR "kzimp: Error %i at initialization of channel %i", err, i);
    return -1;
  }

  devno = MKDEV(kzimp_major, kzimp_minor + i);

  cdev_init(&channel->cdev, &kzimp_fops);
  channel->cdev.owner = THIS_MODULE;

  err = cdev_add(&channel->cdev, devno, 1);
  /* Fail gracefully if need be */
  if (unlikely(err))
  {
    printk(KERN_ERR "kzimp: Error %d adding kzimp%d", err, i);
    return -1;
  }

  return 0;
}

static void kzimp_del_cdev(struct kzimp_comm_chan *channel)
{
  cdev_del(&channel->cdev);
}

static int __init kzimp_start(void)
{
  int i;
  int result;

  // ADDING THE DEVICE FILES
  result = alloc_chrdev_region(&kzimp_dev_t, kzimp_minor, nb_max_communication_channels, DEVICE_NAME);
  kzimp_major = MAJOR(kzimp_dev_t);

  if (unlikely(result < 0))
  {
    printk(KERN_ERR "kzimp: can't get major %d\n", kzimp_major);
    return result;
  }

  kzimp_channels = my_kmalloc(nb_max_communication_channels * sizeof(struct kzimp_comm_chan), GFP_KERNEL);
  if (unlikely(!kzimp_channels))
  {
    printk(KERN_ERR "kzimp: channels allocation error\n");
    return -ENOMEM;
  }

  for (i=0; i<nb_max_communication_channels; i++)
  {
    result = kzimp_init_cdev(&kzimp_channels[i], i);
    if (unlikely(result))
    {
      printk (KERN_ERR "kzimp: creation of channel device %i failed\n", i);
      return -1;
    }
  }

  // CREATE /PROC FILE
  proc_file = create_proc_entry(procfs_name, 0444, NULL);
  if (unlikely(!proc_file))
  {
    remove_proc_entry(procfs_name, NULL);
    printk (KERN_ERR "kzimp: creation of /proc/%s file failed\n", procfs_name);
    return -1;
  }
  proc_file->read_proc = kzimp_read_proc_file;
  proc_file->write_proc = kzimp_write_proc_file;

  return 0;
}

// Note: we assume there are no processes still using the channels.
static void __exit kzimp_end(void)
{
  int i, j;

  // delete channels
  for (i=0; i<nb_max_communication_channels; i++)
  {
    for (j = 0; j < kzimp_channels[i].channel_size; j++)
    {
      if (kzimp_channels[i].msgs[j].data != NULL)
      {
        my_kfree(kzimp_channels[i].msgs[j].data);
      }
    }
    my_vfree(kzimp_channels[i].msgs);

    kzimp_del_cdev(&kzimp_channels[i]);
  }
  my_kfree(kzimp_channels);

  // remove the /proc file
  remove_proc_entry(procfs_name, NULL);

  unregister_chrdev_region(kzimp_dev_t, nb_max_communication_channels);

  my_memory_stats();
}

module_init( kzimp_start);
module_exit( kzimp_end);
