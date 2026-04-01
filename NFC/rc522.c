#include <linux/module.h>//模块加载卸载函数
#include <linux/kernel.h>//内核头文件
#include <linux/types.h>//数据类型定义
#include <linux/fs.h>//file_operations结构体
#include <linux/device.h>//class_create等函数
#include <linux/ioctl.h>
#include <linux/kernel.h>/*包含printk等操作函数*/
#include <linux/of.h>/*设备树操作相关的函数*/
#include <linux/gpio.h>/*gpio接口函数*/
#include <linux/of_gpio.h>
#include <linux/platform_device.h>/*platform device*/
#include <linux/spi/spi.h> /*spi相关api*/
#include <linux/delay.h> /*内核延时函数*/
#include <linux/slab.h> /*kmalloc、kfree函数*/
#include <linux/cdev.h>/*cdev_init cdev_add等函数*/
#include <linux/uaccess.h>/*__copy_from_user 接口函数*/
#include "rc522.h"

#define DEVICE_NAME     "nfc" 
 

typedef struct
{
      struct device_node *node;//设备树节点
      struct cdev cdev;       //定义一个cdev结构体
      struct class *class;    //创建一个rc522类
      struct device *device;  //创建一个rc522设备 该设备是需要挂在rc522类下面的
      int major;              //主设备号
      dev_t  dev_id;
      struct spi_device *spi; /*spi设备*/
//     int cspin;              /*片选脚*/
      int rstpin;
      struct mutex lock;
      void *private_data;
}rc522_typdef;

static rc522_typdef rc522_dev;//定义一个rc522设备

void spi_rst_enable(void)
{
    gpio_set_value(rc522_dev.rstpin, 0); 
}

void spi_rst_disable(void)
{
   gpio_set_value(rc522_dev.rstpin, 1); 
}

void spi_cs_enable(void)
{
    //gpio_set_value(rc522_dev.cspin, 1);
}

void spi_cs_disable(void)
{
    //gpio_set_value(rc522_dev.cspin, 0);
}

static int rc522_read_regs(rc522_typdef *dev, unsigned char reg, unsigned char *dat, unsigned char len)
{
    int ret = -1;
    unsigned char txdata[len];
    unsigned char * rxdata;
    struct spi_message m;
    struct spi_transfer *t;
    struct spi_device *spi = dev->spi;
    
    t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);   /* 申请内存 */
    if(!t) {
        return -ENOMEM;
    }

    rxdata = kzalloc((sizeof(char) * len), GFP_KERNEL); /* 申请内存 */
    if(!rxdata) {
        goto out1;
    }
    spi_cs_enable();
    /* 一共发送len+1个字节的数据，第一个字节为
    寄存器首地址，一共要读取len个字节长度的数据，*/
    txdata[0] = ((reg << 1) & 0x7e) | 0x80;             
    t->tx_buf = txdata;         /* 要发送的数据 */
    t->rx_buf = rxdata;         /* 要读取的数据 */
    t->len = len + 1;           /* t->len=发送的长度+读取的长度 */
    spi_message_init(&m);       /* 初始化spi_message */
    spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
    ret = spi_sync(spi, &m);    /* 同步发送 */
    if(ret) {
        goto out2;
    }
    /* 只需要读取的数据 */
    memcpy(dat , rxdata + 1, len); /* 只需要读取的数据 */

out2:
    kfree(rxdata);                  /* 释放内存 */
out1:   
    kfree(t);                       /* 释放内存 */
    spi_cs_disable();
    return ret;
}

static int rc522_write_regs(rc522_typdef *dev, unsigned char reg, unsigned char *dat, unsigned char len)
{
    int ret = -1;
    unsigned char *txdata;
    struct spi_message m;
    struct spi_transfer *t;
    struct spi_device *spi = dev->spi;
    
    t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);   /* 申请内存 */
    if(!t) {
        return -ENOMEM;
    }
    
    txdata = kzalloc(sizeof(char) + len, GFP_KERNEL);
    if(!txdata) {
        goto out1;
    }
    spi_cs_enable();
    /* 一共发送len+1个字节的数据，第一个字节为
    寄存器首地址，len为要写入的寄存器的集合，*/
    *txdata = ((reg << 1) & 0x7e);  /* 写数据的时候首寄存器地址bit8要清零 */
    memcpy(txdata + 1, dat, len);

    t->tx_buf = txdata;         /* 要发送的数据 */
    t->len = len + 1;                   /* t->len=发送的长度+读取的长度 */
    spi_message_init(&m);       /* 初始化spi_message */
    spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
    ret = spi_sync(spi, &m);    /* 同步发送 */
    if(ret) {
        goto out2;
    }
    
out2:
    kfree(txdata);      /* 释放内存 */
out1:
    kfree(t);                   /* 释放内存 */
    spi_cs_disable();
    return ret; 
}

static unsigned char read_one_reg(rc522_typdef *dev, unsigned char reg)
{
    unsigned char data = 0;

    rc522_read_regs(dev, reg, &data, 1);

    return data;    
}

static void write_one_reg(rc522_typdef *dev,unsigned char reg, unsigned char value)
{
    rc522_write_regs(dev, reg, &value, 1);
}

static int rc522_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &rc522_dev;
   
    spi_rst_disable();
    udelay(10);
    spi_rst_enable();
    udelay(10);
    spi_rst_disable();

    printk("rc522_open ok!\n");
    return 0;
}

static int rc522_release(struct inode* inode ,struct file *filp)
{
    spi_rst_enable();
    printk("rc522_release ok!\n");   
    return 0;
}

// loff_t rc522_llseek(struct file *file, loff_t offset, int whence)
// {
//  return 0;
// }

static ssize_t rc522_write(struct file *filp, const char __user *buf, size_t count,loff_t *f_pos)
{
    unsigned char *write_buf;/*数据缓冲区*/
    int ret;

    write_buf = (unsigned char*)kzalloc(count, GFP_KERNEL);
    if(!write_buf )
       return -ENOMEM;

    ret = copy_from_user(write_buf, buf, count);
    if (ret < 0)
    {
        kfree(write_buf);
        printk("copy from user failed!\r\n");
        return ret;
    }

    write_one_reg(&rc522_dev, write_buf[0], write_buf[1]);

    return 0;
}

static ssize_t rc522_read(struct file *filp,char __user *buf, size_t count,loff_t *f_pos)
{
    int ret; 
    unsigned char adr,value; 

    ret = copy_from_user(&adr, buf, 1);
    if(ret < 0)
    {
        printk("copy from user failed!\r\n");
        return ret;
    }
    value = read_one_reg(&rc522_dev, adr);
    ret = copy_to_user(buf, &value, count);
    if (ret < 0)
    {
        printk("copy to user failed!\r\n");
        return ret;
    }

    return ret;
}

static struct file_operations rc522_fops={
    .owner      = THIS_MODULE,
    .open       = rc522_open,
    .write      = rc522_write,
    .read       = rc522_read,
    .release    = rc522_release,
    // .llseek     = rc522_llseek,
};
        
static int rc522_probe(struct spi_device *spi)
{
    int ret;
    const char *string = NULL;
    
    printk("rc522 probe!\n"); 

   /*获取设备节点*/
   rc522_dev.node = of_find_node_by_path("/spi@fe640000/rc522@0");
   if(rc522_dev.node == NULL)
   {
      printk("device-tree:not found rc522!\r\n"); 
      return -1;
   }

   /*读取rc522设备节点的compatible属性值*/
   ret = of_property_read_string(rc522_dev.node, "compatible", &string);
   if(ret == 0)
   {
       printk("%s\n",string);
   }   

   rc522_dev.rstpin = of_get_named_gpio(rc522_dev.node,"rst-gpio",0);
   if(!gpio_is_valid(rc522_dev.rstpin))
   {
        printk("get gpio error\n");
        ret = -EINVAL;
        return ret;
   }
   
   printk("gpio = %d\n",rc522_dev.rstpin);

   ret = gpio_request(rc522_dev.rstpin,"spi-rst");
   if(ret < 0) 
   {
      printk("gpio_request %d failed\n", rc522_dev.rstpin);
      return ret;
   }
   gpio_direction_output(rc522_dev.rstpin, 1);

   gpio_export(rc522_dev.rstpin, 1);

    /*申请设备号*/
    ret = alloc_chrdev_region(&rc522_dev.dev_id, 0, 1, DEVICE_NAME);
    if(ret < 0)
    {
         printk("alloc dev_id error %d\n", ret);
         return ret;
    }

   /*初始化一个cdev*/
   cdev_init(&rc522_dev.cdev, &rc522_fops);

   /*向cdev中添加一个设备*/
   ret = cdev_add(&rc522_dev.cdev, rc522_dev.dev_id, 1);
   if(ret != 0)
   {
        printk("cdev add error %d \n",ret);
        // goto Error;
   }

   /*创建一个nfc_class类*/
   rc522_dev.class = class_create(THIS_MODULE, "nfc_class");
   if(rc522_dev.class == NULL)
   {
      printk("class_create failed\r\n");
      return -1;
   }
   /*在nfc_class类下创建一个NFC_class设备*/
   //用户空间设备节点（位于 /dev 目录下）
   rc522_dev.device = device_create(rc522_dev.class, NULL, rc522_dev.dev_id, NULL, DEVICE_NAME);   

   /*获取与本驱动匹配的spi设备*/
   rc522_dev.spi = spi;
   spi_setup(rc522_dev.spi);   
   
// Error:
//  cdev_del(&rc522_dev.cdev);
//  unregister_chrdev_region(rc522_dev.dev_id, 1);
    return 0;
}

static int rc522_remove(struct spi_device *spi)
{
/*
    printk("w25qxx remove!\n"); 

    cdev_del(&rc522_dev.cdev);

    unregister_chrdev_region(rc522_dev.dev_id, 1);

    device_destroy(rc522_dev.class, rc522_dev.dev_id);

    class_destroy(rc522_dev.class);

    gpio_free(rc522_dev.rstpin);
	*/
	
	device_destroy(rc522_dev.class, rc522_dev.dev_id);

    class_destroy(rc522_dev.class);

    //3 删除 cdev 
    cdev_del(&rc522_dev.cdev);

    // 4 释放设备号 
    unregister_chrdev_region(rc522_dev.dev_id, 1);

    // 5 释放 GPIO
    gpio_free(rc522_dev.rstpin);

    printk("rc522 remove ok!\n");

    return 0;   
}

static const struct of_device_id rc522_of_match[] = {
   {.compatible = "firefly,rc522"},
   {},
};

static const struct spi_device_id rc522_id[] = {
    { "xxxx", 0 },
    {},
};

static struct spi_driver rc522_driver = {
    .driver = {
        .owner = THIS_MODULE,       
        .name =  "rc522",
        .of_match_table = rc522_of_match,
    },
    .probe = rc522_probe,
    .remove = rc522_remove,
    .id_table = rc522_id,
};

static int __init rc522_init(void)
{
   int ret;

   ret = spi_register_driver(&rc522_driver);
   if(ret < 0)
   {
        printk("spi_register_driver error= %d \n",ret);
   }
   else
   {
        printk("module init ok\n");
   }
   return ret;
}

static void __exit rc522_exit(void)
{
    spi_unregister_driver(&rc522_driver);  
    printk("module exit ok\n");
}

module_init(rc522_init);
module_exit(rc522_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("rc522 driver");
MODULE_AUTHOR("lsjml2022");
