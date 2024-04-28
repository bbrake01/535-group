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
#include <linux/hrtimer.h> // high resolution timer
#include <linux/io.h>

#define CM_PER_BASE 0x44E10000  // Base address for Control Module peripherals
#define MCASP0_FSR_OFFSET 0x88c
#define MCASP0_AXR1_OFFSET 0x8d8
#define MCASP0_AHCLKR_OFFSET 0x8cc
#define MCASP0_ACLKR_OFFSET 0x8d0

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BeagleBone Controller for Sound Activated RC Car");
#define DEBUG 0
#define DEVICE_NAME "TRAILBLAZER"

/* Define GPIO pinout */
#define MIC1 65


/* function Declarations */ 
static void set_pin_mode(u32 offset, u8 mode);
static int kmod_init(void);
static void kmod_exit(void);
static void timer_callback(struct timer_list* t);

static struct timer_list timer;


static void set_pin_mode(u32 offset, u8 mode) {
    void __iomem *reg;

    // Map the control module registers into virtual address space
    reg = ioremap(CM_PER_BASE + offset, 4);
    if (!reg) {
        printk(KERN_ERR "Failed to map register at offset 0x%X\n", offset);
        return;
    }

    // Write the mode to the register
    // Mode value needs to be set in the lowest 3 bits, assuming no other bits are used.
    // If other bits are used for configurations like pull-up/down, they need to be set here as well.
    iowrite32(mode & 0x7, reg); // Only setting the mode, no pull configurations

    // Unmap the register
    iounmap(reg);
}

static int kmod_init(void) {
	
	printk(KERN_ALERT "Initializing module\n");

    // Example to set P8_37 to mode 3 (uart2_ctsn)
    set_pin_mode(MCASP0_FSR_OFFSET, 6); // Offset for P8_37, mode 3
    set_pin_mode(MCASP0_AXR1_OFFSET, 3);
    set_pin_mode(MCASP0_AHCLKR_OFFSET, 3);
    set_pin_mode(MCASP0_ACLKR_OFFSET, 3);

    gpio_request(MIC1, "mic1");
    gpio_direction_input(MIC1);

    printk(KERN_ALERT "Before timer setup\n");
    timer_setup(&timer, timer_callback, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(1000));
    printk(KERN_ALERT "after timer setup\n");

    return 0;
}

static void timer_callback(struct timer_list *t)
{
	printk(KERN_ALERT "in timer loop\n");
    printk(KERN_ALERT "Mic reading: %d\n", gpio_get_value(MIC1));
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));  
}

static void kmod_exit(void) {
	printk(KERN_ALERT "Exiting module\n");
}


module_init(kmod_init);
module_exit(kmod_exit);









