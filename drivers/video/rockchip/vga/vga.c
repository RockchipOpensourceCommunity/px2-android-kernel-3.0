#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/display-sys.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/board.h>

#include <linux/rk_screen.h>
#include <linux/rk_fb.h>
#include "../../edid.h"
#include "vga.h"

#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
	

#define OUT_TYPE		SCREEN_RGB
#define OUT_FACE		OUT_P888
#define DCLK_POL		1
#define SWAP_RB			0
#define LCD_ACLK		800000000

#define DDC_I2C_BUS		1
#define DDC_ADDR		0x50
#define DDC_I2C_RATE	100*1000
#define VGA_DET_PIN     RK30_PIN0_PB6  


static const struct fb_videomode rk29_mode[] = {
	//name				refresh		xres	yres	pixclock			h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag(used for vic)
{	"1024x768p@60Hz",	60,			1024,	768,	KHZ2PICOS(65000),	160,	24,		29,		3,		136,	6,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
{	"1280x720p@60Hz",	60,			1280,	720,	KHZ2PICOS(74250),	220,	110,	20,		5,		40,		5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},	
{	"1280x1024p@60Hz",	60,			1280,	1024,	KHZ2PICOS(108000),	248,	48,		38,		1,		112,	3,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
{	"1366x768p@60Hz",	60,			1366,	768,	KHZ2PICOS(85500),	213,	70,		24,		3,		143,	3,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
{	"1440x900p@60Hz",	60,			1440,	900,	KHZ2PICOS(116500),	232,	80,		25,		3,		152,	6,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
{	"1680x1050p@60Hz",	60,			1680,	1050,	KHZ2PICOS(146250),	280,	104,	30,		3,		176,	6,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
{	"1920x1080p@60Hz",	60,			1920,	1080,	KHZ2PICOS(148500),	148,	88,		36,		4,		44,		5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		0	},
};

struct rk29_monspecs {
	struct i2c_client			*i2c_client;
	struct rk_display_device	*ddev;
	int	io_enable_pin;
	int video_source;
	int property;
	struct fb_monspecs			monspecs;
	struct list_head			modelist;
	struct fb_videomode			*mode;
	int enable;
	struct workqueue_struct *workqueue;
	struct delayed_work delay_work;
	int status;
	#ifdef CONFIG_SWITCH
	struct switch_dev	switch_vga;
	#endif
};

static struct rk29_monspecs rk29_monspec;
static char *vgaenvent[] = {"INTERFACE=VGA", NULL}; 


int rk29_get_vga_connect_status(void)
{
	if(gpio_get_value(VGA_DET_PIN)==GPIO_HIGH){
		return 1;
	}else{
		return 0;
	}
}

static int FB_Switch_Screen( struct rk29fb_screen *screen, u32 enable )
{
	rk_fb_switch_screen(screen, enable ,rk29_monspec.video_source );	
}

static unsigned char *rk29fb_ddc_read(struct i2c_client *client)
{
	unsigned char *buf = kmalloc(EDID_LENGTH, GFP_KERNEL);
	int rc;

	if (!buf) {
		dev_warn(&client->dev, "unable to allocate memory for EDID "
			 "block.\n");
		return NULL;
	}
	// Check ddc i2c communication is available or not.
	memset(buf, 0, EDID_LENGTH);
	rc = i2c_master_reg8_recv(client, 0, buf, 6, DDC_I2C_RATE);
	if(rc == 6)
	{
		// Read EDID.
		memset(buf, 0, EDID_LENGTH);
		rc = i2c_master_reg8_recv(client, 0, buf, EDID_LENGTH, DDC_I2C_RATE);
		if(rc == EDID_LENGTH)
			return buf;
	}

	dev_warn(&client->dev, "unable to read EDID block.\n");
	kfree(buf);
	return NULL;
}

static struct fb_videomode *rk29fb_add_modelist(void)
{
	int i;
	struct fb_videomode *mode = NULL;
	struct list_head	*modelist = &rk29_monspec.modelist;
	
	fb_destroy_modelist(modelist);
	for(i = 0; i < ARRAY_SIZE(rk29_mode); i++)
	{
		mode = (struct fb_videomode *)&rk29_mode[i];	
		display_add_videomode(mode, modelist);
	}

	return;
}

static struct fb_videomode *rk29fb_set_default_modelist(void)
{
	int i;
	struct fb_videomode *mode = NULL;
	struct list_head	*modelist = &rk29_monspec.modelist;
	
	fb_destroy_modelist(modelist);
	for(i = 0; i < ARRAY_SIZE(rk29_mode); i++)
	{
		mode = (struct fb_videomode *)&rk29_mode[i];	
		display_add_videomode(mode, modelist);
	}
	rk29_monspec.mode = (struct fb_videomode *)&rk29_mode[1];
	return rk29_monspec.mode;
}

/*
 * Find monitor prefered video mode. If not find, 
 * set first mode as default mode. 
 */
static struct fb_videomode *rk29fb_read_vga_mode(void)
{
	struct fb_monspecs *specs = &rk29_monspec.monspecs;
	struct fb_videomode *modedb = NULL;
	int i, pixclock;

	printk("rk29fb_read_vga_mode %d\n",specs->modedb_len);
	if(specs->modedb_len) {
		/* Get max resolution timing */
		modedb = &specs->modedb[0];

		printk("rk29fb_read_vga_mode %d ,%d\n",modedb->xres,modedb->yres);
		/*
		for (i = 0; i < specs->modedb_len; i++) {
			if(specs->modedb[i].xres > modedb->xres)
				modedb = &specs->modedb[i];
			else if( (specs->modedb[i].xres == modedb->xres) && (specs->modedb[i].yres > modedb->yres) )
				modedb = &specs->modedb[i];
		}
		*/
		// For some monitor, the max pixclock read from EDID is smaller
		// than the clock of max resolution mode supported. We fix it.
		pixclock = PICOS2KHZ(modedb->pixclock);
		pixclock /= 250;
		pixclock *= 250;
		pixclock *= 1000;
		if(pixclock == 148250000)
			pixclock = 148500000;
		if(pixclock > specs->dclkmax)
			specs->dclkmax = pixclock;
	}
	else{
		printk("read modedb failed!\n");
		modedb = (struct fb_videomode *)&rk29_mode[1];	
	}
	return modedb;
}

static int rk29_mode2screen(struct fb_videomode *modedb, struct rk29fb_screen *screen)
{
	if(modedb == NULL || screen == NULL)
		return -1;
		
	memset(screen, 0, sizeof(struct rk29fb_screen));
	/* screen type & face */
    screen->type = SCREEN_HDMI;
    screen->face = OUT_P888;
	
	/* Screen size */
	screen->x_res = modedb->xres;
    screen->y_res = modedb->yres;
//	screen->xpos = 0;
//    screen->ypos = 0;
    /* Timing */
    screen->pixclock = PICOS2KHZ(modedb->pixclock);
    screen->pixclock /= 250;
    screen->pixclock *= 250;
    screen->pixclock *= 1000;
    printk("pixclock is %d\n", screen->pixclock);
	screen->lcdc_aclk = 500000000;
	screen->left_margin = modedb->left_margin;
	screen->right_margin = modedb->right_margin;
	screen->hsync_len = modedb->hsync_len;
	screen->upper_margin = modedb->upper_margin;
	screen->lower_margin = modedb->lower_margin;
	screen->vsync_len = modedb->vsync_len;

	/* Pin polarity */
	if(FB_SYNC_HOR_HIGH_ACT & modedb->sync)
		screen->pin_hsync = 1;
	else
		screen->pin_hsync = 0;
	if(FB_SYNC_VERT_HIGH_ACT & modedb->sync)
		screen->pin_vsync = 1;
	else
		screen->pin_vsync = 0;	
	screen->pin_den = 0;
	screen->pin_dclk = 1;

	/* Swap rule */
    screen->swap_rb = 0;
    screen->swap_rg = 0;
    screen->swap_gb = 0;
    screen->swap_delta = 0;
    screen->swap_dumy = 0;

    /* Operation function*/
    screen->init = NULL;
    screen->standby = NULL;	
	return 0;
}

static int rk29_set_enable(struct rk_display_device *device, int enable)
{
	struct rk29_monspecs *rk29_monspec = device->priv_data;
	printk("[%s] set enable %d\n", __FUNCTION__, enable);
	if(enable != rk29_monspec->enable)
	{
		if(rk29_monspec->io_enable_pin != INVALID_GPIO) {
			gpio_set_value(rk29_monspec->io_enable_pin, enable?GPIO_HIGH:GPIO_LOW);
		}
		rk29_monspec->enable = enable;
	}
	
	return 0;
}

static int rk29fb_init_screens(struct i2c_client *client)
{
	struct fb_monspecs	*spec = &rk29_monspec.monspecs;
	struct list_head	*modelist = &rk29_monspec.modelist;
	u8 *edid;
	struct fb_videomode *readmode, *mode;
	struct rk29fb_screen screen;	
	int i;
	
	if (client)
		edid = rk29fb_ddc_read(client);
	else
		edid = NULL;

	fb_destroy_modelist(modelist);
	INIT_LIST_HEAD(modelist);
	for(i = 0; i < ARRAY_SIZE(rk29_mode); i++)
	{
		mode = (struct fb_videomode *)&rk29_mode[i];	
		display_add_videomode(mode, modelist);
	}
	
	if(spec->modedb)
		kfree(spec->modedb);
	memset(spec, 0, sizeof(struct fb_monspecs));
	if(edid){
		fb_edid_to_monspecs(edid, spec);
		kfree(edid);	
		readmode = rk29fb_read_vga_mode();
		mode = (struct fb_videomode *)fb_find_nearest_mode(readmode, &rk29_monspec.modelist);
		if(!rk29_get_vga_connect_status()){
			printk("VGA and lcd can,t use at the same time\n");
			return -1;
		}
	}else{
		if(rk29_get_vga_connect_status()){
			mode = (struct fb_videomode *)&rk29_mode[1];
		}else{
			printk("switch lcd device\n");
			return -1;
		}	
	}
	rk29_monspec.mode = mode;	
	rk29_mode2screen(mode, &screen);
	FB_Switch_Screen(&screen, 1);
	rk29_set_enable(rk29_monspec.ddev,1);
	#ifdef CONFIG_SWITCH
	rk29_monspec.switch_vga.name="vga";
	switch_dev_register(&(rk29_monspec.switch_vga));
	#endif
	switch_set_state(&(rk29_monspec.switch_vga), 1);

	kobject_uevent_env(&client->dev.kobj, KOBJ_ADD, vgaenvent);

	return 0;
}




static int rk29_get_enable(struct rk_display_device *device)
{
	struct rk29_monspecs *rk29_monspec = device->priv_data;
	return rk29_monspec->enable;
}

static int rk29_get_status(struct rk_display_device *device)
{
	return 0;
}

static int rk29_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
	*modelist = &rk29_monspec.modelist;
	return 0;
}

static int rk29_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	if(rk29_monspec.mode)
	{
		memcpy(mode, rk29_monspec.mode, sizeof(struct fb_videomode));
		return 0;
	}
	else
		return -1;
}

static int rk29_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;
	if(rk29_get_vga_connect_status())
	{
		for(i = 0; i < ARRAY_SIZE(rk29_mode); i++)
		{
			if(fb_mode_is_equal(&rk29_mode[i], mode))
			{	
				struct rk29fb_screen screen;	
				rk29_mode2screen(mode, &screen);
				FB_Switch_Screen(&screen, 1);
				rk29_monspec.mode = mode;
				return 0;
			}
		}
	}
	return -1;
}
struct rk_display_ops rk29_display_ops = {
	.setenable = rk29_set_enable,
	.getenable = rk29_get_enable,
	.getstatus = rk29_get_status,
	.getmodelist = rk29_get_modelist,
	.setmode = rk29_set_mode,
	.getmode = rk29_get_mode,
};

static int rk29_display_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "VGA");
	device->name = "vga";
	device->property = rk29_monspec.property;
	device->priority = DISPLAY_PRIORITY_VGA;
	device->priv_data = devdata;
	device->ops = &rk29_display_ops;
	

	return 1;
}

static struct rk_display_driver display_rk29 = {
	.probe = rk29_display_probe,
};

static int vga_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{    
    int ret;
    struct rkdisplay_platform_data *vga_data;
    
    memset(&rk29_monspec, 0, sizeof(struct rk29_monspecs));
    rk29_monspec.i2c_client = client;
    rk29_monspec.enable = 0;
    
    if(client->dev.platform_data) {
    	vga_data = client->dev.platform_data;
    	rk29_monspec.io_enable_pin = vga_data->io_pwr_pin;
    	rk29_monspec.video_source = vga_data->video_source;
		rk29_monspec.property = vga_data->property;
    }
   	else {
   		rk29_monspec.io_enable_pin = INVALID_GPIO;
		rk29_monspec.video_source = DISPLAY_SOURCE_LCDC0;
		rk29_monspec.property = DISPLAY_VGA;
    }
    if(rk29_monspec.io_enable_pin != INVALID_GPIO)
    {
        ret = gpio_request(rk29_monspec.io_enable_pin, NULL);
        if(ret != 0)
        {
            gpio_free(rk29_monspec.io_enable_pin);
            printk(">>>>>> vag enable gpio_request err \n ");
            return -1;
        }
		gpio_direction_output(rk29_monspec.io_enable_pin, GPIO_LOW);
    }
    INIT_LIST_HEAD(&rk29_monspec.modelist);
    rk29_monspec.ddev = rk_display_device_register(&display_rk29, &client->dev, &rk29_monspec);
	if(rk29_monspec.ddev == NULL)
	{
		printk("[%s] registor display error\n", __FUNCTION__);
		return -1;
	}
	
	rk29fb_init_screens(client);
	
	printk("VGA probe successful\n");
	return 0;
}


static int __devexit vga_i2c_remove(struct i2c_client *client)
{
	struct fb_monspecs	*spec = &rk29_monspec.monspecs;
	struct list_head	*modelist = &rk29_monspec.modelist;
	fb_destroy_modelist(modelist);
	if(spec->modedb)
		kfree(spec->modedb);
	rk_display_device_unregister(rk29_monspec.ddev);
	return 0;
}

static const struct i2c_device_id vga_id[] = {
	{ "vga_i2c", 0 },
	{ }
};

static struct i2c_driver vga_i2c_driver  = {
    .driver = {
        .name  = "vga_i2c",
        .owner = THIS_MODULE,
    },
    .probe =    &vga_i2c_probe,
    .remove     = &vga_i2c_remove,
    .id_table	= vga_id,
};

static int __init rk29_vga_init(void)
{
    return i2c_add_driver(&vga_i2c_driver);
}

static void __exit rk29_vga_exit(void)
{
    i2c_del_driver(&vga_i2c_driver);
}

module_init(rk29_vga_init);
module_exit(rk29_vga_exit);
