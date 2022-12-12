/*
 * Copyright (C) 2008-2015 Allwinner Technology Co. Ltd.
 * Author: Ning Fang <fangning@allwinnertech.com>
 *         Caoyuan Yang <yangcaoyuan@allwinnertech.com>
 *
 * This software is confidential and proprietary and may be used
 * only as expressly authorized by a licensing agreement from
 * Softwinner Products.
 *
 * The entire notice above must be reproduced on all copies
 * and should not be removed.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef _ENC_ADAPTER_H
#define _ENC_ADAPTER_H

//#include "memoryAdapter.h"
//#include "secureMemoryAdapter.h"
#include <cdc_config.h>
#include <sc_interface.h>

int  EncAdapterInitializeVe(void);

int   EncAdapterInitializeMem(struct ScMemOpsS *memops);

void  EncAdpaterRelease(struct ScMemOpsS *memops);

int   EncAdapterLockVideoEngine(void);

void  EncAdapterUnLockVideoEngine(void);

void  EncAdapterVeReset(void);

int   EncAdapterVeWaitInterrupt(void);

void* EncAdapterVeGetBaseAddress(void);

int   EncAdapterMemGetDramType(void);

void EncAdapterEnableEncoder(void);

void EncAdapterDisableEncoder(void);

void EncAdapterResetEncoder(void);

void EncAdapterInitPerformance(int nMode);

void EncAdapterUninitPerformance(int nMode);

unsigned int EncAdapterGetICVersion(void* pTopBaseAddr);

void EncAdapterSetDramType(void);

void EncAdapterPrintTopVEReg(void* pTopBaseAddr, const char* name);

void EncAdapterPrintEncReg(void* pTopBaseAddr, const char* name);

void EncAdapterPrintIspReg(void* pTopBaseAddr, const char* name);

unsigned int EncAdapterGetVeAddrOffset(void);

int __EncAdapterMemOpen(struct ScMemOpsS *memops);

void __EncAdapterMemClose(struct ScMemOpsS *memops);

void* __EncAdapterMemPalloc(struct ScMemOpsS *memops, int nSize, void *veOps, void *pVeopsSelf);

void* __EncAdapterMemNoCachePalloc(struct ScMemOpsS *memops, int nSize, void *veOps, void *pVeopsSelf);

void __EncAdapterMemPfree(struct ScMemOpsS *memops, void* pMem, void *veOps, void *pVeopsSelf);

void __EncAdapterMemFlushCache(struct ScMemOpsS *memops, void* pMem, int nSize);

void* __EncAdapterMemGetPhysicAddress(struct ScMemOpsS *memops, void* pVirtualAddress);

void* __EncAdapterMemGetPhysicAddressCpu(struct ScMemOpsS *memops, void* pVirtualAddress);

void* __EncAdapterMemGetVirtualAddress(struct ScMemOpsS *memops, void* pPhysicAddress);

unsigned int __EncAdapterMemGetVeAddrOffset(struct ScMemOpsS *memops);

#define EncMemAdapterOpen() __EncAdapterMemOpen(_memops)

#define EncMemAdapterClose() __EncAdapterMemClose(_memops)

#define EncAdapterMemPalloc(nSize) __EncAdapterMemPalloc(_memops, nSize, veOps, pVeopsSelf)

#define EncAdapterNoCacheMemPalloc(nSize) __EncAdapterMemNoCachePalloc(_memops, nSize, veOps, pVeopsSelf)
#if 0//debug used
#define EncAdapterMemPfree(pMem) do{\
       logi("call pfree");\
       __EncAdapterMemPfree(_memops, pMem, veOps, pVeopsSelf);\
   }while(0)
#else   
#define EncAdapterMemPfree(pMem) __EncAdapterMemPfree(_memops, pMem, veOps, pVeopsSelf)
#endif

#define EncAdapterMemFlushCache(pMem, nSize) __EncAdapterMemFlushCache(_memops, pMem, nSize)

#define  EncAdapterGetVeAddrOffset() __EncAdapterMemGetVeAddrOffset(_memops)

#define EncAdapterMemGetPhysicAddress(pVirtualAddress) \
   __EncAdapterMemGetPhysicAddress(_memops, pVirtualAddress)

#define EncAdapterMemGetPhysicAddressCpu(pVirtualAddress) \
   __EncAdapterMemGetPhysicAddressCpu(_memops, pVirtualAddress)

#define EncAdapterMemGetVirtualAddress(pPhysicAddress) \
   __EncAdapterMemGetVirtualAddress(_memops, pPhysicAddress)

#endif //_ENC_ADAPTER_H

#ifdef __cplusplus
}
#endif /* __cplusplus */
