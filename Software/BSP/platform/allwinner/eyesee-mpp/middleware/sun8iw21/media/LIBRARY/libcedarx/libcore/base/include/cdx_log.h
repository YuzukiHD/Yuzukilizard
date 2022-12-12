/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : cdx_log.h
 * Description : Log
 * History :
 *
 */

#ifndef CDX_LOG_H
#define CDX_LOG_H

#ifndef LOG_TAG
#define LOG_TAG "awplayer"
#endif

enum CDX_LOG_LEVEL_TYPE {
    LOG_LEVEL_VERBOSE = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_INFO = 4,
    LOG_LEVEL_WARNING = 5,
    LOG_LEVEL_ERROR = 6,
};

extern enum CDX_LOG_LEVEL_TYPE GLOBAL_LOG_LEVEL;

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL    (0xFFFF)
#endif

#ifdef __ANDROID__
#include <cutils/log.h>

#define CDX_LOG_ORDER \
    ((unsigned)LOG_LEVEL_ERROR   ==  (unsigned)ANDROID_LOG_ERROR) && \
    ((unsigned)LOG_LEVEL_WARNING ==  (unsigned)ANDROID_LOG_WARN) && \
    ((unsigned)LOG_LEVEL_INFO    ==  (unsigned)ANDROID_LOG_INFO) && \
    ((unsigned)LOG_LEVEL_DEBUG   ==  (unsigned)ANDROID_LOG_DEBUG) && \
    ((unsigned)LOG_LEVEL_VERBOSE ==  (unsigned)ANDROID_LOG_VERBOSE)

typedef char CHECK_LOG_LEVEL_EQUAL_TO_ANDROID[CDX_LOG_ORDER > 0 ? 1 : -1];

#define AWLOG(level, fmt, arg...)  \
    do { \
        if (level >= GLOBAL_LOG_LEVEL || level >= CONFIG_LOG_LEVEL) \
            LOG_PRI(level, LOG_TAG, "<%s:%u>: " fmt, __FUNCTION__, __LINE__, ##arg); \
    } while (0)

#define CDX_TRACE() \
    CDX_LOGI("<%s:%u> tid(%d)", __FUNCTION__, __LINE__, gettid())

/*check when realease version*/
#define CDX_FORCE_CHECK(e) \
        LOG_ALWAYS_FATAL_IF(                        \
                !(e),                               \
                "<%s:%d>CDX_CHECK(%s) failed.",     \
                __FUNCTION__, __LINE__, #e)

#define CDX_TRESPASS() \
        LOG_ALWAYS_FATAL("Should not be here.")

#define CDX_LOG_FATAL(fmt, arg...)                          \
        LOG_ALWAYS_FATAL("<%s:%d>" fmt,                      \
            __FUNCTION__, __LINE__, ##arg)

#define CDX_LOG_CHECK(e, fmt, arg...)                           \
    LOG_ALWAYS_FATAL_IF(                                        \
            !(e),                                               \
            "<%s:%d>check (%s) failed:" fmt,                    \
            __FUNCTION__, __LINE__, #e, ##arg)

#ifdef AWP_DEBUG
#define CDX_CHECK(e)                                            \
    LOG_ALWAYS_FATAL_IF(                                        \
            !(e),                                               \
            "<%s:%d>CDX_CHECK(%s) failed.",                     \
            __FUNCTION__, __LINE__, #e)

#else
#define CDX_CHECK(e)
#endif

#else

#include <stdio.h>
#include <string.h>
#include <assert.h>

extern const char *CDX_LOG_LEVEL_NAME[];
#define AWLOG(level, fmt, arg...)  \
    do { \
        if ((level >= GLOBAL_LOG_LEVEL || level >= CONFIG_LOG_LEVEL) && \
                level <= LOG_LEVEL_ERROR) \
            printf("%s: %s <%s:%u>: " fmt "\n", \
                    CDX_LOG_LEVEL_NAME[level], LOG_TAG, __FUNCTION__, __LINE__, ##arg); \
    } while (0)

#define CDX_TRESPASS()

#define CDX_FORCE_CHECK(e) CDX_CHECK(e)

#define CDX_LOG_CHECK(e, fmt, arg...)                           \
    do {                                                        \
        if (!(e))                                               \
        {                                                       \
            CDX_LOGE("check (%s) failed:"fmt, #e, ##arg);       \
            assert(0);                                          \
        }                                                       \
    } while (0)

#ifdef AWP_DEBUG
#define CDX_CHECK(e)                                            \
    do {                                                        \
        if (!(e))                                               \
        {                                                       \
            CDX_LOGE("check (%s) failed.", #e);                 \
            assert(0);                                          \
        }                                                       \
    } while (0)
#else
#define CDX_CHECK(e)
#endif

#endif

#define CDX_LOGV(fmt, arg...) logv(fmt, ##arg)
#define CDX_LOGD(fmt, arg...) logd(fmt, ##arg)
#define CDX_LOGI(fmt, arg...) logi(fmt, ##arg)
#define CDX_LOGW(fmt, arg...) logw(fmt, ##arg)
#define CDX_LOGE(fmt, arg...) loge(fmt, ##arg)

#define CDX_BUF_DUMP(buf, len) \
    do { \
        char *_buf = (char *)buf;\
        char str[1024] = {0};\
        unsigned int index = 0, _len;\
        _len = (unsigned int)len;\
        snprintf(str, 1024, ":%d:[", _len);\
        for (index = 0; index < _len; index++)\
        {\
            snprintf(str + strlen(str), 1024 - strlen(str), "%02hhx ", _buf[index]);\
        }\
        str[strlen(str) - 1] = ']';\
        CDX_LOGD("%s", str);\
    }while (0)

#define CDX_ITF_CHECK(base, itf)    \
    CDX_CHECK(base);                \
    CDX_CHECK(base->ops);           \
    CDX_CHECK(base->ops->itf)

#define CDX_UNUSE(param) (void)param
#define CEDARX_UNUSE(param) (void)param

#define logd(fmt, arg...) AWLOG(LOG_LEVEL_DEBUG, fmt, ##arg)
#define loge(fmt, arg...) AWLOG(LOG_LEVEL_ERROR, "\033[40;31m" fmt "\033[0m", ##arg)
#define logw(fmt, arg...) AWLOG(LOG_LEVEL_WARNING, fmt, ##arg)
#define logi(fmt, arg...) AWLOG(LOG_LEVEL_INFO, fmt, ##arg)
#define logv(fmt, arg...) AWLOG(LOG_LEVEL_VERBOSE, fmt, ##arg)

#ifdef __cplusplus
extern "C"
{
#endif

void log_set_level(unsigned level);

#ifdef __cplusplus
}
#endif

#endif
