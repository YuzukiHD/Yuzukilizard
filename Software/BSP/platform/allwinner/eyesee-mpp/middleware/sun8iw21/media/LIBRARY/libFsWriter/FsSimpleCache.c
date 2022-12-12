/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : FsSimpleCache.c
* Version: V1.0
* By     : eric_wang
* Date   : 2014-12-17
* Description:
    simple cache write file mode. Don't create writeThread to write, cache 
data to 64K to write, to guarantee write block.
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "FsSimpleCache"
#include <utils/plat_log.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <tsemaphore.h>
#include <SystemBase.h>
#include <FsWriter.h>
//#include <ConfigOption.h>

//#define SHOW_FWRITE_TIME

typedef struct tag_FsSimpleCacheContext
{
//    FILE        *mpFile;
//    int         mFd;
    struct cdx_stream_info *mpStream;
    char      *mpCache;
    size_t     mCacheSize;
    size_t     mValidLen;
}FsSimpleCacheContext;

static ssize_t FsSimpleCacheWrite(FsWriter *thiz, const char *buf, size_t size)
{
    FsSimpleCacheContext *pCtx = (FsSimpleCacheContext*)thiz->mPriv;
    size_t nLeftSize;
    if(0 == size || NULL == buf)
    {
        aloge("Invalid input paramter!");
        return -1;
    }
//	if(size > pCtx->mCacheSize)
//	{
//    	ALOGD("(f:%s, l:%d)write[%d]KB[%d]bytes!", size/1024, size%1024);
//	}
    if(pCtx->mValidLen>=pCtx->mCacheSize)
    {
        aloge("fatal error! [%d]>=[%d], check code!", pCtx->mValidLen, pCtx->mCacheSize);
        return -1;
    }
    if(pCtx->mValidLen>0)
    {
        if(pCtx->mValidLen + size <= pCtx->mCacheSize)
        {
            memcpy(pCtx->mpCache+pCtx->mValidLen, buf, size);
            pCtx->mValidLen+=size;
            if(pCtx->mValidLen==pCtx->mCacheSize)
            {
                fileWriter(pCtx->mpStream, pCtx->mpCache, pCtx->mCacheSize);
                pCtx->mValidLen = 0;
            }
            return size;
        }
        else
        {
            size_t nSize0 = pCtx->mCacheSize - pCtx->mValidLen;
            nLeftSize = size - nSize0;
            memcpy(pCtx->mpCache+pCtx->mValidLen, buf, nSize0);
            fileWriter(pCtx->mpStream, pCtx->mpCache, pCtx->mCacheSize);
            pCtx->mValidLen = 0;
        }
    }
    else
    {
        nLeftSize = size;
    }

    if(nLeftSize>=pCtx->mCacheSize)
    {
        size_t nWriteSize = (nLeftSize/pCtx->mCacheSize)*pCtx->mCacheSize;
        alogv("[%d], direct fwrite[%d]kB!", size-nLeftSize, nLeftSize/1024);
        fileWriter(pCtx->mpStream, buf+(size-nLeftSize), nWriteSize);
        nLeftSize -= nWriteSize;
    }
    if(nLeftSize>0)
    {
        //ALOGD("(f:%s, l:%d)need cache!");
        memcpy(pCtx->mpCache, buf+(size-nLeftSize), nLeftSize);
        pCtx->mValidLen = nLeftSize;
    }
    return size;
}

static int FsSimpleCacheSeek(FsWriter *thiz, int64_t nOffset, int fromWhere)
{
    FsSimpleCacheContext *pCtx = (FsSimpleCacheContext*)thiz->mPriv;
    int ret;
    thiz->fsFlush(thiz);
    ret = pCtx->mpStream->seek(pCtx->mpStream, nOffset, fromWhere);
    return ret;
}

static int64_t FsSimpleCacheTell(FsWriter *thiz)
{
    FsSimpleCacheContext *pCtx = (FsSimpleCacheContext*)thiz->mPriv;
    int64_t nFilePos;
    thiz->fsFlush(thiz);
    nFilePos = pCtx->mpStream->tell(pCtx->mpStream);
    return nFilePos;
}

static int FsSimpleCacheTruncate(FsWriter *thiz, int64_t nLength)
{
    int ret;
    FsSimpleCacheContext *pCtx = (FsSimpleCacheContext*)thiz->mPriv;
    thiz->fsFlush(thiz);
    ret = pCtx->mpStream->truncate(pCtx->mpStream, nLength);
    return ret;
}

static int FsSimpleCacheFlush(FsWriter *thiz)
{
    FsSimpleCacheContext *pCtx = (FsSimpleCacheContext*)thiz->mPriv;
    if(pCtx->mValidLen > 0)
    {
        fileWriter(pCtx->mpStream, pCtx->mpCache, pCtx->mValidLen);
        pCtx->mValidLen = 0;
    }
    return 0;
}

FsWriter *initFsSimpleCache(struct cdx_stream_info *pStream, int nCacheSize)
{
	if(nCacheSize<=0)
	{
        aloge("param[%d] wrong![%s]", nCacheSize, strerror(errno));
		return NULL;
	}
    FsWriter *pFsWriter = (FsWriter*)malloc(sizeof(FsWriter));
	if (NULL == pFsWriter) 
    {
        aloge("Failed to alloc FsWriter(%s)", strerror(errno));
		return NULL;
	}
    memset(pFsWriter, 0, sizeof(FsWriter));
    FsSimpleCacheContext *pContext = (FsSimpleCacheContext*)malloc(sizeof(FsSimpleCacheContext));
	if (NULL == pContext) {
        aloge("Failed to alloc FsSimpleCacheContext(%s)", strerror(errno));
		goto ERROR0;
	}
    memset(pContext, 0, sizeof(FsSimpleCacheContext));
    pContext->mpStream = pStream;
//    int ret = posix_memalign((void **)&pContext->mpCache, 4096, nCacheSize);
//    if(ret!=0)
//    {
//        aloge("fatal error! malloc [%d]kByte fail.", nCacheSize/1024);
//        goto ERROR1;
//    }
    pContext->mpCache = malloc(nCacheSize);
    if(NULL == pContext->mpCache)
    {
        aloge("fatal error! malloc [%d]kByte fail.", nCacheSize/1024);
        goto ERROR1;
    }
    pContext->mValidLen = 0;
    pContext->mCacheSize = nCacheSize;
	pFsWriter->fsWrite = FsSimpleCacheWrite;
    pFsWriter->fsSeek = FsSimpleCacheSeek;
    pFsWriter->fsTell = FsSimpleCacheTell;
    pFsWriter->fsTruncate = FsSimpleCacheTruncate;
	pFsWriter->fsFlush = FsSimpleCacheFlush;
	pFsWriter->mMode = FSWRITEMODE_SIMPLECACHE;
    pFsWriter->mPriv = (void*)pContext;
    return pFsWriter;

ERROR1:
	free(pContext);
ERROR0:
	free(pFsWriter);
    return NULL;
}

int deinitFsSimpleCache(FsWriter *pFsWriter)
{
	if (NULL == pFsWriter) {
        aloge("pFsWriter is NULL!!");
		return -1;
	}
	FsSimpleCacheContext *pContext = (FsSimpleCacheContext*)pFsWriter->mPriv;
	if (NULL == pContext) {
        aloge("pContext is NULL!!");
		return -1;
	}
    pFsWriter->fsFlush(pFsWriter);
    if(pContext->mpCache)
    {
        free(pContext->mpCache);
        pContext->mpCache = NULL;
    }
	free(pContext);
	free(pFsWriter);
    return 0;
}

