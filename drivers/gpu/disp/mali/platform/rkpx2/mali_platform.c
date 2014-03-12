/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"
//#include "mali_pmm.h"
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/device.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <mach/cpu.h>
#include <mach/dvfs.h>
#include <linux/cpufreq.h>

#define GPUCLK_NAME  				 "gpu"
#define GPUCLK_PD_NAME				 "pd_gpu"
#define GPU_MHZ 					 1000000
static struct clk               *mali_clock = 0;
static struct clk				*mali_clock_pd = 0;

u32 mali_init_clock = 266;

struct clk* mali_clk_get(unsigned char *name)
{
	struct clk *clk;
	clk = clk_get(NULL,name);
	return clk;
}
unsigned long mali_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}

void mali_clk_set_rate(struct clk *clk,u32 value)
{
	unsigned long rate = (unsigned long)value * GPU_MHZ;
	clk_set_rate(clk, rate);
	rate = mali_clk_get_rate(clk);
}
static _mali_osk_errcode_t enable_mali_clocks(struct clk *clk)
{
	u32 err;
	err = clk_enable(clk);
	MALI_DEBUG_PRINT(2,("enable_mali_clocks\n"));	
	MALI_SUCCESS;
}
static _mali_osk_errcode_t disable_mali_clocks(struct clk *clk)
{
	clk_disable(clk);
	MALI_DEBUG_PRINT(2,("disable_mali_clocks\n"));
	
	MALI_SUCCESS;
}

static mali_bool init_mali_clock(void)
{
	
	mali_bool ret = MALI_TRUE;
	
	if (mali_clock != 0 || mali_clock_pd != 0)
		return ret; 
	
	mali_clock_pd = mali_clk_get(GPUCLK_PD_NAME);
	if (IS_ERR(mali_clock_pd))
	{
		MALI_PRINT( ("MALI Error : failed to get source mali pd\n"));
		ret = MALI_FALSE;
		goto err_gpu_clk;
	}
	enable_mali_clocks(mali_clock_pd);
	
	mali_clock = mali_clk_get(GPUCLK_NAME);
	if (IS_ERR(mali_clock))
	{
		MALI_PRINT( ("MALI Error : failed to get source mali clock\n"));
		ret = MALI_FALSE;
		goto err_gpu_clk;
	}

	enable_mali_clocks(mali_clock);
	mali_clk_set_rate(mali_clock, mali_init_clock);

	MALI_PRINT(("init mali clock success\n"));	
	return MALI_TRUE;

err_gpu_clk:
	MALI_PRINT(("::clk_put:: %s mali_clock\n", __FUNCTION__));
	clk_put(mali_clock_pd);
	clk_put(mali_clock);
	mali_clock = 0;
	mali_clock_pd = 0;

	return ret;
}

static mali_bool deinit_mali_clock(void)
{
	if (mali_clock == 0 && mali_clock_pd == 0)
		return MALI_TRUE;
	disable_mali_clocks(mali_clock);
	disable_mali_clocks(mali_clock_pd);
	clk_put(mali_clock);	
	clk_put(mali_clock_pd);
	mali_clock = 0;
	mali_clock_pd = 0;

	return MALI_TRUE;
}

_mali_osk_errcode_t mali_platform_init(void)
{
	MALI_CHECK(init_mali_clock(), _MALI_OSK_ERR_FAULT);
  MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	deinit_mali_clock();
  MALI_SUCCESS;
}

_mali_osk_errcode_t mali_power_domain_control(u32 bpower_off)
{
    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
    MALI_SUCCESS;
}


void mali_gpu_utilization_handler(struct mali_gpu_utilization_data *data)
{
	;
}

void set_mali_parent_power_domain(void* dev)
{
}



