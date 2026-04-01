#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/fs.h>
#include <linux/pwm.h>

#define DEVICE_NAME "pwm_device"
#define PWM_CHANNEL 1
#define TASK_PROC_NAME "door"

static struct pwm_device *my_pwm;
static int door_value = 0;

static int door_open(struct inode *inode, struct file *file)
{
    return 0;
}

//length 是用户输入的长度
static ssize_t door_read(struct file*file, char __user*user_buffer, size_t length,loff_t *offset)
{
    return 0;
}

static ssize_t door_write(struct file*file, const char __user *user_buffer,size_t length,loff_t *offset)
{
    char buffer[16];
        int ret = 0;
    
    if(length > sizeof(buffer) - 1){
        length = sizeof(buffer) - 1;
    }

    if(copy_from_user(buffer,user_buffer,length)){
        return -EFAULT;
    }

    buffer[length] = '\0';

    ret = kstrtoint(buffer,10,&door_value);  //转为十进制
        if(ret < 0){
            printk(KERN_ERR "Error converting string to int: %d\n", ret);
            return -1;
        }

            if(door_value ==1){
                printk(KERN_ERR "receive 1 \n");
                pwm_config(my_pwm,10 * 20000000 / 100 , 20000000);
                pwm_enable(my_pwm);

                msleep(1500); //开关延迟时间

                pwm_config(my_pwm, 4 * 20000000 / 100 , 20000000);
                pwm_enable(my_pwm);
            }
        return length;
}

static const struct file_operations door_ops = {
        .owner = THIS_MODULE,
        .open  = door_open,
        .read  = door_read,
        .write = door_write, 
};

static int __init door_init(void)
{   //创建proc结点使用户访问
    static struct proc_dir_entry *ent;
    static struct proc_dir_entry *entry_1;
    umode_t perm = 0666;

    //获取PWM设备
    my_pwm = pwm_request(PWM_CHANNEL,DEVICE_NAME);
    if(IS_ERR(my_pwm)){
        printk(KERN_ALERT "Failed to request PWM %d\n",PWM_CHANNEL);
        return PTR_ERR(my_pwm);
    }

    ent = proc_mkdir(TASK_PROC_NAME,NULL);
    if(ent == NULL)
        return -ENOMEM;

    entry_1 = proc_create("state",perm,ent,&door_ops);
    if(entry_1 == NULL){
        printk("%s failed tp create proc file 1\n",__func__);
        return -ENODEV;
    }
    return 0;
}

static void __exit door_exit(void)
{
    remove_proc_entry("state", ent);
    remove_proc_entry(TASK_PROC_NAME,NULL);
    printk("%s\n",__func__);
}

module_init(door_init);
module_exit(door_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple Proc File Example");
MODULE_ALIAS("door");
