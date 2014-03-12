/*
 * Copyright (C) 2013 ROCKCHIP, Inc.
 * drivers/video/display/transmitter/mipi_dsi.c
 * author: hhb@rock-chips.com
 * create date: 2013-01-17
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 

#include <linux/module.h>
#include <linux/init.h>
#include <asm/system.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/rk_fb.h>
#include <linux/rk_screen.h>
#include <linux/ktime.h>

#include "mipi_dsi.h"

#define MAX_DSI_CHIPS 5

/*
*			 Driver Version Note
*
*v1.0 : this driver is a top level architecture of mipi dsi driver;
*v1.1 : add struct mipi_dsi_screen
*/
#define MIPI_DSI_VERSION_AND_TIME  "mipi_dsi v1.1 2013-07-23"


static struct mipi_dsi_ops *dsi_ops[MAX_DSI_CHIPS] = {NULL};
static struct mipi_dsi_ops *cur_dsi_ops;

int register_dsi_ops(struct mipi_dsi_ops *ops) {

	int i = 0;
	for(i = 0; i < MAX_DSI_CHIPS; i++) {
		if(!dsi_ops[i]) {
			dsi_ops[i] = ops;
			break;	
		}	
	}
	if(i == MAX_DSI_CHIPS) {
		printk("dsi ops support 5 chips at most\n");
		return -1;
	}
	return 0;	
}
EXPORT_SYMBOL(register_dsi_ops);


int del_dsi_ops(struct mipi_dsi_ops *ops) {

	int i = 0;
	for(i = 0; i < MAX_DSI_CHIPS; i++) {
		if(dsi_ops[i] == ops) {
			dsi_ops[i] = NULL;
			break;	
		}	
	}
	if(cur_dsi_ops == ops)
		cur_dsi_ops = NULL;
	if(i == MAX_DSI_CHIPS) {
		printk("dsi ops not found\n");
		return -1;
	}
	return 0;	
}
EXPORT_SYMBOL(del_dsi_ops);

int dsi_probe_current_chip(void) {
	int ret = 0;
	u32 i = 0, id;
	struct mipi_dsi_ops *ops = NULL;

	for(i = 0; i < MAX_DSI_CHIPS; i++) {
		if(dsi_ops[i]) {
			ops = dsi_ops[i];
			id = ops->get_id();
			if(id == ops->id) {
				cur_dsi_ops = ops;

				printk("load mipi dsi chip:%s id:%08x\n", ops->name, ops->id);
				printk("%s\n", MIPI_DSI_VERSION_AND_TIME);
				break;
			} else {
				printk("mipi dsi chip is not found, read id:%04x, but %04x is correct\n", id, ops->id);
				dsi_ops[i] = NULL;
				cur_dsi_ops = NULL;
				ret = -1;
			}
		}	
	}
	if(i == MAX_DSI_CHIPS)
		printk("no mipi dsi chip\n");

	return ret;
}
EXPORT_SYMBOL(dsi_probe_current_chip);

int dsi_power_up(void) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->power_up)
		cur_dsi_ops->power_up();
	return 0;
}
EXPORT_SYMBOL(dsi_power_up);


int dsi_power_off(void) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->power_down)
		cur_dsi_ops->power_down();
	return 0;
}
EXPORT_SYMBOL(dsi_power_off);

int dsi_set_regs(void *array, u32 n) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_set_regs)
		cur_dsi_ops->dsi_set_regs(array, n);
	return 0;
}
EXPORT_SYMBOL(dsi_set_regs);

int dsi_init(void *array, u32 n) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_init)
		cur_dsi_ops->dsi_init(array, n);
	return 0;
}
EXPORT_SYMBOL(dsi_init);

int dsi_enable_video_mode(u32 enable) {
	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_enable_video_mode)
		cur_dsi_ops->dsi_enable_video_mode(enable);
	return 0;

}
EXPORT_SYMBOL(dsi_enable_video_mode);

int dsi_enable_command_mode(u32 enable) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_enable_command_mode)
		cur_dsi_ops->dsi_enable_command_mode(enable);
	return 0;

}
EXPORT_SYMBOL(dsi_enable_command_mode);

int dsi_enable_hs_clk(u32 enable) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_enable_hs_clk)
		cur_dsi_ops->dsi_enable_hs_clk(enable);
	return 0;

}
EXPORT_SYMBOL(dsi_enable_hs_clk);

int dsi_is_active(void) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_is_active)
		return cur_dsi_ops->dsi_is_active();
	else
		return -1;
}
EXPORT_SYMBOL(dsi_is_active);


int dsi_send_dcs_packet(unsigned char *packet, u32 n) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_send_dcs_packet)
		cur_dsi_ops->dsi_send_dcs_packet(packet, n);
	return 0;
}
EXPORT_SYMBOL(dsi_send_dcs_packet);


int dsi_read_dcs_packet(unsigned char *packet, u32 n) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_read_dcs_packet)
		cur_dsi_ops->dsi_read_dcs_packet(packet, n);
	return 0;
}
EXPORT_SYMBOL(dsi_read_dcs_packet);


int dsi_send_packet(void *packet, u32 n) {

	if(!cur_dsi_ops)
		return -1;
	if(cur_dsi_ops->dsi_send_packet)
		cur_dsi_ops->dsi_send_packet(packet, n);
		
	return 0;
}
EXPORT_SYMBOL(dsi_send_packet);
