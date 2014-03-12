/*
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
/* Rock-chips rfkill driver for bluetooth
 *
 * BT Դ����ԭ����arch/arm/mach-rkxx/board-xxx-rfkill.c�����˴�
 * GPIO�������� arch/arm/mach-rkxx/board-rkxx-sdk.c ��rfkill_rk_platdata�ṹ����
 *
 * �����ʵ��������ú������ĸ���GPIO���ţ���������˵������:
    .xxxx_gpio   = {
      .io         = -1,      // GPIOֵ�� -1 ��ʾ�޴˹���
      .enable     = -1,      // ʹ��, GPIO_HIGH - ��ʹ�ܣ� GPIO_LOW - ��ʹ��
      .iomux      = {
          .name    = NULL,   // IOMUX���ƣ�NULL ��ʾ���ǵ����ܣ�����Ҫmux
          .fgpio   = -1,     // ����ΪGPIOʱ��Ӧ���õ�ֵ
          .fmux   = -1,      // ����Ϊ���ù���ʱ��Ӧ���õ�ֵ
      },
    },
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/gpio.h>
#include <mach/iomux.h>
#include <linux/delay.h>
#include <linux/rfkill-rk.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#if 0
#define DBG(x...)   printk(KERN_INFO "[BT_RFKILL]: "x)
#else
#define DBG(x...)
#endif

#define LOG(x...)   printk(KERN_INFO "[BT_RFKILL]: "x)

#define BT_WAKEUP_TIMEOUT           10000
#define BT_IRQ_WAKELOCK_TIMEOUT     10*1000

#define BT_BLOCKED     true
#define BT_UNBLOCK     false
#define BT_SLEEP       true
#define BT_WAKEUP      false

enum {
    IOMUX_FNORMAL=0,
    IOMUX_FGPIO,
    IOMUX_FMUX,
};

    #define rk_mux_api_set(name,mode)      rk30_mux_api_set(name,mode)

// RK29+BCM4329, ��wifi��bt��power���ƽ��ǽ���һ���
// �ڸ�bt�µ�ʱ����Ҫ���ж�wifi״̬
#if defined(CONFIG_BCM4329)
#define WIFI_BT_POWER_TOGGLE    1
extern int rk29sdk_bt_power_state;
extern int rk29sdk_wifi_power_state;
#else
#define WIFI_BT_POWER_TOGGLE    0
#endif

struct rfkill_rk_data {
    // ָ��board�ж����platform data
	struct rfkill_rk_platform_data	*pdata;

    // ָ��rfkill�豸������rfkill_alloc����
	struct rfkill				*rfkill_dev;

    // ��IRQ�жϺ�����ʹ�ã�ȷ����BT����AP�󣬲�������
    // ����suspend����˯��
    struct wake_lock            bt_irq_wl;
    
    // �ڻ���BT��ͨ����delay work��ʱһ��ʱ������˯��
    // ���delay work��δִ�о�����һ�� BT wake�������cancel
    // ���е� delay work�������¼�ʱ��
    struct delayed_work         bt_sleep_delay_work;
};

static struct rfkill_rk_data *g_rfkill = NULL;

static const char bt_name[] = 
#if defined (CONFIG_BCM4330)
    #if defined (CONFIG_BT_MODULE_NH660)
        "nh660"
    #else
        "bcm4330"
    #endif
#elif defined (CONFIG_RK903)
    #if defined(CONFIG_RKWIFI_26M)
        "rk903_26M"
    #else
        "rk903"
    #endif
#elif defined(CONFIG_BCM4329)
        "bcm4329"
#elif defined(CONFIG_MV8787)
        "mv8787"
#elif defined(CONFIG_AP6210)
    #if defined(CONFIG_RKWIFI_26M)
        "ap6210"
    #else
        "ap6210_24M"
    #endif
#elif defined(CONFIG_AP6330)
		"ap6330"
#elif defined(CONFIG_AP6476)
		"ap6476"
#elif defined(CONFIG_AP6493)
		"ap6493"
#else
        "bt_default"
#endif
;

/*
 *  rfkill_rk_wake_host_irq - BT_WAKE_HOST �жϻص�����
 *      ����һ��wakelock����ȷ���˴λ��Ѳ������Ͻ���
 */
static irqreturn_t rfkill_rk_wake_host_irq(int irq, void *dev)
{
    struct rfkill_rk_data *rfkill = dev;
    LOG("BT_WAKE_HOST IRQ fired\n");
    
    DBG("BT IRQ wakeup, request %dms wakelock\n", BT_IRQ_WAKELOCK_TIMEOUT);

    wake_lock_timeout(&rfkill->bt_irq_wl, 
                    msecs_to_jiffies(BT_IRQ_WAKELOCK_TIMEOUT));
    
	return IRQ_HANDLED;
}

/*
 * rfkill_rk_setup_gpio - ����GPIO
 *      gpio    - Ҫ���õ�GPIO
 *      mux     - iomux ʲô����
 *      prefix,name - ��ɸ�IO������
 * ����ֵ
 *      ����ֵ�� gpio_request ��ͬ
 */
static int rfkill_rk_setup_gpio(struct rfkill_rk_gpio* gpio, int mux, const char* prefix, const char* name)
{
	if (gpio_is_valid(gpio->io)) {
        int ret=0;
        sprintf(gpio->name, "%s_%s", prefix, name);
		ret = gpio_request(gpio->io, gpio->name);
		if (ret) {
			LOG("Failed to get %s gpio.\n", gpio->name);
			return -1;
		}
        if (gpio->iomux.name)
        {
            if (mux==IOMUX_FGPIO)
                rk_mux_api_set(gpio->iomux.name, gpio->iomux.fgpio);
            else if (mux==IOMUX_FMUX)
                rk_mux_api_set(gpio->iomux.name, gpio->iomux.fmux);
            else
                ;// do nothing
        }
	}

    return 0;
}

// ���� BT_WAKE_HOST IRQ
static int rfkill_rk_setup_wake_irq(struct rfkill_rk_data* rfkill)
{
    int ret=0;
    struct rfkill_rk_irq* irq = &(rfkill->pdata->wake_host_irq);
    
#ifndef CONFIG_BK3515A_COMBO
    ret = rfkill_rk_setup_gpio(&irq->gpio, IOMUX_FGPIO, rfkill->pdata->name, "wake_host");
    if (ret) goto fail1;
#endif

    if (gpio_is_valid(irq->gpio.io))
    {
#ifndef CONFIG_BK3515A_COMBO
        ret = gpio_pull_updown(irq->gpio.io, (irq->gpio.enable==GPIO_LOW)?GPIOPullUp:GPIOPullDown);
        if (ret) goto fail2;
#endif
        LOG("Request irq for bt wakeup host\n");
        irq->irq = gpio_to_irq(irq->gpio.io);
        sprintf(irq->name, "%s_irq", irq->gpio.name);
        ret = request_irq(irq->irq,
                    rfkill_rk_wake_host_irq,
                    (irq->gpio.enable==GPIO_LOW)?IRQF_TRIGGER_FALLING:IRQF_TRIGGER_RISING,
                    irq->name,
                    rfkill);
        if (ret) goto fail2;
        LOG("** disable irq\n");
        disable_irq(irq->irq);
        ret = enable_irq_wake(irq->irq);
        if (ret) goto fail3;
    }

    return ret;

fail3:
    free_irq(irq->gpio.io, rfkill);
fail2:
    gpio_free(irq->gpio.io);
fail1:
    return ret;
}

static inline void rfkill_rk_sleep_bt_internal(struct rfkill_rk_data *rfkill, bool sleep)
{
    struct rfkill_rk_gpio *wake = &rfkill->pdata->wake_gpio;
    
    DBG("*** bt sleep: %d ***\n", sleep);
#ifndef CONFIG_BK3515A_COMBO
    gpio_direction_output(wake->io, sleep?!wake->enable:wake->enable);
#else
    if(!sleep)
    {
        DBG("HOST_UART0_TX pull down 10us\n");
        if (rfkill_rk_setup_gpio(wake, IOMUX_FGPIO, rfkill->pdata->name, "wake") != 0) {
            return;
        }

        gpio_direction_output(wake->io, wake->enable);
        udelay(10);
        gpio_direction_output(wake->io, !wake->enable);

        rk_mux_api_set(wake->iomux.name, wake->iomux.fmux);
        gpio_free(wake->io);
    }
#endif
}

static void rfkill_rk_delay_sleep_bt(struct work_struct *work)
{
    struct rfkill_rk_data *rfkill = NULL;
    DBG("Enter %s\n",__FUNCTION__);

    rfkill = container_of(work, struct rfkill_rk_data, bt_sleep_delay_work.work);

    rfkill_rk_sleep_bt_internal(rfkill, BT_SLEEP);
}

/*
 *  rfkill_rk_sleep_bt - Sleep or Wakeup BT
 *      1 �ڸ�BT�ϵ�ʱ�򣬵��øú�������BT
 *      2 ��suspend����ʱ�򣬵��øú���˯��BT
 *      3 ��HCI�����У���������͸�BTʱ�����øú�������BT
 *
 *  ��������ָ����Ƿ����������ϵ�֮�����1��3����ͬʱ���øú���
 *  ����ָ������û��㷢��2��3����ͬʱ���øú���
 */
void rfkill_rk_sleep_bt(bool sleep)
{
    struct rfkill_rk_data *rfkill = g_rfkill;
    struct rfkill_rk_gpio *wake;
    bool ret;
    DBG("Enter %s\n",__FUNCTION__);
    
    if (rfkill==NULL)
    {
        LOG("*** RFKILL is empty???\n");
        return;
    }

    wake = &rfkill->pdata->wake_gpio;
    if (!gpio_is_valid(wake->io))
    {
        DBG("*** Not support bt wakeup and sleep\n");
        return;
    }

    // ȡ��delay work�����work����pendding��������ȡ��
    // ���work����running����ȴ�ֱ����work����
    ret = cancel_delayed_work_sync(&rfkill->bt_sleep_delay_work);

    rfkill_rk_sleep_bt_internal(rfkill, sleep);

#ifdef CONFIG_BT_AUTOSLEEP
    if (sleep==BT_WAKEUP)
    {
        // ��������delay work
        schedule_delayed_work(&rfkill->bt_sleep_delay_work, 
                            msecs_to_jiffies(BT_WAKEUP_TIMEOUT));
    }
#endif
}
EXPORT_SYMBOL(rfkill_rk_sleep_bt);

static int rfkill_rk_set_power(void *data, bool blocked)
{
	struct rfkill_rk_data *rfkill = data;
    struct rfkill_rk_gpio *poweron = &rfkill->pdata->poweron_gpio;
    struct rfkill_rk_gpio *reset = &rfkill->pdata->reset_gpio;
    struct rfkill_rk_gpio* rts = &rfkill->pdata->rts_gpio;

    DBG("Enter %s\n", __func__);

    DBG("Set blocked:%d\n", blocked);

	if (false == blocked) { 
        rfkill_rk_sleep_bt(BT_WAKEUP); // ensure bt is wakeup

		if (gpio_is_valid(poweron->io))
        {
			gpio_direction_output(poweron->io, poweron->enable);
            msleep(20);
        }
		if (gpio_is_valid(reset->io))
        {
			gpio_direction_output(reset->io, reset->enable);
            msleep(20);
			gpio_direction_output(reset->io, !reset->enable);
            msleep(20);
        }

#if defined(CONFIG_AP6210)
        if (gpio_is_valid(rts->io))
        {
            if (rts->iomux.name)
            {
                rk_mux_api_set(rts->iomux.name, rts->iomux.fgpio);
            }
            LOG("ENABLE UART_RTS\n");
            gpio_direction_output(rts->io, rts->enable);

            msleep(100);

            LOG("DISABLE UART_RTS\n");
            gpio_direction_output(rts->io, !rts->enable);
            if (rts->iomux.name)
            {
                rk_mux_api_set(rts->iomux.name, rts->iomux.fmux);
            }
        }
#endif

    	LOG("bt turn on power\n");
	} else {
#if WIFI_BT_POWER_TOGGLE
		if (!rk29sdk_wifi_power_state) {
#endif
            if (gpio_is_valid(poweron->io))
            {      
                gpio_direction_output(poweron->io, !poweron->enable);
                msleep(20);
            }

    		LOG("bt shut off power\n");
#if WIFI_BT_POWER_TOGGLE
		}else {
			LOG("bt shouldn't shut off power, wifi is using it!\n");
		}
#endif
		if (gpio_is_valid(reset->io))
        {      
			gpio_direction_output(reset->io, reset->enable);/* bt reset active*/
            msleep(20);
        }
	}

#if WIFI_BT_POWER_TOGGLE
	rk29sdk_bt_power_state = !blocked;
#endif
	return 0;
}

static int rfkill_rk_pm_prepare(struct device *dev)
{
    struct rfkill_rk_data *rfkill = g_rfkill;
    struct rfkill_rk_gpio* rts;
    struct rfkill_rk_irq*  wake_host_irq;
    DBG("Enter %s\n",__FUNCTION__);

    if (!rfkill)
        return 0;

    rts = &rfkill->pdata->rts_gpio;
    wake_host_irq = &rfkill->pdata->wake_host_irq;

    //To prevent uart to receive bt data when suspended
    if (gpio_is_valid(rts->io))
    {
        DBG("Disable UART_RTS\n");
        if (rts->iomux.name)
        {
            rk_mux_api_set(rts->iomux.name, rts->iomux.fgpio);
        }
        gpio_direction_output(rts->io, !rts->enable);
    }

#ifdef CONFIG_BT_AUTOSLEEP
    // BT����˯��״̬����������������
    rfkill_rk_sleep_bt(BT_SLEEP);
#endif

    /* ���ˣ��������������ݵ�UART��Ҳ���ٽ������ص�UART����
     * ���ŵ���enable_irqʹ�� bt_wake_host irq����Զ���豸������
     * ����ʱ����ͨ����IRQ��������
     */

    // enable bt wakeup host
    if (gpio_is_valid(wake_host_irq->gpio.io))
    {
#ifdef CONFIG_BK3515A_COMBO
        int ret = 0;
        ret = rfkill_rk_setup_gpio(&wake_host_irq->gpio, IOMUX_FGPIO, rfkill->pdata->name, "wake_host");
        if (ret)
            LOG("irq rfkill_rk_setup_gpio failed\n");

        ret = gpio_pull_updown(wake_host_irq->gpio.io, (wake_host_irq->gpio.enable==GPIO_LOW)?GPIOPullUp:GPIOPullDown);
        if (ret)
            LOG("irq gpio_pull_updown failed\n");
#endif
        DBG("enable irq for bt wakeup host\n");
        enable_irq(wake_host_irq->irq);
    }

#ifdef CONFIG_RFKILL_RESET
    rfkill_set_states(rfkill->rfkill_dev, BT_BLOCKED, false);
    rfkill_rk_set_power(rfkill, BT_BLOCKED);
#endif

    return 0;
}

static void rfkill_rk_pm_complete(struct device *dev)
{
    struct rfkill_rk_data *rfkill = g_rfkill;
    struct rfkill_rk_irq*  wake_host_irq;
    struct rfkill_rk_gpio* rts;
    DBG("Enter %s\n",__FUNCTION__);

    if (!rfkill)
        return;

    wake_host_irq = &rfkill->pdata->wake_host_irq;
    rts = &rfkill->pdata->rts_gpio;

    if (gpio_is_valid(wake_host_irq->gpio.io))
    {
        // ���õ� BT_WAKE_HOST IRQ��ȷ����ϵͳ���Ѻ󲻻���BT�Ĳ���
        // ����δ������ж�
        LOG("** disable irq\n");
        disable_irq(wake_host_irq->irq);
#ifdef CONFIG_BK3515A_COMBO
        rk_mux_api_set(wake_host_irq->gpio.iomux.name, wake_host_irq->gpio.iomux.fmux);
        gpio_free(wake_host_irq->gpio.io);
#endif
    }

    /* ʹ��UART_RTS����ʱ������������ݾͻ��͵�UART
     * �ϲ�������������ݲ�������Ӧ�Ķ���������: ��������
     * ����������ϲ㽫��������������Խ���
     */
    if (gpio_is_valid(rts->io))
    {
        DBG("Enable UART_RTS\n");
        gpio_direction_output(rts->io, rts->enable);
        if (rts->iomux.name)
        {
            rk_mux_api_set(rts->iomux.name, rts->iomux.fmux);
        }
    }
}

static const struct rfkill_ops rfkill_rk_ops = {
    .set_block = rfkill_rk_set_power,
};

#define PROC_DIR	"bluetooth/sleep"

static struct proc_dir_entry *bluetooth_dir, *sleep_dir;

static int bluesleep_read_proc_lpm(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
    *eof = 1;
    return sprintf(page, "unsupported to read\n");
}

static int bluesleep_write_proc_lpm(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
    return count;
}

static int bluesleep_read_proc_btwrite(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
    *eof = 1;
    return sprintf(page, "unsupported to read\n");
}

static int bluesleep_write_proc_btwrite(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
    char b;

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(&b, buffer, 1))
        return -EFAULT;

    DBG("btwrite %c\n", b);
    /* HCI_DEV_WRITE */
    if (b != '0') {
        rfkill_rk_sleep_bt(BT_WAKEUP);
    }

    return count;
}

static int rfkill_rk_probe(struct platform_device *pdev)
{
	struct rfkill_rk_data *rfkill;
	struct rfkill_rk_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
    struct proc_dir_entry *ent;

    DBG("Enter %s\n", __func__);

	if (!pdata) {
		LOG("%s: No platform data specified\n", __func__);
		return -EINVAL;
	}

    pdata->name = (char*)bt_name;

	rfkill = kzalloc(sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	rfkill->pdata = pdata;
    g_rfkill = rfkill;

    bluetooth_dir = proc_mkdir("bluetooth", NULL);
    if (bluetooth_dir == NULL) {
        LOG("Unable to create /proc/bluetooth directory");
        return -ENOMEM;
    }

    sleep_dir = proc_mkdir("sleep", bluetooth_dir);
    if (sleep_dir == NULL) {
        LOG("Unable to create /proc/%s directory", PROC_DIR);
        return -ENOMEM;
    }

	/* read/write proc entries */
    ent = create_proc_entry("lpm", 0, sleep_dir);
    if (ent == NULL) {
        LOG("Unable to create /proc/%s/lpm entry", PROC_DIR);
        ret = -ENOMEM;
        goto fail_alloc;
    }
    ent->read_proc = bluesleep_read_proc_lpm;
    ent->write_proc = bluesleep_write_proc_lpm;

    /* read/write proc entries */
    ent = create_proc_entry("btwrite", 0, sleep_dir);
    if (ent == NULL) {
        LOG("Unable to create /proc/%s/btwrite entry", PROC_DIR);
        ret = -ENOMEM;
        goto fail_alloc;
    }
    ent->read_proc = bluesleep_read_proc_btwrite;
    ent->write_proc = bluesleep_write_proc_btwrite;

    // ����GPIO�Լ�IRQ
    DBG("init gpio\n");
    // ����RK29 BCM4329������poweron io��wifi���ã���boad�ļ����Ѿ�request
    // �˴�����ȥ����
#if !WIFI_BT_POWER_TOGGLE
    ret = rfkill_rk_setup_gpio(&pdata->poweron_gpio, IOMUX_FGPIO, pdata->name, "poweron");
    if (ret) goto fail_alloc;
#endif

    ret = rfkill_rk_setup_gpio(&pdata->reset_gpio, IOMUX_FGPIO, pdata->name, "reset");
    if (ret) goto fail_poweron;

#ifndef CONFIG_BK3515A_COMBO
    ret = rfkill_rk_setup_gpio(&pdata->wake_gpio, IOMUX_FGPIO, pdata->name, "wake");
    if (ret) goto fail_reset;
#endif

    ret = rfkill_rk_setup_wake_irq(rfkill);
    if (ret) goto fail_wake;

    ret = rfkill_rk_setup_gpio(&pdata->rts_gpio, IOMUX_FMUX, rfkill->pdata->name, "rts"); 
    if (ret) goto fail_wake_host_irq;

    // ������ע��RFKILL�豸
    DBG("setup rfkill\n");
	rfkill->rfkill_dev = rfkill_alloc(pdata->name, &pdev->dev, pdata->type,
				&rfkill_rk_ops, rfkill);
	if (!rfkill->rfkill_dev)
		goto fail_rts;

    // cmy: ����rfkill��ʼ״̬Ϊblocked����ע��ʱ������� set_blocked����
    rfkill_set_states(rfkill->rfkill_dev, BT_BLOCKED, false);
	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		goto fail_rfkill;

    wake_lock_init(&(rfkill->bt_irq_wl), WAKE_LOCK_SUSPEND, "rfkill_rk_irq_wl");
    INIT_DELAYED_WORK(&rfkill->bt_sleep_delay_work, rfkill_rk_delay_sleep_bt);

    // cmy: ����������Դ��״̬Ϊ blocked
    rfkill_rk_set_power(rfkill, BT_BLOCKED);

	platform_set_drvdata(pdev, rfkill);

    LOG("%s device registered.\n", pdata->name);

	return 0;

fail_rfkill:
	rfkill_destroy(rfkill->rfkill_dev);
fail_rts:
    if (gpio_is_valid(pdata->rts_gpio.io))
        gpio_free(pdata->rts_gpio.io);
fail_wake_host_irq:
    if (gpio_is_valid(pdata->wake_host_irq.gpio.io)){
        free_irq(pdata->wake_host_irq.irq, rfkill);
#ifndef CONFIG_BK3515A_COMBO
        gpio_free(pdata->wake_host_irq.gpio.io);
#endif
    }
fail_wake:
#ifndef CONFIG_BK3515A_COMBO
    if (gpio_is_valid(pdata->wake_gpio.io))
        gpio_free(pdata->wake_gpio.io);
#endif
fail_reset:
	if (gpio_is_valid(pdata->reset_gpio.io))
		gpio_free(pdata->reset_gpio.io);
fail_poweron:
#if !WIFI_BT_POWER_TOGGLE
    if (gpio_is_valid(pdata->poweron_gpio.io))
        gpio_free(pdata->poweron_gpio.io);
#endif
fail_alloc:
	kfree(rfkill);
    g_rfkill = NULL;

	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);

	return ret;
}

static int rfkill_rk_remove(struct platform_device *pdev)
{
	struct rfkill_rk_data *rfkill = platform_get_drvdata(pdev);

    LOG("Enter %s\n", __func__);

	rfkill_unregister(rfkill->rfkill_dev);
	rfkill_destroy(rfkill->rfkill_dev);

    wake_lock_destroy(&rfkill->bt_irq_wl);
    
    cancel_delayed_work_sync(&rfkill->bt_sleep_delay_work);

    // free gpio
    if (gpio_is_valid(rfkill->pdata->rts_gpio.io))
        gpio_free(rfkill->pdata->rts_gpio.io);
    
    if (gpio_is_valid(rfkill->pdata->wake_host_irq.gpio.io)){
        free_irq(rfkill->pdata->wake_host_irq.irq, rfkill);
#ifndef CONFIG_BK3515A_COMBO
        gpio_free(rfkill->pdata->wake_host_irq.gpio.io);
#endif
    }
    
#ifndef CONFIG_BK3515A_COMBO
    if (gpio_is_valid(rfkill->pdata->wake_gpio.io))
        gpio_free(rfkill->pdata->wake_gpio.io);
#endif
    
    if (gpio_is_valid(rfkill->pdata->reset_gpio.io))
        gpio_free(rfkill->pdata->reset_gpio.io);
    
    if (gpio_is_valid(rfkill->pdata->poweron_gpio.io))
        gpio_free(rfkill->pdata->poweron_gpio.io);

    kfree(rfkill);
    g_rfkill = NULL;

	return 0;
}

static const struct dev_pm_ops rfkill_rk_pm_ops = {
	.prepare = rfkill_rk_pm_prepare,
	.complete = rfkill_rk_pm_complete,
};

static struct platform_driver rfkill_rk_driver = {
	.probe = rfkill_rk_probe,
	.remove = __devexit_p(rfkill_rk_remove),
	.driver = {
		.name = "rfkill_rk",
		.owner = THIS_MODULE,
		.pm = &rfkill_rk_pm_ops,
	},
};

static int __init rfkill_rk_init(void)
{
    LOG("Enter %s\n", __func__);
	return platform_driver_register(&rfkill_rk_driver);
}

static void __exit rfkill_rk_exit(void)
{
    LOG("Enter %s\n", __func__);
	platform_driver_unregister(&rfkill_rk_driver);
}

module_init(rfkill_rk_init);
module_exit(rfkill_rk_exit);

MODULE_DESCRIPTION("rock-chips rfkill for Bluetooth v0.2");
MODULE_AUTHOR("cmy@rock-chips.com, cz@rock-chips.com");
MODULE_LICENSE("GPL");

