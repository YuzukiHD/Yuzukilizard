/*
********************************************************************************
*
*          (c) Copyright 2010-2013, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : FsCache.c
* Version: V1.0
* By     : Eric_wang
* Date   : 2013-8-4
* Description:
    use noLock machanism for fread and fwrite, to speed up as quickly as possible.
    So fwrite (nCacheSize-1) at most.
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "FsCache"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <tsemaphore.h>
#include <SystemBase.h>
#include <FsWriter.h>
//#include <ConfigOption.h>

#define WRITE_BLOCK_SIZE  (128*1024)  //unit:byte.

typedef struct tag_FsCacheThreadContext
{
//    FILE        *mpFile;
//    int         mFd;
    struct cdx_stream_info *mpStream;
    char      *mpCache;
    size_t     mCacheSize;
    pthread_t   mThreadId;
    volatile int    mThreadExitFlag;

    volatile char     *mWritePtr;     //fwrite modify it, thread read it.
    volatile char     *mReadPtr;      //fwrite read it, thread modify it.
    cdx_sem_t           mWriteStartSem; //fwrite notify thread to start to write data to fs.
    int             mFlushFlag;     //fwrite modify it to 1, thread read it and clear it to 0.
    pthread_mutex_t     mFlushLock;     //work with mFlushFlag
    cdx_sem_t           mFlushDoneSem;  //work with mFlushFlag, thread notify to fwrite flush done!
    cdx_sem_t           mWriteDoneSem;
	unsigned int             mVideoCodecId;
}FsCacheThreadContext;

static ssize_t FsCacheThreadWrite(FsWriter *thiz, const char *buf, size_t size)
{
    FsCacheThreadContext *pCtx = (FsCacheThreadContext*)thiz->mPriv;
    char *pRdPtr;
    char *pWtPtr;
    size_t nFreeSize;
    size_t nFreeSizeSection1;
    size_t nFreeSizeSection2;

    if(0 == size)
    {
        return 0;
    }
    while(1)
    {
        pRdPtr = (char*)pCtx->mReadPtr;
        pWtPtr = (char*)pCtx->mWritePtr;
        if(pWtPtr >= pRdPtr)
        {
            nFreeSizeSection1 = pRdPtr - pCtx->mpCache;
            nFreeSizeSection2 = pCtx->mpCache+pCtx->mCacheSize-pWtPtr;
            if(nFreeSizeSection1>0)
            {
                nFreeSizeSection1-=1;
            }
            else if(nFreeSizeSection2>0)
            {
                nFreeSizeSection2-=1;
            }
            else
            {
                aloge("fatal error! at lease left one byte! check code!");
            }

            nFreeSize = nFreeSizeSection1+nFreeSizeSection2;
            if(nFreeSize>=size)
            {
                if(nFreeSizeSection2 >= size)
                {
                    memcpy(pWtPtr, buf, size);
                    pWtPtr+=size;
                    if(pWtPtr == pCtx->mpCache+pCtx->mCacheSize)
                    {
                        pWtPtr = pCtx->mpCache;
                    }
                    else if(pWtPtr > pCtx->mpCache+pCtx->mCacheSize)
                    {
                        aloge("fatal error! check code!");
                    }
                }
                else
                {
                    memcpy(pWtPtr, buf, nFreeSizeSection2);
                    memcpy(pCtx->mpCache, buf+nFreeSizeSection2, size-nFreeSizeSection2);
                    pWtPtr = pCtx->mpCache+size-nFreeSizeSection2;
                }
                pCtx->mWritePtr = pWtPtr;
                break;
            }
            else
            {
				int64_t tm1, tm2;
				alogd("codecID[%d], nFreeSize[%d]<size[%d], wait begin", pCtx->mVideoCodecId, nFreeSize, size);
                tm1 = CDX_GetSysTimeUsMonotonic();
                cdx_sem_up_unique(&pCtx->mWriteStartSem);
				cdx_sem_wait(&pCtx->mWriteDoneSem);
                tm2 = CDX_GetSysTimeUsMonotonic();
				alogd("codecID[%d], nFreeSize[%d]<size[%d], wait end [%lld]ms", pCtx->mVideoCodecId, nFreeSize, size, (tm2-tm1)/1000);
            }
        }
        else    //pWtPtr < pRdPtr
        {
            nFreeSize = pRdPtr-pWtPtr - 1;
            if(nFreeSize>=size)
            {
                memcpy(pWtPtr, buf, size);
                pWtPtr+=size;
                pCtx->mWritePtr = pWtPtr;
                break;
            }
            else
            {
				int64_t tm1, tm2;
				alogd("codecID[%d], nFreeSize[%d]<size[%d], wait begin", pCtx->mVideoCodecId, nFreeSize, size);
                tm1 = CDX_GetSysTimeUsMonotonic();
		        cdx_sem_up_unique(&pCtx->mWriteStartSem);
				cdx_sem_wait(&pCtx->mWriteDoneSem);
                tm2 = CDX_GetSysTimeUsMonotonic();
				alogd("codecID[%d], nFreeSize[%d]<size[%d], wait end [%lld]ms", pCtx->mVideoCodecId, nFreeSize, size, (tm2-tm1)/1000);
            }
        }
    }
    if(pCtx->mCacheSize - (nFreeSize+1-size) >= WRITE_BLOCK_SIZE)
    {
        cdx_sem_up_unique(&pCtx->mWriteStartSem);
//        if(0 == cdx_sem_get_val(&pCtx->mWriteStartSem))
//        {
//            cdx_sem_up(&pCtx->mWriteStartSem);
//        }

    }
    return size;
}

static int FsCacheThreadSeek(FsWriter *thiz, int64_t nOffset, int fromWhere)
{
    FsCacheThreadContext *pCtx = (FsCacheThreadContext*)thiz->mPriv;
    int ret;
    thiz->fsFlush(thiz);
    ret = pCtx->mpStream->seek(pCtx->mpStream, nOffset, fromWhere);
    return ret;
}

static int64_t FsCacheThreadTell(FsWriter *thiz)
{
    FsCacheThreadContext *pCtx = (FsCacheThreadContext*)thiz->mPriv;
    int64_t nFilePos;
    thiz->fsFlush(thiz);
    nFilePos = pCtx->mpStream->tell(pCtx->mpStream);

    return nFilePos;
}

static int FsCacheThreadTruncate(FsWriter *thiz, int64_t nLength)
{
    int ret;
    FsCacheThreadContext *pCtx = (FsCacheThreadContext*)thiz->mPriv;
    thiz->fsFlush(thiz);
    ret = pCtx->mpStream->truncate(pCtx->mpStream, nLength);
    return ret;
}

static int FsCacheThreadFlush(FsWriter *thiz)
{
    FsCacheThreadContext *pCtx = (FsCacheThreadContext*)thiz->mPriv;
    char *pRdPtr = (char*)pCtx->mReadPtr;
    char *pWtPtr = (char*)pCtx->mWritePtr;
    if(pRdPtr == pWtPtr)
    {
        return 0;
    }

    cdx_mutex_lock(&pCtx->mFlushLock);
    pCtx->mFlushFlag = 1;
    cdx_sem_up_unique(&pCtx->mWriteStartSem);
    cdx_mutex_unlock(&pCtx->mFlushLock);
    cdx_sem_down(&pCtx->mFlushDoneSem);
    pRdPtr = (char*)pCtx->mReadPtr;
    pWtPtr = (char*)pCtx->mWritePtr;
    if(pRdPtr != pWtPtr)
    {
        aloge("fatal error! Flush fail[%p][%p]?", pRdPtr, pWtPtr);
        return -1;
    }
	alogd("mpCache=%p, mReadPtr=%p, mWritePtr=%p, mCacheSize=%d", pCtx->mpCache, pCtx->mReadPtr, pCtx->mWritePtr, pCtx->mCacheSize);
    return 0;
}

static void* FsCacheWriteThread(void* pThreadData)
{
    //int ret = 0;
	FsCacheThreadContext *pCtx = (FsCacheThreadContext *)pThreadData;
    char *pRdPtr;
    char *pWtPtr;
    size_t nValidSize;
    size_t nValidSizeSection1;
    size_t nValidSizeSection2;
    size_t nWriteBlockNum;

    alogv("FsCacheWriteThread[%d] started", pCtx->mThreadId);
    while (1) 
    {
		cdx_sem_down(&pCtx->mWriteStartSem);
        //(1) detect basic info before start to fwrite().
        pRdPtr = (char*)pCtx->mReadPtr;
        pWtPtr = (char*)pCtx->mWritePtr;
        if(pWtPtr >= pRdPtr)
        {
            nValidSize = pWtPtr - pRdPtr;
            
        }
        else    //pWtPtr < pRdPtr
        {
            nValidSizeSection1 = pWtPtr - pCtx->mpCache;
            nValidSizeSection2 = pCtx->mpCache + pCtx->mCacheSize-pRdPtr;
            nValidSize = nValidSizeSection1 + nValidSizeSection2;
        }
        nWriteBlockNum = nValidSize/WRITE_BLOCK_SIZE;
        if(nWriteBlockNum>0)
        {
            if(nWriteBlockNum>1)
            {
                alogv("nValidSize[%d]kB, nWriteBlockNum[%d], maybe write slow?", nValidSize/1024, nWriteBlockNum);
            }
        }
        else
        {
            alogv("nValidSize[%d]kB is not enough!", nValidSize/1024);
        }
        //(2) start to fwrite()
        while(1)
        {
            pRdPtr = (char*)pCtx->mReadPtr;
            pWtPtr = (char*)pCtx->mWritePtr;
            if(pWtPtr >= pRdPtr)
            {
                nValidSize = pWtPtr - pRdPtr;
                nWriteBlockNum = nValidSize/WRITE_BLOCK_SIZE;
                if(nWriteBlockNum>0)
                {
                    //nWriteBlockNum = 1;
                    fileWriter(pCtx->mpStream, pRdPtr, WRITE_BLOCK_SIZE*nWriteBlockNum);
                    pRdPtr += WRITE_BLOCK_SIZE*nWriteBlockNum;
                    pCtx->mReadPtr = pRdPtr;
					cdx_sem_signal(&pCtx->mWriteDoneSem);
                }
                else
                {
                    break;
                }
            }
            else    //pWtPtr < pRdPtr
            {
                nValidSizeSection1 = pWtPtr - pCtx->mpCache;
                nValidSizeSection2 = pCtx->mpCache + pCtx->mCacheSize-pRdPtr;
                nValidSize = nValidSizeSection1 + nValidSizeSection2;
                nWriteBlockNum = nValidSizeSection2/WRITE_BLOCK_SIZE;
                if(nWriteBlockNum>0)
                {
                    //nWriteBlockNum = 1;
                    fileWriter(pCtx->mpStream, pRdPtr, WRITE_BLOCK_SIZE*nWriteBlockNum);
                    pRdPtr += WRITE_BLOCK_SIZE*nWriteBlockNum;
                    if(pRdPtr == pCtx->mpCache + pCtx->mCacheSize)
                    {
                        pRdPtr = pCtx->mpCache;
                    }
                    else if(pRdPtr > pCtx->mpCache + pCtx->mCacheSize)
                    {
                        aloge("fatal error! cache overflow![%p][%p][%d]", pRdPtr, pCtx->mpCache, pCtx->mCacheSize);
                    }
                    pCtx->mReadPtr = pRdPtr;
                }
                else
                {
                    aloge("fatal error! Cache status has something wrong, need check[%p][%p][%p][%d][%d]", 
                        pRdPtr, pWtPtr, pCtx->mpCache, pCtx->mCacheSize, nValidSizeSection2);
                    fileWriter(pCtx->mpStream, pRdPtr, nValidSizeSection2);
                    pRdPtr = pCtx->mpCache;
                    pCtx->mReadPtr = pRdPtr;
                }
				cdx_sem_signal(&pCtx->mWriteDoneSem);
            }
        }
        //(3) process mFlushFlag.
        cdx_mutex_lock(&pCtx->mFlushLock);
        if(pCtx->mFlushFlag)
        {
            //flush data to fs
            pRdPtr = (char*)pCtx->mReadPtr;
            pWtPtr = (char*)pCtx->mWritePtr;
            if(pWtPtr >= pRdPtr)
            {
                nValidSize = pWtPtr - pRdPtr;
                if(nValidSize>0)
                {
                    fileWriter(pCtx->mpStream, pRdPtr, nValidSize);
                    pRdPtr += nValidSize;
                    pCtx->mReadPtr = pRdPtr;
					cdx_sem_signal(&pCtx->mWriteDoneSem);
                }
            }
            else    //pWtPtr < pRdPtr
            {
                nValidSizeSection1 = pWtPtr - pCtx->mpCache;
                nValidSizeSection2 = pCtx->mpCache + pCtx->mCacheSize-pRdPtr;
                nValidSize = nValidSizeSection1 + nValidSizeSection2;
                if(nValidSizeSection2 > 0)
                {
                    fileWriter(pCtx->mpStream, pRdPtr, nValidSizeSection2);
                    pRdPtr = pCtx->mpCache;
                    pCtx->mReadPtr = pRdPtr;
					cdx_sem_signal(&pCtx->mWriteDoneSem);
                }
                else
                {
                    aloge("fatal error! nValidSizeSection2==0!");
                }
                if(nValidSizeSection1 > 0)
                {
                    fileWriter(pCtx->mpStream, pRdPtr, nValidSizeSection1);
                    pRdPtr += nValidSizeSection1;
                    pCtx->mReadPtr = pRdPtr;
					cdx_sem_signal(&pCtx->mWriteDoneSem);
                }
            }
            if(pCtx->mReadPtr == pCtx->mWritePtr)
            {
                pCtx->mReadPtr = pCtx->mWritePtr = pCtx->mpCache;
            }
            else
            {
                aloge("fatal error! rdPtr[%p]!=wtPtr[%p]!", pCtx->mReadPtr, pCtx->mWritePtr);
            }
            cdx_sem_up(&pCtx->mFlushDoneSem);
            pCtx->mFlushFlag = 0;
        }
        cdx_mutex_unlock(&pCtx->mFlushLock);
        
        //(4) process mThreadExitFlag, same as flush.
        if(pCtx->mThreadExitFlag)
        {
            //flush data to fs
            pRdPtr = (char*)pCtx->mReadPtr;
            pWtPtr = (char*)pCtx->mWritePtr;
            if(pWtPtr >= pRdPtr)
            {
                nValidSize = pWtPtr - pRdPtr;
                if(nValidSize>0)
                {
                    fileWriter(pCtx->mpStream, pRdPtr, nValidSize);
                    pRdPtr += nValidSize;
                    pCtx->mReadPtr = pRdPtr;
					cdx_sem_signal(&pCtx->mWriteDoneSem);
                }
            }
            else    //pWtPtr < pRdPtr
            {
                nValidSizeSection1 = pWtPtr - pCtx->mpCache;
                nValidSizeSection2 = pCtx->mpCache + pCtx->mCacheSize-pRdPtr;
                nValidSize = nValidSizeSection1 + nValidSizeSection2;
                if(nValidSizeSection2 > 0)
                {
                    fileWriter(pCtx->mpStream, pRdPtr, nValidSizeSection2);
                    pRdPtr = pCtx->mpCache;
                    pCtx->mReadPtr = pRdPtr;
					cdx_sem_signal(&pCtx->mWriteDoneSem);
                }
                else
                {
                    aloge("fatal error! nValidSizeSection2==0!");
                }
                if(nValidSizeSection1 > 0)
                {
                    fileWriter(pCtx->mpStream, pRdPtr, nValidSizeSection1);
                    pRdPtr += nValidSizeSection1;
                    pCtx->mReadPtr = pRdPtr;
					cdx_sem_signal(&pCtx->mWriteDoneSem);
                }
            }
            goto EXIT;
        }
		//cdx_sem_signal(&pCtx->mWriteDoneSem);
		//fflush(pCtx->mpFile);
	}

EXIT:
    alogv("FsCacheWriteThread[%d] will quit now.", pCtx->mThreadId);	
	return NULL;
}


FsWriter *initFsCacheThreadContext(struct cdx_stream_info *pStream, char *pCache, int nCacheSize, unsigned int vCodec)
{
    int err;
	
    FsWriter *pFsWriter = (FsWriter*)malloc(sizeof(FsWriter));
	if (NULL == pFsWriter) {
        aloge("Failed to alloc FsWriter(%s)", strerror(errno));
		return NULL;
	}
    FsCacheThreadContext *pContext = (FsCacheThreadContext*)malloc(sizeof(FsCacheThreadContext));
	if (NULL == pContext) {
        aloge("Failed to alloc FsCacheThreadContext(%s)", strerror(errno));
		goto ERROR0;
	}
    pContext->mpStream = pStream;
    pContext->mCacheSize = nCacheSize;
    pContext->mpCache = pCache;
    if(NULL == pContext->mpCache)
    {
        alogw("fatal error! malloc [%d]kByte fail.", nCacheSize/1024);
        goto ERROR1;
    }
    pContext->mThreadExitFlag = 0;
    pContext->mWritePtr = pContext->mReadPtr = pContext->mpCache;
    pContext->mFlushFlag = 0;
	pContext->mVideoCodecId = vCodec;
    err = pthread_mutex_init(&pContext->mFlushLock, NULL);
    if(err)
    {
        aloge("err[%d]", err);
        goto ERROR2;
    }
    err = cdx_sem_init(&pContext->mWriteStartSem,0);
    if(err)
    {
        aloge("err[%d]", err);
        goto ERROR3;
    }
    err = cdx_sem_init(&pContext->mFlushDoneSem,0);
    if(err)
    {
        aloge("err[%d]", err);
        goto ERROR4;
    }
	err = cdx_sem_init(&pContext->mWriteDoneSem, 0);
    if(err)
    {
        aloge("err[%d]", err);
        goto ERROR5;
    }
    
    // Create writer thread
	err = pthread_create(&pContext->mThreadId, NULL, FsCacheWriteThread, pContext);
	if (err || !pContext->mThreadId) {
		aloge("FsCacheThread create writer thread err");
        goto ERROR6;
	}

	pFsWriter->fsWrite = FsCacheThreadWrite;
    pFsWriter->fsSeek = FsCacheThreadSeek;
    pFsWriter->fsTell = FsCacheThreadTell;
    pFsWriter->fsTruncate = FsCacheThreadTruncate;
	pFsWriter->fsFlush = FsCacheThreadFlush;
    
	pFsWriter->mMode = FSWRITEMODE_CACHETHREAD;
    pFsWriter->mPriv = (void*)pContext;
    return pFsWriter;

ERROR6:
    cdx_sem_deinit(&pContext->mWriteDoneSem);
ERROR5:
    cdx_sem_deinit(&pContext->mFlushDoneSem);
ERROR4:
    cdx_sem_deinit(&pContext->mWriteStartSem);
ERROR3:
    pthread_mutex_destroy(&pContext->mFlushLock);
ERROR2:
ERROR1:
	free(pContext);
ERROR0:
	free(pFsWriter);
    return NULL;
}

int deinitFsCacheThreadContext(FsWriter *pFsWriter)
{
    int eError = 0;
	if (NULL == pFsWriter) {
        aloge("pFsWriter is NULL!!");
		return -1;
	}
	FsCacheThreadContext *pContext = (FsCacheThreadContext*)pFsWriter->mPriv;
	if (NULL == pContext) {
        aloge("pContext is NULL!!");
		return -1;
	}
    pContext->mThreadExitFlag = 1;
    cdx_sem_up_unique(&pContext->mWriteStartSem);
    pthread_join(pContext->mThreadId, (void*) &eError);
    cdx_sem_deinit(&pContext->mWriteDoneSem);
    cdx_sem_deinit(&pContext->mFlushDoneSem);
    cdx_sem_deinit(&pContext->mWriteStartSem);
    pthread_mutex_destroy(&pContext->mFlushLock);
    pContext->mpCache = NULL;
    pContext->mThreadId = 0;
	free(pContext);
	free(pFsWriter);
    return 0;
}

