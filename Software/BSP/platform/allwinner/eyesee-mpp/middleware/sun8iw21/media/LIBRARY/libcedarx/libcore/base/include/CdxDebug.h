/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxDebug.h
 * Description : Debug
 * History :
 *
 */

#ifndef CDX_DEBUG_H
#define CDX_DEBUG_H
#include <pthread.h>
#include <sys/syscall.h>

#define gettid() syscall(__NR_gettid)

#ifdef __cplusplus
extern "C"
{
#endif

void CdxDumpThreadStack(pthread_t tid);

void CdxCallStack(void);

#ifdef __cplusplus
}
#endif

#endif
