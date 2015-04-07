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
#endif /* CONFIG_HAS_EARLYSUSPEND */

#include <linux/miscdevice.h>
#include <asm/uaccess.h>

/*author@xxm*/
#define GPUCLK_NAME  				 "gpu"
#define GPUCLK_PD_NAME				 "pd_gpu"
#define GPU_MHZ 					 1000000
static struct clk               *mali_clock = 0;
static struct clk				*mali_clock_pd = 0;


#define MALI_DVFS_DEFAULT_STEP 0 // 50Mhz default
#define MALI_DVFS_STEPS 7

static int num_clock = MALI_DVFS_STEPS;
static int mali_dvfs[MALI_DVFS_STEPS] = {50,100,133,160,200,266,400};

static u64 percent_of_running = 0;
static u64 percent_of_running_time = 0;
static u32 percent_of_running_enable = 0;

module_param_array(mali_dvfs, int, &num_clock,S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mali_dvfs,"mali clock table");

static int mali_init_clock = 50;
module_param(mali_init_clock, int,S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mali_init_clock,"mali init clock value");

u32 gpu_power_state = 0;
static u32 utilization_global = 0;
unsigned int scale_enable = 1;

#define mali_freq_workqueue_name	 "mali_freq_workqueue"
#define mali_freq_work_name 		 "mali_freq_work"
struct mali_freq_data {
	struct workqueue_struct *wq;
	struct work_struct work;
	u32 freq;
}*mali_freq_data;

typedef struct mali_dvfs_tableTag{
    u32 clock;
    u32 vol;
}mali_dvfs_table;

typedef struct mali_dvfs_statusTag{
    unsigned int currentStep;
    mali_dvfs_table * pCurrentDvfs;

}mali_dvfs_status;

mali_dvfs_status maliDvfsStatus;
mali_io_address clk_register_map=0;

#define GPU_DVFS_UP_THRESHOLD	((int)((255*50)/100))	
#define GPU_DVFS_DOWN_THRESHOLD ((int)((255*35)/100))	
#define level_6_UP_THRESHOLD	((int)((255*80)/100))
#define level_6_DOWN_THRESHOLD  ((int)((255*75)/100))
u32 lastClk;

_mali_osk_lock_t *clockSetlock;
#if 0
mali_dvfs_table mali_dvfs[MALI_DVFS_STEPS]={
 {50, 1000000},
 {100,1000000},
 {133,1100000},
 {160,1100000},
 {200,1100000},
 {266,1100000},
 {400,1100000},
};
#endif

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
static struct kobject *mali400_utility_object;
#if 1 
#include <linux/ktime.h>

static u64 mali_dvfs_time_went[MALI_DVFS_STEPS] = {0};
extern ktime_t ktime_get(void);

static u32     old_step ; 
static u64 	   old64_time;
static u32     run_time_calculate_enable = 0;

static u32 get_old64_time(void)
{
	return old64_time;
}
static void set_old64_time(u64 value)
{
	old64_time = value;
}
static void old_step_set(u32 value)
{
	old_step = value ;
}
static u32 old_step_get()
{
	return old_step ;
}
static u32 get_mali_dvfs_status(void)
{
	return maliDvfsStatus.currentStep;
}
static void set_mali_dvfs_step(u32 value)
{
	maliDvfsStatus.currentStep = value;
}

static void run_time_calculate(u32 step)
{
	u32     new_step ;
	u64     new64_time;
	u64     time64_sub;
	u64     temp64_time;
	u32     temp;
	
	temp  = old_step_get();
	temp64_time = get_old64_time();
	new_step = step;
	new64_time = _mali_osk_time_get_ns();
	time64_sub = new64_time - temp64_time;
	
	mali_dvfs_time_went[temp] += time64_sub;
	old_step_set(new_step);
	set_old64_time(new64_time);
}

static u32 mali_dvfs_time_went_init(void)
{
	u32 i;
	for(i=0;i<num_clock;i++)
	{
		mali_dvfs_time_went[i] = 0;
	}
	return 0;
}
static void mali_dvfs_time_went_show(void)
{
	u32 i;
	for(i=0;i<num_clock;i++)
	{
		MALI_DEBUG_PRINT(2,("mali_dvfs_time_went[%d] running %llu\r\n",i,mali_dvfs_time_went[i]));
	}
}
static void run_time_calculate_enable_set(u32 value)
{
	run_time_calculate_enable = value;
}
#endif
#if 1
void percent_of_running_init(void)
{
	percent_of_running = 0;
	percent_of_running_time = 0;
	percent_of_running_enable = 1;
}
void percent_of_running_terminate(void)
{
	percent_of_running_enable = 0;
	MALI_DEBUG_PRINT(2,("percent_of_running = %llu\r\n",percent_of_running));
	MALI_DEBUG_PRINT(2,("percent_of_running_time = %llu\r\n",(percent_of_running_time*255)));
}
#endif

static void scale_enable_set(u32 value)
{
	scale_enable = value;
}
static u32 mali_dvfs_search(u32 value)
{
	u32 i;	
	u32 clock = value;
	for(i=0;i<num_clock;i++)
	{
		if(clock == mali_dvfs[i])
		{
			_mali_osk_lock_wait(clockSetlock,_MALI_OSK_LOCKMODE_RW);
			mali_clk_set_rate(mali_clock,clock);
			_mali_osk_lock_signal(clockSetlock, _MALI_OSK_LOCKMODE_RW);
			set_mali_dvfs_step(i);
			scale_enable_set(0);
			return 0;
		}
		if(i>=7)
		MALI_DEBUG_PRINT(2,("USER set clock not in the mali_dvfs table\r\n"));
	}
	return 1;
}

static u32 mali400_utility_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", utilization_global);
}
static u32 mali400_clock_set(struct device *dev,struct device_attribute *attr, char *buf,u32 count)
{
	u32 clock;
	u32 currentStep;
	u64 timeValue;
	clock = simple_strtoul(buf, NULL, 10);
	currentStep = get_mali_dvfs_status();
	timeValue = _mali_osk_time_get_ns();
	//MALI_PRINT(("USER SET CLOCK,%d\r\n",clock));
	if(!clock)
	{
		scale_enable_set(1);
		set_old64_time(timeValue);
		old_step_set(currentStep);
		mali_dvfs_time_went_init();
		run_time_calculate_enable_set(1);
		percent_of_running_terminate();
	}
	else
	{
		percent_of_running_init();
		mali_dvfs_search(clock);
		run_time_calculate_enable_set(0);
		mali_dvfs_time_went_show();
	}
	return count;
}
static u32 clock_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	#if 0
	switch(num_clock)
	{
		case 1:
			return snprintf(buf,PAGE_SIZE,"%d,%d\n",num_clock,mali_dvfs[0]);
			break;
		case 2:
			return snprintf(buf,PAGE_SIZE,"%d,%d,%d\n",num_clock,mali_dvfs[0],mali_dvfs[1]);
			break;
		case 3:
			return snprintf(buf,PAGE_SIZE,"%d,%d,%d,%d\n",num_clock,mali_dvfs[0],mali_dvfs[1],mali_dvfs[2]);
			break;
		case 4:
			return snprintf(buf,PAGE_SIZE,"%d,%d,%d,%d,%d\n",num_clock,mali_dvfs[0],mali_dvfs[1],mali_dvfs[2],mali_dvfs[3]);
			break;
		case 5:
			return snprintf(buf,PAGE_SIZE,"%d,%d,%d,%d,%d,%d\n",num_clock,mali_dvfs[0],mali_dvfs[1],mali_dvfs[2],mali_dvfs[3],mali_dvfs[4]);
			break;
		case 6:
			return snprintf(buf,PAGE_SIZE,"%d,%d,%d,%d,%d,%d,%d\n",num_clock,mali_dvfs[0],mali_dvfs[1],mali_dvfs[2],mali_dvfs[3],mali_dvfs[4],mali_dvfs[5]);
			break;
		case 7:
			return  snprintf(buf,PAGE_SIZE,"%d,%d,%d,%d,%d,%d,%d,%d\n",num_clock,mali_dvfs[0],mali_dvfs[1],mali_dvfs[2],mali_dvfs[3],mali_dvfs[4],mali_dvfs[5],mali_dvfs[6]);
			break;
		default:
			MALI_PRINT(("param num error,max 7\n"));
	}
	#endif
	int i;
	char *pos = buf;
	pos += snprintf(pos,PAGE_SIZE,"%d,",num_clock);
	for(i=0;i<(num_clock-1);i++)
	{
		pos += snprintf(pos,PAGE_SIZE,"%d,",mali_dvfs[i]);
	}
	pos +=snprintf(pos,PAGE_SIZE,"%d\n",mali_dvfs[i]); 
	return pos - buf;
}

static DEVICE_ATTR(utility, 0644, mali400_utility_show, mali400_clock_set);
static DEVICE_ATTR(param, 0644, clock_show, NULL);

static mali_bool mali400_utility_sysfs_init(void)
{
	int ret ;

	mali400_utility_object = kobject_create_and_add("mali400_utility", NULL);
	if (mali400_utility_object == NULL) {
		return -1;
	}
	ret = sysfs_create_file(mali400_utility_object, &dev_attr_utility.attr);
	if (ret) {
		return -1;
	}
	ret = sysfs_create_file(mali400_utility_object, &dev_attr_param.attr);
	if (ret) {
		return -1;
	}
	return 0 ;
}	
static unsigned int decideNextStatus(unsigned int utilization)
{
    unsigned int level=0;
#if 1
	//if(maliDvfsStatus.currentStep < (num_clock))
	{
	    if(utilization>GPU_DVFS_UP_THRESHOLD && maliDvfsStatus.currentStep==0 && maliDvfsStatus.currentStep<(num_clock-1))
	        level=1;
	    else if(utilization>GPU_DVFS_UP_THRESHOLD && maliDvfsStatus.currentStep==1 && maliDvfsStatus.currentStep<(num_clock-1))
	        level=2;
		else if(utilization>GPU_DVFS_UP_THRESHOLD && maliDvfsStatus.currentStep==2 && maliDvfsStatus.currentStep<(num_clock-1))
			level=3;
		else if(utilization>GPU_DVFS_UP_THRESHOLD && maliDvfsStatus.currentStep==3 && maliDvfsStatus.currentStep<(num_clock-1))
			level=4;
		else if(utilization>GPU_DVFS_UP_THRESHOLD && maliDvfsStatus.currentStep==4 && maliDvfsStatus.currentStep<(num_clock-1))
			level=5;
	#if 0
		else if(utilization>GPU_DVFS_UP_THRESHOLD && maliDvfsStatus.currentStep==5)
			level=6;
	#endif
		/*
			not support up to level 6
		*/
		else if(utilization<GPU_DVFS_DOWN_THRESHOLD && maliDvfsStatus.currentStep==6 /*&& maliDvfsStatus.currentStep<num_clock*/)
			level=5;
		else if(utilization<GPU_DVFS_DOWN_THRESHOLD && maliDvfsStatus.currentStep==5 /*&& maliDvfsStatus.currentStep<num_clock*/)
			level=4;
		else if(utilization<GPU_DVFS_DOWN_THRESHOLD && maliDvfsStatus.currentStep==4 /*&& maliDvfsStatus.currentStep<num_clock*/)
			level=3;
		else if(utilization<GPU_DVFS_DOWN_THRESHOLD && maliDvfsStatus.currentStep==3 /*&& maliDvfsStatus.currentStep<num_clock*/)
			level=2;
		else if(utilization<GPU_DVFS_DOWN_THRESHOLD && maliDvfsStatus.currentStep==2 /*&& maliDvfsStatus.currentStep<num_clock*/)
			level=1;
		else if(utilization<GPU_DVFS_DOWN_THRESHOLD && maliDvfsStatus.currentStep==1 /*&& maliDvfsStatus.currentStep<num_clock*/)
			level=0;
	    else
	        level = maliDvfsStatus.currentStep;
	}
#endif
    return level;
}

#if 0
static void mali_platform_wating(u32 msec)
{
    /*sample wating
    change this in the future with proper check routine.
    */
	unsigned int read_val;
	while(1)
	{
		read_val = _mali_osk_mem_ioread32(clk_register_map, 0x00);
		if ((read_val & 0x8000)==0x0000) break;

        _mali_osk_time_ubusydelay(100); // 1000 -> 100 : 20101218
	}
    /* _mali_osk_time_ubusydelay(msec*1000);*/
}
#endif
static mali_bool set_mali_dvfs_status(u32 step)
{
    u32 validatedStep=step;
	
	if(0)//(run_time_calculate_enable)
		run_time_calculate(step);
	
	if(1/*scale_enable*/)
	{
		_mali_osk_lock_wait(clockSetlock,_MALI_OSK_LOCKMODE_RW);
    	mali_clk_set_rate(mali_clock, mali_dvfs[step]);
		_mali_osk_lock_signal(clockSetlock, _MALI_OSK_LOCKMODE_RW);
		set_mali_dvfs_step(validatedStep);
		/*for the future use*/
    	//maliDvfsStatus.pCurrentDvfs = &mali_dvfs[validatedStep];
	}
	else
	{
		MALI_DEBUG_PRINT(2,("mali freq scale disable\r\n"));
		return MALI_FALSE;
	}
    return MALI_TRUE;
}

static mali_bool change_mali_dvfs_status(u32 step)
{
#if 0
	if(scale_enable)
		MALI_DEBUG_PRINT(3,("change_mali_dvfs_status: %d\n",step));
#endif
    if(!set_mali_dvfs_status(step))
    {
        MALI_DEBUG_PRINT(2,("error on set_mali_dvfs_status: %d\n",step));
        return MALI_FALSE;
    }
    /*wait until clock and voltage is stablized*/
#if 0
	mali_platform_wating(MALI_DVFS_WATING); /*msec*/
#endif
	//MALI_DEBUG_PRINT(2,("change_mali_dvfs_status OK \r\n"));
	return MALI_TRUE;
}

static unsigned int  mali_freq_scale_work(struct work_struct *work)
{	

	unsigned int nextStatus = 0;
	unsigned int curStatus = 0;
	
	/*decide next step*/
	curStatus = get_mali_dvfs_status();
	nextStatus = decideNextStatus(utilization_global);
	

	if(curStatus!=nextStatus)
	{
	#if 0
		if(scale_enable)
			MALI_DEBUG_PRINT(3,("curStatus %d, nextStatus %d, maliDvfsStatus.currentStep %d \n", curStatus, nextStatus, maliDvfsStatus.currentStep));
	#endif
		if(!change_mali_dvfs_status(nextStatus))
		{
			MALI_DEBUG_PRINT(1, ("error on change_mali_dvfs_status \n"));
			return MALI_FALSE;
		}
	}
	//MALI_DEBUG_PRINT(2,("mali_freq_scale_work OK\r\n"));
	return MALI_TRUE;	
}
static mali_bool init_mali_clock(void)
{
	u64 timeValue;
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
	timeValue = _mali_osk_time_get_ns();
	set_old64_time(timeValue);
	lastClk = (u32)(mali_clk_get_rate(mali_clock)/GPU_MHZ);
	gpu_power_state = 1;

	MALI_PRINT(("init_mali_clock mali_clock %p \n", mali_clock));
	MALI_DEBUG_PRINT(2, ("MALI Clock is set at mali driver\n"));
	
	return MALI_TRUE;

err_gpu_clk:
	MALI_DEBUG_PRINT(3, ("::clk_put:: %s mali_clock\n", __FUNCTION__));
	gpu_power_state = 0;
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

mali_bool init_mali_dvfs_status(int step)
{
	set_mali_dvfs_step(step);
    return MALI_TRUE;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mali_pm_early_suspend(struct early_suspend *mali_dev)
{
	lastClk = (u32)(mali_clk_get_rate(mali_clock)/GPU_MHZ);
	if(!scale_enable)
		scale_enable_set(1);
}
static void mali_pm_late_resume(struct early_suspend *mali_dev)
{
#if 0
	_mali_osk_lock_wait(clockSetlock,_MALI_OSK_LOCKMODE_RW);
	mali_clk_set_rate(mali_clock, lastClk);
	_mali_osk_lock_signal(clockSetlock, _MALI_OSK_LOCKMODE_RW);
#endif
}
static struct early_suspend mali_dev_early_suspend = {
	.suspend = mali_pm_early_suspend,
	.resume = mali_pm_late_resume,
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
};
#endif /* CONFIG_HAS_EARLYSUSPEND */
static long mali_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	MALI_SUCCESS;
}
static int mali_misc_open(struct inode *inode, struct file *file)
{
	MALI_DEBUG_PRINT(2,("mali misc open \r\n"));
	MALI_SUCCESS;
}
static ssize_t mali_misc_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	int ret;
	if(0 != copy_to_user(ubuf,(unsigned char *)mali_dvfs,sizeof(unsigned int)*num_clock))
		return -EFAULT;
	ret = sizeof(unsigned int)*num_clock;
	return ret;
}


static struct file_operations getFreqTable_fops = {
	.owner		= THIS_MODULE,
	.open		= mali_misc_open,
	.read       = mali_misc_read,
	.unlocked_ioctl	= mali_misc_ioctl,
};

struct miscdevice mali_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "mali_misc",
	.fops	= &getFreqTable_fops,
};

_mali_osk_errcode_t mali_platform_init(void)
{
	//int ret;
	MALI_CHECK(init_mali_clock(), _MALI_OSK_ERR_FAULT);
	
	clockSetlock = _mali_osk_lock_init((_mali_osk_lock_flags_t)( _MALI_OSK_LOCKFLAG_READERWRITER | _MALI_OSK_LOCKFLAG_ORDERED), 0, 0);
#if 0
	if (!clk_register_map) 
		clk_register_map = _mali_osk_mem_mapioregion( phy, size, CLK_DESC );	 
#endif
	if(!init_mali_dvfs_status(MALI_DVFS_DEFAULT_STEP))
		MALI_DEBUG_PRINT(1, ("init_mali_dvfs_status failed\n"));
	
	if(mali400_utility_sysfs_init())
		MALI_PRINT(("mali400_utility_sysfs_init error\r\n"));
	
	mali_freq_data = kmalloc(sizeof(struct mali_freq_data), GFP_KERNEL);
	if(!mali_freq_data)
	{
		MALI_PRINT(("kmalloc error\r\n"));
		MALI_ERROR(-1);
	}
	mali_freq_data->wq = create_workqueue(mali_freq_workqueue_name);
	if(!mali_freq_data->wq)
		MALI_ERROR(-1);
	INIT_WORK(&mali_freq_data->work,mali_freq_scale_work);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&mali_dev_early_suspend);
#endif

#if 0
	ret = misc_register(&mali_misc);
	if(ret)
		MALI_PRINT(("mali misc register error \r\n"));
#endif	
    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	deinit_mali_clock();
	_mali_osk_lock_term(clockSetlock);

    MALI_SUCCESS;
}
_mali_osk_errcode_t mali_power_domain_control(u32 bpower_off)
{
	if (!bpower_off)
	{
		if(!gpu_power_state)
		{
			enable_mali_clocks(mali_clock_pd);
			enable_mali_clocks(mali_clock);
			gpu_power_state = 1 ;
		}		
	}
	else if (bpower_off == 2)
	{
		;
	}
	else if (bpower_off == 1)
	{
		if(gpu_power_state)
		{
			disable_mali_clocks(mali_clock);
			disable_mali_clocks(mali_clock_pd);	
			gpu_power_state = 0;
		}
	}
    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
#if 1
	switch(power_mode)
	{
		case MALI_POWER_MODE_ON:
			MALI_DEBUG_PRINT(2,("MALI_POWER_MODE_ON\r\n"));
			mali_power_domain_control(MALI_POWER_MODE_ON);
			break;
		case MALI_POWER_MODE_LIGHT_SLEEP:
			MALI_DEBUG_PRINT(2,("MALI_POWER_MODE_LIGHT_SLEEP\r\n"));
			mali_power_domain_control(MALI_POWER_MODE_LIGHT_SLEEP);
			break;
		case MALI_POWER_MODE_DEEP_SLEEP:
			MALI_DEBUG_PRINT(2,("MALI_POWER_MODE_DEEP_SLEEP\r\n"));
			mali_power_domain_control(MALI_POWER_MODE_DEEP_SLEEP);
			break;
		default:
			MALI_DEBUG_PRINT(2,("mali_platform_power_mode_change:power_mode(%d) not support \r\n",power_mode));
	}
#endif
    MALI_SUCCESS;
}
#if 1
static void por_calculate(u32 utilization)
{
	
	percent_of_running += utilization;
	percent_of_running_time ++;
}
#endif

void mali_gpu_utilization_handler(u32 utilization)
{
	if(utilization > 255)
		return;
	utilization_global = utilization;
	
	//MALI_PRINT(("utilization_global = %d\r\n",utilization_global));
	if(0)//(percent_of_running_enable)
		por_calculate(utilization);
	if(scale_enable)
		queue_work(mali_freq_data->wq,&mali_freq_data->work);
	
	return ;
}

void set_mali_parent_power_domain(void* dev)
{
}



