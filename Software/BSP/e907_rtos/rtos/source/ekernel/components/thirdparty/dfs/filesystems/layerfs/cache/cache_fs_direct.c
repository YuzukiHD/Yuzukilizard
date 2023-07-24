/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_cache sub-system
*                          (module name, e.g.mpeg4 decoder plug-in) module
*
*          (c) Copyright 2010-2012, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : cache_fs_sys.c
* Version: V1.0
* By     : Eric_wang
* Date   : 2011-2-18
* Modify : zengzhijin 2020-04-24
* Description:
********************************************************************************
*/

#include "cache_fs_s.h"
#include "cache_fs_direct.h"
#include <string.h>
#include <log.h>
#include <stdlib.h>
/*
*********************************************************************************************************
*                           CACHE ES_FILE OPEN
*
* Description: open cache media file;
*
* Arguments  : path     media file path;
*              mode     open mode;
*
* Returns    : handle for the file operation;真正的fp
*               =   0, open media file failed;
*              !=   0, open media file successed;
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
/* 打开文件,返回的是真正的文件指针 */
static ES_FILE *CACHE_dfopen(CacheFS *thiz, const __s8 *path, const __s8 *mode)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!path || !mode)
    {
        __wrn("Parameter for open cache file is invalid!");
        return NULL;
    }

    pFSDirect->fp = esFSYS_fopen(path, mode);

    if (!pFSDirect->fp)
    {
        __wrn("Open cache file failed!");
    }

    return pFSDirect->fp;
}

/*
*********************************************************************************************************
*                           CACHE ES_FILE CLOSE
*
* Description: close cache media file;
*
* Arguments  : fp       cache file handle;
*
* Returns    : clsoe file result;
*               =   0, close file successed;
*               <   0, close file failed;
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
static __s32 CACHE_dfclose(CacheFS *thiz)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp)
    {
        __wrn("file handle is invalid when close cache file!");
        return -1;
    }

    return esFSYS_fclose(pFSDirect->fp);
}


/*
*********************************************************************************************************
*                           READ CACHE ES_FILE DATA
*
* Description: read cache media file;
*
* Arguments  : buf      buffer for store file data;
*              size     size of block for read;
*              count    block count for read;
*              fp       cache file handle;
*
* Returns    : size of data read from file;
*
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
static __s32 CACHE_dfread(CacheFS *thiz, void *buf, __s32 size, __s32 count)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp || !size || !count)
    {
        __wrn("File handle or parameter is invalid when read file!");
        return 0;
    }

    return esFSYS_fread(buf, size, count, pFSDirect->fp);
}

/*
*********************************************************************************************************
*                           WRITE CACHE ES_FILE DATA
*
* Description: write cache media file;
*
* Arguments  : buf      buffer for store file data;
*              size     size of block for write;
*              count    block count for write;
*              fp       cache file handle;
*
* Returns    : size of data read from file;
*
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
static __s32 CACHE_dfwrite(CacheFS *thiz, void *buf, __s32 size, __s32 count)   //写文件
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp || !size || !count)
    {
        __wrn("File handle or parameter is invalid when write file!");
        return 0;
    }

    return esFSYS_fwrite(buf, size, count, pFSDirect->fp);
}


/*
*********************************************************************************************************
*                           SEEK CACHE ES_FILE POINTER
*
* Description: seek cache media file;
*
* Arguments  : fp       cache file handle;
*              offset   offset for seek file pointer;
*              origin   position mode for seek file pointer;
*
* Returns    : result;
*               =   0, seek cache file pointer successed;
*              !=   0, seek cache file pointer failed;
*
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
static __s32 CACHE_dfseek(CacheFS *thiz, __s64 offset, __s32 origin)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp)
    {
        __wrn("File handle is invalid when read data!");
        return 0;
    }

    return eLIBs_fllseek(pFSDirect->fp, offset, origin);
}

/*
*********************************************************************************************************
*                           TELL THE POSITION OF CACHE ES_FILE POINTER
*
* Description: tell the position of cache media file;
*
* Arguments  : fp       cache file handle;
*
* Returns    : result, file pointer position;
*
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
static __s64 CACHE_dftell(CacheFS *thiz)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp)
    {
        __wrn("File handle is invalid when tell file pointer!");
        return -1;
    }

    return eLIBs_flltell(pFSDirect->fp);
}

static __s32 CACHE_dSetCacheSize(CacheFS *thiz, __s32 cache_size)
{
    return EPDK_OK;
}

/* 得到真正的文件指针 */
static ES_FILE *CACHE_dGetFp(CacheFS *thiz)
{
    CacheFSDirect *pCacheFSAlign = (CacheFSDirect *)thiz;
    return pCacheFSAlign->fp;
}

static __s32 CACHE_dfstat(CacheFS *thiz, void *buf)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp)
    {
        __wrn("File handle is invalid when tell file pointer!");
        return -1;
    }
    return esFSYS_fstat(pFSDirect->fp, buf);
}

static __s32 CACHE_dfsync(CacheFS *thiz)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp)
    {
        __wrn("File handle is invalid when fsync file pointer!");
        return -1;
    }
    return esFSYS_fsync(pFSDirect->fp);
}

static __s32 CACHE_dftruncate(CacheFS *thiz, off_t length)
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)thiz;

    if (!pFSDirect->fp)
    {
        __wrn("File handle is invalid when ftruncate file pointer!");
        return -1;
    }
    return esFSYS_ftruncate(pFSDirect->fp, length);
}

CacheFSDirect *newCacheFSDirect()
{
    CacheFSDirect *pFSDirect = (CacheFSDirect *)malloc(sizeof(CacheFSDirect));

    if (NULL == pFSDirect)
    {
        __wrn("NewCacheFSDirect malloc fail");
        return NULL;
    }

    memset(pFSDirect, 0, sizeof(CacheFSDirect));
    CacheFS_Init((CacheFS *)pFSDirect);

    pFSDirect->CacheFSBase.cache_fs_work_mode = CACHE_FS_WORK_MODE_NO_CACHE;
    pFSDirect->CacheFSBase.cachefs_fopen = CACHE_dfopen;
    pFSDirect->CacheFSBase.cachefs_fclose = CACHE_dfclose;
    pFSDirect->CacheFSBase.cachefs_fread = CACHE_dfread;
    pFSDirect->CacheFSBase.cachefs_fwrite = CACHE_dfwrite;
    pFSDirect->CacheFSBase.cachefs_fseek = CACHE_dfseek;
    pFSDirect->CacheFSBase.cachefs_ftell = CACHE_dftell;
    pFSDirect->CacheFSBase.cachefs_fstat = CACHE_dfstat;
    pFSDirect->CacheFSBase.cachefs_fsync = CACHE_dfsync;
    pFSDirect->CacheFSBase.cachefs_ftruncate = CACHE_dftruncate;
    pFSDirect->CacheFSBase.SetCacheSize = CACHE_dSetCacheSize;
    pFSDirect->CacheFSBase.GetFp = CACHE_dGetFp;

    pFSDirect->fp = NULL;
    return pFSDirect;
}

void deleteCacheFSDirect(CacheFSDirect *pCacheFSDirect)
{
    free(pCacheFSDirect);
    return;
}
