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

// Timer interval in nanoseconds (essentially sampling rate)
#define INTERVAL 10

//static ssize_t car_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
//static ssize_t car_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static int kmod_init(void);
static void kmod_exit(void);
static void gpio_init(void);
irqreturn_t interrupt_handler(int irq, void *data);
static void timer_callback(struct timer_list* t);

/* fops and status structs */
struct file_operations car_fops = {
    read: car_read,
    write: car_write,
};

static struct hrtimer hr_timer; 

int nw_irqnum, ne_irqnum, se_irqnum, sw_irqnum;
void *dev_id = NULL;


/* Function to init GPIO pins */
static void gpio_init(void) {

    //Initialize mic pins as imputs
    gpio_request(MIC_NW, "micnw");
    gpio_direction_input(MIC_NW, 0);
    gpio_request(MIC_NW, "micne");
    gpio_direction_input(MIC_NE, 0);
    gpio_request(MIC_SW, "micsw");
    gpio_direction_input(MIC_SW, 0);
    gpio_request(MIC_SE, "micse");
    gpio_direction_input(MIC_SE, 0);

    // Initialize motor pins as outputs
    gpio_request(MOTOR, "motor");
    gpio_direction_output(MOTOR);

	/* IRQ SETUP */
    nw_irqnum = gpio_to_irq(MIC_NW);
    if (nw_irqnum < 0) {
        printk(KERN_ERR "Failed to get IRQ number for BTN0\n");
    }
    // Request IRQ
    if (request_irq(nw_irqnum, interrupt_handler, IRQF_TRIGGER_RISING, "my_mic", dev_id)) {
        printk(KERN_ERR "Failed to request IRQ for microphone\n");
    }
    
    ne_irqnum = gpio_to_irq(MIC_NE);
    if (ne_irqnum < 0) {
        printk(KERN_ERR "Failed to get IRQ number for BTN0\n");
    }
    // Request IRQ
    if (request_irq(ne_irqnum, interrupt_handler, IRQF_TRIGGER_RISING, "my_mic", dev_id)) {
        printk(KERN_ERR "Failed to request IRQ for microphone\n");
    }
    
    sw_irqnum = gpio_to_irq(MIC_SW);
    if (sw_irqnum < 0) {
        printk(KERN_ERR "Failed to get IRQ number for BTN0\n");
    }
    // Request IRQ
    if (request_irq(sw_irqnum, interrupt_handler, IRQF_TRIGGER_RISING, "my_mic", dev_id)) {
        printk(KERN_ERR "Failed to request IRQ for microphone\n");
    }
    
    se_irqnum = gpio_to_irq(MIC_SE);
    if (se_irqnum < 0) {
        printk(KERN_ERR "Failed to get IRQ number for BTN0\n");
    }
    // Request IRQ
    if (request_irq(se_irqnum, interrupt_handler, IRQF_TRIGGER_RISING, "my_mic", dev_id)) {
        printk(KERN_ERR "Failed to request IRQ for microphone\n");
    }

    // Initialize and start the high-resolution timer
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = timer_callback;
    hrtimer_start(&hr_timer, ktime_set(0, INTERVAL), HRTIMER_MODE_REL);
    
}


irqreturn_t interrupt_handler(int irq, void *dev_id) {
    // Do nothing, just wake up the kernel thread to handle the timer
    hrtimer_start(&hr_timer, ktime_set(0, INTERVAL), HRTIMER_MODE_REL);

    return IRQ_HANDLED;
}


static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    // Check the amplitude of the microphone input
    if (gpio_get_value(MIC_NW) || gpio_get_value(MIC_NE) || gpio_get_value(MIC_SW) || gpio_get_value(MIC_SE)) {
        printk(KERN_INFO "Noise detected!\n");
    }

    // Reschedule the timer
    hrtimer_forward_now(timer, INTERVAL);

    return HRTIMER_RESTART;
}


static void kmod_exit(void) {

    hrtimer_cancel(&hr_timer);
    
    /* Freeing GPIO pins */
    gpio_free(MIC_NW);
    gpio_free(MIC_NE);
    gpio_free(MIC_SE);
    gpio_free(MIC_SW);
    gpio_free(MOTOR_);
    gpio_free(MOTOR_);
    gpio_free(MOTOR_);
    gpio_free(MOTOR_);
    free_irq(nw_irqnum, NULL);
    free_irq(ne_irqnum, NULL);
    free_irq(sw_irqnum, NULL);
    free_irq(se_irqnum, NULL);
    
}



module_init(kmod_init);
module_exit(kmod_exit);
