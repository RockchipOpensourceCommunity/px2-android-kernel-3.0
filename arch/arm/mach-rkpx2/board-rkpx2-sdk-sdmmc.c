/* arch/arm/mach-rkpx2/board-rkpx2-sdk-sdmmc.c
 *
 * Copyright (C) 2012 ROCKCHIP, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * *
 * History:
 * ver1.0 add combo-wifi operateions. such as commit e049351a09c78db8a08aa5c49ce8eba0a3d6824e, at 2012-09-16
 * ver2.0 Unify all the file versions of board_xxxx_sdmmc.c, at 2012-11-05
 * ver3.0 Unify the interface functions of new-iomux-API due to the modify in IOMUX module, at 2013-01-17
 *
 * Content:
 * Part 1: define the gpio for SD-MMC-SDIO-Wifi functions according to your own projects.
         ***********************************************************************************
        * Please set the value according to your own project.
        ***********************************************************************************
 *
 * Part 2: define the gpio for the SDMMC controller. Based on the chip datasheet.
        ***********************************************************************************
        * Please do not change, each platform has a fixed set.  !!!!!!!!!!!!!!!!!!
        *  The system personnel will set the value depending on the specific arch datasheet,
        *  such as RK29XX, RK30XX.
        * If you have any doubt, please consult BangWang Xie.
        ***********************************************************************************
 *
 *.Part 3: The various operations of the SDMMC-SDIO module
        ***********************************************************************************
        * Please do not change, each platform has a fixed set.  !!!!!!!!!!!!!!!!!!
        * define the varaious operations for SDMMC module
        * Generally only the author of SDMMC module will modify this section.
        * If you have any doubt, please consult BangWang Xie.
        ***********************************************************************************
 *
 *.Part 4: The various operations of the Wifi-BT module
        ***********************************************************************************
        * Please do not change, each module has a fixed set.  !!!!!!!!!!!!!!!!!!
        * define the varaious operations for Wifi module
        * Generally only the author of Wifi module will modify this section.
        * If you have any doubt, please consult BangWang Xie, Weiguo Hu, and Weilong Gao.
        ***********************************************************************************
 *
 */
//use the new iomux-API
#define SDMMC_USE_NEW_IOMUX_API 0


//1.Part 1: define the gpio for SD-MMC-SDIO-Wifi functions  according to your own projects.

/*************************************************************************
* define the gpio for sd-sdio-wifi module
*************************************************************************/
#if defined(CONFIG_SDMMC0_RK29_WRITE_PROTECT)
#define SDMMC0_WRITE_PROTECT_PIN	         RK30_PIN3_PB2	//According to your own project to set the value of write-protect-pin.
#define SDMMC0_WRITE_PROTECT_ENABLE_VALUE    GPIO_HIGH
#endif 

#if defined(CONFIG_SDMMC1_RK29_WRITE_PROTECT)
#define SDMMC1_WRITE_PROTECT_PIN	RK30_PIN3_PB3	//According to your own project to set the value of write-protect-pin.
#define SDMMC1_WRITE_PROTECT_ENABLE_VALUE    GPIO_HIGH
#endif
    
#if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
#define RK29SDK_WIFI_SDIO_CARD_INT         RK30_PIN3_PD2
#endif

//refer to file /arch/arm/mach-rkpx2/include/mach/Iomux.h
//define reset-pin
#define RK29SDK_SD_CARD_DETECT_N                RK30_PIN3_PB6  //According to your own project to set the value of card-detect-pin.
#define RK29SDK_SD_CARD_INSERT_LEVEL            GPIO_LOW         // set the voltage of insert-card. Please pay attention to the default setting.
#define RK29SDK_SD_CARD_DETECT_PIN_NAME         GPIO3B6_SDMMC0DETECTN_NAME
#define RK29SDK_SD_CARD_DETECT_IOMUX_FGPIO      GPIO3B_GPIO3B6
#define RK29SDK_SD_CARD_DETECT_IOMUX_FMUX       GPIO3B_SDMMC0_DETECT_N
    
//
// Define wifi module's power and reset gpio, and gpio sensitive level.
// Please set the value according to your own project.
//
    // refer to file /arch/arm/mach-rkpx2/include/mach/Iomux.h

    //power
    #define RK30SDK_WIFI_GPIO_POWER_N               RK30_PIN3_PD0            
    #define RK30SDK_WIFI_GPIO_POWER_ENABLE_VALUE    GPIO_HIGH        

	//wake up host gpio
	#define RK30SDK_WIFI_GPIO_WIFI_INT_B                RK30_PIN3_PD2
    #define RK30SDK_WIFI_GPIO_WIFI_INT_B_ENABLE_VALUE   GPIO_HIGH
    
//1. Part 2: to define the gpio for the SDMMC controller. Based on the chip datasheet.
/*************************************************************************
* define the gpio for SDMMC module on various platforms
* Generally only system personnel will modify this part
*************************************************************************/

//refer to file /arch/arm/mach-rkpx2/include/mach/Iomux.h
//define PowerEn-pin
#define RK29SDK_SD_CARD_PWR_EN                  RK30_PIN3_PA7 
#define RK29SDK_SD_CARD_PWR_EN_LEVEL            GPIO_LOW
#define RK29SDK_SD_CARD_PWR_EN_PIN_NAME         GPIO3A7_SDMMC0PWREN_NAME
#define RK29SDK_SD_CARD_PWR_EN_IOMUX_FGPIO      GPIO3A_GPIO3A7
#define RK29SDK_SD_CARD_PWR_EN_IOMUX_FMUX       GPIO3A_SDMMC0_PWR_EN

/*
* define the gpio for sdmmc0
*/
struct rksdmmc_gpio_board rksdmmc0_gpio_init = {

     .clk_gpio       = {
        .io             = RK30_PIN3_PB0,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3B0_SDMMC0CLKOUT_NAME,
            .fgpio      = GPIO3B_GPIO3B0,
            .fmux       = GPIO3B_SDMMC0_CLKOUT,
        },
    },   

    .cmd_gpio           = {
        .io             = RK30_PIN3_PB1,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3B1_SDMMC0CMD_NAME,
            .fgpio      = GPIO3B_GPIO3B1,
            .fmux       = GPIO3B_SDMMC0_CMD,
        },
    },      

   .data0_gpio       = {
        .io             = RK30_PIN3_PB2,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3B2_SDMMC0DATA0_NAME,
            .fgpio      = GPIO3B_GPIO3B2,
            .fmux       = GPIO3B_SDMMC0_DATA0,
        },
    },      

    .data1_gpio       = {
        .io             = RK30_PIN3_PB3,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3B3_SDMMC0DATA1_NAME,
            .fgpio      = GPIO3B_GPIO3B3,
            .fmux       = GPIO3B_SDMMC0_DATA1,
        },
    },      

    .data2_gpio       = {
        .io             = RK30_PIN3_PB4,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3B4_SDMMC0DATA2_NAME,
            .fgpio      = GPIO3B_GPIO3B4,
            .fmux       = GPIO3B_SDMMC0_DATA2,
        },
    }, 

    .data3_gpio       = {
        .io             = RK30_PIN3_PB5,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3B5_SDMMC0DATA3_NAME,
            .fgpio      = GPIO3B_GPIO3B5,
            .fmux       = GPIO3B_SDMMC0_DATA3,
        },
    }, 
    
    .power_en_gpio      = {   
#if defined(RK29SDK_SD_CARD_PWR_EN) || (INVALID_GPIO != RK29SDK_SD_CARD_PWR_EN)
        .io             = RK29SDK_SD_CARD_PWR_EN,
        .enable         = RK29SDK_SD_CARD_PWR_EN_LEVEL,
        #ifdef RK29SDK_SD_CARD_PWR_EN_PIN_NAME
        .iomux          = {
            .name       = RK29SDK_SD_CARD_PWR_EN_PIN_NAME,
            #ifdef RK29SDK_SD_CARD_PWR_EN_IOMUX_FGPIO
            .fgpio      = RK29SDK_SD_CARD_PWR_EN_IOMUX_FGPIO,
            #endif
            #ifdef RK29SDK_SD_CARD_PWR_EN_IOMUX_FMUX
            .fmux       = RK29SDK_SD_CARD_PWR_EN_IOMUX_FMUX,
            #endif
        },
        #endif
#else
        .io             = INVALID_GPIO,
        .enable         = GPIO_LOW,
#endif
    }, 

    .detect_irq       = {
#if defined(RK29SDK_SD_CARD_DETECT_N) || (INVALID_GPIO != RK29SDK_SD_CARD_DETECT_N)  
        .io             = RK29SDK_SD_CARD_DETECT_N,
        .enable         = RK29SDK_SD_CARD_INSERT_LEVEL,
        #ifdef RK29SDK_SD_CARD_DETECT_PIN_NAME
        .iomux          = {
            .name       = RK29SDK_SD_CARD_DETECT_PIN_NAME,
            #ifdef RK29SDK_SD_CARD_DETECT_IOMUX_FGPIO
            .fgpio      = RK29SDK_SD_CARD_DETECT_IOMUX_FGPIO,
            #endif
            #ifdef RK29SDK_SD_CARD_DETECT_IOMUX_FMUX
            .fmux       = RK29SDK_SD_CARD_DETECT_IOMUX_FMUX,
            #endif
        },
        #endif
#else
        .io             = INVALID_GPIO,
        .enable         = GPIO_LOW,
#endif            
    },
};


/*
* define the gpio for sdmmc1
*/
static struct rksdmmc_gpio_board rksdmmc1_gpio_init = {

     .clk_gpio       = {
        .io             = RK30_PIN3_PC5,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3C5_SDMMC1CLKOUT_NAME,
            .fgpio      = GPIO3C_GPIO3C5,
            .fmux       = GPIO3B_SDMMC0_CLKOUT,
        },
    },   

    .cmd_gpio           = {
        .io             = RK30_PIN3_PC0,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3C0_SMMC1CMD_NAME,
            .fgpio      = GPIO3C_GPIO3C0,
            .fmux       = GPIO3B_SDMMC0_CMD,
        },
    },      

   .data0_gpio       = {
        .io             = RK30_PIN3_PC1,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3C1_SDMMC1DATA0_NAME,
            .fgpio      = GPIO3C_GPIO3C1,
            .fmux       = GPIO3B_SDMMC0_DATA0,
        },
    },      

    .data1_gpio       = {
        .io             = RK30_PIN3_PC2,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3C2_SDMMC1DATA1_NAME,
            .fgpio      = GPIO3C_GPIO3C2,
            .fmux       = GPIO3B_SDMMC0_DATA1,
        },
    },      

    .data2_gpio       = {
        .io             = RK30_PIN3_PC3,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3C3_SDMMC1DATA2_NAME,
            .fgpio      = GPIO3C_GPIO3C3,
            .fmux       = GPIO3B_SDMMC0_DATA2,
        },
    }, 

    .data3_gpio       = {
        .io             = RK30_PIN3_PC4,
        .enable         = GPIO_HIGH,
        .iomux          = {
            .name       = GPIO3C4_SDMMC1DATA3_NAME,
            .fgpio      = GPIO3C_GPIO3C4,
            .fmux       = GPIO3B_SDMMC0_DATA3,
        },
    }, 
};

//1.Part 3: The various operations of the SDMMC-SDIO module
/*************************************************************************
* define the varaious operations for SDMMC module
* Generally only the author of SDMMC module will modify this section.
*************************************************************************/

#if !defined(CONFIG_SDMMC_RK29_OLD)	
void rk29_sdmmc_gpio_open(int device_id, int on)
{
    switch(device_id)
    {
        case 0://mmc0
        {
            #ifdef CONFIG_SDMMC0_RK29
            if(on)
            {
                gpio_direction_output(rksdmmc0_gpio_init.clk_gpio.io, GPIO_HIGH);//set mmc0-clk to high
                gpio_direction_output(rksdmmc0_gpio_init.cmd_gpio.io, GPIO_HIGH);// set mmc0-cmd to high.
                gpio_direction_output(rksdmmc0_gpio_init.data0_gpio.io,GPIO_HIGH);//set mmc0-data0 to high.
                gpio_direction_output(rksdmmc0_gpio_init.data1_gpio.io,GPIO_HIGH);//set mmc0-data1 to high.
                gpio_direction_output(rksdmmc0_gpio_init.data2_gpio.io,GPIO_HIGH);//set mmc0-data2 to high.
                gpio_direction_output(rksdmmc0_gpio_init.data3_gpio.io,GPIO_HIGH);//set mmc0-data3 to high.

                mdelay(30);
            }
            else
            {
                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc0_gpio_init.clk_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc0_gpio_init.clk_gpio.iomux.name, rksdmmc0_gpio_init.clk_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc0_gpio_init.clk_gpio.io, "mmc0-clk");
                gpio_direction_output(rksdmmc0_gpio_init.clk_gpio.io,GPIO_LOW);//set mmc0-clk to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc0_gpio_init.cmd_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc0_gpio_init.cmd_gpio.iomux.name, rksdmmc0_gpio_init.cmd_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc0_gpio_init.cmd_gpio.io, "mmc0-cmd");
                gpio_direction_output(rksdmmc0_gpio_init.cmd_gpio.io,GPIO_LOW);//set mmc0-cmd to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc0_gpio_init.data0_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc0_gpio_init.data0_gpio.iomux.name, rksdmmc0_gpio_init.data0_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc0_gpio_init.data0_gpio.io, "mmc0-data0");
                gpio_direction_output(rksdmmc0_gpio_init.data0_gpio.io,GPIO_LOW);//set mmc0-data0 to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc0_gpio_init.data1_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc0_gpio_init.data1_gpio.iomux.name, rksdmmc0_gpio_init.data1_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc0_gpio_init.data1_gpio.io, "mmc0-data1");
                gpio_direction_output(rksdmmc0_gpio_init.data1_gpio.io,GPIO_LOW);//set mmc0-data1 to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc0_gpio_init.data2_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc0_gpio_init.data2_gpio.iomux.name, rksdmmc0_gpio_init.data2_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc0_gpio_init.data2_gpio.io, "mmc0-data2");
                gpio_direction_output(rksdmmc0_gpio_init.data2_gpio.io,GPIO_LOW);//set mmc0-data2 to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc0_gpio_init.data3_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc0_gpio_init.data3_gpio.iomux.name, rksdmmc0_gpio_init.data3_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc0_gpio_init.data3_gpio.io, "mmc0-data3");
                gpio_direction_output(rksdmmc0_gpio_init.data3_gpio.io,GPIO_LOW);//set mmc0-data3 to low.

                mdelay(30);
            }
            #endif
        }
        break;
        
        case 1://mmc1
        {
            #ifdef CONFIG_SDMMC1_RK29
            if(on)
            {
                gpio_direction_output(rksdmmc1_gpio_init.clk_gpio.io,GPIO_HIGH);//set mmc1-clk to high
                gpio_direction_output(rksdmmc1_gpio_init.cmd_gpio.io,GPIO_HIGH);//set mmc1-cmd to high.
                gpio_direction_output(rksdmmc1_gpio_init.data0_gpio.io,GPIO_HIGH);//set mmc1-data0 to high.
                gpio_direction_output(rksdmmc1_gpio_init.data1_gpio.io,GPIO_HIGH);//set mmc1-data1 to high.
                gpio_direction_output(rksdmmc1_gpio_init.data2_gpio.io,GPIO_HIGH);//set mmc1-data2 to high.
                gpio_direction_output(rksdmmc1_gpio_init.data3_gpio.io,GPIO_HIGH);//set mmc1-data3 to high.
                mdelay(100);
            }
            else
            {
                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc1_gpio_init.clk_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc1_gpio_init.clk_gpio.iomux.name, rksdmmc1_gpio_init.clk_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc1_gpio_init.clk_gpio.io, "mmc1-clk");
                gpio_direction_output(rksdmmc1_gpio_init.clk_gpio.io,GPIO_LOW);//set mmc1-clk to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc1_gpio_init.cmd_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc1_gpio_init.cmd_gpio.iomux.name, rksdmmc1_gpio_init.cmd_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc1_gpio_init.cmd_gpio.io, "mmc1-cmd");
                gpio_direction_output(rksdmmc1_gpio_init.cmd_gpio.io,GPIO_LOW);//set mmc1-cmd to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc1_gpio_init.data0_gpio.iomux.fgpio);
                #else
                rk30_mux_api_set(rksdmmc1_gpio_init.data0_gpio.iomux.name, rksdmmc1_gpio_init.data0_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc1_gpio_init.data0_gpio.io, "mmc1-data0");
                gpio_direction_output(rksdmmc1_gpio_init.data0_gpio.io,GPIO_LOW);//set mmc1-data0 to low.
                
            #if defined(CONFIG_WIFI_COMBO_MODULE_CONTROL_FUNC) || defined(CONFIG_MT5931) || defined(CONFIG_MT5931_MT6622)
                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc1_gpio_init.data1_gpio.iomux.fgpio);
                #else
                rk29_mux_api_set(rksdmmc1_gpio_init.data1_gpio.iomux.name, rksdmmc1_gpio_init.data1_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc1_gpio_init.data1_gpio.io, "mmc1-data1");
                gpio_direction_output(rksdmmc1_gpio_init.data1_gpio.io,GPIO_LOW);//set mmc1-data1 to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc1_gpio_init.data2_gpio.iomux.fgpio);
                #else
                rk29_mux_api_set(rksdmmc1_gpio_init.data2_gpio.iomux.name, rksdmmc1_gpio_init.data2_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc1_gpio_init.data2_gpio.io, "mmc1-data2");
                gpio_direction_output(rksdmmc1_gpio_init.data2_gpio.io,GPIO_LOW);//set mmc1-data2 to low.

                #if SDMMC_USE_NEW_IOMUX_API
                iomux_set(rksdmmc1_gpio_init.data3_gpio.iomux.fgpio);
                #else
                rk29_mux_api_set(rksdmmc1_gpio_init.data3_gpio.iomux.name, rksdmmc1_gpio_init.data3_gpio.iomux.fgpio);
                #endif
                gpio_request(rksdmmc1_gpio_init.data3_gpio.io, "mmc1-data3");
                gpio_direction_output(rksdmmc1_gpio_init.data3_gpio.io,GPIO_LOW);//set mmc1-data3 to low.
           #endif
                mdelay(100);
            }
            #endif
        }
        break; 
        
        case 2: //mmc2
        break;
        
        default:
        break;
    }
}

static void rk29_sdmmc_set_iomux_mmc0(unsigned int bus_width)
{
    switch (bus_width)
    {
        
    	case 1://SDMMC_CTYPE_4BIT:
    	{
    	    #if SDMMC_USE_NEW_IOMUX_API
            iomux_set(rksdmmc0_gpio_init.data1_gpio.iomux.fmux);
            iomux_set(rksdmmc0_gpio_init.data2_gpio.iomux.fmux);
            iomux_set(rksdmmc0_gpio_init.data3_gpio.iomux.fmux);
    	    #else
        	rk30_mux_api_set(rksdmmc0_gpio_init.data1_gpio.iomux.name, rksdmmc0_gpio_init.data1_gpio.iomux.fmux);
        	rk30_mux_api_set(rksdmmc0_gpio_init.data2_gpio.iomux.name, rksdmmc0_gpio_init.data2_gpio.iomux.fmux);
        	rk30_mux_api_set(rksdmmc0_gpio_init.data3_gpio.iomux.name, rksdmmc0_gpio_init.data3_gpio.iomux.fmux);
        	#endif
    	}
    	break;

    	case 0x10000://SDMMC_CTYPE_8BIT:
    	    break;
    	case 0xFFFF: //gpio_reset
    	{
    	    #if SDMMC_USE_NEW_IOMUX_API
            iomux_set(rksdmmc0_gpio_init.power_en_gpio.iomux.fgpio);
            #else
            rk30_mux_api_set(rksdmmc0_gpio_init.power_en_gpio.iomux.name, rksdmmc0_gpio_init.power_en_gpio.iomux.fgpio);
            #endif
            gpio_request(rksdmmc0_gpio_init.power_en_gpio.io,"sdmmc-power");
            gpio_direction_output(rksdmmc0_gpio_init.power_en_gpio.io, !(rksdmmc0_gpio_init.power_en_gpio.enable)); //power-off

        #if 0 //replace the power control into rk29_sdmmc_set_ios(); modifyed by xbw at 2012-08-12
            rk29_sdmmc_gpio_open(0, 0);

            gpio_direction_output(rksdmmc0_gpio_init.power_en_gpio.io, rksdmmc0_gpio_init.power_en_gpio.enable); //power-on

            rk29_sdmmc_gpio_open(0, 1);
          #endif  
    	}
    	break;

    	default: //case 0://SDMMC_CTYPE_1BIT:
        {
            #if SDMMC_USE_NEW_IOMUX_API
        	iomux_set(rksdmmc0_gpio_init.cmd_gpio.iomux.fmux);
        	iomux_set(rksdmmc0_gpio_init.clk_gpio.iomux.fmux);
        	iomux_set(rksdmmc0_gpio_init.data0_gpio.iomux.fmux);
            #else
        	rk30_mux_api_set(rksdmmc0_gpio_init.cmd_gpio.iomux.name, rksdmmc0_gpio_init.cmd_gpio.iomux.fmux);
        	rk30_mux_api_set(rksdmmc0_gpio_init.clk_gpio.iomux.name, rksdmmc0_gpio_init.clk_gpio.iomux.fmux);
        	rk30_mux_api_set(rksdmmc0_gpio_init.data0_gpio.iomux.name, rksdmmc0_gpio_init.data0_gpio.iomux.fmux);
        	#endif

            #if SDMMC_USE_NEW_IOMUX_API
            iomux_set(rksdmmc0_gpio_init.data1_gpio.iomux.fgpio);
            #else
            rk30_mux_api_set(rksdmmc0_gpio_init.data1_gpio.iomux.name, rksdmmc0_gpio_init.data1_gpio.iomux.fgpio);
            #endif
            gpio_request(rksdmmc0_gpio_init.data1_gpio.io, "mmc0-data1");
            gpio_direction_output(rksdmmc0_gpio_init.data1_gpio.io,GPIO_HIGH);//set mmc0-data1 to high.

            #if SDMMC_USE_NEW_IOMUX_API
            iomux_set(rksdmmc0_gpio_init.data2_gpio.iomux.fgpio);
            #else
            rk30_mux_api_set(rksdmmc0_gpio_init.data2_gpio.iomux.name, rksdmmc0_gpio_init.data2_gpio.iomux.fgpio);
            #endif
            gpio_request(rksdmmc0_gpio_init.data2_gpio.io, "mmc0-data2");
            gpio_direction_output(rksdmmc0_gpio_init.data2_gpio.io,GPIO_HIGH);//set mmc0-data2 to high.

            #if SDMMC_USE_NEW_IOMUX_API
            iomux_set(rksdmmc0_gpio_init.data3_gpio.iomux.fgpio);
            #else
            rk30_mux_api_set(rksdmmc0_gpio_init.data3_gpio.iomux.name, rksdmmc0_gpio_init.data3_gpio.iomux.fgpio);
            #endif
            gpio_request(rksdmmc0_gpio_init.data3_gpio.io, "mmc0-data3");
            gpio_direction_output(rksdmmc0_gpio_init.data3_gpio.io,GPIO_HIGH);//set mmc0-data3 to high.
    	}
    	break;
	}
}

static void rk29_sdmmc_set_iomux_mmc1(unsigned int bus_width)
{
    #if SDMMC_USE_NEW_IOMUX_API
    iomux_set(rksdmmc1_gpio_init.cmd_gpio.iomux.fmux);
    iomux_set(rksdmmc1_gpio_init.clk_gpio.iomux.fmux);
    iomux_set(rksdmmc1_gpio_init.data0_gpio.iomux.fmux);
    iomux_set(rksdmmc1_gpio_init.data1_gpio.iomux.fmux);
    iomux_set(rksdmmc1_gpio_init.data2_gpio.iomux.fmux);
    iomux_set(rksdmmc1_gpio_init.data3_gpio.iomux.fmux);
    #else
    rk30_mux_api_set(rksdmmc1_gpio_init.cmd_gpio.iomux.name, rksdmmc1_gpio_init.cmd_gpio.iomux.fmux);
    rk30_mux_api_set(rksdmmc1_gpio_init.clk_gpio.iomux.name, rksdmmc1_gpio_init.clk_gpio.iomux.fmux);
    rk30_mux_api_set(rksdmmc1_gpio_init.data0_gpio.iomux.name, rksdmmc1_gpio_init.data0_gpio.iomux.fmux);
    rk30_mux_api_set(rksdmmc1_gpio_init.data1_gpio.iomux.name, rksdmmc1_gpio_init.data1_gpio.iomux.fmux);
    rk30_mux_api_set(rksdmmc1_gpio_init.data2_gpio.iomux.name, rksdmmc1_gpio_init.data2_gpio.iomux.fmux);
    rk30_mux_api_set(rksdmmc1_gpio_init.data3_gpio.iomux.name, rksdmmc1_gpio_init.data3_gpio.iomux.fmux);
    #endif
}

static void rk29_sdmmc_set_iomux_mmc2(unsigned int bus_width)
{
    ;//
}

static void rk29_sdmmc_set_iomux(int device_id, unsigned int bus_width)
{
    switch(device_id)
    {
        case 0:
            #ifdef CONFIG_SDMMC0_RK29
            rk29_sdmmc_set_iomux_mmc0(bus_width);
            #endif
            break;
        case 1:
            #ifdef CONFIG_SDMMC1_RK29
            rk29_sdmmc_set_iomux_mmc1(bus_width);
            #endif
            break;
        case 2:
            rk29_sdmmc_set_iomux_mmc2(bus_width);
            break;
        default:
            break;
    }    
}

#endif



//1.Part 4: The various operations of the Wifi-BT module
/*************************************************************************
* define the varaious operations for Wifi module
* Generally only the author of Wifi module will modify this section.
*************************************************************************/

static int rk29sdk_wifi_status(struct device *dev);
static int rk29sdk_wifi_status_register(void (*callback)(int card_presend, void *dev_id), void *dev_id);

static int rk29sdk_wifi_cd = 0;   /* wifi virtual 'card detect' status */
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

struct rksdmmc_gpio_wifi_moudle  rk_platform_wifi_gpio = {
    .power_n = {
            .io             = RK30SDK_WIFI_GPIO_POWER_N, 
            .enable         = RK30SDK_WIFI_GPIO_POWER_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_POWER_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_POWER_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_POWER_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_POWER_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_POWER_IOMUX_FMUX,
                #endif
            },
            #endif
     },
     
    #ifdef RK30SDK_WIFI_GPIO_RESET_N
    .reset_n = {
            .io             = RK30SDK_WIFI_GPIO_RESET_N,
            .enable         = RK30SDK_WIFI_GPIO_RESET_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_RESET_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_RESET_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_RESET_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_RESET_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_RESET_IOMUX_FMUX,
                #endif
            },
            #endif
    },
    #endif
    
    #ifdef RK30SDK_WIFI_GPIO_WIFI_INT_B
    .wifi_int_b = {
            .io             = RK30SDK_WIFI_GPIO_WIFI_INT_B,
            .enable         = RK30SDK_WIFI_GPIO_WIFI_INT_B_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_WIFI_INT_B_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_WIFI_INT_B_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_WIFI_INT_B_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_WIFI_INT_B_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_WIFI_INT_B_IOMUX_FMUX,
                #endif
            },
            #endif
     },
     #endif

    #ifdef RK30SDK_WIFI_GPIO_VCCIO_WL 
    .vddio = {
            .io             = RK30SDK_WIFI_GPIO_VCCIO_WL,
            .enable         = RK30SDK_WIFI_GPIO_VCCIO_WL_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_VCCIO_WL_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_VCCIO_WL_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_VCCIO_WL_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_VCCIO_WL_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_VCCIO_WL_IOMUX_FMUX,
                #endif
            },
            #endif
     },
     #endif
     
     #ifdef RK30SDK_WIFI_GPIO_BGF_INT_B
    .bgf_int_b = {
            .io             = RK30SDK_WIFI_GPIO_BGF_INT_B,
            .enable         = RK30SDK_WIFI_GPIO_BGF_INT_B_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_BGF_INT_B_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_BGF_INT_B_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_BGF_INT_B_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_BGF_INT_B_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_BGF_INT_B_IOMUX_FMUX,
                #endif
            },
            #endif
        },       
    #endif
    
    #ifdef RK30SDK_WIFI_GPIO_GPS_SYNC
    .gps_sync = {
            .io             = RK30SDK_WIFI_GPIO_GPS_SYNC,
            .enable         = RK30SDK_WIFI_GPIO_GPS_SYNC_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_GPS_SYNC_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_GPS_SYNC_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_GPS_SYNC_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_GPS_SYNC_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_GPS_SYNC_IOMUX_FMUX,
                #endif
            },
            #endif
    },
    #endif
    
#if defined(COMBO_MODULE_MT6620_CDT) && COMBO_MODULE_MT6620_CDT
    #ifdef RK30SDK_WIFI_GPIO_ANTSEL2
    .ANTSEL2 = {
            .io             = RK30SDK_WIFI_GPIO_ANTSEL2,
            .enable         = RK30SDK_WIFI_GPIO_ANTSEL2_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_ANTSEL2_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_ANTSEL2_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_ANTSEL2_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_ANTSEL2_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_ANTSEL2_IOMUX_FMUX,
                #endif
            },
            #endif
    },
    #endif

    #ifdef RK30SDK_WIFI_GPIO_ANTSEL3
    .ANTSEL3 = {
            .io             = RK30SDK_WIFI_GPIO_ANTSEL3,
            .enable         = RK30SDK_WIFI_GPIO_ANTSEL3_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_ANTSEL3_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_ANTSEL3_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_ANTSEL3_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_ANTSEL3_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_ANTSEL3_IOMUX_FMUX,
                #endif
            },
            #endif
    },
    #endif

    #ifdef RK30SDK_WIFI_GPIO_GPS_LAN
    .GPS_LAN = {
            .io             = RK30SDK_WIFI_GPIO_GPS_LAN,
            .enable         = RK30SDK_WIFI_GPIO_GPS_LAN_ENABLE_VALUE,
            #ifdef RK30SDK_WIFI_GPIO_GPS_LAN_PIN_NAME
            .iomux          = {
                .name       = RK30SDK_WIFI_GPIO_GPS_LAN_PIN_NAME,
                .fgpio      = RK30SDK_WIFI_GPIO_GPS_LAN_IOMUX_FGPIO,
                #ifdef RK30SDK_WIFI_GPIO_GPS_LAN_IOMUX_FMUX
                .fmux       = RK30SDK_WIFI_GPIO_GPS_LAN_IOMUX_FMUX,
                #endif
            },
            #endif
    },
    #endif
#endif // #if COMBO_MODULE_MT6620_CDT--#endif   
};

#define PREALLOC_WLAN_SEC_NUM           4
#define PREALLOC_WLAN_BUF_NUM           160
#define PREALLOC_WLAN_SECTION_HEADER    24

#define WLAN_SECTION_SIZE_0     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2     (PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3     (PREALLOC_WLAN_BUF_NUM * 1024)

#define WLAN_SKB_BUF_NUM        16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
        void *mem_ptr;
        unsigned long size;
};

static struct wifi_mem_prealloc wifi_mem_array[PREALLOC_WLAN_SEC_NUM] = {
        {NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
        {NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
        {NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
        {NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *rk29sdk_mem_prealloc(int section, unsigned long size)
{
        if (section == PREALLOC_WLAN_SEC_NUM)
                return wlan_static_skb;

        if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
                return NULL;

        if (wifi_mem_array[section].size < size)
                return NULL;

        return wifi_mem_array[section].mem_ptr;
}

static int __init rk29sdk_init_wifi_mem(void)
{
        int i;
        int j;

        for (i = 0 ; i < WLAN_SKB_BUF_NUM ; i++) {
                wlan_static_skb[i] = dev_alloc_skb(
                                ((i < (WLAN_SKB_BUF_NUM / 2)) ? 4096 : 8192));

                if (!wlan_static_skb[i])
                        goto err_skb_alloc;
        }

        for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
                wifi_mem_array[i].mem_ptr =
                                kmalloc(wifi_mem_array[i].size, GFP_KERNEL);

                if (!wifi_mem_array[i].mem_ptr)
                        goto err_mem_alloc;
        }
        return 0;

err_mem_alloc:
        pr_err("Failed to mem_alloc for WLAN\n");
        for (j = 0 ; j < i ; j++)
               kfree(wifi_mem_array[j].mem_ptr);

        i = WLAN_SKB_BUF_NUM;

err_skb_alloc:
        pr_err("Failed to skb_alloc for WLAN\n");
        for (j = 0 ; j < i ; j++)
                dev_kfree_skb(wlan_static_skb[j]);

        return -ENOMEM;
}

static int rk29sdk_wifi_status(struct device *dev)
{
        return rk29sdk_wifi_cd;
}

static int rk29sdk_wifi_status_register(void (*callback)(int card_present, void *dev_id), void *dev_id)
{
        if(wifi_status_cb)
                return -EAGAIN;
        wifi_status_cb = callback;
        wifi_status_cb_devid = dev_id;
        return 0;
}

static int __init rk29sdk_wifi_bt_gpio_control_init(void)
{
    rk29sdk_init_wifi_mem();    
    rk29_mux_api_set(rk_platform_wifi_gpio.power_n.iomux.name, rk_platform_wifi_gpio.power_n.iomux.fgpio);

    if (rk_platform_wifi_gpio.power_n.io != INVALID_GPIO) {
        if (gpio_request(rk_platform_wifi_gpio.power_n.io, "wifi_power")) {
               pr_info("%s: request wifi power gpio failed\n", __func__);
               return -1;
        }
    }

#ifdef RK30SDK_WIFI_GPIO_RESET_N
    if (rk_platform_wifi_gpio.reset_n.io != INVALID_GPIO) {
        if (gpio_request(rk_platform_wifi_gpio.reset_n.io, "wifi reset")) {
               pr_info("%s: request wifi reset gpio failed\n", __func__);
               gpio_free(rk_platform_wifi_gpio.power_n.io);
               return -1;
        }
    }
#endif    

    if (rk_platform_wifi_gpio.power_n.io != INVALID_GPIO)
        gpio_direction_output(rk_platform_wifi_gpio.power_n.io, !(rk_platform_wifi_gpio.power_n.enable) );

#ifdef RK30SDK_WIFI_GPIO_RESET_N 
    if (rk_platform_wifi_gpio.reset_n.io != INVALID_GPIO)
        gpio_direction_output(rk_platform_wifi_gpio.reset_n.io, !(rk_platform_wifi_gpio.reset_n.enable) );
#endif    

    #if defined(CONFIG_SDMMC1_RK29) && !defined(CONFIG_SDMMC_RK29_OLD)
    
    #if !defined(CONFIG_MT5931) && !defined(CONFIG_MT5931_MT6622)  
    rk29_mux_api_set(rksdmmc1_gpio_init.data1_gpio.iomux.name, rksdmmc1_gpio_init.data1_gpio.iomux.fgpio);
    gpio_request(rksdmmc1_gpio_init.data1_gpio.io, "mmc1-data1");
    gpio_direction_output(rksdmmc1_gpio_init.data1_gpio.io,GPIO_LOW);//set mmc1-data1 to low.

    rk29_mux_api_set(rksdmmc1_gpio_init.data2_gpio.iomux.name, rksdmmc1_gpio_init.data2_gpio.iomux.fgpio);
    gpio_request(rksdmmc1_gpio_init.data2_gpio.io, "mmc1-data2");
    gpio_direction_output(rksdmmc1_gpio_init.data2_gpio.io,GPIO_LOW);//set mmc1-data2 to low.

    rk29_mux_api_set(rksdmmc1_gpio_init.data3_gpio.iomux.name,  rksdmmc1_gpio_init.data3_gpio.iomux.fgpio);
    gpio_request(rksdmmc1_gpio_init.data3_gpio.io, "mmc1-data3");
    gpio_direction_output(rksdmmc1_gpio_init.data3_gpio.io,GPIO_LOW);//set mmc1-data3 to low.
    #endif
    
    rk29_sdmmc_gpio_open(1, 0); //added by xbw at 2011-10-13
    #endif    
    pr_info("%s: init finished\n",__func__);

    return 0;
}

int rk29sdk_wifi_power(int on)
{
        pr_info("%s: %d\n", __func__, on);
        if (on){
                gpio_set_value(rk_platform_wifi_gpio.power_n.io, rk_platform_wifi_gpio.power_n.enable);
                mdelay(50);

                #if defined(CONFIG_SDMMC1_RK29) && !defined(CONFIG_SDMMC_RK29_OLD)	
                rk29_sdmmc_gpio_open(1, 1); //added by xbw at 2011-10-13
                #endif

            #ifdef RK30SDK_WIFI_GPIO_RESET_N
                if (rk_platform_wifi_gpio.reset_n.io != INVALID_GPIO)
                    gpio_set_value(rk_platform_wifi_gpio.reset_n.io, rk_platform_wifi_gpio.reset_n.enable);
            #endif                
                mdelay(100);
                pr_info("wifi turn on power\n");
        }else{
//                if (!rk29sdk_bt_power_state){
                        gpio_set_value(rk_platform_wifi_gpio.power_n.io, !(rk_platform_wifi_gpio.power_n.enable));

                        #if defined(CONFIG_SDMMC1_RK29) && !defined(CONFIG_SDMMC_RK29_OLD)	
                        rk29_sdmmc_gpio_open(1, 0); //added by xbw at 2011-10-13
                        #endif
                        
                        mdelay(100);
                        pr_info("wifi shut off power\n");
//                }else
//                {
//                        pr_info("wifi shouldn't shut off power, bt is using it!\n");
//                }
#ifdef RK30SDK_WIFI_GPIO_RESET_N
                if (rk_platform_wifi_gpio.reset_n.io != INVALID_GPIO)
                    gpio_set_value(rk_platform_wifi_gpio.reset_n.io, !(rk_platform_wifi_gpio.reset_n.enable));
#endif 
        }

//        rk29sdk_wifi_power_state = on;
        return 0;
}
EXPORT_SYMBOL(rk29sdk_wifi_power);

static int rk29sdk_wifi_reset_state;
static int rk29sdk_wifi_reset(int on)
{
        pr_info("%s: %d\n", __func__, on);
        //mdelay(100);
        rk29sdk_wifi_reset_state = on;
        return 0;
}

int rk29sdk_wifi_set_carddetect(int val)
{
        pr_info("%s:%d\n", __func__, val);
        rk29sdk_wifi_cd = val;
        if (wifi_status_cb){
                wifi_status_cb(val, wifi_status_cb_devid);
        }else {
                pr_warning("%s, nobody to notify\n", __func__);
        }
        return 0;
}
EXPORT_SYMBOL(rk29sdk_wifi_set_carddetect);


static struct resource resources[] = {
	{
		.start = RK30SDK_WIFI_GPIO_WIFI_INT_B,
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
		.name = "bcmdhd_wlan_irq",
	},
};
 //#if defined(CONFIG_WIFI_CONTROL_FUNC)----#elif

///////////////////////////////////////////////////////////////////////////////////

#if defined(CONFIG_WIFI_CONTROL_FUNC)
static struct wifi_platform_data rk29sdk_wifi_control = {
        .set_power = rk29sdk_wifi_power,
        .set_reset = rk29sdk_wifi_reset,
        .set_carddetect = rk29sdk_wifi_set_carddetect,
        .mem_prealloc   = rk29sdk_mem_prealloc,
};

static struct platform_device rk29sdk_wifi_device = {
        .name = "bcmdhd_wlan",
        .id = 1,
        .num_resources = ARRAY_SIZE(resources),
        .resource = resources,
        .dev = {
                .platform_data = &rk29sdk_wifi_control,
         },
};
#endif


