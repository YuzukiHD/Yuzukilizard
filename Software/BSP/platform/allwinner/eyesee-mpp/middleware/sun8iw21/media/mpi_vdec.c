/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_vdec.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/04
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "mpi_vdec"
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
#include "mm_comm_video.h"
#include "mm_comm_vdec.h"
#include <VdecStream.h>
#include <mm_component.h>
#include "mpi_vdec.h"

typedef struct VDEC_CHN_MAP_S
{
    VDEC_CHN            mVdChn;     // video decoder channel index, [0, VDEC_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mComp;  // video encode component instance
    cdx_sem_t           mSemCompCmd;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}VDEC_CHN_MAP_S;

typedef struct VdecChnManager
{
    struct list_head mList;   //element type: VDEC_CHN_MAP_S
    pthread_mutex_t mLock;
    int mVeFreq;    // 0: don't care frequency, any value will be OK. Maybe use default value. unit:MHz.
} VdecChnManager;

static VdecChnManager *gpVdecChnManager = NULL;

ERRORTYPE VDEC_Construct(void)
{
    int ret;
    if (gpVdecChnManager != NULL) 
    {
        return SUCCESS;
    }
    gpVdecChnManager = (VdecChnManager*)malloc(sizeof(VdecChnManager));
    if (NULL == gpVdecChnManager) 
    {
        aloge("alloc VdecChnManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    memset(gpVdecChnManager, 0, sizeof(VdecChnManager));
    ret = pthread_mutex_init(&gpVdecChnManager->mLock, NULL);
    if (ret != 0) 
    {
        aloge("fatal error! mutex init fail");
        free(gpVdecChnManager);
        gpVdecChnManager = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpVdecChnManager->mList);
    return SUCCESS;
}

ERRORTYPE VDEC_Destruct(void)
{
    if (gpVdecChnManager != NULL) 
    {
        if (!list_empty(&gpVdecChnManager->mList)) 
        {
            aloge("fatal error! some vdec channel still running when destroy vdec device!");
        }
        pthread_mutex_destroy(&gpVdecChnManager->mLock);
        free(gpVdecChnManager);
        gpVdecChnManager = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistChannel_l(VDEC_CHN VdChn, VDEC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    VDEC_CHN_MAP_S* pEntry;

    if (gpVdecChnManager == NULL)
    {
        return FAILURE;
    }

    list_for_each_entry(pEntry, &gpVdecChnManager->mList, mList)
    {
        if(pEntry->mVdChn == VdChn)
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

static ERRORTYPE searchExistChannel(VDEC_CHN VdChn, VDEC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    VDEC_CHN_MAP_S* pEntry;

    if (gpVdecChnManager == NULL) 
    {
        return FAILURE;
    }

    pthread_mutex_lock(&gpVdecChnManager->mLock);
    ret = searchExistChannel_l(VdChn, ppChn);
    pthread_mutex_unlock(&gpVdecChnManager->mLock);
    return ret;
}

static ERRORTYPE addChannel_l(VDEC_CHN_MAP_S *pChn)
{
    if (gpVdecChnManager == NULL)
    {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpVdecChnManager->mList);
    return SUCCESS;
}

static ERRORTYPE addChannel(VDEC_CHN_MAP_S *pChn)
{
    if (gpVdecChnManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVdecChnManager->mLock);
    ERRORTYPE ret = addChannel_l(pChn);
    pthread_mutex_unlock(&gpVdecChnManager->mLock);
    return SUCCESS;
}

static ERRORTYPE removeChannel(VDEC_CHN_MAP_S *pChn)
{
    if (gpVdecChnManager == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVdecChnManager->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpVdecChnManager->mLock);
    return SUCCESS;
}

MM_COMPONENTTYPE* VDEC_GetChnComp(MPP_CHN_S *pMppChn)
{
    VDEC_CHN_MAP_S* pChn;

    if (searchExistChannel(pMppChn->mChnId, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mComp;
}

static VDEC_CHN_MAP_S* VDEC_CHN_MAP_S_Construct()
{
    VDEC_CHN_MAP_S *pChannel = (VDEC_CHN_MAP_S*)malloc(sizeof(VDEC_CHN_MAP_S));
    if(NULL == pChannel)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(VDEC_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}

static void VDEC_CHN_MAP_S_Destruct(VDEC_CHN_MAP_S *pChannel)
{
    if(pChannel->mComp)
    {
        aloge("fatal error! Vdec component need free before!");
        COMP_FreeHandle(pChannel->mComp);
        pChannel->mComp = NULL;
    }
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    free(pChannel);
}

static ERRORTYPE VideoDecEventHandler(
	 PARAM_IN COMP_HANDLETYPE hComponent,
	 PARAM_IN void* pAppData,
	 PARAM_IN COMP_EVENTTYPE eEvent,
	 PARAM_IN unsigned int nData1,
	 PARAM_IN unsigned int nData2,
	 PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S VdecChnInfo;
    ret = COMP_GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &VdecChnInfo);
    if(ret == SUCCESS)
    {
        alogv("video decoder event, MppChannel[%d][%d][%d]", VdecChnInfo.mModId, VdecChnInfo.mDevId, VdecChnInfo.mChnId);
    }
	VDEC_CHN_MAP_S *pChn = (VDEC_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("video decoder EventCmdComplete, current StateSet[%d]", nData2);
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
            if(ERR_VDEC_SAMESTATE == nData1)
            {
                alogv("set same state to vdec!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_VDEC_INVALIDSTATE == nData1)
            {
                aloge("why vdec state turn to invalid?");
                break;
            }
            else if(ERR_VDEC_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! vdec state transition incorrect."); 
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_VDEC_NOMEM == nData1)
            {
                aloge("fatal error! no memory!");
                break;
            }
            else
            {
                aloge("fatal error! unknown error[0x%x]!", nData1);
                break;
            }
        }
        case COMP_EventBufferFlag:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_VDEC;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pChn->mVdChn;
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_NOTIFY_EOF, NULL);
            break;
        }
        case COMP_EventMultiPixelExit:
        {
            alogw("resolution change, what can I do?");
            break;
        }
        case COMP_EventMoreBuffer:
        {
            alogd("vdec no frame buffer! notify user!");
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_VDEC;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pChn->mVdChn;
            if(pChn->mCallbackInfo.callback)
            {
                pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_VDEC_NOTIFY_NO_FRAME_BUFFER, NULL);
            }
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
	return SUCCESS;
}

COMP_CALLBACKTYPE VideoDecCallback = {
	.EventHandler = VideoDecEventHandler,
	.EmptyBufferDone = NULL,
	.FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_VDEC_CreateChn(VDEC_CHN VdChn, const VDEC_CHN_ATTR_S *pAttr)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal VencAttr!");
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&gpVdecChnManager->mLock);
    if(SUCCESS == searchExistChannel_l(VdChn, NULL))
    {
        pthread_mutex_unlock(&gpVdecChnManager->mLock);
        return ERR_VDEC_EXIST;
    }
    VDEC_CHN_MAP_S *pNode = VDEC_CHN_MAP_S_Construct();
    pNode->mVdChn = VdChn;
    
    //create Vdec Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mComp, CDX_ComponentNameVideoDecoder, (void*)pNode, &VideoDecCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_VDEC;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mVdChn;
    eRet = COMP_SetConfig(pNode->mComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = COMP_SetConfig(pNode->mComp, COMP_IndexVendorVdecChnAttr, (void*)pAttr);
    eRet = COMP_SetConfig(pNode->mComp, COMP_IndexVendorVEFreq, (void*)&gpVdecChnManager->mVeFreq);
    eRet = COMP_SendCommand(pNode->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create Venc Component done!

    addChannel_l(pNode);
    pthread_mutex_unlock(&gpVdecChnManager->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_VDEC_DestroyChn(VDEC_CHN VdChn)
{
    ERRORTYPE ret;
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
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
                aloge("fatal error! invalid VdChn[%d] state[0x%x]!", VdChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                removeChannel(pChn);
                COMP_FreeHandle(pChn->mComp);
                pChn->mComp = NULL;
                VDEC_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_VDEC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_VDEC_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no Vdec component!");
        list_del(&pChn->mList);
        VDEC_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_VDEC_GetChnAttr(VDEC_CHN VdChn, VDEC_CHN_ATTR_S *pAttr)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVdecChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_StartRecvStreamEx(VDEC_CHN VdChn, VDEC_DECODE_FRAME_PARAM_S *pDecodeParam)
{    
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    int ret = SUCCESS;
    int eRet = SUCCESS;
    COMP_STATETYPE nCompState;
    COMP_STATETYPE nCompState_new;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StateIdle == nCompState || COMP_StatePause == nCompState)
    {        
        pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVdecDecodeFrameParam, pDecodeParam);
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! send command stateExecuting fail");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        
        pChn->mComp->GetState(pChn->mComp, &nCompState_new); 
        if(nCompState_new == nCompState)
        {
            aloge("fatal_error_change_state_fail:%d-%d",nCompState,nCompState_new);
            ret = FAILURE;
        }
    }
    else
    {
        alogd("VdecChannel[%d] State[0x%x], do nothing!", VdChn, nCompState);
        ret = SUCCESS;
    }
    return ret;
}


ERRORTYPE AW_MPI_VDEC_StartRecvStream(VDEC_CHN VdChn)
{
    return AW_MPI_VDEC_StartRecvStreamEx(VdChn, NULL);
}



ERRORTYPE AW_MPI_VDEC_StopRecvStream(VDEC_CHN VdChn)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
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
        alogv("VdecChannel[%d] State[0x%x], do nothing!", VdChn, nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check VdecChannelState[0x%x]!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_VDEC_Pause(VDEC_CHN VdChn)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
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
        alogd("vdecChannel[%d] already statePause.", VdChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("VdecChannel[%d] stateIdle, can't turn to statePause!", VdChn);
        ret = ERR_VDEC_INCORRECT_STATE_TRANSITION;
    }
    else
    {
        aloge("fatal error! check VdecChannel[%d]State[0x%x]!", VdChn, nCompState);
        ret = ERR_VDEC_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_VDEC_Resume(VDEC_CHN VdChn)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(COMP_StatePause == nCompState)
    {
        eRet = pChn->mComp->SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! Send command statePause fail!");
        }
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else if(COMP_StateExecuting == nCompState)
    {
        alogd("vdecChannel[%d] already stateExecuting.", VdChn);
        ret = SUCCESS;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogd("VdecChannel[%d] stateIdle, can't turn to stateExecuting!", VdChn);
        ret = ERR_VDEC_INCORRECT_STATE_TRANSITION;
    }
    else
    {
        aloge("fatal error! check VdecChannel[%d]State[0x%x]!", VdChn, nCompState);
        ret = ERR_VDEC_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_VDEC_Seek(VDEC_CHN VdChn)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
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
        aloge("fatal error! can't seek int VdecChannel[%d]State[0x%x]!", VdChn, nCompState);
        ret = ERR_VDEC_INCORRECT_STATE_TRANSITION;
    }
    return ret;
}

ERRORTYPE AW_MPI_VDEC_Query(VDEC_CHN VdChn, VDEC_CHN_STAT_S *pStat)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVdecChnState, pStat);
    return ret;
}

/** 
 * Register callback, mpp_vdec use callback to notify caller.
 *
 * @return SUCCESS.
 * @param VdChn vdec channel number.
 * @param pCallback callback struct.
 */
ERRORTYPE AW_MPI_VDEC_RegisterCallback(VDEC_CHN VdChn, MPPCallbackInfo *pCallback)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

/** 
 * set or clear stream end of file, mpp_vdec can send eof callback msg to upper when it be set eof.
 *
 * @return SUCCESS.
 * @param VdChn vdec channel number.
 * @param bEofFlag indicate if eof.
 */
ERRORTYPE AW_MPI_VDEC_SetStreamEof(VDEC_CHN VdChn, BOOL bEofFlag)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
       aloge("wrong state[0x%x], return!", nState);
       return ERR_VDEC_NOT_PERM;
    }
    if(bEofFlag)
    {
        ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorSetStreamEof, NULL);
    }
    else
    {
        ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorClearStreamEof, NULL);
    }
    return ret;
}

//ERRORTYPE AW_MPI_VDEC_GetFd(VDEC_CHN VdChn);

/** 
 * Reset vdec channel, free all resources. must call it in stateIdle.
 *
 * @return SUCCESS, ERR_VDEC_NOT_PERM
 * @param VdChn vdec channel number.
 */
ERRORTYPE AW_MPI_VDEC_ResetChn(VDEC_CHN VdChn)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    COMP_STATETYPE nCompState = COMP_StateInvalid;
    ERRORTYPE eRet, eRet2;
    eRet = pChn->mComp->GetState(pChn->mComp, &nCompState);
    if(eRet == SUCCESS && COMP_StateIdle == nCompState)
    {
        eRet2 = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVdecResetChannel, NULL);
        if(eRet2 != SUCCESS)
        {
            aloge("fatal error! reset channel fail[0x%x]!", eRet2);
        }
        return eRet2;
    }
    else
    {
        aloge("wrong status[0x%x], can't reset vdec channel!", nCompState);
        return ERR_VDEC_NOT_PERM;
    }
}

ERRORTYPE AW_MPI_VDEC_SetChnParam(VDEC_CHN VdChn, VDEC_CHN_PARAM_S* pParam)
{
    if(!(VdChn>=0 && VdChn<VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVdecParam, (void*)pParam);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_GetChnParam(VDEC_CHN VdChn, VDEC_CHN_PARAM_S* pParam)
{
    if(!(VdChn>=0 && VdChn<VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVdecParam, (void*)pParam);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_SetProtocolParam(VDEC_CHN VdChn, VDEC_PRTCL_PARAM_S *pParam)
{
    if(!(VdChn>=0 && VdChn<VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVdecProtocolParam, (void*)pParam);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_GetProtocolParam(VDEC_CHN VdChn,VDEC_PRTCL_PARAM_S *pParam)
{
    if(!(VdChn>=0 && VdChn<VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVdecProtocolParam, (void*)pParam);
    return ret;
}

/* s32MilliSec: -1 is block， 0 is no block，other positive number is timeout */
ERRORTYPE AW_MPI_VDEC_SendStream(VDEC_CHN VdChn, const VDEC_STREAM_S *pStream, int nMilliSec)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    VdecInputStream inputStream;
    inputStream.pStream = (VDEC_STREAM_S*)pStream;
    inputStream.nMilliSec = nMilliSec;
    COMP_BUFFERHEADERTYPE bufferHeader;
    bufferHeader.pAppPrivate = (void*)&inputStream;
    ret = pChn->mComp->EmptyThisBuffer(pChn->mComp, &bufferHeader);
    return ret;
}

/** 
 * get frame in non-binding mode.
 *
 * @return SUCCESS
 * @param VdChn vdec channel number.
 * @param pFrameInfo [out].
 * @param nMilliSec  -1 is block, 0 is no block, other positive number is timeout.
 */

ERRORTYPE AW_MPI_VDEC_GetDoubleImage(VDEC_CHN VdChn, VIDEO_FRAME_INFO_S *pFrameInfo, VIDEO_FRAME_INFO_S *pSubFrameInfo,int nMilliSec)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        aloge("fatal error! can not find VdecChn[%d]", VdChn);
        return ERR_VDEC_UNEXIST;
    }
    if(NULL == pFrameInfo)
    {
        aloge("fatal error! pFrame == NULL!");
        return ERR_VDEC_NULL_PTR;
    }

//    if(nMilliSec < -1)
//    {
//        aloge("fatal error! illegal nMilliSec[%d]!", nMilliSec);
//        return ERR_VDEC_ILLEGAL_PARAM;
//    }
    
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    VdecOutFrame vdecFrame;
    vdecFrame.pFrameInfo = pFrameInfo;
    vdecFrame.pSubFrameInfo = pSubFrameInfo;
    vdecFrame.nMilliSec = nMilliSec;
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVdecGetFrame, (void*)&vdecFrame);
    return ret;       
}

ERRORTYPE AW_MPI_VDEC_ReleaseDoubleImage(VDEC_CHN VdChn, VIDEO_FRAME_INFO_S *pFrameInfo, VIDEO_FRAME_INFO_S *pSubFrameInfo)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        alogw("Be careful! vdecChn[%d] is not exist!", VdChn);
        return ERR_VDEC_UNEXIST;
    }
    if(NULL == pFrameInfo)
    {
        aloge("fatal error! pFrame == NULL!");
        return ERR_VDEC_NULL_PTR;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    VdecOutFrame vdecFrame;
    vdecFrame.pFrameInfo = pFrameInfo;
    vdecFrame.pSubFrameInfo = pSubFrameInfo;
    vdecFrame.nMilliSec = 0;
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVdecReleaseFrame, (void*)&vdecFrame);
    return ret;    
}


ERRORTYPE AW_MPI_VDEC_GetImage(VDEC_CHN VdChn, VIDEO_FRAME_INFO_S *pFrameInfo, int nMilliSec)
{
    return AW_MPI_VDEC_GetDoubleImage(VdChn, pFrameInfo, NULL, nMilliSec);
}

ERRORTYPE AW_MPI_VDEC_ReleaseImage(VDEC_CHN VdChn, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    return AW_MPI_VDEC_ReleaseDoubleImage(VdChn, pFrameInfo, NULL);
}

//ERRORTYPE AW_MPI_VDEC_GetUserData(VDEC_CHN VdChn, VDEC_USERDATA_S *pstUserData, int s32MilliSec);
//ERRORTYPE AW_MPI_VDEC_ReleaseUserData(VDEC_CHN VdChn, VDEC_USERDATA_S *pstUserData);

ERRORTYPE AW_MPI_VDEC_SetRotate(VDEC_CHN VdChn, ROTATE_E enRotate)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorVdecRotate, (void*)&enRotate);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_GetRotate(VDEC_CHN VdChn, ROTATE_E *penRotate)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVdecRotate, (void*)penRotate);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_GetChnLuma(VDEC_CHN VdChn, VDEC_CHN_LUM_S *pLuma)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->GetConfig(pChn->mComp, COMP_IndexVendorVdecLuma, (void*)pLuma);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_ReopenVideoEngine(VDEC_CHN VdChn)
{
    if(!(VdChn>=0 && VdChn <VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mComp->GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StatePause != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = pChn->mComp->SetConfig(pChn->mComp, COMP_IndexVendorReopenVideoEngine, NULL);
    return ret;
}

//ERRORTYPE AW_MPI_VDEC_SetUserPic(VDEC_CHN VdChn, VIDEO_FRAME_INFO_S *pstUsrPic);
//ERRORTYPE AW_MPI_VDEC_EnableUserPic(VDEC_CHN VdChn, BOOL bInstant);
//ERRORTYPE AW_MPI_VDEC_DisableUserPic(VDEC_CHN VdChn);

//ERRORTYPE AW_MPI_VDEC_SetDisplayMode(VDEC_CHN VdChn, VIDEO_DISPLAY_MODE_E enDisplayMode);
//ERRORTYPE AW_MPI_VDEC_GetDisplayMode(VDEC_CHN VdChn, VIDEO_DISPLAY_MODE_E *penDisplayMode);

ERRORTYPE AW_MPI_VDEC_SetVEFreq(VDEC_CHN VdChn, int nFreq)
{
    if(MM_INVALID_CHN == VdChn)
    {
        alogd("change global ve freq[%d]->[%d]", gpVdecChnManager->mVeFreq, nFreq);
        pthread_mutex_lock(&gpVdecChnManager->mLock);
        gpVdecChnManager->mVeFreq = nFreq;
        pthread_mutex_unlock(&gpVdecChnManager->mLock);
        return SUCCESS;
    }
    if(!(VdChn>=0 && VdChn<VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        aloge("fatal error! vdChn[%d] is not exist!", VdChn);
        return ERR_VDEC_UNEXIST;
    }
    pthread_mutex_lock(&gpVdecChnManager->mLock);
    gpVdecChnManager->mVeFreq = nFreq;
    pthread_mutex_unlock(&gpVdecChnManager->mLock);
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorVEFreq, (void*)&gpVdecChnManager->mVeFreq);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_SetVideoStreamInfo(VDEC_CHN VdChn, VideoStreamInfo *pVideoStreamInfo)
{
    if(!(VdChn>=0 && VdChn<VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        aloge("fatal error! vdChn[%d] is not exist!", VdChn);
        return ERR_VDEC_UNEXIST;
    }
    if (NULL == pVideoStreamInfo)
    {
        aloge("fatal error! Set VideoStreamInfo is NULL!");
        return ERR_VDEC_NULL_PTR;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorExtraData, (void*)pVideoStreamInfo);
    return ret;
}

ERRORTYPE AW_MPI_VDEC_ForceFramePackage(VDEC_CHN VdChn, BOOL bFlag)
{
    if(!(VdChn>=0 && VdChn<VDEC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VdChn[%d]!", VdChn);
        return ERR_VDEC_INVALID_CHNID;
    }
    VDEC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VdChn, &pChn))
    {
        aloge("fatal error! vdChn[%d] is not exist!", VdChn);
        return ERR_VDEC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(nState != COMP_StateIdle)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VDEC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorVdecForceFramePackage, (void*)&bFlag);
    return ret;
}

