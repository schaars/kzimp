/* kbfish - character device module */

#include <linux/kernel.h>      /* Needed for KERN_INFO */
#include <linux/init.h>        /* Needed for the macros */
#include <linux/proc_fs.h>     /* proc fs */
#include <linux/sched.h>       /* current macro */
#include <linux/fcntl.h>       /* some flags */
#include <asm/uaccess.h>       /* copy_from_user function */
#include <linux/vmalloc.h>     /* vmalloc */

#include "kbfish.h"

// debug flag
#undef DEBUG

// character device files need a major and minor number.
// They are set automatically when loading the module
static int kbfish_major, kbfish_minor;

// holds device information
static dev_t kbfish_dev_t;

// pointer to the /proc file
static struct proc_dir_entry *proc_file;

// array of the kbfish_dev
static struct kbfish_channel *channels;

/*
 * kbfish open operation.
 * The file must be opened in RW mode.
 * The receiver needs to set the O_CREAT flag,
 * otherwise it will be considered as a sender.
 * Returns:
 *  . -ENOMEM memory allocation failed
 *  . -EEXIST if there is already a registered process
 *  . -EACCESS if the requested access is not allowed (must be RW)
 *  . 0 otherwise
 */
static int kbfish_open(struct inode *inode, struct file *filp)
{
  struct kbfish_channel *chan; /* channel information */
  struct ump_channel *ump_chan; /* channel information */
  struct kbfish_ctrl *ctrl;
  ump_index_t i;
  int retval;

  chan = container_of(inode->i_cdev, struct kbfish_channel, cdev);

  // the file must be opened in RW, otherwise we cannot mmap the areas
  if (!(filp->f_mode & FMODE_READ) && !(filp->f_mode & FMODE_WRITE))
  {
    printk(KERN_ERR "kbfishmem: process %i in open has not the right credentials\n", current->pid);
    return -EACCES;
  }

  ctrl = kmalloc(sizeof(*ctrl), GFP_KERNEL);
  if (unlikely(!ctrl))
  {
    printk(KERN_ERR "kbfish: kbfish_ctrl allocation error\n");
    return -ENOMEM;
  }

  ump_chan = (typeof(ump_chan)) kmalloc(sizeof(*ump_chan), GFP_KERNEL);
  if (!ump_chan)
  {
    printk(KERN_ERR "kbfish: ump_chan allocation error\n");
    return -ENOMEM;
  }

  spin_lock(&chan->bcl);

  // the receiver needs to open the file with O_CREAT
  if (filp->f_flags & O_CREAT)
  {
    if (chan->receiver != -1)
    {
      printk(KERN_ERR "kbfish: process %i in open but there is already a receiver: %i\n", current->pid, chan->receiver);
      retval = -EEXIST;
      kfree(ump_chan);
      goto unlock;
    }
    chan->receiver = current->pid;
    ctrl->is_sender = 0;

    // set the 2 areas to 0
    memset(chan->sender_to_receiver, 0, chan->size_in_bytes);
    memset(chan->receiver_to_sender, 0, chan->size_in_bytes);

    ump_chan->recv_chan.buf
        = (typeof(ump_chan->recv_chan.buf)) chan->sender_to_receiver;
    ump_chan->send_chan.buf
        = (typeof(ump_chan->send_chan.buf)) chan->receiver_to_sender;
  }
  else
  {
    if (chan->sender != -1)
    {
      printk(KERN_ERR "kbfish: process %i in open but there is already a sender: %i\n", current->pid, chan->sender);
      retval = -EEXIST;
      kfree(ump_chan);
      goto unlock;
    }
    chan->sender = current->pid;
    ctrl->is_sender = 1;

    ump_chan->recv_chan.buf
        = (typeof(ump_chan->recv_chan.buf)) chan->receiver_to_sender;
    ump_chan->send_chan.buf
        = (typeof(ump_chan->send_chan.buf)) chan->sender_to_receiver;
  }

  ump_chan->inchanlen = (size_t) chan->channel_size
      * (size_t) chan->max_msg_size;
  ump_chan->outchanlen = (size_t) chan->channel_size
      * (size_t) chan->max_msg_size;

  ump_chan->recv_chan.dir = UMP_INCOMING;
  ump_chan->send_chan.dir = UMP_OUTGOING;

  ump_chan->recv_chan.bufmsgs = chan->channel_size;
  ump_chan->send_chan.bufmsgs = chan->channel_size;

  for (i = 0; i < ump_chan->send_chan.bufmsgs; i++)
  {
    ump_chan->send_chan.buf[i].header.raw = 0;
  }

  ump_chan->max_recv_msgs = chan->channel_size;
  ump_chan->max_send_msgs = chan->channel_size;

  ump_chan->recv_chan.epoch = 1;
  ump_chan->recv_chan.pos = 0;

  ump_chan->send_chan.epoch = 1;
  ump_chan->send_chan.pos = 0;

  ump_chan->sent_id = 1;
  ump_chan->seq_id = 0;
  ump_chan->ack_id = 0;
  ump_chan->last_ack = 0;

  ctrl->pid = current->pid;
  ctrl->ump_chan = ump_chan;
  ctrl->chan = chan;
  filp->private_data = ctrl;
  retval = 0;

  unlock: spin_unlock(&chan->bcl);
  return retval;
}

/*
 * kbfish release operation.
 * Returns:
 *  . 0: it always succeeds
 */
static int kbfish_release(struct inode *inode, struct file *filp)
{
  struct kbfish_channel *chan; /* channel information */
  struct kbfish_ctrl *ctrl;

  ctrl = filp->private_data;
  chan = ctrl->chan;

  spin_lock(&chan->bcl);

  if (ctrl->is_sender)
  {
    chan->sender = -1;
  }
  else
  {
    chan->receiver = -1;
  }

  spin_unlock(&chan->bcl);

  kfree(ctrl->ump_chan);

  kfree(ctrl);

  return 0;
}

/*
 * recv a message.
 * Return values:
 *  . 0 if the size of the user-level buffer is less or equal than 0 or greater than the maximal message size
 *  . -EIO if there is an I/O error (e.g. there should be a message)
 *  . -EFAULT if the buffer *buf is not valid
 *  . -EINTR if the process has been interrupted by a signal while waiting
 *  . -EAGAIN if the operations are non-blocking and the call would block.
 *  . The number of written bytes otherwise
 */
static ssize_t kbfish_read
(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  struct kbfish_channel *chan; /* channel information */
  struct ump_channel *ump_chan; /* channel information */
  struct kbfish_ctrl *kbf_ctrl;

  struct ump_control ctrl;
  struct ump_message *ump_msg;
  int call_recv_again;
  int msgtype;
  DEFINE_WAIT(__wait);

  kbf_ctrl = (typeof(kbf_ctrl))filp->private_data;
  ump_chan = kbf_ctrl->ump_chan;
  chan = kbf_ctrl->chan;

  printk(KERN_DEBUG "{%i}[%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n", current->pid,
      __func__, __LINE__, ump_chan->sent_id, ump_chan->ack_id, ump_chan->max_send_msgs,
      ump_chan->seq_id, ump_chan->last_ack);

  while (!ump_endpoint_can_recv(&ump_chan->recv_chan))
  {
    // file is open in no-blocking mode
    if (filp->f_flags & O_NONBLOCK)
    {
      return -EAGAIN;
    }

    prepare_to_wait(&chan->rq, &__wait, TASK_INTERRUPTIBLE);

    if (unlikely(signal_pending(current)))
    {
      printk(KERN_WARNING "kbfish: process %i in write has been interrupted\n", current->pid);
      return -EINTR;
    }

    schedule();
  }
  finish_wait(&chan->rq, &__wait);

  ump_msg = ump_impl_recv(&ump_chan->recv_chan);
  if (ump_msg == NULL)
  {
    printk(KERN_DEBUG "{%i}[%s:%i] Error: ump_msg should not be null\n", current->pid, __func__, __LINE__);
    return -EIO;
  }

  // what kind of message is this?
  msgtype = ump_control_process(ump_chan, ump_msg->header.control);
  switch (msgtype)
  {
    case UMP_ACK: // this is an ack, we need to call recv again
    printk(KERN_DEBUG "{%i}[%s:%i] Has received an ack\n", current->pid, __func__, __LINE__);
    call_recv_again = 1;
    break;

    case UMP_MSG: // this is a message, we return it
    printk(KERN_DEBUG "{%i}[%s:%i] Has received a message\n", current->pid, __func__, __LINE__);
    count = (MESSAGE_BYTES < count ? MESSAGE_BYTES : count);

    // copy_from_user returns the number of bytes left to copy
    if (unlikely(copy_to_user(buf, ump_msg->data, count)))
    {
      printk(KERN_ERR "kbfish: copy_to_user failed for process %i in write\n", current->pid);
      return -EFAULT;
    }

    call_recv_again = 0;
    break;

    default:
    printk(KERN_DEBUG "{%i}[%s:%i] Error: unknown message type %i\n", current->pid, __func__, __LINE__,
        msgtype);
    call_recv_again = 1;
    break;
  }

  if (ump_send_ack_is_needed(ump_chan))
  {
    printk(KERN_DEBUG "{%i}[%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n", current->pid,
        __func__, __LINE__, ump_chan->sent_id, ump_chan->ack_id, ump_chan->max_send_msgs,
        ump_chan->seq_id, ump_chan->last_ack);
    printk(KERN_DEBUG "{%i}[%s:%i] I need to send an ack\n", current->pid, __func__, __LINE__);

    // this shouldn't happen: I have received a message, thus I have updated my information
    // concerning acks.
    if (!ump_can_send(ump_chan))
    {
      printk(KERN_DEBUG "{%i}[%s:%i] I need to send an ack but I cannot\n", current->pid, __func__, __LINE__);
      return -EIO;
    }

    ump_msg = ump_impl_get_next(&ump_chan->send_chan, &ctrl);
    ump_control_fill(ump_chan, &ctrl, UMP_ACK);
    BARRIER();
    ump_msg->header.control = ctrl;

    // wake up writers
    wake_up(&chan->wq);
  }

  if (call_recv_again)
  {
    printk(KERN_DEBUG "{%i}[%s:%i] Going to receive again\n", current->pid, __func__, __LINE__);
    return kbfish_read(filp, buf, count, f_pos);
  }
  else
  {
    return count;
  }
}

/*
 * wait for an ack
 * Return 0 if there was no error, the error otherwise.
 * Possible errors:
 *  . -EIO: there should be a message but there isn't
 *  . -EINTR if the process has been interrupted by a signal while waiting
 *  . -EAGAIN if the operations are non-blocking and the call would block.
 */
static int recv_ack(struct file* filp)
{
  struct kbfish_ctrl *ctrl;
  struct ump_channel *chan;
  struct kbfish_channel *kb_chan;
  struct ump_message *ump_msg;
  int msgtype;
  DEFINE_WAIT(__wait);

  ctrl = (typeof(ctrl)) filp->private_data;
  chan = ctrl->ump_chan;
  kb_chan = ctrl->chan;

  while (!ump_endpoint_can_recv(&chan->recv_chan))
  {
    // file is open in no-blocking mode
    if (filp->f_flags & O_NONBLOCK)
    {
      return -EAGAIN;
    }

    prepare_to_wait(&kb_chan->wq, &__wait, TASK_INTERRUPTIBLE);

    if (unlikely(signal_pending(current)))
    {
      printk(KERN_WARNING "kbfish: process %i in write has been interrupted\n", current->pid);
      return -EINTR;
    }

    schedule();
  }
  finish_wait(&kb_chan->wq, &__wait);

  ump_msg = ump_impl_recv(&chan->recv_chan);
  if (ump_msg == NULL)
  {
    printk(KERN_DEBUG "{%i} [%s:%i] Error: ump_msg should not be null\n", current->pid, __func__, __LINE__);
    return -EIO;
  }

  // what kind of message is this?
  msgtype = ump_control_process(chan, ump_msg->header.control);
  switch (msgtype)
  {
  case UMP_ACK: // this is an ack, we need to call recv again
    printk(KERN_DEBUG "[%s:%i] Has received an ack\n", __func__, __LINE__);
    break;
  case UMP_MSG: // this is a message, we return it
    printk(KERN_DEBUG "{%i} [%s:%i] Should not have received a message; expecting an ack\n", current->pid,
        __func__, __LINE__);
    break;
  default:
    printk(KERN_DEBUG "{%i} [%s:%i] Error: unknown message type %i\n", current->pid, __func__, __LINE__,
        msgtype);
    break;
  }

  return 0;
}

/*
 * kzimp write operation.
 * Blocking call.
 * Sleeps until it can write the message.
 * Returns:
 *  . 0 if the size of the user-level buffer is less or equal than 0 or greater than the maximal message size
 *  . -EIO if there should be a message but there is not
 *  . -EFAULT if the buffer *buf is not valid
 *  . -EINTR if the process has been interrupted by a signal while waiting
 *  . -EAGAIN if the operations are non-blocking and the call would block.
 *  . The number of written bytes otherwise
 */
static ssize_t kbfish_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  struct kbfish_channel *chan; /* channel information */
  struct ump_channel *ump_chan; /* channel information */
  struct kbfish_ctrl *kbf_ctrl;

  struct ump_control ctrl;
  struct ump_message *ump_msg;
  int r;

  kbf_ctrl = (typeof(kbf_ctrl))filp->private_data;
  ump_chan = kbf_ctrl->ump_chan;
  chan = kbf_ctrl->chan;

  printk(KERN_DEBUG "{%i} [%s:%i] sent_id=%hu, ack_id=%hu, max_send_msgs=%hu, seq_id=%hu, last_ack=%hu\n", current->pid,
      __func__, __LINE__, ump_chan->sent_id, ump_chan->ack_id, ump_chan->max_send_msgs,
      ump_chan->seq_id, ump_chan->last_ack);

  // Check the validity of the arguments
  if (unlikely(count <= 0 || count > chan->max_msg_size))
  {
    printk(KERN_ERR "kbfish: count is not valid: %lu (process %i in write on channel %i)\n", (unsigned long)count, current->pid, chan->chan_id);
    return 0;
  }

  while (!ump_can_send(ump_chan))
  {
    printk(KERN_DEBUG "{%i} [%s:%i] Going to receive an ack\n", current->pid, __func__, __LINE__);
    r = recv_ack(filp);
    if (r < 0)
    {
      return r;
    }
  }

  //code to send:
  ump_msg = ump_impl_get_next(&ump_chan->send_chan, &ctrl);
  count = (MESSAGE_BYTES < count ? MESSAGE_BYTES : count);

  // copy_from_user returns the number of bytes left to copy
  if (unlikely(copy_from_user(ump_msg->data, buf, count)))
  {
    printk(KERN_ERR "kbfish: copy_from_user failed for process %i in write\n", current->pid);
    return -EFAULT;
  }

  ump_control_fill(ump_chan, &ctrl, UMP_MSG);
  BARRIER();
  ump_msg->header.control = ctrl;

  // wake up sleeping readers
  wake_up(&chan->rq);

  return count;
}

// Called by select(), poll() and epoll() syscalls.
// pre-condition: must be called by a reader. The call does not work
// (and does not have sense) for a writer.
static unsigned int kbfish_poll(struct file *filp, poll_table *wait)
{
  unsigned int mask = 0;

  struct kbfish_channel *chan; /* channel information */
  struct ump_channel *ump_chan; /* channel information */
  struct kbfish_ctrl *kbf_ctrl;

  kbf_ctrl = (typeof(kbf_ctrl)) filp->private_data;
  ump_chan = kbf_ctrl->ump_chan;
  chan = kbf_ctrl->chan;

  poll_wait(filp, &chan->rq, wait);

  if (ump_endpoint_can_recv(&ump_chan->recv_chan))
  {
    mask |= POLLIN | POLLRDNORM;
  }

  return mask;
}

static int kbfish_init_channel(struct kbfish_channel *channel, int chan_id,
    int max_msg_size, int channel_size, int init_lock)
{
  channel->chan_id = chan_id;
  channel->sender = -1;
  channel->receiver = -1;
  channel->channel_size = channel_size;
  channel->max_msg_size = max_msg_size;
  channel->size_in_bytes = (unsigned long) channel_size
      * (unsigned long) max_msg_size;
  channel->sender_to_receiver = vmalloc(channel->size_in_bytes);
  channel->receiver_to_sender = vmalloc(channel->size_in_bytes);
  if (!channel->sender_to_receiver || !channel->receiver_to_sender)
  {
    printk(KERN_ERR "kbfish: vmalloc error of %lu bytes: %p %p\n", channel->size_in_bytes, channel->sender_to_receiver, channel->receiver_to_sender);
    return -1;
  }

  init_waitqueue_head(&channel->rq);
  init_waitqueue_head(&channel->wq);

  if (init_lock)
  {
    channel->bcl = SPIN_LOCK_UNLOCKED;
  }

  return 0;
}

// called when reading file /proc/<procfs_name>
static int kbfish_read_proc_file(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
  int len, i;

  len = sprintf(page, "kbfish %s @ %s\n\n", __DATE__, __TIME__);
  len += sprintf(page + len, "nb_max_communication_channels = %i\n",
      nb_max_communication_channels);
  len += sprintf(page + len, "default_channel_size = %i\n",
      default_channel_size);
  len += sprintf(page + len, "default_max_msg_size = %i\n\n",
      default_max_msg_size);

  len
      += sprintf(page + len,
          "chan_id\tchan_size\tmax_msg_size\tsize_in_bytes\tpid_sender\tpid_receiver\n");
  for (i = 0; i < nb_max_communication_channels; i++)
  {
    len += sprintf(page + len, "%i\t%i\t%i\t%lu\t%i\t%i\n",
        channels[i].chan_id, channels[i].channel_size,
        channels[i].max_msg_size, channels[i].size_in_bytes,
        channels[i].sender, channels[i].receiver);
  }

  return len;
}

// called when writing to file /proc/<procfs_name>
static int kbfish_write_proc_file(struct file *file, const char *buffer,
    unsigned long count, void *data)
{
  int len;
  char* kbuff;

  len = count;

  kbuff = (char*) kmalloc(sizeof(char) * len, GFP_KERNEL);

  if (copy_from_user(kbuff, buffer, len))
  {
    kfree(kbuff);
    return -EFAULT;
  }

  kbuff[len - 1] = '\0';

  //TODO: sscanf(kbuff, "", ...);
  //for a channel, change the parameters

  kfree(kbuff);

  return len;
}

static int kbfish_init_cdev(struct kbfish_channel *channel, int i)
{
  int err, devno;

  err = kbfish_init_channel(channel, i, default_max_msg_size,
      default_channel_size, 1);
  if (unlikely(err))
  {
    printk(KERN_ERR "kbfish: Error %i at initialization of channel %i", err, i);
    return -1;
  }

  devno = MKDEV(kbfish_major, kbfish_minor + i);

  cdev_init(&channel->cdev, &kbfish_fops);
  channel->cdev.owner = THIS_MODULE;

  err = cdev_add(&channel->cdev, devno, 1);
  /* Fail gracefully if need be */
  if (unlikely(err))
  {
    printk(KERN_ERR "kbfish: Error %d adding kbfish%d", err, i);
    return -1;
  }

  return 0;
}

static void kbfish_del_cdev(struct kbfish_channel *channel)
{
  vfree(channel->receiver_to_sender);
  vfree(channel->sender_to_receiver);

  cdev_del(&channel->cdev);
}

static int __init kbfish_start(void)
{
  int i;
  int result;

  //check that the max message size is equal to the macro MESSAGE_BYTES
  if (default_max_msg_size != MESSAGE_BYTES)
  {
    printk(KERN_ERR "kbfish: default_max_msg_size != MESSAGE_BYTES (%i != %i). You need to recompile the module with the appropriate value, or change default_max_msg_size\n", default_max_msg_size, MESSAGE_BYTES);
    return -1;
  }

  // ADDING THE DEVICE FILES
  result = alloc_chrdev_region(&kbfish_dev_t, kbfish_minor, nb_max_communication_channels, DEVICE_NAME);
  kbfish_major = MAJOR(kbfish_dev_t);

  if (unlikely(result < 0))
  {
    printk(KERN_ERR "kbfish: can't get major %d\n", kbfish_major);
    return result;
  }

  channels = kmalloc(nb_max_communication_channels * sizeof(struct kbfish_channel), GFP_KERNEL);
  if (unlikely(!channels))
  {
    printk(KERN_ERR "kbfish: channels allocation error\n");
    return -ENOMEM;
  }

  for (i=0; i<nb_max_communication_channels; i++)
  {
    result = kbfish_init_cdev(&channels[i], i);
    if (unlikely(result))
    {
      printk (KERN_ERR "kbfish: creation of channel device %i failed\n", i);
      return -1;
    }
  }

  // CREATE /PROC FILE
  proc_file = create_proc_entry(procfs_name, 0444, NULL);
  if (unlikely(!proc_file))
  {
    remove_proc_entry(procfs_name, NULL);
    printk (KERN_ERR "kbfish: creation of /proc/%s file failed\n", procfs_name);
    return -1;
  }
  proc_file->read_proc = kbfish_read_proc_file;
  proc_file->write_proc = kbfish_write_proc_file;

  return 0;
}

// Note: we assume there are no processes still using the channels.
static void __exit kbfish_end(void)
{
  int i;

  // delete channels
  for (i=0; i<nb_max_communication_channels; i++)
  {
    kbfish_del_cdev(&channels[i]);
  }
  kfree(channels);

  // remove the /proc file
  remove_proc_entry(procfs_name, NULL);

  unregister_chrdev_region(kbfish_dev_t, nb_max_communication_channels);
}

module_init( kbfish_start);
module_exit( kbfish_end);
