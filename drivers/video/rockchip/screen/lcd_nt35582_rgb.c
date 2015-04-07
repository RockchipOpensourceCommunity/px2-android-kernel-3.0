#include <linux/fb.h>
#include <linux/delay.h>
#include "../../rk29_fb.h"
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/board.h>
#include <linux/regulator/consumer.h>

#define RK_SCREEN_INIT 1
#if 1
/* Base */
#define SCREEN_TYPE	    	SCREEN_RGB
#define LVDS_FORMAT		LVDS_8BIT_1
#define OUT_FACE		OUT_P888
#define DCLK			 24000000
#define LCDC_ACLK       150000000     //29 lcdc axi DMA Ƶ��

/* Timing */
#define H_PW			1
#define H_BP			1
#define H_VD			1440
#define H_FP			2

#define V_PW			1
#define V_BP			4
#define V_VD			900
#define V_FP			2

#define LCD_WIDTH       480    //need modify
#define LCD_HEIGHT      800
#define INVALED_DATA   0xff

/* Other */
#define DCLK_POL		0
#define SWAP_RB			0
#define VSYNC_POL	0
#define HSYNC_POL	0
#define DEN_POL		0
#define SWAP_RB		0
#define SWAP_RG		0
#define SWAP_GB		0
#else
/* Base */
#define SCREEN_TYPE	    	SCREEN_RGB
#define LVDS_FORMAT		LVDS_8BIT_1
#define OUT_FACE		OUT_P888
#define DCLK		27*1000*1000	//***27
#define LCDC_ACLK       150000000     //29 lcdc axi DMA Ƶ��           //rk29

/* Timing */
#define H_PW			4 //8ǰ��Ӱ
#define H_BP			8//6
#define H_VD			480//320	//***800 
#define H_FP			8//60

#define V_PW			4//12
#define V_BP			8// 4
#define V_VD			800//480	//***480
#define V_FP			8//40

#define LCD_WIDTH       57    //lcd size *mm
#define LCD_HEIGHT      94
#define INVALED_DATA   0xff
/* Other */
#define DCLK_POL		1//0 
#define SWAP_RB			0

#define VSYNC_POL	0
#define HSYNC_POL	0
#define DEN_POL		0
#define SWAP_RB		0
#define SWAP_RG		0
#define SWAP_GB		0

#endif

#define TXD_PORT        gLcd_info->txd_pin
#define CLK_PORT        gLcd_info->clk_pin
#define CS_PORT         gLcd_info->cs_pin

#define CS_OUT()        gpio_direction_output(CS_PORT, 0)
#define CS_SET()        gpio_set_value(CS_PORT, GPIO_HIGH)
#define CS_CLR()        gpio_set_value(CS_PORT, GPIO_LOW)
#define CLK_OUT()       gpio_direction_output(CLK_PORT, 0)
#define CLK_SET()       gpio_set_value(CLK_PORT, GPIO_HIGH)
#define CLK_CLR()       gpio_set_value(CLK_PORT, GPIO_LOW)
#define TXD_OUT()       gpio_direction_output(TXD_PORT, 0)
#define TXD_SET()       gpio_set_value(TXD_PORT, GPIO_HIGH)
#define TXD_CLR()       gpio_set_value(TXD_PORT, GPIO_LOW)
#define TXD_IN()        gpio_direction_input(TXD_PORT)
#define TXD_GET()       gpio_get_value(TXD_PORT)


#define delay_us(i)      udelay(i)
static struct rk29lcd_info *gLcd_info = NULL;

u32 spi_screenreg_get(u32 Addr)
{
    u32 i;
	u8 addr_h = (Addr>>8) & 0x000000ff;
	u8 addr_l = Addr & 0x000000ff;
	u8 cmd1 = 0x20;   //0010 0000
	u8 cmd2 = 0x00;   //0000 0000
	u8 cmd3 = 0x00;   //0000 0000

    u8 data_l = 0;
    u8 tmp;

    TXD_OUT();
    CLK_OUT();
    CS_OUT();
    delay_us(8);

    CS_SET();
    CLK_CLR();
    TXD_CLR();
    delay_us(4);

	// first transmit
	CS_CLR();
	delay_us(4);
	for(i = 0; i < 8; i++)
	{
		if(cmd1 &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	for(i = 0; i < 8; i++)
	{
		if(addr_h &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	CLK_CLR();
	TXD_CLR();
	delay_us(4);
	CS_SET();
	delay_us(8);

	// second transmit
	CS_CLR();
	delay_us(4);
	for(i = 0; i < 8; i++)
	{
		if(cmd2 &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	for(i = 0; i < 8; i++)
	{
		if(addr_l &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	CLK_CLR();
	TXD_CLR();
	delay_us(4);
	CS_SET();
	delay_us(8);

	// third transmit
	CS_CLR();
	delay_us(4);
	for(i = 0; i < 8; i++)
	{
		if(cmd3 &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	TXD_CLR();
	TXD_IN();
	for(i = 0; i < 8; i++)
	{
		CLK_CLR();
		delay_us(4);
		CLK_SET();

        tmp = TXD_GET();
        data_l += (tmp<<(7-i));

		delay_us(4);
	}
	CLK_CLR();
	TXD_CLR();
	delay_us(4);
	CS_SET();
	delay_us(8);

	return data_l;
}


void spi_screenreg_set(u32 Addr, u32 Data)
{
    u32 i;
	u8 addr_h = (Addr>>8) & 0x000000ff;
	u8 addr_l = Addr & 0x000000ff;
	u8 data_l = Data & 0x000000ff;
	u8 cmd1 = 0x20;   //0010 0000
	u8 cmd2 = 0x00;   //0000 0000
	u8 cmd3 = 0x40;   //0100 0000

    TXD_OUT();
    CLK_OUT();
    CS_OUT();
    delay_us(8);

    CS_SET();
    CLK_CLR();
    TXD_CLR();
    delay_us(4);

	// first transmit
	CS_CLR();
	delay_us(4);
	for(i = 0; i < 8; i++)
	{
		if(cmd1 &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	for(i = 0; i < 8; i++)
	{
		if(addr_h &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	CLK_CLR();
	TXD_CLR();
	delay_us(4);
	CS_SET();
	delay_us(8);

	// second transmit
	CS_CLR();
	delay_us(4);
	for(i = 0; i < 8; i++)
	{
		if(cmd2 &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	for(i = 0; i < 8; i++)
	{
		if(addr_l &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}
	CLK_CLR();
	TXD_CLR();
	delay_us(4);
	CS_SET();
	delay_us(8);

	// third transmit
	CS_CLR();
	delay_us(4);
	for(i = 0; i < 8; i++)
	{
		if(cmd3 &(1<<(7-i)))
			TXD_SET();
		else
			TXD_CLR();

		CLK_CLR();
		delay_us(4);
		CLK_SET();
		delay_us(4);
	}

	if(INVALED_DATA != Data)
	{
		for(i = 0; i < 8; i++)
		{
			if(data_l &(1<<(7-i)))
				TXD_SET();
			else
				TXD_CLR();

			CLK_CLR();
			delay_us(4);
			CLK_SET();
			delay_us(4);
		}
	}
	CLK_CLR();
	TXD_CLR();
	delay_us(4);
	CS_SET();
	delay_us(8);

   // printk("Addr=0x%04x, WData=0x%02x, RData=0x%02x \n", Addr, Data, spi_screenreg_get(Addr));

}

int rk_lcd_init(void)
{

	printk("lcd_nt35582_rgb %s\n", __func__);
#if 0
    GPIO_SetPinDirection(reset_pin, GPIO_OUT);
    GPIO_SetPinLevel(reset_pin,GPIO_HIGH);
    DelayMs_nops(100);
    GPIO_SetPinLevel(reset_pin,GPIO_LOW);
    DelayMs_nops(100);
    GPIO_SetPinLevel(reset_pin,GPIO_HIGH);
#endif

    if(gLcd_info)
        gLcd_info->io_init();

    spi_screenreg_set(0x0100, INVALED_DATA);
    msleep(20);
    spi_screenreg_set(0x1100, INVALED_DATA);
    msleep(20);
    spi_screenreg_set(0x3A00, 0x0070);
    //spi_screenreg_set(0x3B00, 0x0068);

    spi_screenreg_set(0xC000, 0x008A);
    spi_screenreg_set(0xC002, 0x008A);
    spi_screenreg_set(0xC200, 0x0002);
    spi_screenreg_set(0xC202, 0x0032);
    spi_screenreg_set(0xC100, 0x0040);
    spi_screenreg_set(0xC700, 0x008B);
    msleep(220);
    spi_screenreg_set(0x3600, 0x0003);
    spi_screenreg_set(0x2900, INVALED_DATA);
    //spi_screenreg_set(0x2C00, INVALED_DATA);

#if 0
    printk("spi_screenreg_set(0x5555, 0x0055)... \n");
    while(1) {
       spi_screenreg_set(0x5555, 0x0055);
       msleep(1);
    }
#endif

#if 0
    while(1) {
        int i = 0;
        for(i=0; i<400*480; i++)
            mcu_ioctl(MCU_WRDATA, 0xffffffff);
        for(i=0; i<400*480; i++)
            mcu_ioctl(MCU_WRDATA, 0x00000000);
        msleep(1000);
        printk(">>>>> MCU_WRDATA ...\n");

        for(i=0; i<400*480; i++)
            mcu_ioctl(MCU_WRDATA, 0x00000000);
        for(i=0; i<400*480; i++)
            mcu_ioctl(MCU_WRDATA, 0xffffffff);
        msleep(1000);
        printk(">>>>> MCU_WRDATA ...\n");
    }
#endif

    if(gLcd_info)
        gLcd_info->io_deinit();
    return 0;
}


int rk_lcd_standby(u8 enable)
{
	struct regulator *ldo;
	
	printk("lcd_nt35582_rgb %s enable:%d\n", __func__,enable);

	//ldo = regulator_get(NULL, "ldo8");	// vlcd_33

	if(!enable)
	{
	#if 0
	    regulator_set_voltage(ldo, 3300000, 3300000);
	    regulator_set_suspend_voltage(ldo, 3300000);
	    regulator_enable(ldo);
	    //printk("%s set ldo8 vcca_33=%dmV end\n", __func__, regulator_get_voltage(ldo));
	    regulator_put(ldo);
	#endif
	    mdelay(100);
	    gpio_set_value(gLcd_info->reset_pin,1);

	    spi_screenreg_set(0x0100, INVALED_DATA);
	    msleep(20);
	    spi_screenreg_set(0x1100, INVALED_DATA);
	    msleep(20);
	    spi_screenreg_set(0x3A00, 0x0070);
	    //spi_screenreg_set(0x3B00, 0x0068);

	    spi_screenreg_set(0xC000, 0x008A);
	    spi_screenreg_set(0xC002, 0x008A);
	    spi_screenreg_set(0xC200, 0x0002);
	    spi_screenreg_set(0xC202, 0x0032);
	    spi_screenreg_set(0xC100, 0x0040);
	    spi_screenreg_set(0xC700, 0x008B);
	    msleep(220);
	    spi_screenreg_set(0x3600, 0x0003);
	    spi_screenreg_set(0x2900, INVALED_DATA);
	    //spi_screenreg_set(0x2C00, INVALED_DATA);
	}
	else
	{
	    spi_screenreg_set(0x2800, INVALED_DATA);
	    spi_screenreg_set(0x1000, INVALED_DATA);
#if 0
	    regulator_disable(ldo);
	    regulator_put(ldo);
#endif
	    gpio_set_value(gLcd_info->reset_pin,0);
	    TXD_CLR();
	    CLK_CLR();
	    CS_CLR();
	    delay_us(4);
	}
    return 0;
}




