#ifndef __PLAT_CPU_H
#define __PLAT_CPU_H

#include <mach/io.h>
#include <mach/gpio.h>
#include <linux/io.h>

#ifdef CONFIG_ARCH_RK2928
#define SOC_RK2928G     0x01
#define SOC_RK2928L     0x02
#define SOC_RK2926      0x00

static inline bool soc_is_rk2928g(void)
{
	return ((readl_relaxed(RK2928_GPIO3_BASE + 0x50) & 0x07) == SOC_RK2928G);
}

static inline bool soc_is_rk2928l(void)
{
	return ((readl_relaxed(RK2928_GPIO3_BASE + 0x50) & 0x07) == SOC_RK2928L);
}

static inline bool soc_is_rk2926(void)
{
	return ((readl_relaxed(RK2928_GPIO3_BASE + 0x50) & 0x07) == SOC_RK2926);
}

static inline bool cpu_is_rk2928(void) { return true; }
#else
static inline bool cpu_is_rk2928(void) { return false; }
static inline bool soc_is_rk2928g(void) { return false; }
static inline bool soc_is_rk2928l(void) { return false; }
static inline bool soc_is_rk2926(void) { return false; }
#endif

#if defined(CONFIG_ARCH_RKPX2)
static inline bool cpu_is_rk30xx(void)
{
	return readl_relaxed(RK30_ROM_BASE + 0x27f0) == 0x33303041
	    && readl_relaxed(RK30_ROM_BASE + 0x27f4) == 0x32303131
	    && readl_relaxed(RK30_ROM_BASE + 0x27f8) == 0x31313131
	    && readl_relaxed(RK30_ROM_BASE + 0x27fc) == 0x56313031;
}

static inline bool cpu_is_rk3066b(void)
{
	return(readl_relaxed(RK30_ROM_BASE + 0x27f0) == 0x33303041
	    && readl_relaxed(RK30_ROM_BASE + 0x27f4) == 0x32303131
	    && readl_relaxed(RK30_ROM_BASE + 0x27f8) == 0x31313131
	    && readl_relaxed(RK30_ROM_BASE + 0x27fc) == 0x56313030)
	    ||(readl_relaxed(RK30_ROM_BASE + 0x27f0) == 0x33303042
	    && readl_relaxed(RK30_ROM_BASE + 0x27f4) == 0x32303132
	    && readl_relaxed(RK30_ROM_BASE + 0x27f8) == 0x31303031
	    && readl_relaxed(RK30_ROM_BASE + 0x27fc) == 0x56313030);
}

static inline bool soc_is_rk3066b(void)
{
	return cpu_is_rk3066b() && (((readl_relaxed(RK30_GPIO1_BASE + GPIO_EXT_PORT) >> 22) & 3) == 0);
}

static inline bool soc_is_rk3108(void)
{
	return cpu_is_rk3066b() && (((readl_relaxed(RK30_GPIO1_BASE + GPIO_EXT_PORT) >> 22) & 3) == 1);
}

static inline bool soc_is_rk3168m(void)
{
	return cpu_is_rk3066b() && (((readl_relaxed(RK30_GPIO1_BASE + GPIO_EXT_PORT) >> 22) & 3) == 3);
}

static inline bool soc_is_rk3188(void)
{
	return readl_relaxed(RK30_ROM_BASE + 0x27f0) == 0x33313042
	    && readl_relaxed(RK30_ROM_BASE + 0x27f4) == 0x32303132
	    && readl_relaxed(RK30_ROM_BASE + 0x27f8) == 0x31313330
	    && readl_relaxed(RK30_ROM_BASE + 0x27fc) == 0x56313030;
}

static inline bool soc_is_rk3188plus(void)
{
	return readl_relaxed(RK30_ROM_BASE + 0x27f0) == 0x33313042
	    && readl_relaxed(RK30_ROM_BASE + 0x27f4) == 0x32303133
	    && readl_relaxed(RK30_ROM_BASE + 0x27f8) == 0x30313331
	    && readl_relaxed(RK30_ROM_BASE + 0x27fc) == 0x56313031;
}
#else
static inline bool cpu_is_rk30xx(void) { return false; }
static inline bool cpu_is_rk3066b(void) { return false; }

static inline bool soc_is_rk3066b(void) { return false; }
static inline bool soc_is_rk3108(void) { return false; }
static inline bool soc_is_rk3168m(void) { return false; }

static inline bool soc_is_rk3188(void) { return false; }
static inline bool soc_is_rk3188plus(void) { return false; }
#endif

static inline bool cpu_is_rk3188(void)
{
	return soc_is_rk3188plus() || soc_is_rk3188();
}

static inline bool soc_is_rk3028(void) { return soc_is_rk3168m(); }
static inline bool soc_is_rk3168(void) { return soc_is_rk3108(); }
static inline bool soc_is_rk2928b(void) { return soc_is_rk3168m(); }

#ifdef CONFIG_SOC_RK3000
static inline bool soc_is_rk3000(void) { return true; }
#else
static inline bool soc_is_rk3000(void) { return false; }
#endif

#ifdef CONFIG_SOC_RKPX2
static inline bool soc_is_rkpx2(void) { return true; }
#else
static inline bool soc_is_rkpx2(void) { return false; }
#endif

#ifdef CONFIG_SOC_RK3068
static inline bool soc_is_rk3068(void) { return true; }
#else
static inline bool soc_is_rk3068(void) { return false; }
#endif

#endif
