EXTRA_CFLAGS += -Os
ifneq ($(CONFIG_RK_FPGA),y)
obj-y += ../plat-rk/clock.o
obj-$(CONFIG_ARCH_RK30XX) += clock_data.o
endif
obj-y += common.o
CFLAGS_common.o += -DTEXT_OFFSET=$(TEXT_OFFSET)
obj-y += ddr.o
obj-y += devices.o
obj-y += io.o
ifeq ($(CONFIG_ARCH_RK30XX),y)
obj-y += iomux.o
obj-$(CONFIG_CPU_FREQ) += cpufreq.o
endif
obj-y += pmu.o
obj-y += reset.o
obj-y += timer.o
obj-y += tsadc.o
obj-$(CONFIG_SMP) += platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o
obj-$(CONFIG_LOCAL_TIMERS) += localtimer.o
obj-$(CONFIG_PM) += pm.o
obj-$(CONFIG_CPU_IDLE) += cpuidle.o
ifeq ($(CONFIG_DVFS_WITH_UOC),y)
obj-$(CONFIG_DVFS) += dvfs-rk3066b.o
else
obj-$(CONFIG_DVFS) += dvfs.o
endif
obj-$(CONFIG_ARCH_RK30XX) += board.o
board-$(CONFIG_MACH_RKPX2_SDK) += board-rkpx2-sdk.o

