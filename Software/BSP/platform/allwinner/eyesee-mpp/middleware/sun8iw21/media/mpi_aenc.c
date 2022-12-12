/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_aenc.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/12
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/
#define LOG_TAG "mpi_aenc"
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
//#include "mm_comm_audio.h"
#include "mm_comm_aenc.h"
#include "mpi_aenc.h"

//media internal common headers.
#include <media_common.h>
#include <tsemaphore.h>
#include <mm_component.h>
#include <mpi_aenc_common.h>
#include <AencCompStream.h>
#include <aencoder.h>

typedef struct AENC_CHN_MAP_S
{
    AENC_CHN            mAeChn;     // audio encoder channel index, [0, AENC_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mEncComp;  // audio encoder component instance
    cdx_sem_t           mSemCompCmd;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}AENC_CHN_MAP_S;

typedef struct AencChnManager
{
    struct list_head mList;   //element type: AENC_CHN_MAP_S
    pthread_mutex_t mLock;
} AencChnManager;

static AencChnManager *gpAencChnMap = NULL;

ERRORTYPE AENC_Construct(void)
{
    int ret;
    if (gpAencChnMap != NULL) {
        return SUCCESS;
    }
    gpAencChnMap = (AencChnManager*)malloc(sizeof(AencChnManager));
    if (NULL == gpAencChnMap) {
        aloge("alloc AencChnManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    ret = pthread_mutex_init(&gpAencChnMap->mLock, NULL);
    if (ret != 0) {
        aloge("fatal error! mutex init fail");
        free(gpAencChnMap);
        gpAencChnMap = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpAencChnMap->mList);
    return SUCCESS;
}

ERRORTYPE AENC_Destruct(void)
{
    if (gpAencChnMap != NULL) {
        if (!list_empty(&gpAencChnMap->mList)) {
            aloge("fatal error! some aenc channel still running when destroy aenc device!");
        }
        pthread_mutex_destroy(&gpAencChnMap->mLock);
        free(gpAencChnMap);
        gpAencChnMap = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistChannel_l(AENC_CHN AeChn, AENC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    AENC_CHN_MAP_S* pEntry;

    if (gpAencChnMap == NULL) {
        return FAILURE;
    }

    list_for_each_entry(pEntry, &gpAencChnMap->mList, mList)
    {
        if(pEntry->mAeChn == AeChn)
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

static ERRORTYPE searchExistChannel(AENC_CHN AeChn, AENC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    AENC_CHN_MAP_S* pEntry;

    if (gpAencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpAencChnMap->mLock);
    ret = searchExistChannel_l(AeChn, ppChn);
    pthread_mutex_unlock(&gpAencChnMap->mLock);
    return ret;
}

static ERRORTYPE addChannel_l(AENC_CHN_MAP_S *pChn)
{
    if (gpAencChnMap == NULL) {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpAencChnMap->mList);
    return SUCCESS;
}

static ERRORTYPE addChannel(AENC_CHN_MAP_S *pChn)
{
    if (gpAencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpAencChnMap->mLock);
    ERRORTYPE ret = addChannel_l(pChn);
    pthread_mutex_unlock(&gpAencChnMap->mLock);
    return ret;
}

static ERRORTYPE removeChannel(AENC_CHN_MAP_S *pChn)
{
    if (gpAencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpAencChnMap->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpAencChnMap->mLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *AENC_GetChnComp(PARAM_IN MPP_CHN_S *pMppChn)
{
    AENC_CHN_MAP_S* pChn;

    if (searchExistChannel(pMppChn->mChnId, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mEncComp;
}

static AENC_CHN_MAP_S* AENC_CHN_MAP_S_Construct()
{
    AENC_CHN_MAP_S *pChannel = (AENC_CHN_MAP_S*)malloc(sizeof(AENC_CHN_MAP_S));
    if(NULL == pChannel)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(AENC_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}

static void AENC_CHN_MAP_S_Destruct(AENC_CHN_MAP_S *pChannel)
{
    if(pChannel->mEncComp)
    {
        aloge("fatal error! Aenc component need free before!");
        COMP_FreeHandle(pChannel->mEncComp);
        pChannel->mEncComp = NULL;
    }
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    free(pChannel);
}

static ERRORTYPE AudioEncEventHandler(
     PARAM_IN COMP_HANDLETYPE hComponent,
     PARAM_IN void* pAppData,
     PARAM_IN COMP_EVENTTYPE eEvent,
     PARAM_IN unsigned int nData1,
     PARAM_IN unsigned int nData2,
     PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S AencChnInfo;
    ret = ((MM_COMPONENTTYPE*)hComponent)->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &AencChnInfo);
    if(ret == SUCCESS)
    {
        alogv("audio encoder event, MppChannel[%d][%d][%d]", AencChnInfo.mModId, AencChnInfo.mDevId, AencChnInfo.mChnId);
    }
    AENC_CHN_MAP_S *pChn = (AENC_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("audio encoder EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogw("Low probability! what command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventError:
        {
            if(ERR_AENC_SAMESTATE == nData1)
            {
                alogv("set same state to aenc!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_AENC_INVALIDSTATE == nData1)
            {
                aloge("why aenc state turn to invalid?");
                break;
            }
            else if(ERR_AENC_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! aenc state transition incorrect.");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogw("unknown event error, [0x%x]", nData1);
                break;
            }
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
    return SUCCESS;
}

COMP_CALLBACKTYPE AudioEncCallback = {
    .EventHandler = AudioEncEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_AENC_CreateChn(AENC_CHN AeChn, const AENC_CHN_ATTR_S *pAttr)
{
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal AEncAttr!");
        return ERR_AENC_ILLEGAL_PARAM;
    }
    if (gpAencChnMap == NULL)
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpAencChnMap->mLock);
    if(SUCCESS == searchExistChannel_l(AeChn, NULL))
    {
        pthread_mutex_unlock(&gpAencChnMap->mLock);
        return ERR_AENC_EXIST;
    }
    AENC_CHN_MAP_S *pNode = AENC_CHN_MAP_S_Construct();
    pNode->mAeChn = AeChn;

    //create Aenc Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mEncComp, CDX_ComponentNameAudioEncoder, (void*)pNode, &AudioEncCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_AENC;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mAeChn;
    eRet = pNode->mEncComp->SetConfig(pNode->mEncComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = pNode->mEncComp->SetConfig(pNode->mEncComp, COMP_IndexVendorAencChnAttr, (void*)pAttr);
    eRet = pNode->mEncComp->SendCommand(pNode->mEncComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create Aenc Component done!

    addChannel_l(pNode);
    pthread_mutex_unlock(&gpAencChnMap->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_AENC_DestroyChn(AENC_CHN AeChn)
{
    ERRORTYPE ret;
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    ERRORTYPE eRet;
    if(pChn->mEncComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pChn->mEncComp->GetState(pChn->mEncComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            {
                eRet = pChn->mEncComp->SendCommand(pChn->mEncComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
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
                aloge("Fatal error! invalid AeChn[%d] state[0x%x]!", AeChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                removeChannel(pChn);
                COMP_FreeHandle(pChn->mEncComp);
                pChn->mEncComp = NULL;
                AENC_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_AENC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_AENC_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no Aenc component!");
        list_del(&pChn->mList);
        AENC_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_AENC_ResetChn(AENC_CHN AeChn)
{
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    COMP_STATETYPE nCompState = COMP_StateInvalid;
    ERRORTYPE eRet, eRet2;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(eRet == SUCCESS && COMP_StateIdle == nCompState)
    {
        eRet2 = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorAencResetChannel, NULL);
        if(eRet2 != SUCCESS)
        {
            aloge("fatal error! reset channel fail[0x%x]!", eRet2);
        }
        return eRet2;
    }
    else
    {
        aloge("wrong status[0x%x], can't reset aenc channel!", nCompState);
        return ERR_AENC_NOT_PERM;
    }
}

ERRORTYPE AW_MPI_AENC_StartRecvPcm(AENC_CHN AeChn)
{
    if(!(AeChn>=0 && AeChn<AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(COMP_StateIdle == nCompState)
    {
        eRet = pChn->mEncComp->SendCommand(pChn->mEncComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else
    {
        alogd("AencChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_AENC_StopRecvPcm(AENC_CHN AeChn)
{
    if(!(AeChn>=0 && AeChn < AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mEncComp->SendCommand(pChn->mEncComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogv("AencChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check AencChannelState[0x%x]!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_AENC_Query(AENC_CHN AeChn, AENC_CHN_STAT_S *pStat)
{
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_AENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorAencChnState, (void*)pStat);
    return ret;
}

ERRORTYPE AW_MPI_AENC_RegisterCallback(AENC_CHN AeChn, MPPCallbackInfo *pCallback)
{
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_AENC_SetChnAttr(AENC_CHN AeChn, const AENC_CHN_ATTR_S *pAttr)
{
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_AENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorAencChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_AENC_GetChnAttr(AENC_CHN AeChn, AENC_CHN_ATTR_S *pAttr)
{
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_AENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorAencChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_AENC_SendFrame(AENC_CHN AeChn, AUDIO_FRAME_S *pFrame, AEC_FRAME_S* pAecFrm)
{
    if(!(AeChn>=0 && AeChn<AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_AENC_NOT_PERM;
    }
    COMP_BUFFERHEADERTYPE bufferHeader;
    bufferHeader.pOutputPortPrivate = pFrame;
    ret = pChn->mEncComp->EmptyThisBuffer(pChn->mEncComp, &bufferHeader);
    return ret;
}

ERRORTYPE AW_MPI_AENC_GetStream(AENC_CHN AeChn, AUDIO_STREAM_S *pStream, int nMilliSec)
{
    if(!(AeChn>=0 && AeChn<AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    if(NULL == pStream)
    {
        aloge("fatal error! pStream == NULL!");
        return ERR_AENC_NULL_PTR;
    }
    if(nMilliSec < -1)
    {
        aloge("fatal error! illegal nMilliSec[%d]!", nMilliSec);
        return ERR_AENC_ILLEGAL_PARAM;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_AENC_NOT_PERM;
    }
    AEncStream stAencStream;
    stAencStream.pStream = pStream;
    stAencStream.nMilliSec = nMilliSec;
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorAencGetStream, (void*)&stAencStream);
    return ret;
}

ERRORTYPE AW_MPI_AENC_ReleaseStream(AENC_CHN AeChn, AUDIO_STREAM_S *pStream)
{
    if(!(AeChn>=0 && AeChn<AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    if(NULL == pStream)
    {
        aloge("fatal error! pStream == NULL!");
        return ERR_AENC_NULL_PTR;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_AENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorAencReleaseStream, (void*)pStream);
    return ret;
}

int AW_MPI_AENC_GetHandle(AENC_CHN AeChn)
{
    if(!(AeChn>=0 && AeChn <AENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AeChn[%d]!", AeChn);
        return ERR_AENC_INVALID_CHNID;
    }
    AENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AeChn, &pChn))
    {
        return ERR_AENC_UNEXIST;
    }
    ERRORTYPE ret;
    int readFd = -1;
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorMPPChannelFd, (void*)&readFd);
    if(ret == SUCCESS)
    {
        return readFd;
    }
    else
    {
        return -1;
    }
}
