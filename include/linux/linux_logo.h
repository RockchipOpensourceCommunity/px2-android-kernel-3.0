#ifndef _LINUX_LINUX_LOGO_H
#define _LINUX_LINUX_LOGO_H

/*
 *  Linux logo to be displayed on boot
 *
 *  Copyright (C) 1996 Larry Ewing (lewing@isc.tamu.edu)
 *  Copyright (C) 1996,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 *  Copyright (C) 2001 Greg Banks <gnb@alphalink.com.au>
 *  Copyright (C) 2001 Jan-Benedict Glaw <jbglaw@lug-owl.de>
 *  Copyright (C) 2003 Geert Uytterhoeven <geert@linux-m68k.org>
 *
 *  Serial_console ascii image can be any size,
 *  but should contain %s to display the version
 */

#include <linux/init.h>


#define LINUX_LOGO_MONO		1	/* monochrome black/white */
#define LINUX_LOGO_VGA16	2	/* 16 colors VGA text palette */
#define LINUX_LOGO_CLUT224	3	/* 224 colors */
#define LINUX_LOGO_GRAY256	4	/* 256 levels grayscale */
#define LINUX_LOGO_bmp      5  /* truecolours*/


struct linux_logo {
	int type;			/* one of LINUX_LOGO_* */
	unsigned int width;
	unsigned int height;
	unsigned int clutsize;		/* LINUX_LOGO_CLUT224 only */
	const unsigned char *clut;	/* LINUX_LOGO_CLUT224 only */
	const unsigned char *data;
};
extern const struct linux_logo logo_linux_lowerpower_clut224;
extern const struct linux_logo logo_linux_mono;
extern const struct linux_logo logo_linux_vga16;
extern const struct linux_logo logo_linux_clut224;

#ifdef CONFIG_LOGO_PIPO_CLUT224
extern const struct linux_logo logo_pipo_clut224;
#endif
extern const struct linux_logo logo_blackfin_vga16;
extern const struct linux_logo logo_blackfin_clut224;
extern const struct linux_logo logo_dec_clut224;
extern const struct linux_logo logo_mac_clut224;
extern const struct linux_logo logo_parisc_clut224;
extern const struct linux_logo logo_sgi_clut224;
extern const struct linux_logo logo_sun_clut224;
extern const struct linux_logo logo_superh_mono;
extern const struct linux_logo logo_superh_vga16;
extern const struct linux_logo logo_superh_clut224;
extern const struct linux_logo logo_m32r_clut224;
extern const struct linux_logo logo_spe_clut224;
extern const struct linux_logo logo_g3_clut224;
extern const struct linux_logo logo_sunset_bmp;
extern const struct linux_logo logo_android_bmp;
extern const struct linux_logo logo_linux_800x480_clut224;

extern const struct linux_logo *fb_find_logo(int depth);
#ifdef CONFIG_FB_LOGO_EXTRA
extern void fb_append_extra_logo(const struct linux_logo *logo,
				 unsigned int n);
#else
static inline void fb_append_extra_logo(const struct linux_logo *logo,
					unsigned int n)
{}
#endif

#endif /* _LINUX_LINUX_LOGO_H */
