/*
 ******************************************************************************
 *
 * MPI_ISE.h
 *
 * Hawkview ISP - mpi_ise.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   1.0		  yuanxianfeng   2016/07/01		ISE
 *
 *****************************************************************************
 */

#define LOG_TAG "mpi_ise"
#include <utils/plat_log.h>

// ref platform headers
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "cdx_list.h"
#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"

// #include "mm_comm_vi.h"
#include "mm_comm_ise.h"
#include "mm_comm_video.h"
#include "mm_common.h"
#include "mpi_ise.h"

#include "media_common.h"
#include "mm_component.h"
#include "tsemaphore.h"
#include <VideoISECompStream.h>
// #include "vencoder.h"

typedef struct {
    ISE_GRP mIseGrp;          // ISE group id, [0, ISE_MAX_GRP_NUM)
    MM_COMPONENTTYPE *mComp;  // ISE component instance
    cdx_sem_t mSemCompCmd;
    cdx_sem_t mSemAddChn;
    cdx_sem_t mSemRemoveChn;

    MPPCallbackInfo mCallbackInfo;
    struct list_head mList;
} ISE_CHN_GROUP_S;

typedef struct {
    struct list_head mIseGrpList;  // element type: ISE_CHN_GROUP_MAP_S
    pthread_mutex_t mIseGrpListLock;
} IseChnGroupManager;

IseChnGroupManager *gIseGrpMgr = NULL;

static ERRORTYPE addGroup(ISE_CHN_GROUP_S *pGroup)
{
    pthread_mutex_lock(&gIseGrpMgr->mIseGrpListLock);
    list_add_tail(&pGroup->mList, &gIseGrpMgr->mIseGrpList);
    pthread_mutex_unlock(&gIseGrpMgr->mIseGrpListLock);
    return SUCCESS;
}

static ERRORTYPE removeGroup(ISE_CHN_GROUP_S *pGroup)
{
    pthread_mutex_lock(&gIseGrpMgr->mIseGrpListLock);
    list_del(&pGroup->mList);
    pthread_mutex_unlock(&gIseGrpMgr->mIseGrpListLock);
    return SUCCESS;
}

static ISE_CHN_GROUP_S *ISE_CHN_GROUP_S_Construct()
{
    ISE_CHN_GROUP_S *pGroup = (ISE_CHN_GROUP_S *)malloc(sizeof(ISE_CHN_GROUP_S));
    if (NULL == pGroup) {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pGroup, 0, sizeof(ISE_CHN_GROUP_S));
    cdx_sem_init(&pGroup->mSemCompCmd, 0);
    cdx_sem_init(&pGroup->mSemAddChn, 0);
    cdx_sem_init(&pGroup->mSemRemoveChn, 0);
    return pGroup;
}

static void ISE_CHN_GROUP_S_Destruct(ISE_CHN_GROUP_S *pGroup)
{
    if (pGroup->mComp) {
        aloge("fatal error! iseGroup component need free before!");
        COMP_FreeHandle(pGroup->mComp);
        pGroup->mComp = NULL;
    }
    cdx_sem_deinit(&pGroup->mSemCompCmd);
    cdx_sem_deinit(&pGroup->mSemAddChn);
    cdx_sem_deinit(&pGroup->mSemRemoveChn);
    free(pGroup);
}

ERRORTYPE ISE_Construct(void)
{
    ERRORTYPE eError = SUCCESS;
    int ret;
    if (gIseGrpMgr) {
        alogw("mpi_ise module already create!");
        return SUCCESS;
    }
    gIseGrpMgr = malloc(sizeof(IseChnGroupManager));
    if (NULL == gIseGrpMgr) {
        aloge("fatal error! malloc fail!");
        return ERR_ISE_NULL_PTR;
    }
    INIT_LIST_HEAD(&gIseGrpMgr->mIseGrpList);
    ret = pthread_mutex_init(&gIseGrpMgr->mIseGrpListLock, NULL);
    if (ret != 0) {
        aloge("fatal error! mutex init fail");
        eError = ERR_ISE_NOMEM;
        goto _err0;
    }
    return eError;

_err0:
    free(gIseGrpMgr);
    gIseGrpMgr = NULL;
    return eError;
}

ERRORTYPE ISE_Destruct(void)
{
    if (NULL == gIseGrpMgr) {
        alogw("mpi_ise module already NULL!");
        return SUCCESS;
    }
    pthread_mutex_lock(&gIseGrpMgr->mIseGrpListLock);
    if (!list_empty(&gIseGrpMgr->mIseGrpList)) {
        pthread_mutex_unlock(&gIseGrpMgr->mIseGrpListLock);
        aloge("fatal error! ise channel group is not empty!");
        return ERR_ISE_BUSY;
    }
    pthread_mutex_unlock(&gIseGrpMgr->mIseGrpListLock);
    pthread_mutex_destroy(&gIseGrpMgr->mIseGrpListLock);
    free(gIseGrpMgr);
    gIseGrpMgr = NULL;
    return SUCCESS;
}

MM_COMPONENTTYPE *ISE_GetGroupComp(MPP_CHN_S *pMppChn)
{
    MM_COMPONENTTYPE *pComp = NULL;
    ERRORTYPE ret = ERR_ISE_UNEXIST;
    ISE_CHN_GROUP_S *pEntry;
    pthread_mutex_lock(&gIseGrpMgr->mIseGrpListLock);
    list_for_each_entry(pEntry, &gIseGrpMgr->mIseGrpList, mList)
    {
        if (pEntry->mIseGrp == pMppChn->mDevId) {
            pComp = pEntry->mComp;
            ret = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&gIseGrpMgr->mIseGrpListLock);
    return pComp;
}

static ERRORTYPE ISE_searchExistGroup(PARAM_IN ISE_GRP IseGrp, PARAM_OUT ISE_CHN_GROUP_S **ppGroup)
{
    ERRORTYPE ret = ERR_ISE_UNEXIST;
    ISE_CHN_GROUP_S *pEntry;
    pthread_mutex_lock(&gIseGrpMgr->mIseGrpListLock);
    list_for_each_entry(pEntry, &gIseGrpMgr->mIseGrpList, mList)
    {
        if (pEntry->mIseGrp == IseGrp) {
            if (ppGroup) {
                *ppGroup = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&gIseGrpMgr->mIseGrpListLock);
    return ret;
}

static ERRORTYPE VideoIseEventHandler(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN void *pAppData,
                                      PARAM_IN COMP_EVENTTYPE eEvent, PARAM_IN unsigned int nData1,
                                      PARAM_IN unsigned int nData2, PARAM_IN void *pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S iseGroupInfo;
    ret = ((MM_COMPONENTTYPE *)hComponent)->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &iseGroupInfo);
    alogv("iseGroup comp event, iseGroupId[%d][%d][%d]", iseGroupInfo.mModId, iseGroupInfo.mDevId, iseGroupInfo.mChnId);
    ISE_CHN_GROUP_S *pGrp = (ISE_CHN_GROUP_S *)pAppData;

    switch (eEvent) {
        case COMP_EventCmdComplete: {
            if (COMP_CommandStateSet == nData1) {
//                alogv("iseGroup EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pGrp->mSemCompCmd);
                break;
            } else if (COMP_CommandVendorAddChn == nData1) {
                alogv("iseGroup EventCmdComplete, add chn done! result[0x%x]", nData2);
                cdx_sem_up(&pGrp->mSemAddChn);
                break;
            } else if (COMP_CommandVendorRemoveChn == nData1) {
                alogv("iseGroup EventCmdComplete, remove chn done! result[0x%x]", nData2);
                cdx_sem_up(&pGrp->mSemRemoveChn);
                break;
            } else {
                alogw("Low probability! what command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventError: {
            if (ERR_ISE_SAMESTATE == nData1) {
                alogv("set same state to iseGroup!");
                cdx_sem_up(&pGrp->mSemCompCmd);
                break;
            } else if (ERR_ISE_INVALIDSTATE == nData1) {
                aloge("why iseGroup state turn to invalid?");
                break;
            } else if (ERR_ISE_INCORRECT_STATE_TRANSITION == nData1) {
                aloge("fatal error! iseGroup state transition incorrect.");
                break;
            }
            else if (ERR_ISE_ILLEGAL_PARAM == nData1) {
                aloge("fatal error! invalid parameter.");
                return ERR_ISE_ILLEGAL_PARAM;
                break;
            }
            else
            {
                aloge("fatal error! unknown event error[0x%x]!", nData1);
                break;
            }
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
    return SUCCESS;
}

ERRORTYPE VideoIseEmptyBufferDone(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN void *pAppData,
                                  PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{

#if 1
	ISE_CHN_GROUP_S *pChn = (ISE_CHN_GROUP_S*)pAppData;
    VIDEO_FRAME_INFO_S *pFrameInfo =  (VIDEO_FRAME_INFO_S*)pBuffer->pAppPrivate;
    unsigned int frameInfoPort = pBuffer->nFlags;
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_ISE;
    ChannelInfo.mDevId = 0;
    // ChannelInfo.mChnId = pChn->mIseChn;
    CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
    if (0 == frameInfoPort) {/* ISE_PORT_INDEX_CAP0_IN == 0*/
        pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo,
                                MPP_EVENT_RELEASE_ISE_VIDEO_BUFFER0, (void*)pFrameInfo);
    } else if (1 == frameInfoPort) {/* ISE_PORT_INDEX_CAP1_IN == 1*/
        pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo,
                                MPP_EVENT_RELEASE_ISE_VIDEO_BUFFER1, (void*)pFrameInfo);
    }
#endif
    return SUCCESS;
}

COMP_CALLBACKTYPE VideoIseCallback = {
  .EventHandler = VideoIseEventHandler, .EmptyBufferDone = VideoIseEmptyBufferDone, .FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_ISE_CreateGroup(ISE_GRP IseGrp, ISE_GROUP_ATTR_S *pGrpAttr)
{
    if (!((IseGrp >= 0) && (IseGrp < ISE_MAX_GRP_NUM))) {
        aloge("fatal error! invalid IseGrp[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    if (NULL == pGrpAttr) {
        aloge("fatal error! illagal IseAttr!");
        return ERR_ISE_ILLEGAL_PARAM;
    }
    if (SUCCESS == ISE_searchExistGroup(IseGrp, NULL)) {
        return ERR_ISE_EXIST;
    }
    ISE_CHN_GROUP_S *pNode = ISE_CHN_GROUP_S_Construct();
    pNode->mIseGrp = IseGrp;

    // create iseGroup Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE *)&pNode->mComp, CDX_ComponentNameISE, (void *)pNode, &VideoIseCallback);
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_ISE;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mIseGrp;
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorMPPChannelInfo, (void *)&ChannelInfo);
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorIseGroupAttr, (void *)pGrpAttr);
    eRet = pNode->mComp->SendCommand(pNode->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    // create iseGroup Component done!

    addGroup(pNode);
    return SUCCESS;
}

ERRORTYPE AW_MPI_ISE_DestroyGroup(ISE_GRP IseGrp)
{
    ERRORTYPE ret;
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid IseGrp[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE eRet;
    if (pGrp->mComp) {
        COMP_STATETYPE nCompState;
        if (SUCCESS == pGrp->mComp->GetState(pGrp->mComp, &nCompState)) {
            if (nCompState == COMP_StateIdle) {
                eRet = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                cdx_sem_down(&pGrp->mSemCompCmd);
                if (eRet != SUCCESS) {
                    aloge("fatal error! why transmit state to loaded fail?");
                }
            } else if (nCompState == COMP_StateLoaded) {
                eRet = SUCCESS;
            } else if (nCompState == COMP_StateInvalid) {
                alogw("Low probability! Component StateInvalid?");
                eRet = SUCCESS;
            } else {
                aloge("fatal error! invalid VeChn[%d] state[0x%x]!", IseGrp, nCompState);
                eRet = ERR_ISE_INCORRECT_STATE_OPERATION;
            }

            if (eRet == SUCCESS) {
                removeGroup(pGrp);
                COMP_FreeHandle(pGrp->mComp);
                pGrp->mComp = NULL;
                ISE_CHN_GROUP_S_Destruct(pGrp);
            }
            ret = eRet;
        } else {
            aloge("fatal error! GetState fail!");
            ret = ERR_ISE_BUSY;
        }
    } else {
        aloge("fatal error! no iseGroup component!");
        list_del(&pGrp->mList);
        ISE_CHN_GROUP_S_Destruct(pGrp);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_ISE_GetGrpAttr(ISE_GRP IseGrp, ISE_GROUP_ATTR_S *pGrpAttr)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorIseGroupAttr, (void *)pGrpAttr);
    return ret;
}

ERRORTYPE AW_MPI_ISE_SetGrpAttr(ISE_GRP IseGrp, ISE_GROUP_ATTR_S *pGrpAttr)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorIseGroupAttr, (void *)pGrpAttr);
    return ret;
}

ERRORTYPE AW_MPI_ISE_CreatePort(ISE_GRP IseGrp, ISE_CHN IsePort, ISE_CHN_ATTR_S *pChnAttr)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }

    if (!(IsePort >= 0 && IsePort < ISE_MAX_CHN_NUM)) {
        aloge("fatal error! invalid IsePort[%d]!", IsePort);
        return ERR_ISE_INVALID_CHNID;
    }

    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    IseChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(IseChnAttr));
    chnAttr.mGrpId = IseGrp;
    chnAttr.mChnId = IsePort;

    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorIseChnAttr, &chnAttr);  // get default attr

    if (ERR_ISE_UNEXIST == ret) {
        chnAttr.pChnAttr = pChnAttr;
//        ret = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandVendorAddChn, 0, &chnAttr);
//        cdx_sem_down(&pGrp->mSemAddChn);
        ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorIseAddChn, &chnAttr);
        return ret;
    } else if (SUCCESS == ret) {
        alogd("iseChn[%d] of group[%d] is exist!", IsePort, IseGrp);
        return ERR_ISE_EXIST;
    } else {
        aloge("fatal error! add chn[%d] of group[%d] fail[0x%x]!", IsePort, IseGrp, ret);
        return ret;
    }
    return 0;
}

ERRORTYPE AW_MPI_ISE_DestroyPort(ISE_GRP IseGrp, ISE_CHN IsePort)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    if (!(IsePort >= 0 && IsePort < ISE_MAX_CHN_NUM)) {
        aloge("fatal error! invalid IsePort[%d]!", IsePort);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    IseChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(IseChnAttr));
    chnAttr.mGrpId = IseGrp;
    chnAttr.mChnId = IsePort;
    chnAttr.pChnAttr = NULL;
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorIseChnAttr, &chnAttr);
    if (SUCCESS == ret) {
//        ret = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandVendorRemoveChn, IsePort, &chnAttr);
//        cdx_sem_down(&pGrp->mSemRemoveChn);
        ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorIseRemoveChn, &chnAttr);
        return ret;
    } else if (ERR_ISE_UNEXIST == ret) {
        alogd("iseChn[%d] of group[%d] is unexist!", IsePort, IseGrp);
        return ERR_ISE_UNEXIST;
    } else {
        aloge("fatal error! remove chn[%d] of group[%d] fail[0x%x]!", IsePort, IseGrp, ret);
        return ret;
    }
    return 0;
}

ERRORTYPE AW_MPI_ISE_GetPortAttr(ISE_GRP IseGrp, ISE_CHN IsePort, ISE_CHN_ATTR_S *pChnAttr)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    if (!(IsePort >= 0 && IsePort < ISE_MAX_CHN_NUM)) {
        aloge("fatal error! invalid IsePort[%d]!", IsePort);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    IseChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(IseChnAttr));
    chnAttr.mGrpId = IseGrp;
    chnAttr.mChnId = IsePort;
    chnAttr.pChnAttr = pChnAttr;
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorIseChnAttr, &chnAttr);
    return ret;
}

ERRORTYPE AW_MPI_ISE_SetPortAttr(ISE_GRP IseGrp, ISE_CHN IsePort, ISE_CHN_ATTR_S *pChnAttr)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    if (!(IsePort >= 0 && IsePort < ISE_MAX_CHN_NUM)) {
        aloge("fatal error! invalid IsePort[%d]!", IsePort);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    IseChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(IseChnAttr));
    chnAttr.mGrpId = IseGrp;
    chnAttr.mChnId = IsePort;
    chnAttr.pChnAttr = pChnAttr;
    ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorIseChnAttr, &chnAttr);
    return ret;
}

ERRORTYPE AW_MPI_ISE_Start(ISE_GRP IseGrp)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pGrp->mComp->GetState(pGrp->mComp, &nCompState);
    if (COMP_StateIdle == nCompState) {
        eRet = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        cdx_sem_down(&pGrp->mSemCompCmd);
        ret = SUCCESS;
    } else {
        alogd("iseGroup comp state[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_ISE_Stop(ISE_GRP IseGrp)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid iseGroup[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    int eError;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pGrp->mComp->GetState(pGrp->mComp, &nCompState);
    if (COMP_StateExecuting == nCompState || COMP_StatePause == nCompState) {
        eRet = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        cdx_sem_down(&pGrp->mSemCompCmd);
        eError = eRet;
    } else if (COMP_StateIdle == nCompState) {
        alogv("iseGroup comp state[0x%x], do nothing!", nCompState);
        eError = SUCCESS;
    } else {
        aloge("fatal error! check iseGroup state[0x%x]!", nCompState);
        eError = SUCCESS;
    }
    return eError;
}

ERRORTYPE AW_MPI_ISE_GetData(ISE_GRP IseGrp, ISE_CHN IsePort, VIDEO_FRAME_INFO_S *pstIseData, AW_S32 s32MilliSec)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid IseGrp[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    if (!(IsePort >= 0 && IsePort < ISE_MAX_CHN_NUM)) {
        aloge("fatal error! invalid IsePort[%d]!", IsePort);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret = -1;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    ISE_DATA_S iseData;
    iseData.mIseGrp = IseGrp;
    iseData.mIseChn = IsePort;
    iseData.nMilliSec = s32MilliSec;
    iseData.frame = pstIseData;

    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorGetIseData, (void *)&iseData);
    return ret;
}
ERRORTYPE AW_MPI_ISE_ReleaseData(ISE_GRP IseGrp, ISE_CHN IsePort, VIDEO_FRAME_INFO_S *pstIseData)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid IseGrp[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    if (!(IsePort >= 0 && IsePort < ISE_MAX_CHN_NUM)) {
        aloge("fatal error! invalid IsePort[%d]!", IsePort);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    ISE_DATA_S iseData;
    iseData.mIseGrp = IseGrp;
    iseData.mIseChn = IsePort;
    iseData.frame = pstIseData;
    ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorReleaseIseData, (void *)&iseData);
    return ret;
}

ERRORTYPE AW_MPI_ISE_SendPic(ISE_GRP IseGrp, VIDEO_FRAME_INFO_S *pstUserFrame0, VIDEO_FRAME_INFO_S *pstUserFrame1,
                             AW_S32 s32MilliSec)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid VeChn[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    COMP_BUFFERHEADERTYPE bufferHeader;
    bufferHeader.pAppPrivate = (void *)pstUserFrame0;       // camera0
    bufferHeader.pPlatformPrivate = (void *)pstUserFrame1;  // camera1
    ret = pGrp->mComp->EmptyThisBuffer(pGrp->mComp, &bufferHeader);

    return ret;
}

ERRORTYPE AW_MPI_ISE_RegisterCallback(ISE_GRP IseGrp, MPPCallbackInfo *pCallback)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid IseGrp[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    pGrp->mCallbackInfo.callback = pCallback->callback;
    pGrp->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_ISE_SetISEFreq(ISE_GRP IseGrp, int nFreq)
{
    if (!(IseGrp >= 0 && IseGrp < ISE_MAX_GRP_NUM)) {
        aloge("fatal error! invalid IseGrp[%d]!", IseGrp);
        return ERR_ISE_INVALID_CHNID;
    }
    ISE_CHN_GROUP_S *pGrp;
    if (SUCCESS != ISE_searchExistGroup(IseGrp, &pGrp)) {
        return ERR_ISE_UNEXIST;
    }
    int ISEFreq = nFreq;
    if(ISEFreq != 300)
    {
        aloge("fatal error! invalid frequency %dM",ISEFreq);
        return ERR_ISE_ILLEGAL_PARAM;
    }

    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_ISE_NOT_PERM;
    }
    ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorIseFreq, (void*)&ISEFreq);
    return ret;
}

