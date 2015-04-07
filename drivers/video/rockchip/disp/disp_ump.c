/*
 * Copyright (C) 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef CONFIG_FB_RK_UMP
#error This file should not be built without UMP enabled
#endif

#include <ump/ump_kernel_interface_ref_drv.h>
#include <linux/rk_fb.h>
#include <asm/uaccess.h>

static int _disp_get_ump_secure_id(struct fb_info *info, struct rk_fb_inf *g_fbi,unsigned long arg, int buf)
{
	u32 __user *psecureid = (u32 __user *) arg;
	int buf_len = info->fix.smem_len;
	ump_secure_id secure_id;

	if (!g_fbi->ump_wrapped_buffer[buf]) 
	{
		ump_dd_physical_block ump_memory_description;

		ump_memory_description.addr = info->fix.smem_start;
		ump_memory_description.size = info->fix.smem_len;
#if 1
		if (buf > 0) {
			ump_memory_description.addr +=(buf_len * (buf - 1));
			ump_memory_description.size = buf_len;
		}
#endif
		g_fbi->ump_wrapped_buffer[buf] =ump_dd_handle_create_from_phys_blocks(&ump_memory_description, 1);
	}
	
	secure_id = ump_dd_secure_id_get(g_fbi->ump_wrapped_buffer[buf]);
	//printk("buf = %d,g_fbi->ump_wrapped_buffer[%d]= %p,len =0x%08x,secure_id = %d\n",buf,buf,g_fbi->ump_wrapped_buffer[buf],buf_len,secure_id);
	return put_user((unsigned int)secure_id, psecureid);
}

static int __init disp_ump_module_init(void)
{
	int ret = 0;

	disp_get_ump_secure_id = _disp_get_ump_secure_id;
	printk("%s\r\n",__func__);
	return ret;
}

static void __exit disp_ump_module_exit(void)
{
	disp_get_ump_secure_id = NULL;
}

late_initcall_sync(disp_ump_module_init);
module_exit(disp_ump_module_exit);

MODULE_AUTHOR("Simon <xxm@rock-chips.com>");
MODULE_DESCRIPTION("display driver MALI UMP module");
MODULE_LICENSE("GPL");
