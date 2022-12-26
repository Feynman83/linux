// SPDX-License-Identifier: GPL-2.0
/*
 * Sample kobject implementation
 *
 * Copyright (C) 2004-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2007 Novell Inc.
 */
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/slab.h>
#include <linux/platform_device.h>




extern int adc128s022_xfer(int id);
extern int ADC128BUF[];
#define MAX_XBR_ATTR_NUM (64)


static struct kobj_attribute io_attribute[MAX_XBR_ATTR_NUM];
static struct attribute *attrs_xbro[MAX_XBR_ATTR_NUM];

static int attr_gpios[MAX_XBR_ATTR_NUM];
static struct attribute_group attr_group_xbro = {
	.attrs = attrs_xbro,
};

static struct kobject *xbrother_kobj;
static int gpio_num = 0;
static int attr_num = 0;



static ssize_t analog_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	int i;
	int val=0;
	int gpio_val=0;
	for ( i = gpio_num; i < MAX_XBR_ATTR_NUM; i++)
	{
		if (attr==&io_attribute[i]){

			adc128s022_xfer(2);
			gpio_val=gpio_get_value_cansleep(attr_gpios[i-8]);
			if(gpio_val==0){ //current mode
				val=ADC128BUF[2*8+i-gpio_num]*10000/1758;
			}else{ //voltage mode
				val=ADC128BUF[2*8+i-gpio_num]*10/7;
			}

			return sprintf(buf, "%d.%d%d%d\n", val/1000, (val%1000)/100, (val%100)/10, val%10);
		}
	}	
	return sprintf(buf, "%d\n", 0);
}



static ssize_t gpio_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	int i;
	for ( i = 0; i < gpio_num; i++)
	{
		if (attr==&io_attribute[i]){
			return sprintf(buf, "%d\n", gpio_get_value_cansleep(attr_gpios[i]));
		}
	}	
	return sprintf(buf, "%d\n", 0);
}

static ssize_t gpio_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int var, ret,i;

	ret = kstrtoint(buf, 10, &var);
	if (ret < 0)
		return ret;

	for ( i = 0; i < gpio_num; i++)
	{
		if (attr==&io_attribute[i]){
			gpio_set_value_cansleep(attr_gpios[i], !!var);
			break;
		}
	}	
	return count;
}

static struct of_device_id gpio_export_ids[] = {
	{ .compatible = "gpio-export" },
	{ /* sentinel */ }
};
static int __init of_gpio_export_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cnp;
	u32 val;

    struct class * xbrother_class;
    struct device *gpio_dev;
    int err=0;
    const char *sys_name = NULL;
    
    xbrother_class = class_create(THIS_MODULE, "xbrother");
    gpio_dev = device_create(xbrother_class, NULL, 0, NULL, "gpio");

    of_property_read_string(np, "sys_name", &sys_name);
  
    if(err){
        dev_info(&pdev->dev, "sysfs_create_link:%d\n", err);
        return err;
    }
	for_each_child_of_node(np, cnp) {
		
		const char *name = NULL;
		
		int gpio;
		bool dmc;
		int i;
		unsigned flags = 0;
		enum of_gpio_flags of_flags;
		of_property_read_string(cnp, "gpio-export,name", &name);
		
		
		gpio = of_get_gpio_flags(cnp, i, &of_flags);
		if(!gpio_is_valid(gpio)){ //ai
			
			io_attribute[attr_num].attr.name=name;
			io_attribute[attr_num].attr.mode= VERIFY_OCTAL_PERMISSIONS(0444);
			io_attribute[attr_num].show=analog_show;
			io_attribute[attr_num].store=NULL;
			attrs_xbro[attr_num]=&io_attribute[attr_num].attr;
			attr_num++;
			continue;
		}
			
		if (of_flags == OF_GPIO_ACTIVE_LOW)
			flags |= GPIOF_ACTIVE_LOW;
		if (!of_property_read_u32(cnp, "gpio-export,output", &val))
			flags |= val ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		else
			flags |= GPIOF_IN;
		if (devm_gpio_request_one(&pdev->dev, gpio, flags, name ? name : of_node_full_name(np)))
			continue;
		dmc = of_property_read_bool(cnp, "gpio-export,direction_may_change");
		gpio_export(gpio, dmc);
		if(name){
			attr_gpios[attr_num]=gpio;
			io_attribute[attr_num].attr.name=name;
			if(flags & GPIOF_IN){
				io_attribute[attr_num].attr.mode= VERIFY_OCTAL_PERMISSIONS(0444);
			}else{
				io_attribute[attr_num].attr.mode= VERIFY_OCTAL_PERMISSIONS(0644);
			}
			io_attribute[attr_num].show=gpio_show;
			io_attribute[attr_num].store=gpio_store;
			attrs_xbro[attr_num]=&io_attribute[attr_num].attr;
		}
		gpio_num++;
		attr_num++;
		
	}
	attrs_xbro[attr_num]=NULL;

	if(!sys_name) {
        xbrother_kobj = kobject_create_and_add("xbrother", NULL);
    }else{
        xbrother_kobj = kobject_create_and_add(sys_name, NULL);
    }
    
	
	if (!xbrother_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	err = sysfs_create_group(xbrother_kobj, &attr_group_xbro);
	if (err)
		kobject_put(xbrother_kobj);


	dev_info(&pdev->dev, "%d gpio(s) exported\n", gpio_num);
	return 0;
}
static struct platform_driver gpio_export_driver = {
	.driver		= {
		.name		= "gpio-export",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(gpio_export_ids),
	},
};
static int __init xbrother_gpio_init(void)
{
	return platform_driver_probe(&gpio_export_driver, of_gpio_export_probe);
}
late_initcall(xbrother_gpio_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Feynman <lifeng@xbrother.com>");
