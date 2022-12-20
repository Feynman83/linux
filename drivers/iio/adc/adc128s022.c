/*
################################################################################
#                                                                              #
# Copyright (c) 2017 Xbrother Technologies, Inc.                                    #
#                                                                              #
# This program is free software: you can redistribute it and/or modify it      #
# under the terms of the GNU General Public License as published by the Free   #
# Software Foundation, either version 2 of the License, or (at your option)    #
# any later version.                                                           #
#                                                                              #
# This program is distributed in the hope that it will be useful, but WITHOUT  #
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        #
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    #
# more details. You should have received a copy of the GNU General Public      #
# License along with this program.  If not, see <http://www.gnu.org/licenses/>.#
#                                                                              #
# Xbrother Technologies, Inc.                                                       #
# 1F, 531, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.                     #
#                                                                              #
################################################################################

Device Tree:

&ecspi3 {
	
	...
	
	adc128s022_0@0 {
		compatible = "adc128s022";
		spi-max-frequency = <125000>;
		reg = <0>;
	};
};
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/spi/spi.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>

#define DBG(x...) ;
#define ERR(x...) printk(KERN_ERR x)
#define INFO(x...) printk(x)

#define MAX_XFER_NUM 8
#define MAX_READ_BUF 300
#define DEV_NUM 4

#if 0
#define ADC_CHANNEL0 0x00
#define ADC_CHANNEL1 0x08
#define ADC_CHANNEL2 0x10
#define ADC_CHANNEL3 0x18
#define ADC_CHANNEL4 0x20
#define ADC_CHANNEL5 0x28
#define ADC_CHANNEL6 0x30
#define ADC_CHANNEL7 0x38
#else

#define ADC_CHANNEL0 0x08
#define ADC_CHANNEL1 0x10
#define ADC_CHANNEL2 0x18
#define ADC_CHANNEL3 0x20
#define ADC_CHANNEL4 0x28
#define ADC_CHANNEL5 0x30
#define ADC_CHANNEL6 0x38
#define ADC_CHANNEL7 0x00
#endif

int ADC128BUF[8*DEV_NUM];

struct adc128s022_dat_t {
	struct spi_device *spidev;
	struct spi_message msg;
	struct spi_transfer xfer[MAX_XFER_NUM];
	unsigned char tx[MAX_XFER_NUM*2];
	unsigned char rx[MAX_XFER_NUM*2];
	char readbuf[MAX_READ_BUF];
	unsigned int readlen;
	unsigned int readptr;
	int valid;
};

static struct adc128s022_dat_t g_adc128s022dat[DEV_NUM];

void PrintHEX(u8 *buff,u16 len,const char *s)
{
	u16 i;
	if(s != NULL)
	{
		printk("%s",s);	
	}
	for(i = 0;i < len;i++)
	{
		printk("%02X ",buff[i]);
	}
	printk("\n");
}
/* =============================================================================
 *         spi transfer api
 * =============================================================================
 */
unsigned int adc128s022_conv12(unsigned char *val){
	unsigned int ret, tmp;
	#if 0
	ret = (unsigned int)val[1];
	ret = ret >> 3; /* 5-bit at byte 2 */
	tmp = (unsigned int)val[0];
	tmp = tmp << 5; /* 7-bit at byte 1  */
	ret |= tmp;
	ret &= 0xFFF; /* total 12-bit data */
	#else
	tmp = (*val)&0x0F;
	tmp = tmp<<8;
	ret =  tmp + val[1];
	#endif
	
	
	return ret;
}

int adc128s022_xfer(int id){
	struct adc128s022_dat_t *datp;
	
	int val0, val1, val2, val3, val4,val5, val6, val7;

	/* reset all CS (chip select) to inactive (high), fsl bsp keeps CS
	   active after transmission. */
	   /*
	for(i=0; i<DEV_NUM; i++){
		if(g_adc128s022dat[i].valid == 0){
			continue;
		}
		gpio_direction_output(g_adc128s022dat[i].spidev->cs_gpio, 1);
	}*/
	if(id<0||id>3) return -11;
	datp = &g_adc128s022dat[id];
	/* start transfer */
	if(spi_sync(datp->spidev, &datp->msg) != 0){
		ERR("xfer fail!\n");
		return 1;
	}
	//PrintHEX(&datp->rx[0],16,"adc data:");
	val0 = adc128s022_conv12(&datp->rx[0]);
	val1 = adc128s022_conv12(&datp->rx[2]);
	val2 = adc128s022_conv12(&datp->rx[4]);
	val3 = adc128s022_conv12(&datp->rx[6]);
	val4 = adc128s022_conv12(&datp->rx[8]);
	val5 = adc128s022_conv12(&datp->rx[10]);
	val6 = adc128s022_conv12(&datp->rx[12]);
	val7 = adc128s022_conv12(&datp->rx[14]);
	

	ADC128BUF[id*8+0]=val0;
	ADC128BUF[id*8+1]=val1;
	ADC128BUF[id*8+2]=val2;
	ADC128BUF[id*8+3]=val3;
	ADC128BUF[id*8+4]=val4;
	ADC128BUF[id*8+5]=val5;
	ADC128BUF[id*8+6]=val6;
	ADC128BUF[id*8+7]=val7;
	/* generate readable text */
	//DBG("adc128s022.%d read 1:%d 2:%d 3:%d 4:%d\n", id, xp, yp, aux, ym);
	/*id *= MAX_XFER_NUM;
	datp->readlen = snprintf(datp->readbuf, MAX_READ_BUF, "%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n",
		id+1, val0, id+2, val1, id+3, val2, id+4, val3,id+5, val4, id+6, val5, id+7, val6, id+8, val7);*/
	//if(id==0)
	//datp->readlen = snprintf(datp->readbuf, MAX_READ_BUF, "adc%d\n%d:%d\n%d:%d\n",
	//	id,0, val0, 1, val1);
	//else
	datp->readlen = snprintf(datp->readbuf, MAX_READ_BUF, "adc%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n%d:%d\n",
		id,0, val0, 1, val1, 2, val2, 3, val3,4, val4, 5, val5, 6, val6, 7, val7);

	datp->readptr = 0;
	return 0;
}

int adc128s022_xfer_all(void){
	int i;
	for(i=0; i<DEV_NUM; i++){
		if(g_adc128s022dat[i].valid == 0){
			continue;
		}
		if(adc128s022_xfer(i) != 0){
			return -1;
		}
	}
	return 0;
}

/* =============================================================================
 *         file operations
 * =============================================================================
 */
static int adc128s022_fop_open(struct inode *inode, struct file *file){
	//INFO("adc128s022_fop_open %s\n",file->f_path.dentry->d_name.name);
	#if 0
	if(adc128s022_xfer_all() != 0){
		return -EINVAL;
	}
	#else
	adc128s022_xfer(file->f_path.dentry->d_name.name[3]-0x30);
	#endif
	return 0;
}

static int adc128s022_fop_release(struct inode *inode, struct file *file){
	/* reserve for future use*/
	return 0;
}

static long adc128s022_fop_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	/* not support */
	return -EINVAL;
}

ssize_t adc128s022_fop_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *pos){
	int i, j, nread, cur_read, ofs;
	
	/* go through all device, copy readable text for user */

	//file->f_path.dentry->d_name.name[3]-0x30
	nread = 0;
	for(i=0; i<DEV_NUM; i++){
		struct adc128s022_dat_t *datp = &g_adc128s022dat[i];
		if(datp->valid == 0){
			continue;
		}
		ofs = datp->readptr;
		if(ofs >= datp->readlen){
			continue;
		}
		cur_read = count;
		if((ofs + count) >= datp->readlen){
			cur_read = datp->readlen - ofs;
		}
		if(cur_read <= 0){
			continue;
		}
		for(j=0; j<cur_read; j++){
			put_user(datp->readbuf[ofs++], buf++);
		}
		datp->readptr = ofs;
		nread += cur_read;
		count -= cur_read;
	}
	return nread;
}

ssize_t adc128s022_fop_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos){
	return 0;
}

loff_t adc128s022_fop_seek(struct file *file, loff_t pos, int whence){
	switch(whence) {
	case 0: /* SEEK_SET */
		if(pos != 0){ /* only allow reset to 0, this is not block device */
			return -EINVAL;
		}
		
		#if 0
		if(adc128s022_xfer_all() != 0){
			return -EINVAL;
		}
		#else
		adc128s022_xfer(file->f_path.dentry->d_name.name[3]-0x30);
		#endif
		break;
	default: /* ignore other request */
		return -EINVAL;
	}
	
	return 0;
}

static struct file_operations adc128s022_fops = {
	.owner = THIS_MODULE,
	.open = adc128s022_fop_open,
	.release = adc128s022_fop_release,
	.unlocked_ioctl = adc128s022_fop_ioctl,
	.read = adc128s022_fop_read,
	.write = adc128s022_fop_write,
	.llseek = adc128s022_fop_seek,
};

static struct miscdevice adc128s022_miscdev[4] = {
	{.minor = MISC_DYNAMIC_MINOR,
	.name = "adc0",
	.fops = &adc128s022_fops,},
	{.minor = MISC_DYNAMIC_MINOR,
	.name = "adc1",
	.fops = &adc128s022_fops,},
	{.minor = MISC_DYNAMIC_MINOR,
	.name = "adc2",
	.fops = &adc128s022_fops,},
	{.minor = MISC_DYNAMIC_MINOR,
	.name = "adc3",
	.fops = &adc128s022_fops,},
};

/* =============================================================================
 *         driver functions
 * =============================================================================
 */
int _adc128s022_init(struct spi_device *spidev){
	int i;
	struct adc128s022_dat_t *datp;
	unsigned char *xbuf;
	
	INFO("adc128s022 SPI master%d probe...", spidev->master->bus_num);
	if(spidev->master->bus_num >= DEV_NUM){
		INFO("too many devices!\n");
		return -1;
	}

	datp = &g_adc128s022dat[spidev->master->bus_num];
	datp->valid = 0;
	/* setup spi configuration */
	INFO("init device");
	datp->spidev = spidev;
	spidev->max_speed_hz = 125000;
	spidev->mode = SPI_MODE_0;
	spidev->bits_per_word = 8;
	spidev->irq = -1;
	INFO(".");
	if(spi_setup(spidev) < 0){
		INFO("fail!\n");
		return -1;
	}
	INFO(".");
	/* prepare spi message to communicate xbrother spi */
	for(i=0; i<MAX_XFER_NUM; i++){
		memset(&datp->xfer[i], 0, sizeof datp->xfer[i]);
		datp->xfer[i].len = 2;
		datp->xfer[i].tx_buf = &datp->tx[i*2];
		datp->xfer[i].rx_buf = &datp->rx[i*2];
		datp->tx[i*2] = datp->tx[i*2+1] = 0;
		datp->rx[i*2] = datp->rx[i*2+1] = 0;
		datp->xfer[i].bits_per_word = 8;
		datp->xfer[i].delay_usecs = 5000;
		datp->xfer[i].cs_change = 1;
		datp->xfer[i].speed_hz = 100000;
	}
	INFO(".");
	spi_message_init(&datp->msg);
	datp->msg.spi = spidev;
	datp->msg.is_dma_mapped = 0;
	datp->msg.complete = NULL;
	datp->msg.context = (void*) &datp;
	INFO(".");
	
	/* setup first byte: first byte is channel addr (see datasheet) */
	xbuf = (unsigned char*)datp->xfer[0].tx_buf;
	xbuf[0] = ADC_CHANNEL0;
	spi_message_add_tail(&datp->xfer[0], &datp->msg);
	xbuf = (unsigned char*)datp->xfer[1].tx_buf;
	xbuf[0] = ADC_CHANNEL1;
	spi_message_add_tail(&datp->xfer[1], &datp->msg);
	xbuf = (unsigned char*)datp->xfer[2].tx_buf;
	xbuf[0] = ADC_CHANNEL2;
	spi_message_add_tail(&datp->xfer[2], &datp->msg);
	xbuf = (unsigned char*)datp->xfer[3].tx_buf;
	xbuf[0] = ADC_CHANNEL3;
	spi_message_add_tail(&datp->xfer[3], &datp->msg);
	xbuf = (unsigned char*)datp->xfer[4].tx_buf;
	xbuf[0] = ADC_CHANNEL4;
	spi_message_add_tail(&datp->xfer[4], &datp->msg);
	xbuf = (unsigned char*)datp->xfer[5].tx_buf;
	xbuf[0] = ADC_CHANNEL5;
	spi_message_add_tail(&datp->xfer[5], &datp->msg);
	xbuf = (unsigned char*)datp->xfer[6].tx_buf;
	xbuf[0] = ADC_CHANNEL6;
	spi_message_add_tail(&datp->xfer[6], &datp->msg);
	xbuf = (unsigned char*)datp->xfer[7].tx_buf;
	xbuf[0] = ADC_CHANNEL7;
	spi_message_add_tail(&datp->xfer[7], &datp->msg);
	
	INFO("done! cs_gpio=%d\n", spidev->cs_gpio);
	datp->valid = 1;
	return 0;
}

static int adc128s022_probe(struct spi_device *spidev){
	if(_adc128s022_init(spidev) != 0){
		return -1;
	}
	if(misc_register(&adc128s022_miscdev[spidev->master->bus_num]) != 0){
		DBG("register miscdev failed, may already registered\n");
	}
	return 0;
}

static int adc128s022_remove(struct spi_device *spidev){
	INFO("adc128s022 remove\n");
	misc_deregister(&adc128s022_miscdev[spidev->master->bus_num]);
	return 0;
}

static struct spi_driver adc128s022_drv = {
	.probe = adc128s022_probe,
	.remove = adc128s022_remove,
	.driver = {
		.name = "adc128s022",
		.owner = THIS_MODULE,
	},
};  

module_spi_driver(adc128s022_drv);

MODULE_AUTHOR("Sai Wu");
MODULE_DESCRIPTION("adc128s022 ADC driver");
MODULE_LICENSE("GPL");

