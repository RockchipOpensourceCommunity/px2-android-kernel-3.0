/*
 * rk29_i2s.c  --  ALSA SoC ROCKCHIP IIS Audio Layer Platform driver
 *
 * Driver for rockchip iis audio
 *
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/version.h>

#include <asm/dma.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/io.h>

#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/io.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/dma-pl330.h>
#include <linux/spinlock.h>

#include "rk29_pcm.h"
#include "rk29_i2s.h"

#define ANDROID_REC
#if 0
#define I2S_DBG(x...) printk(KERN_INFO x)
#else
#define I2S_DBG(x...) do { } while (0)
#endif

#define pheadi2s  ((pI2S_REG)(i2s->regs))

#define MAX_I2S          3

struct rk29_i2s_info {
	struct device	*dev;
	void __iomem	*regs;
        
	u32     feature;

	struct clk	*iis_clk;
	struct clk	*iis_pclk;

	unsigned char   master;

	struct rockchip_pcm_dma_params  *dma_playback;
	struct rockchip_pcm_dma_params  *dma_capture;

	u32		 suspend_iismod;
	u32		 suspend_iiscon;
	u32		 suspend_iispsr;
	
	bool 	i2s_tx_status;//active = true;
	bool 	i2s_rx_status;
	spinlock_t spinlock_wr;//write read reg spin_lock
};

static struct snd_soc_dai *rk_cpu_dai=NULL;
static struct rk29_dma_client rk29_dma_client_out = {
	.name = "I2S PCM Stereo Out"
};

static struct rk29_dma_client rk29_dma_client_in = {
	.name = "I2S PCM Stereo In"
};

static inline struct rk29_i2s_info *to_info(struct snd_soc_dai *cpu_dai)
{
	return snd_soc_dai_get_drvdata(cpu_dai);
}

static struct rockchip_pcm_dma_params rk29_i2s_pcm_stereo_out[MAX_I2S];
static struct rockchip_pcm_dma_params rk29_i2s_pcm_stereo_in[MAX_I2S];
static struct rk29_i2s_info rk29_i2s[MAX_I2S];

struct snd_soc_dai_driver rk29_i2s_dai[MAX_I2S];
EXPORT_SYMBOL_GPL(rk29_i2s_dai);
#if defined (CONFIG_RK_HDMI) && defined (CONFIG_SND_RK_SOC_HDMI_I2S)
extern int hdmi_get_hotplug(void);
#endif
/* 
 *Turn on or off the transmission path. 
 */
static void rockchip_snd_txctrl(struct rk29_i2s_info *i2s, int on)
{
	u32 opr,xfer,clr;
	spin_lock(&i2s->spinlock_wr);
	opr  = readl(&(pheadi2s->I2S_DMACR));
	xfer = readl(&(pheadi2s->I2S_XFER));
	clr  = readl(&(pheadi2s->I2S_CLR));
	if (on) 
	{         
		I2S_DBG("rockchip_snd_txctrl: on\n");
		if ((opr & I2S_TRAN_DMA_ENABLE) == 0)
		{
			opr  |= I2S_TRAN_DMA_ENABLE;
			writel(opr, &(pheadi2s->I2S_DMACR));	
		}	
		if ((xfer&I2S_TX_TRAN_START)==0 || (xfer&I2S_RX_TRAN_START)==0)
		{		
			xfer |= I2S_TX_TRAN_START;
			xfer |= I2S_RX_TRAN_START;
			writel(xfer, &(pheadi2s->I2S_XFER));
		}
		i2s->i2s_tx_status = true;
		spin_unlock(&i2s->spinlock_wr);
	}
	else
	{
		//stop tx
		i2s->i2s_tx_status = false;
		I2S_DBG("rockchip_snd_txctrl: off\n");
		opr  &= ~I2S_TRAN_DMA_ENABLE;        
		writel(opr, &(pheadi2s->I2S_DMACR));  
		if(!i2s->i2s_tx_status && !i2s->i2s_rx_status//sync stop i2s rx tx lcrk
#if defined (CONFIG_RK_HDMI) && defined (CONFIG_SND_RK_SOC_HDMI_I2S)
			&& 	hdmi_get_hotplug() == 0	//HDMI_HPD_REMOVED
#endif			
		)
		{
			xfer &= ~I2S_TX_TRAN_START;
			xfer &= ~I2S_RX_TRAN_START;		
			writel(xfer, &(pheadi2s->I2S_XFER));	
			clr |= I2S_TX_CLEAR;
			clr |= I2S_RX_CLEAR;
			writel(clr, &(pheadi2s->I2S_CLR));
			spin_unlock(&i2s->spinlock_wr);
			udelay(1);
			I2S_DBG("rockchip_snd_txctrl: stop xfer\n");			
		}
		else
			spin_unlock(&i2s->spinlock_wr);
	}
}

static void rockchip_snd_rxctrl(struct rk29_i2s_info *i2s, int on)
{
	u32 opr,xfer,clr;
	spin_lock(&i2s->spinlock_wr);
	opr  = readl(&(pheadi2s->I2S_DMACR));
	xfer = readl(&(pheadi2s->I2S_XFER));
	clr  = readl(&(pheadi2s->I2S_CLR));
	if (on) 
	{				 
	    I2S_DBG("rockchip_snd_rxctrl: on\n");
		if ((opr & I2S_RECE_DMA_ENABLE) == 0)
		{
			opr  |= I2S_RECE_DMA_ENABLE;
			writel(opr, &(pheadi2s->I2S_DMACR));	
		}
		if ((xfer&I2S_TX_TRAN_START)==0 || (xfer&I2S_RX_TRAN_START)==0)
		{		
			xfer |= I2S_RX_TRAN_START;
			xfer |= I2S_TX_TRAN_START;
			writel(xfer, &(pheadi2s->I2S_XFER));
		}
		i2s->i2s_rx_status = true;
		spin_unlock(&i2s->spinlock_wr);
#ifdef CONFIG_SND_SOC_RT5631
//bard 7-16 s
		schedule_delayed_work(&rt5631_delay_cap,HZ/4);
//bard 7-16 e
#endif
	}
	else
	{
		i2s->i2s_rx_status = false;
		I2S_DBG("rockchip_snd_rxctrl: off\n");
		opr  &= ~I2S_RECE_DMA_ENABLE;
		writel(opr, &(pheadi2s->I2S_DMACR));		
		if(!i2s->i2s_tx_status && !i2s->i2s_rx_status	//sync stop i2s rx tx lcrk
#if defined (CONFIG_RK_HDMI) && defined (CONFIG_SND_RK_SOC_HDMI_I2S)
			&& 	hdmi_get_hotplug() == 0	//HDMI_HPD_REMOVED
#endif			
		)
		{		
			xfer &= ~I2S_RX_TRAN_START;
			xfer &= ~I2S_TX_TRAN_START;		
			writel(xfer, &(pheadi2s->I2S_XFER));		
			clr |= I2S_RX_CLEAR;
			clr |= I2S_TX_CLEAR;
			writel(clr, &(pheadi2s->I2S_CLR));
			spin_unlock(&i2s->spinlock_wr);
			udelay(1);
			I2S_DBG("rockchip_snd_rxctrl: stop xfer\n");				
		}
		else
			spin_unlock(&i2s->spinlock_wr);
	}
}

/*
 * Set Rockchip I2S DAI format
 */
static int rockchip_i2s_set_fmt(struct snd_soc_dai *cpu_dai,
						unsigned int fmt)
{
	struct rk29_i2s_info *i2s = to_info(cpu_dai);	
	u32 tx_ctl,rx_ctl;
	u32 iis_ckr_value;//clock generation register
	
	I2S_DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
    spin_lock(&i2s->spinlock_wr);
	tx_ctl = readl(&(pheadi2s->I2S_TXCR));
	iis_ckr_value = readl(&(pheadi2s->I2S_CKR));
	
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFM:  	
			iis_ckr_value &= ~I2S_MODE_MASK;  
			iis_ckr_value |= I2S_MASTER_MODE;
			break;
		case SND_SOC_DAIFMT_CBS_CFS:
			iis_ckr_value &= ~I2S_MODE_MASK;   
			iis_ckr_value |= I2S_SLAVE_MODE;
			break;
		default:
			I2S_DBG("unknwon master/slave format\n");
			return -EINVAL;
	}       
	writel(iis_ckr_value, &(pheadi2s->I2S_CKR));
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_RIGHT_J:
			tx_ctl &= ~I2S_BUS_MODE_MASK;    //I2S Bus Mode
			tx_ctl |= I2S_BUS_MODE_RSJM;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			tx_ctl &= ~I2S_BUS_MODE_MASK;    //I2S Bus Mode
			tx_ctl |= I2S_BUS_MODE_LSJM;
			break;
		case SND_SOC_DAIFMT_I2S:
			tx_ctl &= ~I2S_BUS_MODE_MASK;    //I2S Bus Mode
			tx_ctl |= I2S_BUS_MODE_NOR;
			break;
		default:
			I2S_DBG("Unknown data format\n");
			return -EINVAL;
	}
	I2S_DBG("Enter::%s----%d, I2S_TXCR=0x%X\n",__FUNCTION__,__LINE__,tx_ctl);

	writel(tx_ctl, &(pheadi2s->I2S_TXCR));

	rx_ctl = tx_ctl & 0x00007FFF;
	writel(rx_ctl, &(pheadi2s->I2S_RXCR));
    spin_unlock(&i2s->spinlock_wr);
	return 0;
}

static int rockchip_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params, struct snd_soc_dai *socdai)
{
	struct rk29_i2s_info *i2s = to_info(socdai);
	u32 iismod;
	u32 dmarc;
	u32 iis_ckr_value;//clock generation register
		
	I2S_DBG("Enter %s, %d >>>>>>>>>>>\n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_dai_set_dma_data(socdai, substream, i2s->dma_playback);
	else
		snd_soc_dai_set_dma_data(socdai, substream, i2s->dma_capture);

	/* Working copies of register */
    spin_lock(&i2s->spinlock_wr);
	iismod = readl(&(pheadi2s->I2S_TXCR));
	
	iismod &= (~((1<<5)-1));
	switch (params_format(params)) {
        case SNDRV_PCM_FORMAT_S8:
        	iismod |= SAMPLE_DATA_8bit;
        	break;
        case SNDRV_PCM_FORMAT_S16_LE:
        	iismod |= I2S_DATA_WIDTH(15);
        	break;
        case SNDRV_PCM_FORMAT_S20_3LE:
			iismod |= I2S_DATA_WIDTH(19);
			break;
        case SNDRV_PCM_FORMAT_S24_LE:
			iismod |= I2S_DATA_WIDTH(23);
			break;
        case SNDRV_PCM_FORMAT_S32_LE:
			iismod |= I2S_DATA_WIDTH(31);
			break;
	}
	
	iis_ckr_value = readl(&(pheadi2s->I2S_CKR));
	#if defined (CONFIG_SND_RK29_CODEC_SOC_SLAVE) 
	iis_ckr_value &= ~I2S_SLAVE_MODE;
	#endif
	#if defined (CONFIG_SND_RK29_CODEC_SOC_MASTER) 
	iis_ckr_value |= I2S_SLAVE_MODE;
	#endif
	writel(iis_ckr_value, &(pheadi2s->I2S_CKR));   
	
//	writel((16<<24) |(16<<18)|(16<<12)|(16<<6)|16, &(pheadi2s->I2S_FIFOLR));
	dmarc = readl(&(pheadi2s->I2S_DMACR));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dmarc = ((dmarc & 0xFFFFFE00) | 16);
	else
		dmarc = ((dmarc & 0xFE00FFFF) | 16<<16);

	writel(dmarc, &(pheadi2s->I2S_DMACR));
	I2S_DBG("Enter %s, %d I2S_TXCR=0x%08X\n", __func__, __LINE__, iismod);  

	writel(iismod, &(pheadi2s->I2S_TXCR));

	iismod = iismod & 0x00007FFF;
	writel(iismod, &(pheadi2s->I2S_RXCR));   
    spin_unlock(&i2s->spinlock_wr);
	return 0;
}

static int rockchip_i2s_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{    
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct rk29_i2s_info *i2s = to_info(rtd->cpu_dai);

	I2S_DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
	switch (cmd) {
        case SNDRV_PCM_TRIGGER_START:
        case SNDRV_PCM_TRIGGER_RESUME:
        case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:   
                if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
	                rockchip_snd_rxctrl(i2s, 1);
                else
	                rockchip_snd_txctrl(i2s, 1);
                break;
        
        case SNDRV_PCM_TRIGGER_SUSPEND:
        case SNDRV_PCM_TRIGGER_STOP:
        case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
                if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
	                rockchip_snd_rxctrl(i2s, 0);
                else
	                rockchip_snd_txctrl(i2s, 0);
                break;
        default:
                ret = -EINVAL;
                break;
	}

	return ret;
}

/*
 * Set Rockchip I2S MCLK source
 */
static int rockchip_i2s_set_sysclk(struct snd_soc_dai *cpu_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct rk29_i2s_info *i2s;        

	i2s = to_info(cpu_dai);
        
	I2S_DBG("Enter:%s, %d, i2s=0x%p, freq=%d\n", __FUNCTION__, __LINE__, i2s, freq);
	/*add scu clk source and enable clk*/
	clk_set_rate(i2s->iis_clk, freq);
	return 0;
}

/*
 * Set Rockchip Clock dividers
 */
static int rockchip_i2s_set_clkdiv(struct snd_soc_dai *cpu_dai,
	int div_id, int div)
{
	struct rk29_i2s_info *i2s;
	u32 reg;

	i2s = to_info(cpu_dai);
        
	//stereo mode MCLK/SCK=4  
    spin_lock(&i2s->spinlock_wr);
	reg = readl(&(pheadi2s->I2S_CKR));

	I2S_DBG("Enter:%s, %d, div_id=0x%08X, div=0x%08X\n", __FUNCTION__, __LINE__, div_id, div);
        
	//when i2s in master mode ,must set codec pll div
	switch (div_id) {
        case ROCKCHIP_DIV_BCLK:
            reg &= ~I2S_TX_SCLK_DIV_MASK;
            reg |= I2S_TX_SCLK_DIV(div);
            reg &= ~I2S_RX_SCLK_DIV_MASK;
            reg |= I2S_RX_SCLK_DIV(div);			
            break;
        case ROCKCHIP_DIV_MCLK:
            reg &= ~I2S_MCLK_DIV_MASK;
            reg |= I2S_MCLK_DIV(div);
            break;
        case ROCKCHIP_DIV_PRESCALER:
            break;
        default:
			return -EINVAL;
	}
	writel(reg, &(pheadi2s->I2S_CKR));
    spin_unlock(&i2s->spinlock_wr);
	return 0;
}

static int i2s_set_gpio_mode(struct snd_soc_dai *dai)
{	
	I2S_DBG("Enter %s, %d >>>>>>>>>>>\n", __func__, __LINE__);
    switch(dai->id) {
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
        case 1:
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_MCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_LRCKRX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_LRCKTX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDI));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDO));
            break;
#elif defined(CONFIG_ARCH_RKPX2)
        case 0:
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_MCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_LRCKRX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_LRCKTX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDI));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDO0));
            #ifdef CONFIG_SND_I2SO_USE_EIGHT_CHANNELS
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDO1));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDO2));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDO3));
            #endif
			break;
        case 1:
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S1_MCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S1_SCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S1_LRCKRX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S1_LRCKTX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S1_SDI));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S1_SDO));
			break;
        case 2:
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S2_MCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S2_SCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S2_LRCKRX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S2_LRCKTX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S2_SDI));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S2_SDO));
            break;
#endif
#if  defined(CONFIG_ARCH_RK2928) || defined(CONFIG_ARCH_RK3026)
        case 0:
        #if 0 //iomux --> gps(.ko)
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_MCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SCLK));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_LRCKRX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_LRCKTX));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDI));
            iomux_set_gpio_mode(iomux_mode_to_gpio(I2S0_SDO));
        #endif
        break;
#endif
        default:
            I2S_DBG("Enter:%s, %d, Error For DevId!!!", __FUNCTION__, __LINE__);
            return -EINVAL;
    }
	return 0;
}

static int rockchip_i2s_dai_probe(struct snd_soc_dai *dai)
{	
	I2S_DBG("Enter %s, %d >>>>>>>>>>>\n", __func__, __LINE__);
    if(rk_cpu_dai == NULL)
        rk_cpu_dai = dai;
    switch(dai->id) {
#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
        case 1:
            iomux_set(I2S0_MCLK);
            iomux_set(I2S0_SCLK);
            iomux_set(I2S0_LRCKRX);
            iomux_set(I2S0_LRCKTX);
            iomux_set(I2S0_SDI);
            iomux_set(I2S0_SDO);
            break;
#elif defined(CONFIG_ARCH_RKPX2)
        case 0:
			rk30_mux_api_set(GPIO0A7_I2S8CHSDI_NAME, GPIO0A_I2S_8CH_SDI);		
			rk30_mux_api_set(GPIO0B0_I2S8CHCLK_NAME, GPIO0B_I2S_8CH_CLK);                
			rk30_mux_api_set(GPIO0B1_I2S8CHSCLK_NAME, GPIO0B_I2S_8CH_SCLK);
			rk30_mux_api_set(GPIO0B2_I2S8CHLRCKRX_NAME, GPIO0B_I2S_8CH_LRCK_RX);
			rk30_mux_api_set(GPIO0B3_I2S8CHLRCKTX_NAME, GPIO0B_I2S_8CH_LRCK_TX);	
			rk30_mux_api_set(GPIO0B4_I2S8CHSDO0_NAME, GPIO0B_I2S_8CH_SDO0);
			#ifdef CONFIG_SND_I2SO_USE_EIGHT_CHANNELS			
			rk30_mux_api_set(GPIO0B5_I2S8CHSDO1_NAME, GPIO0B_I2S_8CH_SDO1);
			rk30_mux_api_set(GPIO0B6_I2S8CHSDO2_NAME, GPIO0B_I2S_8CH_SDO2);
			rk30_mux_api_set(GPIO0B7_I2S8CHSDO3_NAME, GPIO0B_I2S_8CH_SDO3);  
			#endif			
			break;
        case 1:
			rk30_mux_api_set(GPIO0C0_I2S12CHCLK_NAME, GPIO0C_I2S1_2CH_CLK);
			rk30_mux_api_set(GPIO0C1_I2S12CHSCLK_NAME, GPIO0C_I2S1_2CH_SCLK);
			rk30_mux_api_set(GPIO0C2_I2S12CHLRCKRX_NAME, GPIO0C_I2S1_2CH_LRCK_RX);
			rk30_mux_api_set(GPIO0C3_I2S12CHLRCKTX_NAME, GPIO0C_I2S1_2CH_LRCK_TX);				
			rk30_mux_api_set(GPIO0C4_I2S12CHSDI_NAME, GPIO0C_I2S1_2CH_SDI);
			rk30_mux_api_set(GPIO0C5_I2S12CHSDO_NAME, GPIO0C_I2S1_2CH_SDO);
			break;
        case 2:
			rk30_mux_api_set(GPIO0D0_I2S22CHCLK_SMCCSN0_NAME, GPIO0D_I2S2_2CH_CLK);
			rk30_mux_api_set(GPIO0D1_I2S22CHSCLK_SMCWEN_NAME, GPIO0D_I2S2_2CH_SCLK);
			rk30_mux_api_set(GPIO0D2_I2S22CHLRCKRX_SMCOEN_NAME, GPIO0D_I2S2_2CH_LRCK_RX);
			rk30_mux_api_set(GPIO0D3_I2S22CHLRCKTX_SMCADVN_NAME, GPIO0D_I2S2_2CH_LRCK_TX);				
			rk30_mux_api_set(GPIO0D4_I2S22CHSDI_SMCADDR0_NAME, GPIO0D_I2S2_2CH_SDI);
			rk30_mux_api_set(GPIO0D5_I2S22CHSDO_SMCADDR1_NAME, GPIO0D_I2S2_2CH_SDO);
			break;
#endif
#if  defined(CONFIG_ARCH_RK2928) || defined(CONFIG_ARCH_RK3026)
        case 0:
        #if 0 //iomux --> gps(.ko)
                rk30_mux_api_set(GPIO1A0_I2S_MCLK_NAME, GPIO1A_I2S_MCLK);
                rk30_mux_api_set(GPIO1A1_I2S_SCLK_NAME, GPIO1A_I2S_SCLK);
                rk30_mux_api_set(GPIO1A2_I2S_LRCKRX_GPS_CLK_NAME, GPIO1A_I2S_LRCKRX);
                rk30_mux_api_set(GPIO1A3_I2S_LRCKTX_NAME, GPIO1A_I2S_LRCKTX);
                rk30_mux_api_set(GPIO1A4_I2S_SDO_GPS_MAG_NAME, GPIO1A_I2S_SDO);
                rk30_mux_api_set(GPIO1A5_I2S_SDI_GPS_SIGN_NAME, GPIO1A_I2S_SDI);
        #endif
		break;
#endif
        default:
            I2S_DBG("Enter:%s, %d, Error For DevId!!!", __FUNCTION__, __LINE__);
            return -EINVAL;
    }
	return 0;
}

#ifdef CONFIG_PM
int rockchip_i2s_suspend(struct snd_soc_dai *cpu_dai)
{
	I2S_DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
//	clk_disable(clk);
	return 0;
}

int rockchip_i2s_resume(struct snd_soc_dai *cpu_dai)
{
	I2S_DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
//	clk_enable(clk);
	return 0;
}
#else
#define rockchip_i2s_suspend NULL
#define rockchip_i2s_resume NULL
#endif

#ifdef ANDROID_REC
#define ROCKCHIP_I2S_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#else
#define ROCKCHIP_I2S_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		            SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		            SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#endif

static struct snd_soc_dai_ops rockchip_i2s_dai_ops = {
	.trigger = rockchip_i2s_trigger,
	.hw_params = rockchip_i2s_hw_params,
	.set_fmt = rockchip_i2s_set_fmt,
	.set_clkdiv = rockchip_i2s_set_clkdiv,
	.set_sysclk = rockchip_i2s_set_sysclk,
};

static int rk29_i2s_probe(struct platform_device *pdev,
			  struct snd_soc_dai_driver *dai,
			  struct rk29_i2s_info *i2s,
			  unsigned long base)
{
	struct device *dev = &pdev->dev;
	struct resource *res;

	I2S_DBG("Enter %s, %d >>>>>>>>>>>\n", __func__, __LINE__);

	i2s->dev = dev;

	/* record our i2s structure for later use in the callbacks */
	dev_set_drvdata(&pdev->dev, i2s);

	if (!base) {
		res = platform_get_resource(pdev,
					     IORESOURCE_MEM,
					     0);
		if (!res) {
			dev_err(dev, "Unable to get register resource\n");
			return -ENXIO;
		}

		if (!request_mem_region(res->start, resource_size(res),
					"rk29_i2s")) {
			dev_err(dev, "Unable to request register region\n");
			return -EBUSY;
		}

		base = res->start;
	}

	i2s->regs = ioremap(base, (res->end - res->start) + 1); ////res));
	if (i2s->regs == NULL) {
		dev_err(dev, "cannot ioremap registers\n");
		return -ENXIO;
	}

	i2s->iis_pclk = clk_get(dev, "hclk_i2s");
	if (IS_ERR(i2s->iis_pclk)) {
		dev_err(dev, "failed to get iis_clock\n");
		iounmap(i2s->regs);
		return -ENOENT;
	}
	clk_enable(i2s->iis_pclk);


	/* Mark ourselves as in TXRX mode so we can run through our cleanup
	 * process without warnings. */
	rockchip_snd_txctrl(i2s, 0);
	rockchip_snd_rxctrl(i2s, 0);

	return 0;
}

static int __devinit rockchip_i2s_probe(struct platform_device *pdev)
{
	struct rk29_i2s_info *i2s;
	struct snd_soc_dai_driver *dai;
	int    ret;

#if defined(CONFIG_SND_I2S_USE_18V)	
	writel_relaxed(0x2000200,RK30_GRF_BASE + GRF_IO_CON4);//bit9: 1,1.8v;0,3.3v
#elif defined(CONFIG_SND_I2S_USE_33V)
	writel_relaxed(0x2000000,RK30_GRF_BASE + GRF_IO_CON4);
#endif

#if defined(CONFIG_ARCH_RK3066B) || defined(CONFIG_ARCH_RK3188)
	//default 8ma  0xF000F = 12ma 0xF0005=4ma 0xF0000=2ma
	writel_relaxed(0xF000A,RK30_GRF_BASE + GRF_IO_CON1);
#endif	
	I2S_DBG("Enter %s, %d pdev->id = %d >>>>>>>>>>>\n", __func__, __LINE__, pdev->id);
	
	if(pdev->id >= MAX_I2S) {
		dev_err(&pdev->dev, "id %d out of range\n", pdev->id);
		return -EINVAL;        
	}

	i2s = &rk29_i2s[pdev->id];
	dai = &rk29_i2s_dai[pdev->id];
	dai->id = pdev->id;
	dai->symmetric_rates = 1;
	
	switch(pdev->id)
	{
	case 0:
		dai->name = "rk29_i2s.0";
		dai->playback.channels_min = 2;
		dai->playback.channels_max = 8;
		break;
	case 1:
		dai->name = "rk29_i2s.1";
		dai->playback.channels_min = 2;
		dai->playback.channels_max = 2;	
		break;
	case 2:
		dai->name = "rk29_i2s.2";
		dai->playback.channels_min = 2;
		dai->playback.channels_max = 2;			
		break;
	}	

	spin_lock_init(&i2s->spinlock_wr);
	dai->playback.rates = SNDRV_PCM_RATE_8000_192000;
	dai->playback.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |
		SNDRV_PCM_FMTBIT_S24_LE| SNDRV_PCM_FMTBIT_S32_LE;
	dai->capture.channels_min = 2;
	dai->capture.channels_max = 2;
	dai->capture.rates = ROCKCHIP_I2S_RATES;
	dai->capture.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE;
	dai->probe = rockchip_i2s_dai_probe; 
	dai->ops = &rockchip_i2s_dai_ops;
	dai->suspend = rockchip_i2s_suspend;
	dai->resume = rockchip_i2s_resume;

	i2s->dma_capture = &rk29_i2s_pcm_stereo_in[pdev->id];
	i2s->dma_playback = &rk29_i2s_pcm_stereo_out[pdev->id];
	
	switch(pdev->id)
	{
#ifdef CONFIG_ARCH_RKPX2
	case 0:
		i2s->dma_capture->channel = DMACH_I2S0_8CH_RX;
		i2s->dma_capture->dma_addr = RK30_I2S0_8CH_PHYS + I2S_RXR_BUFF;
		i2s->dma_playback->channel = DMACH_I2S0_8CH_TX;
		i2s->dma_playback->dma_addr = RK30_I2S0_8CH_PHYS + I2S_TXR_BUFF;		
		break;
	case 1:
		i2s->dma_capture->channel = DMACH_I2S1_2CH_RX;
		i2s->dma_capture->dma_addr = RK30_I2S1_2CH_PHYS + I2S_RXR_BUFF;
		i2s->dma_playback->channel = DMACH_I2S1_2CH_TX;
		i2s->dma_playback->dma_addr = RK30_I2S1_2CH_PHYS + I2S_TXR_BUFF;		
		break;
	case 2:
		i2s->dma_capture->channel = DMACH_I2S2_2CH_RX;
		i2s->dma_capture->dma_addr = RK30_I2S2_2CH_PHYS + I2S_RXR_BUFF;
		i2s->dma_playback->channel = DMACH_I2S2_2CH_TX;
		i2s->dma_playback->dma_addr = RK30_I2S2_2CH_PHYS + I2S_TXR_BUFF;	
		break;
#endif
#if defined(CONFIG_ARCH_RK3188)
	case 1:
		i2s->dma_capture->channel = DMACH_I2S1_2CH_RX;
		i2s->dma_capture->dma_addr = RK30_I2S1_2CH_PHYS + I2S_RXR_BUFF;
		i2s->dma_playback->channel = DMACH_I2S1_2CH_TX;
		i2s->dma_playback->dma_addr = RK30_I2S1_2CH_PHYS + I2S_TXR_BUFF;
		break;
#endif
#if defined(CONFIG_ARCH_RK2928) || defined(CONFIG_ARCH_RK3026)
	case 0:
		i2s->dma_capture->channel = DMACH_I2S0_8CH_RX;
		i2s->dma_capture->dma_addr = RK2928_I2S_PHYS + I2S_RXR_BUFF;
		i2s->dma_playback->channel = DMACH_I2S0_8CH_TX;
		i2s->dma_playback->dma_addr = RK2928_I2S_PHYS + I2S_TXR_BUFF;		
		break;
#endif
	}

	i2s->dma_capture->client = &rk29_dma_client_in;
	i2s->dma_capture->dma_size = 4;
	i2s->dma_capture->flag = 0;			//add by sxj, used for burst change
	i2s->dma_playback->client = &rk29_dma_client_out;
	i2s->dma_playback->dma_size = 4;
	i2s->dma_playback->flag = 0;			//add by sxj, used for burst change
	i2s->i2s_tx_status = false;	
	i2s->i2s_rx_status = false;	
#ifdef CONFIG_SND_I2S_DMA_EVENT_STATIC
	 WARN_ON(rk29_dma_request(i2s->dma_playback->channel, i2s->dma_playback->client, NULL));
	 WARN_ON(rk29_dma_request(i2s->dma_capture->channel, i2s->dma_capture->client, NULL));
#endif

	i2s->iis_clk = clk_get(&pdev->dev, "i2s");
	I2S_DBG("Enter:%s, %d, iis_clk=%p\n", __FUNCTION__, __LINE__, i2s->iis_clk);
	if (IS_ERR(i2s->iis_clk)) {
		dev_err(&pdev->dev, "failed to get i2s clk\n");
		ret = PTR_ERR(i2s->iis_clk);
		goto err;
	}

	clk_enable(i2s->iis_clk);
	clk_set_rate(i2s->iis_clk, 11289600);

	ret = rk29_i2s_probe(pdev, dai, i2s, 0);
	if (ret)
		goto err_clk;

	ret = snd_soc_register_dai(&pdev->dev, dai);
	if (ret != 0)
		goto err_i2sv2;

	return 0;

err_i2sv2:
	/* Not implemented for I2Sv2 core yet */
err_clk:
	clk_put(i2s->iis_clk);
err:
	return ret;
}

static int rockchip_i2s_suspend_noirq(struct device *dev)
{
    struct snd_soc_dai *dai = rk_cpu_dai;
    I2S_DBG("Enter %s, %d\n", __func__, __LINE__);

	return i2s_set_gpio_mode(dai);
}

static int rockchip_i2s_resume_noirq(struct device *dev)
{
    struct snd_soc_dai *dai = rk_cpu_dai;
    I2S_DBG("Enter %s, %d\n", __func__, __LINE__);

	return rockchip_i2s_dai_probe(dai);
}

static const struct dev_pm_ops rockchip_i2s_pm_ops = {
	.suspend_noirq = rockchip_i2s_suspend_noirq,
	.resume_noirq = rockchip_i2s_resume_noirq,
};

static int __devexit rockchip_i2s_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver rockchip_i2s_driver = {
	.probe  = rockchip_i2s_probe,
	.remove = __devexit_p(rockchip_i2s_remove),
	.driver = {
		.name   = "rk29_i2s",
		.owner  = THIS_MODULE,
		.pm	= &rockchip_i2s_pm_ops,
	},
};

static int __init rockchip_i2s_init(void)
{
	I2S_DBG("Enter %s, %d >>>>>>>>>>>\n", __func__, __LINE__);
	
	return  platform_driver_register(&rockchip_i2s_driver);
}
module_init(rockchip_i2s_init);

static void __exit rockchip_i2s_exit(void)
{
	platform_driver_unregister(&rockchip_i2s_driver);
}
module_exit(rockchip_i2s_exit);

/* Module information */
MODULE_AUTHOR("rockchip");
MODULE_DESCRIPTION("ROCKCHIP IIS ASoC Interface");
MODULE_LICENSE("GPL");


#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
static int proc_i2s_show(struct seq_file *s, void *v)
{
#ifdef CONFIG_SND_RK29_SOC_I2S_8CH
	struct rk29_i2s_info *i2s=&rk29_i2s[0];
#else 
#ifdef CONFIG_SND_RK29_SOC_I2S_2CH
	struct rk29_i2s_info *i2s=&rk29_i2s[1];
#else
	struct rk29_i2s_info *i2s=&rk29_i2s[2];
#endif
#endif
	printk("========Show I2S reg========\n");
        
	printk("I2S_TXCR = 0x%08X\n", readl(&(pheadi2s->I2S_TXCR)));
	printk("I2S_RXCR = 0x%08X\n", readl(&(pheadi2s->I2S_RXCR)));
	printk("I2S_CKR = 0x%08X\n", readl(&(pheadi2s->I2S_CKR)));
	printk("I2S_DMACR = 0x%08X\n", readl(&(pheadi2s->I2S_DMACR)));
	printk("I2S_INTCR = 0x%08X\n", readl(&(pheadi2s->I2S_INTCR)));
	printk("I2S_INTSR = 0x%08X\n", readl(&(pheadi2s->I2S_INTSR)));
	printk("I2S_XFER = 0x%08X\n", readl(&(pheadi2s->I2S_XFER)));

	printk("========Show I2S reg========\n");
#if 0
		writel(0x0000000F, &(pheadi2s->I2S_TXCR));
		writel(0x0000000F, &(pheadi2s->I2S_RXCR));
		writel(0x00071f1F, &(pheadi2s->I2S_CKR));
		writel(0x001F0110, &(pheadi2s->I2S_DMACR));
		writel(0x00000003, &(pheadi2s->I2S_XFER));
		while(1)
		{
			writel(0x5555aaaa, &(pheadi2s->I2S_TXDR));
		}		
#endif	
	return 0;
}

static ssize_t i2s_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
#ifdef CONFIG_SND_RK29_SOC_I2S_8CH
	struct rk29_i2s_info *i2s=&rk29_i2s[0];
#else 
#ifdef CONFIG_SND_RK29_SOC_I2S_2CH
	struct rk29_i2s_info *i2s=&rk29_i2s[1];
#else
	struct rk29_i2s_info *i2s=&rk29_i2s[2];
#endif
#endif
	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long value;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	while (*start == ' ')
		start++;
	value = simple_strtoul(start, &start, 10);

	printk("test --- freq = %ld ret=%d\n",value,clk_set_rate(i2s->iis_clk, value));
	return buf_size;
}

static int proc_i2s_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_i2s_show, NULL);
}

static const struct file_operations proc_i2s_fops = {
	.open		= proc_i2s_open,
	.read		= seq_read,
	.write = i2s_reg_write,	
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init i2s_proc_init(void)
{
	proc_create("i2s_reg", 0, NULL, &proc_i2s_fops);
	return 0;
}
late_initcall(i2s_proc_init);
#endif /* CONFIG_PROC_FS */

