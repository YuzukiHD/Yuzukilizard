/*
*********************************************************************************************************
*                                                    eMOD
*                                   the Easy Portable/Player Operation System
*                                              mod_mmp sub-system
*
*                                    (c) Copyright 2008-2009, Kevin.Z China
*                                              All Rights Reserved
*
* File   : cache_fs_debug.c
* Version: V1.0
* By     : kevin
* Date   : 2010-3-14 9:20:57
* Modify : zengzhijin 2020-04-24
*********************************************************************************************************
*/
#include <stdlib.h>
#include "cache_fs_s.h"
#include "cache_fs_debug.h"
#include <log.h>

/*
 * open cache fs log file.
 *
 * Returns:     log file handle     succeeded
 *              NULL                failed
 */
ES_FILE *cache_fs_open_log(const char *origin_path, int level)
{
    char    tmpPath[512];
    char    *p      = NULL;
    __cache_fs_logging_t *tmpLogFile;

    if (!origin_path)
    {
        __inf("CACHE_FS WARNING : invalid parameter for cache_fs_open_log\n");
        return NULL;
    }

    /* request memory for cache file handle */
    tmpLogFile = (__cache_fs_logging_t *)malloc(sizeof(__cache_fs_logging_t));

    if (!tmpLogFile)
    {
        __inf("Request memory for cache log file handle failed!\n");
        return NULL;
    }

    memset(tmpPath, 0, 512);
    memset(tmpLogFile, 0, sizeof(__cache_fs_logging_t));
    /* format logging file path name :
     * the dest partition is "E:\\"
     * the file name postfix is ".log"
     */
    strcpy(tmpPath, "E:\\");
    p = (char *)(origin_path + strlen(origin_path));

    while (*p == '\\')
    {
        p--;
    }

    while (*p != '\\')
    {
        if (p == tmpPath)
        {
            break;
        }

        p--;
    }

    strcat(tmpPath, p);
    strcat(tmpPath, ".log");
    tmpLogFile->fp = esFSYS_fopen(tmpPath, "wb+");

    if (tmpLogFile->fp == NULL)
    {
        __inf("CACHE_FS WARNING : create logging dest file failed\n");
        free(tmpLogFile);
        return NULL;
    }

    tmpLogFile->level |= level;
    return (ES_FILE *)tmpLogFile;
}

/*
 * close cache fs log file.
 *
 * Returns:     0       succeeded
 *              others  failed
 */
int cache_fs_close_log(ES_FILE *fp)
{
    __cache_fs_logging_t *tmpLogFile;

    if (!fp)
    {
        __inf("CACHE_FS WARNING : close NULL file\n");
        return -1;
    }

    tmpLogFile = (__cache_fs_logging_t *)fp;
    esFSYS_fclose(tmpLogFile->fp);
    free(tmpLogFile);
    return 0;
}

/*
 * enable certain log level.
 */
int cache_fs_set_log_level(ES_FILE *fp, int level)
{
    int old;
    __cache_fs_logging_t *tmpLogFile;

    if (!fp)
    {
        __inf("CACHE_FS WARNING : close NULL file\n");
        return -1;
    }

    tmpLogFile = (__cache_fs_logging_t *)fp;
    old = tmpLogFile->level;
    tmpLogFile->level |= level;
    return old;
}

/*
 * disable certain log level.
 */
int cache_fs_clear_log_level(ES_FILE *fp, int level)
{
    int old;
    __cache_fs_logging_t *tmpLogFile;

    if (!fp)
    {
        __inf("CACHE_FS WARNING : close NULL file\n");
        return -1;
    }

    tmpLogFile = (__cache_fs_logging_t *)fp;
    old = tmpLogFile->level;
    tmpLogFile->level &= (~level);
    return old;
}

/**
 * cache_fs_log_get_prefix - Default prefixes for logging levels
 * @level:  Log level to be prefixed
 *
 * Prefixing the logging output can make it easier to parse.
 *
 * Returns:  "string"  Prefix to be used
 */
static const char *cache_fs_log_get_prefix(int level)
{
    const char *prefix;

    switch (level)
    {
        case CACHE_FS_ERROR:
            prefix = "Cache fs error    : ";
            break;

        case CACHE_FS_WARNING:
            prefix = "Cache fs warning  : ";
            break;

        case CACHE_FS_FREAD:
            prefix = "Cache fs fread    : ";
            break;

        case CACHE_FS_FSEEK:
            prefix = "Cache fs fseek    : ";
            break;

        case CACHE_FS_FTELL:
            prefix = "Cache fs ftell    : ";
            break;

        default:
            prefix = "";
            break;
    }

    return prefix;
}

/**
 * cache_fs_log - write cache fs log information to logging file.
 *              - if logging file NULL, write to sieral directly.
 *
 * level    :   Level at which the line is logged
 * format   :   printf-style formatting string
 * ...      :   Arguments to be formatted
 *
 * Returns:  -1  Error occurred
 *            0  Message wasn't logged
 *          num  Number of output characters
 */
static char stream[1024];

/* use ARM libary */
extern int sprintf(char *buffer, const char *format, ...);
extern int vsnprintf(char *s, __size_t n, const char *format, va_list arg);
int cache_fs_log(ES_FILE *fp, int level, const char *format, ...)
{
    int ret;
    va_list args;
    int pos = 0;
    int num = 0;
    __cache_fs_logging_t *tmpLogFile;

    if (!fp)
    {
        __inf("CACHE_FS WARNING : close NULL file\n");
        return -1;
    }

    tmpLogFile = (__cache_fs_logging_t *)fp;

    if (!(tmpLogFile->level & level))
    {
        /* don't log this message */
        return 0;
    }

    /* get log prefix string */
    pos = sprintf(stream, "%s", cache_fs_log_get_prefix(level));
    /* format arguments to stream buffer */
    va_start(args, format);
    ret = vsnprintf((stream + pos), 1024, format, args);
    va_end(args);

    num = strlen(stream);
    esFSYS_fwrite(stream, 1, num, (__hdle)tmpLogFile->fp);

    return ret;
}
