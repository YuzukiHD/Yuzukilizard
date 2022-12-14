/*
*********************************************************************************************************
*                                                    MELIS
*                                    the Easy Portable/Player Develop Kits
*                                               DRAM CSP Module
*
*                                    (c) Copyright 2006-2010, Berg.Xing China
*                                             All Rights Reserved
*
* File    : csp_dram_para.h
* By      : Berg.Xing
* Version : v1.0
* Date    : 2010-12-2 13:24
* Descript:
* Update  : date                auther      ver     notes
*           2010-12-2 13:24     Berg.Xing   1.0     build the file;
*           2011-1-26 14:00     cpl         1.1     modify for aw1619 system
*********************************************************************************************************
*/
#ifndef __CSP_DRAM_PARA_H__
#define __CSP_DRAM_PARA_H__

#define DRAM_PIN_DEV_ID     (0)
#define DRAM_PIN_LIST       ((uint32_t *)0)
#define DRAM_PIN_NUMBER     (0)

typedef enum __DRAM_TYPE
{
    DRAM_TYPE_SDR   = 0,
    DRAM_TYPE_DDR   = 1,
    DRAM_TYPE_DDR2  = 2,
    DRAM_TYPE_MDDR  = 3,
} __dram_type_e;

typedef enum __DRAM_BNKMOD
{
    DRAM_ACS_INTERLEAVE = 0,
    DRAM_ACS_SEQUENCE   = 1,
} __dram_bnkmod_e;

typedef enum __DRAM_PRIO_LEVEL
{
    DRAM_PRIO_LEVEL_0 = 0,
    DRAM_PRIO_LEVEL_1,
    DRAM_PRIO_LEVEL_2,
    DRAM_PRIO_LEVEL_3,
    DRAM_PRIO_LEVEL_
} __dram_prio_level_e;

//Offset 0-4 is valid
typedef enum __BANDWIDTH_DEV
{
    BW_DEVICE_CPU = 0,
    BW_DEVICE_VE,
    BW_DEVICE_DISP,
    BW_DEVICE_DTMB,
    BW_DEVICE_OTHER,
    BW_DEVICE_TOTAL
} __bw_dev_e;

#if defined(CONFIG_SOC_SUN3IW2) || defined(CONFIG_SOC_SUN8I) || defined(CONFIG_SOC_SUN20IW1) || defined(CONFIG_SOC_SUN20IW3)
typedef struct __DRAM_MASTER
{
    uint32_t    bandwidth_limit0;
    uint32_t    bandwidth_limit1;
    uint32_t    bandwidth_limit2;
    uint32_t    command_number;
    uint32_t    master_n_wait_time;
    uint32_t    master_n_qos_value;
    uint32_t    master_limit_enable;
} __dram_master_t;

typedef struct __DRAM_BANDW
{
    uint32_t    cpu_bw;
    uint32_t    dtmb_bw;
    uint32_t    ve_bw;
    uint32_t    disp_bw;
    uint32_t    other_bw;
    uint32_t    total_bw;
} __dram_bandw_t;

typedef struct __DRAM_BWCONF
{
    uint32_t    max_statistic;
    uint32_t    statistic_unit;
    uint32_t    bw_enable;
} __dram_bwconf_t;

//Offset 0-9 is valid
typedef enum __DRAM_DEV
{
    DRAM_DEVICE_CPU = 0,
    DRAM_DEVICE_DTMB,
    DRAM_DEVICE_MAHB,
    DRAM_DEVICE_DMA,
    DRAM_DEVICE_VE,
    DRAM_DEVICE_TS,
    DRAM_DEVICE_DI,
    DRAM_DEVICE_TVIN,
    DRAM_DEVICE_DE20,
    DRAM_DEVICE_ROT,
    DRAM_DEVICE_
} __dram_dev_e;

typedef struct __DRAM_PARA
{
    //normal configuration
    uint32_t        dram_clk;
    uint32_t        dram_type;      //dram_type         DDR2: 2             DDR3: 3     LPDDR2: 6   LPDDR3: 7   DDR3L: 31
    //uint32_t      lpddr2_type;    //LPDDR2 type       S4:0    S2:1    NVM:2
    uint32_t        dram_zq;        //do not need
    uint32_t        dram_odt_en;

    //control configuration
    uint32_t        dram_para1;
    uint32_t        dram_para2;

    //timing configuration
    uint32_t        dram_mr0;
    uint32_t        dram_mr1;
    uint32_t        dram_mr2;
    uint32_t        dram_mr3;
    uint32_t        dram_tpr0;  //DRAMTMG0
    uint32_t        dram_tpr1;  //DRAMTMG1
    uint32_t        dram_tpr2;  //DRAMTMG2
    uint32_t        dram_tpr3;  //DRAMTMG3
    uint32_t        dram_tpr4;  //DRAMTMG4
    uint32_t        dram_tpr5;  //DRAMTMG5
    uint32_t        dram_tpr6;  //DRAMTMG8

    //reserved for future use
    uint32_t        dram_tpr7;
    uint32_t        dram_tpr8;
    uint32_t        dram_tpr9;
    uint32_t        dram_tpr10;
    uint32_t        dram_tpr11;
    uint32_t        dram_tpr12;
    uint32_t        dram_tpr13;
} __dram_para_t;

#elif defined (CONFIG_SOC_SUN3IW1)

typedef struct __DRAM_MASTER
{
    uint32_t    dram_prio_level;
    uint32_t    dram_threshold0;
    uint32_t    dram_threshold1;
    uint32_t    dram_request_num;
} __dram_master_t;

typedef enum __DRAM_DEV
{
    DRAM_DEVICE_NULL = 0,
    DRAM_DEVICE_DAHB,
    DRAM_DEVICE_IAHB,
    DRAM_DEVICE_DDMA,
    DRAM_DEVICE_NDMA,
    DRAM_DEVICE_SD0,
    DRAM_DEVICE_SD1,
    DRAM_DEVICE_DEBE,
    DRAM_DEVICE_DEFE,
    DRAM_DEVICE_VE,
    DRAM_DEVICE_CSI,
    DRAM_DEVICE_DeInterlace,
    DRAM_DEVICE_TVD,
    DRAM_DEVICE_
} __dram_dev_e;

typedef struct __DRAM_PARA
{
    uint32_t            base;           // dram base address
    uint32_t            size;           // dram size, based on     (unit: MByte)
    uint32_t            clk;            // dram work clock         (unit: MHz)
    uint32_t            access_mode;    // 0: interleave mode 1: sequence mode
    uint32_t            cs_num;         // dram chip count  1: one chip  2: two chip
    uint32_t            ddr8_remap;     // for 8bits data width DDR 0: normal  1: 8bits
    __dram_type_e       type;           // ddr/ddr2/sdr/mddr/lpddr/...
    uint32_t            bwidth;         // dram bus width
    uint32_t            col_width;      // column address width
    uint32_t            row_width;      // row address width
    uint32_t            bank_size;      // dram bank count
    uint32_t            cas;            // dram cas
} __dram_para_t;
#endif

#endif  //__CSP_DRAM_PARA_H__
