/*
 * wifi_power.c
 *
 * Power control for WIFI module.
 *
 * There are Power supply and Power Up/Down controls for WIFI typically.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#include "wifi_power.h"

#if (WIFI_GPIO_POWER_CONTROL == 1)

/*
 * GPIO to control LDO/DCDC.
 *
 * ���ڿ���WIFI�ĵ�Դ��ͨ����3.3V��1.8V������1.2VҲ�����С�
 *
 * �������չIO����ο����������:
 *   POWER_USE_EXT_GPIO, 0, 0, 0, PCA9554_Pin1, GPIO_HIGH
 */
struct wifi_power power_gpio = 
{
	POWER_USE_GPIO, POWER_GPIO_IOMUX, WIFI_PWDN_IOMUX_PINNAME,
    WIFI_PWDN_IOMUX_PINDIR, WIFI_PWDN_IOPIN, GPIO_HIGH
};

/*
 * GPIO to control WIFI PowerDOWN/RESET.
 *
 * ����WIFI��PowerDown�š���Щģ��PowerDown���Ǻ�Reset�Ŷ̽���һ��
 */
struct wifi_power power_save_gpio = 
{
	POWER_USE_GPIO, 0, WIFI_RST_IOMUX_PINNAME,  
	WIFI_RST_IOMUX_PINDIR, WIFI_RST_IOPIN, GPIO_HIGH
};

/*
 * GPIO to reset WIFI. Keep this as NULL normally.
 *
 * ����WIFI��Reset�ţ�ͨ��WiFiģ��û���õ�������š�
 */
struct wifi_power power_reset_gpio = 
{
	0, 0, 0, 0, 0, 0
};

/*
 * If external GPIO chip such as PCA9554 is being used, please
 * implement the following 2 function.
 *
 * id:   is GPIO identifier, such as GPIOPortF_Pin0, or external 
 *       name defined in struct wifi_power.
 * sens: the value should be set to GPIO, usually is GPIO_HIGH or GPIO_LOW.
 *
 * ���������չGPIO������WIFI����ʵ������ĺ���:
 * �����Ĺ����ǣ�����ָ����IO��id��ʹ��״̬�л�ΪҪ���sens״̬��
 * id  : ��IO�ı�ʶ�ţ�����������ʽ��ʶ��
 * sens: ��Ҫ���IO״̬��Ϊ�߻�͡�
 */
void wifi_extgpio_operation(u8 id, u8 sens)
{
	//pca955x_gpio_direction_output(id, sens);
}

/*
 * ��ϵͳ�����Ҫ����WIFI��IO���ƣ���WIFI�µ磬���Ե������½ӿڣ�
 *   void rockchip_wifi_shutdown();
 * ��ע����Ҫ�ں�WIFI_GPIO_POWER_CONTROL�Ŀ����¡�
 */

#endif /* WIFI_GPIO_POWER_CONTROL */

