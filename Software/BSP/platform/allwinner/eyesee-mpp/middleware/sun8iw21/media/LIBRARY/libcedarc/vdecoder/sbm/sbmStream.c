
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : sbm.c
* Description :This is the stream buffer module. The SBM provides
*              methods for managing the stream data before decode.
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#include <stdlib.h>
#include<string.h>
#include <pthread.h>
#include "sbm.h"

//#include "secureMemoryAdapter.h"

#include "cdc_log.h"

#define SBM_FRAME_FIFO_SIZE (2048)  //* store 2048 frames of bitstream data at maximum.

typedef struct StreamBufferManagerStream
{
    SbmInterface sbmInterface;
    pthread_mutex_t mutex;
    char*           pStreamBuffer;
    char*           pStreamBufferEnd;
    int             nStreamBufferSize;
    char*           pWriteAddr;
    int             nValidDataSize;
    StreamFrameFifo frameFifo;
    SbmConfig mConfig;
}SbmStream;

static int lock(SbmStream *pSbm);
static void unlock(SbmStream *pSbm);

/*
**********************************************************************
*                             SbmCreate
*
*Description: Create Stream Buffer Manager module.
*
*Arguments  : nBufferSize     the size of pStreamBuffer, to store stream info.
*
*Return     : result
*               = NULL;     failed;
*              != NULL;     Sbm handler.
*
*Summary    : nBufferSize is between 4MB and 12MB.
*
**********************************************************************
*/
int SbmStreamInit(SbmInterface* pSelf, SbmConfig* pSbmConfig)
{
    SbmStream *pSbm = (SbmStream*)pSelf;
    char *pSbmBuf;
    int i;
    int ret;

    if(pSbmConfig == NULL)
    {
        loge(" pSbmConfig is null");
        return -1;
    }

    if(pSbmConfig->nSbmBufferTotalSize <= 0)
    {
        loge(" pSbmConfig->nBufferSize(%d) is invalid",pSbmConfig->nSbmBufferTotalSize);
        return -1;
    }

    memcpy(&pSbm->mConfig, pSbmConfig, sizeof(SbmConfig));

    if(pSbm->mConfig.bVirFlag == 1)
        pSbmBuf = (char*)malloc(pSbm->mConfig.nSbmBufferTotalSize);
    else
        pSbmBuf = (char*)CdcMemPalloc(pSbm->mConfig.memops, pSbm->mConfig.nSbmBufferTotalSize,
                                      pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);

    if(pSbmBuf == NULL)
    {
        loge("pSbmBuf == NULL.");
        return -1;
    }

    pSbm->frameFifo.pFrames = (VideoStreamDataInfo *)malloc(SBM_FRAME_FIFO_SIZE
                                                 * sizeof(VideoStreamDataInfo));
    if(pSbm->frameFifo.pFrames == NULL)
    {
        loge("sbm->frameFifo.pFrames == NULL.");
        free(pSbm);
        if(pSbm->mConfig.bVirFlag == 1)
            free(pSbmBuf);
        else
            CdcMemPfree(pSbm->mConfig.memops,pSbmBuf,
                        pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);
        return -1;
    }
    memset(pSbm->frameFifo.pFrames, 0,  SBM_FRAME_FIFO_SIZE * sizeof(VideoStreamDataInfo));
    for(i = 0; i < SBM_FRAME_FIFO_SIZE; i++)
    {
        pSbm->frameFifo.pFrames[i].nID = i;
    }

    ret = pthread_mutex_init(&pSbm->mutex, NULL);
    if(ret != 0)
    {
        loge("pthread_mutex_init failed.");
        free(pSbm->frameFifo.pFrames);
        free(pSbm);

        if(pSbm->mConfig.bVirFlag == 1)
            free(pSbmBuf);
        else
            CdcMemPfree(pSbm->mConfig.memops,pSbmBuf,
                        pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);
        return -1;
    }
    pSbm->pStreamBuffer      = pSbmBuf;
    pSbm->pStreamBufferEnd   = pSbmBuf + pSbm->mConfig.nSbmBufferTotalSize -1;
    pSbm->nStreamBufferSize  = pSbm->mConfig.nSbmBufferTotalSize;
    pSbm->pWriteAddr         = pSbmBuf;
    pSbm->nValidDataSize     = 0;

    pSbm->frameFifo.nMaxFrameNum     = SBM_FRAME_FIFO_SIZE;
    pSbm->frameFifo.nValidFrameNum   = 0;
    pSbm->frameFifo.nUnReadFrameNum  = 0;
    pSbm->frameFifo.nReadPos         = 0;
    pSbm->frameFifo.nWritePos        = 0;
    pSbm->frameFifo.nFlushPos        = 0;

    return 0;
}

/*
**********************************************************************
*                             SbmDestroy
*
*Description: Destroy Stream Buffer Manager module, free resource.
*
*Arguments  : pSbm     Created by SbmCreate function.
*
*Return     : NULL
*
*Summary    :
*
**********************************************************************
*/
void SbmStreamDestroy(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm != NULL)
    {
        pthread_mutex_destroy(&pSbm->mutex);

        if(pSbm->pStreamBuffer != NULL)
        {
            if(pSbm->mConfig.bVirFlag == 1)
                free(pSbm->pStreamBuffer);
            else
                CdcMemPfree(pSbm->mConfig.memops,pSbm->pStreamBuffer,
                            pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);

            pSbm->pStreamBuffer = NULL;
        }

        if(pSbm->frameFifo.pFrames != NULL)
        {
            free(pSbm->frameFifo.pFrames);
            pSbm->frameFifo.pFrames = NULL;
        }

        free(pSbm);
    }

    return;
}

/*
**********************************************************************
*                             SbmReset
*
*Description: Reset Stream Buffer Manager module.
*
*Arguments  : pSbm     Created by SbmCreate function.
*
*Return     : NULL
*
*Summary    : If succeed, Stream Buffer Manager module will be resumed to initial state,
*             stream data will be discarded.
*
**********************************************************************
*/
void SbmStreamReset(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return;
    }

    if(lock(pSbm) != 0)
        return;

    pSbm->pWriteAddr                 = pSbm->pStreamBuffer;
    pSbm->nValidDataSize             = 0;

    pSbm->frameFifo.nReadPos         = 0;
    pSbm->frameFifo.nWritePos        = 0;
    pSbm->frameFifo.nFlushPos        = 0;
    pSbm->frameFifo.nValidFrameNum   = 0;
    pSbm->frameFifo.nUnReadFrameNum  = 0;
    unlock(pSbm);

    return;
}

/*
**********************************************************************
*                             SbmBufferAddress
*
*Description: Get the base address of SBM buffer.
*
*Arguments  : pSbm     Created by SbmCreate function.
*
*Return     : The base address of SBM buffer.
*
*Summary    :
*
**********************************************************************
*/
void *SbmStreamGetBufferAddress(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return NULL;
    }

    return pSbm->pStreamBuffer;
}

/*
**********************************************************************
*                             SbmBufferSize
*
*Description: Get the sbm buffer size.
*
*Arguments  : pSbm     Created by SbmCreate function.
*
*Return     : The size of SBM buffer, in Bytes.
*
*Summary    : The size is set when create SBM.
*
**********************************************************************
*/
int SbmStreamGetBufferSize(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return 0;
    }

    return pSbm->nStreamBufferSize;
}

/*
**********************************************************************
*                             SbmStreamFrameNum
*
*Description: Get the total frames of undecoded stream data.
*
*Arguments  : pSbm     Created by SbmCreate function.
*
*Return     : The frames of undecoded stream data.
*
*Summary    :
*
**********************************************************************
*/
int SbmStreamGetStreamFrameNum(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return 0;
    }

    return pSbm->frameFifo.nValidFrameNum;
}

/*
**********************************************************************
*                             SbmStreamDataSize
*
*Description: Get the total size of undecoded data.
*
*Arguments  : pSbm     Created by SbmCreate function.
*
*Return     : The total size of undecoded stream data, in bytes.
*
*Summary    :
*
**********************************************************************
*/
int SbmStreamGetStreamDataSize(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return 0;
    }

    return pSbm->nValidDataSize;
}

char* SbmStreamGetBufferWritePointer(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return 0;
    }

    return pSbm->pWriteAddr;
}

void* SbmStreamGetBufferDataInfo(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    VideoStreamDataInfo *pDataInfo;

    if(pSbm == NULL )
    {
        loge("pSbm == NULL.");
        return NULL;
    }

    if(lock(pSbm) != 0)
    {
        return NULL;
    }

    if(pSbm->frameFifo.nUnReadFrameNum == 0)
    {
        logv("nUnReadFrameNum == 0.");
        unlock(pSbm);
        return NULL;
    }

    pDataInfo = &pSbm->frameFifo.pFrames[pSbm->frameFifo.nReadPos];

    if(pDataInfo == NULL)
    {
        loge("request failed.");
        unlock(pSbm);
        return NULL;
    }
    unlock(pSbm);
    return pDataInfo->pVideoInfo;
}
/*
**********************************************************************
*                             SbmRequestBuffer
*
*Description: Request buffer from sbm module.
*
*Arguments  : pSbm              Created by SbmCreate function;
*             nRequireSize      the required size, in bytes;
*             ppBuf             store the requested buffer address;
*             pBufSize          store the requested buffer size.
*
*Return     : result;
*               = 0;    succeeded;
*               = -1;   failed.
*
*Summary    : SBM buffer is cyclic, if the  buffer turns around, there will be 2 blocks.
*
**********************************************************************
*/
int SbmStreamRequestBuffer(SbmInterface* pSelf, int nRequireSize,
                                   char **ppBuf, int *pBufSize)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    if(pSbm == NULL || ppBuf == NULL || pBufSize == NULL)
    {
        loge("input error.");
        return -1;
    }

    if(lock(pSbm) != 0)
        return -1;

    if(pSbm->frameFifo.nValidFrameNum >= pSbm->frameFifo.nMaxFrameNum)
    {
        logv("nValidFrameNum >= nMaxFrameNum.");
        unlock(pSbm);
        return -1;
    }

    if(pSbm->nValidDataSize < pSbm->nStreamBufferSize)
    {
        int nFreeSize = pSbm->nStreamBufferSize - pSbm->nValidDataSize;
        if((nRequireSize + 64) > nFreeSize)
        {
            unlock(pSbm);
            return -1;
        }

        *ppBuf    = pSbm->pWriteAddr;
        *pBufSize = nRequireSize;

        unlock(pSbm);
        return 0;
    }
    else
    {
        loge("no free buffer.");
        unlock(pSbm);
        return -1;
    }
}

/*
**********************************************************************
*                             SbmAddStream
*
*Description: Add one frame stream to sbm module.
*
*Arguments  : pSbm              Created by SbmCreate function;
*             pDataInfo         the stream info need to be added.
*
*Return     : result;
*               = 0;    succeeded;
*               = -1;   failed.
*
*Summary    : pDataInfo should contain Complete frame, bIsFirstPart=bIsLastPart=1.
*
**********************************************************************
*/
int SbmStreamAddStream(SbmInterface* pSelf, VideoStreamDataInfo *pDataInfo)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    int nWritePos;
    char *pNewWriteAddr;

    if(pSbm == NULL || pDataInfo == NULL)
    {
        loge("input error.");
        return -1;
    }

    if(lock(pSbm) != 0)
        return -1;

    if(pDataInfo->pData == 0)
    {
        loge("data buffer is NULL.\n");
        unlock(pSbm);
        return -1;
    }
    if(pSbm->frameFifo.nValidFrameNum >= pSbm->frameFifo.nMaxFrameNum)
    {
        loge("nValidFrameNum > nMaxFrameNum.");
        unlock(pSbm);
        return -1;
    }

    if(pDataInfo->nLength + pSbm->nValidDataSize > pSbm->nStreamBufferSize)
    {
        loge("no free buffer.");
        unlock(pSbm);
        return -1;
    }
    if(pDataInfo->bValid == 0)
    {
        pDataInfo->bValid = 1;
    }

    nWritePos = pSbm->frameFifo.nWritePos;
    memcpy(&pSbm->frameFifo.pFrames[nWritePos], pDataInfo, sizeof(VideoStreamDataInfo));
    nWritePos++;
    if(nWritePos >= pSbm->frameFifo.nMaxFrameNum)
    {
        nWritePos = 0;
    }

    pSbm->frameFifo.nWritePos = nWritePos;
    pSbm->frameFifo.nValidFrameNum++;
    pSbm->frameFifo.nUnReadFrameNum++;
    pSbm->nValidDataSize += pDataInfo->nLength;

    pNewWriteAddr = pSbm->pWriteAddr + pDataInfo->nLength;
    if(pNewWriteAddr > pSbm->pStreamBufferEnd)
    {
        pNewWriteAddr -= pSbm->nStreamBufferSize;
    }

    pSbm->pWriteAddr = pNewWriteAddr;

    unlock(pSbm);
    return 0;
}

/*
**********************************************************************
*                             SbmRequestStream
*
*Description: Request one frame stream data from sbm module to decoder.
*
*Arguments  : pSbm      Created by SbmCreate function;
*
*Return     : The stream infomation.
*
*Summary    : The stream data obeys FIFO rule.
*
**********************************************************************
*/
VideoStreamDataInfo *SbmStreamRequestStream(SbmInterface* pSelf)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    VideoStreamDataInfo *pDataInfo;

    if(pSbm == NULL )
    {
        loge("pSbm == NULL.");
        return NULL;
    }

    if(lock(pSbm) != 0)
    {
        return NULL;
    }

    if(pSbm->frameFifo.nUnReadFrameNum == 0)
    {
        logv("nUnReadFrameNum == 0.");
        unlock(pSbm);
        return NULL;
    }

    pDataInfo = &pSbm->frameFifo.pFrames[pSbm->frameFifo.nReadPos];

    if(pDataInfo == NULL)
    {
        loge("request failed.");
        unlock(pSbm);
        return NULL;
    }

    pSbm->frameFifo.nReadPos++;
    pSbm->frameFifo.nUnReadFrameNum--;
    if(pSbm->frameFifo.nReadPos >= pSbm->frameFifo.nMaxFrameNum)
    {
        pSbm->frameFifo.nReadPos = 0;
    }
    unlock(pSbm);

    return pDataInfo;
}

/*
**********************************************************************
*                             SbmReturnStream
*
*Description: Return one undecoded frame to sbm module.
*
*Arguments  : pSbm          Created by SbmCreate function;
*             pDataInfo     the stream info need to be returned.
*
*Return     : result;
*               = 0;    succeeded;
*               = -1;   failed.
*
*Summary    : After returned, the stream data's sequence is the same as before.
*
**********************************************************************
*/
int SbmStreamReturnStream(SbmInterface* pSelf, VideoStreamDataInfo *pDataInfo)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    int nReadPos;

    if(pSbm == NULL || pDataInfo == NULL)
    {
        loge("input error.");
        return -1;
    }

    if(lock(pSbm) != 0)
    {
        return -1;
    }

    if(pSbm->frameFifo.nValidFrameNum == 0)
    {
        loge("nValidFrameNum == 0.");
        unlock(pSbm);
        return -1;
    }
    nReadPos = pSbm->frameFifo.nReadPos;
    nReadPos--;
    if(nReadPos < 0)
    {
        nReadPos = pSbm->frameFifo.nMaxFrameNum - 1;
    }
    pSbm->frameFifo.nUnReadFrameNum++;
    if(pDataInfo != &pSbm->frameFifo.pFrames[nReadPos])
    {
        loge("wrong frame sequence.");
        abort();
    }

    pSbm->frameFifo.pFrames[nReadPos] = *pDataInfo;
    pSbm->frameFifo.nReadPos  = nReadPos;

    unlock(pSbm);
    return 0;
}

/*
**********************************************************************
*                             SbmFlushStream
*
*Description: Flush one frame which is requested from SBM.
*
*Arguments  : pSbm          Created by SbmCreate function;
*             pDataInfo     the stream info need to be flushed.
*
*Return     : result;
*               = 0;    succeeded;
*               = -1;   failed.
*
*Summary    : After flushed, the buffer can be used to store new stream.
*
**********************************************************************
*/
int SbmStreamFlushStream(SbmInterface* pSelf, VideoStreamDataInfo *pDataInfo)
{
    SbmStream *pSbm = (SbmStream*)pSelf;

    int nFlushPos;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return -1;
    }

    if(lock(pSbm) != 0)
    {
        return -1;
    }

    if(pSbm->frameFifo.nValidFrameNum == 0)
    {
        loge("no valid frame.");
        unlock(pSbm);
        return -1;
    }

    nFlushPos = pSbm->frameFifo.nFlushPos;
    if(pDataInfo != &pSbm->frameFifo.pFrames[nFlushPos])
    {
        loge("not current nFlushPos.");
        unlock(pSbm);
        abort();
        return -1;
    }

    nFlushPos++;
    if(nFlushPos >= pSbm->frameFifo.nMaxFrameNum)
    {
        nFlushPos = 0;
    }

    pSbm->frameFifo.nValidFrameNum--;
    pSbm->nValidDataSize     -= pDataInfo->nLength;
    pSbm->frameFifo.nFlushPos = nFlushPos;   //*
    unlock(pSbm);
    return 0;
}

static int SbmStreamSetEos(SbmInterface* pSelf, int nEosFlag)
{
    CEDARC_UNUSE(pSelf);
    CEDARC_UNUSE(nEosFlag);
    return 0;
}

static int lock(SbmStream *pSbm)
{
    if(pthread_mutex_lock(&pSbm->mutex) != 0)
        return -1;
    return 0;
}

static void unlock(SbmStream *pSbm)
{
    pthread_mutex_unlock(&pSbm->mutex);
    return;
}

SbmInterface* GetSbmInterfaceStream()
{
    logd("******* sbm-type: Stream*******");

    SbmStream* pSbmStream = NULL;
    pSbmStream = (SbmStream*)malloc(sizeof(SbmStream));
    if(pSbmStream == NULL)
    {
        loge("malloc for sbm stream struct failed");
        return NULL;
    }

    memset(pSbmStream, 0, sizeof(SbmStream));

    pSbmStream->sbmInterface.init    = SbmStreamInit;
    pSbmStream->sbmInterface.destroy = SbmStreamDestroy;
    pSbmStream->sbmInterface.reset   = SbmStreamReset;
    pSbmStream->sbmInterface.getBufferSize         = SbmStreamGetBufferSize;
    pSbmStream->sbmInterface.getStreamDataSize     = SbmStreamGetStreamDataSize;
    pSbmStream->sbmInterface.getStreamFrameNum     = SbmStreamGetStreamFrameNum;
    pSbmStream->sbmInterface.getBufferAddress      = SbmStreamGetBufferAddress;
    pSbmStream->sbmInterface.getBufferWritePointer = SbmStreamGetBufferWritePointer;
    pSbmStream->sbmInterface.getBufferDataInfo = SbmStreamGetBufferDataInfo;
    pSbmStream->sbmInterface.requestBuffer     = SbmStreamRequestBuffer;
    pSbmStream->sbmInterface.addStream         = SbmStreamAddStream;
    pSbmStream->sbmInterface.requestStream     = SbmStreamRequestStream;
    pSbmStream->sbmInterface.returnStream      = SbmStreamReturnStream;
    pSbmStream->sbmInterface.flushStream       = SbmStreamFlushStream;
    pSbmStream->sbmInterface.setEos            = SbmStreamSetEos;

    pSbmStream->sbmInterface.nType = SBM_TYPE_STREAM;
    pSbmStream->sbmInterface.bUseNewVeMemoryProgram = 0;
    return &pSbmStream->sbmInterface;
}

