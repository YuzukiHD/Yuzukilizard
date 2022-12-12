/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : AudioDec_Component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/28
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "AudioDec_Component"
#include <utils/plat_log.h>

//ref platform headers
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include <assert.h>

#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "SystemBase.h"
#include "mm_common.h"
#include "mm_comm_adec.h"
#include "mpi_adec.h"

//media internal common headers.
#include "media_common.h"
#include <media_common_acodec.h>
#include "mm_component.h"
#include "ComponentCommon.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include "adecoder.h"
#include <AdecCompStream.h>
#include <AdecStream.h>
#include <EncodedStream.h>
#include <DemuxCompStream.h>
#include "AudioDec_Component.h"
#include <CDX_Common.h>
#include <CdxTypes.h>
//------------------------------------------------------------------------------------
//#define DEBUG_INPORT_BUFFER_INFO
static void* ComponentThread(void* pThreadData);
static void* InputThread(void* pThreadData);

#define INPUT_BUF_COUNT (1)
ERRORTYPE AudioDec_InputDataInit(AUDIODEC_INPUT_DATA *pInputData, AUDIODECDATATYPE *pAudioDecData)
{
    ERRORTYPE eError = SUCCESS;
    memset(pInputData, 0x0, sizeof(AUDIODEC_INPUT_DATA));
    // Init input thread data
    pInputData->state = COMP_StateLoaded;
    pthread_mutex_init(&pInputData->mStateLock, NULL);
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    int err = pthread_cond_init(&pInputData->mStateCond, &condAttr);
    if (err != 0)
    {
        aloge("fatal error! input thread mStateCond init fail!");
    }
    err = pthread_cond_init(&pInputData->mAbsFullCond, &condAttr);
    if (err != 0) 
    {
        aloge("fatal error! pthread cond init fail!");
    }

    pInputData->mWaitAbsValidFlag = FALSE;
    pInputData->nRequestLen = 0;
    
    INIT_LIST_HEAD(&pInputData->mIdleAbsList);
    INIT_LIST_HEAD(&pInputData->mReadyAbsList);
    INIT_LIST_HEAD(&pInputData->mUsingAbsList);
    pthread_mutex_init(&pInputData->mAbsListLock, NULL);
    
    DMXPKT_NODE_T *pNode;
    int i;
    pthread_mutex_lock(&pInputData->mAbsListLock);
    for (i = 0; i < INPUT_BUF_COUNT; i++)
    {
        pNode = (DMXPKT_NODE_T*)malloc(sizeof(DMXPKT_NODE_T));
        if(pNode == NULL)
        {
            aloge("fatal error! malloc fail!");
            eError = ERR_ADEC_NOMEM;
            pthread_mutex_unlock(&pInputData->mAbsListLock);
            goto EXIT6;
        }
        memset(pNode, 0, sizeof(DMXPKT_NODE_T));
        list_add_tail(&pNode->mList, &pInputData->mIdleAbsList);
        pInputData->mNodeNum++;
    }
    pthread_mutex_unlock(&pInputData->mAbsListLock);
    
    pInputData->pAudioDecData = pAudioDecData;

    if (message_create(&pInputData->cmd_queue)<0)
    {
        aloge("create input cmd queue error!");
        eError = ERR_ADEC_NOMEM;
        goto EXIT6;
    }
    
    err = pthread_create(&pInputData->thread_id, NULL, InputThread, pInputData);
    if (err || !pInputData->thread_id)
    {
        eError = ERR_ADEC_NOMEM;
        goto EXIT7;
    }
    pthread_condattr_destroy(&condAttr);
    return SUCCESS;

EXIT7:
    message_destroy(&pInputData->cmd_queue);
EXIT6:
{
    DMXPKT_NODE_T *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pInputData->mIdleAbsList, mList)
    {
        list_del(&pEntry->mList);
        free(pEntry);
    }
}
    pthread_mutex_destroy(&pInputData->mStateLock);
    pthread_condattr_destroy(&condAttr);
    pthread_cond_destroy(&pInputData->mStateCond);
    pthread_cond_destroy(&pInputData->mAbsFullCond);
    pthread_mutex_destroy(&pInputData->mAbsListLock);
    return eError;
}

ERRORTYPE AudioDec_InputDataDestroy(AUDIODEC_INPUT_DATA *pInputData)
{
    ERRORTYPE eError = SUCCESS;
    message_t msg;
    msg.command = Stop;
    put_message(&pInputData->cmd_queue, &msg);
    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pInputData->thread_id, (void*)&eError);
    message_destroy(&pInputData->cmd_queue);

    DMXPKT_NODE_T *pEntry, *pTmp;
    pthread_mutex_lock(&pInputData->mAbsListLock);
    if (!list_empty(&pInputData->mIdleAbsList)) 
    {
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mIdleAbsList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    if (!list_empty(&pInputData->mReadyAbsList)) 
    {
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mReadyAbsList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    if (!list_empty(&pInputData->mUsingAbsList)) 
    {
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mUsingAbsList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pInputData->mAbsListLock);
    
    pthread_mutex_destroy(&pInputData->mAbsListLock);
    pthread_mutex_destroy(&pInputData->mStateLock);
    pthread_cond_destroy(&pInputData->mStateCond);
    pthread_cond_destroy(&pInputData->mAbsFullCond);

    return SUCCESS;
}

/*****************************************************************************/
ERRORTYPE AudioDecSendCommand(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd,
        PARAM_IN unsigned int nParam1,
        PARAM_IN void* pCmdData)
{
    ERRORTYPE eError = SUCCESS;

    // Get component private data
    AUDIODECDATATYPE* pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (!pAudioDecData)
    {
        eError = ERR_ADEC_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    // Check state
    if (pAudioDecData->state == COMP_StateInvalid)
    {
        eError = ERR_ADEC_SYS_NOTREADY;
        goto COMP_CONF_CMD_FAIL;
    }
    
    // Trans command
    CompInternalMsgType eCmd;
    switch (Cmd)
    {
        case COMP_CommandStateSet:
            eCmd = SetState;
            if(nParam1 == COMP_StateLoaded)
                pAudioDecData->force_exit = 1;
            break;
        case COMP_CommandFlush:
            eCmd = Flush;
            break;
        case COMP_CommandPortDisable:
            eCmd = StopPort;
            break;
        case COMP_CommandPortEnable:
            eCmd = RestartPort;
            break;
        default:
            alogw("impossible comp_command[0x%x]", Cmd);
            eCmd = -1;
            break;
    }

    message_t msg;
    msg.command = eCmd;
    msg.para0   = nParam1;
    put_message(&pAudioDecData->cmd_queue, &msg);

COMP_CONF_CMD_FAIL: 
    return eError;
}

ERRORTYPE AudioDecGetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    ERRORTYPE eError = SUCCESS;
    
    // Get component private data
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    
    if (pPortDef->nPortIndex == pAudioDecData->sInPortDef.nPortIndex)
    {
        memcpy(pPortDef, &pAudioDecData->sInPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else if (pPortDef->nPortIndex == pAudioDecData->sOutPortDef.nPortIndex)
    {
        memcpy(pPortDef, &pAudioDecData->sOutPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else
    {
        eError = ERR_ADEC_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE AudioDecGetPortExtraDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTEXTRADEFINITIONTYPE *pPortExtraDef)
{
    ERRORTYPE eError = SUCCESS;
    
    //Get component private data
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    
    if (pPortExtraDef->nPortIndex == pAudioDecData->sInPortExtraDef.nPortIndex)
    {
        memcpy(pPortExtraDef, &pAudioDecData->sInPortExtraDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else if (pPortExtraDef->nPortIndex == pAudioDecData->sOutPortExtraDef.nPortIndex) 
    {
        pAudioDecData->sOutPortExtraDef.pVendorInfo = pAudioDecData->sInPortExtraDef.pVendorInfo;
        memcpy(pPortExtraDef, &pAudioDecData->sOutPortExtraDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else
    {
        eError = ERR_ADEC_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE AudioDecSetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    ERRORTYPE eError = SUCCESS;
    
    // Get component private data
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (pPortDef->nPortIndex == pAudioDecData->sInPortDef.nPortIndex)
    {
        memcpy(&pAudioDecData->sInPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else if (pPortDef->nPortIndex == pAudioDecData->sOutPortDef.nPortIndex)
    {
        memcpy(&pAudioDecData->sOutPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else
    {
        eError = ERR_ADEC_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE AudioDecGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for (i = 0; i < MAX_ADECODER_PORTS; i++)
    {
        if (pAudioDecData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pAudioDecData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    
    if (bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_ADEC_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE AudioDecSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_ADECODER_PORTS; i++)
    {
        if(pAudioDecData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pAudioDecData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_ADEC_ILLEGAL_PARAM;
    }

    return eError;
}

ERRORTYPE AudioDecGetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT MPP_CHN_S *pChn)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(pChn, &pAudioDecData->mMppChnInfo);
    return SUCCESS;
}

ERRORTYPE AudioDecSetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pAudioDecData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE AudioDecGetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT ADEC_CHN_ATTR_S *pChnAttr)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChnAttr = pAudioDecData->mADecChnAttr;
    return SUCCESS;
}

ERRORTYPE AudioDecSetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN ADEC_CHN_ATTR_S *pChnAttr)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pAudioDecData->mADecChnAttr = *pChnAttr;
    return SUCCESS;
}

ERRORTYPE AudioDecResetChannel(PARAM_IN COMP_HANDLETYPE hComponent)
{
    // Get component private data
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    
    // Ensure component is in Idle state
    if (pAudioDecData->state != COMP_StateIdle) 
    {
        aloge("fatal error! must reset channel in stateIdle!");
        return ERR_ADEC_NOT_PERM;
    }
    
    // wait all outFrame return.
    alogd("wait ADec idleOutFrameList full");
    struct list_head *pList;
    pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
    pAudioDecData->mWaitOutFrameFullFlag = TRUE;
    while (1) 
    {
        int cnt = 0;
        list_for_each(pList, &pAudioDecData->mIdleOutFrameList) 
        {
            cnt++; 
        }
        
        if (cnt < pAudioDecData->mFrameNodeNum) 
        {
            alogd("wait idleOutFrameList [%d]nodes to home", pAudioDecData->mFrameNodeNum - cnt);
            pthread_cond_wait(&pAudioDecData->mOutFrameFullCondition, &pAudioDecData->mOutFrameListMutex);
        } 
        else 
        {
            break;
        }
    }
    pAudioDecData->mWaitOutFrameFullFlag = FALSE;
    pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
    alogd("wait ADec idleOutFrameList full done");
    return SUCCESS;
}

ERRORTYPE AudioDecGetTunnelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = ERR_ADEC_UNEXIST;

    if(pAudioDecData->sInPortTunnelInfo.nPortIndex == pTunnelInfo->nPortIndex)
    {
        memcpy(pTunnelInfo, &pAudioDecData->sInPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else if(pAudioDecData->sOutPortTunnelInfo.nPortIndex == pTunnelInfo->nPortIndex)
    {
        memcpy(pTunnelInfo, &pAudioDecData->sOutPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_ADEC_UNEXIST;
    }
    return eError;
}

/*****************************************************************************/
ERRORTYPE AudioDecGetState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_STATETYPE* pState)
{
    AUDIODECDATATYPE *pAudioDecData;
    ERRORTYPE eError = SUCCESS;

    pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pState = pAudioDecData->state;

    return eError;
}

/*****************************************************************************/
ERRORTYPE AudioDecSetCallbacks(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_CALLBACKTYPE* pCallbacks,
        PARAM_IN void* pAppData)
{
    AUDIODECDATATYPE *pAudioDecData;
    ERRORTYPE eError = SUCCESS;

    pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (!pAudioDecData || !pCallbacks || !pAppData)
    {
        eError = ERR_ADEC_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    pAudioDecData->pCallbacks = pCallbacks;
    pAudioDecData->pAppData   = pAppData;

    COMP_CONF_CMD_FAIL: return eError;
}


/*****************************************************************************/
ERRORTYPE AudioDecGetCacheState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_BUFFERSTATE* pBufState)
{
    AUDIODECDATATYPE *pAudioDecData;
    ERRORTYPE eError = SUCCESS;
    int validPercent;
    int nElementInVbv;

    pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    BsQueryQuality(pAudioDecData->pDecoder, &validPercent, &nElementInVbv);
    pBufState->nElementCounter   = nElementInVbv;
    pBufState->nValidSizePercent = validPercent;

    return eError;
}

static ERRORTYPE config_AUDIO_FRAME_S_by_ADecCompOutputFrame(AUDIO_FRAME_S *dst, ADecCompOutputFrame *src)
{
    dst->mpAddr     = src->mpPcmBuf;
    dst->mLen       = src->mSize;
    dst->mTimeStamp = src->mTimeStamp;
    dst->mId        = src->mId;
    dst->mBitwidth  = (AUDIO_BIT_WIDTH_E)(src->mBitsPerSample / 8 - 1);
    dst->mSoundmode = (src->mChannelNum == 1)?AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    dst->mSamplerate= src->mSampleRate;
    return SUCCESS;
}

ERRORTYPE AudioDecSetStreamEof(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    alogv("adec end flag is set");
    pAudioDecData->priv_flag |= CDX_comp_PRIV_FLAGS_STREAMEOF;
    message_t msg;
    msg.command = ADecComp_AbsAvailable;
    put_message(&pAudioDecData->cmd_queue, &msg);
    return SUCCESS;
}

ERRORTYPE AudioDecClearStreamEof(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    alogv("adec end flag is clear");
    pAudioDecData->priv_flag &= ~(CDX_comp_PRIV_FLAGS_STREAMEOF);
    return SUCCESS;
}

/**
 * send stream to Adeclib, can used in tunnel/non-tunnel mode.
 * currently, non-tunnel use EmptyThisBuffer(), tunnel use RequstBuffer() and ReleaseBuffer()
 *
 * @return SUCCESS
 * @param hComponent adecComp handle.
 * @param pBuffer stream info.
 */
ERRORTYPE AudioDecEmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    
    int                 cmddata;
    CompInternalMsgType cmd;
    message_t           cmd_msg;
    DMXPKT_NODE_T* pEntry = NULL;
    
    // Get component private data
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    AUDIODEC_INPUT_DATA* pInputData = pAudioDecData->pInputData;

    pthread_mutex_lock(&pAudioDecData->mStateLock);
    
    // Ensure component is in Executing or Pause state
    if (pAudioDecData->state != COMP_StateExecuting && pAudioDecData->state != COMP_StatePause) 
    {
        alogw("send stream when adec state[0x%x] is not executing/pause", pInputData->state);
    }
    // pass bs buffer to Decode lib in inport tunnel-mode
    // Notes: data struct is different in tunnel and non-tunnel mode!
    if (pAudioDecData->mInputPortTunnelFlag)
    {
        // check port index
        if (pBuffer->nInputPortIndex == pAudioDecData->sInPortTunnelInfo.nPortIndex)
        {
            // Get buffer pointer from demux component
            EncodedStream* pDmxOutBuf = (EncodedStream*)pBuffer->pOutputPortPrivate;

            // Check it is the same buffer in mUsingAbsList
            pthread_mutex_lock(&pInputData->mAbsListLock);
            
            if (list_empty(&pInputData->mUsingAbsList))
            {
                pthread_mutex_unlock(&pInputData->mAbsListLock);
                pthread_mutex_unlock(&pAudioDecData->mStateLock);
                aloge("fatal error! Calling EmptyThisBuffer while mUsingAbsList is empty!");
                return FAILURE;
            }

            pEntry = list_first_entry(&pInputData->mUsingAbsList, DMXPKT_NODE_T, mList);
            if (  (pEntry->stEncodedStream.pBuffer != pDmxOutBuf->pBuffer)
               || (pEntry->stEncodedStream.pBufferExtra != pDmxOutBuf->pBufferExtra)
               )
           {
               pthread_mutex_unlock(&pInputData->mAbsListLock);
               pthread_mutex_unlock(&pAudioDecData->mStateLock);
               aloge("fatal error! the buffer in EmptyThisBuffer param is not same as in mUsingAbsList");
               return FAILURE;
           }
            
            
            if (-1 == pDmxOutBuf->nFilledLen)
            {// buffer is not enough for component A
                
                alogv("adec lib buffer is not enough for component A (nTobeFillLen = %d)", pDmxOutBuf->nTobeFillLen);
                pInputData->nRequestLen = pDmxOutBuf->nTobeFillLen;
                
                // Move node from mUsingAbsList to mIdleAbsList
                if (!list_empty(&pInputData->mUsingAbsList))
                {// moving from mReadyAbsList to mIdleAbsList
                    pEntry = list_first_entry(&pInputData->mUsingAbsList, DMXPKT_NODE_T, mList);
                    memset(&pEntry->stEncodedStream, 0, sizeof(EncodedStream));
                    list_move_tail(&pEntry->mList, &pInputData->mIdleAbsList);
                }
                    
                // Send Abs Valid message to input thread
                if (pInputData->mWaitAbsValidFlag)
                {
                    pInputData->mWaitAbsValidFlag = FALSE;
                    message_t msg;
                    msg.command = ADecComp_AbsAvailable;
                    put_message(&pInputData->cmd_queue, &msg);
                }
                // signal when all abs return to idleList
                if(pInputData->mbWaitAbsFullFlag)
                {
                    struct list_head *pList;
                    int cnt = 0;
                    list_for_each(pList, &pInputData->mIdleAbsList) { cnt++; }
                    if (cnt >= pInputData->mNodeNum) 
                    {
                        pInputData->mbWaitAbsFullFlag = FALSE;
                        pthread_cond_signal(&pInputData->mAbsFullCond);
                    }
                }
                pthread_mutex_unlock(&pInputData->mAbsListLock);
                pthread_mutex_unlock(&pAudioDecData->mStateLock);
                return eError;
            }

            //alogd("======= nFilledLen[%d]  nTimeStamp[%lld] nOffset[%d]",pDmxOutBuf->nFilledLen,pDmxOutBuf->nTimeStamp,pBuffer->nOffset);
            
            pthread_mutex_lock(&pAudioDecData->mAbsInputMutex);
            int adecRet = ParserUpdateBsBuffer
                (pAudioDecData->pDecoder
                ,pDmxOutBuf->nFilledLen
                ,pDmxOutBuf->nTimeStamp
                ,pBuffer->nOffset
                );
            pthread_mutex_unlock(&pAudioDecData->mAbsInputMutex);
            
            if (adecRet == 0)
            {// Send buf to decode lib success
                eError = SUCCESS;
                if (!list_empty(&pInputData->mUsingAbsList))
                {
                    // Move node from mUsingAbsList to mIdleAbsList
                    pEntry = list_first_entry(&pInputData->mUsingAbsList, DMXPKT_NODE_T, mList);
                    list_move_tail(&pEntry->mList, &pInputData->mIdleAbsList);
                    pthread_mutex_unlock(&pInputData->mAbsListLock);
                    
                    // Send Abs Valid message to work thread
                    if (pAudioDecData->mWaitAbsInputFlag) 
                    {
                        pAudioDecData->mWaitAbsInputFlag = FALSE;
                        message_t msg;
                        msg.command = ADecComp_AbsAvailable;
                        put_message(&pAudioDecData->cmd_queue, &msg);
                    }
                    
                    // Send Abs Valid message to input thread
                    if (pInputData->mWaitAbsValidFlag)
                    {
                        pInputData->mWaitAbsValidFlag = FALSE;
                        message_t msg;
                        msg.command = ADecComp_AbsAvailable;
                        put_message(&pInputData->cmd_queue, &msg);
                    }
                    // signal when all abs return to idleList
                    if(pInputData->mbWaitAbsFullFlag)
                    {
                        struct list_head *pList;
                        int cnt = 0;
                        list_for_each(pList, &pInputData->mIdleAbsList) { cnt++; }
                        if (cnt >= pInputData->mNodeNum) 
                        {
                            pInputData->mbWaitAbsFullFlag = FALSE;
                            pthread_cond_signal(&pInputData->mAbsFullCond);
                        }
                    }
                }
                else
                {
                    pthread_mutex_unlock(&pInputData->mAbsListLock);
                    pthread_mutex_unlock(&pAudioDecData->mStateLock);
                    aloge("fatal error! No Using Abs node while calling EmptyThisBuffer!");
                    return FAILURE;
                }
            }
            else
            {
                aloge("fatal error! submit data fail, check code!");
                eError = FAILURE;
            }
        }
        else
        {
            aloge("fatal error! PortIndex[%u][%u]! fill Abs fail!", pBuffer->nInputPortIndex, pAudioDecData->sInPortTunnelInfo.nPortIndex);
            eError = FAILURE;
        }

        pthread_mutex_unlock(&pAudioDecData->mStateLock);
        return eError;
    }
    else
    {
        AdecInputStream *pInputStream = (AdecInputStream*)pBuffer->pAppPrivate;
        AUDIO_STREAM_S *pStream = pInputStream->pStream;
        int nMilliSec = pInputStream->nMilliSec;
        if (0 == pStream->mLen)
        {
            alogd("indicate EndOfStream");
            AudioDecSetStreamEof(hComponent);
            pthread_mutex_unlock(&pAudioDecData->mStateLock);
            return FAILURE;
        }

        EncodedStream DmxOutBuf;
        memset(&DmxOutBuf, 0, sizeof(EncodedStream));
        // send stream to adeclib.
        pthread_mutex_lock(&pAudioDecData->mAbsInputMutex);
_TryToRequestAbs:
#ifdef DEBUG_INPORT_BUFFER_INFO
        int bsValidSize = AudioStreamDataSize(pAudioDecData->pDecoder); // inport bs data_size that can be decode but have not be decoded
        int bsTotalSize = AudioStreamBufferSize();                      // inport bs space total size: 128KB
        int bsEmptySize = bsTotalSize - bsValidSize;
        alogd("request ADecLib bs buffer. totalSz:%d, validSz:%d, emptySz:%d, requestSz:%d", bsTotalSize, bsValidSize, bsEmptySize, pStream->mLen);
#endif

        // Request buffer from decode lib
        DmxOutBuf.nTobeFillLen = pStream->mLen;
        int ret = ParserRequestBsBuffer
            (pAudioDecData->pDecoder
            ,DmxOutBuf.nTobeFillLen
            ,&DmxOutBuf.pBuffer
            ,(int*)&DmxOutBuf.nBufferLen
            ,&DmxOutBuf.pBufferExtra
            ,(int*)&DmxOutBuf.nBufferExtraLen
            ,(int *)&pBuffer->nOffset
            );
        if (0 == ret)
        {
            if (pStream->mLen > DmxOutBuf.nBufferLen)
            {
                memcpy(DmxOutBuf.pBuffer, pStream->pStream, DmxOutBuf.nBufferLen);
                memcpy(DmxOutBuf.pBufferExtra, pStream->pStream + DmxOutBuf.nBufferLen, pStream->mLen - DmxOutBuf.nBufferLen);
            }
            else
            {
                memcpy(DmxOutBuf.pBuffer, pStream->pStream, pStream->mLen);
            }

            // Send data to decode lib
            ret = ParserUpdateBsBuffer(pAudioDecData->pDecoder, DmxOutBuf.nTobeFillLen, pStream->mTimeStamp, pBuffer->nOffset);
            if (pAudioDecData->mWaitAbsInputFlag)
            {
                pAudioDecData->mWaitAbsInputFlag = FALSE;
                message_t msg;
                msg.command = ADecComp_AbsAvailable;
                put_message(&pAudioDecData->cmd_queue, &msg);
            }
            eError = SUCCESS;
        } 
        else 
        {// get buffer from decode lib fail, try again
            if (0 == nMilliSec)
            {
                eError = ERR_ADEC_BUF_FULL;
            } 
            else if (nMilliSec < 0)
            {
                pAudioDecData->mWaitEmptyAbsFlag = TRUE;
                pAudioDecData->mRequestAbsLen = pStream->mLen;
                pthread_cond_wait(&pAudioDecData->mEmptyAbsCondition, &pAudioDecData->mAbsInputMutex);
                pAudioDecData->mWaitEmptyAbsFlag = FALSE;
                goto _TryToRequestAbs;
            }
            else
            {
                pAudioDecData->mWaitEmptyAbsFlag = TRUE;
                pAudioDecData->mRequestAbsLen = pStream->mLen;
                int waitRet = pthread_cond_wait_timeout(&pAudioDecData->mEmptyAbsCondition, &pAudioDecData->mAbsInputMutex, nMilliSec);
                if (ETIMEDOUT == waitRet)
                {
                    alogv("wait empty abs buffer timeout[%d]ms, ret[%d]", nMilliSec, waitRet);
                    eError = ERR_ADEC_BUF_FULL;
                    pAudioDecData->mWaitEmptyAbsFlag = FALSE;
                }
                else if (0 == waitRet)
                {
                    pAudioDecData->mWaitEmptyAbsFlag = FALSE;
                    goto _TryToRequestAbs;
                }
                else
                {
                    aloge("fatal error! pthread cond wait timeout ret[%d]", waitRet);
                    eError = ERR_ADEC_BUF_FULL;
                    pAudioDecData->mWaitEmptyAbsFlag = FALSE;
                }
            }
        }
        pthread_mutex_unlock(&pAudioDecData->mAbsInputMutex);
        pthread_mutex_unlock(&pAudioDecData->mStateLock);
        return eError;
    }
}

/**
 * get frame, used in non-tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent adec component.
 * @param pAFrame store frame info, caller malloc.
 * @param nMilliSec 0:return immediately, <0:wait forever, >0:wait some time.
 */
ERRORTYPE AudioDecGetFrame(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT AUDIO_FRAME_S *pAFrame,
        PARAM_IN int nMilliSec)
{
    ERRORTYPE eError;
    int ret;
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (  COMP_StateIdle != pAudioDecData->state 
       && COMP_StateExecuting != pAudioDecData->state 
       && COMP_StatePause != pAudioDecData->state
       ) 
    {
        alogw("call getStream in wrong state[0x%x]", pAudioDecData->state);
        return ERR_ADEC_NOT_PERM;
    }
    if (pAudioDecData->mOutputPortTunnelFlag) 
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_ADEC_NOT_PERM;
    }
    pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);

_TryToGetOutFrame:
    if (!list_empty(&pAudioDecData->mReadyOutFrameList)) 
    {
        ADecCompOutputFrame *pOutFrame = list_first_entry(&pAudioDecData->mReadyOutFrameList, ADecCompOutputFrame, mList);
        config_AUDIO_FRAME_S_by_ADecCompOutputFrame(pAFrame, pOutFrame);
        list_move_tail(&pOutFrame->mList, &pAudioDecData->mUsedOutFrameList);
        eError = SUCCESS;
    } 
    else 
    {
        if (0 == nMilliSec) 
        {
            eError = ERR_ADEC_BUF_EMPTY;
        }
        else if (nMilliSec < 0) 
        {
            pAudioDecData->mWaitReadyFrameFlag = TRUE;
            while (list_empty(&pAudioDecData->mReadyOutFrameList)) 
            {
                pthread_cond_wait(&pAudioDecData->mReadyFrameCondition, &pAudioDecData->mOutFrameListMutex);
            }
            pAudioDecData->mWaitReadyFrameFlag = FALSE;
            goto _TryToGetOutFrame;
        } 
        else 
        {
            pAudioDecData->mWaitReadyFrameFlag = TRUE;
            ret = pthread_cond_wait_timeout(&pAudioDecData->mReadyFrameCondition, &pAudioDecData->mOutFrameListMutex, nMilliSec);
            if (ETIMEDOUT == ret) 
            {
                alogv("wait output frame timeout[%d]ms, ret[%d]", nMilliSec, ret);
                eError = ERR_ADEC_BUF_EMPTY;
                pAudioDecData->mWaitReadyFrameFlag = FALSE;
            } 
            else if (0 == ret) 
            {
                pAudioDecData->mWaitReadyFrameFlag = FALSE;
                goto _TryToGetOutFrame;
            } 
            else 
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                eError = ERR_ADEC_BUF_EMPTY;
                pAudioDecData->mWaitReadyFrameFlag = FALSE;
            }
        }
    }
    pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
    return eError;
}

/**
 * release frame, used in non-tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent adec component.
 * @param pFrameInfo frame info, caller malloc.
 */
ERRORTYPE AudioDecReleaseFrame(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN AUDIO_FRAME_S *pFrame)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if (COMP_StateIdle != pAudioDecData->state && COMP_StateExecuting != pAudioDecData->state &&
        COMP_StatePause != pAudioDecData->state) 
    {
        alogw("call getStream in wrong state[0x%x]", pAudioDecData->state);
        return ERR_ADEC_NOT_PERM;
    }
    if (pAudioDecData->mOutputPortTunnelFlag) {
        aloge("fatal error! can't call releaseFrame() in tunnel mode!");
        return ERR_ADEC_NOT_PERM;
    }
    
    pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
    if (!list_empty(&pAudioDecData->mUsedOutFrameList)) 
    {
        int ret;
        int nFindFlag = 0;
        ADecCompOutputFrame *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pAudioDecData->mUsedOutFrameList, mList)
        {
            if ((pEntry->mId==pFrame->mId) && (pEntry->mTimeStamp==pFrame->mTimeStamp)) 
            {
                ret = PlybkUpdatePcmBuffer(pAudioDecData->pDecoder, pEntry->mSize);
                if (ret != 0) 
                {
                    aloge("fatal error! return pcm fail ret[%d]", ret);
                }
                list_move_tail(&pEntry->mList, &pAudioDecData->mIdleOutFrameList);

                if (pAudioDecData->mWaitPcmBufFlag == TRUE)
                {
                    pAudioDecData->mWaitPcmBufFlag = FALSE;
                    message_t msg;
                    msg.command = ADecComp_PcmBufAvailable,
                    put_message(&pAudioDecData->cmd_queue, &msg);
                }

                nFindFlag = 1;
                break;
            }
        }
        if (0 == nFindFlag) 
        {
            aloge("fatal error! adec frameId[0x%x] is not match UsedOutFrameList", pFrame->mId);
            eError = ERR_ADEC_ILLEGAL_PARAM;
        }
    } 
    else 
    {
        aloge("fatal error! adec frameId[0x%x] is not find in UsedOutFrameList", pFrame->mId);
        eError = ERR_ADEC_ILLEGAL_PARAM;
    }
    pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
    return eError;
}

#if 0
/**
 * request abs buf and len from AdecLib, used in tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent adec component.
 * @param nPortIndex indicate abs inputPort index.
 * @param pBuffer contain abs data struct which need to be filled, pBuffer, nBufferLen, etc.
 */
 ERRORTYPE AudioDecRequstBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_INOUT  COMP_BUFFERHEADERTYPE* pBuffer)
{
    alogw("Be careful! old method, should not use now.");
    ERRORTYPE eError = SUCCESS;

    // Get component private data
    AUDIODECDATATYPE* pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    
    // Get buffer pointer from demux component
    EncodedStream *pDmxOutBuf = pBuffer->pOutputPortPrivate;
    
    // Request buffer from Decode lib
    if (pBuffer->nInputPortIndex == pAudioDecData->sInPortDef.nPortIndex) 
    {
        eError = ParserRequestBsBuffer(pAudioDecData->pDecoder,
                            pDmxOutBuf->nTobeFillLen,
                            (unsigned char**)&pDmxOutBuf->pBuffer,
                            (int*)&pDmxOutBuf->nBufferLen,
                            (unsigned char**)&pDmxOutBuf->pBufferExtra,
                            (int*)&pDmxOutBuf->nBufferExtraLen,
                            (int*)&pBuffer->nOffset);
    } 
    else 
    {
        aloge("fatal error! portIndex[%u][%u]! RequstBsBuffer fail!", pBuffer->nInputPortIndex, pAudioDecData->sInPortDef.nPortIndex);
        eError = FAILURE;
    }

    return eError;
}

/**
 * update abs data to AdecLib, used in tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent adec component.
 * @param nPortIndex indicate abs inputPort index.
 * @param pBuffer contain abs data struct.
 */
ERRORTYPE AudioDecReleaseBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    alogw("Be careful! old method, should not use now.");
    ERRORTYPE eError = SUCCESS;

    // Get component private data
    AUDIODECDATATYPE* pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    
    // Get buffer pointer from demux component
    EncodedStream* pDmxOutBuf = (EncodedStream*)pBuffer->pOutputPortPrivate;

    // Return buffer to Decode lib
    if (pBuffer->nInputPortIndex == pAudioDecData->sInPortDef.nPortIndex) 
    {
        pthread_mutex_lock(&pAudioDecData->mAbsInputMutex);
        eError = ParserUpdateBsBuffer(pAudioDecData->pDecoder, pDmxOutBuf->nFilledLen, pDmxOutBuf->nTimeStamp, pBuffer->nOffset);
        if (pAudioDecData->mWaitAbsInputFlag)
        {
            pAudioDecData->mWaitAbsInputFlag = FALSE;
            message_t msg;
            msg.command = ADecComp_AbsAvailable;
            put_message(&pAudioDecData->cmd_queue, &msg);
        }
        pthread_mutex_unlock(&pAudioDecData->mAbsInputMutex);
    }
    else 
    {
        aloge("fatal error! portIndex[%u][%u]! UpdataBsBuffer fail!", pBuffer->nInputPortIndex, pAudioDecData->sInPortDef.nPortIndex);
        eError = FAILURE;
    }

    return eError;
}
#endif

ERRORTYPE AudioDecSeek(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    alogd("ADec seek. flush the bs and pcm buf!");
    AudioDecoderSeek(pAudioDecData->pDecoder, 0);
    pAudioDecData->mbEof = FALSE;
    message_t msg;
    msg.command = ADecComp_AbsAvailable;
    put_message(&pAudioDecData->cmd_queue, &msg);
    return SUCCESS;
}

//1.aac
const int stdSampleRate[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
static ERRORTYPE generate_ExtraData_by_ADEC_CHN_ATTR_S(int *pLen, char **pData, ADEC_CHN_ATTR_S *pAttr)
{
    int i;
    BOOL findFlag = FALSE;
    for (i=0; i<sizeof(stdSampleRate)/sizeof(int); i++) {
        if (pAttr->sampleRate == stdSampleRate[i]) {
            findFlag = TRUE;
            break;
        }
    }
    if (TRUE == findFlag) {
        *pLen = 2;
        *pData = malloc(*pLen);
        memset(*pData, 0, *pLen);
        **pData = 0x10; // AACLC
        **pData |=i>>1;
        *(*pData+1) |=i<<7;
        *(*pData+1) |= (pAttr->channels&0xf)<<3;
    } else {
        *pLen = 5;
        *pData = malloc(*pLen);
        memset(*pData, 0, *pLen);
        **pData = 0x10; // AACLC
        **pData |=0x7;//set 4bits 0xf;
        *(*pData+1) =0x80;
        *(*pData+1) |= (pAttr->sampleRate>>(24-7))&0x7f;
        *(*pData+2) = (pAttr->sampleRate>>(24-15))&0xff;
        *(*pData+3) = (pAttr->sampleRate>>(24-23))&0xff;
        *(*pData+4) = (pAttr->sampleRate&0x01)<<7;
        *(*pData+4) |= (pAttr->channels&0xf)<<3;
    }

    return SUCCESS;
}

static ERRORTYPE config_AudioStreamInfo_by_ADEC_CHN_ATTR_S(AUDIODECDATATYPE *pAudioDecData)
{
    ADEC_CHN_ATTR_S *pAttr = &pAudioDecData->mADecChnAttr;
    AudioStreamInfo *pInfo = &pAudioDecData->mAudioStreamInfo;

    pInfo->eCodecFormat = map_PAYLOAD_TYPE_E_to_EAUDIOCODECFORMAT(pAttr->mType);
    pInfo->nChannelNum = pAttr->channels;
    pInfo->nBitsPerSample = pAttr->bitsPerSample;
    pInfo->nSampleRate = pAttr->sampleRate;
    pInfo->nAvgBitrate = pAttr->bitRate;
//    if(pInfo->eCodecFormat == AUDIO_CODEC_FORMAT_G726)
//    {
//        pInfo->g726_enc_law = pAttr->g726_src_enc_type;
//        pInfo->g726_src_bit_rate = pAttr->g726_src_bit_rate;
//    }
    if (AUDIO_CODEC_FORMAT_MPEG_AAC_LC == pInfo->eCodecFormat) {
        generate_ExtraData_by_ADEC_CHN_ATTR_S(&pInfo->nCodecSpecificDataLen, &pInfo->pCodecSpecificData, pAttr);
    }

    return SUCCESS;
}

ERRORTYPE AudioDecGetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch(nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = AudioDecGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorParamPortExtraDefinition:
        {
            eError = AudioDecGetPortExtraDefinition(hComponent, pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = AudioDecGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = AudioDecGetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAdecChnAttr:
        {
            eError = AudioDecGetChnAttr(hComponent, (ADEC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTunnelInfo:
        {
            eError = AudioDecGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAdecChnPriority:
        {
            alogw("unsupported temporary get adec chn priority!");
            eError = ERR_ADEC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorAdecChnState:
        {
            alogw("unsupported temporary COMP_IndexVendorAdecChnState!");
//            eError = AudioDecGetAdecChnState(hComponent, (ADEC_CHN_STAT_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAdecCacheState:
        {
            eError = AudioDecGetCacheState(hComponent, (COMP_BUFFERSTATE*)pComponentConfigStructure);
            break;
        }
//        case COMP_IndexVendorConfigInputBuffer:
//        {
//            eError = AudioDecRequstBuffer(hComponent, (COMP_BUFFERHEADERTYPE*)pComponentConfigStructure);
//            break;
//        }
        case COMP_IndexVendorAdecGetFrame:
        {
            AdecOutputFrame *pFrame = (AdecOutputFrame *)pComponentConfigStructure;
            eError = AudioDecGetFrame(hComponent, pFrame->pFrame, pFrame->nMilliSec);
            break;
        }
        default:
        {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_ADEC_NOT_SUPPORT;
            break;
        }
    }
    return eError;
}

ERRORTYPE AudioDecSetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    AUDIODECDATATYPE *pAudioDecData;
    ERRORTYPE eError = SUCCESS;

    pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = AudioDecSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = AudioDecSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = AudioDecSetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAdecChnAttr:
        {
            eError = AudioDecSetChnAttr(hComponent, (ADEC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAdecChnPriority:
        {
            alogw("unsupported temporary set adec chn priority!");
            eError = ERR_ADEC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorAdecResetChannel:
        {
            eError = AudioDecResetChannel(hComponent);
            break;
        }
        case COMP_IndexVendorDemuxType:
        {
            pAudioDecData->demux_type = *((int *)pComponentConfigStructure);
            pAudioDecData->demux_type &= 0xffff;
            break;
        }
        case COMP_IndexVendorAdecSetDecodeMode:
        {
            pAudioDecData->decode_mode = *(int*)pComponentConfigStructure;
            pAudioDecData->is_raw_music_mode = (pAudioDecData->decode_mode == CDX_DECODER_MODE_RAWMUSIC);
            break;
        }
        case COMP_IndexVendorAdecSwitchAudio:
        {
            pAudioDecData->priv_flag |= CDX_comp_PRIV_FLAGS_REINIT;
            break;
        }
        case COMP_IndexVendorAdecSelectAudioOut:
        {
            aloge("not support COMP_IndexVendorAdecSelectAudioOut");
//            pAudioDecData->enable_resample = *(int*)pComponentConfigStructure == CDX_AUDIO_OUT_I2S;
            break;
        }
        case COMP_IndexVendorAdecSetVolume:
        {
            pAudioDecData->volumegain = *(int*)pComponentConfigStructure;
            break;
        }
        case COMP_IndexVendorAdecSetAudioChannelMute:
        {
            pAudioDecData->mute = *(int*)pComponentConfigStructure;
            break;
        }
        case COMP_IndexVendorAdecSwitchAuioChannnel:
        {
            pAudioDecData->audio_channel = *(int*)pComponentConfigStructure;
            alogv("audio channel %d", pAudioDecData->audio_channel);
            break;
        }
//        case COMP_IndexVendorConfigInputBuffer:
//        {
//            eError = AudioDecReleaseBuffer(hComponent, pComponentConfigStructure);
//            break;
//        }
        case COMP_IndexVendorSetStreamEof:
        {
            eError = AudioDecSetStreamEof(hComponent);
            break;
        }
        case COMP_IndexVendorClearStreamEof:
        {
            eError = AudioDecClearStreamEof(hComponent);
            break;
        }
        case COMP_IndexVendorSeekToPosition:
        {
            eError = AudioDecSeek(hComponent);
            break;
        }
        case COMP_IndexVendorAdecReleaseFrame:
        {
            eError = AudioDecReleaseFrame(hComponent, ((AdecOutputFrame *)pComponentConfigStructure)->pFrame);
            break;
        }
        default:
        {
            aloge("fatal error! unknown setConfig Index[0x%x]", nIndex);
            eError = ERR_ADEC_NOT_SUPPORT;
            break;
        }
    }

    return eError;
}

/**
 * return frame to adeclib in tunnel mode.
 *
 * @return SUCCESS
 * @param hComponent adecComp handle.
 * @param pBuffer frame buffer info.
 */
ERRORTYPE AudioDecFillThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    int ret;
    ERRORTYPE eError = SUCCESS;
    AUDIODECDATATYPE * pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pBuffer->nOutputPortIndex == pAudioDecData->sOutPortDef.nPortIndex)
    {
        pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
        AUDIO_FRAME_S *pAFrame = (AUDIO_FRAME_S*)pBuffer->pOutputPortPrivate;
        ADecCompOutputFrame *pEntry, *pOutFrame;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pAudioDecData->mUsedOutFrameList, mList)
        {
            if (pEntry->mTimeStamp == pAFrame->mTimeStamp)
            {
                if (FALSE == bFindFlag)
                {
                    bFindFlag = TRUE;
                    pOutFrame = pEntry;
                }
                else
                {
                    aloge("fatal error! find same framePTS[%lld] again!", pEntry->mTimeStamp);
                }
            }
        }
        if(!bFindFlag)
        {
            aloge("fatal error! can't find frameId[%d], pts[%llu], check code!", pAFrame->mId, pAFrame->mTimeStamp);
            pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
            return ERR_ADEC_ILLEGAL_PARAM;
        }
        ret = PlybkUpdatePcmBuffer(pAudioDecData->pDecoder, pOutFrame->mSize);
        if (ret != 0)
        {
            aloge("fatal error! return pcmBuffer fail! ret[%d]", ret);
            eError = FAILURE;
        }
        list_move_tail(&pOutFrame->mList, &pAudioDecData->mIdleOutFrameList);
        if (pAudioDecData->mWaitPcmBufFlag)
        {
            pAudioDecData->mWaitPcmBufFlag = FALSE;
            message_t msg;
            msg.command = ADecComp_PcmBufAvailable;
            put_message(&pAudioDecData->cmd_queue, &msg);
        }
        if (pAudioDecData->mWaitOutFrameFullFlag)
        {
            int cnt = 0;
            struct list_head *pList;
            list_for_each(pList, &pAudioDecData->mIdleOutFrameList)
            {
                cnt++;
            }
            if (cnt >= pAudioDecData->mFrameNodeNum)
            {
                pthread_cond_signal(&pAudioDecData->mOutFrameFullCondition);
            }
        }
        pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
    }
    else
    {
        aloge("fatal error! PortIndex[%u][%u]! Release PcmBuf fail!", pBuffer->nOutputPortIndex, pAudioDecData->sOutPortDef.nPortIndex);
        eError = FAILURE;
    }

    return eError;
}

ERRORTYPE AudioDecComponentTunnelRequest(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  unsigned int nPort,
        PARAM_IN  COMP_HANDLETYPE hTunneledComp,
        PARAM_IN  unsigned int nTunneledPort,
        PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    AUDIODECDATATYPE *pAudioDecData = (AUDIODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pAudioDecData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pAudioDecData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pAudioDecData->state);
        eError = ERR_ADEC_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }

    COMP_PARAM_PORTDEFINITIONTYPE       *pPortDef = NULL;
    COMP_PARAM_PORTEXTRADEFINITIONTYPE  *pPortExtraDef = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE        *pPortTunnelInfo = NULL;
    COMP_PARAM_BUFFERSUPPLIERTYPE       *pPortBufSupplier = NULL;
    BOOL bFindFlag;
    int i;
    bFindFlag = FALSE;
    if(pAudioDecData->sInPortDef.nPortIndex == nPort)
    {
        pPortDef = &pAudioDecData->sInPortDef;
        pPortExtraDef = &pAudioDecData->sInPortExtraDef;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pAudioDecData->sOutPortDef.nPortIndex == nPort)
        {
            pPortDef = &pAudioDecData->sOutPortDef;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_ADEC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    if(pAudioDecData->sInPortTunnelInfo.nPortIndex == nPort)
    {
        pPortTunnelInfo = &pAudioDecData->sInPortTunnelInfo;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pAudioDecData->sOutPortTunnelInfo.nPortIndex == nPort)
        {
            pPortTunnelInfo = &pAudioDecData->sOutPortTunnelInfo;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_ADEC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_ADECODER_PORTS; i++)
    {
        if(pAudioDecData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pAudioDecData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_ADEC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }
    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        if(pPortDef->eDir == COMP_DirOutput)
        {
            pAudioDecData->mOutputPortTunnelFlag = FALSE;
        }
        else
        {
            pAudioDecData->mInputPortTunnelFlag = FALSE;
        }
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        if (pAudioDecData->mOutputPortTunnelFlag) {
            aloge("ADec_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pAudioDecData->mOutputPortTunnelFlag = TRUE;
    }
    else
    {
        if (pAudioDecData->mInputPortTunnelFlag) {
            aloge("ADec_Comp inport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput)
        {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_ADEC_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;
        //pPortDef->pVendorInfo = out_port_def.pVendorInfo;
        COMP_PARAM_PORTEXTRADEFINITIONTYPE out_port_extra_def;
        memset(&out_port_extra_def, 0, sizeof(COMP_PARAM_PORTEXTRADEFINITIONTYPE));
        out_port_extra_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexVendorParamPortExtraDefinition, &out_port_extra_def);
        pPortExtraDef->pVendorInfo = out_port_extra_def.pVendorInfo;

        //The component B informs component A about the final result of negotiation.
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
        {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pAudioDecData->mInputPortTunnelFlag = TRUE;
    }

COMP_CMD_FAIL:
    return eError;
}


ERRORTYPE AudioDecLibInit(AUDIODECDATATYPE *pAudioDecData)
{
    AudioStreamInfo *pAudioStreamInfo = NULL;
    if (TRUE == pAudioDecData->mInputPortTunnelFlag)
    {
        // streamInfo from demux, use it directly!
        pAudioStreamInfo = (AudioStreamInfo *)(pAudioDecData->sInPortExtraDef.pVendorInfo);
        if (NULL == pAudioStreamInfo) {
            alogw("audio stream info has not got, can't init ADecLib now.");
            return FAILURE;
        }
    }
    else
    {
        // set streamInfo by hand!
        config_AudioStreamInfo_by_ADEC_CHN_ATTR_S(pAudioDecData);
        pAudioStreamInfo = &pAudioDecData->mAudioStreamInfo;
    }

    pAudioDecData->pDecoder = CreateAudioDecoder();
    if(pAudioDecData->pDecoder == NULL)
    {
        aloge("Audiodec_Comp create Audiodecoder fail!");
        assert(0);
        return FAILURE;
    }
    memset(&pAudioDecData->ad_cedar_info, 0, sizeof(BsInFor));
    if(InitializeAudioDecoder(pAudioDecData->pDecoder, pAudioStreamInfo, &pAudioDecData->ad_cedar_info) != 0)
    {
        aloge("Initialize AudioDecoder fail!");
        DestroyAudioDecoder(pAudioDecData->pDecoder);
        //pAudioDecData->pDecoder = NULL;
        assert(0);
        return FAILURE;
    }
    SetRawPlayParam(pAudioDecData->pDecoder, (void*)pAudioDecData, 0);

    //alogd("adec inPort and outPort share same pVendorInfo!");
    //TODO just transfer it audio dec may update it!
    //pAudioDecData->sOutPortExtraDef.pVendorInfo = pAudioDecData->sInPortExtraDef.pVendorInfo;

    return SUCCESS;
}

/*****************************************************************************/
ERRORTYPE AudioDecComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    message_t msg;

    alogd("AudioDec Component DeInit");
    AUDIODECDATATYPE* pAudioDecData = (AUDIODECDATATYPE *)(((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    msg.command = eCmd;
    put_message(&pAudioDecData->cmd_queue, &msg);
    alogd("wait component exit!...");
    pAudioDecData->force_exit = 1;
    pthread_join(pAudioDecData->thread_id, (void*)&eError);
    AudioDec_InputDataDestroy(pAudioDecData->pInputData);

    message_destroy(&pAudioDecData->cmd_queue);

//    if (pAudioDecData->pDecoder != NULL)
//    {
//        int ret = DestroyAudioDecoder(pAudioDecData->pDecoder);
//        pAudioDecData->pDecoder = NULL;
//        if ((FALSE==pAudioDecData->mInputPortTunnelFlag) && (pAudioDecData->mAudioStreamInfo.pCodecSpecificData))
//            free(pAudioDecData->mAudioStreamInfo.pCodecSpecificData);
//    }

    pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
    int nodeNum = 0;
    if(!list_empty(&pAudioDecData->mIdleOutFrameList))
    {
        ADecCompOutputFrame *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pAudioDecData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            nodeNum++;
        }
    }
    if(nodeNum!=pAudioDecData->mFrameNodeNum)
    {
        aloge("Fatal error! AudioDec frame_node number is not match[%d][%d]", nodeNum, pAudioDecData->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);

    pthread_mutex_destroy(&pAudioDecData->mStateLock);
    pthread_mutex_destroy(&pAudioDecData->mAbsInputMutex);
    pthread_mutex_destroy(&pAudioDecData->mOutFrameListMutex);

    pthread_cond_destroy(&pAudioDecData->mOutFrameFullCondition);
    pthread_cond_destroy(&pAudioDecData->mReadyFrameCondition);
    pthread_cond_destroy(&pAudioDecData->mEmptyAbsCondition);
    
    if (pAudioDecData->pInputData)
    {
        free(pAudioDecData->pInputData);
    }
    if (pAudioDecData) 
    {
        free(pAudioDecData);
    }

    alogd("AudioDec component exited!");

    return eError;
}


/*****************************************************************************/
ERRORTYPE AudioDecComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    ERRORTYPE eError = SUCCESS;
    unsigned int err;
    int i = 0;
    
    // Create component private data
    MM_COMPONENTTYPE* pComp = (MM_COMPONENTTYPE *) hComponent;
    AUDIODECDATATYPE* pAudioDecData = (AUDIODECDATATYPE *) malloc(sizeof(AUDIODECDATATYPE));
    memset(pAudioDecData, 0x0, sizeof(AUDIODECDATATYPE));

    pComp->pComponentPrivate = (void*)pAudioDecData;
    pAudioDecData->state     = COMP_StateLoaded;
    pthread_mutex_init(&pAudioDecData->mStateLock, NULL);
    pAudioDecData->hSelf     = hComponent;

    pAudioDecData->mAudioInfoVersion     = -1;
    pAudioDecData->mWaitAbsInputFlag     = FALSE;
    pAudioDecData->mWaitPcmBufFlag       = FALSE;
    pAudioDecData->mWaitOutFrameFullFlag = FALSE;
    pAudioDecData->mWaitReadyFrameFlag   = FALSE;
    pAudioDecData->mWaitEmptyAbsFlag     = FALSE;

    INIT_LIST_HEAD(&pAudioDecData->mIdleOutFrameList);
    INIT_LIST_HEAD(&pAudioDecData->mReadyOutFrameList);
    INIT_LIST_HEAD(&pAudioDecData->mUsedOutFrameList);
    for(i = 0; i < ADEC_FRAME_COUNT; i++)
    {
        ADecCompOutputFrame *pNode = (ADecCompOutputFrame*)malloc(sizeof(ADecCompOutputFrame));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        list_add_tail(&pNode->mList, &pAudioDecData->mIdleOutFrameList);
        pAudioDecData->mFrameNodeNum++;
    }

    err = pthread_mutex_init(&pAudioDecData->mAbsInputMutex, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
        eError = ERR_ADEC_SYS_NOTREADY;
        goto EXIT;
    }
    err = pthread_mutex_init(&pAudioDecData->mOutFrameListMutex, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
        eError = ERR_ADEC_SYS_NOTREADY;
        goto EXIT;
    }

    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    err = 0;
    err += pthread_cond_init(&pAudioDecData->mOutFrameFullCondition, &condAttr);
    err += pthread_cond_init(&pAudioDecData->mReadyFrameCondition, &condAttr);
    err += pthread_cond_init(&pAudioDecData->mEmptyAbsCondition, &condAttr);
    if (err != 0)
    {
        aloge("fatal error! pthread mutex init fail!");
        eError = ERR_ADEC_SYS_NOTREADY;
        goto EXIT;
    }

    // Fill in function pointers
    pComp->SetCallbacks           = AudioDecSetCallbacks;
    pComp->SendCommand            = AudioDecSendCommand;
    pComp->GetConfig              = AudioDecGetConfig;
    pComp->SetConfig              = AudioDecSetConfig;
    pComp->GetState               = AudioDecGetState;
    pComp->ComponentTunnelRequest = AudioDecComponentTunnelRequest;
    pComp->ComponentDeInit        = AudioDecComponentDeInit;
    pComp->FillThisBuffer         = AudioDecFillThisBuffer;
    pComp->EmptyThisBuffer        = AudioDecEmptyThisBuffer;

    // Initialize component data structures to default values
    pAudioDecData->sPortParam.nPorts           = 0x2;
    pAudioDecData->sPortParam.nStartPortNumber = 0x0;

    // Initialize the audio parameters for input port
    pAudioDecData->sInPortDef.nPortIndex             =
    pAudioDecData->sInPortExtraDef.nPortIndex        = ADEC_PORT_INDEX_DEMUX;
    pAudioDecData->sInPortDef.bEnabled               = TRUE;
    pAudioDecData->sInPortDef.eDomain                = COMP_PortDomainAudio;
    pAudioDecData->sInPortDef.eDir                   = COMP_DirInput;
    pAudioDecData->sInPortDef.format.audio.cMIMEType = "AAC";
    pAudioDecData->sInPortDef.format.audio.eEncoding = PT_AAC;

    // Initialize the audio parameters for output port
    pAudioDecData->sOutPortDef.nPortIndex             =
    pAudioDecData->sOutPortExtraDef.nPortIndex        = ADEC_OUT_PORT_INDEX_VRENDER;
    pAudioDecData->sOutPortDef.bEnabled               = TRUE;
    pAudioDecData->sOutPortDef.eDomain                = COMP_PortDomainAudio;
    pAudioDecData->sOutPortDef.eDir                   = COMP_DirOutput;
    pAudioDecData->sOutPortDef.format.audio.cMIMEType = "PCM_RAW";

    pAudioDecData->sPortBufSupplier[ADEC_PORT_INDEX_DEMUX].nPortIndex            = ADEC_PORT_INDEX_DEMUX;
    pAudioDecData->sPortBufSupplier[ADEC_PORT_INDEX_DEMUX].eBufferSupplier       = COMP_BufferSupplyInput;
    pAudioDecData->sPortBufSupplier[ADEC_OUT_PORT_INDEX_VRENDER].nPortIndex      = ADEC_OUT_PORT_INDEX_VRENDER;
    pAudioDecData->sPortBufSupplier[ADEC_OUT_PORT_INDEX_VRENDER].eBufferSupplier = COMP_BufferSupplyOutput;

    pAudioDecData->sInPortTunnelInfo.nPortIndex   = ADEC_PORT_INDEX_DEMUX;
    pAudioDecData->sInPortTunnelInfo.eTunnelType  = TUNNEL_TYPE_COMMON;
    pAudioDecData->sOutPortTunnelInfo.nPortIndex  = ADEC_OUT_PORT_INDEX_VRENDER;
    pAudioDecData->sOutPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;

    if (message_create(&pAudioDecData->cmd_queue)<0)
    {
        aloge("create work cmd queue error!");
        eError = ERR_ADEC_NOMEM;
        goto EXIT;
    }

    // Create input thread data
    AUDIODEC_INPUT_DATA* pInputData = (AUDIODEC_INPUT_DATA*) malloc(sizeof(AUDIODEC_INPUT_DATA));
    if (pInputData == NULL)
    {
        aloge("create input data error!");
        eError = ERR_ADEC_NOMEM;
        goto EXIT;
    }
    pAudioDecData->pInputData = pInputData;    
    // Create the component thread
    err = pthread_create(&pAudioDecData->thread_id, NULL, ComponentThread, pAudioDecData);
    if (err || !pAudioDecData->thread_id) 
    {
        eError = ERR_ADEC_NOMEM;
        goto EXIT;
    }
    
    if((eError = AudioDec_InputDataInit(pInputData, pAudioDecData)) != SUCCESS)
    {
        aloge("fatal error! AudioDec InputData init fail");
        goto EXIT;
    }
    pthread_condattr_destroy(&condAttr);
    return eError;

EXIT:
    return eError;
}


/*****************************************************************************/
static void* ComponentThread(void* pThreadData) 
{
    int                 ret;
    int                 cmddata;
    CompInternalMsgType cmd;
    message_t           cmd_msg;
    message_t           input_cmd_msg;
    
    // Get component private data
    AUDIODECDATATYPE* pAudioDecData = (AUDIODECDATATYPE*)pThreadData;
    AUDIODEC_INPUT_DATA* pInputData = pAudioDecData->pInputData;
    pAudioDecData->get_audio_info_flag = 1;  // control get audio info only once

    alogv("AudioDecoder ComponentThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"ADecComp", 0, 0, 0);

    while (1) 
    {
PROCESS_MESSAGE:
        if (get_message(&pAudioDecData->cmd_queue, &cmd_msg) == 0) 
        {// pump message from message queue
            cmd     = cmd_msg.command;
            cmddata = cmd_msg.para0;

            //alogv("AudioDec ComponentThread get_message cmd:%d", cmd);
            
            // State transition command
            if (cmd == SetState)
            {
                pthread_mutex_lock(&pAudioDecData->mStateLock);
                // If the parameter states a transition to the same state
                //   raise a same state transition error.

                if (pAudioDecData->state == (COMP_STATETYPE) (cmddata))
                {
                    pAudioDecData->pCallbacks->EventHandler(pAudioDecData->hSelf,
                            pAudioDecData->pAppData,
                            COMP_EventError,
                            ERR_ADEC_SAMESTATE,
                            0,
                            NULL);
                }
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE) (cmddata))
                    {
                        case COMP_StateInvalid:
                        {
                            pAudioDecData->state = COMP_StateInvalid;
                            pAudioDecData->pCallbacks->EventHandler(pAudioDecData->hSelf,
                                    pAudioDecData->pAppData,
                                    COMP_EventError,
                                    ERR_ADEC_INVALIDSTATE,
                                    0,
                                    NULL);
                            pAudioDecData->pCallbacks->EventHandler(pAudioDecData->hSelf,
                                    pAudioDecData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pAudioDecData->state,
                                    NULL);
                            break;
                        }
                        case COMP_StateLoaded:
                        {
                            if (pAudioDecData->state != COMP_StateIdle)
                            {
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf,
                                        pAudioDecData->pAppData,
                                        COMP_EventError,
                                        ERR_ADEC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            alogd("ADec_Comp StateLoaded begin");

                            int cnt;
                            struct list_head *pList;
                            alogd("wait ADec idleOutFrameList full");
                            pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
                            pAudioDecData->mWaitOutFrameFullFlag = TRUE;
                            //wait all outFrame return.
                            while(1)
                            {
                                cnt = 0;
                                list_for_each(pList, &pAudioDecData->mIdleOutFrameList)
                                {
                                    cnt++;
                                }
                                if(cnt < pAudioDecData->mFrameNodeNum)
                                {
                                    alogd("Wait ADec idleOutFrameList [%d]nodes to home[%d]",
                                        pAudioDecData->mFrameNodeNum - cnt, pAudioDecData->mFrameNodeNum);
                                    pthread_cond_wait(&pAudioDecData->mOutFrameFullCondition, &pAudioDecData->mOutFrameListMutex);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            pAudioDecData->mWaitOutFrameFullFlag = FALSE;
                            pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                            alogd("Wait ADec idleOutFrameList full done");

                            // send message to input_thread
                            message_t InputThreadMsg;
                            InputThreadMsg.command = SetState;
                            InputThreadMsg.para0   = COMP_StateLoaded;
                            put_message(&pInputData->cmd_queue, &InputThreadMsg);

                            // Sync input thread state
                            pthread_mutex_lock(&pInputData->mStateLock);
                            while (pInputData->state != COMP_StateLoaded)
                            {
                                pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                            }
                            pthread_mutex_unlock(&pInputData->mStateLock);

                            if (pAudioDecData->pDecoder != NULL)
                            {
                                int ret = DestroyAudioDecoder(pAudioDecData->pDecoder);
                                pAudioDecData->pDecoder = NULL;
                                if ((FALSE==pAudioDecData->mInputPortTunnelFlag) && (pAudioDecData->mAudioStreamInfo.pCodecSpecificData))
                                {
                                    free(pAudioDecData->mAudioStreamInfo.pCodecSpecificData);
                                    pAudioDecData->mAudioStreamInfo.pCodecSpecificData = NULL;
                                }
                            }
                            pAudioDecData->state = COMP_StateLoaded;
                            pAudioDecData->pCallbacks->EventHandler(
                                    pAudioDecData->hSelf, pAudioDecData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pAudioDecData->state,
                                    NULL);
                            alogd("ADec_Comp StateLoaded ok");
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if(pAudioDecData->state == COMP_StateLoaded)
                            {
                                alogv("ADec_Comp: loaded->idle ...");

                                pAudioDecData->state = COMP_StateIdle;
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf,
                                        pAudioDecData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioDecData->state,
                                        NULL);
                            }
                            else if(pAudioDecData->state == COMP_StatePause || pAudioDecData->state == COMP_StateExecuting)
                            {
                                alogv("ADec_Comp: pause/executing[0x%x]->idle ...", pAudioDecData->state);
                                pAudioDecData->state = COMP_StateIdle;
                                #if 0
                                if (pAudioDecData->pDecoder != NULL)
                                {
                                    int ret = DestroyAudioDecoder(pAudioDecData->pDecoder);
                                    pAudioDecData->pDecoder = NULL;
                                    if ((FALSE==pAudioDecData->mInputPortTunnelFlag) && (pAudioDecData->mAudioStreamInfo.pCodecSpecificData))
                                    {
                                        free(pAudioDecData->mAudioStreamInfo.pCodecSpecificData);
                                        pAudioDecData->mAudioStreamInfo.pCodecSpecificData = NULL;
                                    }
                                }
                                #endif
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf,
                                        pAudioDecData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioDecData->state,
                                        NULL);
                            }
                            else
                            {
                                aloge("Fatal error! current state[0x%x] can't turn to idle!", pAudioDecData->state);
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf,
                                        pAudioDecData->pAppData,
                                        COMP_EventError,
                                        ERR_ADEC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            
                            // send message to input_thread
                            input_cmd_msg.command = SetState;
                            input_cmd_msg.para0   = COMP_StateIdle;
                            put_message(&pInputData->cmd_queue, &input_cmd_msg);

                            // Sync input thread state
                            pthread_mutex_lock(&pInputData->mStateLock);
                            while (pInputData->state != COMP_StateIdle)
                            {
                                pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                            }
                            pthread_mutex_unlock(&pInputData->mStateLock);
                            
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            if (pAudioDecData->state == COMP_StateIdle || pAudioDecData->state == COMP_StatePause)
                            {
                                if (pAudioDecData->state == COMP_StateIdle)
                                {
                                    if (NULL == pAudioDecData->pDecoder) {
                                        if (AudioDecLibInit(pAudioDecData) != SUCCESS) {
                                            aloge("Fatal error! Why AudioDecLibInit fail again?!!!");
                                            goto EXIT;
                                        }
                                    }
                                }
                                pAudioDecData->state = COMP_StateExecuting;
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf,
                                        pAudioDecData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioDecData->state,
                                        NULL);
                                        
                                // send message to input_thread
                                input_cmd_msg.command = SetState;
                                input_cmd_msg.para0   = COMP_StateExecuting;
                                put_message(&pInputData->cmd_queue, &input_cmd_msg);

                                // Sync input thread state
                                pthread_mutex_lock(&pInputData->mStateLock);
                                while (pInputData->state != COMP_StateExecuting)
                                {
                                    pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                                }
                                pthread_mutex_unlock(&pInputData->mStateLock);
                            }
                            else
                            {
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf,
                                        pAudioDecData->pAppData,
                                        COMP_EventError,
                                        ERR_ADEC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pAudioDecData->state == COMP_StateIdle || pAudioDecData->state == COMP_StateExecuting)
                            {
                                pAudioDecData->state = COMP_StatePause;
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf,
                                        pAudioDecData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioDecData->state,
                                        NULL);
                                        
                                // send message to input_thread
                                input_cmd_msg.command = SetState;
                                input_cmd_msg.para0   = COMP_StatePause;
                                put_message(&pInputData->cmd_queue, &input_cmd_msg);

                                // Sync input thread state
                                pthread_mutex_lock(&pInputData->mStateLock);
                                while (pInputData->state != COMP_StatePause)
                                {
                                    pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                                }
                                pthread_mutex_unlock(&pInputData->mStateLock);
                            }
                            else
                            {
                                pAudioDecData->pCallbacks->EventHandler(
                                        pAudioDecData->hSelf, pAudioDecData->pAppData,
                                        COMP_EventError,
                                        ERR_ADEC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&pAudioDecData->mStateLock);
            } 
            else if (cmd == StopPort) 
            {
            } 
            else if (cmd == Flush) 
            {
            } 
            else if (cmd == Stop) 
            {
                // send message to input_thread
//                input_cmd_msg.command = Stop;
//                input_cmd_msg.para0   = 0;
//                put_message(&pInputData->cmd_queue, &input_cmd_msg);
                goto EXIT;
            }
            else if(cmd == ADecComp_AbsAvailable)
            {
                alogv("ADec abs available");
            }
            else if(cmd == ADecComp_PcmBufAvailable)
            {
                alogv("ADec PcmBuf available");
            }
            
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (pAudioDecData->state == COMP_StateExecuting)
        {
            if (pAudioDecData->mbEof)
            {
                alogd("ADec EOF!");
                TMessage_WaitQueueNotEmpty(&pAudioDecData->cmd_queue, 0);
                goto PROCESS_MESSAGE;
            }
            AudioStreamInfo *pAudioStreamInfo = NULL;
            if(FALSE==pAudioDecData->mInputPortTunnelFlag)
            {
                pAudioStreamInfo = &pAudioDecData->mAudioStreamInfo;
            }
            else
            {
                pAudioStreamInfo = (AudioStreamInfo *)(pAudioDecData->sInPortExtraDef.pVendorInfo);
            }
REREQUEST_OUT_BUFFER:

            // Ensure output buffer ready
            pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
            char* pPcmOutBufWrPos = NULL;
            int   pcmOutSize = 0;
            if (DecRequestPcmBuffer(pAudioDecData->pDecoder, &pPcmOutBufWrPos) < 0)
            {
                //alogv("no pcm buffer, wait.");
                pAudioDecData->mWaitPcmBufFlag = TRUE;
                pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                TMessage_WaitQueueNotEmpty(&pAudioDecData->cmd_queue, 0);
                goto PROCESS_MESSAGE;
            }
            pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);

            // config AudioDecoder before decode.
            if ((int)pAudioDecData->audio_channel != pAudioDecData->pDecoder->nAudioChannel) 
            {
                alogw("wow! It is happened! audio_channel[%d]", pAudioDecData->audio_channel);
                pAudioDecData->pDecoder->nAudioChannel = pAudioDecData->audio_channel;
            }
            if (pAudioDecData->volumegain != pAudioDecData->pDecoder->volumegain)
            {
                alogw("wow! It is happened! volumegain[%d]", pAudioDecData->volumegain);
                pAudioDecData->pDecoder->volumegain = pAudioDecData->volumegain;
            }
            if (pAudioDecData->mute != pAudioDecData->pDecoder->mute)
            {
                alogw("wow! It is happened! mute[%d]", pAudioDecData->mute);
                pAudioDecData->pDecoder->mute = pAudioDecData->mute;
            }
            if (pAudioDecData->enable_resample != pAudioDecData->pDecoder->nEnableResample)
            {
                alogw("wow! It is happened! enable_resample[%d]", pAudioDecData->enable_resample);
                pAudioDecData->pDecoder->nEnableResample = pAudioDecData->enable_resample;
            }

            // Decode audio data
            ret = DecodeAudioStream(pAudioDecData->pDecoder, pAudioStreamInfo, pPcmOutBufWrPos, &pcmOutSize);
            //alogv("Calling decoder, ret = %d", ret);
            if (ret == ERR_AUDIO_DEC_NO_BITSTREAM)
            {
                pAudioDecData->mNoBitstreamCounter++;
            }
            else
            {
                pAudioDecData->mNoBitstreamCounter = 0;
            }

            if (ret == ERR_AUDIO_DEC_NONE)
            {
                if(  pAudioStreamInfo->nSampleRate != pAudioDecData->ad_cedar_info.out_samplerate 
                  || pAudioStreamInfo->nChannelNum != pAudioDecData->ad_cedar_info.out_channels
                  )
                {
                    alogd("fatal error! ADecode info channels-sample_rate[%d-%d]!=[%d-%d],bit_width:%d",
                        pAudioDecData->ad_cedar_info.out_channels, pAudioDecData->ad_cedar_info.out_samplerate,
                        pAudioStreamInfo->nChannelNum, pAudioStreamInfo->nSampleRate, 
                        pAudioStreamInfo->nBitsPerSample);
                    pAudioDecData->ad_cedar_info.out_samplerate = pAudioStreamInfo->nSampleRate;
                    pAudioDecData->ad_cedar_info.out_channels = pAudioStreamInfo->nChannelNum;
                }
                
                if (pAudioDecData->get_audio_info_flag) 
                {// get audio info only once
                    AudioStreamInfo *as_info = pAudioStreamInfo;

                    pAudioDecData->get_audio_info_flag = 0;
                    if(pAudioDecData->ad_cedar_info.bitpersample != 16)
                    {
                        alogd("Be careful! why get bitwidth=%d after decode first bs?!", pAudioDecData->ad_cedar_info.bitpersample);
                    }
                    if(0 == pAudioDecData->ad_cedar_info.bitpersample)
                    {
                        if (as_info->nBitsPerSample == 0)//0 means pcm S16_LE, adecoder should optimize
                        {
                            pAudioDecData->ad_cedar_info.bitpersample = 16;
                            alogd("Be careful! reset bitwidth:0->16!");
                        }
                        else
                        {
                            pAudioDecData->ad_cedar_info.bitpersample = as_info->nBitsPerSample;
                            alogd("Be careful! reset bitwidth:0->[%d]", pAudioDecData->ad_cedar_info.bitpersample);
                            //assert(0);
                        }
                    }
                    
                    if (!as_info->nChannelNum || !as_info->nSampleRate || !as_info->nBitsPerSample)
                    {
                        aloge("get audio decoder info fail[%d-%d-%d]!", as_info->nChannelNum, as_info->nSampleRate, as_info->nBitsPerSample);
                    }
                    ADEC_CHN_ATTR_S *pADecAttr = &pAudioDecData->mADecChnAttr;
                    pADecAttr->sampleRate = as_info->nSampleRate;
                    pADecAttr->channels = as_info->nChannelNum;
                    pADecAttr->bitsPerSample = pAudioDecData->ad_cedar_info.bitpersample;
                    alogd("ADecode first BS! channels:%d, sample_rate:%d, bit_width:%d, use this to init AudioRenderHal!",
                        pADecAttr->channels, pADecAttr->sampleRate, pADecAttr->bitsPerSample);
                }

                DecUpdatePcmBuffer(pAudioDecData->pDecoder, pcmOutSize);

                if (FALSE == pAudioDecData->mInputPortTunnelFlag)
                {
                    pthread_mutex_lock(&pAudioDecData->mAbsInputMutex);
                    if (pAudioDecData->mWaitEmptyAbsFlag)
                    {
                        int bsValidSize = AudioStreamDataSize(pAudioDecData->pDecoder); // inport bs data_size that ready to be decoded
                        int bsTotalSize = AudioStreamBufferSize();                      // inport bs space total size: 128KB
                        int bsEmptySize = bsTotalSize - bsValidSize;
                        int bsValidPercent, bsValidChunkCnt;
                        BsQueryQuality(pAudioDecData->pDecoder, &bsValidPercent, &bsValidChunkCnt);
                        int bsTotalChunkCnt = AudioStreamBufferMaxFrameNum();
                        int bsEmptyChunkCnt = bsTotalChunkCnt - bsValidChunkCnt; // for most time, bsEmptyChunkCnt = 0
                        if ((bsEmptySize >= pAudioDecData->mRequestAbsLen) && (bsEmptyChunkCnt > 0)) 
                        {
                            //alogv("signal abs empty_buf ready. reqBufLen:%d, emptyBufLen:%d, emptyChunkCnt:%d",
                            //    pAudioDecData->mRequestAbsLen, bsEmptySize, bsEmptyChunkCnt);
                            pthread_cond_signal(&pAudioDecData->mEmptyAbsCondition);
                        }
                    }
                    pthread_mutex_unlock(&pAudioDecData->mAbsInputMutex);
                }
                
                //send pcm data to AudioRender component by tunnel or to APP by non-tunnel
                while (1)
                {
                    pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
                    if (list_empty(&pAudioDecData->mIdleOutFrameList))
                    {
                        alogw("Low probability! adecComp idle out frame list is empty, malloc more!");
                        ADecCompOutputFrame *pNode = (ADecCompOutputFrame*)malloc(sizeof(ADecCompOutputFrame));
                        if (pNode)
                        {
                            list_add_tail(&pNode->mList, &pAudioDecData->mIdleOutFrameList);
                            pAudioDecData->mFrameNodeNum++;
                        }
                        else
                        {
                            aloge("fatal error! malloc fail[%s]!", strerror(errno));
                            pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                            if (TMessage_WaitQueueNotEmpty(&pAudioDecData->cmd_queue, 200) > 0)
                            {
                                goto PROCESS_MESSAGE;
                            }
                            else
                            {
                                continue;
                            }
                        }
                    }

                    ADecCompOutputFrame *pOutFrame = list_first_entry(&pAudioDecData->mIdleOutFrameList, ADecCompOutputFrame, mList);
                    pOutFrame->mTimeStamp = PlybkRequestPcmPts(pAudioDecData->pDecoder); //Decode Lib API
                    int reqRet = PlybkRequestPcmBuffer(pAudioDecData->pDecoder, (unsigned char**)&pOutFrame->mpPcmBuf, &pOutFrame->mSize); //Decode Lib API
                    if (reqRet == 0)
                    {
                        ADEC_CHN_ATTR_S *pPcmAttr = &pAudioDecData->mADecChnAttr;
                        pOutFrame->mChannelNum    = pPcmAttr->channels;
                        pOutFrame->mBitsPerSample = pPcmAttr->bitsPerSample;
                        pOutFrame->mSampleRate    = pPcmAttr->sampleRate;
                        pOutFrame->mId            = pAudioDecData->mFrameId++;
                        list_del(&pOutFrame->mList);
                        pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                        
                        if (pAudioDecData->mOutputPortTunnelFlag)
                        {
                            MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)(pAudioDecData->sOutPortTunnelInfo.hTunnel);

                            COMP_BUFFERHEADERTYPE obh;
                            obh.nOutputPortIndex = pAudioDecData->sOutPortTunnelInfo.nPortIndex;
                            obh.nInputPortIndex  = pAudioDecData->sOutPortTunnelInfo.nTunnelPortIndex;
                            AUDIO_FRAME_S stAFrame;
                            config_AUDIO_FRAME_S_by_ADecCompOutputFrame(&stAFrame, pOutFrame);
                            obh.pOutputPortPrivate = (void*)&stAFrame;
                            //put frame to usedList first, avoid AO return frame quickly.
                            pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
                            list_add_tail(&pOutFrame->mList, &pAudioDecData->mUsedOutFrameList);
                            pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                            int omxRet = COMP_EmptyThisBuffer(pOutTunnelComp, &obh);
                            if (SUCCESS == omxRet)
                            {
                            }
                            else
                            {
                                if (ERR_AO_SYS_NOTREADY == omxRet)
                                {
                                    alogd("Be careful! ADec output frame fail[0x%x], maybe next component status is Loaded, return frame!", omxRet);
                                }

                                int releaseRet = PlybkUpdatePcmBuffer(pAudioDecData->pDecoder, pOutFrame->mSize);
                                if (releaseRet != 0)
                                {
                                    aloge("fatal error! return pcmBuffer fail ret[%d]", ret);
                                }
                                pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
                                list_move(&pOutFrame->mList, &pAudioDecData->mIdleOutFrameList);
                                pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                            }
                        }
                        else
                        {
                            pthread_mutex_lock(&pAudioDecData->mOutFrameListMutex);
                            list_add_tail(&pOutFrame->mList, &pAudioDecData->mReadyOutFrameList);
                            if (pAudioDecData->mWaitReadyFrameFlag)
                            {
                                pthread_cond_signal(&pAudioDecData->mReadyFrameCondition);
                            }
                            pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                        }
                    }
                    else
                    {
                        pthread_mutex_unlock(&pAudioDecData->mOutFrameListMutex);
                        break;
                    }
                }
            }
            else if(ret == ERR_AUDIO_DEC_ABSEND)
            {
                alogd("adec return abs end!");
                pAudioDecData->pCallbacks->EventHandler(pAudioDecData->hSelf, pAudioDecData->pAppData,
                    COMP_EventBufferFlag, 0, 0, NULL);
                //pAudioDecData->state = COMP_StateIdle;
                pAudioDecData->mbEof = TRUE;
            }
            else if(ret == ERR_AUDIO_DEC_NO_BITSTREAM)
            {
                alogv("adec return no bitstream!");
                if (pAudioDecData->priv_flag & CDX_comp_PRIV_FLAGS_STREAMEOF)
                {
                    alogd("audio decoder notify EOF");
                    CompSendEvent(pAudioDecData, COMP_EventBufferFlag, COMP_CommandStateSet, 0);
                    //pAudioDecData->state = COMP_StateIdle;
                    pAudioDecData->mbEof = TRUE;
                }
                else
                {
                    pthread_mutex_lock(&pAudioDecData->mAbsInputMutex);
                    int streamSize = AudioStreamDataSize(pAudioDecData->pDecoder);
                    if (streamSize > 0 && pAudioDecData->mNoBitstreamCounter < 2)
                    {
                        alogw("Low probability! AudioDec Component has input abs[%d]", streamSize);
                        pthread_mutex_unlock(&pAudioDecData->mAbsInputMutex);
                    }
                    else
                    {
                        pAudioDecData->mWaitAbsInputFlag = TRUE;
                        pthread_mutex_unlock(&pAudioDecData->mAbsInputMutex);
                        TMessage_WaitQueueNotEmpty(&pAudioDecData->cmd_queue, 0);
                    }
                    goto PROCESS_MESSAGE;
                }
            }
            else if(ret == ERR_AUDIO_DEC_EXIT || ret == ERR_AUDIO_DEC_ENDINGCHKFAIL)
            {
                if (ret == ERR_AUDIO_DEC_EXIT)
                {
                    aloge("ret == ERR_AUDIO_DEC_EXIT");
                }
                else
                {
                    aloge("ret == ERR_AUDIO_DEC_ENDINGCHKFAIL");
                }

                //pAudioDecData->pCallbacks->EventHandler(pAudioDecData->hSelf, pAudioDecData->pAppData,
                //    OMX_EventBufferFlag, 0, 0, NULL);
                //pAudioDecData->state = OMX_StateIdle;
            }
            else if (ret == ERR_AUDIO_DEC_VIDEOJUMP)
            {
                alogd("adec jump done[%d]!", pcmOutSize);
            }
            else
            {
                aloge("fatal error! impossible ADec decode ret[%d]!", ret);
                // show inport buf info.
                int bsValidSize = AudioStreamDataSize(pAudioDecData->pDecoder);     // inport bs data_size that can be decode but have not be decoded
                int bsTotalSize = AudioStreamBufferSize();   // inport bs space total size: 128KB
                int bsEmptySize = bsTotalSize - bsValidSize;
                aloge("inport bs buf info: totalSize-%d, validSize-%d, emptySize-%d", bsTotalSize, bsValidSize, bsEmptySize);
                // show outport buf info.
                int pcmValidPercentage, pcmValidSize;
                PcmQueryQuality(pAudioDecData->pDecoder, &pcmValidPercentage, &pcmValidSize);
                //int pcmTotalSize = pcmValidSize*100/pcmValidPercentage;
                //pcmTotalSize = AudioPCMDataSize(pAudioDecData->pDecoder);
                aloge("outport pcm buf info: totalSize-, validPer-%d, validSize-%d", pcmValidPercentage, pcmValidSize);
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pAudioDecData->cmd_queue, 0);
        }
    }

EXIT:
    alogv("AudioDecoder ComponentThread stopped");
    return (void*) SUCCESS;
}

/*****************************************************************************/
static void* InputThread(void* pThreadData) 
{
    ERRORTYPE eError = SUCCESS;
    COMP_BUFFERHEADERTYPE omx_buffer_header;
    EncodedStream dmxOutBuf;
    DMXPKT_NODE_T *pEntry, *pTmp;
    
    int                 cmddata;
    CompInternalMsgType cmd;
    message_t           cmd_msg;
    
    int bsValidSize, bsTotalSize, bsEmptySize;
    
    // Get component private data
    AUDIODEC_INPUT_DATA* pInputData = (AUDIODEC_INPUT_DATA*)pThreadData;
    AUDIODECDATATYPE* pAudioDecData = pInputData->pAudioDecData;
   
    alogd("AudioDecoder InputThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"ADecInput", 0, 0, 0);
    
    while (1) 
    {
PROCESS_MESSAGE:
        if (get_message(&pInputData->cmd_queue, &cmd_msg) == 0) 
        {// pump message from message queue

            //alogd("input thread got message: cmd=%d cmddata=%d (3-Execute 4-Pause)", cmd, cmddata);
            cmd     = cmd_msg.command;
            cmddata = cmd_msg.para0;

            // State transition command
            if (cmd == SetState)
            {
                switch ((COMP_STATETYPE) (cmddata))
                {
                    case COMP_StateLoaded:
                    {
                        if(pInputData->state!=COMP_StateIdle)
                        {
                            aloge("fatal error! inputThread state[0x%x] != Idle", pInputData->state);
                        }
                        pthread_mutex_lock(&pInputData->mAbsListLock);
                        struct list_head *pList;
                        pInputData->mbWaitAbsFullFlag = TRUE;
                        while (1) 
                        {
                            int cnt = 0;
                            list_for_each(pList, &pInputData->mIdleAbsList) { cnt++; }
                            if (cnt < pInputData->mNodeNum) 
                            {
                                alogd("wait input thread [%d]nodes to idleList!", pInputData->mNodeNum - cnt);
                                pthread_cond_wait(&pInputData->mAbsFullCond, &pInputData->mAbsListLock);
                            } 
                            else 
                            {
                                break;
                            }
                        }
                        pInputData->mbWaitAbsFullFlag = FALSE;
                        pthread_mutex_unlock(&pInputData->mAbsListLock);

                        pthread_mutex_lock(&pInputData->mStateLock);
                        pInputData->state = COMP_StateLoaded;
                        pthread_cond_signal(&pInputData->mStateCond);
                        pthread_mutex_unlock(&pInputData->mStateLock);
                        break;
                    }
                    case COMP_StateIdle:
                    {
                        pthread_mutex_lock(&pInputData->mStateLock);
                        pInputData->state = COMP_StateIdle;
                        pthread_cond_signal(&pInputData->mStateCond);
                        pthread_mutex_unlock(&pInputData->mStateLock);
                        break;
                    }
                    case COMP_StateExecuting:
                    {
                        pthread_mutex_lock(&pInputData->mStateLock);
                        pInputData->state = COMP_StateExecuting;
                        pthread_cond_signal(&pInputData->mStateCond);
                        pthread_mutex_unlock(&pInputData->mStateLock);
                        break;
                    }
                    case COMP_StatePause:
                    {
                        pthread_mutex_lock(&pInputData->mStateLock);
                        pInputData->state = COMP_StatePause;
                        pthread_cond_signal(&pInputData->mStateCond);
                        pthread_mutex_unlock(&pInputData->mStateLock);
                        break;
                    }
                    default:
                        break;
                }
            } 
            else if(cmd == ADecComp_AbsAvailable)
            {
            }
            else if (cmd == StopPort) 
            {
            } 
            else if (cmd == Flush) 
            {
            } 
            else if (cmd == Stop) 
            {// Kill thread     
                goto EXIT;
            }

            //precede to process message
            goto PROCESS_MESSAGE;
        }

        // wait until executing state
        if (pInputData->state != COMP_StateExecuting)
        {
            TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }

        if (pAudioDecData->mInputPortTunnelFlag == FALSE)
        {// inport non-tunnel mode
            //Do nothing right now
            TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }

        // Get A component
        MM_COMPONENTTYPE *pInTunnelComp = (MM_COMPONENTTYPE*)(pAudioDecData->sInPortTunnelInfo.hTunnel);
        COMP_PARAM_PORTDEFINITIONTYPE* pPortDef = &pAudioDecData->sInPortDef;

        // check whether we should request buffer
        pthread_mutex_lock(&pInputData->mAbsListLock);
        if (list_empty(&pInputData->mIdleAbsList))
        {// no need to request buffer if Idle list is empty
            if (pInputData->mWaitAbsValidFlag == TRUE)
            {
                alogv("waiting message forever again when mWaitAbsValidFlag is set");
            }
            pInputData->mWaitAbsValidFlag = TRUE;
            pthread_mutex_unlock(&pInputData->mAbsListLock);
            TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 0);
            goto PROCESS_MESSAGE;
        }
        else
        {// request buffer to fill node in Idle list
            // Get buffer status of Decode Lib
            bsValidSize = AudioStreamDataSize(pAudioDecData->pDecoder); // inport bs data_size that can be decode but have not be decoded
            bsTotalSize = AudioStreamBufferSize();                      // inport bs space total size: 128KB
            bsEmptySize = bsTotalSize - bsValidSize;
            //alogd(" bsValidSize[%d] bsTotalSize[%d] bsEmptySize[%d]",bsValidSize, bsTotalSize, bsEmptySize);
            if (bsEmptySize < pInputData->nRequestLen)
            {// request bs buffer again after A component return bs buffer
                //alogw("low buffer (size = %d) from decode lib", bsEmptySize);
                pthread_mutex_unlock(&pInputData->mAbsListLock);
                TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 200);
                goto PROCESS_MESSAGE;
            }
            else
            {
                pInputData->nRequestLen = 0;
            }

            // Request buffer from Decode lib
            pEntry = list_first_entry(&pInputData->mIdleAbsList, DMXPKT_NODE_T, mList);

            pEntry->stEncodedStream.media_type   = CDX_PacketAudio;
            pEntry->stEncodedStream.nTobeFillLen = bsEmptySize;
            omx_buffer_header.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
            omx_buffer_header.nInputPortIndex    = pPortDef->nPortIndex;

            pthread_mutex_lock(&pAudioDecData->mAbsInputMutex);
            int adecRet = ParserRequestBsBuffer
                (pAudioDecData->pDecoder
                ,pEntry->stEncodedStream.nTobeFillLen
                ,(unsigned char**)&pEntry->stEncodedStream.pBuffer
                ,(int*)&pEntry->stEncodedStream.nBufferLen
                ,(unsigned char**)&pEntry->stEncodedStream.pBufferExtra
                ,(int*)&pEntry->stEncodedStream.nBufferExtraLen
                ,(int*)&omx_buffer_header.nOffset
                );
            pthread_mutex_unlock(&pAudioDecData->mAbsInputMutex);
            if (adecRet != 0)
            {
                // compare to InputThread, WorkThread is slower! which means decoder is slower than parser.
                // We need check whether ValidChunkCnt increase to 128(AUDIO_BITSTREAM_BUFFER_MAX_FRAME_NUM)!
                int bsValidPercent, bsValidChunkCnt;
                BsQueryQuality(pAudioDecData->pDecoder, &bsValidPercent, &bsValidChunkCnt);
                int bsRealEmptyBufSize = AudioStreamBufferSize()*bsValidPercent/100;
                int bsTotalChunkCnt = AudioStreamBufferMaxFrameNum();
                int bsEmptyChunkCnt = bsTotalChunkCnt - bsValidChunkCnt; // for most time, bsEmptyChunkCnt = 0
                if (bsEmptyChunkCnt > 0)
                    aloge("RequestBufSz(%d) > RealEmptyBsBufSz(%d)?! ret: %d", bsEmptySize, bsRealEmptyBufSize, adecRet);
                pthread_mutex_unlock(&pInputData->mAbsListLock);
                TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 200);
                goto PROCESS_MESSAGE;
            }

            list_move_tail(&pEntry->mList, &pInputData->mUsingAbsList);
            pthread_mutex_unlock(&pInputData->mAbsListLock);

            // 调用A component的FillThisBuffer
            eError = COMP_FillThisBuffer(pInTunnelComp, &omx_buffer_header);
            if(eError != SUCCESS)
            {
                pthread_mutex_lock(&pInputData->mAbsListLock);
                list_move_tail(&pEntry->mList, &pInputData->mIdleAbsList);
                pthread_mutex_unlock(&pInputData->mAbsListLock);
            }

            if (eError == SUCCESS)
            {
            }
            else if ((eError & 0x1FFF) == EN_ERR_NOT_PERM)
            {
                TMessage_WaitQueueNotEmpty(&pInputData->cmd_queue, 200);
                goto PROCESS_MESSAGE;
            }
            else
            {
                aloge("fatal error! Low probability! Tunneled inport component FillThisBuffer fail(%d)", eError);
                abort();
            }
        }

    }

EXIT:
    alogv("AudioDecoder InputThread stopped");
    return (void*) SUCCESS;
}
