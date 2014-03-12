#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <asm/pgtable-hwdef.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/hardware/cache-l2x0.h>

#include <plat/sram.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/fiq.h>
#include <mach/pmu.h>
#include <mach/loader.h>
#include <mach/ddr.h>
#include <mach/dvfs.h>
#include <mach/cpu_axi.h>
#include <mach/debug_uart.h>

static void __init rk30_cpu_axi_init(void)
{
	CPU_AXI_SET_QOS_PRIORITY(0, 0, DMAC);
	CPU_AXI_SET_QOS_PRIORITY(0, 0, CPU0);
	CPU_AXI_SET_QOS_PRIORITY(0, 0, CPU1R);
	CPU_AXI_SET_QOS_PRIORITY(0, 0, CPU1W);
#ifdef CONFIG_RK29_VMAC
	CPU_AXI_SET_QOS_PRIORITY(2, 2, PERI);
#else
	CPU_AXI_SET_QOS_PRIORITY(0, 0, PERI);
#endif
	CPU_AXI_SET_QOS_PRIORITY(3, 3, LCDC0);
	CPU_AXI_SET_QOS_PRIORITY(3, 3, LCDC1);
	CPU_AXI_SET_QOS_PRIORITY(2, 1, GPU);

	writel_relaxed(0x3f, RK30_CPU_AXI_BUS_BASE + 0x0014);	// memory scheduler read latency
	dsb();
}

#define L2_LY_SP_OFF (0)
#define L2_LY_SP_MSK (0x7)

#define L2_LY_RD_OFF (4)
#define L2_LY_RD_MSK (0x7)

#define L2_LY_WR_OFF (8)
#define L2_LY_WR_MSK (0x7)
#define L2_LY_SET(ly,off) (((ly)-1)<<(off))

#define L2_LATENCY(setup_cycles, read_cycles, write_cycles) \
	L2_LY_SET(setup_cycles, L2_LY_SP_OFF) | \
	L2_LY_SET(read_cycles, L2_LY_RD_OFF) | \
	L2_LY_SET(write_cycles, L2_LY_WR_OFF)

static void __init rk30_l2_cache_init(void)
{
#ifdef CONFIG_CACHE_L2X0
	u32 aux_ctrl, aux_ctrl_mask, data_latency_ctrl;

	unsigned int max_cpu_freq = 1608000; // kHz
	struct cpufreq_frequency_table *table = NULL;
	struct clk *clk_cpu;
	int i;

	clk_cpu = clk_get(NULL, "cpu");
	if (!IS_ERR(clk_cpu)) {
		table = dvfs_get_freq_volt_table(clk_cpu);
		if (!table)
			pr_err("failed to get cpu freq volt table\n");
	} else
		pr_err("failed to get clk cpu\n");
	for (i = 0; table && table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (max_cpu_freq < table[i].frequency)
			max_cpu_freq = table[i].frequency;
	}

	if (max_cpu_freq <= 1608000)
		data_latency_ctrl = L2_LATENCY(4, 6, 1);
	else if (max_cpu_freq <= 1800000)
		data_latency_ctrl = L2_LATENCY(5, 7, 1);
	else if (max_cpu_freq <= 1992000)
		data_latency_ctrl = L2_LATENCY(5, 8, 1);
	else
		data_latency_ctrl = L2_LATENCY(6, 8, 1);

	writel_relaxed(L2_LATENCY(1, 1, 1), RK30_L2C_BASE + L2X0_TAG_LATENCY_CTRL);
	writel_relaxed(data_latency_ctrl, RK30_L2C_BASE + L2X0_DATA_LATENCY_CTRL);

	/* L2X0 Prefetch Control */
	writel_relaxed(0x70000003, RK30_L2C_BASE + L2X0_PREFETCH_CTRL);

	/* L2X0 Power Control */
	writel_relaxed(L2X0_DYNAMIC_CLK_GATING_EN | L2X0_STNDBY_MODE_EN, RK30_L2C_BASE + L2X0_POWER_CTRL);

	aux_ctrl = (
			(0x1 << 25) | 	// round-robin
			(0x1 << 0) |		// Full Line of Zero Enable
			(0x1 << L2X0_AUX_CTRL_NS_LOCKDOWN_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_INSTR_PREFETCH_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_EARLY_BRESP_SHIFT) );

	aux_ctrl_mask = ~(
			(0x1 << 25) | 	// round-robin
			(0x1 << 0) |		// Full Line of Zero Enable
			(0x1 << L2X0_AUX_CTRL_NS_LOCKDOWN_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_INSTR_PREFETCH_SHIFT) |
			(0x1 << L2X0_AUX_CTRL_EARLY_BRESP_SHIFT) );

	l2x0_init(RK30_L2C_BASE, aux_ctrl, aux_ctrl_mask);
#endif
}

static int boot_mode;

static void __init rk30_boot_mode_init(void)
{
	u32 boot_flag = readl_relaxed(RK30_PMU_BASE + PMU_SYS_REG0);
	boot_mode = readl_relaxed(RK30_PMU_BASE + PMU_SYS_REG1);

	if (boot_flag == (SYS_KERNRL_REBOOT_FLAG | BOOT_RECOVER)) {
		boot_mode = BOOT_MODE_RECOVERY;
	}
	if (boot_mode || ((boot_flag & 0xff) && ((boot_flag & 0xffffff00) == SYS_KERNRL_REBOOT_FLAG)))
		printk("Boot mode: %s (%d) flag: %s (0x%08x)\n", boot_mode_name(boot_mode), boot_mode, boot_flag_name(boot_flag), boot_flag);
#ifdef CONFIG_RK29_WATCHDOG
	writel_relaxed(BOOT_MODE_WATCHDOG, RK30_PMU_BASE + PMU_SYS_REG1);
#endif
}

int board_boot_mode(void)
{
	return boot_mode;
}
EXPORT_SYMBOL(board_boot_mode);

void __init rk30_init_irq(void)
{
	gic_init(0, IRQ_LOCALTIMER, RK30_GICD_BASE, RK30_GICC_BASE);
#ifdef CONFIG_FIQ
	rk_fiq_init();
#endif
	rk30_gpio_init();
}

static void usb_uart_init(void)
{
#if defined(CONFIG_ARCH_RK3188) && (CONFIG_RK_DEBUG_UART == 2)
#ifdef CONFIG_RK_USB_UART
	if (!(readl_relaxed(RK30_GRF_BASE + GRF_SOC_STATUS0) & (1 << 13))) { //detect id
		writel_relaxed((0x0300 << 16), RK30_GRF_BASE + GRF_UOC0_CON0);
	} else {
		if (!(readl_relaxed(RK30_GRF_BASE + GRF_SOC_STATUS0) & (1 << 10))) { //detect vbus
			writel_relaxed(((0x01 << 2) | ((0x01 << 2) << 16)), RK30_GRF_BASE + GRF_UOC0_CON2); //software control usb phy enable 
			writel_relaxed((0x2A | (0x3F << 16)), RK30_GRF_BASE + GRF_UOC0_CON3); //usb phy enter suspend
			writel_relaxed((0x0300 | (0x0300 << 16)), RK30_GRF_BASE + GRF_UOC0_CON0);
		} else {
			writel_relaxed((0x0300 << 16), RK30_GRF_BASE + GRF_UOC0_CON0);
		}
	}
#else
	writel_relaxed((0x0300 << 16), RK30_GRF_BASE + GRF_UOC0_CON0);
#endif
#endif
}

void __init rk30_map_io(void)
{
	rk30_map_common_io();
	usb_uart_init();
	rk29_setup_early_printk();
	rk30_cpu_axi_init();
	rk29_sram_init();
	board_clock_init();
	rk30_l2_cache_init();
	ddr_init(DDR_TYPE, DDR_FREQ);
	clk_disable_unused();
	rk30_iomux_init();
	rk30_boot_mode_init();
#if defined(CONFIG_EMMC_IO_3_3V)
	grf_set_io_power_domain_voltage(IO_PD_FLASH, IO_PD_VOLTAGE_3_3V);
#endif
}

static __init u32 rk30_get_ddr_size(void)
{
	u32 size;
	u32 v[3], a[3];
	u32 pgtbl = PAGE_OFFSET + TEXT_OFFSET - 0x4000;
	u32 flag = PMD_TYPE_SECT | PMD_SECT_XN | PMD_SECT_AP_WRITE | PMD_SECT_AP_READ;

	a[0] = pgtbl + (((u32)RK30_CPU_AXI_BUS_BASE >> 20) << 2);
	a[1] = pgtbl + (((u32)RK30_DDR_PUBL_BASE >> 20) << 2);
	a[2] = pgtbl + (((u32)RK30_GRF_BASE >> 20) << 2);
	v[0] = readl_relaxed(a[0]);
	v[1] = readl_relaxed(a[1]);
	v[2] = readl_relaxed(a[2]);
	writel_relaxed(flag | ((RK30_CPU_AXI_BUS_PHYS >> 20) << 20), a[0]);
	writel_relaxed(flag | ((RK30_DDR_PUBL_PHYS >> 20) << 20), a[1]);
	writel_relaxed(flag | ((RK30_GRF_PHYS >> 20) << 20), a[2]);

	size = ddr_get_cap();

	writel_relaxed(v[0], a[0]);
	writel_relaxed(v[1], a[1]);
	writel_relaxed(v[2], a[2]);

	return size;
}

void __init rk30_fixup(struct machine_desc *desc, struct tag *tags,
			char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	mi->bank[0].start = PLAT_PHYS_OFFSET;
	mi->bank[0].size = rk30_get_ddr_size();
}

