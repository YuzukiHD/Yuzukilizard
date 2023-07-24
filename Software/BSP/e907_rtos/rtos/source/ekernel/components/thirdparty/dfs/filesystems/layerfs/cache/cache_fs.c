/*
*********************************************************************************************************
*                                                    eMOD
*                                   the Easy Portable/Player Operation System
*                                              mod_mmp sub-system
*
*                                    (c) Copyright 2008-2009, Kevin.Z China
*                                              All Rights Reserved
*
* File   : cache_fs.c
* Version: V1.0
* By     : kevin
* Date   : 2009-2-2 11:38
* Modify : sunny, 2010-4-1 16:09,
*           文件系统封装层兼容字节对齐和扇区对齐两种机制，由用户自行配置自己需要的机制。
* Modify : zengzhijin 2020-04-24
*********************************************************************************************************
*/
#include <typedef.h>
#include <log.h>
#include "cache_fs_s.h"
#include "cache_fs.h"

__s32   cache_fs_work_mode = CACHE_FS_WORK_MODE_SECTALIGN;
__s32   CACHE_FS_CACHE_SIZE = (1024 * 512);
__s32   CACHE_FS_CACHE_BITS = 16;
__u32   module_id = 0;  //模块id号

__s32  CACHE_fs_set_workmod(__s32 mode)
{
    cache_fs_work_mode = mode;
    return cache_fs_work_mode;
}

__s32  CACHE_fs_get_workmod(void)
{
    return cache_fs_work_mode;
}

/*******************************************************************************
Function name: CACHE_fs_SetCacheSize
Description:
    1.有些psr例如flv_psr不需要太大的cache, cache太大反而对FFRRJUMP性能影响很大.故提供本
    接口，让flv_psr自行把cache_size设小.
Parameters:
    1.mode:表示是哪种mode下的cache size
Return:

Time: 2010/1/10
*******************************************************************************/
int CACHE_fs_SetCacheSize(int cache_size, int mode)
{
    __s32 cache_bits = 0;
    CACHE_FS_CACHE_SIZE = cache_size;

    while (cache_size != 0)
    {
        cache_size = cache_size >> 1;
        cache_bits++;
    }

    CACHE_FS_CACHE_BITS = cache_bits - 1;

    if (mode == CACHE_FS_WORK_MODE_SECTALIGN)
    {
        __wrn("CACHE_FS_CACHE_SIZE[%d], CACHE_FS_CACHE_BITS[%d]\n", CACHE_FS_CACHE_SIZE, cache_bits);
        return 0;
    }
    else if (CACHE_FS_WORK_MODE_HERBCACHE == mode)
    {
        __wrn("mode = CACHE_FS_WORK_MODE_HERBCACHE, cachesize =[%d]MB\n", CACHE_FS_CACHE_SIZE / (1024 * 1024));
        return 0;
    }
    else
    {
        __wrn("fatal error, mode[%x]\n", mode);
        return -1;
    }
}

void  CACHE_fs_set_mid(__u32 mid) //因为fs buffer mode要开线程， epdk系统下，开线程得给一个module id.
{
    module_id = mid;
}

CacheFS *newCacheFs(__s32 nFSMode)  //CACHE_FS_WORK_MODE_BYTEALIGN
{
    CacheFS *pCacheFS;

    switch (nFSMode)
    {
#ifdef CONFIG_LAYERFS_CACHE_NORMAL
        case CACHE_FS_WORK_MODE_BYTEALIGN:
        {
            pCacheFS = (CacheFS *)newCacheFSNormal();
            break;
        }
#endif
#ifdef CONFIG_LAYERFS_CACHE_SECTOR_ALIGN
        case CACHE_FS_WORK_MODE_SECTALIGN:
        {
            pCacheFS = (CacheFS *)newCacheFSAlign();
            break;
        }
#endif
#ifdef CONFIG_LAYERFS_CACHE_HERB
        case CACHE_FS_WORK_MODE_HERBCACHE:
        {
            pCacheFS = (CacheFS *)newHerbFSCache();
            break;
        }
#endif
        default: /* CACHE_FS_WORK_MODE_NO_CACHE */
        {
            pCacheFS = (CacheFS *)newCacheFSDirect();
            break;
        }
    }
    return pCacheFS;
}

void deleteCacheFs(CacheFS *pCacheFS)
{
    switch (pCacheFS->cache_fs_work_mode)
    {
#ifdef CONFIG_LAYERFS_CACHE_NORMAL
        case CACHE_FS_WORK_MODE_BYTEALIGN:
        {
            deleteCacheFSNormal((CacheFSNormal *)pCacheFS);
            break;
        }
#endif
#ifdef CONFIG_LAYERFS_CACHE_SECTOR_ALIGN
        case CACHE_FS_WORK_MODE_SECTALIGN:
        {
            deleteCacheFSAlign((CacheFSAlign *)pCacheFS);
            break;
        }
#endif
#ifdef CONFIG_LAYERFS_CACHE_HERB
        case CACHE_FS_WORK_MODE_HERBCACHE:
        {
            deleteHerbFSCache((HerbFSCache *)pCacheFS);
            break;
        }
#endif
        default: /* CACHE_FS_WORK_MODE_NO_CACHE */
        {
            deleteCacheFSDirect((CacheFSDirect *)pCacheFS);
            break;
        }
    }

    return;
}

/*
*********************************************************************************************************
*                           CACHE ES_FILE OPEN
*
* Description: open cache media file;
*
* Arguments  : path     media file path;
*              mode     open mode;
*
* Returns    : handle for the file operation;
*               =   0, open media file failed;
*              !=   0, open media file successed;
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
int epdk_mode_to_flag_trans(const char *p_mode, __u32 *mode);

ES_FILE *CACHE_fopen(const char *path, const char *mode, __u32 flags)
{
    ES_FILE *fp = NULL;
    CacheFS *pCacheFS = NULL;
    __s32 nFSMode = cache_fs_work_mode;

    if (!path || !mode)
    {
        __wrn("Parameter for open cache file is invalid!\n");
        return NULL;
    }

    if (flags & O_RDONLY || flags == O_RDONLY)
    {
        __hdle fp_stat;
        __ES_FSTAT fstat;

        nFSMode = CACHE_FS_WORK_MODE_SECTALIGN;
        memset(&fstat, 0, sizeof(__ES_FSTAT));
        fp_stat = esFSYS_fopen(path, mode);
        if (EPDK_OK == esFSYS_fstat(fp_stat, &fstat))
        {
            if (fstat.size <= (1024 * 1024))
            {
                nFSMode = CACHE_FS_WORK_MODE_NO_CACHE;
            }
        }
        esFSYS_fclose(fp_stat);
    }
    else if (flags & O_WRONLY)
    {
        nFSMode = CACHE_FS_WORK_MODE_HERBCACHE;
    }
    else
    {
        nFSMode = CACHE_FS_WORK_MODE_NO_CACHE;
    }

    pCacheFS = newCacheFs(nFSMode);
    pCacheFS->SetCacheSize(pCacheFS, CACHE_FS_CACHE_SIZE);
    fp = pCacheFS->cachefs_fopen(pCacheFS, path, mode);

    if (NULL == fp)
    {
        __wrn("fopen fail\n");
        deleteCacheFs(pCacheFS);
        return NULL;
    }

    __msg("cache_fs select mode[%d]\n", pCacheFS->cache_fs_work_mode);
    return (ES_FILE *)pCacheFS;
}

__s32 CACHE_fstat(ES_FILE *fp, void *buf)
{
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (NULL == fp || pCacheFS->cachefs_fstat == NULL)
    {
        __wrn("file handle is invalid when stat cache file!\n");
        return -1;
    }

    return pCacheFS->cachefs_fstat(pCacheFS, buf);
}

__s32 CACHE_fsync(ES_FILE *fp)
{
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (NULL == fp || pCacheFS->cachefs_fsync == NULL)
    {
        __wrn("file handle is invalid when stat cache file!\n");
        return -1;
    }

    return pCacheFS->cachefs_fsync(pCacheFS);
}

__s32 CACHE_ftruncate(ES_FILE *fp, off_t length)
{
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (NULL == fp || pCacheFS->cachefs_ftruncate == NULL)
    {
        __wrn("file handle is invalid when ftruncate file!\n");
        return -1;
    }

    return pCacheFS->cachefs_ftruncate(pCacheFS, length);
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
int CACHE_fclose(ES_FILE *fp)
{
    int retval = -1;
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (!fp)
    {
        __wrn("file handle is invalid when close cache file!\n");
        return -1;
    }

    retval = pCacheFS->cachefs_fclose(pCacheFS);
    deleteCacheFs(pCacheFS);
    return retval;
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
int CACHE_fread(void *buf, int size, int count, ES_FILE *fp)
{
    int retval = 0;
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (!fp || !size || !count || !buf)
    {
        __wrn("File handle or parameter is invalid when read file!\n");
        return 0;
    }

    retval = pCacheFS->cachefs_fread(pCacheFS, buf, size, count);
    return retval;
}

/*
*********************************************************************************************************
*                           WRITE CACHE ES_FILE DATA
*
* Description: write cache media file;
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
int CACHE_fwrite(void *buf, int size, int count, ES_FILE *fp)
{
    int retval = 0;
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (!fp || !size || !count || !buf)
    {
        __wrn("File handle or parameter is invalid when write file!\n");
        return 0;
    }

    retval = pCacheFS->cachefs_fwrite(pCacheFS, buf, size, count);
    return retval;
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
int CACHE_fseek(ES_FILE *fp, __s64 offset, int origin)
{
    int retval = 0;
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (!fp)
    {
        __wrn("File handle is invalid when read data!\n");
        return 0;
    }

    retval = pCacheFS->cachefs_fseek(pCacheFS, offset, origin);
    return retval;
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
__s64 CACHE_ftell(ES_FILE *fp)
{
    __s64 retval = 0;
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (!fp)
    {
        __wrn("File handle is invalid when tell file pointer!\n");
        return -1;
    }

    retval = pCacheFS->cachefs_ftell(pCacheFS);
    return retval;
}

int CACHE_fioctrl(ES_FILE *fp, __s32 Cmd, __s32 Aux, void *pBuffer)
{
    ES_FILE *pFilefp;
    CacheFS *pCacheFS = (CacheFS *)fp;

    if (!fp)
    {
        __wrn("File handle or parameter is invalid when read file!\n");
        return -1;
    }

    pFilefp = pCacheFS->GetFp(pCacheFS);
    return esFSYS_fioctrl(pFilefp, Cmd, Aux, pBuffer);
}

/*
*********************************************************************************************************
*                           TELL THE POSITION OF CACHE ES_FILE POINTER
*
* Description: get the size of cache media file;
*
* Arguments  : fp       cache file handle;
*
* Returns    : result, file pointer position;
*
* Note       : if we read some small block from file system frequently, the speed will
*              be very slow, so, we add an adapt layer to accelerate the access speed.
*********************************************************************************************************
*/
__u32 CACHE_fsize(ES_FILE *fp)
{
    __u32   size;
    __s64   size_s64;
    CacheFS *pCacheFS = (CacheFS *)(fp);

    if (!fp)
    {
        __wrn("File handle is invalid when fsize!\n");
        return (__u32) - 1;
    }

    pCacheFS->cachefs_fseek(pCacheFS, 0, SEEK_END);
    size_s64 = pCacheFS->cachefs_ftell(pCacheFS);
    pCacheFS->cachefs_fseek(pCacheFS, 0, SEEK_SET);

    if (size_s64 > 0xffffffff)
    {
        __wrn("fatal error! file size > __u32\n");
    }

    size = (__u32)size_s64;
    return size;
}
