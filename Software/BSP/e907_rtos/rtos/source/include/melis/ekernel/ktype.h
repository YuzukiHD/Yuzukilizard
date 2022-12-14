/*
 * =====================================================================================
 *
 *       Filename:  ktype.h
 *
 *    Description:  melis type define
 *
 *        Version:  1.0
 *        Created:  2017???10???17??? 14???22???10???
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef MELIS_TYPE_H
#define MELIS_TYPE_H
#include <typedef.h>

typedef __u32 __krnl_flags_t;
typedef void *__krnl_event_t;

typedef struct __RESM_RSB
{
    __hdle      hResfile;
    __u32       offset;
    __u32       size;
} __resm_rsb_t;

/* TASK CONTROL BLOCK */
typedef struct __KRNL_TCB
{
    __u32       OSTCBPrio;                  /* Task priority (0 == highest)                             */
} __krnl_tcb_t;

typedef struct os_sem_data
{
    __u16       OSCnt;                              /* Semaphore count                                          */
} OS_SEM_DATA;

typedef __s32(*__pCB_ClkCtl_t)(__u32, __s32);
typedef __s32(*__pCB_DPMCtl_t)(__u32, void *);
typedef __s32(*__pCBK_t)(void *);
typedef __bool(*__pISR_t)(void *);
//typedef unsigned long(* __pSWI_t)(__u32, __u32, __u32, __u32);

typedef unsigned long (* __pSWI_t)(unsigned long, unsigned long, unsigned long, unsigned long);
typedef void (* __pTHD_t)(void * /*p_arg*/);      /* thread prototype   */
#endif  /*MELIS_TYPE_H*/
