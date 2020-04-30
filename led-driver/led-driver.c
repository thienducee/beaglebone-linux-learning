#include "linux/kernel.h"
#include "linux/init.h"
#include "linux/cdev.h"
#include "linux/device.h"
#include "linux/uaccess.h"
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
//< The license type -- this affects runtime behavior
MODULE_LICENSE("GPL");
//
/////< The author -- visible when you use modinfo
MODULE_AUTHOR("thienducee");
//
/////< The description -- see modinfo
MODULE_DESCRIPTION("Led driver AM35xx");
//
/////< The version of the module
MODULE_VERSION("0.1");


#define DEVICE_NAME "led1"
#define SUCCESS 0
#define FAILED !SUCCESS


#define GPIO1_START 0x4804C000
#define GPIO1_END 0x4804e000
#define GPIO1_SIZE (GPIO1_END - GPIO1_START)
#define GPIO_OE 0x134
#define GPIO_SETDATAOUT 0x194
#define GPIO_CLEARDATAOUT 0x190
#define USER_LED1 13


static dev_t dev;
static struct cdev *my_cdev;
static struct class *dev_class;
static struct device *my_dev;
static int count;

static void __iomem *gpioReg;



static int set_output_io(unsigned char pin_num) {
	    printk(KERN_INFO "LED DRIVER set output pin %d\n", pin_num);
        unsigned long temp = ioread32(gpioReg + GPIO_OE);
        temp &= ~(0x01 << pin_num);
        iowrite32 (temp , gpioReg + GPIO_OE);
        return 0;

}

static int led_on (unsigned char pin_num) {
        printk(KERN_INFO "LED DRIVER set High level pin %d\n", pin_num);
        unsigned long temp = ioread32(gpioReg + GPIO_CLEARDATAOUT);
        temp &= ~(0x01 << pin_num);
        iowrite32 (temp , gpioReg + GPIO_CLEARDATAOUT);


        temp = ioread32(gpioReg + GPIO_SETDATAOUT);
        temp |= (0x01 << pin_num);
        iowrite32 (temp , gpioReg + GPIO_SETDATAOUT);

        return 0;
}

static int led_off (unsigned char pin_num) {
	    printk(KERN_INFO "LED DRIVER set low level pin %d\n", pin_num);
        unsigned long temp = ioread32(gpioReg + GPIO_SETDATAOUT);
        temp &= ~(0x01 << pin_num);
        iowrite32 (temp , gpioReg + GPIO_SETDATAOUT);

        temp = ioread32(gpioReg + GPIO_CLEARDATAOUT);
        temp |= (0x01 << pin_num);
        iowrite32 (temp , gpioReg + GPIO_CLEARDATAOUT);
        return 0;
}


static int dev_open(struct inode *inodep, struct file *filep) {
        printk(KERN_INFO "LED DRIVER %s, %d\n", __func__, __LINE__);
        count = 0;
        return SUCCESS;
}


static ssize_t dev_read(struct file *filep, char __user *buff, size_t len, loff_t *offset) {
    if (count == 0) {
            printk(KERN_INFO "LED DRIVER reading\n");
            count = 1;
            unsigned long temp = ioread32(gpioReg + GPIO_SETDATAOUT);
            unsigned long value = temp & (0x0001 << USER_LED1);
            if(value) {
            	if(copy_to_user(buff, "1", 1)) {
                    printk(KERN_ERR "LED DRIVER copy_to_user failed\n");
                    return FAILED;
            	}
            } else {
            	if(copy_to_user(buff, "0", 1)) {
                    printk(KERN_ERR "LED DRIVER copy_to_user failed\n");
                    return FAILED;
            	}
            }
            
            return strlen("LED DRIVER reading\n");
    }
    return SUCCESS;
}

static ssize_t dev_write(struct file *filep, const char __user *buff, size_t len, loff_t *offset) {
	unsigned char *value_array = kmalloc(sizeof(char) * len, GFP_KERNEL);
	if (count == 0) {
		count = 1;
		if(copy_from_user(value_array, buff, len)) {
        printk(KERN_ERR "LED DRIVER copy_from_user failed\n");
        return FAILED;
        } 

        if(*value_array == '0') {
        	led_off(USER_LED1);
        } else
        	led_on(USER_LED1);

        kfree(value_array);
        return len;
	}
    return SUCCESS;
}


static int dev_close(struct inode *inodep, struct file *filep)
{
        printk("LED DRIVER close\n");
        return SUCCESS;
}

static struct file_operations fops = {
        .open = dev_open,
        .read = dev_read,
        .release = dev_close,
        .write = dev_write,
};


static int __init exam_start(void) {
        printk(KERN_INFO "LED DRIVER init %s, %d", __func__, __LINE__);
        if(alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME)) {
                printk(KERN_ERR "LED DRIVER can not resgister major no\n");
                goto error;
        }
        printk(KERN_INFO "LED DRIVER Resgister device successfully!\n");
        my_cdev = cdev_alloc();
        my_cdev->owner = THIS_MODULE;
        my_cdev->ops = &fops;
        my_cdev->dev = dev;
        cdev_add(my_cdev, dev, 1);
        dev_class = class_create(THIS_MODULE, DEVICE_NAME);
        if(dev_class == NULL) {
                printk(KERN_ERR "LED DRIVER create class device failed\n");
                goto fail_to_create_class;
        }
        my_dev = device_create(dev_class, NULL, dev, NULL, DEVICE_NAME);
        if(IS_ERR(my_dev)) {
                printk(KERN_ERR "LED DRIVER create device failed\n");
                goto fail_to_create_device;
        }
        // setup gpio 
        gpioReg = ioremap(GPIO1_START, GPIO1_SIZE);
        if(gpioReg == NULL)     {
                pr_alert("LED DRIVER fail map base address\n");
                return -1;
        }
        set_output_io(USER_LED1);
        led_on(USER_LED1);
        return SUCCESS;
        fail_to_create_class:
                class_destroy(dev_class);
        fail_to_create_device:
                device_destroy(dev_class, dev);
        error:
                return FAILED;
}

static void __exit exam_stop(void) 
{
        printk(KERN_INFO "LED DRIVER exit %s, %d", __func__, __LINE__);
        led_off(USER_LED1);
        cdev_del(my_cdev);
        device_destroy(dev_class, dev);
        class_destroy(dev_class);
        unregister_chrdev_region(dev, 0);
}

module_init(exam_start);
module_exit(exam_stop);

