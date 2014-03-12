/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include "mali_platform.h"
#include <mach/cpu.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
static int num_cores_total;
static int num_cores_enabled;
static void mali_platform_device_release(struct device *device);
static int mali_os_suspend(struct device *device);
static int mali_os_resume(struct device *device);
static int mali_os_freeze(struct device *device);
static int mali_os_thaw(struct device *device);
#ifdef CONFIG_PM_RUNTIME
static int mali_runtime_suspend(struct device *device);
static int mali_runtime_resume(struct device *device);
static int mali_runtime_idle(struct device *device);
#endif

#define MALI_BASE_ADDR 		   0x10090000

#define MALI_GP_IRQ       37

#define MALI_PP0_IRQ      39
#define MALI_PP1_IRQ      39
#define MALI_PP2_IRQ      39
#define MALI_PP3_IRQ      39

#define MALI_GP_MMU_IRQ   38
#define MALI_PP0_MMU_IRQ  38
#define MALI_PP1_MMU_IRQ  38
#define MALI_PP2_MMU_IRQ  38
#define MALI_PP3_MMU_IRQ  38

/*#include "arm_core_scaling.h"*/
void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);
static struct work_struct wq_work;
static struct resource mali_gpu_resources[] =
{
	MALI_GPU_RESOURCES_MALI400_MP4(MALI_BASE_ADDR,
	                               MALI_GP_IRQ, MALI_GP_MMU_IRQ,
	                               MALI_PP0_IRQ, MALI_PP0_MMU_IRQ,
	                               MALI_PP1_IRQ, MALI_PP1_MMU_IRQ,
	                               MALI_PP2_IRQ, MALI_PP2_MMU_IRQ,
	                               MALI_PP3_IRQ, MALI_PP3_MMU_IRQ)
};

#if 1
static struct dev_pm_ops mali_gpu_device_type_pm_ops =
{
	.suspend = mali_os_suspend,
	.resume = mali_os_resume,
	.freeze = mali_os_freeze,
	.thaw = mali_os_thaw,
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = mali_runtime_suspend,
	.runtime_resume = mali_runtime_resume,
	.runtime_idle = mali_runtime_idle,
#endif
};

static struct device_type mali_gpu_device_device_type =
{
	.pm = &mali_gpu_device_type_pm_ops,
};
#endif
static u64 dma_dmamask = DMA_BIT_MASK(32);
static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.dev.release = mali_platform_device_release,
	/*
	 * We temporarily make use of a device type so that we can control the Mali power
	 * from within the mali.ko (since the default platform bus implementation will not do that).
	 * Ideally .dev.pm_domain should be used instead, as this is the new framework designed
	 * to control the power of devices.
	 */
#if 1
	.dev.type = &mali_gpu_device_device_type, /* We should probably use the pm_domain instead of type on newer kernels */
  .dev.dma_mask = &dma_dmamask,
  .dev.coherent_dma_mask = DMA_BIT_MASK(32),
#endif
};

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = 1024* 1024 * 1024, /* 1GB */
	.fb_start = 0x40000000,
	.fb_size = 0xb1000000,
	.utilization_interval = 1000, /* 0ms */
	.utilization_callback = mali_gpu_utilization_handler,
};
static void set_num_cores(struct work_struct *work)
{
	int err = mali_perf_set_num_pp_cores(num_cores_enabled);
	MALI_DEBUG_ASSERT(0 == err);
	MALI_IGNORE(err);
}
static void enable_one_core(void)
{
	if (num_cores_enabled < num_cores_total)
	{
		++num_cores_enabled;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling one more core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
}
static void disable_one_core(void)
{
	if (1 < num_cores_enabled)
	{
		--num_cores_enabled;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(3, ("Core scaling: Disabling one core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
}
static void enable_max_num_cores(void)
{
	if (num_cores_enabled < num_cores_total)
	{
		num_cores_enabled = num_cores_total;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(3, ("Core scaling: Enabling maximum number of cores\n"));
	}

	MALI_DEBUG_ASSERT(num_cores_total == num_cores_enabled);
}
void mali_core_scaling_init(int num_pp_cores)
{
	INIT_WORK(&wq_work, set_num_cores);

	num_cores_total   = num_pp_cores;
	num_cores_enabled = num_pp_cores;

	/* NOTE: Mali is not fully initialized at this point. */
}
void mali_core_scaling_term(void)
{
	flush_scheduled_work();
}
#define PERCENT_OF(percent, max) ((int) ((percent)*(max)/100.0 + 0.5))
void mali_core_scaling_update(struct mali_gpu_utilization_data *data)
{
	/*
	 * This function implements a very trivial PP core scaling algorithm.
	 *
	 * It is _NOT_ of production quality.
	 * The only intention behind this algorithm is to exercise and test the
	 * core scaling functionality of the driver.
	 * It is _NOT_ tuned for neither power saving nor performance!
	 *
	 * Other metrics than PP utilization need to be considered as well
	 * in order to make a good core scaling algorithm.
	 */

	MALI_DEBUG_PRINT(3, ("Utilization: (%3d, %3d, %3d), cores enabled: %d/%d\n", data->utilization_gpu, data->utilization_gp, data->utilization_pp, num_cores_enabled, num_cores_total));

	/* NOTE: this function is normally called directly from the utilization callback which is in
	 * timer context. */

	if (     PERCENT_OF(90, 256) < data->utilization_pp)
	{
		enable_max_num_cores();
	}
	else if (PERCENT_OF(50, 256) < data->utilization_pp)
	{
		enable_one_core();
	}
	else if (PERCENT_OF(40, 256) < data->utilization_pp)
	{
		/* do nothing */
	}
	else if (PERCENT_OF( 0, 256) < data->utilization_pp)
	{
		disable_one_core();
	}
	else
	{
		/* do nothing */
	}
}
int mali_platform_device_register(void)
{
	int err = 0;
	int num_pp_cores = 0;
	MALI_DEBUG_PRINT(2,("mali_platform_device_register() called\n"));

	/* Connect resources to the device */
	err = platform_device_add_resources(&mali_gpu_device, mali_gpu_resources, sizeof(mali_gpu_resources) / sizeof(mali_gpu_resources[0]));
	num_pp_cores = 4;

	if (0 == err)
	{
		err = platform_device_add_data(&mali_gpu_device, &mali_gpu_data, sizeof(mali_gpu_data));
		if (0 == err)
		{
			/* Register the platform device */
			err = platform_device_register(&mali_gpu_device);
			if (0 == err)
			{
				mali_platform_init();

#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
				pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 1000);
				pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
				pm_runtime_enable(&(mali_gpu_device.dev));
#endif
				mali_core_scaling_init(num_pp_cores);
				return 0;
			}
		}

		platform_device_unregister(&mali_gpu_device);
	}

	return err;
}
void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	mali_platform_deinit();
	mali_core_scaling_term();
	platform_device_unregister(&mali_gpu_device);
}

static void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}
static int mali_os_suspend(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_suspend() called\n"));
	
	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->suspend(device);
	}

	mali_platform_power_mode_change(MALI_POWER_MODE_DEEP_SLEEP);

	return ret;
}

static int mali_os_resume(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_resume() called\n"));

	mali_platform_power_mode_change(MALI_POWER_MODE_ON);

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->resume(device);
	}

	return ret;
}

static int mali_os_freeze(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_freeze() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->freeze)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->freeze(device);
	}

	return ret;
}

static int mali_os_thaw(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_thaw() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->thaw)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->thaw(device);
	}

	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int mali_runtime_suspend(struct device *device)
{
	int ret = 0;
	MALI_DEBUG_PRINT(4, ("mali_runtime_suspend() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_suspend(device);
	}

	mali_platform_power_mode_change(MALI_POWER_MODE_LIGHT_SLEEP);

	return ret;
}

static int mali_runtime_resume(struct device *device)
{
	int ret = 0;
	MALI_DEBUG_PRINT(4, ("mali_runtime_resume() called\n"));

	mali_platform_power_mode_change(MALI_POWER_MODE_ON);

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_resume(device);
	}

	return ret;
}

static int mali_runtime_idle(struct device *device)
{
	int ret = 0;
	MALI_DEBUG_PRINT(4, ("mali_runtime_idle() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_idle)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_idle(device);
		if (0 != ret)
		{
			return ret;
		}
	}

	pm_runtime_suspend(device);

	return 0;
}
#endif
void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
	mali_core_scaling_update(data);
}

