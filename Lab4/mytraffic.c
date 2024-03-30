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

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernelspace Module for BeagleBone Traffic Controller");

#define DEVICE_NAME "MY_TRAFFIC"
// Define GPIO pinout
#define LED_RED 67
#define LED_YELLOW 68
#define LED_GREEN 44
#define BTN0 26
#define BTN1 46

enum op_mode{Normal, FlashRed, FlashYellow};

static ssize_t traffic_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t traffic_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static int kmod_init(void);
static void kmod_exit(void);
static void gpio_init(void);
static void button_interrupt_handler(unsigned int gpio, unsigned int event, void *data);
static void timer_callback(unsigned int data);
const char* op_mode_to_string(enum op_mode mode);

/* fops and status structs */
struct file_operations traffic_fops = {
    read: traffic_read,
    write: traffic_write,
};
static struct traffic_mode{
    enum op_mode current_mode;
    int cycle_rate;
    bool green_status;
    bool yellow_status;
    bool red_status;
    bool pedestrian_btn;
} traffic_info;

/* Timer and cycle measurements */
static struct timer_list timer;
static int ncycles;

/* chrdev setup */
static int traffic_major = 61;
static unsigned capacity = 256;
static unsigned bite = 256;
/* Buffer to store data */
static char *nibbler_buffer;
/* length of the current message */
static int nibbler_len;


/* Function to init GPIO pins */
static void gpio_init(void) {
    //Initialize LED pins as outputs
    gpio_request(LED_GREEN, "green");
    gpio_direction_output(LED_GREEN, 0);
    gpio_request(LED_YELLOW, "yellow");
    gpio_direction_output(LED_YELLOW, 0);
    gpio_request(LED_RED, "red");
    gpio_direction_output(LED_RED, 0);

    // Initialize button pins as inputs
    gpio_request(BTN0, "btn0");
    gpio_direction_input(BTN0);
    gpio_request(BTN1, "btn1");
    gpio_direction_input(BTN1);
}


/* For button interrupts and changing states */
static void button_interrupt_handler(unsigned int gpio, unsigned int event, void *data) {
    if (gpio == BTN0 && event == IRQF_TRIGGER_FALLING) {
        switch (traffic_info.current_mode) { // Mode rotation: Normal -> Yellow -> Red
            case Normal:
                traffic_info.current_mode = FlashYellow;
                break;
            case FlashYellow:
                traffic_info.current_mode = FlashRed;
                break;
            case FlashRed:
                traffic_info.current_mode = Normal;
                ncycles = 1;
                break;
            default:
                printk(KERN_ALERT "Mode change failed");
        }
    } else if (gpio == BTN1 && event == IRQF_TRIGGER_FALLING) {
        if (traffic_info.current_mode == Normal && traffic_info.pedestrian_btn == FALSE) {
            traffic_info.pedestrian_btn = TRUE;
        }
    }   
}


/* Control cycles using timer */
static void timer_callback(unsigned int data) {
    switch (traffic_info.current_mode) {
        case Normal:
            if (ncycles < 4) { //Start with 3 cycles of green
                gpio_set_value(LED_RED, 0);
                gpio_set_value(LED_YELLOW, 0);
                gpio_set_value(LED_GREEN, 1);
            } else if (ncycles < 5) { //Then 1 cycle of yellow
                gpio_set_value(LED_RED, 0);
                gpio_set_value(LED_YELLOW, 1);
                gpio_set_value(LED_GREEN, 0);
            } else { //If pedestrian, 5 cycles red & yellow: else, 2 cycles red
                if (traffic_info.pedestrian_btn && ncycles < 9) {
                    gpio_set_value(LED_RED, 1);
                    gpio_set_value(LED_YELLOW, 1);
                    gpio_set_value(LED_GREEN, 0);
                } else if (ncycles < 6) {
                    gpio_set_value(LED_RED, 1);
                    gpio_set_value(LED_YELLOW, 0);
                    gpio_set_value(LED_GREEN, 0);
                } else //This is an extra cycle added to stop light, so removed 1 cycle from 2 prior ifs
                    ncycles = 0;
            }
            ncycles++;
            break;
        case FlashYellow:
            gpio_set_value(LED_RED, 0);
            gpio_set_value(LED_YELLOW, !gpio_get_value(LED_YELLOW));
            gpio_set_value(LED_GREEN, 0);
            break;
        case FlashRed:
            gpio_set_value(LED_RED, !gpio_get_value(LED_RED));
            gpio_set_value(LED_YELLOW, 0);
            gpio_set_value(LED_GREEN, 0);
            break;
    }
    mod_timer(&timer, jiffies + msecs_to_jiffies(traffic_info.cycle_rate));    
}


/* km base functions */
static int kmod_init(void) {
    int result;
    
    /* Registering chr device */
    result = register_chrdev(traffic_major, DEVICE_NAME, &traffic_fops);
    if (result < 0)
    {
        printk(KERN_ALERT
            "Cannot obtain major number (failed to register) %d\n", traffic_major);
        return result;
    }
    
    /* Initialize GPIO pins and button interrupt handlers */
    gpio_init();
    gpio_request_irq(BTN0, button_interrupt_handler, IRQF_TRIGGER_FALLING, "btn0", NULL);
    gpio_request_irq(BTN1, button_interrupt_handler, IRQF_TRIGGER_FALLING, "btn1", NULL);

    /* Start a timer to measure cycle length */
    timer_setup(&timer, timer_callback, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(traffic_info.cycle_rate));
    ncycles = 1;
    
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

    /* Set default traffic light parameters */
    traffic_info.current_mode = Normal;
    traffic_info.cycle_rate = 1;
    traffic_info.pedestrian_btn = false;
    
    return result;

    fail: 
    kmod_exit(); 
    return result;
}


static void kmod_exit(void) {
        
    /* Freeing the major number */
    unregister_chrdev(traffic_major, DEVICE_NAME);

    /* Free timer */
    del_timer(&timer);
    
    /* Freeing GPIO pins */
    gpio_free(LED_GREEN);
    gpio_free(LED_YELLOW);
    gpio_free(LED_RED);
    gpio_free(BTN0);
    gpio_free(BTN1);
    
    /* Freeing buffer memory */
    if (nibbler_buffer)
    {
        kfree(nibbler_buffer);
    }
}


static ssize_t traffic_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
    
    printk(KERN_ALERT "Chr dev read from\n");

    printk(KERN_ALERT "Current Operational Mode %s\n", op_mode_to_string(traffic_info.current_mode));
    printk(KERN_ALERT "Current Cycle Rate %d\n", traffic_info.cycle_rate);
    
    printk(KERN_ALERT "Current Light Status: \n");
    printk(KERN_ALERT "Green: %s\n", traffic_info.green_status ? "on" : "off");
    printk(KERN_ALERT "Yellow: %s\n", traffic_info.yellow_status ? "on" : "off");
    printk(KERN_ALERT "Red: %s\n", traffic_info.red_status ? "on" : "off");

    printk(KERN_ALERT "Pedestrian Present?\n");
    printk(KERN_ALERT "%s\n", traffic_info.pedestrian_btn ? "Yes" : "No");

    return count;
}


static ssize_t traffic_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {

    int temp;
    char tbuf[256], *tbptr = tbuf;
    printk(KERN_ALERT "Chr dev written to\n");

    /* end of buffer reached */
    if (*f_pos >= capacity)
    {
        printk(KERN_INFO
            "write called: process id %d, command %s, count %d, buffer full\n",
            current->pid, current->comm, count);
        return -ENOSPC;
    }

    /* do not eat more than a bite */
    if (count > bite) count = bite;

    /* do not go over the end */
    if (count > capacity - *f_pos)
        count = capacity - *f_pos;

    /* Copy data from user buffer to kernel buffer */
    if (copy_from_user(nibbler_buffer + *f_pos, buf, count)) {
        return -EFAULT;  // Error copying data from user space
    }

    for (temp = *f_pos; temp < count + *f_pos; temp++)
    {
        tbptr += sprintf(tbptr, "%c", nibbler_buffer[temp]);
    }  

    *f_pos += count;
    nibbler_len = *f_pos;

    printk(KERN_INFO "TBUF STORED INFO: %s\n", tbuf);

    /* use tbuf value to set cycle_rate, will change to type int later */
    // Be sure to set cycle_rate in milliseconds here
    
    return count;
}


/* km helper functions */
const char* op_mode_to_string(enum op_mode mode) {
    switch (mode) {
    case Normal:
        return "Normal";
    case FlashRed:
        return "FlashRed";
    case FlashYellow:
        return "FlashYellow";
    default:
        return "Unknown";
    }
}


/*
GPIO Pinout
Red Light: 67
Yellow Light: 68 
Green Light: 44

Button0 (switch modes): 26
Button1 (pedestrian): 46
*/

/*
Important GPIO functions

int gpio_request(unsigned int gpio, const char *label); //init
void gpio_free(unsigned int gpio); //exit
int gpio_direction_input(unsigned int gpio);
int gpio_direction_output(unsigned int gpio, int value);
int gpio_get_value(unsigned int gpio); //get input pin
void gpio_set_value(unsigned int gpio, int value); //set output pin
int gpio_to_irq(unsigned int gpio); //generate interrupt
*/


module_init(kmod_init);
module_exit(kmod_exit);
