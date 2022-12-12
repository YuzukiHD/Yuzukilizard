/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_ai.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/04/28
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "mpi_ai"
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
#include <AIOCompStream.h>
#include <audio_hw.h>


static AI_CHANNEL_S *AIChannel_Construct(void)
{
    AI_CHANNEL_S *pChn = (AI_CHANNEL_S*)malloc(sizeof(AI_CHANNEL_S));
    if (pChn == NULL) {
        aloge("alloc AI_CHANNEL_S error[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChn, 0, sizeof(AI_CHANNEL_S));
    cdx_sem_init(&pChn->mSemCompCmd, 0);
    return pChn;
}

static void AIChannel_Destruct(AI_CHANNEL_S *pChn)
{
    if (pChn != NULL) {
        if (pChn->mpComp != NULL) {
            aloge("fatal error! AI component need free before!");
            COMP_FreeHandle(pChn->mpComp);
            pChn->mpComp = NULL;
        }
    }
    cdx_sem_deinit(&pChn->mSemCompCmd);
    free(pChn);
}

static ERRORTYPE AIChannel_EventHandler(
     PARAM_IN COMP_HANDLETYPE hComponent,
     PARAM_IN void *pAppData,
     PARAM_IN COMP_EVENTTYPE eEvent,
     PARAM_IN unsigned int nData1,
     PARAM_IN unsigned int nData2,
     PARAM_IN void *pEventData)
{
    //ERRORTYPE ret;
    //MPP_CHN_S aiChnInfo;
    //MM_COMPONENTTYPE *pComp = (MM_COMPONENTTYPE*)hComponent;
    AI_CHANNEL_S *pChn = (AI_CHANNEL_S*)pAppData;

    //ret = pComp->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &aiChnInfo);
    //alogv("audio device event, MppChannel[%d][%d][%d]", aiChnInfo.mModId, aiChnInfo.mDevId, aiChnInfo.mChnId);

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("audio device EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogw("Low probability! what data[%#x, %#x]?", nData1, nData2);
                break;
            }
        }
        case COMP_EventError:
        {
            if(ERR_AI_INVALIDSTATE == nData1)
            {
                aloge("Impossible! Ai chn in invalid state!");
            }
            else if(ERR_AI_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("Ai chn happen wrong state transition!");
            }
            else if(ERR_AI_SAMESTATE == nData1)
            {
                aloge("AI chn change state to the same!");
            }
            else
            {
                aloge("Ai chn happen ErrorEvent with data[%#x, %#x]", nData1, nData2);
            }
            break;
        }
        case COMP_EventBufferPrefilled:
        {
            if(pChn->mCallbackInfo.callback)
            {
                MPP_CHN_S ChannelInfo = {MOD_ID_AI, pChn->mDevId, pChn->mId};
                AISendDataInfo *pUserData = (AISendDataInfo*)pEventData;
                pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_CAPTURE_AUDIO_DATA, (void*)pUserData);
            }
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
    return SUCCESS;
}

static COMP_CALLBACKTYPE AIChannel_Callback = {
    .EventHandler = AIChannel_EventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_AI_RegisterCallback(AUDIO_DEV AudioDevId, AI_CHN AiChn, MPPCallbackInfo *pCallback)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_AI_SetPubAttr(AUDIO_DEV AudioDevId, const AIO_ATTR_S *pstAttr)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SetPubAttr(AudioDevId, pstAttr);
}

ERRORTYPE AW_MPI_AI_GetPubAttr(AUDIO_DEV AudioDevId, AIO_ATTR_S *pstAttr)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_GetPubAttr(AudioDevId, pstAttr);
}

ERRORTYPE AW_MPI_AI_ClrPubAttr(AUDIO_DEV AudioDevId)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_ClrPubAttr(AudioDevId);
}

ERRORTYPE AW_MPI_AI_Enable(AUDIO_DEV AudioDevId)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_Enable(AudioDevId);
}

ERRORTYPE AW_MPI_AI_Disable(AUDIO_DEV AudioDevId)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_Disable(AudioDevId);
}

ERRORTYPE AW_MPI_AI_SetTrackMode(AUDIO_DEV AudioDevId, AUDIO_TRACK_MODE_E enTrackMode)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SetTrackMode(AudioDevId, enTrackMode);
}

ERRORTYPE AW_MPI_AI_GetTrackMode(AUDIO_DEV AudioDevId, AUDIO_TRACK_MODE_E *penTrackMode)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_GetTrackMode(AudioDevId, penTrackMode);
}

ERRORTYPE AW_MPI_AI_CreateChn(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret = SUCCESS;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        ret = audioHw_AI_Enable(AudioDevId);
        if (ret != SUCCESS)
        {
            aloge("enable AiDev failed!");
            return ERR_AI_NOT_ENABLED;
        }
    }
    audioHw_AI_Dev_lock(AudioDevId);
    ret = audioHw_AI_searchChannel_l(AudioDevId, AiChn, &pChn);
    if (SUCCESS != ret) {
        pChn = AIChannel_Construct();
        if (pChn == NULL) {
            audioHw_AI_Dev_unlock(AudioDevId);
            return ERR_AI_NOMEM;
        }
        pChn->mDevId = AudioDevId;
        pChn->mId = AiChn;
        ret = COMP_GetHandle((COMP_HANDLETYPE*)&pChn->mpComp, CDX_ComponentNameAIChannel, (void*)pChn, &AIChannel_Callback);
        MPP_CHN_S ChannelInfo;
        ChannelInfo.mModId = MOD_ID_AI;
        ChannelInfo.mDevId = AudioDevId;
        ChannelInfo.mChnId = AiChn;
        ret = pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
        ret = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        cdx_sem_down(&pChn->mSemCompCmd);
        audioHw_AI_AddChannel_l(AudioDevId, pChn);
    } else {
        ret = ERR_AI_EXIST;
    }
    audioHw_AI_Dev_unlock(AudioDevId);

    return ret;
}

ERRORTYPE AW_MPI_AI_DestroyChn(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret = SUCCESS;
    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }
    if (SUCCESS == audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn))
    {
        COMP_STATETYPE eState = COMP_StateInvalid;
        ERRORTYPE eRet;
        eRet = COMP_GetState(pChn->mpComp, &eState);
        if(eRet == SUCCESS)
        {
            if(COMP_StateIdle == eState)
            {
                eRet = COMP_SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                if(eRet != SUCCESS)
                {
                    aloge("fatal error! comp turn state fail[0x%x]!", eRet);
                }
                cdx_sem_down(&pChn->mSemCompCmd);
            }
            else if(COMP_StateLoaded == eState)
            {
                eRet = SUCCESS;
            }
            else
            {
                aloge("fatal error! AiChn[%d,%d] wrong state:0x%x", AudioDevId, AiChn, eState);
                eRet = FAILURE;
            }
        }
        else
        {
            aloge("fatal error! get status fail[0x%x]!", eRet);
        }
        audioHw_AI_RemoveChannel(AudioDevId, pChn);
        audioHw_AI_Disable(AudioDevId);
        COMP_FreeHandle(pChn->mpComp);
        pChn->mpComp = NULL;
        AIChannel_Destruct(pChn);
    } else {
        ret = ERR_AI_UNEXIST;
    }

    return ret;
}

ERRORTYPE AW_MPI_AI_EnableChn(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret = SUCCESS;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }
    if (SUCCESS == audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        ret = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        cdx_sem_down(&pChn->mSemCompCmd);
    } else {
        ret = ERR_AI_UNEXIST;
    }

    return ret;
}

ERRORTYPE AW_MPI_AI_DisableChn(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pChn->mSemCompCmd);

    return SUCCESS;
}

ERRORTYPE AW_MPI_AI_ResetChn(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    COMP_STATETYPE nCompState = COMP_StateInvalid;
    ERRORTYPE eRet, eRet2;
    eRet = pChn->mpComp->GetState(pChn->mpComp, &nCompState);
    if(eRet == SUCCESS && COMP_StateIdle == nCompState)
    {
        eRet2 = pChn->mpComp->SendCommand(pChn->mpComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
        if(eRet2 != SUCCESS)
        {
            aloge("fatal error! reset channel fail[0x%x]!", eRet2);
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        return eRet2;
    }
    else
    {
        aloge("wrong status[0x%x], can't reset ai channel!", nCompState);
        return ERR_AI_NOT_PERM;
    }
}

ERRORTYPE AW_MPI_AI_PauseChn(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
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
        alogd("AIChannel[%d] already statePause.", AiChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("AIChannel[%d] stateIdle, can't turn to statePause!", AiChn);
        ret = ERR_AI_INCORRECT_STATE_OPERATION;
    }
    else
    {
        aloge("fatal error! check AIChannel[%d] State[0x%x]!", AiChn, nCompState);
        ret = ERR_AI_INCORRECT_STATE_OPERATION;
    }
    return ret;
}

ERRORTYPE AW_MPI_AI_ResumeChn(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
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
        alogd("AIChannel[%d] already stateExecuting.", AiChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("AIChannel[%d] stateIdle, can't turn to stateExecuting!", AiChn);
        ret = ERR_AI_INCORRECT_STATE_OPERATION;
    }
    else
    {
        aloge("fatal error! check AIChannel[%d] State[0x%x]!", AiChn, nCompState);
        ret = ERR_AI_INCORRECT_STATE_OPERATION;
    }
    return ret;
}

ERRORTYPE AW_MPI_AI_GetFrame(AUDIO_DEV AudioDevId, AI_CHN AiChn, AUDIO_FRAME_S *pstFrm, AEC_FRAME_S *pstAecFrm, int s32MilliSec)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    AudioFrame audioFrm;
    audioFrm.pFrame = pstFrm;
    audioFrm.pAecFrame = pstAecFrm;
    audioFrm.nMilliSec = s32MilliSec;
    return pChn->mpComp->GetConfig(pChn->mpComp, COMP_IndexVendorAIChnGetValidFrame, &audioFrm);
}

ERRORTYPE AW_MPI_AI_ReleaseFrame(AUDIO_DEV AudioDevId, AI_CHN AiChn, AUDIO_FRAME_S *pstFrm, AEC_FRAME_S *pstAecFrm)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    AudioFrame audioFrm;
    audioFrm.pFrame = pstFrm;
    audioFrm.pAecFrame = pstAecFrm;
    audioFrm.nMilliSec = 0;
    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIChnReleaseFrame, &audioFrm);
}

ERRORTYPE AW_MPI_AI_SetChnParam(AUDIO_DEV AudioDevId, AI_CHN AiChn, AI_CHN_PARAM_S *pstChnParam)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }
    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIChnParameter, pstChnParam);
}

ERRORTYPE AW_MPI_AI_GetChnParam(AUDIO_DEV AudioDevId, AI_CHN AiChn, AI_CHN_PARAM_S *pstChnParam)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }
    return pChn->mpComp->GetConfig(pChn->mpComp, COMP_IndexVendorAIChnParameter, pstChnParam);
}

//ERRORTYPE AW_MPI_AI_SetVqeAttr(AUDIO_DEV AiDevId, AI_CHN AiChn, AUDIO_DEV AoDevId, AO_CHN AoChn, AI_VQE_CONFIG_S *pstVqeConfig)
ERRORTYPE AW_MPI_AI_SetVqeAttr(AUDIO_DEV AiDevId, AI_CHN AiChn, AI_VQE_CONFIG_S *pstVqeConfig)
{
    CHECK_AI_DEV_ID(AiDevId);
    CHECK_AI_CHN_ID(AiChn);
    //CHECK_AO_DEV_ID(AoDevId);
    //CHECK_AO_CHN_ID(AoChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AiDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AiDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeAttr, pstVqeConfig);
}

ERRORTYPE AW_MPI_AI_GetVqeAttr(AUDIO_DEV AiDevId, AI_CHN AiChn, AI_VQE_CONFIG_S *pstVqeConfig)
{
    CHECK_AI_DEV_ID(AiDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AiDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AiDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->GetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeAttr, pstVqeConfig);
}

ERRORTYPE AW_MPI_AI_EnableVqe(AUDIO_DEV AiDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AiDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AiDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AiDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeEnable, NULL);
}

ERRORTYPE AW_MPI_AI_DisableVqe(AUDIO_DEV AiDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AiDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AiDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AiDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOVqeDisable, NULL);
}

ERRORTYPE AW_MPI_AI_IgnoreData(AUDIO_DEV AudioDevId, AI_CHN AiChn, BOOL bIgnore)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }
    return COMP_SetConfig(pChn->mpComp, COMP_IndexVendorAIIgnoreData, &bIgnore);
}

ERRORTYPE AW_MPI_AI_EnableReSmp(AUDIO_DEV AudioDevId, AI_CHN AiChn, AUDIO_SAMPLE_RATE_E enOutSampleRate)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOReSmpEnable, &enOutSampleRate);
}

ERRORTYPE AW_MPI_AI_DisableReSmp(AUDIO_DEV AudioDevId, AI_CHN AiChn)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAIOReSmpDisable, NULL);
}

ERRORTYPE AW_MPI_AI_SaveFile(AUDIO_DEV AudioDevId, AI_CHN AiChn, AUDIO_SAVE_FILE_INFO_S *pstSaveFileInfo)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->SetConfig(pChn->mpComp, COMP_IndexVendorAISetSaveFileInfo, pstSaveFileInfo);
}

ERRORTYPE AW_MPI_AI_QueryFileStatus(AUDIO_DEV AudioDevId, AI_CHN AiChn, AUDIO_SAVE_FILE_INFO_S *pstSaveFileInfo)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    return pChn->mpComp->GetConfig(pChn->mpComp, COMP_IndexVendorAIQueryFileStatus, pstSaveFileInfo);
}

ERRORTYPE AW_MPI_AI_SetVqeVolume(AUDIO_DEV AudioDevId, AI_CHN AiChn, int s32VolumeDb)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    // todo

    return SUCCESS;
}

ERRORTYPE AW_MPI_AI_GetVqeVolume(AUDIO_DEV AudioDevId, AI_CHN AiChn, int *ps32VolumeDb)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }

    // todo

    return SUCCESS;
}

ERRORTYPE AW_MPI_AI_SetDevDrc(AUDIO_DEV AudioDevId, int enable)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SetAdcDrc(AudioDevId, enable);
}

ERRORTYPE AW_MPI_AI_SetDevHpf(AUDIO_DEV AudioDevId, int enable)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SetAdcHpf(AudioDevId, enable);
}

ERRORTYPE AW_MPI_AI_SetDevVolume(AUDIO_DEV AudioDevId, int s32VolumeDb)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SetVolume(AudioDevId, s32VolumeDb);
}

ERRORTYPE AW_MPI_AI_GetDevVolume(AUDIO_DEV AudioDevId, int *ps32VolumeDb)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_GetVolume(AudioDevId, ps32VolumeDb);
}

ERRORTYPE AW_MPI_AI_SetDevMute(AUDIO_DEV AudioDevId, int bEnableFlag)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SetMute(AudioDevId, bEnableFlag);
}

ERRORTYPE AW_MPI_AI_GetDevMute(AUDIO_DEV AudioDevId, int *pbEnableFlag)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_GetMute(AudioDevId, pbEnableFlag);
}

ERRORTYPE AW_MPI_AI_SetChnMute(AUDIO_DEV AudioDevId, AI_CHN AiChn, BOOL bMute)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }
    return COMP_SetConfig(pChn->mpComp, COMP_IndexVendorAIChnMute, &bMute);
}

ERRORTYPE AW_MPI_AI_GetChnMute(AUDIO_DEV AudioDevId, AI_CHN AiChn, BOOL* pbMute)
{
    CHECK_AI_DEV_ID(AudioDevId);
    CHECK_AI_CHN_ID(AiChn);
    AI_CHANNEL_S *pChn = NULL;
    ERRORTYPE ret;

    if (!audioHw_AI_IsDevStarted(AudioDevId)) {
        return ERR_AI_NOT_ENABLED;
    }

    if (SUCCESS != audioHw_AI_searchChannel(AudioDevId, AiChn, &pChn)) {
        return ERR_AI_UNEXIST;
    }
    return COMP_GetConfig(pChn->mpComp, COMP_IndexVendorAIChnMute, pbMute);
}

/*
 * dynamic suspend/resume ans
 * pCap->mAttr.ai_ans_en must be true, i.e. must enable ans first!
 */
ERRORTYPE AW_MPI_AI_SuspendAns(AUDIO_DEV AudioDevId)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SuspendAns(AudioDevId);
}

ERRORTYPE AW_MPI_AI_ResumeAns(AUDIO_DEV AudioDevId)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_ResumeAns(AudioDevId);
}

/*
 * dynamic suspend/resume aec
 * pCap->mAttr.ai_aec_en must be true, i.e. must enable aec first!
 */
ERRORTYPE AW_MPI_AI_SuspendAec(AUDIO_DEV AudioDevId)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_SuspendAec(AudioDevId);
}

ERRORTYPE AW_MPI_AI_ResumeAec(AUDIO_DEV AudioDevId)
{
    CHECK_AI_DEV_ID(AudioDevId);
    return audioHw_AI_ResumeAec(AudioDevId);
}

