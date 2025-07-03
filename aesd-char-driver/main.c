/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("yuanyimail1005"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    if (!filp->private_data){
	    PDEBUG("Error: private_data NULL");
	    return -EINVAL;
    }
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *device = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t bytes_copy = 0;
    size_t entry_off = 0;

    if (mutex_lock_interruptible(&device->lock)) {
	    PDEBUG("Error locking buffer_mutex");
	    return -ERESTARTSYS;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &entry_off);
    if (!entry){
	    PDEBUG("Reached end of buffer, returning 0 bytes");
	    mutex_unlock(&device->lock);
	    return 0;
    }

    bytes_copy = entry->size - entry_off;
    if(bytes_copy > count){
	    PDEBUG("Count: %zu Offset: ", count, *f_pos);
	    bytes_copy = count;
    }

    if(copy_to_user(buf, entry->buffptr + entry_off, bytes_copy)) {
	    PDEBUG("Error: copy_to_user failed");
	    mutex_unlock(&device->lock);
	    retval = -EFAULT;
    }


    *f_pos += bytes_copy;
    retval = bytes_copy;
    mutex_unlock(&device->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    struct aesd_buffer_entry entry;
    struct aesd_dev *device = filp->private_data;
    char *temp_buf;
    char *ker_buf;
    size_t temp_count;

    ker_buf = kmalloc(count, GFP_KERNEL);
    if(!ker_buf){
	    PDEBUG("Missing Kernel Buffer");
	    return -1;
    }

    if(copy_from_user(ker_buf, buf, count)){
	    PDEBUG("Error copying from user");
	    kfree(ker_buf);
	    return 1;
    }

    entry.buffptr = ker_buf;
    entry.size = count;
    mutex_lock(&device->lock);

    if(&device->buffer_entry.buffptr){
	    PDEBUG("In Buffer");
	    temp_count = device->buffer_entry.size + count;
	    temp_buf = krealloc(device->buffer_entry.buffptr, temp_count, GFP_KERNEL);

	    if(!temp_buf){
		    kfree(ker_buf);
		    mutex_unlock(&device->lock);
		    return retval;
	    }
	    memcpy(temp_buf + device->buffer_entry.size, ker_buf, count);
	    kfree(ker_buf);
	    device->buffer_entry.buffptr = temp_buf;
	    device->buffer_entry.size = temp_count;
    }
    else{
	    device->buffer_entry.buffptr = ker_buf;
	    device->buffer_entry.size = count;
    }
    
    if(device->buffer_entry.buffptr[device->buffer_entry.size - 1] == '\n'){
	    PDEBUG("End of Write");
	    entry.buffptr = device->buffer_entry.buffptr;
	    entry.size = device->buffer_entry.size;

	    aesd_circular_buffer_add_entry(&device->buffer, &entry);

	    device->buffer_entry.buffptr = NULL;
	    device->buffer_entry.size = 0;
    }
    mutex_unlock(&device->lock);
    retval = count;
    PDEBUG("write successful, wrote %zd bytes", retval);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence){
	loff_t new_position;
	loff_t sum = 0;
	size_t i;	
	struct aesd_dev *device = filp->private_data;

	switch (whence) {
		case SEEK_SET:
			new_position = off;
			break;
		case SEEK_CUR:
			if(mutex_lock_interruptible(&device->lock)) {
				PDEBUG("mutex lock failure during llseek");
				return -ERESTARTSYS;
			}
			new_position = filp->f_pos + off;
			mutex_unlock(&device->lock);
			break;
		case SEEK_END:
			if (mutex_lock_interruptible(&device->lock)) {
				PDEBUG("mutex lock failure during llseek");
				return -ERESTARTSYS;
			}
			for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
				sum += device->buffer.entry[i].size;
			}
			mutex_unlock(&device->lock);
			new_position = sum + off;
			break;
		default:
			return -EINVAL;
	}
	if (new_position < 0) {
		return -EINVAL;
	}

	filp->f_pos = new_position;
	return new_position;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	struct aesd_seekto seekto;
	int retval = 0;
    struct aesd_dev *device = filp->private_data;

	switch (cmd) {
		case AESDCHAR_IOCSEEKTO:
			if (__copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto))) {
				return -EFAULT;
			}

			if (seekto.write_cmd < 0 || seekto.write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
				return -EINVAL;
			}

			if (mutex_lock_interruptible(&device->lock)) {
				return -ERESTARTSYS;
			}

			if (device->buffer.entry[seekto.write_cmd].buffptr == NULL) {
				return -EINVAL;
			}

			if (seekto.write_cmd_offset > device->buffer.entry[seekto.write_cmd].size) {
				 return -EINVAL;
			}

			loff_t offset = 0;
			for (int i = 0; i < seekto.write_cmd; i++) {
				offset += device->buffer.entry[i].size;
			}
			
			offset += seekto.write_cmd_offset;
			filp->f_pos = offset;
			mutex_unlock(&device->lock);
		break;
		default: 
			return -ENOTTY;
	}
	return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    printk(KERN_INFO "Loaded moudle aesdchar\n");
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    int i;
    struct aesd_circular_buffer *buffer = &aesd_device.buffer;
    struct aesd_buffer_entry *entry;

    AESD_CIRCULAR_BUFFER_FOREACH(entry, buffer, i){
	    if((entry -> size > 0) && (entry->buffptr != NULL)){
		    kfree(entry->buffptr);
		    entry->size = 0;
	    }
    }
    printk(KERN_INFO "Unloaded module aesdchar");

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
