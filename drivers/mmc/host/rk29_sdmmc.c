/* drivers/mmc/host/rk29_sdmmc.c
 *
 * Copyright (C) 2010 ROCKCHIP, Inc.
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
 * mount -t debugfs debugfs /data/debugfs;cat /data/debugfs/mmc0/status
 * echo 't' >/proc/sysrq-trigger
 * echo 19 >/sys/module/wakelock/parameters/debug_mask
 * vdc volume uevent on
 */
 
#include <linux/blkdev.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>

#include <mach/board.h>
#include <mach/io.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <asm/unaligned.h>

#include <asm/dma.h>
#include <mach/dma-pl330.h>
#include <asm/scatterlist.h>

#include "rk29_sdmmc.h"


#define RK29_SDMMC_xbw_Debug 0

#if RK29_SDMMC_xbw_Debug 
int debug_level = 5;
#define xbwprintk(n, format, arg...) \
	if (n <= debug_level) {	 \
		printk(format,##arg); \
	}
#else
#define xbwprintk(n, arg...)
#endif

#define RK29_SDMMC_VERSION "Ver.6.07 The last modify date is 2013-12-20"

#define RK29_SDMMC_DEFAULT_SDIO_FREQ   0 // 1--run in default frequency(50Mhz); 0---run in 25Mhz, 
#if defined(CONFIG_MT6620)|| defined(CONFIG_ESP8089)
#define RK29_MAX_SDIO_FREQ   50000000    //set max-sdio-frequency 50Mhz in MTK module.
#else
#define RK29_MAX_SDIO_FREQ   25000000    //set max-sdio-frequency 25Mhz at the present time
#endif

//use the new iomux-API
#define DRIVER_SDMMC_USE_NEW_IOMUX_API 0

#define SWITCH_VOLTAGE_18_33            0 //RK30_PIN2_PD7 //Temporary experiment
#define SWITCH_VOLTAGE_ENABLE_VALUE_33  GPIO_LOW

#define SDMMC_SUPPORT_DDR_MODE  1

static void rk29_sdmmc_start_error(struct rk29_sdmmc *host);
static int rk29_sdmmc_clear_fifo(struct rk29_sdmmc *host);
int rk29_sdmmc_hw_init(void *data);

static void rk29_sdmmc_write(unsigned char  __iomem	*regbase, unsigned int regOff,unsigned int val)
{
	__raw_writel(val,regbase + regOff);
}

static unsigned int rk29_sdmmc_read(unsigned char  __iomem	*regbase, unsigned int regOff)
{
	return __raw_readl(regbase + regOff);
}

static int rk29_sdmmc_regs_printk(struct rk29_sdmmc *host)
{
    struct sdmmc_reg *regs = rk_sdmmc_regs;

    while( regs->name != 0 )
    {
        printk("%s: (0x%04x) = 0x%08x\n", regs->name, regs->addr, rk29_sdmmc_read(host->regs,  regs->addr));
        regs++;
	}
	
	printk("=======printk %s-register end =========\n", host->dma_name);
	return 0;
}

static void rk29_sdmmc_enable_irq(struct rk29_sdmmc *host, bool irqflag)
{
	unsigned long flags;

    if(!host)
        return;
    
	local_irq_save(flags);
	if(host->irq_state != irqflag)
	{
	    host->irq_state = irqflag;
	    if(irqflag)
	    {
	        enable_irq(host->irq);
	    }
	    else
	    {
	        disable_irq(host->irq);
	    }
	}
	local_irq_restore(flags);
}


#ifdef RK29_SDMMC_NOTIFY_REMOVE_INSERTION
/*
** debug the progress.
**
**  # echo version > sys/sd-sdio/rescan         //check the current sdmmc-driver version
**
**  # echo sd-reset > sys/sd-sdio/rescan        //run mmc0 mmc_rescan again
**  # echo sd-regs > sys/sd-sdio/rescan         //printk all registers of mmc0.
**
**  # echo sdio1-reset > sys/sd-sdio/rescan     //run mmc1 mmc_rescan again
**  # echo sdio1-regs > sys/sd-sdio/rescan      //printk all registers of mmc1.
**
*/
ssize_t rk29_sdmmc_progress_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
    struct rk29_sdmmc	*host = NULL;
    static u32 unmounting_times = 0;
    static char oldbuf[64];
    
    if( !strncmp(buf,"version" , strlen("version")))
    {
        printk(KERN_INFO "\n The driver SDMMC named 'rk29_sdmmc.c' is %s.\n", RK29_SDMMC_VERSION);
        return count;
    }
    
    //envalue the address of host base on input-parameter.
    if( !strncmp(buf,"sd-" , strlen("sd-")) )
    {
        host = (struct rk29_sdmmc	*)globalSDhost[0];
        if(!host)
        {
            printk(KERN_WARNING "%s..%d.. fail to call progress_store because the host is null. \n",__FUNCTION__,__LINE__);
            return count;
        }
    }    
    else if(  !strncmp(buf,"sdio1-" , strlen("sdio1-")) )
    {
        host = (struct rk29_sdmmc	*)globalSDhost[RK29_CTRL_SDIO1_ID];
        if(!host)
        {
            printk(KERN_WARNING "%s..%d.. fail to call progress_store because the host-sdio1 is null.\n",__FUNCTION__,__LINE__);
            return count;
        }
    }
    else if(  !strncmp(buf,"sdio2-" , strlen("sdio2-")) )
    {
        host = (struct rk29_sdmmc	*)globalSDhost[RK29_CTRL_SDIO2_ID];
        if(!host)
        {
            printk(KERN_WARNING "%s..%d.. fail to call progress_store because the host-sdio2 is null.\n",__FUNCTION__,__LINE__);
            return count;
        }
    }
    else
    {
        printk(KERN_WARNING "%s..%d.. You want to use sysfs for SDMMC but input-parameter is wrong.\n",__FUNCTION__,__LINE__);
        return count;
    }
    rk29_sdmmc_enable_irq(host, false);

    //spin_lock(&host->lock);
    if(strncmp(buf,oldbuf , strlen(buf)))
    {
	    printk(KERN_INFO ".%d.. MMC0 receive the message %s from VOLD.[%s]\n", __LINE__, buf, host->dma_name);
	    strcpy(oldbuf, buf);
	}

	/*
     *  //deal with the message
     *  insert card state-change:  No-Media ==> Pending ==> Idle-Unmounted ==> Checking ==>Mounted
     *  remove card state-change:  Unmounting ==> Idle-Unmounted ==> No-Media
    */
    #if !defined(CONFIG_USE_SDMMC0_FOR_WIFI_DEVELOP_BOARD)
    if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
    {
        if(!strncmp(buf, "sd-Unmounting", strlen("sd-Unmounting")))
        {
            if(unmounting_times++%10 == 0)
            {
                printk(KERN_INFO ".%d.. MMC0 receive the message Unmounting(waitTimes=%d) from VOLD.[%s]\n", \
                    __LINE__, unmounting_times, host->dma_name);
            }

            if(0 == host->mmc->re_initialized_flags)
                mod_timer(&host->detect_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_REMOVAL_DELAY*2));
        }
        else if(!strncmp(buf, "sd-Idle-Unmounted", strlen("sd-Idle-Unmounted")))
        {
            if(0 == host->mmc->re_initialized_flags)
                mod_timer(&host->detect_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_REMOVAL_DELAY*2));
        }
        else if( !strncmp(buf, "sd-No-Media", strlen("sd-No-Media")))
        {
            printk(KERN_INFO ".%d.. MMC0 receive the message No-Media from VOLD. waitTimes=%d [%s]\n" ,\
                __LINE__,unmounting_times, host->dma_name);
                
            del_timer_sync(&host->detect_timer);
            host->mmc->re_initialized_flags = 1;
            unmounting_times = 0;
            
            if(test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags))
            {
                mmc_detect_change(host->mmc, 0);
            }
        }
        else if( !strncmp(buf, "sd-Ready", strlen("sd-Ready")))
        {
            printk(KERN_INFO ".%d.. MMC0 receive the message Ready(ReInitFlag=%d) from VOLD. waitTimes=%d [%s]\n" ,\
                __LINE__, host->mmc->re_initialized_flags, unmounting_times, host->dma_name);
								
            unmounting_times = 0;
			host->mmc->re_initialized_flags = 1;            
        }
        else if( !strncmp(buf,"sd-reset" , strlen("sd-reset")) ) 
        {
            printk(KERN_INFO ".%d.. Now manual reset for SDMMC0. [%s]\n",__LINE__, host->dma_name);
            rk29_sdmmc_hw_init(host);
            mmc_detect_change(host->mmc, 0);           
        }
        else if( !strncmp(buf, "sd-regs", strlen("sd-regs")))
        {
            printk(KERN_INFO ".%d.. Now printk the register of SDMMC0. [%s]\n",__LINE__, host->dma_name); 
            rk29_sdmmc_regs_printk(host);
        }

    }
    #else
    if(0 == host->host_dev_id)
    {
        if( !strncmp(buf,"sd-reset" , strlen("sd-reset")) ) 
        {
            printk(KERN_INFO ".%d.. Now manual reset for SDMMC0. [%s]\n",__LINE__, host->dma_name);
            rk29_sdmmc_hw_init(host);
            mmc_detect_change(host->mmc, 0);           
        }
        else if( !strncmp(buf, "sd-regs", strlen("sd-regs")))
        {
            printk(KERN_INFO ".%d.. Now printk the register of SDMMC0. [%s]\n",__LINE__, host->dma_name); 
            rk29_sdmmc_regs_printk(host);
        }
    }
    #endif
    else if(RK29_CTRL_SDIO1_ID == host->host_dev_id)
    {
        if( !strncmp(buf, "sdio1-regs", strlen("sdio1-regs")))
        {
            printk(KERN_INFO ".%d.. Now printk the register of SDMMC1. [%s]\n",__LINE__, host->dma_name); 
            rk29_sdmmc_regs_printk(host);
        }
        else if( !strncmp(buf,"sdio1-reset" , strlen("sdio1-reset")) ) 
        {
            printk(KERN_INFO ".%d.. Now manual reset for SDMMC1. [%s]\n",__LINE__, host->dma_name);
            rk29_sdmmc_hw_init(host);
            mmc_detect_change(host->mmc, 0);           
        }
    }
    else if(RK29_CTRL_SDIO2_ID == host->host_dev_id)
    {
        if( !strncmp(buf, "sdio2-regs", strlen("sdio2-regs")))
        {
            printk(KERN_INFO ".%d.. Now printk the register of SDMMC2. [%s]\n",__LINE__, host->dma_name); 
            rk29_sdmmc_regs_printk(host);
        }
        else if( !strncmp(buf,"sdio2-reset" , strlen("sdio2-reset")) ) 
        {
            printk(KERN_INFO ".%d.. Now manual reset for SDMMC2. [%s]\n",__LINE__, host->dma_name);
            rk29_sdmmc_hw_init(host);
            mmc_detect_change(host->mmc, 0);           
        }
    }
    
   // spin_unlock(&host->lock);
    
    rk29_sdmmc_enable_irq(host, true);    
    return count;
}

struct kobj_attribute mmc_reset_attrs = 
{
        .attr = {
                .name = "rescan",
                .mode = 0764},
        .show = NULL,
        .store = rk29_sdmmc_progress_store,
};
struct attribute *mmc_attrs[] = 
{
        &mmc_reset_attrs.attr,
        NULL
};

static struct kobj_type mmc_kset_ktype = {
	.sysfs_ops	= &kobj_sysfs_ops,
	.default_attrs = &mmc_attrs[0],
};

static int rk29_sdmmc_progress_add_attr( struct platform_device *pdev )
{
        int result;
		 struct kobject *parentkobject; 
        struct kobject * me = kmalloc(sizeof(struct kobject) , GFP_KERNEL );
        if(!me)
            return -ENOMEM;
            
        memset(me ,0,sizeof(struct kobject));
        kobject_init( me , &mmc_kset_ktype );
        
        parentkobject = &pdev->dev.kobj ;
		result = kobject_add( me , parentkobject->parent->parent->parent,"%s", "sd-sdio" );	

        return result;
}
#endif

/**
**  This function checks whether the core supports the IDMAC.
**  return Returns 1 if HW supports IDMAC, else returns 0.
*/
u32 rk_sdmmc_check_idma_support(struct rk29_sdmmc *host)
{
     u32 retval = 0;
     u32 ctrl_reg;
	 ctrl_reg = rk29_sdmmc_read(host->regs, SDMMC_CTRL);
     if(ctrl_reg & SDMMC_CTRL_USE_IDMAC)
        retval = 1;//Return "true", indicating the hardware supports IDMAC
     else
        retval = 0;// Hardware doesnot support IDMAC

	return retval;
}

static u32 rk29_sdmmc_prepare_command(struct mmc_command *cmd)
{
	u32		cmdr = cmd->opcode;

	switch(cmdr)
	{
	    case MMC_GO_IDLE_STATE: 
            cmdr |= (SDMMC_CMD_INIT | SDMMC_CMD_PRV_DAT_NO_WAIT);
            break;
        case MMC_STOP_TRANSMISSION:
            cmdr |= (SDMMC_CMD_STOP | SDMMC_CMD_PRV_DAT_NO_WAIT);
            break;
        case MMC_SEND_STATUS:
        case MMC_GO_INACTIVE_STATE:   
            cmdr |= SDMMC_CMD_PRV_DAT_NO_WAIT;
            break;
        default:
            cmdr |= SDMMC_CMD_PRV_DAT_WAIT;
            break;
	}

    /* response type */
	switch(mmc_resp_type(cmd))
	{
	    case MMC_RSP_R1:
        case MMC_RSP_R1B:
            // case MMC_RSP_R5:  //R5,R6,R7 is same with the R1
            //case MMC_RSP_R6:
            // case R6m_TYPE:
            // case MMC_RSP_R7:
            cmdr |= (SDMMC_CMD_RESP_CRC | SDMMC_CMD_RESP_SHORT | SDMMC_CMD_RESP_EXP);
            break;
        case MMC_RSP_R3:
            //case MMC_RSP_R4:
            /* these response not contain crc7, so don't care crc error and response error */
            cmdr |= (SDMMC_CMD_RESP_NO_CRC | SDMMC_CMD_RESP_SHORT | SDMMC_CMD_RESP_EXP); 
            break;
        case MMC_RSP_R2:
            cmdr |= (SDMMC_CMD_RESP_CRC | SDMMC_CMD_RESP_LONG | SDMMC_CMD_RESP_EXP);
            break;
        case MMC_RSP_NONE:
            cmdr |= (SDMMC_CMD_RESP_CRC_NOCARE | SDMMC_CMD_RESP_NOCARE | SDMMC_CMD_RESP_NO_EXP);  
            break;
        default:
            cmdr |= (SDMMC_CMD_RESP_CRC_NOCARE | SDMMC_CMD_RESP_NOCARE | SDMMC_CMD_RESP_NO_EXP); 
            break;
	}

	return cmdr;
}

void  rk29_sdmmc_set_frq(struct rk29_sdmmc *host)
{
    struct mmc_host *mmchost = platform_get_drvdata(host->pdev);
    struct mmc_card	*card;
    struct mmc_ios *ios;
	unsigned int max_dtr;
    
    extern void mmc_set_clock(struct mmc_host *host, unsigned int hz);

    if(!mmchost)
        return;

    card = (struct mmc_card	*)mmchost->card;
    ios  = ( struct mmc_ios *)&mmchost->ios;

    if(!card || !ios)
        return;

    if(MMC_POWER_ON == ios->power_mode)
        return;

    max_dtr = (unsigned int)-1;
    
    if (mmc_card_highspeed(card)) 
    {
        if (max_dtr > card->ext_csd.hs_max_dtr)
            max_dtr = card->ext_csd.hs_max_dtr;
            
    }
    else if (max_dtr > card->csd.max_dtr) 
    {
        if(MMC_TYPE_SD == card->type)
        {
	        max_dtr = (card->csd.max_dtr > SD_FPP_FREQ) ? SD_FPP_FREQ : (card->csd.max_dtr);
	    }
	    else
	    {	
            max_dtr = (card->csd.max_dtr > MMC_FPP_FREQ) ? MMC_FPP_FREQ : (card->csd.max_dtr);
	    }	    
    }

    xbwprintk(7, "%s..%d...  call mmc_set_clock() set clk=%d [%s]\n", \
			__FUNCTION__, __LINE__, max_dtr, host->dma_name);

  
    mmc_set_clock(mmchost, max_dtr);

}


static int rk29_sdmmc_start_command(struct rk29_sdmmc *host, struct mmc_command *cmd, u32 cmd_flags)
{
 	int tmo = RK29_SDMMC_SEND_START_TIMEOUT*10;//wait 60ms cycle.
	
 	host->cmd = cmd;
 	host->old_cmd = cmd->opcode;
 	host->errorstep = 0;
 	host->pending_events = 0;
	host->completed_events = 0;	 	
    host->retryfunc = 0;
    host->cmd_status = 0;
	
    if(MMC_STOP_TRANSMISSION != cmd->opcode)
    {
        host->data_status = 0;
    }
    
    if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
    {
        //adjust the frequency division control of SDMMC0 every time.
        rk29_sdmmc_set_frq(host);
    }
			
	rk29_sdmmc_write(host->regs, SDMMC_CMDARG, cmd->arg); // write to SDMMC_CMDARG register
	
#if defined(CONFIG_ARCH_RK29)	
	rk29_sdmmc_write(host->regs, SDMMC_CMD, cmd_flags | SDMMC_CMD_START); // write to SDMMC_CMD register
#else
    rk29_sdmmc_write(host->regs, SDMMC_CMD, cmd_flags | SDMMC_CMD_USE_HOLD_REG |SDMMC_CMD_START); // write to SDMMC_CMD register
#endif

    xbwprintk(1,"\n%s..%d..************.start cmd=%d, arg=0x%x,start_cmd=0x%x ********  [%s]\n", \
			__FUNCTION__, __LINE__, cmd->opcode, cmd->arg,rk29_sdmmc_read(host->regs, SDMMC_CMD), host->dma_name);

    //Special treatment for emmc
    if(RK29_SDMMC_EMMC_ID == host->host_dev_id)
    {
        if((0==cmd->opcode)||(1==cmd->opcode))
            tmo = 5;
    }
    
	/* wait until CIU accepts the command */
	while (--tmo && (rk29_sdmmc_read(host->regs, SDMMC_CMD) & SDMMC_CMD_START))
	{
		udelay(2);//cpu_relax();
	}

	host->complete_done = 0;
	host->mmc->doneflag = 1;	
	
	if(!tmo)
	{
	    if(0==cmd->retries)
	    {
    		printk(KERN_WARNING "%s..%d..  CMD_START timeout! CMD%d(arg=0x%x, retries=%d) [%s]\n",\
    				__FUNCTION__,__LINE__, cmd->opcode, cmd->arg, cmd->retries,host->dma_name);
		}

		cmd->error = -ETIMEDOUT;
		host->mrq->cmd->error = -ETIMEDOUT;
		del_timer_sync(&host->request_timer);
		
		host->errorstep = 0x1;
		return SDM_WAIT_FOR_CMDSTART_TIMEOUT;
	}
    host->errorstep = 0xfe;

	return SDM_SUCCESS;
}

static int rk29_sdmmc_reset_fifo(struct rk29_sdmmc *host)
{
    u32     value;
	int     timeout;
	int     ret = SDM_SUCCESS;
	
    value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
    if (!(value & SDMMC_STAUTS_FIFO_EMPTY))
    {
        value = rk29_sdmmc_read(host->regs, SDMMC_CTRL);
        value |= SDMMC_CTRL_FIFO_RESET;
        rk29_sdmmc_write(host->regs, SDMMC_CTRL, value);

        timeout = 1000;
        while (((value = rk29_sdmmc_read(host->regs, SDMMC_CTRL)) & (SDMMC_CTRL_FIFO_RESET)) && (timeout > 0))
        {
            udelay(1);
            timeout--;
        }
        if (timeout == 0)
        {
            host->errorstep = 0x2;
            ret = SDM_WAIT_FOR_FIFORESET_TIMEOUT;
        }
    }
    
	return ret;
	
}

static int rk29_sdmmc_wait_unbusy(struct rk29_sdmmc *host)
{
    int time_out = 500000;//250000; //max is 250ms; //adapt the value to the sick card.  modify at 2011-10-08

#if SDMMC_USE_INT_UNBUSY
    if((24==host->cmd->opcode)||(25==host->cmd->opcode))
        return SDM_SUCCESS;
#endif  
	while (rk29_sdmmc_read(host->regs, SDMMC_STATUS) & (SDMMC_STAUTS_DATA_BUSY|SDMMC_STAUTS_MC_BUSY)) 
	{
		udelay(1);
		time_out--;

		if(time_out == 0)
		{
		    host->errorstep = 0x3;
		    return SDM_BUSY_TIMEOUT;
		}
	}

	return SDM_SUCCESS;
}

static void rk29_sdmmc_dma_cleanup(struct rk29_sdmmc *host)
{
	if (host->data) 
	{
		dma_unmap_sg(&host->pdev->dev, host->data->sg, host->data->sg_len,
		     ((host->data->flags & MMC_DATA_WRITE)
		      ? DMA_TO_DEVICE : DMA_FROM_DEVICE));		
    }
}

static void rk29_sdmmc_stop_dma(struct rk29_sdmmc *host)
{
    int ret = 0;
    
	if(host->use_dma == 0)
		return;
		
	if (host->dma_info.chn> 0) 
	{
		rk29_sdmmc_dma_cleanup(host); 
		
		ret = rk29_dma_ctrl(host->dma_info.chn,RK29_DMAOP_STOP);
		if(ret < 0)
		{
            printk(KERN_WARNING "%s..%d...rk29_dma_ctrl STOP error! [%s]\n", __FUNCTION__, __LINE__, host->dma_name);
            host->errorstep = 0x95;
            return;
		}

		ret = rk29_dma_ctrl(host->dma_info.chn,RK29_DMAOP_FLUSH);
		if(ret < 0)
		{
            printk(KERN_WARNING "%s..%d...rk29_dma_ctrl FLUSH error! [%s]\n", __FUNCTION__, __LINE__, host->dma_name);
            host->errorstep = 0x96;
            return;
		}
		
	} 
	else 
	{
		/* Data transfer was stopped by the interrupt handler */
		rk29_sdmmc_set_pending(host, EVENT_DATA_COMPLETE);
	}
}

static void rk29_sdmmc_control_host_dma(struct rk29_sdmmc *host, bool enable)
{
    u32 value = rk29_sdmmc_read(host->regs, SDMMC_CTRL);

    if (enable)
        value |= SDMMC_CTRL_DMA_ENABLE;
    else
        value &= ~(SDMMC_CTRL_DMA_ENABLE);

    rk29_sdmmc_write(host->regs, SDMMC_CTRL, value);
}

static void send_stop_cmd(struct rk29_sdmmc *host)
{
    int ret, i;
    
    if(host->mrq->cmd->error)
    {
        //stop DMA
        if(host->dodma)
        {
            rk29_sdmmc_stop_dma(host);
            rk29_sdmmc_control_host_dma(host, FALSE);

            host->dodma = 0;
        }
        
        ret= rk29_sdmmc_clear_fifo(host);
        if(SDM_SUCCESS != ret)
        {
            xbwprintk(3, "%s..%d..  clear fifo error before call CMD_STOP [%s]\n", \
							__FUNCTION__, __LINE__, host->dma_name);
        }
    }

    i = 0;
    while(++i>2)
    {
        ret = rk29_sdmmc_wait_unbusy(host);
        if(SDM_SUCCESS == ret)
            break;
       
        mod_timer(&host->request_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_SEND_START_TIMEOUT+2500));
        xbwprintk(7, "%d..%s:   retry times=%d    before send_stop_cmd [%s].\n", __LINE__, __FUNCTION__, i, host->dma_name);

    }


    host->errorstep = 0xe1;     
    mod_timer(&host->request_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_SEND_START_TIMEOUT+2500));
		
    host->stopcmd.opcode = MMC_STOP_TRANSMISSION;
    host->stopcmd.flags  = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;;
    host->stopcmd.arg = 0;
    host->stopcmd.data = NULL;
    host->stopcmd.mrq = NULL;
    host->stopcmd.retries = 0;
    host->stopcmd.error = 0;
    if(host->mrq && host->mrq->stop)
    {
        host->mrq->stop->error = 0;
    }
    
    host->cmdr = rk29_sdmmc_prepare_command(&host->stopcmd);
    
    ret = rk29_sdmmc_start_command(host, &host->stopcmd, host->cmdr); 
    if(SDM_SUCCESS != ret)
    {
        rk29_sdmmc_start_error(host);

        host->state = STATE_IDLE;
        host->complete_done = 4;
    }
    host->errorstep = 0xe2;
}


/* This function is called by the DMA driver from tasklet context. */
static void rk29_sdmmc_dma_complete(void *arg, int size, enum rk29_dma_buffresult result) 
{
	struct rk29_sdmmc	*host = arg;

	if(host->use_dma == 0)
		return;

	host->intInfo.transLen = host->intInfo.desLen;	
}

static void rk_sdmmc_push_data32(struct rk29_sdmmc *host, void *buf, int cnt)
{
	u32 *pdata = (u32 *)buf;

	WARN_ON(cnt % 4 != 0);
	WARN_ON((unsigned long)pdata & 0x3);

	cnt = cnt >> 2;
	while (cnt > 0) {
		rk29_sdmmc_write(host->regs, SDMMC_DATA, *pdata++);
		cnt--;
	}
}

static void rk_sdmmc_pull_data32(struct rk29_sdmmc *host, void *buf, int cnt)
{
	u32 *pdata = (u32 *)buf;

	WARN_ON(cnt % 4 != 0);
	WARN_ON((unsigned long)pdata & 0x3);

	cnt = cnt >> 2;
	while (cnt > 0) {
		*pdata++ = rk29_sdmmc_read(host->regs, SDMMC_DATA);
		cnt--;
	}
}

static int rk29_sdmmc_read_data_pio(struct rk29_sdmmc *host)
{
	struct scatterlist	*sg;
	void    *buf;
	unsigned int offset;
	struct mmc_data	 *data = host->data;
	u32			value;
	unsigned int		nbytes=0, len;

    value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
	if ( value& SDMMC_STAUTS_FIFO_EMPTY)
	    goto done;

    if(NULL == host->data)
       goto done; 

    if((NULL == host)&&(NULL == host->data))
        goto done;
        
	sg = host->sg;
	buf = sg_virt(sg);
	offset =  host->pio_offset;
	
    while ( (host->intInfo.transLen < host->intInfo.desLen)  && (!(value & SDMMC_STAUTS_FIFO_EMPTY)) )
    {
        len = SDMMC_GET_FCNT(value) << PIO_DATA_SHIFT;
        if (offset + len <= sg->length) {
			host->pull_data(host, (void *)(buf + offset), len);

			offset += len;
			nbytes += len;
			host->intInfo.transLen++;

			if (offset == sg->length) {
				flush_dcache_page(sg_page(sg));
				host->sg = sg = sg_next(sg);
				if (!sg)
					goto done;

				offset = 0;
				buf = sg_virt(sg);
			}
		}else {
			unsigned int remaining = sg->length - offset;
			host->pull_data(host, (void *)(buf + offset),remaining);
			nbytes += remaining;
			host->intInfo.transLen++;

			flush_dcache_page(sg_page(sg));
			host->sg = sg = sg_next(sg);
			if (!sg)
				goto done;

			offset = len - remaining;
			buf = sg_virt(sg);
			host->pull_data(host, buf, offset);
			nbytes += offset;
		}
		
        host->pio_offset = offset;
        data->bytes_xfered += nbytes;

        value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
    }
    return 0;
done:
	data->bytes_xfered += nbytes;
    return 0;
}


static int rk29_sdmmc_write_data_pio(struct rk29_sdmmc *host)
{
	struct scatterlist	*sg;
	void    *buf;
	unsigned int offset;
	struct mmc_data		*data = host->data;
	u32			value;
	unsigned int		nbytes=0, len;

    value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
	if ( value& SDMMC_STAUTS_FIFO_EMPTY)
	    goto done;

    if(NULL == host->data)
       goto done; 

    if((NULL == host)&&(NULL == host->data))
        goto done;

	sg = host->sg;
	buf = sg_virt(sg);
    offset =  host->pio_offset;

    while ( (host->intInfo.transLen < host->intInfo.desLen)  && (!(value & SDMMC_STAUTS_FIFO_EMPTY)) )
    {
        len = SDMMC_FIFO_SZ -(SDMMC_GET_FCNT(value) << PIO_DATA_SHIFT);
		if (offset + len <= sg->length) {
			host->push_data(host, (void *)(buf + offset), len);

			offset += len;
			nbytes += len;
			host->intInfo.transLen++;
			if (offset == sg->length) {
				host->sg = sg = sg_next(sg);
				if (!sg)
					goto done;

				offset = 0;
				buf = sg_virt(sg);
			}
		} else {
			unsigned int remaining = sg->length - offset;

			host->push_data(host, (void *)(buf + offset),
					remaining);
			nbytes += remaining;
			host->intInfo.transLen++;

			host->sg = sg = sg_next(sg);
			if (!sg)
				goto done;

			offset = len - remaining;
			buf = sg_virt(sg);
			host->push_data(host, (void *)buf, offset);
			nbytes += offset;
		}

        host->pio_offset = offset;
	    data->bytes_xfered += nbytes;

        value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
    }
    
    return 0;
    
done:
    data->bytes_xfered += nbytes;
    return 0;
}

static int rk29_sdmmc_submit_data_dma(struct rk29_sdmmc *host, struct mmc_data *data)
{
	unsigned int	i,direction, sgDirection;
	int ret, dma_len=0;
	
	if(host->use_dma == 0)
	{
	    printk(KERN_WARNING "%s..%d...setup DMA fail!!!!!!. host->use_dma=0 [%s]\n", __FUNCTION__, __LINE__, host->dma_name);
	    host->errorstep = 0x4;
		return -ENOSYS;
	}
	/* If we don't have a channel, we can't do DMA */
	if (host->dma_info.chn < 0)
	{
	    printk(KERN_WARNING "%s..%d...setup DMA fail!!!!!!!. dma_info.chn < 0  [%s]\n", __FUNCTION__, __LINE__, host->dma_name);
	    host->errorstep = 0x5;
		return -ENODEV;
	}

	if (data->blksz & 3)
	{
	    printk(KERN_ERR "%s..%d...data_len not aligned to 4bytes.  [%s]\n", \
	        __FUNCTION__, __LINE__, host->dma_name);
	        
		return -EINVAL;
    }

	if (data->flags & MMC_DATA_READ)
	{
		direction = RK29_DMASRC_HW;  
		sgDirection = DMA_FROM_DEVICE; 
	}
	else
	{
		direction = RK29_DMASRC_MEM;
		sgDirection = DMA_TO_DEVICE;
	}

	ret = rk29_dma_ctrl(host->dma_info.chn,RK29_DMAOP_STOP);
	if(ret < 0)
	{
	    printk(KERN_WARNING "%s..%d...rk29_dma_ctrl stop error![%s]\n", __FUNCTION__, __LINE__, host->dma_name);
	    host->errorstep = 0x91;
		return -ENOSYS;
	}
	
	ret = rk29_dma_ctrl(host->dma_info.chn,RK29_DMAOP_FLUSH);	
	if(ret < 0)
	{
        printk(KERN_WARNING "%s..%d...rk29_dma_ctrl flush error![%s]\n", __FUNCTION__, __LINE__, host->dma_name);
        host->errorstep = 0x91;
        return -ENOSYS;
	}

	
    ret = rk29_dma_devconfig(host->dma_info.chn, direction, (unsigned long )(host->dma_addr));
    if(0 != ret)
    {
        printk(KERN_WARNING "%s..%d...call rk29_dma_devconfig() fail ![%s]\n", __FUNCTION__, __LINE__, host->dma_name);
        host->errorstep = 0x8;
        return -ENOSYS;
    }
    
	dma_len = dma_map_sg(&host->pdev->dev, data->sg, data->sg_len, sgDirection);						                	   
	for (i = 0; i < dma_len; i++)
	{
    	ret = rk29_dma_enqueue(host->dma_info.chn, host, sg_dma_address(&data->sg[i]),sg_dma_len(&data->sg[i]));
    	if(ret < 0)
    	{
            printk(KERN_WARNING "%s..%d...call rk29_dma_devconfig() fail ![%s]\n", __FUNCTION__, __LINE__, host->dma_name);
            host->errorstep = 0x93;
            return -ENOSYS;
    	}
    }
    	
	rk29_sdmmc_control_host_dma(host, TRUE);// enable dma
	ret = rk29_dma_ctrl(host->dma_info.chn, RK29_DMAOP_START);
	if(ret < 0)
	{
        printk(KERN_WARNING "%s..%d...rk29_dma_ctrl start error![%s]\n", __FUNCTION__, __LINE__, host->dma_name);
        host->errorstep = 0x94;
        return -ENOSYS;
	}
	
	return 0;
}


static int rk29_sdmmc_prepare_write_data(struct rk29_sdmmc *host, struct mmc_data *data)
{
    int     output;
    u32    i = 0;
    u32     dataLen;
    u32     count, *pBuf = (u32 *)host->pbuf;

    output = SDM_SUCCESS;
    dataLen = data->blocks*data->blksz;
    
    host->dodma = 0; //DMA still no request;
 
    //SDMMC controller request the data is multiple of 4.
    count = (dataLen >> 2) + ((dataLen & 0x3) ? 1:0);
#if 0
    if(count <= FIFO_DEPTH)    
#else
    #if defined(CONFIG_ARCH_RK29)
    if( (count <= 0x20) && (RK29_CTRL_SDMMC_ID != host->host_dev_id))
    #else
    if( (count <= 0x80) && ((RK29_CTRL_SDMMC_ID != host->host_dev_id) || (RK29_SDMMC_EMMC_ID != host->host_dev_id)))
    #endif
#endif
    {
           
        #if 1
        for (i=0; i<count; i++)
        {
            rk29_sdmmc_write(host->regs, SDMMC_DATA, pBuf[i]);
        }
        #else
        rk29_sdmmc_write_data_pio(host);
        #endif
    }
    else
    {
        host->intInfo.desLen = count;
        host->intInfo.transLen = 0;
        host->intInfo.pBuf = (u32 *)pBuf;
        
        if(0)//(host->intInfo.desLen <= TX_WMARK) 
        {  
            //use pio-mode 
            rk29_sdmmc_write_data_pio(host);
            return SDM_SUCCESS;
        } 
        else 
        {
            xbwprintk(7, "%s..%d...   trace data,   [%s]\n", __FUNCTION__, __LINE__,  host->dma_name);
            output = rk29_sdmmc_submit_data_dma(host, data);
            if(output)
            {
        		host->dodma = 0;
        			
        	    printk(KERN_WARNING "%s..%d... CMD%d setupDMA failure!!!!! pre_cmd=%d  [%s]\n", \
						__FUNCTION__, __LINE__, host->cmd->opcode,host->old_cmd, host->dma_name);
        	    
				host->errorstep = 0x81;

        		rk29_sdmmc_control_host_dma(host, FALSE); 
    		}
    		else
    		{
    		    host->dodma = 1;
    		}
        }
       
    }

    return output;
}

static int rk29_sdmmc_prepare_read_data(struct rk29_sdmmc *host, struct mmc_data *data)
{
    u32  count = 0;
    u32  dataLen;
    int   output;

    output = SDM_SUCCESS;
    dataLen = data->blocks*data->blksz;
    
    host->dodma = 0;//DMA still no request;

    //SDMMC controller request the data is multiple of 4.
    count = (dataLen >> 2) ;//+ ((dataLen & 0x3) ? 1:0);

    host->intInfo.desLen = (dataLen >> 2);
    host->intInfo.transLen = 0;
    host->intInfo.pBuf = (u32 *)host->pbuf;
       
    if(count > (RX_WMARK+1))  //datasheet error.actually, it can nont waken the interrupt when less and equal than RX_WMARK+1
    {
        if(0)//(host->intInfo.desLen <= RX_WMARK)
        {
            //use pio-mode
            rk29_sdmmc_read_data_pio(host);
            return SDM_SUCCESS;
        }        
        else 
        {
            output = rk29_sdmmc_submit_data_dma(host, data);
            if(output)
            {
        		host->dodma = 0;
        			
        	    printk(KERN_WARNING "%s..%d... CMD%d setupDMA  failure!!!  [%s]\n", \
						__FUNCTION__, __LINE__, host->cmd->opcode, host->dma_name);

        	    host->errorstep = 0x82;

        		rk29_sdmmc_control_host_dma(host, FALSE); 
    		}
    		else
    		{
    		    host->dodma = 1;
    		}
        }
    }

    return output;
}



static int rk29_sdmmc_read_remain_data(struct rk29_sdmmc *host, u32 originalLen, void *pDataBuf)
{
	struct mmc_data		*data;	
    struct scatterlist  *sg;
    u32   *buf,  i = 0;

    if(1 == host->dodma)
    {
        data = host->data;
        sg = host->sg;
        buf = (u32 *)sg_virt(sg);

        for_each_sg(data->sg, sg, data->sg_len, i) 
        {
            if (!sg)
                return -1;
            if (sg_is_last(sg))
                break;
        }

        flush_dcache_page(sg_page(sg));
        host->sg = sg;
    }

    rk29_sdmmc_read_data_pio(host);
    
    return SDM_SUCCESS;
}

static void rk29_sdmmc_submit_data(struct rk29_sdmmc *host, struct mmc_data *data)
{
    int ret,i;
    struct scatterlist *sg;
    
    if(data)
    {
        host->data = data;
        data->error = 0;
        host->cmd->data = data;
        host->sg = data->sg;
        
        for_each_sg(data->sg, sg, data->sg_len, i) 
        {
    		if (sg->offset & 3 || sg->length & 3) 
    		{
    			data->error = -EILSEQ;
    			printk("%s..%d..CMD%d(arg=0x%x), data->blksz=%d, data->blocks=%d   [%s]\n", \
                               __FUNCTION__, __LINE__, host->cmd->opcode,\
                               host->cmd->arg,data->blksz, data->blocks,  host->dma_name);
    			return ;
    		}
	    }

        data->bytes_xfered = 0;
        host->pbuf = (u32*)sg_virt(data->sg);
        host->pio_offset = 0;

        if (data->flags & MMC_DATA_STREAM)
		{
			host->cmdr |= SDMMC_CMD_STRM_MODE;    //set stream mode
		}
		else
		{
		    host->cmdr |= SDMMC_CMD_BLOCK_MODE;   //set block mode
		}
		
        //set the blocks and blocksize
		rk29_sdmmc_write(host->regs, SDMMC_BYTCNT,data->blksz*data->blocks);
		rk29_sdmmc_write(host->regs, SDMMC_BLKSIZ,data->blksz);

        xbwprintk(1, "%s..%d..CMD%d(arg=0x%x), data->blksz=%d, data->blocks=%d   [%s]\n", \
            __FUNCTION__, __LINE__, host->cmd->opcode,host->cmd->arg,data->blksz, data->blocks,  host->dma_name);
            
		if (data->flags & MMC_DATA_WRITE)
		{
		    host->cmdr |= (SDMMC_CMD_DAT_WRITE | SDMMC_CMD_DAT_EXP);
		    
		    xbwprintk(7,"%s..%d..CMD%d(arg=0x%x), data->blksz=%d, data->blocks=%d   [%s]\n", \
                __FUNCTION__, __LINE__, host->cmd->opcode,host->cmd->arg,\
                data->blksz, data->blocks,  host->dma_name);
			ret = rk29_sdmmc_prepare_write_data(host, data);
	    }
	    else
	    {
	        host->cmdr |= (SDMMC_CMD_DAT_READ | SDMMC_CMD_DAT_EXP);
            xbwprintk(7, "%s..%d...   read data  len=%d   [%s]\n", \
					__FUNCTION__, __LINE__, data->blksz*data->blocks, host->dma_name);
	        
			ret = rk29_sdmmc_prepare_read_data(host, data);
	    }

    }
    else
    {
        rk29_sdmmc_write(host->regs, SDMMC_BLKSIZ, 0);
        rk29_sdmmc_write(host->regs, SDMMC_BYTCNT, 0);
    }
}


static int sdmmc_send_cmd_start(struct rk29_sdmmc *host, unsigned int cmd)
{
	int tmo = RK29_SDMMC_SEND_START_TIMEOUT*10;//wait 60ms cycle.

#if defined(CONFIG_ARCH_RK29)		
	rk29_sdmmc_write(host->regs, SDMMC_CMD, SDMMC_CMD_START | cmd);	
#else	
    rk29_sdmmc_write(host->regs, SDMMC_CMD, SDMMC_CMD_USE_HOLD_REG |SDMMC_CMD_START | cmd);
#endif    
	while (--tmo && (rk29_sdmmc_read(host->regs, SDMMC_CMD) & SDMMC_CMD_START))
	{
	    udelay(2);
	}
	
	if(!tmo && test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags)) 
	{
		printk(KERN_WARNING "%s.. %d   set cmd(value=0x%x) register timeout error !   [%s]\n",\
				__FUNCTION__,__LINE__, cmd, host->dma_name);

		host->errorstep = 0x9;
		return SDM_START_CMD_FAIL;
	}

	return SDM_SUCCESS;
}

static int rk29_sdmmc_get_cd(struct mmc_host *mmc)
{
	struct rk29_sdmmc *host = mmc_priv(mmc);
	u32 cdetect=1;
#if defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)	
    int i=0,cdetect1=0, cdetect2=0;
#endif    
    switch(host->host_dev_id)
    {
        case 0:
        {            
         #if defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
            if(host->det_pin.io == INVALID_GPIO)
            	return 1;

            for(i=0;i<5;i++)
            {
                udelay(10);
                cdetect1 = gpio_get_value(host->det_pin.io);  
                udelay(10);
                cdetect2 = gpio_get_value(host->det_pin.io); 
                if(cdetect1 == cdetect2)
                    break;
            }
            cdetect = cdetect2;          
            if(host->det_pin.enable)
                cdetect = cdetect?1:0;
            else
                cdetect = cdetect?0:1;
                
         #else
        	if(host->det_pin.io == INVALID_GPIO)
        		return 1;

        	cdetect = rk29_sdmmc_read(host->regs, SDMMC_CDETECT);

            cdetect = (cdetect & SDMMC_CARD_DETECT_N)?0:1;
         #endif
         
            break;
        }        

        case 1:
        {
            #if defined(CONFIG_USE_SDMMC1_FOR_WIFI_DEVELOP_BOARD)
            cdetect = 1;
            #else
            if(host->det_pin.io == INVALID_GPIO)
        		return 1;
        		
            cdetect = test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags)?1:0;
            #endif
            break;
        }
        case 2:
        default:
            set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
            cdetect = 1;
            break;
    
	}
#if defined(CONFIG_USE_SDMMC0_FOR_WIFI_DEVELOP_BOARD)
    set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
    return 1;
#endif

	 return cdetect;
}


/**
  * Delay loop.
  * Very rough microsecond delay loop  for the system.
  * \param[in] u32 Value in Number of Microseconds.
  * \return Returns Void
 **/
void rk_sdmmc_udelay(u32 value)
{
	u32 counter;
	for (counter = 0; counter < value ; counter++)
		udelay(1);
}

int rk_sdmmc_reset_host(struct rk29_sdmmc *host)
{
    int ret = SDM_SUCCESS;
    int timeout;
    //reset the host cotroller
    rk29_sdmmc_write(host->regs, SDMMC_CTRL, SDMMC_CTRL_RESET);
    udelay(10);
    
    timeout = 1000;
    while((rk29_sdmmc_read(host->regs, SDMMC_CTRL) & SDMMC_CTRL_RESET)&& timeout--)
        udelay(1);
    if(0 == timeout)
    {
        ret = SDM_FALSE;
        printk(KERN_ERR "%d..  reset ctrl_reset fail! [%s]=\n", __LINE__, host->dma_name);
        goto EXIT_RESET;
    }

#if !defined(CONFIG_ARCH_RK29)
    //reset DMA
    rk29_sdmmc_write(host->regs, SDMMC_CTRL, SDMMC_CTRL_DMA_RESET);
    udelay(10);
    
    timeout = 1000;
    while((rk29_sdmmc_read(host->regs, SDMMC_CTRL) & SDMMC_CTRL_DMA_RESET)&& timeout--)
        udelay(1);    
    if(0 == timeout)
    {
        ret = SDM_FALSE;
        printk(KERN_ERR "%d..  reset dma_reset fail! [%s]=\n", __LINE__, host->dma_name);
        goto EXIT_RESET;
    }
#endif

    //reset FIFO
    rk29_sdmmc_write(host->regs, SDMMC_CTRL, SDMMC_CTRL_FIFO_RESET);
    udelay(10);
    
    timeout = 1000;
    while((rk29_sdmmc_read(host->regs, SDMMC_CTRL) & SDMMC_CTRL_FIFO_RESET)&& timeout--)
        udelay(1);       
    if(0 == timeout)
    {
        ret = SDM_FALSE;
        printk(KERN_ERR "%d..  reset fofo_reset fail! [%s]=\n", __LINE__, host->dma_name);
        goto EXIT_RESET;
    }

#if DRIVER_SDMMC_USE_IDMA
    //reset the internal DMA Controller.
    rk29_sdmmc_write(host->regs, SDMMC_BMOD, BMOD_SWR);
    udelay(10);
    
    timeout = 1000;
    while((rk29_sdmmc_read(host->regs, SDMMC_BMOD) & BMOD_SWR)&& timeout--)
        udelay(1);       
    if(0 == timeout)
    {
        ret = SDM_FALSE;
        printk(KERN_ERR "%d..  reset IDMAC fail! [%s]=\n", __LINE__, host->dma_name);
        goto EXIT_RESET;
    }

    // Program the BMOD register for DMA
    rk29_sdmmc_write(host->regs, SDMMC_BMOD, BMOD_DSL_TWO);
#endif

EXIT_RESET:
    return ret;
}

/****************************************************************/
//reset the SDMMC controller of the current host
/****************************************************************/
int rk29_sdmmc_reset_controller(struct rk29_sdmmc *host)
{
    u32  value = 0;
    int  timeOut = 0;

    rk29_sdmmc_write(host->regs, SDMMC_PWREN, POWER_ENABLE);
    
    //Clean the fifo.
    for(timeOut=0; timeOut<FIFO_DEPTH; timeOut++)
    {
        if(rk29_sdmmc_read(host->regs, SDMMC_STATUS) & SDMMC_STAUTS_FIFO_EMPTY)
            break;
            
        value = rk29_sdmmc_read(host->regs, SDMMC_DATA);
    }

    /* reset */
    value = rk_sdmmc_reset_host(host);
    if(value)
        return SDM_FALSE;

     /* FIFO threshold settings  */
  	rk29_sdmmc_write(host->regs, SDMMC_FIFOTH, FIFO_THRESHOLD_WATERMASK);
  	
    rk29_sdmmc_write(host->regs, SDMMC_CTYPE, SDMMC_CTYPE_1BIT);
    rk29_sdmmc_write(host->regs, SDMMC_CLKSRC, CLK_DIV_SRC_0);

    /* config debounce */
    host->bus_hz = clk_get_rate(host->clk);
    
#if 0//Perhaps in some cases, it is necessary to restrict.
    if((host->bus_hz > 52000000) || (host->bus_hz <= 0))
    {
        printk(KERN_WARNING "%s..%s..%d..****Error!!!!!!  Bus clock %d hz is beyond the prescribed limits. [%s]\n",\
            __FILE__, __FUNCTION__,__LINE__,host->bus_hz, host->dma_name);
        
		host->errorstep = 0x0B;            
        return SDM_PARAM_ERROR;            
    }
#endif

    rk29_sdmmc_write(host->regs, SDMMC_DEBNCE, (DEBOUNCE_TIME*host->bus_hz)& SDMMC_DEFAULT_DEBNCE_VAL);

    /* config interrupt */
    rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, 0xFFFFFFFF);

    if(host->use_dma)
    {
        if((RK29_CTRL_SDMMC_ID == host->host_dev_id) || (RK29_SDMMC_EMMC_ID == host->host_dev_id))
        {
		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA);
		}
		else
		{
		    if(0== host->host_dev_id)
		    {
    		    #if !defined(CONFIG_USE_SDMMC0_FOR_WIFI_DEVELOP_BOARD)
    		        #if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
                    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA);
                    #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA | SDMMC_INT_SDIO);
    		        #endif    		        
    		    #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA);
    		    #endif
		    }
		    else if(1== host->host_dev_id)
		    {
		       #if !defined(CONFIG_USE_SDMMC1_FOR_WIFI_DEVELOP_BOARD)
                    #if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
                    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA);
                    #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA | SDMMC_INT_SDIO);
    		        #endif
    		   #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA);
    		   #endif 
		    }
		    else
		    {
		        #if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
                rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA);
                #else
		        rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEDMA | SDMMC_INT_SDIO);
		        #endif
		    }
		}
	}
	else
	{
		if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
        {
		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO);
		}
		else
		{
		    if(0== host->host_dev_id)
		    {
    		    #if !defined(CONFIG_USE_SDMMC0_FOR_WIFI_DEVELOP_BOARD)
                    #if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
                        rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO);
                    #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO | SDMMC_INT_SDIO);
                    #endif
    		    #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO);
    		    #endif
		    }
		    else if(1== host->host_dev_id)
		    {
		        #if !defined(CONFIG_USE_SDMMC1_FOR_WIFI_DEVELOP_BOARD)
    		        #if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
                        rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO);
                    #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO | SDMMC_INT_SDIO);
                    #endif
    		    #else
    		    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO);
    		    #endif
		    }
		    else
		    {
                #if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
                    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO);
                #else
		            rk29_sdmmc_write(host->regs, SDMMC_INTMASK,RK29_SDMMC_INTMASK_USEIO | SDMMC_INT_SDIO);
                #endif
		    }
		}		
    }
    
#if SDMMC_SUPPORT_DDR_MODE
 
    rk29_sdmmc_write(host->regs, SDMMC_UHS_REG, SDMMC_UHS_DDR_MODE);
#endif 

    /*
    **  Some machines may crash because of sdio-interrupt to open too early.
    **  then, in the initialization phase, close the interruption.
    **  noted by xbw,at 2013-07-25
    */
    rk29_sdmmc_write(host->regs, SDMMC_INTMASK,rk29_sdmmc_read(host->regs, SDMMC_INTMASK) & ~SDMMC_INT_SDIO);
    
	rk29_sdmmc_write(host->regs, SDMMC_PWREN, POWER_ENABLE);
	
#if DRIVER_SDMMC_USE_IDMA 
    rk29_sdmmc_write(host->regs, SDMMC_CTRL, SDMMC_CTRL_INT_ENABLE | SDMMC_CTRL_USE_IDMAC); //Set the bit  use_internal_dmac.
#else
   	rk29_sdmmc_write(host->regs, SDMMC_CTRL,SDMMC_CTRL_INT_ENABLE); // enable mci interrupt
#endif
 
    return SDM_SUCCESS;
}




//enable/disnable the clk.
static int rk29_sdmmc_control_clock(struct rk29_sdmmc *host, bool enable)
{
    u32           value = 0;
    int           tmo = 0;
    int           ret = SDM_SUCCESS;

    //wait previous start to clear
    tmo = 1000;
	while (--tmo && (rk29_sdmmc_read(host->regs, SDMMC_CMD) & SDMMC_CMD_START))
	{
		udelay(1);//cpu_relax();
	}
	if(!tmo)
	{
	    host->errorstep = 0x0C;
	    ret = SDM_START_CMD_FAIL;
		goto Error_exit;	
    }

    if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
    { 
        //SDMMC use low-power mode
        #if SDMMC_CLOCK_TEST
        if (enable)
        {
            value = (SDMMC_CLKEN_ENABLE);
        }
        else
        {
            value = (SDMMC_CLKEN_DISABLE);
        }
        
        #else
        {
            if (enable)
            {
                value = (SDMMC_CLKEN_LOW_PWR | SDMMC_CLKEN_ENABLE);
            }
            else
            {
                value = (SDMMC_CLKEN_LOW_PWR | SDMMC_CLKEN_DISABLE);
            }
        }
        #endif
    }
    else
    {
        //SDIO-card use non-low-power mode
        if (enable)
        {
            value = (SDMMC_CLKEN_ENABLE);
        }
        else
        {
            value = (SDMMC_CLKEN_DISABLE);
        }
    }
  
    rk29_sdmmc_write(host->regs, SDMMC_CLKENA, value);

	/* inform CIU */
	ret = sdmmc_send_cmd_start(host, SDMMC_CMD_UPD_CLK | SDMMC_CMD_PRV_DAT_WAIT);
    if(ret != SDM_SUCCESS)
    {
        goto Error_exit;
    }

    return SDM_SUCCESS;

Error_exit:
    printk(KERN_WARNING "\n%s....%d..  control clock fail!!! Enable=%d, ret=0x%x [%s]\n",\
			__FILE__,__LINE__,enable,ret, host->dma_name);

    return ret;
    
}


//adjust the frequency.ie, to set the frequency division control
int rk29_sdmmc_change_clk_div(struct rk29_sdmmc *host, u32 freqHz)
{
    u32 div;
    u32 tmo;
    int ret = SDM_SUCCESS;

    if(0 == freqHz)
    {
        ret =  SDM_PARAM_ERROR;
        goto  SetFreq_error;
    }

    ret = rk29_sdmmc_control_clock(host, FALSE);
    if (ret != SDM_SUCCESS)
    {
        goto SetFreq_error;
    }

     
    host->bus_hz = clk_get_rate(host->clk);
    if(host->bus_hz <= 0)
    {
        printk(KERN_WARNING "%s..%s..%d..****Error!!!!!!  Bus clock %d hz is beyond the prescribed limits [%s]\n",\
            __FILE__, __FUNCTION__,__LINE__,host->bus_hz, host->dma_name);
            
        host->errorstep = 0x0D;    
        ret = SDM_PARAM_ERROR;   
        goto SetFreq_error;
    }

    //calculate the divider
    div = host->bus_hz/freqHz + ((( host->bus_hz%freqHz ) > 0) ? 1:0 );
    if( (div & 0x01) && (1 != div) )
    {
        //It is sure that the value of div is even. 
        ++div;
    }

    if(div > 1)
    {
        host->clock = host->bus_hz/div;
    }
    else
    {
        host->clock = host->bus_hz;
    }
    div = (div >> 1);

    //wait previous start to clear
    tmo = 1000;
	while (--tmo && (rk29_sdmmc_read(host->regs, SDMMC_CMD) & SDMMC_CMD_START))
	{
		udelay(1);//cpu_relax();
	}
	if(!tmo)
	{
	    host->errorstep = 0x0E; 
	    ret = SDM_START_CMD_FAIL;
		goto SetFreq_error;
    }
           
    /* set clock to desired speed */
    rk29_sdmmc_write(host->regs, SDMMC_CLKDIV, div);

    /* inform CIU */
    ret = sdmmc_send_cmd_start(host, SDMMC_CMD_UPD_CLK | SDMMC_CMD_PRV_DAT_WAIT);
    if(ret != SDM_SUCCESS)
    {
        host->errorstep = 0x0E1; 
        goto SetFreq_error;
    }
    
    if(host->old_div != div)
    {
        printk(KERN_INFO "%s..%d..  newDiv=%u, newCLK=%uKhz [%s]\n", \
            __FUNCTION__, __LINE__,div, host->clock/1000, host->dma_name);
    }

    ret = rk29_sdmmc_control_clock(host, TRUE);
    if(ret != SDM_SUCCESS)
    {
        goto SetFreq_error;
    }
    host->old_div = div;

    return SDM_SUCCESS;
    
SetFreq_error:

    printk(KERN_WARNING "%s..%d..  change division fail, errorStep=0x%x,ret=%d  [%s]\n",\
        __FILE__, __LINE__,host->errorstep,ret, host->dma_name);
        
    return ret;
    
}

int rk29_sdmmc_hw_init(void *data)
{
    struct rk29_sdmmc *host = (struct rk29_sdmmc *)data;
    struct rk29_sdmmc_platform_data *pdata = host->pdev->dev.platform_data;

    //set the iomux
    host->ctype = SDMMC_CTYPE_1BIT;
    host->set_iomux(host->host_dev_id, host->ctype);
    
    if( pdata && pdata->sd_vcc_reset )
    {
	    int cdetect = gpio_get_value(host->det_pin.io) ;
	    if(host->det_pin.enable)
	    {
	        cdetect = cdetect?1:0;
        }
        else
        {
                cdetect = cdetect?0:1;
        }
       
	    if( cdetect )
	    {
		    pdata->sd_vcc_reset();
	    }
    }
  
    /* reset controller */
    rk29_sdmmc_reset_controller(host);

    rk29_sdmmc_change_clk_div(host, FOD_FREQ);
    
    return SDM_SUCCESS;    
}



int rk29_sdmmc_set_buswidth(struct rk29_sdmmc *host)
{
    //int ret;
    if(host->ctype == rk29_sdmmc_read(host->regs, SDMMC_CTYPE))
        return SDM_SUCCESS;
        
    switch (host->ctype)
    {
        case SDMMC_CTYPE_1BIT:
        case SDMMC_CTYPE_4BIT:
            break;
        case SDMMC_CTYPE_8BIT:
            break;//support 8 bit width in some case.
        default:
            return SDM_PARAM_ERROR;
    }

    host->set_iomux(host->host_dev_id, host->ctype);

    /* Set the current  bus width */
	rk29_sdmmc_write(host->regs, SDMMC_CTYPE, host->ctype);
    xbwprintk(5,"%d..%s: ===host-ctype=0x%x, set buswidth=%d =========[%s]\n", __LINE__, __FUNCTION__,host->ctype,\
            (SDMMC_CTYPE_1BIT==host->ctype)?1:((SDMMC_CTYPE_4BIT==host->ctype)?4:8), host->dma_name);

    return SDM_SUCCESS;
}


static void rk29_sdmmc_dealwith_timeout(struct rk29_sdmmc *host)
{ 
    if(0 == host->mmc->doneflag)
        return; //not to generate error flag if the command has been over.
        
    switch(host->state)
    {
        case STATE_IDLE:
            break;    	    
    	case STATE_SENDING_CMD:
    	    host->cmd_status |= SDMMC_INT_RTO;
    	    host->cmd->error = -ETIME;
    	    rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,(SDMMC_INT_CMD_DONE | SDMMC_INT_RTO));  //  clear interrupt
    	    rk29_sdmmc_set_pending(host, EVENT_CMD_COMPLETE);
    	    tasklet_schedule(&host->tasklet);
    	    break;
    	 case STATE_DATA_BUSY:
    	    host->data_status |= (SDMMC_INT_DCRC|SDMMC_INT_EBE);
    	    host->cmd->data->error = -EILSEQ;
    	    rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_DTO);  // clear interrupt
    	    rk29_sdmmc_set_pending(host, EVENT_DATA_COMPLETE);
#if SDMMC_USE_INT_UNBUSY    	    
    	    rk29_sdmmc_set_pending(host, EVENT_DATA_UNBUSY);
#endif    	    
    	    tasklet_schedule(&host->tasklet);
    	    break;
#if SDMMC_USE_INT_UNBUSY
    	 case STATE_DATA_UNBUSY:
    	    host->data_status |= (SDMMC_INT_DCRC|SDMMC_INT_EBE);
    	    host->cmd->data->error = -EILSEQ;
    	    rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_UNBUSY);  // clear interrupt
    	    rk29_sdmmc_set_pending(host, EVENT_DATA_UNBUSY);
    	    tasklet_schedule(&host->tasklet);
    	    break;
#endif    	    
    	 case STATE_SENDING_STOP: 
    	    host->cmd_status |= SDMMC_INT_RTO;
    	    host->cmd->error = -ETIME;
    	    rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,(SDMMC_INT_CMD_DONE | SDMMC_INT_RTO));  //  clear interrupt
    	    rk29_sdmmc_set_pending(host, EVENT_CMD_COMPLETE);
    	    tasklet_schedule(&host->tasklet);
    	    break;
        case STATE_DATA_END:
            break;
        default:
            break; 
    }
}


static void rk29_sdmmc_INT_CMD_DONE_timeout(unsigned long host_data)
{
	struct rk29_sdmmc *host = (struct rk29_sdmmc *) host_data;
    
	rk29_sdmmc_enable_irq(host, false);
	
	if(STATE_SENDING_CMD == host->state)
	{
	    if((0==host->cmd->retries)&&(12 != host->cmd->opcode))
	    {
    	    printk(KERN_WARNING "%d... cmd=%d(arg=0x%x), INT_CMD_DONE timeout, errorStep=0x%x, host->state=%x [%s]\n",\
                 __LINE__,host->cmd->opcode, host->cmd->arg,host->errorstep,host->state,host->dma_name);

            if(++host->timeout_times >= 3)
            {
                printk(KERN_WARNING "I am very sorry to tell you,in order to make the machine correctly,you must remove-insert card again.");                                  
                host->mmc->re_initialized_flags = 0;
                host->timeout_times = 0;
            }
        }
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, 0xFFFFFFFE); // clear INT,except for SDMMC_INT_CD
        rk29_sdmmc_dealwith_timeout(host);  
	}
	
    rk29_sdmmc_enable_irq(host, true);
}


static void rk29_sdmmc_INT_DTO_timeout(unsigned long host_data)
{
	struct rk29_sdmmc *host = (struct rk29_sdmmc *) host_data;
  
	rk29_sdmmc_enable_irq(host, false);

#if SDMMC_USE_INT_UNBUSY
    if( (host->cmdr & SDMMC_CMD_DAT_EXP) &&((STATE_DATA_BUSY == host->state)||(STATE_DATA_UNBUSY == host->state) ))
#else
    if( (host->cmdr & SDMMC_CMD_DAT_EXP) && (STATE_DATA_BUSY == host->state))
#endif	
	{
	    if(0==host->cmd->retries)
	    {
    	   printk(KERN_WARNING "%s..%d...cmd=%d DTO_timeout,cmdr=0x%x, errorStep=0x%x, Hoststate=%x [%s]\n", \
    	        __FUNCTION__, __LINE__,host->cmd->opcode,host->cmdr ,host->errorstep,host->state,host->dma_name);

    	    if(++host->timeout_times >= 3)
            {
                printk(KERN_WARNING "I am very sorry to tell you,in order to make the machine correctly,you must remove-insert card again.");
                host->mmc->re_initialized_flags = 0;
                host->timeout_times = 0;
            }     
	    }
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, 0xFFFFFFFE); // clear INT,except for SDMMC_INT_CD
	    rk29_sdmmc_dealwith_timeout(host);  
	}
	rk29_sdmmc_enable_irq(host, true);
 
}


//to excute a  request 
static int rk29_sdmmc_start_request(struct mmc_host *mmc )
{
    struct rk29_sdmmc *host = mmc_priv(mmc);
	struct mmc_request	*mrq;
	struct mmc_command	*cmd;
	
	u32		cmdr, ret;
	unsigned long iflags;

	spin_lock_irqsave(&host->lock, iflags);
	
	mrq = host->new_mrq;
	cmd = mrq->cmd;
	cmd->error = 0;
	
	cmdr = rk29_sdmmc_prepare_command(cmd);
	ret = SDM_SUCCESS;
	

	/*clean FIFO if it is a new request*/
    if(!(cmdr & SDMMC_CMD_STOP))//test emmc
    {
        ret = rk29_sdmmc_reset_fifo(host);
        if(SDM_SUCCESS != ret)
        {
        		host->mrq = host->new_mrq;///
            cmd->error = -ENOMEDIUM;
            host->errorstep = 0x0F; 
            ret = SDM_FALSE;
            goto start_request_Err; 
        }
    }

    //check data-busy if the current command has the bit13 in command register.
    if( cmdr & SDMMC_CMD_PRV_DAT_WAIT )
    {        
        if(SDM_SUCCESS != rk29_sdmmc_wait_unbusy(host))   //if(rk29_sdmmc_read(host->regs, SDMMC_STATUS) & SDMMC_STAUTS_DATA_BUSY)
        {
        	host->mrq = host->new_mrq;///
            cmd->error = -ETIMEDOUT;
            ret = SDM_BUSY_TIMEOUT;
            host->errorstep = 0x10;
            if(0 == cmd->retries)
            {
                printk(KERN_WARNING "%s..Error happen in CMD_PRV_DAT_WAIT. STATUS-reg=0x%x [%s]\n", \
                    __FUNCTION__, rk29_sdmmc_read(host->regs, SDMMC_STATUS),host->dma_name);
            }
            rk29_sdmmc_clear_fifo(host);
            goto start_request_Err; 
        }
    }
    
    host->state = STATE_SENDING_CMD;
    host->mrq = host->new_mrq;
	mrq = host->mrq;
	cmd = mrq->cmd;
	cmd->error = 0;
	cmd->data = NULL;

    host->cmdr = cmdr;
    host->cmd = cmd;
	host->data_status = 0;
	host->data = NULL;
	
	host->errorstep = 0;
	host->dodma = 0;



    //setting for the data
	rk29_sdmmc_submit_data(host, mrq->data);
    host->errorstep = 0xff;

	xbwprintk(7, "%s..%d...    CMD%d  begin to call rk29_sdmmc_start_command(). [%s]\n", \
			__FUNCTION__, __LINE__ , cmd->opcode,host->dma_name);

	if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
	{
	    mod_timer(&host->request_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_SEND_START_TIMEOUT+700));
	}
	else if(RK29_SDMMC_EMMC_ID == host->host_dev_id)
	{
	    if((5==cmd->opcode)||(8==cmd->opcode)||(55==cmd->opcode))
	    {
	        mod_timer(&host->request_timer, jiffies + msecs_to_jiffies(150));
	    }
	    else if((0==cmd->opcode)|| (1==cmd->opcode))
	    {
	         mod_timer(&host->request_timer, jiffies + msecs_to_jiffies(15));
	    }
	    else
	    {
	        mod_timer(&host->request_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_SEND_START_TIMEOUT+500));
	    }
	}
	else
	{
	    mod_timer(&host->request_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_SEND_START_TIMEOUT+500));
	}

    
	ret = rk29_sdmmc_start_command(host, cmd, host->cmdr);
	if(SDM_SUCCESS != ret)
	{
        cmd->error = -ETIMEDOUT;
        if(0==cmd->retries)
        {
            printk(KERN_WARNING "%s..%d...   start_command(CMD%d, arg=%x, retries=%d)  fail! ret=%d . [%s]\n",\
                __FUNCTION__, __LINE__ , cmd->opcode,cmd->arg, cmd->retries,ret, host->dma_name);
        }
        host->errorstep = 0x11; 
        del_timer_sync(&host->request_timer);
        
        goto start_request_Err; 
	}
	host->errorstep = 0xfd;
    
#if DRIVER_SDMMC_USE_IDMA 
    /*  Check if it is a data command. If yes, we need to handle only IDMAC interrupts
    **  So we disable the Slave mode interrupts and enable DMA mode interrupts.
    **  CTRL and BMOD registers are set up for DMA mode of operation
    */
    if(mrq->data)
    {
        rk29_sdmmc_write(host->regs, SDMMC_INTMASK, 0x00000000);//Mask all slave interrupts
        rk29_sdmmc_write(host->regs, SDMMC_IDINTEN, IDMAC_EN_INT_ALL);
        rk29_sdmmc_write(host->regs, SDMMC_CTRL, SDMMC_CTRL_USE_IDMAC);
        rk29_sdmmc_write(host->regs, SDMMC_BMOD, BMOD_DE);            
        rk29_sdmmc_write(host->regs, SDMMC_BMOD, BMOD_DSL_TWO);// Program the BMOD register for DMA
        rk29_sdmmc_write(host->regs, SDMMC_FIFOTH, FIFO_THRESHOLD_WATERMASK);
    }
#endif    
   
    xbwprintk(7, "%s..%d...  CMD=%d, wait for INT_CMD_DONE, ret=%d , \n  \
        host->state=0x%x, cmdINT=0x%x \n    host->pendingEvent=0x%lu, host->completeEvents=0x%lu [%s]\n\n",\
        __FUNCTION__, __LINE__, host->cmd->opcode,ret, \
        host->state,host->cmd_status, host->pending_events,host->completed_events,host->dma_name);

    spin_unlock_irqrestore(&host->lock, iflags);
    
	return SDM_SUCCESS;
	
start_request_Err:
    rk29_sdmmc_start_error(host);

    if(0 == cmd->retries) 
    {
        printk(KERN_WARNING "%s: CMD%d(arg=%x)  fail to start request.  err=%d, Errorstep=0x%x [%s]\n",\
            __FUNCTION__,  cmd->opcode, cmd->arg,ret,host->errorstep,host->dma_name);
    }

    host->state = STATE_IDLE;  //modifyed by xbw  at 2011-08-15
    
    if(host->mrq && host->mmc->doneflag && host->complete_done)
    {
        host->mmc->doneflag = 0;
        host->complete_done = 0;
        spin_unlock_irqrestore(&host->lock, iflags);
        
        mmc_request_done(host->mmc, host->mrq);
    }
    else
    {
        spin_unlock_irqrestore(&host->lock, iflags);        
    }
    
    return ret; 
	
}

 
static void rk29_sdmmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
    unsigned long iflags;
    int i,ret;
	struct rk29_sdmmc *host = mmc_priv(mmc); 
	
    spin_lock_irqsave(&host->lock, iflags);
    
	#if 0
	//set 1 to close the controller for Debug.
	if(RK29_CTRL_SDIO1_ID==host->host_dev_id)
	{
	    mrq->cmd->error = -ENOMEDIUM;
	    printk(KERN_WARNING "%s..%d..  ==== The %s had been closed by myself for the experiment. [%s]\n",\
				__FUNCTION__, __LINE__, host->dma_name, host->dma_name);

        host->state = STATE_IDLE;
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, 0xFFFFFFFF);
        spin_unlock_irqrestore(&host->lock, iflags);
	    mmc_request_done(mmc, mrq);
		return;
	}
	#endif

    i=0;
	while(++i>2)
    {
        ret = rk29_sdmmc_wait_unbusy(host);
        if(SDM_SUCCESS == ret)
            break;
    }

    xbwprintk(6, "\n%s..%d..New cmd=%2d(arg=0x%8x)=== cardPresent=0x%lu, state=0x%x [%s]\n", \
        __FUNCTION__, __LINE__,mrq->cmd->opcode, mrq->cmd->arg,host->flags,host->state, host->dma_name);

    if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
    {
        if(!rk29_sdmmc_get_cd(mmc) || ((0==mmc->re_initialized_flags)&&(MMC_GO_IDLE_STATE != mrq->cmd->opcode)))
        {
    		mrq->cmd->error = -ENOMEDIUM;

    		if((RK29_CTRL_SDMMC_ID == host->host_dev_id)&&(0==mrq->cmd->retries))
    		{
    	    	if(host->old_cmd != mrq->cmd->opcode)
    	    	{
    	    	    if( ((17==host->old_cmd)&&(18==mrq->cmd->opcode)) || ((18==host->old_cmd)&&(17==mrq->cmd->opcode)) ||\
    	    	         ((24==host->old_cmd)&&(25==mrq->cmd->opcode)) || ((25==host->old_cmd)&&(24==mrq->cmd->opcode)))
    	    	    {
    	    	        host->old_cmd = mrq->cmd->opcode;
    	    	        if(host->error_times++ % (RK29_ERROR_PRINTK_INTERVAL*100) ==0)
            	        {
                    		printk(KERN_INFO "%s: Refuse to run CMD%2d(arg=0x%8x) due to the removal of card.  1==[%s]==\n", \
                    		    __FUNCTION__, mrq->cmd->opcode, mrq->cmd->arg, host->dma_name);
                		}
    	    	    }
    	    	    else
    	    	    {
            	        host->old_cmd = mrq->cmd->opcode;
            	        host->error_times = 0;
            	        printk(KERN_INFO "%s: Refuse to run CMD%2d(arg=0x%8x) due to the removal of card.  2==[%s]==\n", \
                    		    __FUNCTION__, mrq->cmd->opcode, mrq->cmd->arg, host->dma_name); 
                	}
        	    }
        	    else
        	    {
        	        if(host->error_times++ % (RK29_ERROR_PRINTK_INTERVAL*100) ==0)
        	        {
                		printk(KERN_INFO "%s: Refuse to run CMD%2d(arg=0x%8x) due to the removal of card.  3==[%s]==\n", \
                    		    __FUNCTION__, mrq->cmd->opcode, mrq->cmd->arg, host->dma_name);
            		}
            		host->old_cmd = mrq->cmd->opcode;
        	    }	    
    		}
            host->state = STATE_IDLE;
            spin_unlock_irqrestore(&host->lock, iflags);
            
    		mmc_request_done(mmc, mrq);
    		return;
    	}
    	else
    	{
    		if(host->old_cmd != mrq->cmd->opcode)
    		{	
    			host->old_cmd = mrq->cmd->opcode;
				host->error_times = 0;
			}			
    	}
	}
	else
	{
        host->old_cmd = mrq->cmd->opcode;
        host->error_times = 0;

        if(!test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags))
		{
		    host->state = STATE_IDLE;
		    mrq->cmd->error = -ENOMEDIUM;
            spin_unlock_irqrestore(&host->lock, iflags);
            
    		mmc_request_done(mmc, mrq);
    		return;
		}

	}
 
    host->new_mrq = mrq;        

	spin_unlock_irqrestore(&host->lock, iflags);
	        
    rk29_sdmmc_start_request(mmc);
}


extern void rk29_sdmmc_gpio_open(int device_id, int on);
static void rk29_sdmmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
    int timeout = 250;
    unsigned int value;
	struct rk29_sdmmc *host = mmc_priv(mmc);

    rk29_sdmmc_enable_irq(host, false);

    //if(host->bus_mode != ios->power_mode)
    {
        switch (ios->power_mode) 
        {
            case MMC_POWER_UP:
            	rk29_sdmmc_write(host->regs, SDMMC_PWREN, POWER_ENABLE);
            	            	
            	//reset the controller if it is SDMMC0
            	if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
            	{
            	    xbwprintk(7, "%s..%d..POWER_UP, call reset_controller, initialized_flags=%d [%s]\n",\
            	        __FUNCTION__, __LINE__, host->mmc->re_initialized_flags,host->dma_name);
            	        
                    //power-on; 
                    gpio_direction_output(host->gpio_power_en, host->gpio_power_en_level);
                    
            	    mdelay(5);
            	        
            	    rk29_sdmmc_hw_init(host);
            	}
            	if(RK29_SDMMC_EMMC_ID == host->host_dev_id) //emmc
            	{
            	    xbwprintk(7, "%s..%d..POWER_UP, call reset_controller, initialized_flags=%d [%s]\n",\
            	        __FUNCTION__, __LINE__, host->mmc->re_initialized_flags,host->dma_name);
            	         
            	    mdelay(5);
            	        
            	    rk29_sdmmc_hw_init(host);
            	}
               	
               	
            	break;
            case MMC_POWER_OFF:
              
                if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
                {
                    mdelay(5);
                	rk29_sdmmc_control_clock(host, FALSE);
                	rk29_sdmmc_write(host->regs, SDMMC_PWREN, POWER_DISABLE);
                	mdelay(5);                
                	if(5 == host->bus_mode)
                	{
                        mdelay(5);
                        xbwprintk(7, "%s..%d..Fisrt powerOFF, call reset_controller [%s]\n", \
                            __FUNCTION__, __LINE__,host->dma_name);
                            
                        rk29_sdmmc_reset_controller(host);
                	}

                  
			        rk29_sdmmc_gpio_open(0, 0);			        
			        //power-off 
                    gpio_direction_output(host->gpio_power_en, !(host->gpio_power_en_level));  
			        goto out;
            	}

                if(RK29_SDMMC_EMMC_ID == host->host_dev_id) //emmc
            	    rk29_sdmmc_write(host->regs, SDMMC_PWREN, POWER_DISABLE);

            	break;        	
            default:
            	break;
    	}
    	
    	host->bus_mode = ios->power_mode;
    	
	}

    if(test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags) || (RK29_CTRL_SDMMC_ID == host->host_dev_id))
    {
        /*
         * Waiting SDIO controller to be IDLE.
        */
        while (timeout-- > 0)
    	{
    		value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
    		if ((value & SDMMC_STAUTS_DATA_BUSY) == 0 &&(value & SDMMC_CMD_FSM_MASK) == SDMMC_CMD_FSM_IDLE)
    		{
    			break;
    		}
    		
    		mdelay(1);
    	}
    	if (timeout <= 0)
    	{
    		printk(KERN_WARNING "%s..%d...Waiting for SDMMC%d controller to be IDLE timeout.[%s]\n", \
    				__FUNCTION__, __LINE__, host->host_dev_id, host->dma_name);

    		goto out;
    	}
	}


    if((!(test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags))|| !rk29_sdmmc_get_cd(host->mmc))
        &&(RK29_CTRL_SDIO1_ID != host->host_dev_id))
        goto out; //exit the set_ios directly if the SDIO is not present. 

#if SDMMC_SUPPORT_DDR_MODE
    value = rk29_sdmmc_read(host->regs, SDMMC_UHS_REG);

    /* DDR mode set */
    if(ios->timing == MMC_TIMING_UHS_DDR50)
    {
        if(SDMMC_UHS_DDR_MODE != (value&SDMMC_UHS_DDR_MODE)){
            xbwprintk(7, "%d..%s: set DDR mode. value=0x%x.[%s]\n", \
                __LINE__, __FUNCTION__, value, host->dma_name);
                
            value |= SDMMC_UHS_DDR_MODE;
        }
    }
    else
        value &= (~SDMMC_UHS_DDR_MODE);
        
    rk29_sdmmc_write(host->regs, SDMMC_UHS_REG, value);
#endif
        
	if(host->ctype != ios->bus_width)
	{
    	switch (ios->bus_width) 
    	{
            case MMC_BUS_WIDTH_1:
                host->ctype = SDMMC_CTYPE_1BIT;
                break;
            case MMC_BUS_WIDTH_4:
                host->ctype = SDMMC_CTYPE_4BIT;
                break;
            case MMC_BUS_WIDTH_8:
                host->ctype = SDMMC_CTYPE_8BIT;
                break;
            default:
                host->ctype = 0;
                break;
    	}

	    rk29_sdmmc_set_buswidth(host);
	    
	}
	
	if (ios->clock && (ios->clock != host->clock)) 
	{	
		/*
		 * Use mirror of ios->clock to prevent race with mmc
		 * core ios update when finding the minimum.
		 */
		//host->clock = ios->clock;	
		rk29_sdmmc_change_clk_div(host, ios->clock);
	}
out:	   
    rk29_sdmmc_enable_irq(host, true);
    
}

static int rk29_sdmmc_get_ro(struct mmc_host *mmc)
{
    struct rk29_sdmmc *host = mmc_priv(mmc);
    int ret=0;

    switch(host->host_dev_id)
    {
        case 0:
        {
            #if defined(CONFIG_SDMMC0_RK29_WRITE_PROTECT) || defined(CONFIG_SDMMC1_RK29_WRITE_PROTECT)            
        	if(INVALID_GPIO == host->write_protect)
        	    ret = 0;//no write-protect
        	else
                ret = (host->protect_level == gpio_get_value(host->write_protect)?1:0;
           
                xbwprintk(7,"%s..%d.. write_prt_pin=%d, get_ro=%d. [%s]\n",\
                    __FUNCTION__, __LINE__,host->write_protect, ret, host->dma_name);
                            
            #else
        	u32 wrtprt = rk29_sdmmc_read(host->regs, SDMMC_WRTPRT);
        	
        	ret = (wrtprt & SDMMC_WRITE_PROTECT)?1:0;
            #endif

            break;
        }
        
        case 1:
        case 2:
            ret = 0;//no write-protect
            break;
        
        default:
            ret = 0;
        break;   
    }

    return ret;

}

#if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
static irqreturn_t rk29_sdmmc_sdio_irq_cb(int irq, void *dev_id)
{
	struct rk29_sdmmc *host = dev_id;
	
    if(host && host->mmc)
        mmc_signal_sdio_irq(host->mmc);

	return IRQ_HANDLED;
}
#endif


static void rk29_sdmmc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
#if !defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)	
	u32 intmask;	
	unsigned long flags;
#endif	
	struct rk29_sdmmc *host = mmc_priv(mmc);
		
#if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)	
    if(enable)
    {
        enable_irq(host->sdio_irq);
        
        #if !defined(CONFIG_MTK_COMBO_DRIVER_VERSION_JB2)
        enable_irq_wake(host->sdio_irq);
        #endif
    }
    else
    {
        disable_irq_nosync(host->sdio_irq);
        
        #if !defined(CONFIG_MTK_COMBO_DRIVER_VERSION_JB2)
        disable_irq_wake(host->sdio_irq);
        #endif
    }

#else
    spin_lock_irqsave(&host->lock, flags);

	intmask = rk29_sdmmc_read(host->regs, SDMMC_INTMASK);	
	if(enable)
		rk29_sdmmc_write(host->regs, SDMMC_INTMASK, intmask | SDMMC_INT_SDIO);
	else
		rk29_sdmmc_write(host->regs, SDMMC_INTMASK, intmask & ~SDMMC_INT_SDIO);

	spin_unlock_irqrestore(&host->lock, flags);	
#endif		
    
    
}

#ifdef CONFIG_ESP8089
void sdmmc_ack_interrupt(struct mmc_host *mmc)
{
       struct rk29_sdmmc *host;
       host = mmc_priv(mmc);
       rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, SDMMC_INT_SDIO);
}
EXPORT_SYMBOL_GPL(sdmmc_ack_interrupt);
#endif


static void  rk29_sdmmc_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
        card->quirks = MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;

}

static int rk29_sdmmc_clear_fifo(struct rk29_sdmmc *host)
{
    unsigned int timeout, value;
    int ret = SDM_SUCCESS;

    if((RK29_CTRL_SDMMC_ID == host->host_dev_id) ||(RK29_SDMMC_EMMC_ID == host->host_dev_id))
    {
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, 0xFFFFFFFF);
    }

    rk29_sdmmc_stop_dma(host);
    rk29_sdmmc_control_host_dma(host, FALSE);
    host->dodma = 0;
   
    //Clean the fifo.
    for(timeout=0; timeout<FIFO_DEPTH; timeout++)
    {
        if(rk29_sdmmc_read(host->regs, SDMMC_STATUS) & SDMMC_STAUTS_FIFO_EMPTY)
            break;
            
        value = rk29_sdmmc_read(host->regs, SDMMC_DATA);
    }

     /* reset */
    timeout = 1000;
    value = rk29_sdmmc_read(host->regs, SDMMC_CTRL);
    value |= (SDMMC_CTRL_RESET | SDMMC_CTRL_FIFO_RESET | SDMMC_CTRL_DMA_RESET);
    rk29_sdmmc_write(host->regs, SDMMC_CTRL, value);

    value = rk29_sdmmc_read(host->regs, SDMMC_CTRL);
    
    while( (value & (SDMMC_CTRL_FIFO_RESET | SDMMC_CTRL_RESET | SDMMC_CTRL_DMA_RESET)) && (timeout > 0))
    {
        udelay(1);
        timeout--;
        value = rk29_sdmmc_read(host->regs, SDMMC_CTRL);
    }

    if (timeout == 0)
    {
        host->errorstep = 0x0A;
        ret = SDM_WAIT_FOR_FIFORESET_TIMEOUT;
    }

    return ret;
}

static int rk_sdmmc_signal_voltage_switch(struct mmc_host *mmc,
	struct mmc_ios *ios)
{
	struct rk29_sdmmc *host;
	unsigned int value,uhs_reg;

	host = mmc_priv(mmc);

	/*
	 * We first check whether the request is to set signalling voltage
	 * to 3.3V. If so, we change the voltage to 3.3V and return quickly.
	 */
	uhs_reg = rk29_sdmmc_read(host->regs, SDMMC_UHS_REG); 
    if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) 
    {
    	//set 3.3v
    	#if SWITCH_VOLTAGE_18_33
    	if(NULL != SWITCH_VOLTAGE_18_33)
    	    gpio_direction_output(SWITCH_VOLTAGE_18_33, SWITCH_VOLTAGE_ENABLE_VALUE_33);
        #endif
    	//set High-power mode
    	value = rk29_sdmmc_read(host->regs, SDMMC_CLKENA);
    	rk29_sdmmc_write(host->regs,SDMMC_CLKENA , value& ~SDMMC_CLKEN_LOW_PWR);

    	//SDMMC_UHS_REG
    	rk29_sdmmc_write(host->regs,SDMMC_UHS_REG , uhs_reg & ~SDMMC_UHS_VOLT_REG_18);

        /* Wait for 5ms */
		usleep_range(5000, 5500);

		/* 3.3V regulator output should be stable within 5 ms */
		uhs_reg = rk29_sdmmc_read(host->regs, SDMMC_UHS_REG);
        if( !(uhs_reg & SDMMC_UHS_VOLT_REG_18))
            return 0;
        else
        {
            printk(KERN_INFO  ": Switching to 3.3V "
				"signalling voltage failed.  [%s]\n", host->dma_name);
			return -EIO;
        }   

    }
    else if (!(uhs_reg & SDMMC_UHS_VOLT_REG_18) && (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180))
    {
        /* Stop SDCLK */
        rk29_sdmmc_control_clock(host, FALSE);

		/* Check whether DAT[3:0] is 0000 */
		value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
		if ((value & SDMMC_STAUTS_DATA_BUSY) == 0)
		{
			/*
			 * Enable 1.8V Signal Enable in the Host register 
			 */
			rk29_sdmmc_write(host->regs,SDMMC_UHS_REG , uhs_reg |SDMMC_UHS_VOLT_REG_18);

			/* Wait for 5ms */
			usleep_range(5000, 5500);

			uhs_reg = rk29_sdmmc_read(host->regs, SDMMC_UHS_REG);
            if( uhs_reg & SDMMC_UHS_VOLT_REG_18)
            {

                /* Provide SDCLK again and wait for 1ms*/
				rk29_sdmmc_control_clock(host, TRUE);
				usleep_range(1000, 1500);

				/*
				 * If DAT[3:0] level is 1111b, then the card
				 * was successfully switched to 1.8V signaling.
				 */
				value = rk29_sdmmc_read(host->regs, SDMMC_STATUS);
		        if ((value & SDMMC_STAUTS_DATA_BUSY) == 0)
		            return 0;
            }
		}

    	/*
		 * If we are here, that means the switch to 1.8V signaling
		 * failed. We power cycle the card, and retry initialization
		 * sequence by setting S18R to 0.
		 */
		#if SWITCH_VOLTAGE_18_33
		if(NULL != SWITCH_VOLTAGE_18_33)
            gpio_direction_output(SWITCH_VOLTAGE_18_33, !(SWITCH_VOLTAGE_ENABLE_VALUE_33));
        #endif
        
		/* Wait for 1ms as per the spec */
		usleep_range(1000, 1500);

        #if SWITCH_VOLTAGE_18_33
        if(NULL != SWITCH_VOLTAGE_18_33)
		    gpio_direction_output(SWITCH_VOLTAGE_18_33, SWITCH_VOLTAGE_ENABLE_VALUE_33);
		#endif    

		printk(KERN_INFO ": Switching to 1.8V signalling "
			"voltage failed, retrying with S18R set to 0. [%s]\n", host->dma_name);
		return -EAGAIN;

    }
    else
    {
        /* No signal voltage switch required */
		return 0;
    }
}


static const struct mmc_host_ops rk29_sdmmc_ops[] = {
	{
		.request	= rk29_sdmmc_request,
		.set_ios	= rk29_sdmmc_set_ios,
		.get_ro		= rk29_sdmmc_get_ro,
		.get_cd		= rk29_sdmmc_get_cd,
	    .start_signal_voltage_switch	= rk_sdmmc_signal_voltage_switch,
	},
	{
		.request	= rk29_sdmmc_request,
		.set_ios	= rk29_sdmmc_set_ios,
		.get_ro		= rk29_sdmmc_get_ro,
		.get_cd		= rk29_sdmmc_get_cd,
	#if !defined(CONFIG_MTK_COMBO_MT66XX)
		.enable_sdio_irq = rk29_sdmmc_enable_sdio_irq,
	#endif
		.init_card       = rk29_sdmmc_init_card,
	},

    {
		.request	= rk29_sdmmc_request,
		.set_ios	= rk29_sdmmc_set_ios,
		.get_ro		= rk29_sdmmc_get_ro,
		.get_cd		= rk29_sdmmc_get_cd,
	},
};

static void rk29_sdmmc_request_end(struct rk29_sdmmc *host, struct mmc_command *cmd)
{
	u32 status = host->data_status;
	int output=SDM_SUCCESS;

	xbwprintk(7, "%s..%d...  cmd=%d, host->state=0x%x,pendingEvent=0x%lu, completeEvents=0x%lu [%s]\n",\
        __FUNCTION__, __LINE__,cmd->opcode,host->state, host->pending_events,host->completed_events,host->dma_name);

    del_timer_sync(&host->DTO_timer);

    if((RK29_CTRL_SDMMC_ID == host->host_dev_id) ||(RK29_SDMMC_EMMC_ID == host->host_dev_id))
    {
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, 0xFFFFFFFF); //added by xbw at 2011-08-15
    }

    //stop DMA
    if(host->dodma)
    {
        rk29_sdmmc_stop_dma(host);
        rk29_sdmmc_control_host_dma(host, FALSE);
        host->dodma = 0;
    }

    if(cmd->error)
    {
        
	    xbwprintk(7, "%s..%d...  cmd=%d, host->state=0x%x,pendingEvent=0x%lu, completeEvents=0x%lu [%s]\n",\
            __FUNCTION__, __LINE__,cmd->opcode,host->state, host->pending_events,host->completed_events,host->dma_name);
        goto exit;//It need not to wait-for-busy if the CMD-ERROR happen.
    }
    host->errorstep = 0xf7;
    if(cmd->data)
    {        
        if(host->cmdr & SDMMC_CMD_DAT_WRITE)
        {
            if( (MMC_BUS_TEST_W != cmd->opcode) && (status & (SDMMC_INT_DCRC | SDMMC_INT_EBE)))
            {
                cmd->data->error = -EILSEQ;               
                output = SDM_DATA_CRC_ERROR;
                host->errorstep = 0x16; 
            }
            else
            {
                output = rk29_sdmmc_wait_unbusy(host);
                if(SDM_SUCCESS != output)
                {
                    host->errorstep = 0x17;
                    cmd->data->error = -ETIMEDOUT;
                }

                host->data->bytes_xfered = host->data->blocks * host->data->blksz;
            }
        }
        else
        {   
            if(MMC_BUS_TEST_R==cmd->opcode)
                  status = 0;//emmc

            if( status  & SDMMC_INT_SBE)
            {
                cmd->data->error = -EIO;
                host->errorstep = 0x18;
                output = SDM_START_BIT_ERROR;
            }
            else if((status  & SDMMC_INT_EBE) && (cmd->opcode != 14)) //MMC4.0, BUSTEST_R, A host read the reserved bus testing data parttern from a card.
            {
                cmd->data->error = -EILSEQ;
                host->errorstep = 0x19;
                output = SDM_END_BIT_ERROR;
            }
            else if(status  & SDMMC_INT_DRTO)
            {
                cmd->data->error = -ETIMEDOUT;
                host->errorstep = 0x1A;
                output = SDM_DATA_READ_TIMEOUT;
            }
            else if(status  & SDMMC_INT_DCRC)
            {
                host->errorstep = 0x1B;
                cmd->data->error = -EILSEQ;
                output = SDM_DATA_CRC_ERROR;
            }
            else
            {
                output = rk29_sdmmc_read_remain_data(host, (host->data->blocks * host->data->blksz), host->pbuf);
                if(SDM_SUCCESS == output)
                {
                    host->data->bytes_xfered = host->data->blocks * host->data->blksz;
                }
            }       
        }
    }

    if(SDM_SUCCESS == output)
    {
        if ((mmc_resp_type(cmd) == MMC_RSP_R1B) || (MMC_STOP_TRANSMISSION == cmd->opcode))
        {
            output = rk29_sdmmc_wait_unbusy(host);
            if((SDM_SUCCESS != output) && (!host->mrq->cmd->error))
            {
                printk(KERN_WARNING "%s..%d...   CMD12 wait busy timeout!!!!! errorStep=0x%x   [%s]\n", \
						__FUNCTION__, __LINE__, host->errorstep, host->dma_name);
                rk29_sdmmc_clear_fifo(host);
                cmd->error = -ETIMEDOUT;
                host->mrq->cmd->error = -ETIMEDOUT;
                host->errorstep = 0x1C;
            }
        }
    }
    host->errorstep = 0xf6;
    
    //trace error
    if(cmd->data && cmd->data->error)
    { 
        if( (!cmd->error) && (0==cmd->retries))
        {         
            printk(KERN_WARNING "%s..%d......CMD=%d error!!!(arg=0x%x,cmdretry=%d,blksize=%d, blocks=%d), \n \
                statusReg=0x%x, ctrlReg=0x%x, nerrorTimes=%d, errorStep=0x%x. [%s]\n",\
                __FUNCTION__, __LINE__, cmd->opcode, cmd->arg, cmd->retries,cmd->data->blksz, cmd->data->blocks,
                rk29_sdmmc_read(host->regs, SDMMC_STATUS),
                rk29_sdmmc_read(host->regs, SDMMC_CTRL),
                host->error_times,host->errorstep, host->dma_name);
        }
        cmd->error = -ENODATA;
    }
    host->errorstep = 0xf5;

exit:

#ifdef RK29_SDMMC_LIST_QUEUE
	if (!list_empty(&host->queue)) 
	{
		printk(KERN_WARNING "%s..%d..  Danger!Danger!. continue the next request in the queue.  [%s]\n",\
		        __FUNCTION__, __LINE__, host->dma_name);

		host = list_entry(host->queue.next,
				struct rk29_sdmmc, queue_node);
		list_del(&host->queue_node);
		host->state = STATE_SENDING_CMD;
		rk29_sdmmc_start_request(host->mmc);
	} 
	else 
	{	
		dev_vdbg(&host->pdev->dev, "list empty\n");
		host->state = STATE_IDLE;
	}
#else
    dev_vdbg(&host->pdev->dev, "list empty\n");
	host->state = STATE_IDLE;
#endif
	
}

static int rk29_sdmmc_command_complete(struct rk29_sdmmc *host,
			struct mmc_command *cmd)
{
	u32	 value, status = host->cmd_status;
	int  timeout, output= SDM_SUCCESS;

    xbwprintk(7, "%s..%d.  cmd=%d, host->state=0x%x, cmdINT=0x%x\n,pendingEvent=0x%lu,completeEvents=0x%lu. [%s]\n",\
        __FUNCTION__, __LINE__,cmd->opcode,host->state,status, host->pending_events,host->completed_events,host->dma_name);


    del_timer_sync(&host->request_timer);
    
    host->cmd_status = 0;

	if(host->cmdr & SDMMC_CMD_STOP)
    {
        output = rk29_sdmmc_reset_fifo(host);
        if (SDM_SUCCESS != output)
        {
            printk(KERN_WARNING "%s..%d......reset fifo fail! CMD%d(arg=0x%x, Retries=%d) [%s]\n",__FUNCTION__, __LINE__, \
                cmd->opcode, cmd->arg, cmd->retries,host->dma_name);
                
            cmd->error = -ETIMEDOUT;
            host->mrq->cmd->error = cmd->error;
            output = SDM_ERROR;
            host->errorstep = 0x1C;
            goto CMD_Errror;
        }
    }

    if(status & SDMMC_INT_RTO)
	{
	    xbwprintk(7, "%s..%d.  cmd=%d, host->state=0x%x, cmdINT=0x%x\n,pendingEvent=0x%lu,completeEvents=0x%lu. [%s]\n",\
            __FUNCTION__, __LINE__,cmd->opcode,host->state,status, host->pending_events,host->completed_events,host->dma_name);
	    cmd->error = -ENOMEM;
	    host->mrq->cmd->error = cmd->error;
        output = SDM_BUSY_TIMEOUT;
        host->errorstep = 0x1E;

        //rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_RTO);
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,0xFFFFFFFF);  //modifyed by xbw at 2011-08-15
        
        if(host->use_dma)//if(host->dodma)
        {
           if(host->dodma) 
           {
                rk29_sdmmc_stop_dma(host);
                rk29_sdmmc_control_host_dma(host, FALSE);
                host->dodma = 0;
           }
            
            value = rk29_sdmmc_read(host->regs, SDMMC_CTRL);
            value |= SDMMC_CTRL_FIFO_RESET;
            rk29_sdmmc_write(host->regs, SDMMC_CTRL, value);

            timeout = 1000;
            while (((value = rk29_sdmmc_read(host->regs, SDMMC_CTRL)) & (SDMMC_CTRL_FIFO_RESET)) && (timeout > 0))
            {
                udelay(1);
                timeout--;
            }
            if (timeout == 0)
            {   
                output = SDM_FALSE;
                host->errorstep = 0x1D;
                printk(KERN_WARNING "%s..%d......reset CTRL fail! CMD%d(arg=0x%x, Retries=%d).[%s]\n",\
                    __FUNCTION__, __LINE__, cmd->opcode, cmd->arg, cmd->retries,host->dma_name);
                
               goto CMD_Errror;
            }
        }

	}	

	if(cmd->flags & MMC_RSP_PRESENT) 
	{
	    if(cmd->flags & MMC_RSP_136) 
	    {
            cmd->resp[3] = rk29_sdmmc_read(host->regs, SDMMC_RESP0);
            cmd->resp[2] = rk29_sdmmc_read(host->regs, SDMMC_RESP1);
            cmd->resp[1] = rk29_sdmmc_read(host->regs, SDMMC_RESP2);
            cmd->resp[0] = rk29_sdmmc_read(host->regs, SDMMC_RESP3);
	    } 
	    else 
	    {
	        cmd->resp[0] = rk29_sdmmc_read(host->regs, SDMMC_RESP0);
	    }
	}

     #if 1	
	if(cmd->error)
	{
	    del_timer_sync(&host->DTO_timer);

        //trace error
	    if((0==cmd->retries) && (host->error_times++%(RK29_ERROR_PRINTK_INTERVAL*3) == 0) && (12 != cmd->opcode))
	    {
	        if( ((RK29_CTRL_SDMMC_ID==host->pdev->id)&&(MMC_SLEEP_AWAKE!=cmd->opcode)) || 
	             ((RK29_CTRL_SDMMC_ID!=host->pdev->id)&&(MMC_SEND_EXT_CSD!=cmd->opcode))  )
	        {
	            printk(KERN_WARNING "%s..%d...CMD%d(arg=0x%x), hoststate=%d, errorTimes=%d, errorStep=0x%x ! [%s]\n",\
                    __FUNCTION__, __LINE__, cmd->opcode, cmd->arg, host->state,host->error_times,host->errorstep, host->dma_name);
	        }
	    }

	}
    #endif

    del_timer_sync(&host->request_timer);


	return SDM_SUCCESS;
   
CMD_Errror:
    del_timer_sync(&host->request_timer);
	del_timer_sync(&host->DTO_timer);

	if((0==cmd->retries) && (host->error_times++%RK29_ERROR_PRINTK_INTERVAL == 0))
    {
        printk(KERN_WARNING "%s..%d....command_complete(CMD=%d, arg=%x) error=%d. [%s]\n",\
            __FUNCTION__, __LINE__, host->cmd->opcode,host->cmd->arg, output, host->dma_name);
    }
        
    return output;
    
}


static void rk29_sdmmc_start_error(struct rk29_sdmmc *host)
{
    host->cmd->error = -EIO;
    host->mrq->cmd->error = -EIO;
    host->cmd_status |= SDMMC_INT_RTO;

    del_timer_sync(&host->request_timer);

    rk29_sdmmc_command_complete(host, host->mrq->cmd);    
    rk29_sdmmc_request_end(host, host->mrq->cmd);
}

static void rk29_sdmmc_tasklet_func(unsigned long priv)
{
	struct rk29_sdmmc	*host = (struct rk29_sdmmc *)priv;
	struct mmc_data		*data = host->cmd->data;
	enum rk29_sdmmc_state	state = host->state;
	int pending_flag, stopflag;

	rk29_sdmmc_enable_irq(host, false);
	spin_lock(&host->lock);//spin_lock_irqsave(&host->lock, iflags); 
	
	state = host->state;
	pending_flag = 0;
	stopflag = 0;
	
	do 
	{
        switch (state) 
        {
            case STATE_IDLE:
            {
                xbwprintk(7, "%s..%d..   prev_state=  STATE_IDLE  [%s]\n", \
						__FUNCTION__, __LINE__, host->dma_name);
            	break;
            }

            case STATE_SENDING_CMD:
            {
                xbwprintk(7, "%s..%d..   prev_state=  STATE_SENDING_CMD, cmderr=%d , pendingEvernt=0x%lu  [%s]\n",\
                    __FUNCTION__, __LINE__, host->cmd->error,host->completed_events, host->dma_name);
                if(host->cmd->error)
                {
                    del_timer_sync(&host->request_timer);
                }
                
                if (!rk29_sdmmc_test_and_clear_pending(host, EVENT_CMD_COMPLETE))
                	break;
                 host->errorstep = 0xfb;

                del_timer_sync(&host->request_timer); //delete the timer for INT_COME_DONE

                rk29_sdmmc_set_completed(host, EVENT_CMD_COMPLETE);
                rk29_sdmmc_command_complete(host, host->cmd);
          
                if (!data)
                {
                    rk29_sdmmc_request_end(host, host->cmd);

                    xbwprintk(7, "%s..%d..  CMD%d call mmc_request_done() . [%s]\n", \
							__FUNCTION__, __LINE__,host->cmd->opcode,host->dma_name);
                    
                    host->complete_done = 1;
                    break;
                }
                host->errorstep = 0xfa;
                if(host->cmd->error)
                {
                    del_timer_sync(&host->DTO_timer); //delete the timer for INT_DTO
                    
                    if((data->stop) && (MMC_STOP_TRANSMISSION != host->cmd->opcode)) 
                    {
                        xbwprintk(7, "%s..%d..  cmderr, so call send_stop_cmd() [%s]\n", \
								__FUNCTION__, __LINE__, host->dma_name);

                        stopflag = 1;  //Moidfyed by xbw at 2011-09-08

                        break;
                    }

                    rk29_sdmmc_set_pending(host, EVENT_DATA_COMPLETE);
                }

                host->errorstep = 0xf9;
                state = STATE_DATA_BUSY;
                /* fall through */
            }

            case STATE_DATA_BUSY:
            {
                xbwprintk(7, "%s..%d..   prev_state= STATE_DATA_BUSY, pendingEvernt=0x%lu [%s]\n", \
						__FUNCTION__, __LINE__,host->pending_events, host->dma_name);

                if (!rk29_sdmmc_test_and_clear_pending(host, EVENT_DATA_COMPLETE))
                	break;	
                host->errorstep = 0xf8;
                rk29_sdmmc_set_completed(host, EVENT_DATA_COMPLETE);

             #if SDMMC_USE_INT_UNBUSY
                if((MMC_WRITE_BLOCK==host->cmd->opcode)||(MMC_WRITE_MULTIPLE_BLOCK==host->cmd->opcode))
                {
                    /*
                    ** use DTO_timer for waiting for INT_UNBUSY.
                    ** max 250ms in specification, but adapt 500 for the compatibility of all kinds of sick sdcard. 
                    */                    
                    mod_timer(&host->DTO_timer, jiffies + msecs_to_jiffies(5000));
                }
                else
                {
                    del_timer_sync(&host->DTO_timer); //delete the timer for INT_DTO
                }
  
                state = STATE_DATA_UNBUSY;

             #else
                del_timer_sync(&host->DTO_timer); //delete the timer for INT_DTO
             #endif
             }

             case STATE_DATA_UNBUSY:
             {
             #if SDMMC_USE_INT_UNBUSY
                if((MMC_WRITE_BLOCK==host->cmd->opcode)||(MMC_WRITE_MULTIPLE_BLOCK==host->cmd->opcode))
                {
                    if (!rk29_sdmmc_test_and_clear_pending(host, EVENT_DATA_UNBUSY))
                    	break;

                    del_timer_sync(&host->DTO_timer);
                }
                rk29_sdmmc_set_completed(host, EVENT_DATA_UNBUSY);
                state = STATE_DATA_END;
             #endif 
                rk29_sdmmc_request_end(host, host->cmd);

                if (data && !data->stop) 
                {
                    xbwprintk(7, "%s..%d..  CMD%d call mmc_request_done(). [%s]\n", \
							__FUNCTION__, __LINE__,host->cmd->opcode,host->dma_name);

                    if(!( (MMC_READ_SINGLE_BLOCK == host->cmd->opcode)&&( -EIO == data->error))) //deal with START_BIT_ERROR
                    {
                    	host->complete_done = 2;
                    	break;
                    }

                }
                host->errorstep = 0xf4;
                xbwprintk(7, "%s..%d..  after DATA_COMPLETE, so call send_stop_cmd() [%s]\n", \
						__FUNCTION__, __LINE__, host->dma_name);

                stopflag = 2; //Moidfyed by xbw at 2011-09-08
                
                break;
            }

            case STATE_SENDING_STOP:
            {
                xbwprintk(7, "%s..%d..   prev_state=  STATE_SENDING_STOP, pendingEvernt=0x%lu  [%s]\n", \
						__FUNCTION__, __LINE__, host->pending_events, host->dma_name);

                if (!rk29_sdmmc_test_and_clear_pending(host, EVENT_CMD_COMPLETE))
                	break;

                rk29_sdmmc_command_complete(host, host->cmd);
                del_timer_sync(&host->request_timer); //delete the timer for INT_CMD_DONE int CMD12
                rk29_sdmmc_request_end(host, host->cmd);
                
                host->complete_done = 3;
                break;
            }
            
            case STATE_DATA_END:
                break;
            default:
                break;     	
        }

        pending_flag = (host->complete_done > 0) && (host->retryfunc<50) \
                       && (rk29_sdmmc_test_pending(host, EVENT_CMD_COMPLETE)|| rk29_sdmmc_test_pending(host, EVENT_DATA_COMPLETE) ) \
                       && test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);

        if(pending_flag)
        {
            xbwprintk(7, "%s..%d...  cmd=%d(arg=0x%x),completedone=%d, retrycount=%d, doneflag=%d, \n \
                host->state=0x%x, switchstate=%x, \n \
                pendingEvent=0x%lu, completeEvents=0x%lu, \n \
                mrqCMD=%d, arg=0x%x [%s]\n",\
                
                __FUNCTION__, __LINE__,host->cmd->opcode, host->cmd->arg, host->complete_done,\
                host->retryfunc, host->mmc->doneflag,host->state, state, \
                host->pending_events,host->completed_events,\
                host->mrq->cmd->opcode, host->mrq->cmd->arg, host->dma_name);
                
            cpu_relax();
        }
                        
	} while(pending_flag && ++host->retryfunc); //while(0);

	if(0!=stopflag)
    {
        if(host->cmd->error)
        xbwprintk(3,"%d:  call send_stop_cmd== %d,  completedone=%d, doneflag=%d, hoststate=%x, statusReg=0x%x \n", \
            __LINE__,stopflag, host->complete_done, host->mmc->doneflag, state, rk29_sdmmc_read(host->regs, SDMMC_STATUS));
            
        host->errorstep = 0xe0;  
        
        if(test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags))
        {
            state = STATE_SENDING_CMD;
            send_stop_cmd(host);   //Moidfyed by xbw at 2011-09-08
        }
        else
        {
            host->complete_done = 5;
        }
    }

	host->state = state;
		 
    if((0==host->complete_done)&& host->mmc->doneflag && test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags))
    {
        host->errorstep = 0xf2;

     #if 0
        //debug
        if(12==host->cmd->opcode)
        {
             printk(KERN_ERR "%d... cmd=%d(arg=0x%x),blksz=%d,blocks=%d,errorStep=0x%x,\n host->state=%x, statusReg=0x%x  [%s]\n",\
                 __LINE__,host->mrq->cmd->opcode, host->mrq->cmd->arg, host->mrq->cmd->data->blksz, host->mrq->cmd->data->blocks,\
                 host->errorstep,host->state,rk29_sdmmc_read(host->regs, SDMMC_STATUS),host->dma_name);
        }
      #endif  
        
        spin_unlock(&host->lock);//spin_unlock_irqrestore(&host->lock, iflags);
        rk29_sdmmc_enable_irq(host, true);
        return;
    }
    host->errorstep = 0xf3; 
	host->state = STATE_IDLE;
	 
	 if(host->mrq && host->mmc->doneflag && host->complete_done)
	 {
	    host->mmc->doneflag = 0;
	    host->complete_done = 0;
	    spin_unlock(&host->lock);//spin_unlock_irqrestore(&host->lock, iflags);
	    rk29_sdmmc_enable_irq(host, true);
	    mmc_request_done(host->mmc, host->mrq);
	 }
	 else
	 {
	    spin_unlock(&host->lock);//spin_unlock_irqrestore(&host->lock, iflags);
	    rk29_sdmmc_enable_irq(host, true);
	 }
}


static inline void rk29_sdmmc_cmd_interrupt(struct rk29_sdmmc *host, u32 status)
{
    u32 multi, unit;
    
	host->cmd_status |= status;
    host->errorstep = 0xfc;
    if((MMC_STOP_TRANSMISSION != host->cmd->opcode) && (host->cmdr & SDMMC_CMD_DAT_EXP))
    {
        unit = 2*1024*1024;
        multi = rk29_sdmmc_read(host->regs, SDMMC_BYTCNT)/unit;
        multi += ((rk29_sdmmc_read(host->regs, SDMMC_BYTCNT)%unit) ? 1 :0 );
        multi = (multi>0) ? multi : 1;
        multi += (host->cmd->retries>2)?2:host->cmd->retries;
	    mod_timer(&host->DTO_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_WAIT_DTO_INTERNVAL*multi));//max wait 8s larger  
	}
	
	smp_wmb();
	rk29_sdmmc_set_pending(host, EVENT_CMD_COMPLETE);
	tasklet_schedule(&host->tasklet);
}

static irqreturn_t rk29_sdmmc_interrupt(int irq, void *dev_id)
{
	struct rk29_sdmmc	*host = dev_id;
	u32			status,  pending;
	bool present;
	bool present_old;
	int sdio_irq=0;

	spin_lock(&host->lock);

    status = rk29_sdmmc_read(host->regs, SDMMC_RINTSTS);
    pending = rk29_sdmmc_read(host->regs, SDMMC_MINTSTS);// read only mask reg
    if (!pending)
    {
    	goto Exit_INT;
    }


    if(pending & SDMMC_INT_CD) 
    {
        //disable_irq_nosync(host->irq);
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS, SDMMC_INT_CD); // clear sd detect int
        smp_wmb();
    	present = rk29_sdmmc_get_cd(host->mmc);
    	present_old = test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
  	
    	if(present != present_old)
    	{    	    
        	printk(KERN_INFO "\n******************\n%s:INT_CD=0x%x,INT-En=%d,hostState=%d,  present Old=%d ==> New=%d . [%s]\n",\
                    __FUNCTION__, pending, host->mmc->re_initialized_flags, host->state, present_old, present,  host->dma_name);

    	    rk28_send_wakeup_key(); //wake up backlight
    	    host->error_times = 0;

    	    #if 1
    	    del_timer(&host->request_timer);
	        del_timer(&host->DTO_timer);
    	    rk29_sdmmc_dealwith_timeout(host);       	    
            #endif
        	            
    	    if(present)
    	    {
    	        set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);

    	        if(host->mmc->re_initialized_flags)
        	    {
        	        mod_timer(&host->detect_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_REMOVAL_DELAY));
        	    }
        	    else
        	    {
        	        mod_timer(&host->detect_timer, jiffies + msecs_to_jiffies(RK29_SDMMC_REMOVAL_DELAY*2));
        	    }
    	    }
    	    else
    	    {
    	        clear_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
    	        host->mmc->re_initialized_flags = 0;

    	        mmc_detect_change(host->mmc, 200);
    	    }

    	}

        goto Exit_INT;

    }
    
    if (pending & SDMMC_INT_CMD_DONE) {

        xbwprintk(6, "%s..%d..  CMD%d INT_CMD_DONE  INT=0x%x   [%s]\n", \
				__FUNCTION__, __LINE__, host->cmd->opcode,pending, host->dma_name);

        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_CMD_DONE);  //  clear interrupt
        rk29_sdmmc_cmd_interrupt(host, status);
        smp_wmb();

        goto Exit_INT;
    }

#if !defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
    if(pending & SDMMC_INT_SDIO) 
    {	
        xbwprintk(7, "%s..%d..  INT_SDIO  INT=0x%x   [%s]\n", \
				__FUNCTION__, __LINE__, pending, host->dma_name);

        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_SDIO);
        smp_wmb();
        sdio_irq = 1;

        goto Exit_INT;
    }
#endif

    if(pending & SDMMC_INT_RTO) 
    {
    	xbwprintk(7, "%s..%d..  CMD%d CMD_ERROR_FLAGS  INT=0x%x   [%s]\n", \
				__FUNCTION__, __LINE__, host->cmd->opcode,pending, host->dma_name);

        //rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_RTO);
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,0xFFFFFFFF); //Modifyed by xbw at 2011-08-15
        host->cmd_status = status;
        smp_wmb();
        rk29_sdmmc_set_pending(host, EVENT_CMD_COMPLETE);

        if(!(pending & SDMMC_INT_CMD_DONE))
        {
            tasklet_schedule(&host->tasklet);
        }

        goto Exit_INT;
    }


    if(pending & SDMMC_INT_HLE)
    {
        printk(KERN_WARNING "%s: Error due to hardware locked. Please check your hardware. INT=0x%x, CMD%d(arg=0x%x, retries=%d). [%s]\n",\
				__FUNCTION__, pending,host->cmd->opcode, host->cmd->arg, host->cmd->retries, host->dma_name);  	      
    
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_HLE); 
        goto Exit_INT;
    }


    if(pending & SDMMC_INT_DTO) 
    {	
        xbwprintk(1,"%d..%s: DTO INT=0x%x ,RINTSTS=0x%x, CMD%d(arg=0x%x, retries=%d),host->state=0x%x.  [%s]\n", \
				__LINE__,__FUNCTION__, pending,status, host->cmd->opcode, host->cmd->arg, host->cmd->retries, host->state,host->dma_name);

        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_DTO); 
        del_timer(&host->DTO_timer); //delete the timer for INT_DTO

    	host->data_status |= status;

        smp_wmb();

        rk29_sdmmc_set_pending(host, EVENT_DATA_COMPLETE);
        tasklet_schedule(&host->tasklet);
        goto Exit_INT;
    }


    if (pending & SDMMC_INT_FRUN) 
    { 
    	printk(KERN_WARNING "%s: INT=0x%x Oh!My God,let me see!What happened?Why?Where? CMD%d(arg=0x%x, retries=%d). [%s]\n", \
				__FUNCTION__, pending, host->cmd->opcode, host->cmd->arg, host->cmd->retries,host->dma_name);
    	
        //rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_FRUN);
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,0xFFFFFFFE); 
        goto Exit_INT;
    }

    if (pending & SDMMC_INT_RXDR) 
    {	
        xbwprintk(6, "%s..%d..  SDMMC_INT_RXDR  INT=0x%x   [%s]\n", \
				__FUNCTION__, __LINE__, pending, host->dma_name);

        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_RXDR);  //  clear interrupt
        rk29_sdmmc_read_data_pio(host);
    }

    if (pending & SDMMC_INT_TXDR) 
    {
        xbwprintk(6, "%s..%d..  SDMMC_INT_TXDR  INT=0x%x   [%s]\n", \
				__FUNCTION__, __LINE__, pending, host->dma_name);
				
        rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_TXDR);  //  clear interrupt
        rk29_sdmmc_write_data_pio(host); 
    }

#if SDMMC_USE_INT_UNBUSY
        if(pending & SDMMC_INT_UNBUSY) 
        {
            xbwprintk(6, "%d..%s: INT=0x%x ,RINTSTS=0x%x, CMD%d(arg=0x%x, retries=%d),host->state=0x%x. [%s]\n", \
                    __LINE__,__FUNCTION__, pending,status, host->cmd->opcode, host->cmd->arg, host->cmd->retries, \
                    host->state,host->dma_name);
    
            rk29_sdmmc_write(host->regs, SDMMC_RINTSTS,SDMMC_INT_UNBUSY); 
         if (!rk29_sdmmc_test_pending(host, EVENT_DATA_UNBUSY))   
         {
            host->data_status = status;
            smp_wmb();
            rk29_sdmmc_set_pending(host, EVENT_DATA_UNBUSY);
            tasklet_schedule(&host->tasklet);  
          }  
            
            goto Exit_INT;
        }
#endif

Exit_INT:

	spin_unlock(&host->lock);

    if(1 == sdio_irq)
    {
        mmc_signal_sdio_irq(host->mmc);
    }
	
	return IRQ_HANDLED;
}

/*
 *
 * MMC card detect thread, kicked off from detect interrupt, 1 timer 
 *
 */
static void rk29_sdmmc_detect_change(unsigned long data)
{
	struct rk29_sdmmc *host = (struct rk29_sdmmc *)data;

	if(!host->mmc)
	    return;
   
	smp_rmb();

    if((RK29_CTRL_SDMMC_ID == host->host_dev_id) && rk29_sdmmc_get_cd(host->mmc))
    {
        host->mmc->re_initialized_flags =1;
    }
    
	mmc_detect_change(host->mmc, 0);	

}

static void rk29_sdmmc1_check_status(unsigned long data)
{
        struct rk29_sdmmc *host = (struct rk29_sdmmc *)data;
        struct rk29_sdmmc_platform_data *pdata = host->pdev->dev.platform_data;
        unsigned int status;

    status = pdata->status(mmc_dev(host->mmc));
    
    pr_info("%s: slot status change detected(%d-%d)\n",mmc_hostname(host->mmc), host->oldstatus, status);
    
    if (status ^ host->oldstatus)
    {        
        if (status) 
        {
            #if RK_SDMMC_USE_SDIO_SUSPEND_RESUME
	        if(host->host_dev_id == RK29_CTRL_SDIO1_ID)
		       host->mmc->pm_caps |= (MMC_PM_KEEP_POWER|MMC_PM_WAKE_SDIO_IRQ);
		    #endif
		    
            rk29_sdmmc_hw_init(host);
            set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
            mod_timer(&host->detect_timer, jiffies + msecs_to_jiffies(200));
        }
        else 
        {
            clear_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
            rk29_sdmmc_detect_change((unsigned long)host);
        }
    }

    host->oldstatus = status;
}

static void rk29_sdmmc1_status_notify_cb(int card_present, void *dev_id)
{
        struct rk29_sdmmc *host = dev_id;

        rk29_sdmmc1_check_status((unsigned long)host);
}


#if defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
static irqreturn_t det_keys_isr(int irq, void *dev_id);
static void rk29_sdmmc_detect_change_work(struct work_struct *work)
{
	int ret;
    struct rk29_sdmmc *host =  container_of(work, struct rk29_sdmmc, work.work);

    rk28_send_wakeup_key();
	rk29_sdmmc_detect_change(host);               	 
}
#endif

static irqreturn_t det_keys_isr(int irq, void *dev_id)
{
	struct rk29_sdmmc *host = dev_id;

#if defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
	bool present;
	bool present_old;
	int value;

    present = rk29_sdmmc_get_cd(host->mmc);
    present_old = test_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);

    if(present != present_old)
    {
        printk(KERN_INFO "\n******************\n%s: present Old=%d ==> New=%d . [%s]\n",\
                __FUNCTION__,  present_old, present,  host->dma_name);

    #if 1
        value = gpio_get_value(host->det_pin.io) ?IRQ_TYPE_EDGE_FALLING : IRQ_TYPE_EDGE_RISING;
    
	    irq_set_irq_type(host->gpio_irq, value);
    #endif
    
        #if 1
        del_timer(&host->request_timer);
        del_timer(&host->DTO_timer);
        rk29_sdmmc_dealwith_timeout(host);              
        #endif
        enable_irq_wake(host->gpio_irq);
		    
    	if(present)
    	{
    	    set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
    		schedule_delayed_work(&host->work, msecs_to_jiffies(500));
    	}
    	else
    	{
    	    clear_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
            host->mmc->re_initialized_flags = 0;
    		schedule_delayed_work(&host->work, 0);
        }
    }
#else
	dev_info(&host->pdev->dev, "sd det_gpio changed(%s), send wakeup key!\n",
		gpio_get_value(host->det_pin.io)?"removed":"insert");
	rk29_sdmmc_detect_change((unsigned long)dev_id);
#endif	
	return IRQ_HANDLED;
}

static int rk29_sdmmc_probe(struct platform_device *pdev)
{
	struct mmc_host 		*mmc;
	struct rk29_sdmmc		*host;
	struct resource			*regs;
	struct rk29_sdmmc_platform_data *pdata;
    int level_value;
	int		real_dev_id,ret = 0;

#if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)	
	unsigned long trigger_flags;
#endif

    /* must have platform data */
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "Platform data missing\n");
		ret = -ENODEV;
		goto out;
	}

	//For compatibility with old code, do not set this dev->id to 2, refer to devices.c
	real_dev_id = (-1 == pdev->id)?RK29_SDMMC_EMMC_ID:pdev->id;	

#ifdef USE_SDMMC_DATA4_DATA7
	if(pdata->emmc_is_selected)
	{	    
 	    if(!pdata->emmc_is_selected(real_dev_id))
	    {
	        printk(KERN_ERR "%s: internal_storage is NOT emmc.\n", __FUNCTION__);
	        goto out;
	    }
	}
#endif

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs)
	{
		return -ENXIO;
	}

	mmc = mmc_alloc_host(sizeof(struct rk29_sdmmc), &pdev->dev);
	if (!mmc)
	{
		ret = -ENOMEM;
		goto rel_regions;
	}	
    
	host = mmc_priv(mmc);
    host->mmc = mmc;
    host->pdev = pdev;

	host->ctype = 0; // set default 1 bit mode
	host->errorstep = 0;
	host->bus_mode = 5;
	host->old_cmd = 100;
	host->clock =0;
	host->old_div = 0xFF;
	host->error_times = 0;
	host->state = STATE_IDLE;
	host->complete_done = 0;
	host->retryfunc = 0;
	host->mrq = NULL;
	host->new_mrq = NULL;
	host->irq_state = true;
	host->timeout_times = 0;
	host->host_dev_id = real_dev_id;

	//detect pin info
    host->det_pin.io        = pdata->det_pin_info.io;
    host->det_pin.enable    = pdata->det_pin_info.enable;
    host->det_pin.iomux.name  = pdata->det_pin_info.iomux.name;
    host->det_pin.iomux.fgpio = pdata->det_pin_info.iomux.fgpio;
    host->det_pin.iomux.fmux  = pdata->det_pin_info.iomux.fmux;
    //power pin info
    host->gpio_power_en = pdata->power_en;
    host->gpio_power_en_level = pdata->power_en_level;

    host->set_iomux = pdata->set_iomux;

#if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
    if(RK29_CTRL_SDIO1_ID == host->host_dev_id)
    {
        host->sdio_INT_gpio = pdata->sdio_INT_gpio;
        #ifdef USE_SDIO_INT_LEVEL
        host->trigger_level = pdata->sdio_INT_level;
        #endif
    }
#endif

	if(pdata->io_init)
		pdata->io_init();
		
	spin_lock_init(&host->lock);
    
#ifdef RK29_SDMMC_LIST_QUEUE	
	INIT_LIST_HEAD(&host->queue);
#endif	

    if(RK29_SDMMC_EMMC_ID == host->host_dev_id)//emmc
    {
    //get clk for eMMC controller.
    host->clk = clk_get(&pdev->dev, "emmc");
    clk_set_rate(host->clk,MMCHS_52_FPP_FREQ*2);
    clk_enable(host->clk);
	clk_enable(clk_get(&pdev->dev, "hclk_emmc"));
}
else
{
	host->clk = clk_get(&pdev->dev, "mmc");

#if RK29_SDMMC_DEFAULT_SDIO_FREQ
    clk_set_rate(host->clk,SDHC_FPP_FREQ);
#else    
        if(RK29_CTRL_SDMMC_ID== host->host_dev_id)
	    clk_set_rate(host->clk,SDHC_FPP_FREQ);
	else
	    clk_set_rate(host->clk,RK29_MAX_SDIO_FREQ); 

#endif

	clk_enable(host->clk);
	clk_enable(clk_get(&pdev->dev, "hclk_mmc"));
}
	ret = -ENOMEM;
	host->regs = ioremap(regs->start, regs->end - regs->start + 1);
	if (!host->regs)
	{
	    host->errorstep = 0x8A;
	    goto err_freemap; 
	}
	
    mmc->host_dev_id = host->host_dev_id;
    mmc->ops = &rk29_sdmmc_ops[host->host_dev_id];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))    
	mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
#endif	
	mmc->f_min = FOD_FREQ;
	
    if(RK29_SDMMC_EMMC_ID == host->host_dev_id)//emmc
{
    //defined for eMMC controller.
    mmc->f_max = MMCHS_52_FPP_FREQ;

    mmc->rk_sdmmc_emmc_used = 1;//supprot eMMC code.
}
else
{
    mmc->rk_sdmmc_emmc_used = 0;
#if RK29_SDMMC_DEFAULT_SDIO_FREQ
    mmc->f_max = SDHC_FPP_FREQ;
#else
        if(RK29_CTRL_SDMMC_ID== host->host_dev_id)
    {
        mmc->f_max = SDHC_FPP_FREQ;
    }
    else
    {
        mmc->f_max = RK29_MAX_SDIO_FREQ;
    }

#endif 
}
	mmc->ocr_avail = pdata->host_ocr_avail;
	mmc->ocr_avail |= MMC_VDD_27_28|MMC_VDD_28_29|MMC_VDD_29_30|MMC_VDD_30_31
                     | MMC_VDD_31_32|MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_34_35| MMC_VDD_35_36;    ///set valid volage 2.7---3.6v
#if 1

    mmc->ocr_avail = mmc->ocr_avail |MMC_VDD_26_27 |MMC_VDD_25_26 |MMC_VDD_24_25 |MMC_VDD_23_24
                     |MMC_VDD_22_23 |MMC_VDD_21_22 |MMC_VDD_20_21 |MMC_VDD_165_195;
#endif
	mmc->caps = pdata->host_caps;
#if 1	
    mmc->caps = mmc->caps | MMC_CAP_1_8V_DDR |MMC_CAP_1_2V_DDR /*|MMC_CAP_DRIVER_TYPE_A |MMC_CAP_DRIVER_TYPE_C |MMC_CAP_DRIVER_TYPE_D*/
                |MMC_CAP_UHS_SDR12 |MMC_CAP_UHS_SDR25 |MMC_CAP_UHS_SDR50
               /* |MMC_CAP_MAX_CURRENT_200 |MMC_CAP_MAX_CURRENT_400 |MMC_CAP_MAX_CURRENT_600 |MMC_CAP_MAX_CURRENT_800
                |MMC_CAP_SET_XPC_330*/;
#endif
#if SDMMC_SUPPORT_DDR_MODE
    mmc->caps = mmc->caps |MMC_CAP_UHS_DDR50 |MMC_CAP_UHS_SDR104;
#endif
    mmc->caps = mmc->caps |MMC_CAP_BUS_WIDTH_TEST| MMC_CAP_ERASE | MMC_CAP_CMD23;
    
	mmc->re_initialized_flags = 1;
	mmc->doneflag = 1;
	mmc->sdmmc_host_hw_init = rk29_sdmmc_hw_init;

    /*
	 * We can do SGIO
	*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
	mmc->max_segs = 64;
#else
	mmc->max_phys_segs = 64;
	mmc->max_hw_segs = 64; 
#endif

	/*
	 * Block size can be up to 2048 bytes, but must be a power of two.
	*/
	mmc->max_blk_size = 65536;//4095;

	/*
	 * No limit on the number of blocks transferred.
	*/
	mmc->max_blk_count = 65535;//4096; 

	/*
	 * Since we only have a 16-bit data length register, we must
	 * ensure that we don't exceed 2^16-1 bytes in a single request.
	*/
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count; //8M bytes(2K*4K)

    /*
	 * Set the maximum segment size.  Since we aren't doing DMA
	 * (yet) we are only limited by the data length register.
	*/
	mmc->max_seg_size = mmc->max_req_size;

	tasklet_init(&host->tasklet, rk29_sdmmc_tasklet_func, (unsigned long)host);

    /* Create card detect handler thread  */
	setup_timer(&host->detect_timer, rk29_sdmmc_detect_change,(unsigned long)host);
	setup_timer(&host->request_timer,rk29_sdmmc_INT_CMD_DONE_timeout,(unsigned long)host);
	setup_timer(&host->DTO_timer,rk29_sdmmc_INT_DTO_timeout,(unsigned long)host);

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0)
	{
	    host->errorstep = 0x8B;
		ret = -EINVAL;
		goto err_freemap;
	}

    memcpy(host->dma_name, pdata->dma_name, 8);    
	host->use_dma = pdata->use_dma;

    xbwprintk(7,"%s..%s..%d..***********  Bus clock= %d Khz  **** [%s]\n",\
        __FILE__, __FUNCTION__,__LINE__,clk_get_rate(host->clk)/1000, host->dma_name);

	/*DMA init*/
	if(host->use_dma)
	{
        host->dma_info = rk29_sdmmc_dma_infos[host->host_dev_id];            
        ret = rk29_dma_request(host->dma_info.chn, &(host->dma_info.client), NULL); 
        if (ret < 0)
        {
        	printk(KERN_WARNING "%s..%d...rk29_dma_request error=%d. [%s]\n", \
					__FUNCTION__, __LINE__,ret, host->dma_name);
        	host->errorstep = 0x97;
            goto err_freemap; 
        }
        
#if 0  //deal with the old API of DMA-module 
		ret = rk29_dma_config(host->dma_info.chn, 4);
#else  //deal with the new API of DMA-module 
        if(RK29_CTRL_SDMMC_ID== host->host_dev_id)
        {
            ret = rk29_dma_config(host->dma_info.chn, 4, 16);
        }
        else
        {
            #if defined(CONFIG_ARCH_RK29)
                // to maintain set this value to 1 in RK29,noted at 2012-07-16
                ret = rk29_dma_config(host->dma_info.chn, 4, 1);  
            #else
                // a unified set the burst value to 16 in RK30,noted at 2012-07-16
                ret = rk29_dma_config(host->dma_info.chn, 4, 16); 
            #endif 
        }
#endif
        if(ret < 0)
		{
            printk(KERN_WARNING "%s..%d..  rk29_dma_config error=%d. [%s]\n", \
					__FUNCTION__, __LINE__, ret, host->dma_name);
            host->errorstep = 0x98;
            goto err_dmaunmap;
		}

        ret = rk29_dma_set_buffdone_fn(host->dma_info.chn, rk29_sdmmc_dma_complete);	
		if(ret < 0)
		{
            printk(KERN_WARNING "%s..%d..  dma_set_buffdone_fn error=%d. [%s]\n", \
					__FUNCTION__, __LINE__, ret, host->dma_name);
            host->errorstep = 0x99;
            goto err_dmaunmap;
		}
		
		host->dma_addr = regs->start + SDMMC_DATA;
	}

    /*
	 * Get the host data width,default 32bit
	*/
    host->push_data = rk_sdmmc_push_data32;
	host->pull_data = rk_sdmmc_pull_data32;

#if defined(CONFIG_SDMMC0_RK29_WRITE_PROTECT) || defined(CONFIG_SDMMC1_RK29_WRITE_PROTECT)
	host->write_protect = pdata->write_prt;
	host->protect_level = pdata->write_prt_enalbe_level;
#endif	

#if defined(CONFIG_ARCH_RK29)
   if((RK29_CTRL_SDMMC_ID == host->host_dev_id)|| (RK29_SDMMC_EMMC_ID == host->host_dev_id)) 
    {
       rk29_sdmmc_hw_init(host);
    }
#endif

    ret = request_irq(host->irq, rk29_sdmmc_interrupt, 0, dev_name(&pdev->dev), host);
	if (ret)
	{	

	    printk(KERN_WARNING "%s..%d..  request_irq error=%d. [%s]\n", \
				__FUNCTION__, __LINE__, ret, host->dma_name);
	    host->errorstep = 0x8C;
	    goto err_dmaunmap;
	}

	//gpio request for switch_voltage 
	if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
	    gpio_request(SWITCH_VOLTAGE_18_33, "sd_volt_switch");

#if defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
    if((RK29_CTRL_SDMMC_ID == host->host_dev_id) && (INVALID_GPIO != host->det_pin.io))
    {
        INIT_DELAYED_WORK(&host->work, rk29_sdmmc_detect_change_work);
        ret = gpio_request(host->det_pin.io, "sd_detect");
		if(ret < 0) {
			dev_err(&pdev->dev, "gpio_request error\n");
			goto err_dmaunmap;
		}
		gpio_direction_input(host->det_pin.io);

        level_value = gpio_get_value(host->det_pin.io);       
        
		host->gpio_irq = gpio_to_irq(host->det_pin.io);
        ret = request_irq(host->gpio_irq, det_keys_isr,
					    level_value?IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING,
					    "sd_detect",
					    host);
		if(ret < 0) {
			dev_err(&pdev->dev, "gpio request_irq error\n");
			goto err_dmaunmap;
		}

		enable_irq_wake(host->gpio_irq);
    }
#elif DRIVER_SDMMC_USE_NEW_IOMUX_API
    if(RK29_CTRL_SDMMC_ID == host->host_dev_id)
    {
        iomux_set(MMC0_DETN);
    }

#endif
	
#if !defined( CONFIG_BCM_OOB_ENABLED) && !defined(CONFIG_MTK_COMBO_MT66XX)
#if defined(CONFIG_RK29_SDIO_IRQ_FROM_GPIO)
    if(RK29_CTRL_SDIO1_ID == host->host_dev_id)
    {
        gpio_request(host->sdio_INT_gpio, "sdio_interrupt");

         // intput + pull_Up,
        gpio_direction_input(host->sdio_INT_gpio);
        //gpio_direction_output(host->sdio_INT_gpio, GPIO_HIGH);
        gpio_pull_updown(host->sdio_INT_gpio, 0); //disable default internal pull-down

        host->sdio_irq = gpio_to_irq(host->sdio_INT_gpio);
        #ifdef USE_SDIO_INT_LEVEL
        trigger_flags = (host->trigger_level==GPIO_HIGH)?IRQF_TRIGGER_HIGH:IRQF_TRIGGER_LOW;
        #else
        trigger_flags = IRQF_TRIGGER_LOW;
        #endif
        //printk("%d..%s  sdio interrupt gpio level=%lu   ====[%s]====\n", __LINE__, __FUNCTION__, trigger_flags, host->dma_name);
        ret = request_irq(host->sdio_irq, rk29_sdmmc_sdio_irq_cb,
                    trigger_flags,
                    "sdio_interrupt",
                    host);                    
        if (ret)
        {	

            printk("%s..%d..  sdio_request_INT_irq error=%d ====xbw[%s]====\n", \
        			__FUNCTION__, __LINE__, ret, host->dma_name);
            host->errorstep = 0x8D;
            goto err_dmaunmap;
        }
        
        disable_irq_nosync(host->sdio_irq);
        enable_irq_wake(host->sdio_irq);
    }

#endif
#endif //#ifndef CONFIG_BCM_OOB_ENABLE
    
    /* setup sdmmc1 wifi card detect change */
    if (pdata->register_status_notify) {
        pdata->register_status_notify(rk29_sdmmc1_status_notify_cb, host);
    }

    if(RK29_CTRL_SDMMC_ID== host->host_dev_id)
    {
        if(rk29_sdmmc_get_cd(host->mmc))
        {
            set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
        }
        else
        {
            clear_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
        }
    }
    else if (RK29_CTRL_SDIO1_ID== host->host_dev_id)
    {
        #if defined(CONFIG_USE_SDMMC0_FOR_WIFI_DEVELOP_BOARD)
        if(0== host->host_dev_id)
        {
            set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
        }
        #endif

        #if defined(CONFIG_USE_SDMMC1_FOR_WIFI_DEVELOP_BOARD)
        if(1== host->host_dev_id)
        {
            set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
        }
        #endif
    }
    else
        set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);


    /* sdmmc1 wifi card slot status initially */
    if (pdata->status) {
        host->oldstatus = pdata->status(mmc_dev(host->mmc));
        if (host->oldstatus)  {
            set_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
        }else {
            clear_bit(RK29_SDMMC_CARD_PRESENT, &host->flags);
        }
    }


	platform_set_drvdata(pdev, mmc); 	

	mmc_add_host(mmc);

#ifdef RK29_SDMMC_NOTIFY_REMOVE_INSERTION
    
    globalSDhost[host->host_dev_id] = (struct rk29_sdmmc	*)host;
 #if defined(CONFIG_SDMMC0_RK29)   
    if(0== host->host_dev_id)
 #elif defined(CONFIG_SDMMC1_RK29)   
    if(1== host->host_dev_id) 
 #elif defined(CONFIG_SDMMC2_RK29)   
    if(2== host->host_dev_id)
 #else 
    if(0== host->host_dev_id)
 #endif   
    {
        rk29_sdmmc_progress_add_attr(pdev);
    }
#endif	
	
    printk(KERN_INFO ".Line%d..The End of SDMMC-probe %s.  [%s]\n", __LINE__, RK29_SDMMC_VERSION,host->dma_name);
	return 0;


err_dmaunmap:
	if(host->use_dma)
	{
	    rk29_dma_free(host->dma_info.chn, &host->dma_info.client);
	}

err_freemap:
	iounmap(host->regs);

rel_regions:
    mmc_free_host(mmc);

out:
	
	return ret;
}



static int __exit rk29_sdmmc_remove(struct platform_device *pdev)
{

    struct mmc_host *mmc = platform_get_drvdata(pdev);
    struct rk29_sdmmc *host;
    struct resource		*regs;

    if (!mmc)
        return -1;

    host = mmc_priv(mmc); 
    
	smp_wmb();
    rk29_sdmmc_control_clock(host, 0);

    /* Shutdown detect IRQ and kill detect thread */
	del_timer_sync(&host->detect_timer);
	del_timer_sync(&host->request_timer);
	del_timer_sync(&host->DTO_timer);

	tasklet_disable(&host->tasklet);
	free_irq(platform_get_irq(pdev, 0), host);
	if(host->use_dma)
	{
		rk29_dma_free(host->dma_info.chn, &host->dma_info.client);
	}

	mmc_remove_host(mmc);

	iounmap(host->regs);
	
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(regs->start,resource_size(regs));  

    mmc_free_host(mmc);
    platform_set_drvdata(pdev, NULL);

	return 0;
}


#ifdef CONFIG_PM

static int rk29_sdmmc_sdcard_suspend(struct rk29_sdmmc *host)
{
	int ret = 0;
#if !defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
    rk29_sdmmc_enable_irq(host,false);
    #if DRIVER_SDMMC_USE_NEW_IOMUX_API
    //need not to change mode to gpio.
    #else
    rk29_mux_api_set(host->det_pin.iomux.name, host->det_pin.iomux.fgpio);
    #endif
	gpio_request(host->det_pin.io, "sd_detect");
	gpio_direction_output(host->det_pin.io, GPIO_HIGH);
	gpio_direction_input(host->det_pin.io);

	host->gpio_irq = gpio_to_irq(host->det_pin.io);
	ret = request_irq(host->gpio_irq, det_keys_isr,
					    (gpio_get_value(host->det_pin.io))?IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING,
					    "sd_detect",
					    host);
	
	enable_irq_wake(host->gpio_irq);
	
#endif
	return ret;
}

static void rk29_sdmmc_sdcard_resume(struct rk29_sdmmc *host)
{
#if !defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
	disable_irq_wake(host->gpio_irq);
	free_irq(host->gpio_irq,host);
	gpio_free(host->det_pin.io);
	#if DRIVER_SDMMC_USE_NEW_IOMUX_API
	iomux_set(MMC0_DETN);
	#else
    rk29_mux_api_set(host->det_pin.iomux.name, host->det_pin.iomux.fmux);
    #endif
    rk29_sdmmc_enable_irq(host, true);
#endif
}

static int rk29_sdmmc_suspend(struct platform_device *pdev, pm_message_t state)
{
    struct mmc_host *mmc = platform_get_drvdata(pdev);
    struct rk29_sdmmc *host = mmc_priv(mmc);
    int ret = 0;

    if(host && host->pdev && (RK29_CTRL_SDMMC_ID == host->host_dev_id) )
    {
        if (mmc)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
            ret = mmc_suspend_host(mmc);
#else
            ret = mmc_suspend_host(mmc, state);
#endif

#if !defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
        if(rk29_sdmmc_sdcard_suspend(host) < 0)
			dev_info(&host->pdev->dev, "rk29_sdmmc_sdcard_suspend error\n");
#endif    
    }
#if RK_SDMMC_USE_SDIO_SUSPEND_RESUME    
    else if(host && host->pdev && (RK29_CTRL_SDIO1_ID == host->host_dev_id))
    {
        if (mmc)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
            ret = mmc_suspend_host(mmc);
#else
            ret = mmc_suspend_host(mmc, state);
#endif
        if(!ret)
        {
            clk_disable(host->clk);
            clk_disable(clk_get(&pdev->dev, "hclk_mmc"));
        }
    }
#endif // --#if RK_SDMMC_USE_SDIO_SUSPEND_RESUME
    else if(RK29_SDMMC_EMMC_ID == host->host_dev_id)
    {
        if (mmc)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
            ret = mmc_suspend_host(mmc);
#else
            ret = mmc_suspend_host(mmc, state);
#endif   
    }

    return ret;
}

static int rk29_sdmmc_resume(struct platform_device *pdev)
{
    struct mmc_host *mmc = platform_get_drvdata(pdev);
    struct rk29_sdmmc *host = mmc_priv(mmc);
    int ret = 0;

    if(host && host->pdev && (RK29_CTRL_SDMMC_ID == host->host_dev_id) )
    {
        if (mmc)
        {
            
            #if !defined(CONFIG_SDMMC0_RK29_SDCARD_DET_FROM_GPIO)
            rk29_sdmmc_sdcard_resume(host);	
            #endif
            
    		ret = mmc_resume_host(mmc);
    	}
    }
#if RK_SDMMC_USE_SDIO_SUSPEND_RESUME        
    else if(host && host->pdev && (RK29_CTRL_SDIO1_ID == host->host_dev_id))
    {
        if (mmc)
        {
             	clk_enable(host->clk);
	       clk_enable(clk_get(&pdev->dev, "hclk_mmc"));
                mdelay(20);
    		ret = mmc_resume_host(mmc);
    	}
    } 
#endif // --#if RK_SDMMC_USE_SDIO_SUSPEND_RESUME
    else if(RK29_SDMMC_EMMC_ID == host->host_dev_id)
    {
        if (mmc)
            ret = mmc_resume_host(mmc);
    }

	return ret;
}
#else
#define rk29_sdmmc_suspend	NULL
#define rk29_sdmmc_resume	NULL
#endif

static struct platform_driver rk29_sdmmc_driver = {
	.suspend    = rk29_sdmmc_suspend,
	.resume     = rk29_sdmmc_resume,
	.remove		= __exit_p(rk29_sdmmc_remove),
	.driver		= {
		.name		= "rk29_sdmmc",
	},
};

#if defined(CONFIG_SDMMC2_RK29)
static struct platform_driver rk29_sdmmc_emmc_driver = {
	.suspend    = rk29_sdmmc_suspend,
	.resume     = rk29_sdmmc_resume,
	.remove		= __exit_p(rk29_sdmmc_remove),
	.driver		= {
		.name		= "emmc",
	},
};
#endif
static int __init rk29_sdmmc_init(void)
{
	return platform_driver_probe(&rk29_sdmmc_driver, rk29_sdmmc_probe);
}

static void __exit rk29_sdmmc_exit(void)
{
	platform_driver_unregister(&rk29_sdmmc_driver);
}

#if defined(CONFIG_SDMMC2_RK29)
static int __init rk29_sdmmc_emmc_init(void)
{
	return platform_driver_probe(&rk29_sdmmc_emmc_driver, rk29_sdmmc_probe);
}
static void __exit rk29_sdmmc_emmc_exit(void)
{
	return platform_driver_unregister(&rk29_sdmmc_emmc_driver);
}
#endif

module_init(rk29_sdmmc_init);
module_exit(rk29_sdmmc_exit);

#if defined(CONFIG_SDMMC2_RK29)
fs_initcall(rk29_sdmmc_emmc_init);
module_exit(rk29_sdmmc_emmc_init);
#endif

MODULE_DESCRIPTION("Rk29 Multimedia Card Interface driver");
MODULE_AUTHOR("xbw@rock-chips.com");
MODULE_LICENSE("GPL v2");
 
