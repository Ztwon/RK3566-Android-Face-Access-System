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

#define rc522_class "NFC_class"
#define rc522_device "nfc"

/*  rc522结构体
    open
        注入结构体
        rst
    read
        spi_read
            spi_read_one
    write
        spi_write
            spi_write_one
    结构体fops
        open
        read
        write
        relase
    匹配表
    驱动 driver
        .probe
        .table
    
    probe
        找dts结点
        申请gpio结点 并给结构体
        export暴露给用户（可选
        申请设备号
        初始化cdev设备
        创建NFC class
        将设备放到class下
    init
        spi_register_driver
    exit
    MODULE()
*/


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


typedef struct{
	struct device_node *node; //设备树结点
	struct cdev cdev;
	int rstpin; //重置脚
	dev_t dev_id;
	struct class * class;
	struct device device;
	struct spi_device *spi; /*spi设备*/

}rc522_typdef;

struct rc522_typdef rc522_dev;

void rst_enable(){
	//错误
	//rc522_dev.rstpin = 0;

	gpio_set_value(rc522_dev.rstpin, 0); 
}

void rst_disable(){
	//错误
	//rc522_dev.rstpin = 1;

	gpio_set_value(rc522_dev.rstpin, 1); 
}

static int rc522_open(struct inode *inode, struct file *file) //1.注入file->private_data 2.rst
{
	//确保每个rc522_dev分开
	file -> private_data = &rc522_dev;
	rst_disable();
	udelay(10);
	rst_enable();
	udelay(10);
	rst_disable();

	printk("rc522.dev is ok!");
	return 0;
}

static ssize_t rc522_write(struct file *file, const char __user *user,size_t length, loff_t *offset)
{
	unsigned char * buf;
	buf = (unsigned char *)kzalloc(sizeof(char)*length, GFP_KERNEL);
	if(copy_from_user(buf,user,length)){
		printk("copy error!");
		return -1;
	}
	write_one_reg(&rc522_dev,buf[0],buf[1]);
	
	return 0;
}

static int rc522_release(struct inode* inode ,struct file *filp){
	rst_disable();
	//gpio_free(rc522_dev.rstpin);
	printk("release ok!");
	return 0;
}


struct file_operations rc522_fops={
	.owner		= THIS_MODULE,
	.open  		= rc522_open,
	.write 		= rc522_write,
	.release 	= rc522_release,
};

static const struct of_device_id rc522_match[] = {
	{.compatible = "firefly,rc522"},
	{},
};

static int rc522_probe(struct spi_device *spi){

	int count = 1;
	int ret;
	
	printk("probe ok!");
	rc522_dev.node = of_find_node_by_path("/spi@fe640000/rc522@0");
	rc522_dev.rstpin = of_get_named_gpio(rc522_dev.node,"rst-gpio",0);
	gpio_request(rc522_dev.rstpin,"spi-rst");
	
	gpio_direction_output(rc522_dev.rstpin, 1);
	gpio_export(rc522_dev.rstpin,1); // 1 - > 代表用户有修改方向的权限

	//申请设备号
	ret = alloc_chrdev_region(&rc522_dev.dev_id, 0, count, "rc522");
	if (ret < 0){ 
		return ret;  
	}
	//初始化cdev设备
	cdev_init(&rc522_dev.cdev, &rc522_fops);
	//设备加入cdev 由cdev统一管理
	cdev_add(&rc522_dev.cdev, rc522_dev.dev_id, 1);

	//创建NFC_class
	rc522_dev.class = class_create(THIS_MODULE, rc522_class);
	//把设备挂在class下面
	rc522_dev.device = device_create(rc522_dev.class, NULL, rc522_dev.dev_id, NULL, rc522_device);
	
	rc522_dev.spi = spi;
	spi_setup(rc522_dev.spi);   
  	return 0;
}



static int rc522_remove(struct spi_device *spi){
	device_destroy(rc522_dev.class, rc522_dev.dev_id);
	class_destroy(rc522_dev.class);
	cdev_del(rc522_dev.cdev);
	unregister_chrdev_region(rc522_dev.dev_id, 1);
	gpio_free(rc522_dev.rstpin);
	return 0;
		
}
static int __init rc522_init(void){
	//init可能会因为内存不足失败 所以需要检查
	int ret;
	
	ret = spi_register_driver(&rc522_driver);

	if(ret){
		printk("init error");
		return -1
	}
	return 0;
}

static void __exit  rc522_exit(void){
	//卸载由内核保证 所以无需检查 一定安全
	spi_unregister_driver(&rc522_driver);
	return 0;
}

static struct spi_driver rc522_driver = {
	.probe		= rc522_probe,
	.remove		= rc522_remove,
	.driver = {
		.owner			= THIS_MODULE,
		.name			= "rc522",
		.of_match_table = rc522_match,
	},
};

MODULE_LICENSE("GPL");
module_init(rc522_init);
module_exit(rc522_exit);





