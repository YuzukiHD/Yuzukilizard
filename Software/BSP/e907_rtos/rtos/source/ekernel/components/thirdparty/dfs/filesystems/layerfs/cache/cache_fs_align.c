/*
*********************************************************************************************************
*                                                    eMOD
*                                   the Easy Portable/Player Operation System
*                                              mod_mmp sub-system
*
*                                    (c) Copyright 2008-2009, Kevin.Z China
*                                              All Rights Reserved
*
* File   : cache_fs_align.c
* Version: V1.0
* By     : kevin
* Date   : 2009-2-2 11:38
* Modify : zengzhijin 2020-04-24
*********************************************************************************************************
*/

#include "cache_fs_s.h"
#include "cache_fs_align.h"
#include "cache_fs_debug.h"
#include <string.h>
#include <log.h>
#include <stdlib.h>

#define CACHE_FS_SECTOR_SIZE     512    /* 扇区的大小，单位byte */
#define CACHE_FS_SECTOR_BITS     9      /* 2的9次方 = 512Byte */

/*
*********************************************************************************************************
*                           CACHE ES_FILE OPEN
*
* Description: open cache media file;
*               打开文件,返回的是真正的文件指针
*               默认调用本函数前，cache_size和cache_bits已通过CACHE_aSetCacheSize()设置
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
ES_FILE *CACHE_afopen(CacheFS *thiz, const __s8 *path, const __s8 *mode)
{
    CacheFSAlign *pCacheFSAlign = (CacheFSAlign *)thiz;
    __cache_afile_ptr_t *tmpCacheFile = &pCacheFSAlign->AFilePtr;

    if (!path || !mode)
    {
        __wrn("Parameter for open cache file is invalid!");
        goto _err0_cache_fopen;
    }

    tmpCacheFile->fp = esFSYS_fopen(path, mode);

    if (!tmpCacheFile->fp)
    {
        __wrn("Open cache file failed!");
        goto _err1_cache_fopen;
    }

    /* request memory for cache file data */
    tmpCacheFile->cache = PHY_MALLOC(tmpCacheFile->cache_size, 1024);

    if (!tmpCacheFile->cache)
    {
        __wrn("Request memory for cache fs cache failed!");
        goto _err2_cache_fopen;
    }

    /* initialize cache information */
    tmpCacheFile->cache_offset  = 0;
    tmpCacheFile->cache_left    = 0;

    /* initilize fs sector information */
    /* FIXME : maybe sector size is not 512 byte, should maybe get from fs. */
    tmpCacheFile->sector_size   = CACHE_FS_SECTOR_SIZE;
    tmpCacheFile->sector_bits   = CACHE_FS_SECTOR_BITS;

    /* initialize file position information */
    tmpCacheFile->file_pos      = 0;

    /* get file size */
    eLIBs_fllseek(tmpCacheFile->fp, 0, SEEK_END);
    tmpCacheFile->file_size = eLIBs_flltell(tmpCacheFile->fp);
    eLIBs_fllseek(tmpCacheFile->fp, tmpCacheFile->file_pos, SEEK_SET);

#ifdef  CONFIG_LAYERFS_CACHE_DEBUG
    tmpCacheFile->log_fp = cache_fs_open_log(path, CACHE_FS_INFO);
    /* initialize cache statistical parameters */
    tmpCacheFile->cache_hits        =   0;
    tmpCacheFile->cache_discard_cnt =   0;
    tmpCacheFile->direct_read_cnt   =   0;
    tmpCacheFile->total_read_cnt    =   0;
    tmpCacheFile->total_read_size   =   0;
    tmpCacheFile->total_seek_cnt    =   0;
#endif
    return (ES_FILE *)tmpCacheFile->fp;

_err2_cache_fopen:
    esFSYS_fclose(tmpCacheFile->fp);
_err1_cache_fopen:
_err0_cache_fopen:
    return (ES_FILE *)0;
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
__s32 CACHE_afclose(CacheFS *thiz)
{
    int ret = EPDK_OK;

    CacheFSAlign *pCacheFSAlign = (CacheFSAlign *)thiz;
    __cache_afile_ptr_t  *tmpCacheFile = &pCacheFSAlign->AFilePtr;

    if (!tmpCacheFile->fp)
    {
        __wrn("file handle is invalid when CACHE_afclose()!");
        return -1;
    }

#ifdef CONFIG_LAYERFS_CACHE_DEBUG
    {
        /* report cache statistical state */
        ES_FILE *log_fp = tmpCacheFile->log_fp;
        cache_fs_log(log_fp, CACHE_FS_INFO, "========================================");
        cache_fs_log(log_fp, CACHE_FS_INFO, "   file cache statistical information:");
        cache_fs_log(log_fp, CACHE_FS_INFO, "========================================");
        cache_fs_log(log_fp, CACHE_FS_INFO, " cache hit times       : %d", tmpCacheFile->cache_hits);
        cache_fs_log(log_fp, CACHE_FS_INFO, " cache discard times   : %d", tmpCacheFile->cache_discard_cnt);
        cache_fs_log(log_fp, CACHE_FS_INFO, " direct read times     : %d", tmpCacheFile->direct_read_cnt);
        cache_fs_log(log_fp, CACHE_FS_INFO, " total read times      : %d", tmpCacheFile->total_read_cnt);
        cache_fs_log(log_fp, CACHE_FS_INFO, " total read size       : %d", tmpCacheFile->total_read_size);
        cache_fs_log(log_fp, CACHE_FS_INFO, " average read size     : %d",
                     tmpCacheFile->total_read_cnt ? tmpCacheFile->total_read_size / tmpCacheFile->total_read_cnt : 0);
        cache_fs_log(log_fp, CACHE_FS_INFO, " total seek times      : %d", tmpCacheFile->total_seek_cnt);
        cache_fs_log(log_fp, CACHE_FS_INFO, "========================================");
        cache_fs_close_log(log_fp);
    }
#endif

    /* release cache buffer */
    if (tmpCacheFile->cache)
    {
        PHY_FREE(tmpCacheFile->cache);
        tmpCacheFile->cache = 0;
    }

    /* close true file handle */
    if (tmpCacheFile->fp)
    {
        if ((ret = esFSYS_fclose(tmpCacheFile->fp)) != EPDK_OK)
        {
            __wrn("eLIBs_fclose fail, [%d]", ret);
        }

        tmpCacheFile->fp = NULL;
    }

    return ret;
}

/*
*********************************************************************************************************
*                           READ CACHE ES_FILE DATA
*
* Description: close cache media file;
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
__s32 CACHE_afread(CacheFS *thiz, void *buf, __s32 size, __s32 count)
{
    __u32               tmpDataSize;    //还要读进buf的字节总数
    __u8                *tmpUsrPtr;     //当前在buf中写到的位置
    __s32               tmpCount;       //读到cache里面的字节长度
    __u32               tmpCopyCount;   //要拷贝的字节数， bytes
    __s32               tmpOffset;      //在文件中读到的位置距离靠近文件头的sector的偏差
    CacheFSAlign        *pCacheFSAlign = (CacheFSAlign *)thiz;
    __cache_afile_ptr_t *tmpCacheFile = &pCacheFSAlign->AFilePtr;

    if (!tmpCacheFile || !size || !count)
    {
        __wrn("File handle or parameter is invalid when read file!");
        return 0;
    }

    tmpDataSize = size * count;
    tmpUsrPtr = (__u8 *)buf;

    /* maybe aligned by sector size. */
    tmpOffset = (__s32)(tmpCacheFile->file_pos & (tmpCacheFile->sector_size - 1));

    if (tmpOffset || tmpCacheFile->cache_left)
    {
        if (tmpCacheFile->cache_left)
        {
            /* cache have data, copy left data directly. */
            tmpCopyCount = (tmpDataSize < tmpCacheFile->cache_left) ? tmpDataSize : tmpCacheFile->cache_left;
            memcpy(tmpUsrPtr, (tmpCacheFile->cache + tmpCacheFile->cache_offset), tmpCopyCount);
            /* update user buffer position */
            tmpDataSize -= tmpCopyCount;
            tmpUsrPtr   += tmpCopyCount;
            /* update file handle parameter */
            tmpCacheFile->cache_offset  += tmpCopyCount;
            tmpCacheFile->cache_left    -= tmpCopyCount;
#ifdef CONFIG_LAYERFS_CACHE_DEBUG
            /* update cache statistical parameters */
            if (tmpDataSize == 0)
            {
                /* cache hit one time */
                tmpCacheFile->cache_hits++;
            }
#endif
        }
        else
        {
            //要读的数据除包括一个未读的sector外，还包括大于等于1个的cachesize，那么在这里直接读取，读取完后就是sector对齐了
            if (tmpDataSize >= tmpCacheFile->sector_size - tmpOffset + tmpCacheFile->cache_size)
            {
                tmpCopyCount = (((tmpDataSize - (tmpCacheFile->sector_size - tmpOffset)) >> tmpCacheFile->cache_bits) << tmpCacheFile->cache_bits)
                               + (tmpCacheFile->sector_size - tmpOffset);
                tmpCount = esFSYS_fread(tmpUsrPtr, 1, tmpCopyCount, tmpCacheFile->fp);

                if (tmpCount < 0)
                {
                    __wrn("fread [%d] bytes, fatal error!", tmpCount);
                    return 0;
                }

                /* update user buffer position */
                tmpDataSize -= tmpCount;
                tmpUsrPtr   += tmpCount;
                /* update file handle parameter */
                tmpCacheFile->file_pos += tmpCount;
#ifdef CONFIG_LAYERFS_CACHE_DEBUG
                /* update cache statistical parameters */
                tmpCacheFile->direct_read_cnt++;
#endif
            }
            else
            {
                if (unlikely(tmpCacheFile->file_size < tmpCacheFile->sector_size))
                {
                    tmpOffset = 0;
                }

                /* adjust file position to aligned by sector. */
                if (eLIBs_fllseek(tmpCacheFile->fp, -tmpOffset, SEEK_CUR))
                {
                    /* seek file pointer failed, need not invalid cache. */
                    __wrn("fseek fail");
                    return 0;
                }

                /* use cache to read */
                tmpCount = esFSYS_fread(tmpCacheFile->cache,
                                        1,
                                        tmpCacheFile->cache_size,
                                        tmpCacheFile->fp);

                if (tmpCount < 0)
                {
                    __wrn("fread [%d] bytes, fatal error", tmpCount);
                    return 0;
                }

                if (tmpCount <= tmpOffset)
                {
                    /* read failed */
                    __wrn("fread [%d] bytes <= tmpOffset[%d], size[%d], count[%d],fatal error", tmpCount, tmpOffset, size, count);
                    return 0;
                }

                /* copy data to user buffer first */
                tmpCopyCount = (tmpDataSize < (tmpCount - tmpOffset)) ? tmpDataSize : (tmpCount - tmpOffset);
                memcpy(tmpUsrPtr, (tmpCacheFile->cache + tmpOffset), tmpCopyCount);
                /* update user buffer position. */
                tmpDataSize -= tmpCopyCount;
                tmpUsrPtr   += tmpCopyCount;
                /* update file handle parameter */
                tmpCacheFile->cache_offset  =  tmpOffset + tmpCopyCount;
                tmpCacheFile->cache_left    =  tmpCount - tmpOffset - tmpCopyCount;
                tmpCacheFile->file_pos      += (tmpCount - tmpOffset);
            }
        }
    }

    /* 经过上面的读取，对于剩下的要读进buf的字节数。先把整数个cache_size读进buf，如果大于1个的话. */
    /* 这样可以减少内存拷贝量，会有相当一部分数据是直接读进buf，而不是经过cachebuf中转，减少了一次内存拷贝，提高了效率 */
    if (tmpDataSize >> (tmpCacheFile->cache_bits))
    {
        /* read data size should aligned by sector size ??maybe aligned by cache size */
        tmpCopyCount = (tmpDataSize >> tmpCacheFile->sector_bits) << tmpCacheFile->sector_bits;
        /* use user buffer to read directly. */
        tmpCount = esFSYS_fread(tmpUsrPtr,
                                1,
                                tmpCopyCount,
                                tmpCacheFile->fp);

        if (tmpCount < 0)
        {
            __wrn("fread [%d] bytes, fatal error!", tmpCount);
            return 0;
        }

        /* update user buffer position */
        tmpDataSize -= tmpCount;
        tmpUsrPtr   += tmpCount;
        /* update file handle parameter */
        tmpCacheFile->file_pos += tmpCount;
#ifdef CONFIG_LAYERFS_CACHE_DEBUG
        /* update cache statistical parameters */
        tmpCacheFile->direct_read_cnt++;
#endif
    }

    /* 如果还没读完，那么剩下的要读取的字节数已经不到一个cachesize了，再读一个cachesize的大小，cachesize保证是sector的整数倍 */
    /* 这样就实现了以sector为单位的读取，加快读取速度 */
    if (tmpDataSize)
    {
        /* use cache to read */
        tmpCount = esFSYS_fread(tmpCacheFile->cache,
                                1,
                                tmpCacheFile->cache_size,
                                tmpCacheFile->fp);

        if (tmpCount < 0)
        {
            __wrn("fread [%d] bytes", tmpCount);
            return 0;
        }

        /* copy data to user buffer first. */
        tmpCopyCount = (tmpDataSize < tmpCount) ? tmpDataSize : tmpCount;
        memcpy(tmpUsrPtr, tmpCacheFile->cache, tmpCopyCount);
        /* update user buffer position. */
        tmpDataSize -= tmpCopyCount;
        tmpUsrPtr   += tmpCopyCount;//no use after this step
        /* update file handle parameter. */
        tmpCacheFile->cache_offset  =  tmpCopyCount;
        tmpCacheFile->cache_left    =  tmpCount - tmpCopyCount;
        tmpCacheFile->file_pos      += tmpCount;
    }

    /* 如果是文件末尾，那么可能还没装满buf，但已经没法再读取了 */
    /* caclute read items count */
    tmpCount = ((size * count) - tmpDataSize) / size;
#ifdef CONFIG_LAYERFS_CACHE_DEBUG
    cache_fs_log(tmpCacheFile->log_fp, CACHE_FS_FREAD,
                 "fp (%x), size (%d), count (%d)", tmpCacheFile->fp, size, count);
    /* update cache statistical parameters */
    tmpCacheFile->total_read_cnt++;
    tmpCacheFile->total_read_size += ((__u32)(size * count)) - tmpDataSize;
#endif
    return tmpCount;
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
__s32 CACHE_afwrite(CacheFS *thiz, void *buf, __s32 size, __s32 count)
{
    __wrn("when cache_fs_work_mode = CACHE_FS_WORK_MODE_SECTALIGN, CACHE_afwrite() is unsuccessful!");
    return 0;
}

/*
*********************************************************************************************************
*                           GET CACHE ES_FILE INFORMATION
*
* Description: get cache media file information;
*
* Arguments  : buf      buffer for store file information;
*
* Returns    : 0
*
* Note       : it will support stat function when open the cache media file.
*********************************************************************************************************
*/
__s32 CACHE_afstat(CacheFS *thiz, void *buf)
{
    __ES_FSTAT *stat_buf = (__ES_FSTAT *)buf;
    CacheFSAlign *pCacheFSAlign = (CacheFSAlign *)thiz;
    __cache_afile_ptr_t  *tmpCacheFile = &pCacheFSAlign->AFilePtr;

    stat_buf->pos = tmpCacheFile->file_pos - tmpCacheFile->cache_left;
    stat_buf->size = tmpCacheFile->file_size;
    return 0;
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
__s32 CACHE_afseek(CacheFS *thiz, __s64 offset, __s32 origin)   //fseek
{
    CacheFSAlign *pCacheFSAlign = (CacheFSAlign *)thiz;
    __cache_afile_ptr_t  *tmpCacheFile = &pCacheFSAlign->AFilePtr;
    __s64                tmpSeekOffset; //相对于用户以为读到的地方的偏移量
    __s64                tmpCPos;   //用户以为读到的地方，在文件中的映射

    if (!tmpCacheFile || !tmpCacheFile->fp)
    {
        __wrn("File handle is invalid when read data!");
        return 0;
    }

#ifdef CONFIG_LAYERFS_CACHE_DEBUG
    cache_fs_log(tmpCacheFile->log_fp, CACHE_FS_FSEEK,
                 "fp (%x), offset (%d%d), origin (%d)",
                 tmpCacheFile->fp, (__s32)(offset >> 32), (__s32)offset, origin);
    tmpCacheFile->total_seek_cnt++;
#endif
    tmpCPos = tmpCacheFile->file_pos - tmpCacheFile->cache_left;

    /* calcute seek offset and adjust file position */
    switch (origin)
    {
        case SEEK_CUR :
            tmpSeekOffset = offset;
            break;

        case SEEK_END :
            tmpSeekOffset = tmpCacheFile->file_size - tmpCPos + offset;
            break;

        case SEEK_SET :
            tmpSeekOffset = offset - tmpCPos;
            break;

        default      :
            __wrn("parameter is invalid when seek file pointer!");
            return -1;
    }

    if (((tmpCacheFile->cache_offset + tmpSeekOffset) < 0) ||
        (tmpCacheFile->cache_left <= tmpSeekOffset))    //若fseek的目的地超出了cache的范围，那么直接忽略cache
    {
        /* do actually fs seek operation */
        /* 向文件尾端fseek，那么就要计算真正的相对于文件已读到的位置的偏移量 */

        /* discard cache left data. */
        if (tmpCacheFile->cache_left)
        {
            /* 变为真正的相对于文件已读到的位置的偏移量 */
            tmpSeekOffset -= tmpCacheFile->cache_left;
#ifdef CONFIG_LAYERFS_CACHE_DEBUG
            cache_fs_log(tmpCacheFile->log_fp, CACHE_FS_WARNING,
                         "discard cache data, left len [%d]",
                         tmpCacheFile->cache_left);
            /* update cache discard time. */
            tmpCacheFile->cache_discard_cnt++;
#endif
        }

        if (tmpSeekOffset == 0)
        {
            return 0;
        }

        /* seek file pointer directly */
        if (eLIBs_fllseek(tmpCacheFile->fp, tmpSeekOffset, SEEK_CUR))
        {
            /* seek file pointer failed, need not invalid cache */
            __wrn("cache_afseek: fseek fail");
            return -1;
        }

        /* cache invalid */
        tmpCacheFile->cache_offset  = 0;
        tmpCacheFile->cache_left    = 0;
        /* update file position */
        tmpCacheFile->file_pos += tmpSeekOffset;
    }
    else    //若fseek的目的地没有超出范围
    {
        /* seek within cache */
        /* adjust cache paramaters */
        tmpCacheFile->cache_offset  += (__u32)(tmpSeekOffset);
        tmpCacheFile->cache_left    -= (__u32)(tmpSeekOffset);
    }

    return 0;
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
__s64 CACHE_aftell(CacheFS *thiz)
{
    CacheFSAlign *pCacheFSAlign = (CacheFSAlign *)thiz;
    __cache_afile_ptr_t  *tmpCacheFile = &pCacheFSAlign->AFilePtr;

    if (!tmpCacheFile || !tmpCacheFile->fp)
    {
        __wrn("File handle is invalid when tell file pointer!");
        return -1;
    }

#ifdef CONFIG_LAYERFS_CACHE_DEBUG
    cache_fs_log(tmpCacheFile->log_fp, CACHE_FS_FTELL,
                 "fp (%x), position (%d%d)", tmpCacheFile->fp,
                 (tmpCacheFile->file_pos - (__s64)(tmpCacheFile->cache_left)) >> 32,
                 (__u32)(tmpCacheFile->file_pos - (__s64)(tmpCacheFile->cache_left)));
#endif
    return (tmpCacheFile->file_pos - (__s64)(tmpCacheFile->cache_left));
}

/*
 * 适用于mode = CACHE_FS_WORK_MODE_SECTALIGN等有cache的模式
 */
__s32 CACHE_aSetCacheSize(CacheFS *thiz, __s32 cache_size)
{
    __u32   cache_bits = 0;
    CacheFSAlign *pCacheFSAlign = (CacheFSAlign *)thiz;
    pCacheFSAlign->AFilePtr.cache_size = cache_size;

    while (cache_size != 0)
    {
        cache_size = cache_size >> 1;
        cache_bits++;
    }

    pCacheFSAlign->AFilePtr.cache_bits = cache_bits - 1;
    return EPDK_OK;
}

/* 得到真正的文件指针 */
ES_FILE *CACHE_aGetFp(CacheFS *thiz)
{
    CacheFSAlign *pCacheFSAlign = (CacheFSAlign *)thiz;
    __cache_afile_ptr_t  *tmpCacheFile = &pCacheFSAlign->AFilePtr;
    return tmpCacheFile->fp;
}

CacheFSAlign *newCacheFSAlign()
{
    CacheFSAlign *pFSAlign = (CacheFSAlign *)malloc(sizeof(CacheFSAlign));

    if (NULL == pFSAlign)
    {
        __wrn("NewCacheFSAlign malloc fail");
        return NULL;
    }

    memset(pFSAlign, 0, sizeof(CacheFSAlign));
    CacheFS_Init((CacheFS *)pFSAlign);

    pFSAlign->CacheFSBase.cache_fs_work_mode = CACHE_FS_WORK_MODE_SECTALIGN;
    pFSAlign->CacheFSBase.cachefs_fopen = CACHE_afopen;
    pFSAlign->CacheFSBase.cachefs_fclose = CACHE_afclose;
    pFSAlign->CacheFSBase.cachefs_fread = CACHE_afread;
    pFSAlign->CacheFSBase.cachefs_fwrite = CACHE_afwrite;
    pFSAlign->CacheFSBase.cachefs_fseek = CACHE_afseek;
    pFSAlign->CacheFSBase.cachefs_ftell = CACHE_aftell;
    pFSAlign->CacheFSBase.SetCacheSize = CACHE_aSetCacheSize;
    pFSAlign->CacheFSBase.GetFp = CACHE_aGetFp;
    pFSAlign->CacheFSBase.cachefs_fstat = CACHE_afstat;

    memset(&pFSAlign->AFilePtr, 0, sizeof(__cache_afile_ptr_t));
    return pFSAlign;
}

/*******************************************************************************
Function name: deleteCacheFSAlign
Description:
    1.默认在fclose之后调用，所以不检查了，直接释放内存
Parameters:

Return:

Time: 2011/2/20
*******************************************************************************/
void deleteCacheFSAlign(CacheFSAlign *pCacheFSAlign)
{
    free(pCacheFSAlign);
    return;
}
