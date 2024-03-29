#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernelspace Module for BeagleBone Traffic Controller");
#define DEVICE_NAME "MY_TRAFFIC"

static ssize_t traffic_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t traffic_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static int kmod_init(void);
static int kmod_exit(void);

/*fops and status structs */
struct file_operations traffic_fops = {
	read: traffic_read,
	write: traffic_write,
}

/* chrdev setup */
static int traffic_major=61;
static unsigned capacity = 256;
static unsigned bite = 256;
/* Buffer to store data */
static char *nibbler_buffer;
/* length of the current message */
static int nibbler_len;


static int kmod_init(void) {
	/* Registering chr device */
    result = register_chrdev(traffic_major, DEVICE_NAME, &traffic_fops);
    if (result < 0)
    {
        printk(KERN_ALERT
            "Cannot obtain major number (failed to register) %d\n", traffic_major);
        return result;
    }

    /* Allocating nibbler for the buffer */
    nibbler_buffer = kmalloc(capacity, GFP_KERNEL); 
    if (!nibbler_buffer)
    { 
        printk(KERN_ALERT "Insufficient kernel memory\n"); 
        result = -ENOMEM;
        goto fail; 
    } 
    memset(nibbler_buffer, 0, capacity);
    nibbler_len = 0;

    printk(KERN_ALERT "Inserting traffic module\n");

    fail: 
    kmod_exit(); 
    return result;
}

static void kmod_exit(void)
{
	unregister_chrdev(DEV_MAJOR, "mytraffic");
}



module_init(kmod_init);
module_exit(kmod_exit);

























