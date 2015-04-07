/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALI_DRV_H_
#define _MALI_DRV_H_

#define DRIVER_AUTHOR		"ARM"
#define DRIVER_NAME		"mali_drm"
#define DRIVER_DESC		"DRM module for Mali-200, Mali-400"
#define DRIVER_DATE		"20100520"
#define DRIVER_MAJOR		0
#define DRIVER_MINOR		1
#define DRIVER_PATCHLEVEL	0

#include <drm/drm_sman.h>

typedef struct drm_mali_private 
{
	drm_local_map_t *mmio;
	unsigned int idle_fault;
	struct drm_sman sman;
	int vram_initialized;
	unsigned long vram_offset;
} drm_mali_private_t;

extern int mali_idle(struct drm_device *dev);
extern void mali_reclaim_buffers_locked(struct drm_device *dev, struct drm_file *file_priv);
extern void mali_lastclose(struct drm_device *dev);
extern struct drm_ioctl_desc mali_ioctls[];
extern int mali_max_ioctl;

#endif /* _MALI_DRV_H_ */
