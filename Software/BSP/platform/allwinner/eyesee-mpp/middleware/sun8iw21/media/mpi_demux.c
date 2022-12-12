/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_demux.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/09
  Last Modified :
  Description   : mpi functions implement for demux module.
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "mpi_demux"
#include <utils/plat_log.h>

//ref platform headers
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "mm_common.h"
#include "mm_comm_demux.h"
#include "mpi_demux.h"

//media internal common headers.
#include <tsemaphore.h>
#include <media_common.h>
#include <media_common_vcodec.h>
#include <media_common_acodec.h>

#include <mm_component.h>
//#include <mpi_mux_common.h>
#include <CdxParser.h>

typedef struct DEMUX_CHN_MAP_S
{
    DEMUX_CHN           mDemuxChn;     // demux channel index, [0, DEMUX_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mDemuxComp;  // demux component instance
    cdx_sem_t           mSemCompCmd;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}DEMUX_CHN_MAP_S;

typedef struct DemuxChnManager
{
    struct list_head mList;   //element type: DEMUX_CHN_MAP_S
    pthread_mutex_t mLock;
}DemuxChnManager;

static DemuxChnManager *gpDemuxChnMap = NULL;

ERRORTYPE DEMUX_Construct(void)
{
    int ret;
    if (gpDemuxChnMap != NULL) {
        return SUCCESS;
    }
    gpDemuxChnMap = (DemuxChnManager*)malloc(sizeof(DemuxChnManager));
    if (NULL == gpDemuxChnMap) {
        aloge("alloc DemuxChnManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    ret = pthread_mutex_init(&gpDemuxChnMap->mLock, NULL);
    if (ret != 0) {
        aloge("fatal error! mutex init fail");
        free(gpDemuxChnMap);
        gpDemuxChnMap = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpDemuxChnMap->mList);
    return SUCCESS;
}

ERRORTYPE DEMUX_Destruct(void)
{
    if (gpDemuxChnMap != NULL) {
        if (!list_empty(&gpDemuxChnMap->mList)) {
            aloge("fatal error! some demux channel still running when destroy demux device!");
        }
        pthread_mutex_destroy(&gpDemuxChnMap->mLock);
        free(gpDemuxChnMap);
        gpDemuxChnMap = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE DEMUX_searchExistChannel_l(DEMUX_CHN DemuxChn, DEMUX_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    DEMUX_CHN_MAP_S* pEntry;

    if (gpDemuxChnMap == NULL) {
        return FAILURE;
    }

    list_for_each_entry(pEntry, &gpDemuxChnMap->mList, mList)
    {
        if(pEntry->mDemuxChn == DemuxChn)
        {
            if(ppChn)
            {
                *ppChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

static ERRORTYPE DEMUX_searchExistChannel(DEMUX_CHN DemuxChn, DEMUX_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    DEMUX_CHN_MAP_S* pEntry;

    if (gpDemuxChnMap == NULL) {
        return FAILURE;
    }

    pthread_mutex_lock(&gpDemuxChnMap->mLock);
    ret = DEMUX_searchExistChannel_l(DemuxChn, ppChn);
    pthread_mutex_unlock(&gpDemuxChnMap->mLock);
    return ret;
}

static ERRORTYPE DEMUX_addChannel_l(DEMUX_CHN_MAP_S *pChn)
{
    if (gpDemuxChnMap == NULL) {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpDemuxChnMap->mList);
    return SUCCESS;
}

static ERRORTYPE DEMUX_addChannel(DEMUX_CHN_MAP_S *pChn)
{
    if (gpDemuxChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpDemuxChnMap->mLock);
    ERRORTYPE ret = DEMUX_addChannel_l(pChn);
    pthread_mutex_unlock(&gpDemuxChnMap->mLock);
    return SUCCESS;
}

static ERRORTYPE DEMUX_removeChannel(DEMUX_CHN_MAP_S *pChn)
{
    if (gpDemuxChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpDemuxChnMap->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpDemuxChnMap->mLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *DEMUX_GetChnComp(MPP_CHN_S *pMppChn)
{
    DEMUX_CHN_MAP_S* pChn;

    if (DEMUX_searchExistChannel(pMppChn->mChnId, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mDemuxComp;
}

static DEMUX_CHN_MAP_S* DEMUX_CHN_MAP_S_Construct()
{
    DEMUX_CHN_MAP_S *pChannel = (DEMUX_CHN_MAP_S*)malloc(sizeof(DEMUX_CHN_MAP_S));
    if(NULL == pChannel)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(DEMUX_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}

static void DEMUX_CHN_MAP_S_Destruct(DEMUX_CHN_MAP_S *pChannel)
{
    if(pChannel->mDemuxComp)
    {
        aloge("fatal error! Demux component need free before!");
        COMP_FreeHandle(pChannel->mDemuxComp);
        pChannel->mDemuxComp = NULL;
    }
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    free(pChannel);
}


static ERRORTYPE DemuxEventHandler(
	 COMP_HANDLETYPE hComponent,
	 void* pAppData,
	 COMP_EVENTTYPE eEvent,
	 unsigned int nData1,
	 unsigned int nData2,
	 void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S DemuxChnInfo;
    ret = ((MM_COMPONENTTYPE*)hComponent)->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &DemuxChnInfo);
    if(ret == SUCCESS)
    {
        alogv("demux event, MppChannel[%d][%d][%d]", DemuxChnInfo.mModId, DemuxChnInfo.mDevId, DemuxChnInfo.mChnId);
    }
	DEMUX_CHN_MAP_S *pChn = (DEMUX_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("Demux EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogw("Low probability! What command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventError:
        {
            if(ERR_DEMUX_SAMESTATE == nData1)
            {
                alogw("Demux set same StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_DEMUX_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("Why invalid state transition?! CurState[%#x]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                aloge("Fatal error! What command[%#x]?!", nData1);
                break;
            }
        }
        case COMP_EventBufferFlag:
        {
            alogw("EOF found!");
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_DEMUX;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pChn->mDemuxChn;
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_NOTIFY_EOF, NULL);
            break;
        }
        default:
        {
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
        }
    }
	return SUCCESS;
}

COMP_CALLBACKTYPE DemuxCallback = {
	.EventHandler = DemuxEventHandler,
	.EmptyBufferDone = NULL,
	.FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_DEMUX_CreateChn(DEMUX_CHN dmxChn, const DEMUX_CHN_ATTR_S *pAttr)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal DemuxAttr!");
        return ERR_DEMUX_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&gpDemuxChnMap->mLock);
    if(SUCCESS == DEMUX_searchExistChannel_l(dmxChn, NULL))
    {
        pthread_mutex_unlock(&gpDemuxChnMap->mLock);
        return ERR_DEMUX_EXIST;
    }
    DEMUX_CHN_MAP_S *pNode = DEMUX_CHN_MAP_S_Construct();
    pNode->mDemuxChn = dmxChn;

    //create Demux Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mDemuxComp, CDX_ComponentNameDemux, (void*)pNode, &DemuxCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_DEMUX;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mDemuxChn;
    eRet = pNode->mDemuxComp->SetConfig(pNode->mDemuxComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = pNode->mDemuxComp->SetConfig(pNode->mDemuxComp, COMP_IndexVendorDemuxChnAttr, (void*)pAttr);
    eRet = pNode->mDemuxComp->SetConfig(pNode->mDemuxComp, COMP_IndexVendorDemuxSetDataSource, NULL);
    eRet = pNode->mDemuxComp->SetConfig(pNode->mDemuxComp, COMP_IndexVendorDemuxPreparePorts, NULL);
    if (SUCCESS != eRet)
    {
        COMP_FreeHandle(pNode->mDemuxComp);
        pNode->mDemuxComp = NULL;
        DEMUX_CHN_MAP_S_Destruct(pNode);
        pthread_mutex_unlock(&gpDemuxChnMap->mLock);
        return ERR_DEMUX_FILE_EXCEPTION;
    }
    eRet = pNode->mDemuxComp->SendCommand(pNode->mDemuxComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create Demux Component done!
    DEMUX_addChannel_l(pNode);
    pthread_mutex_unlock(&gpDemuxChnMap->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_DEMUX_DestroyChn(DEMUX_CHN dmxChn)
{
    ERRORTYPE ret;
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    ERRORTYPE eRet;
    if(pChn->mDemuxComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            {
                eRet = pChn->mDemuxComp->SendCommand(pChn->mDemuxComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                cdx_sem_down(&pChn->mSemCompCmd);
                eRet = SUCCESS;
            }
            else if(nCompState == COMP_StateLoaded)
            {
                eRet = SUCCESS;
            }
            else if(nCompState == COMP_StateInvalid)
            {
                alogw("Low probability! Component StateInvalid?");
                eRet = SUCCESS;
            }
            else
            {
                aloge("fatal error! invalid DemuxChn[%d] state[0x%x]!", dmxChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                DEMUX_removeChannel(pChn);
                COMP_FreeHandle(pChn->mDemuxComp);
                pChn->mDemuxComp = NULL;
                DEMUX_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_DEMUX_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_DEMUX_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no Demux component!");
        list_del(&pChn->mList);
        DEMUX_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_RegisterCallback(DEMUX_CHN dmxChn, MPPCallbackInfo *pCallback)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DmxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_DEMUX_SetChnAttr(DEMUX_CHN dmxChn, DEMUX_CHN_ATTR_S *pAttr)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid dmxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_DEMUX_NOT_PERM;
    }
    ret = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_GetChnAttr(DEMUX_CHN dmxChn, DEMUX_CHN_ATTR_S *pAttr)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_DEMUX_NOT_PERM;
    }
    ret = pChn->mDemuxComp->GetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxChnAttr, (void*)pAttr);
    return ret;
}

static ERRORTYPE convert_CdxMediaInfoT_to_DEMUX_MEDIA_INFO_S(DEMUX_MEDIA_INFO_S *pDemuxMediaInfo, CdxMediaInfoT *pCdxMediaInfo)
{
    memset(pDemuxMediaInfo, 0, sizeof(DEMUX_MEDIA_INFO_S));
    struct CdxProgramS *pProgram = &pCdxMediaInfo->program[0];
    int i;
    pDemuxMediaInfo->mFileSize = pCdxMediaInfo->fileSize;
    pDemuxMediaInfo->mDuration = pProgram->duration;
    if(pProgram->videoNum > 0)
    {
        pDemuxMediaInfo->mVideoNum = pProgram->videoNum;
        if(pDemuxMediaInfo->mVideoNum > DEMUX_MAX_VIDEO_STREAM_NUM)
        {
            aloge("fatal error! videoNum[%d]>max[%d]", pDemuxMediaInfo->mVideoNum, DEMUX_MAX_VIDEO_STREAM_NUM);
            pDemuxMediaInfo->mVideoNum = DEMUX_MAX_VIDEO_STREAM_NUM;
        }
        pDemuxMediaInfo->mVideoIndex = pProgram->videoIndex;
        if(pDemuxMediaInfo->mVideoIndex >= pDemuxMediaInfo->mVideoNum)
        {
            aloge("fatal error! videoIndex[%d]>num[%d]", pDemuxMediaInfo->mVideoIndex, pDemuxMediaInfo->mVideoNum);
        }
        for(i=0; i<pDemuxMediaInfo->mVideoNum; i++)
        {
            pDemuxMediaInfo->mVideoStreamInfo[i].mCodecType = map_EVIDEOCODECFORMAT_to_PAYLOAD_TYPE_E(pProgram->video[i].eCodecFormat);
            pDemuxMediaInfo->mVideoStreamInfo[i].mWidth = pProgram->video[i].nWidth;
            pDemuxMediaInfo->mVideoStreamInfo[i].mHeight = pProgram->video[i].nHeight;
            pDemuxMediaInfo->mVideoStreamInfo[i].mFrameRate = pProgram->video[i].nFrameRate;
            pDemuxMediaInfo->mVideoStreamInfo[i].mAvgBitsRate = pCdxMediaInfo->bitrate;
            pDemuxMediaInfo->mVideoStreamInfo[i].mMaxBitsRate = pCdxMediaInfo->bitrate;
            VideoStreamInfo *pVideoStreamInfo = (void*)&pProgram->video[i];
            pDemuxMediaInfo->mVideoStreamInfo[i].nCodecSpecificDataLen = pVideoStreamInfo->nCodecSpecificDataLen;
            pDemuxMediaInfo->mVideoStreamInfo[i].pCodecSpecificData    = pVideoStreamInfo->pCodecSpecificData;
        }
    }
    if(pProgram->audioNum > 0)
    {
        pDemuxMediaInfo->mAudioNum = pProgram->audioNum;
        if(pDemuxMediaInfo->mAudioNum > DEMUX_MAX_AUDIO_STREAM_NUM)
        {
            aloge("fatal error! audioNum[%d]>max[%d]", pDemuxMediaInfo->mAudioNum, DEMUX_MAX_AUDIO_STREAM_NUM);
            pDemuxMediaInfo->mAudioNum = DEMUX_MAX_AUDIO_STREAM_NUM;
        }
        pDemuxMediaInfo->mAudioIndex = pProgram->audioIndex;
        if(pDemuxMediaInfo->mAudioIndex >= pDemuxMediaInfo->mAudioNum)
        {
            aloge("fatal error! audioIndex[%d]>num[%d]", pDemuxMediaInfo->mAudioIndex, pDemuxMediaInfo->mAudioNum);
        }
        for(i=0; i<pDemuxMediaInfo->mAudioNum; i++)
        {
            pDemuxMediaInfo->mAudioStreamInfo[i].mCodecType = map_EAUDIOCODECFORMAT_to_PAYLOAD_TYPE_E(pProgram->audio[i].eCodecFormat, pProgram->audio[i].eSubCodecFormat);
            pDemuxMediaInfo->mAudioStreamInfo[i].mChannelNum = pProgram->audio[i].nChannelNum;
            pDemuxMediaInfo->mAudioStreamInfo[i].mBitsPerSample = pProgram->audio[i].nBitsPerSample;
            pDemuxMediaInfo->mAudioStreamInfo[i].mSampleRate = pProgram->audio[i].nSampleRate;
            pDemuxMediaInfo->mAudioStreamInfo[i].mAvgBitsRate = pProgram->audio[i].nAvgBitrate;
            pDemuxMediaInfo->mAudioStreamInfo[i].mMaxBitsRate = pProgram->audio[i].nMaxBitRate;
        }
    }
    //subtitle
    if (pProgram->subtitleNum > 0)
    {
        pDemuxMediaInfo->mSubtitleNum = pProgram->subtitleNum;
        if (pDemuxMediaInfo->mSubtitleNum > DEMUX_MAX_SUBTITLE_STREAM_NUM)
        {
            aloge("fatal error! subtitleNum[%d]>max[%d]", \
                pDemuxMediaInfo->mSubtitleNum, DEMUX_MAX_SUBTITLE_STREAM_NUM);
            pDemuxMediaInfo->mSubtitleNum = DEMUX_MAX_SUBTITLE_STREAM_NUM;
        }
        pDemuxMediaInfo->mSubtitleIndex = pProgram->subtitleIndex;
        if (pDemuxMediaInfo->mSubtitleIndex >= pDemuxMediaInfo->mSubtitleNum)
        {
            aloge("fatal error! subtitleIndex[%d]>num[%d]", \
                pDemuxMediaInfo->mSubtitleIndex, pDemuxMediaInfo->mSubtitleNum);
        }
    }
    return SUCCESS;
}

ERRORTYPE AW_MPI_DEMUX_GetMediaInfo(DEMUX_CHN dmxChn, DEMUX_MEDIA_INFO_S *pDemuxMediaInfo)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nState);
    if(COMP_StateIdle != nState && COMP_StateExecuting!= nState && COMP_StatePause!= nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_DEMUX_NOT_PERM;
    }
    CdxMediaInfoT stMediaInfo;
    ret = pChn->mDemuxComp->GetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxMediaInfo, (void*)&stMediaInfo);
    convert_CdxMediaInfoT_to_DEMUX_MEDIA_INFO_S(pDemuxMediaInfo, &stMediaInfo);
    return ret;
}


ERRORTYPE AW_MPI_DEMUX_Start(DEMUX_CHN dmxChn)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mDemuxComp->SendCommand(pChn->mDemuxComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else
    {
        alogd("DemuxChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_Stop(DEMUX_CHN dmxChn)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        alogd("chn not exist");
        return ERR_DEMUX_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mDemuxComp->SendCommand(pChn->mDemuxComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogv("DemuxChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check DemuxChannelState[0x%x]!", nCompState);
        ret = FAILURE;
    }
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_Pause(DEMUX_CHN dmxChn)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if(COMP_StateExecuting == nCompState)
    {
        eRet = pChn->mDemuxComp->SendCommand(pChn->mDemuxComp, COMP_CommandStateSet, COMP_StatePause, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogv("DemuxChannelState[%#x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("Fatal error! Check DemuxChannelState[%#x]!", nCompState);
        ret = FAILURE;
    }
    return ret;
}

//ERRORTYPE AW_MPI_DEMUX_ResetChn(DEMUX_CHN dmxChn)
//{
//    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
//    {
//        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
//        return ERR_DEMUX_INVALID_CHNID;
//    }
//    DEMUX_CHN_MAP_S *pChn;
//    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
//    {
//        return ERR_DEMUX_UNEXIST;
//    }
//    COMP_STATETYPE nCompState = COMP_StateInvalid;
//    ERRORTYPE eRet, eRet2;
//    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
//    if(eRet == SUCCESS && COMP_StateIdle == nCompState)
//    {
//        eRet2 = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxResetChannel, NULL);
//        if(eRet2 != SUCCESS)
//        {
//            aloge("fatal error! reset channel fail[0x%x]!", eRet2);
//        }
//        return eRet2;
//    }
//    else
//    {
//        aloge("wrong status[0x%x], can't reset demux channel!", nCompState);
//        return ERR_DEMUX_NOT_PERM;
//    }
//}

ERRORTYPE AW_MPI_DEMUX_Seek(DEMUX_CHN dmxChn, int msec)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }
    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StatePause == nCompState)
    {
        CedarXSeekPara seekPara;
        memset(&seekPara, 0, sizeof(CedarXSeekPara));
        seekPara.seek_time = msec;
        eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorSeekToPosition, (void*)&seekPara);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! demux seek fail!");
        }
        ret = eRet;
    }
    else
    {
        alogd("seek in wrong DemuxChannelState[0x%x], do nothing!", nCompState);
        ret = ERR_DEMUX_INCORRECT_STATE_OPERATION;
    }
    return ret;
}

//ERRORTYPE AW_MPI_DEMUX_DisableTrack(DEMUX_CHN dmxChn, int nDisableTrack)
//{
//    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
//    {
//        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
//        return ERR_DEMUX_INVALID_CHNID;
//    }
//    DEMUX_CHN_MAP_S *pChn;
//    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
//    {
//        return ERR_DEMUX_UNEXIST;
//    }
//    int ret;
//    int eRet;
//    COMP_STATETYPE nCompState;
//    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
//    if(COMP_StateIdle == nCompState)
//    {
//        eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxDisableTrack, (void*)&nDisableTrack);
//        if(eRet != SUCCESS)
//        {
//            aloge("fatal error! send command stateExecuting fail");
//        }
//        ret = SUCCESS;
//    }
//    else
//    {
//        aloge("DisableTrack in WRONG State[%#x]!", nCompState);
//        ret = FAILURE;
//    }
//    return ret;
//}

//ERRORTYPE AW_MPI_DEMUX_DisableMediaType(DEMUX_CHN dmxChn, int nDisableMediaType)
//{
//    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
//    {
//        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
//        return ERR_DEMUX_INVALID_CHNID;
//    }
//    DEMUX_CHN_MAP_S *pChn;
//    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
//    {
//        return ERR_DEMUX_UNEXIST;
//    }
//    int ret;
//    int eRet;
//    COMP_STATETYPE nCompState;
//    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
//    if(COMP_StateIdle == nCompState)
//    {
//        eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxDisableMediaType, (void*)&nDisableMediaType);
//        if(eRet != SUCCESS)
//        {
//            aloge("fatal error! send command stateExecuting fail");
//        }
//        ret = SUCCESS;
//    }
//    else
//    {
//        alogw("DisableMediaType in WRONG State[%#x]!", nCompState);
//        ret = FAILURE;
//    }
//    return ret;
//}

ERRORTYPE AW_MPI_DEMUX_getDmxOutPutBuf(DEMUX_CHN dmxChn, EncodedStream *pDmxOutBuf, int nMilliSec)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }

    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if(COMP_StateExecuting == nCompState)
    {
        DemuxStream stDmxStream;
        stDmxStream.pEncodedStream = pDmxOutBuf;
        stDmxStream.nMilliSec = nMilliSec;
        eRet = pChn->mDemuxComp->GetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxOutBuffer, (void *)&stDmxStream);
        if(eRet != SUCCESS)
        {
            /*aloge("get buf fail[0x%x]", eRet);*/
            ret = eRet;
        }
        else
        {
            ret = SUCCESS;
        }
    }
    else
    {
        alogw("DisableMediaTrack in WRONG State[%#x]!", nCompState);
        ret = ERR_DEMUX_INCORRECT_STATE_OPERATION;
    }

    return ret;
}

ERRORTYPE AW_MPI_DEMUX_releaseDmxBuf(DEMUX_CHN dmxChn, EncodedStream *pDmxOutBuf)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }

    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StateIdle == nCompState)
    {
	    eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxOutBuffer, (void *)pDmxOutBuf);
	    if(eRet != SUCCESS)
	    {
            aloge("get buf fail");
            ret = FAILURE;
        }
	    else
	    {
            ret = SUCCESS;
        }
    }
    else
    {
	    //alogw("DisableMediaTrack in WRONG State[%#x]!", nCompState);
	    ret = ERR_DEMUX_INCORRECT_STATE_OPERATION;
    }
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_SelectVideoStream(DEMUX_CHN dmxChn, int nVideoIndex)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }


    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if (COMP_StateIdle == nCompState)
    {
        eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorSwitchVideo, (void *)&nVideoIndex);
        if (eRet != SUCCESS)
        {
            aloge("select video stream fail");
            return FAILURE;
        }
        else
        {
            ret = SUCCESS;
        }
    }
    else
    {
        alogw("Select video stream in WRONG State[%#x]!", nCompState);
        ret = FAILURE;
    }
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_SelectAudioStream(DEMUX_CHN dmxChn, int nAudioIndex)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }


    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if (COMP_StateIdle == nCompState)
    {
        eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorSwitchAudio, (void *)&nAudioIndex);
        if (eRet != SUCCESS)
        {
            aloge("select audio stream fail");
            return FAILURE;
        }
        else
        {
            ret = SUCCESS;
        }
    }
    else
    {
        alogw("Select audio stream in WRONG State[%#x]!", nCompState);
        ret = FAILURE;
    }
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_SelectSubtitleStream(DEMUX_CHN dmxChn, int nSubtitlrIndex)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }


    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if (COMP_StateIdle == nCompState)
    {
        eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorSwitchSubtitle, (void *)&nSubtitlrIndex);
        if (eRet != SUCCESS)
        {
            aloge("select subtitle stream fail");
            return FAILURE;
        }
        else
        {
            ret = SUCCESS;
        }
    }
    else
    {
        alogw("Select subtitle stream in WRONG State[%#x]!", nCompState);
        ret = FAILURE;
    }
    return ret;
}

ERRORTYPE AW_MPI_DEMUX_SwitchDateSource(DEMUX_CHN dmxChn, DEMUX_CHN_ATTR_S *pChnAttr)
{
    if(!(dmxChn>=0 && dmxChn <DEMUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid DemuxChn[%d]!", dmxChn);
        return ERR_DEMUX_INVALID_CHNID;
    }

    DEMUX_CHN_MAP_S *pChn;
    if(SUCCESS != DEMUX_searchExistChannel(dmxChn, &pChn))
    {
        return ERR_DEMUX_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mDemuxComp->GetState(pChn->mDemuxComp, &nCompState);
    if ((COMP_StatePause == nCompState) || (COMP_StateIdle == nCompState))
    {
        eRet = pChn->mDemuxComp->SetConfig(pChn->mDemuxComp, COMP_IndexVendorDemuxSwitchDateSource, (void *)pChnAttr);
        if (eRet != SUCCESS)
        {
            aloge("select subtitle stream fail");
            return FAILURE;
        }
        else
        {
            ret = SUCCESS;
        }
    }
    else
    {
        alogw("set next file now in WRONG State[%#x]!", nCompState);
        ret = FAILURE;
    }
    return ret;
}

