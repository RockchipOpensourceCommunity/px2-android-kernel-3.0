/*
 * arch/arm/mach-rkpx2/ddr.c
 *
 * Function Driver for DDR controller
 *
 * Copyright (C) 2011 Fuzhou Rockchip Electronics Co.,Ltd
 * Author: 
 * hcy@rock-chips.com
 * yk@rock-chips.com
 * 
 * v1.00 
 */
 
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/clk.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include <mach/sram.h>
#include <mach/ddr.h>
#include <mach/cpu.h>
#include <plat/efuse.h>
#include <linux/rk_fb.h>
//#include <linux/delay.h>

typedef uint32_t uint32;

//#define ENABLE_DDR_CLCOK_GPLL_PATH  //for RK3188

#define DDR3_DDR2_ODT_DLL_DISABLE_FREQ    (333)
#define SR_IDLE                       (0x1)   //unit:32*DDR clk cycle, and 0 for disable auto self-refresh
#define PD_IDLE                       (0X40)  //unit:DDR clk cycle, and 0 for disable auto power-down

#if (DDR3_DDR2_ODT_DLL_DISABLE_FREQ > 333)
#error
#endif

#define PMU_BASE_ADDR           RK30_PMU_BASE
#define SDRAMC_BASE_ADDR        RK30_DDR_PCTL_BASE
#define DDR_PUBL_BASE           RK30_DDR_PUBL_BASE
#define CRU_BASE_ADDR           RK30_CRU_BASE
#define REG_FILE_BASE_ADDR      RK30_GRF_BASE
#define SysSrv_DdrConf          (RK30_CPU_AXI_BUS_BASE+0x08)
#define SysSrv_DdrTiming        (RK30_CPU_AXI_BUS_BASE+0x0c)
#define SysSrv_DdrMode          (RK30_CPU_AXI_BUS_BASE+0x10)
#define SysSrv_ReadLatency      (RK30_CPU_AXI_BUS_BASE+0x14)

#define SRAM_SIZE       RK30_IMEM_SIZE
#define ddr_print(x...) printk( "DDR DEBUG: " x )

/***********************************
 * LPDDR2 define
 ***********************************/
//MR0 (Device Information)
#define  LPDDR2_DAI    (0x1)        // 0:DAI complete, 1:DAI still in progress
#define  LPDDR2_DI     (0x1<<1)     // 0:S2 or S4 SDRAM, 1:NVM
#define  LPDDR2_DNVI   (0x1<<2)     // 0:DNV not supported, 1:DNV supported
#define  LPDDR2_RZQI   (0x3<<3)     // 00:RZQ self test not supported, 01:ZQ-pin may connect to VDDCA or float
                                    // 10:ZQ-pin may short to GND.     11:ZQ-pin self test completed, no error condition detected.

//MR1 (Device Feature)
#define LPDDR2_BL4     (0x2)
#define LPDDR2_BL8     (0x3)
#define LPDDR2_BL16    (0x4)
#define LPDDR2_nWR(n)  (((n)-2)<<5)

//MR2 (Device Feature 2)
#define LPDDR2_RL3_WL1  (0x1)
#define LPDDR2_RL4_WL2  (0x2)
#define LPDDR2_RL5_WL2  (0x3)
#define LPDDR2_RL6_WL3  (0x4)
#define LPDDR2_RL7_WL4  (0x5)
#define LPDDR2_RL8_WL4  (0x6)

//MR3 (IO Configuration 1)
#define LPDDR2_DS_34    (0x1)
#define LPDDR2_DS_40    (0x2)
#define LPDDR2_DS_48    (0x3)
#define LPDDR2_DS_60    (0x4)
#define LPDDR2_DS_80    (0x6)
#define LPDDR2_DS_120   (0x7)   //optional

//MR4 (Device Temperature)
#define LPDDR2_tREF_MASK (0x7)
#define LPDDR2_4_tREF    (0x1)
#define LPDDR2_2_tREF    (0x2)
#define LPDDR2_1_tREF    (0x3)
#define LPDDR2_025_tREF  (0x5)
#define LPDDR2_025_tREF_DERATE    (0x6)

#define LPDDR2_TUF       (0x1<<7)

//MR8 (Basic configuration 4)
#define LPDDR2_S4        (0x0)
#define LPDDR2_S2        (0x1)
#define LPDDR2_N         (0x2)
#define LPDDR2_Density(mr8)  (8<<(((mr8)>>2)&0xf))   // Unit:MB
#define LPDDR2_IO_Width(mr8) (32>>(((mr8)>>6)&0x3))

//MR10 (Calibration)
#define LPDDR2_ZQINIT   (0xFF)
#define LPDDR2_ZQCL     (0xAB)
#define LPDDR2_ZQCS     (0x56)
#define LPDDR2_ZQRESET  (0xC3)

//MR16 (PASR Bank Mask)
// S2 SDRAM Only
#define LPDDR2_PASR_Full (0x0)    
#define LPDDR2_PASR_1_2  (0x1)
#define LPDDR2_PASR_1_4  (0x2)
#define LPDDR2_PASR_1_8  (0x3)

//MR17 (PASR Segment Mask) 1Gb-8Gb S4 SDRAM only

//MR32 (DQ Calibration Pattern A)

//MR40 (DQ Calibration Pattern B)

/***********************************
 * DDR3 define
 ***********************************/
//mr0 for ddr3
#define DDR3_BL8          (0)
#define DDR3_BC4_8        (1)
#define DDR3_BC4          (2)
#define DDR3_CL(n)        (((((n)-4)&0x7)<<4)|((((n)-4)&0x8)>>1))
#define DDR3_WR(n)        (((n)&0x7)<<9)
#define DDR3_DLL_RESET    (1<<8)
#define DDR3_DLL_DeRESET  (0<<8)
    
//mr1 for ddr3
#define DDR3_DLL_ENABLE    (0)
#define DDR3_DLL_DISABLE   (1)
#define DDR3_MR1_AL(n)  (((n)&0x3)<<3)
    
#define DDR3_DS_40            (0)
#define DDR3_DS_34            (1<<1)
#define DDR3_Rtt_Nom_DIS      (0)
#define DDR3_Rtt_Nom_60       (1<<2)
#define DDR3_Rtt_Nom_120      (1<<6)
#define DDR3_Rtt_Nom_40       ((1<<2)|(1<<6))
    
    //mr2 for ddr3
#define DDR3_MR2_CWL(n) ((((n)-5)&0x7)<<3)
#define DDR3_Rtt_WR_DIS       (0)
#define DDR3_Rtt_WR_60        (1<<9)
#define DDR3_Rtt_WR_120       (2<<9)

/***********************************
 * DDR2 define
 ***********************************/
//MR;                     //Mode Register                                                
#define DDR2_BL4           (2)
#define DDR2_BL8           (3)
#define DDR2_CL(n)         (((n)&0x7)<<4)
#define DDR2_WR(n)        ((((n)-1)&0x7)<<9)
#define DDR2_DLL_RESET    (1<<8)
#define DDR2_DLL_DeRESET  (0<<8)
    
//EMR;                    //Extended Mode Register      
#define DDR2_DLL_ENABLE    (0)
#define DDR2_DLL_DISABLE   (1)

#define DDR2_STR_FULL     (0)
#define DDR2_STR_REDUCE   (1<<1)
#define DDR2_AL(n)        (((n)&0x7)<<3)
#define DDR2_Rtt_Nom_DIS      (0)
#define DDR2_Rtt_Nom_150      (0x40)
#define DDR2_Rtt_Nom_75       (0x4)
#define DDR2_Rtt_Nom_50       (0x44)

/***********************************
 * LPDDR define
 ***********************************/
#define mDDR_BL2           (1)
#define mDDR_BL4           (2)
#define mDDR_BL8           (3)
#define mDDR_CL(n)         (((n)&0x7)<<4)
    
#define mDDR_DS_Full       (0)
#define mDDR_DS_1_2        (1<<5)
#define mDDR_DS_1_4        (2<<5)
#define mDDR_DS_1_8        (3<<5)
#define mDDR_DS_3_4        (4<<5)


//PMU_MISC_CON1
#define idle_req_cpu_cfg    (1<<1)
#define idle_req_peri_cfg   (1<<2)
#define idle_req_gpu_cfg    (1<<3)
#define idle_req_video_cfg  (1<<4)
#define idle_req_vio_cfg    (1<<5)
#define idle_req_core_cfg    (1<<14)
#define idle_req_dma_cfg    (1<<16)

//PMU_PWRDN_ST
#define idle_cpu    (1<<26)
#define idle_peri   (1<<25)
#define idle_gpu    (1<<24)
#define idle_video  (1<<23)
#define idle_vio    (1<<22)
#define idle_core    (1<<15)
#define idle_dma    (1<<14)

#define pd_a9_0_pwr_st    (1<<0)
#define pd_a9_1_pwr_st    (1<<1)
#define pd_peri_pwr_st    (1<<6)
#define pd_vio_pwr_st    (1<<7)
#define pd_video_pwr_st    (1<<8)
#define pd_gpu_pwr_st    (1<<9)


//PMU registers
typedef volatile struct tagPMU_FILE
{
    uint32 PMU_WAKEUP_CFG[2];
    uint32 PMU_PWRDN_CON;
    uint32 PMU_PWRDN_ST;
    uint32 PMU_INT_CON;
    uint32 PMU_INT_ST;
    uint32 PMU_MISC_CON;
    uint32 PMU_OSC_CNT;
    uint32 PMU_PLL_CNT;
    uint32 PMU_PMU_CNT;
    uint32 PMU_DDRIO_PWRON_CNT;
    uint32 PMU_WAKEUP_RST_CLR_CNT;
    uint32 PMU_SCU_PWRDWN_CNT;
    uint32 PMU_SCU_PWRUP_CNT;
    uint32 PMU_MISC_CON1;
    uint32 PMU_GPIO6_CON;
    uint32 PMU_PMU_SYS_REG[4];
} PMU_FILE, *pPMU_FILE;

#define pPMU_Reg ((pPMU_FILE)PMU_BASE_ADDR)

#define PLL_RESET  (((0x1<<5)<<16) | (0x1<<5))
#define PLL_DE_RESET  (((0x1<<5)<<16) | (0x0<<5))
#define NR(n)       ((((n)-1)<<8)|(0x3F<<24))
#define NO(n)       (((n)-1)|(0xF<<16))
#define NF(n)       (((n)-1)|(0x1FFF<<16))
#define NB(n,pull)  ((pull?((n)-1):0xFF)|(0xFFF<<16))

 //CRU Registers
typedef volatile struct tagCRU_STRUCT
{
    uint32 CRU_PLL_CON[4][4];
    uint32 CRU_MODE_CON;
    uint32 CRU_CLKSEL_CON[35];
    uint32 CRU_CLKGATE_CON[10];
    uint32 reserved1[2];
    uint32 CRU_GLB_SRST_FST_VALUE;
    uint32 CRU_GLB_SRST_SND_VALUE;
    uint32 reserved2[2];
    uint32 CRU_SOFTRST_CON[9];
    uint32 CRU_MISC_CON;
    uint32 reserved3[2];
    uint32 CRU_GLB_CNT_TH;
} CRU_REG, *pCRU_REG;

#define pCRU_Reg ((pCRU_REG)CRU_BASE_ADDR)

#define bank2_to_rank_en   ((1<<2) | ((1<<2)<<16))
#define bank2_to_rank_dis   ((0<<2) | ((1<<2)<<16))
#define rank_to_row15_en   ((1<<1) | ((1<<1)<<16))
#define rank_to_row15_dis   ((0<<1) | ((1<<1)<<16))

typedef struct tagGPIO_LH
{
    uint32 GPIOL;
    uint32 GPIOH;
}GPIO_LH_T;

typedef struct tagGPIO_IOMUX
{
    uint32 GPIOA_IOMUX;
    uint32 GPIOB_IOMUX;
    uint32 GPIOC_IOMUX;
    uint32 GPIOD_IOMUX;
}GPIO_IOMUX_T;

//REG FILE registers
typedef volatile struct tagREG_FILE
{
    GPIO_LH_T GRF_GPIO_DIR[7];
    GPIO_LH_T GRF_GPIO_DO[7];
    GPIO_LH_T GRF_GPIO_EN[7];
    GPIO_IOMUX_T GRF_GPIO_IOMUX[7];
    GPIO_LH_T GRF_GPIO_PULL[7];
    uint32 GRF_SOC_CON[3];
    uint32 GRF_SOC_STATUS0;
    uint32 GRF_DMAC1_CON[3];
    uint32 GRF_DMAC2_CON[4];
    uint32 GRF_UOC0_CON[3];
    uint32 GRF_UOC1_CON[4];
    uint32 GRF_DDRC_CON0;
    uint32 GRF_DDRC_STAT;
    uint32 reserved[(0x1c8-0x1a0)/4];
    uint32 GRF_OS_REG[4];
} REG_FILE, *pREG_FILE;

#define pGRF_Reg ((pREG_FILE)REG_FILE_BASE_ADDR)

//REG FILE registers
typedef volatile struct tagREG_FILE_RK3066B
{
    GPIO_LH_T GRF_GPIO_DIR[4];
    GPIO_LH_T GRF_GPIO_DO[4];
    GPIO_LH_T GRF_GPIO_EN[4];
    GPIO_IOMUX_T GRF_GPIO_IOMUX[4];
    uint32 GRF_SOC_CON[3];
    uint32 GRF_SOC_STATUS0;
    uint32 GRF_DMAC0_CON[3];
    uint32 GRF_DMAC1_CON[4];
    uint32 reserved0[(0xec-0xcc)/4];
    uint32 GRF_DDRC_CON0;
    uint32 GRF_DDRC_STAT;
    uint32 GRF_IO_CON[5];
    uint32 reserved1;
    uint32 GRF_UOC0_CON[4];
    uint32 GRF_UOC1_CON[4];
    uint32 GRF_UOC2_CON[2];
    uint32 reserved2;
    uint32 GRF_UOC3_CON[2];
    uint32 GRF_HSIC_STAT;
    uint32 GRF_OS_REG[8];
} REG_FILE_RK3066B, *pREG_FILE_RK3066B;

#define pGRF_Reg_RK3066B ((pREG_FILE_RK3066B)REG_FILE_BASE_ADDR)

//SCTL
#define INIT_STATE                     (0)
#define CFG_STATE                      (1)
#define GO_STATE                       (2)
#define SLEEP_STATE                    (3)
#define WAKEUP_STATE                   (4)

//STAT
#define Init_mem                       (0)
#define Config                         (1)
#define Config_req                     (2)
#define Access                         (3)
#define Access_req                     (4)
#define Low_power                      (5)
#define Low_power_entry_req            (6)
#define Low_power_exit_req             (7)

//MCFG
#define mddr_lpddr2_clk_stop_idle(n)   ((n)<<24)
#define pd_idle(n)                     ((n)<<8)
#define mddr_en                        (2<<22)
#define lpddr2_en                      (3<<22)
#define ddr2_en                        (0<<5)
#define ddr3_en                        (1<<5)
#define lpddr2_s2                      (0<<6)
#define lpddr2_s4                      (1<<6)
#define mddr_lpddr2_bl_2               (0<<20)
#define mddr_lpddr2_bl_4               (1<<20)
#define mddr_lpddr2_bl_8               (2<<20)
#define mddr_lpddr2_bl_16              (3<<20)
#define ddr2_ddr3_bl_4                 (0)
#define ddr2_ddr3_bl_8                 (1)
#define tfaw_cfg(n)                    (((n)-4)<<18)
#define pd_exit_slow                   (0<<17)
#define pd_exit_fast                   (1<<17)
#define pd_type(n)                     ((n)<<16)
#define two_t_en(n)                    ((n)<<3)
#define bl8int_en(n)                   ((n)<<2)
#define cke_or_en(n)                   ((n)<<1)

//POWCTL
#define power_up_start                 (1<<0)

//POWSTAT
#define power_up_done                  (1<<0)

//DFISTSTAT0
#define dfi_init_complete              (1<<0)

//CMDTSTAT
#define cmd_tstat                      (1<<0)

//CMDTSTATEN
#define cmd_tstat_en                   (1<<1)

//MCMD
#define Deselect_cmd                   (0)
#define PREA_cmd                       (1)
#define REF_cmd                        (2)
#define MRS_cmd                        (3)
#define ZQCS_cmd                       (4)
#define ZQCL_cmd                       (5)
#define RSTL_cmd                       (6)
#define MRR_cmd                        (8)
#define DPDE_cmd                       (9)

#define lpddr2_op(n)                   ((n)<<12)
#define lpddr2_ma(n)                   ((n)<<4)

#define bank_addr(n)                   ((n)<<17)
#define cmd_addr(n)                    ((n)<<4)

#define start_cmd                      (1u<<31)

typedef union STAT_Tag
{
    uint32 d32;
    struct
    {
        unsigned ctl_stat : 3;
        unsigned reserved3 : 1;
        unsigned lp_trig : 3;
        unsigned reserved7_31 : 25;
    }b;
}STAT_T;

typedef union SCFG_Tag
{
    uint32 d32;
    struct
    {
        unsigned hw_low_power_en : 1;
        unsigned reserved1_5 : 5;
        unsigned nfifo_nif1_dis : 1;
        unsigned reserved7 : 1;
        unsigned bbflags_timing : 4;
        unsigned reserved12_31 : 20;
    } b;
}SCFG_T;

/* DDR Controller register struct */
typedef volatile struct DDR_REG_Tag
{
    //Operational State, Control, and Status Registers
    SCFG_T SCFG;                   //State Configuration Register
    volatile uint32 SCTL;                   //State Control Register
    STAT_T STAT;                   //State Status Register
    volatile uint32 INTRSTAT;               //Interrupt Status Register
    uint32 reserved0[(0x40-0x10)/4];
    //Initailization Control and Status Registers
    volatile uint32 MCMD;                   //Memory Command Register
    volatile uint32 POWCTL;                 //Power Up Control Registers
    volatile uint32 POWSTAT;                //Power Up Status Register
    volatile uint32 CMDTSTAT;               //Command Timing Status Register
    volatile uint32 CMDTSTATEN;             //Command Timing Status Enable Register
    uint32 reserved1[(0x60-0x54)/4];
    volatile uint32 MRRCFG0;                //MRR Configuration 0 Register
    volatile uint32 MRRSTAT0;               //MRR Status 0 Register
    volatile uint32 MRRSTAT1;               //MRR Status 1 Register
    uint32 reserved2[(0x7c-0x6c)/4];
    //Memory Control and Status Registers
    volatile uint32 MCFG1;                  //Memory Configuration 1 Register
    volatile uint32 MCFG;                   //Memory Configuration Register
    volatile uint32 PPCFG;                  //Partially Populated Memories Configuration Register
    volatile uint32 MSTAT;                  //Memory Status Register
    volatile uint32 LPDDR2ZQCFG;            //LPDDR2 ZQ Configuration Register
    uint32 reserved3;
    //DTU Control and Status Registers
    volatile uint32 DTUPDES;                //DTU Status Register
    volatile uint32 DTUNA;                  //DTU Number of Random Addresses Created Register
    volatile uint32 DTUNE;                  //DTU Number of Errors Register
    volatile uint32 DTUPRD0;                //DTU Parallel Read 0
    volatile uint32 DTUPRD1;                //DTU Parallel Read 1
    volatile uint32 DTUPRD2;                //DTU Parallel Read 2
    volatile uint32 DTUPRD3;                //DTU Parallel Read 3
    volatile uint32 DTUAWDT;                //DTU Address Width
    uint32 reserved4[(0xc0-0xb4)/4];
    //Memory Timing Registers
    volatile uint32 TOGCNT1U;               //Toggle Counter 1U Register
    volatile uint32 TINIT;                  //t_init Timing Register
    volatile uint32 TRSTH;                  //Reset High Time Register
    volatile uint32 TOGCNT100N;             //Toggle Counter 100N Register
    volatile uint32 TREFI;                  //t_refi Timing Register
    volatile uint32 TMRD;                   //t_mrd Timing Register
    volatile uint32 TRFC;                   //t_rfc Timing Register
    volatile uint32 TRP;                    //t_rp Timing Register
    volatile uint32 TRTW;                   //t_rtw Timing Register
    volatile uint32 TAL;                    //AL Latency Register
    volatile uint32 TCL;                    //CL Timing Register
    volatile uint32 TCWL;                   //CWL Register
    volatile uint32 TRAS;                   //t_ras Timing Register
    volatile uint32 TRC;                    //t_rc Timing Register
    volatile uint32 TRCD;                   //t_rcd Timing Register
    volatile uint32 TRRD;                   //t_rrd Timing Register
    volatile uint32 TRTP;                   //t_rtp Timing Register
    volatile uint32 TWR;                    //t_wr Timing Register
    volatile uint32 TWTR;                   //t_wtr Timing Register
    volatile uint32 TEXSR;                  //t_exsr Timing Register
    volatile uint32 TXP;                    //t_xp Timing Register
    volatile uint32 TXPDLL;                 //t_xpdll Timing Register
    volatile uint32 TZQCS;                  //t_zqcs Timing Register
    volatile uint32 TZQCSI;                 //t_zqcsi Timing Register
    volatile uint32 TDQS;                   //t_dqs Timing Register
    volatile uint32 TCKSRE;                 //t_cksre Timing Register
    volatile uint32 TCKSRX;                 //t_cksrx Timing Register
    volatile uint32 TCKE;                   //t_cke Timing Register
    volatile uint32 TMOD;                   //t_mod Timing Register
    volatile uint32 TRSTL;                  //Reset Low Timing Register
    volatile uint32 TZQCL;                  //t_zqcl Timing Register
    volatile uint32 TMRR;                   //t_mrr Timing Register
    volatile uint32 TCKESR;                 //t_ckesr Timing Register
    volatile uint32 TDPD;                   //t_dpd Timing Register
    uint32 reserved5[(0x180-0x148)/4];
    //ECC Configuration, Control, and Status Registers
    volatile uint32 ECCCFG;                   //ECC Configuration Register
    volatile uint32 ECCTST;                   //ECC Test Register
    volatile uint32 ECCCLR;                   //ECC Clear Register
    volatile uint32 ECCLOG;                   //ECC Log Register
    uint32 reserved6[(0x200-0x190)/4];
    //DTU Control and Status Registers
    volatile uint32 DTUWACTL;                 //DTU Write Address Control Register
    volatile uint32 DTURACTL;                 //DTU Read Address Control Register
    volatile uint32 DTUCFG;                   //DTU Configuration Control Register
    volatile uint32 DTUECTL;                  //DTU Execute Control Register
    volatile uint32 DTUWD0;                   //DTU Write Data 0
    volatile uint32 DTUWD1;                   //DTU Write Data 1
    volatile uint32 DTUWD2;                   //DTU Write Data 2
    volatile uint32 DTUWD3;                   //DTU Write Data 3
    volatile uint32 DTUWDM;                   //DTU Write Data Mask
    volatile uint32 DTURD0;                   //DTU Read Data 0
    volatile uint32 DTURD1;                   //DTU Read Data 1
    volatile uint32 DTURD2;                   //DTU Read Data 2
    volatile uint32 DTURD3;                   //DTU Read Data 3
    volatile uint32 DTULFSRWD;                //DTU LFSR Seed for Write Data Generation
    volatile uint32 DTULFSRRD;                //DTU LFSR Seed for Read Data Generation
    volatile uint32 DTUEAF;                   //DTU Error Address FIFO
    //DFI Control Registers
    volatile uint32 DFITCTRLDELAY;            //DFI tctrl_delay Register
    volatile uint32 DFIODTCFG;                //DFI ODT Configuration Register
    volatile uint32 DFIODTCFG1;               //DFI ODT Configuration 1 Register
    volatile uint32 DFIODTRANKMAP;            //DFI ODT Rank Mapping Register
    //DFI Write Data Registers
    volatile uint32 DFITPHYWRDATA;            //DFI tphy_wrdata Register
    volatile uint32 DFITPHYWRLAT;             //DFI tphy_wrlat Register
    uint32 reserved7[(0x260-0x258)/4];
    volatile uint32 DFITRDDATAEN;             //DFI trddata_en Register
    volatile uint32 DFITPHYRDLAT;             //DFI tphy_rddata Register
    uint32 reserved8[(0x270-0x268)/4];
    //DFI Update Registers
    volatile uint32 DFITPHYUPDTYPE0;          //DFI tphyupd_type0 Register
    volatile uint32 DFITPHYUPDTYPE1;          //DFI tphyupd_type1 Register
    volatile uint32 DFITPHYUPDTYPE2;          //DFI tphyupd_type2 Register
    volatile uint32 DFITPHYUPDTYPE3;          //DFI tphyupd_type3 Register
    volatile uint32 DFITCTRLUPDMIN;           //DFI tctrlupd_min Register
    volatile uint32 DFITCTRLUPDMAX;           //DFI tctrlupd_max Register
    volatile uint32 DFITCTRLUPDDLY;           //DFI tctrlupd_dly Register
    uint32 reserved9;
    volatile uint32 DFIUPDCFG;                //DFI Update Configuration Register
    volatile uint32 DFITREFMSKI;              //DFI Masked Refresh Interval Register
    volatile uint32 DFITCTRLUPDI;             //DFI tctrlupd_interval Register
    uint32 reserved10[(0x2ac-0x29c)/4];
    volatile uint32 DFITRCFG0;                //DFI Training Configuration 0 Register
    volatile uint32 DFITRSTAT0;               //DFI Training Status 0 Register
    volatile uint32 DFITRWRLVLEN;             //DFI Training dfi_wrlvl_en Register
    volatile uint32 DFITRRDLVLEN;             //DFI Training dfi_rdlvl_en Register
    volatile uint32 DFITRRDLVLGATEEN;         //DFI Training dfi_rdlvl_gate_en Register
    //DFI Status Registers
    volatile uint32 DFISTSTAT0;               //DFI Status Status 0 Register
    volatile uint32 DFISTCFG0;                //DFI Status Configuration 0 Register
    volatile uint32 DFISTCFG1;                //DFI Status configuration 1 Register
    uint32 reserved11;
    volatile uint32 DFITDRAMCLKEN;            //DFI tdram_clk_enalbe Register
    volatile uint32 DFITDRAMCLKDIS;           //DFI tdram_clk_disalbe Register
    volatile uint32 DFISTCFG2;                //DFI Status configuration 2 Register
    volatile uint32 DFISTPARCLR;              //DFI Status Parity Clear Register
    volatile uint32 DFISTPARLOG;              //DFI Status Parity Log Register
    uint32 reserved12[(0x2f0-0x2e4)/4];
    //DFI Low Power Registers
    volatile uint32 DFILPCFG0;                //DFI Low Power Configuration 0 Register
    uint32 reserved13[(0x300-0x2f4)/4];
    //DFI Training 2 Registers
    volatile uint32 DFITRWRLVLRESP0;          //DFI Training dif_wrlvl_resp Status 0 Register
    volatile uint32 DFITRWRLVLRESP1;          //DFI Training dif_wrlvl_resp Status 1 Register
    volatile uint32 DFITRWRLVLRESP2;          //DFI Training dif_wrlvl_resp Status 2 Register
    volatile uint32 DFITRRDLVLRESP0;          //DFI Training dif_rdlvl_resp Status 0 Register
    volatile uint32 DFITRRDLVLRESP1;          //DFI Training dif_rdlvl_resp Status 1 Register
    volatile uint32 DFITRRDLVLRESP2;          //DFI Training dif_rdlvl_resp Status 2 Register
    volatile uint32 DFITRWRLVLDELAY0;         //DFI Training dif_wrlvl_delay Configuration 0 Register
    volatile uint32 DFITRWRLVLDELAY1;         //DFI Training dif_wrlvl_delay Configuration 1 Register
    volatile uint32 DFITRWRLVLDELAY2;         //DFI Training dif_wrlvl_delay Configuration 2 Register
    volatile uint32 DFITRRDLVLDELAY0;         //DFI Training dif_rdlvl_delay Configuration 0 Register
    volatile uint32 DFITRRDLVLDELAY1;         //DFI Training dif_rdlvl_delay Configuration 1 Register
    volatile uint32 DFITRRDLVLDELAY2;         //DFI Training dif_rdlvl_delay Configuration 2 Register
    volatile uint32 DFITRRDLVLGATEDELAY0;     //DFI Training dif_rdlvl_gate_delay Configuration 0 Register
    volatile uint32 DFITRRDLVLGATEDELAY1;     //DFI Training dif_rdlvl_gate_delay Configuration 1 Register
    volatile uint32 DFITRRDLVLGATEDELAY2;     //DFI Training dif_rdlvl_gate_delay Configuration 2 Register
    volatile uint32 DFITRCMD;                 //DFI Training Command Register
    uint32 reserved14[(0x3f8-0x340)/4];
    //IP Status Registers
    volatile uint32 IPVR;                     //IP Version Register
    volatile uint32 IPTR;                     //IP Type Register
}DDR_REG_T, *pDDR_REG_T;

#define pDDR_Reg ((pDDR_REG_T)SDRAMC_BASE_ADDR)

//PIR
#define INIT                 (1<<0)
#define DLLSRST              (1<<1)
#define DLLLOCK              (1<<2)
#define ZCAL                 (1<<3)
#define ITMSRST              (1<<4)
#define DRAMRST              (1<<5)
#define DRAMINIT             (1<<6)
#define QSTRN                (1<<7)
#define EYETRN               (1<<8)
#define ICPC                 (1<<16)
#define DLLBYP               (1<<17)
#define CTLDINIT             (1<<18)
#define CLRSR                (1<<28)
#define LOCKBYP              (1<<29)
#define ZCALBYP              (1<<30)
#define INITBYP              (1u<<31)

//PGCR
#define DFTLMT(n)            ((n)<<3)
#define DFTCMP(n)            ((n)<<2)
#define DQSCFG(n)            ((n)<<1)
#define ITMDMD(n)            ((n)<<0)
#define RANKEN(n)            ((n)<<18)

//PGSR
#define IDONE                (1<<0)
#define DLDONE               (1<<1)
#define ZCDONE               (1<<2)
#define DIDONE               (1<<3)
#define DTDONE               (1<<4)
#define DTERR                (1<<5)
#define DTIERR               (1<<6)
#define DFTERR               (1<<7)
#define TQ                   (1u<<31)

//PTR0
#define tITMSRST(n)          ((n)<<18)
#define tDLLLOCK(n)          ((n)<<6)
#define tDLLSRST(n)          ((n)<<0)

//PTR1
#define tDINIT1(n)           ((n)<<19)
#define tDINIT0(n)           ((n)<<0)

//PTR2
#define tDINIT3(n)           ((n)<<17)
#define tDINIT2(n)           ((n)<<0)

//DSGCR
#define DQSGE(n)             ((n)<<8)
#define DQSGX(n)             ((n)<<5)

typedef union DCR_Tag
{
    uint32 d32;
    struct
    {
        unsigned DDRMD : 3;
        unsigned DDR8BNK : 1;
        unsigned PDQ : 3;
        unsigned MPRDQ : 1;
        unsigned DDRTYPE : 2;
        unsigned reserved10_26 : 17;
        unsigned NOSRA : 1;
        unsigned DDR2T : 1;
        unsigned UDIMM : 1;
        unsigned RDIMM : 1;
        unsigned TPD : 1;
    } b;
}DCR_T;


typedef volatile struct DATX8_REG_Tag
{
    volatile uint32 DXGCR;                 //DATX8 General Configuration Register
    volatile uint32 DXGSR[2];              //DATX8 General Status Register
    volatile uint32 DXDLLCR;               //DATX8 DLL Control Register
    volatile uint32 DXDQTR;                //DATX8 DQ Timing Register
    volatile uint32 DXDQSTR;               //DATX8 DQS Timing Register
    uint32 reserved[0x80-0x76];
}DATX8_REG_T;

/* DDR PHY register struct */
typedef volatile struct DDRPHY_REG_Tag
{
    volatile uint32 RIDR;                   //Revision Identification Register
    volatile uint32 PIR;                    //PHY Initialization Register
    volatile uint32 PGCR;                   //PHY General Configuration Register
    volatile uint32 PGSR;                   //PHY General Status Register
    volatile uint32 DLLGCR;                 //DLL General Control Register
    volatile uint32 ACDLLCR;                //AC DLL Control Register
    volatile uint32 PTR[3];                 //PHY Timing Registers 0-2
    volatile uint32 ACIOCR;                 //AC I/O Configuration Register
    volatile uint32 DXCCR;                  //DATX8 Common Configuration Register
    volatile uint32 DSGCR;                  //DDR System General Configuration Register
    DCR_T DCR;                    //DRAM Configuration Register
    volatile uint32 DTPR[3];                //DRAM Timing Parameters Register 0-2
    volatile uint32 MR[4];                    //Mode Register 0-3
    volatile uint32 ODTCR;                  //ODT Configuration Register
    volatile uint32 DTAR;                   //Data Training Address Register
    volatile uint32 DTDR[2];                //Data Training Data Register 0-1

    uint32 reserved1[0x30-0x18];
    uint32 DCU[0x38-0x30];
    uint32 reserved2[0x40-0x38];
    uint32 BIST[0x51-0x40];
    uint32 reserved3[0x60-0x51];

    volatile uint32 ZQ0CR[2];               //ZQ 0 Impedance Control Register 0-1
    volatile uint32 ZQ0SR[2];               //ZQ 0 Impedance Status Register 0-1
    volatile uint32 ZQ1CR[2];               //ZQ 1 Impedance Control Register 0-1
    volatile uint32 ZQ1SR[2];               //ZQ 1 Impedance Status Register 0-1
    volatile uint32 ZQ2CR[2];               //ZQ 2 Impedance Control Register 0-1
    volatile uint32 ZQ2SR[2];               //ZQ 2 Impedance Status Register 0-1
    volatile uint32 ZQ3CR[2];               //ZQ 3 Impedance Control Register 0-1
    volatile uint32 ZQ3SR[2];               //ZQ 3 Impedance Status Register 0-1

    DATX8_REG_T     DATX8[9];               //DATX8 Register
}DDRPHY_REG_T, *pDDRPHY_REG_T;

#define pPHY_Reg ((pDDRPHY_REG_T)DDR_PUBL_BASE)

typedef enum DRAM_TYPE_Tag
{
    LPDDR = 0,
    DDR,
    DDR2,
    DDR3,
    LPDDR2,

    DRAM_MAX
}DRAM_TYPE;

typedef struct PCTRL_TIMING_Tag
{
    uint32 ddrFreq;
    //Memory Timing Registers
    uint32 togcnt1u;               //Toggle Counter 1U Register
    uint32 tinit;                  //t_init Timing Register
    uint32 trsth;                  //Reset High Time Register
    uint32 togcnt100n;             //Toggle Counter 100N Register
    uint32 trefi;                  //t_refi Timing Register
    uint32 tmrd;                   //t_mrd Timing Register
    uint32 trfc;                   //t_rfc Timing Register
    uint32 trp;                    //t_rp Timing Register
    uint32 trtw;                   //t_rtw Timing Register
    uint32 tal;                    //AL Latency Register
    uint32 tcl;                    //CL Timing Register
    uint32 tcwl;                   //CWL Register
    uint32 tras;                   //t_ras Timing Register
    uint32 trc;                    //t_rc Timing Register
    uint32 trcd;                   //t_rcd Timing Register
    uint32 trrd;                   //t_rrd Timing Register
    uint32 trtp;                   //t_rtp Timing Register
    uint32 twr;                    //t_wr Timing Register
    uint32 twtr;                   //t_wtr Timing Register
    uint32 texsr;                  //t_exsr Timing Register
    uint32 txp;                    //t_xp Timing Register
    uint32 txpdll;                 //t_xpdll Timing Register
    uint32 tzqcs;                  //t_zqcs Timing Register
    uint32 tzqcsi;                 //t_zqcsi Timing Register
    uint32 tdqs;                   //t_dqs Timing Register
    uint32 tcksre;                 //t_cksre Timing Register
    uint32 tcksrx;                 //t_cksrx Timing Register
    uint32 tcke;                   //t_cke Timing Register
    uint32 tmod;                   //t_mod Timing Register
    uint32 trstl;                  //Reset Low Timing Register
    uint32 tzqcl;                  //t_zqcl Timing Register
    uint32 tmrr;                   //t_mrr Timing Register
    uint32 tckesr;                 //t_ckesr Timing Register
    uint32 tdpd;                   //t_dpd Timing Register
}PCTL_TIMING_T;

typedef union DTPR_0_Tag
{
    uint32 d32;
    struct 
    {
        unsigned tMRD : 2;
        unsigned tRTP : 3;
        unsigned tWTR : 3;
        unsigned tRP : 4;
        unsigned tRCD : 4;
        unsigned tRAS : 5;
        unsigned tRRD : 4;
        unsigned tRC : 6;
        unsigned tCCD : 1;
    } b;
}DTPR_0_T;

typedef union DTPR_1_Tag
{
    uint32 d32;
    struct 
    {
        unsigned tAOND : 2;
        unsigned tRTW : 1;
        unsigned tFAW : 6;
        unsigned tMOD : 2;
        unsigned tRTODT : 1;
        unsigned reserved12_15 : 4;
        unsigned tRFC : 8;
        unsigned tDQSCK : 3;
        unsigned tDQSCKmax : 3;
        unsigned reserved30_31 : 2;
    } b;
}DTPR_1_T;

typedef union DTPR_2_Tag
{
    uint32 d32;
    struct 
    {
        unsigned tXS : 10;
        unsigned tXP : 5;
        unsigned tCKE : 4;
        unsigned tDLLK : 10;
        unsigned reserved29_31 : 3;
    } b;
}DTPR_2_T;

typedef struct PHY_TIMING_Tag
{
    DTPR_0_T  dtpr0;
    DTPR_1_T  dtpr1;
    DTPR_2_T  dtpr2;
    uint32    mr[4];   //LPDDR2 no MR0, mr[2] is mDDR MR1
}PHY_TIMING_T;

typedef union NOC_TIMING_Tag
{
    uint32 d32;
    struct 
    {
        unsigned ActToAct : 6;
        unsigned RdToMiss : 6;
        unsigned WrToMiss : 6;
        unsigned BurstLen : 3;
        unsigned RdToWr : 5;
        unsigned WrToRd : 5;
        unsigned BwRatio : 1;
    } b;
}NOC_TIMING_T;

typedef struct PCTL_REG_Tag
{
    uint32 SCFG;
    uint32 CMDTSTATEN;
    uint32 MCFG1;
    uint32 MCFG;
    PCTL_TIMING_T pctl_timing;
    //DFI Control Registers
    uint32 DFITCTRLDELAY;
    uint32 DFIODTCFG;
    uint32 DFIODTCFG1;
    uint32 DFIODTRANKMAP;
    //DFI Write Data Registers
    uint32 DFITPHYWRDATA;
    uint32 DFITPHYWRLAT;
    //DFI Read Data Registers
    uint32 DFITRDDATAEN;
    uint32 DFITPHYRDLAT;
    //DFI Update Registers
    uint32 DFITPHYUPDTYPE0;
    uint32 DFITPHYUPDTYPE1;
    uint32 DFITPHYUPDTYPE2;
    uint32 DFITPHYUPDTYPE3;
    uint32 DFITCTRLUPDMIN;
    uint32 DFITCTRLUPDMAX;
    uint32 DFITCTRLUPDDLY;
    uint32 DFIUPDCFG;
    uint32 DFITREFMSKI;
    uint32 DFITCTRLUPDI;
    //DFI Status Registers
    uint32 DFISTCFG0;
    uint32 DFISTCFG1;
    uint32 DFITDRAMCLKEN;
    uint32 DFITDRAMCLKDIS;
    uint32 DFISTCFG2;
    //DFI Low Power Register
    uint32 DFILPCFG0;
}PCTL_REG_T;

typedef struct PUBL_REG_Tag
{
    uint32 PIR;
    uint32 PGCR;
    uint32 DLLGCR;
    uint32 ACDLLCR;
    uint32 PTR[3];
    uint32 ACIOCR;
    uint32 DXCCR;
    uint32 DSGCR;
    uint32 DCR;
    PHY_TIMING_T phy_timing;
    uint32 ODTCR;
    uint32 DTAR;
    uint32 ZQ0CR0;
    uint32 ZQ1CR0;
    
    uint32 DX0GCR;
    uint32 DX0DLLCR;
    uint32 DX0DQTR;
    uint32 DX0DQSTR;

    uint32 DX1GCR;
    uint32 DX1DLLCR;
    uint32 DX1DQTR;
    uint32 DX1DQSTR;

    uint32 DX2GCR;
    uint32 DX2DLLCR;
    uint32 DX2DQTR;
    uint32 DX2DQSTR;

    uint32 DX3GCR;
    uint32 DX3DLLCR;
    uint32 DX3DQTR;
    uint32 DX3DQSTR;
}PUBL_REG_T;

typedef struct BACKUP_REG_Tag
{
    PCTL_REG_T pctl;
    PUBL_REG_T publ;
    uint32 DdrConf;
    NOC_TIMING_T noc_timing;
    uint32 DdrMode;
    uint32 ReadLatency;
}BACKUP_REG_T;

#define READ_CS_INFO()   ((((pPMU_Reg->PMU_PMU_SYS_REG[2])>>11)&0x1)+1)
#define READ_BW_INFO()   (2>>(((pPMU_Reg->PMU_PMU_SYS_REG[2])>>2)&0x3))
#define READ_COL_INFO()  (9+(((pPMU_Reg->PMU_PMU_SYS_REG[2])>>9)&0x3))
#define READ_BK_INFO()   (3-(((pPMU_Reg->PMU_PMU_SYS_REG[2])>>8)&0x1))
#define READ_CS0_ROW_INFO()  (13+(((pPMU_Reg->PMU_PMU_SYS_REG[2])>>6)&0x3))
#define READ_CS1_ROW_INFO()  (13+(((pPMU_Reg->PMU_PMU_SYS_REG[2])>>4)&0x3))        
#define READ_DIE_BW_INFO()   (2>>(pPMU_Reg->PMU_PMU_SYS_REG[2]&0x3))

__sramdata BACKUP_REG_T ddr_reg;

uint8_t  ddr_cfg_2_rbc[] =
{
    /****************************/
    // [7:6]  bank(n:n bit bank)
    // [5:4]  row(13+n)
    // [3:2]  bank(n:n bit bank)
    // [1:0]  col(9+n)
    /****************************/
    //bank, row,    bank,  col
    ((3<<6)|(2<<4)|(0<<2)|2),  // 0 bank ahead
    ((0<<6)|(2<<4)|(3<<2)|1),  // 1
    ((0<<6)|(1<<4)|(3<<2)|1),  // 2
    ((0<<6)|(0<<4)|(3<<2)|1),  // 3
    ((0<<6)|(2<<4)|(3<<2)|2),  // 4
    ((0<<6)|(1<<4)|(3<<2)|2),  // 5
    ((0<<6)|(0<<4)|(3<<2)|2),  // 6
    ((0<<6)|(1<<4)|(3<<2)|0),  // 7
    ((0<<6)|(0<<4)|(3<<2)|0),  // 8
    ((1<<6)|(2<<4)|(2<<2)|2),  // 9
    ((1<<6)|(1<<4)|(2<<2)|2),  // 10
    ((1<<6)|(2<<4)|(2<<2)|1),  // 11
    ((1<<6)|(1<<4)|(2<<2)|1),  // 12
    ((1<<6)|(2<<4)|(2<<2)|0),  // 13
    ((1<<6)|(1<<4)|(2<<2)|0),  // 14
    ((3<<6)|(2<<4)|(0<<2)|1),  // 15 bank ahead
};

__attribute__((aligned(4096))) uint32_t ddr_data_training_buf[32];

uint8_t __sramdata ddr3_cl_cwl[22][4]={
/*   0~330           330~400         400~533        speed
* tCK  >3             2.5~3          1.875~2.5      1.5~1.875
*    cl<<4, cwl     cl<<4, cwl    cl<<4, cwl              */
    {((5<<4)|5),   ((5<<4)|5),    0          ,   0}, //DDR3_800D
    {((5<<4)|5),   ((6<<4)|5),    0          ,   0}, //DDR3_800E

    {((5<<4)|5),   ((5<<4)|5),    ((6<<4)|6),   0}, //DDR3_1066E
    {((5<<4)|5),   ((6<<4)|5),    ((7<<4)|6),   0}, //DDR3_1066F
    {((5<<4)|5),   ((6<<4)|5),    ((8<<4)|6),   0}, //DDR3_1066G

    {((5<<4)|5),   ((5<<4)|5),    ((6<<4)|6),   ((7<<4)|7)}, //DDR3_1333F
    {((5<<4)|5),   ((5<<4)|5),    ((7<<4)|6),   ((8<<4)|7)}, //DDR3_1333G
    {((5<<4)|5),   ((6<<4)|5),    ((8<<4)|6),   ((9<<4)|7)}, //DDR3_1333H
    {((5<<4)|5),   ((6<<4)|5),    ((8<<4)|6),   ((10<<4)|7)}, //DDR3_1333J

    {((5<<4)|5),   ((5<<4)|5),    ((6<<4)|6),   ((7<<4)|7)}, //DDR3_1600G
    {((5<<4)|5),   ((5<<4)|5),    ((6<<4)|6),   ((8<<4)|7)}, //DDR3_1600H
    {((5<<4)|5),   ((5<<4)|5),    ((7<<4)|6),   ((9<<4)|7)}, //DDR3_1600J
    {((5<<4)|5),   ((6<<4)|5),    ((7<<4)|6),   ((10<<4)|7)}, //DDR3_1600K

    {((5<<4)|5),   ((5<<4)|5),    ((6<<4)|6),   ((8<<4)|7)}, //DDR3_1866J
    {((5<<4)|5),   ((5<<4)|5),    ((7<<4)|6),   ((8<<4)|7)}, //DDR3_1866K
    {((6<<4)|5),   ((6<<4)|5),    ((7<<4)|6),   ((9<<4)|7)}, //DDR3_1866L
    {((6<<4)|5),   ((6<<4)|5),    ((8<<4)|6),   ((10<<4)|7)}, //DDR3_1866M

    {((5<<4)|5),   ((5<<4)|5),    ((6<<4)|6),   ((7<<4)|7)}, //DDR3_2133K
    {((5<<4)|5),   ((5<<4)|5),    ((6<<4)|6),   ((8<<4)|7)}, //DDR3_2133L
    {((5<<4)|5),   ((5<<4)|5),    ((7<<4)|6),   ((9<<4)|7)}, //DDR3_2133M
    {((6<<4)|5),   ((6<<4)|5),    ((7<<4)|6),   ((9<<4)|7)},  //DDR3_2133N

    {((6<<4)|5),   ((6<<4)|5),    ((8<<4)|6),   ((10<<4)|7)} //DDR3_DEFAULT
};

uint16_t __sramdata ddr3_tRC_tFAW[22]={
/**    tRC    tFAW   */
    ((50<<8)|50), //DDR3_800D
    ((53<<8)|50), //DDR3_800E

    ((49<<8)|50), //DDR3_1066E
    ((51<<8)|50), //DDR3_1066F
    ((53<<8)|50), //DDR3_1066G

    ((47<<8)|45), //DDR3_1333F
    ((48<<8)|45), //DDR3_1333G
    ((50<<8)|45), //DDR3_1333H
    ((51<<8)|45), //DDR3_1333J

    ((45<<8)|40), //DDR3_1600G
    ((47<<8)|40), //DDR3_1600H
    ((48<<8)|40), //DDR3_1600J
    ((49<<8)|40), //DDR3_1600K

    ((45<<8)|35), //DDR3_1866J
    ((46<<8)|35), //DDR3_1866K
    ((47<<8)|35), //DDR3_1866L
    ((48<<8)|35), //DDR3_1866M

    ((44<<8)|35), //DDR3_2133K
    ((45<<8)|35), //DDR3_2133L
    ((46<<8)|35), //DDR3_2133M
    ((47<<8)|35), //DDR3_2133N

    ((53<<8)|50)  //DDR3_DEFAULT
};

__sramdata uint32_t mem_type;    // 0:LPDDR, 1:DDR, 2:DDR2, 3:DDR3, 4:LPDDR2
static __sramdata uint32_t ddr_speed_bin;    // used for ddr3 only
static __sramdata uint32_t ddr_capability_per_die;  // one chip cs capability
static __sramdata uint32_t ddr_freq;
static __sramdata uint32_t ddr_sr_idle;

/****************************************************************************
Internal sram us delay function
Cpu highest frequency is 1.6 GHz
1 cycle = 1/1.6 ns
1 us = 1000 ns = 1000 * 1.6 cycles = 1600 cycles
*****************************************************************************/
static __sramdata volatile uint32_t loops_per_us;

#define LPJ_100MHZ  999456UL

/*static*/ void __sramlocalfunc ddr_delayus(uint32_t us)
{
    do
    {
        unsigned int i = (loops_per_us*us);
        if (i < 7) i = 7;
        barrier();
        asm volatile(".align 4; 1: subs %0, %0, #1; bne 1b;" : "+r" (i));
    } while (0);
}

__sramfunc void ddr_copy(uint32 *pDest, uint32 *pSrc, uint32 words)
{
    uint32 i;

    for(i=0; i<words; i++)
    {
        pDest[i] = pSrc[i];
    }
}

uint32 ddr_get_row(void)
{
    uint32 i;
    uint32 row;

    if(pPMU_Reg->PMU_PMU_SYS_REG[2])
    {
        row=READ_CS0_ROW_INFO();
    }
    else
    {
        i = *(volatile uint32*)SysSrv_DdrConf;
        row = 13+((ddr_cfg_2_rbc[i]>>4)&0x3);
        if(pGRF_Reg->GRF_SOC_CON[2] &  (1<<1))
        {
            row += 1;
        }
    }
    return row;
}

uint32 ddr_get_bank(void)
{
    uint32 i;
    uint32 bank;

    if(pPMU_Reg->PMU_PMU_SYS_REG[2])
    {
        bank = READ_BK_INFO();
    }
    else
    {
        i = *(volatile uint32*)SysSrv_DdrConf;
        bank = ((ddr_cfg_2_rbc[i]>>6)&0x3) + ((ddr_cfg_2_rbc[i]>>2)&0x3);
        if(pGRF_Reg->GRF_SOC_CON[2] &  (1<<2))
        {
            bank -= 1;
        }
    }
    return bank;
}

uint32 ddr_get_col(void)
{
    uint32 i;
    uint32 col;

    if(pPMU_Reg->PMU_PMU_SYS_REG[2])
    {
        col=READ_COL_INFO();
    }
    else
    {
        i = *(volatile uint32*)SysSrv_DdrConf;
        col = 9+(ddr_cfg_2_rbc[i]&0x3);
        if(pDDR_Reg->PPCFG & 1)
        {
            col +=1;
        }
    }
    return col;
}

uint32 ddr_get_bw(void)
{
    uint32 bw;

    if(pDDR_Reg->PPCFG & 1)
    {
        bw=1;
    }
    else
    {
        bw=2;
    }
    return bw;
}

uint32 ddr_get_cs(void)
{
    uint32 cs;
    
    switch((pPHY_Reg->PGCR>>18) & 0xF)
    {
        case 0xF:
            cs = 4;
        case 7:
            cs = 3;
            break;
        case 3:
            cs = 2;
            break;
        default:
            cs = 1;
            break;
    }
    return cs;
}

uint32_t ddr_get_datatraing_addr(void)
{
    uint32_t          value=0;
    uint32_t          addr;
    uint32_t          col = 0;
    uint32_t          row = 0;
    uint32_t          bank = 0;
    uint32_t          bw = 0;
    uint32_t          i;
    
    // caculate aglined physical address 
    addr =  __pa((unsigned long)ddr_data_training_buf);
    if(addr&0x3F)
    {
        addr += (64-(addr&0x3F));
    }
    addr -= 0x60000000;
    // find out col��row��bank
    row = ddr_get_row();
    bank = ddr_get_bank();
    col = ddr_get_col();
    bw = ddr_get_bw();
    // according different address mapping, caculate DTAR register value
    i = (*(volatile uint32*)SysSrv_DdrConf);
    value |= (addr>>bw) & ((0x1<<col)-1);  // col
    if(row==16)
    {
        value |= ((addr>>(bw+col+((ddr_cfg_2_rbc[i]>>2)&0x3))) & 0x7FFF) << 12;  // row
        value |= (((addr>>(bw+col+bank+15))&0x1)<<15)<<12;
        row = 15;  //use for bank
    }
    else
    {
        value |= ((addr>>(bw+col+((ddr_cfg_2_rbc[i]>>2)&0x3))) & ((0x1<<row)-1)) << 12;  // row
    }
    if(((ddr_cfg_2_rbc[i]>>6)&0x3)==1)
    {
        value |= (((addr>>(bw+col)) & 0x3) << 28)
                 | (((addr>>(bw+col+2+row)) & (bank-2))  << 30);  // bank
    }
    else if(((ddr_cfg_2_rbc[i]>>6)&0x3)==3)
    {
        value |= (((addr>>(bw+col+row)) & ((0x1<<bank)-1))  << 28);  // bank
    }
    else
    {
        value |= (((addr>>(bw+col)) & 0x7) << 28);  // bank
    }

    return value;
}

__sramlocalfunc void ddr_reset_dll(void)
{
    pPHY_Reg->ACDLLCR &= ~0x40000000;
    pPHY_Reg->DATX8[0].DXDLLCR &= ~0x40000000;
    pPHY_Reg->DATX8[1].DXDLLCR &= ~0x40000000;
    if(!(pDDR_Reg->PPCFG & 1))
    {
        pPHY_Reg->DATX8[2].DXDLLCR &= ~0x40000000;
        pPHY_Reg->DATX8[3].DXDLLCR &= ~0x40000000;
    }
    ddr_delayus(1);
    pPHY_Reg->ACDLLCR |= 0x40000000;
    pPHY_Reg->DATX8[0].DXDLLCR |= 0x40000000;
    pPHY_Reg->DATX8[1].DXDLLCR |= 0x40000000;
    if(!(pDDR_Reg->PPCFG & 1))
    {
        pPHY_Reg->DATX8[2].DXDLLCR |= 0x40000000;
        pPHY_Reg->DATX8[3].DXDLLCR |= 0x40000000;
    }
    ddr_delayus(1);
}

__sramfunc void ddr_move_to_Lowpower_state(void)
{
    volatile uint32 value;

    while(1)
    {
        value = pDDR_Reg->STAT.b.ctl_stat;
        if(value == Low_power)
        {
            break;
        }
        switch(value)
        {
            case Init_mem:
                pDDR_Reg->SCTL = CFG_STATE;
                dsb();
                while((pDDR_Reg->STAT.b.ctl_stat) != Config);
            case Config:
                pDDR_Reg->SCTL = GO_STATE;
                dsb();
                while((pDDR_Reg->STAT.b.ctl_stat) != Access);
            case Access:
                pDDR_Reg->SCTL = SLEEP_STATE;
                dsb();
                while((pDDR_Reg->STAT.b.ctl_stat) != Low_power);
                break;
            default:  //Transitional state
                break;
        }
    }
}

__sramfunc void ddr_move_to_Access_state(void)
{
    volatile uint32 value;

    //set auto self-refresh idle
    pDDR_Reg->MCFG1=(pDDR_Reg->MCFG1&0xffffff00)|ddr_sr_idle | (1<<31);
    dsb();

    while(1)
    {
        value = pDDR_Reg->STAT.b.ctl_stat;
        if((value == Access)
           || ((pDDR_Reg->STAT.b.lp_trig == 1) && ((pDDR_Reg->STAT.b.ctl_stat) == Low_power)))
        {
            break;
        }
        switch(value)
        {
            case Low_power:
                pDDR_Reg->SCTL = WAKEUP_STATE;
                dsb();
                while((pDDR_Reg->STAT.b.ctl_stat) != Access);
                while((pPHY_Reg->PGSR & DLDONE) != DLDONE);  //wait DLL lock
                break;
            case Init_mem:
                pDDR_Reg->SCTL = CFG_STATE;
                dsb();
                while((pDDR_Reg->STAT.b.ctl_stat) != Config);
            case Config:
                pDDR_Reg->SCTL = GO_STATE;
                dsb();
                while(!(((pDDR_Reg->STAT.b.ctl_stat) == Access)
                      || ((pDDR_Reg->STAT.b.lp_trig == 1) && ((pDDR_Reg->STAT.b.ctl_stat) == Low_power))));
                break;
            default:  //Transitional state
                break;
        }
    }

    pGRF_Reg->GRF_SOC_CON[2] = (1<<16 | 0);//de_hw_wakeup :enable auto sr if sr_idle != 0
}

__sramfunc void ddr_move_to_Config_state(void)
{
    volatile uint32 value;
    pGRF_Reg->GRF_SOC_CON[2] = (1<<16 | 1); //hw_wakeup :disable auto sr
	dsb();

    while(1)
    {
        value = pDDR_Reg->STAT.b.ctl_stat;
        if(value == Config)
        {
            break;
        }
        switch(value)
        {
            case Low_power:
                pDDR_Reg->SCTL = WAKEUP_STATE;
                dsb();
                while((pDDR_Reg->STAT.b.ctl_stat) != Access);
                while((pPHY_Reg->PGSR & DLDONE) != DLDONE);  //wait DLL lock
            case Access:
            case Init_mem:
                pDDR_Reg->SCTL = CFG_STATE;
                dsb();
                while((pDDR_Reg->STAT.b.ctl_stat) != Config);
                break;
            default:  //Transitional state
                break;
        }
    }
}

//arg����bank_addr��cmd_addr
void __sramlocalfunc ddr_send_command(uint32 rank, uint32 cmd, uint32 arg)
{
    pDDR_Reg->MCMD = (start_cmd | (rank<<20) | arg | cmd);
    dsb();
    while(pDDR_Reg->MCMD & start_cmd);
}

//��type���͵�DDR�ļ���cs����DTT
//0  DTT�ɹ�
//!0 DTTʧ��
uint32_t __sramlocalfunc ddr_data_training(void)
{
    uint32 value,cs,i,byte=2;

    // disable auto refresh
    value = pDDR_Reg->TREFI;
    pDDR_Reg->TREFI = 0;
    dsb();
    if(mem_type != LPDDR2)
    {
        // passive window
        pPHY_Reg->PGCR |= (1<<1);    
    }
    // clear DTDONE status
    pPHY_Reg->PIR |= CLRSR;
    cs = ((pPHY_Reg->PGCR>>18) & 0xF);
    pPHY_Reg->PGCR = (pPHY_Reg->PGCR & (~(0xF<<18))) | (1<<18);  //use cs0 dtt
    // trigger DTT
    pPHY_Reg->PIR |= INIT | QSTRN | LOCKBYP | ZCALBYP | CLRSR | ICPC;
    dsb();
    // wait echo byte DTDONE
    while((pPHY_Reg->DATX8[0].DXGSR[0] & 1) != 1);
    while((pPHY_Reg->DATX8[1].DXGSR[0] & 1) != 1);
    if(!(pDDR_Reg->PPCFG & 1))
    {
        while((pPHY_Reg->DATX8[2].DXGSR[0] & 1) != 1);
        while((pPHY_Reg->DATX8[3].DXGSR[0] & 1) != 1);  
        byte=4;
    }
    pPHY_Reg->PGCR = (pPHY_Reg->PGCR & (~(0xF<<18))) | (cs<<18);  //restore cs
    for(i=0;i<byte;i++)
    {
        pPHY_Reg->DATX8[i].DXDQSTR = (pPHY_Reg->DATX8[i].DXDQSTR & (~((0x7<<3)|(0x3<<14)))) 
                                      | ((pPHY_Reg->DATX8[i].DXDQSTR & 0x7)<<3)
                                      | (((pPHY_Reg->DATX8[i].DXDQSTR>>12) & 0x3)<<14);
    }
    // send some auto refresh to complement the lost while DTT��//�⵽1��CS��DTT�ʱ����10.7us����ಹ2��ˢ��
    if(cs > 1)
    {
        ddr_send_command(cs, REF_cmd, 0);
        ddr_send_command(cs, REF_cmd, 0);
        ddr_send_command(cs, REF_cmd, 0);
        ddr_send_command(cs, REF_cmd, 0);
    }
    else
    {
        ddr_send_command(cs, REF_cmd, 0);
        ddr_send_command(cs, REF_cmd, 0);
    }
    if(mem_type != LPDDR2)
    {
        // active window
        pPHY_Reg->PGCR &= ~(1<<1);
    }
    // resume auto refresh
    pDDR_Reg->TREFI = value;

    if(pPHY_Reg->PGSR & DTERR)
    {
        return (-1);
    }
    else
    {
        return 0;
    }
}

void __sramlocalfunc ddr_set_dll_bypass(uint32 freq)
{
    if(freq<=150)
    {
        pPHY_Reg->DLLGCR &= ~(1<<23);
        pPHY_Reg->ACDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[0].DXDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[1].DXDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[2].DXDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[3].DXDLLCR |= 0x80000000;
        pPHY_Reg->PIR |= DLLBYP;
    }
    else if(freq<=250)
    {
        pPHY_Reg->DLLGCR |= (1<<23);
        pPHY_Reg->ACDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[0].DXDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[1].DXDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[2].DXDLLCR |= 0x80000000;
        pPHY_Reg->DATX8[3].DXDLLCR |= 0x80000000;
        pPHY_Reg->PIR |= DLLBYP;
    }
    else
    {
        pPHY_Reg->DLLGCR &= ~(1<<23);
        pPHY_Reg->ACDLLCR &= ~0x80000000;
        pPHY_Reg->DATX8[0].DXDLLCR &= ~0x80000000;
        pPHY_Reg->DATX8[1].DXDLLCR &= ~0x80000000;
        if(!(pDDR_Reg->PPCFG & 1))
        {
            pPHY_Reg->DATX8[2].DXDLLCR &= ~0x80000000;
            pPHY_Reg->DATX8[3].DXDLLCR &= ~0x80000000;
        }
        pPHY_Reg->PIR &= ~DLLBYP;
    }
}

static __sramdata uint32_t clkr;
static __sramdata uint32_t clkf;
static __sramdata uint32_t clkod;

static __sramdata uint32_t dpllvaluel=0,gpllvaluel=0;
static __sramdata uint32_t ddr_select_gpll_div=0; // 0-Disable, 1-1:1, 2-2:1, 4-4:1

static __sramdata bool ddr_soc_is_rk3188_plus=false;

typedef enum PLL_ID_Tag
{
    APLL=0,
    DPLL,
    CPLL,
    GPLL,    
    PLL_MAX
}PLL_ID;

uint32_t __sramlocalfunc ddr_get_pll_freq(PLL_ID pll_id)   //APLL-1;CPLL-2;DPLL-3;GPLL-4
{
    uint32_t ret = 0;

     // freq = (Fin/NR)*NF/OD
    if(((pCRU_Reg->CRU_MODE_CON>>(pll_id*4))&3) == 1)             // DPLL Normal mode
        ret= 24 *((pCRU_Reg->CRU_PLL_CON[pll_id][1]&0xffff)+1)    // NF = 2*(CLKF+1)
                /((((pCRU_Reg->CRU_PLL_CON[pll_id][0]>>8)&0x3f)+1)           // NR = CLKR+1
                *((pCRU_Reg->CRU_PLL_CON[pll_id][0]&0x3F)+1));             // OD = 2^CLKOD
    else
        ret = 24;
    
    return ret;
}

static __sramdata bool ddr_rk3188_dpll_is_good=true;
#if defined(CONFIG_ARCH_RK3188)
bool ddr_get_dpll_status(void) //CPLL or DPLL bad rerurn false;good return true;
{
    if (rk_pll_flag() & 0x3)
        return false;
    else
        return true;
}
#endif

uint32_t __sramlocalfunc ddr_set_pll(uint32_t nMHz, uint32_t set)
{
    uint32_t ret = 0;
    int delay = 1000;
    uint32 pull;
    //NOһ��Ҫż��,NR����С��jitter�ͻ�С
    
    if(nMHz == 24)
    {
        ret = 24;
        goto out;
    }
    
    if(!set)
    {
        if(nMHz <= 100)
        {
            clkod = 8;
        }
        else if(nMHz <= 200)
        {
            clkod = 6;
        }
        else if(nMHz <= 300)
        {
            clkod = 4;
        }
        else if(nMHz <= 500)
        {
            clkod = 2;
        }
        else
        {
            clkod = 1;
        }
        clkr = 1;
        clkf=(nMHz*clkr*clkod)/24;
        ret = (24*clkf)/(clkr*clkod);
    }
    else
    {
        pull = *(volatile uint32 *)(REG_FILE_BASE_ADDR + 0x1cc);		
        pull = pull & (1<<(pull&0xf));
        pCRU_Reg->CRU_MODE_CON = (0x3<<20) | (0x0<<4);            
        dsb();
        pCRU_Reg->CRU_PLL_CON[1][3] = PLL_RESET;
        pCRU_Reg->CRU_PLL_CON[1][0] = NR(clkr) | NO(clkod);
        pCRU_Reg->CRU_PLL_CON[1][1] = NF(clkf);
        pCRU_Reg->CRU_PLL_CON[1][2] = NB((clkf>>1),pull);
        ddr_delayus(1);
        pCRU_Reg->CRU_PLL_CON[1][3] = PLL_DE_RESET;
        dsb();
        while (delay > 0)
        {
            ddr_delayus(1);
            if (pGRF_Reg->GRF_SOC_STATUS0 & (0x1<<4))
            break;
            delay--;
         }
        pCRU_Reg->CRU_MODE_CON = (0x3<<20)  | (0x1<<4);            //PLL normal
        dsb();
    }
out:
    return ret;
}

uint32_t ddr_get_parameter(uint32_t nMHz)
{
    uint32_t tmp;
    uint32_t ret = 0;
    uint32_t al;
    uint32_t bl,bl_tmp;
    uint32_t cl;
    uint32_t cwl;
    PCTL_TIMING_T *p_pctl_timing=&(ddr_reg.pctl.pctl_timing);
    PHY_TIMING_T  *p_publ_timing=&(ddr_reg.publ.phy_timing);
    NOC_TIMING_T  *p_noc_timing=&(ddr_reg.noc_timing);

    p_pctl_timing->togcnt1u = nMHz;
    p_pctl_timing->togcnt100n = nMHz/10;
    p_pctl_timing->tinit = 200;
    p_pctl_timing->trsth = 500;

    if(mem_type == DDR3)
    {
        if(ddr_speed_bin > DDR3_DEFAULT){
            ret = -1;
            goto out;
        }

        #define DDR3_tREFI_7_8_us    (78)  //unit 100ns
        #define DDR3_tMRD            (4)   //tCK
        #define DDR3_tRFC_512Mb      (90)  //ns
        #define DDR3_tRFC_1Gb        (110) //ns
        #define DDR3_tRFC_2Gb        (160) //ns
        #define DDR3_tRFC_4Gb        (300) //ns
        #define DDR3_tRFC_8Gb        (350) //ns
        #define DDR3_tRTW            (2)   //register min valid value
        #define DDR3_tRAS            (37)  //ns
        #define DDR3_tRRD            (10)  //ns
        #define DDR3_tRTP            (7)   //ns
        #define DDR3_tWR             (15)  //ns
        #define DDR3_tWTR            (7)   //ns
        #define DDR3_tXP             (7)   //ns
        #define DDR3_tXPDLL          (24)  //ns
        #define DDR3_tZQCS           (80)  //ns
        #define DDR3_tZQCSI          (0)   //ns
        #define DDR3_tDQS            (1)   //tCK
        #define DDR3_tCKSRE          (10)  //ns
        #define DDR3_tCKE_400MHz     (7)   //ns
        #define DDR3_tCKE_533MHz     (6)   //ns
        #define DDR3_tMOD            (15)  //ns
        #define DDR3_tRSTL           (100) //ns
        #define DDR3_tZQCL           (320) //ns
        #define DDR3_tDLLK           (512) //tCK

        al = 0;
        bl = 8;
        if(nMHz <= 330)
        {
            tmp = 0;
        }
        else if(nMHz<=400)
        {
            tmp = 1;
        }
        else if(nMHz<=533)
        {
            tmp = 2;
        }
        else //666MHz
        {
            tmp = 3;
        }
        if(nMHz < DDR3_DDR2_ODT_DLL_DISABLE_FREQ)       //when dll bypss cl = cwl = 6;
        {
            cl = 6;
            cwl = 6;
        }
        else 
        {
            cl = (ddr3_cl_cwl[ddr_speed_bin][tmp] >> 4)&0xf;
            cwl = ddr3_cl_cwl[ddr_speed_bin][tmp] & 0xf;
        }
        if(cl == 0)
            ret = -4;
        if(nMHz <= DDR3_DDR2_ODT_DLL_DISABLE_FREQ)
        {
            p_publ_timing->mr[1] = DDR3_DS_40 | DDR3_Rtt_Nom_DIS;
        }
        else
        {
            p_publ_timing->mr[1] = DDR3_DS_40 | DDR3_Rtt_Nom_120;
        }
        p_publ_timing->mr[2] = DDR3_MR2_CWL(cwl) /* | DDR3_Rtt_WR_60 */;
        p_publ_timing->mr[3] = 0;
        /**************************************************
         * PCTL Timing
         **************************************************/
        /*
         * tREFI, average periodic refresh interval, 7.8us
         */
        p_pctl_timing->trefi = DDR3_tREFI_7_8_us;
        /*
         * tMRD, 4 tCK
         */
        p_pctl_timing->tmrd = DDR3_tMRD & 0x7;
        p_publ_timing->dtpr0.b.tMRD = DDR3_tMRD-4;
        /*
         * tRFC, 90ns(512Mb),110ns(1Gb),160ns(2Gb),300ns(4Gb),350ns(8Gb)
         */
        if(ddr_capability_per_die <= 0x4000000)         // 512Mb 90ns
        {
            tmp = DDR3_tRFC_512Mb;
        }
        else if(ddr_capability_per_die <= 0x8000000)    // 1Gb 110ns
        {
            tmp = DDR3_tRFC_1Gb;
        }
        else if(ddr_capability_per_die <= 0x10000000)   // 2Gb 160ns
        {
            tmp = DDR3_tRFC_2Gb;
        }
        else if(ddr_capability_per_die <= 0x20000000)   // 4Gb 300ns
        {
            tmp = DDR3_tRFC_4Gb;
        }
        else    // 8Gb  350ns
        {
            tmp = DDR3_tRFC_8Gb;
        }
        p_pctl_timing->trfc = (tmp*nMHz+999)/1000;
        p_publ_timing->dtpr1.b.tRFC = ((tmp*nMHz+999)/1000)&0xFF;
        /*
         * tXSR, =tDLLK=512 tCK
         */
        p_pctl_timing->texsr = DDR3_tDLLK;
        p_publ_timing->dtpr2.b.tXS = DDR3_tDLLK;
        /*
         * tRP=CL
         */
        p_pctl_timing->trp = cl;
        p_publ_timing->dtpr0.b.tRP = cl;
        /*
         * WrToMiss=WL*tCK + tWR + tRP + tRCD
         */
        p_noc_timing->b.WrToMiss = ((cwl+((DDR3_tWR*nMHz+999)/1000)+cl+cl)&0x3F);
        /*
         * tRC=tRAS+tRP
         */
        p_pctl_timing->trc = ((((ddr3_tRC_tFAW[ddr_speed_bin]>>8)*nMHz+999)/1000)&0x3F);
        p_noc_timing->b.ActToAct = ((((ddr3_tRC_tFAW[ddr_speed_bin]>>8)*nMHz+999)/1000)&0x3F);
        p_publ_timing->dtpr0.b.tRC = (((ddr3_tRC_tFAW[ddr_speed_bin]>>8)*nMHz+999)/1000)&0xF;

        p_pctl_timing->trtw = (cl+2-cwl);//DDR3_tRTW;
        p_publ_timing->dtpr1.b.tRTW = 0;
        p_noc_timing->b.RdToWr = ((cl+2-cwl)&0x1F);
        p_pctl_timing->tal = al;
        p_pctl_timing->tcl = cl;
        p_pctl_timing->tcwl = cwl;
        /*
         * tRAS, 37.5ns(400MHz)     37.5ns(533MHz)
         */
        p_pctl_timing->tras = (((DDR3_tRAS*nMHz+(nMHz>>1)+999)/1000)&0x3F);
        p_publ_timing->dtpr0.b.tRAS = ((DDR3_tRAS*nMHz+(nMHz>>1)+999)/1000)&0x1F;
        /*
         * tRCD=CL
         */
        p_pctl_timing->trcd = cl;
        p_publ_timing->dtpr0.b.tRCD = cl;
        /*
         * tRRD = max(4nCK, 7.5ns), DDR3-1066(1K), DDR3-1333(2K), DDR3-1600(2K)
         *        max(4nCK, 10ns), DDR3-800(1K,2K), DDR3-1066(2K)
         *        max(4nCK, 6ns), DDR3-1333(1K), DDR3-1600(1K)
         *
         */
        tmp = ((DDR3_tRRD*nMHz+999)/1000);
        if(tmp < 4)
        {
            tmp = 4;
        }
        p_pctl_timing->trrd = (tmp&0xF);
        p_publ_timing->dtpr0.b.tRRD = tmp&0xF;
        /*
         * tRTP, max(4 tCK,7.5ns)
         */
        tmp = ((DDR3_tRTP*nMHz+(nMHz>>1)+999)/1000);
        if(tmp < 4)
        {
            tmp = 4;
        }
        p_pctl_timing->trtp = tmp&0xF;
        p_publ_timing->dtpr0.b.tRTP = tmp;
        /*
         * RdToMiss=tRTP+tRP + tRCD - (BL/2 * tCK)
         */
        p_noc_timing->b.RdToMiss = ((tmp+cl+cl-(bl>>1))&0x3F);
        /*
         * tWR, 15ns
         */
        tmp = ((DDR3_tWR*nMHz+999)/1000);
        p_pctl_timing->twr = tmp&0x1F;
        if(tmp<9)
            tmp = tmp - 4;
        else
            tmp = tmp>>1;
        bl_tmp = (bl == 8) ? DDR3_BL8 : DDR3_BC4;
        p_publ_timing->mr[0] = bl_tmp | DDR3_CL(cl) | DDR3_WR(tmp);

        /*
         * tWTR, max(4 tCK,7.5ns)
         */
        tmp = ((DDR3_tWTR*nMHz+(nMHz>>1)+999)/1000);
        if(tmp < 4)
        {
            tmp = 4;
        }
        p_pctl_timing->twtr = tmp&0xF;
        p_publ_timing->dtpr0.b.tWTR = tmp&0x7;
        p_noc_timing->b.WrToRd = ((tmp+cwl)&0x1F);
        /*
         * tXP, max(3 tCK, 7.5ns)(<933MHz)
         */
        tmp = ((DDR3_tXP*nMHz+(nMHz>>1)+999)/1000);
        if(tmp < 3)
        {
            tmp = 3;
        }
        p_pctl_timing->txp = tmp&0x7;
        /*
         * tXPDLL, max(10 tCK,24ns)
         */
        tmp = ((DDR3_tXPDLL*nMHz+999)/1000);
        if(tmp < 10)
        {
            tmp = 10;
        }
        p_pctl_timing->txpdll = tmp & 0x3F;
        p_publ_timing->dtpr2.b.tXP = tmp&0x1F;
        /*
         * tZQCS, max(64 tCK, 80ns)
         */
        tmp = ((DDR3_tZQCS*nMHz+999)/1000);
        if(tmp < 64)
        {
            tmp = 64;
        }
        p_pctl_timing->tzqcs = tmp&0x7F;
        /*
         * tZQCSI,
         */
        p_pctl_timing->tzqcsi = DDR3_tZQCSI;
        /*
         * tDQS,
         */
        p_pctl_timing->tdqs = DDR3_tDQS;
        /*
         * tCKSRE, max(5 tCK, 10ns)
         */
        tmp = ((DDR3_tCKSRE*nMHz+999)/1000);
        if(tmp < 5)
        {
            tmp = 5;
        }
        p_pctl_timing->tcksre = tmp & 0x1F;
        /*
         * tCKSRX, max(5 tCK, 10ns)
         */
        p_pctl_timing->tcksrx = tmp & 0x1F;
        /*
         * tCKE, max(3 tCK,7.5ns)(400MHz) max(3 tCK,5.625ns)(533MHz)
         */
        if(nMHz>=533)
        {
            tmp = ((DDR3_tCKE_533MHz*nMHz+999)/1000);
        }
        else
        {
            tmp = ((DDR3_tCKE_400MHz*nMHz+(nMHz>>1)+999)/1000);
        }
        if(tmp < 3)
        {
            tmp = 3;
        }
        p_pctl_timing->tcke = tmp & 0x7;
        p_publ_timing->dtpr2.b.tCKE = tmp;
        /*
         * tCKESR, =tCKE + 1tCK
         */
        p_pctl_timing->tckesr = (tmp+1)&0xF;
        /*
         * tMOD, max(12 tCK,15ns)
         */
        tmp = ((DDR3_tMOD*nMHz+999)/1000);
        if(tmp < 12)
        {
            tmp = 12;
        }
        p_pctl_timing->tmod = tmp&0x1F;
        p_publ_timing->dtpr1.b.tMOD = tmp;
        /*
         * tRSTL, 100ns
         */
        p_pctl_timing->trstl = ((DDR3_tRSTL*nMHz+999)/1000)&0x7F;
        /*
         * tZQCL, max(256 tCK, 320ns)
         */
        tmp = ((DDR3_tZQCL*nMHz+999)/1000);
        if(tmp < 256)
        {
            tmp = 256;
        }
        p_pctl_timing->tzqcl = tmp&0x3FF;
        /*
         * tMRR, 0 tCK
         */
        p_pctl_timing->tmrr = 0;
        /*
         * tDPD, 0
         */
        p_pctl_timing->tdpd = 0;

        /**************************************************
         * PHY Timing
         **************************************************/
        /*
         * tCCD, BL/2 for DDR2 and 4 for DDR3
         */
        p_publ_timing->dtpr0.b.tCCD = 0;
        /*
         * tDQSCKmax,5.5ns
         */
        p_publ_timing->dtpr1.b.tDQSCKmax = 0;
        /*
         * tRTODT, 0:ODT may be turned on immediately after read post-amble
         *         1:ODT may not be turned on until one clock after the read post-amble
         */
        p_publ_timing->dtpr1.b.tRTODT = 1;
        /*
         * tFAW,40ns(400MHz 1KB page) 37.5ns(533MHz 1KB page) 50ns(400MHz 2KB page)   50ns(533MHz 2KB page)
         */
        p_publ_timing->dtpr1.b.tFAW = (((ddr3_tRC_tFAW[ddr_speed_bin]&0x0ff)*nMHz+999)/1000)&0x7F;
        /*
         * tAOND_tAOFD
         */
        p_publ_timing->dtpr1.b.tAOND = 0;
        /*
         * tDLLK,512 tCK
         */
        p_publ_timing->dtpr2.b.tDLLK = DDR3_tDLLK;
        /**************************************************
         * NOC Timing
         **************************************************/
        p_noc_timing->b.BurstLen = ((bl>>1)&0x7);
    }
    else if(mem_type == LPDDR2)
    {
        #define LPDDR2_tREFI_3_9_us    (38)  //unit 100ns
        #define LPDDR2_tREFI_7_8_us    (78)  //unit 100ns
        #define LPDDR2_tMRD            (5)   //tCK
        #define LPDDR2_tRFC_8Gb        (210)  //ns
        #define LPDDR2_tRFC_4Gb        (130)  //ns
        #define LPDDR2_tRP_4_BANK               (24)  //ns
        #define LPDDR2_tRPab_SUB_tRPpb_4_BANK   (0)
        #define LPDDR2_tRP_8_BANK               (27)  //ns
        #define LPDDR2_tRPab_SUB_tRPpb_8_BANK   (3)
        #define LPDDR2_tRTW          (1)   //tCK register min valid value
        #define LPDDR2_tRAS          (42)  //ns
        #define LPDDR2_tRCD          (24)  //ns
        #define LPDDR2_tRRD          (10)  //ns
        #define LPDDR2_tRTP          (7)   //ns
        #define LPDDR2_tWR           (15)  //ns
        #define LPDDR2_tWTR_GREAT_200MHz         (7)  //ns
        #define LPDDR2_tWTR_LITTLE_200MHz        (10) //ns
        #define LPDDR2_tXP           (7)  //ns
        #define LPDDR2_tXPDLL        (0)
        #define LPDDR2_tZQCS         (90) //ns
        #define LPDDR2_tZQCSI        (0)
        #define LPDDR2_tDQS          (1)
        #define LPDDR2_tCKSRE        (1)  //tCK
        #define LPDDR2_tCKSRX        (2)  //tCK
        #define LPDDR2_tCKE          (3)  //tCK
        #define LPDDR2_tMOD          (0)
        #define LPDDR2_tRSTL         (0)
        #define LPDDR2_tZQCL         (360)  //ns
        #define LPDDR2_tMRR          (2)    //tCK
        #define LPDDR2_tCKESR        (15)   //ns
        #define LPDDR2_tDPD_US       (500)
        #define LPDDR2_tFAW_GREAT_200MHz    (50)  //ns
        #define LPDDR2_tFAW_LITTLE_200MHz   (60)  //ns
        #define LPDDR2_tDLLK         (2)  //tCK
        #define LPDDR2_tDQSCK_MAX    (3)  //tCK
        #define LPDDR2_tDQSCK_MIN    (0)  //tCK
        #define LPDDR2_tDQSS         (1)  //tCK

        uint32 trp_tmp;
        uint32 trcd_tmp;
        uint32 tras_tmp;
        uint32 trtp_tmp;
        uint32 twr_tmp;

        al = 0;
        if(nMHz>=200)
        {
            bl = 4;  //you can change burst here
        }
        else
        {
            bl = 8;  // freq < 200MHz, BL fixed 8
        }
        /*     1066 933 800 667 533 400 333
         * RL,   8   7   6   5   4   3   3
         * WL,   4   4   3   2   2   1   1
         */
        if(nMHz<=200)
        {
            cl = 3;
            cwl = 1;
            p_publ_timing->mr[2] = LPDDR2_RL3_WL1;
        }
        else if(nMHz<=266)
        {
            cl = 4;
            cwl = 2;
            p_publ_timing->mr[2] = LPDDR2_RL4_WL2;
        }
        else if(nMHz<=333)
        {
            cl = 5;
            cwl = 2;
            p_publ_timing->mr[2] = LPDDR2_RL5_WL2;
        }
        else if(nMHz<=400)
        {
            cl = 6;
            cwl = 3;
            p_publ_timing->mr[2] = LPDDR2_RL6_WL3;
        }
        else if(nMHz<=466)
        {
            cl = 7;
            cwl = 4;
            p_publ_timing->mr[2] = LPDDR2_RL7_WL4;
        }
        else //(nMHz<=1066)
        {
            cl = 8;
            cwl = 4;
            p_publ_timing->mr[2] = LPDDR2_RL8_WL4;
        }
        p_publ_timing->mr[3] = LPDDR2_DS_34;
        p_publ_timing->mr[0] = 0;
        /**************************************************
         * PCTL Timing
         **************************************************/
        /*
         * tREFI, average periodic refresh interval, 15.6us(<256Mb) 7.8us(256Mb-1Gb) 3.9us(2Gb-8Gb)
         */
        if(ddr_capability_per_die >= 0x10000000)   // 2Gb
        {
            p_pctl_timing->trefi = LPDDR2_tREFI_3_9_us;
        }
        else
        {
            p_pctl_timing->trefi = LPDDR2_tREFI_7_8_us;
        }

        /*
         * tMRD, (=tMRW), 5 tCK
         */
        p_pctl_timing->tmrd = LPDDR2_tMRD & 0x7;
        p_publ_timing->dtpr0.b.tMRD = 3;
        /*
         * tRFC, 90ns(<=512Mb) 130ns(1Gb-4Gb) 210ns(8Gb)
         */
        if(ddr_capability_per_die >= 0x40000000)   // 8Gb
        {
            p_pctl_timing->trfc = (LPDDR2_tRFC_8Gb*nMHz+999)/1000;
            p_publ_timing->dtpr1.b.tRFC = ((LPDDR2_tRFC_8Gb*nMHz+999)/1000)&0xFF;
            /*
             * tXSR, max(2tCK,tRFC+10ns)
             */
            tmp=(((LPDDR2_tRFC_8Gb+10)*nMHz+999)/1000);
        }
        else
        {
            p_pctl_timing->trfc = (LPDDR2_tRFC_4Gb*nMHz+999)/1000;
            p_publ_timing->dtpr1.b.tRFC = ((LPDDR2_tRFC_4Gb*nMHz+999)/1000)&0xFF;
            tmp=(((LPDDR2_tRFC_4Gb+10)*nMHz+999)/1000);
        }
        if(tmp<2)
        {
            tmp=2;
        }
        p_pctl_timing->texsr = tmp&0x3FF;
        p_publ_timing->dtpr2.b.tXS = tmp&0x3FF;

        /*
         * tRP, max(3tCK, 4-bank:15ns(Fast) 18ns(Typ) 24ns(Slow), 8-bank:18ns(Fast) 21ns(Typ) 27ns(Slow))
         */
        if(pPHY_Reg->DCR.b.DDR8BNK)
        {
            trp_tmp = ((LPDDR2_tRP_8_BANK*nMHz+999)/1000);
            if(trp_tmp<3)
            {
                trp_tmp=3;
            }
            p_pctl_timing->trp = ((((LPDDR2_tRPab_SUB_tRPpb_8_BANK*nMHz+999)/1000) & 0x3)<<16) | (trp_tmp&0xF);
        }
        else
        {
            trp_tmp = ((LPDDR2_tRP_4_BANK*nMHz+999)/1000);
            if(trp_tmp<3)
            {
                trp_tmp=3;
            }
            p_pctl_timing->trp = (LPDDR2_tRPab_SUB_tRPpb_4_BANK<<16) | (trp_tmp&0xF);
        }
        p_publ_timing->dtpr0.b.tRP = trp_tmp;
        /*
         * tRAS, max(3tCK,42ns)
         */
        tras_tmp=((LPDDR2_tRAS*nMHz+999)/1000);
        if(tras_tmp<3)
        {
            tras_tmp=3;
        }
        p_pctl_timing->tras = (tras_tmp&0x3F);
        p_publ_timing->dtpr0.b.tRAS = tras_tmp&0x1F;

        /*
         * tRCD, max(3tCK, 15ns(Fast) 18ns(Typ) 24ns(Slow))
         */
        trcd_tmp = ((LPDDR2_tRCD*nMHz+999)/1000);
        if(trcd_tmp<3)
        {
            trcd_tmp=3;
        }
        p_pctl_timing->trcd = (trcd_tmp&0xF);
        p_publ_timing->dtpr0.b.tRCD = trcd_tmp&0xF;

        /*
         * tRTP, max(2tCK, 7.5ns)
         */
        trtp_tmp = ((LPDDR2_tRTP*nMHz+(nMHz>>1)+999)/1000);
        if(trtp_tmp<2)
        {
            trtp_tmp = 2;
        }
        p_pctl_timing->trtp = trtp_tmp&0xF;
        p_publ_timing->dtpr0.b.tRTP = trtp_tmp;

        /*
         * tWR, max(3tCK,15ns)
         */
        twr_tmp=((LPDDR2_tWR*nMHz+999)/1000);
        if(twr_tmp<3)
        {
            twr_tmp=3;
        }
        p_pctl_timing->twr = twr_tmp&0x1F;
        bl_tmp = (bl == 16) ? LPDDR2_BL16 : ((bl == 8) ? LPDDR2_BL8 : LPDDR2_BL4);
        p_publ_timing->mr[1] = bl_tmp | LPDDR2_nWR(twr_tmp);

        /*
         * WrToMiss=WL*tCK + tDQSS + tWR + tRP + tRCD
         */
        p_noc_timing->b.WrToMiss = ((cwl+LPDDR2_tDQSS+twr_tmp+trp_tmp+trcd_tmp)&0x3F);
        /*
         * RdToMiss=tRTP + tRP + tRCD - (BL/2 * tCK)
         */
        p_noc_timing->b.RdToMiss = ((trtp_tmp+trp_tmp+trcd_tmp-(bl>>1))&0x3F);
        /*
         * tRC=tRAS+tRP
         */
        p_pctl_timing->trc = ((tras_tmp+trp_tmp)&0x3F);
        p_noc_timing->b.ActToAct = ((tras_tmp+trp_tmp)&0x3F);
        p_publ_timing->dtpr0.b.tRC = (tras_tmp+trp_tmp)&0xF;

        /*
         * RdToWr=RL+tDQSCK-WL
         */
        p_pctl_timing->trtw = (cl+LPDDR2_tDQSCK_MAX+(bl/2)+1-cwl);//LPDDR2_tRTW;
        p_publ_timing->dtpr1.b.tRTW = 0;
        p_noc_timing->b.RdToWr = ((cl+LPDDR2_tDQSCK_MAX+1-cwl)&0x1F);
        p_pctl_timing->tal = al;
        p_pctl_timing->tcl = cl;
        p_pctl_timing->tcwl = cwl;
        /*
         * tRRD, max(2tCK,10ns)
         */
        tmp=((LPDDR2_tRRD*nMHz+999)/1000);
        if(tmp<2)
        {
            tmp=2;
        }
        p_pctl_timing->trrd = (tmp&0xF);
        p_publ_timing->dtpr0.b.tRRD = tmp&0xF;
        /*
         * tWTR, max(2tCK, 7.5ns(533-266MHz)  10ns(200-166MHz))
         */
        if(nMHz > 200)
        {
            tmp=((LPDDR2_tWTR_GREAT_200MHz*nMHz+(nMHz>>1)+999)/1000);
        }
        else
        {
            tmp=((LPDDR2_tWTR_LITTLE_200MHz*nMHz+999)/1000);
        }
        if(tmp<2)
        {
            tmp=2;
        }
        p_pctl_timing->twtr = tmp&0xF;
        p_publ_timing->dtpr0.b.tWTR = tmp&0x7;
        /*
         * WrToRd=WL+tDQSS+tWTR
         */
        p_noc_timing->b.WrToRd = ((cwl+LPDDR2_tDQSS+tmp)&0x1F);
        /*
         * tXP, max(2tCK,7.5ns)
         */
        tmp=((LPDDR2_tXP*nMHz+(nMHz>>1)+999)/1000);
        if(tmp<2)
        {
            tmp=2;
        }
        p_pctl_timing->txp = tmp&0x7;
        p_publ_timing->dtpr2.b.tXP = tmp&0x1F;
        /*
         * tXPDLL, 0ns
         */
        p_pctl_timing->txpdll = LPDDR2_tXPDLL;
        /*
         * tZQCS, 90ns
         */
        p_pctl_timing->tzqcs = ((LPDDR2_tZQCS*nMHz+999)/1000)&0x7F;
        /*
         * tZQCSI,
         */
        if(pDDR_Reg->MCFG &= lpddr2_s4)
        {
            p_pctl_timing->tzqcsi = LPDDR2_tZQCSI;
        }
        else
        {
            p_pctl_timing->tzqcsi = 0;
        }
        /*
         * tDQS,
         */
        p_pctl_timing->tdqs = LPDDR2_tDQS;
        /*
         * tCKSRE, 1 tCK
         */
        p_pctl_timing->tcksre = LPDDR2_tCKSRE;
        /*
         * tCKSRX, 2 tCK
         */
        p_pctl_timing->tcksrx = LPDDR2_tCKSRX;
        /*
         * tCKE, 3 tCK
         */
        p_pctl_timing->tcke = LPDDR2_tCKE;
        p_publ_timing->dtpr2.b.tCKE = LPDDR2_tCKE;
        /*
         * tMOD, 0 tCK
         */
        p_pctl_timing->tmod = LPDDR2_tMOD;
        p_publ_timing->dtpr1.b.tMOD = LPDDR2_tMOD;
        /*
         * tRSTL, 0 tCK
         */
        p_pctl_timing->trstl = LPDDR2_tRSTL;
        /*
         * tZQCL, 360ns
         */
        p_pctl_timing->tzqcl = ((LPDDR2_tZQCL*nMHz+999)/1000)&0x3FF;
        /*
         * tMRR, 2 tCK
         */
        p_pctl_timing->tmrr = LPDDR2_tMRR;
        /*
         * tCKESR, max(3tCK,15ns)
         */
        tmp = ((LPDDR2_tCKESR*nMHz+999)/1000);
        if(tmp < 3)
        {
            tmp = 3;
        }
        p_pctl_timing->tckesr = tmp&0xF;
        /*
         * tDPD, 500us
         */
        p_pctl_timing->tdpd = LPDDR2_tDPD_US;

        /**************************************************
         * PHY Timing
         **************************************************/
        /*
         * tCCD, BL/2 for DDR2 and 4 for DDR3
         */
        p_publ_timing->dtpr0.b.tCCD = 0;
        /*
         * tDQSCKmax,5.5ns
         */
        p_publ_timing->dtpr1.b.tDQSCKmax = LPDDR2_tDQSCK_MAX;
        /*
         * tDQSCKmin,2.5ns
         */
        p_publ_timing->dtpr1.b.tDQSCK = LPDDR2_tDQSCK_MIN;
        /*
         * tRTODT, 0:ODT may be turned on immediately after read post-amble
         *         1:ODT may not be turned on until one clock after the read post-amble
         */
        p_publ_timing->dtpr1.b.tRTODT = 1;
        /*
         * tFAW,max(8tCK, 50ns(200-533MHz)  60ns(166MHz))
         */
        if(nMHz>=200)
        {
            tmp=((LPDDR2_tFAW_GREAT_200MHz*nMHz+999)/1000);
        }
        else
        {
            tmp=((LPDDR2_tFAW_LITTLE_200MHz*nMHz+999)/1000);
        }
        if(tmp<8)
        {
            tmp=8;
        }
        p_publ_timing->dtpr1.b.tFAW = tmp&0x7F;
        /*
         * tAOND_tAOFD
         */
        p_publ_timing->dtpr1.b.tAOND = 0;
        /*
         * tDLLK,0
         */
        p_publ_timing->dtpr2.b.tDLLK = LPDDR2_tDLLK;
        /**************************************************
         * NOC Timing
         **************************************************/
        p_noc_timing->b.BurstLen = ((bl>>1)&0x7);
    }

out:
    return ret;
}

uint32_t __sramlocalfunc ddr_update_timing(void)
{
    uint32_t i,bl_tmp=0;
    PCTL_TIMING_T *p_pctl_timing=&(ddr_reg.pctl.pctl_timing);
    PHY_TIMING_T  *p_publ_timing=&(ddr_reg.publ.phy_timing);
    NOC_TIMING_T  *p_noc_timing=&(ddr_reg.noc_timing);

    ddr_copy((uint32_t *)&(pDDR_Reg->TOGCNT1U), (uint32_t*)&(p_pctl_timing->togcnt1u), 34);
    ddr_copy((uint32_t *)&(pPHY_Reg->DTPR[0]), (uint32_t*)&(p_publ_timing->dtpr0), 3);
    *(volatile uint32_t *)SysSrv_DdrTiming = p_noc_timing->d32;
    // Update PCTL BL
    if(mem_type == DDR3)
    {
        bl_tmp = ((p_publ_timing->mr[0] & 0x3) == DDR3_BL8) ? ddr2_ddr3_bl_8 : ddr2_ddr3_bl_4;
        pDDR_Reg->MCFG = (pDDR_Reg->MCFG & (~(0x1|(0x3<<18)|(0x1<<17)|(0x1<<16)))) | bl_tmp | tfaw_cfg(5)|pd_exit_slow|pd_type(1);
        if((ddr_freq <= DDR3_DDR2_ODT_DLL_DISABLE_FREQ) && ddr_soc_is_rk3188_plus)
        {
            pDDR_Reg->DFITRDDATAEN   = pDDR_Reg->TCL-3;
        }
        else
        {
            pDDR_Reg->DFITRDDATAEN   = pDDR_Reg->TCL-2;
        }
        pDDR_Reg->DFITPHYWRLAT   = pDDR_Reg->TCWL-1;
    }
    else if(mem_type == LPDDR2)
    {
        if(((p_publ_timing->mr[1]) & 0x7) == LPDDR2_BL8)
        {
            bl_tmp = mddr_lpddr2_bl_8;
        }
        else if(((p_publ_timing->mr[1]) & 0x7) == LPDDR2_BL4)
        {
            bl_tmp = mddr_lpddr2_bl_4;
        }
        else //if(((p_publ_timing->mr[1]) & 0x7) == LPDDR2_BL16)
        {
            bl_tmp = mddr_lpddr2_bl_16;
        }
        if(ddr_freq>=200)
        {
            pDDR_Reg->MCFG = (pDDR_Reg->MCFG & (~((0x3<<20)|(0x3<<18)|(0x1<<17)|(0x1<<16)))) | bl_tmp | tfaw_cfg(5)|pd_exit_fast|pd_type(1);
        }
        else
        {
            pDDR_Reg->MCFG = (pDDR_Reg->MCFG & (~((0x3<<20)|(0x3<<18)|(0x1<<17)|(0x1<<16)))) | mddr_lpddr2_bl_8 | tfaw_cfg(6)|pd_exit_fast|pd_type(1);
        }
        i = ((pPHY_Reg->DTPR[1] >> 27) & 0x7) - ((pPHY_Reg->DTPR[1] >> 24) & 0x7);
        pPHY_Reg->DSGCR = (pPHY_Reg->DSGCR & (~(0x3F<<5))) | (i<<5) | (i<<8);  //tDQSCKmax-tDQSCK
        pDDR_Reg->DFITRDDATAEN   = pDDR_Reg->TCL-1;
        pDDR_Reg->DFITPHYWRLAT   = pDDR_Reg->TCWL;
    }
    
    return 0;
}

uint32_t __sramlocalfunc ddr_update_mr(void)
{
    PHY_TIMING_T  *p_publ_timing=&(ddr_reg.publ.phy_timing);
    uint32_t cs,dll_off;

    cs = ((pPHY_Reg->PGCR>>18) & 0xF);
    dll_off = (pPHY_Reg->MR[1] & DDR3_DLL_DISABLE) ? 1:0;
    ddr_copy((uint32_t *)&(pPHY_Reg->MR[0]), (uint32_t*)&(p_publ_timing->mr[0]), 4);
    if((mem_type == DDR3) || (mem_type == DDR2))
    {
        if(ddr_freq>DDR3_DDR2_ODT_DLL_DISABLE_FREQ)
        {
            if(dll_off)  // off -> on
            {
                ddr_send_command(cs, MRS_cmd, bank_addr(0x1) | cmd_addr((p_publ_timing->mr[1])));  //DLL enable
                ddr_send_command(cs, MRS_cmd, bank_addr(0x0) | cmd_addr(((p_publ_timing->mr[0]))| DDR3_DLL_RESET));  //DLL reset
                ddr_delayus(2);  //at least 200 DDR cycle
                ddr_send_command(cs, MRS_cmd, bank_addr(0x0) | cmd_addr((p_publ_timing->mr[0])));
            }
            else // on -> on
            {
                ddr_send_command(cs, MRS_cmd, bank_addr(0x1) | cmd_addr((p_publ_timing->mr[1])));
                ddr_send_command(cs, MRS_cmd, bank_addr(0x0) | cmd_addr((p_publ_timing->mr[0])));
            }
        }
        else
        {
            pPHY_Reg->MR[1] = (((p_publ_timing->mr[1])) | DDR3_DLL_DISABLE);
            ddr_send_command(cs, MRS_cmd, bank_addr(0x1) | cmd_addr(((p_publ_timing->mr[1])) | DDR3_DLL_DISABLE));  //DLL disable
            ddr_send_command(cs, MRS_cmd, bank_addr(0x0) | cmd_addr((p_publ_timing->mr[0])));
        }
        ddr_send_command(cs, MRS_cmd, bank_addr(0x2) | cmd_addr((p_publ_timing->mr[2])));
    }
    else if(mem_type == LPDDR2)
    {
        ddr_send_command(cs, MRS_cmd, lpddr2_ma(0x1) | lpddr2_op((p_publ_timing->mr[1])));
        ddr_send_command(cs, MRS_cmd, lpddr2_ma(0x2) | lpddr2_op((p_publ_timing->mr[2])));
        ddr_send_command(cs, MRS_cmd, lpddr2_ma(0x3) | lpddr2_op((p_publ_timing->mr[3])));
    }
    else //mDDR
    {
        ddr_send_command(cs, MRS_cmd, bank_addr(0x0) | cmd_addr((p_publ_timing->mr[0])));
        ddr_send_command(cs, MRS_cmd, bank_addr(0x1) | cmd_addr((p_publ_timing->mr[2]))); //mr[2] is mDDR MR1
    }
    return 0;
}

void __sramlocalfunc ddr_update_odt(void)
{
    uint32_t cs,tmp;
    
    //adjust DRV and ODT
    if((mem_type == DDR3) || (mem_type == DDR2))
    {
        if(ddr_freq <= DDR3_DDR2_ODT_DLL_DISABLE_FREQ)
        {
            pPHY_Reg->DATX8[0].DXGCR &= ~(0x3<<9);  //dynamic RTT disable
            pPHY_Reg->DATX8[1].DXGCR &= ~(0x3<<9);
            if(!(pDDR_Reg->PPCFG & 1))
            {
                pPHY_Reg->DATX8[2].DXGCR &= ~(0x3<<9);
                pPHY_Reg->DATX8[3].DXGCR &= ~(0x3<<9);
            }
        }
        else
        {
            pPHY_Reg->DATX8[0].DXGCR |= (0x3<<9);  //dynamic RTT enable
            pPHY_Reg->DATX8[1].DXGCR |= (0x3<<9);
            if(!(pDDR_Reg->PPCFG & 1))
            {
                pPHY_Reg->DATX8[2].DXGCR |= (0x3<<9);
                pPHY_Reg->DATX8[3].DXGCR |= (0x3<<9);
            }
        }
    }
    else
    {
        pPHY_Reg->DATX8[0].DXGCR &= ~(0x3<<9);  //dynamic RTT disable
        pPHY_Reg->DATX8[1].DXGCR &= ~(0x3<<9);
        if(!(pDDR_Reg->PPCFG & 1))
        {
            pPHY_Reg->DATX8[2].DXGCR &= ~(0x3<<9);
            pPHY_Reg->DATX8[3].DXGCR &= ~(0x3<<9);
        }
    }
    tmp = (0x1<<28) | (0x2<<15) | (0x2<<10) | (0xb<<5) | 0xb;  //DS=34ohm,ODT=171ohm
    cs = ((pPHY_Reg->PGCR>>18) & 0xF);
    if(cs > 1)
    {
        pPHY_Reg->ZQ1CR[0] = tmp;
        dsb();
    }
    pPHY_Reg->ZQ0CR[0] = tmp;
    dsb();
}

__sramfunc void ddr_adjust_config(uint32_t dram_type)
{
    uint32 value;
    unsigned long save_sp;
    u32 i;
    volatile u32 n; 
    volatile unsigned int * temp=(volatile unsigned int *)SRAM_CODE_OFFSET;

    //get data training address before idle port
    value = ddr_get_datatraing_addr();

    /** 1. Make sure there is no host access */
    flush_cache_all();
    outer_flush_all();
    flush_tlb_all();
    isb();
    DDR_SAVE_SP(save_sp);

    for(i=0;i<SRAM_SIZE/4096;i++) 
    {
        n=temp[1024*i];
        barrier();
    }
    n= pDDR_Reg->SCFG.d32;
    n= pPHY_Reg->RIDR;
    n= pCRU_Reg->CRU_PLL_CON[0][0];
    n= pPMU_Reg->PMU_WAKEUP_CFG[0];
    n= *(volatile uint32_t *)SysSrv_DdrConf;
    n= pGRF_Reg->GRF_SOC_STATUS0;
    dsb();
    
    //enter config state
    ddr_move_to_Config_state();

    //set data training address
    pPHY_Reg->DTAR = value;

    //set auto power down idle
    pDDR_Reg->MCFG=(pDDR_Reg->MCFG&0xffff00ff)|(PD_IDLE<<8);

    //CKDV=00
    pPHY_Reg->PGCR &= ~(0x3<<12);

    //enable the hardware low-power interface
    pDDR_Reg->SCFG.b.hw_low_power_en = 1;

    if(pDDR_Reg->PPCFG & 1)
    {
        pPHY_Reg->DATX8[2].DXGCR &= ~(1);          //disable byte
        pPHY_Reg->DATX8[3].DXGCR &= ~(1);
        pPHY_Reg->DATX8[2].DXDLLCR |= 0x80000000;  //disable DLL
        pPHY_Reg->DATX8[3].DXDLLCR |= 0x80000000;
    }

    ddr_update_odt();

    //enter access state
    ddr_move_to_Access_state();

    DDR_RESTORE_SP(save_sp);
}


void __sramlocalfunc idle_port(void)
{
    int i;
    uint32 clk_gate[10];

    //save clock gate status
    for(i=0;i<10;i++)
        clk_gate[i]=pCRU_Reg->CRU_CLKGATE_CON[i];

    //enable all clock gate for request idle
    for(i=0;i<10;i++)
        pCRU_Reg->CRU_CLKGATE_CON[i]=0xffff0000;

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_a9_0_pwr_st) == 0 )
    {
#ifdef CONFIG_ARCH_RK3188
        pPMU_Reg->PMU_MISC_CON1 |= idle_req_dma_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_dma) == 0 );
#else
        pPMU_Reg->PMU_MISC_CON1 |= idle_req_cpu_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_cpu) == 0 );
#endif
    }

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_peri_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 |= idle_req_peri_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_peri) == 0 );
    }

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_vio_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 |= idle_req_vio_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_vio) == 0 );
    }

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_video_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 |= idle_req_video_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_video) == 0 );
    }

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_gpu_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 |= idle_req_gpu_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_gpu) == 0 );
    }

	//resume clock gate status
    for(i=0;i<10;i++)
        pCRU_Reg->CRU_CLKGATE_CON[i]=  (clk_gate[i] | 0xffff0000);
}

void __sramlocalfunc deidle_port(void)
{
    int i;
    uint32 clk_gate[10];

    //save clock gate status
    for(i=0;i<10;i++)
        clk_gate[i]=pCRU_Reg->CRU_CLKGATE_CON[i];

    //enable all clock gate for request idle
    for(i=0;i<10;i++)
        pCRU_Reg->CRU_CLKGATE_CON[i]=0xffff0000;

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_a9_0_pwr_st) == 0 )
    {

#ifdef CONFIG_ARCH_RK3188
        pPMU_Reg->PMU_MISC_CON1 &= ~idle_req_dma_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_dma) != 0 );
#else
        pPMU_Reg->PMU_MISC_CON1 &= ~idle_req_cpu_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_cpu) != 0 );
#endif
    }
    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_peri_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 &= ~idle_req_peri_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_peri) != 0 );
    }

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_vio_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 &= ~idle_req_vio_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_vio) != 0 );
    }

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_video_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 &= ~idle_req_video_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_video) != 0 );
    }

    if ( (pPMU_Reg->PMU_PWRDN_ST & pd_gpu_pwr_st) == 0 )
    {
        pPMU_Reg->PMU_MISC_CON1 &= ~idle_req_gpu_cfg;
        dsb();
        while( (pPMU_Reg->PMU_PWRDN_ST & idle_gpu) != 0 );
    }

    //resume clock gate status
    for(i=0;i<10;i++)
        pCRU_Reg->CRU_CLKGATE_CON[i]=  (clk_gate[i] | 0xffff0000);

}

void __sramlocalfunc ddr_selfrefresh_enter(uint32 nMHz)
{
    PHY_TIMING_T  *p_publ_timing=&(ddr_reg.publ.phy_timing);
    uint32 cs;
    
    ddr_move_to_Config_state();
    pDDR_Reg->TZQCSI = 0;
    if((nMHz<=DDR3_DDR2_ODT_DLL_DISABLE_FREQ) && ((mem_type == DDR3) || (mem_type == DDR2)))  // DLL disable
    {
        cs = ((pPHY_Reg->PGCR>>18) & 0xF);
        pPHY_Reg->MR[1] = (((p_publ_timing->mr[1])) | DDR3_DLL_DISABLE);
        ddr_send_command(cs, MRS_cmd, bank_addr(0x1) | cmd_addr(((p_publ_timing->mr[1])) | DDR3_DLL_DISABLE));
    }
    ddr_move_to_Lowpower_state();
    
    ddr_set_dll_bypass(0);  //dll bypass
    pCRU_Reg->CRU_CLKGATE_CON[0] = ((0x1<<2)<<16) | (1<<2);  //disable DDR PHY clock
    ddr_delayus(1);
}

void __sramlocalfunc ddr_selfrefresh_exit(void)
{
    uint32 n;

    pCRU_Reg->CRU_CLKGATE_CON[0] = ((0x1<<2)<<16) | (0<<2);  //enable DDR PHY clock
    dsb();
    ddr_set_dll_bypass(ddr_freq);    
    ddr_reset_dll();
    //ddr_delayus(10);   //wait DLL lock

    ddr_move_to_Config_state();
    ddr_update_timing();
    ddr_update_mr();
    ddr_update_odt();
    n = ddr_data_training();
    ddr_move_to_Access_state();
    if(n!=0)
    {
        sram_printascii("DTT failed!\n");
    }
}

uint32_t __sramfunc ddr_change_freq_sram(uint32_t nMHz , struct ddr_freq_t ddr_freq_t)
{
    uint32_t ret;
    u32 i;
    volatile u32 n;	
    unsigned long flags;
    volatile unsigned int * temp=(volatile unsigned int *)SRAM_CODE_OFFSET;
    unsigned long save_sp;
    uint32_t freq;

    freq = ddr_get_pll_freq(APLL); //APLL

    loops_per_us = LPJ_100MHZ*freq / 1000000;

    ret=ddr_set_pll(nMHz,0);

    ddr_get_parameter(ret);

    /** 1. Make sure there is no host access */
    local_irq_save(flags);
    local_fiq_disable();
    flush_cache_all();
    outer_flush_all();
    flush_tlb_all();
    isb();
    DDR_SAVE_SP(save_sp);

    for(i=0;i<SRAM_SIZE/4096;i++)     
    {
        n=temp[1024*i];
        barrier();
    }

    n= pDDR_Reg->SCFG.d32;
    n= pPHY_Reg->RIDR;
    n= pCRU_Reg->CRU_PLL_CON[0][0];
    n= pPMU_Reg->PMU_WAKEUP_CFG[0];
    n= *(volatile uint32_t *)SysSrv_DdrConf;
    n= pGRF_Reg->GRF_SOC_STATUS0;
    dsb();

#if defined (DDR_CHANGE_FREQ_IN_LCDC_VSYNC)  
    n = ddr_freq_t.screen_ft_us;
    n = ddr_freq_t.t0;
    dsb();

    if(ddr_freq_t.screen_ft_us > 0)
    {
        ddr_freq_t.t1 = cpu_clock(0);
        ddr_freq_t.t2 = (u32)(ddr_freq_t.t1 - ddr_freq_t.t0);   //ns

        //if test_count exceed maximum test times,ddr_freq_t.screen_ft_us == 0xfefefefe by ddr_freq.c
        if( (ddr_freq_t.t2 > ddr_freq_t.screen_ft_us*1000) && (ddr_freq_t.screen_ft_us != 0xfefefefe))
        {
            DDR_RESTORE_SP(save_sp);
            local_fiq_enable();
            local_irq_restore(flags);
            return 0;
        }
        else
        {
            rk_fb_poll_wait_frame_complete();
        }
    }
#endif

    /** 2. ddr enter self-refresh mode or precharge power-down mode */
   idle_port();
   ddr_selfrefresh_enter(ret);

    /** 3. change frequence  */
    ddr_set_pll(ret,1);
    ddr_freq = ret;
    
    /** 5. Issues a Mode Exit command   */
    ddr_selfrefresh_exit();

    deidle_port();
    dsb();
    DDR_RESTORE_SP(save_sp);
    local_fiq_enable();
    local_irq_restore(flags);
    return ret;
}

uint32_t ddr_change_freq_gpll_dpll(uint32_t nMHz)
{
    uint32_t gpll_freq,gpll_div;
    struct ddr_freq_t ddr_freq_t;
    ddr_freq_t.screen_ft_us = 0;

    gpllvaluel = ddr_get_pll_freq(GPLL);

    if((200 < gpllvaluel) ||( gpllvaluel <1600))      //GPLL:200MHz~1600MHz
    {
        if( gpllvaluel > 800)     //800-1600MHz  /4:200MHz-400MHz
        {
            gpll_freq = gpllvaluel/4;
            gpll_div = 4;
        }
        else if( gpllvaluel > 400)    //400-800MHz  /2:200MHz-400MHz
        {
            gpll_freq = gpllvaluel/2;
            gpll_div = 2;
        }
        else        //200-400MHz  /1:200MHz-400MHz
        {
            gpll_freq = gpllvaluel;
            gpll_div = 1;
        }

        ddr_select_gpll_div=gpll_div;    //select GPLL
        ddr_change_freq_sram(gpll_freq,ddr_freq_t);
        ddr_select_gpll_div=0;

        ddr_set_pll(nMHz,0); //count DPLL
        ddr_set_pll(nMHz,2); //lock DPLL only,but not select DPLL
    }
    else
    {
        ddr_print("GPLL frequency = %dMHz,Not suitable for ddr_clock \n",gpllvaluel);
    }

    return ddr_change_freq_sram(nMHz,ddr_freq_t);

}

/*****************************************
if rk3188 DPLL is bad,use GPLL
            GPLL                   DDR_CLCOK
1000MHz-2000MHz       4:250MHz-500MHz
800MHz-1000MHz        4:200MHz-250MHz    2:400MHz-500MHz     
500MHz-800MHz          2:250MHz-400MHz
200MHz-500MHz          1:200MHz-500MHz   
******************************************/
uint32_t ddr_change_freq(uint32_t nMHz)
{
    struct ddr_freq_t ddr_freq_t;
    ddr_freq_t.screen_ft_us = 0;

#if defined(ENABLE_DDR_CLCOK_GPLL_PATH) && defined(CONFIG_ARCH_RK3188)
    return ddr_change_freq_gpll_dpll(nMHz);
#else
    return ddr_change_freq_sram(nMHz,ddr_freq_t);
#endif
}
EXPORT_SYMBOL(ddr_change_freq);

void ddr_set_auto_self_refresh(bool en)
{
    //set auto self-refresh idle
    ddr_sr_idle = en ? SR_IDLE : 0;
}
EXPORT_SYMBOL(ddr_set_auto_self_refresh);

enum rk_plls_id {
	APLL_IDX = 0,
	DPLL_IDX,
	CPLL_IDX,
	GPLL_IDX,
	END_PLL_IDX,
};
#define PLL_MODE_SLOW(id)	((0x0<<((id)*4))|(0x3<<(16+(id)*4)))
#define PLL_MODE_NORM(id)	((0x1<<((id)*4))|(0x3<<(16+(id)*4)))

#define CRU_W_MSK(bits_shift, msk)	((msk) << ((bits_shift) + 16))
#define CRU_SET_BITS(val,bits_shift, msk)	(((val)&(msk)) << (bits_shift))

#define CRU_W_MSK_SETBITS(val,bits_shift,msk) (CRU_W_MSK(bits_shift, msk)|CRU_SET_BITS(val,bits_shift, msk))

#define PERI_ACLK_DIV_MASK 0x1f
#define PERI_ACLK_DIV_OFF 0

#define PERI_HCLK_DIV_MASK 0x3
#define PERI_HCLK_DIV_OFF 8

#define PERI_PCLK_DIV_MASK 0x3
#define PERI_PCLK_DIV_OFF 12
static __sramdata u32 cru_sel32_sram;
void __sramfunc ddr_suspend(void)
{
    u32 i;
    volatile u32 n;	
    volatile unsigned int * temp=(volatile unsigned int *)SRAM_CODE_OFFSET;
    int pll_idx;

if(pCRU_Reg->CRU_CLKSEL_CON[26]&(1<<8))
	pll_idx=GPLL_IDX;
else
	pll_idx=DPLL_IDX;
    /** 1. Make sure there is no host access */
    flush_cache_all();
    outer_flush_all();
    //flush_tlb_all();

    for(i=0;i<SRAM_SIZE/4096;i++)     
    {
        n=temp[1024*i];
        barrier();
    }

    n= pDDR_Reg->SCFG.d32;
    n= pPHY_Reg->RIDR;
    n= pCRU_Reg->CRU_PLL_CON[0][0];
    n= pPMU_Reg->PMU_WAKEUP_CFG[0];
    n= *(volatile uint32_t *)SysSrv_DdrConf;
    n= pGRF_Reg->GRF_SOC_STATUS0;
    dsb();
    
    ddr_selfrefresh_enter(0);

    pCRU_Reg->CRU_MODE_CON = PLL_MODE_SLOW(pll_idx);   //PLL slow-mode
    dsb();
    ddr_delayus(1);    
    pCRU_Reg->CRU_PLL_CON[pll_idx][3] = ((0x1<<1)<<16) | (0x1<<1);         //PLL power-down
    dsb();
    ddr_delayus(1);    
    if(pll_idx==GPLL_IDX)
    {	
    	cru_sel32_sram=   pCRU_Reg->CRU_CLKSEL_CON[10];
    
    	pCRU_Reg->CRU_CLKSEL_CON[10]=CRU_W_MSK_SETBITS(0, PERI_ACLK_DIV_OFF, PERI_ACLK_DIV_MASK)
    				   | CRU_W_MSK_SETBITS(0, PERI_HCLK_DIV_OFF, PERI_HCLK_DIV_MASK)
    				   |CRU_W_MSK_SETBITS(0, PERI_PCLK_DIV_OFF, PERI_PCLK_DIV_MASK);
    }
    pPHY_Reg->DSGCR = pPHY_Reg->DSGCR&(~((0x1<<28)|(0x1<<29)));  //CKOE
}
EXPORT_SYMBOL(ddr_suspend);

void __sramfunc ddr_resume(void)
{
    int delay=1000;
    int pll_idx;
	u32 bit = 0x10;

	if(pCRU_Reg->CRU_CLKSEL_CON[26]&(1<<8))
	{	
		pll_idx=GPLL_IDX;
		bit =bit<<3;
	}
	else
	{
		pll_idx=DPLL_IDX;
		bit=bit<<0;
	}
	
	pPHY_Reg->DSGCR = pPHY_Reg->DSGCR|((0x1<<28)|(0x1<<29));  //CKOE
	dsb();
	
	if(pll_idx==GPLL_IDX)
	pCRU_Reg->CRU_CLKSEL_CON[10]=0xffff0000|cru_sel32_sram;


	
    pCRU_Reg->CRU_PLL_CON[pll_idx][3] = ((0x1<<1)<<16) | (0x0<<1);         //PLL no power-down
    dsb();
    while (delay > 0) 
    {
	ddr_delayus(1);

        if (pGRF_Reg->GRF_SOC_STATUS0 & (1<<4))
            break;

        delay--;
    }
    
    pCRU_Reg->CRU_MODE_CON = PLL_MODE_NORM(pll_idx);   //PLL normal
    dsb();

    ddr_selfrefresh_exit();
}
EXPORT_SYMBOL(ddr_resume);

//��ȡ�����������ֽ���
uint32 ddr_get_cap(void)
{
    uint32 cap;

    if(pPMU_Reg->PMU_PMU_SYS_REG[2])
    {
        cap = (1 << (READ_CS0_ROW_INFO()+READ_COL_INFO()+READ_BK_INFO()+READ_BW_INFO()));
        if(READ_CS_INFO()>1)
        {
            cap +=(1 << (READ_CS1_ROW_INFO()+READ_COL_INFO()+READ_BK_INFO()+READ_BW_INFO()));
        }
    }
    else
    {
        cap = (1 << (ddr_get_row()+ddr_get_col()+ddr_get_bank()+ddr_get_bw()))*ddr_get_cs();
    }
    
    return cap;
}
EXPORT_SYMBOL(ddr_get_cap);

void ddr_reg_save(void)
{
    //PCTLR
    ddr_reg.pctl.SCFG = pDDR_Reg->SCFG.d32;
    ddr_reg.pctl.CMDTSTATEN = pDDR_Reg->CMDTSTATEN;
    ddr_reg.pctl.MCFG1 = pDDR_Reg->MCFG1;
    ddr_reg.pctl.MCFG = pDDR_Reg->MCFG;
    ddr_reg.pctl.pctl_timing.ddrFreq = ddr_freq;
    ddr_reg.pctl.DFITCTRLDELAY = pDDR_Reg->DFITCTRLDELAY;
    ddr_reg.pctl.DFIODTCFG = pDDR_Reg->DFIODTCFG;
    ddr_reg.pctl.DFIODTCFG1 = pDDR_Reg->DFIODTCFG1;
    ddr_reg.pctl.DFIODTRANKMAP = pDDR_Reg->DFIODTRANKMAP;
    ddr_reg.pctl.DFITPHYWRDATA = pDDR_Reg->DFITPHYWRDATA;
    ddr_reg.pctl.DFITPHYWRLAT = pDDR_Reg->DFITPHYWRLAT;
    ddr_reg.pctl.DFITRDDATAEN = pDDR_Reg->DFITRDDATAEN;
    ddr_reg.pctl.DFITPHYRDLAT = pDDR_Reg->DFITPHYRDLAT;
    ddr_reg.pctl.DFITPHYUPDTYPE0 = pDDR_Reg->DFITPHYUPDTYPE0;
    ddr_reg.pctl.DFITPHYUPDTYPE1 = pDDR_Reg->DFITPHYUPDTYPE1;
    ddr_reg.pctl.DFITPHYUPDTYPE2 = pDDR_Reg->DFITPHYUPDTYPE2;
    ddr_reg.pctl.DFITPHYUPDTYPE3 = pDDR_Reg->DFITPHYUPDTYPE3;
    ddr_reg.pctl.DFITCTRLUPDMIN = pDDR_Reg->DFITCTRLUPDMIN;
    ddr_reg.pctl.DFITCTRLUPDMAX = pDDR_Reg->DFITCTRLUPDMAX;
    ddr_reg.pctl.DFITCTRLUPDDLY = pDDR_Reg->DFITCTRLUPDDLY;
    
    ddr_reg.pctl.DFIUPDCFG = pDDR_Reg->DFIUPDCFG;
    ddr_reg.pctl.DFITREFMSKI = pDDR_Reg->DFITREFMSKI;
    ddr_reg.pctl.DFITCTRLUPDI = pDDR_Reg->DFITCTRLUPDI;
    ddr_reg.pctl.DFISTCFG0 = pDDR_Reg->DFISTCFG0;
    ddr_reg.pctl.DFISTCFG1 = pDDR_Reg->DFISTCFG1;
    ddr_reg.pctl.DFITDRAMCLKEN = pDDR_Reg->DFITDRAMCLKEN;
    ddr_reg.pctl.DFITDRAMCLKDIS = pDDR_Reg->DFITDRAMCLKDIS;
    ddr_reg.pctl.DFISTCFG2 = pDDR_Reg->DFISTCFG2;
    ddr_reg.pctl.DFILPCFG0 = pDDR_Reg->DFILPCFG0;

    //PUBL
    ddr_reg.publ.PIR = pPHY_Reg->PIR;
    ddr_reg.publ.PGCR = pPHY_Reg->PGCR;
    ddr_reg.publ.DLLGCR = pPHY_Reg->DLLGCR;
    ddr_reg.publ.ACDLLCR = pPHY_Reg->ACDLLCR;
    ddr_reg.publ.PTR[0] = pPHY_Reg->PTR[0];
    ddr_reg.publ.PTR[1] = pPHY_Reg->PTR[1];
    ddr_reg.publ.PTR[2] = pPHY_Reg->PTR[2];
    ddr_reg.publ.ACIOCR = pPHY_Reg->ACIOCR;
    ddr_reg.publ.DXCCR = pPHY_Reg->DXCCR;
    ddr_reg.publ.DSGCR = pPHY_Reg->DSGCR;
    ddr_reg.publ.DCR = pPHY_Reg->DCR.d32;
    ddr_reg.publ.ODTCR = pPHY_Reg->ODTCR;
    ddr_reg.publ.DTAR = pPHY_Reg->DTAR;
    ddr_reg.publ.ZQ0CR0 = (pPHY_Reg->ZQ0SR[0] & 0x0FFFFFFF) | (0x1<<28);
    ddr_reg.publ.ZQ1CR0 = (pPHY_Reg->ZQ1SR[0] & 0x0FFFFFFF) | (0x1<<28);
    
    ddr_reg.publ.DX0GCR = pPHY_Reg->DATX8[0].DXGCR;
    ddr_reg.publ.DX0DLLCR = pPHY_Reg->DATX8[0].DXDLLCR;
    ddr_reg.publ.DX0DQTR = pPHY_Reg->DATX8[0].DXDQTR;
    ddr_reg.publ.DX0DQSTR = pPHY_Reg->DATX8[0].DXDQSTR;

    ddr_reg.publ.DX1GCR = pPHY_Reg->DATX8[1].DXGCR;
    ddr_reg.publ.DX1DLLCR = pPHY_Reg->DATX8[1].DXDLLCR;
    ddr_reg.publ.DX1DQTR = pPHY_Reg->DATX8[1].DXDQTR;
    ddr_reg.publ.DX1DQSTR = pPHY_Reg->DATX8[1].DXDQSTR;

    ddr_reg.publ.DX2GCR = pPHY_Reg->DATX8[2].DXGCR;
    ddr_reg.publ.DX2DLLCR = pPHY_Reg->DATX8[2].DXDLLCR;
    ddr_reg.publ.DX2DQTR = pPHY_Reg->DATX8[2].DXDQTR;
    ddr_reg.publ.DX2DQSTR = pPHY_Reg->DATX8[2].DXDQSTR;

    ddr_reg.publ.DX3GCR = pPHY_Reg->DATX8[3].DXGCR;
    ddr_reg.publ.DX3DLLCR = pPHY_Reg->DATX8[3].DXDLLCR;
    ddr_reg.publ.DX3DQTR = pPHY_Reg->DATX8[3].DXDQTR;
    ddr_reg.publ.DX3DQSTR = pPHY_Reg->DATX8[3].DXDQSTR;

    //NOC
    ddr_reg.DdrConf = *(volatile uint32_t *)SysSrv_DdrConf;
    ddr_reg.DdrMode = *(volatile uint32_t *)SysSrv_DdrMode;
    ddr_reg.ReadLatency = *(volatile uint32_t *)SysSrv_ReadLatency;
}
EXPORT_SYMBOL(ddr_reg_save);

__attribute__((aligned(4))) __sramdata uint32 ddr_reg_resume[] = 
{
#include "ddr_reg_resume.inc"
};

int ddr_init(uint32_t dram_speed_bin, uint32_t freq)
{
    volatile uint32_t value = 0;
    uint32_t die=1;
    uint32_t gsr,dqstr;

    ddr_print("version 1.00 201300814 \n");

    mem_type = pPHY_Reg->DCR.b.DDRMD;
    ddr_speed_bin = dram_speed_bin;

    if(freq != 0)
        ddr_freq = freq;
    else
        ddr_freq = clk_get_rate(clk_get(NULL, "ddr"))/1000000;

    ddr_sr_idle = 0;

#if defined(CONFIG_ARCH_RK3188)
    ddr_soc_is_rk3188_plus = soc_is_rk3188plus();
    ddr_rk3188_dpll_is_good = ddr_get_dpll_status();
#endif        
    switch(mem_type)
    {
        case DDR3:
            if(pPMU_Reg->PMU_PMU_SYS_REG[2])
            {
                die = (8<<READ_BW_INFO())/(8<<READ_DIE_BW_INFO());
            }
            else
            {
                if(pDDR_Reg->PPCFG & 1)
                {
                        die=1;
                }
                else
                {
                        die = 2;
                }
            }
            ddr_print("DDR3 Device\n");
            break;
        case LPDDR2:
            ddr_print("LPDDR2 Device\n");
            break;
        case DDR2:
            ddr_print("DDR2 Device\n");
            break;
        case DDR:
            ddr_print("DDR Device\n");
            break;
        default:
            ddr_print("LPDDR Device\n");
            break;
    }
    //get capability per chip, not total size, used for calculate tRFC
    ddr_capability_per_die = ddr_get_cap()/(ddr_get_cs()*die);
    ddr_print("Bus Width=%d Col=%d Bank=%d Row=%d CS=%d Total Capability=%dMB\n", 
                                                                    ddr_get_bw()*16,\
                                                                    ddr_get_col(), \
                                                                    (0x1<<(ddr_get_bank())), \
                                                                    ddr_get_row(), \
                                                                    ddr_get_cs(), \
                                                                    (ddr_get_cap()>>20));
    ddr_adjust_config(mem_type);
    
    if(ddr_rk3188_dpll_is_good == true)
   {
        if(freq != 0)
            value=ddr_change_freq(freq);
        else
            value=ddr_change_freq(clk_get_rate(clk_get(NULL, "ddr"))/1000000);
    }    
    clk_set_rate(clk_get(NULL, "ddr"), 0);
    ddr_print("init success!!! freq=%luMHz\n", clk_get_rate(clk_get(NULL, "ddr"))/1000000);

    for(value=0;value<4;value++)
    {
        gsr = pPHY_Reg->DATX8[value].DXGSR[0];
        dqstr = pPHY_Reg->DATX8[value].DXDQSTR;
        ddr_print("DTONE=0x%x, DTERR=0x%x, DTIERR=0x%x, DTPASS=0x%x, DGSL=%d extra clock, DGPS=%d\n", \
                   (gsr&0xF), ((gsr>>4)&0xF), ((gsr>>8)&0xF), ((gsr>>13)&0xFFF), (dqstr&0x7), ((((dqstr>>12)&0x3)+1)*90));
    }
    ddr_print("ZERR=%x, ZDONE=%x, ZPD=0x%x, ZPU=0x%x, OPD=0x%x, OPU=0x%x\n", \
                                                (pPHY_Reg->ZQ0SR[0]>>30)&0x1, \
                                                (pPHY_Reg->ZQ0SR[0]>>31)&0x1, \
                                                pPHY_Reg->ZQ0SR[1]&0x3,\
                                                (pPHY_Reg->ZQ0SR[1]>>2)&0x3,\
                                                (pPHY_Reg->ZQ0SR[1]>>4)&0x3,\
                                                (pPHY_Reg->ZQ0SR[1]>>6)&0x3);
    ddr_print("DRV Pull-Up=0x%x, DRV Pull-Dwn=0x%x\n", pPHY_Reg->ZQ0SR[0]&0x1F, (pPHY_Reg->ZQ0SR[0]>>5)&0x1F);
    ddr_print("ODT Pull-Up=0x%x, ODT Pull-Dwn=0x%x\n", (pPHY_Reg->ZQ0SR[0]>>10)&0x1F, (pPHY_Reg->ZQ0SR[0]>>15)&0x1F);

    return 0;
}
EXPORT_SYMBOL(ddr_init);

