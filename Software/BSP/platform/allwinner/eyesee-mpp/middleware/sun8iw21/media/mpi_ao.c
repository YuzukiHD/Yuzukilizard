/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_ao.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/03/30
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "mpi_ao"
#include <utils/plat_log.h>

//ref platform headers
#include <plat_type.h>
#include <plat_errno.h>
#include <plat_defines.h>
#include <plat_math.h>
#include <cdx_list.h>

//media api headers to app
#include <mm_common.h>
#include <media_common.h>
#include <audio_hw.h>
#include <AIOCompStream.h>

static AO_CHANNEL_S *AOChannel_Construct(void)
{
    AO_CHANNEL_S *pChn = (AO_CHANNEL_S*)malloc(sizeof(AO_CHANNEL_S));
    if (pChn == NULL) {
        aloge("alloc AO_CHANNEL_S error[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChn, 0, sizeof(AO_CHANNEL_S));
    cdx_sem_init(&pChn->mSemCompCmd, 0);
    cdx_sem_init(&pChn->mSemWaitAudioBufRelease, 0);
    return pChn;
}

static void AOChannel_Destruct(AO_CHANNEL_S *pChn)
{
    if (pChn != NULL) {
        if (pChn->mpComp != NULL) {
            aloge("fatal error! AO component need free before!");
            COMP_FreeHandle(pChn->mpComp);
            pChn->mpComp = NULL;
        }
    }
    cdx_sem_deinit(&pChn->mSemCompCmd);
    cdx_sem_deinit(&pChn->mSemWaitAudioBufRelease);
    free(pChn);
}

static ERRORTYPE AOChannel_EventHandler(
     PARAM_IN COMP_HANDLETYPE hComponent,
     PARAM_IN void *pAppData,
     PARAM_IN COMP_EVENTTYPE eEvent,
     PARAM_IN unsigned int nData1,
     PARAM_IN unsigned int nData2,
     PARAM_IN void *pEventData)
{
    AO_CHANNEL_S *pChn = (AO_CHANNEL_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1) {
                //alogv("audio device EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            } else {
                alogw("Low probability! what command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventBufferFlag:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_AO;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pChn->mId;
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_NOTIFY_EOF, NULL);
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
    return SUCCESS;
}

static ERRORTYPE AOChannel_EmptyBufferDone(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN void* pAppData,
        PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    AO_CHANNEL_S *pChn = (AO_CHANNEL_S*)pAppData;
    AUDIO_FRAME_S *pAFrame = (AUDIO_FRAME_S*)pBuffer->pOutputPortPrivate;
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_AO;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pChn->mId;
    /* AO_AUDIO_FRAME_ID_INVALID: means usr send frame synchronously */
    if (AO_AUDIO_FRAME_ID_INVALID == pAFrame->mId)
    {
        cdx_sem_up(&pChn->mSemWaitAudioBufRelease);
        return SUCCESS;
    }
    CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
    pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_RELEASE_AUDIO_BUFFER, (void*)pAFrame);
    return SUCCESS;
}

static COMP_CALLBACKTYPE AOChannel_Callback = {
    .EventHandler = AOChannel_EventHandler,
    .EmptyBufferDone = AOChannel_EmptyBufferDone,
    .FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_AO_SetPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, const AIO_ATTR_S *pstAttr)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return AudioHw_AO_SetPubAttr(AudioDevId,AoChn, pstAttr);
}

ERRORTYPE AW_MPI_AO_GetPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, AIO_ATTR_S *pstAttr)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return AudioHw_AO_GetPubAttr(AudioDevId, AoChn, pstAttr);
}

ERRORTYPE AW_MPI_AO_ClrPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_ClrPubAttr(AudioDevId, AoChn);
}

ERRORTYPE AW_MPI_AO_SetTrackMode(AUDIO_DEV AudioDevId, AO_CHN AoChn,AUDIO_TRACK_MODE_E enTrackMode)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_SetTrackMode(AudioDevId, AoChn, enTrackMode);
}

ERRORTYPE AW_MPI_AO_GetTrackMode(AUDIO_DEV AudioDevId,AO_CHN AoChn, AUDIO_TRACK_MODE_E *penTrackMode)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_GetTrackMode(AudioDevId, AoChn, penTrackMode);
}

ERRORTYPE AW_MPI_AO_Enable(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_Enable(AudioDevId, AoChn);
}

ERRORTYPE AW_MPI_AO_Disable(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_Disable(AudioDevId, AoChn);
}

ERRORTYPE AW_MPI_AO_SetDevDrc(AUDIO_DEV AudioDevId, int enable)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_SetDacDrc(AudioDevId, enable);
}

ERRORTYPE AW_MPI_AO_SetDevHpf(AUDIO_DEV AudioDevId, int enable)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_SetDacHpf(AudioDevId, enable);
}

ERRORTYPE AW_MPI_AO_SetDevVolume(AUDIO_DEV AudioDevId, int s32VolumeDb)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_SetVolume(AudioDevId, s32VolumeDb);
}

ERRORTYPE AW_MPI_AO_GetDevVolume(AUDIO_DEV AudioDevId, int *ps32VolumeDb)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_GetVolume(AudioDevId, ps32VolumeDb);
}

ERRORTYPE AW_MPI_AO_SetDevMute(AUDIO_DEV AudioDevId, BOOL bEnable, AUDIO_FADE_S *pstFade)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_SetMute(AudioDevId, bEnable, pstFade);
}

ERRORTYPE AW_MPI_AO_GetDevMute(AUDIO_DEV AudioDevId, BOOL *pbEnable, AUDIO_FADE_S *pstFade)
{
    CHECK_AO_DEV_ID(AudioDevId);
    return audioHw_AO_GetMute(AudioDevId, pbEnable, pstFade);
}

ERRORTYPE AW_MPI_AO_SetChnMute(AUDIO_DEV AudioDevId, AO_CHN AoChn, BOOL bMute)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn))
    {
        return ERR_AO_UNEXIST;
    }
    return COMP_SetConfig(pChn->mpComp, COMP_IndexVendorAOChnMute, &bMute);
}
ERRORTYPE AW_MPI_AO_GetChnMute(AUDIO_DEV AudioDevId, AO_CHN AoChn, BOOL* pbMute)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn))
    {
        return ERR_AO_UNEXIST;
    }
    return COMP_GetConfig(pChn->mpComp, COMP_IndexVendorAOChnMute, pbMute);
}


ERRORTYPE AW_MPI_AO_CreateChn(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret = SUCCESS;

    if (audioHw_AO_IsDevStarted(AudioDevId, AoChn)) {
        alogw("AODev has started when enableChn(%d)! It must be started by other chns!", AoChn);
    }
    if (audioHw_AO_IsDevConfigured(AudioDevId,AoChn)) {
        AW_MPI_AO_Enable(AudioDevId,AoChn);
    }
    audioHw_AO_Dev_lock(AudioDevId);
    if (SUCCESS == audioHw_AO_searchChannel_l(AudioDevId, AoChn, &pChn)) {
        audioHw_AO_Dev_unlock(AudioDevId);
        return ERR_AO_EXIST;
    }
    audioHw_AO_Dev_unlock(AudioDevId);

    pChn = AOChannel_Construct();
    pChn->mId = AoChn;

    //create AO Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pChn->mpComp, CDX_ComponentNameAOChannel, (void*)pChn, &AOChannel_Callback);
    if (eRet != SUCCESS) {
        aloge("fatal error! get comp handle fail!");
        return ERR_AO_NOT_ENABLED;
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_AO;
    ChannelInfo.mDevId = AudioDevId;
    ChannelInfo.mChnId = AoChn;
    ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    char avSyncFlag = 1;
    ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorSetAVSync, &avSyncFlag);
    COMP_TIME_CONFIG_ACTIVEREFCLOCKTYPE ref_clock;
    ref_clock.eClock = COMP_TIME_RefClockAudio;
    ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexConfigTimeActiveRefClock, &ref_clock);
    ret = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pChn->mSemCompCmd);
    //create AO Component done!

    audioHw_AO_Dev_lock(AudioDevId);
    audioHw_AO_AddChannel_l(AudioDevId, pChn);
    audioHw_AO_Dev_unlock(AudioDevId);
    return ret;
}

ERRORTYPE AW_MPI_AO_DestroyChn(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    ERRORTYPE eRet;
    if(pChn->mpComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pChn->mpComp->GetState(pChn->mpComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            {
                eRet = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
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
                aloge("fatal error! invalid VoChn[%d] state[0x%x]!", AoChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                audioHw_AO_RemoveChannel(AudioDevId, pChn);
                COMP_FreeHandle(pChn->mpComp);
                pChn->mpComp = NULL;
                AOChannel_Destruct(pChn);
                audioHw_AO_Disable(AudioDevId,AoChn);
                audioHw_AO_ClrPubAttr(AudioDevId,AoChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_AO_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_AO_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no AO component!");
        list_del(&pChn->mList);
        AOChannel_Destruct(pChn);
        audioHw_AO_Disable(AudioDevId,AoChn);
        audioHw_AO_ClrPubAttr(AudioDevId,AoChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_AO_RegisterCallback(AUDIO_DEV AudioDevId, AO_CHN AoChn, MPPCallbackInfo *pCallback)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_AO_SetPcmCardType(AUDIO_DEV AudioDevId, AO_CHN AoChn, PCM_CARD_TYPE_E cardId)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    ERRORTYPE ret, eRet;
    COMP_STATETYPE nCompState = COMP_StateInvalid;
    eRet = pChn->mpComp->GetState(pChn->mpComp, &nCompState);
    if (COMP_StateIdle == nCompState)
    {
        ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAOChnPcmCardType, &cardId);
    }
    else
    {
        aloge("fatal error! why AoChn(%d) state is %d? should set pcm_card when idle!!!", AoChn, nCompState);
        ret = ERR_AO_INCORRECT_STATE_OPERATION;
    }

    return ret;
}

ERRORTYPE AW_MPI_AO_StartChn(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret, eRet;

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    COMP_STATETYPE nCompState = COMP_StateInvalid;
    eRet = pChn->mpComp->GetState(pChn->mpComp, &nCompState);
    if (COMP_StateIdle == nCompState || COMP_StatePause == nCompState) {
        eRet = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    } else if (COMP_StateExecuting == nCompState) {
        alogd("AOChannel[%d] already stateExecuting.", AoChn);
        ret = SUCCESS;
    } else {
        aloge("wrong status[0x%x], can't start ao channel!", nCompState);
        ret = ERR_AO_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_AO_StopChn(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret, eRet;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    COMP_STATETYPE nCompState = COMP_StateInvalid;
    eRet = pChn->mpComp->GetState(pChn->mpComp, &nCompState);
    if (COMP_StateExecuting == nCompState || COMP_StatePause== nCompState)
    {
        eRet = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if (COMP_StateIdle == nCompState)
    {
        alogd("AOChannel[%d] already stateIdle.", AoChn);
        ret = SUCCESS;
    }
    else
    {
        aloge("wrong status[0x%x], can't stop ao channel!", nCompState);
        ret = ERR_AO_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_AO_SendFrame(AUDIO_DEV AudioDevId, AO_CHN AoChn, AUDIO_FRAME_S *pstAFrame, int s32MilliSec)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

	/*  Story:
	 *  comment the state check, no need to call api:
	 *  AW_MPI_AO_SetPubAttr() and
	 *  AW_MPI_AO_Enable()
	 *  just construct the valid frm info when call AW_MPI_AO_SendFrame(). 
	*/
/* 
    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }
 */
    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    if (AO_AUDIO_FRAME_ID_INVALID == pstAFrame->mId)
    {
        aloge("invalid audio frame id %d ", pstAFrame->mId);
        return ERR_AO_ILLEGAL_PARAM;
    }

    COMP_BUFFERHEADERTYPE compBuffer;
    compBuffer.pOutputPortPrivate = (void*)pstAFrame;
    ret = COMP_EmptyThisBuffer(pChn->mpComp, &compBuffer);
    return ret;
}

ERRORTYPE AW_MPI_AO_SendFrameSync(AUDIO_DEV AudioDevId, AO_CHN AoChn, AUDIO_FRAME_S *pstAFrame)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret = SUCCESS;
    int result = 0;

//    if (!audioHw_AO_IsDevStarted(AudioDevId)) {
//        return ERR_AO_NOT_ENABLED;
//    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    // ignore usr pstAFrame->mId and don't change it, just use it to identify
    // sync or async in AOChannel_EmptyBufferDone().
    AUDIO_FRAME_S tempAFrame;
    memcpy(&tempAFrame, pstAFrame, sizeof(AUDIO_FRAME_S));
    tempAFrame.mId = AO_AUDIO_FRAME_ID_INVALID;

    COMP_BUFFERHEADERTYPE compBuffer;
    compBuffer.pOutputPortPrivate = (void*)&tempAFrame;
    ret = COMP_EmptyThisBuffer(pChn->mpComp, &compBuffer);

    // blocked here, until the audio buf can be released
    cdx_sem_down(&pChn->mSemWaitAudioBufRelease);

    return ret;
}

ERRORTYPE AW_MPI_AO_EnableReSmp(AUDIO_DEV AudioDevId, AO_CHN AoChn, AUDIO_SAMPLE_RATE_E enInSampleRate)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

//    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
//        return ERR_AO_NOT_ENABLED;
//    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOReSmpEnable, &enInSampleRate);
}

ERRORTYPE AW_MPI_AO_DisableReSmp(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOReSmpDisable, NULL);
}

ERRORTYPE AW_MPI_AO_QueryChnStat(AUDIO_DEV AudioDevId ,AO_CHN AoChn, AO_CHN_STATE_S *pstStatus)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->GetConfig(pChn->mpComp, COMP_IndexVendorAOQueryChnStat, pstStatus);
}

ERRORTYPE AW_MPI_AO_PauseChn(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }
    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mpComp->GetState(pChn->mpComp, &nCompState);
    if(COMP_StateExecuting == nCompState)
    {
        eRet = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StatePause, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command statePause fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StatePause == nCompState)
    {
        alogd("AOChannel[%d] already statePause.", AoChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("AOChannel[%d] stateIdle, can't turn to statePause!", AoChn);
        ret = ERR_AO_INCORRECT_STATE_OPERATION;
    }
    else
    {
        aloge("fatal error! check AoChannel[%d] State[0x%x]!", AoChn, nCompState);
        ret = ERR_AO_INCORRECT_STATE_OPERATION;
    }
    return ret;
}

ERRORTYPE AW_MPI_AO_ResumeChn(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mpComp->GetState(pChn->mpComp, &nCompState);
    if(COMP_StatePause == nCompState)
    {
        eRet = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command statePause fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateExecuting == nCompState)
    {
        alogd("AOChannel[%d] already stateExecuting.", AoChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("AOChannel[%d] stateIdle, can't turn to stateExecuting!", AoChn);
        ret = ERR_AO_INCORRECT_STATE_OPERATION;
    }
    else
    {
        aloge("fatal error! check AoChannel[%d] State[0x%x]!", AoChn, nCompState);
        ret = ERR_AO_INCORRECT_STATE_OPERATION;
    }
    return ret;
}

ERRORTYPE AW_MPI_AO_Seek(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }
    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mpComp->GetState(pChn->mpComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorSeekToPosition, NULL);
    }
    else
    {
        aloge("fatal error! can't seek in AOChannel[%d] State[0x%x]!", AoChn, nCompState);
        ret = ERR_AO_INCORRECT_STATE_OPERATION;
    }
    return ret;
}

ERRORTYPE AW_MPI_AO_SetVqeAttr(AUDIO_DEV AudioDevId, AO_CHN AoChn, AO_VQE_CONFIG_S *pstVqeConfig)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeAttr, pstVqeConfig);
}

ERRORTYPE AW_MPI_AO_GetVqeAttr(AUDIO_DEV AudioDevId, AO_CHN AoChn, AO_VQE_CONFIG_S *pstVqeConfig)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->GetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeAttr, pstVqeConfig);
}

ERRORTYPE AW_MPI_AO_EnableVqe(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeEnable, NULL);
}

ERRORTYPE AW_MPI_AO_DisableVqe(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeDisable, NULL);
}

ERRORTYPE AW_MPI_AO_EnableSoftDrc(AUDIO_DEV AudioDevId, AO_CHN AoChn,AO_DRC_CONFIG_S *pstDrcConfig)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    /* if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    } */

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIODrcEnable, pstDrcConfig);
}
ERRORTYPE AW_MPI_AO_EnableAgc(AUDIO_DEV AudioDevId, AO_CHN AoChn, my_agc_handle *pstAgcConfig)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    /* if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    } */

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOAgcEnable, pstAgcConfig);
}

ERRORTYPE AW_MPI_AO_SetStreamEof(AUDIO_DEV AudioDevId, AO_CHN AoChn, BOOL bEofFlag, BOOL bDrainFlag)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        alogw("warning! ao dev was not started!");
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mpComp->GetState(pChn->mpComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
       aloge("wrong state[0x%x], return!", nState);
       return ERR_AO_NOT_PERM;
    }
    if(bEofFlag)
    {
        ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorSetStreamEof, &bDrainFlag);
    }
    else
    {
        ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorClearStreamEof, NULL);
    }
    return ret;
}

ERRORTYPE AW_MPI_AO_SaveFile(AUDIO_DEV AudioDevId, AO_CHN AoChn, AUDIO_SAVE_FILE_INFO_S *pstSaveFileInfo)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    /* if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
     *     return ERR_AO_NOT_ENABLED;
     * } */

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAOSetSaveFileInfo, pstSaveFileInfo);
}

ERRORTYPE AW_MPI_AO_QueryFileStatus(AUDIO_DEV AudioDevId, AO_CHN AoChn, AUDIO_SAVE_FILE_INFO_S *pstSaveFileInfo)
{
    CHECK_AO_DEV_ID(AudioDevId);
    CHECK_AO_CHN_ID(AoChn);
    AO_CHANNEL_S *pChn = NULL;

    if (!audioHw_AO_IsDevStarted(AudioDevId,AoChn)) {
        return ERR_AO_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AO_searchChannel(AudioDevId, AoChn, &pChn)) {
        return ERR_AO_UNEXIST;
    }

    return pChn->mpComp->GetConfig(pChn->mpComp, COMP_IndexVendorAOQueryFileStatus, pstSaveFileInfo);
}

ERRORTYPE AW_MPI_AO_SetPA(AUDIO_DEV AudioDevId, BOOL bLevelVal)
{
    CHECK_AO_DEV_ID(AudioDevId);

    return audioHw_AO_SetPA(AudioDevId, bLevelVal);
}

ERRORTYPE AW_MPI_AO_GetPA(AUDIO_DEV AudioDevId, BOOL *pbLevelVal)
{
    CHECK_AO_DEV_ID(AudioDevId);

    return audioHw_AO_GetPA(AudioDevId, pbLevelVal);
}
