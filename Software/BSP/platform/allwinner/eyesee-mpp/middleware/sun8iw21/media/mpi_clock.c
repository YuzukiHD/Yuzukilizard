/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_clock.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/31
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "mpi_clock"
#include <utils/plat_log.h>

//rely on platform headers
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//rely on media internal common headers.
#include "tsemaphore.h"

//media api headers to app, some are relied on .
#include "mm_common.h"
#include <mm_component.h>
#include "mpi_clock.h"

typedef struct CLOCK_CHN_MAP_S
{
    CLOCK_CHN           mChn;     // video decoder channel index, [0, CLOCK_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mComp;  // video encode component instance
    cdx_sem_t           mSemCompCmd;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}CLOCK_CHN_MAP_S;

typedef struct ClockChnManager
{
    struct list_head mList;   //element type: CLOCK_CHN_MAP_S
    pthread_mutex_t mLock;
} ClockChnManager;

static ClockChnManager *gpClockChnManager = NULL;

ERRORTYPE CLOCK_Construct(void)
{
    int ret;
    if (gpClockChnManager != NULL) 
    {
        return SUCCESS;
    }
    gpClockChnManager = (ClockChnManager*)malloc(sizeof(ClockChnManager));
    if (NULL == gpClockChnManager) 
    {
        aloge("alloc ClockChnManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    ret = pthread_mutex_init(&gpClockChnManager->mLock, NULL);
    if (ret != 0)
    {
        aloge("fatal error! mutex init fail");
        free(gpClockChnManager);
        gpClockChnManager = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpClockChnManager->mList);
    return SUCCESS;
}

ERRORTYPE CLOCK_Destruct(void)
{
    if (gpClockChnManager != NULL) 
    {
        if (!list_empty(&gpClockChnManager->mList)) 
        {
            aloge("fatal error! some clock channel still running when destroy clock device!");
        }
        pthread_mutex_destroy(&gpClockChnManager->mLock);
        free(gpClockChnManager);
        gpClockChnManager = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistChannel_l(CLOCK_CHN ClockChn, CLOCK_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    CLOCK_CHN_MAP_S* pEntry;

    if (gpClockChnManager == NULL) 
    {
        return FAILURE;
    }

    list_for_each_entry(pEntry, &gpClockChnManager->mList, mList)
    {
        if(pEntry->mChn == ClockChn)
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

static ERRORTYPE searchExistChannel(CLOCK_CHN ClockChn, CLOCK_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    CLOCK_CHN_MAP_S* pEntry;

    if (gpClockChnManager == NULL)
    {
        return FAILURE;
    }

    pthread_mutex_lock(&gpClockChnManager->mLock);
    ret = searchExistChannel_l(ClockChn, ppChn);
    pthread_mutex_unlock(&gpClockChnManager->mLock);
    return ret;
}

static ERRORTYPE addChannel_l(CLOCK_CHN_MAP_S *pChn)
{
    if (gpClockChnManager == NULL)
    {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpClockChnManager->mList);
    return SUCCESS;
}

static ERRORTYPE addChannel(CLOCK_CHN_MAP_S *pChn)
{
    if (gpClockChnManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpClockChnManager->mLock);
    ERRORTYPE ret = addChannel_l(pChn);
    pthread_mutex_unlock(&gpClockChnManager->mLock);
    return SUCCESS;
}

static ERRORTYPE removeChannel(CLOCK_CHN_MAP_S *pChn)
{
    if (gpClockChnManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpClockChnManager->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpClockChnManager->mLock);
    return SUCCESS;
}

MM_COMPONENTTYPE* CLOCK_GetChnComp(MPP_CHN_S *pMppChn)
{
    CLOCK_CHN_MAP_S* pChn;

    if (searchExistChannel(pMppChn->mChnId, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mComp;
}

static CLOCK_CHN_MAP_S* CLOCK_CHN_MAP_S_Construct()
{
    CLOCK_CHN_MAP_S *pChannel = (CLOCK_CHN_MAP_S*)malloc(sizeof(CLOCK_CHN_MAP_S));
    if(NULL == pChannel)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(CLOCK_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}

static void CLOCK_CHN_MAP_S_Destruct(CLOCK_CHN_MAP_S *pChannel)
{
    if(pChannel->mComp)
    {
        aloge("fatal error! Clock component need free before!");
        COMP_FreeHandle(pChannel->mComp);
        pChannel->mComp = NULL;
    }
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    free(pChannel);
}

static ERRORTYPE ClockEventHandler(
	 PARAM_IN COMP_HANDLETYPE hComponent,
	 PARAM_IN void* pAppData,
	 PARAM_IN COMP_EVENTTYPE eEvent,
	 PARAM_IN unsigned int nData1,
	 PARAM_IN unsigned int nData2,
	 PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S ClockChnInfo;
    ret = COMP_GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &ClockChnInfo);
    if(ret == SUCCESS)
    {
        alogv("clock event, MppChannel[%d][%d][%d]", ClockChnInfo.mModId, ClockChnInfo.mDevId, ClockChnInfo.mChnId);
    }
	CLOCK_CHN_MAP_S *pChn = (CLOCK_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("clock EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(COMP_CommandVendorChangeANativeWindow == nData1)
            {
                alogd("change video layer?");
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
            if(ERR_CLOCK_SAMESTATE == nData1)
            {
                alogv("set same state to clock!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_CLOCK_INVALIDSTATE == nData1)
            {
                aloge("why clock state turn to invalid!");
                break;
            }
            else if(ERR_CLOCK_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! clock state transition incorrect.");
                break;
            }
            else
            {
                aloge("fatal error! unknown error[0x%x]!", nData1);
                break;
            }
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
	return SUCCESS;
}

COMP_CALLBACKTYPE ClockCallback = {
	.EventHandler = ClockEventHandler,
	.EmptyBufferDone = NULL,
	.FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_CLOCK_CreateChn(CLOCK_CHN ClockChn, CLOCK_CHN_ATTR_S *pAttr)
{
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal VencAttr!");
        return ERR_CLOCK_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&gpClockChnManager->mLock);
    if(SUCCESS == searchExistChannel_l(ClockChn, NULL))
    {
        pthread_mutex_unlock(&gpClockChnManager->mLock);
        return ERR_CLOCK_EXIST;
    }
    CLOCK_CHN_MAP_S *pNode = CLOCK_CHN_MAP_S_Construct();
    pNode->mChn = ClockChn;
    
    //create Clock Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mComp, CDX_ComponentNameClock, (void*)pNode, &ClockCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_CLOCK;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mChn;
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    COMP_TIME_CONFIG_CLOCKSTATETYPE clockState;
    clockState.eState = COMP_TIME_ClockStateWaitingForStartTime;
    clockState.nWaitMask = pAttr->nWaitMask;
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexConfigTimeClockState, (void*)&clockState);
    eRet = pNode->mComp->SendCommand(pNode->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create clock Component done!

    addChannel_l(pNode);
    pthread_mutex_unlock(&gpClockChnManager->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_CLOCK_DestroyChn(CLOCK_CHN ClockChn)
{
    ERRORTYPE ret;
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    CLOCK_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ClockChn, &pChn))
    {
        return ERR_CLOCK_UNEXIST;
    }
    ERRORTYPE eRet;
    if(pChn->mComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pChn->mComp->GetState(pChn->mComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            {
                eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
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
                aloge("fatal error! invalid ClockChn[%d] state[0x%x]!", ClockChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                removeChannel(pChn);
                COMP_FreeHandle(pChn->mComp);
                pChn->mComp = NULL;
                CLOCK_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_CLOCK_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_CLOCK_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no Clock component!");
        list_del(&pChn->mList);
        CLOCK_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_CLOCK_Start(CLOCK_CHN ClockChn)
{
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    CLOCK_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ClockChn, &pChn))
    {
        return ERR_CLOCK_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else
    {
        alogd("ClockChannel[%d] State[0x%x], do nothing!", ClockChn, nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_CLOCK_Stop(CLOCK_CHN ClockChn)
{
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    CLOCK_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ClockChn, &pChn))
    {
        return ERR_CLOCK_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateIdle fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogv("ClockChannel[%d] State[0x%x], do nothing!", ClockChn, nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check ClockChannelState[0x%x]!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_CLOCK_Pause(CLOCK_CHN ClockChn)
{
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    CLOCK_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ClockChn, &pChn))
    {
        return ERR_CLOCK_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateExecuting == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StatePause, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command statePause fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StatePause == nCompState)
    {
        alogd("ClockChannel[%d] already statePause.", ClockChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("ClockChannel[%d] stateIdle, can't turn to statePause!", ClockChn);
        ret = ERR_CLOCK_INCORRECT_STATE_TRANSITION;
    }
    else
    {
        aloge("fatal error! check ClockChannel[%d]State[0x%x]!", ClockChn, nCompState);
        ret = ERR_CLOCK_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_CLOCK_Seek(CLOCK_CHN ClockChn)
{
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    CLOCK_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ClockChn, &pChn))
    {
        return ERR_CLOCK_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorSeekToPosition, NULL);
    }
    else
    {
        aloge("fatal error! check ClockChannel[%d]State[0x%x]!", ClockChn, nCompState);
        ret = ERR_CLOCK_INCORRECT_STATE_OPERATION;
    }
    return ret; 
}

/** 
 * Register callback, mpp_clock use callback to notify caller.
 *
 * @return SUCCESS.
 * @param ClockChn clock channel number.
 * @param pCallback callback struct.
 */
ERRORTYPE AW_MPI_CLOCK_RegisterCallback(CLOCK_CHN ClockChn, MPPCallbackInfo *pCallback)
{
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    CLOCK_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ClockChn, &pChn))
    {
        return ERR_CLOCK_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_CLOCK_GetCurrentMediaTime(CLOCK_CHN ClockChn, int* pMediaTime)
{
    if(!(ClockChn>=0 && ClockChn <CLOCK_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid ClockChn[%d]!", ClockChn);
        return ERR_CLOCK_INVALID_CHNID;
    }
    CLOCK_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(ClockChn, &pChn))
    {
        return ERR_CLOCK_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        COMP_TIME_CONFIG_TIMESTAMPTYPE timeStamp;
        ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexConfigTimeCurrentMediaTime, &timeStamp);
        if(-1 == timeStamp.nTimestamp)
        {
            *pMediaTime = -1;
        }
        else
        {
            *pMediaTime = timeStamp.nTimestamp/1000;
        }
    }
    else
    {
        aloge("fatal error! call in wrong ClockChannel[%d]State[0x%x]!", ClockChn, nCompState);
        *pMediaTime = -1;
        ret = SUCCESS;
    }
    return ret; 
}

