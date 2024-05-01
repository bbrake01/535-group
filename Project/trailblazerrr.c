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
#include <linux/ktime.h>
#include <linux/delay.h>

#define CM_PER_BASE 0x44E10000// Base address for Control Module peripherals
#define MCASP_BASE 0x48038  // Base address for MCASP
#define MCASP0_FSR_OFFSET 0x9a4
#define MCASP0_AXR1_OFFSET 0x9a8
#define MCASP0_ACLKR_OFFSET 0x9a0
#define MCASP_ACLKR_CTRL_VAL (0x200 | (11 << 4))
#define MCASP_FSR_CTRL_VAL (0x100) // Enable internal frame sync generation
#define MCASP_AXR1_CTRL_VAL (0x01 | (0x20))  // Enable AXR as receiver and set I2S mode, adjust 0x10 based on exact format requirement

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BeagleBone Controller for Sound Activated RC Car");
#define DEBUG 1
#define DEVICE_NAME "TRAILBLAZER"
#define THRESHOLD 20

/* Define GPIO pinout */
#define DOUT 116
#define LEDR 49
#define LEDL 117
#define TRIG_PIN 51
#define ECHO_PIN 48
#define ACLKR 114
#define FSR 115
#define AXR 116

/* function Declarations */ 
static void set_pin_mode(u32 offset, u8 mode);
static int kmod_init(void);
static void kmod_exit(void);
static void gpio_init(void);
static void timer_callback(struct timer_list* t);
static irqreturn_t echo_isr(int irq, void *data);
static void init_echo_irq(void);
static void init_obstacle_irq(void);
static void sound_direction(void);
static void config_mcasp(void);


/* struct & global var declarations */
static struct timer_list timer;
static ktime_t echo_start, echo_end;
static bool measuring = false;
static int irq_echo;
static int distance;
void __iomem *mcasp_base;



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

static irqreturn_t echo_isr(int irq, void *data) {
    s64 time_elapsed;
    
    if (gpio_get_value(ECHO_PIN)) {
        // Rising edge detected, start the timer
        echo_start = ktime_get();
        measuring = true;
    } else if (measuring) {
        // Falling edge detected, stop the timer and calculate distance
        echo_end = ktime_get();
        time_elapsed = ktime_to_us(ktime_sub(echo_end, echo_start));
        distance = (int)(time_elapsed * 340);
        distance = distance/20000; // Speed of sound is 340 m/s and divide by 2 (to and fro)
        #if DEBUG
        printk(KERN_ALERT "Distance: %d cm\n", distance);
        #endif
        measuring = false;
    }
    return IRQ_HANDLED;
}

static void init_echo_irq(void) {
    int result;
    irq_echo = gpio_to_irq(ECHO_PIN);
    result = request_irq(irq_echo, echo_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "echo_irq_handler", NULL);
    if (result) {
        printk(KERN_ERR "Unable to request IRQ: %d\n", result);
    }
}

static void gpio_init(void) {

    //mic
    set_pin_mode(MCASP0_FSR_OFFSET, 6);
    set_pin_mode(MCASP0_AXR1_OFFSET, 3);
	set_pin_mode(MCASP0_ACLKR_OFFSET, 3);

	//LEDs
	gpio_request(LEDR, "LEDright");
	gpio_direction_output(LEDR, 0);
	gpio_request(LEDL, "LEDleft");
	gpio_direction_output(LEDL, 0);

    //distance sensor
    gpio_request(TRIG_PIN, "trig");
    gpio_direction_output(TRIG_PIN, 0);
    gpio_request(ECHO_PIN, "echo");
    gpio_direction_input(ECHO_PIN);
    init_echo_irq();
}

static void config_mcasp(void)
{   
    mcasp_base = ioremap(0x48038000, 0x2000);
    writel(MCASP_ACLKR_CTRL_VAL, mcasp_base + MCASP0_ACLKR_OFFSET);  // Configure clock
    writel(MCASP_FSR_CTRL_VAL, mcasp_base + MCASP0_FSR_OFFSET);      // Configure frame sync
    writel(MCASP_AXR1_CTRL_VAL, mcasp_base + MCASP0_AXR1_OFFSET);    // Enable AXR1 as receiver
}

static void sound_direction(void)
{
    unsigned int value;
    value = readl(mcasp_base + MCASP0_AXR1_OFFSET);
    printk(KERN_ALERT, "mic reading: %d", value);
    if(value)
    {
        gpio_set_value(LEDR, 1);
    }
    
}

static int kmod_init(void) {
	
	printk(KERN_ALERT "Initializing module\n");

    gpio_init();

    timer_setup(&timer, timer_callback, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(500));
    config_mcasp();

    printk(KERN_ALERT "Module loaded\n");
    
    return 0;
}

static void timer_callback(struct timer_list *t)
{
    // Trigger HC-SR04
    gpio_set_value(TRIG_PIN, 1);
    udelay(10);
    gpio_set_value(TRIG_PIN, 0);
	gpio_set_value(LEDR, 0);
    gpio_set_value(LEDL, 0);
	
    while(distance<THRESHOLD)
    {
        gpio_set_value(LEDR, !gpio_get_value(LEDR));
	    gpio_set_value(LEDL, !gpio_get_value(LEDL));
        gpio_set_value(TRIG_PIN, 1);
        udelay(10);
        gpio_set_value(TRIG_PIN, 0);
    }

	sound_direction();
    
	mod_timer(&timer, jiffies + msecs_to_jiffies(500));  
}

static void kmod_exit(void) {
    free_irq(irq_echo, NULL);
    gpio_free(TRIG_PIN);
    gpio_free(ECHO_PIN);
    gpio_free(LEDR);
    gpio_free(LEDL);
    iounmap(mcasp_base);
    printk(KERN_ALERT "Exiting module\n");
}


module_init(kmod_init);
module_exit(kmod_exit);
