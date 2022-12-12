/*
 ******************************************************************************
 *
 * MPI_EIS.H mpi_eis.ch module
 *
 * Copyright (c) 2018 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version        Author         Date           Description
 *
 *   1.1          huangbohan   2018/05/08       EIS
 *
 *****************************************************************************
 */

#define LOG_TAG "mpi_eis"
#include <utils/plat_log.h>

/* ref platform headers */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* #include "cdx_list.h" */
#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"

/* media api headers to app */
#include "mm_comm_video.h"
#include "mm_common.h"
#include "mpi_eis.h"

/* media internal common headers. */
#include <media_common.h>
#include <mm_component.h>
#include <tsemaphore.h>
#include <cdx_list.h>

/* EIS channel data struct definition */
typedef struct EIS_CHN_MAP_S {
    int mThdRunning;
    EIS_CHN mEISChn;
    /* eis component instance */
    MM_COMPONENTTYPE *mEISComp;
    cdx_sem_t mSemCompCmd;
    MPPCallbackInfo mCallbackInfo;
    struct list_head mList;
} EIS_CHN_MAP_S;

typedef struct EISChnManager
{
    /* Channel lock, used to manage EIS channels */
    pthread_mutex_t mLock;
    /* element type: EIS_CHN_MAP_S */
    struct list_head mChnList;
} EISChnManager;

static EISChnManager *g_pEISChnMng = NULL;

/* callback functions */
static ERRORTYPE VideoEISEventHandler(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN void *pAppData,
                                     PARAM_IN COMP_EVENTTYPE eEvent, PARAM_IN unsigned int nData1,
                                     PARAM_IN unsigned int nData2, PARAM_IN void *pEventData)
{
    EIS_CHN_MAP_S *pChn = (EIS_CHN_MAP_S *)pAppData;
    switch (eEvent) {
        case COMP_EventCmdComplete: {
            if (COMP_CommandStateSet == nData1) {
                alogv("video vi EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
            } else {
                alogw("Low probability! what command[0x%x]?", nData1);
            }
            break;
        }
        case COMP_EventError: {
            if (ERR_EIS_SAMESTATE == nData1) {
                alogv("Set same state to EIS component!");
                cdx_sem_up(&pChn->mSemCompCmd);
            } else if (ERR_EIS_INVALIDSTATE == nData1) {
                aloge("Why EIS component state turn to invalid?");
            } else if (ERR_EIS_INCORRECT_STATE_TRANSITION == nData1) {
                aloge("Fatal error! EIS component state transition incorrect.");
            }
            break;
        }
        default: {
            aloge("Fatal error! unknown event[0x%x]", eEvent);
            break;
        }
    }
    return SUCCESS;
}

ERRORTYPE VideoEISEmptyBufferDone(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN void* pAppData,
    PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    EIS_CHN_MAP_S *pChnMap = (EIS_CHN_MAP_S*)pAppData;
    VIDEO_FRAME_INFO_S *pFrameInfo =  (VIDEO_FRAME_INFO_S*)pBuffer->pAppPrivate;

    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_EIS;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pChnMap->mEISChn;
    CHECK_MPP_CALLBACK(pChnMap->mCallbackInfo.callback);

    pChnMap->mCallbackInfo.callback(pChnMap->mCallbackInfo.cookie, &ChannelInfo,
                            MPP_EVENT_RELEASE_VIDEO_BUFFER, (void*)pFrameInfo);

    return SUCCESS;
}

ERRORTYPE VideoEISFillBufferDone(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN void* pAppData,
    PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    return SUCCESS;
}

COMP_CALLBACKTYPE VideoEISCallback = {
    .EventHandler = VideoEISEventHandler,
    .EmptyBufferDone = VideoEISEmptyBufferDone,
    .FillBufferDone = VideoEISFillBufferDone,
};

MM_COMPONENTTYPE *EIS_GetGroupComp(MPP_CHN_S *pMppChn)
{
    MM_COMPONENTTYPE *pComp = NULL;
    ERRORTYPE ret = ERR_EIS_UNEXIST;
    EIS_CHN_MAP_S *pEntry;
    pthread_mutex_lock(&g_pEISChnMng->mLock);
    list_for_each_entry(pEntry, &g_pEISChnMng->mChnList, mList)
    {
        if (pEntry->mEISChn == pMppChn->mChnId) {
            pComp = pEntry->mEISComp;
            ret = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&g_pEISChnMng->mLock);
    return pComp;
}

/* It will be invoked in MPP_SysInit, do some ready works. */
ERRORTYPE EIS_Construct(void)
{
    ERRORTYPE iRet;
    if (g_pEISChnMng != NULL) {
        return SUCCESS;
    }
    g_pEISChnMng = (EISChnManager*)malloc(sizeof(EISChnManager));
    if (NULL == g_pEISChnMng) {
        aloge("alloc EISChnManager error(%s)!", strerror(errno));
        return ERR_EIS_NOMEM;
    }
    memset(g_pEISChnMng, 0, sizeof(EISChnManager));

    iRet = pthread_mutex_init(&g_pEISChnMng->mLock, NULL);
    if (iRet != 0) {
        aloge("fatal error! EIS channel's mutex init fail");
        free(g_pEISChnMng);
        g_pEISChnMng = NULL;
        return ERR_EIS_SYS_NOTREADY;
    }
    /* List of EIS_CHN_MAP_S */
    INIT_LIST_HEAD(&g_pEISChnMng->mChnList);

    return SUCCESS;
}

ERRORTYPE EIS_Destruct(void)
{
    if (g_pEISChnMng == NULL) {
        return SUCCESS;
    } else if (!list_empty(&g_pEISChnMng->mChnList)) {
        aloge("fatal error! some EIS channel still running when destroy EIS!");
        return ERR_EIS_BUSY;
    } else {
        pthread_mutex_destroy(&g_pEISChnMng->mLock);
        free(g_pEISChnMng);
        g_pEISChnMng = NULL;
    }
    return SUCCESS;
}

static ERRORTYPE searchExistChannel(EIS_CHN EISChn, EIS_CHN_MAP_S** ppChn)
{
    ERRORTYPE ret = ERR_EIS_UNEXIST;
    EIS_CHN_MAP_S* pEntry;

    if (g_pEISChnMng == NULL) {
        return ERR_EIS_UNEXIST;
    }


    pthread_mutex_lock(&g_pEISChnMng->mLock);
    list_for_each_entry(pEntry, &g_pEISChnMng->mChnList, mList) {
        if(pEntry->mEISChn == EISChn) {
			if(ppChn != NULL)
			{
				*ppChn = pEntry;
			}
            ret = SUCCESS;
        }
    }

Find:
    pthread_mutex_unlock(&g_pEISChnMng->mLock);
    return ret;
}

static ERRORTYPE addChannel(EIS_CHN_MAP_S *pChn)
{
    ERRORTYPE eRet = ERR_EIS_UNEXIST;
    EIS_CHN_MAP_S* pEntry;

    if (g_pEISChnMng != NULL && pChn != NULL) {
        pthread_mutex_lock(&g_pEISChnMng->mLock);
        list_add_tail(&pChn->mList, &g_pEISChnMng->mChnList);
        pthread_mutex_unlock(&g_pEISChnMng->mLock);
        eRet = SUCCESS;
    }

    return eRet;
}

static ERRORTYPE delChannel(EIS_CHN_MAP_S *pChn)
{
    ERRORTYPE eRet = ERR_EIS_UNEXIST;
    EIS_CHN_MAP_S* pEntry;

    if (g_pEISChnMng != NULL && pChn != NULL) {
        pthread_mutex_lock(&g_pEISChnMng->mLock);
        list_del(&pChn->mList);
        pthread_mutex_unlock(&g_pEISChnMng->mLock);
        eRet = SUCCESS;
    }

    return eRet;
}

/* EIS channel functions */
static EIS_CHN_MAP_S *createEIS_CHN_MAP_S(void)
{
    EIS_CHN_MAP_S *pEISChn = NULL;

    pEISChn = malloc(sizeof(EIS_CHN_MAP_S));
    if (NULL == pEISChn) {
        aloge("alloc EIS_CHN_MAP_S failed!!");
        return NULL;
    }
    memset(pEISChn, 0, sizeof(EIS_CHN_MAP_S));

    cdx_sem_init(&pEISChn->mSemCompCmd, 0);
    return pEISChn;
}

static void destroyEIS_CHN_MAP_S(EIS_CHN_MAP_S *pEISChn)
{
    if (pEISChn) {
        cdx_sem_deinit(&pEISChn->mSemCompCmd);
        free(pEISChn);
    }
}

ERRORTYPE AW_MPI_EIS_CreateChn(EIS_CHN EISChn, EIS_ATTR_S *pstAttr)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode = NULL;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS == eRet) {
        aloge("EISChn[%d] has exist!!\n", EISChn);
        return ERR_EIS_EXIST;
    }

    pNode = createEIS_CHN_MAP_S();
    if (NULL == pNode) {
        aloge("create EIS channel struct failed!!");
        eRet = ERR_EIS_NOMEM;
        goto ECrtEISChn;
    }
    pNode->mEISChn = EISChn;

    eRet = COMP_GetHandle((COMP_HANDLETYPE *)&(pNode->mEISComp), CDX_ComponentNameEIS,
                (void *)pNode, &VideoEISCallback);
    if (eRet != SUCCESS) {
        aloge("get component handle failed!!");
        goto EGetCompHdl;
    }

    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_EIS;
    /* just set to 0, we got only one real devices */
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = EISChn;

    eRet = COMP_SetConfig(pNode->mEISComp, COMP_IndexVendorEisChnAttr, (void *)pstAttr);
    eRet = COMP_SetConfig(pNode->mEISComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);

    eRet = COMP_SendCommand(pNode->mEISComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    if (eRet != SUCCESS) {
        //TODO : shoudle we do something?
        aloge("Fatel error, set EIS[%d] into COMP_StateIdle failed.!!");
    }
    cdx_sem_down(&pNode->mSemCompCmd);
    eRet = addChannel(pNode);

    return eRet;

EGetCompHdl:
    destroyEIS_CHN_MAP_S(pNode);
ECrtEISChn:
    return eRet;
}

AW_S32 AW_MPI_EIS_DestroyChn(EIS_CHN EISChn)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    if (pNode && pNode->mEISComp) {
        COMP_STATETYPE nCompState;
        if (SUCCESS != COMP_GetState(pNode->mEISComp, &nCompState)) {
            aloge("fatal error! get EIS component's status fail!");
            eRet = ERR_EIS_BUSY;
            goto ECompBusy;
        }

        switch (nCompState) {
            case COMP_StateIdle: {
                eRet = COMP_SendCommand(pNode->mEISComp, COMP_CommandStateSet,
                    COMP_StateLoaded, NULL);
                cdx_sem_down(&pNode->mSemCompCmd);
                eRet = SUCCESS;
            } break;
            case COMP_StateLoaded:
            case COMP_StateInvalid:
                eRet = SUCCESS;
                break;
            default: {
                aloge("fatal error! invalid EIS component state[0x%x]!", nCompState);
                eRet = FAILURE;
            } break;
        }
    }

    delChannel(pNode);
    /* if SUCCESS == eRet, that says the component has been stoped */
    if (SUCCESS == eRet) {
        COMP_FreeHandle(pNode->mEISComp);
        pNode->mEISComp = NULL;
    }
    destroyEIS_CHN_MAP_S(pNode);

ECompBusy:
    return eRet;
}

static AW_S32 setEISSpecAttrs(EIS_CHN EISChn, COMP_INDEXTYPE iIndex, void *pstAttr)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    if (pNode && pNode->mEISComp) {
        COMP_STATETYPE nCompState;
        if (SUCCESS != COMP_GetState(pNode->mEISComp, &nCompState)) {
            aloge("fatal error! get EIS component's status fail!");
            eRet = ERR_EIS_BUSY;
            goto ECompBusy;
        }

        switch (nCompState) {
            case COMP_StateIdle: {
                eRet = COMP_SetConfig(pNode->mEISComp, iIndex, pstAttr);
            } break;
            default: {
                aloge("fatal error! invalid EIS component state[0x%x]!", nCompState);
                eRet = ERR_EIS_NOT_PERM;
            } break;
        }
    }

ECompBusy:
    return eRet;
}

/*
* If the componment is started and running, you can only set <iDelayTimeMs>,
* else, you can set all the parameters.
*/
AW_S32 AW_MPI_EIS_SetChnAttr(EIS_CHN EISChn, EIS_ATTR_S *pstAttr)
{
    return setEISSpecAttrs(EISChn, COMP_IndexVendorEisChnAttr, pstAttr);
}

AW_S32 AW_MPI_EIS_GetChnAttr(EIS_CHN EISChn, EIS_ATTR_S *pstAttr)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    if (pNode && pNode->mEISComp) {
        eRet = pNode->mEISComp->GetConfig(pNode->mEISComp, COMP_IndexVendorEisChnAttr, (void *)pstAttr);
    }

ECompBusy:
    return eRet;
}

/* Must be setting before component run.
*/
AW_S32 AW_MPI_EIS_SetVideoFmtAttr(EIS_CHN EISChn, EIS_VIDEO_FMT_ATTR_S *pstAttr)
{
    return setEISSpecAttrs(EISChn, COMP_IndexVendorEisVFmtAttr, pstAttr);
}

/* Must be setting before component run.
*/
AW_S32 AW_MPI_EIS_SetVideoDataAttr(EIS_CHN EISChn, EIS_DATA_PROC_ATTR_S *pstAttr)
{
    return setEISSpecAttrs(EISChn, COMP_IndexVendorEisDataProcAttr, pstAttr);
}

/* Must be setting before component run.
*/
AW_S32 AW_MPI_EIS_SetGyroDataAttr(EIS_CHN EISChn, EIS_GYRO_DATA_ATTR_S *pstAttr)
{
    return setEISSpecAttrs(EISChn, COMP_IndexVendorEisGyroAttr, pstAttr);
}

/* Must be setting before component run.
*/
AW_S32 AW_MPI_EIS_SetKmatDataAttr(EIS_CHN EISChn, EIS_KMAT_S *pstAttr)
{
    return setEISSpecAttrs(EISChn, COMP_IndexVendorEisKmatAttr, pstAttr);
}

/* Must be setting before component run.
*/
AW_S32 AW_MPI_EIS_SetAlgoModeAttr(EIS_CHN EISChn, EIS_ALGO_MODE *pstAttr)
{
    return setEISSpecAttrs(EISChn, COMP_IndexVendorEisAlgoModeAttr, pstAttr);
}

AW_S32 AW_MPI_EIS_EnableChn(EIS_CHN EISChn)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    int iEnableFlag = 1;
    COMP_STATETYPE nCompState;
    eRet = COMP_GetState(pNode->mEISComp, &nCompState);
    switch (nCompState) {
        case COMP_StateIdle: {
            eRet = COMP_SetConfig(pNode->mEISComp, COMP_IndexVendorEisEnable, (void *)&iEnableFlag);
        } break;
        default: {
            /* if we are in COMP_StateExecuting state, the EnableFlag has already be set.
            * in Loaded or Invalid state, we can't set this flag.
            */
            aloge("Enable EIS component in wrong state[%d] do nothing!", nCompState);
            eRet = ERR_EIS_BUSY;
        } break;
    }

    return eRet;
}

AW_S32 AW_MPI_EIS_DisableChn(EIS_CHN EISChn)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    int iEnableFlag = 0;
    COMP_STATETYPE nCompState;
    eRet = COMP_GetState(pNode->mEISComp, &nCompState);
    switch (nCompState) {
        case COMP_StateExecuting: {
            /* if we are in COMP_StateExecuting state, the EnableFlag can't be clear.
            */
            aloge("Disbale EIS component in wrong state[%d] do nothing!", nCompState);
            eRet = ERR_EIS_BUSY;
        } break;
        default: {
            /* No matter which state we are in(exclude COMP_StateExecuting),
            * we always should close it to avoid memory leak and crumble.
            */
            eRet = COMP_SetConfig(pNode->mEISComp, COMP_IndexVendorEisEnable, (void *)&iEnableFlag);
        } break;
    }

    return eRet;
}

/* Mostly, it should be a sync operation */
AW_S32 AW_MPI_EIS_FlushChn(EIS_CHN EISChn, int iSync)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    COMP_STATETYPE nCompState;
    eRet = COMP_GetState(pNode->mEISComp, &nCompState);
    if (nCompState == COMP_StateIdle) {
        eRet = COMP_SendCommand(pNode->mEISComp, COMP_CommandFlush, iSync, NULL);
        if (iSync && SUCCESS == eRet) {
            cdx_sem_down(&pNode->mSemCompCmd);
        }
    } else {
        aloge("Do flush channel operation in a wrong state[%d]", nCompState);
        eRet = ERR_EIS_BUSY;
    }

    return eRet;
}

AW_S32 AW_MPI_EIS_StartChn(EIS_CHN EISChn)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    COMP_STATETYPE nCompState;
    eRet = COMP_GetState(pNode->mEISComp, &nCompState);
    switch (nCompState) {
        case COMP_StateIdle: {
            eRet = COMP_SendCommand(pNode->mEISComp, COMP_CommandStateSet,
                COMP_StateExecuting, NULL);
            cdx_sem_down(&pNode->mSemCompCmd);
            eRet = SUCCESS;
        } break;
        case COMP_StateExecuting:
            eRet = SUCCESS;
            break;
        default: {
            aloge("EIS component wrong state translate [0x%x->0x%x], do nothing!",
                nCompState, COMP_StateExecuting);
            eRet = ERR_EIS_INCORRECT_STATE_TRANSITION;
        } break;
    }

    return eRet;
}

AW_S32 AW_MPI_EIS_StopChn(EIS_CHN EISChn)
{
    ERRORTYPE eRet = SUCCESS;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    COMP_STATETYPE nCompState;
    eRet = COMP_GetState(pNode->mEISComp, &nCompState);
    switch (nCompState) {
        case COMP_StateExecuting: {
            eRet = COMP_SendCommand(pNode->mEISComp, COMP_CommandStateSet,
                COMP_StateIdle, NULL);
            cdx_sem_down(&pNode->mSemCompCmd);
            eRet = SUCCESS;
        } break;
        case COMP_StateIdle:
            eRet = SUCCESS;
            break;
        default: {
            aloge("EIS component wrong state translate [0x%x->0x%x], do nothing!",
                nCompState, COMP_StateExecuting);
            eRet = ERR_EIS_INCORRECT_STATE_TRANSITION;
        } break;
    }

    return eRet;
}

/* Used in unbind mode */
AW_S32 AW_MPI_EIS_GetData(EIS_CHN EISChn, VIDEO_FRAME_INFO_S *pstFrameInfo, AW_S32 s32MilliSec)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    EIS_PARAMS_S EISParams;
    EISParams.mChn = EISChn;
    EISParams.pstFrameInfo = pstFrameInfo;
    EISParams.s32MilliSec = s32MilliSec;

    eRet = COMP_GetState(pNode->mEISComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("get frame in wrong state[0x%x], return!", nState);
        return ERR_EIS_NOT_PERM;
    }

    eRet = pNode->mEISComp->GetConfig(pNode->mEISComp, COMP_IndexVendorEisGetData, (void *)&EISParams);
    return eRet;
}

/* Used in unbind mode */
AW_S32 AW_MPI_EIS_ReleaseData(EIS_CHN EISChn, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    EIS_PARAMS_S EISParams;
    EISParams.mChn = EISChn;
    EISParams.pstFrameInfo = pstFrameInfo;
    EISParams.s32MilliSec = 0;

    eRet = COMP_GetState(pNode->mEISComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("release frame in wrong state[0x%x], return!", nState);
        return ERR_EIS_NOT_PERM;
    }

    eRet = COMP_SetConfig(pNode->mEISComp, COMP_IndexVendorEisReleaseData, (void *)&EISParams);
    return eRet;
}

/* Used in unbind mode */
AW_S32 AW_MPI_EIS_SendPic(EIS_CHN EISChn, VIDEO_FRAME_INFO_S *pstFrameInfo, AW_S32 s32MilliSec)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    eRet = COMP_GetState(pNode->mEISComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("release frame in wrong state[0x%x], return!", nState);
        return ERR_EIS_NOT_PERM;
    }

    COMP_BUFFERHEADERTYPE BufferHeader;
    BufferHeader.nInputPortIndex   = EIS_CHN_PORT_INDEX_VIDEO_IN;
    BufferHeader.pAppPrivate = pstFrameInfo;
    eRet = COMP_EmptyThisBuffer(pNode->mEISComp, &BufferHeader);

    return eRet;
}

/* Used in unbind mode.
* But you can only use this function in Offline Simulation. mostly, you needn't use it.
*/
AW_S32 AW_MPI_EIS_SendGyroPacket(EIS_CHN EISChn, EIS_GYRO_PACKET_S *pstGyroPacket, unsigned int uPktNum)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    eRet = COMP_GetState(pNode->mEISComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("release frame in wrong state[0x%x], return!", nState);
        return ERR_EIS_NOT_PERM;
    }

    COMP_BUFFERHEADERTYPE BufferHeader;
    BufferHeader.nInputPortIndex   = EIS_CHN_PORT_INDEX_GYRO_IN;
    BufferHeader.pAppPrivate = pstGyroPacket;
    BufferHeader.nFilledLen  = uPktNum;
    eRet = COMP_EmptyThisBuffer(pNode->mEISComp, &BufferHeader);

    return eRet;
}

ERRORTYPE AW_MPI_EIS_RegisterCallback(EIS_CHN EISChn, MPPCallbackInfo *pCallback)
{
    ERRORTYPE eRet;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    if (pCallback) {
        pthread_mutex_lock(&g_pEISChnMng->mLock);
        pNode->mCallbackInfo = *pCallback;
        pthread_mutex_unlock(&g_pEISChnMng->mLock);
    }

    return eRet;
}

/* nFreq: MHz */
AW_S32 AW_MPI_EIS_SetEISFreq(EIS_CHN EISChn, int nFreq)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    eRet = COMP_GetState(pNode->mEISComp, &nState);
    if (COMP_StateIdle != nState) {
        aloge("release frame in wrong state[0x%x], return!", nState);
        return ERR_EIS_NOT_PERM;
    }

    eRet = COMP_SetConfig(pNode->mEISComp, COMP_IndexVendorEisFreq, &nFreq);

    return eRet;
    return SUCCESS;
}

/* nFreq: MHz */
AW_S32 AW_MPI_EIS_GetEISFreq(EIS_CHN EISChn, int* nFreq)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    eRet = COMP_GetState(pNode->mEISComp, &nState);
    if (COMP_StateInvalid == nState || COMP_StateLoaded == nState) {
        aloge("release frame in wrong state[0x%x], return!", nState);
        return ERR_EIS_NOT_PERM;
    }

    eRet = COMP_GetConfig(pNode->mEISComp, COMP_IndexVendorEisFreq, &nFreq);

    return eRet;
    return SUCCESS;
}

ERRORTYPE AW_MPI_EIS_Debug_StoreFrame(EIS_CHN EISChn, const char* pDirPath)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    EIS_CHN_MAP_S *pNode;

    MPI_EIS_CHECK_CHN_VALID(EISChn);
    eRet = searchExistChannel(EISChn, &pNode);
    if (SUCCESS != eRet) {
        aloge("EISChn[%d] is unexist!!\n", EISChn);
        return ERR_EIS_UNEXIST;
    }

    if(NULL == pDirPath) {
        aloge("must set a directory path! return now");
        return ERR_EIS_INVALID_PARA;
    }

    eRet = COMP_GetState(pNode->mEISComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("release frame in wrong state[0x%x], return!", nState);
        return ERR_EIS_NOT_PERM;
    }

    eRet = COMP_SetConfig(pNode->mEISComp, COMP_IndexVendorEisStoreFrame, (void*)pDirPath);
    return eRet;
}

