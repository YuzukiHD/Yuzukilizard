#ifndef USER_LOG_H_
#define USER_LOG_H_

#include <log/log.h>

#define DB_ERROR
#define DB_WARN
#define DB_INFO
#define DB_DEBUG
#define DB_MSG

#ifdef DB_ERROR
#define db_error(fmt, arg...)	\
	do { \
		ALOGE("<line[%04d] %s()> " fmt, __LINE__, __FUNCTION__, ##arg); \
	} while(0)
#else
#define db_error(fmt, arg...)
#endif

#ifdef DB_WARN
#define db_warn(fmt, arg...) \
	do { \
		ALOGW("<line[%04d] %s()> " fmt, __LINE__, __FUNCTION__, ##arg); \
	} while(0)
#else
#define db_warn(fmt, arg...)
#endif

#ifdef DB_INFO
#define db_info(fmt, arg...)	\
	do { \
		ALOGI("<line[%04d] %s()> " fmt, __LINE__, __FUNCTION__, ##arg); \
	} while(0)
#else
#define db_info(fmt, arg...)
#endif

#ifdef DB_DEBUG
#define db_debug(fmt, arg...)	\
	do { \
		ALOGD("<line[%04d] %s()> " fmt, __LINE__, __FUNCTION__, ##arg); \
	} while(0)
#else
#define db_debug(fmt, arg...)
#endif

#ifdef DB_MSG
#define db_msg(fmt, arg...)	\
	do { \
		ALOGV("<line[%04d] %s()> " fmt, __LINE__, __FUNCTION__, ##arg); \
	} while(0)
#else
#define db_msg(fmt, arg...)
#endif

#endif
