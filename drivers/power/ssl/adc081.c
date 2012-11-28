#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>

static inline unsigned  adc081_vbattery(struct spi_device *spi)
{
	unsigned value =0, vbattery = 0;
	char buff[2] = {0};
	spi_read(spi, buff, 2);
	buff[0] = (buff[0]<<3)&0xF8;
	buff[1] = (buff[1]>>5)&0x07;
	value = buff[1] | buff[0];
	vbattery = (3*value*2200)>>8;
	vbattery += 50; 
	return vbattery;
}

static ssize_t adc081_show(struct spi_device *spi,
                                struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%u\n", adc081_vbattery(spi));
}

static DEVICE_ATTR(in1_input, S_IRUGO, adc081_show, NULL);

static struct attribute *adc081_attributes[] = {
	&dev_attr_in1_input.attr, NULL,
};

static struct attribute_group adc081_attr_group = {
	.attrs = adc081_attributes,
};

static int __devinit adc081_spi_probe(struct spi_device *spi)
{
	int err = sysfs_create_group(&spi->dev.kobj, &adc081_attr_group);
	if (err)
		printk("register sysfs for power is failure.\n"); 
	return 0;
}

static int __devexit adc081_spi_remove(struct spi_device *spi)
{
	sysfs_remove_group(&spi->dev.kobj, &adc081_attr_group);
	return 0;
}

static struct spi_driver adc081_spi_driver = {
	.driver = {
		.name	= "adc081",
		.owner 	= THIS_MODULE,
	},
	.probe		= adc081_spi_probe,
	.remove		= __devexit_p(adc081_spi_remove),
};

static int __init adc081_init(void)
{
	return spi_register_driver(&adc081_spi_driver);
}

static void __exit adc081_exit(void)
{
	spi_unregister_driver(&adc081_spi_driver);
}

module_init(adc081_init);
module_exit(adc081_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("vbattery driver for AG");
