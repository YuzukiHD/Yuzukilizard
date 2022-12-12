/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_venc.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/03/15
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG
#define LOG_TAG "mpi_venc"
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
#include "mm_comm_video.h"
#include "mm_comm_venc.h"
#include <mm_comm_vi.h>
#include "mpi_venc.h"

//media internal common headers.
#include <media_common.h>
#include <tsemaphore.h>
#include <mm_component.h>
#include <ChannelRegionInfo.h>
#include <mpi_venc_private.h>
#include <VencCompStream.h>
#include <vencoder.h>
#include <BITMAP_S.h>

typedef struct VEncFrameNode
{
    VIDEO_FRAME_INFO_S mFrameInfo;
    struct list_head mList;
}VEncFrameNode;

typedef struct VENC_CHN_MAP_S
{
    VENC_CHN            mVeChn;     // video encoder channel index, [0, VENC_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mEncComp;  // video encode component instance
    cdx_sem_t           mSemCompCmd;
    //struct list_head    mUsedFrameList; //VEncFrameNode
    //struct list_head    mIdleFrameList; //VEncFrameNode
    //pthread_mutex_t     mFrameListLock; //for lock mUsedFrameList
    //cdx_sem_t           mUsedFrameSem;
    //BOOL                mWaitUsedFrameFlag;
    //store some command result
    int mStateTransitionError;

    //osd manage    
    pthread_mutex_t mRegionLock;
    struct list_head mOverlayList;    //ChannelRegionInfo
    struct list_head mCoverList;    //ChannelRegionInfo

    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}VENC_CHN_MAP_S;

typedef struct VencChnManager
{
    pthread_mutex_t mLock;
    struct list_head mChnList;   //element type: VENC_CHN_MAP_S
    int mVeFreq;    // 0: don't care frequency, any value will be OK. Maybe use default value. unit:MHz.
} VencChnManager;

static VencChnManager *gpVencChnMap = NULL;

ERRORTYPE VENC_Construct(void)
{
    int ret;
    if (gpVencChnMap != NULL) {
        return SUCCESS;
    }
    gpVencChnMap = (VencChnManager*)malloc(sizeof(VencChnManager));
    if (NULL == gpVencChnMap) {
        aloge("alloc VencChnManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    memset(gpVencChnMap, 0, sizeof(VencChnManager));
    ret = pthread_mutex_init(&gpVencChnMap->mLock, NULL);
    if (ret != 0) {
        aloge("fatal error! mutex init fail");
        free(gpVencChnMap);
        gpVencChnMap = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpVencChnMap->mChnList);
    return SUCCESS;
}

ERRORTYPE VENC_Destruct(void)
{
    if (gpVencChnMap != NULL) {
        if (!list_empty(&gpVencChnMap->mChnList)) {
            aloge("fatal error! some venc channel still running when destroy venc device!");
        }
        pthread_mutex_destroy(&gpVencChnMap->mLock);
        free(gpVencChnMap);
        gpVencChnMap = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistChannel_l(VENC_CHN VeChn, VENC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    VENC_CHN_MAP_S* pEntry;

    if (gpVencChnMap == NULL) {
        return FAILURE;
    }

    list_for_each_entry(pEntry, &gpVencChnMap->mChnList, mList)
    {
        if(pEntry->mVeChn == VeChn)
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

static ERRORTYPE searchExistChannel(VENC_CHN VeChn, VENC_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = FAILURE;
    VENC_CHN_MAP_S* pEntry;

    if (gpVencChnMap == NULL) {
        return FAILURE;
    }

    pthread_mutex_lock(&gpVencChnMap->mLock);
    ret = searchExistChannel_l(VeChn, ppChn);
    pthread_mutex_unlock(&gpVencChnMap->mLock);
    return ret;
}

static ERRORTYPE addChannel_l(VENC_CHN_MAP_S *pChn)
{
    if (gpVencChnMap == NULL) {
        return FAILURE;
    }
    list_add_tail(&pChn->mList, &gpVencChnMap->mChnList);
    return SUCCESS;
}

static ERRORTYPE addChannel(VENC_CHN_MAP_S *pChn)
{
    ERRORTYPE ret;
    if (gpVencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVencChnMap->mLock);
    ret = addChannel_l(pChn);
    pthread_mutex_unlock(&gpVencChnMap->mLock);
    return ret;
}

static ERRORTYPE removeChannel(VENC_CHN_MAP_S *pChn)
{
    if (gpVencChnMap == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVencChnMap->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpVencChnMap->mLock);
    return SUCCESS;
}

MM_COMPONENTTYPE* VENC_GetChnComp(MPP_CHN_S *pMppChn)
{
    VENC_CHN_MAP_S* pChn;

    if (searchExistChannel(pMppChn->mChnId, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mEncComp;
}

static VENC_CHN_MAP_S* VENC_CHN_MAP_S_Construct()
{
    VENC_CHN_MAP_S *pChannel = (VENC_CHN_MAP_S*)malloc(sizeof(VENC_CHN_MAP_S));
    if(NULL == pChannel)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(VENC_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    //INIT_LIST_HEAD(&pChannel->mUsedFrameList);
    //INIT_LIST_HEAD(&pChannel->mIdleFrameList);
    //pthread_mutex_init(&pChannel->mFrameListLock, NULL);
    //cdx_sem_init(&pChannel->mUsedFrameSem, 0);

    int err = pthread_mutex_init(&pChannel->mRegionLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    INIT_LIST_HEAD(&pChannel->mOverlayList);
    INIT_LIST_HEAD(&pChannel->mCoverList);
    return pChannel;
}

static void VENC_CHN_MAP_S_Destruct(VENC_CHN_MAP_S *pChannel)
{
    if(pChannel->mEncComp)
    {
        aloge("fatal error! Venc component need free before!");
        COMP_FreeHandle(pChannel->mEncComp);
        pChannel->mEncComp = NULL;
    }
//    pthread_mutex_lock(&pChannel->mFrameListLock);
//    if(!list_empty(&pChannel->mUsedFrameList))
//    {
//        int cnt = 0;
//        VEncFrameNode *pEntry, *pTmp;
//        list_for_each_entry_safe(pEntry, pTmp, &pChannel->mUsedFrameList, mList)
//        {
//            list_del(&pEntry->mList);
//            free(pEntry);
//            cnt++;
//        }
//        alogw("Be careful! must request all used [%d]frames before destruct venc channel[%d]!", cnt, pChannel->mVeChn);
//    }
//    if(!list_empty(&pChannel->mIdleFrameList))
//    {
//        int cnt = 0;
//        VEncFrameNode *pEntry, *pTmp;
//        list_for_each_entry_safe(pEntry, pTmp, &pChannel->mIdleFrameList, mList)
//        {
//            list_del(&pEntry->mList);
//            free(pEntry);
//            cnt++;
//        }
//        alogv("free [%d]idle frames before destruct venc channel[%d]!", cnt, pChannel->mVeChn);
//    }
//    pthread_mutex_unlock(&pChannel->mFrameListLock);
//    pthread_mutex_destroy(&pChannel->mFrameListLock);
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    //cdx_sem_deinit(&pChannel->mUsedFrameSem);

    pthread_mutex_lock(&pChannel->mRegionLock);
    ChannelRegionInfo *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pChannel->mOverlayList, mList)
    {
        list_del(&pEntry->mList);
        ChannelRegionInfo_Destruct(pEntry);
    }
    list_for_each_entry_safe(pEntry, pTmp, &pChannel->mCoverList, mList)
    {
        list_del(&pEntry->mList);
        ChannelRegionInfo_Destruct(pEntry);
    }
    pthread_mutex_unlock(&pChannel->mRegionLock);
    pthread_mutex_destroy(&pChannel->mRegionLock);
    free(pChannel);
}

static ERRORTYPE VideoEncEventHandler(
	 PARAM_IN COMP_HANDLETYPE hComponent,
	 PARAM_IN void* pAppData,
	 PARAM_IN COMP_EVENTTYPE eEvent,
	 PARAM_IN unsigned int nData1,
	 PARAM_IN unsigned int nData2,
	 PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S VencChnInfo;
    ret = ((MM_COMPONENTTYPE*)hComponent)->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &VencChnInfo);
    if(ret == SUCCESS)
    {
        alogv("video encoder event, MppChannel[%d][%d][%d]", VencChnInfo.mModId, VencChnInfo.mDevId, VencChnInfo.mChnId);
    }
	VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("video encoder EventCmdComplete, current StateSet[%d]", nData2);
                pChn->mStateTransitionError = SUCCESS;
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
            if(ERR_VENC_SAMESTATE == nData1)
            {
                alogv("set same state to venc!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_VENC_INVALIDSTATE == nData1)
            {
                aloge("why venc state turn to invalid?");
                break;
            }
            else if(ERR_VENC_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! venc state transition incorrect:%x.",nData2);
                if(nData2 == 0)     // no more detail error info
                {
                    pChn->mStateTransitionError = ERR_VENC_INCORRECT_STATE_TRANSITION;
                }
                else
                {
                    pChn->mStateTransitionError = nData2;
                }
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_VENC_TIMEOUT == nData1)
            {
                uint64_t framePts = *(uint64_t*)pEventData;
                alogw("Be careful! detect encode timeout, pts[%lld]us", framePts);
                VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
                MPP_CHN_S ChannelInfo = {MOD_ID_VENC, 0, pChn->mVeChn};
                CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
                pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_VENC_TIMEOUT, (void*)&framePts);
                break;
            }
            else
            {
                aloge("unknown event error[0x%x]", nData1);
                break;
            }
        }
        case COMP_EventRecVbvFull:
        {
            VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
            MPP_CHN_S ChannelInfo = {MOD_ID_VENC, 0, pChn->mVeChn};
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_VENC_BUFFER_FULL, NULL);
            break;
        }
        case COMP_EventQpMapUpdateMBModeInfo:
        {
            VencMBModeCtrl *pMBModeCtrl = (VencMBModeCtrl *)pEventData;
            VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
            MPP_CHN_S ChannelInfo = {MOD_ID_VENC, 0, pChn->mVeChn};
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_VENC_QPMAP_UPDATE_MB_MODE_INFO, pMBModeCtrl);
            break;
        }
        case COMP_EventQpMapUpdateMBStatInfo:
        {
            VencMBSumInfo *pMbSumInfo = (VencMBSumInfo *)pEventData;
            VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
            MPP_CHN_S ChannelInfo = {MOD_ID_VENC, 0, pChn->mVeChn};
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_VENC_QPMAP_UPDATE_MB_STAT_INFO, pMbSumInfo);
            break;
        }
        case COMP_EventLinkageIsp2VeParam:
        {
            VencIsp2VeParam *pParam = (VencIsp2VeParam *)pEventData;
            VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
            MPP_CHN_S ChannelInfo = {MOD_ID_VENC, 0, pChn->mVeChn};
            if (pChn->mCallbackInfo.callback)
            {
                pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_LINKAGE_ISP2VE_PARAM, pParam);
            }
            break;
        }
        case COMP_EventLinkageVe2IspParam:
        {
            VencVe2IspParam *pParam = (VencVe2IspParam *)pEventData;
            VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
            MPP_CHN_S ChannelInfo = {MOD_ID_VENC, 0, pChn->mVeChn};
            if (pChn->mCallbackInfo.callback)
            {
                pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_LINKAGE_VE2ISP_PARAM, pParam);
            }
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
	return SUCCESS;
}
/*
ERRORTYPE VideoEncEmptyBufferDone(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN void* pAppData,
        PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
    VIDEO_FRAME_INFO_S *pFrameInfo =  (VIDEO_FRAME_INFO_S*)pBuffer->pAppPrivate;
    VEncFrameNode *pNode;
    pthread_mutex_lock(&pChn->mFrameListLock);
    if(!list_empty(&pChn->mIdleFrameList))
    {
        pNode = list_first_entry(&pChn->mIdleFrameList, VEncFrameNode, mList);
        list_del(&pNode->mList);
    }
    else
    {
        pNode = malloc(sizeof(VEncFrameNode));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
            pthread_mutex_unlock(&pChn->mFrameListLock);
            return ERR_VENC_NOMEM;
        }
    }
    memset(pNode, 0, sizeof(VEncFrameNode));
    copy_VIDEO_FRAME_INFO_S(&pNode->mFrameInfo, pFrameInfo);
    list_add(&pNode->mList, &pChn->mUsedFrameList);
    if(pChn->mWaitUsedFrameFlag)
    {
        cdx_sem_up_unique(&pChn->mUsedFrameSem);
        pChn->mWaitUsedFrameFlag = FALSE;
    }
    pthread_mutex_unlock(&pChn->mFrameListLock);
    return SUCCESS;
}
*/

ERRORTYPE VideoEncEmptyBufferDone(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN void* pAppData,
        PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    VENC_CHN_MAP_S *pChn = (VENC_CHN_MAP_S*)pAppData;
    VIDEO_FRAME_INFO_S *pFrameInfo =  (VIDEO_FRAME_INFO_S*)pBuffer->pAppPrivate;
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_VENC;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pChn->mVeChn;
    CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
    pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_RELEASE_VIDEO_BUFFER, (void*)pFrameInfo);
    return SUCCESS;
}

COMP_CALLBACKTYPE VideoEncCallback = {
	.EventHandler = VideoEncEventHandler,
	.EmptyBufferDone = VideoEncEmptyBufferDone,
	.FillBufferDone = NULL,
};

/**
 * reference encode param:
 * mjpeg: 1280x720@30fps, 15~20Mbps;
 *        1920x1080@30fps, 20M~30Mbps;
 * h264:  1280x720@30fps, 8Mbps;
 *        1920x1080@30fps, 14Mbps;
 */
ERRORTYPE AW_MPI_VENC_CreateChn(VENC_CHN VeChn, const VENC_CHN_ATTR_S *pAttr)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal VencAttr!");
        return ERR_VENC_ILLEGAL_PARAM;
    }
    if (gpVencChnMap == NULL) 
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVencChnMap->mLock);
    if(SUCCESS == searchExistChannel_l(VeChn, NULL))
    {
        pthread_mutex_unlock(&gpVencChnMap->mLock);
        return ERR_VENC_EXIST;
    }
    VENC_CHN_MAP_S *pNode = VENC_CHN_MAP_S_Construct();
    pNode->mVeChn = VeChn;
    
    //create Venc Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mEncComp, CDX_ComponentNameVideoEncoder, (void*)pNode, &VideoEncCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_VENC;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mVeChn;
    eRet = COMP_SetConfig(pNode->mEncComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = COMP_SetConfig(pNode->mEncComp, COMP_IndexVendorVencChnAttr, (void*)pAttr);
    eRet = COMP_SetConfig(pNode->mEncComp, COMP_IndexVendorVEFreq, (void*)&gpVencChnMap->mVeFreq);
    eRet = COMP_SendCommand(pNode->mEncComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create Venc Component done!

    addChannel_l(pNode);
    pthread_mutex_unlock(&gpVencChnMap->mLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_VENC_DestroyChn(VENC_CHN VeChn)
{
    ERRORTYPE ret;
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
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
                aloge("fatal error! invalid VeChn[%d] state[0x%x]!", VeChn, nCompState);
                eRet = FAILURE;
            }

            if(eRet == SUCCESS)
            {
                removeChannel(pChn);
                COMP_FreeHandle(pChn->mEncComp);
                pChn->mEncComp = NULL;
                VENC_CHN_MAP_S_Destruct(pChn);
                ret = SUCCESS;
            }
            else
            {
                ret = ERR_VENC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_VENC_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no Venc component!");
        list_del(&pChn->mList);
        VENC_CHN_MAP_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetChnlPriority(VENC_CHN VeChn, unsigned int nPriority)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    if(nPriority >= 2)
    {
        aloge("fatal error! illagal param!");
        return ERR_VENC_ILLEGAL_PARAM;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    int ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencChnPriority, &nPriority);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetChnlPriority(VENC_CHN VeChn, unsigned int *pPriority)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    if(NULL == pPriority)
    {
        aloge("fatal error! illagal param!");
        return ERR_VENC_ILLEGAL_PARAM;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    int ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencChnPriority, pPriority);
    return ret;
}

ERRORTYPE AW_MPI_VENC_ResetChn(VENC_CHN VeChn)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    COMP_STATETYPE nCompState = COMP_StateInvalid;
    ERRORTYPE eRet, eRet2;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(eRet == SUCCESS && COMP_StateIdle == nCompState)
    {
        eRet2 = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencResetChannel, NULL);
        if(eRet2 != SUCCESS)
        {
            aloge("fatal error! reset channel fail[0x%x]!", eRet2);
        }
        return eRet2;
    }
    else
    {
        aloge("wrong status[0x%x], can't reset venc channel!", nCompState);
        return ERR_VENC_NOT_PERM;
    }
}

ERRORTYPE AW_MPI_VENC_StartRecvPicEx(VENC_CHN VeChn, VENC_RECV_PIC_PARAM_S *pRecvParam)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pChn->mEncComp->GetState(pChn->mEncComp, &nCompState);
    if(COMP_StateIdle == nCompState )
    {
        pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencRecvPicParam, pRecvParam);
        eRet = pChn->mEncComp->SendCommand(pChn->mEncComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        if(eRet == SUCCESS)
        {
            cdx_sem_down(&pChn->mSemCompCmd);
            ret = pChn->mStateTransitionError;
            pChn->mStateTransitionError = 0;
        }
        else
        {
            aloge("fatal error! send command stateExecuting fail.");
            ret = eRet;
        }
    }
    else
    {
        alogd("VencChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_VENC_StartRecvPic(VENC_CHN VeChn)
{
    return AW_MPI_VENC_StartRecvPicEx(VeChn, NULL);
}

ERRORTYPE AW_MPI_VENC_StopRecvPic(VENC_CHN VeChn)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
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
        alogv("VencChannelState[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    else
    {
        aloge("fatal error! check VencChannelState[0x%x]!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_VENC_DestroyEncoder(VENC_CHN VeChn)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencDestroyEncoder, NULL);
    if(ret != SUCCESS)
    {
        aloge("fatal error! destory encoder error[0x%x]", ret);
    }
    return ret;
}


ERRORTYPE AW_MPI_VENC_Query(VENC_CHN VeChn, VENC_CHN_STAT_S *pStat)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencChnState, pStat);
    return ret;
}

ERRORTYPE AW_MPI_VENC_RegisterCallback(VENC_CHN VeChn, MPPCallbackInfo *pCallback)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_VENC_SetChnAttr(VENC_CHN VeChn, const VENC_CHN_ATTR_S *pAttr)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal VencAttr!");
        return ERR_VENC_ILLEGAL_PARAM;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetChnAttr(VENC_CHN VeChn, VENC_CHN_ATTR_S *pAttr)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    if(NULL == pAttr)
    {
        aloge("fatal error! illagal VencAttr!");
        return ERR_VENC_ILLEGAL_PARAM;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetStream(VENC_CHN VeChn, VENC_STREAM_S *pStream, int nMilliSec)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    if(NULL == pStream)
    {
        aloge("fatal error! pStream == NULL!");
        return ERR_VENC_NULL_PTR;
    }
    if(nMilliSec < -1)
    {
        aloge("fatal error! illegal nMilliSec[%d]!", nMilliSec);
        return ERR_VENC_ILLEGAL_PARAM;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    VEncStream stVencStream;
    stVencStream.pStream = pStream;
    stVencStream.nMilliSec = nMilliSec;
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencGetStream, (void*)&stVencStream);
    return ret;
}

ERRORTYPE AW_MPI_VENC_ReleaseStream(VENC_CHN VeChn, VENC_STREAM_S *pStream)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    if(NULL == pStream)
    {
        aloge("fatal error! pStream == NULL!");
        return ERR_VENC_NULL_PTR;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencReleaseStream, (void*)pStream);
    return ret;
}

ERRORTYPE AW_MPI_VENC_InsertUserData(VENC_CHN VeChn, unsigned char *pData, unsigned int nLen)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    DataSection stDataSection;
    stDataSection.mpData = pData;
    stDataSection.mLen = nLen;
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencUserData, (void*)&stDataSection);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SendFrame(VENC_CHN VeChn, VIDEO_FRAME_INFO_S *pFrame ,int nMilliSec)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    COMP_BUFFERHEADERTYPE bufferHeader;
    bufferHeader.pAppPrivate = (void*)pFrame;
    ret = COMP_EmptyThisBuffer(pChn->mEncComp, &bufferHeader);
    return ret;
}
/*
ERRORTYPE AW_MPI_VENC_GetUsedFrame(VENC_CHN VeChn, VIDEO_FRAME_INFO_S *pFrame ,int nMilliSec)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
_TryToGetFrame:
    pthread_mutex_lock(&pChn->mFrameListLock);
    if(!list_empty(&pChn->mUsedFrameList))
    {
        VEncFrameNode *pNode = list_first_entry(&pChn->mUsedFrameList, VEncFrameNode, mList);
        copy_VIDEO_FRAME_INFO_S(pFrame, &pNode->mFrameInfo);
        list_move_tail(&pNode->mList, &pChn->mIdleFrameList);
        pthread_mutex_unlock(&pChn->mFrameListLock);
        return SUCCESS;
    }
    else
    {
        if(0 == nMilliSec)
        {
            pthread_mutex_unlock(&pChn->mFrameListLock);
            return ERR_VENC_BUF_EMPTY;
        }
        else if(nMilliSec < 0)
        {
            pChn->mWaitUsedFrameFlag = TRUE;
            pthread_mutex_unlock(&pChn->mFrameListLock);
            cdx_sem_down(&pChn->mUsedFrameSem);
            goto _TryToGetFrame;
        }
        else if(nMilliSec > 0)
        {
            pChn->mWaitUsedFrameFlag = TRUE;
            pthread_mutex_unlock(&pChn->mFrameListLock);
            int twRet = cdx_sem_down_timedwait(&pChn->mUsedFrameSem, nMilliSec);
            if(0 == twRet)
            {
                goto _TryToGetFrame;
            }
            else if(ETIMEDOUT == twRet)
            {
                return ERR_VENC_BUF_EMPTY;
            }
            else
            {
                return ERR_VENC_BUF_EMPTY;
            }
        }
    }
}
*/

ERRORTYPE AW_MPI_VENC_RequestIDR(VENC_CHN VeChn, BOOL bInstant)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencRequestIDR, (void*)&bInstant);
    return ret;
}

int AW_MPI_VENC_GetHandle(VENC_CHN VeChn)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
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

ERRORTYPE AW_MPI_VENC_SetRoiCfg(VENC_CHN VeChn, VENC_ROI_CFG_S *pVencRoiCfg)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencRoiCfg, (void*)pVencRoiCfg);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetRoiCfg(VENC_CHN VeChn, unsigned int nIndex, VENC_ROI_CFG_S *pVencRoiCfg)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    if(NULL == pVencRoiCfg)
    {
        return ERR_VENC_NULL_PTR;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    pVencRoiCfg->Index = nIndex;
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencRoiCfg, (void*)pVencRoiCfg);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetRoiBgFrameRate(VENC_CHN VeChn, const VENC_ROIBG_FRAME_RATE_S *pstRoiBgFrmRate)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencRoiBgFrameRate, (void*)pstRoiBgFrmRate);
    return ret;
}
ERRORTYPE AW_MPI_VENC_GetRoiBgFrameRate(VENC_CHN VeChn, VENC_ROIBG_FRAME_RATE_S *pstRoiBgFrmRate)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencRoiBgFrameRate, (void*)pstRoiBgFrmRate);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264SliceSplit(VENC_CHN VeChn, const VENC_PARAM_H264_SLICE_SPLIT_S *pSliceSplit)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264SliceSplit, (void*)pSliceSplit);
    return ret;
    
}

ERRORTYPE AW_MPI_VENC_GetH264SliceSplit(VENC_CHN VeChn, VENC_PARAM_H264_SLICE_SPLIT_S *pSliceSplit)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264SliceSplit, (void*)pSliceSplit);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264InterPred(VENC_CHN VeChn, const VENC_PARAM_H264_INTER_PRED_S *pH264InterPred)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264InterPred, (void*)pH264InterPred);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264InterPred(VENC_CHN VeChn, VENC_PARAM_H264_INTER_PRED_S *pH264InterPred)
{
    if(!(VeChn>=0 && VeChn <VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264InterPred, (void*)pH264InterPred);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264IntraPred(VENC_CHN VeChn, const VENC_PARAM_H264_INTRA_PRED_S *pH264IntraPred)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264IntraPred, (void*)pH264IntraPred);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264IntraPred(VENC_CHN VeChn, VENC_PARAM_H264_INTRA_PRED_S *pH264IntraPred)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264IntraPred, (void*)pH264IntraPred);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264Trans(VENC_CHN VeChn, const VENC_PARAM_H264_TRANS_S *pH264Trans)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Trans, (void*)pH264Trans);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264Trans(VENC_CHN VeChn, VENC_PARAM_H264_TRANS_S *pH264Trans)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Trans, (void*)pH264Trans);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264Entropy(VENC_CHN VeChn, const VENC_PARAM_H264_ENTROPY_S *pH264EntropyEnc)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Entropy, (void*)pH264EntropyEnc);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264Entropy(VENC_CHN VeChn, VENC_PARAM_H264_ENTROPY_S *pH264EntropyEnc)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Entropy, (void*)pH264EntropyEnc);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264Poc(VENC_CHN VeChn, const VENC_PARAM_H264_POC_S *pH264Poc)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Poc, (void*)pH264Poc);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264Poc(VENC_CHN VeChn, VENC_PARAM_H264_POC_S *pH264Poc)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Poc, (void*)pH264Poc);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264Dblk(VENC_CHN VeChn, const VENC_PARAM_H264_DBLK_S *pH264Dblk)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Dblk, (void*)pH264Dblk);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264Dblk(VENC_CHN VeChn, VENC_PARAM_H264_DBLK_S *pH264Dblk)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Dblk, (void*)pH264Dblk);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH264Vui(VENC_CHN VeChn, const VENC_PARAM_H264_VUI_S *pH264Vui)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Vui, (void*)pH264Vui);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264Vui(VENC_CHN VeChn, VENC_PARAM_H264_VUI_S *pH264Vui)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH264Vui, (void*)pH264Vui);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH265Vui(VENC_CHN VeChn, const VENC_PARAM_H265_VUI_S *pH265Vui)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Vui, (void*)pH265Vui);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265Vui(VENC_CHN VeChn, VENC_PARAM_H265_VUI_S *pH265Vui)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Vui, (void*)pH265Vui);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH264SpsPpsInfo(VENC_CHN VeChn, VencHeaderData *pH264SpsPpsInfo)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorExtraData, (void*)pH264SpsPpsInfo);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265SpsPpsInfo(VENC_CHN VeChn, VencHeaderData *pH265SpsPpsInfo)
{
    return AW_MPI_VENC_GetH264SpsPpsInfo(VeChn, pH265SpsPpsInfo);
}

ERRORTYPE AW_MPI_VENC_SetJpegParam(VENC_CHN VeChn, const VENC_PARAM_JPEG_S *pJpegParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencJpegParam, (void*)pJpegParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetJpegParam(VENC_CHN VeChn, VENC_PARAM_JPEG_S *pJpegParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencJpegParam, (void*)pJpegParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetJpegExifInfo(VENC_CHN VeChn, const VENC_EXIFINFO_S *pJpegExifInfo)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencJpegExifInfo, (void*)pJpegExifInfo);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetJpegExifInfo(VENC_CHN VeChn, VENC_EXIFINFO_S *pJpegExifInfo)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencJpegExifInfo, (void*)pJpegExifInfo);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetJpegThumbBuffer(VENC_CHN VeChn, VENC_JPEG_THUMB_BUFFER_S *pThumbBuffer)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencJpegThumbBuffer, (void*)pThumbBuffer);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetMjpegParam(VENC_CHN VeChn, const VENC_PARAM_MJPEG_S *pMjpegParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencMjpegParam, (void*)pMjpegParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetMjpegParam(VENC_CHN VeChn, VENC_PARAM_MJPEG_S *pMjpegParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencMjpegParam, (void*)pMjpegParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetHighPassFilter(VENC_CHN VeChn, VencHighPassFilter *pHighPassFilter)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencHighPassFilter, (void*)pHighPassFilter);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetHighPassFilter(VENC_CHN VeChn, const VencHighPassFilter *pHighPassFilter)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState && COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencHighPassFilter, (void*)pHighPassFilter);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetDayOrNight(VENC_CHN VeChn, int *DayOrNight)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencDayOrNight, (void*)DayOrNight);
    return ret;
}


ERRORTYPE AW_MPI_VENC_SetDayOrNight(VENC_CHN VeChn, int *DayOrNight)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState && COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencDayOrNight, (void*)DayOrNight);
    return ret;
} 


ERRORTYPE AW_MPI_VENC_SetFrameRate(VENC_CHN VeChn, const VENC_FRAME_RATE_S *pFrameRate)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState && COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencFrameRate, (void*)pFrameRate);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetFrameRate(VENC_CHN VeChn, VENC_FRAME_RATE_S *pFrameRate)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencFrameRate, (void*)pFrameRate);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetTimeLapse(VENC_CHN VeChn, int64_t nTimeLapse)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState && COMP_StateExecuting != nState && COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorTimeLapse, (void*)&nTimeLapse);
    return ret;
}
ERRORTYPE AW_MPI_VENC_GetTimeLapse(VENC_CHN VeChn, int64_t *pTimeLapse)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState && COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorTimeLapse, (void*)pTimeLapse);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetRcParam(VENC_CHN VeChn, VENC_RC_PARAM_S *pRcParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencRcParam, (void*)pRcParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetRcParam(VENC_CHN VeChn, const VENC_RC_PARAM_S *pRcParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencRcParam, (void*)pRcParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetRefParam(VENC_CHN VeChn, const VENC_PARAM_REF_S *pstRefParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencRefParam, (void*)pstRefParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetRefParam(VENC_CHN VeChn, VENC_PARAM_REF_S *pstRefParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencRefParam, (void*)pstRefParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetColor2Grey(VENC_CHN VeChn, const VENC_COLOR2GREY_S *pChnColor2Grey)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencColor2Grey, (void*)pChnColor2Grey);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetColor2Grey(VENC_CHN VeChn, VENC_COLOR2GREY_S *pChnColor2Grey)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencColor2Grey, (void*)pChnColor2Grey);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetCrop(VENC_CHN VeChn, const VENC_CROP_CFG_S *pCropCfg)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencCrop, (void*)pCropCfg);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetCrop(VENC_CHN VeChn, VENC_CROP_CFG_S *pCropCfg)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencCrop, (void*)pCropCfg);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetJpegSnapMode(VENC_CHN VeChn, VENC_JPEG_SNAP_MODE_E enJpegSnapMode)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencJpegSnapMode, (void*)&enJpegSnapMode);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetJpegSnapMode(VENC_CHN VeChn, VENC_JPEG_SNAP_MODE_E *penJpegSnapMode)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencJpegSnapMode, (void*)penJpegSnapMode);
    return ret;
}

ERRORTYPE AW_MPI_VENC_EnableIDR(VENC_CHN VeChn, BOOL bEnableIDR)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencEnableIDR, (void*)&bEnableIDR);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetStreamBufInfo(VENC_CHN VeChn, VENC_STREAM_BUF_INFO_S *pStreamBufInfo)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencStreamBufInfo, (void*)pStreamBufInfo);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetRcPriority(VENC_CHN VeChn, VENC_RC_PRIORITY_E enRcPriority)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencRcPriority, (void*)&enRcPriority);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetRcPriority(VENC_CHN VeChn, VENC_RC_PRIORITY_E *penRcPriority)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencRcPriority, (void*)penRcPriority);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH265SliceSplit(VENC_CHN VeChn, const VENC_PARAM_H265_SLICE_SPLIT_S *pSliceSplit)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH265SliceSplit, (void*)pSliceSplit);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265SliceSplit(VENC_CHN VeChn, VENC_PARAM_H265_SLICE_SPLIT_S *pSliceSplit)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH265SliceSplit, (void*)pSliceSplit);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH265PredUnit(VENC_CHN VeChn, const VENC_PARAM_H265_PU_S *pPredUnit)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH265PredUnit, (void*)pPredUnit);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265PredUnit(VENC_CHN VeChn, VENC_PARAM_H265_PU_S *pPredUnit)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH265PredUnit, (void*)pPredUnit);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH265Trans(VENC_CHN VeChn, const VENC_PARAM_H265_TRANS_S *pH265Trans)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Trans, (void*)pH265Trans);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265Trans(VENC_CHN VeChn, VENC_PARAM_H265_TRANS_S *pH265Trans)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Trans, (void*)pH265Trans);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH265Entropy(VENC_CHN VeChn, const VENC_PARAM_H265_ENTROPY_S *pH265Entropy)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Entropy, (void*)pH265Entropy);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265Entropy(VENC_CHN VeChn, VENC_PARAM_H265_ENTROPY_S *pH265Entropy)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Entropy, (void*)pH265Entropy);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH265Dblk(VENC_CHN VeChn, const VENC_PARAM_H265_DBLK_S *pH265Dblk)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Dblk, (void*)pH265Dblk);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265Dblk(VENC_CHN VeChn, VENC_PARAM_H265_DBLK_S *pH265Dblk)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Dblk, (void*)pH265Dblk);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetH265Sao(VENC_CHN VeChn, const VENC_PARAM_H265_SAO_S *pH265Sao)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Sao, (void*)pH265Sao);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetH265Sao(VENC_CHN VeChn, VENC_PARAM_H265_SAO_S *pH265Sao)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencH265Sao, (void*)pH265Sao);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetFrameLostStrategy(VENC_CHN VeChn, const VENC_PARAM_FRAMELOST_S *pFrmLostParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencFrameLostStrategy, (void*)pFrmLostParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetFrameLostStrategy(VENC_CHN VeChn, VENC_PARAM_FRAMELOST_S *pFrmLostParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencFrameLostStrategy, (void*)pFrmLostParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetSuperFrameCfg(VENC_CHN VeChn, const VENC_SUPERFRAME_CFG_S *pSuperFrmParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencSuperFrameCfg, (void*)pSuperFrmParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetSuperFrameCfg(VENC_CHN VeChn,VENC_SUPERFRAME_CFG_S *pSuperFrmParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencSuperFrameCfg, (void*)pSuperFrmParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetIntraRefresh(VENC_CHN VeChn, VENC_PARAM_INTRA_REFRESH_S *pIntraRefresh)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencIntraRefresh, (void*)pIntraRefresh);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetIntraRefresh(VENC_CHN VeChn, VENC_PARAM_INTRA_REFRESH_S *pIntraRefresh)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencIntraRefresh, (void*)pIntraRefresh);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetSmartP(VENC_CHN VeChn, VencSmartFun *pSmartPParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencSmartP, (void*)pSmartPParam);
    return ret;
}
ERRORTYPE AW_MPI_VENC_GetSmartP(VENC_CHN VeChn, VencSmartFun *pSmartPParam)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencSmartP, (void*)pSmartPParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetBrightness(VENC_CHN VeChn, VencBrightnessS *pBrightness)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencBrightness, (void*)pBrightness);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetBrightness(VENC_CHN VeChn, VencBrightnessS *pBrightness)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencBrightness, (void*)pBrightness);
    return ret;
}

/*
ERRORTYPE AW_MPI_VENC_setOsdMaskRegions(VENC_CHN VeChn, VENC_OVERLAY_INFO *pOsdMaskRegion)
{
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencOverLay, (void*)pOsdMaskRegion);
    return ret;
}

ERRORTYPE AW_MPI_VENC_removeOsdMaskRegions(VENC_CHN VeChn)
{
    VENC_OVERLAY_INFO OsdMaskRegion = {0};
    return AW_MPI_VENC_setOsdMaskRegions(VeChn, &OsdMaskRegion);
}
*/
ERRORTYPE AW_MPI_VENC_SetVEFreq(VENC_CHN VeChn, int nFreq)
{
    if(MM_INVALID_CHN == VeChn)
    {
        alogd("change global ve freq[%d]->[%d]", gpVencChnMap->mVeFreq, nFreq);
        pthread_mutex_lock(&gpVencChnMap->mLock);
        gpVencChnMap->mVeFreq = nFreq;
        pthread_mutex_unlock(&gpVencChnMap->mLock);
        return SUCCESS;
    }
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    pthread_mutex_lock(&gpVencChnMap->mLock);
    gpVencChnMap->mVeFreq = nFreq;
    pthread_mutex_unlock(&gpVencChnMap->mLock);
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVEFreq, (void*)&gpVencChnMap->mVeFreq);
    return ret;
}
/*
 * MppAlpha: [0,128]
 * VencAlpha:[0, 15]
 */
static unsigned char convertMppAlpha2VencAlpha(unsigned int nMppAlpha)
{
    int nVencAlpha = ((int)nMppAlpha+1)*16/129 - 1;
    if (nVencAlpha < 0)
    {
        nVencAlpha = 0;
    }
    if(nVencAlpha > 15)
    {
        nVencAlpha = 15;
    }
    return (unsigned char)nVencAlpha;
}

static __inline int checkRectValid(RECT_S rect)
{
    if ((rect.X < 0) || (rect.Y < 0) || (rect.Width==0) || (rect.Height==0))
    {
        aloge("invalid region rect!!");
        return -1;
    }
    else
    {
        return 0;
    }
}

static unsigned int convert_INVERT_COLOR_MODE_E_to_vencMode(INVERT_COLOR_MODE_E mppMode)
{
    unsigned int eVencMode;
    switch(mppMode)
    {
        case LESSTHAN_LUM_THRESH:
            eVencMode = 0;
            break;
        case MORETHAN_LUM_THRESH:
            eVencMode = 1;
            break;
        case LESSTHAN_LUMDIFF_THRESH:
            eVencMode = 2;
            break;
        case MORETHAN_LUMDIFF_THRESH:
            eVencMode = 3;
            break;
        default:
            aloge("fatal error! unknown invert color mode[0x%x]", mppMode);
            eVencMode = 2;
            break;
    }
    return eVencMode;
}

/*
 * overlay list and cover list has been already resorted.
 */
static ERRORTYPE configVencOsd(VENC_CHN_MAP_S *pChn, VencOverlayInfoS *pVencOverlay)
{
    memset(pVencOverlay, 0, sizeof(VencOverlayInfoS));
    int index = 0;
    ChannelRegionInfo *pEntry;
    list_for_each_entry(pEntry, &pChn->mCoverList, mList)
    {
        if(index >= MAX_OVERLAY_SIZE)
        {
            alogw("Be careful! osd number[%d] is max!", index);
            break;
        }
        if(FALSE == pEntry->mbDraw)
        {
            continue;
        }
        if(pEntry->mRgnChnAttr.enType != COVER_RGN)
        {
            aloge("fatal error! rgn type[0x%x] wrong!", pEntry->mRgnChnAttr.enType);
        }
        if(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType != AREA_RECT)
        {
            aloge("fatal error! rgn area type[0x%x] wrong!", pEntry->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType);
        }
        if (checkRectValid(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect) < 0)
        {
            continue;
        }
        pVencOverlay->overlayHeaderList[index].start_mb_x = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.X+1, 16) / 16 - 1;
        pVencOverlay->overlayHeaderList[index].start_mb_y = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Y+1, 16) / 16 - 1;
        pVencOverlay->overlayHeaderList[index].end_mb_x = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.X+pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Width, 16) / 16 - 1;
        pVencOverlay->overlayHeaderList[index].end_mb_y = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Y+pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Height, 16) / 16 - 1;
        pVencOverlay->overlayHeaderList[index].extra_alpha_flag = 0;
        pVencOverlay->overlayHeaderList[index].extra_alpha = 0;
        pVencOverlay->overlayHeaderList[index].cover_yuv.use_cover_yuv_flag = 0;
        pVencOverlay->overlayHeaderList[index].cover_yuv.cover_y = (unsigned char)(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.mColor>>16&0xFF);
        pVencOverlay->overlayHeaderList[index].cover_yuv.cover_u = (unsigned char)(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.mColor>>8&0xFF);
        pVencOverlay->overlayHeaderList[index].cover_yuv.cover_v = (unsigned char)(pEntry->mRgnChnAttr.unChnAttr.stCoverChn.mColor&0xFF);
        pVencOverlay->overlayHeaderList[index].overlay_type = COVER_OVERLAY;
        pVencOverlay->overlayHeaderList[index].overlay_blk_addr = NULL;
        pVencOverlay->overlayHeaderList[index].bitmap_size = 0;
        ++index;
    }

    pVencOverlay->argb_type = VENC_OVERLAY_ARGB8888;
    list_for_each_entry(pEntry, &pChn->mOverlayList, mList)
    {
        if(index >= MAX_OVERLAY_SIZE)
        {
            alogw("Be careful! osd number[%d] is max!", index);
            break;
        }
        if(FALSE == pEntry->mbDraw)
        {
            continue;
        }
        if(pEntry->mRgnChnAttr.enType != OVERLAY_RGN)
        {
            aloge("fatal error! rgn type[0x%x] wrong!", pEntry->mRgnChnAttr.enType);
        }
        if (pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X < 0 || pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y < 0 
            || pEntry->mRgnAttr.unAttr.stOverlay.mSize.Width == 0 || pEntry->mRgnAttr.unAttr.stOverlay.mSize.Height == 0)
        {
            continue;
        }
        pVencOverlay->overlayHeaderList[index].start_mb_x = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X+1, 16) / 16 - 1;
        pVencOverlay->overlayHeaderList[index].start_mb_y = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y+1, 16) / 16 - 1;
        pVencOverlay->overlayHeaderList[index].end_mb_x = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X+pEntry->mRgnAttr.unAttr.stOverlay.mSize.Width, 16) / 16 - 1;
        pVencOverlay->overlayHeaderList[index].end_mb_y = ALIGN(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y+pEntry->mRgnAttr.unAttr.stOverlay.mSize.Height, 16) / 16 - 1;
        if(MM_PIXEL_FORMAT_RGB_8888 == pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt)
        {
            pVencOverlay->overlayHeaderList[index].extra_alpha_flag = 0;
            pVencOverlay->overlayHeaderList[index].extra_alpha = 0;
            pVencOverlay->argb_type = VENC_OVERLAY_ARGB8888;
        }
        else if(MM_PIXEL_FORMAT_RGB_1555 == pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt)
        {
            if(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha == pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.mBgAlpha)
            {
                pVencOverlay->overlayHeaderList[index].extra_alpha_flag = 1;
                pVencOverlay->overlayHeaderList[index].extra_alpha = convertMppAlpha2VencAlpha(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha);
            }
            else
            {
                pVencOverlay->overlayHeaderList[index].extra_alpha_flag = 0;
                pVencOverlay->overlayHeaderList[index].extra_alpha = 0;
            }
            pVencOverlay->argb_type = VENC_OVERLAY_ARGB1555;
        }
        else if(MM_PIXEL_FORMAT_RGB_4444 == pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt)
        {
            pVencOverlay->overlayHeaderList[index].extra_alpha_flag = 0;
            pVencOverlay->overlayHeaderList[index].extra_alpha = 0;
            pVencOverlay->argb_type = VENC_OVERLAY_ARGB4444;
        }
        else
        {
            aloge("fatal error! unsupport overlay pixFmt[0x%x]", pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt);
        }
        if(!pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn)
        {
            pVencOverlay->overlayHeaderList[index].overlay_type = NORMAL_OVERLAY;
        }
        else
        {
            pVencOverlay->overlayHeaderList[index].overlay_type = LUMA_REVERSE_OVERLAY;
        }
        pVencOverlay->overlayHeaderList[index].overlay_blk_addr = (unsigned char*)pEntry->mBmp.mpData;
        pVencOverlay->overlayHeaderList[index].bitmap_size = BITMAP_S_GetdataSize(&pEntry->mBmp);

        pVencOverlay->overlayHeaderList[index].bforce_reverse_flag = 0;
        pVencOverlay->overlayHeaderList[index].reverse_unit_mb_w_minus1 = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width/16 - 1;
        pVencOverlay->overlayHeaderList[index].reverse_unit_mb_h_minus1 = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height/16 - 1;

        pVencOverlay->invert_mode = convert_INVERT_COLOR_MODE_E_to_vencMode(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod);
        pVencOverlay->invert_threshold = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh;
        ++index;
    }
    pVencOverlay->blk_num = index;
    return SUCCESS;
}

/**
 * @return true: priority first < second, false:priority first >= second
 */
static BOOL compareRegionPriority(const RGN_CHN_ATTR_S *pFirst, const RGN_CHN_ATTR_S *pSecond)
{
    if(pFirst->enType != pSecond->enType)
    {
        aloge("fatal error! why rgnType is not match[0x%x]!=[0x%x]", pFirst->enType, pSecond->enType);
        return FALSE;
    }
    if(OVERLAY_RGN == pFirst->enType)
    {
        if(pFirst->unChnAttr.stOverlayChn.mLayer < pSecond->unChnAttr.stOverlayChn.mLayer)
        {
            return TRUE;
        }
        return FALSE;
    }
    else if(COVER_RGN == pFirst->enType)
    {
        if(AREA_RECT == pFirst->unChnAttr.stCoverChn.enCoverType)
        {
            if(pFirst->unChnAttr.stCoverChn.mLayer < pSecond->unChnAttr.stCoverChn.mLayer)
            {
                return TRUE;
            }
            return FALSE;
        }
        else
        {
            aloge("fatal error! not support cover type[0x%x]", pFirst->unChnAttr.stCoverChn.enCoverType);
            return FALSE;
        }
    }
    else
    {
        aloge("fatal error! unsupport rgnType[0x%x]", pFirst->enType);
        return FALSE;
    }
}

ERRORTYPE checkRegionValidForVenc(const RGN_ATTR_S *pRgnAttr, const RGN_CHN_ATTR_S *pRgnChnAttr)
{
    ERRORTYPE ret = SUCCESS;
    if(pRgnAttr->enType != pRgnChnAttr->enType)
    {
        aloge("fatal error! type[0x%x]!=[0x%x]", pRgnAttr->enType, pRgnChnAttr->enType);
        return ERR_VENC_ILLEGAL_PARAM;
    }
    if(OVERLAY_RGN == pRgnAttr->enType)
    {
        if(pRgnAttr->unAttr.stOverlay.mSize.Width%16 != 0 || pRgnAttr->unAttr.stOverlay.mSize.Height%16 != 0)
        {
            aloge("fatal error! overlay width[%d] and height[%d] must all 16 align!", pRgnAttr->unAttr.stOverlay.mSize.Width, pRgnAttr->unAttr.stOverlay.mSize.Height);
            return ERR_VENC_ILLEGAL_PARAM;
        }
        if(pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X%16 != 0 || pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y%16 != 0)
        {
            aloge("fatal error! overlay position X[%d] and Y[%d] must all 16 align!", pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X, pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y);
            return ERR_VENC_ILLEGAL_PARAM;
        }
    }
    else if(COVER_RGN == pRgnAttr->enType)
    {
        if(pRgnChnAttr->unChnAttr.stCoverChn.stRect.X%16 != 0 || pRgnChnAttr->unChnAttr.stCoverChn.stRect.X%16 != 0
            || pRgnChnAttr->unChnAttr.stCoverChn.stRect.Width%16 != 0 || pRgnChnAttr->unChnAttr.stCoverChn.stRect.Height%16 != 0)
        {
            aloge("fatal error! cover rect X[%d], Y[%d], W[%d], H[%d] must all 16 align!", 
                pRgnChnAttr->unChnAttr.stCoverChn.stRect.X, pRgnChnAttr->unChnAttr.stCoverChn.stRect.Y, 
                pRgnChnAttr->unChnAttr.stCoverChn.stRect.Width, pRgnChnAttr->unChnAttr.stCoverChn.stRect.Height);
            return ERR_VENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        aloge("fatal error! unknown region type:%d", pRgnAttr->enType);
        return ERR_VENC_ILLEGAL_PARAM;
    }
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetRegion(VENC_CHN VeChn, RGN_HANDLE RgnHandle, RGN_ATTR_S *pRgnAttr, const RGN_CHN_ATTR_S *pRgnChnAttr, BITMAP_S *pBmp)
{
    ERRORTYPE ret = SUCCESS;
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    //check param valid
    ret = checkRegionValidForVenc(pRgnAttr, pRgnChnAttr);
    if(ret != SUCCESS)
    {
        return ret;
    }
    pthread_mutex_lock(&pChn->mRegionLock);
    //if handle is exist, return.
    ChannelRegionInfo *pEntry;
    list_for_each_entry(pEntry, &pChn->mOverlayList, mList)
    {
        if(RgnHandle == pEntry->mRgnHandle)
        {
            aloge("fatal error! RgnHandle[%d] is already exist!", RgnHandle);
            ret = ERR_VENC_EXIST;
            goto _err0;
        }
    }
    list_for_each_entry(pEntry, &pChn->mCoverList, mList)
    {
        if(RgnHandle == pEntry->mRgnHandle)
        {
            aloge("fatal error! RgnHandle[%d] is already exist!", RgnHandle);
            ret = ERR_VENC_EXIST;
            goto _err0;
        }
    }
    ChannelRegionInfo *pRegion = ChannelRegionInfo_Construct();
    if(NULL == pRegion)
    {
        aloge("fatal error! malloc fail!");
        ret = ERR_VI_NOMEM;
        goto _err0;
    }
    pRegion->mRgnHandle = RgnHandle;
    pRegion->mRgnAttr = *pRgnAttr;
    pRegion->mRgnChnAttr = *pRgnChnAttr;
    if(pBmp)
    {
        pRegion->mbSetBmp = TRUE;
        pRegion->mBmp = *pBmp;
    }
    else
    {
        pRegion->mbSetBmp = FALSE;
    }
    //#define OSD_NEED_SORT
    if(OVERLAY_RGN == pRegion->mRgnAttr.enType)
    {
        //sort from small priority to large priority.
        if(!list_empty(&pChn->mOverlayList))
        {
            BOOL bInsert = FALSE;
            ChannelRegionInfo *pEntry;
            list_for_each_entry(pEntry, &pChn->mOverlayList, mList)
            {
                if(TRUE == compareRegionPriority(&pRegion->mRgnChnAttr, &pEntry->mRgnChnAttr))
                {
                    list_add_tail(&pRegion->mList, &pEntry->mList);
                    bInsert = TRUE;
                    break;
                }
            }
            if(!bInsert)
            {
                list_add_tail(&pRegion->mList, &pChn->mOverlayList);
            }
        }
        else
        {
            list_add_tail(&pRegion->mList, &pChn->mOverlayList);
        }
    }
    else if(COVER_RGN == pRegion->mRgnAttr.enType)
    {
        //sort from small priority to large priority.
        if(!list_empty(&pChn->mCoverList))
        {
            BOOL bInsert = FALSE;
            ChannelRegionInfo *pEntry;
            list_for_each_entry(pEntry, &pChn->mCoverList, mList)
            {
                if(TRUE == compareRegionPriority(&pRegion->mRgnChnAttr, &pEntry->mRgnChnAttr))
                {
                    list_add_tail(&pRegion->mList, &pEntry->mList);
                    bInsert = TRUE;
                    break;
                }
            }
            if(!bInsert)
            {
                list_add_tail(&pRegion->mList, &pChn->mCoverList);
            }
        }
        else
        {
            list_add_tail(&pRegion->mList, &pChn->mCoverList);
        }
    }
    else
    {
        aloge("fatal error! unsupport rgnType[0x%x]", pRegion->mRgnAttr.enType);
        if(pRegion->mBmp.mpData)
        {
            pRegion->mBmp.mpData = NULL;
        }
        free(pRegion);
        goto _err0;
    }
    //decide if draw this region
    if(pRegion->mRgnChnAttr.bShow)
    {
        if(OVERLAY_RGN == pRegion->mRgnAttr.enType)
        {
            if(pRegion->mbSetBmp)
            {
                pRegion->mbDraw = TRUE;
            }
            else
            {
                pRegion->mbDraw = FALSE;
            }
        }
        else
        {
            pRegion->mbDraw = TRUE;
        }
    }
    else
    {
        pRegion->mbDraw = FALSE;
    }

    if(pRegion->mbDraw)
    {
        VencOverlayInfoS stVencOsd;
        configVencOsd(pChn, &stVencOsd);
        ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencOsd, (void*)&stVencOsd);
    }
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;

_err0:
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;
}

ERRORTYPE AW_MPI_VENC_DeleteRegion(VENC_CHN VeChn, RGN_HANDLE RgnHandle)
{
    ERRORTYPE ret = SUCCESS;
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    pthread_mutex_lock(&pChn->mRegionLock);
    //find handle
    BOOL bFind = FALSE;
    ChannelRegionInfo *pRegion;
    if(!list_empty(&pChn->mOverlayList))
    {
        list_for_each_entry(pRegion, &pChn->mOverlayList, mList)
        {
            if(RgnHandle == pRegion->mRgnHandle)
            {
                bFind = TRUE;
                break;
            }
        }
    }
    if(FALSE == bFind && !list_empty(&pChn->mCoverList))
    {
        list_for_each_entry(pRegion, &pChn->mCoverList, mList)
        {
            if(RgnHandle == pRegion->mRgnHandle)
            {
                bFind = TRUE;
                break;
            }
        }
    }
    if(FALSE == bFind)
    {
        aloge("fatal error! can't find rgnHandle[%d]", RgnHandle);
        ret = ERR_VENC_UNEXIST;
        goto _err0;
    }
    list_del(&pRegion->mList);
    if(pRegion->mbDraw)
    {
        //need redraw osd
        VencOverlayInfoS stVencOsd;
        configVencOsd(pChn, &stVencOsd);
        ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencOsd, (void*)&stVencOsd);
    }
    ChannelRegionInfo_Destruct(pRegion);
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;

_err0:
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;
}

ERRORTYPE AW_MPI_VENC_UpdateOverlayBitmap(VENC_CHN VeChn, RGN_HANDLE RgnHandle, BITMAP_S *pBitmap)
{
    ERRORTYPE ret = SUCCESS;
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    if(pBitmap->mWidth%16 != 0 || pBitmap->mHeight%16 != 0)
    {
        aloge("fatal error! bmp width[%d] and height[%d] must all 16 align!", pBitmap->mWidth, pBitmap->mHeight);
        return ERR_VENC_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&pChn->mRegionLock);
    //find handle
    BOOL bFind = FALSE;
    ChannelRegionInfo *pRegion;
    list_for_each_entry(pRegion, &pChn->mOverlayList, mList)
    {
        if(RgnHandle == pRegion->mRgnHandle)
        {
            bFind = TRUE;
            break;
        }
    }
    if(FALSE == bFind)
    {
        ret = ERR_VENC_UNEXIST;
        goto _err0;
    }
    if(pRegion->mRgnAttr.enType != OVERLAY_RGN)
    {
        aloge("fatal error! rgn type[0x%x] is not overlay!", pRegion->mRgnAttr.enType);
        ret = ERR_VENC_ILLEGAL_PARAM;
        goto _err0;
    }
    int size0 = 0;
    int size1 = BITMAP_S_GetdataSize(pBitmap);
    if(pRegion->mbSetBmp)
    {
        size0 = BITMAP_S_GetdataSize(&pRegion->mBmp);
        if(size0 != size1)
        {
            aloge("fatal error! bmp size[%d]!=[%d]", size0, size1);
//            pRegion->mBmp.mpData = NULL;
//            pRegion->mbSetBmp = FALSE;
        }
        pRegion->mBmp = *pBitmap;
    }
    if(FALSE == pRegion->mbSetBmp)
    {
        pRegion->mBmp = *pBitmap;
        pRegion->mbSetBmp = TRUE;
    }
    if(pBitmap->mWidth != pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width || pBitmap->mHeight != pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height)
    {
        alogw("Be careful! bitmap size[%dx%d] != region size[%dx%d], need update region size!", pBitmap->mWidth, pBitmap->mHeight, pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width, pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height);
        pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width = pBitmap->mWidth;
        pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height = pBitmap->mHeight;
    }
    //decide if draw this region
    if(pRegion->mRgnChnAttr.bShow)
    {
        pRegion->mbDraw = TRUE;
    }
    else
    {
        pRegion->mbDraw = FALSE;
    }

    if(pRegion->mbDraw)
    {
        VencOverlayInfoS stVencOsd;
        configVencOsd(pChn, &stVencOsd);
        ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencOsd, (void*)&stVencOsd);
    }
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;

_err0:
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;
}

ERRORTYPE AW_MPI_VENC_UpdateRegionChnAttr(VENC_CHN VeChn, RGN_HANDLE RgnHandle, const RGN_CHN_ATTR_S *pRgnChnAttr)
{
    ERRORTYPE ret = SUCCESS;
    if(!(VeChn>=0 && VeChn<VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    pthread_mutex_lock(&pChn->mRegionLock);
    //find handle
    BOOL bFind = FALSE;
    ChannelRegionInfo *pRegion;
    if(OVERLAY_RGN == pRgnChnAttr->enType)
    {
        list_for_each_entry(pRegion, &pChn->mOverlayList, mList)
        {
            if(RgnHandle == pRegion->mRgnHandle)
            {
                bFind = TRUE;
                break;
            }
        }
    }
    else if(COVER_RGN == pRgnChnAttr->enType)
    {
        list_for_each_entry(pRegion, &pChn->mCoverList, mList)
        {
            if(RgnHandle == pRegion->mRgnHandle)
            {
                bFind = TRUE;
                break;
            }
        }
    }
    if(FALSE == bFind)
    {
        ret = ERR_VENC_UNEXIST;
        goto _err0;
    }

    if(OVERLAY_RGN == pRgnChnAttr->enType)
    {
        BOOL bUpdate = FALSE;
        if(pRegion->mRgnChnAttr.bShow != pRgnChnAttr->bShow)
        {
            alogv("bShow change [%d]->[%d]", pRegion->mRgnChnAttr.bShow, pRgnChnAttr->bShow);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X != pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X
            || pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y != pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y)
        {
            alogv("stPoint change [%d,%d]->[%d,%d]", 
                pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X,
                pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y,
                pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X,
                pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y
                );
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha != pRgnChnAttr->unChnAttr.stOverlayChn.mFgAlpha)
        {
            alogv("FgAlpha change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha, pRgnChnAttr->unChnAttr.stOverlayChn.mFgAlpha);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mLayer != pRgnChnAttr->unChnAttr.stOverlayChn.mLayer)
        {
            alogv("overlay priority(mLayer) change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mLayer, pRgnChnAttr->unChnAttr.stOverlayChn.mLayer);
            //need re-arrange pRegion's position in overlay list
            pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mLayer = pRgnChnAttr->unChnAttr.stOverlayChn.mLayer;
            if(!list_is_singular(&pChn->mOverlayList))
            {
                list_del(&pRegion->mList);
                BOOL bInsert = FALSE;
                ChannelRegionInfo *pEntry;
                list_for_each_entry(pEntry, &pChn->mOverlayList, mList)
                {
                    if(TRUE == compareRegionPriority(&pRegion->mRgnChnAttr, &pEntry->mRgnChnAttr))
                    {
                        list_add_tail(&pRegion->mList, &pEntry->mList);
                        bInsert = TRUE;
                        break;
                    }
                }
                if(!bInsert)
                {
                    list_add_tail(&pRegion->mList, &pChn->mOverlayList);
                }
            }
            bUpdate = TRUE;
        }
        pRegion->mRgnChnAttr = *pRgnChnAttr;
        //decide if draw this region
        if(pRegion->mRgnChnAttr.bShow && pRegion->mbSetBmp)
        {
            pRegion->mbDraw = TRUE;
        }
        else
        {
            pRegion->mbDraw = FALSE;
        }
        if(bUpdate)
        {
            if(pRegion->mbSetBmp)
            {
                VencOverlayInfoS stVencOsd;
                configVencOsd(pChn, &stVencOsd);
                ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencOsd, (void*)&stVencOsd);
            }
        }
    }
    else if(COVER_RGN == pRgnChnAttr->enType)
    {
        BOOL bUpdate = FALSE;
        if(pRegion->mRgnChnAttr.bShow != pRgnChnAttr->bShow)
        {
            alogv("bShow change [%d]->[%d]", pRegion->mRgnChnAttr.bShow, pRgnChnAttr->bShow);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType != pRgnChnAttr->unChnAttr.stCoverChn.enCoverType)
        {
            aloge("fatal error! cover type change [0x%x]->[0x%x]", pRegion->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType, pRgnChnAttr->unChnAttr.stCoverChn.enCoverType);
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.X != pRgnChnAttr->unChnAttr.stCoverChn.stRect.X
            || pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Y != pRgnChnAttr->unChnAttr.stCoverChn.stRect.Y
            || pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Width != pRgnChnAttr->unChnAttr.stCoverChn.stRect.Width
            || pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Height != pRgnChnAttr->unChnAttr.stCoverChn.stRect.Height)
        {
            alogv("cover rect change [%d,%d,%d,%d]->[%d,%d,%d,%d]", 
                pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.X, pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Y,
                pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Width, pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Height,
                pRgnChnAttr->unChnAttr.stCoverChn.stRect.X, pRgnChnAttr->unChnAttr.stCoverChn.stRect.Y,
                pRgnChnAttr->unChnAttr.stCoverChn.stRect.Width, pRgnChnAttr->unChnAttr.stCoverChn.stRect.Height);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mColor != pRgnChnAttr->unChnAttr.stCoverChn.mColor)
        {
            alogv("cover color change [0x%x]->[0x%x]", pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mColor, pRgnChnAttr->unChnAttr.stCoverChn.mColor);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mLayer != pRgnChnAttr->unChnAttr.stCoverChn.mLayer)
        {
            alogv("cover priority(mLayer) change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mLayer, pRgnChnAttr->unChnAttr.stCoverChn.mLayer);
            pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mLayer = pRgnChnAttr->unChnAttr.stCoverChn.mLayer;
            if(!list_is_singular(&pChn->mCoverList))
            {
                list_del(&pRegion->mList);
                BOOL bInsert = FALSE;
                ChannelRegionInfo *pEntry;
                list_for_each_entry(pEntry, &pChn->mCoverList, mList)
                {
                    if(TRUE == compareRegionPriority(&pRegion->mRgnChnAttr, &pEntry->mRgnChnAttr))
                    {
                        list_add_tail(&pRegion->mList, &pEntry->mList);
                        bInsert = TRUE;
                        break;
                    }
                }
                if(!bInsert)
                {
                    list_add_tail(&pRegion->mList, &pChn->mCoverList);
                }
            }
            bUpdate = TRUE;
        }
        pRegion->mRgnChnAttr = *pRgnChnAttr;
        //decide if draw this region
        if(pRegion->mRgnChnAttr.bShow)
        {
            pRegion->mbDraw = TRUE;
        }
        else
        {
            pRegion->mbDraw = FALSE;
        }
        if(bUpdate)
        {
            VencOverlayInfoS stVencOsd;
            configVencOsd(pChn, &stVencOsd);
            ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencOsd, (void*)&stVencOsd);
        }
    }
    else
    {
        aloge("fatal error! rgn type[0x%x]", pRgnChnAttr->enType);
    }
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;

_err0:
    pthread_mutex_unlock(&pChn->mRegionLock);
    return ret;
}

ERRORTYPE AW_MPI_VENC_Set2DFilter(VENC_CHN VeChn, const s2DfilterParam *p2DfilterParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVenc2DFilter, (void*)p2DfilterParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_Get2DFilter(VENC_CHN VeChn, s2DfilterParam *p2DfilterParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVenc2DFilter, p2DfilterParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_Set3DFilter(VENC_CHN VeChn, const s3DfilterParam *p3DfilterParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVenc3DFilter, (void*)p3DfilterParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_Get3DFilter(VENC_CHN VeChn, s3DfilterParam *p3DfilterParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVenc3DFilter, p3DfilterParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetHorizonFlip(VENC_CHN VeChn, BOOL bHorizonFlipFlag)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencHorizonFlip, (void*)&bHorizonFlipFlag);
    return ret;  
}

ERRORTYPE AW_MPI_VENC_GetHorizonFlip(VENC_CHN VeChn, BOOL *bpHorizonFlipFlag)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencHorizonFlip, (void*)bpHorizonFlipFlag);
    return ret; 
}

ERRORTYPE AW_MPI_VENC_SetAdaptiveIntraInP(VENC_CHN VeChn, BOOL bAdaptiveIntraInPFlag)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->SetConfig(pChn->mEncComp, COMP_IndexVendorVencAdaptiveIntraInP, (void*)&bAdaptiveIntraInPFlag);
    return ret;    
}

ERRORTYPE AW_MPI_VENC_SetH264SVCSkip(VENC_CHN VeChn, VencH264SVCSkip *pSVCSkip)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencH264SVCSkip, (void*)pSVCSkip);
    return ret;
}

ERRORTYPE AW_MPI_VENC_EnableNullSkip(VENC_CHN VeChn, BOOL bEnable)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState)
    {
        alogw("Be careful! not call in recommended state[0x%x]!", nState);
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencEnableNullSkip, (void*)&bEnable);
    return ret;    
}

ERRORTYPE AW_MPI_VENC_EnablePSkip(VENC_CHN VeChn, BOOL bEnable)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(COMP_StateIdle != nState)
    {
        alogw("Be careful! not call in recommended state[0x%x]!", nState);
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencEnablePSkip, (void*)&bEnable);
    return ret;
}

ERRORTYPE AW_MPI_VENC_ForbidDiscardingFrame(VENC_CHN VeChn, BOOL bForbid)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencForbidDiscardingFrame, (void*)&bForbid);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetCacheState(VENC_CHN VeChn, CacheState *pCacheState)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = pChn->mEncComp->GetConfig(pChn->mEncComp, COMP_IndexVendorVencCacheState, pCacheState);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetMotionSearchParam(VENC_CHN VeChn, const VencMotionSearchParam *pMotionParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencMotionSearchParam, (void*)pMotionParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetMotionSearchParam(VENC_CHN VeChn, VencMotionSearchParam *pMotionParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencMotionSearchParam, (void*)pMotionParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetMotionSearchResult(VENC_CHN VeChn, VencMotionSearchResult *pMotionResult)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencMotionSearchResult, (void*)pMotionResult);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SaveBsFile(VENC_CHN VeChn, VencSaveBSFile *pSaveParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], save bs file only in executing or idle!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorSaveBSFile, pSaveParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetProcSet(VENC_CHN VeChn, VeProcSet *pVeProcSet)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pChn->mEncComp->GetState(pChn->mEncComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], open ve proc info only in executing or idle!", nState);
        return ERR_VENC_INCORRECT_STATE_OPERATION;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencProcSet, pVeProcSet);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetVe2IspParam(VENC_CHN VeChn, VencVe2IspParam *pParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(nState != COMP_StateExecuting)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencVe2IspParam, (void*)pParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetIsp2VeParam(VENC_CHN VeChn, VencIspMotionParam *pParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(nState != COMP_StateExecuting)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencIsp2VeParam, (void*)pParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_SetWbYuv(VENC_CHN VeChn, sWbYuvParam *pParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    ret = COMP_SetConfig(pChn->mEncComp, COMP_IndexVendorVencSetWbYuv, (void*)pParam);
    return ret;
}

ERRORTYPE AW_MPI_VENC_GetWbYuv(VENC_CHN VeChn, VencThumbInfo *pParam)
{
    if(!(VeChn >= 0 && VeChn < VENC_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid VeChn[%d]!", VeChn);
        return ERR_VENC_INVALID_CHNID;
    }
    VENC_CHN_MAP_S *pChn;
    if(SUCCESS != searchExistChannel(VeChn, &pChn))
    {
        return ERR_VENC_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mEncComp, &nState);
    if(nState != COMP_StateExecuting)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VENC_NOT_PERM;
    }
    ret = COMP_GetConfig(pChn->mEncComp, COMP_IndexVendorVencGetWbYuv, (void*)pParam);
    return ret;
}

