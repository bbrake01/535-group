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
#include <linux/gpio.h> //for working with GPIO
#include <linux/interrupt.h> // or interrupt handling

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BeagleBone Controller for Sound Activated RC Car");
#define DEBUG 0
#define DEVICE_NAME "TRAILBLAZER"

/* Define GPIO pinout */
#define MIC_NW 1
#define MIC_NE 1
#define MIC_SE 1
#define MIC_SW 1
#define MOTOR_NW 1
#define MOTOR_NE 1
#define MOTOR_SE 1
#define MOTOR_SW 1

static ssize_t car_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t car_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static int kmod_init(void);
static void kmod_exit(void);
static void gpio_init(void);
irqreturn_t interrupt_handler(int irq, void *data);

/* fops and status structs */
struct file_operations car_fops = {
    read: car_read,
    write: car_write,
};

















module_init(kmod_init);
module_exit(kmod_exit);
