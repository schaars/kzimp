/* kbfish - character device module */

#include <linux/kernel.h>      /* Needed for KERN_INFO */
#include <linux/init.h>        /* Needed for the macros */
#include <linux/proc_fs.h>     /* proc fs */
#include <linux/sched.h>       /* current macro */
#include <linux/fcntl.h>       /* some flags */
#include <asm/uaccess.h>       /* copy_from_user function */
#include <linux/vmalloc.h>      /* vmalloc */

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
 *  . -EACCESS if the requested access is not allowed (must be RW)
 *  . -EEXIST if there is already a registered process
 *  . 0 otherwise
 */
static int kbfish_open(struct inode *inode, struct file *filp)
{
   struct kbfish_channel *chan; /* channel information */
   struct kbfish_ctrl *ctrl;
   int retval;

   // the file must be opened in RW, otherwise we cannot mmap the areas
   if (!(filp->f_mode & FMODE_READ) && !(filp->f_mode & FMODE_WRITE)) {
      printk(KERN_ERR "kbfish: process %i in open has not the right credentials\n", current->pid);
      retval = -EACCES;
      goto out;
   }

   chan = container_of(inode->i_cdev, struct kbfish_channel, cdev);

   ctrl = kmalloc(sizeof(*ctrl), GFP_KERNEL);
   if (unlikely(!ctrl))
   {
      printk(KERN_ERR "kbfish: kbfish_ctrl allocation error\n");
      return -ENOMEM;
   }

   spin_lock(&chan->bcl);

   // the receiver needs the O_CREAT flag
   if (filp->f_flags & O_CREAT) {
      if (chan->receiver != -1) {
         printk(KERN_ERR "kbfish: process %i in open but there is already a receiver: %i\n", current->pid, chan->receiver);
         retval = -EEXIST;
         goto unlock;
      }
      chan->receiver = current->pid;
      ctrl->is_sender = 0;
   } else {
      if (chan->sender != -1) {
         printk(KERN_ERR "kbfish: process %i in open but there is already a sender: %i\n", current->pid, chan->sender);
         retval = -EEXIST;
         goto unlock;
      }
      chan->sender = current->pid;
      ctrl->is_sender = 1;
   }

   ctrl->pid = current->pid;
   ctrl->chan = chan;
   filp->private_data = ctrl;
   retval = 0;

unlock:
   spin_unlock(&chan->bcl);
out:
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

   if (ctrl->is_sender) {
      chan->sender = -1;
   } else {
      chan->receiver = -1;
   }

   spin_unlock(&chan->bcl);

   kfree(ctrl);

   return 0;
}

static int kbfish_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf) {
   //TODO
   return 0;
}


/*
 * Verify credentials for the sender, when it calls mmap
 * Returns:
 *  . -EACCES if the process has not the credentials for the requested permission.
 *  . -EINVAL if offset is not valid
 *  . 0 otherwise.
 */
static int verify_credentials_for_sender(struct vm_area_struct *vma, struct kbfish_ctrl *ctrl) {
   int retval = 0;

   switch(vma->vm_pgoff) {
      case 1:
         if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_READ)) {
            //TODO: access granted! 
            //TODO: modify ctrl with the informations about the vma
            printk(KERN_DEBUG "kbfish: process %i in kbfish_mmap is a sender and has access to the send area in WO\n", current->pid);
         } else {
            retval = -EACCES;
         }
         break;

      case 2:
         if ((vma->vm_flags & VM_READ) && !(vma->vm_flags & VM_WRITE)) {
            //TODO: access granted! 
            //TODO: modify ctrl with the informations about the vma
            printk(KERN_DEBUG "kbfish: process %i in kbfish_mmap is a sender and has access to the read area in RO\n", current->pid);
         } else {
            retval = -EACCES;
         }
         break;

      default:         
         retval = -EINVAL;
         break;
   }

   return retval;
}

/*
 * Verify credentials for the receiver, when it calls mmap
 * Returns:
 *  . -EACCES if the process has not the credentials for the requested permission.
 *  . -EINVAL if offset is not valid
 *  . 0 otherwise.
 */
static int verify_credentials_for_receiver(struct vm_area_struct *vma, struct kbfish_ctrl *ctrl) {
   int retval = 0;

   switch(vma->vm_pgoff) {
      case 1:
         if ((vma->vm_flags & VM_READ) && !(vma->vm_flags & VM_WRITE)) {
            //TODO: access granted! 
            //TODO: modify ctrl with the informations about the vma
            printk(KERN_DEBUG "kbfish: process %i in kbfish_mmap is a receiver and has access to the send area in RO\n", current->pid);
         } else {
            retval = -EACCES;
         }
         break;

      case 2:
         if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_READ)) {
            //TODO: access granted! 
            //TODO: modify ctrl with the informations about the vma
            printk(KERN_DEBUG "kbfish: process %i in kbfish_mmap is a receiver and has access to the read area in WO\n", current->pid);
         } else {
            retval = -EACCES;
         }
         break;

      default:         
         retval = -EINVAL;
         break;
   }

   return retval;
}

/*
 * kbfish mmap operation.
 * Returns:
 *  . -EACCES if the process has not the credentials for the requested permission.
 *  . -EINVAL if offset is not valid
 *  . 0 otherwise.
 */
static int kbfish_mmap(struct file *filp, struct vm_area_struct *vma) {
   int r;
   struct kbfish_channel *chan; /* channel information */
   struct kbfish_ctrl *ctrl;

   ctrl = filp->private_data;
   chan = ctrl->chan;

   printk(KERN_DEBUG "kbfish: process %i in kbfish_mmap\n", current->pid);
   printk(KERN_DEBUG "kbfish: vm_start=%lu, vm_end=%lu, vm_pgoff=%lu, vm_flags=%lu\n", vma->vm_start, vma->vm_end, vma->vm_pgoff, vma->vm_flags);

   //TODO: vma protection (RO or WO)...
   // vma->vm_flags contains the requested memory protection.
   // We verify the process has the credentials.
   // We use vma->vm_pgoff to know it the process wants to map the send or the read area.
   //vma->vm_flags |= VM_READ   VM_WRITE    VM_SHARED
   if (ctrl->is_sender) {
      r = verify_credentials_for_sender(vma, ctrl);
   } else {
      r = verify_credentials_for_receiver(vma, ctrl);
   }

   if (r < 0) {
      if (r == -EINVAL) {
         printk(KERN_ERR "kbfish: process %i in mmap does not have a valid offset: %lu\n", current->pid, vma->vm_pgoff);
      } else if (r == -EACCES) {
         printk(KERN_ERR "kbfish: process %i in mmap does not have the rights for the requested credwentials\n", current->pid);
      }

      return r;
   }

   /* don't do anything here: fault handles the page faults and the mapping */
   vma->vm_ops = &kbfish_vm_ops;
   vma->vm_flags |= VM_RESERVED; // do not attempt to swap out the vma
   vma->vm_flags |= VM_CAN_NONLINEAR; // Has ->fault & does nonlinear pages
   vma->vm_private_data = filp->private_data; // pointer to the control structure

   return 0;
}

static int kbfish_init_channel(struct kbfish_channel *channel, int chan_id,
      int max_msg_size, int channel_size, int init_lock)
{
   channel->chan_id = chan_id;
   channel->sender = -1;
   channel->receiver = -1;
   channel->channel_size = channel_size;
   channel->max_msg_size = max_msg_size;
   channel->size_in_bytes = ROUND_UP_SIZE(channel_size * max_msg_size); 
   channel->sender_to_receiver = vmalloc(channel->size_in_bytes);
   channel->receiver_to_sender = vmalloc(channel->size_in_bytes);
   if (!channel->sender_to_receiver || !channel->receiver_to_sender) {
      printk(KERN_ERR "kbfish: vmalloc error of %lu bytes: %p %p\n", channel->size_in_bytes, channel->sender_to_receiver, channel->receiver_to_sender);
      return -1;
   }

   if (init_lock)
   {
      channel->bcl = SPIN_LOCK_UNLOCKED;
   }

   return 0;
}

// called when reading file /proc/<procfs_name>
static int kbfish_read_proc_file(char *page, char **start, off_t off, int count,
      int *eof, void *data)
{
   int len, i;

   len = sprintf(page, "kbfish %s @ %s\n\n", __DATE__, __TIME__);
   len += sprintf(page + len, "page size = %lu\n", PAGE_SIZE);
   len += sprintf(page + len, "nb_max_communication_channels = %i\n",
         nb_max_communication_channels);
   len += sprintf(page + len, "default_channel_size = %i\n",
         default_channel_size);
   len += sprintf(page + len, "default_max_msg_size = %i\n\n",
         default_max_msg_size);

   len += sprintf(page + len, "chan_id\tchan_size\tmax_msg_size\tsize_in_bytes\tpid_sender\tpid_receiver\n");
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

   err = kbfish_init_channel(channel, i, default_max_msg_size, default_channel_size, 1);
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
   vfree(channel->sender_to_receiver);
   vfree(channel->receiver_to_sender);

   cdev_del(&channel->cdev);
}


static int __init kbfish_start(void)
{
   int i;
   int result;

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

module_init(kbfish_start);
module_exit(kbfish_end);
