/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : log.c
 * Description : log module
 * History :
 *
 */
#include <stdio.h>
#include <stdarg.h>
#include "cdx_log.h"

enum CDX_LOG_LEVEL_TYPE GLOBAL_LOG_LEVEL = LOG_LEVEL_WARNING;

#ifndef __ANDROID__
const char *CDX_LOG_LEVEL_NAME[] = {
    "",
    "",
    [LOG_LEVEL_VERBOSE] = "VERBOSE",
    [LOG_LEVEL_DEBUG]   = "DEBUG  ",
    [LOG_LEVEL_INFO]    = "INFO   ",
    [LOG_LEVEL_WARNING] = "WARNING",
    [LOG_LEVEL_ERROR]   = "ERROR  ",
};
#endif

void log_set_level(unsigned level)
{
    logd("Set log level to %u", level);
    GLOBAL_LOG_LEVEL = level;
}
