#ifndef __RK_VGA_H__
#define __RK_VGA_H__

enum vga_video_mode
{
	VGA_1280x720p_60Hz=1,
	VGA_1920x1080p_60Hz,
	VGA_1280x720p_50Hz,	
	VGA_1920x1080p_50Hz,
};

int rk29_get_vga_connect_status(void);
#endif