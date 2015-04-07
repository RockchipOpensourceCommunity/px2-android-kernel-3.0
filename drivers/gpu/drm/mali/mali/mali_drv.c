/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <linux/vermagic.h>
#include <linux/version.h>
#include <drm/drmP.h>
#include "mali_drm.h"
#include "mali_drv.h"

static struct platform_device *pdev;


#if 0
static const struct drm_device_id dock_device_ids[] = {
	{"MALIDRM", 0},
	{"", 0},
};
#endif

static int mali_driver_load(struct drm_device *dev, unsigned long chipset)
{
	int ret;
	//unsigned long base, size;
	drm_mali_private_t *dev_priv;
	printk(KERN_ERR "DRM: mali_driver_load start\n");

	dev_priv = kcalloc(1, sizeof(drm_mali_private_t), GFP_KERNEL);
	if ( dev_priv == NULL ) return -ENOMEM;

	dev->dev_private = (void *)dev_priv;

	if ( NULL == dev->platformdev )
	{
		dev->platformdev = platform_device_register_simple(DRIVER_NAME, 0, NULL, 0);
		pdev = dev->platformdev;
	}

	#if 0
	base = drm_get_resource_start(dev, 1 );
	size = drm_get_resource_len(dev, 1 );
	#endif
	ret = drm_sman_init(&dev_priv->sman, 2, 12, 8);
	//if ( ret ) drm_free(dev_priv, sizeof(dev_priv), DRM_MEM_DRIVER);
	if ( ret ) kfree( dev_priv );

	printk(KERN_ERR "DRM: mali_driver_load done\n");

	return ret;
}

static int mali_driver_unload( struct drm_device *dev )
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: mali_driver_unload start\n");

	drm_sman_takedown(&dev_priv->sman);
	//drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);
	kfree( dev_priv );
	printk(KERN_ERR "DRM: mali_driver_unload done\n");

	return 0;
}

static struct drm_driver driver = 
{
	.driver_features = DRIVER_BUS_PLATFORM,
	.load = mali_driver_load,
	.unload = mali_driver_unload,
	.context_dtor = NULL,
	.dma_quiescent = mali_idle,
	.reclaim_buffers = NULL,
	.reclaim_buffers_idlelocked = mali_reclaim_buffers_locked,
	.lastclose = mali_lastclose,
	//.get_map_ofs = drm_core_get_map_ofs,
	//.get_reg_ofs = drm_core_get_reg_ofs,
	.ioctls = mali_ioctls,
	.fops = {
		 .owner = THIS_MODULE,
		 .open = drm_open,
		 .release = drm_release,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
		 .ioctl = drm_ioctl,
#else
		 .unlocked_ioctl = drm_ioctl,
#endif
		 .mmap = drm_mmap,
		 .poll = drm_poll,
		 .fasync = drm_fasync,
	},
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

int mali_drm_init(struct platform_device *dev)
{
	printk(KERN_INFO "Mali DRM initialize, driver name: %s, version %d.%d\n", DRIVER_NAME,
	       DRIVER_MAJOR,DRIVER_MINOR);
	if (dev == pdev) 
	{
		driver.num_ioctls = 0;
		driver.kdriver.platform_device = dev;
		return drm_platform_init(&driver, dev);
	} 
	else
	{
		printk("Mali DRM initialize error\r\n");
	}
	return 0;
}
void mali_drm_exit(struct platform_device *dev)
{
	if (driver.kdriver.platform_device == dev)
	{
		drm_platform_exit(&driver, dev);
	}
}
static int __devinit mali_platform_drm_probe(struct platform_device *dev)
{
	return mali_drm_init(dev);
}
static int mali_platform_drm_remove(struct platform_device *dev)
{
	mali_drm_exit(dev);
	return 0;
}
static int mali_platform_drm_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}
static int mali_platform_drm_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver platform_drm_driver = {
	.probe = mali_platform_drm_probe,
	.remove = __devexit_p(mali_platform_drm_remove),
	.suspend = mali_platform_drm_suspend,
	.resume = mali_platform_drm_resume,
	.driver = {
	.owner = THIS_MODULE,
	.name = DRIVER_NAME,
	},
};

static int __init mali_init(void)
{
	//driver.num_ioctls = mali_max_ioctl;
	pdev = platform_device_register_simple("mali_drm", 0, NULL, 0);
	//return drm_init(&driver);
	printk("mali drm init\r\n");
	return platform_driver_register( &platform_drm_driver );
}

static void __exit mali_exit(void)
{
	platform_driver_unregister( &platform_drm_driver );
	platform_device_unregister( pdev );
	//drm_exit(&driver);
}

late_initcall(mali_init);
module_exit(mali_exit);

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_AUTHOR("ARM Ltd.");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
