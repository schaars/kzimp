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
#include <linux/mman.h>        /* PROT_READ and PROT_WRITE */
#include <linux/mm.h>          /* mprotect_fixup */

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
  struct big_mem_area_elt *bma;

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

    ctrl->big_msg_area = NULL;
  }
  else
  {
    ctrl->next_read_idx = -1;
    ctrl->bitmap_bit = -1;
    ctrl->online = -1;
    ctrl->next.prev = ctrl->next.next = NULL;
  }

  if (filp->f_mode & FMODE_WRITE)
  {
    // there is one more message, otherwise the writer would be able to send channel_size messages and
    // attempt to write to the first messages that has not been read yet and is still RO.
    ctrl->big_msg_area_len = (unsigned long) chan->max_msg_size_page_rounded
        * (unsigned long) (chan->channel_size + 1);
    ctrl->big_msg_area = my_vmalloc(ctrl->big_msg_area_len);

    bma = my_kmalloc(sizeof(*bma), GFP_KERNEL);
    if (unlikely(!bma))
    {
      printk(KERN_ERR "kzimp: big_mem_area elt allocation error\n");
      return -ENOMEM;
    }

    bma->addr = ctrl->big_msg_area;
    bma->len = ctrl->big_msg_area_len;
    list_add_tail(&bma->next, &chan->writers_big_msg);

    // the writer needs the FMODE_READ right, otherwise it cannot mmap
    // this is because it needs to mmap with the MAP_SHARED flag.
    filp->f_mode |= FMODE_READ;
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

  // writers need FMODE_READ in order to mmap the big messages area. However, they are
  // not readers, so we check the value of next_read_idx
  if ((filp->f_mode & FMODE_READ) && ctrl->next_read_idx >= 0)
  {
    chan = ctrl->channel;

    spin_lock(&chan->bcl);

    clear_bit(ctrl->bitmap_bit, &(chan->multicast_mask));

    list_del(&ctrl->next);
    chan->nb_readers--;

    spin_unlock(&chan->bcl);

    for (i = 0; i < chan->channel_size; i++)
    {
      clear_bit(ctrl->bitmap_bit, &(chan->msgs[i].bitmap));
    }
  }

  // for a writer, you should free the memory area allocated for big messages.
  // However this cannot be done here: you need to be sure there is no message left to be read.
  // We choose to free the area only when unloading the module.

  my_kfree(ctrl);

  return 0;
}

/*
 * kzimp read operation.
 * Blocking by default. May be non blocking (if O_NONBLOCK is set when calling open()).
 * Returns:
 *  . -EFAULT if the copy to buf has failed
 *  . -EAGAIN if the operations are non-blocking and the call would block.
 *  . -EBADF if this reader is no longer online (because the writer has experienced a timeout) or this process is not allowed to read
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
  char *content;

  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  if (unlikely(ctrl->next_read_idx < 0))
  {
    return -EBADF;
  }

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

    // A simple schedule() causes a race condition.
    // If we check again the condition then there is no problem.
    // With the schedule_timeout() performance seem to be better.
    // if (!reader_can_read(m->bitmap, ctrl->bitmap_bit))
    //   schedule();
    schedule_timeout(HZ/100);
  }
  finish_wait(&chan->rq, &__wait);

  // check length
  count = (m->len < count ? m->len : count);

  if (m->data == NULL)
  {
    content = m->big_msg_data;
  }
  else
  {
    content = m->data;
  }

  // compute checksum if required
  m4chksum.checksum = 0;
  if (chan->compute_checksum)
  {
    // construct the header
    memset(&m4chksum, 0, KZIMP_HEADER_SIZE);
    m4chksum.len = count;
    m4chksum.bitmap = 0;
    m4chksum.checksum = oneC_sum(0, &m4chksum, KZIMP_HEADER_SIZE);

    if (chan->compute_checksum == 1)
    {
      m4chksum.checksum = oneC_sum(m4chksum.checksum, content, count);
    }
  }

  if (m4chksum.checksum == m->checksum)
  {
    if (unlikely(copy_to_user(buf, content, count)))
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
      //todo: if using big_msg_area, then send the pages RW again.
      if (m->data == NULL)
      {
        //todo  m->big_msg_data;
      }

      wake_up_interruptible(&chan->wq);
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
  unsigned long tmp;

  unsigned long bitmap = m->bitmap;

  spin_lock(&chan->bcl);

  // remove the bits from the multicast mask
  tmp = chan->multicast_mask & ~bitmap;
  __asm volatile ("" : : : "memory");
  chan->multicast_mask = tmp; // this operation is atomic

  // remove the bits from all the messages
  for (i = 0; i < chan->channel_size; i++)
  {
    // test if bitmap is different from 0, otherwise we may loose a message:
    // process A                        process B
    //                      msg.bitmap = 0
    //   | a = msg.bitmap & ~bitmap        |
    //   |                                 | msg.bitmap = multicast_mask
    //   | msg.bitmap = a                  |
    // The bitmap is now 0 instead of multicast_mask. The message has been lost.
    if (chan->msgs[i].bitmap != 0)
    {
      chan->msgs[i].bitmap &= ~bitmap;
    }
  }

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

  spin_unlock(&chan->bcl);
}

// Wait for writing if needed.
// Return 1 if everything is ok, an error otherwise.
static ssize_t kzimp_wait_for_writing_if_needed(struct file *filp,
    size_t count, struct kzimp_message **mf)
{
  long to_expired;
  struct kzimp_message *m;
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

  spin_lock(&chan->bcl);

  m = &(chan->msgs[chan->next_write_idx]);
  chan->next_write_idx = (chan->next_write_idx + 1) % chan->channel_size;

  spin_unlock(&chan->bcl);

  // there is only 1 writer at a time, which will sleep until
  // the bitmap is empty or the timeout expires.
  // It keeps the writer lock while waiting.
  to_expired = 1;
  while (!writer_can_write(m->bitmap) && to_expired)
  {
    // file is open in no-blocking mode
    if (filp->f_flags & O_NONBLOCK)
    {
      return -EAGAIN;
    }

    prepare_to_wait(&chan->wq, &__wait, TASK_INTERRUPTIBLE);

    if (unlikely(signal_pending(current)))
    {
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

  *mf = m;

  return 1;
}

// finalize the write
static void kzimp_finalize_write(struct kzimp_comm_chan *chan,
    struct kzimp_message *m, char *buf, size_t count)
{
  m->len = count;

  // compute checksum if required
  m->checksum = 0;
  if (chan->compute_checksum)
  {
    m->checksum = oneC_sum(0, m, KZIMP_HEADER_SIZE);

    if (chan->compute_checksum == 1)
    {
      m->checksum = oneC_sum(m->checksum, buf, count);
    }
  }

  m->bitmap = chan->multicast_mask;

  // wake up sleeping readers
  wake_up_interruptible(&chan->rq);
}

/*
 * kzimp write operation.
 * Blocking call.
 * Sleeps until it can write the message.
 * FIXME: if there are more writers than the number of messages in the channel, there can be
 * FIXME: 2 writers on the same message. We assume the channel size is less than the number of writers.
 * FIXME: To fix that we can add a counter.
 * Returns:
 *  . 0 if the size of the user-level buffer is less or equal than 0 or greater than the maximal message size
 *  . -EFAULT if the buffer *buf is not valid
 *  . -EINTR if the process has been interrupted by a signal while waiting
 *  . -EAGAIN if the operations are non-blocking and the call would block. Note that if this is the case, the buffer is copied
 *            from user to kernel space.
 *  . The number of written bytes otherwise
 */
static ssize_t kzimp_write
(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  struct kzimp_message *m;
  ssize_t ret;

  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  ret = kzimp_wait_for_writing_if_needed(filp, count, &m);
  if (unlikely(ret != 1))
  {
    return ret;
  }

  // copy_from_user returns the number of bytes left to copy
  if (unlikely(copy_from_user(m->data, buf, count)))
  {
    printk(KERN_ERR "kzimp: copy_from_user failed for process %i in write\n", current->pid);
    return -EFAULT;
  }

  m->big_msg_data = NULL;

  kzimp_finalize_write(chan, m, m->data, count);

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

/*
 * kzimp mmap operation.
 * FIXME: we should check the mapping has not already been requested.
 * FIXME: This means saving the offsets for which a mapping has been requested and checking
 * FIXME: if the present call is performed with a new offset or not. One problem with that is if
 * FIXME: a process mmap, munmap, and then mmap again the same offset: the 2nd mmap will fail.
 * Returns:
 *  . -EACCES if the process has not the credentials for the requested permission.
 *  . -EINVAL if the offset or the length are not valid
 *  . 0 otherwise.
 */
static int kzimp_mmap(struct file *filp, struct vm_area_struct *vma)
{
  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  /*
   printk(KERN_DEBUG "kzimp: process %i in kzimp_mmap\n", current->pid);
   printk(KERN_DEBUG "kzimp: vm_start=%lu, vm_end=%lu, vm_pgoff=%lu, vm_flags=%lu\n", vma->vm_start, vma->vm_end, vma->vm_pgoff, vma->vm_flags);
   */

  // Check the requested size: if greater than the max msg size, then return -EACCES.
  if (vma->vm_end - vma->vm_start > chan->max_msg_size_page_rounded)
  {
    printk(KERN_DEBUG "Request size too big: %lu > %i\n", vma->vm_end - vma->vm_start, chan->max_msg_size_page_rounded);
    return -EINVAL;
  }

  // Is the offset valid? Return -EINVAL if not
  if (vma->vm_pgoff > chan->channel_size + 1)
  {
    printk(KERN_ERR "Invalid offset: %lu > %i\n", vma->vm_pgoff, chan->channel_size + 1);
    return -EINVAL;
  }

  if (unlikely(!(vma->vm_flags & VM_WRITE) || !(vma->vm_flags & VM_READ)))
  {
    printk(KERN_ERR "kzimp: process %i in mmap does not have the rights for the requested credentials\n", current->pid);
    return -EACCES;
  }

  /* don't do anything here: fault handles the page faults and the mapping */
  vma->vm_ops = &kzimp_vm_ops;
  vma->vm_flags |= VM_RESERVED; // do not attempt to swap out the vma
  vma->vm_flags |= VM_CAN_NONLINEAR; // Has ->fault & does nonlinear pages
  vma->vm_private_data = filp->private_data; // pointer to the control structure

  return 0;
}

static int kzimp_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;
  struct page *peyj;
  unsigned long msg_offset, offset;
  int retval;

  ctrl = vma->vm_private_data;
  chan = ctrl->channel;

  /*
   printk(KERN_DEBUG "kzimp: process %i in kzimp_vma_fault\n", current->pid);
   printk(KERN_DEBUG "kzimp: vm_start=%lu, vm_end=%lu, vm_pgoff=%lu, vm_flags=%lu\n", vma->vm_start, vma->vm_end, vma->vm_pgoff, vma->vm_flags);
   printk(KERN_DEBUG "kzimp: flags=%u, pgoff=%lu, virtual_addr=%p, page=%p\n", vmf->flags, vmf->pgoff, vmf->virtual_address, vmf->page);
   */

  // vma->vm_pgoff is the offset of the beginning of this message
  // vmf->pgoff is the offset inside this message
  msg_offset = vma->vm_pgoff * chan->max_msg_size_page_rounded;
  offset = (vmf->pgoff - vma->vm_pgoff) * PAGE_SIZE;

  //printk(KERN_DEBUG "kzimp: msg_offset=%lu, offset=%lu, sum=%lu, max=%lu\n", msg_offset, offset, msg_offset+offset, ctrl->big_msg_area_len);

  peyj = vmalloc_to_page(
      (const void*) &(ctrl->big_msg_area[msg_offset + offset]));

  get_page(peyj);
  retval = VM_FAULT_MINOR;

  vmf->page = peyj;

  return 0;
}

static long kzimp_ioctl_write(struct file *filp, unsigned long uaddr,
    unsigned long offset, size_t count)
{
  long retval;
  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;
  struct kzimp_message *m;
  struct vm_area_struct *vma, *prev;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  //printk(KERN_DEBUG "uaddr=%p, offset=%lu, count=%lu\n", (char*)uaddr, offset, count);

  retval = kzimp_wait_for_writing_if_needed(filp, count, &m);
  if (unlikely(retval != 1))
  {
    return retval;
  }

  m->big_msg_data = ctrl->big_msg_area + offset * chan->max_msg_size_page_rounded;

  //printk(KERN_DEBUG "big_msg_area=%p, offset=%lu, big_msg_data=%p\n", ctrl->big_msg_area, (unsigned long)offset, m->big_msg_data);

  // check if the address is valid (i.e. in the big messages area)
  if (unlikely(m->big_msg_data < ctrl->big_msg_area || m->big_msg_data
      >= ctrl->big_msg_area + ctrl->big_msg_area_len))
  {
    printk(KERN_DEBUG "%p < %p or %p >= %p\n", m->big_msg_data, ctrl->big_msg_area, m->big_msg_data
        , ctrl->big_msg_area + ctrl->big_msg_area_len);
    return -EFAULT;
  }

  m->data = NULL;

  //todo: set the pages RO, otherwise this optimization is completely unreliable and pointless
  //todo: maybe we also need to unmap the pages in the writer's address space (so that it will perform a fault the next time it accesses them)
  //todo: tlb flush?
  vma = find_vma(current->mm, uaddr);
  prev = vma->vm_prev;
  if (vma)
  {
    //todo: modify the kernel so that we can call mprotect_fixup
    retval = mprotect_fixup(vma, &prev, vma->vm_start, vma->vm_end, PROT_READ);
    //vma->vm_flags &= ~VM_WRITE; is not necessary: this is done by mprotect_fixup
  }

  //printk    (KERN_DEBUG "kzimp: process %i in kzimp_ioctl for a message @%p of size %i\n", current->pid, m->big_msg_data, m->len);

  kzimp_finalize_write(chan, m, m->big_msg_data, count);

  return count;
}

/*
 * kzimp IOCTL
 * cmd must be KZIMP_IOCTL_SPLICE_WRITE
 * Return:
 *  . 0 if the size of the user-level buffer is less or equal than 0 or greater than the maximal message size
 *  . -EFAULT if the buffer in the struct iovec is not valid
 *  . -EINTR if the process has been interrupted by a signal while waiting
 *  . -EAGAIN if the operations are non-blocking and the call would block.
 *  . -EACCES if the process has not the rights to perform the requested action
 *  . The number of written bytes otherwise
 */
static long kzimp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  struct kzimp_comm_chan *chan; /* channel information */
  struct kzimp_ctrl *ctrl;
  long retval;
  unsigned long uaddr;
  unsigned long offset;
  unsigned long *kzimp_addr_struct;
  size_t count;

  retval = 0;

  ctrl = filp->private_data;
  chan = ctrl->channel;

  // arg is a unsigned long[3]. It contains:
  //  -arg[0]: user-space address of the message
  //  -arg[1]: index of this message in the big messages area
  //  -arg[2]: length
  kzimp_addr_struct = (unsigned long*) arg;

  //printk(KERN_DEBUG "kzimp: process %i in kzimp_ioctl with cmd=%u and arg=%p\n", current->pid, cmd, iov);

  switch (cmd)
  {
  case KZIMP_IOCTL_SPLICE_WRITE:
    if (!(filp->f_mode & FMODE_WRITE))
    {
      retval = -EACCES;
      break;
    }

    retval = get_user(uaddr, &kzimp_addr_struct[0]);
    if (unlikely(retval))
    {
      break;
    }

    retval = get_user(offset, &kzimp_addr_struct[1]);
    if (unlikely(retval))
    {
      break;
    }

    retval = get_user(count, &kzimp_addr_struct[2]);
    if (unlikely(retval))
    {
      break;
    }

    retval = kzimp_ioctl_write(filp, uaddr, offset, count);
    break;

  default:
    retval = -EINVAL;
    break;
  }

  return retval;
}

static int kzimp_init_channel(struct kzimp_comm_chan *channel, int chan_id,
    int max_msg_size, int channel_size, long to, int compute_checksum,
    int init_lock)
{
  int i;
  char *addr;
  unsigned long size;

  channel->chan_id = chan_id;
  channel->max_msg_size = max_msg_size;
  channel->max_msg_size_page_rounded = ROUND_UP_PAGE_SIZE(max_msg_size);
  channel->channel_size = channel_size;
  channel->compute_checksum = compute_checksum;
  channel->timeout_in_ms = to;
  channel->multicast_mask = 0;
  channel->nb_readers = 0;
  channel->next_write_idx = 0;
  init_waitqueue_head(&channel->rq);
  init_waitqueue_head(&channel->wq);
  INIT_LIST_HEAD(&channel->readers);
  INIT_LIST_HEAD(&channel->writers_big_msg);

  size = (unsigned long) channel->max_msg_size
      * (unsigned long) channel->channel_size;
  channel->messages_area = my_vmalloc(size);
  if (unlikely(!channel->messages_area))
  {
    printk(KERN_ERR "kzimp: channel messages allocation of %lu bytes error\n", size);
    return -ENOMEM;
  }

  size = sizeof(*channel->msgs) * channel->channel_size;
  channel->msgs = my_kmalloc(size, GFP_KERNEL);
  if (unlikely(!channel->msgs))
  {
    printk(KERN_ERR "kzimp: channel messages allocation of %lu bytes error\n", size);
    return -ENOMEM;
  }

  addr = channel->messages_area;
  for (i = 0; i < channel->channel_size; i++)
  {
    channel->msgs[i].data = addr;
    channel->msgs[i].bitmap = 0;
    addr += channel->max_msg_size;
  }

  if (init_lock)
  {
    spin_lock_init(&channel->bcl);
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

static void kzimp_free_channel(struct kzimp_comm_chan *chan)
{
  struct big_mem_area_elt *p, *next;

  my_vfree(chan->messages_area);
  my_kfree(chan->msgs);

  // set the remaining readers on that bitmap to offline
  list_for_each_entry_safe(p, next, &chan->writers_big_msg, next)
  {
    my_vfree(p->addr);
    list_del(&p->next);
    my_kfree(p);
  }
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
    kzimp_free_channel(&kzimp_channels[chan_id]);
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
  int i;

  // delete channels
  for (i=0; i<nb_max_communication_channels; i++)
  {
    kzimp_free_channel(&kzimp_channels[i]);
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
