#ifndef LOG_WRAPPER_H_
#define LOG_WRAPPER_H_

#define COLOR_LOG                   1
#define FORMAT_LOG                  1

#ifdef USE_STD_LOG
#include "std_log.h"
#else
#include "log.h"
#endif

#ifndef NDEBUG
#define DB_ERROR
#define DB_WARN
#define DB_INFO
#define DB_DEBUG
#define DB_MSG
#else
#define DB_ERROR
#define DB_WARN
#endif

#ifdef DB_ERROR
#define LOGE(fmt, arg...) \
    do { \
        ALOGE(fmt, \
                ##arg); \
    } while(0)
#else
#define LOGE(fmt, arg...)
#endif

#ifdef DB_WARN
#define LOGW(fmt, arg...) \
    do { \
        ALOGW(fmt, \
                ##arg); \
    } while(0)
#else
#define LOGW(fmt, arg...)
#endif

#ifdef DB_INFO
#define LOGI(fmt, arg...) \
    do { \
        ALOGI(fmt, \
                ##arg); \
    } while(0)
#else
#define LOGI(fmt, arg...)
#endif

#ifdef DB_DEBUG
#define LOGD(fmt, arg...) \
    do { \
        ALOGD(fmt, \
                ##arg); \
    } while(0)
#else
#define LOGD(fmt, arg...)
#endif

#ifdef DB_MSG
#define LOGV(fmt, arg...) \
    do { \
        ALOGV(fmt, \
                ##arg); \
    } while(0)
#else
#define LOGV(fmt, arg...)
#endif

#endif

