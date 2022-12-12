/*
 * =====================================================================================
 *
 *       Filename:  cdc_log.c
 *
 *    Description: Log debug module
 *
 *        Version:  1.0
 *        Created:  2020年01月08日 19时39分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *        Company: AllwinnerTech
 *
 * =====================================================================================
 */
#include <stdio.h>
#include <stdarg.h>
#include "cdc_log.h"
#include <stdlib.h>
#ifdef __ANDROID__
#include <cutils/properties.h>
#endif

enum CDC_LOG_LEVEL_TYPE CDC_GLOBAL_LOG_LEVEL = LOG_LEVEL_INFO;

#ifndef __ANDROID__
const char *CDC_LOG_LEVEL_NAME[] = {
     "",
     "",
     [LOG_LEVEL_VERBOSE] = "VERBOSE",
     [LOG_LEVEL_DEBUG]   = "DEBUG  ",
     [LOG_LEVEL_INFO]    = "INFO   ",
     [LOG_LEVEL_WARNING] = "WARNING",
     [LOG_LEVEL_ERROR]   = "ERROR  ",
};
#endif

//TODO: set module tag in log
void cdc_log_set_level(unsigned level)
{
    logi("Set log level to %u from /vendor/etc/cedarc.conf", level);
#ifdef __ANDROID__
    char val[256];
    property_get("vendor.omx.debuglevel", val, "3");
    if(atoi(val) >= LOG_LEVEL_VERBOSE && atoi(val) <= LOG_LEVEL_ERROR)
    {
       level = atoi(val);
       logw("Reset log level to %u from property of vendor.cedarc.debuglevel", level);
    }
#endif
    CDC_GLOBAL_LOG_LEVEL = level;
}
