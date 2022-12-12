/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_venc.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/23
  Last Modified :
  Description   : mpi functions implement for mux module.
  Function List :
  History       :
******************************************************************************/
#define LOG_TAG "mpi_mux"
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
#include "mm_comm_mux.h"
#include "mpi_mux.h"

//media internal common headers.
#include <tsemaphore.h>
#include <media_common.h>
#include <mm_component.h>
#include <mpi_mux_common.h>
#include <RecRender_Component.h>

//static LIST_HEAD(gMuxChnGrpMapList);    //element type: MUX_CHN_GROUP_MAP_S
typedef struct
{
    struct list_head mMuxChnGrpList;   //element type: MUX_CHN_GROUP_MAP_S
    pthread_mutex_t mMuxChnGrpListLock;
}MuxChnGroupManager;

MuxChnGroupManager *gMuxGrpMgr = NULL;

typedef struct MUX_CHN_GROUP_S
{
    MUX_GRP             mMuxGrp;    // mux group id, [0, MUX_MAX_GRP_NUM)
    MM_COMPONENTTYPE    *mComp;  // mux component instance
    cdx_sem_t           mSemCompCmd;
    cdx_sem_t           mSemAddChn;
    cdx_sem_t           mSemRemoveChn;
    MPPCallbackInfo     mCallbackInfo;
    struct list_head    mList;
}MUX_CHN_GROUP_S;

static ERRORTYPE MUX_searchExistGroup_l(PARAM_IN MUX_GRP muxGrp, PARAM_OUT MUX_CHN_GROUP_S **ppGroup)
{
    ERRORTYPE ret = ERR_MUX_UNEXIST;
    MUX_CHN_GROUP_S* pEntry;
    list_for_each_entry(pEntry, &gMuxGrpMgr->mMuxChnGrpList, mList)
    {
        if(pEntry->mMuxGrp == muxGrp)
        {
            if(ppGroup)
            {
                *ppGroup = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

static ERRORTYPE MUX_searchExistGroup(PARAM_IN MUX_GRP muxGrp, PARAM_OUT MUX_CHN_GROUP_S **ppGroup)
{
    ERRORTYPE ret = ERR_MUX_UNEXIST;
    MUX_CHN_GROUP_S* pEntry;
    pthread_mutex_lock(&gMuxGrpMgr->mMuxChnGrpListLock);
    ret = MUX_searchExistGroup_l(muxGrp, ppGroup);
    pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
    return ret;
}

static ERRORTYPE addGroup_l(MUX_CHN_GROUP_S *pGroup)
{
    list_add_tail(&pGroup->mList, &gMuxGrpMgr->mMuxChnGrpList);
    return SUCCESS;
}

static ERRORTYPE addGroup(MUX_CHN_GROUP_S *pGroup)
{
    pthread_mutex_lock(&gMuxGrpMgr->mMuxChnGrpListLock);
    list_add_tail(&pGroup->mList, &gMuxGrpMgr->mMuxChnGrpList);
    pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
    return SUCCESS;
}

static ERRORTYPE removeGroup(MUX_CHN_GROUP_S *pGroup)
{
    pthread_mutex_lock(&gMuxGrpMgr->mMuxChnGrpListLock);
    list_del(&pGroup->mList);
    pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
    return SUCCESS;
}

static MUX_CHN_GROUP_S* MUX_CHN_GROUP_S_Construct()
{
    MUX_CHN_GROUP_S *pGroup = (MUX_CHN_GROUP_S*)malloc(sizeof(MUX_CHN_GROUP_S));
    if(NULL == pGroup)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pGroup, 0, sizeof(MUX_CHN_GROUP_S));
    cdx_sem_init(&pGroup->mSemCompCmd, 0);
    cdx_sem_init(&pGroup->mSemAddChn, 0);
    cdx_sem_init(&pGroup->mSemRemoveChn, 0);
    return pGroup;
}

static void MUX_CHN_GROUP_S_Destruct(MUX_CHN_GROUP_S *pGroup)
{
    if(pGroup->mComp)
    {
        aloge("fatal error! muxGroup component need free before!");
        COMP_FreeHandle(pGroup->mComp);
        pGroup->mComp = NULL;
    }
    cdx_sem_deinit(&pGroup->mSemCompCmd);
    cdx_sem_deinit(&pGroup->mSemAddChn);
    cdx_sem_deinit(&pGroup->mSemRemoveChn);
    free(pGroup);
}

ERRORTYPE MUX_Construct(void)
{
    ERRORTYPE eError = SUCCESS;
    int ret;
    if(gMuxGrpMgr)
    {
        alogw("mpi_mux module already create!");
        return SUCCESS;
    }
    gMuxGrpMgr = malloc(sizeof(MuxChnGroupManager));
    if(NULL == gMuxGrpMgr)
    {
        aloge("fatal error! malloc fail!");
        return ERR_MUX_NULL_PTR;
    }
    INIT_LIST_HEAD(&gMuxGrpMgr->mMuxChnGrpList);
    ret = pthread_mutex_init(&gMuxGrpMgr->mMuxChnGrpListLock, NULL);
	if (ret!=0)
	{
        aloge("fatal error! mutex init fail");
        eError = ERR_MUX_NOMEM;
        goto _err0;
	}
    return eError;

_err0:
    free(gMuxGrpMgr);
    gMuxGrpMgr = NULL;
    return eError;

}

ERRORTYPE MUX_Destruct(void)
{
    if(NULL == gMuxGrpMgr)
    {
        alogw("mpi_mux module already NULL!");
        return SUCCESS;
    }
    pthread_mutex_lock(&gMuxGrpMgr->mMuxChnGrpListLock);
    if(!list_empty(&gMuxGrpMgr->mMuxChnGrpList))
    {
        pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
        aloge("fatal error! mux channel group is not empty!");
        return ERR_MUX_BUSY;
    }
    pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
    pthread_mutex_destroy(&gMuxGrpMgr->mMuxChnGrpListLock);
    free(gMuxGrpMgr);
    gMuxGrpMgr = NULL;
    return SUCCESS;

}

MM_COMPONENTTYPE *MUX_GetGroupComp(PARAM_IN MPP_CHN_S *pMppchn)
{
    MUX_CHN_GROUP_S* pGroup;

    if (MUX_searchExistGroup(pMppchn->mChnId, &pGroup) != SUCCESS)
    {
        return NULL;
    }
    return pGroup->mComp;
}

static ERRORTYPE RecRenderEventHandler(
	 PARAM_IN COMP_HANDLETYPE hComponent,
	 PARAM_IN void* pAppData,
	 PARAM_IN COMP_EVENTTYPE eEvent,
	 PARAM_IN unsigned int nData1,
	 PARAM_IN unsigned int nData2,
	 PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MPP_CHN_S muxGroupInfo;
    ret = ((MM_COMPONENTTYPE*)hComponent)->GetConfig(hComponent, COMP_IndexVendorMPPChannelInfo, &muxGroupInfo);
    if(SUCCESS == ret)
    {
        alogv("muxGroup comp event, muxGroupId[%d][%d][%d]", muxGroupInfo.mModId, muxGroupInfo.mDevId, muxGroupInfo.mChnId);
    }
    MUX_CHN_GROUP_S *pGrp = (MUX_CHN_GROUP_S*)pAppData;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("muxGroup EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pGrp->mSemCompCmd);
                break;
            }
            else if(COMP_CommandVendorAddChn == nData1)
            {
                alogv("muxGroup EventCmdComplete, add chn done! result[0x%x]", nData2);
                cdx_sem_up(&pGrp->mSemAddChn);
                break;
            }
            else if(COMP_CommandVendorRemoveChn == nData1)
            {
                alogv("muxGroup EventCmdComplete, remove chn done! result[0x%x]", nData2);
                cdx_sem_up(&pGrp->mSemRemoveChn);
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
            if(ERR_MUX_SAMESTATE == nData1)
            {
                alogv("set same state to muxGroup!");
                cdx_sem_up(&pGrp->mSemCompCmd);
                break;
            }
            else if(ERR_MUX_INVALIDSTATE == nData1)
            {
                aloge("why muxGroup state turn to invalid?");
                break;
            }
            else if(ERR_MUX_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! muxGroup state transition incorrect.");
                break;
            }
            else
            {
                aloge("fatal error! other param[%d][%d]", nData1, nData2);
                break;
            }
        }
        case COMP_EventNeedNextFd:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_MUX;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pGrp->mMuxGrp;
            CHECK_MPP_CALLBACK(pGrp->mCallbackInfo.callback);
            pGrp->mCallbackInfo.callback(pGrp->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_NEED_NEXT_FD, (void*)&nData1);
            break;
        }
        case COMP_EventRecordDone:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_MUX;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pGrp->mMuxGrp;
            CHECK_MPP_CALLBACK(pGrp->mCallbackInfo.callback);
            pGrp->mCallbackInfo.callback(pGrp->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_RECORD_DONE, (void*)&nData1);
            break;
        }
        case COMP_EventBsframeAvailable:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_MUX;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pGrp->mMuxGrp;
            CHECK_MPP_CALLBACK(pGrp->mCallbackInfo.callback);
            pGrp->mCallbackInfo.callback(pGrp->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_BSFRAME_AVAILABLE, pEventData);
            break;
        }
        case COMP_EventWriteDiskError:
        {
            MPP_CHN_S ChannelInfo;
            ChannelInfo.mModId = MOD_ID_MUX;
            ChannelInfo.mDevId = 0;
            ChannelInfo.mChnId = pGrp->mMuxGrp;
            CHECK_MPP_CALLBACK(pGrp->mCallbackInfo.callback);
            pGrp->mCallbackInfo.callback(pGrp->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_WRITE_DISK_ERROR, (void*)&nData1);
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
    return SUCCESS;
}

COMP_CALLBACKTYPE RecRenderCallback = {
	.EventHandler = RecRenderEventHandler,
	.EmptyBufferDone = NULL,
	.FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_MUX_CreateGrp(MUX_GRP muxGrp, MUX_GRP_ATTR_S *pGrpAttr)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGrp[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    if(NULL == pGrpAttr)
    {
        aloge("fatal error! illagal VencAttr!");
        return ERR_MUX_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&gMuxGrpMgr->mMuxChnGrpListLock);
    if(SUCCESS == MUX_searchExistGroup_l(muxGrp, NULL))
    {
        pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
        return ERR_MUX_EXIST;
    }

    MUX_CHN_GROUP_S *pNode = MUX_CHN_GROUP_S_Construct();
    pNode->mMuxGrp = muxGrp;

    //create muxGroup Component here...
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mComp, CDX_ComponentNameMuxer, (void*)pNode, &RecRenderCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_MUX;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mMuxGrp;
    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);

    eRet = pNode->mComp->SetConfig(pNode->mComp, COMP_IndexVendorMuxGroupAttr, (void*)pGrpAttr);
    eRet = pNode->mComp->SendCommand(pNode->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create muxGroup Component done!

    addGroup_l(pNode);
    pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_MUX_DestroyGrpEx(MUX_GRP muxGrp, BOOL bShutDownNowFlag)
{
    ERRORTYPE ret;
    if(!(muxGrp >= 0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGrp[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE eRet;
    if(pGrp->mComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == pGrp->mComp->GetState(pGrp->mComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            {
                ShutDownType nShutDownType;
                nShutDownType.mMuxerId = -1; // for all muxerId
                nShutDownType.mbShutDownNowFlag = bShutDownNowFlag;
                pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorMuxShutDownType, (void *)&nShutDownType);
                eRet = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                cdx_sem_down(&pGrp->mSemCompCmd);
                if(eRet != SUCCESS)
                {
                    aloge("fatal error! why transmit state to loaded fail?");
                }
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
                aloge("fatal error! invalid VeChn[%d] state[0x%x]!", muxGrp, nCompState);
                eRet = ERR_MUX_INCORRECT_STATE_OPERATION;
            }

            if(eRet == SUCCESS)
            {
                removeGroup(pGrp);
                COMP_FreeHandle(pGrp->mComp);
                pGrp->mComp = NULL;
                MUX_CHN_GROUP_S_Destruct(pGrp);
            }
            ret = eRet;
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_MUX_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no muxGroup component!");
        list_del(&pGrp->mList);
        MUX_CHN_GROUP_S_Destruct(pGrp);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_MUX_DestroyGrp(MUX_GRP muxGrp)
{
    return AW_MPI_MUX_DestroyGrpEx(muxGrp, FALSE);
}


ERRORTYPE AW_MPI_MUX_StartGrp(MUX_GRP muxGrp)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pGrp->mComp->GetState(pGrp->mComp, &nCompState);
    if(SUCCESS == eRet && COMP_StateIdle == nCompState)
    {
        eRet = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        cdx_sem_down(&pGrp->mSemCompCmd);
        ret = SUCCESS;
    }
    else
    {
        alogd("muxGroup comp state[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_MUX_StopGrp(MUX_GRP muxGrp)
{
    if(!(muxGrp>=0 && muxGrp<MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid mux group[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    int eError;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pGrp->mComp->GetState(pGrp->mComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        eRet = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        cdx_sem_down(&pGrp->mSemCompCmd);
        eError = eRet;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogv("muxGroup comp state[0x%x], do nothing!", nCompState);
        eError = SUCCESS;
    }
    else
    {
        aloge("fatal error! check muxGroup state[0x%x]!", nCompState);
        eError = SUCCESS;
    }
    return eError;
}

ERRORTYPE AW_MPI_MUX_GetGrpAttr(MUX_GRP muxGrp, MUX_GRP_ATTR_S *pGrpAttr)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxGroupAttr, (void*)&pGrpAttr);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SetGrpAttr(MUX_GRP muxGrp, MUX_GRP_ATTR_S *pGrpAttr)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorMuxGroupAttr, (void*)pGrpAttr);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SetH264SpsPpsInfo(MUX_GRP muxGrp, VENC_CHN VeChn, VencHeaderData *pH264SpsPpsInfo)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    VencHeaderDataParam stVencHeaderDataParam;
    stVencHeaderDataParam.mVeChn = VeChn;
    stVencHeaderDataParam.mH264SpsPpsInfo.nLength = pH264SpsPpsInfo->nLength;
    stVencHeaderDataParam.mH264SpsPpsInfo.pBuffer = pH264SpsPpsInfo->pBuffer;
    ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorExtraData, (void*)&stVencHeaderDataParam);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SetH265SpsPpsInfo(MUX_GRP muxGrp, VENC_CHN VeChn, VencHeaderData *pH265SpsPpsInfo)
{
    return AW_MPI_MUX_SetH264SpsPpsInfo(muxGrp, VeChn, pH265SpsPpsInfo);
}

ERRORTYPE AW_MPI_MUX_CreateChn(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr, int nFd)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        pthread_mutex_unlock(&gMuxGrpMgr->mMuxChnGrpListLock);
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(MuxChnAttr));
    chnAttr.mGrpId = muxGrp;
    chnAttr.nChnId = muxChn;
    chnAttr.pChnAttr = NULL;
    ret = COMP_GetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
    if(ERR_MUX_UNEXIST == ret)
    {
        chnAttr.mGrpId = muxGrp;
        chnAttr.nChnId = muxChn;
        chnAttr.pChnAttr = pChnAttr;
        ret = pGrp->mComp->SendCommand(pGrp->mComp, COMP_CommandVendorAddChn, nFd, &chnAttr);
        cdx_sem_down(&pGrp->mSemAddChn);
        return ret;
    }
    else if(SUCCESS == ret)
    {
        alogv("muxChn[%d] of group[%d] is exist!", muxChn, muxGrp);
        return ERR_MUX_EXIST;
    }
    else
    {
        aloge("fatal error! add chn[%d] of group[%d] fail[0x%x]!", muxChn, muxGrp, ret);
        return ret;
    }
}

ERRORTYPE AW_MPI_MUX_DestroyChnEx(MUX_GRP muxGrp, MUX_CHN muxChn, BOOL bShutDownNowFlag)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(MuxChnAttr));
    MUX_CHN_ATTR_S nMuxChnAttr;
    memset(&nMuxChnAttr, 0, sizeof nMuxChnAttr);
    chnAttr.nChnId = muxChn;
    chnAttr.pChnAttr = &nMuxChnAttr;
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
    if(SUCCESS == ret)
    {
        ShutDownType nShutDownType;
        nShutDownType.mMuxerId = chnAttr.pChnAttr->mMuxerId;
        nShutDownType.mbShutDownNowFlag = bShutDownNowFlag;
        pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorMuxShutDownType, (void *)&nShutDownType);
        ret = COMP_SendCommand(pGrp->mComp, COMP_CommandVendorRemoveChn, muxChn, NULL);
        cdx_sem_down(&pGrp->mSemRemoveChn);
        return ret;
    }
    else if(ERR_MUX_UNEXIST == ret)
    {
        alogd("muxChn[%d] of group[%d] is unexist!", muxChn, muxGrp);
        return ERR_MUX_UNEXIST;
    }
    else
    {
        aloge("fatal error! remove chn[%d] of group[%d] fail[0x%x]!", muxChn, muxGrp, ret);
        return ret;
    }
}

ERRORTYPE AW_MPI_MUX_DestroyChn(MUX_GRP muxGrp, MUX_CHN muxChn)
{
    return AW_MPI_MUX_DestroyChnEx(muxGrp, muxChn, FALSE);
}


ERRORTYPE AW_MPI_MUX_GetChnAttr(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(MuxChnAttr));
    chnAttr.nChnId = muxChn;
    chnAttr.pChnAttr = pChnAttr;
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SetChnAttr(MUX_GRP muxGrp, MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(MuxChnAttr));
    chnAttr.nChnId = muxChn;
    chnAttr.pChnAttr = NULL;
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
    if(SUCCESS == ret)
    {
        chnAttr.nChnId = muxChn;
        chnAttr.pChnAttr = pChnAttr;
        ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
        return ret;
    }
    else if(ERR_MUX_UNEXIST == ret)
    {
        alogd("muxChn[%d] of group[%d] is unexist!", muxChn, muxGrp);
        return ERR_MUX_UNEXIST;
    }
    else
    {
        aloge("fatal error! setChnAttr chn[%d] of group[%d] fail[0x%x]!", muxChn, muxGrp, ret);
        return ret;
    }
}


ERRORTYPE AW_MPI_MUX_SetThmPic(MUX_GRP muxGrp, MUX_CHN muxChn, char *p_thm_pic, int thm_pic_size)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(MuxChnAttr));
    chnAttr.nChnId = muxChn;
    MUX_CHN_ATTR_S stMppChnAttr;
    chnAttr.pChnAttr = &stMppChnAttr;
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
    if(SUCCESS == ret)
    {
        THM_INFO thm_pic_info;
        thm_pic_info.thm_pic.p_thm_pic_addr = p_thm_pic;
        thm_pic_info.thm_pic.thm_pic_size = thm_pic_size;
        thm_pic_info.mMuxerId = stMppChnAttr.mMuxerId;
        ret = pGrp->mComp->SetConfig(pGrp->mComp,COMP_IndexVendorMuxSetThmPic , (void*)&thm_pic_info);
        return ret;
    }
    else
    {
        aloge("fatal error! not find MuxChannel group[%d] channelId[%d]", muxGrp, muxChn);
        return ERR_MUX_UNEXIST;
    }
}
ERRORTYPE AW_MPI_MUX_SwitchFd(MUX_GRP muxGrp, MUX_CHN muxChn, int fd, int nFallocateLen)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(MuxChnAttr));
    chnAttr.nChnId = muxChn;
    MUX_CHN_ATTR_S stMppChnAttr;
    chnAttr.pChnAttr = &stMppChnAttr;
    ret = pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
    if(SUCCESS == ret)
    {
        CdxFdT stCdxFd;
        stCdxFd.mFd = fd;
        stCdxFd.mnFallocateLen = nFallocateLen;
        //stCdxFd.mIsImpact = 0;
        stCdxFd.mMuxerId = stMppChnAttr.mMuxerId;
        ret = pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorMuxSwitchFd, (void*)&stCdxFd);
        return ret;
    }
    else
    {
        aloge("fatal error! not find MuxChannel group[%d] channelId[%d]", muxGrp, muxChn);
        return ERR_MUX_UNEXIST;
    }
}

ERRORTYPE AW_MPI_MUX_SwitchFileNormal(MUX_GRP muxGrp, MUX_CHN muxChn, int fd, int nFallocateLen, BOOL bIncludeCache)
{
    if(!(muxGrp>=0 && muxGrp < MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxChnAttr chnAttr;
    memset(&chnAttr, 0, sizeof(MuxChnAttr));
    chnAttr.nChnId = muxChn;
    MUX_CHN_ATTR_S stMppChnAttr;
    chnAttr.pChnAttr = &stMppChnAttr;
    ret = COMP_GetConfig(pGrp->mComp, COMP_IndexVendorMuxChnAttr, &chnAttr);
    if(SUCCESS == ret)
    {
        //ret = COMP_SendCommand(pGrp->mComp, COMP_CommandSwitchFileNormal, stMppChnAttr.mMuxerId, NULL);
        SwitchFileNormalInfo stInfo;
        memset(&stInfo, 0, sizeof(SwitchFileNormalInfo));
        stInfo.mMuxerId = stMppChnAttr.mMuxerId;
        stInfo.mFd = fd;
        stInfo.mnFallocateLen = nFallocateLen;
        stInfo.mbIncludeCache = bIncludeCache;
        ret = COMP_SetConfig(pGrp->mComp, COMP_IndexSwitchFileNormal, (void*)&stInfo);
        return ret;
    }
    else
    {
        aloge("fatal error! not find MuxChannel group[%d] channelId[%d]", muxGrp, muxChn);
        return ERR_MUX_UNEXIST;
    }
}

ERRORTYPE AW_MPI_MUX_RegisterCallback(MUX_GRP muxGrp, MPPCallbackInfo *pCallback)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    pGrp->mCallbackInfo.callback = pCallback->callback;
    pGrp->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

ERRORTYPE AW_MPI_MUX_GetCacheStatus(MUX_GRP muxGrp, CacheState *pCacheState)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    return pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxCacheState, pCacheState);
}

ERRORTYPE AW_MPI_MUX_SetMuxCacheDuration(MUX_GRP muxGrp, int nCacheMs)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        aloge("fatal error! ERR_MUX_UNEXIST!");
        return ERR_MUX_UNEXIST;
    }

    int64_t mCacheMs = nCacheMs; //unit:ms
    return COMP_SetConfig(pGrp->mComp, COMP_IndexVendorMuxCacheDuration, &mCacheMs);
}

ERRORTYPE AW_MPI_MUX_SetMuxCacheStrmIds(MUX_GRP muxGrp, unsigned int nStreamIds)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        aloge("fatal error! ERR_MUX_UNEXIST!");
        return ERR_MUX_UNEXIST;
    }

    return COMP_SetConfig(pGrp->mComp, COMP_IndexVendorMuxCacheStrmIds, &nStreamIds);
}
ERRORTYPE AW_MPI_MUX_SetSwitchFileDurationPolicy(MUX_GRP muxGrp, MUX_CHN muxChn, RecordFileDurationPolicy ePolicy)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    return pGrp->mComp->SetConfig(pGrp->mComp, COMP_IndexVendorMuxSwitchPolicy, (void *)&ePolicy);
}

ERRORTYPE AW_MPI_MUX_GetSwitchFileDurationPolicy(MUX_GRP muxGrp, MUX_CHN muxChn, RecordFileDurationPolicy *pPolicy)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = pGrp->mComp->GetState(pGrp->mComp, &nState);
    if(COMP_StateIdle != nState || COMP_StateExecuting != nState || COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    return pGrp->mComp->GetConfig(pGrp->mComp, COMP_IndexVendorMuxSwitchPolicy, (void *)pPolicy);

}

ERRORTYPE AW_MPI_MUX_SetSdCardState(MUX_GRP muxGrp, BOOL bExist)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    int bState = (int)bExist;
    return COMP_SetConfig(pGrp->mComp, COMP_IndexVendorSdcardState, (void*)&bState);
}

ERRORTYPE AW_MPI_MUX_SetVeChnBindStreamId(MUX_GRP muxGrp, VENC_CHN VeChn, int nStreamId)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pGrp->mComp, &nState);
    if(nState != COMP_StateIdle)
    {
        aloge("fatal error! wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    VeChnBindStreamIdNode stVeChnBindStreamId;
    memset(&stVeChnBindStreamId, 0, sizeof(stVeChnBindStreamId));
    stVeChnBindStreamId.mStreamId = nStreamId;
    stVeChnBindStreamId.mVeChn.mModId = MOD_ID_VENC;
    stVeChnBindStreamId.mVeChn.mDevId = 0;
    stVeChnBindStreamId.mVeChn.mChnId = VeChn;
    return COMP_SetConfig(pGrp->mComp, COMP_IndexVendorMuxVeChnBindStreamId, &stVeChnBindStreamId);
}

static ERRORTYPE aw_mux_SendStream(MUX_GRP muxGrp, void *pStream, int nStreamId, int nTimeout)
{
    if(!(muxGrp>=0 && muxGrp <MUX_MAX_GRP_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxGrp);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_GROUP_S *pGrp;
    if(SUCCESS != MUX_searchExistGroup(muxGrp, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pGrp->mComp, &nState);
    if(nState != COMP_StateExecuting)
    {
        aloge("fatal error! wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    MuxCompStreamWrap stStreamWrap = {pStream, nTimeout};
    COMP_BUFFERHEADERTYPE bufferHeader;
    bufferHeader.pAppPrivate = &stStreamWrap;
    bufferHeader.nInputPortIndex = nStreamId;
    ret = COMP_EmptyThisBuffer(pGrp->mComp, &bufferHeader);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SendVideoStream(MUX_GRP muxGrp, VENC_STREAM_S *pStream, int nStreamId, int nTimeout)
{
    return aw_mux_SendStream(muxGrp, (void*)pStream, nStreamId, nTimeout);
}

ERRORTYPE AW_MPI_MUX_SendAudioStream(MUX_GRP muxGrp, AUDIO_STREAM_S *pStream, int nStreamId, int nTimeout)
{
    return aw_mux_SendStream(muxGrp, (void*)pStream, nStreamId, nTimeout);
}

