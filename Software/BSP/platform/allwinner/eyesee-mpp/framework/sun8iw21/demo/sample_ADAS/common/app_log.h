#ifndef USER_LOG_H_
#define USER_LOG_H_

#define COLOR_LOG                   1
#define FORMAT_LOG                  1

#ifndef NDEBUG
#define DB_ERROR
#define DB_WARN
#define DB_INFO
#define DB_DEBUG
#define DB_MSG
#else
#define DB_ERROR
#define DB_WARN
#undef DB_INFO
#undef DB_DEBUG
#undef DB_MSG
#endif


#ifdef USE_STD_LOG
#include <log/std_log.h>

#ifdef DB_ERROR
#define db_error(fmt, arg...) \
    do { \
        ALOGE(FMT_COLOR_FG(LIGHT, RED) "<%s:%d> " XPOSTO(90) FMT_DEFAULT fmt, \
                __FUNCTION__, __LINE__, ##arg); \
    } while(0)
#else
#define db_error(fmt, arg...)
#endif

#ifdef DB_WARN
#define db_warn(fmt, arg...) \
    do { \
        ALOGW(FMT_COLOR_FG(LIGHT, YELLOW) "<%s:%d> " XPOSTO(90) FMT_DEFAULT fmt, \
                __FUNCTION__, __LINE__, ##arg); \
    } while(0)
#else
#define db_warn(fmt, arg...)
#endif

#ifdef DB_INFO
#define db_info(fmt, arg...) \
    do { \
        ALOGI(FMT_COLOR_FG(LIGHT, GREEN) "<%s:%d> " XPOSTO(90) FMT_DEFAULT fmt, \
                __FUNCTION__, __LINE__, ##arg); \
    } while(0)
#else
#define db_info(fmt, arg...)
#endif

#ifdef DB_DEBUG
#define db_debug(fmt, arg...) \
    do { \
        ALOGD(FMT_COLOR_FG(LIGHT, BLUE) "<%s:%d> " XPOSTO(90) FMT_DEFAULT fmt, \
                __FUNCTION__, __LINE__, ##arg); \
    } while(0)
#else
#define db_debug(fmt, arg...)
#endif

#ifdef DB_MSG
#define db_msg(fmt, arg...) \
    do { \
        ALOGV(FMT_COLOR_FG(LIGHT, WRITE) "<%s:%d> " XPOSTO(90) FMT_DEFAULT fmt, \
                __FUNCTION__, __LINE__, ##arg); \
    } while(0)
#else
#define db_msg(fmt, arg...)
#endif
// USE_STD_LOG
#else
#include <log/log.h>

#ifdef DB_FATAL
#define db_fatal(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_FATAL, fmt, ##arg); \
    } while(0)
#else
#define db_fatal(fmt, arg...)
#endif

#ifdef DB_ERROR
#define db_error(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_ERROR, fmt, ##arg); \
    } while(0)
#else
#define db_error(fmt, arg...)
#endif

#ifdef DB_WARN
#define db_warn(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_WARN, fmt, ##arg); \
    } while(0)
#else
#define db_warn(fmt, arg...)
#endif

#ifdef DB_INFO
#define db_info(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_INFO, fmt, ##arg); \
    } while(0)
#else
#define db_info(fmt, arg...)
#endif

#ifdef DB_DEBUG
#define db_debug(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_INFO, fmt, ##arg); \
    } while(0)
#else
#define db_debug(fmt, arg...)
#endif

#ifdef DB_MSG
#define db_msg(fmt, arg...) \
    do { \
        GLOG_PRINT(_GLOG_INFO, fmt, ##arg); \
    } while(0)
#else
#define db_msg(fmt, arg...)
#endif

#endif

#endif
