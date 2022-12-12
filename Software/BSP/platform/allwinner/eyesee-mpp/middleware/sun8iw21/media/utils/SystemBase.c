/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : SystemBase.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/15
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SystemBase"
#include <utils/plat_log.h>

//ref platform headers
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>

//media api headers to app
#include "SystemBase.h"


//media internal common headers.


/**
 * extend pthread functions. add timeout. 
 * @param msecs timeout threshold, unit: ms.
 */
int pthread_cond_wait_timeout(pthread_cond_t* const condition, pthread_mutex_t* const mutex, unsigned int msecs)
{
    int ret;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int relative_sec = msecs/1000;
    int relative_nsec = (msecs%1000)*1000000;
    ts.tv_sec += relative_sec;
    ts.tv_nsec += relative_nsec;
    ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
    ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);
    ret = pthread_cond_timedwait(condition, mutex, &ts);
    if(ETIMEDOUT == ret)
    {
        //alogd("pthread cond timeout np timeout[%d]", ret);
    }
    else if(0 == ret)
    {
    }
    else
    {
        //aloge("fatal error! pthread cond timedwait[%d]", ret);
    }
    return ret;
}

int64_t CDX_GetSysTimeUsMonotonic()
{
    long long curr;
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
    return (int64_t)curr;
}

int CDX_SetTimeUs(int64_t timeUs)
{
    struct timeval tv;
    tv.tv_sec = timeUs / 1000000;
    tv.tv_usec = timeUs % 1000000;

    if(settimeofday(&tv, NULL) < 0) {
        return -1;
    }
    return 0;
}

int64_t CDX_GetTimeUs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_usec + tv.tv_sec * 1000000ll;
}
#if 0
void dumpCallStack(const char* pTag)
{
    void *buffer[30] = {0};
    size_t size;
    char **strings = NULL;
    size_t i = 0;

    size = backtrace(buffer, 30);
    alogd("tag[%s]: Obtained %zd stack frames.nm", pTag, size);
    strings = backtrace_symbols(buffer, size);
    if (strings == NULL)
    {
        alogd("backtrace_symbols.");
        //exit(EXIT_FAILURE);
    }

    for (i = 0; i < size; i++)
    {
        alogd("%s", strings[i]);
    }
    free(strings);
    strings = NULL;
}
#endif
