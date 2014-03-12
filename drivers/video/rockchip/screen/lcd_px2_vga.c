#ifndef __LCD_PS2_VGA__
#define __LCD_PS2_VGA__

#define SCREEN_TYPE	    	SCREEN_RGB

#define LVDS_FORMAT      	LVDS_8BIT_2
#define OUT_FACE	    	OUT_P888

#define DCLK	          	74250000							 
#define LCDC_ACLK         	300000000           //29 lcdc axi DMA ÆµÂÊ

/* Timing */


#define H_PW			40
#define H_BP			220
#define H_VD			1280
#define H_FP			110

#define V_PW			5
#define V_BP			20
#define V_VD			720
#define V_FP			5

#define LCD_WIDTH          	216
#define LCD_HEIGHT         	135


/* Other */

#define DCLK_POL	0

#define DEN_POL		0
#define VSYNC_POL	1
#define HSYNC_POL	1

#define SWAP_RB		0
#define SWAP_RG		0
#define SWAP_GB		0


#endif


