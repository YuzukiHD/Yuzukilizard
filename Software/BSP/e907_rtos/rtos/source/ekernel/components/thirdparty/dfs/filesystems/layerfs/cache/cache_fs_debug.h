/*
*********************************************************************************************************
*                                                    eMOD
*                                   the Easy Portable/Player Operation System
*                                              mod_mmp sub-system
*
*                                    (c) Copyright 2008-2009, Kevin.Z China
*                                              All Rights Reserved
*
* File   : cache_fs_debug.h
* Version: V1.0
* By     : kevin
* Date   : 2010-3-14 9:20:57
    目前是给cache_fs_align.c专用的，用于debug。
* Modify : zengzhijin 2020-04-24
*********************************************************************************************************
*/
#ifndef _CONFIG_LAYERFS_CACHE_DEBUG_H_
#define _CONFIG_LAYERFS_CACHE_DEBUG_H_

//#include  <stdarg.h>
//#include  "cache_fs.h"

/* cache logging levels */
#define     CACHE_FS_INFO       ((int)(1<<0))
#define     CACHE_FS_WARNING    ((int)(1<<1))
#define     CACHE_FS_ERROR      ((int)(1<<2))
#define     CACHE_FS_FREAD      ((int)(1<<3))
#define     CACHE_FS_FSEEK      ((int)(1<<4))
#define     CACHE_FS_FTELL      ((int)(1<<5))

/* cache_fs_logging structure */
typedef struct __CACHE_FS_LOGGING
{
    ES_FILE    *fp;            //file pointer for write log, if NULL, write to serial directly.
    int     level;
} __cache_fs_logging_t;

/* open/close cache fs log file         */
ES_FILE *cache_fs_open_log(const char *origin_path, int level);
int cache_fs_close_log(ES_FILE *fp);

/* enable/disable certain log level     */
int cache_fs_set_log_level(ES_FILE *fp, int level);
int cache_fs_clear_log_level(ES_FILE *fp, int level);

/* write cache fs log                   */
int cache_fs_log(ES_FILE *fp, int level, const char *format, ...);

#endif      /* _CONFIG_LAYERFS_CACHE_DEBUG_H_ */
