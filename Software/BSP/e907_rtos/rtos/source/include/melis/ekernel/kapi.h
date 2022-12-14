/*
 * =====================================================================================
 *
 *       Filename:  kapi.h
 *
 *    Description:  melis system api for user module.
 *
 *        Version:  Melis3.0
 *         Create:  2018-01-11 20:29:43
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-08-10 13:54:13
 *
 * =====================================================================================
 */

#ifndef  __MELIS_API_H__
#define  __MELIS_API_H__
#include <typedef.h>
#include <errno.h>
#include "kmsg.h"
#include "boot.h"
#include "sys_charset.h"
#include "sys_clock.h"
#include "sys_device.h"
#include "sys_fsys.h"
#include "sys_hwsc.h"
#include "sys_input.h"
#include "sys_mems.h"
#include "sys_pins.h"
#include "sys_powerman.h"
#include "sys_svc.h"
#include "sys_time.h"

#include <csp_ccm_para.h>
#include <csp_dma_para.h>
#include <csp_dram_para.h>
#include <csp_int_para.h>

typedef void (*OS_TMR_CALLBACK)(void *parg);

/* definition for KMSG para max bytes len */

typedef struct
{
    uint32_t    size;
    void        *msg;
    uint32_t    msgtype;
    uint32_t    msgsize;
    uint32_t    msglen;
    void        *data;
    uint32_t    datsize;
    uint32_t    datatype;
    uint32_t    datalen;
    uint8_t     reserve[12];
} __krnl_sktfrm_t;

#define ERESTART                        (ESTRPIPE +  1) /* Interrupted system call should be restarted */
#define EUCLEAN                         (ESTRPIPE +  2) /* Structure needs cleaning */
#define ENOTNAM                         (ESTRPIPE +  3) /* Not a XENIX named type file */
#define ENAVAIL                         (ESTRPIPE +  4) /* No XENIX semaphores available */
#define EISNAM                          (ESTRPIPE +  5) /* Is a named type file */
#define EREMOTEIO                       (ESTRPIPE +  6) /* Remote I/O error */
#define EMEDIUMTYPE                     (ESTRPIPE +  7) /* Wrong medium type */
#define ENOKEY                          (ESTRPIPE +  8) /* Required key not available */
#define EKEYEXPIRED                     (ESTRPIPE +  9) /* Key has expired */
#define EKEYREVOKED                     (ESTRPIPE + 10) /* Key has been revoked */
#define EKEYREJECTED                    (ESTRPIPE + 11) /* Key was rejected by service */

#define	SEEK_SET	                    0	    /* set file offset to offset */
#define	SEEK_CUR	                    1	    /* set file offset to current plus offset */
#define	SEEK_END	                    2	    /* set file offset to EOF plus offset */
#define ES_EOF                          (-1)    /* end of file */

#define KRNL_SKT_USR_IN                 0
#define KRNL_SKT_USR_OUT                1
#define KRNL_SKT_BUF_PHY_UNSEQ          (0x00000000)
#define KRNL_SKT_BUF_PHY_SEQ            (0x01000000)
#define KRNL_SKT_BUF_PHY_SEQMASK        (0x01000000)
#define KRNL_SKT_BUF_TYP_MASK           (0xff000000)

#define OS_PRIO_SELF                    0xFFu
#define OS_EVENT_NAME_SIZE              16
#define OS_TIMEOUT                      10u

#define OS_NO_ERR                       0u
#define KRNL_NO_ERR                     0u

#define OS_TICK_STEP_EN                 1       /* Enable tick stepping feature for uC/OS-View                  */
#define OS_TICKS_PER_SEC                100     /* Set the number of ticks in one second                        */

/* EVENTFLAGS */
#define OS_FLAG_WAIT_CLR_ALL            0u      /* Wait for ALL    the bits specified to be CLR (i.e. 0)        */
#define OS_FLAG_WAIT_CLR_AND            0u
#define OS_FLAG_WAIT_CLR_ANY            1u      /* Wait for ANY of the bits specified to be CLR (i.e. 0)        */
#define OS_FLAG_WAIT_CLR_OR             1u
#define OS_FLAG_WAIT_SET_ALL            2u      /* Wait for ALL    the bits specified to be SET (i.e. 1)        */
#define OS_FLAG_WAIT_SET_AND            2u
#define OS_FLAG_WAIT_SET_ANY            3u      /* Wait for ANY of the bits specified to be SET (i.e. 1)        */
#define OS_FLAG_WAIT_SET_OR             3u
#define OS_FLAG_CONSUME                 0x80u   /* Consume the flags if condition(s) satisfied                  */
#define OS_FLAG_CLR                     0u
#define OS_FLAG_SET                     1u

/* OS_XXX_PostOpt() OPTIONS */
#define OS_POST_OPT_NONE                0x00u   /* NO option selected                                           */
#define OS_POST_OPT_BROADCAST           0x01u   /* Broadcast message to ALL tasks waiting                       */
#define OS_POST_OPT_FRONT               0x02u   /* Post to highest priority task waiting                        */
#define OS_POST_OPT_NO_SCHED            0x04u   /* Do not call the scheduler if this option is selected         */

#define OS_TASK_STAT_EN                 0
#define OS_TMR_CFG_NAME_SIZE            16      /* Determine the size of a timer name                       */

#define OS_ERR_EVENT_TYPE               1u
#define OS_ERR_PEND_ISR                 2u
#define OS_ERR_POST_NULL_PTR            3u
#define OS_ERR_PEVENT_NULL              4u
#define OS_ERR_POST_ISR                 5u
#define OS_ERR_QUERY_ISR                6u
#define OS_ERR_INVALID_OPT              7u
#define OS_ERR_TASK_WAITING             8u
#define OS_ERR_PDATA_NULL               9u

#define OS_TIMEOUT                      10u
#define OS_TASK_NOT_EXIST               11u
#define OS_ERR_EVENT_NAME_TOO_LONG      12u
#define OS_ERR_FLAG_NAME_TOO_LONG       13u
#define OS_ERR_TASK_NAME_TOO_LONG       14u
#define OS_ERR_PNAME_NULL               15u
#define OS_ERR_TASK_CREATE_ISR          16u
#define OS_ERR_PEND_LOCKED              17u

#define OS_MBOX_FULL                    20u

#define OS_TMR_OPT_NONE                 0u      /*  No option selected                                           */
#define OS_TMR_OPT_ONE_SHOT             1u      /*  Timer will not automatically restart when it expires         */
#define OS_TMR_OPT_PERIODIC             2u      /*  Timer will     automatically restart when it expires         */
#define OS_TMR_OPT_CALLBACK             3u      /*  OSTmrStop() option to call 'callback' w/ timer arg.          */
#define OS_TMR_OPT_CALLBACK_ARG         4u      /*  OSTmrStop() option to call 'callback' w/ new   arg.          */
#define OS_TMR_OPT_NORMAL_MASK          0x0fu   /*  soft timer normal option mask                                */
#define OS_TMR_OPT_PRIO_MASK            0x30u   /*  mask of the priority soft timer                              */
#define OS_TMR_OPT_PRIO_LOW             0x00u   /*  low priority soft timer                                      */
#define OS_TMR_OPT_PRIO_HIGH            0x10u   /*  special soft timer, can be operated in isr, used be careful  */
#define OS_TMR_STATE_UNUSED             0u
#define OS_TMR_STATE_STOPPED            1u
#define OS_TMR_STATE_COMPLETED          2u
#define OS_TMR_STATE_RUNNING            3u

#define OS_DEL_NO_PEND                  0u
#define OS_DEL_ALWAYS                   1u

#define KRNL_Q_FULL                     30u
#define KRNL_Q_EMPTY                    31u

#define OS_PRIO_EXIST                   40u
#define OS_PRIO_ERR                     41u
#define OS_PRIO_INVALID                 42u

#define OS_SEM_OVF                      50u

#define OS_TASK_DEL_ERR                 60u
#define OS_TASK_DEL_IDLE                61u
#define OS_TASK_DEL_REQ                 62u
#define OS_TASK_DEL_ISR                 63u

#define OS_NO_MORE_TCB                  70u

#define OS_TIME_NOT_DLY                 80u
#define OS_TIME_INVALID_MINUTES         81u
#define OS_TIME_INVALID_SECONDS         82u
#define OS_TIME_INVALID_MILLI           83u
#define OS_TIME_ZERO_DLY                84u

#define OS_TASK_SUSPEND_PRIO            90u
#define OS_TASK_SUSPEND_IDLE            91u

#define OS_TASK_RESUME_PRIO             100u
#define OS_TASK_NOT_SUSPENDED           101u


#define OS_ERR_NOT_MUTEX_OWNER          120u

#define OS_TASK_OPT_ERR                 130u

#define OS_ERR_DEL_ISR                  140u
#define OS_ERR_CREATE_ISR               141u

#define OS_FLAG_INVALID_PGRP            150u
#define OS_FLAG_ERR_WAIT_TYPE           151u
#define OS_FLAG_ERR_NOT_RDY             152u
#define OS_FLAG_INVALID_OPT             153u
#define KRNL_FLAG_GRP_DEPLETED          154u

#define OS_ERR_PIP_LOWER                160u

#define KRNL_priolevel0                 0       /* reserved for system(highest level)                           */
#define KRNL_priolevel1                 1
#define KRNL_priolevel2                 2
#define KRNL_priolevel3                 3
#define KRNL_priolevel4                 4
#define KRNL_priolevel5                 5
#define KRNL_priolevel6                 6
#define KRNL_priolevel7                 7       /* reserved for system(lowest level)                            */

#define EXEC_pidself                    0xff
#define EXEC_prioself                   0xff

/*wait process main function to return  */
#define EXEC_CREATE_WAIT_RET            (1<<0)

typedef struct  KNRL_STMR
{
    uint16_t        err;
} __krnl_stmr_t;

typedef struct os_flag_grp                      /* Event Flag Group                                         */
{
    uint8_t         OSFlagType;                 /* Should be set to OS_EVENT_TYPE_FLAG                      */
    void            *OSFlagWaitList;            /* Pointer to first NODE of task waiting on event flag      */
    __krnl_flags_t  OSFlagFlags;                /* 8, 16 or 32 bit flags                                    */
} __krnl_flag_grp_t;

typedef struct __EXEC_MGSEC
{
    char            magic[8];
    uint32_t        version;
    uint8_t         type;
    uint32_t        heapaddr;
    uint32_t        heapsize;
    int32_t         (*main)(void *p_arg);
    uint32_t        mtskstksize;
    uint8_t         mtskprio;
} __exec_mgsec_t;

typedef struct __krnl_q_data_t
{
    void            *OSMsg;                     /* Pointer to next message to be extracted from queue       */
    uint16_t        OSNMsgs;                    /* Number of messages in message queue                      */
    uint16_t        OSQSize;                    /* Size of message queue                                    */
    uint16_t        OSEventTbl[16];             /* List of tasks waiting for event to occur         */
    uint16_t        OSEventGrp;                 /* Group corresponding to tasks waiting for event to occur  */
} __krnl_q_data_t;

typedef enum enum_EPDK_VER_TYPE
{
    EPDK_VER,
    EPDK_VER_OS,
    EPDK_VER_CHIP,
    EPDK_VER_PID,
    EPDK_VER_SID,
    EPDK_VER_BID,
    EPDK_VER_CHIP_SUB,
} __epdk_ver_type_t;

//CFG
extern int32_t      esCFG_Exit(void);
extern int32_t      esCFG_Exit_Ex(intptr_t* parser);
extern int32_t      esCFG_GetGPIOSecData(char *GPIOSecName, void *pGPIOCfg, int32_t GPIONum);
extern int32_t      esCFG_GetGPIOSecData_Ex(intptr_t* parser, char *GPIOSecName, void *pGPIOCfg, int32_t GPIONum);
extern int32_t      esCFG_GetGPIOSecKeyCount(char *GPIOSecName);
extern int32_t      esCFG_GetGPIOSecKeyCount_Ex(intptr_t* parser, char *GPIOSecName);
extern int32_t      esCFG_GetKeyValue(char *SecName, char *KeyName, int32_t Value[], int32_t Count);
extern int32_t      esCFG_GetKeyValue_Ex(intptr_t* parser, char *KeyName, int32_t Value[], int32_t Count);
extern int32_t      esCFG_GetSecCount(void);
extern int32_t      esCFG_GetSecCount_Ex(intptr_t* parser);
extern int32_t      esCFG_GetSecKeyCount(char *SecName);
extern int32_t      esCFG_GetSecKeyCount_Ex(intptr_t* parser, char *SecName);
extern int32_t      esCFG_Init(uint8_t *CfgVAddr, uint32_t size);
extern intptr_t*    esCFG_Init_Ex(char *path);
extern void         esCFG_Dump(void);

//CHS
extern int32_t      esCHS_Char2Uni(int32_t type, const uint8_t *str, uint32_t len, uint16_t *uni);
extern uint32_t     esCHS_GetChLowerTbl(int32_t charset_type, void *buf, uint32_t size);
extern uint32_t     esCHS_GetChUpperTbl(int32_t type, void *buf, uint32_t size);
extern int32_t      eschs_init(uint32_t mode, void *p_arg);
extern int32_t      esCHS_Uni2Char(int32_t type, uint16_t uni, uint8_t *str, uint32_t len);

//CLOCK
#ifdef CONFIG_MELIS_LEGACY_DRIVER_MAN
extern int32_t     esCLK_CloseMclk(sys_clk_id_t mclk);
extern int32_t     esCLK_GetMclkDiv(sys_clk_id_t mclk);
extern sys_clk_id_t    esCLK_GetMclkSrc(sys_clk_id_t mclk);
extern int32_t     esCLK_SetSrcFreq(sys_clk_id_t sclk, uint32_t nFreq);
extern uint32_t    esCLK_GetSrcFreq(sys_clk_id_t sclk);
extern int32_t     esCLK_OpenMclk(sys_clk_id_t mclk);
extern int32_t     esCLK_MclkRegCb(sys_clk_id_t mclk, __pCB_ClkCtl_t pCb);
extern int32_t     esCLK_MclkUnregCb(sys_clk_id_t mclk, __pCB_ClkCtl_t pCb);
extern int32_t     esCLK_SetMclkSrc(sys_clk_id_t mclk, sys_clk_id_t sclk);
extern int32_t     esCLK_GetRound_Rate(sys_clk_id_t clk, uint32_t rate);
extern int32_t     esCLK_SetMclkDiv(sys_clk_id_t mclk, uint32_t nDiv);
extern int32_t     esCLK_MclkOnOff(sys_clk_id_t mclk, uint32_t bOnOff);
extern int32_t     esCLK_MclkAssert(sys_clk_id_t r_mclk);
extern int32_t     esCLK_MclkDeassert(sys_clk_id_t r_mclk);
extern int32_t     esCLK_MclkReset(sys_clk_id_t r_mclk);
extern int32_t     esCLK_MclkGetRstStatus(sys_clk_id_t r_mclk);
#endif
extern void        esCLK_SysInfo(const char *name);
extern void        esCLK_ModInfo(const char *name);

//DEVICE
extern void*       esDEV_DevReg(const char *classname, const char *name, const __dev_devop_t *pDevOp, void *pOpenArg);
extern int32_t     esDEV_DevUnreg(void* hNode);
extern int32_t     esDEV_Plugin(char *plgmsg, uint32_t devno, void *p_arg, uint8_t prio);
extern int32_t     esDEV_Plugout(char *plgmsg, uint32_t devno);
extern int32_t     esDEV_Ioctl(void* hDev, uint32_t cmd, long aux, void *pbuffer);
extern uint32_t    esDEV_Read(void *pdata, uint32_t size, uint32_t n, void* hDev);
extern void*       esDEV_Open(void* hNode, uint32_t Mode);
extern int32_t     esDEV_Close(void* hDev);
extern uint32_t    esDEV_Write(const void *pdata, uint32_t size, uint32_t n, void* hDev);
extern int32_t     esDEV_Lock(void* hNode);
extern int32_t     esDEV_Unlock(void* hNode);
extern int32_t     esDEV_Insmod(char *modfile, uint32_t devno, void *p_arg);
extern int32_t     esDEV_Unimod(char *modfile, uint32_t devno);

//DMA
extern void*        esDMA_Request(void);
extern int32_t      esDMA_Release(void* dma);
extern int32_t      esDMA_Setting(void* dma, void *arg);
extern int32_t      esDMA_Start(void* dma);
extern int32_t      esDMA_Stop(void* dma);
extern int32_t      esDMA_Restart(void* dma);
extern csp_dma_status   esDMA_QueryStat(void* dma);
extern unsigned long    esDMA_QuerySrc(void* dma);
extern unsigned long    esDMA_QueryDst(void* dma);
extern int32_t          esDMA_EnableINT(void* dma, int32_t type);
extern int32_t          esDMA_DisableINT(void* dma, int32_t type);
extern unsigned long    esDMA_QueryRestCount(void* dma);
extern int32_t      esDMA_ChangeMode(void* dma, int32_t mode);
extern int32_t      esDMA_RegDmaHdler(void* dma, void * hdler, void *arg);
extern int32_t      esDMA_UnregDmaHdler(void* dma, int32_t type, __pCBK_t hdler);
extern void         esDMA_Information(void);

// EXEC
extern uint8_t      esEXEC_PCreate(const char *filename, void *p_arg, uint32_t mode, uint32_t *ret);
extern int8_t       esEXEC_PDel(uint8_t id);
extern int8_t       esEXEC_PDelReq(uint8_t pid);
extern int8_t       esEXEC_Run(const char *pfilename, void *p_arg, uint32_t mode, uint32_t *ret);

//FSYS
extern int32_t      esFSYS_clearpartupdateflag(const char *path);
extern int32_t      esFSYS_closedir(void* hDir);
extern int32_t      esFSYS_fclose(void* hFile);
extern void*        esFSYS_fd2file(int32_t  fd);
extern void         esFSYS_ferrclr(void* hFile);
extern int32_t      esFSYS_ferror(void* hFile);
extern int32_t      esFSYS_file2fd(void* hFile);
extern int32_t      esFSYS_fioctrl(__hdle hFile, int32_t Cmd, long Aux, void *pBuffer);
extern void*        esFSYS_fopen(const char *pFileName, const char *pMode);
extern int32_t      esFSYS_format(const char *partname, const char *fstype, void* fmtpara);
extern uint32_t     esFSYS_fread(void *pData, uint32_t Size, uint32_t N, void* hFile);
extern int32_t      esFSYS_fsdbg(const char *cmd, const char *para);
extern int32_t      esFSYS_fseek(void* hFile, int32_t Offset, int32_t Whence);
extern int64_t      esFSYS_fseekex(void* hFile, int32_t l_off, int32_t h_off, int32_t Whence);
extern int32_t      esFSYS_fsreg(void* hFS);
extern int32_t      esFSYS_fstat(void* hFile, void *stat_buf);
extern int32_t      esFSYS_fsunreg(void* hFS);
extern int32_t      esFSYS_fsync(void* hFile);
extern int32_t      esFSYS_ftell(void* hFile);
extern int32_t      esFSYS_ftellex(void* hFile, int32_t *l_pos, int32_t *h_pos);
extern int32_t      esFSYS_ftruncate(void*  filehandle, uint32_t length);
extern uint32_t     esFSYS_fwrite(const void *pData, uint32_t Size, uint32_t N, void* hFile);
extern int32_t      esFSYS_getfscharset(const char *partname, int32_t *pCharset);
extern int32_t      esFSYS_mkdir(const char *pDirName);
extern int32_t      esFSYS_mntfs(void* part);
extern int32_t      esFSYS_mntparts(void* hNode);
extern int32_t      esFSYS_open(const char *name, int32_t flag, int32_t prems);
extern void*        esFSYS_opendir(const char *pDirName);
extern int32_t      esFSYS_partfslck(char *partname);
extern int32_t      esFSYS_partfsunlck(char *partname);
extern int32_t      esFSYS_pclose(void* hPart);
extern int32_t      esFSYS_pdreg(void* hPD);
extern int32_t      esFSYS_pdunreg(void* hPD);
extern void         esFSYS_perrclr(void* hPart);
extern int32_t      esFSYS_perror(void* hPart);
extern int32_t      esFSYS_pioctrl(void* hPart, uint32_t cmd, long aux, void *pbuffer);
extern void*        esFSYS_popen(const char *PartName, const char *pMode);
extern uint32_t     esFSYS_pread(void *pData, uint32_t sector, uint32_t n, void* hPart);
extern int32_t      esFSYS_premove(const char *pFileName);
extern int32_t      esFSYS_prename(const char *newfilename, const char *oldfilename);
extern uint32_t     esFSYS_pwrite(const void *pData, uint32_t Sector, uint32_t N, void* hPart);
extern int32_t      esFSYS_querypartupdateflag(const char *path, __bool *flag);
extern void*        esFSYS_readdir(void* hDir);
extern int32_t      esFSYS_remove(const char *pFileName);
extern int32_t      esFSYS_rename(const char *newname, const char *oldname);
extern void         esFSYS_rewinddir(void* hDir);
extern int32_t      esFSYS_rmdir(const char *pDirName);
extern int32_t      esFSYS_setfs(char *partname, uint32_t cmd, int32_t aux, char *para);
extern int32_t      esFSYS_statfs(const char *path, void* buf, uint32_t flags);
extern int32_t      esFSYS_statpt(const char *path, void* buf);
extern int32_t      esFSYS_umntfs(void* part, int32_t force);
extern int32_t      esFSYS_umntparts(void* hNode, uint32_t force);
extern uint32_t     fsys_init(void);

//HID
extern int32_t      esHID_SendMsg(uint32_t msgid);
extern int32_t      esHID_hiddevreg(void* hNode);
extern int32_t      esHID_hiddevunreg(void* hNode, uint32_t mode);
extern int32_t      HID_Init(void);
extern int32_t      HID_Exit(void);

//INPUT
extern int32_t      esINPUT_GetLdevID(void* graber);
extern int32_t      esINPUT_LdevCtl(int32_t LdevId, int32_t cmd, int32_t aux, void *pBuffer);
extern int32_t      esINPUT_LdevFeedback(void* graber, __input_event_packet_t *packet);
extern void*        esINPUT_LdevGrab(char *ldev, __pCBK_t callback, void *pArg, int32_t aux);
extern int32_t      esINPUT_LdevRelease(void* graber);
extern int32_t      esINPUT_RegDev(__input_dev_t *dev);
extern int32_t      esINPUT_SendEvent(__input_dev_t *dev, uint32_t type, uint32_t code, int32_t value);
extern int32_t      esINPUT_UnregDev(__input_dev_t *dev);

//INT
extern int32_t      esINT_InsISR(uint32_t irq, char* name, void *handler, void *argv);
extern int32_t      esINT_UniISR(uint32_t irq, void *dev_id);
extern int32_t      esINT_InsFIR(uint32_t fiqno, __pISR_t pFirMain, __pCBK_t pFirTail, void *pArg);
extern int32_t      esINT_UniFIR(uint32_t fiqno, __pISR_t pFirMain, __pCBK_t pFirTail);
extern int32_t      esINT_SetIRQPrio(uint32_t irqno, uint32_t prio);
extern int32_t      esINT_DisableINT(uint32_t irqno);
extern int32_t      esINT_EnableINT(uint32_t irqno);

//KRNL
extern void         esKRNL_TimeDly(uint16_t ticks);
extern void         esKRNL_TimeSet(uint32_t ticks);
extern void         esKRNL_SemPend(void* sem, uint16_t timeout, uint8_t *err);
extern void         esKRNL_SemSet(void* pevent, uint16_t cnt, uint8_t *err);
extern void         esKRNL_MutexPend(void* pevent, uint16_t timeout, uint8_t *err);
extern void         esKRNL_FlagNameSet(void* pgrp, uint8_t *pname, uint8_t *err);
extern void         esKRNL_SchedLock(void);
extern void         esKRNL_SchedUnlock(void);
extern unsigned long esKRNL_InterruptDisable(void);
extern void         esKRNL_InterruptEnable(unsigned long level);
extern void         esKRNL_TaskPrefEn(uint32_t en);
extern void         esKRNL_MemLeakChk(uint32_t en);
extern void         esKRNL_DumpStack(void);

extern int8_t       esKRNL_TDel(uint32_t prio);
extern int8_t       esKRNL_TDelReq(uint32_t prio_ex);
extern int8_t       esKRNL_TaskDel(uint32_t prio);
extern int8_t       esKRNL_TaskDelReq(uint32_t prio_ex);
extern uint8_t      esKRNL_TaskResume(uint32_t prio);
extern long         esKRNL_TaskNameSet(uint32_t prio, char *name);
extern uint8_t      esKRNL_TaskSuspend(uint32_t prio);
extern uint8_t      esKRNL_TimeDlyResume(uint32_t prio_ex);
extern uint8_t      esKRNL_TaskQuery(uint32_t prio, __krnl_tcb_t *p_task_data);
extern uint8_t      esKRNL_SemPost(void* sem);
extern uint8_t      esKRNL_SemQuery(void* sem, OS_SEM_DATA *p_sem_data);
extern uint8_t      esKRNL_MboxPost(void* pevent, unsigned long msg);
extern uint8_t      esKRNL_MboxPostOpt(void* pevent, uint32_t msg, uint8_t opt);
extern uint8_t      esKRNL_MboxQuery(void* pevent, void *p_mbox_data);
extern uint8_t      esKRNL_MutexPost(void* pevent);
extern uint8_t      esKRNL_QFlush(void* pevent);
extern uint8_t      esKRNL_QPost(void* pevent, unsigned long msg);
extern uint8_t      esKRNL_QPostFront(void* pevent, void *msg);
extern uint8_t      esKRNL_QPostOpt(void* pevent, void *msg, uint8_t opt);
extern uint8_t      esKRNL_QQuery(void* pevent, __krnl_q_data_t *p_q_data);
extern uint8_t      esKRNL_TmrStateGet(void* ptmr);
extern uint8_t      esKRNL_FlagNameGet(void* pgrp, uint8_t *pname, uint8_t *err);
extern uint8_t      esKSRV_GetSocID(void);

extern uint32_t         esKRNL_TCreate(void (*entry)(void *p_arg), void *p_arg, uint32_t stksize, uint16_t id_priolevel);
extern uint32_t         esKRNL_TimeGet(void);
extern unsigned long    esKRNL_MboxAccept(void* pevent);
extern unsigned long    esKRNL_MboxDel(void* pevent, uint8_t opt, uint8_t *err);
extern uint32_t         esKRNL_MboxPend(void* pevent, uint16_t timeout, uint8_t *err);
extern unsigned long    esKRNL_MutexDel(void* pevent, uint8_t opt, uint8_t *err);
extern unsigned long    esKRNL_QAccept(void* pevent, uint8_t *err);
extern unsigned long    esKRNL_QPend(void* pevent, uint16_t timeout, uint8_t *err);
extern uint32_t         esKRNL_FlagAccept(void* pgrp, uint32_t flags, uint8_t wait_type, uint8_t *err);
extern unsigned long    esKRNL_FlagDel(void* pgrp, uint8_t opt, uint8_t *err);
extern uint32_t         esKRNL_FlagPend(void* pgrp, uint32_t flags, uint32_t waittype_timeout, uint8_t *err);
extern uint32_t         esKRNL_FlagPendGetFlagsRdy(void);
extern uint32_t         esKRNL_FlagPost(void* pgrp, uint32_t flags, uint8_t opt, uint8_t *err);
extern uint32_t         esKRNL_FlagQuery(void* pgrp, uint8_t *err);
extern uint32_t         esKRNL_TmrRemainGet(void* ptmr);
extern unsigned long    esKRNL_GetTIDCur(void);
extern uint32_t         esKRNL_Time(void);

extern int32_t      esKRNL_TmrDel(void* ptmr);
extern int32_t      esKRNL_TmrStart(void* ptmr);
extern int32_t      esKRNL_TmrStop(void* ptmr, int8_t opt, void *callback_arg);
extern int32_t      esKRNL_SktDel(void* skt, uint8_t opt);
extern int32_t      esKRNL_SktPost(void* skt, uint8_t user, __krnl_sktfrm_t *frm);

extern uint16_t     esKRNL_SemAccept(void *psem);
extern int16_t      esKRNL_TmrError(void* ptmr);
extern uint16_t     esKRNL_Version(void);

extern void*        esKRNL_MutexCreate(uint8_t prio, uint8_t *err);
extern void*        esKRNL_SktCreate(uint32_t depth, uint32_t dbuf_attr, uint32_t mbuf_attr);
extern void*        esKRNL_SemCreate(uint16_t cnt);
extern void*        esKRNL_SemDel(void* sem, uint8_t opt, uint8_t *err);
extern void*        esKRNL_MboxCreate(uint32_t msg);
extern void*        esKRNL_QCreate(uint16_t size);
extern void*        esKRNL_QDel(void* pevent, uint8_t opt, uint8_t *err);
extern void*        esKRNL_FlagCreate(uint32_t flags, uint8_t *err);
extern void*        esKRNL_TmrCreate(uint32_t period, uint8_t opt, OS_TMR_CALLBACK callback, void *callback_arg);
extern __pCBK_t     esKRNL_GetCallBack(__pCBK_t cb);
extern long         esKRNL_CallBack(__pCBK_t cb, void *arg);
extern __krnl_sktfrm_t  *esKRNL_SktPend(void* skt, uint8_t user, uint32_t timeout);
extern __krnl_sktfrm_t  *esKRNL_SktAccept(void* skt, uint8_t user);
extern long         esKRNL_Ioctrl(void *hdle, int cmd, void *arg);

//KRSV
extern void             esKSRV_Reset(void);
extern void             esKSRV_PowerOff(void);
extern int32_t          esKSRV_SysInfo(void);
extern void*            esKSRV_Get_Display_Hld(void);
extern void             esKSRV_Get_Mixture_Hld(int *mid,unsigned long *mp);
extern void             esKSRV_Save_Mixture_Hld(int mid, void *mp);
extern void             esKSRV_Save_Display_Hld(void *hld);
extern int32_t          esKSRV_SendMsgEx(void *msg);
extern unsigned long    esKSRV_GetMsg(void);
extern uint32_t         esKSRV_GetVersion(__epdk_ver_type_t type);
extern uint32_t         esKSRV_Random(uint32_t max);
extern int32_t          esKSRV_SendMsg(uint32_t msgid, uint32_t prio);
extern int32_t          esKSRV_EnableWatchDog(void);
extern int32_t          esKSRV_DisableWatchDog(void);
extern int32_t          esKSRV_ClearWatchDog(void);
extern int32_t          esKSRV_EnableWatchDogSrv(void);
extern int32_t          esKSRV_DisableWatchDogSrv(void);
extern int32_t          esKSRV_memcpy(void *pdest, const void *psrc, size_t size);
extern unsigned long    esKSRV_GetLowMsg(void);
extern unsigned long    esKSRV_GetHighMsg(void);
extern int32_t          esKSRV_GetPara(uint32_t type, void *format, void *para);
extern int32_t          esKSRV_GetDramCfgPara(__dram_para_t *drampara);
extern int32_t          esKSRV_memset(void *pmem, uint8_t value, size_t size);
extern int32_t          esKSRV_GetAddPara(__ksrv_add_para *ksrv_add_para);
extern uint32_t         esKSRV_close_logo(void);
extern uint32_t         esKSRV_release_logo_buf(void);

//MEMS
extern uint32_t         esMEMS_TotalMemSize(void);
extern uint32_t         esMEMS_FreeMemSize(void);
extern unsigned long    esMEMS_VA2PA(unsigned long vaddr);
extern uint32_t         iomem_phy2virt_addr(uint32_t phyaddr);
extern uint32_t         esMEMS_GetIoVAByPA(uint32_t phyaddr, uint32_t size);
extern int32_t          esMEMS_LockCache_Init(void);

extern int32_t          esMEMS_VMCreate(void *pBlk, uint32_t npage, int8_t domain);
extern int32_t          esMEMS_HeapCreate(void *heapaddr, uint32_t initnpage);
extern int32_t          esMEMS_LockICache(void *addr, uint32_t size);
extern int32_t          esMEMS_UnlockICache(void *addr);
extern int32_t          esMEMS_LockDCache(void *addr, uint32_t size);
extern int32_t          esMEMS_PhyAddrConti(void *mem, unsigned long size);
extern int32_t          esMEMS_UnlockDCache(void *addr);
extern int32_t          esMEMS_Info(void);
extern int32_t          esMEM_DramWakeup(void);
extern int32_t          esMEM_DramSuspend(void);
extern int32_t          esMEM_UnRegDramAccess(void* user);
extern int32_t          esMEM_MasterSet(__dram_dev_e mod, __dram_master_t *master);
extern int32_t          esMEM_MasterGet(__dram_dev_e mod, __dram_master_t *master);
extern int32_t          esMEM_RequestDramUsrMode(__dram_user_mode_t mode);
extern int32_t          esMEM_ReleaseDramUsrMode(__dram_user_mode_t mode);
extern int32_t          esMEM_BWEnable(void);
extern int32_t          esMEM_BWDisable(void);
extern int32_t          esMEM_SramSwitchBlk(intptr_t* hSram, csp_sram_module_t uMap);
extern intptr_t         *esMEM_SramReqBlk(csp_sram_module_t uBlock, csp_sram_req_mode_e uMode);
extern int32_t          esMEM_SramRelBlk(intptr_t* hSram);
extern int32_t          esMEM_BWGet(__bw_dev_e mod);
extern void             esMEMS_Pfree(void *mblk, uint32_t npage);
extern void             *esMEMS_VMalloc(uint32_t size);
extern void             esMEMS_VMDelete(void *pBlk, uint32_t npage);
extern void             esMEMS_VMfree(void *ptr);
extern void             esMEMS_HeapDel(void *heap);
extern void             esMEMS_Mfree(void *heap, void *ptr);
extern void             esMEMS_Bfree(void *addr, unsigned long size);

extern void*            esMEMS_Malloc(void *heap, uint32_t size);
extern void*            esMEMS_Palloc(uint32_t npage, uint32_t mode);
extern void*            esMEMS_Realloc(void *heap, void *f, uint32_t size);
extern void*            esMEMS_Calloc(void *heap, uint32_t n, uint32_t m);
extern void*            esMEMS_Balloc(unsigned long size);

extern void             esMEMS_CleanDCache(void);
extern void             esMEMS_CleanFlushDCache(void);
extern void             esMEMS_CleanFlushCache(void);
extern void             esMEMS_FlushDCache(void);
extern void             esMEMS_FlushICache(void);
extern void             esMEMS_FlushCache(void);
extern void             esMEMS_CleanDCacheRegion(void *adr, uint32_t bytes);
extern void             esMEMS_CleanFlushDCacheRegion(void *adr, uint32_t bytes);
extern void             esMEMS_CleanFlushCacheRegion(void *adr, uint32_t bytes);
extern void             esMEMS_FlushDCacheRegion(void *adr, uint32_t bytes);
extern void             esMEMS_FlushICacheRegion(void *adr, uint32_t bytes);
extern void             esMEMS_FlushCacheRegion(void *adr, uint32_t bytes);
extern void             esMEMS_CleanInvalidateCacheAll(void);
extern void*            esMEM_RegDramAccess(uint8_t dev_type, __pCB_ClkCtl_t dram_access);

//MSTUB
extern unsigned long    esMSTUB_GetFuncEntry(int32_t id, uint32_t funcnum);
extern int32_t          esMSTUB_UnRegFuncTbl(int32_t id);
extern int32_t          esMSTUB_RegFuncTbl(int32_t id, void *tbl);

//PIN
extern int32_t      esPINS_ClearPending(void* hPin);
extern int32_t      esPINS_DisbaleInt(void* hPin);
extern void*        esPINS_PinGrpReq(user_gpio_set_t *pGrpStat, uint32_t GrpSize);
extern int32_t      esPINS_PinGrpRel(void* hPin, int32_t bRestore);
extern int32_t      esPINS_GetPinGrpStat(void* hPin, user_gpio_set_t *pGrpStat, uint32_t GrpSize, __bool bFromHW);
extern int32_t      esPINS_GetPinStat(void* hPin, user_gpio_set_t *pPinStat, const char *pPinName, __bool bFromHW);
extern int32_t      esPINS_SetPinStat(void* hPin, user_gpio_set_t *pPinStat, const char *pPinName, __bool bSetUserStat);
extern int32_t      esPINS_SetPinIO(void* hPin, __bool bOut, const char *pPinName);
extern int32_t      esPINS_SetPinPull(void* hPin, uint32_t PullStat, const char *pPinName);
extern int32_t      esPINS_SetPinDrive(void* hPin, uint32_t DriveLevel, const char *pPinName);
extern int32_t      esPINS_ReadPinData(void* hPin, const char *pPinName);
extern int32_t      esPINS_WritePinData(void* hPin, uint32_t Value, const char *pPinName);
extern int32_t      esPINS_EnbaleInt(void* hPin);
extern int32_t      esPINS_QueryInt(void* hPin, __bool *pIntStatus);
extern int32_t      esPINS_SetIntMode(void* hPin, uint32_t IntMode);
extern int32_t      esPINS_RegIntHdler(void* hPin, __pCBK_t hdler, void *arg);
extern int32_t      esPINS_UnregIntHdler(void* hPin, __pCBK_t hdler);

//POWER
extern int32_t      esPWRMAN_ReqPwrmanMode(int32_t mode);
extern int32_t      esPWRMAN_RelPwrmanMode(int32_t mode);
extern void         esPWRMAN_EnterStandby(uint32_t power_off_flag);
extern void         esPWRMAN_UsrEventNotify(void);
extern int32_t      esPWRMAN_LockCpuFreq(void);
extern int32_t      esPWRMAN_UnlockCpuFreq(void);
extern int32_t      esPWRMAN_RegDevice(__sys_pwrman_dev_e device, __pCB_DPMCtl_t cb, void *parg);
extern int32_t      esPWRMAN_UnregDevice(__sys_pwrman_dev_e device, __pCB_DPMCtl_t cb);
extern int32_t      esPWRMAN_GetStandbyPara(__sys_pwrman_para_t *pStandbyPara);
extern int32_t      esPWRMAN_SetStandbyMode(uint32_t standby_mode);

// RESM
extern void*        esRESM_ROpen(const char *file, const char *mode);
extern int32_t      esRESM_RClose(__resm_rsb_t *res);
extern uint32_t     esRESM_RRead(void *pdata, uint32_t size, uint32_t n, __resm_rsb_t *res);
extern int32_t      esRESM_RSeek(__resm_rsb_t *res, int32_t Offset, int32_t Whence);

// SIOS
extern int32_t      esSIOS_putchar(char data);
extern uint8_t      esSIOS_getchar(void);
extern void         esSIOS_gets(char *str);
extern void         esSIOS_putarg(uint32_t arg, char format);
extern void         esSIOS_putstr(const char *str);
extern void         esSIOS_setbaud(uint32_t baud);

//SVC
extern int32_t      esSVC_RegCreatePath(const char *path);
extern int32_t      esSVC_RegDeletePath(const char *path);
extern int32_t      esSVC_RegCloseNode(void* handle);
extern int32_t      esSVC_RegDeleteNode(const  char *node);
extern int32_t      esSVC_RegCreateSet(void* handle, const char *set_name);
extern int32_t      esSVC_RegDeleteSet(void* handle, const char *set_name);
extern int32_t      esSVC_RegGetSetCount(void* handle, uint32_t *count_p);
extern int32_t      esSVC_RegGetFirstSet(void* handle);
extern int32_t      esSVC_RegGetNextSet(void* handle, char *set_name);
extern int32_t      esSVC_RegCreateKey(void* hNode, const char *set_name, const char *key_name, const char *key_value);
extern int32_t      esSVC_RegDeleteKey(void* hNode, const char *set_name, const char *key_name);
extern int32_t      esSVC_RegGetKeyCount(void* handle, const char *set_name, uint32_t *count_p);
extern int32_t      esSVC_RegGetFirstKey(void* handle, const char *set_name);
extern int32_t      esSVC_RegGetNextKey(void* hNode, const char *set_name, char *key_name);
extern int32_t      esSVC_RegSetKeyValue(void* hNode, const char *set_name, const char *key_name, const char *key_value);
extern int32_t      esSVC_RegGetKeyValue(void* hNode, const char *set_name, const char *key_name, char *key_value);
extern int32_t      esSVC_RegGetError(void* handle);
extern int32_t      esSVC_RegClrError(void* handle);
extern int32_t      esSVC_ResourceRel(void* hRes);
extern int32_t      esSVC_RegIni2Reg(const char *ini_file);
extern int32_t      esSVC_RegReg2Ini(const char *ini_file);

extern void         esSVC_RegSetRootPath(const char *path);
extern void         esSVC_RegGetRootPath(char *path);

extern void*        esSVC_ResourceReq(uint32_t res, uint32_t mode, uint32_t timeout);
extern void*        esSVC_RegOpenNode(const char *node, int32_t mode);


// TIME/ TIMER/ COUNTER
extern int32_t      esTIME_RequestTimer(__csp_timer_req_type_t *tmrType, __pCBK_t pHdlr, void *pArg, char *pUsr);
extern int32_t      esTIME_ReleaseTimer(int32_t timer_id);
extern int32_t      esTIME_StartTimer(int32_t timer_id);
extern int32_t      esTIME_StopTimer(int32_t timer_id);
extern uint32_t     esTIME_QuerryTimer(int32_t timer_id);

extern int32_t      esTIME_GetTime(__awos_time_t *time);
extern int32_t      esTIME_SetTime(__awos_time_t *time);
extern int32_t      esTIME_GetDate(__awos_date_t *date);
extern int32_t      esTIME_SetDate(__awos_date_t *date);
extern void*        esTIME_RequestAlarm(uint32_t mode);
extern int32_t      esTIME_ReleaseAlarm(void* alarm);
extern int32_t      esTIME_StartAlarm(void* alarm, uint32_t time);
extern int32_t      esTIME_StopAlarm(void* alarm);
extern uint32_t     esTIME_QuerryAlarm(void* alarm);
extern void*        esTIME_RequestCntr(__pCB_ClkCtl_t cb, char *pUsr);
extern int32_t      esTIME_ReleaseCntr(void* hCntr);
extern int32_t      esTIME_StartCntr(void* hCntr);
extern int32_t      esTIME_StopCntr(void* hCntr);
extern int32_t      esTIME_PauseCntr(void* hCntr);
extern int32_t      esTIME_ContiCntr(void* hCntr);
extern int32_t      esTIME_SetCntrValue(void* hCntr, uint32_t value);
extern uint32_t     esTIME_QuerryCntr(void* hCntr);
extern int32_t      esTIME_SetCntrPrescale(void* hCntr, int32_t prescl);
extern int32_t      esTIME_QuerryCntrStat(void* hCntr);

typedef struct
{
    void *SWIHandler_SIOS           ;
    void *SWIHandler_KRNL           ;
    void *SWIHandler_MEMS           ;
    void *SWIHandler_FSYS           ;
    void *SWIHandler_EXEC           ;
    void *SWIHandler_MODS           ;
    void *SWIHandler_RESM           ;
    void *SWIHandler_INT            ;
    void *SWIHandler_DMA            ;
    void *SWIHandler_TIME           ;
    void *SWIHandler_IPIPE          ;
    void *SWIHandler_PWRS           ;
    void *SWIHandler_ERRS           ;
    void *SWIHandler_SVC            ;
    void *SWIHandler_DEV            ;
    void *SWIHandler_KSRV           ;
    void *SWIHandler_PINS           ;
    void *SWIHandler_CLK            ;
    void *SWIHandler_MEM            ;
    void *SWIHandler_HID            ;
    void *SWIHandler_PWRMAN         ;
    void *SWIHandler_CHS            ;
    void *SWIHandler_MSTUB          ;
    void *SWIHandler_INPUT          ;
    void *SWIHandler_CONFIG         ;
    void *SWIHandler_PTHREAD        ;
    void *SWIHandler_NLIBOPS        ;
#ifdef CONFIG_KASAN
    void *SWIHandler_KASANOPS      ;
#endif
} SWIHandler_SWIT_t;

typedef struct
{
    void* esSIOS_getchar            ;
    void* esSIOS_gets               ;
    void* esSIOS_putarg             ;
    void* esSIOS_putstr             ;
    void* esSIOS_setbaud            ;
} SWIHandler_SIOS_t;

typedef struct
{
    void* esKRNL_TCreate                ;
    void* esKRNL_TDel                   ;
    void* esKRNL_TDelReq                ;
    void* esKRNL_GetPrio                ;
    void* esKRNL_FreePrio               ;
    void* esKRNL_TaskChangePrio         ;
    void* esKRNL_TaskNameGet            ;
    void* esKRNL_TaskNameSet            ;
    void* esKRNL_TaskResume             ;
    void* esKRNL_TaskSuspend            ;
    void* esKRNL_TaskStkChk             ;
    void* esKRNL_TaskQuery              ;
    void* esKRNL_TimeDly                ;
    void* esKRNL_TimeDlyHMSM            ;
    void* esKRNL_TimeDlyResume          ;
    void* esKRNL_TimeGet                ;
    void* esKRNL_TimeSet                ;
    void* esKRNL_SemAccept              ;
    void* esKRNL_SemCreate              ;
    void* esKRNL_SemDel                 ;
    void* esKRNL_SemPend                ;
    void* esKRNL_SemPost                ;
    void* esKRNL_SemQuery               ;
    void* esKRNL_SemSet                 ;
    void* esKRNL_MboxAccept             ;
    void* esKRNL_MboxCreate             ;
    void* esKRNL_MboxDel                ;
    void* esKRNL_MboxPend               ;
    void* esKRNL_MboxPost               ;
    void* esKRNL_MboxPostOpt            ;
    void* esKRNL_MboxQuery              ;
    void* esKRNL_MutexAccept            ;
    void* esKRNL_MutexCreate            ;
    void* esKRNL_MutexDel               ;
    void* esKRNL_MutexPend              ;
    void* esKRNL_MutexPost              ;
    void* esKRNL_MutexQuery             ;
    void* esKRNL_QAccept                ;
    void* esKRNL_QCreate                ;
    void* esKRNL_QDel                   ;
    void* esKRNL_QFlush                 ;
    void* esKRNL_QPend                  ;
    void* esKRNL_QPost                  ;
    void* esKRNL_QPostFront             ;
    void* esKRNL_QPostOpt               ;
    void* esKRNL_QQuery                 ;
    void* esKRNL_FlagAccept             ;
    void* esKRNL_FlagCreate             ;
    void* esKRNL_FlagDel                ;
    void* esKRNL_FlagNameGet            ;
    void* esKRNL_FlagNameSet            ;
    void* esKRNL_FlagPend               ;
    void* esKRNL_FlagPendGetFlagsRdy    ;
    void* esKRNL_FlagPost               ;
    void* esKRNL_FlagQuery              ;
    void* esKRNL_TmrCreate              ;
    void* esKRNL_TmrDel                 ;
    void* esKRNL_TmrStart               ;
    void* esKRNL_TmrStop                ;
    void* esKRNL_TmrRemainGet           ;
    void* esKRNL_TmrStateGet            ;
    void* esKRNL_TmrError               ;
    void* esKRNL_Version                ;
    void* esKRNL_SchedLock              ;
    void* esKRNL_SchedUnlock            ;
    void* esKRNL_GetCallBack            ;
    void* esKRNL_CallBack               ;
    void* esKRNL_GetTIDCur              ;
    void* esKRNL_SktCreate              ;
    void* esKRNL_SktDel                 ;
    void* esKRNL_SktPend                ;
    void* esKRNL_SktPost                ;
    void* esKRNL_SktAccept              ;
    void* esKRNL_SktFlush               ;
    void* esKRNL_SktError               ;
    void* esKRNL_Time                   ;
#if (OS_TASK_STAT_EN > 0) && (OS_CPU_HOOKS_EN > 0)
    void* esKRNL_CPUStatStart           ;
    void* esKRNL_CPUStatStop            ;
    void* esKRNL_CPUStatReport          ;
#endif
    void* esKRNL_TaskPrefEn             ;
    void* esKRNL_MemLeakChk             ;
    void* esKRNL_DumpStack              ;
    void* esKRNL_InterruptEnable        ;
    void* esKRNL_InterruptDisable       ;
#ifdef CONFIG_DYNAMIC_LOG_LEVEL_SUPPORT
    void* set_log_level                 ;
    void* get_log_level                 ;
#endif
#ifdef CONFIG_DEBUG_BACKTRACE
    void* backtrace                     ;
#endif
    void* mmap 							;
    void* select 						;
    void* esKRNL_Ioctrl                 ;
} SWIHandler_KRNL_t;

typedef struct
{
    void* esMEMS_Palloc                 ;
    void* esMEMS_Pfree                  ;
    void* esMEMS_VA2PA                  ;
    void* esMEMS_VMCreate               ;
    void* esMEMS_VMDelete               ;
    void* esMEMS_HeapCreate             ;
    void* esMEMS_HeapDel                ;
    void* esMEMS_Malloc                 ;
    void* esMEMS_Mfree                  ;
    void* esMEMS_Realloc                ;
    void* esMEMS_Calloc                 ;
    void* esMEMS_Balloc                 ;
    void* esMEMS_Bfree                  ;
    void* esMEMS_Info                   ;
    void* esMEMS_CleanDCache            ;
    void* esMEMS_CleanFlushDCache       ;
    void* esMEMS_CleanFlushCache        ;
    void* esMEMS_FlushDCache            ;
    void* esMEMS_FlushICache            ;
    void* esMEMS_FlushCache             ;
    void* esMEMS_CleanDCacheRegion      ;
    void* esMEMS_CleanFlushDCacheRegion ;
    void* esMEMS_CleanFlushCacheRegion  ;
    void* esMEMS_FlushDCacheRegion      ;
    void* esMEMS_FlushICacheRegion      ;
    void* esMEMS_FlushCacheRegion       ;
    void* esMEMS_VMalloc                ;
    void* esMEMS_VMfree                 ;
    void* esMEMS_FreeMemSize            ;
    void* esMEMS_TotalMemSize           ;
    void* esMEMS_LockICache             ;
    void* esMEMS_LockDCache             ;
    void* esMEMS_UnlockICache           ;
    void* esMEMS_UnlockDCache           ;
    void* esMEMS_GetIoVAByPA            ;
    void* esMEMS_PhyAddrConti           ;
    void* esMEMS_CleanInvalidateCacheAll;
} SWIHandler_MEMS_t;

typedef struct
{
    void* esFSYS_clearpartupdateflag     ;
    void* esFSYS_closedir                ;
    void* esFSYS_fclose                  ;
    void* esFSYS_fd2file                 ;
    void* esFSYS_ferrclr                 ;
    void* esFSYS_ferror                  ;
    void* esFSYS_file2fd                 ;
    void* esFSYS_fioctrl                 ;
    void* esFSYS_fopen                   ;
    void* esFSYS_format                  ;
    void* esFSYS_fread                   ;
    void* esFSYS_fsdbg                   ;
    void* esFSYS_fseek                   ;
    void* esFSYS_fseekex                 ;
    void* esFSYS_fsreg                   ;
    void* esFSYS_fstat                   ;
    void* esFSYS_fsunreg                 ;
    void* esFSYS_fsync                   ;
    void* esFSYS_ftell                   ;
    void* esFSYS_ftellex                 ;
    void* esFSYS_ftruncate               ;
    void* esFSYS_fwrite                  ;
    void* esFSYS_getfscharset            ;
    void* esFSYS_mkdir                   ;
    void* esFSYS_mntfs                   ;
    void* esFSYS_mntparts                ;
    void* esFSYS_open                    ;
    void* esFSYS_opendir                 ;
    void* esFSYS_partfslck               ;
    void* esFSYS_partfsunlck             ;
    void* esFSYS_pclose                  ;
    void* esFSYS_pdreg                   ;
    void* esFSYS_pdunreg                 ;
    void* esFSYS_perrclr                 ;
    void* esFSYS_perror                  ;
    void* esFSYS_pioctrl                 ;
    void* esFSYS_popen                   ;
    void* esFSYS_pread                   ;
    void* esFSYS_premove                 ;
    void* esFSYS_prename                 ;
    void* esFSYS_pwrite                  ;
    void* esFSYS_querypartupdateflag     ;
    void* esFSYS_readdir                 ;
    void* esFSYS_remove                  ;
    void* esFSYS_rename                  ;
    void* esFSYS_rewinddir               ;
    void* esFSYS_rmdir                   ;
    void* esFSYS_setfs                   ;
    void* esFSYS_statfs                  ;
    void* esFSYS_statpt                  ;
    void* esFSYS_umntparts               ;
    void* esFSYS_umntfs                  ;
    /* main for POSIX interface support */
    /* main for update media file information performance */
} SWIHandler_FSYS_t;

typedef struct
{
    void* esEXEC_PCreate            ;
    void* esEXEC_PDel               ;
    void* esEXEC_PDelReq            ;
    void* esEXEC_Run                ;
} SWIHandler_EXEC_t;


typedef struct
{
    void* esMODS_MInstall           ;
    void* esMODS_MUninstall         ;
    void* esMODS_MOpen              ;
    void* esMODS_MClose             ;
    void* esMODS_MRead              ;
    void* esMODS_MWrite             ;
    void* esMODS_MIoctrl            ;
} SWIHandler_MODS_t;

typedef struct
{
    void* esRESM_ROpen              ;
    void* esRESM_RClose             ;
    void* esRESM_RRead              ;
    void* esRESM_RSeek              ;
} SWIHandler_RESM_t;

typedef struct
{
    void* esINT_InsISR              ;
    void* esINT_UniISR              ;
    void* esINT_InsFIR              ;
    void* esINT_UniFIR              ;
    void* esINT_SetIRQPrio          ;
    void* esINT_DisableINT          ;
    void* esINT_EnableINT           ;
} SWIHandler_INT_t;

typedef struct
{
    void* esDMA_Request             ;
    void* esDMA_Release             ;
    void* esDMA_Setting             ;
    void* esDMA_Start               ;
    void* esDMA_Restart             ;
    void* esDMA_Stop                ;
    void* esDMA_QueryStat           ;
    void* esDMA_QuerySrc            ;
    void* esDMA_QueryDst            ;
    void* esDMA_QueryRestCount      ;
    void* esDMA_ChangeMode          ;
    void* esDMA_EnableINT           ;
    void* esDMA_DisableINT          ;
    void* esDMA_RegDmaHdler         ;
    void* esDMA_UnregDmaHdler       ;
    void* esDMA_Information         ;
} SWIHandler_DMA_t;

typedef struct
{
    void* esTIME_RequestTimer       ;
    void* esTIME_ReleaseTimer       ;
    void* esTIME_StartTimer         ;
    void* esTIME_StopTimer          ;
    void* esTIME_QuerryTimer        ;
    void* esTIME_GetTime            ;
    void* esTIME_SetTime            ;
    void* esTIME_GetDate            ;
    void* esTIME_SetDate            ;
    void* esTIME_RequestAlarm       ;
    void* esTIME_ReleaseAlarm       ;
    void* esTIME_StartAlarm         ;
    void* esTIME_StopAlarm          ;
    void* esTIME_QuerryAlarm        ;
    void* esTIME_RequestCntr        ;
    void* esTIME_ReleaseCntr        ;
    void* esTIME_StartCntr          ;
    void* esTIME_StopCntr           ;
    void* esTIME_PauseCntr          ;
    void* esTIME_ContiCntr          ;
    void* esTIME_SetCntrValue       ;
    void* esTIME_QuerryCntr         ;
    void* esTIME_SetCntrPrescale    ;
    void* esTIME_QuerryCntrStat     ;
} SWIHandler_TIME_t;


typedef struct
{
    void* esIPIPE_Open              ;
    void* esIPIPE_Close             ;
    void* esIPIPE_Request           ;
    void* esIPIPE_Release           ;
    void* esIPIPE_Query             ;
    void* esIPIPE_Reset             ;
    void* esIPIPE_RX                ;
    void* esIPIPE_TX                ;
} SWIHandler_IPIPE_t;

typedef struct
{
    void* esSVC_RegCreatePath       ;
    void* esSVC_RegDeletePath       ;
    void* esSVC_RegOpenNode         ;
    void* esSVC_RegCloseNode        ;
    void* esSVC_RegDeleteNode       ;
    void* esSVC_RegCreateSet        ;
    void* esSVC_RegDeleteSet        ;
    void* esSVC_RegGetSetCount      ;
    void* esSVC_RegGetFirstSet      ;
    void* esSVC_RegGetNextSet       ;
    void* esSVC_RegCreateKey        ;
    void* esSVC_RegDeleteKey        ;
    void* esSVC_RegGetKeyCount      ;
    void* esSVC_RegGetFirstKey      ;
    void* esSVC_RegGetNextKey       ;
    void* esSVC_RegSetKeyValue      ;
    void* esSVC_RegGetKeyValue      ;
    void* esSVC_RegIni2Reg          ;
    void* esSVC_RegReg2Ini          ;
    void* esSVC_RegSetRootPath      ;
    void* esSVC_RegGetRootPath      ;
    void* esSVC_RegGetError         ;
    void* esSVC_RegClrError         ;
    // resource maangement sevices
    void* esSVC_ResourceReq         ;
    void* esSVC_ResourceRel         ;
} SWIHandler_SVC_t;

typedef struct
{
    void* esDEV_Plugin               ;
    void* esDEV_Plugout              ;
    void* esDEV_DevReg               ;
    void* esDEV_DevUnreg             ;
    void* esDEV_Open                 ;
    void* esDEV_Close                ;
    void* esDEV_Read                 ;
    void* esDEV_Write                ;
    void* esDEV_Ioctl                ;
    void* esDEV_Lock                 ;
    void* esDEV_Unlock               ;
    void* esDEV_Insmod               ;
    void* esDEV_Unimod               ;
    void* rt_device_register         ;
    void* rt_device_unregister       ;
} SWIHandler_DEV_t;

typedef struct
{
    void* esKSRV_SysInfo             ;
    void* esKSRV_Get_Display_Hld     ;
    void* esKSRV_Save_Display_Hld    ;
    void* esKSRV_SendMsgEx           ;
    void* esKSRV_GetMsg              ;
    void* esKSRV_GetVersion          ;
    void* esKSRV_Random              ;
    void* esKSRV_Reset               ;
    void* esKSRV_GetSocID            ;
    void* esKSRV_PowerOff            ;
    void* esKSRV_SendMsg             ;
    void* esKSRV_GetTargetPara       ;
    void* esKSRV_EnableWatchDog      ;
    void* esKSRV_DisableWatchDog     ;
    void* esKSRV_ClearWatchDog       ;
    void* esKSRV_EnableWatchDogSrv   ;
    void* esKSRV_DisableWatchDogSrv  ;
    void* esKSRV_memcpy              ;
    void* esKSRV_GetLowMsg           ;
    void* esKSRV_GetHighMsg          ;
    void* esKSRV_GetPara             ;
    void* esKSRV_GetDramCfgPara      ;
    void* esKSRV_memset              ;
    void* esKSRV_GetAddPara          ;
    void* esKSRV_Save_Mixture_Hld    ;
    void* esKSRV_Get_Mixture_Hld     ;
} SWIHandler_KSRV_t;

typedef struct
{
    void* esPINS_PinGrpReq              ;
    void* esPINS_PinGrpRel              ;
    void* esPINS_GetPinGrpStat          ;
    void* esPINS_GetPinStat             ;
    void* esPINS_SetPinStat             ;
    void* esPINS_SetPinIO               ;
    void* esPINS_SetPinPull             ;
    void* esPINS_SetPinDrive            ;
    void* esPINS_ReadPinData            ;
    void* esPINS_WritePinData           ;
    void* esPINS_EnbaleInt              ;
    void* esPINS_DisbaleInt             ;
    void* esPINS_QueryInt               ;
    void* esPINS_SetIntMode             ;
    void* esPINS_RegIntHdler            ;
    void* esPINS_UnregIntHdler          ;
    void* esPINS_ClearPending           ;
} SWIHandler_PINS_t;

typedef struct
{
    void* esCLK_SetSrcFreq              ;
    void* esCLK_GetSrcFreq              ;
    void* esCLK_OpenMclk                ;
    void* esCLK_CloseMclk               ;
    void* esCLK_MclkRegCb               ;
    void* esCLK_MclkUnregCb             ;
    void* esCLK_SetMclkSrc              ;
    void* esCLK_GetMclkSrc              ;
    void* esCLK_SetMclkDiv              ;
    void* esCLK_GetMclkDiv              ;
    void* esCLK_MclkOnOff               ;
    void* esCLK_MclkAssert              ;
    void* esCLK_MclkDeassert            ;
    void* esCLK_MclkReset               ;
    void* esCLK_MclkGetRstStatus        ;
    void* esCLK_SysInfo                 ;
    void* esCLK_ModInfo                 ;
    void* esCLK_GetRound_Rate           ;
} SWIHandler_CLK_t;

typedef struct
{
    void* esMEM_DramWakeup              ;
    void* esMEM_DramSuspend             ;
    void* esMEM_RegDramAccess           ;
    void* esMEM_UnRegDramAccess         ;
    void* esMEM_MasterSet               ;
    void* esMEM_MasterGet               ;
    void* esMEM_RequestDramUsrMode      ;
    void* esMEM_ReleaseDramUsrMode      ;
    void* esMEM_BWEnable                ;
    void* esMEM_BWDisable               ;
    void* esMEM_BWGet                   ;
    void* esMEM_SramReqBlk              ;
    void* esMEM_SramRelBlk              ;
    void* esMEM_SramSwitchBlk           ;
} SWIHandler_MEM_t;

typedef struct
{
    void* esHID_SendMsg                 ;
    void* esHID_hiddevreg               ;
    void* esHID_hiddevunreg             ;
} SWIHandler_HID_t;

typedef struct
{
    void* esPWRMAN_ReqPwrmanMode        ;
    void* esPWRMAN_RelPwrmanMode        ;
    void* esPWRMAN_EnterStandby         ;
    void* esPWRMAN_UsrEventNotify       ;
    void* esPWRMAN_LockCpuFreq          ;
    void* esPWRMAN_UnlockCpuFreq        ;
    void* esPWRMAN_RegDevice            ;
    void* esPWRMAN_UnregDevice          ;
    void* esPWRMAN_GetStandbyPara       ;
    void* esPWRMAN_SetStandbyMode       ;
} SWIHandler_PWRMAN_t;


typedef struct
{
    void* esCHS_Uni2Char                ;
    void* esCHS_Char2Uni                ;
    void* esCHS_GetChUpperTbl           ;
    void* esCHS_GetChLowerTbl           ;
} SWIHandler_CHS_t;

typedef struct
{
    void* esMSTUB_RegFuncTbl            ;
    void* esMSTUB_UnRegFuncTbl          ;
    void* esMSTUB_GetFuncEntry          ;
} SWIHandler_MSTUB_t;

typedef struct
{
    void* esCFG_GetKeyValue             ;
    void* esCFG_GetSecKeyCount          ;
    void* esCFG_GetSecCount             ;
    void* esCFG_GetGPIOSecKeyCount      ;
    void* esCFG_GetGPIOSecData          ;
    void* esCFG_Init_Ex                 ;
    void* esCFG_Exit_Ex                 ;
    void* esCFG_GetKeyValue_Ex          ;
    void* esCFG_GetSecKeyCount_Ex       ;
    void* esCFG_GetSecCount_Ex          ;
    void* esCFG_GetGPIOSecKeyCount_Ex   ;
    void* esCFG_GetGPIOSecData_Ex       ;
} SWIHandler_CONFIG_t;

typedef struct
{
    void* esINPUT_RegDev                ;
    void* esINPUT_UnregDev              ;
    void* esINPUT_LdevGrab              ;
    void* esINPUT_LdevRelease           ;
    void* esINPUT_SendEvent             ;
    void* esINPUT_LdevFeedback          ;
    void* esINPUT_GetLdevID             ;
    void* esINPUT_LdevCtl               ;
} SWIHandler_INPUT_t;

typedef struct
{
    void* pthread_attr_destroy          ;
    void* pthread_attr_init             ;
    void* pthread_attr_setdetachstate   ;
    void* pthread_attr_getdetachstate   ;
    void* pthread_attr_setschedpolicy   ;
    void* pthread_attr_getschedpolicy   ;
    void* pthread_attr_setschedparam    ;
    void* pthread_attr_getschedparam    ;
    void* pthread_attr_setstacksize     ;
    void* pthread_attr_getstacksize     ;
    void* pthread_attr_setstackaddr     ;
    void* pthread_attr_getstackaddr     ;
    void* pthread_attr_setstack         ;
    void* pthread_attr_getstack         ;
    void* pthread_attr_setguardsize     ;
    void* pthread_attr_getguardsize     ;
    void* pthread_attr_setscope         ;
    void* pthread_attr_getscope         ;
    void* pthread_system_init           ;
    void* pthread_create                ;
    void* pthread_detach                ;
    void* pthread_join                  ;
    void* pthread_exit                  ;
    void* pthread_once                  ;
    void* pthread_cleanup_pop           ;
    void* pthread_cleanup_push          ;
    void* pthread_cancel                ;
    void* pthread_testcancel            ;
    void* pthread_setcancelstate        ;
    void* pthread_setcanceltype         ;
    void* pthread_atfork                ;
    void* pthread_kill                  ;
    void* pthread_self                  ;
    void* pthread_mutex_init            ;
    void* pthread_mutex_destroy         ;
    void* pthread_mutex_lock            ;
    void* pthread_mutex_unlock          ;
    void* pthread_mutex_trylock         ;
    void* pthread_mutexattr_init        ;
    void* pthread_mutexattr_destroy     ;
    void* pthread_mutexattr_gettype     ;
    void* pthread_mutexattr_settype     ;
    void* pthread_mutexattr_setpshared  ;
    void* pthread_mutexattr_getpshared  ;
    void* pthread_condattr_destroy      ;
    void* pthread_condattr_init         ;
    void* pthread_condattr_getclock     ;
    void* pthread_condattr_setclock     ;
    void* pthread_cond_init             ;
    void* pthread_cond_destroy          ;
    void* pthread_cond_broadcast        ;
    void* pthread_cond_signal           ;
    void* pthread_cond_wait             ;
    void* pthread_cond_timedwait        ;
    void* pthread_rwlockattr_init       ;
    void* pthread_rwlockattr_destroy    ;
    void* pthread_rwlockattr_getpshared ;
    void* pthread_rwlockattr_setpshared ;
    void* pthread_rwlock_init           ;
    void* pthread_rwlock_destroy        ;
    void* pthread_rwlock_rdlock         ;
    void* pthread_rwlock_tryrdlock      ;
    void* pthread_rwlock_timedrdlock    ;
    void* pthread_rwlock_timedwrlock    ;
    void* pthread_rwlock_unlock         ;
    void* pthread_rwlock_wrlock         ;
    void* pthread_rwlock_trywrlock      ;
    void* pthread_spin_init             ;
    void* pthread_spin_destroy          ;
    void* pthread_spin_lock             ;
    void* pthread_spin_trylock          ;
    void* pthread_spin_unlock           ;
    void* pthread_barrierattr_destroy   ;
    void* pthread_barrierattr_init      ;
    void* pthread_barrierattr_getpshared;
    void* pthread_barrierattr_setpshared;
    void* pthread_barrier_destroy       ;
    void* pthread_barrier_init          ;
    void* pthread_barrier_wait          ;
    void* pthread_getspecific           ;
    void* pthread_setspecific           ;
    void* pthread_key_create            ;
    void* pthread_key_delete            ;
    void* sem_close                     ;
    void* sem_destroy                   ;
    void* sem_getvalue                  ;
    void* sem_init                      ;
    void* sem_open                      ;
    void* sem_post                      ;
    void* sem_timedwait                 ;
    void* sem_trywait                   ;
    void* sem_unlink                    ;
    void* sem_wait                      ;
    void* clock_settime                 ;
    void* clock_gettime                 ;
    void* clock_getres                  ;
    void* usleep                        ;
    void* msleep                        ;
    void* sleep                         ;
} SWIHandler_PTHREAD_t;

typedef struct
{
    void* _close_r                      ;
    void* _open_r                       ;
    void* _read_r                       ;
    void* _write_r                      ;
    void* _lseek_r                      ;
    void* _rename_r                     ;
    void* _stat_r                       ;
    void* _mkdir_r                      ;
    void* _unlink_r                     ;
    void* _gettimeofday_r               ;
    void* settimeofday                  ;
    void* _malloc_r                     ;
    void* _realloc_r                    ;
    void* _calloc_r                     ;
    void* _free_r                       ;
    void* _isatty_r                     ;
    void* _isatty                       ;
    void* __libc_init_array             ;
    void* _exit                         ;
    void* _system                       ;

    void* opendir                       ;
    void* readdir                       ;
    void* rewinddir                     ;
    void* rmdir                         ;
    void* closedir                      ;
    void* mkdir                         ;
    void* ioctl                         ;
    void* remove                        ;

    void* _fstat_r                      ;

    void* fd2file                       ;
    void* ferrclr                       ;
    void* ferror                        ;
    void* file2fd                       ;
    void* fioctrl                       ;
    void* format                        ;
    void* fsdbg                         ;
    void* fseekex                       ;
    void* fsync                         ;
    void* ftellex                       ;
    void* ftruncate                     ;
    void* getfscharset                  ;
    void* setfs                         ;
    void* statfs                        ;
} SWIHandler_NLIBOPS_t;

typedef struct
{
    void* __asan_load1                  ;
    void* __asan_load2                  ;
    void* __asan_load4                  ;
    void* __asan_load8                  ;
    void* __asan_load16                 ;
    void* __asan_loadN                  ;
    void* __asan_store1                 ;
    void* __asan_store2                 ;
    void* __asan_store4                 ;
    void* __asan_store8                 ;
    void* __asan_store16                ;
    void* __asan_storeN                 ;
    void* __asan_load1_noabort          ;
    void* __asan_load2_noabort          ;
    void* __asan_load4_noabort          ;
    void* __asan_load8_noabort          ;
    void* __asan_load16_noabort         ;
    void* __asan_loadN_noabort          ;
    void* __asan_store1_noabort         ;
    void* __asan_store2_noabort         ;
    void* __asan_store4_noabort         ;
    void* __asan_store8_noabort         ;
    void* __asan_store16_noabort        ;
    void* __asan_storeN_noabort         ;
    void* __asan_poison_stack_memory    ;
    void* __asan_unpoison_stack_memory  ;
    void* __asan_alloca_poison          ;
    void* __asan_alloca_unpoison        ;
    void* __asan_handle_no_return       ;
    void* __asan_register_globals       ;
    void* __asan_unregister_globals     ;
}SWIHandler_KASANOPS_t;

#ifndef __LP64__
#define SWINO(base,type,isr)    (base+(((long)(&(((type*)0)->isr)))>> 2))
#else
#define SWINO(base,type,isr)    (base+(((long)(&(((type*)0)->isr)))>> 3))
#endif

#define SYSCALL_SWINO(x) (SWINO(0, SWIHandler_SWIT_t, x) << 8)

#define SWINO_SIOS      SYSCALL_SWINO(SWIHandler_SIOS)  /* ??׼IOģ??ϵͳ           */
#define SWINO_KRNL      SYSCALL_SWINO(SWIHandler_KRNL)  /* ?ں?ϵͳ                 */
#define SWINO_MEMS      SYSCALL_SWINO(SWIHandler_MEMS)  /* ?ڴ?????ϵͳ             */
#define SWINO_FSYS      SYSCALL_SWINO(SWIHandler_FSYS)  /* ?ļ?ϵͳ                 */
#define SWINO_EXEC      SYSCALL_SWINO(SWIHandler_EXEC)  /* ???̹???ϵͳ             */
#define SWINO_MODS      SYSCALL_SWINO(SWIHandler_MODS)  /* ģ??????ϵͳ             */
#define SWINO_RESM      SYSCALL_SWINO(SWIHandler_RESM)  /* ??Դ????ϵͳ             */
#define SWINO_INT       SYSCALL_SWINO(SWIHandler_INT)   /* ?жϿ???ϵͳ             */
#define SWINO_DMA       SYSCALL_SWINO(SWIHandler_DMA )  /* DMA                      */
#define SWINO_TIME      SYSCALL_SWINO(SWIHandler_TIME)  /* ʱ??????ϵͳ             */
#define SWINO_IPIPE     SYSCALL_SWINO(SWIHandler_IPIPE) /*                          */
#define SWINO_PWRS      SYSCALL_SWINO(SWIHandler_PWRS)  /* ???Ĺ???ϵͳ             */
#define SWINO_ERRS      SYSCALL_SWINO(SWIHandler_ERRS)  /* ???�??ʹ???ϵͳ       */
#define SWINO_SVC       SYSCALL_SWINO(SWIHandler_SVC)   /*                          */
#define SWINO_DEV       SYSCALL_SWINO(SWIHandler_DEV)   /* ?豸????ϵͳ             */
#define SWINO_KSRV      SYSCALL_SWINO(SWIHandler_KSRV)  /* ?ں˷???                 */
#define SWINO_PINS      SYSCALL_SWINO(SWIHandler_PINS)  /* pin????                  */
#define SWINO_CLK       SYSCALL_SWINO(SWIHandler_CLK)   /* ʱ?ӷ???                 */
#define SWINO_MEM       SYSCALL_SWINO(SWIHandler_MEM)   /* ?ڴ??豸????             */
#define SWINO_HID       SYSCALL_SWINO(SWIHandler_HID)   /* hid??ϵͳ                */
#define SWINO_PWRMAN    SYSCALL_SWINO(SWIHandler_PWRMAN)/* pwm??ϵͳ                */
#define SWINO_CHS       SYSCALL_SWINO(SWIHandler_CHS)   /* charset ??ϵͳ           */
#define SWINO_MSTUB     SYSCALL_SWINO(SWIHandler_MSTUB) /* module stub ??ϵͳ       */
#define SWINO_INPUT     SYSCALL_SWINO(SWIHandler_INPUT) /* ??????ϵͳ               */
#define SWINO_CFG       SYSCALL_SWINO(SWIHandler_CONFIG)/* ??????ϵͳ               */
#define SWINO_PTD       SYSCALL_SWINO(SWIHandler_PTHREAD) /*PTHREADϵͳ*/
#define SWINO_NLIBOPS  SYSCALL_SWINO(SWIHandler_NLIBOPS)
#define SWINO_KASANOPS  SYSCALL_SWINO(SWIHandler_KASANOPS)

//#define SYSCALL_KRNL(y) SWINO(SWINO_KRNL, SWIHandler_KRNL_t, y)
#if !defined(CONFIG_KERNEL_USE_SBI)
#define SYSCALL_SIOS(y)     SWINO(SWINO_SIOS,       SWIHandler_SIOS_t, y)
#define SYSCALL_KRNL(y)     SWINO(SWINO_KRNL,       SWIHandler_KRNL_t, y)
#define SYSCALL_MEMS(y)     SWINO(SWINO_MEMS,       SWIHandler_MEMS_t, y)
#define SYSCALL_FSYS(y)     SWINO(SWINO_FSYS, 		SWIHandler_FSYS_t, y)
#define SYSCALL_EXEC(y)     SWINO(SWINO_EXEC,       SWIHandler_EXEC_t, y)
#define SYSCALL_MODS(y)     SWINO(SWINO_MODS,       SWIHandler_MODS_t, y)
#define SYSCALL_RESM(y)     SWINO(SWINO_RESM,       SWIHandler_RESM_t, y)
#define SYSCALL_INT(y)      SWINO(SWINO_INT,        SWIHandler_INT_t, y)
#define SYSCALL_DMA(y)      SWINO(SWINO_DMA,        SWIHandler_DMA_t, y)
#define SYSCALL_TIME(y)     SWINO(SWINO_TIME,       SWIHandler_TIME_t, y)
#define SYSCALL_IPIPE(y)    SWINO(SWINO_IPIPE,      SWIHandler_IPIPE_t, y)
#define SYSCALL_SVC(y)      SWINO(SWINO_SVC,        SWIHandler_SVC_t, y)
#define SYSCALL_DEV(y)      SWINO(SWINO_DEV,        SWIHandler_DEV_t, y)
#define SYSCALL_KSRV(y)     SWINO(SWINO_KSRV,       SWIHandler_KSRV_t, y)
#define SYSCALL_PINS(y)     SWINO(SWINO_PINS,       SWIHandler_PINS_t, y)
#define SYSCALL_CLK(y)      SWINO(SWINO_CLK,        SWIHandler_CLK_t, y)
#define SYSCALL_MEM(y)      SWINO(SWINO_MEM,        SWIHandler_MEM_t, y)
#define SYSCALL_HID(y)      SWINO(SWINO_HID,        SWIHandler_HID_t, y)
#define SYSCALL_PWRMAN(y)   SWINO(SWINO_PWRMAN,     SWIHandler_PWRMAN_t, y)
#define SYSCALL_CHS(y)      SWINO(SWINO_CHS,        SWIHandler_CHS_t, y)
#define SYSCALL_MSTUB(y)    SWINO(SWINO_MSTUB,      SWIHandler_MSTUB_t, y)
#define SYSCALL_INPUT(y)    SWINO(SWINO_INPUT,      SWIHandler_INPUT_t, y)
#define SYSCALL_CFG(y)      SWINO(SWINO_CFG,        SWIHandler_CONFIG_t, y)
#define SYSCALL_PTD(y)      SWINO(SWINO_PTD,        SWIHandler_PTHREAD_t, y)
#define SYSCALL_NLIBOPS(y)  SWINO(SWINO_NLIBOPS,   SWIHandler_NLIBOPS_t, y)
#define SYSCALL_KASANOPS(y) SWINO(SWINO_KASANOPS,   SWIHandler_KASANOPS_t, y)
#else
/*if using sbi module, the syscall number need to be different from sbi function number*/
#define SYSCALL_SIOS(y)     (0x10000000 | SWINO(SWINO_SIOS,     SWIHandler_SIOS_t, y))
#define SYSCALL_KRNL(y)     (0x10000000 | SWINO(SWINO_KRNL,     SWIHandler_KRNL_t, y))
#define SYSCALL_MEMS(y)     (0x10000000 | SWINO(SWINO_MEMS,     SWIHandler_MEMS_t, y))
#define SYSCALL_FSYS(y)     (0x10000000 | SWINO(SWINO_FSYS,    	SWIHandler_FSYS_t, y))
#define SYSCALL_EXEC(y)     (0x10000000 | SWINO(SWINO_EXEC,     SWIHandler_EXEC_t, y))
#define SYSCALL_MODS(y)     (0x10000000 | SWINO(SWINO_MODS,     SWIHandler_MODS_t, y))
#define SYSCALL_RESM(y)     (0x10000000 | SWINO(SWINO_RESM,     SWIHandler_RESM_t, y))
#define SYSCALL_INT(y)      (0x10000000 | SWINO(SWINO_INT,      SWIHandler_INT_t, y))
#define SYSCALL_DMA(y)      (0x10000000 | SWINO(SWINO_DMA,      SWIHandler_DMA_t, y))
#define SYSCALL_TIME(y)     (0x10000000 | SWINO(SWINO_TIME,     SWIHandler_TIME_t, y))
#define SYSCALL_IPIPE(y)    (0x10000000 | SWINO(SWINO_IPIPE,    SWIHandler_IPIPE_t, y))
#define SYSCALL_SVC(y)      (0x10000000 | SWINO(SWINO_SVC,      SWIHandler_SVC_t, y))
#define SYSCALL_DEV(y)      (0x10000000 | SWINO(SWINO_DEV,      SWIHandler_DEV_t, y))
#define SYSCALL_KSRV(y)     (0x10000000 | SWINO(SWINO_KSRV,     SWIHandler_KSRV_t, y))
#define SYSCALL_PINS(y)     (0x10000000 | SWINO(SWINO_PINS,     SWIHandler_PINS_t, y))
#define SYSCALL_CLK(y)      (0x10000000 | SWINO(SWINO_CLK,      SWIHandler_CLK_t, y))
#define SYSCALL_MEM(y)      (0x10000000 | SWINO(SWINO_MEM,      SWIHandler_MEM_t, y))
#define SYSCALL_HID(y)      (0x10000000 | SWINO(SWINO_HID,      SWIHandler_HID_t, y))
#define SYSCALL_PWRMAN(y)   (0x10000000 | SWINO(SWINO_PWRMAN,   SWIHandler_PWRMAN_t, y))
#define SYSCALL_CHS(y)      (0x10000000 | SWINO(SWINO_CHS,      SWIHandler_CHS_t, y))
#define SYSCALL_MSTUB(y)    (0x10000000 | SWINO(SWINO_MSTUB,    SWIHandler_MSTUB_t, y))
#define SYSCALL_INPUT(y)    (0x10000000 | SWINO(SWINO_INPUT,    SWIHandler_INPUT_t, y))
#define SYSCALL_CFG(y)      (0x10000000 | SWINO(SWINO_CFG,      SWIHandler_CONFIG_t, y))
#define SYSCALL_PTD(y)      (0x10000000 | SWINO(SWINO_PTD,      SWIHandler_PTHREAD_t, y))
#define SYSCALL_NLIBOPS(y)  (0x10000000 | SWINO(SWINO_NLIBOPS,  SWIHandler_NLIBOPS_t, y))
#define SYSCALL_KASANOPS(y) (0x10000000 | SWINO(SWINO_KASANOPS, SWIHandler_KASANOPS_t, y))
#endif
#endif
