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
#define PWM_CHANNEL 1
#define DEVICENAME "pwm_device"

static struct pwm_device *my_pwm;


static int mydoor_open(struct inode *inode, struct file *file){
	return 0;
}

static ssize_t  mydoor_read(struct file*file, char __user*user_buffer, size_t length,loff_t *offset){

	return 0;	
}


static ssize_t  mydoor_write(struct file*file, char __user*user_buffer, size_t length,loff_t *offset){

	char writebuf[16];
	unsigned int value;
	int ret;
	if(length>sizeof(writebuf)){
		length = sizeof(writebuf) - 1;
	}

	if(copy_from_user(writebuf,user_buffer,length)) return -EFAULT;


	//别忘了！！！！！！！！！！！！
	writebuf[length] = '\0';

	ret = kstrtouint(writebuf, 10, &value);

	if(ret < 0){
		printk("kstrtouint errot!");
		return -1;
	}

	if(value == 1){
		pwm_config(my_pwm, 10 * 20000000 / 100, 20000000); /* 10 % @ 50 Hz */
		pwm_enable(my_pwm);

		msleep(1500);

		pwm_config(my_pwm, 10 * 20000000 / 100, 20000000); /* 10 % @ 50 Hz */
		pwm_enable(my_pwm);
	}

	return 0;
	
}







static struct file_operations mydoor_fops = {
	.owner 		= THIS_MODULE,
	.open  		= mydoor_open,
	.read		= mydoor_read,
	.write 		= mydoor_write,
	//.release	= ,
};

static int __init mydoor_init(void){

	my_pwm = pwm_request(PWM_CHANNEL ,DEVICENAME);

	static struct proc_dir_entry *ent;
	static struct proc_dir_entry *ent_1;

	umode_t perm = 0666;

	ent = proc_mkdir("door", NULL);
	if (!ent)  return -ENOMEM;
	ent_1 = proc_create("state", perm, ent, &mydoor_fops);
	if (!ent_1)  return -ENOMEM;
    return 0;

}

static void __exit mydoor_exit(void){

	proc_remove(ent_1);
	proc_remove(ent);
	pwm_put(my_pwm); 
}


module_init(mydoor_init);
module_exit(mydoor_exit);
MODULE_LICENSE("GPL");

