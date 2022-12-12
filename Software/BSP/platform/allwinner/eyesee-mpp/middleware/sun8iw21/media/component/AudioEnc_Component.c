/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : AudioEnc_Component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/14
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "AudioEnc_Component"
#include <utils/plat_log.h>

//ref platform headers
#include <unistd.h>
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
#include <SystemBase.h>
//media api headers to app
#include "SystemBase.h"
#include "mm_common.h"
#include "mm_comm_aenc.h"
#include "mpi_aenc.h"

//media internal common headers.
#include "media_common.h"
#include "mm_component.h"
#include "ComponentCommon.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include "aencoder.h"
#include <aenc_sw_lib.h>
#include "AudioEnc_Component.h"
#include "AencCompStream.h"
#include "EncodedStream.h"

//------------------------------------------------------------------------------------

//#define AUDIO_ENC_TIME_DEBUG
//#define AENC_SAVE_AUDIO_BS
//#define AENC_SAVE_AUDIO_PCM

/*****************************************************************************/
static void* ComponentThread(void* pThreadData);

static int AudioEncLibInit(AUDIOENCDATATYPE *pAudioEncData)
{
    ERRORTYPE eError = SUCCESS;
    AENC_ATTR_S *pAttr = &pAudioEncData->mEncChnAttr.AeAttr;

    pAudioEncData->mAudioInfo.nInSamplerate  = pAttr->sampleRate;
    pAudioEncData->mAudioInfo.nInChan        = pAttr->channels;
    pAudioEncData->mAudioInfo.nBitrate       = pAttr->bitRate;
    pAudioEncData->mAudioInfo.nSamplerBits   = pAttr->bitsPerSample;
    pAudioEncData->mAudioInfo.nOutSamplerate = pAttr->sampleRate;
    pAudioEncData->mAudioInfo.nOutChan       = pAttr->channels;
    pAudioEncData->mAudioInfo.nFrameStyle   = 0;
    pAudioEncData->mAudioInfo.mInBufSize    = pAttr->mInBufSize;
    pAudioEncData->mAudioInfo.mOutBufCnt    = pAttr->mOutBufCnt;

    char *support_format[] = {"aac", "lpcm", "pcm", "adpcm", "mp3", "g711a", "g711u", "g726a", "g726u", "other"};
    char *format_ptr = NULL;

    switch (pAttr->Type) {
        case PT_AAC:
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_AAC_TYPE;
            if (pAttr->attachAACHeader) {
                pAudioEncData->mAudioInfo.nFrameStyle = 0;  // add head
            } else {
                pAudioEncData->mAudioInfo.nFrameStyle = 1;  // raw data
            }
            format_ptr = support_format[0];
            break;
        case PT_LPCM:
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_LPCM_TYPE;
            pAudioEncData->mAudioInfo.nFrameStyle = 2;
            format_ptr = support_format[1];
            break;
        case PT_PCM_AUDIO:
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_PCM_TYPE;
            format_ptr = support_format[2];
            break;
//        case PT_ADPCMA:
//            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_ADPCM_TYPE;
//            format_ptr = support_format[3];
//            break;
        case PT_MP3:
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_MP3_TYPE;
            format_ptr = support_format[4];
            break;
        case PT_G711A:
            pAudioEncData->mAudioInfo.nFrameStyle = 0;
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_G711A_TYPE;
            format_ptr = support_format[5];
            break;
        case PT_G711U:
            pAudioEncData->mAudioInfo.nFrameStyle = 1;
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_G711U_TYPE;
            format_ptr = support_format[6];
            break;
        case PT_G726:
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_G726A_TYPE;
            //pAudioEncData->mAudioInfo.g726_enc_law = pAttr->enc_law;
            format_ptr = support_format[7];
            break;
        case PT_G726U:
            pAudioEncData->mAudioInfo.nType = AUDIO_ENCODER_G726U_TYPE;
            format_ptr = support_format[8];
            break;
        default:
            format_ptr = support_format[9];
            aloge("AEncLib type(%d) NOT support! Check whether set AEncChnAttr!", pAttr->Type);
            eError = FAILURE;
            assert(0);
            return eError;
    }

    /*if ((pAudioEncData->mAudioInfo.nType==AUDIO_ENCODER_ADPCM_TYPE) ||
        (pAudioEncData->mAudioInfo.nType==AUDIO_ENCODER_G711_TYPE) ||
        (pAudioEncData->mAudioInfo.nType==AUDIO_ENCODER_G726_TYPE))
    {
        if ((pAttr->channels!=1) || (pAttr->sampleRate!=8000))
        {
            aloge("wrong aenc attr(type:%s, chnCnt:%d, smpRate:%d), voice(adpcm/g711/g726) only support mono and 8000!",
                format_ptr, pAttr->channels, pAttr->sampleRate);
            assert(0);
        }
    }*/
#if 0
    pAudioEncData->pCedarA = AudioEncInit(&pAudioEncData->mAudioInfo, pAudioEncData->mAudioEncodeType);
    if (pAudioEncData->pCedarA == NULL) {
        aloge("Fatal error! AudioEncInit fail!");
        assert(0);
        eError = FAILURE;
    }
#else
    __audio_enc_result_t aencRet;
    pAudioEncData->pCedarA = CreateAudioEncoder();
    if(NULL == pAudioEncData->pCedarA)
    {
        aloge("fatal error! create audio encoder fail!");
        assert(0);
        eError = FAILURE;
        goto _exit0;
    }
    aencRet = InitializeAudioEncoder(pAudioEncData->pCedarA, &pAudioEncData->mAudioInfo);
    if(aencRet != ERR_AUDIO_ENC_NONE)
    {
        aloge("fatal error! initialize audio encoder fail!");
        DestroyAudioEncoder(pAudioEncData->pCedarA);
        eError = FAILURE;
        goto _exit0;
    }
#endif

#ifdef AENC_SAVE_AUDIO_BS
    static int bs_cnt;
    char bs_name[64];
    sprintf(bs_name, "/mnt/extsd/aenc_bs_%d.%s", bs_cnt++, format_ptr);
    pAudioEncData->bs_fp = fopen(bs_name, "wb");
#endif

#ifdef AENC_SAVE_AUDIO_PCM
    static int pcm_cnt;
    char pcm_name[64];
    sprintf(pcm_name, "/mnt/extsd/aenc_pcm_%d", pcm_cnt++);
    pAudioEncData->pcm_fp = fopen(pcm_name, "wb");
#endif

_exit0:
    return eError;
}

ERRORTYPE AudioEncGetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (pPortDef->nPortIndex == pAudioEncData->sInPortDef.nPortIndex)
        memcpy(pPortDef, &pAudioEncData->sInPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pAudioEncData->sOutPortDef.nPortIndex)
        memcpy(pPortDef, &pAudioEncData->sOutPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else
        eError = ERR_AENC_ILLEGAL_PARAM;

    return eError;
}

ERRORTYPE AudioEncSetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if (pPortDef->nPortIndex == pAudioEncData->sInPortDef.nPortIndex)
        memcpy(&pAudioEncData->sInPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pAudioEncData->sOutPortDef.nPortIndex)
        memcpy(&pAudioEncData->sOutPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else
        eError = ERR_AENC_ILLEGAL_PARAM;

    return eError;
}

ERRORTYPE AudioEncGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_AENCODER_PORTS; i++)
    {
        if(pAudioEncData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pAudioEncData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_AENC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE AudioEncSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_AENCODER_PORTS; i++)
    {
        if(pAudioEncData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pAudioEncData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_AENC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE AudioEncGetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT MPP_CHN_S *pChn)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(pChn, &pAudioEncData->mMppChnInfo);
    return SUCCESS;
}

ERRORTYPE AudioEncSetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pAudioEncData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE AudioEncGetChannelFd(PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT int *pChnFd)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChnFd = pAudioEncData->mOutputFrameNotifyPipeFds[0];
    return SUCCESS;
}

ERRORTYPE AudioEncGetTunnelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = ERR_AENC_UNEXIST;

    if(pAudioEncData->sInPortTunnelInfo.nPortIndex == pTunnelInfo->nPortIndex)
    {
        memcpy(pTunnelInfo, &pAudioEncData->sInPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else if(pAudioEncData->sOutPortTunnelInfo.nPortIndex == pTunnelInfo->nPortIndex)
    {
        memcpy(pTunnelInfo, &pAudioEncData->sOutPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_AENC_UNEXIST;
    }
    return eError;
}

ERRORTYPE AudioEncGetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT AENC_CHN_ATTR_S *pChnAttr)
{
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChnAttr = pAudioEncData->mEncChnAttr;
    return SUCCESS;
}

ERRORTYPE AudioEncGetChnState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT AENC_CHN_STAT_S *pChnStat)
{
    ERRORTYPE eError;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pAudioEncData->pCedarA)
    {
        pChnStat->mValidPcmSize = AudioEncoder_GetValidPcmDataSize(pAudioEncData->pCedarA);
        pChnStat->mTotalPcmBufSize = AudioEncoder_GetTotalPcmBufSize(pAudioEncData->pCedarA);
        pChnStat->mLeftBsNodes = AudioEncoder_GetEmptyFrameNum(pAudioEncData->pCedarA);
        pChnStat->mTotalBsNodes = AudioEncoder_GetTotalFrameNum(pAudioEncData->pCedarA);
        eError = SUCCESS;
    }
    else
    {
        aloge("AudioEncoder has NOT init!");
        eError = ERR_AENC_SYS_NOTREADY;
    }
    return eError;
}

ERRORTYPE AudioEncSetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT AENC_CHN_ATTR_S *pChnAttr)
{
    ERRORTYPE eError;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pAudioEncData->pCedarA)
    {
        //when AEncLib is exist, only can change dynamic attribute.
        //now support none.
        aloge("Can NOT set AudioEncLib attr when it exist!");
        eError = ERR_AENC_NOT_SUPPORT;
    }
    else
    {
        pAudioEncData->mEncChnAttr = *pChnAttr;
        eError = SUCCESS;
    }
    return eError;
}
        
static ERRORTYPE ReturnAllAudioInputFrames(AUDIOENCDATATYPE *pAudioEncData)
{
    ERRORTYPE ret = SUCCESS;
    if(pAudioEncData->mInputPortTunnelFlag)
    {
        int num =  0;
        pthread_mutex_lock(&pAudioEncData->mInputPcmMutex);
        if(!list_empty(&pAudioEncData->mBufQ.mReadyList))
        {
            AEncCompInputFrameNode *pEntry, *pTmp;
            COMP_BUFFERHEADERTYPE obh;
            memset(&obh, 0, sizeof(obh));
            list_for_each_entry_safe(pEntry, pTmp, &pAudioEncData->mBufQ.mReadyList, mList)
            {
                obh.pOutputPortPrivate = (void*)&pEntry->mAudioFrame;
                obh.nOutputPortIndex = pAudioEncData->sInPortTunnelInfo.nTunnelPortIndex;
                obh.nInputPortIndex = pAudioEncData->sInPortTunnelInfo.nPortIndex;
                int compRet = COMP_FillThisBuffer(pAudioEncData->sInPortTunnelInfo.hTunnel, &obh);
                if(compRet != SUCCESS)
                {
                    aloge("fatal error! AencChn[%d] return inputFrame fail[0x%x]", pAudioEncData->mMppChnInfo.mChnId, compRet);
                }
                list_move_tail(&pEntry->mList, &pAudioEncData->mBufQ.mIdleList);
                num++;
            }
        }
        pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
        alogd("AencChn[%d] return %d audio input frames to aiChannel", pAudioEncData->mMppChnInfo.mChnId, num);
    }
    return ret;
}

ERRORTYPE AudioEncResetChannel(PARAM_IN COMP_HANDLETYPE hComponent)
{
    //ERRORTYPE eError;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pAudioEncData->state != COMP_StateIdle)
    {
        aloge("fatal error! must reset channel in stateIdle!");
        return ERR_AENC_NOT_PERM;
    }

    //need return input pcm in tunnel mode.
    ReturnAllAudioInputFrames(pAudioEncData);
    // return output frames to aenclib.
    if(FALSE == pAudioEncData->mOutputPortTunnelFlag)
    {
        //return output streams to aenclib directly. Don't worry about user take streams,
        //if user return stream after it, return ERR_AENC_ILLEGAL_PARAM.
        //user must guarantee that he return all streams before call this function.
        pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
        if(!list_empty(&pAudioEncData->mUsedOutFrameList))
        {
            ENCODER_NODE_T *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pAudioEncData->mUsedOutFrameList, mList)
            {
                ReturnAudioFrameBuffer
                    (pAudioEncData->pCedarA
                    ,(char*)pEntry->stEncodedStream.pBuffer
                    ,pEntry->stEncodedStream.nBufferLen
                    ,pEntry->stEncodedStream.nTimeStamp
                    ,pEntry->stEncodedStream.nID
                    );
                list_move_tail(&pEntry->mList, &pAudioEncData->mIdleOutFrameList);
            }
        }
        if(!list_empty(&pAudioEncData->mReadyOutFrameList))
        {
            ENCODER_NODE_T *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pAudioEncData->mReadyOutFrameList, mList)
            {
                ReturnAudioFrameBuffer
                    (pAudioEncData->pCedarA
                    ,(char*)pEntry->stEncodedStream.pBuffer
                    ,pEntry->stEncodedStream.nBufferLen
                    ,pEntry->stEncodedStream.nTimeStamp
                    ,pEntry->stEncodedStream.nID
                    );
                list_move_tail(&pEntry->mList, &pAudioEncData->mIdleOutFrameList);
            }
        }
        pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
    }
    else
    {
        //verify all output frames back to aenclib.
        //when component turn to stateIdle, it will guarantee all output frames back.
        pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
        if(!list_empty(&pAudioEncData->mUsedOutFrameList))
        {
            aloge("fatal error! aenc is in tunnel mode!");
        }
        if(!list_empty(&pAudioEncData->mReadyOutFrameList))
        {
            aloge("fatal error! aenc is in tunnel mode!");
        }
        pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
    }

    pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pAudioEncData->mIdleOutFrameList)
    {
        cnt++;
    }
    if(cnt != pAudioEncData->mFrameNodeNum)
    {
        alogw("Be careful! aenc output frames count not match [%d]!=[%d]", cnt, pAudioEncData->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
    return SUCCESS;
}

static ERRORTYPE config_AENC_STREAM_S_by_AencOutputBuffer(AUDIO_STREAM_S *pDst, EncodedStream *pSrc, AUDIOENCDATATYPE *pAudioEncData)
{
    pDst->pStream = (unsigned char*)pSrc->pBuffer;
    pDst->mLen = pSrc->nBufferLen;
    pDst->mTimeStamp = pSrc->nTimeStamp;
    pDst->mId = pSrc->nID;

    return SUCCESS;
}

static ERRORTYPE AudioEncGetStream(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT AUDIO_STREAM_S *pStream,
    PARAM_IN int nMilliSec)
{
    ERRORTYPE eError;
    int ret;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(COMP_StateIdle != pAudioEncData->state && COMP_StateExecuting != pAudioEncData->state)
    {
        alogw("call getStream in wrong state[0x%x]", pAudioEncData->state);
        return ERR_AENC_NOT_PERM;
    }
    if(pAudioEncData->mOutputPortTunnelFlag)
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_AENC_NOT_PERM;
    }
    pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
_TryToGetOutFrame:
    if(!list_empty(&pAudioEncData->mReadyOutFrameList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pAudioEncData->mReadyOutFrameList, ENCODER_NODE_T, mList);
        config_AENC_STREAM_S_by_AencOutputBuffer(pStream, &pEntry->stEncodedStream, pAudioEncData);
        list_move_tail(&pEntry->mList, &pAudioEncData->mUsedOutFrameList);
        char tmpRdCh;
        read(pAudioEncData->mOutputFrameNotifyPipeFds[0], &tmpRdCh, 1);
        eError = SUCCESS;
    }
    else
    {
        if(0 == nMilliSec)
        {
            eError = ERR_AENC_BUF_EMPTY;
        }
        else if(nMilliSec < 0)
        {
            pAudioEncData->mWaitOutFrameFlag = TRUE;
            while(list_empty(&pAudioEncData->mReadyOutFrameList))
            {
                pthread_cond_wait(&pAudioEncData->mOutFrameCondition, &pAudioEncData->mOutFrameListMutex);
            }
            pAudioEncData->mWaitOutFrameFlag = FALSE;
            goto _TryToGetOutFrame;
        }
        else
        {
            pAudioEncData->mWaitOutFrameFlag = TRUE;
            ret = pthread_cond_wait_timeout(&pAudioEncData->mOutFrameCondition, &pAudioEncData->mOutFrameListMutex, nMilliSec);
            if(ETIMEDOUT == ret)
            {
                alogv("wait output frame timeout[%d]ms, ret[%d]", nMilliSec, ret);
                eError = ERR_AENC_BUF_EMPTY;
                pAudioEncData->mWaitOutFrameFlag = FALSE;
            }
            else if(0 == ret)
            {
                pAudioEncData->mWaitOutFrameFlag = FALSE;
                goto _TryToGetOutFrame;
            }
            else
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                eError = ERR_AENC_BUF_EMPTY;
                pAudioEncData->mWaitOutFrameFlag = FALSE;
            }
        }
    }
    pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
    return eError;
}

static ERRORTYPE AudioEncReleaseStream(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN AUDIO_STREAM_S *pStream)
{
    ERRORTYPE eError;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(COMP_StateIdle != pAudioEncData->state && COMP_StateExecuting != pAudioEncData->state)
    {
        alogw("call getStream in wrong state[0x%x]", pAudioEncData->state);
        return ERR_AENC_NOT_PERM;
    }
    if(pAudioEncData->mOutputPortTunnelFlag)
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_AENC_NOT_PERM;
    }
    pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
    if(!list_empty(&pAudioEncData->mUsedOutFrameList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pAudioEncData->mUsedOutFrameList, ENCODER_NODE_T, mList);
        if(  pStream->pStream == pEntry->stEncodedStream.pBuffer
          && pStream->mLen == pEntry->stEncodedStream.nBufferLen
          )
        {
            ReturnAudioFrameBuffer
                (pAudioEncData->pCedarA
                ,(char*)pEntry->stEncodedStream.pBuffer
                ,pEntry->stEncodedStream.nBufferLen
                ,pEntry->stEncodedStream.nTimeStamp
                ,pEntry->stEncodedStream.nID
                );
            list_move_tail(&pEntry->mList, &pAudioEncData->mIdleOutFrameList);
            eError = SUCCESS;
        }
        else
        {
            aloge("fatal error! aenc stream[%p][%u] is not match UsedOutFrameList first entry[%p][%d]",
                pStream->pStream, pStream->mLen, pEntry->stEncodedStream.pBuffer, pEntry->stEncodedStream.nBufferLen);
            eError = ERR_AENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        alogw("Be careful! aenc stream[%p][%u] is not find, maybe reset channel before call this function?", pStream->pStream, pStream->mLen);
        eError = ERR_AENC_ILLEGAL_PARAM;
    }
    pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
    if(pAudioEncData->mNoOutFrameFlag)
    {
        message_t msg;
        msg.command = AEncComp_OutFrameAvailable;
        put_message(&pAudioEncData->cmd_queue, &msg);
    }

    return eError;
}


/*****************************************************************************/

ERRORTYPE AudioEncSendCommand(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd,
        PARAM_IN unsigned int nParam1,
        PARAM_IN void* pCmdData)
{
    AUDIOENCDATATYPE *pAudioEncData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    alogv("AudioEncSendCommand: %d", Cmd);

    pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(!pAudioEncData)
    {
        eError = ERR_AENC_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    if (pAudioEncData->state == COMP_StateInvalid)
    {
        eError = ERR_AENC_SYS_NOTREADY;
        goto COMP_CONF_CMD_FAIL;
    }
    switch (Cmd)
    {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        default:
            alogw("impossible comp_command[0x%x]", Cmd);
            eCmd = -1;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pAudioEncData->cmd_queue, &msg);

    COMP_CONF_CMD_FAIL: return eError;
}

/*****************************************************************************/
ERRORTYPE AudioEncGetState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_STATETYPE* pState)
{
    ERRORTYPE eError = SUCCESS;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    *pState = pAudioEncData->state;

    return eError;
}

/*****************************************************************************/
ERRORTYPE AudioEncSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_CALLBACKTYPE* pCallbacks, PARAM_IN void* pAppData)
{
    AUDIOENCDATATYPE *pAudioEncData;
    ERRORTYPE eError = SUCCESS;

    pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(!pAudioEncData || !pCallbacks || !pAppData)
    {
        eError = ERR_AENC_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_FAIL;
    }

    pAudioEncData->pCallbacks = pCallbacks;
    pAudioEncData->pAppData = pAppData;

    COMP_CONF_CMD_FAIL: return eError;
}

ERRORTYPE AudioEncGetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch(nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = AudioEncGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = AudioEncGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = AudioEncGetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelFd:
        {
            eError = AudioEncGetChannelFd(hComponent, (int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTunnelInfo:
        {
            eError = AudioEncGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAencChnAttr:
        {
            eError = AudioEncGetChnAttr(hComponent, (AENC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAencChnPriority:
        {
            alogw("unsupported temporary get aenc chn priority!");
            eError = ERR_AENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorAencChnState:
        {
            eError = AudioEncGetChnState(hComponent, (AENC_CHN_STAT_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAencGetStream:
        {
            AEncStream *pStream = (AEncStream*)pComponentConfigStructure;
            eError = AudioEncGetStream(hComponent, pStream->pStream, pStream->nMilliSec);
            break;
        }
        default:
        {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_AENC_NOT_SUPPORT;
            break;
        }
    }
    return eError;
}

ERRORTYPE AudioEncSetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = AudioEncSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = AudioEncSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = AudioEncSetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAencChnAttr:
        {
            eError = AudioEncSetChnAttr(hComponent, (AENC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAencChnPriority:
        {
            alogw("unsupported temporary set aenc chn priority!");
            eError = ERR_AENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorAencReleaseStream:
        {
            eError = AudioEncReleaseStream(hComponent, (AUDIO_STREAM_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAencResetChannel:
        {
            eError = AudioEncResetChannel(hComponent);
            break;
        }
        default:
        {
            aloge("unknown Index[0x%x]", nIndex);
            eError = ERR_AENC_ILLEGAL_PARAM;
            break;
        }
    }

    return eError;
}

ERRORTYPE AudioEncComponentTunnelRequest(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  unsigned int nPort,
        PARAM_IN  COMP_HANDLETYPE hTunneledComp,
        PARAM_IN  unsigned int nTunneledPort,
        PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pAudioEncData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pAudioEncData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pAudioEncData->state);
        eError = ERR_AENC_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }

    COMP_PARAM_PORTDEFINITIONTYPE   *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE    *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE   *pPortBufSupplier;
    BOOL bFindFlag;
    int i;
    bFindFlag = FALSE;
    if(pAudioEncData->sInPortDef.nPortIndex == nPort)
    {
        pPortDef = &pAudioEncData->sInPortDef;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pAudioEncData->sOutPortDef.nPortIndex == nPort)
        {
            pPortDef = &pAudioEncData->sOutPortDef;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    if(pAudioEncData->sInPortTunnelInfo.nPortIndex == nPort)
    {
        pPortTunnelInfo = &pAudioEncData->sInPortTunnelInfo;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pAudioEncData->sOutPortTunnelInfo.nPortIndex == nPort)
        {
            pPortTunnelInfo = &pAudioEncData->sOutPortTunnelInfo;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_AENCODER_PORTS; i++)
    {
        if(pAudioEncData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pAudioEncData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AENC_ILLEGAL_PARAM;
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
            pAudioEncData->mOutputPortTunnelFlag = FALSE;
        }
        else
        {
            pAudioEncData->mInputPortTunnelFlag = FALSE;
        }
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        if (pAudioEncData->mOutputPortTunnelFlag) {
            aloge("AEnc_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pAudioEncData->mOutputPortTunnelFlag = TRUE;
    }
    else
    {
        if (pAudioEncData->mInputPortTunnelFlag) {
            aloge("AEnc_Comp inport already bind, why bind again?!");
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
            eError = ERR_AENC_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;

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
        pAudioEncData->mInputPortTunnelFlag = TRUE;
    }

COMP_CMD_FAIL:
    return eError;
}

ERRORTYPE AudioEncEmptyThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    AUDIOENCDATATYPE *pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    AENC_PCM_FRM_INFO pcm_frm_info;
    if (pAudioEncData->state != COMP_StateExecuting)
    {
        alogw("send frame when aenc state[0x%x] isn't executing", pAudioEncData->state);
        //eError = COMP_ErrorInvalidState;
        //goto ERROR;
    }
   
    memset(&pcm_frm_info,0,sizeof(AENC_PCM_FRM_INFO));
    if (pAudioEncData->mInputPortTunnelFlag==FALSE)
    {
        AUDIO_FRAME_S *pFrm = (AUDIO_FRAME_S *)pBuffer->pOutputPortPrivate;
        void *pAudioBuf = pFrm->mpAddr;
        unsigned int bufSize = pFrm->mLen;

        pcm_frm_info.frm_pts = pFrm->mTimeStamp;
        pcm_frm_info.p_frm_buff = pFrm->mpAddr;

        alogv("pAudioBuf: %p, bufSize: %u", pAudioBuf, bufSize);

#ifdef AENC_SAVE_AUDIO_PCM
        fwrite(pAudioBuf, 1, bufSize, pAudioEncData->pcm_fp);
        pAudioEncData->pcm_sz += bufSize;
#endif

        if(-1 == pAudioEncData->first_audio_pts)
        {
            pAudioEncData->first_audio_pts = pFrm->mTimeStamp;
        }

        pthread_mutex_lock(&pAudioEncData->mInputPcmMutex);
        int ret = WriteAudioStreamBuffer(pAudioEncData->pCedarA, (void *)&pcm_frm_info,/*pAudioBuf,*/ bufSize);
        if (ret != 0)
        {
            pAudioEncData->mSendPcmFailCnt++;
            pAudioEncData->mFailPcmLen += bufSize;
            alogd("RequestWriteBuf failed! chn_id:%d, buf: %p, size/total: %d/%lld, fail_cnt:%lld",
                pAudioEncData->mMppChnInfo.mChnId, pAudioBuf, bufSize, pAudioEncData->mFailPcmLen, pAudioEncData->mSendPcmFailCnt);
            eError = ERR_AENC_BUF_FULL;
            pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
            goto ERROR;
        }
        if(pAudioEncData->mNoInputPcmFlag)
        {
            pAudioEncData->mNoInputPcmFlag = 0;
            message_t msg;
            msg.command = AEncComp_InputPcmAvailable;
            put_message(&pAudioEncData->cmd_queue, &msg);
        }
        pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
    }
    else
    {
        AUDIO_FRAME_S *pFrm = (AUDIO_FRAME_S *)pBuffer->pOutputPortPrivate;
        if (pBuffer->nInputPortIndex==pAudioEncData->sInPortDef.nPortIndex)
        {
            pthread_mutex_lock(&pAudioEncData->mInputPcmMutex);
            if(list_empty(&pAudioEncData->mBufQ.mIdleList))
            {
                alogd("AencChn[%d] idle input frame is empty, malloc one!", pAudioEncData->mMppChnInfo.mChnId);
                AEncCompInputFrameNode *pNode = (AEncCompInputFrameNode*)malloc(sizeof(AEncCompInputFrameNode));
                if(NULL == pNode)
                {
                    pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
                    aloge("fatal error! AencChn[%d] malloc fail!", pAudioEncData->mMppChnInfo.mChnId);
                    eError = ERR_AENC_NOMEM;
                    goto ERROR;
                }
                list_add_tail(&pNode->mList, &pAudioEncData->mBufQ.mIdleList);
            }
            AEncCompInputFrameNode *pFirstNode = list_first_entry(&pAudioEncData->mBufQ.mIdleList, AEncCompInputFrameNode, mList);
            memcpy(&pFirstNode->mAudioFrame, pFrm, sizeof(AUDIO_FRAME_S));
            list_move_tail(&pFirstNode->mList, &pAudioEncData->mBufQ.mReadyList);
            if(pAudioEncData->mNoInputPcmFlag)
            {
                pAudioEncData->mNoInputPcmFlag = 0;
                message_t msg;
                msg.command = AEncComp_InputPcmAvailable;
                put_message(&pAudioEncData->cmd_queue, &msg);
            }
            pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
        }
        else
        {
            aloge("fatal error! AencComp inputPortIndex is not match[%d!=%d]!", pBuffer->nInputPortIndex, pAudioEncData->sInPortDef.nPortIndex);
            eError = ERR_AENC_ILLEGAL_PARAM;
        }
    }

ERROR:
    return eError;
}

ERRORTYPE AudioEncFillThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    AUDIOENCDATATYPE *pAudioEncData;
    ERRORTYPE eError = SUCCESS;
    int ret;

    pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pBuffer->nOutputPortIndex == pAudioEncData->sOutPortDef.nPortIndex)
    {
        pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
        EncodedStream *pOutFrame = (EncodedStream*)pBuffer->pOutputPortPrivate;

        BOOL bFind = FALSE;
        ENCODER_NODE_T *pEntry;
        list_for_each_entry(pEntry, &pAudioEncData->mUsedOutFrameList, mList)
        {
            if (pEntry->stEncodedStream.nID == pOutFrame->nID)
            {
                bFind = TRUE;
                break;
            }
        }

        if (!bFind)
        {
            pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
            aloge("fatal error! try to release one output buffer that never be used! ID = %d", pOutFrame->nID);
            return ERR_AENC_ILLEGAL_PARAM;
        }

        ret = ReturnAudioFrameBuffer
            (pAudioEncData->pCedarA
            ,(char*)pEntry->stEncodedStream.pBuffer
            ,pEntry->stEncodedStream.nBufferLen
            ,pEntry->stEncodedStream.nTimeStamp
            ,pEntry->stEncodedStream.nID
            );
        if (ret != 0)
        {
            eError = FAILURE;
        }

        list_move_tail(&pEntry->mList, &pAudioEncData->mIdleOutFrameList);

        if (pAudioEncData->mNoOutFrameFlag)
        {
            message_t msg;
            msg.command = AEncComp_OutFrameAvailable;
            put_message(&pAudioEncData->cmd_queue, &msg);
        }

        if(pAudioEncData->mWaitOutFrameFullFlag)
        {
            int cnt = 0;
            struct list_head *pList;
            list_for_each(pList, &pAudioEncData->mIdleOutFrameList)
            {
                cnt++;
            }
            if(cnt>=pAudioEncData->mFrameNodeNum)
            {
                pthread_cond_signal(&pAudioEncData->mOutFrameFullCondition);
            }
        }
        pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
    }
    else
    {
        aloge("fatal error! outPortIndex[%d]!=[%d]", pBuffer->nOutputPortIndex, pAudioEncData->sOutPortDef.nPortIndex);
        eError = FAILURE;
    }

    return eError;
}

/*****************************************************************************/
ERRORTYPE AudioEncComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AUDIOENCDATATYPE *pAudioEncData = NULL;
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    message_t msg;
    alogv("AudioEnc Component DeInit");
    pAudioEncData = (AUDIOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

	if(NULL == pAudioEncData)
	{
		aloge("pAudioEncData is NULL!!!\n");
		return FAILURE;
	}
    if(pAudioEncData->mFailPcmLen > 0)
    {
        int sample_chn_size = 2;
        int chn_cnt = pAudioEncData->mEncChnAttr.AeAttr.channels;
        int sample_rate = pAudioEncData->mEncChnAttr.AeAttr.sampleRate;
        if(0==chn_cnt || 0==sample_rate)
        {
            aloge("fatal error! AEncComp param wrong:%d-%d", chn_cnt, sample_rate);
        }
        else
        {
            int lostDuration = (int)(pAudioEncData->mFailPcmLen*1000/(chn_cnt*sample_rate*sample_chn_size));
            alogw("Be careful! aenc_chn_id:%d, discard total bytes: %lld, fail_cnt:%lld, chncnt[%d], samplerate[%d], lost duration:[%d]ms",
                    pAudioEncData->mMppChnInfo.mChnId, pAudioEncData->mFailPcmLen, pAudioEncData->mSendPcmFailCnt, chn_cnt, sample_rate, lostDuration);
        }
    }

    msg.command = eCmd;
    put_message(&pAudioEncData->cmd_queue, &msg);
    alogv("wait AudioEnc component exit!...");
    pthread_join(pAudioEncData->thread_id, (void*) &eError);

    if(pAudioEncData->pCedarA)
    {
        DestroyAudioEncoder(pAudioEncData->pCedarA);
        pAudioEncData->pCedarA = NULL;
#ifdef AENC_SAVE_AUDIO_BS
        fclose(pAudioEncData->bs_fp);
        alogd("AEnc_Comp bs_file size: %d", pAudioEncData->bs_sz);
#endif
#ifdef AENC_SAVE_AUDIO_PCM
        fclose(pAudioEncData->pcm_fp);
        alogd("AEnc_Comp pcm_file size: %d", pAudioEncData->pcm_sz);
#endif
    }
    message_destroy(&pAudioEncData->cmd_queue);

    pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
    if(!list_empty(&pAudioEncData->mUsedOutFrameList))
    {
        aloge("fatal error! outUsedFrame must be 0!");
    }
    if(!list_empty(&pAudioEncData->mReadyOutFrameList))
    {
        aloge("fatal error! outReadyFrame must be 0!");
    }
    int nodeNum = 0;
    if(!list_empty(&pAudioEncData->mIdleOutFrameList))
    {
        ENCODER_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pAudioEncData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            nodeNum++;
        }
    }
    if(nodeNum != pAudioEncData->mFrameNodeNum)
    {
        aloge("Fatal error! AudioEnc frame_node number is not match: [%d][%d]", nodeNum, pAudioEncData->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);

    pthread_mutex_lock(&pAudioEncData->mInputPcmMutex);
    if(!list_empty(&pAudioEncData->mBufQ.mReadyList))
    {
        aloge("fatal error! AudioInputFrame list must be 0!");
    }
    nodeNum = 0;
    if(!list_empty(&pAudioEncData->mBufQ.mIdleList))
    {
        AEncCompInputFrameNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pAudioEncData->mBufQ.mIdleList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            nodeNum++;
        }
    }
    alogd("AencChn[%d] release %d audioInputFrameNode", pAudioEncData->mMppChnInfo.mChnId, nodeNum);
    pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);

    pthread_mutex_destroy(&pAudioEncData->mInputPcmMutex);
    pthread_mutex_destroy(&pAudioEncData->mOutFrameListMutex);
    pthread_mutex_destroy(&pAudioEncData->mStateLock);
    pthread_cond_destroy(&pAudioEncData->mOutFrameFullCondition);
    pthread_cond_destroy(&pAudioEncData->mOutFrameCondition);

	if(NULL != pAudioEncData)
	{
	    close(pAudioEncData->mOutputFrameNotifyPipeFds[0]);
    	close(pAudioEncData->mOutputFrameNotifyPipeFds[1]);
	}
  
    if (pAudioEncData)
    {
        free(pAudioEncData);
    }

    alogd("AudioEnc component exited!");

    return eError;
}

/*****************************************************************************/
ERRORTYPE AudioEncComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    AUDIOENCDATATYPE *pAudioEncData;
    ERRORTYPE eError = SUCCESS;
    unsigned int err;
    int i = 0;

    pComp = (MM_COMPONENTTYPE *) hComponent;

    // Create private data
    pAudioEncData = (AUDIOENCDATATYPE *) malloc(sizeof(AUDIOENCDATATYPE));
    memset(pAudioEncData, 0x0, sizeof(AUDIOENCDATATYPE));

    err = pipe(pAudioEncData->mOutputFrameNotifyPipeFds);
    if (err)
    {
        eError = ERR_AENC_NOMEM;
        goto EXIT;
    }
    pComp->pComponentPrivate = (void*)pAudioEncData;
    pAudioEncData->state = COMP_StateLoaded;
    pthread_mutex_init(&pAudioEncData->mStateLock,NULL);
    pAudioEncData->hSelf = hComponent;

    pAudioEncData->mNoInputPcmFlag = 0;
    pAudioEncData->mNoOutFrameFlag = 0;
    INIT_LIST_HEAD(&pAudioEncData->mIdleOutFrameList);
    INIT_LIST_HEAD(&pAudioEncData->mReadyOutFrameList);
    INIT_LIST_HEAD(&pAudioEncData->mUsedOutFrameList);
    for(i=0; i<AENC_FIFO_LEVEL; i++)
    {
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        memset(pNode, 0, sizeof(ENCODER_NODE_T));
        list_add_tail(&pNode->mList, &pAudioEncData->mIdleOutFrameList);
        pAudioEncData->mFrameNodeNum++;
    }
    INIT_LIST_HEAD(&pAudioEncData->mBufQ.mIdleList);
    INIT_LIST_HEAD(&pAudioEncData->mBufQ.mReadyList);
    for(i=0; i<MAX_AUDIOPCMFRAME_NODE_NUM; i++)
    {
        AEncCompInputFrameNode *pNode = (AEncCompInputFrameNode*)malloc(sizeof(AEncCompInputFrameNode));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        memset(pNode, 0, sizeof(AEncCompInputFrameNode));
        list_add_tail(&pNode->mList, &pAudioEncData->mBufQ.mIdleList);
    }

    err = pthread_mutex_init(&pAudioEncData->mInputPcmMutex, NULL);
    if(err != 0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_AENC_NOMEM;
        goto EXIT;
    }
    err = pthread_mutex_init(&pAudioEncData->mOutFrameListMutex, NULL);
    if(err != 0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_AENC_NOMEM;
        goto EXIT;
    }

    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    err = pthread_cond_init(&pAudioEncData->mOutFrameFullCondition, &condAttr);
    if(err != 0)
    {
        aloge("pthread cond init fail!");
        eError = ERR_AENC_NOMEM;
        goto EXIT;
    }
    pthread_cond_init(&pAudioEncData->mOutFrameCondition, &condAttr);

    // Fill in function pointers
    pComp->SetCallbacks     = AudioEncSetCallbacks;
    pComp->SendCommand      = AudioEncSendCommand;
    pComp->GetConfig        = AudioEncGetConfig;
    pComp->SetConfig        = AudioEncSetConfig;
    pComp->GetState         = AudioEncGetState;
    pComp->ComponentTunnelRequest = AudioEncComponentTunnelRequest;
    pComp->ComponentDeInit  = AudioEncComponentDeInit;
    pComp->EmptyThisBuffer  = AudioEncEmptyThisBuffer;
    pComp->FillThisBuffer   = AudioEncFillThisBuffer;

    // Initialize component data structures to default values
    pAudioEncData->sPortParam.nPorts = 0x2;
    pAudioEncData->sPortParam.nStartPortNumber = 0x0;
    pAudioEncData->first_audio_pts = -1;

    // Initialize the audio parameters for input port
    pAudioEncData->sInPortDef.nPortIndex = 0x0;
    pAudioEncData->sInPortDef.bEnabled = TRUE;
    pAudioEncData->sInPortDef.eDomain = COMP_PortDomainAudio;
    pAudioEncData->sInPortDef.eDir = COMP_DirInput;
    pAudioEncData->sInPortDef.format.audio.cMIMEType = "AAC";
    pAudioEncData->sInPortDef.format.audio.eEncoding = PT_AAC;

    // Initialize the audio parameters for output port
    pAudioEncData->sOutPortDef.nPortIndex = 0x1;
    pAudioEncData->sOutPortDef.bEnabled = TRUE;
    pAudioEncData->sOutPortDef.eDomain = COMP_PortDomainAudio;
    pAudioEncData->sOutPortDef.eDir = COMP_DirOutput;
    pAudioEncData->sOutPortDef.format.audio.cMIMEType = "AAC";
    pAudioEncData->sOutPortDef.format.audio.eEncoding = PT_AAC;

    pAudioEncData->sPortBufSupplier[0].nPortIndex = 0x0;
    pAudioEncData->sPortBufSupplier[0].eBufferSupplier = COMP_BufferSupplyOutput;
    pAudioEncData->sPortBufSupplier[1].nPortIndex = 0x1;
    pAudioEncData->sPortBufSupplier[1].eBufferSupplier = COMP_BufferSupplyOutput;

    pAudioEncData->sInPortTunnelInfo.nPortIndex = 0x0;
    pAudioEncData->sInPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;
    pAudioEncData->sOutPortTunnelInfo.nPortIndex = 0x1;
    pAudioEncData->sOutPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;
    
    pAudioEncData->last_chk_time = -1;
    

    if (message_create(&pAudioEncData->cmd_queue)<0)
    {
        aloge("message error!");
        eError = ERR_AENC_NOMEM;
        goto EXIT;
    }

    // Create the component thread
    err = pthread_create(&pAudioEncData->thread_id, NULL, ComponentThread, pAudioEncData);
    if (err || !pAudioEncData->thread_id)
    {
        eError = ERR_AENC_NOMEM;
        goto EXIT;
    }

    EXIT: return eError;
}

/**
 *  Component Thread
 *    The ComponentThread function is exeuted in a separate pThread and
 *    is used to implement the actual component functions.
 */
/*****************************************************************************/
static void* ComponentThread(void* pThreadData)
{
    int cmddata;
    CompInternalMsgType cmd;
    AUDIOENCDATATYPE* pAudioEncData = (AUDIOENCDATATYPE*)pThreadData;
    message_t cmd_msg;
    //int64_t tm1, tm2, tm3, itl;

    alogv("AudioEncoder ComponentThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"AEncComp", 0, 0, 0);

    while (1)
    {
PROCESS_MESSAGE:
        if(get_message(&pAudioEncData->cmd_queue, &cmd_msg) == 0)
        {
            cmd = cmd_msg.command;
            cmddata = cmd_msg.para0;

            alogv("AudioEnc ComponentThread get_message cmd:%d", cmd);

            // State transition command
            if (cmd == SetState)
            {
                // If the parameter states a transition to the same state
                //   raise a same state transition error.
                if (pAudioEncData->state == (COMP_STATETYPE) (cmddata))
                    pAudioEncData->pCallbacks->EventHandler(pAudioEncData->hSelf,
                            pAudioEncData->pAppData,
                            COMP_EventError,
                            ERR_AENC_SAMESTATE,
                            0,
                            NULL);
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE) (cmddata))
                    {
                        case COMP_StateInvalid:
                        {
                            pAudioEncData->state = COMP_StateInvalid;
                            pAudioEncData->pCallbacks->EventHandler(pAudioEncData->hSelf,
                                    pAudioEncData->pAppData,
                                    COMP_EventError,
                                    ERR_AENC_INVALIDSTATE,
                                    0,
                                    NULL);
                            pAudioEncData->pCallbacks->EventHandler(pAudioEncData->hSelf,
                                    pAudioEncData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pAudioEncData->state,
                                    NULL);
                            break;
                        }
                        case COMP_StateLoaded:
                        {
                            if (pAudioEncData->state != COMP_StateIdle)
                            {
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf,
                                        pAudioEncData->pAppData,
                                        COMP_EventError,
                                        ERR_AENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            alogv("AEnc_Comp StateLoaded begin");
                            if(pAudioEncData->mInputPortTunnelFlag)
                            {
                                pthread_mutex_lock(&pAudioEncData->mInputPcmMutex);
                                if(!list_empty(&pAudioEncData->mBufQ.mReadyList))
                                {
                                    struct list_head *pList;
                                    int cnt = 0;
                                    list_for_each(pList, &pAudioEncData->mBufQ.mReadyList) { cnt++; }
                                    aloge("fatal error! now [%d] audio input frames should not exist!", cnt);
                                }
                                pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
                            }
                            AudioEncResetChannel(pAudioEncData->hSelf);
                            pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
                            pAudioEncData->mWaitOutFrameFullFlag = TRUE;
                            //wait all outFrame return.
                            int cnt;
                            struct list_head *pList;
                            while(1)
                            {
                                cnt = 0;
                                list_for_each(pList, &pAudioEncData->mIdleOutFrameList)
                                {
                                    cnt++;
                                }
                                if(cnt < pAudioEncData->mFrameNodeNum)
                                {
                                    alogd("Wait AEnc idleOutFrameList full");
                                    pthread_cond_wait(&pAudioEncData->mOutFrameFullCondition, &pAudioEncData->mOutFrameListMutex);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            pAudioEncData->mWaitOutFrameFullFlag = FALSE;
                            pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
                            alogv("Wait AEnc idleOutFrameList full done");
                            pAudioEncData->state = COMP_StateLoaded;
                            pAudioEncData->pCallbacks->EventHandler(
                                    pAudioEncData->hSelf, pAudioEncData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pAudioEncData->state,
                                    NULL);
                            alogd("AEnc_Comp StateLoaded ok");
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if(pAudioEncData->state == COMP_StateLoaded)
                            {
                                alogv("AEnc_Comp: loaded->idle ...");
                                AudioEncLibInit(pAudioEncData);
                                pAudioEncData->state = COMP_StateIdle;
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf,
                                        pAudioEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioEncData->state,
                                        NULL);
                            }
                            else if(pAudioEncData->state == COMP_StatePause || pAudioEncData->state == COMP_StateExecuting)
                            {
                                alogv("AEnc_Comp: pause/executing[0x%x]->idle ...", pAudioEncData->state);
                                ReturnAllAudioInputFrames(pAudioEncData);
                                pAudioEncData->state = COMP_StateIdle;
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf,
                                        pAudioEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioEncData->state,
                                        NULL);
                            }
                            else
                            {
                                aloge("Fatal error! current state[0x%x] can't turn to idle!", pAudioEncData->state);
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf,
                                        pAudioEncData->pAppData,
                                        COMP_EventError,
                                        ERR_AENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pAudioEncData->state == COMP_StateIdle || pAudioEncData->state == COMP_StatePause)
                            {
                                pAudioEncData->state = COMP_StateExecuting;
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf,
                                        pAudioEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioEncData->state,
                                        NULL);
                            }
                            else
                            {
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf,
                                        pAudioEncData->pAppData,
                                        COMP_EventError,
                                        ERR_AENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pAudioEncData->state == COMP_StateIdle || pAudioEncData->state == COMP_StateExecuting)
                            {
                                pAudioEncData->state = COMP_StatePause;
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf,
                                        pAudioEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pAudioEncData->state,
                                        NULL);
                            }
                            else
                            {
                                pAudioEncData->pCallbacks->EventHandler(
                                        pAudioEncData->hSelf, pAudioEncData->pAppData,
                                        COMP_EventError,
                                        ERR_AENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            else if (cmd == Flush)
            {
            }
            else if (cmd == Stop)
            {
                // Kill thread
                goto EXIT;
            }
            else if(cmd == AEncComp_InputPcmAvailable)
            {
                pAudioEncData->mNoInputPcmFlag = 0;
            }
            else if(cmd == AEncComp_OutFrameAvailable)
            {
                pAudioEncData->mNoOutFrameFlag = 0;
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (pAudioEncData->state == COMP_StateExecuting)
        {
            if(pAudioEncData->mInputPortTunnelFlag)
            {
                //try to send all input audio frames to aencLib.
                pthread_mutex_lock(&pAudioEncData->mInputPcmMutex);
                if(!list_empty(&pAudioEncData->mBufQ.mReadyList))
                {
                    AEncCompInputFrameNode *pEntry, *pTmp;
                    list_for_each_entry_safe(pEntry, pTmp, &pAudioEncData->mBufQ.mReadyList, mList)
                    {
                        if(-1 == pAudioEncData->first_audio_pts)
                        {
                            pAudioEncData->first_audio_pts = pEntry->mAudioFrame.mTimeStamp;
                        }
                        AENC_PCM_FRM_INFO pcmFrameInfo;
                        memset(&pcmFrameInfo, 0, sizeof(AENC_PCM_FRM_INFO));
                        pcmFrameInfo.frm_pts = pEntry->mAudioFrame.mTimeStamp;
                        pcmFrameInfo.p_frm_buff = pEntry->mAudioFrame.mpAddr;
                        int ret = WriteAudioStreamBuffer(pAudioEncData->pCedarA, (void *)&pcmFrameInfo, pEntry->mAudioFrame.mLen);
                        if(0 == ret)
                        {
                            COMP_BUFFERHEADERTYPE obh;
                            memset(&obh, 0, sizeof(obh));
                            obh.pOutputPortPrivate = (void*)&pEntry->mAudioFrame;
                            obh.nOutputPortIndex = pAudioEncData->sInPortTunnelInfo.nTunnelPortIndex;
                            obh.nInputPortIndex = pAudioEncData->sInPortTunnelInfo.nPortIndex;
                            int compRet = COMP_FillThisBuffer(pAudioEncData->sInPortTunnelInfo.hTunnel, &obh);
                            if(compRet != SUCCESS)
                            {
                                aloge("fatal error! AencChn[%d] return inputFrame fail[0x%x]", pAudioEncData->mMppChnInfo.mChnId, compRet);
                            }
                            list_move_tail(&pEntry->mList, &pAudioEncData->mBufQ.mIdleList);
                        }
                        else
                        {
                            alogd("AencChn[%d] RequestWriteBuf failed! maybe abs is full, stop sending pcm, start to encode.", pAudioEncData->mMppChnInfo.mChnId);
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
            }
                
            int getRet, releaseRet;
            __audio_enc_result_t ret = EncodeAudioStream(pAudioEncData->pCedarA);
            if(ret == ERR_AUDIO_ENC_NONE)
            {
TryGetAudioEncBuf:
                pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
                if(list_empty(&pAudioEncData->mIdleOutFrameList))
                {
                    alogw("Low probability! AEnc_Comp IdleOutFrameList is empty, malloc more!");
                    ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
                    if(pNode)
                    {
                        memset(pNode, 0, sizeof(ENCODER_NODE_T));
                        list_add_tail(&pNode->mList, &pAudioEncData->mIdleOutFrameList);
                        pAudioEncData->mFrameNodeNum++;
                    }
                    else
                    {
                        aloge("Fatal error! malloc fail!");
                        pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
                        if(TMessage_WaitQueueNotEmpty(&pAudioEncData->cmd_queue, 200) > 0)
                        {
                            goto PROCESS_MESSAGE;
                        }
                        else
                        {
                            goto TryGetAudioEncBuf;
                        }
                    }
                }
                ENCODER_NODE_T *pEntry = list_first_entry(&pAudioEncData->mIdleOutFrameList, ENCODER_NODE_T, mList);
                getRet = RequestAudioFrameBuffer
                    (pAudioEncData->pCedarA
                    ,(char **)&pEntry->stEncodedStream.pBuffer
                    ,(unsigned int*)&pEntry->stEncodedStream.nBufferLen
                    ,&pEntry->stEncodedStream.nTimeStamp
                    ,&pEntry->stEncodedStream.nID
                    );
                pEntry->stEncodedStream.nFilledLen      = pEntry->stEncodedStream.nBufferLen;
                pEntry->stEncodedStream.media_type      = CDX_PacketAudio;
                pEntry->stEncodedStream.nTimeStamp      = pEntry->stEncodedStream.nTimeStamp * 1000; //unit ms-->us
                pEntry->stEncodedStream.pBufferExtra    = NULL;
                pEntry->stEncodedStream.nBufferExtraLen = 0;
                if (getRet != 0)
                {
                    alogv("getAudioEncBuf fail.");
                    pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
                    continue;
                }

                if (NULL == pEntry->stEncodedStream.pBuffer || 0 == pEntry->stEncodedStream.nBufferLen)
                {
                    aloge("Fatal error! Audio EncBuf[%p] size[%d], check AEncLib!", pEntry->stEncodedStream.pBuffer, pEntry->stEncodedStream.nBufferLen);
                }
/* 
                if (pAudioEncData->mSendPcmFailCnt)
                {
                    int track_cnt = pAudioEncData->mEncChnAttr.AeAttr.channels;
                    int sample_rate = pAudioEncData->mEncChnAttr.AeAttr.sampleRate;
                    pEntry->stEncodedStream.nTimeStamp += pAudioEncData->mFailPcmLen * 1000*1000 / (2*track_cnt*sample_rate);
					
                    long long tmp_cur_time = CDX_GetTimeUs();
                    if(-1!=pAudioEncData->last_chk_time && 30000000 <(tmp_cur_time-pAudioEncData->last_chk_time))   // print out every half minutes
                    {
                        aloge("aenc_pts_compensation:%lld",pAudioEncData->mFailPcmLen * 1000*1000 / (2*track_cnt*sample_rate));
                    }
                    pAudioEncData->last_chk_time = tmp_cur_time;  
                }
 */
#ifdef AENC_SAVE_AUDIO_BS
                fwrite(pEntry->stEncodedStream.pBuffer, 1, pEntry->stEncodedStream.nBufferLen, pAudioEncData->bs_fp);
                pAudioEncData->bs_sz += pEntry->stEncodedStream.nBufferLen;
                //MM_COMPONENTTYPE *pComp = (MM_COMPONENTTYPE*)pAudioEncData->hSelf;
                //pComp->FillThisBuffer(pComp, &obh);
#endif
                if(pAudioEncData->mOutputPortTunnelFlag)
                {
                    list_move_tail(&pEntry->mList, &pAudioEncData->mUsedOutFrameList);  //Notes:Must move buf node to UsedList before calling EmptyThisBuffer
                    pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);

                    MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)(pAudioEncData->sOutPortTunnelInfo.hTunnel);
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nOutputPortIndex   = pAudioEncData->sOutPortTunnelInfo.nPortIndex;
                    obh.nInputPortIndex    = pAudioEncData->sOutPortTunnelInfo.nTunnelPortIndex;
                    obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
                    int omxRet = pOutTunnelComp->EmptyThisBuffer(pOutTunnelComp, &obh);
                    if (SUCCESS==omxRet)
                    {
                        goto TryGetAudioEncBuf;
                    }
                    else
                    {
                        alogw("Be careful! AEnc_Comp output frame fail[0x%x], release it!", omxRet);
                        releaseRet = ReturnAudioFrameBuffer
                            (pAudioEncData->pCedarA
                            ,(char*)pEntry->stEncodedStream.pBuffer
                            ,pEntry->stEncodedStream.nBufferLen
                            ,pEntry->stEncodedStream.nTimeStamp
                            ,pEntry->stEncodedStream.nID
                            );
                        if (releaseRet != SUCCESS)
                        {
                            aloge("Fatal error! releaseAudioEncBuf fail!");
                        }
                        pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
                        list_move_tail(&pEntry->mList, &pAudioEncData->mIdleOutFrameList);
                        pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
                    }
                }
                else
                {
                    list_move_tail(&pEntry->mList, &pAudioEncData->mReadyOutFrameList);
                    if(pAudioEncData->mWaitOutFrameFlag)
                    {
                        pthread_cond_signal(&pAudioEncData->mOutFrameCondition);
                    }
                    pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
                    char tmpWrCh = 'A';
                    write(pAudioEncData->mOutputFrameNotifyPipeFds[1], &tmpWrCh, 1);
                }
            }
            else if (ret == ERR_AUDIO_ENC_PCMUNDERFLOW)
            {
                // alogd("AEnc_pcm_buf_udr");
                pthread_mutex_lock(&pAudioEncData->mInputPcmMutex);
                BOOL nDataAvailFlag;
                int dataSize = AudioEncoder_GetValidPcmDataSize(pAudioEncData->pCedarA);
                int nLeftSampleNum = dataSize/(pAudioEncData->mAudioInfo.nInChan*pAudioEncData->mAudioInfo.nSamplerBits/8);
                if(nLeftSampleNum > MAXDECODESAMPLE)
                {
                    nDataAvailFlag = TRUE;
                }
                else
                {
                    nDataAvailFlag = FALSE;
                }
                // nLeftSampleNum may bigger than 1024 when adpcm
                if(nDataAvailFlag /*&& (pAudioEncData->mAudioInfo.nType!=AUDIO_ENCODER_ADPCM_TYPE)*/)
                {
                    alogw("Low probability! AudioEncoder left pcmDataSize[%d],[%d]samples", dataSize, nLeftSampleNum);
                    pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
                }
                else
                {
                    if(FALSE == pAudioEncData->mInputPortTunnelFlag)
                    {
                        pAudioEncData->mNoInputPcmFlag = 1;
                        pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
                        TMessage_WaitQueueNotEmpty(&pAudioEncData->cmd_queue, 1000);
                    }
                    else
                    {
                        if(list_empty(&pAudioEncData->mBufQ.mReadyList))
                        {
                            pAudioEncData->mNoInputPcmFlag = 1;
                            pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
                            TMessage_WaitQueueNotEmpty(&pAudioEncData->cmd_queue, 0);
                        }
                        else
                        {
                            pthread_mutex_unlock(&pAudioEncData->mInputPcmMutex);
                        }
                    }
                }
            }
            else if(ret == ERR_AUDIO_ENC_OUTFRAME_UNDERFLOW)
            {
                alogw("AEnc_bs_buf_overflow");
                //pthread_mutex_lock(&pAudioEncData->mOutFrameListMutex);
                int emptyFrameNum = AudioEncoder_GetEmptyFrameNum(pAudioEncData->pCedarA);
                if(emptyFrameNum > 1)
                {
                    alogw("Low probability! AEncLib has empty frames[%d]", emptyFrameNum);
                    //pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
                }
                else
                {
                    pAudioEncData->mNoOutFrameFlag = 1;
                    //pthread_mutex_unlock(&pAudioEncData->mOutFrameListMutex);
                    TMessage_WaitQueueNotEmpty(&pAudioEncData->cmd_queue, 0);
                }
            }
            else
            {
                aloge("Unexpected ret[%d], sleep[%d]ms", ret, WAIT_PCMBUF_READY/1000);
                usleep(WAIT_PCMBUF_READY);
            }
        }
        else
        {
            alogv("AEnc_Comp not StateExecuting");
            TMessage_WaitQueueNotEmpty(&pAudioEncData->cmd_queue, 0);
        }
    }
EXIT:
    alogd("AEnc channel ComponentThread stopped!");
    return (void*) SUCCESS;
}
