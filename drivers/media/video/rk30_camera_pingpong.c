/*
 * V4L2 Driver for RK28 camera host
 *
 * Copyright (C) 2006, Sascha Hauer, Pengutronix
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#if defined(CONFIG_ARCH_RK2928) || defined(CONFIG_ARCH_RK30) ||defined(CONFIG_ARCH_RK3188)
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <mach/iomux.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf-dma-contig.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <mach/io.h>
#include <plat/ipp.h>
#include <plat/vpu_service.h>
#include "../../video/rockchip/rga/rga.h"
#if defined(CONFIG_ARCH_RK30)||defined(CONFIG_ARCH_RK3188)
#include <mach/rk30_camera.h>
#include <mach/cru.h>
#include <mach/pmu.h>
#endif

#if defined(CONFIG_ARCH_RK2928)
#include <mach/rk2928_camera.h>
#include <mach/cru.h>
#include <mach/pmu.h>
#define SOFT_RST_CIF1 (SOFT_RST_MAX+1)
#endif
#include <asm/cacheflush.h>
static int debug = 0;
module_param(debug, int, S_IRUGO|S_IWUSR);

#if defined(CONFIG_TRACE_LOG_PRINTK)
 #define RK30_CAM_DEBUG_TRACE(format, ...) printk(KERN_ERR"rk30_camera: " format, ## __VA_ARGS__)
#else
 #define RK30_CAM_DEBUG_TRACE(format, ...)
#endif
#define RK30_CAM_LOG_TRACE(format, ...) printk(KERN_WARNING"rk30_camera: " format, ## __VA_ARGS__)

#define dprintk(level, fmt, arg...) do {			\
	if (debug >= level) 					\
	printk(KERN_WARNING"rk_camera: " fmt , ## arg); } while (0)

#define RKCAMERA_TR(format, ...) printk(KERN_ERR format, ## __VA_ARGS__)
#define RKCAMERA_DG(format, ...) dprintk(1, format, ## __VA_ARGS__)
// CIF Reg Offset
#define  CIF_CIF_CTRL                       0x00
#define  CIF_CIF_INTEN                      0x04
#define  CIF_CIF_INTSTAT                    0x08
#define  CIF_CIF_FOR                        0x0c
#define  CIF_CIF_LINE_NUM_ADDR              0x10
#define  CIF_CIF_FRM0_ADDR_Y                0x14
#define  CIF_CIF_FRM0_ADDR_UV               0x18
#define  CIF_CIF_FRM1_ADDR_Y                0x1c
#define  CIF_CIF_FRM1_ADDR_UV               0x20
#define  CIF_CIF_VIR_LINE_WIDTH             0x24
#define  CIF_CIF_SET_SIZE                   0x28
#define  CIF_CIF_SCM_ADDR_Y                 0x2c
#define  CIF_CIF_SCM_ADDR_U                 0x30
#define  CIF_CIF_SCM_ADDR_V                 0x34
#define  CIF_CIF_WB_UP_FILTER               0x38
#define  CIF_CIF_WB_LOW_FILTER              0x3c
#define  CIF_CIF_WBC_CNT                    0x40
#define  CIF_CIF_CROP                       0x44
#define  CIF_CIF_SCL_CTRL                   0x48
#define	 CIF_CIF_SCL_DST                    0x4c
#define	 CIF_CIF_SCL_FCT                    0x50
#define	 CIF_CIF_SCL_VALID_NUM              0x54
#define	 CIF_CIF_LINE_LOOP_CTR              0x58
#define	 CIF_CIF_FRAME_STATUS               0x60
#define	 CIF_CIF_CUR_DST                    0x64
#define	 CIF_CIF_LAST_LINE                  0x68
#define	 CIF_CIF_LAST_PIX                   0x6c

//The key register bit descrition
// CIF_CTRL Reg , ignore SCM,WBC,ISP,
#define  DISABLE_CAPTURE              (0x00<<0)
#define  ENABLE_CAPTURE               (0x01<<0)
#define  MODE_ONEFRAME                (0x00<<1)
#define  MODE_PINGPONG                (0x01<<1)
#define  MODE_LINELOOP                (0x02<<1)
#define  AXI_BURST_16                 (0x0F << 12)

//CIF_CIF_INTEN
#define  FRAME_END_EN			(0x01<<1)
#define  BUS_ERR_EN				(0x01<<6)
#define  SCL_ERR_EN				(0x01<<7)

//CIF_CIF_FOR
#define  VSY_HIGH_ACTIVE                   (0x01<<0)
#define  VSY_LOW_ACTIVE                    (0x00<<0)
#define  HSY_LOW_ACTIVE 			       (0x01<<1)
#define  HSY_HIGH_ACTIVE			       (0x00<<1)
#define  INPUT_MODE_YUV 			       (0x00<<2)
#define  INPUT_MODE_PAL 			       (0x02<<2)
#define  INPUT_MODE_NTSC 			       (0x03<<2)
#define  INPUT_MODE_RAW 			       (0x04<<2)
#define  INPUT_MODE_JPEG 			       (0x05<<2)
#define  INPUT_MODE_MIPI			       (0x06<<2)
#define  YUV_INPUT_ORDER_UYVY(ori)	       (ori & (~(0x03<<5)))
#define  YUV_INPUT_ORDER_YVYU(ori)		   ((ori & (~(0x01<<6)))|(0x01<<5))
#define  YUV_INPUT_ORDER_VYUY(ori)		   ((ori & (~(0x01<<5))) | (0x1<<6))
#define  YUV_INPUT_ORDER_YUYV(ori)		   (ori|(0x03<<5))
#define  YUV_INPUT_422		               (0x00<<7)
#define  YUV_INPUT_420		               (0x01<<7)
#define  INPUT_420_ORDER_EVEN		       (0x00<<8)
#define  INPUT_420_ORDER_ODD		       (0x01<<8)
#define  CCIR_INPUT_ORDER_ODD		       (0x00<<9)
#define  CCIR_INPUT_ORDER_EVEN             (0x01<<9)
#define  RAW_DATA_WIDTH_8                  (0x00<<11)
#define  RAW_DATA_WIDTH_10                 (0x01<<11)
#define  RAW_DATA_WIDTH_12                 (0x02<<11)	
#define  YUV_OUTPUT_422                    (0x00<<16)
#define  YUV_OUTPUT_420                    (0x01<<16)
#define  OUTPUT_420_ORDER_EVEN             (0x00<<17)
#define  OUTPUT_420_ORDER_ODD              (0x01<<17)
#define  RAWD_DATA_LITTLE_ENDIAN           (0x00<<18)
#define  RAWD_DATA_BIG_ENDIAN              (0x01<<18)
#define  UV_STORAGE_ORDER_UVUV             (0x00<<19)
#define  UV_STORAGE_ORDER_VUVU             (0x01<<19)

//CIF_CIF_SCL_CTRL
#define ENABLE_SCL_DOWN                    (0x01<<0)		
#define DISABLE_SCL_DOWN                   (0x00<<0)
#define ENABLE_SCL_UP                      (0x01<<1)		
#define DISABLE_SCL_UP                     (0x00<<1)
#define ENABLE_YUV_16BIT_BYPASS            (0x01<<4)
#define DISABLE_YUV_16BIT_BYPASS           (0x00<<4)
#define ENABLE_RAW_16BIT_BYPASS            (0x01<<5)
#define DISABLE_RAW_16BIT_BYPASS           (0x00<<5)
#define ENABLE_32BIT_BYPASS                (0x01<<6)
#define DISABLE_32BIT_BYPASS               (0x00<<6)

//CIF_CIF_FRAME_STATUS
#define CIF_F0_READY (0x01<<0)
#define CIF_F1_READY (0x01<<1)

#define MIN(x,y)   ((x<y) ? x: y)
#define MAX(x,y)    ((x>y) ? x: y)
#define RK_SENSOR_12MHZ      12*1000*1000 
#define RK_SENSOR_24MHZ      24*1000*1000          /* MHz */
#define RK_SENSOR_48MHZ      48*1000*1000

#define write_cif_reg(base,addr, val)  __raw_writel(val, addr+(base))
#define read_cif_reg(base,addr) __raw_readl(addr+(base))
#define mask_cif_reg(addr, msk, val)    write_cif_reg(addr, (val)|((~(msk))&read_cif_reg(addr)))

#if defined(CONFIG_ARCH_RK30) || defined(CONFIG_ARCH_RK3188)
//CRU,PIXCLOCK
#define CRU_PCLK_REG30                     0xbc
#define ENANABLE_INVERT_PCLK_CIF0          ((0x1<<24)|(0x1<<8))
#define DISABLE_INVERT_PCLK_CIF0           ((0x1<<24)|(0x0<<8))
#define ENANABLE_INVERT_PCLK_CIF1          ((0x1<<28)|(0x1<<12))
#define DISABLE_INVERT_PCLK_CIF1           ((0x1<<28)|(0x0<<12))

#define CRU_CIF_RST_REG30                  0x128
#define MASK_RST_CIF0                      (0x01 << 30)
#define MASK_RST_CIF1                      (0x01 << 31)
#define RQUEST_RST_CIF0                    (0x01 << 14)
#define RQUEST_RST_CIF1                    (0x01 << 15)

#define write_cru_reg(addr, val)  __raw_writel(val, addr+RK30_CRU_BASE)
#define read_cru_reg(addr) __raw_readl(addr+RK30_CRU_BASE)
#define mask_cru_reg(addr, msk, val)	write_cru_reg(addr,(val)|((~(msk))&read_cru_reg(addr)))
#endif

#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
//GRF_IO_CON3                        0x100
#define CIF_DRIVER_STRENGTH_2MA            (0x00 << 12)
#define CIF_DRIVER_STRENGTH_4MA            (0x01 << 12)
#define CIF_DRIVER_STRENGTH_8MA            (0x02 << 12)
#define CIF_DRIVER_STRENGTH_12MA           (0x03 << 12)
#define CIF_DRIVER_STRENGTH_MASK           (0x03 << 28)

//GRF_IO_CON4                        0x104
#define CIF_CLKOUT_AMP_3V3                 (0x00 << 10)
#define CIF_CLKOUT_AMP_1V8                 (0x01 << 10)
#define CIF_CLKOUT_AMP_MASK                (0x01 << 26)

#define write_grf_reg(addr, val)  __raw_writel(val, addr+RK30_GRF_BASE)
#define read_grf_reg(addr) __raw_readl(addr+RK30_GRF_BASE)
#define mask_grf_reg(addr, msk, val)	write_grf_reg(addr,(val)|((~(msk))&read_grf_reg(addr)))
#else
#define write_grf_reg(addr, val)  
#define read_grf_reg(addr)                0
#define mask_grf_reg(addr, msk, val)	
#endif

#if defined(CONFIG_ARCH_RK2928)
#define write_cru_reg(addr, val)  
#define read_cru_reg(addr)                 0 
#define mask_cru_reg(addr, msk, val)	
#endif


//when work_with_ipp is not enabled,CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_OFF is not defined.something wrong with it
#ifdef CONFIG_VIDEO_RK29_WORK_IPP//CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_OFF
#ifdef CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_OFF
#define CAM_WORKQUEUE_IS_EN()   ((pcdev->host_width != pcdev->icd->user_width) || (pcdev->host_height != pcdev->icd->user_height)\
                                      || (pcdev->icd_cb.sensor_cb))
#define CAM_IPPWORK_IS_EN()     ((pcdev->host_width != pcdev->icd->user_width) || (pcdev->host_height != pcdev->icd->user_height))                                  
#else
#define CAM_WORKQUEUE_IS_EN()  (true)
#define CAM_IPPWORK_IS_EN()     ((pcdev->zoominfo.a.c.width != pcdev->icd->user_width) || (pcdev->zoominfo.a.c.height != pcdev->icd->user_height))
#endif
#else //CONFIG_VIDEO_RK29_WORK_IPP
#define CAM_WORKQUEUE_IS_EN()    (false)
#define CAM_IPPWORK_IS_EN()      (false) 
#endif

#define IS_CIF0()		(pcdev->hostid == RK_CAM_PLATFORM_DEV_ID_0)
#if (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_IPP)
#define CROP_ALIGN_BYTES (0x03)
#define CIF_DO_CROP 0
#elif (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_ARM)
#define CROP_ALIGN_BYTES (0x03)
#define CIF_DO_CROP 0
#elif (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_RGA)
#define CROP_ALIGN_BYTES (0x03)
#define CIF_DO_CROP 0
#elif(CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_PP)
#define CROP_ALIGN_BYTES (0x0F)
#define CIF_DO_CROP 1
#endif
//Configure Macro
/*
*			 Driver Version Note
*
*v0.0.x : this driver is 2.6.32 kernel driver;
*v0.1.x : this driver is 3.0.8 kernel driver;
*
*v0.x.1 : this driver first support rk2918;
*v0.x.2 : fix this driver support v4l2 format is V4L2_PIX_FMT_NV12 and V4L2_PIX_FMT_NV16,is not V4L2_PIX_FMT_YUV420 
*		  and V4L2_PIX_FMT_YUV422P;
*v0.x.3 : this driver support VIDIOC_ENUM_FRAMEINTERVALS;
*v0.x.4 : this driver support digital zoom;
*v0.x.5 : this driver support test framerate and query framerate from board file configuration;
*v0.x.6 : this driver improve test framerate method;
*v0.x.7 : digital zoom use the ipp to do scale and crop , otherwise ipp just do the scale. Something wrong with digital zoom if
          we do crop with cif and do scale with ipp , we will fix this  next version.
*v0.x.8 : temp version,reinit capture list when setup video buf.
*v0.x.9 : 1. add the case of IPP unsupportted ,just cropped by CIF(not do the scale) this version. 
          2. flush workqueue when releas buffer
*v0.x.a: 1. reset cif and wake up vb when cif have't receive data in a fixed time(now setted as 2 secs) so that app can
             be quitted
          2. when the flash is on ,flash off in a fixed time to prevent from flash light too hot.
          3. when front and back camera are the same sensor,and one has the flash ,other is not,flash can't work corrrectly ,fix it
          4. add  menu configs for convineuent to customize sensor series
*v0.x.b:  specify the version is  NOT sure stable.
*v0.x.c:  1. add cif reset when resolution changed to avoid of collecting data erro
          2. irq process is splitted to two step.
*v0.x.e: fix bugs of early suspend when display_pd is closed. 
*v0.x.f: fix calculate ipp memory size is enough or not in try_fmt function; 
*v0.x.11: fix struct rk_camera_work may be reentrant
*v0.x.13: 1.add scale by arm,rga and pp.
          2.CIF do the crop when digital zoom.
		  3.fix bug in prob func:request mem twice. 
		  4.video_vq may be null when reinit work,fix it
		  5.arm scale algorithm has something wrong(may exceed the bound of width or height) ,fix it.
*v0.x.15: 
*         1. support rk3066b;
*v0.4.15:
*         1. cif work on pingping mode;
*v0.x.17: 
*         1. support 8Mega picture;
*v0.x.19:
*         1. invalidate the limit which scale is invalidat when scale ratio > 2;
*v0.x.1b: 1. fix oops bug  when using arm to do scale_crop if preview buffer is not allocated correctly 
          2. rk_camera_set_fmt will called twice, optimize the procedure. at the first time call ,just to do the sensor init.

*v0.x.1c:
*         1. fix query resolution error;
*v0.x.1d: 
*         1. add mv9335+ov5650 driver;
*         2. fix 2928 digitzoom erro(arm crop scale) of selected zone;
*v0.x.1f:
*         1. support rk3188
*v0.x.21:
		  1. cif change mode to free run.support icatch 7002.
*/
#define RK_CAM_VERSION_CODE KERNEL_VERSION(0, 4, 0x21)

/* limit to rk29 hardware capabilities */
#define RK_CAM_BUS_PARAM   (SOCAM_MASTER |\
                SOCAM_HSYNC_ACTIVE_HIGH |\
                SOCAM_HSYNC_ACTIVE_LOW |\
                SOCAM_VSYNC_ACTIVE_HIGH |\
                SOCAM_VSYNC_ACTIVE_LOW |\
                SOCAM_PCLK_SAMPLE_RISING |\
                SOCAM_PCLK_SAMPLE_FALLING|\
                SOCAM_DATA_ACTIVE_HIGH |\
                SOCAM_DATA_ACTIVE_LOW|\
                SOCAM_DATAWIDTH_8|SOCAM_DATAWIDTH_10|\
                SOCAM_MCLK_24MHZ |SOCAM_MCLK_48MHZ)

#define RK_CAM_W_MIN        48
#define RK_CAM_H_MIN        32
#define RK_CAM_W_MAX        3856            /* ddl@rock-chips.com : 10M Pixel */
#define RK_CAM_H_MAX        2764
#define RK_CAM_FRAME_INVAL_INIT 0
#define RK_CAM_FRAME_INVAL_DC 0          /* ddl@rock-chips.com :  */
#define RK30_CAM_FRAME_MEASURE  5
extern void videobuf_dma_contig_free(struct videobuf_queue *q, struct videobuf_buffer *buf);
extern dma_addr_t videobuf_to_dma_contig(struct videobuf_buffer *buf);

/* buffer for one video frame */
struct rk_camera_buffer
{
    /* common v4l buffer stuff -- must be first */
    struct videobuf_buffer vb;
    enum v4l2_mbus_pixelcode	code;
    int			inwork;
};

enum rk_camera_reg_state
{
	Reg_Invalidate,
	Reg_Validate
};

struct rk_camera_reg
{
	unsigned int cifCtrl;
	unsigned int cifCrop;
	unsigned int cifFs;
	unsigned int cifIntEn;
	unsigned int cifFmt;
    unsigned int cifVirWidth;
    unsigned int cifScale;
//	unsigned int VipCrm;
	enum rk_camera_reg_state Inval;
};
struct rk_camera_work
{
	struct videobuf_buffer *vb;
	struct rk_camera_dev *pcdev;
	struct work_struct work;
    struct list_head queue;
    unsigned int index; 
    unsigned int ts;
};

struct rk_camera_frmivalenum
{
    struct v4l2_frmivalenum fival;
    struct rk_camera_frmivalenum *nxt;
};

struct rk_camera_frmivalinfo
{
    struct soc_camera_device *icd;
    struct rk_camera_frmivalenum *fival_list;
};

struct rk_camera_zoominfo
{
    struct semaphore sem;
    struct v4l2_crop a;
    int vir_width;
   int vir_height;
    int zoom_rate;
};

#if CAMERA_VIDEOBUF_ARM_ACCESS
struct rk29_camera_vbinfo
{
    unsigned int phy_addr;
    void __iomem *vir_addr;
    unsigned int size;
};
#endif

struct rk_camera_timer{
	struct rk_camera_dev *pcdev;
	struct hrtimer timer;
    bool istarted;
};
struct rk_cif_clk
{
    //************must modify start************/
    struct clk *pd_cif;
    struct clk *aclk_cif;
    struct clk *hclk_cif;
    struct clk *cif_clk_in;
    struct clk *cif_clk_out;
    //************must modify end************/

    spinlock_t lock;
    bool on;
};
static struct rk_cif_clk  cif_clk[2];
struct hdr_exposure
{
    unsigned int set_ts;
    unsigned int get_ts;
    unsigned int code;
};
struct rk_hdr_info_s
{
    bool en;
    struct hdr_exposure frame[3];
};
struct rk_cif_crop
{
    spinlock_t lock;
    struct v4l2_rect c;
    struct v4l2_rect bounds;
};

#define CONFIG_CIF_STOP_SYNC 1

struct rk_camera_dev
{
	struct soc_camera_host	soc_host;
	struct device		*dev;
	/* RK2827x is only supposed to handle one camera on its Quick Capture
	 * interface. If anyone ever builds hardware to enable more than
	 * one camera, they will have to modify this driver too */
	struct soc_camera_device *icd;

	//************must modify start************/
	struct clk *pd_cif;
	struct clk *aclk_cif;
    struct clk *hclk_cif;
    struct clk *cif_clk_in;
    struct clk *cif_clk_out;
	//************must modify end************/
	void __iomem *base;
	int frame_inval;           /* ddl@rock-chips.com : The first frames is invalidate  */
	unsigned int irq;
	unsigned int fps;
    unsigned int last_fps;
    unsigned long frame_interval;
	unsigned int pixfmt;
	//for ipp	
	unsigned int vipmem_phybase;
    void __iomem *vipmem_virbase;
	unsigned int vipmem_size;
	unsigned int vipmem_bsize;
#if CAMERA_VIDEOBUF_ARM_ACCESS    
    struct rk29_camera_vbinfo *vbinfo;
#endif  
	unsigned int vbinfo_count;

	int host_width;
	int host_height;
	int host_left;  //sensor output size ?
	int host_top;
	int hostid;
    int icd_width;
    int icd_height;
    struct rk_cif_crop cropinfo;

	struct rk29camera_platform_data *pdata;
	struct resource		*res;
	struct list_head	capture;
	struct rk_camera_zoominfo zoominfo;

	spinlock_t		lock;

	struct videobuf_buffer	*active0;
	struct videobuf_buffer	*active1;
	struct rk_camera_reg reginfo_suspend;
	struct workqueue_struct *camera_wq;
	struct rk_camera_work *camera_work;
    struct list_head camera_work_queue;
    spinlock_t camera_work_lock;
	unsigned int camera_work_count;
	struct rk_camera_timer fps_timer;
	struct rk_camera_work camera_reinit_work;
	int icd_init;
	rk29_camera_sensor_cb_s icd_cb;
	struct rk_camera_frmivalinfo icd_frmival[2];
    bool timer_get_fps;
    unsigned int reinit_times; 
    struct videobuf_queue *video_vq;
    volatile bool stop_cif;
#if CONFIG_CIF_STOP_SYNC
	wait_queue_head_t cif_stop_done;
	volatile  bool cif_stopped;
#endif
    struct timeval first_tv;

    struct rk_hdr_info_s  hdr_info;
//	spinlock_t		irq_lock;
};

static const struct v4l2_queryctrl rk_camera_controls[] =
{
	#ifdef CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_ON
    {
        .id		= V4L2_CID_ZOOM_ABSOLUTE,
        .type		= V4L2_CTRL_TYPE_INTEGER,
        .name		= "DigitalZoom Control",
        .minimum	= 100,
        .maximum	= 300,
        .step		= 5,
        .default_value = 100,
    },
    #endif

    {
        .id		= V4L2_CID_HDR,
        .type		= V4L2_CTRL_TYPE_BOOLEAN,
        .name		= "HDR",
        .minimum	= 0,
        .maximum	= 1,
        .step		= 1,
        .default_value = 0,
    }

    
};

static DEFINE_MUTEX(camera_lock);
static const char *rk_cam_driver_description = "RK_Camera";

static int rk_camera_s_stream(struct soc_camera_device *icd, int enable);
static void rk_camera_capture_process(struct work_struct *work);

// #define OPTIMIZE_MEMORY_USE

/*
 *  Videobuf operations
 */
static int rk_videobuf_setup(struct videobuf_queue *vq, unsigned int *count,
                               unsigned int *size)
{
    struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
	unsigned int i;
    struct rk_camera_work *wk;

	 struct soc_mbus_pixelfmt fmt;
	int bytes_per_line;
	int bytes_per_line_host;
	fmt.packing = SOC_MBUS_PACKING_1_5X8;

		bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
	if(icd->current_fmt->host_fmt->fourcc ==  V4L2_PIX_FMT_RGB565)
		 bytes_per_line_host = soc_mbus_bytes_per_line(pcdev->host_width,
						&fmt);
	else if(icd->current_fmt->host_fmt->fourcc ==  V4L2_PIX_FMT_RGB24)
		bytes_per_line_host = pcdev->host_width*3;
	else
		bytes_per_line_host = soc_mbus_bytes_per_line(pcdev->host_width,
					   icd->current_fmt->host_fmt);

	if (bytes_per_line_host < 0)
		return bytes_per_line_host;

	/* planar capture requires Y, U and V buffers to be page aligned */
	*size = PAGE_ALIGN(bytes_per_line*icd->user_height);	   /* Y pages UV pages, yuv422*/
	pcdev->vipmem_bsize = PAGE_ALIGN(bytes_per_line_host * pcdev->host_height);

	if (CAM_WORKQUEUE_IS_EN()) {
        #ifdef CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_OFF
        if (CAM_IPPWORK_IS_EN()) 
        #endif
        {
            BUG_ON(pcdev->vipmem_size<pcdev->vipmem_bsize);
        #ifndef OPTIMIZE_MEMORY_USE
    		if (*count > pcdev->vipmem_size/pcdev->vipmem_bsize) {    /* Buffers must be limited, when this resolution is genered by IPP */
    			*count = pcdev->vipmem_size/pcdev->vipmem_bsize;
    		}
		#endif
        }
		if ((pcdev->camera_work_count != *count) && pcdev->camera_work) {
			kfree(pcdev->camera_work);
			pcdev->camera_work = NULL;
			pcdev->camera_work_count = 0;
		}

		if (pcdev->camera_work == NULL) {
			pcdev->camera_work = wk = kzalloc(sizeof(struct rk_camera_work)*(*count), GFP_KERNEL);
			if (pcdev->camera_work == NULL) {
				RK30_CAM_DEBUG_TRACE("\n %s kmalloc fail\n", __FUNCTION__);
				BUG();
			}
            INIT_LIST_HEAD(&pcdev->camera_work_queue);

            for (i=0; i<(*count); i++) {
                wk->index = i;                
                list_add_tail(&wk->queue, &pcdev->camera_work_queue);
                wk++; 
            }
			pcdev->camera_work_count = (*count);
		}
#if CAMERA_VIDEOBUF_ARM_ACCESS
        if (pcdev->vbinfo && (pcdev->vbinfo_count != *count)) {
            kfree(pcdev->vbinfo);
            pcdev->vbinfo = NULL;
            pcdev->vbinfo_count = 0x00;
        }

        if (pcdev->vbinfo == NULL) {
            pcdev->vbinfo = kzalloc(sizeof(struct rk29_camera_vbinfo)*(*count), GFP_KERNEL);
            if (pcdev->vbinfo == NULL) {
				RK30_CAM_DEBUG_TRACE("\n %s vbinfo kmalloc fail\n", __FUNCTION__);
				BUG();
			}
            memset(pcdev->vbinfo,0,sizeof(struct rk29_camera_vbinfo)*(*count));
			pcdev->vbinfo_count = *count;
        }
#endif        
	}
	
	pcdev->vbinfo_count = *count;
    pcdev->video_vq = vq;
    RK30_CAM_DEBUG_TRACE("%s..%d.. videobuf size:%d, vipmem_buf size:%d, count:%d \n",__FUNCTION__,__LINE__, *size,pcdev->vipmem_size, *count);

    return 0;
}
static void rk_videobuf_free(struct videobuf_queue *vq, struct rk_camera_buffer *buf)
{
    struct soc_camera_device *icd = vq->priv_data;

    dev_dbg(&icd->dev, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
            &buf->vb, buf->vb.baddr, buf->vb.bsize);

	/* ddl@rock-chips.com: buf_release called soc_camera_streamoff and soc_camera_close*/
	if (buf->vb.state == VIDEOBUF_NEEDS_INIT)
		return;

    if (in_interrupt())
        BUG();
    /*
	 * This waits until this buffer is out of danger, i.e., until it is no
	 * longer in STATE_QUEUED or STATE_ACTIVE
	 */
	//videobuf_waiton(vq, &buf->vb, 0, 0);
    videobuf_dma_contig_free(vq, &buf->vb);
    dev_dbg(&icd->dev, "%s freed\n", __func__);
    buf->vb.state = VIDEOBUF_NEEDS_INIT;
	return;
}
static int rk_videobuf_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb, enum v4l2_field field)
{
    struct soc_camera_device *icd = vq->priv_data;
    struct rk_camera_buffer *buf;
    int ret;
    int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
	if ((bytes_per_line < 0) || (vb->boff == 0))
		return -EINVAL;

    buf = container_of(vb, struct rk_camera_buffer, vb);

    dev_dbg(&icd->dev, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
            vb, vb->baddr, vb->bsize);
    
    /* Added list head initialization on alloc */
    WARN_ON(!list_empty(&vb->queue));    

    BUG_ON(NULL == icd->current_fmt);

    if (buf->code    != icd->current_fmt->code ||
        vb->width   != icd->user_width ||
        vb->height  != icd->user_height ||
        vb->field   != field) {
        buf->code    = icd->current_fmt->code;
        vb->width   = icd->user_width;
        vb->height  = icd->user_height;
        vb->field   = field;
        vb->state   = VIDEOBUF_NEEDS_INIT;
    }

    vb->size = bytes_per_line*vb->height;          /* ddl@rock-chips.com : fmt->depth is coorect */
    if (0 != vb->baddr && vb->bsize < vb->size) {
        ret = -EINVAL;
        goto out;
    }

    if (vb->state == VIDEOBUF_NEEDS_INIT) {
        ret = videobuf_iolock(vq, vb, NULL);
        if (ret) {
            goto fail;
        }
        vb->state = VIDEOBUF_PREPARED;
    }
    
    return 0;
fail:
    rk_videobuf_free(vq, buf);
out:
    return ret;
}

#if 0
static void rk_camera_store_register(struct rk_camera_dev *pcdev)
{
#if defined(CONFIG_ARCH_RK3188)
	pcdev->reginfo_suspend.cifCtrl = read_cif_reg(pcdev->base,CIF_CIF_CTRL);
	pcdev->reginfo_suspend.cifCrop = read_cif_reg(pcdev->base,CIF_CIF_CROP);
	pcdev->reginfo_suspend.cifFs = read_cif_reg(pcdev->base,CIF_CIF_SET_SIZE);
	pcdev->reginfo_suspend.cifIntEn = read_cif_reg(pcdev->base,CIF_CIF_INTEN);
	pcdev->reginfo_suspend.cifFmt= read_cif_reg(pcdev->base,CIF_CIF_FOR);
	pcdev->reginfo_suspend.cifVirWidth = read_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH);
	pcdev->reginfo_suspend.cifScale= read_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL);
	
	cru_set_soft_reset(SOFT_RST_CIF0, true);
	udelay(3);
	cru_set_soft_reset(SOFT_RST_CIF0, false);
	
#endif
}
static void rk_camera_restore_register(struct rk_camera_dev *pcdev)
{
#if defined(CONFIG_ARCH_RK3188)
	write_cif_reg(pcdev->base,CIF_CIF_CTRL, pcdev->reginfo_suspend.cifCtrl&~ENABLE_CAPTURE);
	write_cif_reg(pcdev->base,CIF_CIF_INTEN, pcdev->reginfo_suspend.cifIntEn);
	write_cif_reg(pcdev->base,CIF_CIF_CROP, pcdev->reginfo_suspend.cifCrop);
	write_cif_reg(pcdev->base,CIF_CIF_SET_SIZE, pcdev->reginfo_suspend.cifFs);
	write_cif_reg(pcdev->base,CIF_CIF_FOR, pcdev->reginfo_suspend.cifFmt);
	write_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH,pcdev->reginfo_suspend.cifVirWidth);
	write_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL, pcdev->reginfo_suspend.cifScale);
#endif
}
#endif

static inline void rk_videobuf_capture(struct videobuf_buffer *vb,struct rk_camera_dev *rk_pcdev, int fmt_ready)
{
	unsigned int y_addr,uv_addr;
	struct rk_camera_dev *pcdev = rk_pcdev;

    if (vb) {
		if (CAM_WORKQUEUE_IS_EN()) {
		    #ifdef OPTIMIZE_MEMORY_USE
			y_addr = vb->boff;
			uv_addr = y_addr + vb->width * vb->height;
		    #else
			y_addr = pcdev->vipmem_phybase + vb->i*pcdev->vipmem_bsize;
			uv_addr = y_addr + pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height;
			if (y_addr > (pcdev->vipmem_phybase + pcdev->vipmem_size - pcdev->vipmem_bsize)) {
				RK30_CAM_DEBUG_TRACE("vipmem for IPP is overflow! %dx%d -> %dx%d vb_index:%d\n",pcdev->host_width,pcdev->host_height,
					          pcdev->icd->user_width,pcdev->icd->user_height, vb->i);
				BUG();
			}
			#endif
		} else {
			y_addr = vb->boff;
			uv_addr = y_addr + vb->width * vb->height;
		}
        
		switch(fmt_ready)
		{
			case 0:
				write_cif_reg(pcdev->base,CIF_CIF_FRM0_ADDR_Y, y_addr);
        		write_cif_reg(pcdev->base,CIF_CIF_FRM0_ADDR_UV, uv_addr);
				break;
			case 1:
				write_cif_reg(pcdev->base,CIF_CIF_FRM1_ADDR_Y, y_addr);
        		write_cif_reg(pcdev->base,CIF_CIF_FRM1_ADDR_UV, uv_addr);
				break;
			default:
				RK30_CAM_DEBUG_TRACE("%s(%d): fmt_ready(%d) is wrong!\n", __FUNCTION__, __LINE__,fmt_ready);
				break;
		}
      
    }
}
/* Locking: Caller holds q->irqlock */
static void rk_videobuf_queue(struct videobuf_queue *vq,
                                struct videobuf_buffer *vb)
{
    struct soc_camera_device *icd = vq->priv_data;
    struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
#if CAMERA_VIDEOBUF_ARM_ACCESS    
    struct rk29_camera_vbinfo *vb_info;
#endif

    dev_dbg(&icd->dev, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
            vb, vb->baddr, vb->bsize);
   // spin_lock_irqsave(&pcdev->irq_lock, flags);

    vb->state = VIDEOBUF_QUEUED;
	if (list_empty(&pcdev->capture)) {
		list_add_tail(&vb->queue, &pcdev->capture);
	} else {
		if (list_entry(pcdev->capture.next, struct videobuf_buffer, queue) != vb)
			list_add_tail(&vb->queue, &pcdev->capture);
		else
			BUG();    /* ddl@rock-chips.com : The same videobuffer queue again */
	}
#if CAMERA_VIDEOBUF_ARM_ACCESS
    if (pcdev->vbinfo) {
        vb_info = pcdev->vbinfo+vb->i;
        if ((vb_info->phy_addr != vb->boff) || (vb_info->size != vb->bsize)) {
            if (vb_info->vir_addr) {
                iounmap(vb_info->vir_addr);
                release_mem_region(vb_info->phy_addr, vb_info->size);
                vb_info->vir_addr = NULL;
                vb_info->phy_addr = 0x00;
                vb_info->size = 0x00;
            }

            if (request_mem_region(vb->boff,vb->bsize,"rk_camera_vb")) {
                vb_info->vir_addr = ioremap_cached(vb->boff,vb->bsize); 
            }
            
            if (vb_info->vir_addr) {
                vb_info->size = vb->bsize;
                vb_info->phy_addr = vb->boff;
            } else {
                RK30_CAM_DEBUG_TRACE("%s..%d:ioremap videobuf %d failed\n",__FUNCTION__,__LINE__, vb->i);
            }
        }
    }
#endif
    if((((read_cif_reg(pcdev->base,CIF_CIF_CTRL)) & ENABLE_CAPTURE) == 0)){
        if (!pcdev->active0) {
            pcdev->active0 = vb;
            rk_videobuf_capture(vb,pcdev,0);
    		list_del_init(&(vb->queue));
        } else if (!pcdev->active1) {
            pcdev->active1 = vb;
            rk_videobuf_capture(vb,pcdev,1);
    		list_del_init(&(vb->queue));
        }
    }
	//spin_unlock_irqrestore(&pcdev->irq_lock, flags);
}
static int rk_pixfmt2ippfmt(unsigned int pixfmt, int *ippfmt)
{
	switch (pixfmt)
	{
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
		{
			*ippfmt = IPP_Y_CBCR_H2V1;
			break;
		}
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		{
			*ippfmt = IPP_Y_CBCR_H2V2;
			break;
		}
		default:
			goto rk_pixfmt2ippfmt_err;
	}

	return 0;
rk_pixfmt2ippfmt_err:
	return -1;
}

#if 0
static int rk_pixfmt2rgafmt(unsigned int pixfmt, int *ippfmt)
{
	switch (pixfmt)
	{
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_UYVY: // yuv 422, but sensor has defined this format(in fact ,should be defined yuv 420), treat this as yuv 420.
		case V4L2_PIX_FMT_YUYV: 
			{
				*ippfmt = RK_FORMAT_YCbCr_420_SP;
				break;
			}
		case V4L2_PIX_FMT_YVU420:
		case V4L2_PIX_FMT_VYUY:
		case V4L2_PIX_FMT_YVYU:
			{
				*ippfmt = RK_FORMAT_YCrCb_420_SP;
				break;
			}
		case V4L2_PIX_FMT_RGB565:
			{
				*ippfmt = RK_FORMAT_RGB_565;
				break;
			}
		case V4L2_PIX_FMT_RGB24:
			{
				*ippfmt = RK_FORMAT_RGB_888;
				break;
			}
		default:
			goto rk_pixfmt2rgafmt_err;
	}

	return 0;
rk_pixfmt2rgafmt_err:
	return -1;
}
#endif

#if (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_PP)
static int rk_camera_scale_crop_pp(struct work_struct *work){
	struct rk_camera_work *camera_work = container_of(work, struct rk_camera_work, work);
	struct videobuf_buffer *vb = camera_work->vb;
	struct rk_camera_dev *pcdev = camera_work->pcdev;
	int vipdata_base;
	unsigned long int flags;
	int scale_times,w,h;
	int src_y_offset;
	PP_OP_HANDLE hnd;
	PP_OPERATION init;
	int ret = 0;
	vipdata_base = pcdev->vipmem_phybase + vb->i*pcdev->vipmem_bsize;
	
	memset(&init, 0, sizeof(init));
	init.srcAddr 	= vipdata_base;
	init.srcFormat	= PP_IN_FORMAT_YUV420SEMI;
	init.srcWidth	= init.srcHStride = pcdev->zoominfo.vir_width;
	init.srcHeight	= init.srcVStride = pcdev->zoominfo.vir_height;
	
	init.dstAddr 	= vb->boff;
	init.dstFormat	= PP_OUT_FORMAT_YUV420INTERLAVE;
	init.dstWidth	= init.dstHStride = pcdev->icd->user_width;
	init.dstHeight	= init.dstVStride = pcdev->icd->user_height;

	printk("srcWidth = %d,srcHeight = %d,dstWidth = %d,dstHeight = %d\n",init.srcWidth,init.srcHeight,init.dstWidth,init.dstHeight);
	#if 0
	ret = ppOpInit(&hnd, &init);
	if (!ret) {
		ppOpPerform(hnd);
		ppOpSync(hnd);
		ppOpRelease(hnd);
	} else {
		printk("can not create ppOp handle\n");
	}
	#endif
	return ret;
}
#endif
#if (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_RGA)
extern rga_service_info rga_service;
extern int rga_blit_sync(rga_session *session, struct rga_req *req);
extern	 void rga_service_session_clear(rga_session *session);
static int rk_camera_scale_crop_rga(struct work_struct *work){
	struct rk_camera_work *camera_work = container_of(work, struct rk_camera_work, work);
	struct videobuf_buffer *vb = camera_work->vb;
	struct rk_camera_dev *pcdev = camera_work->pcdev;
	int vipdata_base;
	unsigned long int flags;
	int scale_times,w,h;
	int src_y_offset;
	struct rga_req req;
	rga_session session;
	int rga_times = 3;
	const struct soc_mbus_pixelfmt *fmt;
	int ret = 0;
	fmt = soc_mbus_get_fmtdesc(pcdev->icd->current_fmt->code);
	vipdata_base = pcdev->vipmem_phybase + vb->i*pcdev->vipmem_bsize;
	if((pcdev->icd->current_fmt->host_fmt->fourcc != V4L2_PIX_FMT_RGB565)
		&& (pcdev->icd->current_fmt->host_fmt->fourcc != V4L2_PIX_FMT_RGB24)){
		RK30_CAM_DEBUG_TRACE("RGA not support this format !\n");
		goto do_ipp_err;
		}
	if ((pcdev->icd->user_width > 0x800) || (pcdev->icd->user_height > 0x800)) {
		scale_times = MAX((pcdev->icd->user_width/0x800),(pcdev->icd->user_height/0x800));		  
		scale_times++;
	} else {
		scale_times = 1;
	}
	session.pid = current->pid;
	INIT_LIST_HEAD(&session.waiting);
	INIT_LIST_HEAD(&session.running);
	INIT_LIST_HEAD(&session.list_session);
	init_waitqueue_head(&session.wait);
	/* no need to protect */
	list_add_tail(&session.list_session, &rga_service.session);
	atomic_set(&session.task_running, 0);
	atomic_set(&session.num_done, 0);
	
	memset(&req,0,sizeof(struct rga_req));
	req.src.act_w = pcdev->zoominfo.a.c.width/scale_times;
	req.src.act_h = pcdev->zoominfo.a.c.height/scale_times;

	req.src.vir_w = pcdev->zoominfo.vir_width;
	req.src.vir_h =pcdev->zoominfo.vir_height;
	req.src.yrgb_addr = vipdata_base;
	req.src.uv_addr =vipdata_base + pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height;;
	req.src.v_addr = req.src.uv_addr ;
	req.src.format =fmt->fourcc;
	rk_pixfmt2rgafmt(fmt->fourcc,&req.src.format);
	req.src.x_offset = pcdev->zoominfo.a.c.left;
	req.src.y_offset = pcdev->zoominfo.a.c.top;

	req.dst.act_w = pcdev->icd->user_width/scale_times;
	req.dst.act_h = pcdev->icd->user_height/scale_times;

	req.dst.vir_w = pcdev->icd->user_width;
	req.dst.vir_h = pcdev->icd->user_height;
	req.dst.x_offset = 0;
	req.dst.y_offset = 0;
	req.dst.yrgb_addr = vb->boff;
	rk_pixfmt2rgafmt(pcdev->icd->current_fmt->host_fmt->fourcc,&req.dst.format);
	req.clip.xmin = 0;
	req.clip.xmax = req.dst.vir_w-1;
	req.clip.ymin = 0;
	req.clip.ymax = req.dst.vir_h -1;

	req.rotate_mode = 1;
	req.scale_mode = 2;

	req.sina = 0;
	req.cosa = 65536;
	req.mmu_info.mmu_en = 0;

	for (h=0; h<scale_times; h++) {
		for (w=0; w<scale_times; w++) {
			rga_times = 3;
	
			req.src.yrgb_addr = vipdata_base;
			req.src.uv_addr =vipdata_base + pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height;;
			req.src.x_offset = pcdev->zoominfo.a.c.left+w*pcdev->zoominfo.a.c.width/scale_times;
			req.src.y_offset = pcdev->zoominfo.a.c.top+h*pcdev->zoominfo.a.c.height/scale_times;
			req.dst.x_offset =  pcdev->icd->user_width*w/scale_times;
			req.dst.y_offset = pcdev->icd->user_height*h/scale_times;
			req.dst.yrgb_addr = vb->boff ;
		//	RK30_CAM_DEBUG_TRACE("src.act_w = %d , src.act_h  = %d! vir_w = %d , vir_h = %d,off_x = %d,off_y = %d\n",req.src.act_w,req.src.act_h ,req.src.vir_w,req.src.vir_h,req.src.x_offset,req.src.y_offset);
		//	RK30_CAM_DEBUG_TRACE("dst.act_w = %d , dst.act_h  = %d! vir_w = %d , vir_h = %d,off_x = %d,off_y = %d\n",req.dst.act_w,req.dst.act_h ,req.dst.vir_w,req.dst.vir_h,req.dst.x_offset,req.dst.y_offset);
		//	RK30_CAM_DEBUG_TRACE("req.src.yrgb_addr = 0x%x,req.dst.yrgb_addr = 0x%x\n",req.src.yrgb_addr,req.dst.yrgb_addr);

			while(rga_times-- > 0) {
				if (rga_blit_sync(&session, &req)){
					RK30_CAM_DEBUG_TRACE("rga do erro,do again,rga_times = %d!\n",rga_times);
				 } else {
					break;
				 }
			}
		
			if (rga_times <= 0) {
				spin_lock_irqsave(&pcdev->lock, flags);
				vb->state = VIDEOBUF_NEEDS_INIT;
				spin_unlock_irqrestore(&pcdev->lock, flags);
				mutex_lock(&rga_service.lock);
				list_del(&session.list_session);
				rga_service_session_clear(&session);
				mutex_unlock(&rga_service.lock);
				goto session_done;
			}
			}
	}
	session_done:
	mutex_lock(&rga_service.lock);
	list_del(&session.list_session);
	rga_service_session_clear(&session);
	mutex_unlock(&rga_service.lock);

	do_ipp_err:

		return ret;
	
}

#endif
#if ((defined(CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_ON)) && (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_IPP))

static int rk_camera_scale_crop_ipp(struct work_struct *work)
{
	struct rk_camera_work *camera_work = container_of(work, struct rk_camera_work, work);
	struct videobuf_buffer *vb = camera_work->vb;
	struct rk_camera_dev *pcdev = camera_work->pcdev;
	int vipdata_base;
	unsigned long int flags;

	struct rk29_ipp_req ipp_req;
	int src_y_offset,src_uv_offset,dst_y_offset,dst_uv_offset,src_y_size,dst_y_size;
	int scale_times,w,h;
	int ret = 0;
    /*
    *ddl@rock-chips.com: 
    * IPP Dest image resolution is 2047x1088, so scale operation break up some times
    */
    if ((pcdev->icd->user_width > 0x7f0) || (pcdev->icd->user_height > 0x430)) {
        scale_times = MAX((pcdev->icd->user_width/0x7f0),(pcdev->icd->user_height/0x430));        
        scale_times++;
    } else {
        scale_times = 1;
    }
    memset(&ipp_req, 0, sizeof(struct rk29_ipp_req));
#ifdef OPTIMIZE_MEMORY_USE
    //need copy to ipp buffer?
    if((pcdev->zoominfo.a.c.width != pcdev->zoominfo.vir_width)
        ||(pcdev->zoominfo.a.c.height != pcdev->zoominfo.vir_height)){
            if((pcdev->zoominfo.vir_width != pcdev->icd->user_width) || (pcdev->zoominfo.vir_height != pcdev->icd->user_height)){
                printk("OPTIMIZE_MEMORY_USE erro: src size not equal to dst size\n");
                goto do_ipp_err;
            }
            ipp_req.timeout = 3000;
            ipp_req.flag = IPP_ROT_0; 
            ipp_req.store_clip_mode =1;
            ipp_req.src0.w = pcdev->zoominfo.vir_width/scale_times;
            ipp_req.src0.h = pcdev->zoominfo.vir_height/scale_times;
            ipp_req.dst0.w = pcdev->icd->user_width/scale_times;
            ipp_req.dst0.h = pcdev->icd->user_height/scale_times;
            ipp_req.src_vir_w =  pcdev->zoominfo.vir_width; 
            ipp_req.dst_vir_w = pcdev->icd->user_width; 
            rk_pixfmt2ippfmt(pcdev->pixfmt, &ipp_req.src0.fmt);
            rk_pixfmt2ippfmt(pcdev->pixfmt, &ipp_req.dst0.fmt);
            vipdata_base = pcdev->vipmem_phybase;
            src_y_size = pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height;  //vipmem
            dst_y_size = pcdev->icd->user_width*pcdev->icd->user_height;
            for (h=0; h<scale_times; h++) {
                for (w=0; w<scale_times; w++) {
                    src_y_offset = (h*pcdev->zoominfo.vir_height/scale_times)* pcdev->zoominfo.vir_width 
                                +  w*pcdev->zoominfo.vir_width/scale_times;
        		    src_uv_offset = (h*pcdev->zoominfo.vir_height/scale_times)* pcdev->zoominfo.vir_width/2
                                + w*pcdev->zoominfo.vir_width/scale_times;

                    dst_y_offset = pcdev->icd->user_width*pcdev->icd->user_height*h/scale_times + pcdev->icd->user_width*w/scale_times;
                    dst_uv_offset = pcdev->icd->user_width*pcdev->icd->user_height*h/scale_times/2 + pcdev->icd->user_width*w/scale_times;
            		ipp_req.src0.YrgbMst = vb->boff + src_y_offset;
            		ipp_req.src0.CbrMst = vb->boff + src_y_size + src_uv_offset;
            		ipp_req.dst0.YrgbMst = vipdata_base + dst_y_offset;
            		ipp_req.dst0.CbrMst = vipdata_base + dst_y_size + dst_uv_offset;
                    if (ipp_blit_sync(&ipp_req)){
                        RK30_CAM_DEBUG_TRACE("ipp do erro\n");
                     }
                 }
             }
            memset(&ipp_req, 0, sizeof(struct rk29_ipp_req));
        }
#endif

    ipp_req.timeout = 3000;
    ipp_req.flag = IPP_ROT_0; 
    ipp_req.store_clip_mode =1;
    ipp_req.src0.w = pcdev->zoominfo.a.c.width/scale_times;
    ipp_req.src0.h = pcdev->zoominfo.a.c.height/scale_times;
    ipp_req.src_vir_w = pcdev->zoominfo.vir_width; 
    rk_pixfmt2ippfmt(pcdev->pixfmt, &ipp_req.src0.fmt);
    ipp_req.dst0.w = pcdev->icd->user_width/scale_times;
    ipp_req.dst0.h = pcdev->icd->user_height/scale_times;
    ipp_req.dst_vir_w = pcdev->icd->user_width;        
    rk_pixfmt2ippfmt(pcdev->pixfmt, &ipp_req.dst0.fmt);
#ifdef OPTIMIZE_MEMORY_USE
    if((pcdev->zoominfo.a.c.width != pcdev->zoominfo.vir_width)
        ||(pcdev->zoominfo.a.c.height != pcdev->zoominfo.vir_height)){
        vipdata_base = pcdev->vipmem_phybase;
    }else
        vipdata_base = vb->boff;

#else
    vipdata_base = pcdev->vipmem_phybase + vb->i*pcdev->vipmem_bsize;
#endif
    src_y_size = pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height;  //vipmem
    dst_y_size = pcdev->icd->user_width*pcdev->icd->user_height;
    for (h=0; h<scale_times; h++) {
        for (w=0; w<scale_times; w++) {
            int ipp_times = 3;
            src_y_offset = (pcdev->zoominfo.a.c.top + h*pcdev->zoominfo.a.c.height/scale_times)* pcdev->zoominfo.vir_width 
                        + pcdev->zoominfo.a.c.left + w*pcdev->zoominfo.a.c.width/scale_times;
		    src_uv_offset = (pcdev->zoominfo.a.c.top + h*pcdev->zoominfo.a.c.height/scale_times)* pcdev->zoominfo.vir_width/2
                        + pcdev->zoominfo.a.c.left + w*pcdev->zoominfo.a.c.width/scale_times;

            dst_y_offset = pcdev->icd->user_width*pcdev->icd->user_height*h/scale_times + pcdev->icd->user_width*w/scale_times;
            dst_uv_offset = pcdev->icd->user_width*pcdev->icd->user_height*h/scale_times/2 + pcdev->icd->user_width*w/scale_times;

    		ipp_req.src0.YrgbMst = vipdata_base + src_y_offset;
    		ipp_req.src0.CbrMst = vipdata_base + src_y_size + src_uv_offset;
    		ipp_req.dst0.YrgbMst = vb->boff + dst_y_offset;
    		ipp_req.dst0.CbrMst = vb->boff + dst_y_size + dst_uv_offset;
    		while(ipp_times-- > 0) {
                if (ipp_blit_sync(&ipp_req)){
                    RK30_CAM_DEBUG_TRACE("ipp do erro,do again,ipp_times = %d!\n",ipp_times);
                 } else {
                    break;
                 }
            }
            
            if (ipp_times <= 0) {
    			spin_lock_irqsave(&pcdev->lock, flags);
    			vb->state = VIDEOBUF_NEEDS_INIT;
    			spin_unlock_irqrestore(&pcdev->lock, flags);
    			RK30_CAM_DEBUG_TRACE("Capture image(vb->i:0x%x) which IPP operated is error:\n",vb->i);
    			RK30_CAM_DEBUG_TRACE("widx:%d hidx:%d ",w,h);
    			RK30_CAM_DEBUG_TRACE("%dx%d@(%d,%d)->%dx%d\n",pcdev->zoominfo.a.c.width,pcdev->zoominfo.a.c.height,pcdev->zoominfo.a.c.left,pcdev->zoominfo.a.c.top,pcdev->icd->user_width,pcdev->icd->user_height);
    			RK30_CAM_DEBUG_TRACE("ipp_req.src0.YrgbMst:0x%x ipp_req.src0.CbrMst:0x%x \n", ipp_req.src0.YrgbMst,ipp_req.src0.CbrMst);
    			RK30_CAM_DEBUG_TRACE("ipp_req.src0.w:0x%x ipp_req.src0.h:0x%x \n",ipp_req.src0.w,ipp_req.src0.h);
    			RK30_CAM_DEBUG_TRACE("ipp_req.src0.fmt:0x%x\n",ipp_req.src0.fmt);
    			RK30_CAM_DEBUG_TRACE("ipp_req.dst0.YrgbMst:0x%x ipp_req.dst0.CbrMst:0x%x \n",ipp_req.dst0.YrgbMst,ipp_req.dst0.CbrMst);
    			RK30_CAM_DEBUG_TRACE("ipp_req.dst0.w:0x%x ipp_req.dst0.h:0x%x \n",ipp_req.dst0.w ,ipp_req.dst0.h);
    			RK30_CAM_DEBUG_TRACE("ipp_req.dst0.fmt:0x%x\n",ipp_req.dst0.fmt);
    			RK30_CAM_DEBUG_TRACE("ipp_req.src_vir_w:0x%x ipp_req.dst_vir_w :0x%x\n",ipp_req.src_vir_w ,ipp_req.dst_vir_w);
    			RK30_CAM_DEBUG_TRACE("ipp_req.timeout:0x%x ipp_req.flag :0x%x\n",ipp_req.timeout,ipp_req.flag);
                
    			goto do_ipp_err;
    		}
        }
    }

do_ipp_err:
	return ret;    
}
#endif
#if (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_ARM)
static int rk_camera_scale_crop_arm(struct work_struct *work)
{
    struct rk_camera_work *camera_work = container_of(work, struct rk_camera_work, work);	
    struct videobuf_buffer *vb = camera_work->vb;	
    struct rk_camera_dev *pcdev = camera_work->pcdev;	
    struct rk29_camera_vbinfo *vb_info;        
    unsigned char *psY,*pdY,*psUV,*pdUV; 
    unsigned char *src,*dst;
    unsigned long src_phy,dst_phy;
    int srcW,srcH,cropW,cropH,dstW,dstH;
    long zoomindstxIntInv,zoomindstyIntInv;
    long x,y;
    long yCoeff00,yCoeff01,xCoeff00,xCoeff01;
    long sX,sY;
    long r0,r1,a,b,c,d;
    int ret = 0;

    src_phy = pcdev->vipmem_phybase + vb->i*pcdev->vipmem_bsize;    
    src = psY = (unsigned char*)(pcdev->vipmem_virbase + vb->i*pcdev->vipmem_bsize);
    psUV = psY + pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height;
	
    srcW = pcdev->zoominfo.vir_width;
    srcH = pcdev->zoominfo.vir_height;
    cropW = pcdev->zoominfo.a.c.width;
    cropH = pcdev->zoominfo.a.c.height;
	
    psY = psY + pcdev->zoominfo.a.c.top*pcdev->zoominfo.vir_width+pcdev->zoominfo.a.c.left;
    psUV = psUV + pcdev->zoominfo.a.c.top*pcdev->zoominfo.vir_width/2+pcdev->zoominfo.a.c.left; 
    
    vb_info = pcdev->vbinfo+vb->i; 
    dst_phy = vb_info->phy_addr;
    dst = pdY = (unsigned char*)vb_info->vir_addr; 
    pdUV = pdY + pcdev->icd->user_width*pcdev->icd->user_height;
    dstW = pcdev->icd->user_width;
    dstH = pcdev->icd->user_height;

    zoomindstxIntInv = ((unsigned long)(cropW)<<16)/dstW + 1;
    zoomindstyIntInv = ((unsigned long)(cropH)<<16)/dstH + 1;
    //y
    //for(y = 0; y<dstH - 1 ; y++ ) {   
    for(y = 0; y<dstH; y++ ) {   
        yCoeff00 = (y*zoomindstyIntInv)&0xffff;
        yCoeff01 = 0xffff - yCoeff00; 
        sY = (y*zoomindstyIntInv >> 16);
        sY = (sY >= srcH - 1)? (srcH - 2) : sY;      
        for(x = 0; x<dstW; x++ ) {
            xCoeff00 = (x*zoomindstxIntInv)&0xffff;
            xCoeff01 = 0xffff - xCoeff00; 	
            sX = (x*zoomindstxIntInv >> 16);
            sX = (sX >= srcW -1)?(srcW- 2) : sX;
            a = psY[sY*srcW + sX];
            b = psY[sY*srcW + sX + 1];
            c = psY[(sY+1)*srcW + sX];
            d = psY[(sY+1)*srcW + sX + 1];

            r0 = (a * xCoeff01 + b * xCoeff00)>>16 ;
            r1 = (c * xCoeff01 + d * xCoeff00)>>16 ;
            r0 = (r0 * yCoeff01 + r1 * yCoeff00)>>16;

            pdY[x] = r0;
        }
        pdY += dstW;
    }

    dstW /= 2;
    dstH /= 2;
    srcW /= 2;
    srcH /= 2;

    //UV
    //for(y = 0; y<dstH - 1 ; y++ ) {
    for(y = 0; y<dstH; y++ ) {
        yCoeff00 = (y*zoomindstyIntInv)&0xffff;
        yCoeff01 = 0xffff - yCoeff00; 
        sY = (y*zoomindstyIntInv >> 16);
        sY = (sY >= srcH -1)? (srcH - 2) : sY;      
        for(x = 0; x<dstW; x++ ) {
            xCoeff00 = (x*zoomindstxIntInv)&0xffff;
            xCoeff01 = 0xffff - xCoeff00; 	
            sX = (x*zoomindstxIntInv >> 16);
            sX = (sX >= srcW -1)?(srcW- 2) : sX;
            //U
            a = psUV[(sY*srcW + sX)*2];
            b = psUV[(sY*srcW + sX + 1)*2];
            c = psUV[((sY+1)*srcW + sX)*2];
            d = psUV[((sY+1)*srcW + sX + 1)*2];

            r0 = (a * xCoeff01 + b * xCoeff00)>>16 ;
            r1 = (c * xCoeff01 + d * xCoeff00)>>16 ;
            r0 = (r0 * yCoeff01 + r1 * yCoeff00)>>16;

            pdUV[x*2] = r0;

            //V
            a = psUV[(sY*srcW + sX)*2 + 1];
            b = psUV[(sY*srcW + sX + 1)*2 + 1];
            c = psUV[((sY+1)*srcW + sX)*2 + 1];
            d = psUV[((sY+1)*srcW + sX + 1)*2 + 1];

            r0 = (a * xCoeff01 + b * xCoeff00)>>16 ;
            r1 = (c * xCoeff01 + d * xCoeff00)>>16 ;
            r0 = (r0 * yCoeff01 + r1 * yCoeff00)>>16;

            pdUV[x*2 + 1] = r0;
        }
        pdUV += dstW*2;
    }

rk_camera_scale_crop_arm_end:

    dmac_flush_range((void*)src,(void*)(src+pcdev->vipmem_bsize));
    outer_flush_range((phys_addr_t)src_phy,(phys_addr_t)(src_phy+pcdev->vipmem_bsize));
    
    dmac_flush_range((void*)dst,(void*)(dst+vb_info->size));
    outer_flush_range((phys_addr_t)dst_phy,(phys_addr_t)(dst_phy+vb_info->size));

	return ret;    
}
#endif

static void rk_camera_capture_process(struct work_struct *work)
{
    struct rk_camera_work *camera_work = container_of(work, struct rk_camera_work, work);    
    struct videobuf_buffer *vb = camera_work->vb;    
    struct rk_camera_dev *pcdev = camera_work->pcdev;    
    //enum v4l2_mbus_pixelcode icd_code = pcdev->icd->current_fmt->code;    
    struct device *control = to_soc_camera_control(pcdev->icd);
    struct v4l2_subdev *sd=dev_get_drvdata(control);
    unsigned long flags = 0;    
    int err = 0,i;    

    if (!CAM_WORKQUEUE_IS_EN())        
        goto rk_camera_capture_process_end; 

    camera_work->vb->rk_code = 0x00;
    if (pcdev->hdr_info.en) {
		printk("rk_camera_capture_process hdr %d fps\n",camera_work->ts);
        if (pcdev->hdr_info.frame[0].set_ts == 0) {
            v4l2_subdev_call(sd, core, ioctl, RK29_CAM_SUBDEV_HDR_EXPOSURE,(void*)pcdev->hdr_info.frame[0].code);
            pcdev->hdr_info.frame[0].set_ts = pcdev->fps;
            printk("set hdr %d @ %d fps\n",0,pcdev->fps);
        } else {
            if ((camera_work->ts - pcdev->hdr_info.frame[0].set_ts) > 1) {
                for (i=0; i<3; i++) {
                    if (pcdev->hdr_info.frame[i].get_ts == 0) {
						printk("get hdr %d @ %d fps %dx%d\n",i,camera_work->ts,camera_work->vb->width,camera_work->vb->height);
                        pcdev->hdr_info.frame[i].get_ts = camera_work->ts;
                        RK_VIDEOBUF_CODE_SET(camera_work->vb->rk_code,pcdev->hdr_info.frame[i].code);
                        break;
                    }
                }

                if (i==2) {
                    v4l2_subdev_call(sd, core, ioctl, RK29_CAM_SUBDEV_HDR_EXPOSURE,(void*)RK_VIDEOBUF_HDR_EXPOSURE_FINISH);
                    pcdev->hdr_info.en = false;
                    printk("hdr off\n");
                }
            }
        }
    }
    
    
    down(&pcdev->zoominfo.sem);
    if (pcdev->icd_cb.scale_crop_cb){
        err = (pcdev->icd_cb.scale_crop_cb)(work);
    	}
    up(&pcdev->zoominfo.sem); 
    
    if (pcdev->icd_cb.sensor_cb)        
        (pcdev->icd_cb.sensor_cb)(vb);    

rk_camera_capture_process_end:    
    if (err) {        
        vb->state = VIDEOBUF_ERROR;    
    } else {
        if ((vb->state == VIDEOBUF_QUEUED) || (vb->state == VIDEOBUF_ACTIVE)) {
	        vb->state = VIDEOBUF_DONE;
	        vb->field_count++;
		}
    }       
    spin_lock_irqsave(&pcdev->camera_work_lock, flags);    
    list_add_tail(&camera_work->queue, &pcdev->camera_work_queue);
    spin_unlock_irqrestore(&pcdev->camera_work_lock, flags); 
    wake_up(&(camera_work->vb->done));  
    return;
}
#if defined(CONFIG_ARCH_RK3188)
static void rk_camera_store_resore_register(struct rk_camera_dev *pcdev)
{

	pcdev->reginfo_suspend.cifCtrl = read_cif_reg(pcdev->base,CIF_CIF_CTRL);
	pcdev->reginfo_suspend.cifCrop = read_cif_reg(pcdev->base,CIF_CIF_CROP);
	pcdev->reginfo_suspend.cifFs = read_cif_reg(pcdev->base,CIF_CIF_SET_SIZE);
	pcdev->reginfo_suspend.cifIntEn = read_cif_reg(pcdev->base,CIF_CIF_INTEN);
	pcdev->reginfo_suspend.cifFmt= read_cif_reg(pcdev->base,CIF_CIF_FOR);
	pcdev->reginfo_suspend.cifVirWidth = read_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH);
	pcdev->reginfo_suspend.cifScale= read_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL);
	
	cru_set_soft_reset(SOFT_RST_CIF0, true);
	udelay(5);
	cru_set_soft_reset(SOFT_RST_CIF0, false);
	
    if (pcdev->reginfo_suspend.cifCtrl&ENABLE_CAPTURE)
        write_cif_reg(pcdev->base,CIF_CIF_CTRL, pcdev->reginfo_suspend.cifCtrl&~ENABLE_CAPTURE);
	write_cif_reg(pcdev->base,CIF_CIF_INTEN, pcdev->reginfo_suspend.cifIntEn);
	write_cif_reg(pcdev->base,CIF_CIF_CROP, pcdev->reginfo_suspend.cifCrop);
	write_cif_reg(pcdev->base,CIF_CIF_SET_SIZE, pcdev->reginfo_suspend.cifFs);
	write_cif_reg(pcdev->base,CIF_CIF_FOR, pcdev->reginfo_suspend.cifFmt);
	write_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH,pcdev->reginfo_suspend.cifVirWidth);
	write_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL, pcdev->reginfo_suspend.cifScale);
	
}
#endif

static irqreturn_t rk_camera_irq(int irq, void *data)
{
    struct rk_camera_dev *pcdev = data;
    struct videobuf_buffer *vb;
	struct rk_camera_work *wk;
	struct timeval tv;
    unsigned long tmp_intstat;
    unsigned long tmp_cifctrl; 
	unsigned long tmp_cif_frmst; 
	struct videobuf_buffer **active = 0;
	int flag = 0;
	 unsigned int invalid_y_addr ,invalid_uv_addr;
#ifdef OPTIMIZE_MEMORY_USE
	invalid_y_addr = 0/*pcdev->vipmem_phybase + pcdev->vipmem_bsize*/;
	invalid_uv_addr = 0/*invalid_y_addr + pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height*/;
#else
	invalid_y_addr = pcdev->vipmem_phybase + pcdev->vbinfo_count *pcdev->vipmem_bsize;
	invalid_uv_addr = invalid_y_addr + pcdev->zoominfo.vir_width*pcdev->zoominfo.vir_height;
#endif 
    tmp_intstat = read_cif_reg(pcdev->base,CIF_CIF_INTSTAT);
    tmp_cifctrl = read_cif_reg(pcdev->base,CIF_CIF_CTRL);
	tmp_cif_frmst = read_cif_reg(pcdev->base,CIF_CIF_FRAME_STATUS);

#if (CONFIG_CIF_STOP_SYNC == 0)
    if(pcdev->stop_cif == true) {
		//RK30_CAM_DEBUG_TRACE("%s(%d): cif has stopped by app,needn't to deal this irq\n",__FUNCTION__,__LINE__);
		write_cif_reg(pcdev->base,CIF_CIF_INTSTAT,0xFFFFFFFF);  /* clear vip interrupte single  */
		return IRQ_HANDLED;
	}
#endif

    if ((tmp_intstat & 0x0200) /*&& ((tmp_intstat & 0x1)==0)*/) {//bit9 =1 ,bit0 = 0
    	write_cif_reg(pcdev->base,CIF_CIF_INTSTAT,0x0200);  /* clear vip interrupte single  */
 //       if(tmp_cifctrl & ENABLE_CAPTURE)
 //           write_cif_reg(pcdev->base,CIF_CIF_CTRL, (tmp_cifctrl & ~ENABLE_CAPTURE));
         return IRQ_HANDLED;
    }
    
    /* ddl@rock-chps.com : Current VIP is run in One Frame Mode, Frame 1 is validate */
    if (tmp_cif_frmst & (CIF_F0_READY | CIF_F1_READY)) {
    	write_cif_reg(pcdev->base,CIF_CIF_INTSTAT,0x01);  /* clear vip interrupte single  */
#if CONFIG_CIF_STOP_SYNC 
			if(pcdev->stop_cif == true) {
				//RK30_CAM_DEBUG_TRACE("%s(%d): cif has stopped by app,needn't to deal this irq\n",__FUNCTION__,__LINE__);
				write_cif_reg(pcdev->base,CIF_CIF_INTSTAT,0xFFFFFFFF);	/* clear vip interrupte single	*/
				
#if 1
				//		write_cif_reg(pcdev->base,CIF_CIF_INTEN, 0);	 //capture complete interrupt enable
						{
#if (defined(CONFIG_ARCH_RK30) || defined(CONFIG_ARCH_RK2928))
					mdelay(100);
					if(IS_CIF0()){
						cru_set_soft_reset(SOFT_RST_CIF0, true);
						udelay(5);
						cru_set_soft_reset(SOFT_RST_CIF0, false);
			
					}else{
						cru_set_soft_reset(SOFT_RST_CIF1, true);
						udelay(5);
						cru_set_soft_reset(SOFT_RST_CIF1, false);
					}
#elif defined(CONFIG_ARCH_RK3188)
					cru_set_soft_reset(SOFT_RST_CIF0, true);
					udelay(5);
					cru_set_soft_reset(SOFT_RST_CIF0, false);
#endif
				}
#endif
			    spin_lock(&pcdev->lock);
				pcdev->cif_stopped = true;
				wake_up(&pcdev->cif_stop_done);
				spin_unlock(&pcdev->lock);
				return IRQ_HANDLED;
			}
#endif
        if (!pcdev->fps) {
            do_gettimeofday(&pcdev->first_tv);            
        }
process_another_frame: 
        if((tmp_cif_frmst & CIF_F0_READY) && (tmp_cif_frmst & CIF_F1_READY)){
		    printk(KERN_DEBUG"%s:f0 && f1 ready ,need to resart cif!!!!!\n",__func__);
    	    spin_lock(&pcdev->lock);

            rk_camera_store_resore_register(pcdev);
			rk_videobuf_capture(pcdev->active0,pcdev,0);
			rk_videobuf_capture(pcdev->active1,pcdev,1);
			tmp_cifctrl &=~ENABLE_CAPTURE;
		    spin_unlock(&pcdev->lock);

			goto RK_CAMERA_IRQ_END;
        }
		pcdev->fps++;
        
		if (tmp_cif_frmst & CIF_F0_READY){
			active = &pcdev->active0;
			flag = 0;
		} else if (tmp_cif_frmst & CIF_F1_READY){
			active = &pcdev->active1;
			flag = 1;
		} else {
			printk("irq frame status erro \n");
			goto RK_CAMERA_IRQ_END;
		}
#if 0	
		if (!(*active)){
			goto RK_CAMERA_IRQ_END;
		}
#endif
		if (pcdev->frame_inval>0) {
			pcdev->frame_inval--;
			rk_videobuf_capture(*active,pcdev,flag);
			goto first_frame_done;
		} else if (pcdev->frame_inval) {
			RK30_CAM_DEBUG_TRACE("frame_inval : %0x",pcdev->frame_inval);
			pcdev->frame_inval = 0;
		}
        
		if(pcdev->fps == RK30_CAM_FRAME_MEASURE) {
			do_gettimeofday(&tv);            
			pcdev->frame_interval = ((tv.tv_sec*1000000 + tv.tv_usec) - (pcdev->first_tv.tv_sec*1000000 + pcdev->first_tv.tv_usec))
									/(RK30_CAM_FRAME_MEASURE-1);
        }
		
		vb = *active;
		
#if 0
		if(!vb){
			RK30_CAM_DEBUG_TRACE("no acticve buffer!!!\n");
			goto RK_CAMERA_IRQ_END;
		}

		if (vb->stream.prev != &(pcdev->video_vq->stream)) {
			RK30_CAM_DEBUG_TRACE("vb(%d) isn't first node in stream list\n", vb->i);
			goto RK_CAMERA_IRQ_END;
		}
#endif
		*active = NULL;
	    spin_lock(&pcdev->lock);
		if (!list_empty(&pcdev->capture)) {
			*active = list_entry(pcdev->capture.next, struct videobuf_buffer, queue);
			if (*active) {
				WARN_ON((*active)->state != VIDEOBUF_QUEUED); 
				if (tmp_cif_frmst & CIF_F0_READY){
			        pcdev->active0 = *active;
        		} else if (tmp_cif_frmst & CIF_F1_READY){
			        pcdev->active1 = *active;
        		}else{
                    printk("irq frame status erro !\n");
        		}
				rk_videobuf_capture((*active),pcdev,flag);
				list_del_init(&((*active)->queue));
			}
		}
	    spin_unlock(&pcdev->lock);
		if ((*active) == NULL) {
		//	RK30_CAM_DEBUG_TRACE("%s video_buf queue is empty!\n",__FUNCTION__);
			if(flag == 0){			
			    pcdev->active0 = NULL;
				write_cif_reg(pcdev->base,CIF_CIF_FRM0_ADDR_Y, invalid_y_addr);
				write_cif_reg(pcdev->base,CIF_CIF_FRM0_ADDR_UV, invalid_uv_addr);
				}else{
			    pcdev->active1 = NULL;
				write_cif_reg(pcdev->base,CIF_CIF_FRM1_ADDR_Y, invalid_y_addr);
				write_cif_reg(pcdev->base,CIF_CIF_FRM1_ADDR_UV, invalid_uv_addr);
				}
		}
		if(vb)
			do_gettimeofday(&vb->ts);
		if (CAM_WORKQUEUE_IS_EN()) {
			if(vb){
					if (!list_empty(&pcdev->camera_work_queue)) {
						wk = list_entry(pcdev->camera_work_queue.next, struct rk_camera_work, queue);
						list_del_init(&wk->queue);
						INIT_WORK(&(wk->work), rk_camera_capture_process);
						wk->vb = vb;
						wk->pcdev = pcdev;
		        		wk->ts = pcdev->fps;
						queue_work(pcdev->camera_wq, &(wk->work));
					}else{
						printk("work queue is empty \n!!!!!!!!");
					}
				}
		} else {
			if ((vb->state == VIDEOBUF_QUEUED) || (vb->state == VIDEOBUF_ACTIVE)) {
				vb->state = VIDEOBUF_DONE;    	        
				vb->field_count++;
			}
			wake_up(&vb->done);
		}
    }
first_frame_done:
	if ((tmp_cif_frmst & CIF_F0_READY) && (tmp_cif_frmst & CIF_F1_READY)){
		    printk(KERN_DEBUG"%s:f0 && f1 ready ,need to process f1 too!!!!!\n",__func__);
            tmp_cif_frmst &=~CIF_F0_READY;
            goto process_another_frame;
	}

RK_CAMERA_IRQ_END:
    if((tmp_cifctrl & ENABLE_CAPTURE) == 0)
        write_cif_reg(pcdev->base,CIF_CIF_CTRL, (tmp_cifctrl | ENABLE_CAPTURE));
    return IRQ_HANDLED;
}


static void rk_videobuf_release(struct videobuf_queue *vq,
                                  struct videobuf_buffer *vb)
{
    struct rk_camera_buffer *buf = container_of(vb, struct rk_camera_buffer, vb);
    struct soc_camera_device *icd = vq->priv_data;
    struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
#if CAMERA_VIDEOBUF_ARM_ACCESS    
    struct rk29_camera_vbinfo *vb_info =NULL;
#endif

#ifdef DEBUG
    dev_dbg(&icd->dev, "%s (vb=0x%p) 0x%08lx %d\n", __func__,
            vb, vb->baddr, vb->bsize);

    switch (vb->state)
    {
        case VIDEOBUF_ACTIVE:
            dev_dbg(&icd->dev, "%s (active)\n", __func__);
            break;
        case VIDEOBUF_QUEUED:
            dev_dbg(&icd->dev, "%s (queued)\n", __func__);
            break;
        case VIDEOBUF_PREPARED:
            dev_dbg(&icd->dev, "%s (prepared)\n", __func__);
            break;
        default:  
            dev_dbg(&icd->dev, "%s (unknown)\n", __func__);
            break;
    }
#endif

	#if 0   /* ddl@rock-chips.com: this wait operation is not nessary, invalidate in v0.x.1f */
	if (vb == pcdev->active0 || vb == pcdev->active1) {
		RK30_CAM_DEBUG_TRACE("%s Wait for this video buf(0x%x) write finished!\n ",__FUNCTION__,(unsigned int)vb);
		interruptible_sleep_on_timeout(&vb->done, msecs_to_jiffies(500));
		RK30_CAM_DEBUG_TRACE("%s This video buf(0x%x) write finished, release now!!\n",__FUNCTION__,(unsigned int)vb);
	}
	#endif
	flush_workqueue(pcdev->camera_wq); 
#if CAMERA_VIDEOBUF_ARM_ACCESS
	if ((pcdev->vbinfo) && (vb->i < pcdev->vbinfo_count)) {
		vb_info = pcdev->vbinfo + vb->i;
        
		if (vb_info->vir_addr) {
			iounmap(vb_info->vir_addr);
			release_mem_region(vb_info->phy_addr, vb_info->size);
			memset(vb_info, 0x00, sizeof(struct rk29_camera_vbinfo));
		}
	}
#endif    
	rk_videobuf_free(vq, buf);
}

static struct videobuf_queue_ops rk_videobuf_ops =
{
    .buf_setup      = rk_videobuf_setup,
    .buf_prepare    = rk_videobuf_prepare,
    .buf_queue      = rk_videobuf_queue,
    .buf_release    = rk_videobuf_release,
};

static void rk_camera_init_videobuf(struct videobuf_queue *q,
                                      struct soc_camera_device *icd)
{
    struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;

    /* We must pass NULL as dev pointer, then all pci_* dma operations
     * transform to normal dma_* ones. */
    videobuf_queue_dma_contig_init(q,
                                   &rk_videobuf_ops,
                                   ici->v4l2_dev.dev, &pcdev->lock,
                                   V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                   V4L2_FIELD_NONE,
                                   sizeof(struct rk_camera_buffer),
                                   icd,&icd->video_lock);
}

static int rk_camera_mclk_ctrl(int cif_idx, int on, int clk_rate){
    int err = 0,cif;
    struct rk_cif_clk *clk;
    struct clk *cif_clk_out_div;

    cif = cif_idx - RK29_CAM_PLATFORM_DEV_ID;
    if ((cif<0)||(cif>1)) {
        RKCAMERA_TR(KERN_ERR "cif index(%d) is invalidate\n",cif_idx);
        err = -1;
        goto rk_camera_clk_ctrl_end;
    }

    clk = &cif_clk[cif];

    if(!clk->aclk_cif || !clk->hclk_cif || !clk->cif_clk_in || !clk->cif_clk_out) {
        RKCAMERA_TR(KERN_ERR "failed to get cif clock source\n");
        err = -ENOENT;
        goto rk_camera_clk_ctrl_end;
    }
    spin_lock(&clk->lock);
    if (on && !clk->on) {
        clk_enable(clk->pd_cif);
        clk_enable(clk->aclk_cif);
        clk_enable(clk->hclk_cif);
        clk_enable(clk->cif_clk_in);
        clk_enable(clk->cif_clk_out);
        clk_set_rate(clk->cif_clk_out,clk_rate);
        mdelay(10);
        clk->on = true;
        
    } else if (!on && clk->on) {
        clk_disable(clk->aclk_cif);
        clk_disable(clk->hclk_cif);
        clk_disable(clk->cif_clk_in);

        clk_disable(clk->cif_clk_out);
        clk_disable(clk->pd_cif);
        clk->on = false;
        if(cif){
            cif_clk_out_div =  clk_get(NULL, "cif1_out_div");
        }else{
            cif_clk_out_div =  clk_get(NULL, "cif0_out_div");
            if(IS_ERR_OR_NULL(cif_clk_out_div)) {
                cif_clk_out_div =  clk_get(NULL, "cif_out_div");
            }
        }

        if(IS_ERR_OR_NULL(cif_clk_out_div)) {
            err = clk_set_parent(clk->cif_clk_out, cif_clk_out_div);
            clk_put(cif_clk_out_div);
        } else {
            err = -1;
        }

        if(err)
           RKCAMERA_TR("WARNING %s_%s_%d: camera sensor mclk maybe not close, please check!!!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    spin_unlock(&clk->lock);
rk_camera_clk_ctrl_end:
    return err;
}
static int rk_camera_activate(struct rk_camera_dev *pcdev, struct soc_camera_device *icd)
{
    write_cif_reg(pcdev->base,CIF_CIF_CTRL,AXI_BURST_16|MODE_PINGPONG|DISABLE_CAPTURE);   /* ddl@rock-chips.com : vip ahb burst 16 */

    write_cif_reg(pcdev->base,CIF_CIF_INTEN, 0x01);    //capture complete interrupt enable
    RK30_CAM_DEBUG_TRACE("%s..%d.. CIF_CIF_CTRL = 0x%x\n",__FUNCTION__,__LINE__,read_cif_reg(pcdev->base, CIF_CIF_CTRL));
    return 0;
RK_CAMERA_ACTIVE_ERR:
    return -ENODEV;
}

static void rk_camera_deactivate(struct rk_camera_dev *pcdev)
{
    return;
}

/* The following two functions absolutely depend on the fact, that
 * there can be only one camera on RK28 quick capture interface */
static int rk_camera_add_device(struct soc_camera_device *icd)
{
    struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
    struct device *control = to_soc_camera_control(icd);
    struct v4l2_subdev *sd;
    int ret,i,icd_catch;
    struct rk_camera_frmivalenum *fival_list,*fival_nxt;
    struct v4l2_cropcap cropcap;
    struct v4l2_mbus_framefmt mf;
    const struct soc_camera_format_xlate *xlate = NULL;
    
    mutex_lock(&camera_lock);

    if (pcdev->icd) {
        ret = -EBUSY;
        goto ebusy;
    }

    RK30_CAM_DEBUG_TRACE("%s driver attached to %s\n",RK29_CAM_DRV_NAME,dev_name(icd->pdev));

	pcdev->frame_inval = RK_CAM_FRAME_INVAL_INIT;
    pcdev->active0 = NULL;
	pcdev->active1 = NULL;
    pcdev->icd = NULL;
	pcdev->reginfo_suspend.Inval = Reg_Invalidate;
    pcdev->zoominfo.zoom_rate = 100;
    pcdev->fps_timer.istarted = false;
        
	/* ddl@rock-chips.com: capture list must be reset, because this list may be not empty,
     * if app havn't dequeue all videobuf before close camera device;
	*/
    INIT_LIST_HEAD(&pcdev->capture);

    ret = rk_camera_activate(pcdev,icd);
    if (ret)
        goto ebusy;
    /* ddl@rock-chips.com : v4l2_subdev is not created when ici->ops->add called in soc_camera_probe  */
    if (control) {
        sd = dev_get_drvdata(control);
		v4l2_subdev_call(sd, core, ioctl, RK29_CAM_SUBDEV_IOREQUEST,(void*)pcdev->pdata);
        #if 0
        ret = v4l2_subdev_call(sd,core, init, 0);
        if (ret)
            goto ebusy;
        #endif
        v4l2_subdev_call(sd, core, ioctl, RK29_CAM_SUBDEV_CB_REGISTER,(void*)(&pcdev->icd_cb));

        if (v4l2_subdev_call(sd, video, cropcap, &cropcap) == 0) {
            memcpy(&pcdev->cropinfo.bounds ,&cropcap.bounds,sizeof(struct v4l2_rect));
        } else {
            xlate = soc_camera_xlate_by_fourcc(icd, V4L2_PIX_FMT_NV12);
            mf.width = 10000;
            mf.height = 10000;
            mf.field = V4L2_FIELD_NONE;
            mf.code = xlate->code;
            mf.reserved[6] = 0xfefe5a5a;
            v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);

            pcdev->cropinfo.bounds.left = 0;
            pcdev->cropinfo.bounds.top = 0;
            pcdev->cropinfo.bounds.width = mf.width;
            pcdev->cropinfo.bounds.height = mf.height;
        }
    }
    pcdev->icd = icd;
    pcdev->icd_init = 0;

    icd_catch = 0;
    for (i=0; i<2; i++) {
        if (pcdev->icd_frmival[i].icd == icd)
            icd_catch = 1;
        if (pcdev->icd_frmival[i].icd == NULL) {
            pcdev->icd_frmival[i].icd = icd;
            icd_catch = 1;
        }
    }
    if (icd_catch == 0) {
        fival_list = pcdev->icd_frmival[0].fival_list;
        fival_nxt = fival_list;
        while(fival_nxt != NULL) {
            fival_nxt = fival_list->nxt;
            kfree(fival_list);
            fival_list = fival_nxt;
        }
        pcdev->icd_frmival[0].icd = icd;
        pcdev->icd_frmival[0].fival_list = kzalloc(sizeof(struct rk_camera_frmivalenum),GFP_KERNEL);
    }
ebusy:
    mutex_unlock(&camera_lock);

    return ret;
}
static void rk_camera_remove_device(struct soc_camera_device *icd)
{
    struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
#if CAMERA_VIDEOBUF_ARM_ACCESS    
    struct rk29_camera_vbinfo *vb_info;
    unsigned int i;
#endif    

	mutex_lock(&camera_lock);
    BUG_ON(icd != pcdev->icd);

    RK30_CAM_DEBUG_TRACE("%s driver detached from %s\n",RK29_CAM_DRV_NAME,dev_name(icd->pdev));

	/* ddl@rock-chips.com: Application will call VIDIOC_STREAMOFF before close device, but
	   stream may be turn on again before close device, if suspend and resume happened. */
	if (read_cif_reg(pcdev->base,CIF_CIF_CTRL) & ENABLE_CAPTURE) {
		rk_camera_s_stream(icd,0);
	}
    
    //soft reset  the registers
    #if 0 //has somthing wrong when suspend and resume now
    if(IS_CIF0()){
        write_cru_reg(CRU_CIF_RST_REG30,MASK_RST_CIF0|RQUEST_RST_CIF0 | (read_cru_reg(CRU_CIF_RST_REG30)));
        write_cru_reg(CRU_CIF_RST_REG30,(read_cru_reg(CRU_CIF_RST_REG30)&(~RQUEST_RST_CIF0)) | MASK_RST_CIF0);
    }else{
        write_cru_reg(CRU_CIF_RST_REG30,MASK_RST_CIF1|RQUEST_RST_CIF1 | (read_cru_reg(CRU_CIF_RST_REG30)));
        write_cru_reg(CRU_CIF_RST_REG30,(read_cru_reg(CRU_CIF_RST_REG30)&(~RQUEST_RST_CIF1)) | MASK_RST_CIF1);
    }
    #endif
    v4l2_subdev_call(sd, core, ioctl, RK29_CAM_SUBDEV_DEACTIVATE,NULL);
    //if stream off is not been executed,timer is running.
    if(pcdev->fps_timer.istarted){
         hrtimer_cancel(&pcdev->fps_timer.timer);
         pcdev->fps_timer.istarted = false;
    }
    flush_work(&(pcdev->camera_reinit_work.work));
	flush_workqueue((pcdev->camera_wq));
    
	if (pcdev->camera_work) {
		kfree(pcdev->camera_work);
		pcdev->camera_work = NULL;
		pcdev->camera_work_count = 0;
        INIT_LIST_HEAD(&pcdev->camera_work_queue);
	}
	rk_camera_deactivate(pcdev);
#if CAMERA_VIDEOBUF_ARM_ACCESS
    if (pcdev->vbinfo) {
        vb_info = pcdev->vbinfo;
        for (i=0; i<pcdev->vbinfo_count; i++) {
            if (vb_info->vir_addr) {
                iounmap(vb_info->vir_addr);
                release_mem_region(vb_info->phy_addr, vb_info->size);
                memset(vb_info, 0x00, sizeof(struct rk29_camera_vbinfo));
            }
            vb_info++;
        }
		kfree(pcdev->vbinfo);
		pcdev->vbinfo = NULL;
		pcdev->vbinfo_count = 0;
	}
#endif
	pcdev->active0 = NULL;
	pcdev->active1 = NULL;
    pcdev->icd = NULL;
    pcdev->icd_cb.sensor_cb = NULL;
	pcdev->reginfo_suspend.Inval = Reg_Invalidate;
	/* ddl@rock-chips.com: capture list must be reset, because this list may be not empty,
     * if app havn't dequeue all videobuf before close camera device;
	*/
    INIT_LIST_HEAD(&pcdev->capture);

	mutex_unlock(&camera_lock);
	RK30_CAM_DEBUG_TRACE("%s exit\n",__FUNCTION__);

	return;
}
static int rk_camera_set_bus_param(struct soc_camera_device *icd, __u32 pixfmt)
{
    unsigned long bus_flags, camera_flags, common_flags;
    unsigned int cif_ctrl_val = 0;
	const struct soc_mbus_pixelfmt *fmt;
	int ret = 0;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct rk_camera_dev *pcdev = ici->priv;
    RK30_CAM_DEBUG_TRACE("%s..%d..\n",__FUNCTION__,__LINE__);

	fmt = soc_mbus_get_fmtdesc(icd->current_fmt->code);
	if (!fmt)
		return -EINVAL;

    bus_flags = RK_CAM_BUS_PARAM;
	/* If requested data width is supported by the platform, use it */
	switch (fmt->bits_per_sample) {
    	case 10:
    		if (!(bus_flags & SOCAM_DATAWIDTH_10))
    			return -EINVAL;    		
    		break;
    	case 9:
    		if (!(bus_flags & SOCAM_DATAWIDTH_9))
    			return -EINVAL;    		
    		break;
    	case 8:
    		if (!(bus_flags & SOCAM_DATAWIDTH_8))
    			return -EINVAL;    		
    		break;
    	default:
    		return -EINVAL;
	}
    
	if (icd->ops->query_bus_param)
    	camera_flags = icd->ops->query_bus_param(icd);
	else
		camera_flags = 0;

    common_flags = soc_camera_bus_param_compatible(camera_flags, bus_flags);
    if (!common_flags) {
        ret = -EINVAL;
        goto RK_CAMERA_SET_BUS_PARAM_END;
    }

    ret = icd->ops->set_bus_param(icd, common_flags);
    if (ret < 0)
        goto RK_CAMERA_SET_BUS_PARAM_END;

    cif_ctrl_val = read_cif_reg(pcdev->base,CIF_CIF_FOR);
	RK30_CAM_DEBUG_TRACE("%s..%d..cif_ctrl_val = 0x%x\n",__FUNCTION__,__LINE__,cif_ctrl_val);
    if (common_flags & SOCAM_PCLK_SAMPLE_FALLING) {
       	if(IS_CIF0()) {
    		write_cru_reg(CRU_PCLK_REG30, read_cru_reg(CRU_PCLK_REG30) | ENANABLE_INVERT_PCLK_CIF0);
            RK30_CAM_DEBUG_TRACE("enable cif0 pclk invert\n");
        } else {
    		write_cru_reg(CRU_PCLK_REG30, read_cru_reg(CRU_PCLK_REG30) | ENANABLE_INVERT_PCLK_CIF1);
            RK30_CAM_DEBUG_TRACE("enable cif1 pclk invert\n");
        }
    } else {
		if(IS_CIF0()){
			write_cru_reg(CRU_PCLK_REG30, (read_cru_reg(CRU_PCLK_REG30) & 0xFFFFEFF ) | DISABLE_INVERT_PCLK_CIF0);
            RK30_CAM_DEBUG_TRACE("diable cif0 pclk invert\n");
        } else {
			write_cru_reg(CRU_PCLK_REG30, (read_cru_reg(CRU_PCLK_REG30) & 0xFFFEFFF) | DISABLE_INVERT_PCLK_CIF1);
            RK30_CAM_DEBUG_TRACE("diable cif1 pclk invert\n");
        }
    }
    if (common_flags & SOCAM_HSYNC_ACTIVE_LOW) {
        cif_ctrl_val |= HSY_LOW_ACTIVE;
    } else {
		cif_ctrl_val &= ~HSY_LOW_ACTIVE;
    }
    if (common_flags & SOCAM_VSYNC_ACTIVE_HIGH) {
        cif_ctrl_val |= VSY_HIGH_ACTIVE;
    } else {
		cif_ctrl_val &= ~VSY_HIGH_ACTIVE;
    }

    /* ddl@rock-chips.com : Don't enable capture here, enable in stream_on */
    //vip_ctrl_val |= ENABLE_CAPTURE;
    write_cif_reg(pcdev->base,CIF_CIF_FOR, cif_ctrl_val);
    RK30_CAM_DEBUG_TRACE("%s..ctrl:0x%x CIF_CIF_FOR=%x  \n",__FUNCTION__,cif_ctrl_val,read_cif_reg(pcdev->base,CIF_CIF_FOR));

RK_CAMERA_SET_BUS_PARAM_END:
	if (ret)
    	RK30_CAM_DEBUG_TRACE("\n%s..%d.. ret = %d \n",__FUNCTION__,__LINE__, ret);
    return ret;
}

static int rk_camera_try_bus_param(struct soc_camera_device *icd, __u32 pixfmt)
{
    unsigned long bus_flags, camera_flags;
    int ret;

    bus_flags = RK_CAM_BUS_PARAM;
	if (icd->ops->query_bus_param) {
        camera_flags = icd->ops->query_bus_param(icd);
	} else {
		camera_flags = 0;
	}
    ret = soc_camera_bus_param_compatible(camera_flags, bus_flags) ;

	/* rockchip BUGBUG
		if (ret < 0)
			dev_warn(icd->dev.parent,
				 "Flags incompatible: camera %lx, host %lx\n",
				 camera_flags, bus_flags);
	*/
	if(!ret){
		dev_warn(icd->dev.parent, "Flags incompatible: camera %lx, host %lx\n", camera_flags, bus_flags);
		ret = -EINVAL;
	}
	else{
		ret = 0;
	}
    return ret;
}

static const struct soc_mbus_pixelfmt rk_camera_formats[] = {
   {
		.fourcc			= V4L2_PIX_FMT_NV12,
		.name			= "YUV420 NV12",
		.bits_per_sample	= 8,
		.packing		= SOC_MBUS_PACKING_1_5X8,
		.order			= SOC_MBUS_ORDER_LE,
	},{
		.fourcc			= V4L2_PIX_FMT_NV16,
		.name			= "YUV422 NV16",
		.bits_per_sample	= 8,
		.packing		= SOC_MBUS_PACKING_2X8_PADHI,
		.order			= SOC_MBUS_ORDER_LE,
	},{
		.fourcc 		= V4L2_PIX_FMT_NV21,
		.name			= "YUV420 NV21",
		.bits_per_sample	= 8,
		.packing		= SOC_MBUS_PACKING_1_5X8,
		.order			= SOC_MBUS_ORDER_LE,
	},{
		.fourcc 		= V4L2_PIX_FMT_NV61,
		.name			= "YUV422 NV61",
		.bits_per_sample	= 8,
		.packing		= SOC_MBUS_PACKING_2X8_PADHI,
		.order			= SOC_MBUS_ORDER_LE,
	},{
		.fourcc 		= V4L2_PIX_FMT_RGB565,
		.name			= "RGB565",
		.bits_per_sample	= 8,
		.packing		= SOC_MBUS_PACKING_2X8_PADHI,
		.order			= SOC_MBUS_ORDER_LE,
	},{
		.fourcc 		= V4L2_PIX_FMT_RGB24,
		.name			= "RGB888",
		.bits_per_sample	= 8,
		.packing		= SOC_MBUS_PACKING_2X8_PADHI,
		.order			= SOC_MBUS_ORDER_LE,
	}
};

static void rk_camera_setup_format(struct soc_camera_device *icd, __u32 host_pixfmt, enum v4l2_mbus_pixelcode icd_code, struct v4l2_rect *rect)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
    unsigned int cif_fs = 0,cif_crop = 0;
    unsigned int cif_fmt_val = read_cif_reg(pcdev->base,CIF_CIF_FOR) | INPUT_MODE_YUV|YUV_INPUT_422|INPUT_420_ORDER_EVEN|OUTPUT_420_ORDER_EVEN;
	
	const struct soc_mbus_pixelfmt *fmt;
	fmt = soc_mbus_get_fmtdesc(icd_code);

	if((host_pixfmt == V4L2_PIX_FMT_RGB565) || (host_pixfmt == V4L2_PIX_FMT_RGB24)){
		if(fmt->fourcc == V4L2_PIX_FMT_NV12)
			host_pixfmt = V4L2_PIX_FMT_NV12;
		else if(fmt->fourcc == V4L2_PIX_FMT_NV21)
			host_pixfmt = V4L2_PIX_FMT_NV21;
	}
    switch (host_pixfmt)
    {
        case V4L2_PIX_FMT_NV16:
            cif_fmt_val &= ~YUV_OUTPUT_422;
		    cif_fmt_val &= ~UV_STORAGE_ORDER_UVUV;
		    pcdev->frame_inval = RK_CAM_FRAME_INVAL_DC;
		    pcdev->pixfmt = host_pixfmt;
            break;
    	case V4L2_PIX_FMT_NV61:
    		cif_fmt_val &= ~YUV_OUTPUT_422;
    		cif_fmt_val |= UV_STORAGE_ORDER_VUVU;
    		pcdev->frame_inval = RK_CAM_FRAME_INVAL_DC;
    		pcdev->pixfmt = host_pixfmt;
    		break;
        case V4L2_PIX_FMT_NV12:
            cif_fmt_val |= YUV_OUTPUT_420;
    		cif_fmt_val &= ~UV_STORAGE_ORDER_UVUV;
			if (pcdev->frame_inval != RK_CAM_FRAME_INVAL_INIT)
				pcdev->frame_inval = RK_CAM_FRAME_INVAL_INIT;
			pcdev->pixfmt = host_pixfmt;
            break;
    	case V4L2_PIX_FMT_NV21:
    		cif_fmt_val |= YUV_OUTPUT_420;
    		cif_fmt_val |= UV_STORAGE_ORDER_VUVU;
    		if (pcdev->frame_inval != RK_CAM_FRAME_INVAL_INIT)
    			pcdev->frame_inval = RK_CAM_FRAME_INVAL_INIT;
    		pcdev->pixfmt = host_pixfmt;
    		break;
            default:                                                                                /* ddl@rock-chips.com : vip output format is hold when pixfmt is invalidate */
    			cif_fmt_val |= YUV_OUTPUT_422;
                break;
    }
    switch (icd_code)
    {
        case V4L2_MBUS_FMT_UYVY8_2X8:
            cif_fmt_val = YUV_INPUT_ORDER_UYVY(cif_fmt_val);
            break;
        case V4L2_MBUS_FMT_YUYV8_2X8:
            cif_fmt_val = YUV_INPUT_ORDER_YUYV(cif_fmt_val);
            break;
    	case V4L2_MBUS_FMT_YVYU8_2X8:
    		cif_fmt_val = YUV_INPUT_ORDER_YVYU(cif_fmt_val);
    		break;
    	case V4L2_MBUS_FMT_VYUY8_2X8:
    		cif_fmt_val = YUV_INPUT_ORDER_VYUY(cif_fmt_val);
    		break;
        default :
			cif_fmt_val = YUV_INPUT_ORDER_YUYV(cif_fmt_val);
            break;
    }
#if 1
        {
#if (defined(CONFIG_ARCH_RK30) || defined(CONFIG_ARCH_RK2928))
           mdelay(100);
            if(IS_CIF0()){
        //		pmu_set_idle_request(IDLE_REQ_VIO, true);
        		cru_set_soft_reset(SOFT_RST_CIF0, true);
        		udelay(5);
        		cru_set_soft_reset(SOFT_RST_CIF0, false);
        //		pmu_set_idle_request(IDLE_REQ_VIO, false);

            }else{
        //	 	pmu_set_idle_request(IDLE_REQ_VIO, true);
        		cru_set_soft_reset(SOFT_RST_CIF1, true);
        		udelay(5);
        		cru_set_soft_reset(SOFT_RST_CIF1, false);
        //		pmu_set_idle_request(IDLE_REQ_VIO, false);  
            }
#elif defined(CONFIG_ARCH_RK3188)
//		pmu_set_idle_request(IDLE_REQ_VIO, true);
		cru_set_soft_reset(SOFT_RST_CIF0, true);
		udelay(5);
		cru_set_soft_reset(SOFT_RST_CIF0, false);
//		pmu_set_idle_request(IDLE_REQ_VIO, false);

#endif
        }
    write_cif_reg(pcdev->base,CIF_CIF_CTRL,AXI_BURST_16|MODE_PINGPONG|DISABLE_CAPTURE);   /* ddl@rock-chips.com : vip ahb burst 16 */
    write_cif_reg(pcdev->base,CIF_CIF_INTEN, 0x01|0x200);    //capture complete interrupt enable
#endif
    write_cif_reg(pcdev->base,CIF_CIF_FOR,cif_fmt_val);         /* ddl@rock-chips.com: VIP capture mode and capture format must be set before FS register set */

   // read_cif_reg(pcdev->base,CIF_CIF_INTSTAT);                /* clear vip interrupte single  */
    write_cif_reg(pcdev->base,CIF_CIF_INTSTAT,0xFFFFFFFF);
	/* 
    if((read_cif_reg(pcdev->base,CIF_CIF_CTRL) & MODE_PINGPONG)
		||(read_cif_reg(pcdev->base,CIF_CIF_CTRL) & MODE_LINELOOP)) {
	    BUG();	
    } else*/{ // this is one frame mode
	    cif_crop = (rect->left+ (rect->top<<16));
	    cif_fs	= ((rect->width ) + (rect->height<<16));
	}

	write_cif_reg(pcdev->base,CIF_CIF_CROP, cif_crop);
	write_cif_reg(pcdev->base,CIF_CIF_SET_SIZE, cif_fs);
	write_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH, rect->width);
	write_cif_reg(pcdev->base,CIF_CIF_FRAME_STATUS,  0x00000003);

    //MUST bypass scale 
	write_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL,0x10);
    RK30_CAM_DEBUG_TRACE("%s.. crop:0x%x fs:0x%x cif_fmt_val:0x%x CIF_CIF_FOR:0x%x\n",__FUNCTION__,cif_crop,cif_fs,cif_fmt_val,read_cif_reg(pcdev->base,CIF_CIF_FOR));
	return;
}

static int rk_camera_get_formats(struct soc_camera_device *icd, unsigned int idx,
				  struct soc_camera_format_xlate *xlate)
{
    struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
    struct device *dev = icd->dev.parent;
    int formats = 0, ret;
	enum v4l2_mbus_pixelcode code;
	const struct soc_mbus_pixelfmt *fmt;

	ret = v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code);
	if (ret < 0)
		/* No more formats */
		return 0;

	fmt = soc_mbus_get_fmtdesc(code);
	if (!fmt) {
		dev_err(dev, "Invalid format code #%u: %d\n", idx, code);
		return 0;
	}

    ret = rk_camera_try_bus_param(icd, fmt->bits_per_sample);
    if (ret < 0)
        return 0;

    switch (code) {
            case V4L2_MBUS_FMT_UYVY8_2X8:
            case V4L2_MBUS_FMT_YUYV8_2X8:
            case V4L2_MBUS_FMT_YVYU8_2X8:
            case V4L2_MBUS_FMT_VYUY8_2X8:
#if (CONFIG_CAMERA_SCALE_CROP_MACHINE != RK_CAM_SCALE_CROP_RGA)
		formats++;
		if (xlate) {
			xlate->host_fmt = &rk_camera_formats[0];
			xlate->code = code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				rk_camera_formats[0].name,code);
		}
		
		formats++;
		if (xlate) {
			xlate->host_fmt = &rk_camera_formats[1];
			xlate->code = code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				rk_camera_formats[1].name,code);
		}
		
		formats++;
		if (xlate) {
			xlate->host_fmt = &rk_camera_formats[2];
			xlate->code = code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				rk_camera_formats[2].name,code);
		} 
		
		formats++;
		if (xlate) {
			xlate->host_fmt = &rk_camera_formats[3];
			xlate->code = code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				rk_camera_formats[3].name,code);
		}
		break;	
#else 
		formats++;
		if (xlate) {
			xlate->host_fmt = &rk_camera_formats[4];
			xlate->code = code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				rk_camera_formats[4].name,code);
		}
		formats++;
		if (xlate) {
			xlate->host_fmt = &rk_camera_formats[5];
			xlate->code = code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				rk_camera_formats[5].name,code);
		}
			break;		
#endif			
        default:
            break;
    }

    return formats;
}

static void rk_camera_put_formats(struct soc_camera_device *icd)
{
	return;
}
static int rk_camera_get_crop(struct soc_camera_device *icd,struct v4l2_crop *crop)
{
    struct soc_camera_host *ici =to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;

    spin_lock(&pcdev->cropinfo.lock);
    memcpy(&crop->c,&pcdev->cropinfo.c,sizeof(struct v4l2_rect));
    spin_unlock(&pcdev->cropinfo.lock);
    
    return 0;
}
static int rk_camera_set_crop(struct soc_camera_device *icd,
			       struct v4l2_crop *a)
{
    struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_framefmt mf;
	u32 fourcc = icd->current_fmt->host_fmt->fourcc;
    int ret;

    ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
    if (ret < 0)
        return ret;

    if ((mf.width < (a->c.left + a->c.width)) || (mf.height < (a->c.top + a->c.height)))  {

        mf.width = a->c.left + a->c.width;
        mf.height = a->c.top + a->c.height;

        v4l_bound_align_image(&mf.width, RK_CAM_W_MIN, RK_CAM_W_MAX, 1,
            &mf.height, RK_CAM_H_MIN, RK_CAM_H_MAX, 0,
            fourcc == V4L2_PIX_FMT_NV16 ?4 : 0);

        ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
        if (ret < 0)
            return ret;
    }

    rk_camera_setup_format(icd, fourcc, mf.code, &a->c);

    icd->user_width = mf.width;
    icd->user_height = mf.height;

    return 0;
}

static int rk_camera_set_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
    struct device *dev = icd->dev.parent;
    struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
    const struct soc_camera_format_xlate *xlate = NULL;
	struct soc_camera_host *ici =to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
    struct v4l2_pix_format *pix = &f->fmt.pix;
    struct v4l2_mbus_framefmt mf;
    struct v4l2_rect rect;
    int ret,usr_w,usr_h,sensor_w,sensor_h;
    int stream_on = 0;
    int ratio, bounds_aspect;
    
	usr_w = pix->width;
	usr_h = pix->height;
    
    RK30_CAM_DEBUG_TRACE("enter width:%d  height:%d\n",usr_w,usr_h);
    xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
    if (!xlate) {
        dev_err(dev, "Format %x not found\n", pix->pixelformat);
        ret = -EINVAL;
        goto RK_CAMERA_SET_FMT_END;
    }
    
    /* ddl@rock-chips.com: sensor init code transmit in here after open */
    if (pcdev->icd_init == 0) {
        v4l2_subdev_call(sd,core, init, 0); 
        pcdev->icd_init = 1;
        return 0;
    }
    stream_on = read_cif_reg(pcdev->base,CIF_CIF_CTRL);
    if (stream_on & ENABLE_CAPTURE)
        write_cif_reg(pcdev->base,CIF_CIF_CTRL, (stream_on & (~ENABLE_CAPTURE)));

    mf.width	= pix->width;
    mf.height	= pix->height;
    mf.field	= pix->field;
    mf.colorspace	= pix->colorspace;
    mf.code		= xlate->code;
    mf.reserved[0] = pix->priv;              /* ddl@rock-chips.com : v0.3.3 */
    mf.reserved[1] = 0;

    ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
    if (mf.code != xlate->code)
		return -EINVAL;    		

    if ((pcdev->cropinfo.c.width == pcdev->cropinfo.bounds.width) && 
        (pcdev->cropinfo.c.height == pcdev->cropinfo.bounds.height)) {
        bounds_aspect = (pcdev->cropinfo.bounds.width*10/pcdev->cropinfo.bounds.height);
        if ((mf.width*10/mf.height) != bounds_aspect) {
            RK30_CAM_DEBUG_TRACE("User request fov unchanged in %dx%d, But sensor %dx%d is croped, so switch to full resolution %dx%d\n",
                        usr_w,usr_h,mf.width, mf.height,pcdev->cropinfo.bounds.width,pcdev->cropinfo.bounds.height);
            
            mf.width	= pcdev->cropinfo.bounds.width/4;
            mf.height	= pcdev->cropinfo.bounds.height/4;

            mf.field	= pix->field;
            mf.colorspace	= pix->colorspace;
            mf.code		= xlate->code;
            mf.reserved[0] = pix->priv; 
            mf.reserved[1] = 0;

            ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
            if (mf.code != xlate->code)
            	return -EINVAL; 
        }
    }

    sensor_w = mf.width;
    sensor_h = mf.height;

    ratio = ((mf.width*mf.reserved[1])/100)&(~0x03);      // 4 align
    mf.width -= ratio;

    ratio = ((ratio*mf.height/mf.width)+1)&(~0x01);       // 2 align
    mf.height -= ratio;

	if ((mf.width != usr_w) || (mf.height != usr_h)) {
        
        if (unlikely((mf.width <16) || (mf.width > 8190) || (mf.height < 16) || (mf.height > 8190))) {
    		RKCAMERA_TR("Senor and IPP both invalid source resolution(%dx%d)\n",mf.width,mf.height);
    		ret = -EINVAL;
    		goto RK_CAMERA_SET_FMT_END;
        }    	
    	if (unlikely((usr_w <16)||(usr_h < 16))) {
    		RKCAMERA_TR("Senor and IPP both invalid destination resolution(%dx%d)\n",usr_w,usr_h);
    		ret = -EINVAL;
            goto RK_CAMERA_SET_FMT_END;
        }
        
		spin_lock(&pcdev->cropinfo.lock);
		if (((mf.width*10/mf.height) != (usr_w*10/usr_h))) {  
            if ((pcdev->cropinfo.c.width == 0)&&(pcdev->cropinfo.c.height == 0)) {        
                //Scale + Crop center is for keep aspect ratio unchange
                ratio = ((mf.width*10/usr_w) >= (mf.height*10/usr_h))?(mf.height*10/usr_h):(mf.width*10/usr_w);
                pcdev->host_width = ratio*usr_w/10;
                pcdev->host_height = ratio*usr_h/10;
                pcdev->host_width &= ~CROP_ALIGN_BYTES;
                pcdev->host_height &= ~CROP_ALIGN_BYTES;

                pcdev->host_left = ((sensor_w-pcdev->host_width )>>1);
                pcdev->host_top = ((sensor_h-pcdev->host_height)>>1);
            } else {    
                //Scale + crop(user define)
                pcdev->host_width = pcdev->cropinfo.c.width*mf.width/pcdev->cropinfo.bounds.width;
                pcdev->host_height = pcdev->cropinfo.c.height*mf.height/pcdev->cropinfo.bounds.height;
                pcdev->host_left = (pcdev->cropinfo.c.left*mf.width/pcdev->cropinfo.bounds.width);
                pcdev->host_top = (pcdev->cropinfo.c.top*mf.height/pcdev->cropinfo.bounds.height);
            }

            pcdev->host_left &= (~0x01);
            pcdev->host_top &= (~0x01);
        } else { 
            if ((pcdev->cropinfo.c.width == 0)&&(pcdev->cropinfo.c.height == 0)) {
                //Crop Center for cif can work , then scale
                pcdev->host_width = mf.width;
                pcdev->host_height = mf.height;
                pcdev->host_left = ((sensor_w - mf.width)>>1)&(~0x01);
                pcdev->host_top = ((sensor_h - mf.height)>>1)&(~0x01);
            } else {
                //Crop center for cif can work + crop(user define), then scale 
                pcdev->host_width = pcdev->cropinfo.c.width*mf.width/pcdev->cropinfo.bounds.width;
                pcdev->host_height = pcdev->cropinfo.c.height*mf.height/pcdev->cropinfo.bounds.height;
                pcdev->host_left = (pcdev->cropinfo.c.left*mf.width/pcdev->cropinfo.bounds.width)+((sensor_w - mf.width)>>1);
                pcdev->host_top = (pcdev->cropinfo.c.top*mf.height/pcdev->cropinfo.bounds.height)+((sensor_h - mf.height)>>1);
            }

            pcdev->host_left &= (~0x01);
            pcdev->host_top &= (~0x01);
        }
        spin_unlock(&pcdev->cropinfo.lock);
    } else {
        spin_lock(&pcdev->cropinfo.lock);
        if ((pcdev->cropinfo.c.width == 0)&&(pcdev->cropinfo.c.height == 0)) {
            pcdev->host_width = mf.width;
            pcdev->host_height = mf.height;
            pcdev->host_left = 0;
            pcdev->host_top = 0;
        } else {
            pcdev->host_width = pcdev->cropinfo.c.width*mf.width/pcdev->cropinfo.bounds.width;
            pcdev->host_height = pcdev->cropinfo.c.height*mf.height/pcdev->cropinfo.bounds.height;
            pcdev->host_left = (pcdev->cropinfo.c.left*mf.width/pcdev->cropinfo.bounds.width);
            pcdev->host_top = (pcdev->cropinfo.c.top*mf.height/pcdev->cropinfo.bounds.height);
        }
        spin_unlock(&pcdev->cropinfo.lock);
    }
    
    icd->sense = NULL;
    if (!ret) {
        rect.width = pcdev->host_width;
        rect.height = pcdev->host_height;
    	rect.left = pcdev->host_left;
    	rect.top = pcdev->host_top;
        
        down(&pcdev->zoominfo.sem);
#if CIF_DO_CROP   // this crop is only for digital zoom
        pcdev->zoominfo.a.c.left = 0;
        pcdev->zoominfo.a.c.top = 0;
        pcdev->zoominfo.a.c.width = pcdev->host_width*100/pcdev->zoominfo.zoom_rate;
        pcdev->zoominfo.a.c.width &= ~CROP_ALIGN_BYTES;
        pcdev->zoominfo.a.c.height = pcdev->host_height*100/pcdev->zoominfo.zoom_rate;
        pcdev->zoominfo.a.c.height &= ~CROP_ALIGN_BYTES;
        pcdev->zoominfo.vir_width = pcdev->zoominfo.a.c.width;
        pcdev->zoominfo.vir_height = pcdev->zoominfo.a.c.height;
        //recalculate the CIF width & height
        rect.width = pcdev->zoominfo.a.c.width ;
        rect.height = pcdev->zoominfo.a.c.height;
        rect.left = ((((pcdev->host_width - pcdev->zoominfo.a.c.width)>>1))+pcdev->host_left)&(~0x01);
        rect.top = ((((pcdev->host_height - pcdev->zoominfo.a.c.height)>>1))+pcdev->host_top)&(~0x01);
#else
        pcdev->zoominfo.a.c.width = pcdev->host_width*100/pcdev->zoominfo.zoom_rate;
        pcdev->zoominfo.a.c.width &= ~CROP_ALIGN_BYTES;
        pcdev->zoominfo.a.c.height = pcdev->host_height*100/pcdev->zoominfo.zoom_rate;
        pcdev->zoominfo.a.c.height &= ~CROP_ALIGN_BYTES;
        //now digital zoom use ipp to do crop and scale
        if(pcdev->zoominfo.zoom_rate != 100){
            pcdev->zoominfo.a.c.left = ((pcdev->host_width - pcdev->zoominfo.a.c.width)>>1)&(~0x01);
            pcdev->zoominfo.a.c.top = ((pcdev->host_height - pcdev->zoominfo.a.c.height)>>1)&(~0x01);
        } else {
            pcdev->zoominfo.a.c.left = 0;
            pcdev->zoominfo.a.c.top = 0;
        }
        pcdev->zoominfo.vir_width = pcdev->host_width;
        pcdev->zoominfo.vir_height = pcdev->host_height;
#endif
        up(&pcdev->zoominfo.sem);

        /* ddl@rock-chips.com: IPP work limit check */
        if ((pcdev->zoominfo.a.c.width != usr_w) || (pcdev->zoominfo.a.c.height != usr_h)) {
            if (usr_w > 0x7f0) {
                if (((usr_w>>1)&0x3f) && (((usr_w>>1)&0x3f) <= 8)) {
                    RKCAMERA_TR("IPP Destination resolution(%dx%d, ((%d div 1) mod 64)=%d is <= 8)",usr_w,usr_h, usr_w, (int)((usr_w>>1)&0x3f));
                    ret = -EINVAL;
                    goto RK_CAMERA_SET_FMT_END;
                }
            } else {
                if ((usr_w&0x3f) && ((usr_w&0x3f) <= 8)) {
                    RKCAMERA_TR("IPP Destination resolution(%dx%d, %d mod 64=%d is <= 8)",usr_w,usr_h, usr_w, (int)(usr_w&0x3f));
                    ret = -EINVAL;
                    goto RK_CAMERA_SET_FMT_END;
                }
            }
        }
        
        RK30_CAM_DEBUG_TRACE("%s CIF Host:%dx%d@(%d,%d) Sensor:%dx%d->%dx%d User crop:(%d,%d,%d,%d)in(%d,%d) (zoom: %dx%d@(%d,%d)->%dx%d)\n",xlate->host_fmt->name,
			           pcdev->host_width,pcdev->host_height,pcdev->host_left,pcdev->host_top,
			           sensor_w,sensor_h,mf.width,mf.height,
			           pcdev->cropinfo.c.left,pcdev->cropinfo.c.top,pcdev->cropinfo.c.width,pcdev->cropinfo.c.height,
			           pcdev->cropinfo.bounds.width,pcdev->cropinfo.bounds.height,
			           pcdev->zoominfo.a.c.width,pcdev->zoominfo.a.c.height, pcdev->zoominfo.a.c.left,pcdev->zoominfo.a.c.top,
			           pix->width, pix->height);
        rk_camera_setup_format(icd, pix->pixelformat, mf.code, &rect); 
        
		if (CAM_IPPWORK_IS_EN()) {
			BUG_ON(pcdev->vipmem_phybase == 0);
		}
        pix->width = usr_w;
    	pix->height = usr_h;
    	pix->field = mf.field;
    	pix->colorspace = mf.colorspace;
    	icd->current_fmt = xlate;   
        pcdev->icd_width = mf.width;
        pcdev->icd_height = mf.height;
    }

RK_CAMERA_SET_FMT_END:
    if (stream_on & ENABLE_CAPTURE)
        write_cif_reg(pcdev->base,CIF_CIF_CTRL, (read_cif_reg(pcdev->base,CIF_CIF_CTRL) | ENABLE_CAPTURE));
	if (ret)
    	RKCAMERA_TR("\n%s..%d.. ret = %d  \n",__FUNCTION__,__LINE__, ret);
    return ret;
}
static bool rk_camera_fmt_capturechk(struct v4l2_format *f)
{
    bool ret = false;

	if ((f->fmt.pix.width == 1024) && (f->fmt.pix.height == 768)) {
		ret = true;
	} else if ((f->fmt.pix.width == 1280) && (f->fmt.pix.height == 1024)) {
		ret = true;
	} else if ((f->fmt.pix.width == 1600) && (f->fmt.pix.height == 1200)) {
		ret = true;
	} else if ((f->fmt.pix.width == 2048) && (f->fmt.pix.height == 1536)) {
		ret = true;
	} else if ((f->fmt.pix.width == 2592) && (f->fmt.pix.height == 1944)) {
		ret = true;
	} else if ((f->fmt.pix.width == 3264) && (f->fmt.pix.height == 2448)) {
		ret = true;
	}

	if (ret == true)
		RK30_CAM_DEBUG_TRACE("%s %dx%d is capture format\n", __FUNCTION__, f->fmt.pix.width, f->fmt.pix.height);
	return ret;
}
static int rk_camera_try_fmt(struct soc_camera_device *icd,
                                   struct v4l2_format *f)
{
    struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct rk_camera_dev *pcdev = ici->priv;
    struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
    const struct soc_camera_format_xlate *xlate;
    struct v4l2_pix_format *pix = &f->fmt.pix;
    __u32 pixfmt = pix->pixelformat;
    int ret,usr_w,usr_h,i;
	bool is_capture = rk_camera_fmt_capturechk(f);
	bool vipmem_is_overflow = false;
    struct v4l2_mbus_framefmt mf;
    int bytes_per_line_host;
    
	usr_w = pix->width;
	usr_h = pix->height;
    
    xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
    if (!xlate) {
        dev_err(icd->dev.parent, "Format (%c%c%c%c) not found\n", pixfmt & 0xFF, (pixfmt >> 8) & 0xFF,
			(pixfmt >> 16) & 0xFF, (pixfmt >> 24) & 0xFF);
        ret = -EINVAL;
        RK30_CAM_DEBUG_TRACE("%s(version:%c%c%c) support format:\n",rk_cam_driver_description,(RK_CAM_VERSION_CODE&0xff0000)>>16,
            (RK_CAM_VERSION_CODE&0xff00)>>8,(RK_CAM_VERSION_CODE&0xff));
        for (i = 0; i < icd->num_user_formats; i++)
		    RK30_CAM_DEBUG_TRACE("(%c%c%c%c)-%s\n",
		    icd->user_formats[i].host_fmt->fourcc & 0xFF, (icd->user_formats[i].host_fmt->fourcc >> 8) & 0xFF,
			(icd->user_formats[i].host_fmt->fourcc >> 16) & 0xFF, (icd->user_formats[i].host_fmt->fourcc >> 24) & 0xFF,
			icd->user_formats[i].host_fmt->name);
        goto RK_CAMERA_TRY_FMT_END;
    }
   /* limit to rk29 hardware capabilities */
    v4l_bound_align_image(&pix->width, RK_CAM_W_MIN, RK_CAM_W_MAX, 1,
    	      &pix->height, RK_CAM_H_MIN, RK_CAM_H_MAX, 0,
    	      pixfmt == V4L2_PIX_FMT_NV16 ? 4 : 0);

    pix->bytesperline = soc_mbus_bytes_per_line(pix->width,
						    xlate->host_fmt);
	if (pix->bytesperline < 0)
		return pix->bytesperline;

    /* limit to sensor capabilities */
    memset(&mf,0x00,sizeof(struct v4l2_mbus_framefmt));
	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;
    /* ddl@rock-chips.com : It is query max resolution only. */
    if ((usr_w == 10000) && (usr_h == 10000)) {
        mf.reserved[6] = 0xfefe5a5a;
    }

	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		goto RK_CAMERA_TRY_FMT_END;
    
	//query resolution.
	if((usr_w == 10000) && (usr_h == 10000)) {
		pix->width = mf.width;
        pix->height = mf.height;
        RK30_CAM_DEBUG_TRACE("%s: Sensor resolution : %dx%d\n",__FUNCTION__,mf.width,mf.height);
		goto RK_CAMERA_TRY_FMT_END;
	} else {
        RK30_CAM_DEBUG_TRACE("%s: user demand: %dx%d  sensor output: %dx%d \n",__FUNCTION__,usr_w,usr_h,mf.width,mf.height);
	}
    
	#ifdef CONFIG_VIDEO_RK29_WORK_IPP       
	if ((mf.width != usr_w) || (mf.height != usr_h)) {
        bytes_per_line_host = soc_mbus_bytes_per_line(mf.width,icd->current_fmt->host_fmt); 
        #ifndef OPTIMIZE_MEMORY_USE
		if (is_capture) {
			vipmem_is_overflow = (PAGE_ALIGN(bytes_per_line_host*mf.height) > pcdev->vipmem_size);
		} else {
			/* Assume preview buffer minimum is 4 */
			vipmem_is_overflow = (PAGE_ALIGN(bytes_per_line_host*mf.height)*4 > pcdev->vipmem_size);
		}
		#else
		    vipmem_is_overflow =false;
		#endif
		if (vipmem_is_overflow == false) {
			pix->width = usr_w;
			pix->height = usr_h;
		} else {
			RK30_CAM_DEBUG_TRACE("vipmem for IPP is overflow, This resolution(%dx%d -> %dx%d) is invalidate!\n",mf.width,mf.height,usr_w,usr_h);
            pix->width = mf.width;
            pix->height = mf.height;            
		}
        /* ddl@rock-chips.com: Invalidate these code, because sensor need interpolate */
        #if 0     
        if ((mf.width < usr_w) || (mf.height < usr_h)) {
            if (((usr_w>>1) > mf.width) || ((usr_h>>1) > mf.height)) {
                RK30_CAM_DEBUG_TRACE("The aspect ratio(%dx%d/%dx%d) is bigger than 2 !\n",mf.width,mf.height,usr_w,usr_h);
                pix->width = mf.width;
                pix->height = mf.height;
            }
        }    
        #endif
	}
	#else
	//need to change according to crop and scale capablicity
	if ((mf.width > usr_w) && (mf.height > usr_h)) {
			pix->width = usr_w;
			pix->height = usr_h;
	    } else if ((mf.width < usr_w) && (mf.height < usr_h)) {
			RK30_CAM_DEBUG_TRACE("%dx%d can't scale up to %dx%d!\n",mf.width,mf.height,usr_w,usr_h);
            pix->width	= mf.width;
        	pix->height	= mf.height;	
        }
    #endif
    pix->colorspace	= mf.colorspace;    

    switch (mf.field) {
	case V4L2_FIELD_ANY:
	case V4L2_FIELD_NONE:
		pix->field	= V4L2_FIELD_NONE;
		break;
	default:
		/* TODO: support interlaced at least in pass-through mode */
		dev_err(icd->dev.parent, "Field type %d unsupported.\n",
			mf.field);
		goto RK_CAMERA_TRY_FMT_END;
	}

RK_CAMERA_TRY_FMT_END:
	if (ret)
    	RK30_CAM_DEBUG_TRACE("\n%s..%d.. ret = %d  \n",__FUNCTION__,__LINE__, ret);
    return ret;
}

static int rk_camera_reqbufs(struct soc_camera_device *icd,
                               struct v4l2_requestbuffers *p)
{
    int i;

    /* This is for locking debugging only. I removed spinlocks and now I
     * check whether .prepare is ever called on a linked buffer, or whether
     * a dma IRQ can occur for an in-work or unlinked buffer. Until now
     * it hadn't triggered */
    for (i = 0; i < p->count; i++) {
        struct rk_camera_buffer *buf = container_of(icd->vb_vidq.bufs[i],
                                                           struct rk_camera_buffer, vb);
        buf->inwork = 0;
        INIT_LIST_HEAD(&buf->vb.queue);
    }

    return 0;
}

static unsigned int rk_camera_poll(struct file *file, poll_table *pt)
{
    struct soc_camera_device *icd = file->private_data;
    struct rk_camera_buffer *buf;

    buf = list_entry(icd->vb_vidq.stream.next, struct rk_camera_buffer,
                    vb.stream);

    poll_wait(file, &buf->vb.done, pt);

    if (buf->vb.state == VIDEOBUF_DONE ||
            buf->vb.state == VIDEOBUF_ERROR)
        return POLLIN|POLLRDNORM;

    return 0;
}

static int rk_camera_querycap(struct soc_camera_host *ici,
                                struct v4l2_capability *cap)
{
    struct rk_camera_dev *pcdev = ici->priv;
    struct rkcamera_platform_data *new_camera;
    char orientation[5];
    char fov[9];
    int i;

    strlcpy(cap->card, dev_name(pcdev->icd->pdev), 20);
    memset(orientation,0x00,sizeof(orientation));
    for (i=0; i<RK_CAM_NUM;i++) {
        if ((pcdev->pdata->info[i].dev_name!=NULL) && (strcmp(dev_name(pcdev->icd->pdev), pcdev->pdata->info[i].dev_name) == 0)) {
            sprintf(orientation,"-%d",pcdev->pdata->info[i].orientation);
            sprintf(fov,"_50_50");
        }
    }

    i=0;
    new_camera = pcdev->pdata->register_dev_new;
    while (strstr(new_camera->dev_name,"end")==NULL) {
        if (strcmp(dev_name(pcdev->icd->pdev), new_camera->dev_name) == 0) {
            sprintf(orientation,"-%d",new_camera->orientation);
            sprintf(fov,"_%d_%d",new_camera->fov_h,new_camera->fov_v);
        }
        new_camera++;
    }

    if (orientation[0] != '-') {
        RKCAMERA_TR("%s: %s is not registered in rk29_camera_platform_data, orientation apply default value",__FUNCTION__,dev_name(pcdev->icd->pdev));
        if (strstr(dev_name(pcdev->icd->pdev),"front"))
            strcat(cap->card,"-270");
        else
            strcat(cap->card,"-90");
    } else {
        strcat(cap->card,orientation);
    }

    strcat(cap->card,fov);                          /* ddl@rock-chips.com: v0.3.f */
    cap->version = RK_CAM_VERSION_CODE;
    cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

    return 0;
}
static int rk_camera_suspend(struct soc_camera_device *icd, pm_message_t state)
{
    struct soc_camera_host *ici =
                    to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd;
    int ret = 0;

	mutex_lock(&camera_lock);
	if ((pcdev->icd == icd) && (icd->ops->suspend)) {
		rk_camera_s_stream(icd, 0);
		sd = soc_camera_to_subdev(icd);
		v4l2_subdev_call(sd, video, s_stream, 0);
		ret = icd->ops->suspend(icd, state);

		pcdev->reginfo_suspend.cifCtrl = read_cif_reg(pcdev->base,CIF_CIF_CTRL);
		pcdev->reginfo_suspend.cifCrop = read_cif_reg(pcdev->base,CIF_CIF_CROP);
		pcdev->reginfo_suspend.cifFs = read_cif_reg(pcdev->base,CIF_CIF_SET_SIZE);
		pcdev->reginfo_suspend.cifIntEn = read_cif_reg(pcdev->base,CIF_CIF_INTEN);
		pcdev->reginfo_suspend.cifFmt= read_cif_reg(pcdev->base,CIF_CIF_FOR);
		pcdev->reginfo_suspend.cifVirWidth = read_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH);
		pcdev->reginfo_suspend.cifScale= read_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL);
		
		pcdev->reginfo_suspend.Inval = Reg_Validate;
		rk_camera_deactivate(pcdev);

		RK30_CAM_DEBUG_TRACE("%s Enter Success...\n", __FUNCTION__);
	} else {
		RK30_CAM_DEBUG_TRACE("%s icd has been deattach, don't need enter suspend\n", __FUNCTION__);
	}
	mutex_unlock(&camera_lock);
    return ret;
}

static int rk_camera_resume(struct soc_camera_device *icd)
{
    struct soc_camera_host *ici =
                    to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd;
    int ret = 0;

	mutex_lock(&camera_lock);
	if ((pcdev->icd == icd) && (icd->ops->resume)) {
		if (pcdev->reginfo_suspend.Inval == Reg_Validate) {
			rk_camera_activate(pcdev, icd);
			write_cif_reg(pcdev->base,CIF_CIF_CTRL, pcdev->reginfo_suspend.cifCtrl&~ENABLE_CAPTURE);
			write_cif_reg(pcdev->base,CIF_CIF_INTEN, pcdev->reginfo_suspend.cifIntEn);
			write_cif_reg(pcdev->base,CIF_CIF_CROP, pcdev->reginfo_suspend.cifCrop);
			write_cif_reg(pcdev->base,CIF_CIF_SET_SIZE, pcdev->reginfo_suspend.cifFs);
			write_cif_reg(pcdev->base,CIF_CIF_FOR, pcdev->reginfo_suspend.cifFmt);
			write_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH,pcdev->reginfo_suspend.cifVirWidth);
			write_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL, pcdev->reginfo_suspend.cifScale);
			rk_videobuf_capture(pcdev->active0,pcdev,0);
			rk_videobuf_capture(pcdev->active0,pcdev,1);
			rk_camera_s_stream(icd, 1);
			pcdev->reginfo_suspend.Inval = Reg_Invalidate;
		} else {
			RK30_CAM_DEBUG_TRACE("Resume fail, vip register recored is invalidate!!\n");
			goto rk_camera_resume_end;
		}

		ret = icd->ops->resume(icd);
		sd = soc_camera_to_subdev(icd);
		v4l2_subdev_call(sd, video, s_stream, 1);

		RK30_CAM_DEBUG_TRACE("%s Enter success\n",__FUNCTION__);
	} else {
		RK30_CAM_DEBUG_TRACE("%s icd has been deattach, don't need enter resume\n", __FUNCTION__);
	}

rk_camera_resume_end:
	mutex_unlock(&camera_lock);
    return ret;
}

static void rk_camera_reinit_work(struct work_struct *work)
{
    struct v4l2_subdev *sd;
	struct rk_camera_work *camera_work = container_of(work, struct rk_camera_work, work);
	struct rk_camera_dev *pcdev = camera_work->pcdev;
    struct soc_camera_link *tmp_soc_cam_link;
    int index = 0;
	unsigned long flags = 0;
    if(pcdev->icd == NULL)
        return;
    sd = soc_camera_to_subdev(pcdev->icd);
    tmp_soc_cam_link = to_soc_camera_link(pcdev->icd);
	//dump regs
	{
		RK30_CAM_DEBUG_TRACE("CIF_CIF_CTRL = 0x%x\n",read_cif_reg(pcdev->base,CIF_CIF_CTRL));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_INTEN = 0x%x\n",read_cif_reg(pcdev->base,CIF_CIF_INTEN));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_INTSTAT = 0x%x\n",read_cif_reg(pcdev->base,CIF_CIF_INTSTAT));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_FOR = 0x%x\n",read_cif_reg(pcdev->base,CIF_CIF_FOR));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_CROP = 0x%x\n",read_cif_reg(pcdev->base,CIF_CIF_CROP));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_SET_SIZE = 0x%x\n",read_cif_reg(pcdev->base,CIF_CIF_SET_SIZE));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_SCL_CTRL = 0x%x\n",read_cif_reg(pcdev->base,CIF_CIF_SCL_CTRL));
		RK30_CAM_DEBUG_TRACE("CRU_PCLK_REG30 = 0X%x\n",read_cru_reg(CRU_PCLK_REG30));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_LAST_LINE = 0X%x\n",read_cif_reg(pcdev->base,CIF_CIF_LAST_LINE));
		
		RK30_CAM_DEBUG_TRACE("CIF_CIF_LAST_PIX = 0X%x\n",read_cif_reg(pcdev->base,CIF_CIF_LAST_PIX));
		RK30_CAM_DEBUG_TRACE("CIF_CIF_VIR_LINE_WIDTH = 0X%x\n",read_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH));
    	RK30_CAM_DEBUG_TRACE("CIF_CIF_LINE_NUM_ADDR = 0X%x\n",read_cif_reg(pcdev->base,CIF_CIF_LINE_NUM_ADDR));
    	RK30_CAM_DEBUG_TRACE("CIF_CIF_FRM0_ADDR_Y = 0X%x\n",read_cif_reg(pcdev->base,CIF_CIF_FRM0_ADDR_Y));
    	RK30_CAM_DEBUG_TRACE("CIF_CIF_FRM0_ADDR_UV = 0X%x\n",read_cif_reg(pcdev->base,CIF_CIF_FRM0_ADDR_UV));
    	RK30_CAM_DEBUG_TRACE("CIF_CIF_FRAME_STATUS = 0X%x\n",read_cif_reg(pcdev->base,CIF_CIF_FRAME_STATUS));
	}
    return;
    pcdev->stop_cif = true;
	write_cif_reg(pcdev->base,CIF_CIF_CTRL, (read_cif_reg(pcdev->base,CIF_CIF_CTRL)&(~ENABLE_CAPTURE)));
	RK30_CAM_DEBUG_TRACE("the reinit times = %d\n",pcdev->reinit_times);
   if(pcdev->video_vq && pcdev->video_vq->irqlock){
   	spin_lock_irqsave(pcdev->video_vq->irqlock, flags);
    	for (index = 0; index < VIDEO_MAX_FRAME; index++) {
    		if (NULL == pcdev->video_vq->bufs[index])
    			continue;
            
    		if (pcdev->video_vq->bufs[index]->state == VIDEOBUF_QUEUED) 
            {
    			list_del_init(&pcdev->video_vq->bufs[index]->queue);
    			pcdev->video_vq->bufs[index]->state = VIDEOBUF_NEEDS_INIT;
    			wake_up_all(&pcdev->video_vq->bufs[index]->done);
                printk("wake up video buffer index = %d  !!!\n",index);
    		}
    	}
    	spin_unlock_irqrestore(pcdev->video_vq->irqlock, flags); 
    }else{
    RK30_CAM_DEBUG_TRACE("video queue has somthing wrong !!\n");
    }

	RK30_CAM_DEBUG_TRACE("the %d reinit times ,wake up video buffers!\n ",pcdev->reinit_times);
}
static enum hrtimer_restart rk_camera_fps_func(struct hrtimer *timer)
{
    struct rk_camera_frmivalenum *fival_nxt=NULL,*fival_pre=NULL, *fival_rec=NULL;
	struct rk_camera_timer *fps_timer = container_of(timer, struct rk_camera_timer, timer);
	struct rk_camera_dev *pcdev = fps_timer->pcdev;
    int rec_flag,i;
   // static unsigned int last_fps = 0;
    struct soc_camera_link *tmp_soc_cam_link;
    tmp_soc_cam_link = to_soc_camera_link(pcdev->icd);

	RK30_CAM_DEBUG_TRACE("rk_camera_fps_func fps:%d\n",(pcdev->fps - pcdev->last_fps)/3);
	if ((pcdev->fps < 1) || (pcdev->last_fps == pcdev->fps)) {
		RK30_CAM_DEBUG_TRACE("Camera host haven't recevie data from sensor,Reinit sensor delay,last fps = %d,pcdev->fps = %d!\n",pcdev->last_fps,pcdev->fps);
		pcdev->camera_reinit_work.pcdev = pcdev;
		//INIT_WORK(&(pcdev->camera_reinit_work.work), rk_camera_reinit_work);
        pcdev->reinit_times++;
		queue_work(pcdev->camera_wq,&(pcdev->camera_reinit_work.work));
	} else if(!pcdev->timer_get_fps) {
	    pcdev->timer_get_fps = true;
	    for (i=0; i<2; i++) {
            if (pcdev->icd == pcdev->icd_frmival[i].icd) {
                fival_nxt = pcdev->icd_frmival[i].fival_list;                
            }
        }
        
        rec_flag = 0;
        fival_pre = fival_nxt;
        while (fival_nxt != NULL) {

            RK30_CAM_DEBUG_TRACE("%s %c%c%c%c %dx%d framerate : %d/%d\n", dev_name(&pcdev->icd->dev),
                fival_nxt->fival.pixel_format & 0xFF, (fival_nxt->fival.pixel_format >> 8) & 0xFF,
			    (fival_nxt->fival.pixel_format >> 16) & 0xFF, (fival_nxt->fival.pixel_format >> 24),
			    fival_nxt->fival.width, fival_nxt->fival.height, fival_nxt->fival.discrete.denominator,
			    fival_nxt->fival.discrete.numerator);
            
            if (((fival_nxt->fival.pixel_format == pcdev->pixfmt) 
                && (fival_nxt->fival.height == pcdev->icd->user_height)
                && (fival_nxt->fival.width == pcdev->icd->user_width))
                || (fival_nxt->fival.discrete.denominator == 0)) {

                if (fival_nxt->fival.discrete.denominator == 0) {
                    fival_nxt->fival.index = 0;
                    fival_nxt->fival.width = pcdev->icd->user_width;
                    fival_nxt->fival.height= pcdev->icd->user_height;
                    fival_nxt->fival.pixel_format = pcdev->pixfmt;
                    fival_nxt->fival.discrete.denominator = pcdev->frame_interval;
                    fival_nxt->fival.reserved[1] = (pcdev->icd_width<<16)
                                                    |(pcdev->icd_height);
                    fival_nxt->fival.discrete.numerator = 1000000;
                    fival_nxt->fival.type = V4L2_FRMIVAL_TYPE_DISCRETE;
                }
                rec_flag = 1;
                fival_rec = fival_nxt;
            }
            fival_pre = fival_nxt;
            fival_nxt = fival_nxt->nxt;            
        }

        if ((rec_flag == 0) && fival_pre) {
            fival_pre->nxt = kzalloc(sizeof(struct rk_camera_frmivalenum), GFP_ATOMIC);
            if (fival_pre->nxt != NULL) {
                fival_pre->nxt->fival.index = fival_pre->fival.index++;
                fival_pre->nxt->fival.width = pcdev->icd->user_width;
                fival_pre->nxt->fival.height= pcdev->icd->user_height;
                fival_pre->nxt->fival.pixel_format = pcdev->pixfmt;

                fival_pre->nxt->fival.discrete.denominator = pcdev->frame_interval;
                fival_pre->nxt->fival.reserved[1] = (pcdev->icd_width<<16)
                                                    |(pcdev->icd_height);
                fival_pre->nxt->fival.discrete.numerator = 1000000;
                fival_pre->nxt->fival.type = V4L2_FRMIVAL_TYPE_DISCRETE;
                rec_flag = 1;
                fival_rec = fival_pre->nxt;
            }
        }
	}
    pcdev->last_fps = pcdev->fps ;
    pcdev->fps_timer.timer.node.expires= ktime_add_us(pcdev->fps_timer.timer.node.expires, ktime_to_us(ktime_set(3, 0)));
    pcdev->fps_timer.timer._softexpires= ktime_add_us(pcdev->fps_timer.timer._softexpires, ktime_to_us(ktime_set(3, 0)));
    //return HRTIMER_NORESTART;
    if(pcdev->reinit_times >=2)
        return HRTIMER_NORESTART;
    else
        return HRTIMER_RESTART;
}
static int rk_camera_s_stream(struct soc_camera_device *icd, int enable)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct rk_camera_dev *pcdev = ici->priv;
	int cif_ctrl_val;
	int ret;
	unsigned long flags;

	WARN_ON(pcdev->icd != icd);

	cif_ctrl_val = read_cif_reg(pcdev->base,CIF_CIF_CTRL);
	if (enable) {
		pcdev->fps = 0;
		pcdev->last_fps = 0;
		pcdev->frame_interval = 0;
		hrtimer_cancel(&(pcdev->fps_timer.timer));
		pcdev->fps_timer.pcdev = pcdev;
		pcdev->timer_get_fps = false;
		pcdev->reinit_times  = 0;
		pcdev->stop_cif = false;
		pcdev->cif_stopped = false;
		//	hrtimer_start(&(pcdev->fps_timer.timer),ktime_set(3, 0),HRTIMER_MODE_REL);
		cif_ctrl_val |= ENABLE_CAPTURE;
		write_cif_reg(pcdev->base,CIF_CIF_CTRL, cif_ctrl_val);
		hrtimer_start(&(pcdev->fps_timer.timer),ktime_set(3, 0),HRTIMER_MODE_REL);
		pcdev->fps_timer.istarted = true;
	} else {
	    //cancel timer before stop cif
		ret = hrtimer_cancel(&pcdev->fps_timer.timer);
        pcdev->fps_timer.istarted = false;
        flush_work(&(pcdev->camera_reinit_work.work));
        cif_ctrl_val &= ~ENABLE_CAPTURE;
		spin_lock_irqsave(&pcdev->lock, flags);
		//	write_cif_reg(pcdev->base,CIF_CIF_CTRL, cif_ctrl_val);
        pcdev->stop_cif = true;
    	spin_unlock_irqrestore(&pcdev->lock, flags);
#if CONFIG_CIF_STOP_SYNC
				init_waitqueue_head(&pcdev->cif_stop_done);
				if (wait_event_timeout(pcdev->cif_stop_done, pcdev->cif_stopped, msecs_to_jiffies(1000)) == 0) {
					RKCAMERA_DG("%s:%d, wait cif stop timeout!",__func__,__LINE__);
				}
#endif
		//mdelay(35);
		flush_workqueue((pcdev->camera_wq));
		RK30_CAM_DEBUG_TRACE("STREAM_OFF cancel timer and flush work:0x%x \n", ret);
	}
    //must be reinit,or will be somthing wrong in irq process.
    if(enable == false) {
        pcdev->active0 = NULL;
		pcdev->active1 = NULL;
        INIT_LIST_HEAD(&pcdev->capture);
	}
	return 0;
}
int rk_camera_enum_frameintervals(struct soc_camera_device *icd, struct v4l2_frmivalenum *fival)
{
    struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
    struct rk_camera_dev *pcdev = ici->priv;
    struct rk_camera_frmivalenum *fival_list = NULL;
    struct v4l2_frmivalenum *fival_head = NULL;
    int i,ret = 0,index;
    
    index = fival->index & 0x00ffffff;
    if ((fival->index & 0xff000000) == 0xff000000) {   /* ddl@rock-chips.com: detect framerate */ 
        for (i=0; i<2; i++) {
            if (pcdev->icd_frmival[i].icd == icd) {
                fival_list = pcdev->icd_frmival[i].fival_list;            
            }
        }
        
        if (fival_list != NULL) {
            i = 0;
            while (fival_list != NULL) {
                if ((fival->pixel_format == fival_list->fival.pixel_format)
                    && (fival->height == fival_list->fival.height) 
                    && (fival->width == fival_list->fival.width)) {
                    if (i == index)
                        break;
                    i++;
                }                
                fival_list = fival_list->nxt;                
            }
            
            if ((i==index) && (fival_list != NULL)) {
                memcpy(fival, &fival_list->fival, sizeof(struct v4l2_frmivalenum));
            } else {
                ret = -EINVAL;
            }
        } else {
            RK30_CAM_DEBUG_TRACE("%s: fival_list is NULL\n",__FUNCTION__);
            ret = -EINVAL;
        }
    }  else {  

        for (i=0; i<RK_CAM_NUM; i++) {
            if (pcdev->pdata->info[i].dev_name && (strcmp(dev_name(pcdev->icd->pdev),pcdev->pdata->info[i].dev_name) == 0)) {
                fival_head = pcdev->pdata->info[i].fival;
            }
        }
        
        if (fival_head == NULL) {
            RK30_CAM_DEBUG_TRACE("%s: %s is not registered in rk_camera_platform_data!!",__FUNCTION__,dev_name(pcdev->icd->pdev));
            ret = -EINVAL;
            goto rk_camera_enum_frameintervals_end;
        }
        
        i = 0;
        while (fival_head->width && fival_head->height) {
            if ((fival->pixel_format == fival_head->pixel_format)
                && (fival->height == fival_head->height) 
                && (fival->width == fival_head->width)) {
                if (i == index) {
                    break;
                }
                i++;
            }
            fival_head++;  
        }

        if ((i == index) && (fival->height == fival_head->height) && (fival->width == fival_head->width)) {
            memcpy(fival, fival_head, sizeof(struct v4l2_frmivalenum));
            RK30_CAM_DEBUG_TRACE("%s %dx%d@%c%c%c%c framerate : %d/%d\n", dev_name(pcdev->icd->pdev),
                fival->width, fival->height,
                fival->pixel_format & 0xFF, (fival->pixel_format >> 8) & 0xFF,
			    (fival->pixel_format >> 16) & 0xFF, (fival->pixel_format >> 24),
			     fival->discrete.denominator,fival->discrete.numerator);			    
        } else {
            if (index == 0)
                RK30_CAM_DEBUG_TRACE("%s have not catch %d%d@%c%c%c%c index(%d) framerate\n",dev_name(pcdev->icd->pdev),
                    fival->width,fival->height, 
                    fival->pixel_format & 0xFF, (fival->pixel_format >> 8) & 0xFF,
    			    (fival->pixel_format >> 16) & 0xFF, (fival->pixel_format >> 24),
    			    index);
            else
                RK30_CAM_DEBUG_TRACE("%s have not catch %d%d@%c%c%c%c index(%d) framerate\n",dev_name(pcdev->icd->pdev),
                    fival->width,fival->height, 
                    fival->pixel_format & 0xFF, (fival->pixel_format >> 8) & 0xFF,
    			    (fival->pixel_format >> 16) & 0xFF, (fival->pixel_format >> 24),
    			    index);
            ret = -EINVAL;
        }
    }
rk_camera_enum_frameintervals_end:
    return ret;
}

#ifdef CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_ON
static int rk_camera_set_digit_zoom(struct soc_camera_device *icd,
								const struct v4l2_queryctrl *qctrl, int zoom_rate)
{
	struct v4l2_crop a;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct rk_camera_dev *pcdev = ici->priv;
#if CIF_DO_CROP    
	unsigned long tmp_cifctrl; 
#endif	

	//change the crop and scale parameters
	
#if CIF_DO_CROP
	a.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//a.c.width = pcdev->host_width*100/zoom_rate;
	a.c.width = pcdev->host_width*100/zoom_rate;
	a.c.width &= ~CROP_ALIGN_BYTES;    
	a.c.height = pcdev->host_height*100/zoom_rate;
	a.c.height &= ~CROP_ALIGN_BYTES;
	a.c.left = (((pcdev->host_width - a.c.width)>>1)+pcdev->host_left)&(~0x01);
	a.c.top = (((pcdev->host_height - a.c.height)>>1)+pcdev->host_top)&(~0x01);
	pcdev->stop_cif = true;
	tmp_cifctrl = read_cif_reg(pcdev->base,CIF_CIF_CTRL);
	write_cif_reg(pcdev->base,CIF_CIF_CTRL, (tmp_cifctrl & ~ENABLE_CAPTURE));
	hrtimer_cancel(&(pcdev->fps_timer.timer));
	flush_workqueue((pcdev->camera_wq));
	down(&pcdev->zoominfo.sem);
	pcdev->zoominfo.a.c.left = 0;
	pcdev->zoominfo.a.c.top = 0;
	pcdev->zoominfo.a.c.width = a.c.width;
	pcdev->zoominfo.a.c.height = a.c.height;
	pcdev->zoominfo.vir_width = pcdev->zoominfo.a.c.width;
	pcdev->zoominfo.vir_height = pcdev->zoominfo.a.c.height;
	pcdev->frame_inval = 1;
	write_cif_reg(pcdev->base,CIF_CIF_CROP, (a.c.left + (a.c.top<<16)));
	write_cif_reg(pcdev->base,CIF_CIF_SET_SIZE, ((a.c.width ) + (a.c.height<<16)));
	write_cif_reg(pcdev->base,CIF_CIF_VIR_LINE_WIDTH, a.c.width);
	write_cif_reg(pcdev->base,CIF_CIF_FRAME_STATUS,  0x00000002);//frame1 has been ready to receive data,frame 2 is not used
	if(pcdev->active0)
		rk_videobuf_capture(pcdev->active0,pcdev,0);
	if(pcdev->active1)
		rk_videobuf_capture(pcdev->active1,pcdev,1);
	if(tmp_cifctrl & ENABLE_CAPTURE)
		write_cif_reg(pcdev->base,CIF_CIF_CTRL, (tmp_cifctrl | ENABLE_CAPTURE));
	up(&pcdev->zoominfo.sem);
	pcdev->stop_cif = false;
	hrtimer_start(&(pcdev->fps_timer.timer),ktime_set(3, 0),HRTIMER_MODE_REL);
	RK30_CAM_DEBUG_TRACE("%s..zoom_rate:%d (%dx%d at (%d,%d)-> %dx%d)\n",__FUNCTION__, zoom_rate,a.c.width, a.c.height, a.c.left, a.c.top, icd->user_width, icd->user_height );
#else
	a.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a.c.width = pcdev->host_width*100/zoom_rate;
	a.c.width &= ~CROP_ALIGN_BYTES;    
	a.c.height = pcdev->host_height*100/zoom_rate;
	a.c.height &= ~CROP_ALIGN_BYTES;
	a.c.left = (pcdev->host_width - a.c.width)>>1;
	a.c.top = (pcdev->host_height - a.c.height)>>1;
	down(&pcdev->zoominfo.sem);
	pcdev->zoominfo.a.c.height = a.c.height;
	pcdev->zoominfo.a.c.width = a.c.width;
	pcdev->zoominfo.a.c.top = a.c.top;
	pcdev->zoominfo.a.c.left = a.c.left;
	pcdev->zoominfo.vir_width = pcdev->host_width;
	pcdev->zoominfo.vir_height= pcdev->host_height;
	up(&pcdev->zoominfo.sem);
	RK30_CAM_DEBUG_TRACE("%s..zoom_rate:%d (%dx%d at (%d,%d)-> %dx%d)\n",__FUNCTION__, zoom_rate,a.c.width, a.c.height, a.c.left, a.c.top, icd->user_width, icd->user_height );
#endif	

	return 0;
}
#endif
static inline struct v4l2_queryctrl const *rk_camera_soc_camera_find_qctrl(
	struct soc_camera_host_ops *ops, int id)
{
	int i;

	for (i = 0; i < ops->num_controls; i++)
		if (ops->controls[i].id == id)
			return &ops->controls[i];

	return NULL;
}


static int rk_camera_set_ctrl(struct soc_camera_device *icd,
								struct v4l2_control *sctrl)
{

	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	const struct v4l2_queryctrl *qctrl;
#ifdef CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_ON    
    struct rk_camera_dev *pcdev = ici->priv;
#endif
    int ret = 0;

	qctrl = rk_camera_soc_camera_find_qctrl(ici->ops, sctrl->id);
	if (!qctrl) {
		ret = -ENOIOCTLCMD;
        goto rk_camera_set_ctrl_end;
	}
	
    if ((sctrl->value < qctrl->minimum) || (sctrl->value > qctrl->maximum)){
		ret = -EINVAL;
        goto rk_camera_set_ctrl_end;
	}
	
	switch (sctrl->id)
	{
	#ifdef CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_ON
		case V4L2_CID_ZOOM_ABSOLUTE:
		{
			
            ret = rk_camera_set_digit_zoom(icd, qctrl, sctrl->value);
			if (ret == 0) {
				pcdev->zoominfo.zoom_rate = sctrl->value;
            } else { 
                goto rk_camera_set_ctrl_end;
            }
			break;
		}
    #endif

        case V4L2_CID_HDR:
        {            
            if (pcdev->hdr_info.en != sctrl->value) {
                pcdev->hdr_info.en = sctrl->value;
                if (sctrl->value) {
					struct device *control = to_soc_camera_control(pcdev->icd);
					struct v4l2_subdev *sd=dev_get_drvdata(control);

                    printk("hdr on\n");
                    pcdev->hdr_info.frame[0].code = RK_VIDEOBUF_HDR_EXPOSURE_MINUS_1;
                    pcdev->hdr_info.frame[0].set_ts = 0;
                    pcdev->hdr_info.frame[0].get_ts = 0;
                    pcdev->hdr_info.frame[1].code = RK_VIDEOBUF_HDR_EXPOSURE_NORMAL;
                    pcdev->hdr_info.frame[1].set_ts = 0;
                    pcdev->hdr_info.frame[1].get_ts = 0;
                    pcdev->hdr_info.frame[2].code = RK_VIDEOBUF_HDR_EXPOSURE_PLUS_1;
                    pcdev->hdr_info.frame[2].set_ts = 0;
                    pcdev->hdr_info.frame[2].get_ts = 0;
                    v4l2_subdev_call(sd, core, ioctl, RK29_CAM_SUBDEV_HDR_EXPOSURE,RK_VIDEOBUF_HDR_EXPOSURE_NORMAL);
                }
            }
            break;
        }
    
		default:
			ret = -ENOIOCTLCMD;
			break;
	}
rk_camera_set_ctrl_end:
	return ret;
}

int rk_camera_enum_fsizes(struct soc_camera_device *icd, struct v4l2_frmsizeenum *fsize)
{
    struct device *control = to_soc_camera_control(icd);
    struct v4l2_subdev *sd;

    sd = dev_get_drvdata(control);
    return v4l2_subdev_call(sd, video, enum_framesizes, fsize);

}

static struct soc_camera_host_ops rk_soc_camera_host_ops =
{
    .owner		= THIS_MODULE,
    .add		= rk_camera_add_device,
    .remove		= rk_camera_remove_device,
    .suspend	= rk_camera_suspend,
    .resume		= rk_camera_resume,
    .enum_frameinervals = rk_camera_enum_frameintervals,
    .enum_fsizes = rk_camera_enum_fsizes,
    .set_crop	= rk_camera_set_crop,
    .get_formats	= rk_camera_get_formats, 
    .put_formats	= rk_camera_put_formats,
    .set_fmt	= rk_camera_set_fmt,
    .try_fmt	= rk_camera_try_fmt,
    .init_videobuf	= rk_camera_init_videobuf,
    .reqbufs	= rk_camera_reqbufs,
    .poll		= rk_camera_poll,
    .querycap	= rk_camera_querycap,
    .set_bus_param	= rk_camera_set_bus_param,
    .s_stream = rk_camera_s_stream,   /* ddl@rock-chips.com : Add stream control for host */
    .set_ctrl = rk_camera_set_ctrl,
    .controls = rk_camera_controls,
    .num_controls = ARRAY_SIZE(rk_camera_controls)
};

static void rk_camera_cif_iomux(int cif_index)
{
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
    switch(cif_index){
        case 0:
			iomux_set(CIF0_CLKOUT);
            write_grf_reg(GRF_IO_CON3, (CIF_DRIVER_STRENGTH_MASK|CIF_DRIVER_STRENGTH_4MA));
            write_grf_reg(GRF_IO_CON4, (CIF_CLKOUT_AMP_MASK|CIF_CLKOUT_AMP_1V8));
            #if (CONFIG_CAMERA_INPUT_FMT_SUPPORT & (RK_CAM_INPUT_FMT_RAW10|RK_CAM_INPUT_FMT_RAW12))
	        iomux_set(CIF0_D0);
	        iomux_set(CIF0_D1);
            #endif
            #if (CONFIG_CAMERA_INPUT_FMT_SUPPORT & RK_CAM_INPUT_FMT_RAW12)
	        iomux_set(CIF0_D10);
	        iomux_set(CIF0_D11);
            RK30_CAM_DEBUG_TRACE("%s(%d): WARNING: Cif 0 is configurated that support RAW 12bit, so I2C3 is invalidate!!\n",__FUNCTION__,__LINE__);
            #endif
            
            break;
        default:
            RK30_CAM_DEBUG_TRACE("%s(%d): Cif index(%d) is invalidate!!!\n",__FUNCTION__,__LINE__, cif_index);
            break;
    }
#elif defined(CONFIG_ARCH_RK30)
    switch(cif_index){
        case 0:
            rk30_mux_api_set(GPIO1B3_CIF0CLKOUT_NAME, GPIO1B_CIF0_CLKOUT);
            #if (CONFIG_CAMERA_INPUT_FMT_SUPPORT & (RK_CAM_INPUT_FMT_RAW10|RK_CAM_INPUT_FMT_RAW12))
	        rk30_mux_api_set(GPIO1B4_CIF0DATA0_NAME, GPIO1B_CIF0_DATA0);
	        rk30_mux_api_set(GPIO1B5_CIF0DATA1_NAME, GPIO1B_CIF0_DATA1);
            #endif
            #if (CONFIG_CAMERA_INPUT_FMT_SUPPORT & RK_CAM_INPUT_FMT_RAW12)
	        rk30_mux_api_set(GPIO1B6_CIFDATA10_NAME, GPIO1B_CIF_DATA10);
	        rk30_mux_api_set(GPIO1B7_CIFDATA11_NAME, GPIO1B_CIF_DATA11);
            #endif
            break;
        case 1:
            rk30_mux_api_set(GPIO1C0_CIF1DATA2_RMIICLKOUT_RMIICLKIN_NAME,GPIO1C_CIF1_DATA2);
            rk30_mux_api_set(GPIO1C1_CIFDATA3_RMIITXEN_NAME,GPIO1C_CIF_DATA3);
            rk30_mux_api_set(GPIO1C2_CIF1DATA4_RMIITXD1_NAME,GPIO1C_CIF1_DATA4);
            rk30_mux_api_set(GPIO1C3_CIFDATA5_RMIITXD0_NAME,GPIO1C_CIF_DATA5);
            rk30_mux_api_set(GPIO1C4_CIFDATA6_RMIIRXERR_NAME,GPIO1C_CIF_DATA6);
            rk30_mux_api_set(GPIO1C5_CIFDATA7_RMIICRSDVALID_NAME,GPIO1C_CIF_DATA7);
            rk30_mux_api_set(GPIO1C6_CIFDATA8_RMIIRXD1_NAME,GPIO1C_CIF_DATA8);
            rk30_mux_api_set(GPIO1C7_CIFDATA9_RMIIRXD0_NAME,GPIO1C_CIF_DATA9);
            
            rk30_mux_api_set(GPIO1D0_CIF1VSYNC_MIIMD_NAME,GPIO1D_CIF1_VSYNC);
            rk30_mux_api_set(GPIO1D1_CIF1HREF_MIIMDCLK_NAME,GPIO1D_CIF1_HREF);
            rk30_mux_api_set(GPIO1D2_CIF1CLKIN_NAME,GPIO1D_CIF1_CLKIN);
            rk30_mux_api_set(GPIO1D3_CIF1DATA0_NAME,GPIO1D_CIF1_DATA0);
            rk30_mux_api_set(GPIO1D4_CIF1DATA1_NAME,GPIO1D_CIF1_DATA1);
            rk30_mux_api_set(GPIO1D5_CIF1DATA10_NAME,GPIO1D_CIF1_DATA10);
            rk30_mux_api_set(GPIO1D6_CIF1DATA11_NAME,GPIO1D_CIF1_DATA11);
            rk30_mux_api_set(GPIO1D7_CIF1CLKOUT_NAME,GPIO1D_CIF1_CLKOUT);
            break;
        default:
            RK30_CAM_DEBUG_TRACE("%s(%d): Cif index(%d) is invalidate!!!\n",__FUNCTION__,__LINE__, cif_index);
            break;
        }
#endif
                
            
}
static int rk_camera_probe(struct platform_device *pdev)
{
    struct rk_camera_dev *pcdev;
    struct resource *res;
    struct rk_camera_frmivalenum *fival_list,*fival_nxt;
    struct rk29camera_mem_res *meminfo_ptr,*meminfo_ptrr;
    int irq,i;
    int err = 0;
    struct rk_cif_clk *clk=NULL;

    printk("%s version: v%d.%d.%d  Zoom by %s\n",RK29_CAM_DRV_NAME,(RK_CAM_VERSION_CODE&0xff0000)>>16,
        (RK_CAM_VERSION_CODE&0xff00)>>8,RK_CAM_VERSION_CODE&0xff,CAMERA_SCALE_CROP_MACHINE);    

    if ((pdev->id == RK_CAM_PLATFORM_DEV_ID_1) && (RK_SUPPORT_CIF1 == 0)) {
        RK30_CAM_DEBUG_TRACE("%s(%d): This chip is not support CIF1!!\n",__FUNCTION__,__LINE__);
        BUG();
    }

    if ((pdev->id == RK_CAM_PLATFORM_DEV_ID_0) && (RK_SUPPORT_CIF0 == 0)) {
        RK30_CAM_DEBUG_TRACE("%s(%d): This chip is not support CIF0!!\n",__FUNCTION__,__LINE__);
        BUG();
    }
    
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    irq = platform_get_irq(pdev, 0);
    if (!res || irq < 0) {
        err = -ENODEV;
        goto exit;
    }
    pcdev = kzalloc(sizeof(*pcdev), GFP_KERNEL);
    if (!pcdev) {
        dev_err(&pdev->dev, "Could not allocate pcdev\n");
        err = -ENOMEM;
        goto exit_alloc;
    }

	pcdev->zoominfo.zoom_rate = 100;
	pcdev->hostid = pdev->id;
    /*config output clk*/ // must modify start
    if(IS_CIF0()){
        clk = &cif_clk[0];
        cif_clk[0].pd_cif = clk_get(NULL, "pd_cif0");
        cif_clk[0].aclk_cif = clk_get(NULL, "aclk_cif0");
        cif_clk[0].hclk_cif = clk_get(NULL, "hclk_cif0");
        cif_clk[0].cif_clk_in = clk_get(NULL, "cif0_in");
        cif_clk[0].cif_clk_out = clk_get(NULL, "cif0_out");
        spin_lock_init(&cif_clk[0].lock);
        cif_clk[0].on = false;
        rk_camera_cif_iomux(0);
    } else {
        clk = &cif_clk[1];
        cif_clk[1].pd_cif = clk_get(NULL, "pd_cif1");
        cif_clk[1].aclk_cif = clk_get(NULL, "aclk_cif1");
        cif_clk[1].hclk_cif = clk_get(NULL, "hclk_cif1");
        cif_clk[1].cif_clk_in = clk_get(NULL, "cif1_in");
        cif_clk[1].cif_clk_out = clk_get(NULL, "cif1_out");
        spin_lock_init(&cif_clk[1].lock);
        cif_clk[1].on = false;
        
        rk_camera_cif_iomux(1);
    }
    
    
    dev_set_drvdata(&pdev->dev, pcdev);
    pcdev->res = res;
    pcdev->pdata = pdev->dev.platform_data;             /* ddl@rock-chips.com : Request IO in init function */
	pcdev->pdata->sensor_mclk = rk_camera_mclk_ctrl;
	if (pcdev->pdata && pcdev->pdata->io_init) {
        pcdev->pdata->io_init();
    }
	#ifdef CONFIG_VIDEO_RK29_WORK_IPP
    meminfo_ptr = IS_CIF0()? (&pcdev->pdata->meminfo):(&pcdev->pdata->meminfo_cif1);
    meminfo_ptrr = IS_CIF0()? (&pcdev->pdata->meminfo_cif1):(&pcdev->pdata->meminfo);
    
    if (meminfo_ptr->vbase == NULL) {

        if ((meminfo_ptr->start == meminfo_ptrr->start)
            && (meminfo_ptr->size == meminfo_ptrr->size) && meminfo_ptrr->vbase) {

            meminfo_ptr->vbase = meminfo_ptrr->vbase;
        } else {        
        
            if (!request_mem_region(meminfo_ptr->start,meminfo_ptr->size,"rk29_vipmem")) {
                err = -EBUSY;
                RK30_CAM_DEBUG_TRACE("%s(%d): request_mem_region(start:0x%x size:0x%x) failed \n",__FUNCTION__,__LINE__, pcdev->pdata->meminfo.start,pcdev->pdata->meminfo.size);
                goto exit_ioremap_vipmem;
            }
            meminfo_ptr->vbase = pcdev->vipmem_virbase = ioremap_cached(meminfo_ptr->start,meminfo_ptr->size);
            if (pcdev->vipmem_virbase == NULL) {
                RK30_CAM_DEBUG_TRACE("%s(%d): ioremap of CIF internal memory(Ex:IPP process/raw process) failed\n",__FUNCTION__,__LINE__);
                err = -ENXIO;
                goto exit_ioremap_vipmem;
            }
        }
    }
    
    pcdev->vipmem_phybase = meminfo_ptr->start;
	pcdev->vipmem_size = meminfo_ptr->size;
    pcdev->vipmem_virbase = meminfo_ptr->vbase;
	#endif
    INIT_LIST_HEAD(&pcdev->capture);
    INIT_LIST_HEAD(&pcdev->camera_work_queue);
    spin_lock_init(&pcdev->lock);
    spin_lock_init(&pcdev->camera_work_lock);

    memset(&pcdev->cropinfo.c,0x00,sizeof(struct v4l2_rect));
    spin_lock_init(&pcdev->cropinfo.lock);
    sema_init(&pcdev->zoominfo.sem,1);

    /*
     * Request the regions.
     */
    if(res) {
        if (!request_mem_region(res->start, res->end - res->start + 1,
                                RK29_CAM_DRV_NAME)) {
            err = -EBUSY;
            goto exit_reqmem_vip;
        }
        pcdev->base = ioremap(res->start, res->end - res->start + 1);
        if (pcdev->base == NULL) {
            dev_err(pcdev->dev, "ioremap() of registers failed\n");
            err = -ENXIO;
            goto exit_ioremap_vip;
        }
    }
	
    pcdev->irq = irq;
    pcdev->dev = &pdev->dev;

    /* config buffer address */
    /* request irq */
    if(irq > 0){
        err = request_irq(pcdev->irq, rk_camera_irq, 0, RK29_CAM_DRV_NAME,
                          pcdev);
        if (err) {
            dev_err(pcdev->dev, "Camera interrupt register failed \n");
            goto exit_reqirq;
        }
   	}
   
//#ifdef CONFIG_VIDEO_RK29_WORK_IPP
    if(IS_CIF0()) {
    	pcdev->camera_wq = create_workqueue("rk_cam_wkque_cif0");
    } else {
    	pcdev->camera_wq = create_workqueue("rk_cam_wkque_cif1");
    }
    if (pcdev->camera_wq == NULL)
    	goto exit_free_irq;
//#endif

	pcdev->camera_reinit_work.pcdev = pcdev;
	INIT_WORK(&(pcdev->camera_reinit_work.work), rk_camera_reinit_work);

    for (i=0; i<2; i++) {
        pcdev->icd_frmival[i].icd = NULL;
        pcdev->icd_frmival[i].fival_list = kzalloc(sizeof(struct rk_camera_frmivalenum),GFP_KERNEL);
        
    }
    pcdev->soc_host.drv_name	= RK29_CAM_DRV_NAME;
    pcdev->soc_host.ops		= &rk_soc_camera_host_ops;
    pcdev->soc_host.priv		= pcdev;
    pcdev->soc_host.v4l2_dev.dev	= &pdev->dev;
    pcdev->soc_host.nr		= pdev->id;

    err = soc_camera_host_register(&pcdev->soc_host);
    if (err)
        goto exit_free_irq;
	pcdev->fps_timer.pcdev = pcdev;
	hrtimer_init(&(pcdev->fps_timer.timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pcdev->fps_timer.timer.function = rk_camera_fps_func;
    pcdev->icd_cb.sensor_cb = NULL;
#if ((defined(CONFIG_VIDEO_RK29_DIGITALZOOM_IPP_ON)) && (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_IPP))
    pcdev->icd_cb.scale_crop_cb = rk_camera_scale_crop_ipp;
#elif (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_ARM)
    pcdev->icd_cb.scale_crop_cb = rk_camera_scale_crop_arm;
#elif (CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_RGA)
    pcdev->icd_cb.scale_crop_cb = rk_camera_scale_crop_rga;	
#elif(CONFIG_CAMERA_SCALE_CROP_MACHINE == RK_CAM_SCALE_CROP_PP)
	pcdev->icd_cb.scale_crop_cb = rk_camera_scale_crop_pp; 
#endif
    RK30_CAM_DEBUG_TRACE("%s(%d) Exit  \n",__FUNCTION__,__LINE__);
    return 0;

exit_free_irq:
    
    for (i=0; i<2; i++) {
        fival_list = pcdev->icd_frmival[i].fival_list;
        fival_nxt = fival_list;
        while(fival_nxt != NULL) {
            fival_nxt = fival_list->nxt;
            kfree(fival_list);
            fival_list = fival_nxt;
        }
    }
    
    free_irq(pcdev->irq, pcdev);
	if (pcdev->camera_wq) {
		destroy_workqueue(pcdev->camera_wq);
		pcdev->camera_wq = NULL;
	}
exit_reqirq:
    iounmap(pcdev->base);
exit_ioremap_vip:
    release_mem_region(res->start, res->end - res->start + 1);
exit_ioremap_vipmem:
    if (pcdev->vipmem_virbase)
        iounmap(pcdev->vipmem_virbase);
    release_mem_region(pcdev->vipmem_phybase,pcdev->vipmem_size);
exit_reqmem_vip:
    if(pcdev->aclk_cif)
        pcdev->aclk_cif = NULL;
    if(pcdev->hclk_cif)
        pcdev->hclk_cif = NULL;
    if(pcdev->cif_clk_in)
        pcdev->cif_clk_in = NULL;
    if(pcdev->cif_clk_out)
        pcdev->cif_clk_out = NULL;

    kfree(pcdev);
exit_alloc:

exit:
    return err;
}

static int __devexit rk_camera_remove(struct platform_device *pdev)
{
    struct rk_camera_dev *pcdev = platform_get_drvdata(pdev);
    struct resource *res;
    struct rk_camera_frmivalenum *fival_list,*fival_nxt;
    struct rk29camera_mem_res *meminfo_ptr,*meminfo_ptrr;
    int i;
    
    free_irq(pcdev->irq, pcdev);

	if (pcdev->camera_wq) {
		destroy_workqueue(pcdev->camera_wq);
		pcdev->camera_wq = NULL;
	}

    for (i=0; i<2; i++) {
        fival_list = pcdev->icd_frmival[i].fival_list;
        fival_nxt = fival_list;
        while(fival_nxt != NULL) {
            fival_nxt = fival_list->nxt;
            kfree(fival_list);
            fival_list = fival_nxt;
        }
    }

    soc_camera_host_unregister(&pcdev->soc_host);

    meminfo_ptr = IS_CIF0()? (&pcdev->pdata->meminfo):(&pcdev->pdata->meminfo_cif1);
    meminfo_ptrr = IS_CIF0()? (&pcdev->pdata->meminfo_cif1):(&pcdev->pdata->meminfo);
    if (meminfo_ptr->vbase) {
        if (meminfo_ptr->vbase == meminfo_ptrr->vbase) {
            meminfo_ptr->vbase = NULL;
        } else {
            iounmap((void __iomem*)pcdev->vipmem_virbase);
            release_mem_region(pcdev->vipmem_phybase, pcdev->vipmem_size);
            meminfo_ptr->vbase = NULL;
        }
    }

    res = pcdev->res;
    iounmap((void __iomem*)pcdev->base);
    release_mem_region(res->start, res->end - res->start + 1);
    if (pcdev->pdata && pcdev->pdata->io_deinit) {         /* ddl@rock-chips.com : Free IO in deinit function */
        pcdev->pdata->io_deinit(0);
        pcdev->pdata->io_deinit(1);
    }

    kfree(pcdev);

    dev_info(&pdev->dev, "RK28 Camera driver unloaded\n");

    return 0;
}

static struct platform_driver rk_camera_driver =
{
    .driver 	= {
        .name	= RK29_CAM_DRV_NAME,
    },
    .probe		= rk_camera_probe,
    .remove		= __devexit_p(rk_camera_remove),
};

static int rk_camera_init_async(void *unused)
{
    platform_driver_register(&rk_camera_driver);
    return 0;
}

static int __devinit rk_camera_init(void)
{
    RK30_CAM_DEBUG_TRACE("%s..%s..%d  \n",__FUNCTION__,__FILE__,__LINE__);
    kthread_run(rk_camera_init_async, NULL, "rk_camera_init");
    return 0;
}

static void __exit rk_camera_exit(void)
{
    platform_driver_unregister(&rk_camera_driver);
}

device_initcall_sync(rk_camera_init);
module_exit(rk_camera_exit);

MODULE_DESCRIPTION("RKSoc Camera Host driver");
MODULE_AUTHOR("ddl <ddl@rock-chips>");
MODULE_LICENSE("GPL");
#endif
