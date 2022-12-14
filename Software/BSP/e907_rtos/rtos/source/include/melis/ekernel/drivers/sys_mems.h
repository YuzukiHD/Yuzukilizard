/*
*********************************************************************************************************
*                                                    ePOS
*                                   the Easy Portable/Player Operation System
*                                               mems sub-system
*
*                          (c) Copyright 1992-2006, Jean J. Labrosse, Weston, FL
*                                             All Rights Reserved
*
* File    : mems.h
* By      : Steven.ZGJ
* Version : V1.00
*********************************************************************************************************
*/
#ifndef _SYS_MEMS_H_
#define _SYS_MEMS_H_
#include <ktype.h>
#include <csp_dram_para.h>

//**********************************************************************************************************
//* define level 0( system level)
//************************************************

#define MEMS_domain_user    0x0f
#define MEMS_domain_system  0x0f
#define MEMS_domain_client  0x0e

typedef enum
{
    CSP_SRAM_ZONE_NULL      =   0x00,

    CSP_SRAM_ZONE_BOOT,                 /* ��ʹ�õ�ģ��Ϊ: BOOT��NORMAL. ע��:OS��ؽ������������ΪNORMAL  */
    CSP_SRAM_ZONE_C,                   /* ��ʹ�õ�ģ��Ϊ: CPU_DMA��VE  */
    CSP_SRAM_ZONE_ICACHE,               /* not used */
    CSP_SRAM_ZONE_DCACHE,               /* not used */

    CSP_SRAM_ZONE_MAX_NR
} csp_sram_zone_id_t;

//------Module--------------------------------------
typedef enum
{
    CSP_SRAM_MODULE_NULL    =   0x00,
    CSP_SRAM_MODULE_BOOT,             /* bootģ��  : zone c1,c2,c3��������   */
    CSP_SRAM_MODULE_NORMAL,           /* normalģ��: zone c1(CPU_DMA/VE),zone c2(DE_FE),zone c3(DE_BE) */
    CSP_SRAM_MODULE_CPU_DMA,
    CSP_SRAM_MODULE_VE,

    CSP_SRAM_MODULE_MAX_NR
} csp_sram_module_t;

typedef enum __SRAM_REQ_MODE
{
    SRAM_REQ_MODE_WAIT  = 0,    //request sram block in wait mode
    SRAM_REQ_MODE_NWAIT = 1,    //request sram block in none-wait mode
    SRAM_REQ_MODE_
} csp_sram_req_mode_e;

typedef struct  sram_zone_info
{
    csp_sram_zone_id_t  zone_id;
    uint32_t            reserved;  //uint32_t zone_size;
} csp_sram_zone_info_t;

typedef struct __SRAM_BLK_NODE
{
    intptr_t            *pLock;     //sram block lock
    csp_sram_zone_id_t  zone_id;       //sram block id
    csp_sram_module_t   module;       //sram map
    int32_t             bUse;       //flag to mark if the sram block is free
} csp_sram_blk_node_t;

typedef struct __SRAM_BLK_MANAGER
{
    csp_sram_blk_node_t *pBlkLst;   //sram block node list
    int32_t             nBlkCnt;    //sram block count
} csp_sram_blk_manager;

typedef struct __SYS_MEM_CFG
{
    intptr_t            *task_prio;
    intptr_t            *task_sem;
    uint8_t             task_flag;
    uint32_t            bw_num;
    uint32_t            bw_total;
    uint32_t            bw_max;
    uint32_t            bw_average;

    //define parameter for switch work mode and user mode
    int32_t             vid_flag ;
    int32_t             pic_flag ;
    int32_t             tv_flag ;
} __sys_mem_cfg_t;

typedef enum __DRAM_ACCESS_CMD
{
    DRAM_ACCESS_DISABLE_REQ = 0,    /* dram请求禁止访问 */
    DRAM_ACCESS_DISABLE_CANCEL,     /* dram取消禁止访问 */
    DRAM_ACCESS_
} __dram_access_cmd_t;

typedef enum __DRAM_WORK_MODE
{
    DRAM_WORK_MODE_LCD,             /* 当前为LCD工作模式�?    需要配置DRAM参数满足系统平均带宽需�?*/
    DRAM_WORK_MODE_TV,              /* 当前为TV工作模式�?     需要配置DRAM参数满足系统瞬时带宽需�?*/
    DRAM_WORK_MODE_TV_480P,         /* 当前为TV  480P工作模式，需要配置DRAM参数满足系统瞬时带宽需�?*/
    DRAM_WORK_MODE_TV_576P,         /* 当前为TV  720P工作模式，需要配置DRAM参数满足系统瞬时带宽需�?*/
    DRAM_WORK_MODE_TV_720P,         /* 当前为TV  720P工作模式，需要配置DRAM参数满足系统瞬时带宽需�?*/
    DRAM_WORK_MODE_TV_1080P,        /* 当前为TV 1080P工作模式，需要配置DRAM参数满足系统瞬时带宽需�?*/
    DRAM_WORK_MODE_
} __dram_work_mode_t;

typedef enum __DRAM_USER_MODE
{
    DRAM_USER_MODE_NONE = 0,
    DRAM_USER_MODE_VIDEO,
    DRAM_USER_MODE_MUSIC,
    DRAM_USER_MODE_PICTURE,
    DRAM_USER_MODE_ENCODER,
    DRAM_USER_MODE_
} __dram_user_mode_t;

//************************************************
// 函数定义
__s32 MEM_EnableDramSelfFresh(void);
__s32 MEM_DisableDramSelfFresh(__u32 nDrmClk);
__s32 MEM_DisableDramAcess(void);
__s32 MEM_EnableDramAcess(void);

//**********************************************************************************************************


#endif  /* _KRNL_H_ */
