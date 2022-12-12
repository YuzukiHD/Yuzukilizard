
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

extern s32 AvcSbmFrameCheckBitStreamType(SbmFrame* pSbm);
extern s32 HevcSbmFrameCheckBitStreamType(SbmFrame* pSbm);
extern void HevcSbmFrameDetectOneFramePic(SbmFrame* pSbm);
extern void AvcSbmFrameDetectOneFramePic(SbmFrame* pSbm);
static void* ProcessThread(void* pThreadData);

extern SbmInterface* GetSbmInterfaceStream();


int SbmFrameBufferSizeCalculate(int nConfigSbmBufferSize,int nVideoWidth)
{
    int nBufferSize = 0;

    if(nVideoWidth == 0)
        nVideoWidth = 1920;

    if(nVideoWidth < 720)
    {
        nBufferSize = 256*1024;
    }
    else if(nVideoWidth < 1920)
    {
        nBufferSize = 512*1024;
    }
    else if(nVideoWidth < 3840)
    {
        nBufferSize = 1*1024*1024;
    }
    else
    {
        nBufferSize = 3*1024*1024/2;
    }
    if(nConfigSbmBufferSize>1*1024*1024 && nConfigSbmBufferSize<=32*1024*1024)
    {
        if(nBufferSize>(nConfigSbmBufferSize/2))
        {
            nBufferSize = nConfigSbmBufferSize/2;
        }
    }
    logd("*********nBufferSize=%d\n", nBufferSize);
    return nBufferSize;
}


void FIFOEnqueue(FiFoQueueInst** ppHead, FiFoQueueInst* p)
{
    FiFoQueueInst* pCur;

    pCur = *ppHead;

    if(pCur == NULL)
    {
        *ppHead  = p;
        p->pNext = NULL;
        return;
    }
    else
    {
        while(pCur->pNext != NULL)
            pCur = pCur->pNext;

        pCur->pNext = p;
        p->pNext    = NULL;

        return;
    }
}


FiFoQueueInst* FIFODequeue(FiFoQueueInst** ppHead)
{
    FiFoQueueInst* pHead;

    pHead = *ppHead;

    if(pHead == NULL)
        return NULL;
    else
    {
        *ppHead = pHead->pNext;
        pHead->pNext = NULL;
        return pHead;
    }
}


void FIFOEnqueueToHead(FiFoQueueInst ** ppHead, FiFoQueueInst* p)
{
     FiFoQueueInst* pCur;

     pCur     = *ppHead;
    *ppHead  = p;
     p->pNext = pCur;
     return;
}

static int lock(SbmFrame *pSbm)
{
    if(pthread_mutex_lock(&pSbm->mutex) != 0)
        return -1;
    return 0;
}

static void unlock(SbmFrame *pSbm)
{
    pthread_mutex_unlock(&pSbm->mutex);
    return;
}


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
int SbmFrameInit(SbmInterface* pSelf, SbmConfig* pSbmConfig)
{
    SbmFrame *pSbm = (SbmFrame*)pSelf;
    char *pSbmBuf;
    int i;
    int ret;
    int nSbmFrameBufferSize = 0;
    size_addr phyAddr,highPhyAddr,lowPhyAddr;


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


    logd("************pSbm->sbmInterface.bUseNewVeMemoryProgram=%d\n", pSbm->sbmInterface.bUseNewVeMemoryProgram);

    if(pSbm->sbmInterface.bUseNewVeMemoryProgram == 1)
    {
        pSbmBuf = (char*)malloc(pSbm->mConfig.nSbmBufferTotalSize);//*
        if(pSbmBuf == NULL)
        {
            loge(" palloc for sbmBuf failed, size = %d MB",
                    pSbm->mConfig.nSbmBufferTotalSize/1024/1024);
            goto ERROR;
        }
        pSbm->pSbmFrameBuffer =
          (SbmFrameBufferNode*)malloc(MAX_FRAME_BUFFER_NUM*sizeof(SbmFrameBufferNode));
        if(pSbm->pSbmFrameBuffer == NULL)
        {
            loge("malloc memory for pSbm->pSbmFrameBuffer failed\n");
            goto ERROR;
        }
        ret = pthread_mutex_init(&pSbm->pSbmFrameBufferMutex, NULL);
        if(ret != 0)
        {
            loge("pthread_mutex_init failed.");
            goto ERROR;
        }

        nSbmFrameBufferSize = SbmFrameBufferSizeCalculate(pSbmConfig->nConfigSbmBufferSize,
            pSbmConfig->nWidth);

        for(i=0; i<MAX_FRAME_BUFFER_NUM; i++)
        {
            SbmFrameBufferNode* pSbmFrameBuffer = &(pSbm->pSbmFrameBuffer[i]);
            memset(pSbmFrameBuffer, 0, sizeof(SbmFrameBufferNode));
            //pSbmFrameBuffer->pFramePic.pNaluInfoList = (NaluInfo*)malloc(DEFAULT_NALU_NUM*sizeof(NaluInfo));
            //if(pSbmFrameBuffer->pFramePic.pNaluInfoList == NULL)
            //{
            //    loge("malloc for naluInfo failed");
            //    goto ERROR;
            //}

            pSbmFrameBuffer->pSbmBufferManager =
            (SbmFrameBufferManager*)malloc(sizeof(SbmFrameBufferManager));
            if(pSbmFrameBuffer->pSbmBufferManager  == NULL)
            {
                loge("ammloc memory for pSbmFrameBuffer->pSbmBufferManager failed\n");
                goto ERROR;
            }

            pSbmFrameBuffer->pSbmBufferManager->nFrameBufferSize = nSbmFrameBufferSize;
            pSbmFrameBuffer->pSbmBufferManager->pFrameBuffer =
                    (char*)CdcMemPalloc(pSbm->mConfig.memops, nSbmFrameBufferSize,
                                      pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);//
            if(pSbmFrameBuffer->pSbmBufferManager->pFrameBuffer == NULL)
            {
                loge("malloc buffer for pSbm->H264FrameBufferManager.pFrameBuffer failed\n");
                goto ERROR;;
            }
            phyAddr = (size_addr)(CdcMemGetPhysicAddress(pSbm->mConfig.memops,
            (void*)pSbmFrameBuffer->pSbmBufferManager->pFrameBuffer));

            if(pSbm->sbmInterface.nType == SBM_TYPE_FRAME_AVC)
            {
                highPhyAddr = (phyAddr>>28) & 0x0f;

                lowPhyAddr =  phyAddr & 0x0ffffff0;
                pSbmFrameBuffer->pSbmBufferManager->pPhyFrameBuffer = lowPhyAddr+highPhyAddr;
            }
            else if(pSbm->sbmInterface.nType == SBM_TYPE_FRAME)
            {
                pSbmFrameBuffer->pSbmBufferManager->pPhyFrameBuffer = phyAddr;
            }
            pSbmFrameBuffer->pSbmBufferManager->pPhyFrameBufferEnd = (u32)(phyAddr+nSbmFrameBufferSize-1);
            FIFOEnqueue((FiFoQueueInst**)(&pSbm->pSbmFrameBufferEmptyQueue), (FiFoQueueInst*)pSbmFrameBuffer);
        }
    }
    else
    {
        pSbmBuf = (char*)CdcMemPalloc(pSbm->mConfig.memops, pSbm->mConfig.nSbmBufferTotalSize,
                                      pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);//*
        if(pSbmBuf == NULL)
        {
            loge(" palloc for sbmBuf failed, size = %d MB",
                    pSbm->mConfig.nSbmBufferTotalSize/1024/1024);
            goto ERROR;
        }
    }

    if(pSbm->sbmInterface.nType == SBM_TYPE_FRAME_AVC)
    {
        pSbm->checkBitStreamType = AvcSbmFrameCheckBitStreamType;
        pSbm->detectOneFramePic  = AvcSbmFrameDetectOneFramePic;
    }
    else if(pSbm->sbmInterface.nType == SBM_TYPE_FRAME)
    {
        pSbm->checkBitStreamType = HevcSbmFrameCheckBitStreamType;
        pSbm->detectOneFramePic  = HevcSbmFrameDetectOneFramePic;
    }

    pSbm->frameFifo.pFrames = (VideoStreamDataInfo *)malloc(SBM_FRAME_FIFO_SIZE
                                                 * sizeof(VideoStreamDataInfo));
    if(pSbm->frameFifo.pFrames == NULL)
    {
        loge("sbm->frameFifo.pFrames == NULL.");
        goto ERROR;
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
        goto ERROR;
    }
    pSbm->pStreamBuffer      = pSbmBuf;
    pSbm->pStreamBufferEnd   = pSbmBuf + pSbm->mConfig.nSbmBufferTotalSize - 1;
    pSbm->nStreamBufferSize  = pSbm->mConfig.nSbmBufferTotalSize;
    pSbm->pWriteAddr         = pSbmBuf;
    pSbm->nValidDataSize     = 0;

    pSbm->frameFifo.nMaxFrameNum     = SBM_FRAME_FIFO_SIZE;
    pSbm->frameFifo.nValidFrameNum   = 0;
    pSbm->frameFifo.nUnReadFrameNum  = 0;
    pSbm->frameFifo.nReadPos         = 0;
    pSbm->frameFifo.nWritePos        = 0;
    pSbm->frameFifo.nFlushPos        = 0;

    pSbm->mFramePicFifo.pFramePics = (FramePicInfo*)malloc(MAX_FRAME_PIC_NUM*sizeof(FramePicInfo));
    if(pSbm->mFramePicFifo.pFramePics == NULL)
    {
        loge("malloc for framePic failed");
        goto ERROR;
    }
    memset(pSbm->mFramePicFifo.pFramePics, 0, MAX_FRAME_PIC_NUM*sizeof(FramePicInfo));
    pSbm->mFramePicFifo.nMaxFramePicNum = MAX_FRAME_PIC_NUM;

    for(i = 0; i < MAX_FRAME_PIC_NUM; i++)
    {
        pSbm->mFramePicFifo.pFramePics[i].pNaluInfoList
            = (NaluInfo*)malloc(DEFAULT_NALU_NUM*sizeof(NaluInfo));
        if(pSbm->mFramePicFifo.pFramePics[i].pNaluInfoList == NULL)
        {
            loge("malloc for naluInfo failed");
            goto ERROR;
        }
        memset(pSbm->mFramePicFifo.pFramePics[i].pNaluInfoList, 0, \
                DEFAULT_NALU_NUM*sizeof(NaluInfo));
        pSbm->mFramePicFifo.pFramePics[i].nMaxNaluNum = DEFAULT_NALU_NUM;
    }

    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        pSbm->pTmpBuffer = (char*)malloc(TMP_BUFFER_SIZE);
        if(pSbm->pTmpBuffer == NULL)
        {
            loge("malloc for pTmpBuffer failed");
            goto ERROR;
        }
        pSbm->nTmpBufferSize = TMP_BUFFER_SIZE;
    }

    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        pSbm->pDetectBufStart = pSbm->pTmpBuffer;
        pSbm->pDetectBufEnd   = pSbm->pTmpBuffer + pSbm->nTmpBufferSize - 1;
    }
    else
    {
        pSbm->pDetectBufStart = pSbm->pStreamBuffer;
        pSbm->pDetectBufEnd   = pSbm->pStreamBufferEnd;
    }

    sem_init(&pSbm->streamDataSem, 0, 0);
    sem_init(&pSbm->emptyFramePicSem, 0, 0);
    sem_init(&pSbm->resetSem, 0, 0);

    pSbm->mq = CdcMessageQueueCreate(32, "sbm_Thread");

    int err = pthread_create(&pSbm->mThreadId, NULL, ProcessThread, pSbm);
    if(err || pSbm->mThreadId == 0)
    {
        loge("create sbm pthread failed");
        goto ERROR;
    }

    CdcMessage mMsg;
    memset(&mMsg, 0, sizeof(CdcMessage));
    mMsg.messageId = SBM_THREAD_CMD_READ;
    CdcMessageQueuePostMessage(pSbm->mq, &mMsg);

    pSbm->bStreamWithStartCode = -1;
    return 0;

ERROR:
    if(pSbm->sbmInterface.bUseNewVeMemoryProgram == 1)
    {
        if(pSbmBuf)
        {
            free(pSbmBuf);
            pSbmBuf = NULL;
        }

        if(pSbm)
        {
            if(pSbm->pSbmFrameBuffer != NULL)
            {
                for(i=0; i<MAX_FRAME_BUFFER_NUM; i++)
                {
                    if(pSbm->pSbmFrameBuffer[i].pSbmBufferManager != NULL)
                     {
                        if(pSbm->pSbmFrameBuffer[i].pSbmBufferManager->pFrameBuffer != NULL)
                        {
                            CdcMemPfree(pSbm->mConfig.memops,
                                pSbm->pSbmFrameBuffer[i].pSbmBufferManager->pFrameBuffer,
                                pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);
                            pSbm->pSbmFrameBuffer[i].pSbmBufferManager->pFrameBuffer = NULL;
                        }
                        free(pSbm->pSbmFrameBuffer[i].pSbmBufferManager);
                        pSbm->pSbmFrameBuffer[i].pSbmBufferManager  = NULL;
                     }
                }
                free(pSbm->pSbmFrameBuffer);
                pSbm->pSbmFrameBuffer = NULL;
            }
        }
     }
     else
     {
        if(pSbmBuf)
        {
            CdcMemPfree(pSbm->mConfig.memops,pSbmBuf,
                        pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);
            pSbmBuf = NULL;
        }
    }

    if(pSbm->pTmpBuffer)
        free(pSbm->pTmpBuffer);

    if(pSbm)
    {

        sem_destroy(&pSbm->streamDataSem);
        sem_destroy(&pSbm->emptyFramePicSem);
        sem_destroy(&pSbm->resetSem);

        if(pSbm->frameFifo.pFrames)
            free(pSbm->frameFifo.pFrames);

        if(pSbm->mFramePicFifo.pFramePics)
        {
            for(i=0; i < MAX_FRAME_PIC_NUM; i++)
            {
                if(pSbm->mFramePicFifo.pFramePics[i].pNaluInfoList)
                    free(pSbm->mFramePicFifo.pFramePics[i].pNaluInfoList);
            }
            free(pSbm->mFramePicFifo.pFramePics);
        }
        free(pSbm);
    }

    return -1;

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
static void SbmFrameDestroy(SbmInterface* pSelf)
{
    int i = 0;
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    logv(" sbm destroy");
    if(pSbm != NULL)
    {
            //* send stop cmd to thread here
        if(pSbm->mq)
        {
            CdcMessage msg;
            memset(&msg, 0, sizeof(CdcMessage));
            msg.messageId = SBM_THREAD_CMD_QUIT;
            CdcMessageQueuePostMessage(pSbm->mq,&msg);
            logv("*** post quit message");

            int error;
            pthread_join(pSbm->mThreadId, (void**)&error);
            logv("*** pthread_join finish");

            pthread_mutex_destroy(&pSbm->mutex);
            if(pSbm->sbmInterface.bUseNewVeMemoryProgram == 1)
            {
                pthread_mutex_destroy(&pSbm->pSbmFrameBufferMutex);
            }
        }
        sem_destroy(&pSbm->streamDataSem);
        sem_destroy(&pSbm->emptyFramePicSem);
        sem_destroy(&pSbm->resetSem);


        if(pSbm->sbmInterface.bUseNewVeMemoryProgram == 1)
        {
            if(pSbm->pStreamBuffer != NULL)
            {
                free(pSbm->pStreamBuffer);
                pSbm->pStreamBuffer = NULL;
            }

            if(pSbm->pSbmFrameBuffer != NULL)
            {
                for(i=0; i<MAX_FRAME_BUFFER_NUM; i++)
                {
                    if(pSbm->pSbmFrameBuffer[i].pSbmBufferManager != NULL)
                    {
                        if(pSbm->pSbmFrameBuffer[i].pSbmBufferManager->pFrameBuffer != NULL)
                        {
                            CdcMemPfree(pSbm->mConfig.memops,
                                pSbm->pSbmFrameBuffer[i].pSbmBufferManager->pFrameBuffer,
                                pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);
                            pSbm->pSbmFrameBuffer[i].pSbmBufferManager->pFrameBuffer = NULL;
                        }
                        free(pSbm->pSbmFrameBuffer[i].pSbmBufferManager);
                        pSbm->pSbmFrameBuffer[i].pSbmBufferManager  = NULL;
                    }
                }
                free(pSbm->pSbmFrameBuffer);
                pSbm->pSbmFrameBuffer = NULL;
            }
        }
        else
        {
            if(pSbm->pStreamBuffer != NULL)
            {
                CdcMemPfree(pSbm->mConfig.memops,pSbm->pStreamBuffer,
                            pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);
                pSbm->pStreamBuffer = NULL;
            }
        }

        if(pSbm->frameFifo.pFrames != NULL)
        {
            free(pSbm->frameFifo.pFrames);
            pSbm->frameFifo.pFrames = NULL;
        }

        if(pSbm->mq)
            CdcMessageQueueDestroy(pSbm->mq);

        if(pSbm->mFramePicFifo.pFramePics)
        {
            int i = 0;
            for(i=0; i < MAX_FRAME_PIC_NUM; i++)
            {
                if(pSbm->mFramePicFifo.pFramePics[i].pNaluInfoList)
                    free(pSbm->mFramePicFifo.pFramePics[i].pNaluInfoList);
            }
            free(pSbm->mFramePicFifo.pFramePics);
        }

        if(pSbm->pTmpBuffer)
            free(pSbm->pTmpBuffer);

        free(pSbm);
    }
    logv(" sbm destroy finish");

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
static void SbmFrameReset(SbmInterface* pSelf)
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return;
    }

    CdcMessage mMsg;
    memset(&mMsg, 0, sizeof(CdcMessage));
    mMsg.messageId = SBM_THREAD_CMD_RESET;
    mMsg.params[0] = (uintptr_t)(&pSbm->resetSem);
    CdcMessageQueuePostMessage(pSbm->mq, &mMsg);

    logd("** wait for reset sem");
    SemTimedWait(&pSbm->resetSem, -1);
    logd("** wait for reset sem ok");

    memset(&mMsg, 0, sizeof(CdcMessage));
    mMsg.messageId = SBM_THREAD_CMD_READ;
    CdcMessageQueuePostMessage(pSbm->mq, &mMsg);
    logd("SbmFrameReset finish");
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
static void *SbmFrameGetBufferAddress(SbmInterface* pSelf)
{
    SbmFrame *pSbm = (SbmFrame*)pSelf;
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
static int SbmFrameGetBufferSize(SbmInterface* pSelf)
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;

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
static int SbmFrameGetStreamFrameNum(SbmInterface* pSelf)
{
    SbmFrame *pSbm = (SbmFrame*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return 0;
    }

    return pSbm->frameFifo.nValidFrameNum + pSbm->mFramePicFifo.nValidFramePicNum;
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
static int SbmFrameGetStreamDataSize(SbmInterface* pSelf)
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return 0;
    }

    return pSbm->nValidDataSize;
}

static char* SbmFrameGetBufferWritePointer(SbmInterface* pSelf)
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;

    if(pSbm == NULL)
    {
        loge("pSbm == NULL.");
        return 0;
    }

    return pSbm->pWriteAddr;
}

static void* SbmFrameGetBufferDataInfo(SbmInterface* pSelf)
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    FramePicInfo* pFramePic = NULL;

    if(pSbm == NULL )
    {
        loge("pSbm == NULL.");
        return NULL;
    }

    if(lock(pSbm) != 0)
    {
        return NULL;
    }

    if(pSbm->mFramePicFifo.nUnReadFramePicNum == 0)
    {
        logv("nUnReadFrameNum == 0.");
        unlock(pSbm);
        return NULL;
    }
    pFramePic = &pSbm->mFramePicFifo.pFramePics[pSbm->mFramePicFifo.nFPReadPos];
    if(pFramePic == NULL)
    {
        loge("request failed.");
        unlock(pSbm);
        return NULL;
    }

    unlock(pSbm);
    return pFramePic->pVideoInfo;
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
static int SbmFrameRequestBuffer(SbmInterface* pSelf, int nRequireSize,
                                  char **ppBuf, int *pBufSize)
{
    SbmFrame *pSbm = (SbmFrame*)pSelf;

    if(pSbm == NULL || ppBuf == NULL || pBufSize == NULL)
    {
        loge("input error.");
        return -1;
    }

    if(lock(pSbm) != 0)
        return -1;

    if(pSbm->frameFifo.nValidFrameNum >= pSbm->frameFifo.nMaxFrameNum)
    {
        logd("nValidFrameNum >= nMaxFrameNum.");
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
static int SbmFrameAddStream(SbmInterface* pSelf, VideoStreamDataInfo *pDataInfo)
{
    SbmFrame *pSbm = (SbmFrame*)pSelf;

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

    int nSemCnt = 0;
    if(sem_getvalue(&pSbm->streamDataSem, &nSemCnt) == 0)
    {
        if(nSemCnt == 0)
            sem_post(&pSbm->streamDataSem);
    }

    unlock(pSbm);
    return 0;
}

//* in fact, this is the function of requestFramePic
static VideoStreamDataInfo* SbmFrameRequestFramePic(SbmInterface* pSelf) //* requestFramePic
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    FramePicInfo* pFramePic = NULL;

    if(pSbm == NULL )
    {
        loge("the_222,pSbm == NULL.");
        return NULL;
    }

    if(lock(pSbm) != 0)
        return NULL;

    if(pSbm->mFramePicFifo.nUnReadFramePicNum == 0)
    {
        logv("nUnReadFramePicNum == 0.");
        unlock(pSbm);
        return NULL;
    }

    pFramePic = &pSbm->mFramePicFifo.pFramePics[pSbm->mFramePicFifo.nFPReadPos];
    if(pFramePic == NULL)
    {
        loge("request framePic failed");
        unlock(pSbm);
        return NULL;
    }

    pSbm->mFramePicFifo.nFPReadPos++;
    pSbm->mFramePicFifo.nUnReadFramePicNum--;
    if(pSbm->mFramePicFifo.nFPReadPos >= pSbm->mFramePicFifo.nMaxFramePicNum)
    {
        pSbm->mFramePicFifo.nFPReadPos = 0;
    }
    logv("sbm request stream, pos = %d, pFrame = %p, pts = %lld",
          pSbm->mFramePicFifo.nFPReadPos,pFramePic, pFramePic->nPts);
    unlock(pSbm);
    return (VideoStreamDataInfo*)pFramePic;
}

//* in fact, this is the function of returnFramePic
static int SbmFrameReturnFramePic(SbmInterface* pSelf,
                                                VideoStreamDataInfo *pDataInfo)//* returnFramePic
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    FramePicInfo* pFramePic = (FramePicInfo*)pDataInfo;
    int nReadPos;

    if(pSbm == NULL || pFramePic == NULL)
    {
        loge("input error.");
        return -1;
    }

    if(lock(pSbm) != 0)
        return -1;

    if(pSbm->mFramePicFifo.nValidFramePicNum == 0)
    {
        loge("nValidFrameNum == 0.");
        unlock(pSbm);
        return -1;
    }

    nReadPos = pSbm->mFramePicFifo.nFPReadPos;
    nReadPos--;
    if(nReadPos < 0)
    {
        nReadPos = pSbm->mFramePicFifo.nMaxFramePicNum - 1;
    }

    if(pFramePic != &pSbm->mFramePicFifo.pFramePics[nReadPos])
    {
        loge("wrong frame pic sequence.");
        abort();
    }

    pSbm->mFramePicFifo.nFPReadPos = nReadPos;
    pSbm->mFramePicFifo.nUnReadFramePicNum++;
    unlock(pSbm);
    return 0;
}

//* in fact, this is the function of flushFramePic
static int SbmFrameFlushFramePic(SbmInterface* pSelf,
                                               VideoStreamDataInfo *pDataInfo) //* flushFramePic
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;

    FramePicInfo* pFramePic = (FramePicInfo*)pDataInfo;
    int nFlushPos;

    if(pSbm == NULL || pFramePic == NULL)
    {
        loge("input error");
        return -1;
    }

    if(lock(pSbm) != 0)
        return -1;

    if(pSbm->mFramePicFifo.nValidFramePicNum == 0)
    {
        logw("nValidFrameNum == 0.");
        unlock(pSbm);
        return -1;
    }
    nFlushPos = pSbm->mFramePicFifo.nFPFlushPos;
    logv("the_333,sbm flush stream , pos = %d, pFrame = %p, %p",nFlushPos,
          pFramePic, &pSbm->mFramePicFifo.pFramePics[nFlushPos]);
    if(pFramePic != &pSbm->mFramePicFifo.pFramePics[nFlushPos])
    {
        loge("not current nFlushPos.");
        abort();
    }

    nFlushPos++;
    if(nFlushPos >= pSbm->mFramePicFifo.nMaxFramePicNum)
    {
        nFlushPos = 0;
    }

    pSbm->mFramePicFifo.nValidFramePicNum--;
    pSbm->mFramePicFifo.nFPFlushPos = nFlushPos;
    pSbm->nValidDataSize -= pFramePic->nlength;

    int nSemCnt = 0;
    if(sem_getvalue(&pSbm->emptyFramePicSem, &nSemCnt) == 0)
    {
        if(nSemCnt == 0)
            sem_post(&pSbm->emptyFramePicSem);
    }
    unlock(pSbm);
    return 0;
}

static int SbmFrameSetEos(SbmInterface* pSelf, int nEosFlag)
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    if(pSbm == NULL)
    {
        logw("set eos failed");
        return -1;
    }
    pSbm->nEosFlag = nEosFlag;

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
VideoStreamDataInfo *requestStream(SbmFrame *pSbm)
{
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
/*
    logv("*** reqeust stream, pDataInfo = %p, pos = %d",pDataInfo, pSbm->frameFifo.nReadPos - 1);
    logv("*** reqeust stream, data: %x %x %x %x %x %x %x %x ",
         pDataInfo->pData[0], pDataInfo->pData[1],pDataInfo->pData[2],pDataInfo->pData[3],
         pDataInfo->pData[4],pDataInfo->pData[5],pDataInfo->pData[6],pDataInfo->pData[7]);
*/
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
int returnStream(SbmFrame* pSbm , VideoStreamDataInfo *pDataInfo)
{
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
int flushStream(SbmFrame *pSbm, VideoStreamDataInfo *pDataInfo, int bFlush)
{
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
        loge("no valid frame., flush pos = %d, pDataInfo = %p",
             pSbm->frameFifo.nFlushPos, pDataInfo);
        unlock(pSbm);
        return -1;
    }

    nFlushPos = pSbm->frameFifo.nFlushPos;

    logv("flush stream, pDataInfo = %p, pos = %d, %p",pDataInfo, nFlushPos, \
            &pSbm->frameFifo.pFrames[nFlushPos]);
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
    if(bFlush)
       pSbm->nValidDataSize     -= pDataInfo->nLength;
    pSbm->frameFifo.nFlushPos = nFlushPos;   //*
    unlock(pSbm);
    return 0;
}


FramePicInfo* requestEmptyFramePic(SbmFrame* pSbm)
{
    int nWritePos = -1;
    FramePicInfo* pFramePic = NULL;

    if(pSbm == NULL)
    {
        logd("pSbm == NULL.");
        return NULL;
    }

    if(lock(pSbm) != 0)
        return NULL;

    if(pSbm->mFramePicFifo.nValidFramePicNum >= pSbm->mFramePicFifo.nMaxFramePicNum)
    {
        logv("no emptye framePic");
        unlock(pSbm);
        return NULL;
    }

    nWritePos = pSbm->mFramePicFifo.nFPWritePos;
    pFramePic = &pSbm->mFramePicFifo.pFramePics[nWritePos];

    unlock(pSbm);
    logv("request empty frame pic, pos = %d, pFramePic = %p",nWritePos, pFramePic);
    return pFramePic;
}

int addFramePic(SbmFrame* pSbm, FramePicInfo* pFramePic) //* addFramePic
{
    int nWritePos = -1;

    if(pSbm == NULL || pFramePic == NULL)
    {
        loge("the_444,error input");
        return -1;
    }

    if(lock(pSbm) != 0)
        return -1;

    if(pSbm->mFramePicFifo.nValidFramePicNum >= pSbm->mFramePicFifo.nMaxFramePicNum)
    {
        loge("the_444,nValidFrameNum >= nMaxFrameNum.");
        unlock(pSbm);
        return -1;
    }

    nWritePos = pSbm->mFramePicFifo.nFPWritePos;
    if(pFramePic != &pSbm->mFramePicFifo.pFramePics[nWritePos])
    {
        loge("the_444, frame pic is not match: %p, %p, %d",
              pFramePic, &pSbm->mFramePicFifo.pFramePics[nWritePos], nWritePos);
        abort();
    }

    nWritePos++;
    if(nWritePos >= pSbm->mFramePicFifo.nMaxFramePicNum)
    {
        nWritePos = 0;
    }

    pSbm->mFramePicFifo.nFPWritePos = nWritePos;
    pSbm->mFramePicFifo.nValidFramePicNum++;
    pSbm->mFramePicFifo.nUnReadFramePicNum++;
    unlock(pSbm);
    return 0;
}


VideoStreamDataInfo* SbmFrameRequestFramePicNew(SbmInterface* pSelf) //* requestFramePic
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    SbmFrameBufferNode* pSbmFrameBufferNode = NULL;

    if(pthread_mutex_lock(&pSbm->pSbmFrameBufferMutex)< 0)
    {
        abort();
        return NULL;
    }
    pSbmFrameBufferNode =
       (SbmFrameBufferNode*)FIFODequeue((FiFoQueueInst**)&pSbm->pSbmFrameBufferValidQueue);

    if(pSbmFrameBufferNode != NULL)
    {
        pSbm->pSbmFrameBufferLastNode = pSbmFrameBufferNode;
        pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
        return (VideoStreamDataInfo*)(pSbmFrameBufferNode->pFramePic);
    }
    else
    {
        pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
        return NULL;
    }
}

//* in fact, this is the function of returnFramePic
int SbmFrameReturnFramePicNew(SbmInterface* pSelf,
                                                VideoStreamDataInfo *pDataInfo)//* returnFramePic
{
    SbmFrame* pSbm = (SbmFrame*)pSelf;
    //FramePicInfo* pFramePicInf = NULL;

    if(pSbm->pSbmFrameBufferLastNode == NULL)
    {
        loge("*********error:pSbm->pSbmFrameBufferLastNode=NULL\n");
        return -1;
    }

    if(pSbm->pSbmFrameBufferLastNode->pFramePic != (FramePicInfo*)pDataInfo)
    {
        loge("*********error:     \
          pSbm->pSbmFrameBufferLastNode->pFramePic != (FramePicInfo*)pDataInfo)\n");
        return -1;
    }

    if(pthread_mutex_lock(&pSbm->pSbmFrameBufferMutex)< 0)
    {
        loge("flush stream failed\n");
        return -1;
    }
    logd("************SbmFrameReturnFramePicNew\n");
    FIFOEnqueueToHead((FiFoQueueInst**)&pSbm->pSbmFrameBufferValidQueue,
      (FiFoQueueInst*)pSbm->pSbmFrameBufferLastNode);
    pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
    pSbm->pSbmFrameBufferLastNode = NULL;
    return 0;
}

//* in fact, this is the function of flushFramePic
int SbmFrameFlushFramePicNew(SbmInterface* pSelf,
                                               VideoStreamDataInfo *pDataInfo) //* flushFramePic
{
    int i = 0;
    SbmFrame* pSbm = (SbmFrame*)pSelf;

    FramePicInfo* pFramePic = (FramePicInfo*)pDataInfo;

    SbmFrameBufferNode* pSbmFrameBufferNode = NULL;

    for(i=0; i<MAX_FRAME_BUFFER_NUM; i++)
    {
        if(pFramePic->pSbmFrameBufferManager ==
            pSbm->pSbmFrameBuffer[i].pFramePic->pSbmFrameBufferManager)
        {
            break;
        }
    }
    pSbmFrameBufferNode = &(pSbm->pSbmFrameBuffer[i]);
    if(pthread_mutex_lock(&pSbm->pSbmFrameBufferMutex)< 0)
    {
        abort();
        return -1;
    }
    FIFOEnqueue((FiFoQueueInst**)&(pSbm->pSbmFrameBufferEmptyQueue),
        (FiFoQueueInst*)pSbmFrameBufferNode);
    pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
    SbmFrameFlushFramePic(pSelf, pDataInfo);
    return 0;
}

void CopySbmFrameData(SbmFrame* pSbm)
{
    FramePicInfo* pFramePic = NULL;
    SbmInterface* pSelf = NULL;
    char* pLastDataBuf = NULL;
    pSelf = (SbmInterface*)pSbm;
    SbmFrameBufferNode* pSbmFrameBufferNode = NULL;
    SbmFrameBufferManager*pPStreamManager = NULL;
    size_addr phyAddr = 0;
    size_addr highPhyAddr = 0;
    size_addr lowPhyAddr = 0;

    if(lock(pSbm) != 0)
        return;
    if(pSbm->mFramePicFifo.nUnReadFramePicNum == 0)
    {
        logv("nUnReadFramePicNum == 0.");

        if(pSbm->nEosFlag ==1)
        {
            //loge("*************xyliu: reach the file end\n");
        }
        unlock(pSbm);
        return;
    }
    unlock(pSbm);

    if(pthread_mutex_lock(&pSbm->pSbmFrameBufferMutex)< 0)
    {
        return;
    }
    pSbmFrameBufferNode =
       (SbmFrameBufferNode*)FIFODequeue((FiFoQueueInst**)&pSbm->pSbmFrameBufferEmptyQueue);
    pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
    if(pSbmFrameBufferNode == NULL)
    {
       return;
    }

    pFramePic = (FramePicInfo*)SbmFrameRequestFramePic(pSelf);
    if(pFramePic == NULL)
    {
        if(pthread_mutex_lock(&pSbm->pSbmFrameBufferMutex)< 0)
        {
            abort();
            return;
        }
        FIFOEnqueueToHead((FiFoQueueInst**)&pSbm->pSbmFrameBufferEmptyQueue,
          (FiFoQueueInst*)pSbmFrameBufferNode);
        pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
    }
    else
    {

        pPStreamManager = pSbmFrameBufferNode->pSbmBufferManager;
        if(pFramePic->nlength > (int)pPStreamManager->nFrameBufferSize)
        {
            loge("*********pFramePic->nlength=%d\n", pFramePic->nlength);
            CdcMemPfree(pSbm->mConfig.memops, pPStreamManager->pFrameBuffer,
                pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);
            pPStreamManager->nFrameBufferSize = (pFramePic->nlength+1023) &~1023;
            pPStreamManager->pFrameBuffer =    \
                CdcMemPalloc(pSbm->mConfig.memops,pPStreamManager->nFrameBufferSize,    \
                pSbm->mConfig.veOpsS, pSbm->mConfig.pVeOpsSelf);

            if(pPStreamManager->pFrameBuffer == NULL)
            {
                abort();
            }

            phyAddr = (size_addr)(CdcMemGetPhysicAddress(pSbm->mConfig.memops,
                      (void*)pPStreamManager->pFrameBuffer));
            if(pSbm->sbmInterface.nType == SBM_TYPE_FRAME_AVC)
            {
                highPhyAddr = (phyAddr>>28) & 0x0f;
                lowPhyAddr =  phyAddr & 0x0ffffff0;
                pPStreamManager->pPhyFrameBuffer = lowPhyAddr+highPhyAddr;
            }
            else if(pSbm->sbmInterface.nType == SBM_TYPE_FRAME)
            {
                pPStreamManager->pPhyFrameBuffer = phyAddr;
            }
            pPStreamManager->pPhyFrameBufferEnd =
                (u32)(phyAddr+pPStreamManager->nFrameBufferSize-1);
        }

		int validDataSize1 = 0;
		int validDataSize2 = 0;

        if((pFramePic->pDataStartAddr+pFramePic->nlength) <= pSbm->pStreamBufferEnd)
        {
            validDataSize1 = pFramePic->nlength;
            validDataSize2 = 0;
        }
        else
        {
            validDataSize1 = pSbm->pStreamBufferEnd-pFramePic->pDataStartAddr+1;
            validDataSize2 = pFramePic->nlength - validDataSize1;
        }

        CdcMemCopy(pSbm->mConfig.memops,pPStreamManager->pFrameBuffer,
            pFramePic->pDataStartAddr, validDataSize1);
        if(validDataSize2 != 0)
        {
            CdcMemCopy(pSbm->mConfig.memops,
                pPStreamManager->pFrameBuffer+validDataSize1,
                pSbm->pStreamBuffer, validDataSize2);
        }
        CdcMemFlushCache(pSbm->mConfig.memops,pPStreamManager->pFrameBuffer,
            pFramePic->nlength);

        pSbmFrameBufferNode->pFramePic = pFramePic;
        pSbmFrameBufferNode->pFramePic->pSbmFrameBufferManager = pPStreamManager;

		int i = 0;
		int offset = 0;
        for(i=0; i<pFramePic->nCurNaluIdx; i++)
        {
            pLastDataBuf = pFramePic->pNaluInfoList[i].pDataBuf;

            if(pLastDataBuf >= pFramePic->pDataStartAddr)
            {
                offset = pLastDataBuf-pFramePic->pDataStartAddr;
            }
            else
            {
                offset = pLastDataBuf+pSbm->nStreamBufferSize-pFramePic->pDataStartAddr;
            }

            pSbmFrameBufferNode->pFramePic->pNaluInfoList[i].pDataBuf =
                pPStreamManager->pFrameBuffer+offset;
        }

        if(pthread_mutex_lock(&pSbm->pSbmFrameBufferMutex)< 0)
        {
            loge("************error: wait the lock failed\n");
            abort();
            return;
        }
        FIFOEnqueue((FiFoQueueInst**)&pSbm->pSbmFrameBufferValidQueue,
         (FiFoQueueInst*)pSbmFrameBufferNode);
        pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
     }
}


static void* ProcessThread(void* pThreadData)
{
    SbmFrame* pSbm = (SbmFrame*)pThreadData;
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;
    CdcMessage msg;
    memset(&msg, 0, sizeof(CdcMessage));

    while(1)
    {
        if(CdcMessageQueueTryGetMessage(pSbm->mq, &msg, 20) == 0)
        {
            //* process message here
            if(msg.messageId == SBM_THREAD_CMD_QUIT)
            {
                goto EXIT;
            }
            else if(msg.messageId == SBM_THREAD_CMD_READ)
            {
                if(pSbm->bStreamWithStartCode == -1)
                {
                    pSbm->checkBitStreamType(pSbm);
                }
                else
                {
                    pSbm->detectOneFramePic(pSbm);
                }

                if(pSbm->sbmInterface.bUseNewVeMemoryProgram == 1)
                {
                    CopySbmFrameData(pSbm);
                }

                if(CdcMessageQueueGetCount(pSbm->mq) <= 0)
                {
                    msg.messageId = SBM_THREAD_CMD_READ;
                    CdcMessageQueuePostMessage(pSbm->mq, &msg);
                }
            }
            else if(msg.messageId == SBM_THREAD_CMD_RESET)
            {
                logd("*** post reset sem");
                if(pDetectInfo->pCurStream)
                {
                    flushStream(pSbm, pDetectInfo->pCurStream, 0);
                    pDetectInfo->pCurStream = NULL;
                }

                lock(pSbm);

                pDetectInfo->bCurFrameStartCodeFound = 0;
                pDetectInfo->nCurStreamDataSize = 0;
                pDetectInfo->nCurStreamRebackFlag = 0;
                pDetectInfo->pCurStreamDataptr = NULL;

                if(pDetectInfo->pCurFramePic)
                {
                   pDetectInfo->pCurFramePic = NULL;
                }

                pSbm->pWriteAddr                 = pSbm->pStreamBuffer;
                pSbm->nValidDataSize             = 0;

                pSbm->frameFifo.nReadPos         = 0;
                pSbm->frameFifo.nWritePos        = 0;
                pSbm->frameFifo.nFlushPos        = 0;
                pSbm->frameFifo.nValidFrameNum   = 0;
                pSbm->frameFifo.nUnReadFrameNum  = 0;

                pSbm->mFramePicFifo.nFPFlushPos = 0;
                pSbm->mFramePicFifo.nFPReadPos  = 0;
                pSbm->mFramePicFifo.nFPWritePos = 0;
                pSbm->mFramePicFifo.nUnReadFramePicNum = 0;
                pSbm->mFramePicFifo.nValidFramePicNum = 0;
                unlock(pSbm);
                if(pSbm->sbmInterface.bUseNewVeMemoryProgram == 1)
                {
                    pthread_mutex_lock(&pSbm->pSbmFrameBufferMutex);
                    SbmFrameBufferNode* pSbmFrameBufferNode = NULL;
                    while(1)
                    {
                        pSbmFrameBufferNode =                \
                            (SbmFrameBufferNode*)FIFODequeue((FiFoQueueInst**)&pSbm->pSbmFrameBufferValidQueue);
                        if(pSbmFrameBufferNode != NULL)
                        {
                            FIFOEnqueue((FiFoQueueInst**)&pSbm->pSbmFrameBufferEmptyQueue,
                             (FiFoQueueInst*)pSbmFrameBufferNode);
                        }
                        else
                        {
                            break;
                        }
                        usleep(1);
                    }
                    pthread_mutex_unlock(&pSbm->pSbmFrameBufferMutex);
                }

                sem_t* replySem = (sem_t*)msg.params[0];
                sem_post(replySem);
                //* do nothing
            }
        }

    }

EXIT:
    logd(" exit sbm thread ");
    return NULL;

}

SbmInterface* GetSbmInterfaceFrame(int nType)
{
    logd("******* sbm-type: Frame*******");
    SbmFrame* pSbmFrame = NULL;
    pSbmFrame = (SbmFrame*)malloc(sizeof(SbmFrame));
    if(pSbmFrame == NULL)
    {
        loge("malloc for sbm frame struct failed");
        return NULL;
    }

    memset(pSbmFrame, 0, sizeof(SbmFrame));

    pSbmFrame->sbmInterface.init    = SbmFrameInit;
    pSbmFrame->sbmInterface.destroy = SbmFrameDestroy;
    pSbmFrame->sbmInterface.reset   = SbmFrameReset;
    pSbmFrame->sbmInterface.getBufferSize         = SbmFrameGetBufferSize;
    pSbmFrame->sbmInterface.getStreamDataSize     = SbmFrameGetStreamDataSize;
    pSbmFrame->sbmInterface.getStreamFrameNum     = SbmFrameGetStreamFrameNum;
    pSbmFrame->sbmInterface.getBufferAddress      = SbmFrameGetBufferAddress;
    pSbmFrame->sbmInterface.getBufferWritePointer = SbmFrameGetBufferWritePointer;
    pSbmFrame->sbmInterface.getBufferDataInfo = SbmFrameGetBufferDataInfo;
    pSbmFrame->sbmInterface.requestBuffer     = SbmFrameRequestBuffer;
    pSbmFrame->sbmInterface.addStream         = SbmFrameAddStream;
#if ENABLE_NEW_MEMORY_OPTIMIZATION_PROGRAM
    pSbmFrame->sbmInterface.requestStream     = SbmFrameRequestFramePicNew;
    pSbmFrame->sbmInterface.returnStream      = SbmFrameReturnFramePicNew;
    pSbmFrame->sbmInterface.flushStream       = SbmFrameFlushFramePicNew;
    pSbmFrame->sbmInterface.bUseNewVeMemoryProgram = 1;
#else
    pSbmFrame->sbmInterface.requestStream     = SbmFrameRequestFramePic;
    pSbmFrame->sbmInterface.returnStream      = SbmFrameReturnFramePic;
    pSbmFrame->sbmInterface.flushStream       = SbmFrameFlushFramePic;
    pSbmFrame->sbmInterface.bUseNewVeMemoryProgram = 0;
#endif
    pSbmFrame->sbmInterface.setEos            = SbmFrameSetEos;

    pSbmFrame->sbmInterface.nType = nType;

    return &pSbmFrame->sbmInterface;
}

SbmInterface* GetSbmInterface(int nType)
{
    logd("*********GetSbmInterface, nType=%d\n", nType);
    switch(nType)
    {
        case SBM_TYPE_STREAM:
        {
            return GetSbmInterfaceStream();
        }
        case SBM_TYPE_FRAME:
        case SBM_TYPE_FRAME_AVC:
        {
            return GetSbmInterfaceFrame(nType);
        }
        default:
        {
            loge("not support the sbm interface type = %d",nType);
            return NULL;
        }
    }
    return NULL;
}
