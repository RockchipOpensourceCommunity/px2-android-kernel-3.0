/*
 * Copyright (C) 2013 ROCKCHIP, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <plat/efuse.h>
#include <linux/string.h>

u8 efuse_buf[32 + 1] = {0, 0};
static char efuse_val[65];

static int efuse_readregs(u32 addr, u32 length, u8 *buf)
{
#ifndef efuse_readl
	return 0;
#else
	unsigned long flags;
	static DEFINE_SPINLOCK(efuse_lock);
	int ret = length;

	if (!length)
		return 0;

	spin_lock_irqsave(&efuse_lock, flags);

	efuse_writel(EFUSE_CSB, REG_EFUSE_CTRL);
	efuse_writel(EFUSE_LOAD | EFUSE_PGENB, REG_EFUSE_CTRL);
	udelay(2);
	do {
		efuse_writel(efuse_readl(REG_EFUSE_CTRL) & (~(EFUSE_A_MASK << EFUSE_A_SHIFT)), REG_EFUSE_CTRL);
		efuse_writel(efuse_readl(REG_EFUSE_CTRL) | ((addr & EFUSE_A_MASK) << EFUSE_A_SHIFT), REG_EFUSE_CTRL);
		udelay(2);
		efuse_writel(efuse_readl(REG_EFUSE_CTRL) | EFUSE_STROBE, REG_EFUSE_CTRL);
		udelay(2);
		*buf = efuse_readl(REG_EFUSE_DOUT);
		efuse_writel(efuse_readl(REG_EFUSE_CTRL) & (~EFUSE_STROBE), REG_EFUSE_CTRL);
		udelay(2);
		buf++;
		addr++;
	} while(--length);
	udelay(2);
	efuse_writel(efuse_readl(REG_EFUSE_CTRL) | EFUSE_CSB, REG_EFUSE_CTRL);
	udelay(1);

	spin_unlock_irqrestore(&efuse_lock, flags);
	return ret;
#endif
}

void rk_efuse_init(void)
{
	int i=0;
	char temp[3];
	efuse_readregs(0x0, 32, efuse_buf);
	efuse_readregs(0x0, 32, efuse_buf);
	for(i=0;i<32;i++){
		sprintf(temp,"%02x",efuse_buf[i]);
		strcat(efuse_val,temp);
	}

}

int rk_pll_flag(void)
{
	return efuse_buf[22] & 0x3;
}
int rk_tflag(void)
{
	return efuse_buf[22] & (0x1 << 3);
}

int efuse_version_val(void)
{
	int ret = efuse_buf[4] & (~(0x1 << 3));
	printk("%s: efuse version = %02x\n", __func__, ret);
	return ret;
}

int rk_leakage_val(void)
{
	/*
	 * efuse_buf[22]
	 * bit[2]:
	 * 	0:enable leakage level auto voltage scale
	 * 	1:disalbe leakage level avs
	 */
	
	int leakage_level = 0;
	int leakage_val = 0;
	int efuse_version = efuse_version_val();

	if ((efuse_buf[22] >> 2) & 0x1){
		return 0;
	} else {
		return leakage_val;
	}
}

int rk3028_version_val(void)
{
	return efuse_buf[5];
}

int rk3026_version_val(void)
{
	if (efuse_buf[24]){
		return efuse_buf[24];
	} else {
		return efuse_buf[5];
	}
}

char *rk_efuse_value(void)
{
	return efuse_val;
}
