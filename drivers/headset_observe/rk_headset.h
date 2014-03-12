#ifndef RK_HEADSET_H
#define RK_HEADSET_H

#define HEADSET_IN_HIGH 0x00000001
#define HEADSET_IN_LOW  0x00000000

#define HOOK_DOWN_HIGH 0x00000001
#define HOOK_DOWN_LOW  0x00000000

struct io_info{
	char	iomux_name[50];
	int		iomux_mode;	
};


struct rk_headset_pdata{
	unsigned int Hook_gpio;//Detection Headset--Must be set
	unsigned int Sw_mic_gpio;
	unsigned int Hp_mic_io_value;
	unsigned int Main_mic_io_value;
	unsigned int Hook_adc_chn; //adc channel
	unsigned int Hook_down_type; //Hook key down status   
	int	hook_key_code;
	unsigned int Headset_gpio;//Detection Headset--Must be set
	unsigned int headset_in_type;//	Headphones into the state level--Must be set
	int (*headset_io_init)(int);
	int (*hook_io_init)(int);
};

#endif
