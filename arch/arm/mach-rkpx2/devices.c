/* arch/arm/mach-rkpx2/devices.c
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
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/pmu.h>
#include <mach/irqs.h>
#include <mach/board.h>
#include <plat/dma-pl330.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/debug_uart.h>
#include <plat/rk_fiq_debugger.h>
#ifdef CONFIG_RK_REMOTECTL
#include <mach/remotectl.h>
#endif

#ifdef CONFIG_ADC_RKPX2
static struct adc_platform_data rk30_adc_pdata = {
        .ref_volt = 2500, //2500mV
        .base_chn = -1,
};
static struct resource rk30_adc_resource[] = {
	{
		.start	= IRQ_SARADC,
		.end	= IRQ_SARADC,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_SARADC_PHYS,
		.end	= RK30_SARADC_PHYS + RK30_SARADC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device device_adc = {
	.name		= "rk30-adc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rk30_adc_resource),
	.resource	= rk30_adc_resource,
        .dev            = {
		.platform_data = &rk30_adc_pdata,
        },
};
#endif

#if defined(IRQ_TSADC)
static struct resource rk30_tsadc_resource[] = {
	{
		.start	= IRQ_TSADC,
		.end	= IRQ_TSADC,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_TSADC_PHYS,
		.end	= RK30_TSADC_PHYS + RK30_TSADC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_tsadc = {
	.name		= "rk30-tsadc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rk30_tsadc_resource),
	.resource	= rk30_tsadc_resource,
};

static void __init rk30_init_tsadc(void)
{
	platform_device_register(&device_tsadc);
}
#else
static void __init rk30_init_tsadc(void) {}
#endif

static u64 dma_dmamask = DMA_BIT_MASK(32);

static struct resource resource_dmac1[] = {
	[0] = {
		.start  = RK30_DMACS1_PHYS,
		.end    = RK30_DMACS1_PHYS + RK30_DMACS1_SIZE -1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_DMAC1_0,
		.end	= IRQ_DMAC1_1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct rk29_pl330_platdata dmac1_pdata = {
	.peri = {
		[0] = DMACH_UART0_TX,
		[1] = DMACH_UART0_RX,
		[2] = DMACH_UART1_TX,
		[3] = DMACH_UART1_RX,
		[4] = DMACH_I2S0_8CH_TX,
		[5] = DMACH_I2S0_8CH_RX,
		[6] = DMACH_I2S1_2CH_TX,
		[7] = DMACH_I2S1_2CH_RX,
		[8] = DMACH_SPDIF_TX,
		[9] = DMACH_I2S2_2CH_TX,
		[10] = DMACH_I2S2_2CH_RX,
		[11] = DMACH_DMAC0_MEMTOMEM,
		[12] = DMACH_MAX,
		[13] = DMACH_MAX,
		[14] = DMACH_MAX,
		[15] = DMACH_MAX,
		[16] = DMACH_MAX,
		[17] = DMACH_MAX,
		[18] = DMACH_MAX,
		[19] = DMACH_MAX,
		[20] = DMACH_MAX,
		[21] = DMACH_MAX,
		[22] = DMACH_MAX,
		[23] = DMACH_MAX,
		[24] = DMACH_MAX,
		[25] = DMACH_MAX,
		[26] = DMACH_MAX,
		[27] = DMACH_MAX,
		[28] = DMACH_MAX,
		[29] = DMACH_MAX,
		[30] = DMACH_MAX,
		[31] = DMACH_MAX,
	},
};

static struct platform_device device_dmac1 = {
	.name		= "rk29-pl330",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resource_dmac1),
	.resource	= resource_dmac1,
	.dev		= {
		.dma_mask = &dma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &dmac1_pdata,
	},
};

static struct resource resource_dmac2[] = {
	[0] = {
		.start  = RK30_DMAC2_PHYS,
		.end    = RK30_DMAC2_PHYS + RK30_DMAC2_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_DMAC2_0,
		.end	= IRQ_DMAC2_1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct rk29_pl330_platdata dmac2_pdata = {
	.peri = {
		[0] = DMACH_HSADC,
		[1] = DMACH_SDMMC,
		[2] = DMACH_MAX,
		[3] = DMACH_SDIO,
		[4] = DMACH_EMMC,
		[5] = DMACH_PID_FILTER,
		[6] = DMACH_UART2_TX,
		[7] = DMACH_UART2_RX,
		[8] = DMACH_UART3_TX,
		[9] = DMACH_UART3_RX,
		[10] = DMACH_SPI0_TX,
		[11] = DMACH_SPI0_RX,
		[12] = DMACH_SPI1_TX,
		[13] = DMACH_SPI1_RX,
		[14] = DMACH_DMAC1_MEMTOMEM,
		[15] = DMACH_MAX,
		[16] = DMACH_MAX,
		[17] = DMACH_MAX,
		[18] = DMACH_MAX,
		[19] = DMACH_MAX,
		[20] = DMACH_MAX,
		[21] = DMACH_MAX,
		[22] = DMACH_MAX,
		[23] = DMACH_MAX,
		[24] = DMACH_MAX,
		[25] = DMACH_MAX,
		[26] = DMACH_MAX,
		[27] = DMACH_MAX,
		[28] = DMACH_MAX,
		[29] = DMACH_MAX,
		[30] = DMACH_MAX,
		[31] = DMACH_MAX,
	},
};

static struct platform_device device_dmac2 = {
	.name		= "rk29-pl330",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(resource_dmac2),
	.resource	= resource_dmac2,
	.dev		= {
		.dma_mask = &dma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &dmac2_pdata,
	},
};

static struct platform_device *rk30_dmacs[] __initdata = {
	&device_dmac1,
	&device_dmac2,
};

static void __init rk30_init_dma(void)
{
	platform_add_devices(rk30_dmacs, ARRAY_SIZE(rk30_dmacs));
}

#ifdef CONFIG_UART0_RK29
static struct resource resources_uart0[] = {
	{
		.start	= IRQ_UART0,
		.end	= IRQ_UART0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_UART0_PHYS,
		.end	= RK30_UART0_PHYS + RK30_UART0_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_uart0 = {
	.name	= "rk_serial",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart0),
	.resource	= resources_uart0,
};
#endif

#ifdef CONFIG_UART1_RK29
static struct resource resources_uart1[] = {
	{
		.start	= IRQ_UART1,
		.end	= IRQ_UART1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_UART1_PHYS,
		.end	= RK30_UART1_PHYS + RK30_UART1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_uart1 = {
	.name	= "rk_serial",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_uart1),
	.resource	= resources_uart1,
};
#endif

#ifdef CONFIG_UART2_RK29
static struct resource resources_uart2[] = {
	{
		.start	= IRQ_UART2,
		.end	= IRQ_UART2,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_UART2_PHYS,
		.end	= RK30_UART2_PHYS + RK30_UART2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_uart2 = {
	.name	= "rk_serial",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(resources_uart2),
	.resource	= resources_uart2,
};
#endif

#ifdef CONFIG_UART3_RK29
static struct resource resources_uart3[] = {
	{
		.start	= IRQ_UART3,
		.end	= IRQ_UART3,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_UART3_PHYS,
		.end	= RK30_UART3_PHYS + RK30_UART3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_uart3 = {
	.name	= "rk_serial",
	.id	= 3,
	.num_resources	= ARRAY_SIZE(resources_uart3),
	.resource	= resources_uart3,
};
#endif

#ifdef CONFIG_RK_REMOTECTL
void rk30_remotectl_iomux(void){	
;}

struct RKxx_remotectl_platform_data rk30_remotectl_pdata = {    
	.gpio	=   RK30_PIN6_PA1,     
	.wakeup	= 1,    
	.rep    = 0,    
	.set_iomux = rk30_remotectl_iomux,    
};

static struct platform_device rk30_device_remotectl = {	
	.name		= "rkxx-remotectl",	
	.id		= -1,	
	.dev		= {		
		.platform_data	= &rk30_remotectl_pdata,
	},
};
#endif

static void __init rk30_init_uart(void)
{
#ifdef CONFIG_UART0_RK29
	platform_device_register(&device_uart0);
#endif
#ifdef CONFIG_UART1_RK29
	platform_device_register(&device_uart1);
#endif
#ifdef CONFIG_UART2_RK29
	platform_device_register(&device_uart2);
#endif
#ifdef CONFIG_UART3_RK29
	platform_device_register(&device_uart3);
#endif
}

#ifdef CONFIG_I2C0_CONTROLLER_RK29
#define I2C0_ADAP_TYPE  I2C_RK29_ADAP
#define I2C0_START      RK30_I2C0_PHYS
#define I2C0_END        RK30_I2C0_PHYS + SZ_4K - 1
#endif
#ifdef CONFIG_I2C0_CONTROLLER_RKPX2
#define I2C0_ADAP_TYPE   I2C_RK30_ADAP
#define I2C0_START      RK30_I2C0_PHYS + SZ_4K
#define I2C0_END        RK30_I2C0_PHYS + SZ_8K - 1
#endif

#ifdef CONFIG_I2C1_CONTROLLER_RK29
#define I2C1_ADAP_TYPE  I2C_RK29_ADAP
#define I2C1_START      RK30_I2C1_PHYS
#define I2C1_END        RK30_I2C1_PHYS + SZ_4K - 1
#endif
#ifdef CONFIG_I2C1_CONTROLLER_RKPX2
#define I2C1_ADAP_TYPE   I2C_RK30_ADAP
#define I2C1_START      RK30_I2C1_PHYS + SZ_4K
#define I2C1_END        RK30_I2C1_PHYS + SZ_8K - 1
#endif

#ifdef CONFIG_I2C2_CONTROLLER_RK29
#define I2C2_ADAP_TYPE  I2C_RK29_ADAP
#define I2C2_START      RK30_I2C2_PHYS
#define I2C2_END        RK30_I2C2_PHYS + SZ_8K - 1
#endif
#ifdef CONFIG_I2C2_CONTROLLER_RKPX2
#define I2C2_ADAP_TYPE   I2C_RK30_ADAP
#define I2C2_START      RK30_I2C2_PHYS + SZ_8K
#define I2C2_END        RK30_I2C2_PHYS + SZ_16K - 1
#endif

#ifdef CONFIG_I2C3_CONTROLLER_RK29
#define I2C3_ADAP_TYPE  I2C_RK29_ADAP
#define I2C3_START      RK30_I2C3_PHYS
#define I2C3_END        RK30_I2C3_PHYS + SZ_8K - 1
#endif
#ifdef CONFIG_I2C3_CONTROLLER_RKPX2
#define I2C3_ADAP_TYPE   I2C_RK30_ADAP
#define I2C3_START      RK30_I2C3_PHYS + SZ_8K
#define I2C3_END        RK30_I2C3_PHYS + SZ_16K - 1
#endif

#ifdef CONFIG_I2C4_CONTROLLER_RK29
#define I2C4_ADAP_TYPE  I2C_RK29_ADAP
#define I2C4_START      RK30_I2C4_PHYS
#define I2C4_END        RK30_I2C4_PHYS + SZ_8K - 1
#endif
#ifdef CONFIG_I2C4_CONTROLLER_RKPX2
#define I2C4_ADAP_TYPE   I2C_RK30_ADAP
#define I2C4_START      RK30_I2C4_PHYS + SZ_8K
#define I2C4_END        RK30_I2C4_PHYS + SZ_16K - 1
#endif

#ifdef CONFIG_I2C0_RKPX2
static struct rk30_i2c_platform_data default_i2c0_data = {
	.bus_num = 0,
	.is_div_from_arm = 1,
	.adap_type = I2C0_ADAP_TYPE,
	.sda_mode = I2C0_SDA,
	.scl_mode = I2C0_SCL,
};

static struct resource resources_i2c0[] = {
	{
		.start	= IRQ_I2C0,
		.end	= IRQ_I2C0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= I2C0_START,
        .end    = I2C0_END,    
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_i2c0 = {
	.name	= "rk30_i2c",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_i2c0),
	.resource	= resources_i2c0,
	.dev 		= {
		.platform_data = &default_i2c0_data,
	},
};
#endif

#ifdef CONFIG_I2C1_RKPX2
static struct rk30_i2c_platform_data default_i2c1_data = {
	.bus_num = 1,
	.is_div_from_arm = 1,
	.adap_type = I2C1_ADAP_TYPE,
	.sda_mode = I2C1_SDA,
	.scl_mode = I2C1_SCL,
};

static struct resource resources_i2c1[] = {
	{
		.start	= IRQ_I2C1,
		.end	= IRQ_I2C1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= I2C1_START,
        .end    = I2C1_END,    
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_i2c1 = {
	.name	= "rk30_i2c",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_i2c1),
	.resource	= resources_i2c1,
	.dev 		= {
		.platform_data = &default_i2c1_data,
	},
};
#endif

#ifdef CONFIG_I2C2_RKPX2
static struct rk30_i2c_platform_data default_i2c2_data = {
	.bus_num = 2,
	.is_div_from_arm = 0,
	.adap_type = I2C2_ADAP_TYPE,
	.sda_mode = I2C2_SDA,
	.scl_mode = I2C2_SCL,
};

static struct resource resources_i2c2[] = {
	{
		.start	= IRQ_I2C2,
		.end	= IRQ_I2C2,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= I2C2_START,
        .end    = I2C2_END,    
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_i2c2 = {
	.name	= "rk30_i2c",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(resources_i2c2),
	.resource	= resources_i2c2,
	.dev 		= {
		.platform_data = &default_i2c2_data,
	},
};
#endif

#ifdef CONFIG_I2C3_RKPX2
static struct rk30_i2c_platform_data default_i2c3_data = {
	.bus_num = 3,
	.is_div_from_arm = 0,
	.adap_type = I2C3_ADAP_TYPE,
	.sda_mode = I2C3_SDA,
	.scl_mode = I2C3_SCL,
};

static struct resource resources_i2c3[] = {
	{
		.start	= IRQ_I2C3,
		.end	= IRQ_I2C3,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= I2C3_START,
        .end    = I2C3_END,    
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_i2c3 = {
	.name	= "rk30_i2c",
	.id	= 3,
	.num_resources	= ARRAY_SIZE(resources_i2c3),
	.resource	= resources_i2c3,
	.dev 		= {
		.platform_data = &default_i2c3_data,
	},
};
#endif

#ifdef CONFIG_I2C4_RKPX2
static struct rk30_i2c_platform_data default_i2c4_data = {
	.bus_num = 4,
	.is_div_from_arm = 0,
	.adap_type = I2C4_ADAP_TYPE,
	.sda_mode = I2C4_SDA,
	.scl_mode = I2C4_SCL,
};

static struct resource resources_i2c4[] = {
	{
		.start	= IRQ_I2C4,
		.end	= IRQ_I2C4,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= I2C4_START,
        .end    = I2C4_END,    
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_i2c4 = {
	.name	= "rk30_i2c",
	.id	= 4,
	.num_resources	= ARRAY_SIZE(resources_i2c4),
	.resource	= resources_i2c4,
	.dev 		= {
		.platform_data = &default_i2c4_data,
	},
};
#endif

#ifdef CONFIG_I2C_GPIO_RKPX2
static struct platform_device device_i2c_gpio = {
        .name   = "i2c-gpio",
        .id = 5,
        .dev            = {
                .platform_data = &default_i2c_gpio_data,
        },
};
#endif

static void __init rk30_init_i2c(void)
{
#ifdef CONFIG_I2C0_RKPX2
	platform_device_register(&device_i2c0);
#endif
#ifdef CONFIG_I2C1_RKPX2
	platform_device_register(&device_i2c1);
#endif
#ifdef CONFIG_I2C2_RKPX2
	platform_device_register(&device_i2c2);
#endif
#ifdef CONFIG_I2C3_RKPX2
	platform_device_register(&device_i2c3);
#endif
#ifdef CONFIG_I2C4_RKPX2
	platform_device_register(&device_i2c4);
#endif
#ifdef CONFIG_I2C_GPIO_RKPX2
	platform_device_register(&device_i2c_gpio);
#endif
}

#if defined(CONFIG_SPIM0_RK29) || defined(CONFIG_SPIM1_RK29)
/*****************************************************************************************
 * spi devices
 * author: cmc@rock-chips.com
 *****************************************************************************************/
#define SPI_CHIPSELECT_NUM 2

static int spi_io_init(struct spi_cs_gpio *cs_gpios, int cs_num)
{
	int i;
	if (cs_gpios) {
		for (i = 0; i < cs_num; i++) {
			rk30_mux_api_set(cs_gpios[i].cs_iomux_name, cs_gpios[i].cs_iomux_mode);
		}
	}
	return 0;
}

static int spi_io_deinit(struct spi_cs_gpio *cs_gpios, int cs_num)
{
	return 0;
}

static int spi_io_fix_leakage_bug(void)
{
#if 0
	gpio_direction_output(RK29_PIN2_PC1, GPIO_LOW);
#endif
	return 0;
}

static int spi_io_resume_leakage_bug(void)
{
#if 0
	gpio_direction_output(RK29_PIN2_PC1, GPIO_HIGH);
#endif
	return 0;
}
#endif

/*
 * rk29xx spi master device
 */
#ifdef CONFIG_SPIM0_RK29
static struct spi_cs_gpio rk29xx_spi0_cs_gpios[SPI_CHIPSELECT_NUM] = {
};

static struct rk29xx_spi_platform_data rk29xx_spi0_platdata = {
	.num_chipselect = SPI_CHIPSELECT_NUM,
	.chipselect_gpios = rk29xx_spi0_cs_gpios,
	.io_init = spi_io_init,
	.io_deinit = spi_io_deinit,
	.io_fix_leakage_bug = spi_io_fix_leakage_bug,
	.io_resume_leakage_bug = spi_io_resume_leakage_bug,
};

static struct resource rk29_spi0_resources[] = {
	{
		.start	= IRQ_SPI0,
		.end	= IRQ_SPI0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_SPI0_PHYS,
		.end	= RK30_SPI0_PHYS + RK30_SPI0_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start  = DMACH_SPI0_TX,
		.end    = DMACH_SPI0_TX,
		.flags  = IORESOURCE_DMA,
	},
	{
		.start  = DMACH_SPI0_RX,
		.end    = DMACH_SPI0_RX,
		.flags  = IORESOURCE_DMA,
	},
};

struct platform_device rk29xx_device_spi0m = {
	.name	= "rk29xx_spim",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(rk29_spi0_resources),
	.resource	= rk29_spi0_resources,
	.dev			= {
		.dma_mask = &dma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data	= &rk29xx_spi0_platdata,
	},
};
#endif

#ifdef CONFIG_SPIM1_RK29
static struct spi_cs_gpio rk29xx_spi1_cs_gpios[SPI_CHIPSELECT_NUM] = {
};

static struct rk29xx_spi_platform_data rk29xx_spi1_platdata = {
	.num_chipselect = SPI_CHIPSELECT_NUM,
	.chipselect_gpios = rk29xx_spi1_cs_gpios,
	.io_init = spi_io_init,
	.io_deinit = spi_io_deinit,
	.io_fix_leakage_bug = spi_io_fix_leakage_bug,
	.io_resume_leakage_bug = spi_io_resume_leakage_bug,
};

static struct resource rk29_spi1_resources[] = {
	{
		.start	= IRQ_SPI1,
		.end	= IRQ_SPI1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_SPI1_PHYS,
		.end	= RK30_SPI1_PHYS + RK30_SPI1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start  = DMACH_SPI1_TX,
		.end    = DMACH_SPI1_TX,
		.flags  = IORESOURCE_DMA,
	},
	{
		.start  = DMACH_SPI1_RX,
		.end    = DMACH_SPI1_RX,
		.flags  = IORESOURCE_DMA,
	},
};

struct platform_device rk29xx_device_spi1m = {
	.name	= "rk29xx_spim",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(rk29_spi1_resources),
	.resource	= rk29_spi1_resources,
	.dev			= {
		.dma_mask = &dma_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data	= &rk29xx_spi1_platdata,
	},
};
#endif

static void __init rk30_init_spim(void)
{
#ifdef CONFIG_SPIM0_RK29
	platform_device_register(&rk29xx_device_spi0m);
#endif
#ifdef CONFIG_SPIM1_RK29
	platform_device_register(&rk29xx_device_spi1m);
#endif
}

#ifdef CONFIG_MTD_NAND_RK29XX
static struct resource resources_nand[] = {
	{
		.start	= RK30_NANDC_PHYS,
		.end	= RK30_NANDC_PHYS + RK30_NANDC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device device_nand = {
	.name		= "rk29xxnand",
	.id		= -1,
	.resource	= resources_nand,
	.num_resources	= ARRAY_SIZE(resources_nand),
};
#endif

#ifdef CONFIG_HDMI_RKPX2
static struct resource resource_hdmi[] = {
	[0] = {
		.start = RK30_HDMI_PHYS,
		.end   = RK30_HDMI_PHYS + RK30_HDMI_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_HDMI,
		.end   = IRQ_HDMI,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device device_hdmi = {
	.name				= "rk30-hdmi",
	.id					= -1,
	.num_resources		= ARRAY_SIZE(resource_hdmi),
	.resource			= resource_hdmi,
};
#endif

#ifdef CONFIG_RGA_RKPX2
static struct resource resource_rga[] = {
	[0] = {
		.start = RK30_RGA_PHYS,
		.end   = RK30_RGA_PHYS + RK30_RGA_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_RGA,
		.end   = IRQ_RGA,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device device_rga = {
	.name		= "rga",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resource_rga),
	.resource	= resource_rga,
};
#endif

static struct resource resource_ipp[] = {
	[0] = {
		.start = RK30_IPP_PHYS,
		.end   = RK30_IPP_PHYS + RK30_IPP_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_IPP,
		.end   = IRQ_IPP,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device device_ipp = {
	.name		= "rk29-ipp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resource_ipp),
	.resource	= resource_ipp,
};

#ifdef CONFIG_SND_RK29_SOC_I2S
#ifdef CONFIG_SND_RK29_SOC_I2S_8CH
static struct resource resource_iis0_8ch[] = {
	[0] = {
		.start	= RK30_I2S0_8CH_PHYS,
		.end	= RK30_I2S0_8CH_PHYS + RK30_I2S0_8CH_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= DMACH_I2S0_8CH_TX,
		.end	= DMACH_I2S0_8CH_TX,
		.flags	= IORESOURCE_DMA,
	},
	[2] = {
		.start	= DMACH_I2S0_8CH_RX,
		.end	= DMACH_I2S0_8CH_RX,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= IRQ_I2S0_8CH,
		.end	= IRQ_I2S0_8CH,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device device_iis0_8ch = {
	.name		= "rk29_i2s",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resource_iis0_8ch),
	.resource	= resource_iis0_8ch,
};
#endif
#ifdef CONFIG_SND_RK29_SOC_I2S_2CH
static struct resource resource_iis1_2ch[] = {
	[0] = {
		.start	= RK30_I2S1_2CH_PHYS,
		.end	= RK30_I2S1_2CH_PHYS + RK30_I2S1_2CH_SIZE -1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= DMACH_I2S1_2CH_TX,
		.end	= DMACH_I2S1_2CH_TX,
		.flags	= IORESOURCE_DMA,
	},
	[2] = {
		.start	= DMACH_I2S1_2CH_RX,
		.end	= DMACH_I2S1_2CH_RX,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= IRQ_I2S1_2CH,
		.end	= IRQ_I2S1_2CH,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device device_iis1_2ch = {
	.name		= "rk29_i2s",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resource_iis1_2ch),
	.resource	= resource_iis1_2ch,
};
#endif
#ifdef CONFIG_SND_RK_SOC_I2S2_2CH
static struct resource resource_iis2_2ch[] = {
	[0] = {
		.start	= RK30_I2S2_2CH_PHYS,
		.end	= RK30_I2S2_2CH_PHYS + RK30_I2S2_2CH_SIZE -1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= DMACH_I2S2_2CH_TX,
		.end	= DMACH_I2S2_2CH_TX,
		.flags	= IORESOURCE_DMA,
	},
	[2] = {
		.start	= DMACH_I2S2_2CH_RX,
		.end	= DMACH_I2S2_2CH_RX,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= IRQ_I2S2_2CH,
		.end	= IRQ_I2S2_2CH,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device device_iis2_2ch = {
	.name		= "rk29_i2s",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(resource_iis2_2ch),
	.resource	= resource_iis2_2ch,
};
#endif
#endif

static struct platform_device device_pcm = {
	.name = "rockchip-audio",
	.id = -1,
};

static void __init rk30_init_i2s(void)
{
#ifdef CONFIG_SND_RK29_SOC_I2S_8CH
	platform_device_register(&device_iis0_8ch);
#endif
#ifdef CONFIG_SND_RK29_SOC_I2S_2CH
	platform_device_register(&device_iis1_2ch);
#endif
#ifdef CONFIG_SND_RK_SOC_I2S2_2CH
	platform_device_register(&device_iis2_2ch);
#endif
	platform_device_register(&device_pcm);
}



#ifdef CONFIG_KEYS_RK29
extern struct rk29_keys_platform_data rk29_keys_pdata;
static struct platform_device device_keys = {
	.name		= "rk29-keypad",
	.id		= -1,
	.dev		= {
		.platform_data	= &rk29_keys_pdata,
	},
};
#endif

#ifdef CONFIG_EMMC_RK
static struct resource resources_emmc[] = {
	{
		.start 	= IRQ_EMMC,
		.end 	= IRQ_EMMC,
		.flags 	= IORESOURCE_IRQ,
	},
	{
		.start 	= RK30_EMMC_PHYS,
		.end 	= RK30_EMMC_PHYS + RK30_EMMC_SIZE - 1,
		.flags 	= IORESOURCE_MEM,
	}
};

static struct platform_device device_emmc = {
	.name		= "emmc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_emmc),
	.resource	= resources_emmc,
	.dev 		= {
		.platform_data = NULL,
	},
};
#endif

#ifdef CONFIG_SDMMC2_RK29
static struct resource resources_sdmmc2[] = {
	{
		.start 	= IRQ_EMMC,
		.end 	= IRQ_EMMC,
		.flags 	= IORESOURCE_IRQ,
	},
	{
		.start 	= RK30_EMMC_PHYS,
		.end 	= RK30_EMMC_PHYS + RK30_EMMC_SIZE - 1,
		.flags 	= IORESOURCE_MEM,
	}

};

static struct platform_device device_sdmmc2 = {
	.name		= "emmc",
	.id		= -1, //For compatibility with old code, do not set this id-value to 2
	.num_resources	= ARRAY_SIZE(resources_sdmmc2),
	.resource	= resources_sdmmc2,
	.dev 		= {
		.platform_data = &default_sdmmc2_data,
	},
};
#endif
#ifdef CONFIG_SDMMC0_RK29
static struct resource resources_sdmmc0[] = {
	{
		.start 	= IRQ_SDMMC,
		.end 	= IRQ_SDMMC,
		.flags 	= IORESOURCE_IRQ,
	},
	{
		.start 	= RK30_SDMMC0_PHYS,
		.end 	= RK30_SDMMC0_PHYS + RK30_SDMMC0_SIZE -1,
		.flags 	= IORESOURCE_MEM,
	}
};

static struct platform_device device_sdmmc0 = {
	.name		= "rk29_sdmmc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_sdmmc0),
	.resource	= resources_sdmmc0,
	.dev 		= {
		.platform_data = &default_sdmmc0_data,
	},
};
#endif

#ifdef CONFIG_SDMMC1_RK29
static struct resource resources_sdmmc1[] = {
	{
		.start 	= IRQ_SDIO,
		.end 	= IRQ_SDIO,
		.flags 	= IORESOURCE_IRQ,
	},
	{
		.start 	= RK30_SDIO_PHYS,
		.end 	= RK30_SDIO_PHYS + RK30_SDIO_SIZE - 1,
		.flags 	= IORESOURCE_MEM,
	}
};

static struct platform_device device_sdmmc1 = {
	.name		= "rk29_sdmmc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resources_sdmmc1),
	.resource	= resources_sdmmc1,
	.dev 		= {
		.platform_data = &default_sdmmc1_data,
	},
};
#endif

static void __init rk30_init_sdmmc(void)
{
#ifdef CONFIG_EMMC_RK
	platform_device_register(&device_emmc);
#endif
#ifdef CONFIG_SDMMC2_RK29
    //register eMMC
	platform_device_register(&device_sdmmc2);
#endif
#ifdef CONFIG_SDMMC0_RK29
	platform_device_register(&device_sdmmc0);
#endif
#ifdef CONFIG_SDMMC1_RK29
	platform_device_register(&device_sdmmc1);
#endif
}

#ifdef CONFIG_SND_RK_SOC_SPDIF
static struct resource resources_spdif[] = {
    [0] = {
		.name 	= "spdif_base",
        .start 	= RK30_SPDIF_PHYS,
        .end    = RK30_SPDIF_PHYS + RK30_SPDIF_SIZE - 1,
        .flags  = IORESOURCE_MEM,
    },
    [1] = {
		.name 	= "spdif_irq",
        .start 	= IRQ_SPDIF,
        .end    = IRQ_SPDIF,
        .flags  = IORESOURCE_IRQ,
    },
    [2] = {
		.name 	= "spdif_dma",
        .start 	= DMACH_SPDIF_TX,
        .end    = DMACH_SPDIF_TX,
        .flags  = IORESOURCE_DMA,
    },
};
struct platform_device rk29_device_spdif = {
    .name             = "rk-spdif",
    .id               = 0,
    .num_resources    = ARRAY_SIZE(resources_spdif),
    .resource         = resources_spdif,
};
#endif

#ifdef CONFIG_RK29_VMAC
static u64 eth_dmamask = DMA_BIT_MASK(32);
static struct resource resources_vmac[] = {
	[0] = {
		.start	= RK30_MAC_PHYS,
		.end	= RK30_MAC_PHYS + RK30_MAC_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_MAC,
		.end	= IRQ_MAC,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device device_vmac = {
	.name		= "rk29 vmac",
	.id		= 0,
	.dev = {
		.dma_mask		= &eth_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &board_vmac_data,
	},
	.num_resources	= ARRAY_SIZE(resources_vmac),
	.resource	= resources_vmac,
};
#endif

#ifdef CONFIG_RK29_WATCHDOG
static struct resource resources_wdt[] = {
	{
		.start	= IRQ_WDT,
		.end	= IRQ_WDT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= RK30_WDT_PHYS,
		.end	= RK30_WDT_PHYS + RK30_WDT_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device device_wdt = {
	.name	= "rk29-wdt",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_wdt),
	.resource	= resources_wdt,
};
#endif

static struct resource resource_arm_pmu[] = {
	{
		.start	= IRQ_ARM_PMU,
		.end	= IRQ_ARM_PMU,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= IRQ_ARM_PMU + 1,
		.end	= IRQ_ARM_PMU + 1,
		.flags	= IORESOURCE_IRQ,
	},
#if defined(CONFIG_ARCH_RK3188)
	{
		.start	= IRQ_ARM_PMU + 2,
		.end	= IRQ_ARM_PMU + 2,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= IRQ_ARM_PMU + 3,
		.end	= IRQ_ARM_PMU + 3,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

struct platform_device device_arm_pmu = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.num_resources	= ARRAY_SIZE(resource_arm_pmu),
	.resource	= resource_arm_pmu,
};

static int __init rk30_init_devices(void)
{
	rk30_init_dma();
	rk30_init_uart();
	rk30_init_i2c();
	rk30_init_spim();
#ifdef CONFIG_RGA_RKPX2
	platform_device_register(&device_rga);
#endif
	platform_device_register(&device_ipp);

#ifdef CONFIG_HDMI_RKPX2
	platform_device_register(&device_hdmi);
#endif
#ifdef CONFIG_ADC_RKPX2
	platform_device_register(&device_adc);
#endif
#ifdef CONFIG_KEYS_RK29
	platform_device_register(&device_keys);
#endif
	rk30_init_tsadc();
	rk30_init_sdmmc();
#if defined(CONFIG_FIQ_DEBUGGER) && defined(DEBUG_UART_PHYS)
	rk_serial_debug_init(DEBUG_UART_BASE, IRQ_DEBUG_UART, IRQ_UART_SIGNAL, -1);
#endif
	rk30_init_i2s();

#ifdef CONFIG_SND_RK_SOC_SPDIF
    platform_device_register(&rk29_device_spdif);
#endif
#ifdef CONFIG_RK29_VMAC
	platform_device_register(&device_vmac);
#endif
#ifdef CONFIG_RK29_WATCHDOG
	platform_device_register(&device_wdt);
#endif
	platform_device_register(&device_arm_pmu);
#ifdef CONFIG_MTD_NAND_RK29XX
	platform_device_register(&device_nand);
#endif

	platform_device_register(&rk30_device_remotectl);
	
	return 0;
}
arch_initcall(rk30_init_devices);
