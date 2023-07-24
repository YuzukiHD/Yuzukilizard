/*
*********************************************************************************************************
*                                                    eMOD
*                                   the Easy Portable/Player Operation System
*                                              mod_mmp sub-system
*
*                                    (c) Copyright 2008-2009, Kevin.Z China
*                                              All Rights Reserved
*
* File   : cache_fs_normal.c
* Version: V1.0
* By     : kevin
* Date   : 2009-2-2 11:38
* Modify : zengzhijin 2020-04-24
* Description:
    普通fs缓存模式，因为没有考虑扇区对齐,所以读取方面会慢很多。目前基本放弃
    使用了。
*********************************************************************************************************
*/
#include "cache_fs_s.h"
#include "cache_fs_normal.h"
#include <stdlib.h>
#include <string.h>
#include <log.h>

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
ES_FILE *CACHE_nfopen(CacheFS *thiz, const __s8 *path, const __s8 *mode)   //打开文件,返回的是真正的文件指针
{
    CacheFSNormal *pFSNormal = (CacheFSNormal *)thiz;
    __cache_nfile_ptr_t  *tmpCacheFile = &pFSNormal->NFilePtr;

    if (!path || !mode)
    {
        __wrn("Parameter for open cache file is invalid!\n");
        goto _err0_cache_fopen;
    }

    tmpCacheFile->fp = esFSYS_fopen(path, mode);

    if (!tmpCacheFile->fp)
    {
        __wrn("Open cache file failed!\n");
        goto _err1_cache_fopen;
    }

    /* request memory for cache file data */
    tmpCacheFile->cache = PHY_MALLOC(tmpCacheFile->buf_size, 1024);

    if (!tmpCacheFile->cache)
    {
        __wrn("Request memory for cache fs cache failed!\n");
        goto _err2_cache_fopen;
    }

    /* tmpCacheFile->buf_size = CACHE_FS_CACHE_SIZE; */
    tmpCacheFile->file_pst  = eLIBs_ftell(tmpCacheFile->fp);  //一般是0
    tmpCacheFile->cache_pst = tmpCacheFile->file_pst;
    tmpCacheFile->cur_ptr   = tmpCacheFile->cache;
    tmpCacheFile->left_len  = 0;
    eLIBs_fseek(tmpCacheFile->fp, 0, SEEK_END);
    tmpCacheFile->file_size = eLIBs_ftell(tmpCacheFile->fp);
    eLIBs_fseek(tmpCacheFile->fp, tmpCacheFile->file_pst, SEEK_SET);

    return (ES_FILE *)tmpCacheFile->fp;

_err2_cache_fopen:
    esFSYS_fclose(tmpCacheFile->fp);
_err1_cache_fopen:
_err0_cache_fopen:
    return 0;
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
__s32 CACHE_nfclose(CacheFS *thiz)
{
    CacheFSNormal *pFSNormal = (CacheFSNormal *)thiz;
    __cache_nfile_ptr_t  *tmpCacheFile = &pFSNormal->NFilePtr;
    int   ret = EPDK_OK;

    if (!tmpCacheFile->fp)
    {
        __wrn("file handle is invalid when close cache file!\n");
        return -1;
    }

    /* release cache buffer */
    if (tmpCacheFile->cache)
    {
        PHY_FREE(tmpCacheFile->cache);
        tmpCacheFile->cache = NULL;
    }

    /* close file handle */
    if (tmpCacheFile->fp)
    {
        if ((ret = esFSYS_fclose(tmpCacheFile->fp)) != EPDK_OK)
        {
            __wrn("esFSYS_fclose FAIL, [%d]\n", ret);
        }

        tmpCacheFile->fp = 0;
    }

    return ret;
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
__s32 CACHE_nfwrite(CacheFS *thiz, void *buf, __s32 size, __s32 count)
{
    __wrn("when cache_fs_work_mode = CACHE_FS_WORK_MODE_BYTEALIGN, CACHE_nfwrite() is unsuccessful!\n");
    return 0;
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
__s32 CACHE_nfread(CacheFS *thiz, void *buf, __s32 size, __s32 count)   //读文件
{
    CacheFSNormal *pFSNormal = (CacheFSNormal *)thiz;
    __cache_nfile_ptr_t  *tmpCacheFile = &pFSNormal->NFilePtr;
    __s32               tmpDataSize;    //还要读取的字节数
    __s32               tmpCount;
    __u8                *tmpUsrPtr; //在buf的当前的位置

    if (!tmpCacheFile->fp || !size || !count)
    {
        __wrn("File handle or parameter is invalid when read file!\n");
        return 0;
    }

    tmpDataSize = size * count;

    //如果要读的字节数超出left_len和cache_size的总和，直接读取
    if (tmpDataSize >= tmpCacheFile->left_len + tmpCacheFile->buf_size)
    {
        tmpUsrPtr = (__u8 *)buf;

        //read left cache
        if (tmpCacheFile->left_len)
        {
            memcpy(tmpUsrPtr, tmpCacheFile->cur_ptr, tmpCacheFile->left_len);
            tmpUsrPtr += tmpCacheFile->left_len;
            tmpDataSize -= tmpCacheFile->left_len;
        }

        //flush file data cache
        tmpCacheFile->cur_ptr = tmpCacheFile->cache;
        tmpCacheFile->cache_pst = tmpCacheFile->file_pst;
        tmpCacheFile->left_len = 0;
        //read file data to user buffer derectly.
        tmpCount = esFSYS_fread(tmpUsrPtr, 1, tmpDataSize, tmpCacheFile->fp);

        if (tmpCount < 0)
        {
            __wrn("fread [%d] bytes\n", tmpCount);
            return 0;
        }

        //set file position and cache file postion
        tmpCacheFile->file_pst = eLIBs_ftell(tmpCacheFile->fp);
        tmpCacheFile->cache_pst = tmpCacheFile->file_pst;
        tmpDataSize -= tmpCount;//还有多少字节没有读。这里一般是0
        tmpCount = (size * count - tmpDataSize) / size;
        return tmpCount;
    }

    //需要使用cache了，可以确定再读64k字节，就一定可以读完要读的字节数
    tmpUsrPtr = (__u8 *)buf;

    //check if data is enough for user, or, need read data from file
    //如果cache里现有的字节数不够，那么先读完,再读满cache，或者读完文件
    if (tmpCacheFile->left_len < tmpDataSize)
    {
        if (tmpCacheFile->left_len)
        {
            //copy the left data to user buffer first
            memcpy(tmpUsrPtr, tmpCacheFile->cur_ptr, tmpCacheFile->left_len);
            tmpUsrPtr += tmpCacheFile->left_len;
            tmpDataSize -= tmpCacheFile->left_len;
        }

        //flush file data cache
        tmpCacheFile->cur_ptr = tmpCacheFile->cache;
        tmpCacheFile->cache_pst = tmpCacheFile->file_pst;
        tmpCacheFile->left_len = 0;

        //read file data to buffer
        if (tmpCacheFile->file_size - tmpCacheFile->file_pst < tmpCacheFile->buf_size)
        {
            tmpCount = esFSYS_fread(tmpCacheFile->cache,
                                    1,
                                    tmpCacheFile->file_size - tmpCacheFile->file_pst,
                                    tmpCacheFile->fp);
        }
        else
        {
            tmpCount = esFSYS_fread(tmpCacheFile->cache,
                                    1,
                                    tmpCacheFile->buf_size,
                                    tmpCacheFile->fp);
        }

        if (tmpCount < 0)
        {
            __wrn("fread [%d] bytes\n", tmpCount);
            return 0;
        }

        //update file handle parameter
        tmpCacheFile->file_pst += tmpCount;
        tmpCacheFile->left_len = tmpCount;
    }

    //现在cache里的数据一定足够读取了，除非文件已经读完
    //copy data to user buffer
    if (tmpCacheFile->left_len >= tmpDataSize)
    {
        //copy the data to user buffer
        memcpy(tmpUsrPtr, tmpCacheFile->cur_ptr, tmpDataSize);
        //update file handle parameter
        tmpCacheFile->cache_pst  += tmpDataSize;
        tmpCacheFile->cur_ptr    += tmpDataSize;
        tmpCacheFile->left_len   -= tmpDataSize;
        tmpCount = count;
    }
    else    //这种情况就是文件读完了，数据不够才会遇到
    {
        if (tmpCacheFile->left_len)
        {
            //copy the data to user buffer
            memcpy(tmpUsrPtr, tmpCacheFile->cur_ptr, tmpCacheFile->left_len);
            //update file handle parameter
            tmpCacheFile->cache_pst  = tmpCacheFile->file_pst;
            tmpCacheFile->cur_ptr    = tmpCacheFile->cache;
            tmpDataSize -= tmpCacheFile->left_len;//还有多少字节没有读。
            tmpCacheFile->left_len   = 0;
        }

        tmpCount = ((size * count) - tmpDataSize) / size;
    }

    return tmpCount;
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
__s32 CACHE_nfseek(CacheFS *thiz, __s64 offset, __s32 origin)   //fseek
{
    CacheFSNormal *pFSNormal = (CacheFSNormal *)thiz;
    __cache_nfile_ptr_t  *tmpCacheFile = &pFSNormal->NFilePtr;
    __s64   tmpCPos;    //用户在cache读到的地方在文件中的映射位置
    __s64   tmpSeekOffset;  //fseek的目的地相对于用户在cahce里读到的地方的偏移量
    __s64   cacheOffset;    //当前读到的位置在cache中的偏移量

    if (!tmpCacheFile->fp)
    {
        __wrn("File handle is invalid when read data!\n");
        return 0;
    }

    tmpCPos = tmpCacheFile->file_pst - tmpCacheFile->left_len;

    /* calcute seek offset and adjust file position */
    switch (origin)
    {
        case SEEK_CUR :
            tmpSeekOffset = offset;
            break;

        case SEEK_END : /* 不管SEEK_END,SEEK_SET，offset为负都表示向文件头靠近 */
            tmpSeekOffset = tmpCacheFile->file_size - tmpCPos + offset;
            break;

        case SEEK_SET :
            tmpSeekOffset = offset - tmpCPos;
            break;

        default      :
            __wrn("parameter is invalid when seek file pointer!\n");
            return -1;
    }

    /* 判断fseek的目的地是否超出了cache的范围 */
    cacheOffset = (__s64)(tmpCacheFile->cur_ptr - tmpCacheFile->cache);

    if (cacheOffset + tmpSeekOffset < 0 || tmpCacheFile->left_len <= tmpSeekOffset) /* 超出了cache范围 */
    {
        if (tmpCacheFile->left_len > 0)
        {
            tmpSeekOffset -= tmpCacheFile->left_len;    /* 把tmpSeekOffset变为真正的相对于文件指针的偏移量 */
        }

        //seek file pointer directly
        if (eLIBs_fseek(tmpCacheFile->fp, tmpSeekOffset, SEEK_CUR))
        {
            /* seek file pointer failed, need not invalid cache */
            __wrn("cache_nfseek: fseek fail\n");
            return -1;
        }

        /* 重新配置cache的描述变量 */
        tmpCacheFile->cur_ptr = tmpCacheFile->cache;
        tmpCacheFile->left_len = 0;
        tmpCacheFile->file_pst += tmpSeekOffset;
        tmpCacheFile->cache_pst = tmpCacheFile->file_pst;
    }
    else
    {
        tmpCacheFile->cur_ptr += tmpSeekOffset;
        tmpCacheFile->left_len -= tmpSeekOffset;
        tmpCacheFile->cache_pst +=  tmpSeekOffset;
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
__s64 CACHE_nftell(CacheFS *thiz)
{
    CacheFSNormal *pFSNormal = (CacheFSNormal *)thiz;
    __cache_nfile_ptr_t  *tmpCacheFile = &pFSNormal->NFilePtr;

    if (!tmpCacheFile)
    {
        __wrn("File handle is invalid when tell file pointer!\n");
        return -1;
    }

    return tmpCacheFile->cache_pst;
}

/* 适用于mode = CACHE_FS_WORK_MODE_SECTALIGN等有cache的模式 */
__s32 CACHE_nSetCacheSize(CacheFS *thiz, __s32 cache_size)
{
    CacheFSNormal *pCacheFSNormal = (CacheFSNormal *)thiz;
    pCacheFSNormal->NFilePtr.buf_size = cache_size;
    return EPDK_OK;
}

/* 得到真正的文件指针 */
ES_FILE *CACHE_nGetFp(CacheFS *thiz)
{
    CacheFSNormal *pCacheFSNormal = (CacheFSNormal *)thiz;
    __cache_nfile_ptr_t  *tmpCacheFile = &pCacheFSNormal->NFilePtr;
    return tmpCacheFile->fp;
}

static __s32 CACHE_nfstat(CacheFS *thiz, void *buf)
{
    __ES_FSTAT *stat_buf = (__ES_FSTAT *)buf;
    CacheFSNormal *pCacheFSNormal = (CacheFSNormal *)thiz;
    __cache_nfile_ptr_t  *tmpCacheFile = &pCacheFSNormal->NFilePtr;

    stat_buf->pos = tmpCacheFile->file_pst - tmpCacheFile->left_len;
    stat_buf->size = tmpCacheFile->file_size;
    return 0;
}

CacheFSNormal *newCacheFSNormal()
{
    CacheFSNormal *pFSNormal = (CacheFSNormal *)malloc(sizeof(CacheFSNormal));

    if (NULL == pFSNormal)
    {
        __wrn("NewCacheFSNormal malloc fail\n");
        return NULL;
    }

    memset(pFSNormal, 0, sizeof(CacheFSNormal));
    CacheFS_Init((CacheFS *)pFSNormal);

    pFSNormal->CacheFSBase.cache_fs_work_mode = CACHE_FS_WORK_MODE_BYTEALIGN;
    pFSNormal->CacheFSBase.cachefs_fopen = CACHE_nfopen;
    pFSNormal->CacheFSBase.cachefs_fclose = CACHE_nfclose;
    pFSNormal->CacheFSBase.cachefs_fread = CACHE_nfread;
    pFSNormal->CacheFSBase.cachefs_fwrite = CACHE_nfwrite;
    pFSNormal->CacheFSBase.cachefs_fseek = CACHE_nfseek;
    pFSNormal->CacheFSBase.cachefs_ftell = CACHE_nftell;
    pFSNormal->CacheFSBase.cachefs_fstat = CACHE_nfstat;
    pFSNormal->CacheFSBase.SetCacheSize = CACHE_nSetCacheSize;
    pFSNormal->CacheFSBase.GetFp = CACHE_nGetFp;

    memset(&pFSNormal->NFilePtr, 0, sizeof(__cache_nfile_ptr_t));
    return pFSNormal;
}

void deleteCacheFSNormal(CacheFSNormal *pCacheFSNormal)
{
    free(pCacheFSNormal);
    return;
}
