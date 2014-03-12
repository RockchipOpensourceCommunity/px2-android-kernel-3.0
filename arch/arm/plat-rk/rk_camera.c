#include <plat/rk_camera.h> 

#ifndef PMEM_CAM_SIZE
#ifdef CONFIG_VIDEO_RK29 
/*---------------- Camera Sensor Fixed Macro Begin  ------------------------*/
// Below Macro is fixed, programer don't change it!!!!!!

#if defined (CONFIG_SENSOR_IIC_ADDR_0) && (CONFIG_SENSOR_IIC_ADDR_0 != 0x00)
#define PMEM_SENSOR_FULL_RESOLUTION_0  CONS(CONFIG_SENSOR_0,_FULL_RESOLUTION)

    #ifdef CONFIG_SENSOR_CIF_INDEX_0
    #define SENSOR_CIF_BUSID_0             CONS(RK_CAM_PLATFORM_DEV_ID_,CONFIG_SENSOR_CIF_INDEX_0)
    #else
    #define SENSOR_CIF_BUSID_0             RK29_CAM_PLATFORM_DEV_ID
    #endif
    
    #if !(PMEM_SENSOR_FULL_RESOLUTION_0)
    #undef PMEM_SENSOR_FULL_RESOLUTION_0
    #define PMEM_SENSOR_FULL_RESOLUTION_0  RK_CAM_SUPPORT_RESOLUTION
    #endif
    
    #if(SENSOR_CIF_BUSID_0 == RK_CAM_PLATFORM_DEV_ID_0)
    #define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 PMEM_SENSOR_FULL_RESOLUTION_0
    #define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 0
    #else
    #define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 PMEM_SENSOR_FULL_RESOLUTION_0
    #define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 0
    #endif
#else
#define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 0x00
#define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 0x00
#endif
 
#if defined (CONFIG_SENSOR_IIC_ADDR_1) && (CONFIG_SENSOR_IIC_ADDR_1 != 0x00)
#define PMEM_SENSOR_FULL_RESOLUTION_1  CONS(CONFIG_SENSOR_1,_FULL_RESOLUTION)

    #ifdef CONFIG_SENSOR_CIF_INDEX_1
    #define SENSOR_CIF_BUSID_1             CONS(RK_CAM_PLATFORM_DEV_ID_,CONFIG_SENSOR_CIF_INDEX_1)
    #else
    #define SENSOR_CIF_BUSID_1             RK29_CAM_PLATFORM_DEV_ID
    #endif
    
    #if !(PMEM_SENSOR_FULL_RESOLUTION_1)
    #undef PMEM_SENSOR_FULL_RESOLUTION_1
    #define PMEM_SENSOR_FULL_RESOLUTION_1  RK_CAM_SUPPORT_RESOLUTION
    #endif
    #if (SENSOR_CIF_BUSID_1 == RK_CAM_PLATFORM_DEV_ID_0)
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 < PMEM_SENSOR_FULL_RESOLUTION_1)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_0
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 PMEM_SENSOR_FULL_RESOLUTION_1
	   #endif
    #else
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 < PMEM_SENSOR_FULL_RESOLUTION_1)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_1
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 PMEM_SENSOR_FULL_RESOLUTION_1
	   #endif
    #endif
#endif

#if  defined (CONFIG_SENSOR_IIC_ADDR_01) && (CONFIG_SENSOR_IIC_ADDR_01!=0x00)
#define PMEM_SENSOR_FULL_RESOLUTION_01  CONS(CONFIG_SENSOR_01,_FULL_RESOLUTION)

    #ifdef CONFIG_SENSOR_CIF_INDEX_01
    #define SENSOR_CIF_BUSID_01             CONS(RK_CAM_PLATFORM_DEV_ID_,CONFIG_SENSOR_CIF_INDEX_01)
    #else
    #define SENSOR_CIF_BUSID_01             RK29_CAM_PLATFORM_DEV_ID
    #endif
    
    #if !(PMEM_SENSOR_FULL_RESOLUTION_01)
    #undef PMEM_SENSOR_FULL_RESOLUTION_01
    #define PMEM_SENSOR_FULL_RESOLUTION_01  RK_CAM_SUPPORT_RESOLUTION
    #endif
    #if (SENSOR_CIF_BUSID_01 == RK_CAM_PLATFORM_DEV_ID_0)
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 < PMEM_SENSOR_FULL_RESOLUTION_01)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_0
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 PMEM_SENSOR_FULL_RESOLUTION_01
	   #endif
    #else
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 < PMEM_SENSOR_FULL_RESOLUTION_01)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_1
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 PMEM_SENSOR_FULL_RESOLUTION_01
	   #endif
    #endif
#endif

#if defined (CONFIG_SENSOR_IIC_ADDR_02) && (CONFIG_SENSOR_IIC_ADDR_02!=0x00)
#define PMEM_SENSOR_FULL_RESOLUTION_02  CONS(CONFIG_SENSOR_02,_FULL_RESOLUTION)

    #ifdef CONFIG_SENSOR_CIF_INDEX_02
    #define SENSOR_CIF_BUSID_02             CONS(RK_CAM_PLATFORM_DEV_ID_,CONFIG_SENSOR_CIF_INDEX_02)
    #else
    #define SENSOR_CIF_BUSID_02             RK29_CAM_PLATFORM_DEV_ID
    #endif
    
    #if !(PMEM_SENSOR_FULL_RESOLUTION_02)
    #undef PMEM_SENSOR_FULL_RESOLUTION_02
    #define PMEM_SENSOR_FULL_RESOLUTION_02  RK_CAM_SUPPORT_RESOLUTION
    #endif
    #if (SENSOR_CIF_BUSID_02 == RK_CAM_PLATFORM_DEV_ID_0)
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 < PMEM_SENSOR_FULL_RESOLUTION_02)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_0
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 PMEM_SENSOR_FULL_RESOLUTION_02
	   #endif
    #else
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 < PMEM_SENSOR_FULL_RESOLUTION_02)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_1
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 PMEM_SENSOR_FULL_RESOLUTION_02
	   #endif
    #endif
#endif

#ifdef CONFIG_SENSOR_IIC_ADDR_11
#if (CONFIG_SENSOR_IIC_ADDR_11 != 0x00)
#define PMEM_SENSOR_FULL_RESOLUTION_11  CONS(CONFIG_SENSOR_11,_FULL_RESOLUTION)

    #ifdef CONFIG_SENSOR_CIF_INDEX_11
    #define SENSOR_CIF_BUSID_11             CONS(RK_CAM_PLATFORM_DEV_ID_,CONFIG_SENSOR_CIF_INDEX_11)
    #else
    #define SENSOR_CIF_BUSID_11             RK29_CAM_PLATFORM_DEV_ID
    #endif
    
    #if !(PMEM_SENSOR_FULL_RESOLUTION_11)
    #undef PMEM_SENSOR_FULL_RESOLUTION_11
    #define PMEM_SENSOR_FULL_RESOLUTION_11  RK_CAM_SUPPORT_RESOLUTION
    #endif
    #if (SENSOR_CIF_BUSID_11 == RK_CAM_PLATFORM_DEV_ID_0)
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 < PMEM_SENSOR_FULL_RESOLUTION_11)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_0
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 PMEM_SENSOR_FULL_RESOLUTION_11
	   #endif
    #else
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 < PMEM_SENSOR_FULL_RESOLUTION_11)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_1
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 PMEM_SENSOR_FULL_RESOLUTION_11
	   #endif
    #endif
#endif
#endif


#ifdef CONFIG_SENSOR_IIC_ADDR_12
#if (CONFIG_SENSOR_IIC_ADDR_12 != 0x00)
#define PMEM_SENSOR_FULL_RESOLUTION_12  CONS(CONFIG_SENSOR_12,_FULL_RESOLUTION)

    #ifdef CONFIG_SENSOR_CIF_INDEX_12
    #define SENSOR_CIF_BUSID_12             CONS(RK_CAM_PLATFORM_DEV_ID_,CONFIG_SENSOR_CIF_INDEX_12)
    #else
    #define SENSOR_CIF_BUSID_12             RK29_CAM_PLATFORM_DEV_ID
    #endif
    
    #if !(PMEM_SENSOR_FULL_RESOLUTION_12)
    #undef PMEM_SENSOR_FULL_RESOLUTION_12
    #define PMEM_SENSOR_FULL_RESOLUTION_12  RK_CAM_SUPPORT_RESOLUTION
    #endif
    #if (SENSOR_CIF_BUSID_12 == RK_CAM_PLATFORM_DEV_ID_0)
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 < PMEM_SENSOR_FULL_RESOLUTION_12)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_0
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_0 PMEM_SENSOR_FULL_RESOLUTION_12
	   #endif
    #else
	   #if (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 < PMEM_SENSOR_FULL_RESOLUTION_12)
	   #undef PMEM_SENSOR_FULL_RESOLUTION_CIF_1
	   #define PMEM_SENSOR_FULL_RESOLUTION_CIF_1 PMEM_SENSOR_FULL_RESOLUTION_12
	   #endif
    #endif
#endif
#endif

#if (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 > RK_CAM_SUPPORT_RESOLUTION)
    #error "PMEM_SENSOR_FULL_RESOLUTION_CIF_0 is larger than 5Meag"
#endif

#if (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 > RK_CAM_SUPPORT_RESOLUTION)
    #error "PMEM_SENSOR_FULL_RESOLUTION_CIF_1 is larger than 5Meag"
#endif

//CIF 0
#if (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x800000)   /* ddl@rock-chips.com : It is only support 5Mega interplate to 8Mega */
#define PMEM_CAM_NECESSARY_CIF_0   0x1900000       /* 1280*720*1.5*4(preview) + 12M(capture raw) + 7M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x800000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x500000)
#define PMEM_CAM_NECESSARY_CIF_0   0x1400000       /* 1280*720*1.5*4(preview) + 7.5M(capture raw) + 4M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x800000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x300000)
#define PMEM_CAM_NECESSARY_CIF_0   0xf00000        /* 1280*720*1.5*4(preview) + 4.5M(capture raw) + 3M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x600000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x200000) /* 1280*720*1.5*4(preview) + 3M(capture raw) + 3M(jpeg encode output) */
#define PMEM_CAM_NECESSARY_CIF_0   0xc00000
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x600000
#elif ((PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x100000) || (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x130000))
#define PMEM_CAM_NECESSARY_CIF_0   0xa00000        /* 800*600*1.5*4(preview) + 2M(capture raw) + 2M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x600000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x30000)
#define PMEM_CAM_NECESSARY_CIF_0   0x600000        /* 640*480*1.5*4(preview) + 1M(capture raw) + 1M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x600000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_0 == 0x00)
#define PMEM_CAM_NECESSARY_CIF_0   0x00
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x00
#else
#define PMEM_CAM_NECESSARY_CIF_0   0x1400000
#define PMEM_CAMIPP_NECESSARY_CIF_0    0x800000
#endif

//CIF 1
#if (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 == 0x800000)   /* ddl@rock-chips.com : It is only support 5Mega interplate to 8Mega */
#define PMEM_CAM_NECESSARY_CIF_1   0x1900000       /* 1280*720*1.5*4(preview) + 12M(capture raw) + 7M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_1    0x800000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 == 0x500000)
#define PMEM_CAM_NECESSARY_CIF_1	 0x1400000		 /* 1280*720*1.5*4(preview) + 7.5M(capture raw) + 4M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_1	  0x800000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 == 0x300000)
#define PMEM_CAM_NECESSARY_CIF_1	 0xf00000		 /* 1280*720*1.5*4(preview) + 4.5M(capture raw) + 3M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_1    0x600000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_1== 0x200000) /* 1280*720*1.5*4(preview) + 3M(capture raw) + 3M(jpeg encode output) */
#define PMEM_CAM_NECESSARY_CIF_1	 0xc00000
#define PMEM_CAMIPP_NECESSARY_CIF_1    0x600000
#elif ((PMEM_SENSOR_FULL_RESOLUTION_CIF_1 == 0x100000) || (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 == 0x130000))
#define PMEM_CAM_NECESSARY_CIF_1	 0xa00000		 /* 800*600*1.5*4(preview) + 2M(capture raw) + 2M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_1    0x600000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 == 0x30000)
#define PMEM_CAM_NECESSARY_CIF_1	 0x600000		 /* 640*480*1.5*4(preview) + 1M(capture raw) + 1M(jpeg encode output) */
#define PMEM_CAMIPP_NECESSARY_CIF_1    0x600000
#elif (PMEM_SENSOR_FULL_RESOLUTION_CIF_1 == 0x00)
#define PMEM_CAM_NECESSARY_CIF_1   0x00
#define PMEM_CAMIPP_NECESSARY_CIF_1    0x00
#else
#define PMEM_CAM_NECESSARY_CIF_1	 0x1400000
#define PMEM_CAMIPP_NECESSARY_CIF_1    0x800000
#endif


#ifdef CONFIG_VIDEO_RKCIF_WORK_SIMUL_OFF
    #if (PMEM_CAM_NECESSARY_CIF_0 > PMEM_CAM_NECESSARY_CIF_1)
    #define PMEM_CAM_NECESSARY PMEM_CAM_NECESSARY_CIF_0
    #define PMEM_CAMIPP_NECESSARY PMEM_CAMIPP_NECESSARY_CIF_0
    #else
    #define PMEM_CAM_NECESSARY PMEM_CAM_NECESSARY_CIF_1
    #define PMEM_CAMIPP_NECESSARY PMEM_CAMIPP_NECESSARY_CIF_1
    #endif
#endif

#if (!defined(CONFIG_VIDEO_RKCIF_WORK_SIMUL_OFF) && !defined(CONFIG_VIDEO_RKCIF_WORK_SIMUL_ON))
    #if PMEM_CAM_NECESSARY_CIF_0
    #define PMEM_CAM_NECESSARY PMEM_CAM_NECESSARY_CIF_0
    #define PMEM_CAMIPP_NECESSARY PMEM_CAMIPP_NECESSARY_CIF_0  
    #else
    #define PMEM_CAM_NECESSARY PMEM_CAM_NECESSARY_CIF_1
    #define PMEM_CAMIPP_NECESSARY PMEM_CAMIPP_NECESSARY_CIF_1  
    #endif
#endif

#ifdef CONFIG_VIDEO_RK29_CAMMEM_ION
    #undef PMEM_CAM_NECESSARY
    #define PMEM_CAM_NECESSARY 0x00000000
#endif


/*---------------- Camera Sensor Fixed Macro End  ------------------------*/
#else	//#ifdef CONFIG_VIDEO_RK 
#define PMEM_CAM_NECESSARY	 0x00000000
#endif

#else   // #ifndef PMEM_CAM_SIZE
/*
*      Driver Version Note
*v0.0.1: this driver is compatible with generic_sensor
*v0.1.1:
*        Cam_Power return success in rk_sensor_ioctrl when power io havn't config;
*v0.1.3:
*        1. this version support fov configuration in new_camera_device;
*        2. Reduce delay time after power off or power down camera;
*v0.1.5:
*        1. if sensor power callback failed, power down sensor;
*v0.1.6:
		 1. when power down,HWRST just need to set to powerdown mode.
*v0.1.7:
*        1. add support af power control;
*/
static int camio_version = KERNEL_VERSION(0,1,7);
module_param(camio_version, int, S_IRUGO);


static int camera_debug;
module_param(camera_debug, int, S_IRUGO|S_IWUSR);    

#undef  CAMMODULE_NAME
#define CAMMODULE_NAME   "rk_cam_io"

#define ddprintk(level, fmt, arg...) do {			\
	if (camera_debug >= level) 					\
	    printk(KERN_WARNING"%s(%d):" fmt"\n", CAMMODULE_NAME,__LINE__,## arg); } while (0)

#define dprintk(format, ...) ddprintk(1, format, ## __VA_ARGS__)  
#define eprintk(format, ...) printk(KERN_ERR "%s(%d):" format"\n",CAMMODULE_NAME,__LINE__,## __VA_ARGS__)  

#define SENSOR_NAME_0 STR(CONFIG_SENSOR_0)			/* back camera sensor 0 */
#define SENSOR_NAME_1 STR(CONFIG_SENSOR_1)			/* front camera sensor 0 */
#define SENSOR_DEVICE_NAME_0  STR(CONS(CONFIG_SENSOR_0, _back))
#define SENSOR_DEVICE_NAME_1  STR(CONS(CONFIG_SENSOR_1, _front))
#ifdef CONFIG_SENSOR_01
#define SENSOR_NAME_01 STR(CONFIG_SENSOR_01)			/* back camera sensor 1 */
#define SENSOR_DEVICE_NAME_01  STR(CONS(CONFIG_SENSOR_01, _back_1))
#endif
#ifdef CONFIG_SENSOR_02
#define SENSOR_NAME_02 STR(CONFIG_SENSOR_02)			/* back camera sensor 2 */
#define SENSOR_DEVICE_NAME_02  STR(CONS(CONFIG_SENSOR_02, _back_2))
#endif
#ifdef CONFIG_SENSOR_11
#define SENSOR_NAME_11 STR(CONFIG_SENSOR_11)			/* front camera sensor 1 */
#define SENSOR_DEVICE_NAME_11  STR(CONS(CONFIG_SENSOR_11, _front_1))
#endif
#ifdef CONFIG_SENSOR_12
#define SENSOR_NAME_12 STR(CONFIG_SENSOR_12)			/* front camera sensor 2 */
#define SENSOR_DEVICE_NAME_12  STR(CONS(CONFIG_SENSOR_12, _front_2))
#endif

static int rk_sensor_io_init(void);
static int rk_sensor_io_deinit(int sensor);
static int rk_sensor_ioctrl(struct device *dev,enum rk29camera_ioctrl_cmd cmd, int on);
static int rk_sensor_power(struct device *dev, int on);
static int rk_sensor_register(void);
//static int rk_sensor_reset(struct device *dev);

static int rk_sensor_powerdown(struct device *dev, int on);

static struct rk29camera_platform_data rk_camera_platform_data = {
    .io_init = rk_sensor_io_init,
    .io_deinit = rk_sensor_io_deinit,
    .iomux = rk_sensor_iomux,
    .sensor_ioctrl = rk_sensor_ioctrl,
    .sensor_register = rk_sensor_register,
    .gpio_res = {
        {
    #if defined CONFIG_SENSOR_IIC_ADDR_0 && CONFIG_SENSOR_IIC_ADDR_0            
            .gpio_reset = CONFIG_SENSOR_RESET_PIN_0,
            .gpio_power = CONFIG_SENSOR_POWER_PIN_0,
            .gpio_powerdown = CONFIG_SENSOR_POWERDN_PIN_0,
            .gpio_flash = CONFIG_SENSOR_FALSH_PIN_0,
            .gpio_flag = (CONFIG_SENSOR_POWERACTIVE_LEVEL_0|CONFIG_SENSOR_RESETACTIVE_LEVEL_0|CONFIG_SENSOR_POWERDNACTIVE_LEVEL_0|CONFIG_SENSOR_FLASHACTIVE_LEVEL_0),
            .gpio_init = 0,            
            .dev_name = SENSOR_DEVICE_NAME_0,
   #else
            .gpio_reset = INVALID_GPIO,
            .gpio_power = INVALID_GPIO,
            .gpio_powerdown = INVALID_GPIO,
            .gpio_flash = INVALID_GPIO,
            .gpio_flag = 0,
            .gpio_init = 0,            
            .dev_name = NULL,
   #endif
        }, {
   #if defined CONFIG_SENSOR_IIC_ADDR_1 && CONFIG_SENSOR_IIC_ADDR_1  
            .gpio_reset = CONFIG_SENSOR_RESET_PIN_1,
            .gpio_power = CONFIG_SENSOR_POWER_PIN_1,
            .gpio_powerdown = CONFIG_SENSOR_POWERDN_PIN_1,
            .gpio_flash = CONFIG_SENSOR_FALSH_PIN_1,
            .gpio_flag = (CONFIG_SENSOR_POWERACTIVE_LEVEL_1|CONFIG_SENSOR_RESETACTIVE_LEVEL_1|CONFIG_SENSOR_POWERDNACTIVE_LEVEL_1|CONFIG_SENSOR_FLASHACTIVE_LEVEL_1),
            .gpio_init = 0,
            .dev_name = SENSOR_DEVICE_NAME_1,
   #else
            .gpio_reset = INVALID_GPIO,
            .gpio_power = INVALID_GPIO,
            .gpio_powerdown = INVALID_GPIO,
            .gpio_flash = INVALID_GPIO,
            .gpio_flag = 0,
            .gpio_init = 0,            
            .dev_name = NULL,
   #endif
        }, 
        #ifdef CONFIG_SENSOR_01
        {
        #if CONFIG_SENSOR_IIC_ADDR_01
            .gpio_reset = CONFIG_SENSOR_RESET_PIN_01,
            .gpio_power = CONFIG_SENSOR_POWER_PIN_01,
            .gpio_powerdown = CONFIG_SENSOR_POWERDN_PIN_01,
            .gpio_flash = CONFIG_SENSOR_FALSH_PIN_01,
            .gpio_flag = (CONFIG_SENSOR_POWERACTIVE_LEVEL_01|CONFIG_SENSOR_RESETACTIVE_LEVEL_01|CONFIG_SENSOR_POWERDNACTIVE_LEVEL_01|CONFIG_SENSOR_FLASHACTIVE_LEVEL_01),
            .gpio_init = 0,            
            .dev_name = SENSOR_DEVICE_NAME_01,
        #else
            .gpio_reset = INVALID_GPIO,
            .gpio_power = INVALID_GPIO,
            .gpio_powerdown = INVALID_GPIO,
            .gpio_flash = INVALID_GPIO,
            .gpio_flag = 0,
            .gpio_init = 0,            
            .dev_name = NULL,
        #endif
        }, 
        #endif
        #ifdef CONFIG_SENSOR_02
        {
        #if CONFIG_SENSOR_IIC_ADDR_02
            .gpio_reset = CONFIG_SENSOR_RESET_PIN_02,
            .gpio_power = CONFIG_SENSOR_POWER_PIN_02,
            .gpio_powerdown = CONFIG_SENSOR_POWERDN_PIN_02,
            .gpio_flash = CONFIG_SENSOR_FALSH_PIN_02,
            .gpio_flag = (CONFIG_SENSOR_POWERACTIVE_LEVEL_02|CONFIG_SENSOR_RESETACTIVE_LEVEL_02|CONFIG_SENSOR_POWERDNACTIVE_LEVEL_02|CONFIG_SENSOR_FLASHACTIVE_LEVEL_02),
            .gpio_init = 0,            
            .dev_name = SENSOR_DEVICE_NAME_02, 
        #else
            .gpio_reset = INVALID_GPIO,
            .gpio_power = INVALID_GPIO,
            .gpio_powerdown = INVALID_GPIO,
            .gpio_flash = INVALID_GPIO,
            .gpio_flag = 0,
            .gpio_init = 0,            
            .dev_name = NULL,
        #endif        
        },
        #endif
        #ifdef CONFIG_SENSOR_11
        {
        #if CONFIG_SENSOR_IIC_ADDR_11
            .gpio_reset = CONFIG_SENSOR_RESET_PIN_11,
            .gpio_power = CONFIG_SENSOR_POWER_PIN_11,
            .gpio_powerdown = CONFIG_SENSOR_POWERDN_PIN_11,
            .gpio_flash = CONFIG_SENSOR_FALSH_PIN_11,
            .gpio_flag = (CONFIG_SENSOR_POWERACTIVE_LEVEL_11|CONFIG_SENSOR_RESETACTIVE_LEVEL_11|CONFIG_SENSOR_POWERDNACTIVE_LEVEL_11|CONFIG_SENSOR_FLASHACTIVE_LEVEL_11),
            .gpio_init = 0,
            .dev_name = SENSOR_DEVICE_NAME_11,
        #else
            .gpio_reset = INVALID_GPIO,
            .gpio_power = INVALID_GPIO,
            .gpio_powerdown = INVALID_GPIO,
            .gpio_flash = INVALID_GPIO,
            .gpio_flag = 0,
            .gpio_init = 0,            
            .dev_name = NULL,
        #endif        
        }, 
        #endif
        #ifdef CONFIG_SENSOR_12
        {
        #if CONFIG_SENSOR_IIC_ADDR_12
            .gpio_reset = CONFIG_SENSOR_RESET_PIN_12,
            .gpio_power = CONFIG_SENSOR_POWER_PIN_12,
            .gpio_powerdown = CONFIG_SENSOR_POWERDN_PIN_12,
            .gpio_flash = CONFIG_SENSOR_FALSH_PIN_12,
            .gpio_flag = (CONFIG_SENSOR_POWERACTIVE_LEVEL_12|CONFIG_SENSOR_RESETACTIVE_LEVEL_12|CONFIG_SENSOR_POWERDNACTIVE_LEVEL_12|CONFIG_SENSOR_FLASHACTIVE_LEVEL_12),
            .gpio_init = 0,
            .dev_name = SENSOR_DEVICE_NAME_12,
        #else
            .gpio_reset = INVALID_GPIO,
            .gpio_power = INVALID_GPIO,
            .gpio_powerdown = INVALID_GPIO,
            .gpio_flash = INVALID_GPIO,
            .gpio_flag = 0,
            .gpio_init = 0,            
            .dev_name = NULL,
        #endif
        }
        #endif
    },
    
    #ifdef CONFIG_VIDEO_RK29_WORK_IPP
    #ifdef MEM_CAMIPP_BASE
	.meminfo = {
	    .name  = "camera_ipp_mem",
		.start = MEM_CAMIPP_BASE,
		.size   = MEM_CAMIPP_SIZE,
	},
	#endif
	#endif
    
    .info = {
        {
        #ifdef CONFIG_SENSOR_0             
            .dev_name = SENSOR_DEVICE_NAME_0,
            .orientation = CONFIG_SENSOR_ORIENTATION_0,  
        #else
            .dev_name = NULL,
            .orientation = 0x00, 
        #endif
        },{
        #ifdef CONFIG_SENSOR_1
            .dev_name = SENSOR_DEVICE_NAME_1,
            .orientation = CONFIG_SENSOR_ORIENTATION_1,
        #else
            .dev_name = NULL,
            .orientation = 0x00, 
        #endif
        #ifdef CONFIG_SENSOR_01
	    },{
	        .dev_name = SENSOR_DEVICE_NAME_01,
            .orientation = CONFIG_SENSOR_ORIENTATION_01, 
        #else
        },{
	        .dev_name = NULL,
            .orientation = 0x00, 
        #endif
        #ifdef CONFIG_SENSOR_02
	    },{
	        .dev_name = SENSOR_DEVICE_NAME_02,
            .orientation = CONFIG_SENSOR_ORIENTATION_02, 
        #else
        },{
	        .dev_name = NULL,
            .orientation = 0x00, 
        #endif
        
        #ifdef CONFIG_SENSOR_11 
        },{
            .dev_name = SENSOR_DEVICE_NAME_11,
            .orientation = CONFIG_SENSOR_ORIENTATION_11, 
        #else
        },{
	        .dev_name = NULL,
            .orientation = 0x00, 
        #endif
        #ifdef CONFIG_SENSOR_12
	    },{
	        .dev_name = SENSOR_DEVICE_NAME_12,
            .orientation = CONFIG_SENSOR_ORIENTATION_12, 
        #else
        },{
	        .dev_name = NULL,
            .orientation = 0x00, 
        #endif
	    },
	},
    
    .register_dev = {
        #ifdef CONFIG_SENSOR_0
        {
        #if (CONFIG_SENSOR_IIC_ADDR_0 != 0x00)
            .i2c_cam_info = {
                I2C_BOARD_INFO(SENSOR_NAME_0, CONFIG_SENSOR_IIC_ADDR_0>>1),
            },
            .link_info = {
                #ifdef SENSOR_CIF_BUSID_0
            	.bus_id= SENSOR_CIF_BUSID_0,
            	#else
                .bus_id= RK29_CAM_PLATFORM_DEV_ID,
                #endif
            	.power		= rk_sensor_power,
                #if (CONFIG_SENSOR_RESET_PIN_0 != INVALID_GPIO)
            	.reset		= rk_sensor_reset,
                #endif	  
            	.powerdown	= rk_sensor_powerdown,
            	//.board_info = &rk_i2c_cam_info_0[0],
            	
            	.i2c_adapter_id = CONFIG_SENSOR_IIC_ADAPTER_ID_0,
            	.module_name	= SENSOR_NAME_0,
            },
            .device_info = {
            	.name	= "soc-camera-pdrv",
            	//.id = 0,
            	.dev	= {
            		.init_name = SENSOR_DEVICE_NAME_0,
            		//.platform_data = &rk_iclink_0,
            	}
            }
        #endif
	    },
        #endif
        #ifdef CONFIG_SENSOR_1
        {
	    #if (CONFIG_SENSOR_IIC_ADDR_1 != 0x00)
            .i2c_cam_info = {
                I2C_BOARD_INFO(SENSOR_NAME_1, CONFIG_SENSOR_IIC_ADDR_1>>1)
            },
            .link_info = {
            	#ifdef SENSOR_CIF_BUSID_1
            	.bus_id= SENSOR_CIF_BUSID_1,
            	#else
                .bus_id= RK29_CAM_PLATFORM_DEV_ID,
                #endif
            	.power		= rk_sensor_power,
                #if (CONFIG_SENSOR_RESET_PIN_1 != INVALID_GPIO)
            	.reset		= rk_sensor_reset,
                #endif	  
            	.powerdown	= rk_sensor_powerdown,
            	//.board_info = &rk_i2c_cam_info_0[0],
            	
            	.i2c_adapter_id = CONFIG_SENSOR_IIC_ADAPTER_ID_1,
            	.module_name	= SENSOR_NAME_1,
            },
            .device_info = {
            	.name	= "soc-camera-pdrv",
            	//.id = 1,
            	.dev	= {
            		.init_name = SENSOR_DEVICE_NAME_1,
            		//.platform_data = &rk_iclink_0,
            	}
            }
        #endif
	    },
	    #endif
        #ifdef CONFIG_SENSOR_01
        {
        #if (CONFIG_SENSOR_IIC_ADDR_01 != 0x00)
            .i2c_cam_info = {
                I2C_BOARD_INFO(SENSOR_NAME_01, CONFIG_SENSOR_IIC_ADDR_01>>1)
            },
            .link_info = {
            	#ifdef SENSOR_CIF_BUSID_01
            	.bus_id= SENSOR_CIF_BUSID_01,
            	#else
                .bus_id= RK29_CAM_PLATFORM_DEV_ID,
                #endif
            	.power		= rk_sensor_power,
                #if (CONFIG_SENSOR_RESET_PIN_01 != INVALID_GPIO)
            	.reset		= rk_sensor_reset,
                #endif	  
            	.powerdown	= rk_sensor_powerdown,
            	//.board_info = &rk_i2c_cam_info_0[0],
            	
            	.i2c_adapter_id = CONFIG_SENSOR_IIC_ADAPTER_ID_01,
            	.module_name	= SENSOR_NAME_01,
            },
            .device_info = {
            	.name	= "soc-camera-pdrv",
            	//.id = 1,
            	.dev	= {
            		.init_name = SENSOR_DEVICE_NAME_01,
            		//.platform_data = &rk_iclink_0,
            	}
            }
        #endif
	    },
        #endif
        #ifdef CONFIG_SENSOR_02 
        {
	    #if (CONFIG_SENSOR_IIC_ADDR_02 != 0x00)
            .i2c_cam_info = {
                I2C_BOARD_INFO(SENSOR_NAME_02, CONFIG_SENSOR_IIC_ADDR_02>>1)
            },
            .link_info = {
            	#ifdef SENSOR_CIF_BUSID_02
            	.bus_id= SENSOR_CIF_BUSID_02,
            	#else
                .bus_id= RK29_CAM_PLATFORM_DEV_ID,
                #endif
            	.power		= rk_sensor_power,
                #if (CONFIG_SENSOR_RESET_PIN_02 != INVALID_GPIO)
            	.reset		= rk_sensor_reset,
                #endif	  
            	.powerdown	= rk_sensor_powerdown,
            	//.board_info = &rk_i2c_cam_info_0[0],
            	
            	.i2c_adapter_id = CONFIG_SENSOR_IIC_ADAPTER_ID_02,
            	.module_name	= SENSOR_NAME_02,
            },
            .device_info = {
            	.name	= "soc-camera-pdrv",
            	//.id = 1,
            	.dev	= {
            		.init_name = SENSOR_DEVICE_NAME_02,
            		//.platform_data = &rk_iclink_0,
            	}
            }
        #endif
	    },
        #endif
        #ifdef CONFIG_SENSOR_11
        {	    
        #if (CONFIG_SENSOR_IIC_ADDR_11 != 0x00)
            .i2c_cam_info = {
                I2C_BOARD_INFO(SENSOR_NAME_11, CONFIG_SENSOR_IIC_ADDR_11>>1)
            },
            .link_info = {
            	#ifdef SENSOR_CIF_BUSID_11
            	.bus_id= SENSOR_CIF_BUSID_11,
            	#else
                .bus_id= RK29_CAM_PLATFORM_DEV_ID,
                #endif
            	.power		= rk_sensor_power,
                #if (CONFIG_SENSOR_RESET_PIN_11 != INVALID_GPIO)
            	.reset		= rk_sensor_reset,
                #endif	  
            	.powerdown	= rk_sensor_powerdown,
            	//.board_info = &rk_i2c_cam_info_0[0],
            	
            	.i2c_adapter_id = CONFIG_SENSOR_IIC_ADAPTER_ID_11,
            	.module_name	= SENSOR_NAME_11,
            },
            .device_info = {
            	.name	= "soc-camera-pdrv",
            	//.id = 1,
            	.dev	= {
            		.init_name = SENSOR_DEVICE_NAME_11,
            		//.platform_data = &rk_iclink_0,
            	}
            }
        #endif        
	    },
        #endif
        #ifdef CONFIG_SENSOR_12
        {	    
	    #if (CONFIG_SENSOR_IIC_ADDR_12 != 0x00)
            .i2c_cam_info = {
                I2C_BOARD_INFO(SENSOR_NAME_12, CONFIG_SENSOR_IIC_ADDR_12>>1)
            },
            .link_info = {
            	#ifdef SENSOR_CIF_BUSID_12
            	.bus_id= SENSOR_CIF_BUSID_12,
            	#else
                .bus_id= RK29_CAM_PLATFORM_DEV_ID,
                #endif
            	.power		= rk_sensor_power,
                #if (CONFIG_SENSOR_RESET_PIN_12 != INVALID_GPIO)
            	.reset		= rk_sensor_reset,
                #endif	  
            	.powerdown	= rk_sensor_powerdown,
            	//.board_info = &rk_i2c_cam_info_0[0],
            	
            	.i2c_adapter_id = CONFIG_SENSOR_IIC_ADAPTER_ID_12,
            	.module_name	= SENSOR_NAME_12,
            },
            .device_info = {
            	.name	= "soc-camera-pdrv",
            	//.id = 1,
            	.dev	= {
            		.init_name = SENSOR_DEVICE_NAME_12,
            		//.platform_data = &rk_iclink_0,
            	}
            }
        #endif        
	    },
        #endif
	},
    .register_dev_new = new_camera,
};


static int sensor_power_default_cb (struct rk29camera_gpio_res *res, int on)
{
    int camera_power = res->gpio_power;
    int camera_ioflag = res->gpio_flag;
    int camera_io_init = res->gpio_init;
    int ret = 0;
    
    if (camera_power != INVALID_GPIO)  {
		if (camera_io_init & RK29_CAM_POWERACTIVE_MASK) {
            if (on) {
            	gpio_set_value(camera_power, ((camera_ioflag&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
    			dprintk("%s PowerPin=%d ..PinLevel = %x",res->dev_name, camera_power, ((camera_ioflag&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
    			msleep(10);
    		} else {
    			gpio_set_value(camera_power, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
    			dprintk("%s PowerPin=%d ..PinLevel = %x",res->dev_name, camera_power, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
    		}
		} else {
			ret = RK29_CAM_EIO_REQUESTFAIL;
			eprintk("%s PowerPin=%d request failed!", res->dev_name,camera_power);
	    }        
    } else {
		ret = RK29_CAM_EIO_INVALID;
    } 

    return ret;
}

static int sensor_reset_default_cb (struct rk29camera_gpio_res *res, int on)
{
    int camera_reset = res->gpio_reset;
    int camera_ioflag = res->gpio_flag;
    int camera_io_init = res->gpio_init;  
    int ret = 0;
    
    if (camera_reset != INVALID_GPIO) {
		if (camera_io_init & RK29_CAM_RESETACTIVE_MASK) {
			if (on) {
	        	gpio_set_value(camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
	        	dprintk("%s ResetPin=%d ..PinLevel = %x",res->dev_name,camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
			} else {
				gpio_set_value(camera_reset,(((~camera_ioflag)&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
        		dprintk("%s ResetPin= %d..PinLevel = %x",res->dev_name, camera_reset, (((~camera_ioflag)&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
	        }
		} else {
			ret = RK29_CAM_EIO_REQUESTFAIL;
			eprintk("%s ResetPin=%d request failed!", res->dev_name,camera_reset);
		}
    } else {
		ret = RK29_CAM_EIO_INVALID;
    }

    return ret;
}

static int sensor_powerdown_default_cb (struct rk29camera_gpio_res *res, int on)
{
    int camera_powerdown = res->gpio_powerdown;
    int camera_ioflag = res->gpio_flag;
    int camera_io_init = res->gpio_init;  
    int ret = 0;    

    if (camera_powerdown != INVALID_GPIO) {
		if (camera_io_init & RK29_CAM_POWERDNACTIVE_MASK) {
			if (on) {
	        	gpio_set_value(camera_powerdown, ((camera_ioflag&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));
	        	dprintk("%s PowerDownPin=%d ..PinLevel = %x" ,res->dev_name,camera_powerdown, ((camera_ioflag&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));
			} else {
				gpio_set_value(camera_powerdown,(((~camera_ioflag)&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));
        		dprintk("%s PowerDownPin= %d..PinLevel = %x" ,res->dev_name, camera_powerdown, (((~camera_ioflag)&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));
	        }
		} else {
			ret = RK29_CAM_EIO_REQUESTFAIL;
			dprintk("%s PowerDownPin=%d request failed!", res->dev_name,camera_powerdown);
		}
    } else {
		ret = RK29_CAM_EIO_INVALID;
    }
    return ret;
}


static int sensor_flash_default_cb (struct rk29camera_gpio_res *res, int on)
{
    int camera_flash = res->gpio_flash;
    int camera_ioflag = res->gpio_flag;
    int camera_io_init = res->gpio_init;  
    int ret = 0;    

    if (camera_flash != INVALID_GPIO) {
		if (camera_io_init & RK29_CAM_FLASHACTIVE_MASK) {
            switch (on)
            {
                case Flash_Off:
                {
                    gpio_set_value(camera_flash,(((~camera_ioflag)&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));
        		    dprintk("%s FlashPin= %d..PinLevel = %x", res->dev_name, camera_flash, (((~camera_ioflag)&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS)); 
        		    break;
                }

                case Flash_On:
                {
                    gpio_set_value(camera_flash, ((camera_ioflag&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));
	        	    dprintk("%s FlashPin=%d ..PinLevel = %x", res->dev_name,camera_flash, ((camera_ioflag&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));
	        	    break;
                }

                case Flash_Torch:
                {
                    gpio_set_value(camera_flash, ((camera_ioflag&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));
	        	    dprintk("%s FlashPin=%d ..PinLevel = %x", res->dev_name,camera_flash, ((camera_ioflag&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));
	        	    break;
                }

                default:
                {
                    eprintk("%s Flash command(%d) is invalidate", res->dev_name,on);
                    break;
                }
            }
		} else {
			ret = RK29_CAM_EIO_REQUESTFAIL;
			eprintk("%s FlashPin=%d request failed!", res->dev_name,camera_flash);
		}
    } else {
		ret = RK29_CAM_EIO_INVALID;
    }
    return ret;
}

static int sensor_afpower_default_cb (struct rk29camera_gpio_res *res, int on)
{
	int ret = 0;   
	int camera_af = res->gpio_af;
	if (camera_af != INVALID_GPIO) {
		gpio_set_value(camera_af, on);
	}

	return ret;
}

static void rk29_sensor_fps_get(int idx, unsigned int *val, int w, int h)
{
    switch (idx)
    {
        #ifdef CONFIG_SENSOR_0
        case 0:
        {
            if ((w==176) && (h==144)) {
                *val = CONFIG_SENSOR_QCIF_FPS_FIXED_0;
            #ifdef CONFIG_SENSOR_240X160_FPS_FIXED_0
            } else if ((w==240) && (h==160)) {
                *val = CONFIG_SENSOR_240X160_FPS_FIXED_0;
            #endif
            } else if ((w==320) && (h==240)) {
                *val = CONFIG_SENSOR_QVGA_FPS_FIXED_0;
            } else if ((w==352) && (h==288)) {
                *val = CONFIG_SENSOR_CIF_FPS_FIXED_0;
            } else if ((w==640) && (h==480)) {
                *val = CONFIG_SENSOR_VGA_FPS_FIXED_0;
            } else if ((w==720) && (h==480)) {
                *val = CONFIG_SENSOR_480P_FPS_FIXED_0;
            } else if ((w==800) && (h==600)) {
                *val = CONFIG_SENSOR_SVGA_FPS_FIXED_0;
            } else if ((w==1280) && (h==720)) {
                *val = CONFIG_SENSOR_720P_FPS_FIXED_0;
            }
            break;
        }
        #endif
        #ifdef CONFIG_SENSOR_1
        case 1:
        {
            if ((w==176) && (h==144)) {
                *val = CONFIG_SENSOR_QCIF_FPS_FIXED_1;
            #ifdef CONFIG_SENSOR_240X160_FPS_FIXED_1
            } else if ((w==240) && (h==160)) {
                *val = CONFIG_SENSOR_240X160_FPS_FIXED_1;
            #endif
            } else if ((w==320) && (h==240)) {
                *val = CONFIG_SENSOR_QVGA_FPS_FIXED_1;
            } else if ((w==352) && (h==288)) {
                *val = CONFIG_SENSOR_CIF_FPS_FIXED_1;
            } else if ((w==640) && (h==480)) {
                *val = CONFIG_SENSOR_VGA_FPS_FIXED_1;
            } else if ((w==720) && (h==480)) {
                *val = CONFIG_SENSOR_480P_FPS_FIXED_1;
            } else if ((w==800) && (h==600)) {
                *val = CONFIG_SENSOR_SVGA_FPS_FIXED_1;
            } else if ((w==1280) && (h==720)) {
                *val = CONFIG_SENSOR_720P_FPS_FIXED_1;
            }
            break;
        }
        #endif
        #ifdef CONFIG_SENSOR_01
        case 2:
        {
            if ((w==176) && (h==144)) {
                *val = CONFIG_SENSOR_QCIF_FPS_FIXED_01;
            #ifdef CONFIG_SENSOR_240X160_FPS_FIXED_01
            } else if ((w==240) && (h==160)) {
                *val = CONFIG_SENSOR_240X160_FPS_FIXED_01;
            #endif
            } else if ((w==320) && (h==240)) {
                *val = CONFIG_SENSOR_QVGA_FPS_FIXED_01;
            } else if ((w==352) && (h==288)) {
                *val = CONFIG_SENSOR_CIF_FPS_FIXED_01;
            } else if ((w==640) && (h==480)) {
                *val = CONFIG_SENSOR_VGA_FPS_FIXED_01;
            } else if ((w==720) && (h==480)) {
                *val = CONFIG_SENSOR_480P_FPS_FIXED_01;
            } else if ((w==800) && (h==600)) {
                *val = CONFIG_SENSOR_SVGA_FPS_FIXED_01;
            } else if ((w==1280) && (h==720)) {
                *val = CONFIG_SENSOR_720P_FPS_FIXED_01;
            }
            break;
        }
        #endif
        #ifdef CONFIG_SENSOR_02
        case 3:
        {
            if ((w==176) && (h==144)) {
                *val = CONFIG_SENSOR_QCIF_FPS_FIXED_02;
            #ifdef CONFIG_SENSOR_240X160_FPS_FIXED_02
            } else if ((w==240) && (h==160)) {
                *val = CONFIG_SENSOR_240X160_FPS_FIXED_02;
            #endif
            } else if ((w==320) && (h==240)) {
                *val = CONFIG_SENSOR_QVGA_FPS_FIXED_02;
            } else if ((w==352) && (h==288)) {
                *val = CONFIG_SENSOR_CIF_FPS_FIXED_02;
            } else if ((w==640) && (h==480)) {
                *val = CONFIG_SENSOR_VGA_FPS_FIXED_02;
            } else if ((w==720) && (h==480)) {
                *val = CONFIG_SENSOR_480P_FPS_FIXED_02;
            } else if ((w==800) && (h==600)) {
                *val = CONFIG_SENSOR_SVGA_FPS_FIXED_02;
            } else if ((w==1280) && (h==720)) {
                *val = CONFIG_SENSOR_720P_FPS_FIXED_02;
            }
            break;
        }
        #endif
        
        #ifdef CONFIG_SENSOR_11
        case 4:
        {
            if ((w==176) && (h==144)) {
                *val = CONFIG_SENSOR_QCIF_FPS_FIXED_11;
            #ifdef CONFIG_SENSOR_240X160_FPS_FIXED_11
            } else if ((w==240) && (h==160)) {
                *val = CONFIG_SENSOR_240X160_FPS_FIXED_11;
            #endif
            } else if ((w==320) && (h==240)) {
                *val = CONFIG_SENSOR_QVGA_FPS_FIXED_11;
            } else if ((w==352) && (h==288)) {
                *val = CONFIG_SENSOR_CIF_FPS_FIXED_11;
            } else if ((w==640) && (h==480)) {
                *val = CONFIG_SENSOR_VGA_FPS_FIXED_11;
            } else if ((w==720) && (h==480)) {
                *val = CONFIG_SENSOR_480P_FPS_FIXED_11;
            } else if ((w==800) && (h==600)) {
                *val = CONFIG_SENSOR_SVGA_FPS_FIXED_11;
            } else if ((w==1280) && (h==720)) {
                *val = CONFIG_SENSOR_720P_FPS_FIXED_11;
            }
            break;
        }
        #endif
        #ifdef CONFIG_SENSOR_12
        case 5:
        {
            if ((w==176) && (h==144)) {
                *val = CONFIG_SENSOR_QCIF_FPS_FIXED_12;
            #ifdef CONFIG_SENSOR_240X160_FPS_FIXED_12
            } else if ((w==240) && (h==160)) {
                *val = CONFIG_SENSOR_240X160_FPS_FIXED_12;
            #endif
            } else if ((w==320) && (h==240)) {
                *val = CONFIG_SENSOR_QVGA_FPS_FIXED_12;
            } else if ((w==352) && (h==288)) {
                *val = CONFIG_SENSOR_CIF_FPS_FIXED_12;
            } else if ((w==640) && (h==480)) {
                *val = CONFIG_SENSOR_VGA_FPS_FIXED_12;
            } else if ((w==720) && (h==480)) {
                *val = CONFIG_SENSOR_480P_FPS_FIXED_12;
            } else if ((w==800) && (h==600)) {
                *val = CONFIG_SENSOR_SVGA_FPS_FIXED_12;
            } else if ((w==1280) && (h==720)) {
                *val = CONFIG_SENSOR_720P_FPS_FIXED_12;
            }
            break;
        }
        #endif
        default:
            eprintk(" sensor-%d have not been define in board file!",idx);
    }
}
static int _rk_sensor_io_init_(struct rk29camera_gpio_res *gpio_res)
{
    int ret = 0, i;
    unsigned int camera_reset = INVALID_GPIO, camera_power = INVALID_GPIO;
	unsigned int camera_powerdown = INVALID_GPIO, camera_flash = INVALID_GPIO;
	unsigned int camera_af = INVALID_GPIO,camera_ioflag;
    struct rk29camera_gpio_res *io_res;
    bool io_requested_in_camera;

    camera_reset = gpio_res->gpio_reset;
	camera_power = gpio_res->gpio_power;
	camera_powerdown = gpio_res->gpio_powerdown;
	camera_flash = gpio_res->gpio_flash;
	camera_af = gpio_res->gpio_af;	
	camera_ioflag = gpio_res->gpio_flag;
	gpio_res->gpio_init = 0;

    if (camera_power != INVALID_GPIO) {
        ret = gpio_request(camera_power, "camera power");
        if (ret) {
            io_requested_in_camera = false;
            for (i=0; i<RK_CAM_NUM; i++) {
                io_res = &rk_camera_platform_data.gpio_res[i];
                if (io_res->gpio_init & RK29_CAM_POWERACTIVE_MASK) {
                    if (io_res->gpio_power == camera_power)
                        io_requested_in_camera = true;    
                }
            }

            if (io_requested_in_camera==false) {
                i=0;
                while (strstr(new_camera[i].dev_name,"end")==NULL) {
                    io_res = &new_camera[i].io;
                    if (io_res->gpio_init & RK29_CAM_POWERACTIVE_MASK) {
                        if (io_res->gpio_power == camera_power)
                            io_requested_in_camera = true;    
                    }
                    i++;
                }
            }
            
            if (io_requested_in_camera==false) {
                printk( "%s power pin(%d) init failed\n", gpio_res->dev_name,camera_power);
                goto _rk_sensor_io_init_end_;
            } else {
                ret =0;
            }
        }

        if (rk_camera_platform_data.iomux(camera_power) < 0) {
            ret = -1;
            eprintk("%s power pin(%d) iomux init failed",gpio_res->dev_name,camera_power);
            goto _rk_sensor_io_init_end_;
        }
        
		gpio_res->gpio_init |= RK29_CAM_POWERACTIVE_MASK;
        gpio_set_value(camera_reset, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));
        gpio_direction_output(camera_power, (((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));

		dprintk("%s power pin(%d) init success(0x%x)" ,gpio_res->dev_name,camera_power,(((~camera_ioflag)&RK29_CAM_POWERACTIVE_MASK)>>RK29_CAM_POWERACTIVE_BITPOS));

    }

    if (camera_reset != INVALID_GPIO) {
        ret = gpio_request(camera_reset, "camera reset");
        if (ret) {
            io_requested_in_camera = false;
            for (i=0; i<RK_CAM_NUM; i++) {
                io_res = &rk_camera_platform_data.gpio_res[i];
                if (io_res->gpio_init & RK29_CAM_RESETACTIVE_MASK) {
                    if (io_res->gpio_reset == camera_reset)
                        io_requested_in_camera = true;    
                }
            }

            if (io_requested_in_camera==false) {
                i=0;
                while (strstr(new_camera[i].dev_name,"end")==NULL) {
                    io_res = &new_camera[i].io;
                    if (io_res->gpio_init & RK29_CAM_RESETACTIVE_MASK) {
                        if (io_res->gpio_reset == camera_reset)
                            io_requested_in_camera = true;    
                    }
                    i++;
                }
            }
            
            if (io_requested_in_camera==false) {
                eprintk("%s reset pin(%d) init failed" ,gpio_res->dev_name,camera_reset);
                goto _rk_sensor_io_init_end_;
            } else {
                ret =0;
            }
        }

        if (rk_camera_platform_data.iomux(camera_reset) < 0) {
            ret = -1;
            eprintk("%s reset pin(%d) iomux init failed", gpio_res->dev_name,camera_reset);
            goto _rk_sensor_io_init_end_;
        }
        
		gpio_res->gpio_init |= RK29_CAM_RESETACTIVE_MASK;
        gpio_set_value(camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));
        gpio_direction_output(camera_reset, ((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));

		dprintk("%s reset pin(%d) init success(0x%x)" ,gpio_res->dev_name,camera_reset,((camera_ioflag&RK29_CAM_RESETACTIVE_MASK)>>RK29_CAM_RESETACTIVE_BITPOS));

    }

	if (camera_powerdown != INVALID_GPIO) {
        ret = gpio_request(camera_powerdown, "camera powerdown");
        if (ret) {
            io_requested_in_camera = false;
            for (i=0; i<RK_CAM_NUM; i++) {
                io_res = &rk_camera_platform_data.gpio_res[i];
                if (io_res->gpio_init & RK29_CAM_POWERDNACTIVE_MASK) {
                    if (io_res->gpio_powerdown == camera_powerdown)
                        io_requested_in_camera = true;    
                }
            }

            if (io_requested_in_camera==false) {
                i=0;
                while (strstr(new_camera[i].dev_name,"end")==NULL) {
                    io_res = &new_camera[i].io;
                    if (io_res->gpio_init & RK29_CAM_POWERDNACTIVE_MASK) {
                        if (io_res->gpio_powerdown == camera_powerdown)
                            io_requested_in_camera = true;    
                    }
                    i++;
                }
            }
            
            if (io_requested_in_camera==false) {
                eprintk("%s powerdown pin(%d) init failed",gpio_res->dev_name,camera_powerdown);
                goto _rk_sensor_io_init_end_;
            } else {
                ret =0;
            }
        }

        if (rk_camera_platform_data.iomux(camera_powerdown) < 0) {
            ret = -1;
            eprintk("%s powerdown pin(%d) iomux init failed",gpio_res->dev_name,camera_powerdown);
            goto _rk_sensor_io_init_end_;
        }
        
		gpio_res->gpio_init |= RK29_CAM_POWERDNACTIVE_MASK;
        gpio_set_value(camera_powerdown, ((camera_ioflag&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));
        gpio_direction_output(camera_powerdown, ((camera_ioflag&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));

		dprintk("%s powerdown pin(%d) init success(0x%x)" ,gpio_res->dev_name,camera_powerdown,((camera_ioflag&RK29_CAM_POWERDNACTIVE_BITPOS)>>RK29_CAM_POWERDNACTIVE_BITPOS));

    }

	if (camera_flash != INVALID_GPIO) {
        ret = gpio_request(camera_flash, "camera flash");
        if (ret) {
            io_requested_in_camera = false;
            for (i=0; i<RK_CAM_NUM; i++) {
                io_res = &rk_camera_platform_data.gpio_res[i];
                if (io_res->gpio_init & RK29_CAM_POWERDNACTIVE_MASK) {
                    if (io_res->gpio_powerdown == camera_powerdown)
                        io_requested_in_camera = true;    
                }
            }

            if (io_requested_in_camera==false) {
                i=0;
                while (strstr(new_camera[i].dev_name,"end")==NULL) {
                    io_res = &new_camera[i].io;
                    if (io_res->gpio_init & RK29_CAM_POWERDNACTIVE_MASK) {
                        if (io_res->gpio_powerdown == camera_powerdown)
                            io_requested_in_camera = true;    
                    }
                    i++;
                }
            }
            
            ret = 0;        //ddl@rock-chips.com : flash is only a function, sensor is also run;
            if (io_requested_in_camera==false) {
                eprintk("%s flash pin(%d) init failed",gpio_res->dev_name,camera_flash);
                goto _rk_sensor_io_init_end_;
            }
        }

        if (rk_camera_platform_data.iomux(camera_flash) < 0) {
            printk("%s flash pin(%d) iomux init failed\n",gpio_res->dev_name,camera_flash);                            
        }
        
		gpio_res->gpio_init |= RK29_CAM_FLASHACTIVE_MASK;
        gpio_set_value(camera_flash, ((~camera_ioflag&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));    /* falsh off */
        gpio_direction_output(camera_flash, ((~camera_ioflag&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));

		dprintk("%s flash pin(%d) init success(0x%x)",gpio_res->dev_name, camera_flash,((camera_ioflag&RK29_CAM_FLASHACTIVE_MASK)>>RK29_CAM_FLASHACTIVE_BITPOS));

    }  


	if (camera_af != INVALID_GPIO) {
		ret = gpio_request(camera_af, "camera af");
		if (ret) {
			io_requested_in_camera = false;
			for (i=0; i<RK_CAM_NUM; i++) {
				io_res = &rk_camera_platform_data.gpio_res[i];
				if (io_res->gpio_init & RK29_CAM_AFACTIVE_MASK) {
					if (io_res->gpio_af == camera_af)
						io_requested_in_camera = true;	  
				}
			}

			if (io_requested_in_camera==false) {
				i=0;
				while (strstr(new_camera[i].dev_name,"end")==NULL) {
					io_res = &new_camera[i].io;
					if (io_res->gpio_init & RK29_CAM_AFACTIVE_MASK) {
						if (io_res->gpio_af == camera_af)
							io_requested_in_camera = true;	  
					}
					i++;
				}
			}
			
			if (io_requested_in_camera==false) {
				eprintk("%s af pin(%d) init failed",gpio_res->dev_name,camera_af);
				goto _rk_sensor_io_init_end_;
			} else {
                ret =0;
            }
			
		}

		if (rk_camera_platform_data.iomux(camera_af) < 0) {
			 ret = -1;
			eprintk("%s af pin(%d) iomux init failed\n",gpio_res->dev_name,camera_af); 	
            goto _rk_sensor_io_init_end_;			
		}
		
		gpio_res->gpio_init |= RK29_CAM_AFACTIVE_MASK;
		gpio_set_value(camera_af, GPIO_HIGH);	
		gpio_direction_output(camera_af, ((~camera_ioflag&RK29_CAM_AFACTIVE_MASK)>>RK29_CAM_AFACTIVE_BITPOS));
		dprintk("%s af pin(%d) init success(0x%x)",gpio_res->dev_name, camera_af,((camera_ioflag&RK29_CAM_AFACTIVE_MASK)>>RK29_CAM_AFACTIVE_BITPOS));

	}


	
_rk_sensor_io_init_end_:
    return ret;

}

static int _rk_sensor_io_deinit_(struct rk29camera_gpio_res *gpio_res)
{
    unsigned int camera_reset = INVALID_GPIO, camera_power = INVALID_GPIO;
	unsigned int camera_powerdown = INVALID_GPIO, camera_flash = INVALID_GPIO,camera_af = INVALID_GPIO;
    
    camera_reset = gpio_res->gpio_reset;
    camera_power = gpio_res->gpio_power;
	camera_powerdown = gpio_res->gpio_powerdown;
    camera_flash = gpio_res->gpio_flash;
    camera_af = gpio_res->gpio_af;

	if (gpio_res->gpio_init & RK29_CAM_POWERACTIVE_MASK) {
	    if (camera_power != INVALID_GPIO) {
	        gpio_direction_input(camera_power);
	        gpio_free(camera_power);
	    }
	}

	if (gpio_res->gpio_init & RK29_CAM_RESETACTIVE_MASK) {
	    if (camera_reset != INVALID_GPIO)  {
	        gpio_direction_input(camera_reset);
	        gpio_free(camera_reset);
	    }
	}

	if (gpio_res->gpio_init & RK29_CAM_POWERDNACTIVE_MASK) {
	    if (camera_powerdown != INVALID_GPIO)  {
	        gpio_direction_input(camera_powerdown);
	        gpio_free(camera_powerdown);
	    }
	}

	if (gpio_res->gpio_init & RK29_CAM_FLASHACTIVE_MASK) {
	    if (camera_flash != INVALID_GPIO)  {
	        gpio_direction_input(camera_flash);
	        gpio_free(camera_flash);
	    }
	}
	if (gpio_res->gpio_init & RK29_CAM_AFACTIVE_MASK) {
	    if (camera_af != INVALID_GPIO)  {
	       // gpio_direction_input(camera_af);
	        gpio_free(camera_af);
	    }
	}	
	gpio_res->gpio_init = 0;
	
    return 0;
}

static int rk_sensor_io_init(void)
{
    int i,j;
	static bool is_init = false;
	struct rk29camera_platform_data* plat_data = &rk_camera_platform_data;

    if(is_init) {		
		return 0;
	} else {
		is_init = true;
	}
    
    if (sensor_ioctl_cb.sensor_power_cb == NULL)
        sensor_ioctl_cb.sensor_power_cb = sensor_power_default_cb;
    if (sensor_ioctl_cb.sensor_reset_cb == NULL)
        sensor_ioctl_cb.sensor_reset_cb = sensor_reset_default_cb;
    if (sensor_ioctl_cb.sensor_powerdown_cb == NULL)
        sensor_ioctl_cb.sensor_powerdown_cb = sensor_powerdown_default_cb;
    if (sensor_ioctl_cb.sensor_flash_cb == NULL)
        sensor_ioctl_cb.sensor_flash_cb = sensor_flash_default_cb;
    if (sensor_ioctl_cb.sensor_af_cb == NULL)
        sensor_ioctl_cb.sensor_af_cb = sensor_afpower_default_cb;	
    
	for(i = 0;i < RK_CAM_NUM; i++) {
        if (plat_data->gpio_res[i].dev_name == NULL)
            continue;
        
		if (_rk_sensor_io_init_(&plat_data->gpio_res[i])<0)
            goto sensor_io_init_erro;
        
        for (j=0; j<10; j++) {
            memset(&plat_data->info[i].fival[j],0x00,sizeof(struct v4l2_frmivalenum));

            if (j==0) {
                plat_data->info[i].fival[j].width = 176;
                plat_data->info[i].fival[j].height = 144;
            } else if (j==1) {
                plat_data->info[i].fival[j].width = 320;
                plat_data->info[i].fival[j].height = 240;
            } else if (j==2) {
                plat_data->info[i].fival[j].width = 352;
                plat_data->info[i].fival[j].height = 288;
            } else if (j==3) {
                plat_data->info[i].fival[j].width = 640;
                plat_data->info[i].fival[j].height = 480;
            } else if (j==4) {
                plat_data->info[i].fival[j].width = 720;
                plat_data->info[i].fival[j].height = 480;
            } else if (j==5) {
                plat_data->info[i].fival[j].width = 1280;
                plat_data->info[i].fival[j].height = 720;
            } else if (j==6) {
                plat_data->info[i].fival[j].width = 240;
                plat_data->info[i].fival[j].height = 160;
            } else if (j==7) {
                plat_data->info[i].fival[j].width = 800;
                plat_data->info[i].fival[j].height = 600;
            } 
            if (plat_data->info[i].fival[j].width && plat_data->info[i].fival[j].height) {
                rk29_sensor_fps_get(i,&plat_data->info[i].fival[j].discrete.denominator,
                    plat_data->info[i].fival[j].width,plat_data->info[i].fival[j].height);
                plat_data->info[i].fival[j].discrete.numerator= 1000;
                plat_data->info[i].fival[j].index = 0;
                plat_data->info[i].fival[j].pixel_format = V4L2_PIX_FMT_NV12;
                plat_data->info[i].fival[j].type = V4L2_FRMIVAL_TYPE_DISCRETE;
            }
        }
        
	continue;
sensor_io_init_erro:
		_rk_sensor_io_deinit_(&plat_data->gpio_res[i]);
	}
    
    i = 0;
    while (strstr(new_camera[i].dev_name,"end")==NULL) {
        if (_rk_sensor_io_init_(&new_camera[i].io)<0)
            _rk_sensor_io_deinit_(&new_camera[i].io);

        i++;
    }
	return 0;
}

static int rk_sensor_io_deinit(int sensor)
{
    int i;
	struct rk29camera_platform_data* plat_data = &rk_camera_platform_data;
    
    for(i = 0;i < RK_CAM_NUM; i++) {
        if (plat_data->gpio_res[i].dev_name == NULL)
            continue;
        
		_rk_sensor_io_deinit_(&plat_data->gpio_res[i]);
    }

    i = 0;
    while (strstr(new_camera[i].dev_name,"end")==NULL) {
        _rk_sensor_io_deinit_(&new_camera[i].io);

        i++;
    }
    
    return 0;
}
static int rk_sensor_ioctrl(struct device *dev,enum rk29camera_ioctrl_cmd cmd, int on)
{
    struct rk29camera_gpio_res *res = NULL;
    struct rkcamera_platform_data *new_cam_dev = NULL;
	struct rk29camera_platform_data* plat_data = &rk_camera_platform_data;
    int ret = RK29_CAM_IO_SUCCESS,i = 0;
    struct soc_camera_link *dev_icl = NULL;

    //for test reg
	for(i = 0;i < RK_CAM_NUM;i++){
		if(plat_data->gpio_res[i].dev_name &&  (strcmp(plat_data->gpio_res[i].dev_name, dev_name(dev)) == 0)) {
			res = (struct rk29camera_gpio_res *)&plat_data->gpio_res[i];
            dev_icl = &plat_data->register_dev[i].link_info;
			break;
	    } 
    }

    if (res == NULL) {
        i = 0;
        while (strstr(new_camera[i].dev_name,"end")==NULL) {
            if (strcmp(new_camera[i].dev_name, dev_name(dev)) == 0) {
                res = (struct rk29camera_gpio_res *)&new_camera[i].io; 
                new_cam_dev = &new_camera[i];
                dev_icl = &new_camera[i].dev.link_info;
                break;
            }
            i++;
        }
    }
    
    if (res == NULL) {
        eprintk("%s is not regisiterd in rk29_camera_platform_data!!",dev_name(dev));
        ret = RK29_CAM_EIO_INVALID;
        goto rk_sensor_ioctrl_end;
    }
	
	switch (cmd)
 	{
 		case Cam_Power:
		{
			if (sensor_ioctl_cb.sensor_power_cb) {
                ret = sensor_ioctl_cb.sensor_power_cb(res, on);   
                ret = (ret != RK29_CAM_EIO_INVALID)?ret:0;     /* ddl@rock-chips.com: v0.1.1 */ 
			} else {
                eprintk("sensor_ioctl_cb.sensor_power_cb is NULL");
                WARN_ON(1);
			}

			printk("ret: %d\n",ret);
			break;
		}
		case Cam_Reset:
		{
			if (sensor_ioctl_cb.sensor_reset_cb) {
                ret = sensor_ioctl_cb.sensor_reset_cb(res, on);

                ret = (ret != RK29_CAM_EIO_INVALID)?ret:0;
			} else {
                eprintk( "sensor_ioctl_cb.sensor_reset_cb is NULL");
                WARN_ON(1);
			}
			break;
		}

		case Cam_PowerDown:
		{
			if (sensor_ioctl_cb.sensor_powerdown_cb) {
                ret = sensor_ioctl_cb.sensor_powerdown_cb(res, on);
			} else {
                eprintk( "sensor_ioctl_cb.sensor_powerdown_cb is NULL");
                WARN_ON(1);
			}
			break;
		}

		case Cam_Flash:
		{
			if (sensor_ioctl_cb.sensor_flash_cb) {
                ret = sensor_ioctl_cb.sensor_flash_cb(res, on);
			} else {
                eprintk( "sensor_ioctl_cb.sensor_flash_cb is NULL!");
                WARN_ON(1);
			}
			break;
		}
		
		case Cam_Af:
		{
			if (sensor_ioctl_cb.sensor_af_cb) {
                ret = sensor_ioctl_cb.sensor_af_cb(res, on);
			} else {
                eprintk( "sensor_ioctl_cb.sensor_af_cb is NULL!");
                WARN_ON(1);
			}
			break;
		}

        case Cam_Mclk:
        {
            if (plat_data->sensor_mclk && dev_icl) {
                plat_data->sensor_mclk(dev_icl->bus_id,(on!=0)?1:0,on);
            } else { 
                eprintk( "%s(%d): sensor_mclk(%p) or dev_icl(%p) is NULL",
                    __FUNCTION__,__LINE__,plat_data->sensor_mclk,dev_icl);
            }
            break;
        }
        
		default:
		{
			eprintk("%s cmd(0x%x) is unknown!",__FUNCTION__, cmd);
			break;
		}
 	}
rk_sensor_ioctrl_end:
    return ret;
}

static int rk_sensor_pwrseq(struct device *dev,int powerup_sequence, int on, int mclk_rate)
{
    int ret =0;
    int i,powerup_type;
    
    for (i=0; i<8; i++) {

        if (on == 1)
            powerup_type = SENSOR_PWRSEQ_GET(powerup_sequence,i);
        else
            powerup_type = SENSOR_PWRSEQ_GET(powerup_sequence,(7-i));
        
        switch (powerup_type)
        {
            case SENSOR_PWRSEQ_AVDD:
            case SENSOR_PWRSEQ_DOVDD:
            case SENSOR_PWRSEQ_DVDD:
            case SENSOR_PWRSEQ_PWR:
            {  
                ret = rk_sensor_ioctrl(dev,Cam_Power, on);
                if (ret<0) {
                    eprintk("SENSOR_PWRSEQ_PWR failed");
                } else { 
                    msleep(10);
                    dprintk("SensorPwrSeq-power: %d",on);
                }
                break;
            }

            case SENSOR_PWRSEQ_HWRST:
            {
                if(!on){
                    rk_sensor_ioctrl(dev,Cam_Reset, 1);
                }else{
                    ret = rk_sensor_ioctrl(dev,Cam_Reset, 1);
                    msleep(2);
                    ret |= rk_sensor_ioctrl(dev,Cam_Reset, 0); 
                }
                if (ret<0) {
                    eprintk("SENSOR_PWRSEQ_HWRST failed");
                } else {
                    dprintk("SensorPwrSeq-reset: %d",on);
                }
                break;
            }

            case SENSOR_PWRSEQ_PWRDN:
            {     
                ret = rk_sensor_ioctrl(dev,Cam_PowerDown, !on);
                if (ret<0) {
                    eprintk("SENSOR_PWRSEQ_PWRDN failed");
                } else {
                    dprintk("SensorPwrSeq-power down: %d",!on);
                }
                break;
            }

            case SENSOR_PWRSEQ_CLKIN:
            {
                ret = rk_sensor_ioctrl(dev,Cam_Mclk, (on?mclk_rate:on));
                if (ret<0) {
                    eprintk("SENSOR_PWRSEQ_CLKIN failed");
                } else {
                    dprintk("SensorPwrSeq-clock: %d",on);
                }
                break;
            }

            default:
                break;
        }
        
    } 

    return ret;
}

static int rk_sensor_power(struct device *dev, int on)
{
    int i, powerup_sequence,mclk_rate;
    
    struct rk29camera_platform_data* plat_data = &rk_camera_platform_data;
    struct rk29camera_gpio_res *dev_io = NULL;
    struct rkcamera_platform_data *new_camera=NULL, *new_device=NULL;
    bool real_pwroff = true;
    int ret = 0;
    
    //ddl@rock-chips.com: other sensor must switch into standby before turn on power;
    for(i = 0;i < RK_CAM_NUM; i++) {
        if (plat_data->gpio_res[i].dev_name == NULL)
            continue;

        if (plat_data->gpio_res[i].gpio_powerdown != INVALID_GPIO) {
            gpio_direction_output(plat_data->gpio_res[i].gpio_powerdown,
                ((plat_data->gpio_res[i].gpio_flag&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));            
        }
        
        if (strcmp(plat_data->gpio_res[i].dev_name,dev_name(dev))) {
            if (sensor_ioctl_cb.sensor_powerdown_cb && on)
                sensor_ioctl_cb.sensor_powerdown_cb(&plat_data->gpio_res[i],1);
        } else {            
            dev_io = &plat_data->gpio_res[i];
            real_pwroff = true;
        }
    }

    new_camera = plat_data->register_dev_new;
    while (strstr(new_camera->dev_name,"end")==NULL) {

        if (new_camera->io.gpio_powerdown != INVALID_GPIO) {
            gpio_direction_output(new_camera->io.gpio_powerdown,
                ((new_camera->io.gpio_flag&RK29_CAM_POWERDNACTIVE_MASK)>>RK29_CAM_POWERDNACTIVE_BITPOS));            
        }
        
        if (strcmp(new_camera->dev_name,dev_name(dev))) {
            if (sensor_ioctl_cb.sensor_powerdown_cb && on)
                sensor_ioctl_cb.sensor_powerdown_cb(&new_camera->io,1);
        } else {
            new_device = new_camera;
            dev_io = &new_camera->io;
            
            if (!Sensor_Support_DirectResume(new_camera->pwdn_info))
                real_pwroff = true;
            else
                real_pwroff = false;
        }
        new_camera++;
    }

    if (new_device != NULL) {
        powerup_sequence = new_device->powerup_sequence;
        if ((new_device->mclk_rate == 24) || (new_device->mclk_rate == 48))
            mclk_rate = new_device->mclk_rate*1000000;
        else 
            mclk_rate = 24000000;
    } else {
        powerup_sequence = sensor_PWRSEQ_DEFAULT;
        mclk_rate = 24000000;
    }
        
    if (on) {
        rk_sensor_pwrseq(dev, powerup_sequence, on,mclk_rate);  
    } else {
        if (real_pwroff) {
            if (rk_sensor_pwrseq(dev, powerup_sequence, on,mclk_rate)<0)    /* ddl@rock-chips.com: v0.1.5 */
                goto PowerDown;
            
            /*ddl@rock-chips.com: all power down switch to Hi-Z after power off*/
            for(i = 0;i < RK_CAM_NUM; i++) {
                if (plat_data->gpio_res[i].dev_name == NULL)
                    continue;
                if (plat_data->gpio_res[i].gpio_powerdown != INVALID_GPIO) {
                    gpio_direction_input(plat_data->gpio_res[i].gpio_powerdown);            
                }
            }
            
            new_camera = plat_data->register_dev_new;
            while (strstr(new_camera->dev_name,"end")==NULL) {
                if (new_camera->io.gpio_powerdown != INVALID_GPIO) {
                    gpio_direction_input(new_camera->io.gpio_powerdown);            
                }
                new_camera->pwdn_info |= 0x01;
                new_camera++;
            }
        } else {  
PowerDown:
            rk_sensor_ioctrl(dev,Cam_PowerDown, !on);

            rk_sensor_ioctrl(dev,Cam_Mclk, 0);
        }

        mdelay(10);/* ddl@rock-chips.com: v0.1.3 */
    }
    return ret;
}
#if 0
static int rk_sensor_reset(struct device *dev)
{
#if 0
	rk_sensor_ioctrl(dev,Cam_Reset,1);
	msleep(2);
	rk_sensor_ioctrl(dev,Cam_Reset,0);
#else
    /*
    *ddl@rock-chips.com : the rest function invalidate, because this operate is put together in rk_sensor_power;
    */
#endif
	return 0;
}
#endif
static int rk_sensor_powerdown(struct device *dev, int on)
{
	return rk_sensor_ioctrl(dev,Cam_PowerDown,on);
}
#if ((defined PMEM_CAM_NECESSARY)&&(defined CONFIG_VIDEO_RK29_CAMMEM_PMEM))
static struct android_pmem_platform_data android_pmem_cam_pdata = {
	.name		= "pmem_cam",
	.start		= PMEM_CAM_BASE,
	.size		= PMEM_CAM_SIZE,
	.no_allocator	= 1,
	.cached		= 1,
};

static struct platform_device android_pmem_cam_device = {
	.name		= "android_pmem",
	.id		= 1,
	.dev		= {
		.platform_data = &android_pmem_cam_pdata,
	},
};
#endif
#ifdef CONFIG_RK_CONFIG
int camera_set_platform_param(int id, int i2c, int gpio)
{
        int i;
        char *dev_name[] = {
                SENSOR_DEVICE_NAME_0, 
                SENSOR_DEVICE_NAME_01, 
                SENSOR_DEVICE_NAME_02, 
                SENSOR_DEVICE_NAME_1, 
                SENSOR_DEVICE_NAME_11, 
                SENSOR_DEVICE_NAME_12
        };
        char *module_name[] = {
                SENSOR_NAME_0,
                SENSOR_NAME_01,
                SENSOR_NAME_02,
                SENSOR_NAME_1,
                SENSOR_NAME_11,
                SENSOR_NAME_12
        };

        if(id < 0 || id >= 6)
                return -EINVAL;
        for(i = 0; i < 6; i++){
            if(i == id){
                printk("%s: id = %d, i2c = %d, gpio = %d\n", __func__, id, i2c, gpio);
            }
            if(rk_camera_platform_data.gpio_res[i].dev_name &&
               strcmp(rk_camera_platform_data.gpio_res[i].dev_name, dev_name[id]) == 0)
                    rk_camera_platform_data.gpio_res[i].gpio_powerdown = gpio;
            if(rk_camera_platform_data.register_dev[i].link_info.module_name &&
               strcmp(rk_camera_platform_data.register_dev[i].link_info.module_name, module_name[id]) == 0)
                    rk_camera_platform_data.register_dev[i].link_info.i2c_adapter_id = i2c;
        }

        return 0;
}
#else
int camera_set_platform_param(int id, int i2c, int gpio)
{
        return 0;
}
#endif

int rk_sensor_register(void)
{
    int i;
    
    i = 0;
    while (strstr(new_camera[i].dev.device_info.dev.init_name,"end")==NULL) {

        if (new_camera[i].dev.i2c_cam_info.addr == INVALID_VALUE) {
            WARN(1, 
                KERN_ERR "%s(%d): new_camera[%d] i2c addr is invalidate!",
                __FUNCTION__,__LINE__,i);
            continue;
        }
        
        sprintf(new_camera[i].dev_name,"%s_%d",new_camera[i].dev.device_info.dev.init_name,i+3);
        new_camera[i].dev.device_info.dev.init_name =(const char*)&new_camera[i].dev_name[0];
        new_camera[i].io.dev_name =(const char*)&new_camera[i].dev_name[0];
        
        if (new_camera[i].orientation == INVALID_VALUE) {
            if (strstr(new_camera[i].dev_name,"back")) {
                new_camera[i].orientation = 90;
            } else {
                new_camera[i].orientation = 270;
            }
        }
        /* ddl@rock-chips.com: v0.1.3 */
        if ((new_camera[i].fov_h <= 0) || (new_camera[i].fov_h>360))
            new_camera[i].fov_h = 100;
        
        if ((new_camera[i].fov_v <= 0) || (new_camera[i].fov_v>360))
            new_camera[i].fov_v = 100;        

        new_camera[i].dev.link_info.power = rk_sensor_power;
        new_camera[i].dev.link_info.powerdown = rk_sensor_powerdown; 

        new_camera[i].dev.link_info.board_info =&new_camera[i].dev.i2c_cam_info;
        new_camera[i].dev.device_info.id = i+6;
        new_camera[i].dev.device_info.dev.platform_data = &new_camera[i].dev.link_info;

        if (new_camera[i].flash == true) {
            if (CONFIG_SENSOR_FLASH_IOCTL_USR == 0) {
                eprintk("new_camera[%d] have been configed as attach flash, but CONFIG_SENSOR_FLASH_IOCTL_USR isn't turn on!",i);

                new_camera[i].flash = false;    
            }
        }

        new_camera[i].dev.link_info.priv_usr = &rk_camera_platform_data;
        platform_device_register(&new_camera[i].dev.device_info);
        i++;
    }
    
    sprintf(new_camera[i].dev_name,"%s",new_camera[i].dev.device_info.dev.init_name);

    return 0;
}
#endif

