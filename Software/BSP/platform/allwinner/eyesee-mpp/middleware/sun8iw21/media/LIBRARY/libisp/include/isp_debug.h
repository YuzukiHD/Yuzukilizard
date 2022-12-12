
/*
 ******************************************************************************
 *
 * isp_debug.h
 *
 * Hawkview ISP - isp_debug.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   3.0		  Yang Feng   	2016/03/16	VIDEO INPUT
 *
 *****************************************************************************
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>
#include <errno.h>

//#define ANDROID_PLATFORM

#ifdef ANDROID_PLATFORM
#include <log/log.h>
#endif


//#define ISP_DGB_FL

#ifdef ANDROID_PLATFORM
#define ISP_WARN(fmt,arg...) ALOGW(fmt, ##arg)
#define ISP_PRINT(fmt,arg...) ALOGV(fmt, ##arg)
#define ISP_ERR(fmt, arg...) ALOGE(fmt, ##arg)
#else
#define ISP_ERR(x, arg...) printf("[ISP_ERR]%s, line: %d," x , __FUNCTION__, __LINE__, ##arg)
#define ISP_WARN(x, arg...) printf("[ISP_WARN]" x , ##arg)
#define ISP_PRINT(x, arg...) printf("[ISP]" x , ##arg)
#endif

#ifdef ISP_DGB_FL
#ifdef ANDROID_PLATFORM
    #define  FUNCTION_LOG do { ALOGV("%s, line: %d\n", __FUNCTION__, __LINE__); } while(0)
#else
    #define  FUNCTION_LOG do { printf("%s, line: %d\n", __FUNCTION__, __LINE__); } while(0)
#endif
#else
#define  FUNCTION_LOG do { } while(0)
#endif

#define ISP_LOG_AE				(1 << 0)	//0x1
#define ISP_LOG_AWB				(1 << 1)	//0x2
#define ISP_LOG_AF				(1 << 2)	//0x4
#define ISP_LOG_ISO				(1 << 3)	//0x8
#define ISP_LOG_GAMMA				(1 << 4)	//0x10
#define ISP_LOG_COLOR_MATRIX			(1 << 5)	//0x20
#define ISP_LOG_AFS				(1 << 6)	//0x40
#define ISP_LOG_MOTION_DETECT			(1 << 7)	//0x80
#define ISP_LOG_GAIN_OFFSET			(1 << 8)	//0x100
#define ISP_LOG_DEFOG				(1 << 9)	//0x200
#define ISP_LOG_LSC				(1 << 10)	//0x400
#define ISP_LOG_GTM				(1 << 11)	//0x800
#define ISP_LOG_PLTM				(1 << 12)	//0x1000

#define ISP_LOG_SUBDEV				(1 << 13)	//0x2000
#define ISP_LOG_CFG				(1 << 14)	//0x4000
#define ISP_LOG_VIDEO				(1 << 15)	//0x8000
#define ISP_LOG_ISP				(1 << 16)	//0x10000
#define ISP_LOG_FLASH				(1 << 17)	//0x20000

#ifdef ANDROID_PLATFORM
int ISP_LIB_LOG(int flag,const char *fmt,...);

#define ISP_DEV_LOG(flag, msg...)\
	do {\
		if (isp_dev_log_param & flag)\
		ALOGV("[ISP_DEBUG]: " msg);\
	} while (0);\

/*#define ISP_LIB_LOG(flag, msg...)\
	do {\
		if (isp_lib_log_param & flag)\
		ALOGV("[ISP_DEBUG]: " msg);\
	} while (0);\*/

#define ISP_CFG_LOG(flag, msg...)\
	do {\
		if (isp_cfg_log_param & flag)\
		ALOGV("[ISP_DEBUG]: " msg);\
	} while (0);\

#else

#define ISP_DEV_LOG(flag, msg...)\
	do {\
		if (isp_dev_log_param & flag)\
		printf("[ISP_DEBUG]: " msg);\
	} while (0);\

#define ISP_LIB_LOG(flag, msg...)\
	do {\
		if (isp_lib_log_param & flag)\
		printf("[ISP_DEBUG]: " msg);\
	} while (0);\

#define ISP_CFG_LOG(flag, msg...)\
	do {\
		if (isp_cfg_log_param & flag)\
		printf("[ISP_DEBUG]: " msg);\
	} while (0);\

#endif

#endif /*_DEBUG_H_*/


