/*
 * Regulator driver for Active-semi act8931 PMIC chip for rk29xx
 *
 * Copyright (C) 2010, 2011 ROCKCHIP, Inc.

 * Based on act8891.c that is work by zhangqing<zhangqing@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/act8931.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/iomux.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <plat/board.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
  
#if 0
#define DBG(x...)	printk(KERN_INFO x)
#else
#define DBG(x...)
#endif
#if 1
#define DBG_INFO(x...)	printk(KERN_INFO x)
#else
#define DBG_INFO(x...)
#endif
#define PM_CONTROL

struct act8931 *g_act8931;

struct act8931 {
	unsigned int irq;
	struct device *dev;
	struct mutex io_lock;
	struct i2c_client *i2c;
	int num_regulators;
	struct regulator_dev **rdev;
	struct early_suspend act8931_suspend;
};

static u8 act8931_reg_read(struct act8931 *act8931, u8 reg);
static int act8931_set_bits(struct act8931 *act8931, u8 reg, u16 mask, u16 val);


#define act8931_BUCK1_SET_VOL_BASE 0x20
#define act8931_BUCK2_SET_VOL_BASE 0x30
#define act8931_BUCK3_SET_VOL_BASE 0x40
#define act8931_LDO1_SET_VOL_BASE 0x50
#define act8931_LDO2_SET_VOL_BASE 0x54
#define act8931_LDO3_SET_VOL_BASE 0x60
#define act8931_LDO4_SET_VOL_BASE 0x64

#define act8931_BUCK1_CONTR_BASE 0x22
#define act8931_BUCK2_CONTR_BASE 0x32
#define act8931_BUCK3_CONTR_BASE 0x42
#define act8931_LDO1_CONTR_BASE 0x51
#define act8931_LDO2_CONTR_BASE 0x55
#define act8931_LDO3_CONTR_BASE 0x61
#define act8931_LDO4_CONTR_BASE 0x65

#define BUCK_VOL_MASK 0x3f
#define LDO_VOL_MASK 0x3f

#define VOL_MIN_IDX 0x00
#define VOL_MAX_IDX 0x3f

#define INSTAT_MASK (1<<5)
#define CHGSTAT_MASK (1<<4)
#define INDAT_MASK (1<<1)
#define CHGDAT_MASK (1<<0)

#define INCON_MASK (1<<5)
#define CHGEOCIN_MASK (1<<4)
#define INDIS_MASK (1<<1)
#define CHGEOCOUT_MASK (1<<0)

int act8931_charge_det, act8931_charge_ok;
EXPORT_SYMBOL(act8931_charge_det);
EXPORT_SYMBOL(act8931_charge_ok);

const static int buck_set_vol_base_addr[] = {
	act8931_BUCK1_SET_VOL_BASE,
	act8931_BUCK2_SET_VOL_BASE,
	act8931_BUCK3_SET_VOL_BASE,
};
const static int buck_contr_base_addr[] = {
	act8931_BUCK1_CONTR_BASE,
 	act8931_BUCK2_CONTR_BASE,
 	act8931_BUCK3_CONTR_BASE,
};
#define act8931_BUCK_SET_VOL_REG(x) (buck_set_vol_base_addr[x])
#define act8931_BUCK_CONTR_REG(x) (buck_contr_base_addr[x])


const static int ldo_set_vol_base_addr[] = {
	act8931_LDO1_SET_VOL_BASE,
	act8931_LDO2_SET_VOL_BASE,
	act8931_LDO3_SET_VOL_BASE,
	act8931_LDO4_SET_VOL_BASE, 
};
const static int ldo_contr_base_addr[] = {
	act8931_LDO1_CONTR_BASE,
	act8931_LDO2_CONTR_BASE,
	act8931_LDO3_CONTR_BASE,
	act8931_LDO4_CONTR_BASE,
};
#define act8931_LDO_SET_VOL_REG(x) (ldo_set_vol_base_addr[x])
#define act8931_LDO_CONTR_REG(x) (ldo_contr_base_addr[x])

const static int buck_voltage_map[] = {
	 600, 625, 650, 675, 700, 725, 750, 775,
	 800, 825, 850, 875, 900, 925, 950, 975,
	 1000, 1025, 1050, 1075, 1100, 1125, 1150,
	 1175, 1200, 1250, 1300, 1350, 1400, 1450,
	 1500, 1550, 1600, 1650, 1700, 1750, 1800, 
	 1850, 1900, 1950, 2000, 2050, 2100, 2150, 
	 2200, 2250, 2300, 2350, 2400, 2500, 2600, 
	 2700, 2800, 2850, 2900, 3000, 3100, 3200,
	 3300, 3400, 3500, 3600, 3700, 3800, 3900,
};

const static int ldo_voltage_map[] = {
	 600, 625, 650, 675, 700, 725, 750, 775,
	 800, 825, 850, 875, 900, 925, 950, 975,
	 1000, 1025, 1050, 1075, 1100, 1125, 1150,
	 1175, 1200, 1250, 1300, 1350, 1400, 1450,
	 1500, 1550, 1600, 1650, 1700, 1750, 1800, 
	 1850, 1900, 1950, 2000, 2050, 2100, 2150, 
	 2200, 2250, 2300, 2350, 2400, 2500, 2600, 
	 2700, 2800, 2850, 2900, 3000, 3100, 3200,
	 3300, 3400, 3500, 3600, 3700, 3800, 3900,
};

static int act8931_ldo_list_voltage(struct regulator_dev *dev, unsigned index)
{
	if (index >= ARRAY_SIZE(ldo_voltage_map))
		return -EINVAL;
	return 1000 * ldo_voltage_map[index];
}
static int act8931_ldo_is_enabled(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) -ACT8931_LDO1;
	u16 val;
	u16 mask=0x80;
	val = act8931_reg_read(act8931, act8931_LDO_CONTR_REG(ldo));	 
	if (val < 0)
		return val;
	val=val&~0x7f;
	if (val & mask)
		return 1;
	else
		return 0; 	
}
static int act8931_ldo_enable(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8931_LDO1;
	u16 mask=0x80;	

	return act8931_set_bits(act8931, act8931_LDO_CONTR_REG(ldo), mask, 0x80);
	
}
static int act8931_ldo_disable(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8931_LDO1;
	u16 mask=0x80;
	
	return act8931_set_bits(act8931, act8931_LDO_CONTR_REG(ldo), mask, 0);

}
static int act8931_ldo_get_voltage(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8931_LDO1;
	u16 reg = 0;
	int val;
	reg = act8931_reg_read(act8931,act8931_LDO_SET_VOL_REG(ldo));
	reg &= LDO_VOL_MASK;
	val = 1000 * ldo_voltage_map[reg];	
	return val;
}
static int act8931_ldo_set_voltage(struct regulator_dev *dev,
				  int min_uV, int max_uV, unsigned *selector)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8931_LDO1;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const int *vol_map =ldo_voltage_map;
	u16 val;
	int ret = 0;
	if (min_vol < vol_map[VOL_MIN_IDX] ||
	    min_vol > vol_map[VOL_MAX_IDX])
		return -EINVAL;

	for (val = VOL_MIN_IDX; val <= VOL_MAX_IDX;
	     val++){
		if (vol_map[val] >= min_vol)
			break;	}
		
	if (vol_map[val] > max_vol)
		return -EINVAL;

	ret = act8931_set_bits(act8931, act8931_LDO_SET_VOL_REG(ldo),
	       	LDO_VOL_MASK, val);
	return ret;

}
static unsigned int act8931_ldo_get_mode(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) -ACT8931_LDO1 ;
	u16 mask = 0xcf;
	u16 val;
	val = act8931_reg_read(act8931, act8931_LDO_CONTR_REG(ldo));
	val=val|mask;
	if (val== mask)
		return REGULATOR_MODE_NORMAL;
	else
		return REGULATOR_MODE_STANDBY;

}
static int act8931_ldo_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) -ACT8931_LDO1 ;
	u16 mask = 0x20;
	switch(mode)
	{
	case REGULATOR_MODE_NORMAL:
		return act8931_set_bits(act8931, act8931_LDO_CONTR_REG(ldo), mask, 0);		
	case REGULATOR_MODE_STANDBY:
		return act8931_set_bits(act8931, act8931_LDO_CONTR_REG(ldo), mask, mask);
	default:
		printk("error:pmu_act8931 only lowpower and nomal mode\n");
		return -EINVAL;
	}


}
static struct regulator_ops act8931_ldo_ops = {
	.set_voltage = act8931_ldo_set_voltage,
	.get_voltage = act8931_ldo_get_voltage,
	.list_voltage = act8931_ldo_list_voltage,
	.is_enabled = act8931_ldo_is_enabled,
	.enable = act8931_ldo_enable,
	.disable = act8931_ldo_disable,
	.get_mode = act8931_ldo_get_mode,
	.set_mode = act8931_ldo_set_mode,
	
};

static int act8931_dcdc_list_voltage(struct regulator_dev *dev, unsigned index)
{
	if (index >= ARRAY_SIZE(buck_voltage_map))
		return -EINVAL;
	return 1000 * buck_voltage_map[index];
}
static int act8931_dcdc_is_enabled(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8931_DCDC1;
	u16 val;
	u16 mask=0x80;	
	val = act8931_reg_read(act8931, act8931_BUCK_CONTR_REG(buck));
	if (val < 0)
		return val;
	 val=val&~0x7f;
	if (val & mask)
		return 1;
	else
		return 0; 	
}
static int act8931_dcdc_enable(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8931_DCDC1 ;
	u16 mask=0x80;	
	return act8931_set_bits(act8931, act8931_BUCK_CONTR_REG(buck), mask, 0x80);

}
static int act8931_dcdc_disable(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8931_DCDC1 ;
	u16 mask=0x80;
	 return act8931_set_bits(act8931, act8931_BUCK_CONTR_REG(buck), mask, 0);
}
static int act8931_dcdc_get_voltage(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8931_DCDC1 ;
	u16 reg = 0;
	int val;
	reg = act8931_reg_read(act8931,act8931_BUCK_SET_VOL_REG(buck));
	reg &= BUCK_VOL_MASK;
        DBG("%d\n", reg);
	val = 1000 * buck_voltage_map[reg];	
        DBG("%d\n", val);
	return val;
}
static int act8931_dcdc_set_voltage(struct regulator_dev *dev,
				  int min_uV, int max_uV, unsigned *selector)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8931_DCDC1 ;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const int *vol_map = buck_voltage_map;
	u16 val;
	int ret = 0;

        DBG("%s, min_uV = %d, max_uV = %d!\n", __func__, min_uV, max_uV);
	if (min_vol < vol_map[VOL_MIN_IDX] ||
	    min_vol > vol_map[VOL_MAX_IDX])
		return -EINVAL;

	for (val = VOL_MIN_IDX; val <= VOL_MAX_IDX;
	     val++){
		if (vol_map[val] >= min_vol)
			break;}

	if (vol_map[val] > max_vol)
		printk("WARNING:this voltage is not support!voltage set is %d mv\n",vol_map[val]);
	ret = act8931_set_bits(act8931, act8931_BUCK_SET_VOL_REG(buck),
	       	BUCK_VOL_MASK, val);
	ret = act8931_set_bits(act8931, act8931_BUCK_SET_VOL_REG(buck) + 0x01,
	       	BUCK_VOL_MASK, val);
	//if (ret)
		return ret;
}
static unsigned int act8931_dcdc_get_mode(struct regulator_dev *dev)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8931_DCDC1 ;
	u16 mask = 0xcf;
	u16 val;
	val = act8931_reg_read(act8931, act8931_BUCK_CONTR_REG(buck));
	val=val|mask;
	if (val== mask)
		return REGULATOR_MODE_STANDBY;
	else
		return REGULATOR_MODE_NORMAL;

}
static int act8931_dcdc_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct act8931 *act8931 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8931_DCDC1 ;
	u16 mask = 0x20;
	switch(mode)
	{
	case REGULATOR_MODE_STANDBY:
		return act8931_set_bits(act8931, act8931_BUCK_CONTR_REG(buck), mask, 0);
	case REGULATOR_MODE_NORMAL:
		return act8931_set_bits(act8931, act8931_BUCK_CONTR_REG(buck), mask, mask);
	default:
		printk("error:pmu_act8931 only powersave and pwm mode\n");
		return -EINVAL;
	}


}
static struct regulator_ops act8931_dcdc_ops = { 
	.set_voltage = act8931_dcdc_set_voltage,
	.get_voltage = act8931_dcdc_get_voltage,
	.list_voltage= act8931_dcdc_list_voltage,
	.is_enabled = act8931_dcdc_is_enabled,
	.enable = act8931_dcdc_enable,
	.disable = act8931_dcdc_disable,
	.get_mode = act8931_dcdc_get_mode,
	.set_mode = act8931_dcdc_set_mode,
};
static struct regulator_desc regulators[] = {
	{
		.name = "ACT_LDO1",
		.id =0,
		.ops = &act8931_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_LDO2",
		.id = 1,
		.ops = &act8931_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_LDO3",
		.id = 2,
		.ops = &act8931_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_LDO4",
		.id = 3,
		.ops = &act8931_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},

	{
		.name = "ACT_DCDC1",
		.id = 4,
		.ops = &act8931_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_DCDC2",
		.id = 5,
		.ops = &act8931_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_DCDC3",
		.id = 6,
		.ops = &act8931_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	
};

/*
 *
 */
static int act8931_i2c_read(struct i2c_client *i2c, char reg, int count,	u16 *dest)
{
      int ret;
    struct i2c_adapter *adap;
    struct i2c_msg msgs[2];

    if(!i2c)
		return ret;

	if (count != 1)
		return -EIO;  
  
    adap = i2c->adapter;		
    
    msgs[0].addr = i2c->addr;
    msgs[0].buf = &reg;
    msgs[0].flags = i2c->flags;
    msgs[0].len = 1;
    msgs[0].scl_rate = 200*1000;
    
    msgs[1].buf = (u8 *)dest;
    msgs[1].addr = i2c->addr;
    msgs[1].flags = i2c->flags | I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].scl_rate = 200*1000;
    ret = i2c_transfer(adap, msgs, 2);

	DBG("***run in %s %d msgs[1].buf = %d\n",__FUNCTION__,__LINE__,*(msgs[1].buf));

	return 0;   
}

static int act8931_i2c_write(struct i2c_client *i2c, char reg, int count, const u16 src)
{
	int ret=-1;
	
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	char tx_buf[2];

	if(!i2c)
		return ret;
	if (count != 1)
		return -EIO;
    
	adap = i2c->adapter;		
	tx_buf[0] = reg;
	tx_buf[1] = src;
	
	msg.addr = i2c->addr;
	msg.buf = &tx_buf[0];
	msg.len = 1 +1;
	msg.flags = i2c->flags;   
	msg.scl_rate = 200*1000;	

	ret = i2c_transfer(adap, &msg, 1);
	return 0;	
}

static u8 act8931_reg_read(struct act8931 *act8931, u8 reg)
{
	u16 val = 0;

	mutex_lock(&act8931->io_lock);

	act8931_i2c_read(act8931->i2c, reg, 1, &val);

	DBG("reg read 0x%02x -> 0x%02x\n", (int)reg, (unsigned)val&0xff);

	mutex_unlock(&act8931->io_lock);

	return val & 0xff;	
}

static int act8931_set_bits(struct act8931 *act8931, u8 reg, u16 mask, u16 val)
{
	u16 tmp;
	int ret;

	mutex_lock(&act8931->io_lock);

	ret = act8931_i2c_read(act8931->i2c, reg, 1, &tmp);
	DBG("1 reg read 0x%02x -> 0x%02x\n", (int)reg, (unsigned)tmp&0xff);
	tmp = (tmp & ~mask) | val;
	if (ret == 0) {
		ret = act8931_i2c_write(act8931->i2c, reg, 1, tmp);
		DBG("reg write 0x%02x -> 0x%02x\n", (int)reg, (unsigned)val&0xff);
	}
	act8931_i2c_read(act8931->i2c, reg, 1, &tmp);
	DBG("2 reg read 0x%02x -> 0x%02x\n", (int)reg, (unsigned)tmp&0xff);
	mutex_unlock(&act8931->io_lock);

	return ret;	
}
static int __devinit setup_regulators(struct act8931 *act8931, struct act8931_platform_data *pdata)
{	
	int i, err;

	act8931->num_regulators = pdata->num_regulators;
	act8931->rdev = kcalloc(pdata->num_regulators,
			       sizeof(struct regulator_dev *), GFP_KERNEL);
	if (!act8931->rdev) {
		return -ENOMEM;
	}
	/* Instantiate the regulators */
	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		act8931->rdev[i] = regulator_register(&regulators[id],
			act8931->dev, pdata->regulators[i].initdata, act8931);
/*
		if (IS_ERR(act8931->rdev[i])) {
			err = PTR_ERR(act8931->rdev[i]);
			dev_err(act8931->dev, "regulator init failed: %d\n",
				err);
			goto error;
		}*/
	}

	return 0;
error:
	while (--i >= 0)
		regulator_unregister(act8931->rdev[i]);
	kfree(act8931->rdev);
	act8931->rdev = NULL;
	return err;
}

int act8931_device_shutdown(void)
{
	int ret;
	int err = -1;
	struct act8931 *act8931 = g_act8931;
	
	printk("%s\n",__func__);

	ret = act8931_reg_read(act8931,0x01);
	ret = act8931_set_bits(act8931, 0x01,(0x1<<5) |(0x3<<0),(0x1<<5) | (0x3<<0));
	if (ret < 0) {
		printk("act8931 set 0x00 error!\n");
		return err;
	}
	return 0;	
}
EXPORT_SYMBOL_GPL(act8931_device_shutdown);


static irqreturn_t act8931_irq_thread(unsigned int irq, void *dev_id)
{
	struct act8931 *act8931 = (struct act8931 *)dev_id;
	int ret;
	u8 val;
	val = act8931_reg_read(act8931,0x78);
	act8931_charge_det = (val & INDAT_MASK )? 1:0;
	act8931_charge_ok = (val & CHGDAT_MASK )? 1:0;
	DBG(charge_det? "connect! " : "disconnect! ");
	DBG(charge_ok? "charge ok! \n" : "charging or discharge! \n");

	/* reset related regs according to spec */
	ret = act8931_set_bits(act8931, 0x78, INSTAT_MASK | CHGSTAT_MASK, 
			INSTAT_MASK | CHGSTAT_MASK);
	if (ret < 0) {
		printk("act8931 set 0x78 error!\n");
	}

	/* FIXME: it's better that waking up screen in battery driver */
	rk28_send_wakeup_key();
	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
__weak void act8931_early_suspend(struct early_suspend *h) {}
__weak void act8931_late_resume(struct early_suspend *h) {}
#endif

static int __devinit act8931_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct act8931 *act8931;	
	struct act8931_platform_data *pdata = i2c->dev.platform_data;
	int ret;
	u8 val;
	act8931 = kzalloc(sizeof(struct act8931), GFP_KERNEL);
	if (act8931 == NULL) {
		ret = -ENOMEM;		
		goto err;
	}
	act8931->i2c = i2c;
	act8931->dev = &i2c->dev;
	i2c_set_clientdata(i2c, act8931);
	mutex_init(&act8931->io_lock);	

	ret = act8931_reg_read(act8931,0x22);
	if ((ret < 0) || (ret == 0xff)){
		printk("The device is not act8931 \n");
		return 0;
	}
	
	if (pdata) {
		ret = setup_regulators(act8931, pdata);
		if (ret < 0)		
			goto err;
	} else
		dev_warn(act8931->dev, "No platform init data supplied\n");

	ret = act8931_reg_read(act8931,0x01);
	if (ret < 0)		
			goto err;
	ret = act8931_set_bits(act8931, 0x01,(0x1<<5) | (0x1<<0),(0x1<<0));
	if (ret < 0) {
		printk("act8931 set 0x01 error!\n");
		goto err;
	}
	
	g_act8931 = act8931;
	
	pdata->set_init(act8931);

	/* Initialize charge status */
	val = act8931_reg_read(act8931,0x78);
	act8931_charge_det = (val & INDAT_MASK )? 1:0;
	act8931_charge_ok = (val & CHGDAT_MASK )? 1:0;
	DBG(charge_det? "connect! " : "disconnect! ");
	DBG(charge_ok? "charge ok! \n" : "charging or discharge! \n");
	
	ret = act8931_set_bits(act8931, 0x78, INSTAT_MASK | CHGSTAT_MASK, 
			INSTAT_MASK | CHGSTAT_MASK);
	if (ret < 0) {
		printk("act8931 set 0x78 error!\n");
		goto err;
	}
	
	ret = act8931_set_bits(act8931, 0x79, INCON_MASK | CHGEOCIN_MASK | INDIS_MASK | CHGEOCOUT_MASK, 
			INCON_MASK | CHGEOCIN_MASK | INDIS_MASK | CHGEOCOUT_MASK);
	if (ret < 0) {
		printk("act8931 set 0x79 error!\n");
		goto err;
	}

	ret = gpio_request(i2c->irq, "act8931 gpio");
	if(ret)
	{
		printk("act8931 gpio request fail\n");
		gpio_free(i2c->irq);
		goto err;
	}
	
	act8931->irq = gpio_to_irq(i2c->irq);
	gpio_pull_updown(i2c->irq,GPIOPullUp);
	ret = request_threaded_irq(act8931->irq, NULL, act8931_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, i2c->dev.driver->name, act8931);
	if (ret < 0)
	{
		printk("request act8931 irq fail\n");
		goto err;
	}	

	enable_irq_wake(act8931->irq);

	#ifdef CONFIG_HAS_EARLYSUSPEND
	act8931->act8931_suspend.suspend = act8931_early_suspend,
	act8931->act8931_suspend.resume = act8931_late_resume,
	act8931->act8931_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
	register_early_suspend(&act8931->act8931_suspend);
	#endif

	return 0;

err:
	return ret;	

}

static int __devexit act8931_i2c_remove(struct i2c_client *i2c)
{
	struct act8931 *act8931 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < act8931->num_regulators; i++)
		if (act8931->rdev[i])
			regulator_unregister(act8931->rdev[i]);
	kfree(act8931->rdev);
	i2c_set_clientdata(i2c, NULL);
	kfree(act8931);

	return 0;
}

static const struct i2c_device_id act8931_i2c_id[] = {
       { "act8931", 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, act8931_i2c_id);

static struct i2c_driver act8931_i2c_driver = {
	.driver = {
		.name = "act8931",
		.owner = THIS_MODULE,
	},
	.probe    = act8931_i2c_probe,
	.remove   = __devexit_p(act8931_i2c_remove),
	.id_table = act8931_i2c_id,
};

static int __init act8931_module_init(void)
{
	int ret;
	ret = i2c_add_driver(&act8931_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);
	return ret;
}
//module_init(act8931_module_init);
//subsys_initcall(act8931_module_init);
//rootfs_initcall(act8931_module_init);
subsys_initcall_sync(act8931_module_init);

static void __exit act8931_module_exit(void)
{
	i2c_del_driver(&act8931_i2c_driver);
}
module_exit(act8931_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xhc <xhc@rock-chips.com>");
MODULE_DESCRIPTION("act8931 PMIC driver");


