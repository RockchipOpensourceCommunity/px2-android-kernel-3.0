#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>

#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/cru.h>

#include "usbdev_rk.h"
#include "dwc_otg_regs.h" 

#if defined(CONFIG_ARCH_RKPX2) || defined(CONFIG_ARCH_RK3188)

#define GRF_REG_BASE	RK30_GRF_BASE	
#define USBOTG_SIZE	RK30_USBOTG20_SIZE
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
#define USBGRF_SOC_STATUS0	(GRF_REG_BASE+0xac)
#define USBGRF_UOC0_CON0	(GRF_REG_BASE+0x10c)
#define USBGRF_UOC0_CON2	(GRF_REG_BASE+0x114)
#define USBGRF_UOC0_CON3	(GRF_REG_BASE+0x118)
#define USBGRF_UOC1_CON0	(GRF_REG_BASE+0x11C)
#define USBGRF_UOC1_CON2	(GRF_REG_BASE+0x124)
#define USBGRF_UOC1_CON3	(GRF_REG_BASE+0x128)
#define USBGRF_UOC3_CON0	(GRF_REG_BASE+0x138)

#define USBGRF_UOC2_CON0	(GRF_REG_BASE+0x12C)
#if defined(CONFIG_SOC_RK3066B) || defined(CONFIG_SOC_RK3108) 
#define RK3066B_HOST_DRV_VBUS RK30_PIN0_PD7
#define RK3066B_OTG_DRV_VBUS  RK30_PIN0_PD6
#elif defined(CONFIG_SOC_RK3168) || defined(CONFIG_SOC_RK3188) || defined(CONFIG_SOC_RK3168M)
#define RK3066B_HOST_DRV_VBUS RK30_PIN0_PC0
#define RK3066B_OTG_DRV_VBUS  RK30_PIN3_PD5
#elif defined(CONFIG_SOC_RK3028)
#define RK3066B_HOST_DRV_VBUS RK30_PIN1_PA4
#define RK3066B_OTG_DRV_VBUS  RK30_PIN3_PD7
#endif

#else
#define USBGRF_SOC_STATUS0	(GRF_REG_BASE+0x15c)
#define USBGRF_UOC0_CON2	(GRF_REG_BASE+0x184)
#define USBGRF_UOC1_CON2	(GRF_REG_BASE+0x190)
#endif

int dwc_otg_check_dpdm(void)
{
	static uint8_t * reg_base = 0;
    volatile unsigned int * otg_dctl;
    volatile unsigned int * otg_gotgctl;
    volatile unsigned int * otg_hprt0;
    int bus_status = 0;
    unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC0_CON2);
    
    // softreset & clockgate 
    *(unsigned int*)(RK30_CRU_BASE+0x120) = ((7<<5)<<16)|(7<<5);    // otg0 phy clkgate
    udelay(3);
    *(unsigned int*)(RK30_CRU_BASE+0x120) = ((7<<5)<<16)|(0<<5);    // otg0 phy clkgate
    dsb();
    *(unsigned int*)(RK30_CRU_BASE+0xd4) = ((1<<5)<<16);    // otg0 phy clkgate
    *(unsigned int*)(RK30_CRU_BASE+0xe4) = ((1<<13)<<16);   // otg0 hclk clkgate
    *(unsigned int*)(RK30_CRU_BASE+0xe0) = ((3<<5)<<16);    // hclk usb clkgate
    
    // exit phy suspend 
        *otg_phy_con1 = ((0x01<<2)<<16);    // exit suspend.
    
    // soft connect
    if(reg_base == 0){
        reg_base = ioremap(RK30_USBOTG20_PHYS,USBOTG_SIZE);
        if(!reg_base){
            bus_status = -1;
            goto out;
        }
    }
    mdelay(105);
    printk("regbase %p 0x%x, otg_phy_con%p, 0x%x\n",
        reg_base, *(reg_base), otg_phy_con1, *otg_phy_con1);
    otg_dctl = (unsigned int * )(reg_base+0x804);
    otg_gotgctl = (unsigned int * )(reg_base);
    otg_hprt0 = (unsigned int * )(reg_base + DWC_OTG_HOST_PORT_REGS_OFFSET);
    if(*otg_gotgctl &(1<<19)){
        bus_status = 1;
        *otg_dctl &= ~2;
        mdelay(50);    // delay about 10ms
    // check dp,dm
        if((*otg_hprt0 & 0xc00)==0xc00)
            bus_status = 2;
    }
out:
    return bus_status;
}

EXPORT_SYMBOL(dwc_otg_check_dpdm);

#ifdef CONFIG_USB20_OTG
/*DWC_OTG*/
static struct resource usb20_otg_resource[] = {
	{
		.start = IRQ_USB_OTG,
		.end   = IRQ_USB_OTG,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = RK30_USBOTG20_PHYS,
		.end   = RK30_USBOTG20_PHYS + RK30_USBOTG20_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},

};

void usb20otg_hw_init(void)
{
#ifndef CONFIG_USB20_HOST
        // close USB 2.0 HOST phy and clock
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
        unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC1_CON2);
        unsigned int * otg_phy_con2 = (unsigned int*)(USBGRF_UOC1_CON3);
        *otg_phy_con1 =  (0x01<<2)|((0x01<<2)<<16);     //enable soft control
        *otg_phy_con2 =  0x2A|(0x3F<<16);               // enter suspend   
#else
        unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC1_CON2);
        *otg_phy_con1 = 0x554|(0xfff<<16);   // enter suspend.
#endif
#endif
        // usb phy config init
#ifdef CONFIG_ARCH_RK3188
        //usb phy enter usb mode
        unsigned int * otg_phy_con3 = (unsigned int*)(USBGRF_UOC0_CON0);
        *otg_phy_con3 = (0x0300 << 16);
#endif    
        // other haredware init
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
        //GPIO init
        gpio_request(RK3066B_OTG_DRV_VBUS, NULL);
        gpio_direction_output(RK3066B_OTG_DRV_VBUS, GPIO_LOW);
#else
        rk30_mux_api_set(GPIO0A5_OTGDRVVBUS_NAME, GPIO0A_OTG_DRV_VBUS);
#endif
}

void usb20otg_phy_suspend(void* pdata, int suspend)
{
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
        struct dwc_otg_platform_data *usbpdata=pdata;
        unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC0_CON2);
        unsigned int * otg_phy_con2 = (unsigned int*)(USBGRF_UOC0_CON3);
        if(suspend){
            *otg_phy_con1 =  (0x01<<2)|((0x01<<2)<<16); ;   //enable soft control
            *otg_phy_con2 =  0x2A|(0x3F<<16);;              // enter suspend
            usbpdata->phy_status = 1;
        }
        else{
            *otg_phy_con1 = ((0x01<<2)<<16);    // exit suspend.
            usbpdata->phy_status = 0;
        }
#else
        struct dwc_otg_platform_data *usbpdata=pdata;
        unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC0_CON2);
        if(suspend){
            *otg_phy_con1 = 0x554|(0xfff<<16);   // enter suspend.
            usbpdata->phy_status = 1;
        }
        else{
            *otg_phy_con1 = ((0x01<<2)<<16);    // exit suspend.
            usbpdata->phy_status = 0;
        }
#endif
}
void usb20otg_soft_reset(void)
{
    cru_set_soft_reset(SOFT_RST_USBOTG0, true);
    cru_set_soft_reset(SOFT_RST_USBPHY0, true);
    cru_set_soft_reset(SOFT_RST_OTGC0, true);
    udelay(1);

    cru_set_soft_reset(SOFT_RST_USBOTG0, false);
    cru_set_soft_reset(SOFT_RST_USBPHY0, false);
    cru_set_soft_reset(SOFT_RST_OTGC0, false);
    mdelay(1);
}
void usb20otg_clock_init(void* pdata)
{
    struct dwc_otg_platform_data *usbpdata=pdata;
    struct clk* ahbclk,*phyclk;
    ahbclk = clk_get(NULL, "hclk_otg0");
    phyclk = clk_get(NULL, "otgphy0");
	usbpdata->phyclk = phyclk;
	usbpdata->ahbclk = ahbclk;
}
void usb20otg_clock_enable(void* pdata, int enable)
{
    struct dwc_otg_platform_data *usbpdata=pdata;

    if(enable){
        clk_enable(usbpdata->ahbclk);
        clk_enable(usbpdata->phyclk);
    }
    else{
        clk_disable(usbpdata->phyclk);
        clk_disable(usbpdata->ahbclk);
    }
}
int usb20otg_get_status(int id)
{
        int ret = -1;
        unsigned int usbgrf_status = *(unsigned int*)(USBGRF_SOC_STATUS0);
        switch(id)
        {
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
            case USB_STATUS_BVABLID:
                // bvalid in grf
                ret = (usbgrf_status &(1<<10));
                break;
            case USB_STATUS_DPDM:
                // dpdm in grf
                ret = (usbgrf_status &(3<<11));
                break;
            case USB_STATUS_ID:
                // id in grf
                ret = (usbgrf_status &(1<<13));
                break;
#else
            case USB_STATUS_BVABLID:
                // bvalid in grf
                ret = (usbgrf_status &0x20000);
                break;
            case USB_STATUS_DPDM:
                // dpdm in grf
                ret = (usbgrf_status &(3<<18));
                break;
            case USB_STATUS_ID:
                // id in grf
                ret = (usbgrf_status &(1<<20));
                break;
#endif
            default:
                break;
        }
        return ret;
}

#ifdef CONFIG_RK_USB_UART
void dwc_otg_uart_mode(void* pdata, int enter_usb_uart_mode)
{
	unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC0_CON0);
	      
	if(1 == enter_usb_uart_mode){  //enter uart mode
		/*note: can't disable otg here! If otg disable, the ID change
		*interrupt can't be triggered when otg cable connect without
		*device.At the same time, uart can't be used normally 
		*/
		//*otg_phy_con1 = (0x10 | (0x10 << 16));//otg disable
		*otg_phy_con1 = (0x0300 | (0x0300 << 16));//bypass dm       
	}
	if(0 == enter_usb_uart_mode){  //enter usb mode 
		*otg_phy_con1 = (0x0300 << 16); //bypass dm disable
		//*otg_phy_con1 = (0x10 << 16);//otg enable		
	}
}
#endif

#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
void usb20otg_power_enable(int enable)
{ 
    unsigned int usbgrf_status = *(unsigned int*)(USBGRF_SOC_STATUS0);
    if(0 == enable)//disable
    {
        gpio_set_value(RK3066B_OTG_DRV_VBUS, GPIO_LOW); 

    }
    if(1 == enable)//enable
    {
        gpio_set_value(RK3066B_OTG_DRV_VBUS, GPIO_HIGH); 
    }   
}
#endif
struct dwc_otg_platform_data usb20otg_pdata = {
    .phyclk = NULL,
    .ahbclk = NULL,
    .busclk = NULL,
    .phy_status = 0,
    .hw_init=usb20otg_hw_init,
    .phy_suspend=usb20otg_phy_suspend,
    .soft_reset=usb20otg_soft_reset,
    .clock_init=usb20otg_clock_init,
    .clock_enable=usb20otg_clock_enable,
    .get_status=usb20otg_get_status,
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
    .power_enable=usb20otg_power_enable,
#endif
#ifdef CONFIG_RK_USB_UART
    .dwc_otg_uart_mode=dwc_otg_uart_mode,
#endif
};

struct platform_device device_usb20_otg = {
	.name		  = "usb20_otg",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(usb20_otg_resource),
	.resource	  = usb20_otg_resource,
	.dev		= {
		.platform_data	= &usb20otg_pdata,
	},
};
#endif
#ifdef CONFIG_USB20_HOST
static struct resource usb20_host_resource[] = {
    {
        .start = IRQ_USB_HOST,
        .end   = IRQ_USB_HOST,
        .flags = IORESOURCE_IRQ,
    },
    {
        .start = RK30_USBHOST20_PHYS,
        .end   = RK30_USBHOST20_PHYS + RK30_USBHOST20_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },

};
void usb20host_hw_init(void)
{
    // usb phy config init

    // other haredware init
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
    gpio_request(RK3066B_HOST_DRV_VBUS, NULL);
    gpio_direction_output(RK3066B_HOST_DRV_VBUS, GPIO_HIGH);
#else
    rk30_mux_api_set(GPIO0A6_HOSTDRVVBUS_NAME, GPIO0A_HOST_DRV_VBUS);
#endif
}
void usb20host_phy_suspend(void* pdata, int suspend)
{ 
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
        struct dwc_otg_platform_data *usbpdata=pdata;
        unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC1_CON2);
        unsigned int * otg_phy_con2 = (unsigned int*)(USBGRF_UOC1_CON3);
        if(suspend)
        {
            *otg_phy_con1 =  (0x01<<2)|((0x01<<2)<<16); ;   //enable soft control
            *otg_phy_con2 =  0x2A|(0x3F<<16);;                     // enter suspend
            usbpdata->phy_status = 1;
        }
        else
        {
            *otg_phy_con1 = ((0x01<<2)<<16);    // exit suspend.
            usbpdata->phy_status = 0;
        }   
#else
        struct dwc_otg_platform_data *usbpdata=pdata;
        unsigned int * otg_phy_con1 = (unsigned int*)(USBGRF_UOC1_CON2);
        if(suspend)
        {
            *otg_phy_con1 = 0x554|(0xfff<<16);   // enter suspend.
            usbpdata->phy_status = 1;
        }
        else
        {
            *otg_phy_con1 = ((0x01<<2)<<16);    // exit suspend.
            usbpdata->phy_status = 0;
        }
#endif 
}
void usb20host_soft_reset(void)
{
    cru_set_soft_reset(SOFT_RST_USBOTG1, true);
    cru_set_soft_reset(SOFT_RST_USBPHY1, true);
    cru_set_soft_reset(SOFT_RST_OTGC1, true);
    udelay(5);

    cru_set_soft_reset(SOFT_RST_USBOTG1, false);
    cru_set_soft_reset(SOFT_RST_USBPHY1, false);
    cru_set_soft_reset(SOFT_RST_OTGC1, false);
    mdelay(2);
}
void usb20host_clock_init(void* pdata)
{
    struct dwc_otg_platform_data *usbpdata=pdata;
    struct clk* ahbclk,*phyclk;
    ahbclk = clk_get(NULL, "hclk_otg1");
    phyclk = clk_get(NULL, "otgphy1");
	usbpdata->phyclk = phyclk;
	usbpdata->ahbclk = ahbclk;
}
void usb20host_clock_enable(void* pdata, int enable)
{
    struct dwc_otg_platform_data *usbpdata=pdata;
    
    if(enable){
        clk_enable(usbpdata->ahbclk);
        clk_enable(usbpdata->phyclk);
    }
    else{
        clk_disable(usbpdata->phyclk);
        clk_disable(usbpdata->ahbclk);
    }
}
int usb20host_get_status(int id)
{
    int ret = -1;
    unsigned int usbgrf_status = *(unsigned int*)(USBGRF_SOC_STATUS0);
    switch(id)
    {
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
        case USB_STATUS_BVABLID:
            // bvalid in grf
            ret = (usbgrf_status &(1<<17));
            break;
        case USB_STATUS_DPDM:
            // dpdm in grf
            ret = (usbgrf_status &(3<<18));
            break;
        case USB_STATUS_ID:
            // id in grf
            ret = (usbgrf_status &(1<<20));
            break;
#else
        case USB_STATUS_BVABLID:
            // bvalid in grf
            ret = (usbgrf_status &(1<<22));
            break;
        case USB_STATUS_DPDM:
            // dpdm in grf
            ret = (usbgrf_status &(3<<23));
            break;
        case USB_STATUS_ID:
            // id in grf
            ret = 0;
            break;
#endif
        default:
            break;
    }
    return ret;
}

#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
void usb20host_power_enable(int enable)
{ 

    if(0 == enable)//disable
    {
        //ret = gpio_request(RK3066B_HOST_DRV_VBUS, NULL);
        //if (ret != 0) {
        //    gpio_free(RK3066B_HOST_DRV_VBUS);
        //}
        //gpio_direction_output(RK3066B_HOST_DRV_VBUS, 1);
        //gpio_set_value(RK3066B_HOST_DRV_VBUS, 0);
        //printk("!!!!!!!!!!!!!!!!!!!disable host power!!!!!!!!!!!!!!!!!!\n");
    }

    if(1 == enable)//enable
    {
        gpio_set_value(RK3066B_HOST_DRV_VBUS, GPIO_HIGH);
        //printk("!!!!!!!!!!!!!!!!!!!!!enable host power!!!!!!!!!!!!!!!!!!\n");
    }   
}
#endif
struct dwc_otg_platform_data usb20host_pdata = {
    .phyclk = NULL,
    .ahbclk = NULL,
    .busclk = NULL,
    .phy_status = 0,
    .hw_init=usb20host_hw_init,
    .phy_suspend=usb20host_phy_suspend,
    .soft_reset=usb20host_soft_reset,
    .clock_init=usb20host_clock_init,
    .clock_enable=usb20host_clock_enable,
    .get_status=usb20host_get_status,
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
    .power_enable=usb20host_power_enable,
#endif    
};

struct platform_device device_usb20_host = {
    .name             = "usb20_host",
    .id               = -1,
    .num_resources    = ARRAY_SIZE(usb20_host_resource),
    .resource         = usb20_host_resource,
	.dev		= {
		.platform_data	= &usb20host_pdata,
	},
};
#endif
#ifdef CONFIG_USB_EHCI_RK
void rkehci_hw_init(void)
{
	unsigned int * phy_con0 = (unsigned int*)(USBGRF_UOC2_CON0);
	unsigned int * phy_con1 = (unsigned int*)(USBGRF_UOC1_CON0);
	unsigned int * phy_con2 = (unsigned int*)(USBGRF_UOC0_CON0);
	unsigned int * phy_con3 = (unsigned int*)(USBGRF_UOC3_CON0);
	// usb phy config init
	// hsic phy config init, set hsicphy_txsrtune
	*phy_con0 = ((0xf<<6)<<16)|(0xf<<6);

	// other haredware init
	// set common_on, in suspend mode, otg/host PLL blocks remain powered
#ifdef CONFIG_ARCH_RK3188
	*phy_con1 = (1<<16)|0;
#else
	*phy_con2 = (1<<16)|0;
#endif
	/* change INCR to INCR16 or INCR8(beats less than 16)
	 * or INCR4(beats less than 8) or SINGLE(beats less than 4)
	 */
	*phy_con3 = 0x00ff00bc;
}

void rkehci_clock_init(void* pdata)
{
	struct rkehci_platform_data *usbpdata=pdata;

#ifdef CONFIG_ARCH_RK3188  
	struct clk *clk_otg, *clk_hs;

	/* By default, hsicphy_480m's parent is otg phy 480MHz clk
	 * rk3188 must use host phy 480MHz clk
	 */
	clk_hs = clk_get(NULL, "hsicphy_480m");
	clk_otg = clk_get(NULL, "otgphy1_480m");
	clk_set_parent(clk_hs, clk_otg);
#endif

	usbpdata->hclk_hsic = clk_get(NULL, "hclk_hsic");
	usbpdata->hsic_phy_480m = clk_get(NULL, "hsicphy_480m");
	usbpdata->hsic_phy_12m = clk_get(NULL, "hsicphy_12m");
}

void rkehci_clock_enable(void* pdata, int enable)
{
	struct rkehci_platform_data *usbpdata=pdata;

	if(enable == usbpdata->clk_status)
		return;

	if(enable){
		clk_enable(usbpdata->hclk_hsic);
		clk_enable(usbpdata->hsic_phy_480m);
		clk_enable(usbpdata->hsic_phy_12m);
		usbpdata->clk_status = 1;
	}else{
		clk_disable(usbpdata->hsic_phy_12m);
		clk_disable(usbpdata->hsic_phy_480m);
		clk_disable(usbpdata->hclk_hsic);
		usbpdata->clk_status = 0;
	}
}

void rkehci_soft_reset(void)
{
	unsigned int * phy_con0 = (unsigned int*)(USBGRF_UOC2_CON0);

	cru_set_soft_reset(SOFT_RST_HSICPHY, true);
	udelay(12);
	cru_set_soft_reset(SOFT_RST_HSICPHY, false);
	mdelay(2);

	*phy_con0 = ((1<<10)<<16)|(1<<10);
	udelay(2);
	*phy_con0 = ((1<<10)<<16)|(0<<10);
	udelay(2);

	cru_set_soft_reset(SOFT_RST_HSIC_AHB, true);
	udelay(2);
	cru_set_soft_reset(SOFT_RST_HSIC_AHB, false);
	udelay(2);
}

struct rkehci_platform_data rkehci_pdata = {
	.hclk_hsic = NULL,
	.hsic_phy_12m = NULL,
	.hsic_phy_480m = NULL,
	.clk_status = -1,
	.hw_init = rkehci_hw_init,
	.clock_init = rkehci_clock_init,
	.clock_enable = rkehci_clock_enable,
	.soft_reset = rkehci_soft_reset,
};

static struct resource resources_hsusb_host[] = {
    {
        .start = IRQ_HSIC,
        .end   = IRQ_HSIC,
        .flags = IORESOURCE_IRQ,
    },
    {
        .start = RK30_HSIC_PHYS,
        .end   = RK30_HSIC_PHYS + RK30_HSIC_SIZE - 1,
        .flags = IORESOURCE_MEM,
    },
};

struct platform_device device_hsusb_host = {
    .name           = "rk_hsusb_host",
    .id             = -1,
    .num_resources  = ARRAY_SIZE(resources_hsusb_host),
    .resource       = resources_hsusb_host,
    .dev            = {
        .coherent_dma_mask      = 0xffffffff,
        .platform_data  = &rkehci_pdata,
    },
};
#endif

static int __init usbdev_init_devices(void)
{
#ifdef CONFIG_USB_EHCI_RK
	platform_device_register(&device_hsusb_host);
#endif
#ifdef CONFIG_USB20_OTG
	platform_device_register(&device_usb20_otg);
#endif
#ifdef CONFIG_USB20_HOST
	platform_device_register(&device_usb20_host);
#endif
    return 0;
}
arch_initcall(usbdev_init_devices);
#endif
