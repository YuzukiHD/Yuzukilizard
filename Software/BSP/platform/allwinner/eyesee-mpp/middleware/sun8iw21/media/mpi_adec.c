/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_adec.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/12
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/
#define LOG_TAG "mpi_adec"
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
#include "mm_comm_adec.h"
#include "mpi_adec.h"

//rely on media internal common headers.
#include <tsemaphore.h>

//media internal common headers.
#include <media_common.h>
#include <mm_component.h>
#include <AdecStream.h>

typedef struct ADEC_CHN_MAP_S
{
    ADEC_CHN            mADecChn;     // audio decoder channel index, [0, ADEC_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mADecComp;  // audio decoder component instance
    cdx_sem_t           mSemCompCmd;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}ADEC_CHN_MAP_S;

typedef struct ADecChnManager
{
    struct list_head mList;   //element type: ADEC_CHN_MAP_S
    pthread_mutex_t mLock;
} ADecChnManager;

static ADecChnManager *gpADecChnMap = NULL;

ERRORTYPE ADEC_Construct(void)
{
    int ret;
    if (gpADecChnMap != NULL) {
        return SUCCESS;
    }
    gpADecChnMap = (ADecChnManager*)malloc(sizeof(ADecChnManager));
    if (NULL == gpADecChnMap) {
        aloge("alloc ADecChnManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    ret = pthread_mutex_init(&gpADecChnMap->mLock, NULL);
    if (ret != 0) {
        aloge("fatal error! mutex init fail");
        free(gpADecChnMap);
        gpADecChnMap = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpADecChnMap->mList);
    return SUCCESS;
}

ERRORTYPE ADEC_Destruct(void)
{
    if (gpADecChnMap != NULL) {
        if (!list_empty(&gpADecChnMap->mList)) {
            aloge("fatal error! some adec channel still running when destroy adec device!");
        }
        pthread_mutex_destroy(&gpADecChnMap->mLock);
        free(gpADecChnMap);
        gpADecChnMap = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistChannel_l(ADEC_CHN ADecChn, ADEC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    ADEC_CHN_MAP_S* pEntry;

    if (gpADecChnMap == NULL) {
        return FAILURE;
    }
    list_for_each_entry(pEntry, &gpADecChnMap->mList, mList)
    {
        if(pEntry->mADecChn == ADecChn)
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

static ERRORTYPE searchExistChannel(ADEC_CHN ADecChn, ADEC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    ADEC_CHN_MAP_S* pEntry;

    if (gpADecChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpADecChnMap->mLock);
    ret = searchExistChannel_l(ADecChn, ppChn);
    pthread_mutex_unlock(&gpADecChnMap->mLock);
    return ret;
}

static ERRORTYPE addChannel_l(ADEC_CHN_MAP_S *pChn)
{
    if (gpADecChnMap == NULL) {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpADecChnMap->mList);
    return SUCCESS;
}

static ERRORTYPE addChannel(ADEC_CHN_MAP_S *pChn)
{
    if (gpADecChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpADecChnMap->mLock);
    ERRORTYPE ret = addChannel_l(pChn);
    pthread_mutex_unlock(&gpADecChnMap->mLock);
    return ret;
}

static ERRORTYPE removeChannel(ADEC_CHN_MAP_S *pChn)
{
    if (gpADecChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpADecChnMap->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpADecChnMap->mLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *ADEC_GetChnComp(MPP_CHN_S *pMppChn)
{
    ADEC_CHN_MAP_S* pChn;
    if (searchExistChannel(pMppChn->mChnId, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mADecComp;
}

static ADEC_CHN_MAP_S* ADEC_CHN_MAP_S_Construct()
{
    ADEC_CHN_MAP_S *pChannel = (ADEC_CHN_MAP_S*)malloc(sizeof(ADEC_CHN_MAP_S));
    if(NULL == pChannel)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(ADEC_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}

static void ADEC_CHN_MAP_S_Destruct(ADEC_CHN_MAP_S *pChannel)
{
    if(pChannel->mADecComp)
    {
        aloge("fatal error! ADec component need free before!");
        COMP_FreeHandle(pChannel->mADecComp);
        pChannel->mADecComp = NULL;
    }
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    free(pChannel);
}

static ERRORTYPE AudioDecEventHandler(
     PARAM_IN COMP_HANDLETYPE hComponent,
     PARAM_IN void* pAppData,
     PARAM_IN COMP_EVENTTYPE eEvent,
     PARAM_IN unsigned int nData1,
     PARAM_IN unsigned int nData2,
     PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S ADecChnInfo;
    ret = ((MM_COMPONENTTYPE*)hComponent)->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &ADecChnInfo);
    if(ret == SUCCESS)
    {
        alogv("audio decoder event, MppChannel[%d][%d][%d]", ADecChnInfo.mModId, ADecChnInfo.mDevId, ADecChnInfo.mChnId);
    }
    ADEC_CHN_MAP_S *pChn = (ADEC_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("audio decoder EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogw("Low probability! what command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventBufferFlag:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_ADEC;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pChn->mADecChn;
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_NOTIFY_EOF, NULL);
            break;
        }
        case COMP_EventError:
        {
            if(ERR_ADEC_SAMESTATE == nData1)
            {
                alogv("set same state to adec!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_ADEC_INVALIDSTATE == nData1)
            {
                aloge("why adec state turn to invalid?");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_ADEC_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! adec state transition incorrect.");
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

COMP_CALLBACKTYPE AudioDecCallback = {
    .EventHandler = AudioDecEventHandler,
    .EmptyBufferDone = NULL,
    .FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_ADEC_CreateChn(ADEC_CHN ADecChn, const ADEC_CHN_ATTR_S *pAttr)
{
    if(!(ADecChn>=0 && ADecChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ADecChn[%d]!", ADecChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal ADecAttr!");
        return ERR_ADEC_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&gpADecChnMap->mLock);
    if(SUCCESS == searchExistChannel_l(ADecChn, NULL))
    {
        pthread_mutex_unlock(&gpADecChnMap->mLock);
        return ERR_ADEC_EXIST;
    }
    ADEC_CHN_MAP_S *pNode = ADEC_CHN_MAP_S_Construct();
    pNode->mADecChn = ADecChn;

    //create ADec Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mADecComp, CDX_ComponentNameAudioDecoder, (void*)pNode, &AudioDecCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_ADEC;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mADecChn;
    eRet = pNode->mADecComp->SetConfig(pNode->mADecComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = pNode->mADecComp->SetConfig(pNode->mADecComp, COMP_IndexVendorAdecChnAttr, (void*)pAttr);
    eRet = pNode->mADecComp->SendCommand(pNode->mADecComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create ADec Component done!

    addChannel_l(pNode);
    pthread_mutex_unlock(&gpADecChnMap->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_ADEC_DestroyChn(ADEC_CHN ADecChn)
{
    ERRORTYPE ret;
    if (!(ADecChn>=0 && ADecChn<ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ADecChn[%d]!", ADecChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if (SUCCESS != searchExistChannel(ADecChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    ERRORTYPE eRet;
    if (pChn->mADecComp)
    {
        COMP_STATETYPE nCompState;
        if (SUCCESS == pChn->mADecComp->GetState(pChn->mADecComp, &nCompState))
        {
            if (nCompState == COMP_StateIdle)
            {
                eRet = pChn->mADecComp->SendCommand(pChn->mADecComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                cdx_sem_down(&pChn->mSemCompCmd);
                eRet = SUCCESS;
            }
            else if (nCompState == COMP_StateLoaded)
            {
                eRet = SUCCESS;
            }
            else if (nCompState == COMP_StateInvalid)
            {
                alogw("Low probability! Component StateInvalid?");
                eRet = SUCCESS;
            }
            else
            {
                aloge("Fatal error! invalid ADecChn[%d] state[0x%x]!", ADecChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                removeChannel(pChn);
                COMP_FreeHandle(pChn->mADecComp);
                pChn->mADecComp = NULL;
                ADEC_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_ADEC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_ADEC_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no ADec component!");
        list_del(&pChn->mList);
        ADEC_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

/**
 * Reset adec channel, free all resources. must call it in stateIdle. Used it in non-tunnel mode.
 *
 * @return SUCCESS, ERR_ADEC_NOT_PERM
 * @param AdChn adec channel number.
 */
ERRORTYPE AW_MPI_ADEC_ResetChn(ADEC_CHN ADecChn)
{
    if(!(ADecChn>=0 && ADecChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ADecChn[%d]!", ADecChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ADecChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    COMP_STATETYPE nCompState = COMP_StateInvalid;
    ERRORTYPE eRet, eRet2;
    eRet = pChn->mADecComp->GetState(pChn->mADecComp, &nCompState);
    if(eRet == SUCCESS && COMP_StateIdle == nCompState)
    {
        eRet2 = pChn->mADecComp->SetConfig(pChn->mADecComp, COMP_IndexVendorAdecResetChannel, NULL);
        if(eRet2 != SUCCESS)
        {
            aloge("fatal error! reset channel fail[0x%x]!", eRet2);
        }
        return eRet2;
    }
    else
    {
        aloge("wrong status[0x%x], can't reset adec channel!", nCompState);
        return ERR_ADEC_NOT_PERM;
    }
}

ERRORTYPE AW_MPI_ADEC_RegisterCallback(ADEC_CHN ADecChn, MPPCallbackInfo *pCallback)
{
    if(!(ADecChn>=0 && ADecChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ADecChn[%d]!", ADecChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ADecChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_ADEC_SetStreamEof(ADEC_CHN AdChn, BOOL bEofFlag)
{
    if(!(AdChn>=0 && AdChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdecChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mADecComp->GetState(pChn->mADecComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
       aloge("wrong state[0x%x], return!", nState);
       return ERR_ADEC_NOT_PERM;
    }
    if(bEofFlag)
    {
        ret = pChn->mADecComp->SetConfig(pChn->mADecComp, COMP_IndexVendorSetStreamEof, NULL);
    }
    else
    {
        ret = pChn->mADecComp->SetConfig(pChn->mADecComp, COMP_IndexVendorClearStreamEof, NULL);
    }
    return ret;
}

ERRORTYPE AW_MPI_ADEC_SetChnAttr(ADEC_CHN ADecChn, const ADEC_CHN_ATTR_S *pAttr)
{
    if(!(ADecChn>=0 && ADecChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ADecChn[%d]!", ADecChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ADecChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mADecComp->GetState(pChn->mADecComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ADEC_NOT_PERM;
    }
    ret = pChn->mADecComp->SetConfig(pChn->mADecComp, COMP_IndexVendorAdecChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_ADEC_GetChnAttr(ADEC_CHN ADecChn, ADEC_CHN_ATTR_S *pAttr)
{
    if(!(ADecChn>=0 && ADecChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ADecChn[%d]!", ADecChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ADecChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mADecComp->GetState(pChn->mADecComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ADEC_NOT_PERM;
    }
    ret = pChn->mADecComp->GetConfig(pChn->mADecComp, COMP_IndexVendorAdecChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_ADEC_StartRecvStream(ADEC_CHN AdChn)
{
    if(!(AdChn>=0 && AdChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mADecComp->GetState(pChn->mADecComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mADecComp->SendCommand(pChn->mADecComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else
    {
        alogd("AdecChannel[%d] State[0x%x], do nothing!", AdChn, nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_ADEC_Pause(ADEC_CHN AdChn)
{
    if(!(AdChn>=0 && AdChn<ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mADecComp->GetState(pChn->mADecComp, &nCompState);
    if(COMP_StateExecuting == nCompState)
    {
        eRet = pChn->mADecComp->SendCommand(pChn->mADecComp, COMP_CommandStateSet, COMP_StatePause, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command statePause fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StatePause == nCompState)
    {
        alogd("AdecChannel[%d] already statePause.", AdChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("AdecChannel[%d] stateIdle, can't turn to statePause!", AdChn);
        ret = ERR_ADEC_INCORRECT_STATE_TRANSITION;
    }
    else
    {
        aloge("fatal error! check AdecChannel[%d]State[0x%x]!", AdChn, nCompState);
        ret = ERR_ADEC_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_ADEC_Seek(ADEC_CHN AdChn)
{
    if(!(AdChn>=0 && AdChn<ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    int ret = SUCCESS;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mADecComp->GetState(pChn->mADecComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        ret = pChn->mADecComp->SetConfig(pChn->mADecComp, COMP_IndexVendorSeekToPosition, NULL);
    }
    else
    {
        aloge("fatal error! can't seek int AdecChannel[%d] State[0x%x]!", AdChn, nCompState);
        ret = ERR_ADEC_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_ADEC_StopRecvStream(ADEC_CHN AdChn)
{
    if(!(AdChn>=0 && AdChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mADecComp->GetState(pChn->mADecComp, &nCompState);
    if (COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mADecComp->SendCommand(pChn->mADecComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateIdle fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if (COMP_StateIdle == nCompState)
    {
        alogv("AdecChannel[%d] State[0x%x], do nothing!", AdChn, nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check AdecChannelState[0x%x]!", nCompState);
        ret = ERR_ADEC_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

//int AW_MPI_ADEC_GetFd(ADEC_CHN ADecChn)
//{
//    if(!(ADecChn>=0 && ADecChn <ADEC_MAX_CHN_NUM))
//    {
//        aloge("fatal error! invalid ADecChn[%d]!", ADecChn);
//        return ERR_ADEC_INVALID_CHNID;
//    }
//    ADEC_CHN_MAP_S *pChn;
//    if(SUCCESS != searchExistChannel(ADecChn, &pChn))
//    {
//        return ERR_ADEC_UNEXIST;
//    }
//    ERRORTYPE ret;
//    int readFd = -1;
//    ret = pChn->mADecComp->GetConfig(pChn->mADecComp, COMP_IndexVendorMPPChannelFd, (void*)&readFd);
//    if(ret == SUCCESS)
//    {
//        return readFd;
//    }
//    else
//    {
//        return -1;
//    }
//}

ERRORTYPE AW_MPI_ADEC_SendStream(ADEC_CHN AdChn, const AUDIO_STREAM_S *pStream, int nMilliSec)
{
    if(!(AdChn>=0 && AdChn<ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mADecComp->GetState(pChn->mADecComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ADEC_NOT_PERM;
    }
    AdecInputStream inputStream;
    inputStream.pStream = (AUDIO_STREAM_S *)pStream;
    inputStream.nMilliSec = nMilliSec;
    COMP_BUFFERHEADERTYPE bufferHeader;
    bufferHeader.pAppPrivate = (void*)&inputStream;
    ret = pChn->mADecComp->EmptyThisBuffer(pChn->mADecComp, &bufferHeader);
    return ret;
}

ERRORTYPE AW_MPI_ADEC_GetFrame(ADEC_CHN AdChn, AUDIO_FRAME_S *pFrame, int nMilliSec)
{
    if(!(AdChn>=0 && AdChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    if(NULL == pFrame)
    {
        aloge("fatal error! pFrame == NULL!");
        return ERR_ADEC_NULL_PTR;
    }
    if(nMilliSec < -1)
    {
        aloge("fatal error! illegal nMilliSec[%d]!", nMilliSec);
        return ERR_ADEC_ILLEGAL_PARAM;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mADecComp->GetState(pChn->mADecComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ADEC_NOT_PERM;
    }
    AdecOutputFrame adecFrame;
    adecFrame.pFrame = pFrame;
    adecFrame.nMilliSec = nMilliSec;
    ret = pChn->mADecComp->GetConfig(pChn->mADecComp, COMP_IndexVendorAdecGetFrame, (void*)&adecFrame);
    return ret;
}

ERRORTYPE AW_MPI_ADEC_ReleaseFrame(ADEC_CHN AdChn, AUDIO_FRAME_S *pFrame)
{
    if(!(AdChn>=0 && AdChn <ADEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid AdChn[%d]!", AdChn);
        return ERR_ADEC_INVALID_CHNID;
    }
    ADEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(AdChn, &pChn))
    {
        return ERR_ADEC_UNEXIST;
    }
    if(NULL == pFrame)
    {
        aloge("fatal error! pFrame == NULL!");
        return ERR_ADEC_NULL_PTR;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mADecComp->GetState(pChn->mADecComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ADEC_NOT_PERM;
    }
    AdecOutputFrame adecFrame;
    adecFrame.pFrame = pFrame;
    adecFrame.nMilliSec = 0;
    ret = pChn->mADecComp->SetConfig(pChn->mADecComp, COMP_IndexVendorAdecReleaseFrame, (void*)&adecFrame);
    return ret;
}
