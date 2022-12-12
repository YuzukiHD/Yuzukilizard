/******************************************************************************
     Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
******************************************************************************/

#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include <stdio.h>
#include <string.h>

#define MALLOC(var, type) \
    (var) = (type *)soap_malloc(_soap, sizeof(type));
#define MALLOCN(var, type, n) \
    (var) = (type *)soap_malloc(_soap, sizeof(type) * (n));
#define local(type, var) \
        type var; \
        memset(&(var), 0, sizeof(type));

#define StripFileName(s) (strrchr(s, '/')?(strrchr(s, '/')+1):s)

#ifndef USE_LOG_LIB_GLOG

#define DBG(fmt, args...) \
    fprintf(stderr, "onvif> %*s:%*d | %*s | " fmt "\n", 35, StripFileName(__FILE__), -4, __LINE__, -30, __FUNCTION__, ##args)

#else

#include "log/log.h"
#define DBG(fmt, args...) \
		GLOG_PRINT(_GLOG_INFO, fmt, ##args)
#endif

#define LOG(fmt, args...) DBG(fmt, ##args)

#define INFO_LENGTH 128
#define LARGE_INFO_LENGTH 1024
#define SMALL_INFO_LENGTH 512

#define BITRATE_MEASURE 1024
#define TIMESTAMP_YEAR_BASE 1900
#define TIMESTAMP_MONTH_BASE 1
#define TIMESTAMP_DAY_BASE 0

#define MACH_ADDR_LENGTH 6

#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/prctl.h>

inline pid_t gettid()
{
    return syscall(SYS_gettid);
}

#endif /* SRC_COMMON_H_ */
