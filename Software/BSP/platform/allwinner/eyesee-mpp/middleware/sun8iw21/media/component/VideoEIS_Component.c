/******************************************************************************
  Copyright (C), 2018-2020, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VideoEIS_Component.c
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2018/04/20
  Last Modified :
  Description   : EIS mpp component implement
  Function List :
  History       :
******************************************************************************/

#define LOG_TAG "VideoEIS_Component"
#include <utils/plat_log.h>

//ref platform headers
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>

#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"
#include "cdx_list.h"

#include "SystemBase.h"
#include <VIDEO_FRAME_INFO_S.h>
#include "mm_comm_eis.h"
#include "mm_comm_video.h"
#include "mm_comm_vi.h"
#include "mm_common.h"

#include "ComponentCommon.h"
#include "media_common.h"
#include "mm_component.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include "vencoder.h"
#include "ion_memmanager.h"

#include "VideoEIS_Component.h"
#include "video_buffer_manager.h"
#include "utils/ring_buffer.h"
#include "utils/iio_utils.h"
#include "utils/generic_buffer.h"
#include "algo/include/EIS_lib.h"

typedef struct VideoEisPrivAttr {
    float fGyroAxScaleFactor;
    float fGyroAyScaleFactor;
    float fGyroAzScaleFactor;
    float fGyroVxScaleFactor;
    float fGyroVyScaleFactor;
    float fGyroVzScaleFactor;
} VIDEO_EIS_PRIVATTR;

typedef struct VideoEisDataType {
    COMP_STATETYPE state;
    pthread_mutex_t mStateLock;
    COMP_CALLBACKTYPE* pCallbacks;
    void* pAppData;
    COMP_HANDLETYPE hSelf;

    COMP_PORT_PARAM_TYPE sPortParam;
    COMP_PARAM_PORTDEFINITIONTYPE sPortDef[EIS_CHN_MAX_PORTS];
    COMP_INTERNAL_TUNNELINFOTYPE sPortTunnelInfo[EIS_CHN_MAX_PORTS];
    COMP_PARAM_BUFFERSUPPLIERTYPE sPortBufSupplier[EIS_CHN_MAX_PORTS];
    /* TRUE: tunnel mode; FALSE: non-tunnel mode. */
    bool bInputPortTunnelFlag;
    bool bOutputPortTunnelFlag;

    MPP_CHN_S mMppChnInfo;
    pthread_t mEISTrd;
    message_queue_t cmd_queue;

    /* Gyro informations */
    pthread_t mGyroTrd;
    bool bGyroRunFlag;
    bool bHasGyroDev;
    struct gyro_device_attr mGyroAttr;
    struct ring_buffer* GryoRingBufHd;
    unsigned int iGVSyncPktId;
    struct gyro_instance *pGyroIns;

    /* Video buffer informations */
    bool bWaitingInputFrmFlag;
    VideoBufferManager *pInputBufMgr;
    pthread_mutex_t mInputFrmLock;

    bool bWaitingOutFrmFlag;
    cdx_sem_t mSemWaitOutFrame;

    pthread_mutex_t mOutFrmListLock;
    struct list_head mOutIdleList;
    struct list_head mOutValidList;
    struct list_head mOutUsedList;
    int iOutFrmCnt;
    int iOutFrmIdleCnt;

    /* Begin from the beginning of first line exposure,
    * end to the beginning of last line exposure. */
    int iVideoLineTime; /* Unit: ms */
    EISE_FrameData* pstEISPkt;
    unsigned long long iEISPktIdx;
    unsigned long long mEisPktMap; // We use map to manage EIS packets
    EISE_FrameData* pLastUsedEISPkt;
    uint64_t dLastInputFrmPts;

    EIS_ATTR_S mEISAttr;
    VIDEO_EIS_PRIVATTR mEisPrivAttr;
    /* Hardware EIS attributions */
    int iEisHwFreq;
    EISE_HANDLE mEisHd;
    EISE_CFG_PARA mEisCfg;
    pthread_mutex_t mEISAttrLock;
    uint64_t dEisGetInputFrmCnt;

    cdx_sem_t mAllFrameRelSem;
    volatile int mWaitAllFrameReleaseFlag;
    bool bEnableFlag;
    pthread_mutex_t mEISLock;

    bool bByPassMode;
    char *pBPDataSavePath;
    FILE *pVideoPtsFd;
    FILE *pGyroPtsDataFd;
    FILE *pVideoDataFd;
    int iCacheBufferNum;
    bool bStoreFrame;
    int mStoreFrameCount;
    char mDbgStoreFrameFilePath[EIS_MAXPATHSIZE];
} VIDEOEISDATATYPE;

#define LOCK_DEBUG
//#define GYRO_DATA_SAVE_DEBUG
//#define PROC_TIME_TEST

#define TIME_INIT_START_END(x) struct timeval start_##x; \
                       struct timeval end_##x; \
                       float DiffTime_##x;
#define TIME_GET_START(x) (gettimeofday(&start_##x, NULL))
#define TIME_GET_END(x) (gettimeofday(&end_##x, NULL)); \
                (DiffTime_##x = \
                (((end_##x.tv_sec + end_##x.tv_usec * 1e-6) - \
                (start_##x.tv_sec + start_##x.tv_usec * 1e-6)) * 1000))
#define TIME_PRINT_DIFF(x) (DiffTime_##x)

static void *EIS_CompGyroThread(void *pThreadData);
static void *EIS_CompStabThread(void *pThreadData);

ERRORTYPE DoVideoEisGetTunnelInfo(PARAM_IN COMP_HANDLETYPE hComponent,
                                 PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int i;
    for(i = 0; i < EIS_CHN_MAX_PORTS; i++) {
        if(pVideoEISData->sPortBufSupplier[i].nPortIndex == pTunnelInfo->nPortIndex) {
            memcpy(pTunnelInfo, &pVideoEISData->sPortTunnelInfo[i], sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
            break;
        }
    }
    if(i == EIS_CHN_MAX_PORTS) {
        aloge("No such port index[%d] in EIS[%d].", pTunnelInfo->nPortIndex, pVideoEISData->mMppChnInfo.mChnId);
        eError = ERR_EIS_INVALID_PARA;
    }

    return eError;
}

static void DoVideoEisStoreProcessedFrm(VIDEOEISDATATYPE* pVideoEISData, VIDEO_FRAME_INFO_S* SFrame)
{
    char strPixFmt[16];
    if(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420==SFrame->VFrame.mPixelFormat || MM_PIXEL_FORMAT_AW_NV21S==SFrame->VFrame.mPixelFormat)
        strcpy(strPixFmt, "nv21");
    else if(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == SFrame->VFrame.mPixelFormat)
        strcpy(strPixFmt, "nv12");
    else
        strcpy(strPixFmt, "unknown");

    int nPos = strlen(pVideoEISData->mDbgStoreFrameFilePath);
    snprintf(pVideoEISData->mDbgStoreFrameFilePath+nPos, EIS_MAXPATHSIZE-nPos,
        "/EisChn[%d]Pic[%d].%s", 
        pVideoEISData->mMppChnInfo.mChnId, pVideoEISData->mStoreFrameCount++, strPixFmt);
    FILE *dbgFp = fopen(pVideoEISData->mDbgStoreFrameFilePath, "wb");
    VideoFrameBufferSizeInfo FrameSizeInfo;
    getVideoFrameBufferSizeInfo(SFrame, &FrameSizeInfo);
    int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
    for(int i = 0; i < 3; i++) {
        if(SFrame->VFrame.mpVirAddr[i] != NULL) {
            fwrite(SFrame->VFrame.mpVirAddr[i], 1, yuvSize[i], dbgFp);
            alogd("virAddr[%d]=[%p], length=[%d]", i, SFrame->VFrame.mpVirAddr[i], yuvSize[i]);
        }
    }
    fclose(dbgFp);
    alogd("Store EIS frame in file[%s]", pVideoEISData->mDbgStoreFrameFilePath);
}

ERRORTYPE DoVideoEisGetData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT EIS_PARAMS_S *pstParams, PARAM_IN int nMilliSec)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoEISData->state && COMP_StateExecuting != pVideoEISData->state) {
        alogw("call getStream in wrong state[0x%x]", pVideoEISData->state);
        return ERR_VI_NOT_PERM;
    }
    int ret;

    /* not support get data in Tunnel mode for app */
    if (TRUE == pVideoEISData->bOutputPortTunnelFlag) {
        aloge("You are in output bind mode, do not get data in yourself.");
        return ERR_VI_NOT_PERM;
    }

    VIDEO_FRAME_INFO_S *pstFrameInfo = pstParams->pstFrameInfo;
    VideoFrameListInfo *pVFrmListTmp = NULL;
_TryToGetFrame:
    pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
    if (!list_empty(&pVideoEISData->mOutValidList)) {
        pVFrmListTmp = list_first_entry(&pVideoEISData->mOutValidList, VideoFrameListInfo, mList);
        memcpy(pstFrameInfo, &pVFrmListTmp->mFrame, sizeof(VIDEO_FRAME_INFO_S));
        pstFrameInfo->VFrame.mOffsetTop = 0;
        pstFrameInfo->VFrame.mOffsetBottom = pstFrameInfo->VFrame.mHeight;
        pstFrameInfo->VFrame.mOffsetLeft = 0;
        pstFrameInfo->VFrame.mOffsetRight = pstFrameInfo->VFrame.mWidth;

        if(pVideoEISData->bStoreFrame) {
            DoVideoEisStoreProcessedFrm(pVideoEISData, pstFrameInfo);
            pVideoEISData->bStoreFrame = FALSE;
        }
        eError = SUCCESS;
        list_move_tail(&pVFrmListTmp->mList, &pVideoEISData->mOutUsedList);
        pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);
    } else {
        pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);
        if(0 == nMilliSec) {
            eError = ERR_EIS_BUF_EMPTY;
        } else if(nMilliSec < 0) {
            pVideoEISData->bWaitingOutFrmFlag = TRUE;
            cdx_sem_down(&pVideoEISData->mSemWaitOutFrame);
            pVideoEISData->bWaitingOutFrmFlag = FALSE;
            goto _TryToGetFrame;
        } else {
            pVideoEISData->bWaitingOutFrmFlag = TRUE;
            ret = cdx_sem_down_timedwait(&pVideoEISData->mSemWaitOutFrame, nMilliSec);
            if(0 == ret) {
                pVideoEISData->bWaitingOutFrmFlag = FALSE;
                goto _TryToGetFrame;
            } else {
                aloge("Fatal error! EIS[%d] pthread cond wait timeout ret[%d]", pVideoEISData->mMppChnInfo.mChnId, ret);
                eError = ERR_EIS_BUF_EMPTY;
                pVideoEISData->bWaitingOutFrmFlag = FALSE;
            }
        }
    }

    return eError;
}

ERRORTYPE DoVideoEisReleaseData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN EIS_PARAMS_S *pstParams)
{
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoEISData->state && COMP_StateExecuting != pVideoEISData->state) {
        alogw("Call getStream in wrong state[0x%x]", pVideoEISData->state);
        return ERR_EIS_INVALIDSTATE;
    }
    if (TRUE == pVideoEISData->bOutputPortTunnelFlag) { /* Tunnel mode */
        aloge("You are in output bind mode, do not get data in yourself.");
        return ERR_EIS_NOT_PERM;
    }

    int iEisChn = pstParams->mChn;
    bool bFindFlag = 0;
    VIDEO_FRAME_INFO_S *pstFrameInfo = pstParams->pstFrameInfo;
    VideoFrameListInfo *pVFrmList;

    pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
    list_for_each_entry(pVFrmList, &pVideoEISData->mOutUsedList, mList)
    {
        if (pVFrmList->mFrame.mId == pstFrameInfo->mId ||
                pVFrmList->mFrame.VFrame.mpVirAddr[0] == pstFrameInfo->VFrame.mpVirAddr[0]) {
            bFindFlag = 1;
            break;
        }
    }

    if (bFindFlag) {
        list_move_tail(&pVFrmList->mList, &pVideoEISData->mOutIdleList);
        pVideoEISData->iOutFrmIdleCnt++;
    } else {
        aloge("No such video frame Id[%d] is using, please check it.", pstFrameInfo->mId);
        pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);
        return ERR_EIS_INVALID_PARA;
    }
    pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);

    return SUCCESS;
}

ERRORTYPE DoVideoEisStoreFrameOpr(PARAM_IN COMP_HANDLETYPE hComponent, const char* pDirPath)
{
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (!pVideoEISData->bOutputPortTunnelFlag) {
        alogw("in non-tunnel mode, you should store frame by your self, we don't do it\n");
        return ERR_EIS_NOT_SUPPORT;
    }

    message_t msg;
    InitMessage(&msg);
    msg.command = EisComp_StoreFrame;
    msg.mpData = (void*)pDirPath;
    msg.mDataSize = strlen(pDirPath)+1;
    putMessageWithData(&pVideoEISData->cmd_queue, &msg);

    return SUCCESS;
}

ERRORTYPE VideoEISSendCommand(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_COMMANDTYPE Cmd,
                             PARAM_IN unsigned int nParam1, PARAM_IN void *pCmdData)
{
    VIDEOEISDATATYPE *pVideoEISData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    alogv("VideoEISSendCommand: %d", Cmd);

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (!pVideoEISData) {
        eError = ERR_EIS_SYS_NOTREADY;
        goto ENotReady;
    }

    if (pVideoEISData->state == COMP_StateInvalid) {
        eError = ERR_EIS_NOT_PERM;
        goto ENotReady;
    }
    switch (Cmd) {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        default:
            alogw("impossible comp_command[0x%x]", Cmd);
            eCmd = -1;
            eError = ERR_EIS_NOT_SUPPORT;
            goto ENotSupport;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pVideoEISData->cmd_queue, &msg);

ENotSupport:
ENotReady:
    return eError;
}

ERRORTYPE VideoEISGetState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT COMP_STATETYPE *pState)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    *pState = pVideoEISData->state;

    return eError;
}

ERRORTYPE VideoEISSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_CALLBACKTYPE *pCallbacks,
                              PARAM_IN void *pAppData)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (!pVideoEISData || !pCallbacks || !pAppData) {
        eError = ERR_VI_INVALID_PARA;
        goto COMP_CONF_CMD_FAIL;
    }

    pVideoEISData->pCallbacks = pCallbacks;
    pVideoEISData->pAppData = pAppData;

COMP_CONF_CMD_FAIL:
    return eError;
}

ERRORTYPE DoVideoEisChnConfigOpr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_INOUT EIS_ATTR_S *pEisAttr,
                              PARAM_IN EIS_CONF_OPR_E eConfOpr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    switch (eConfOpr) {
        case EIS_CHN_CONF_SET: {
            if (pEisAttr) {
                memcpy(&pVideoEISData->mEISAttr, pEisAttr, sizeof(EIS_ATTR_S));
                pVideoEISData->mGyroAttr.sample_freq = pEisAttr->iGyroFreq;
                pVideoEISData->mGyroAttr.axi_num = pEisAttr->iGyroAxiNum;
            }
        } break;
        case EIS_CHN_CONF_GET: {
            if (pEisAttr)
                memcpy(pEisAttr, &pVideoEISData->mEISAttr, sizeof(EIS_ATTR_S));
            else {
                aloge("You input a NULL pointer to store EIS attribution, fix it.");
                eError = ERR_EIS_INVALID_NULL_PTR;
            }
        } break;
        default: {
            aloge("Wrong config operation[%d].", eConfOpr);
            eError = ERR_EIS_NOT_SUPPORT;
        } break;
    }

    return eError;
}

ERRORTYPE DoSetEisChnAlgoModeOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_INOUT EIS_ALGO_MODE *pEisAttr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (NULL == pEisAttr) {
        aloge("You input a NULL pointer to store EIS attribution, fix it.");
        eError = ERR_EIS_INVALID_NULL_PTR;
        goto ENullPtr;
    }

    pthread_mutex_lock(&pVideoEISData->mStateLock);
    if (pVideoEISData->state != COMP_StateIdle && pVideoEISData->state != COMP_StateLoaded) {
        aloge("Send frame when EIS component state[0x%x].", pVideoEISData->state);
        eError = ERR_EIS_NOT_PERM;
        goto EState;
    }

EState:
    pthread_mutex_unlock(&pVideoEISData->mStateLock);

ENullPtr:
    return eError;
}

ERRORTYPE DoSetEisChnVFmtOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_INOUT EIS_VIDEO_FMT_ATTR_S *pEisAttr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (NULL == pEisAttr) {
        aloge("You input a NULL pointer to store EIS attribution, fix it.");
        eError = ERR_EIS_INVALID_NULL_PTR;
        goto ENullPtr;
    }
    pthread_mutex_lock(&pVideoEISData->mStateLock);
    if (pVideoEISData->state != COMP_StateIdle && pVideoEISData->state != COMP_StateLoaded) {
        aloge("Send frame when EIS component state[0x%x].", pVideoEISData->state);
        eError = ERR_EIS_NOT_PERM;
        goto EState;
    }

    pVideoEISData->mEISAttr.iVideoInWidth = pEisAttr->iVideoInWidth;
    pVideoEISData->mEISAttr.iVideoInWidthStride = pEisAttr->iVideoInWidthStride;
    pVideoEISData->mEISAttr.iVideoInHeight = pEisAttr->iVideoInHeight;
    pVideoEISData->mEISAttr.iVideoInHeightStride = pEisAttr->iVideoInHeightStride;
    pVideoEISData->mEISAttr.iVideoOutWidth = pEisAttr->iVideoOutWidth;
    pVideoEISData->mEISAttr.iVideoOutHeight = pEisAttr->iVideoOutHeight;
    pVideoEISData->mEISAttr.eVideoFmt = pEisAttr->eVideoFmt;

EState:
    pthread_mutex_unlock(&pVideoEISData->mStateLock);

ENullPtr:
    return eError;
}

ERRORTYPE DoSetEisChnDataProcOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_INOUT EIS_DATA_PROC_ATTR_S *pEisAttr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (NULL == pEisAttr) {
        aloge("You input a NULL pointer to store EIS attribution, fix it.");
        eError = ERR_EIS_INVALID_NULL_PTR;
        goto ENullPtr;
    }
    pthread_mutex_lock(&pVideoEISData->mStateLock);
    if (pVideoEISData->state != COMP_StateIdle && pVideoEISData->state != COMP_StateLoaded) {
        aloge("Send frame when EIS component state[0x%x].", pVideoEISData->state);
        eError = ERR_EIS_NOT_PERM;
        goto EState;
    }

    pVideoEISData->mEISAttr.bRetInFrmFast = pEisAttr->bRetInFrmFast;
    pVideoEISData->mEISAttr.bHorizFlip    = pEisAttr->bHorizFlip;
    pVideoEISData->mEISAttr.bVerticalFlip = pEisAttr->bVerticalFlip;
    pVideoEISData->mEISAttr.iInputBufNum  = pEisAttr->iInputBufNum;
    pVideoEISData->mEISAttr.iOutputBufBum = pEisAttr->iOutputBufBum;
    pVideoEISData->mEISAttr.iDelayTimeMs  = pEisAttr->iDelayTimeMs;
    pVideoEISData->mEISAttr.iEisFilterWidth   = pEisAttr->iEisFilterWidth;
    pVideoEISData->mEISAttr.iSyncErrTolerance = pEisAttr->iSyncErrTolerance;

EState:
    pthread_mutex_unlock(&pVideoEISData->mStateLock);

ENullPtr:
    return eError;
}

ERRORTYPE DoSetEisChnGyroOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_INOUT EIS_GYRO_DATA_ATTR_S *pEisAttr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (NULL == pEisAttr) {
        aloge("You input a NULL pointer to store EIS attribution, fix it.");
        eError = ERR_EIS_INVALID_NULL_PTR;
        goto ENullPtr;
    }
    pthread_mutex_lock(&pVideoEISData->mStateLock);
    if (pVideoEISData->state != COMP_StateIdle && pVideoEISData->state != COMP_StateLoaded) {
        aloge("Send frame when EIS component state[0x%x].", pVideoEISData->state);
        eError = ERR_EIS_NOT_PERM;
        goto EState;
    }

    pVideoEISData->mEISAttr.iGyroFreq = pEisAttr->iGyroFreq;
    pVideoEISData->mEISAttr.iGyroPoolSize = pEisAttr->iGyroPoolSize;
    pVideoEISData->mEISAttr.iGyroAxiNum   = pEisAttr->iGyroAxiNum;

EState:
    pthread_mutex_unlock(&pVideoEISData->mStateLock);

ENullPtr:
    return eError;
}

ERRORTYPE DoSetEisChnKmatOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_INOUT EIS_KMAT_S *pEisAttr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (NULL == pEisAttr) {
        aloge("You input a NULL pointer to store EIS attribution, fix it.");
        eError = ERR_EIS_INVALID_NULL_PTR;
        goto ENullPtr;
    }
    pthread_mutex_lock(&pVideoEISData->mStateLock);
    if (pVideoEISData->state != COMP_StateIdle && pVideoEISData->state != COMP_StateLoaded) {
        aloge("Send frame when EIS component state[0x%x].", pVideoEISData->state);
        eError = ERR_EIS_NOT_PERM;
        goto EState;
    }

    pVideoEISData->mEISAttr.bUseKmat  = 1;
    pVideoEISData->mEISAttr.stEisKmat = *pEisAttr;

EState:
    pthread_mutex_unlock(&pVideoEISData->mStateLock);

ENullPtr:
    return eError;
}

ERRORTYPE DoVideoEisMPPChannelInfoOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_IN MPP_CHN_S *pstInfo, PARAM_IN EIS_CONF_OPR_E eConfOpr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    switch (eConfOpr) {
        case EIS_CHN_CONF_SET: {
            copy_MPP_CHN_S(&pVideoEISData->mMppChnInfo, pstInfo);
        } break;
        case EIS_CHN_CONF_GET: {
            copy_MPP_CHN_S(pstInfo, &pVideoEISData->mMppChnInfo);
        } break;
        default: {
            aloge("Wrong config operation[%d].", eConfOpr);
            eError = ERR_EIS_NOT_SUPPORT;
        } break;
    }

    return eError;
}

ERRORTYPE _EisEnableGyroDeviceMulElem(VIDEOEISDATATYPE *pVideoEISData, int iEnableFlag)
{
    ERRORTYPE eError = SUCCESS;
    int iRet = 0;

    if (iEnableFlag) {
        if (!pVideoEISData->mEISAttr.bSimuOffline) {
            /* Online mode, then create and open the gyro device. */
            pVideoEISData->pGyroIns = create_gyro_inst();
            if (NULL == pVideoEISData->pGyroIns) {
                aloge("Create gyro instance failed.");
                eError = ERR_EIS_NOMEM;
                goto ECrtIns;
            }

            iRet = gyro_ins_open(pVideoEISData->pGyroIns, &pVideoEISData->mGyroAttr);
            if (0 != iRet) {
                aloge("FFFFFFatal error, open gyro instance failed, we will use \"zero\" datas as valid gyro datas.");
                pVideoEISData->bHasGyroDev = 0;
                pVideoEISData->GryoRingBufHd = ring_buffer_create(sizeof(EIS_GYRO_PACKET_S),
                    pVideoEISData->mEISAttr.iGyroPoolSize, RB_FL_NONE);
                if (NULL == pVideoEISData->GryoRingBufHd) {
                    aloge("Create ring buffer for gyro buffers failed.");
                    eError = ERR_EIS_NOMEM;
                    goto EOpenIns;
                }
            } else {
                pVideoEISData->bHasGyroDev = 1;
                pVideoEISData->GryoRingBufHd = gyro_ins_get_rbhdl(pVideoEISData->pGyroIns);
            }

            if (pVideoEISData->mEISAttr.eEisAlgoMode == EIS_ALGO_MODE_BP)
                pVideoEISData->bByPassMode = 1;

            if (pVideoEISData->bByPassMode) {
                pVideoEISData->bGyroRunFlag = 1;
                if (pthread_create(&pVideoEISData->mGyroTrd, NULL, EIS_CompGyroThread, pVideoEISData)) {
                    aloge("create EIS_CompGyroThread failed!");
                    eError = ERR_EIS_FAILED_NOTENABLE;
                    pVideoEISData->bGyroRunFlag = 0;
                    goto ECrtThread;
                }
            }
        } else {
            /* Offline simulation mode, just create ring_buffer module. */
            pVideoEISData->GryoRingBufHd = ring_buffer_create(sizeof(EIS_GYRO_PACKET_S),
                pVideoEISData->mEISAttr.iGyroPoolSize, RB_FL_NONE);
            if (NULL == pVideoEISData->GryoRingBufHd) {
                aloge("Create ring buffer for gyro buffers failed.");
                eError = ERR_EIS_NOMEM;
                goto ECrtBuffer;
            }
        }
    } else {
        if (!pVideoEISData->mEISAttr.bSimuOffline) {
            /* Close and wait gyro read thread exit. */
            if (pVideoEISData->pGyroIns) {
                if (pVideoEISData->bByPassMode) {
                    pVideoEISData->bGyroRunFlag = 0;
                    pthread_join(pVideoEISData->mGyroTrd, NULL);
                    pVideoEISData->mGyroTrd = 0;
                }
                /* If we got no gyro device, then we should destroy ring buffer by ourselves. */
                if (!pVideoEISData->bHasGyroDev && pVideoEISData->GryoRingBufHd)
                    ring_buffer_destroy(pVideoEISData->GryoRingBufHd);
                gyro_ins_close(pVideoEISData->pGyroIns);
                destory_gyro_inst(pVideoEISData->pGyroIns);
            } else {
                aloge("You have not enable gyro device yet.");
                eError = ERR_EIS_NOT_PERM;
            }
        } else {
            /* Destroy ring buffers and close mpu devices */
            ring_buffer_destroy(pVideoEISData->GryoRingBufHd);
        }
        pVideoEISData->GryoRingBufHd = NULL;
    }

    return eError;

ECrtThread:
    gyro_ins_close(pVideoEISData->pGyroIns);
EOpenIns:
    pVideoEISData->GryoRingBufHd = NULL;
    destory_gyro_inst(pVideoEISData->pGyroIns);
ECrtIns:
ECrtBuffer:
    return eError;
}

ERRORTYPE DoVideoEisChnEnableOpr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int* iEnableFlag,
                              PARAM_IN EIS_CONF_OPR_E eConfOpr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pVideoEISData->mEISLock);
    switch (eConfOpr) {
        case EIS_CHN_CONF_SET: {
            /* Only when [old_val=0,new_val=1] and [old_val=1,new_val=0], we will do real operation invoke,
            * else [old_val=0,new_val=0] and [old_val=0,new_val=0], just return. */
            if (*iEnableFlag + pVideoEISData->bEnableFlag == 1) {
                eError = _EisEnableGyroDeviceMulElem(pVideoEISData, *iEnableFlag);
                if (eError != SUCCESS) {
                    aloge("Do enable or disable Gyro device failed. ret 0x%x.", eError);
                    goto EEnable;
                }
                pVideoEISData->bEnableFlag = *iEnableFlag;
            } else
                alogw("EIS[%d] has already been enabled or disabled.", pVideoEISData->mMppChnInfo.mChnId);
        } break;
        case EIS_CHN_CONF_GET: {
            *iEnableFlag = pVideoEISData->bEnableFlag;
        } break;
        default: {
            aloge("Wrong config operation[%d].", eConfOpr);
            eError = ERR_EIS_NOT_SUPPORT;
        } break;
    }

EEnable:
    pthread_mutex_unlock(&pVideoEISData->mEISLock);

    return eError;
}

ERRORTYPE DoVideoEisFreqOpr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int* iFreqVal,
                              PARAM_IN EIS_CONF_OPR_E eConfOpr)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pVideoEISData->mEISLock);
    switch (eConfOpr) {
        case EIS_CHN_CONF_SET: {
            pVideoEISData->iEisHwFreq = *iFreqVal;
        } break;
        case EIS_CHN_CONF_GET: {
            *iFreqVal = pVideoEISData->iEisHwFreq;
        } break;
        default: {
            aloge("Wrong config operation[%d].", eConfOpr);
            eError = ERR_EIS_NOT_SUPPORT;
        } break;
    }

EEnable:
    pthread_mutex_unlock(&pVideoEISData->mEISLock);

    return eError;
}

ERRORTYPE DoVideoEisPortDefinitionOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                     PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef,
                     PARAM_IN EIS_CONF_OPR_E eConfOpr)
{
    VIDEOEISDATATYPE *pVideoEisData;
    ERRORTYPE eError = SUCCESS;

    int i;
    pVideoEisData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    for(i = 0; i < EIS_CHN_MAX_PORTS; i++) {
        if (pPortDef->nPortIndex == pVideoEisData->sPortDef[i].nPortIndex)
            break;
    }
    if (i == EIS_CHN_MAX_PORTS) {
        eError = ERR_EIS_INVALID_PARA;
    } else {
        switch (eConfOpr) {
            case EIS_CHN_CONF_SET: {
                memcpy(&pVideoEisData->sPortDef[i], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            } break;
            case EIS_CHN_CONF_GET: {
                memcpy(pPortDef, &pVideoEisData->sPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
            } break;
            default: {
                aloge("Wrong config operation[%d].", eConfOpr);
                eError = ERR_EIS_NOT_SUPPORT;
            } break;
        }
    }

    return eError;
}

ERRORTYPE DoVideoEisCompBufferSupplierOpr(PARAM_IN COMP_HANDLETYPE hComponent,
                     PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier,
                     PARAM_IN EIS_CONF_OPR_E eConfOpr)
{
    VIDEOEISDATATYPE *pVideoEisData;
    ERRORTYPE eError = SUCCESS;

    int i;
    pVideoEisData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    for(i = 0; i < EIS_CHN_MAX_PORTS; i++) {
        if(pVideoEisData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
            break;
    }

    if (i == EIS_CHN_MAX_PORTS) {
        eError = ERR_EIS_INVALID_PARA;
    } else {
        switch (eConfOpr) {
            case EIS_CHN_CONF_SET: {
                memcpy(&pVideoEisData->sPortBufSupplier[i], pPortBufSupplier,
                    sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            } break;
            case EIS_CHN_CONF_GET: {
                memcpy(pPortBufSupplier, &pVideoEisData->sPortBufSupplier[i],
                    sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            } break;
            default: {
                aloge("Wrong config operation[%d].", eConfOpr);
                eError = ERR_EIS_NOT_SUPPORT;
            } break;
        }
    }

    return eError;
}

ERRORTYPE VideoEISGetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                           PARAM_IN void *pComponentConfigStructure)
{
    // VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    switch (nIndex) {
        case COMP_IndexVendorEisChnAttr: {
            eError = DoVideoEisChnConfigOpr(hComponent, (EIS_ATTR_S*)pComponentConfigStructure, EIS_CHN_CONF_GET);
        } break;

        case COMP_IndexVendorMPPChannelInfo: {
            eError = DoVideoEisMPPChannelInfoOpr(hComponent, (MPP_CHN_S *)pComponentConfigStructure, EIS_CHN_CONF_GET);
        } break;

        case COMP_IndexVendorEisEnable: {
            eError = DoVideoEisChnEnableOpr(hComponent, (int*)pComponentConfigStructure, EIS_CHN_CONF_GET);
        } break;

        case COMP_IndexVendorEisGetData: {
            EIS_PARAMS_S *pData = (EIS_PARAMS_S *)pComponentConfigStructure;
            eError = DoVideoEisGetData(hComponent, pData, pData->s32MilliSec);
        } break;

        case COMP_IndexVendorTunnelInfo: {
            eError = DoVideoEisGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE *)pComponentConfigStructure);
        } break;

        case COMP_IndexParamPortDefinition: {
            eError = DoVideoEisPortDefinitionOpr(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure, EIS_CHN_CONF_GET);
        } break;

        case COMP_IndexParamCompBufferSupplier: {
            eError = DoVideoEisCompBufferSupplierOpr(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure, EIS_CHN_CONF_GET);
        } break;

        default: {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_VI_NOT_SUPPORT;
        } break;
    }
    return eError;
}

ERRORTYPE VideoEISSetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                           PARAM_IN void *pComponentConfigStructure)
{
    // VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    switch (nIndex) {
        case COMP_IndexVendorEisChnAttr: {
            eError = DoVideoEisChnConfigOpr(hComponent, (EIS_ATTR_S*)pComponentConfigStructure, EIS_CHN_CONF_SET);
        } break;

        case COMP_IndexVendorEisAlgoModeAttr: {
            eError = DoSetEisChnAlgoModeOpr(hComponent, (EIS_ALGO_MODE*)pComponentConfigStructure);
        } break;

        case COMP_IndexVendorEisVFmtAttr: {
            eError = DoSetEisChnVFmtOpr(hComponent, (EIS_VIDEO_FMT_ATTR_S*)pComponentConfigStructure);
        } break;

        case COMP_IndexVendorEisDataProcAttr: {
            eError = DoSetEisChnDataProcOpr(hComponent, (EIS_DATA_PROC_ATTR_S*)pComponentConfigStructure);
        } break;

        case COMP_IndexVendorEisGyroAttr: {
            eError = DoSetEisChnGyroOpr(hComponent, (EIS_GYRO_DATA_ATTR_S*)pComponentConfigStructure);
        } break;

        case COMP_IndexVendorEisKmatAttr: {
            eError = DoSetEisChnKmatOpr(hComponent, (EIS_KMAT_S*)pComponentConfigStructure);
        } break;

        case COMP_IndexVendorMPPChannelInfo: {
            eError = DoVideoEisMPPChannelInfoOpr(hComponent, (MPP_CHN_S *)pComponentConfigStructure, EIS_CHN_CONF_SET);
        } break;

        case COMP_IndexVendorEisEnable: {
            eError = DoVideoEisChnEnableOpr(hComponent, (int*)pComponentConfigStructure, EIS_CHN_CONF_SET);
        } break;

        case COMP_IndexVendorEisReleaseData: {
            eError = DoVideoEisReleaseData(hComponent, (EIS_PARAMS_S *)pComponentConfigStructure);
        } break;

        case COMP_IndexVendorEisStoreFrame: {
            eError = DoVideoEisStoreFrameOpr(hComponent, (const char*)pComponentConfigStructure);
        } break;

        case COMP_IndexVendorEisFreq: {
            eError = DoVideoEisFreqOpr(hComponent, (int*)pComponentConfigStructure, EIS_CHN_CONF_SET);
        } break;

        case COMP_IndexParamPortDefinition: {
            eError = DoVideoEisPortDefinitionOpr(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure, EIS_CHN_CONF_SET);
        } break;

        case COMP_IndexParamCompBufferSupplier: {
            eError = DoVideoEisCompBufferSupplierOpr(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure, EIS_CHN_CONF_SET);
        } break;

        default: {
            aloge("Unknown EIS configuration index[0x%x]", nIndex);
            eError = ERR_EIS_NOT_SUPPORT;
        } break;
    }

    return eError;
}

ERRORTYPE VideoEISComponentTunnelRequest(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN unsigned int nPort,
                                        PARAM_IN COMP_HANDLETYPE hTunneledComp, PARAM_IN unsigned int nTunneledPort,
                                        PARAM_INOUT COMP_TUNNELSETUPTYPE *pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    COMP_PARAM_PORTDEFINITIONTYPE   *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE    *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE   *pPortBufSupplier;
    int i;

    /* You can bind input ports and output ports,
    *but better not do this in Executing state
    */
    if (pVideoEISData->state == COMP_StateExecuting) {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    } else if (pVideoEISData->state != COMP_StateIdle) {
        aloge("Fatal error! tunnel request can't be in state[0x%x]",pVideoEISData->state);
        eError = ERR_EIS_INCORRECT_STATE_OPERATION;
        goto EBind;
    }

    for (i = 0; i < EIS_CHN_MAX_PORTS; ++i) {
        if (pVideoEISData->sPortDef[i].nPortIndex == nPort) {
            pPortDef = &pVideoEISData->sPortDef[i];
            break;
        }
    }
    if (i == EIS_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_EIS_INVALID_PARA;
        goto EBind;
    }

    for (i = 0; i < EIS_CHN_MAX_PORTS; ++i) {
        if (pVideoEISData->sPortTunnelInfo[i].nPortIndex == nPort) {
            pPortTunnelInfo = &pVideoEISData->sPortTunnelInfo[i];
            break;
        }
    }
    if (i == EIS_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_EIS_INVALID_PARA;
        goto EBind;
    }

    for (i = 0; i < EIS_CHN_MAX_PORTS; ++i) {
        if (pVideoEISData->sPortBufSupplier[i].nPortIndex == nPort) {
            pPortBufSupplier = &pVideoEISData->sPortBufSupplier[i];
            break;
        }
    }
    if (i == EIS_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_EIS_INVALID_PARA;
        goto EBind;
    }

    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType =
        (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    /* When all the parameters is zero, we unbind it, it is a engage */
    if(NULL == hTunneledComp && 0 == nTunneledPort && NULL == pTunnelSetup) {
        alogd("Receive a cancel setup tunnel request on port[%d]", nPort);
        pPortTunnelInfo->hTunnel = NULL;

        if(pPortDef->eDir == COMP_DirOutput)
            pVideoEISData->bOutputPortTunnelFlag = FALSE;
        else if (pPortDef->eDir == COMP_DirInput)
            pVideoEISData->bInputPortTunnelFlag = FALSE;
        eError = SUCCESS;
        goto EBind;
    }

    if(pPortDef->eDir == COMP_DirOutput) {
        if (pVideoEISData->bOutputPortTunnelFlag) {
            aloge("EIS component[%d] output port has already binded, why bind again?!",
                pVideoEISData->mMppChnInfo.mChnId);
            eError = ERR_EIS_NOT_PERM;
            goto EBind;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pVideoEISData->bOutputPortTunnelFlag = TRUE;
    } else if (pPortDef->eDir == COMP_DirInput && pVideoEISData->mEISAttr.bSimuOffline){
            aloge("EIS component[%d] input port can't be bind in offline simulation mode.",
                pVideoEISData->mMppChnInfo.mChnId);
            eError = ERR_EIS_NOT_PERM;
            goto EBind;
    } else if (pPortDef->eDir == COMP_DirInput) {
        if (pVideoEISData->bInputPortTunnelFlag) {
            aloge("EIS component[%d] input port has already binded, why bind again?!",
                pVideoEISData->mMppChnInfo.mChnId);
            eError = ERR_EIS_NOT_PERM;
            goto EBind;
        }
        /* Check the data compatibility between the ports using one or more GetParameter calls. */
        /* B checks if its input port is compatible with the output port of component A. */
        COMP_PARAM_PORTDEFINITIONTYPE stTunnelPortDef;
        stTunnelPortDef.nPortIndex = nTunneledPort;
        COMP_GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &stTunnelPortDef);
        if(stTunnelPortDef.eDir != COMP_DirOutput) {
            aloge("Fatal error! you are binding a input tunnel port[index%d] to an input port[index%d].",
                nTunneledPort, pPortDef->nPortIndex);
            eError = ERR_EIS_INVALID_PARA;
            goto EBind;
        }
#if 0
        if (strcmp((const char*)&pPortDef->format, (const char*)&stTunnelPortDef.format)) {
            aloge("Fatal error! you are binding a output tunnel port[fmt%d] to an input port[fmt%d].",
                stTunnelPortDef.format, pPortDef->format);
            eError = ERR_EIS_INVALID_PARA;
            goto EBind;
        }
#endif
        /* The component B informs component A about the final result of negotiation. */
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier) {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!",
                nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }

        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        COMP_GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        COMP_SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pVideoEISData->bInputPortTunnelFlag = TRUE;
    }

EBind:
    return eError;
}

static void DoVideoEisSendBackInputFrame
    (VIDEOEISDATATYPE *pVideoEisData, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    COMP_BUFFERHEADERTYPE BufferHeader;
    memset(&BufferHeader, 0, sizeof(COMP_BUFFERHEADERTYPE));
    if (FALSE == pVideoEisData->bInputPortTunnelFlag) {
        BufferHeader.pAppPrivate = pFrameInfo;
        pVideoEisData->pCallbacks->EmptyBufferDone(pVideoEisData->hSelf, pVideoEisData->pAppData, &BufferHeader);
    } else {
        BufferHeader.pOutputPortPrivate = pFrameInfo;
        MM_COMPONENTTYPE *pTunnelComp = (MM_COMPONENTTYPE *)pVideoEisData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_VIDEO_IN].hTunnel;
        BufferHeader.nOutputPortIndex = pVideoEisData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_VIDEO_IN].nTunnelPortIndex;
        COMP_FillThisBuffer(pTunnelComp, &BufferHeader);
    }
    alogv("Release input FrameId[%d].", pFrameInfo->mId);
}

static void DoVideoEisReturnBackAllInputFrames(VIDEOEISDATATYPE *pVideoEisData)
{
    VIDEO_FRAME_INFO_S *pFrameInfo = NULL;
    VIDEO_FRAME_INFO_S stFrameInfoRet;

    while (1) {
        pFrameInfo = VideoBufMgrGetAllValidUsingFrame(pVideoEisData->pInputBufMgr);
        if (NULL == pFrameInfo)
            break;
        stFrameInfoRet = *pFrameInfo;
        DoVideoEisSendBackInputFrame(pVideoEisData, &stFrameInfoRet);
    }
    alogw("Return all input frames done, EIS Chn[%d].", pVideoEisData->mMppChnInfo.mChnId);
}

ERRORTYPE VideoEISEmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pthread_mutex_lock(&pVideoEISData->mStateLock);
    if (pVideoEISData->state != COMP_StateExecuting) {
        alogw("Send frame when EIS component state[0x%x] isn not executing", pVideoEISData->state);
        eError = ERR_EIS_NOT_PERM;
        pthread_mutex_unlock(&pVideoEISData->mStateLock);
        goto EState;
    }
    pthread_mutex_unlock(&pVideoEISData->mStateLock);

    /* put data ptr to CapMgr_queue for tunnel or user mpi_get\release */
    if (pBuffer->nInputPortIndex == pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].nPortIndex) {
        VIDEO_FRAME_INFO_S *pVInputFrm = NULL;

        if(pVideoEISData->bInputPortTunnelFlag)
            pVInputFrm = (VIDEO_FRAME_INFO_S *)pBuffer->pOutputPortPrivate;
        else
            pVInputFrm = (VIDEO_FRAME_INFO_S *)pBuffer->pAppPrivate;

        eError = VideoBufMgrPushFrame(pVideoEISData->pInputBufMgr, pVInputFrm);
        if (eError != SUCCESS) {
            aloge("Failed to push this frame, it will be droped\n");
            eError =  ERR_EIS_BUF_FULL;
            goto EPushFrm;
        }

        alogv("VideoEisEmptyThisBuffer pushFrame process, %p.\r\n", pVInputFrm->VFrame.mpVirAddr[0]);

        if (pVideoEISData->bWaitingInputFrmFlag) {
            message_t msg;
            msg.command = EisComp_InputFrameAvailable;
            put_message(&pVideoEISData->cmd_queue, &msg);
        }
    } else if (pBuffer->nInputPortIndex == pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].nPortIndex) {
        /* We accept gyro datas and ready to process it. */
        EIS_GYRO_PACKET_S *pVInputGyroPkt = NULL;
        if(pVideoEISData->bInputPortTunnelFlag)
            pVInputGyroPkt = (EIS_GYRO_PACKET_S *)pBuffer->pOutputPortPrivate;
        else
            pVInputGyroPkt = (EIS_GYRO_PACKET_S *)pBuffer->pAppPrivate;

        if (ring_buffer_full(pVideoEISData->GryoRingBufHd)) {
            EIS_GYRO_PACKET_S stGyroPktOut;
            aloge("Gyro ring buffer is full, drop the oldest gyro data.");
            ring_buffer_out(pVideoEISData->GryoRingBufHd, &stGyroPktOut, pBuffer->nFilledLen);
        }
        ring_buffer_in(pVideoEISData->GryoRingBufHd, pVInputGyroPkt, pBuffer->nFilledLen);
    } else {
        aloge("fatal error! inputPortIndex[%d] match nothing!", pBuffer->nOutputPortIndex);
    }

EPushFrm:
EState:

    return eError;
}

ERRORTYPE VideoEISFillThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    VIDEOEISDATATYPE *pVideoEISData;
    ERRORTYPE eError = SUCCESS;
    int ret;

    pVideoEISData = (VIDEOEISDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    COMP_INTERNAL_TUNNELINFOTYPE *pPortTunnelInfo = &pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_VIDEO_IN];
    MM_COMPONENTTYPE *pInTunnelComp = (MM_COMPONENTTYPE*)pPortTunnelInfo->hTunnel;

    /* First of all, this interface can be used only in bind mode. */
    if (!pVideoEISData->bOutputPortTunnelFlag) {
        aloge("You can not invoke this interface in non-tunnel out port mode.");
        eError = ERR_EIS_NOT_PERM;
    }

    if (pBuffer->nOutputPortIndex == pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].nPortIndex
            && pInTunnelComp) {
        VIDEO_FRAME_INFO_S *pstFrameInfo = pBuffer->pAppPrivate;
        VideoFrameListInfo *pVFrmList;
        bool bFindFlag = 0;

        pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
        list_for_each_entry(pVFrmList, &pVideoEISData->mOutUsedList, mList)
        {
            if (pVFrmList->mFrame.mId == pstFrameInfo->mId ||
                    pVFrmList->mFrame.VFrame.mpVirAddr[0] == pstFrameInfo->VFrame.mpVirAddr[0]) {
                bFindFlag = 1;
                break;
            }
        }

        if (bFindFlag) {
            list_move_tail(&pVFrmList->mList, &pVideoEISData->mOutIdleList);
            pVideoEISData->iOutFrmIdleCnt++;
        } else {
            aloge("No such video frame Id[%d]VirAddr[%p] is using, please check it.",
                pstFrameInfo->mId, pstFrameInfo->VFrame.mpVirAddr[0]);
            eError = ERR_EIS_INVALID_PARA;
        }
        pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);
    } else {
        aloge("Fatal error! outputPortIndex[%d]!=[%d]", pBuffer->nOutputPortIndex, pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].nPortIndex);
    }

    return eError;
}

static void _EisTraverOutputBufPool(VIDEOEISDATATYPE *pVideoEISData)
{
    VideoFrameListInfo* EisOutFrmTmp = NULL;
    VideoFrameListInfo* EisOutFrmCur = NULL;

    pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
    if (!list_empty(&pVideoEISData->mOutUsedList)) {
        list_for_each_entry_safe(EisOutFrmCur, EisOutFrmTmp, &pVideoEISData->mOutUsedList, mList)
        {
            if (EisOutFrmCur) {
                aloge("Release video input frame[%d], vir addr[%p][%p].", EisOutFrmCur->mFrame.mId,
                    EisOutFrmCur->mFrame.VFrame.mpVirAddr[0], EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
            }
        }
        EisOutFrmCur = NULL;
    }

    if (!list_empty(&pVideoEISData->mOutValidList)) {
        list_for_each_entry_safe(EisOutFrmCur, EisOutFrmTmp, &pVideoEISData->mOutValidList, mList)
        {
            if (EisOutFrmCur) {
                aloge("Release video input frame[%d], vir addr[%p][%p].", EisOutFrmCur->mFrame.mId,
                    EisOutFrmCur->mFrame.VFrame.mpVirAddr[0], EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
            }
        }
        EisOutFrmCur = NULL;    
    }

    if (!list_empty(&pVideoEISData->mOutIdleList)) {
        list_for_each_entry_safe(EisOutFrmCur, EisOutFrmTmp, &pVideoEISData->mOutIdleList, mList)
        {
            if (EisOutFrmCur) {
                aloge("Release video input frame[%d], vir addr[%p][%p].", EisOutFrmCur->mFrame.mId,
                    EisOutFrmCur->mFrame.VFrame.mpVirAddr[0], EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
            }
        }
        EisOutFrmCur = NULL;
    }
    pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);
}

ERRORTYPE _EisCreateOutputBufPool(VIDEOEISDATATYPE *pVideoEISData)
{
    int i = 0;
    int iPoolBufNum = pVideoEISData->mEISAttr.iOutputBufBum;
    VideoFrameListInfo* EisOutFrm;
    ERRORTYPE eError = SUCCESS;

    pthread_mutex_init(&pVideoEISData->mOutFrmListLock, NULL);
    INIT_LIST_HEAD(&pVideoEISData->mOutIdleList);
    INIT_LIST_HEAD(&pVideoEISData->mOutValidList);
    INIT_LIST_HEAD(&pVideoEISData->mOutUsedList);
    for (i = 0; i < iPoolBufNum; i++) {
        /* alloc size for output buffers */
        EisOutFrm = malloc(sizeof(VideoFrameListInfo));
        if (NULL == EisOutFrm)
            continue;
        EisOutFrm->mFrame.VFrame.mpVirAddr[0] = ion_allocMem(
            pVideoEISData->mEISAttr.iVideoOutWidth*pVideoEISData->mEISAttr.iVideoOutHeight);
        EisOutFrm->mFrame.VFrame.mpVirAddr[1] = ion_allocMem(
            pVideoEISData->mEISAttr.iVideoOutWidth*pVideoEISData->mEISAttr.iVideoOutHeight/2);
        EisOutFrm->mFrame.VFrame.mPhyAddr[0]  = ion_getMemPhyAddr(EisOutFrm->mFrame.VFrame.mpVirAddr[0]);
        EisOutFrm->mFrame.VFrame.mPhyAddr[1]  = ion_getMemPhyAddr(EisOutFrm->mFrame.VFrame.mpVirAddr[1]);
        EisOutFrm->mFrame.VFrame.mWidth  = pVideoEISData->mEISAttr.iVideoOutWidth;
        EisOutFrm->mFrame.VFrame.mHeight = pVideoEISData->mEISAttr.iVideoOutHeight;
        EisOutFrm->mFrame.VFrame.mOffsetTop  = 0;
        EisOutFrm->mFrame.VFrame.mOffsetLeft = 0;
        EisOutFrm->mFrame.VFrame.mOffsetRight  = pVideoEISData->mEISAttr.iVideoOutWidth;
        EisOutFrm->mFrame.VFrame.mOffsetBottom = pVideoEISData->mEISAttr.iVideoOutHeight;
        EisOutFrm->mFrame.VFrame.mField = VIDEO_FIELD_FRAME;
        EisOutFrm->mFrame.VFrame.mVideoFormat = VIDEO_FORMAT_LINEAR;
        EisOutFrm->mFrame.VFrame.mCompressMode = COMPRESS_MODE_NONE;

        EisOutFrm->mFrame.VFrame.mStride[0] = 0;
        EisOutFrm->mFrame.VFrame.mStride[1] = 0;
        EisOutFrm->mFrame.VFrame.mStride[2] = 0;
        EisOutFrm->mFrame.VFrame.mPixelFormat = pVideoEISData->mEISAttr.eVideoFmt;
        EisOutFrm->mFrame.mId = pVideoEISData->iOutFrmCnt;
        list_add_tail(&EisOutFrm->mList, &pVideoEISData->mOutIdleList);
        pVideoEISData->iOutFrmCnt++;
    }
    pVideoEISData->iOutFrmIdleCnt = pVideoEISData->iOutFrmCnt;

//    _EisTraverOutputBufPool(pVideoEISData);

    return eError;
}

void _EisDestroyOutputBufPool(VIDEOEISDATATYPE *pVideoEISData)
{
    VideoFrameListInfo* EisOutFrmTmp = NULL;
    VideoFrameListInfo* EisOutFrmCur = NULL;

    pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
    if (!list_empty(&pVideoEISData->mOutUsedList)) {
        list_for_each_entry_safe(EisOutFrmCur, EisOutFrmTmp, &pVideoEISData->mOutUsedList, mList)
        {
            if (EisOutFrmCur) {
                aloge("Release video input frame[%d], vir addr[%p][%p].", EisOutFrmCur->mFrame.mId,
                    EisOutFrmCur->mFrame.VFrame.mpVirAddr[0], EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
                ion_freeMem(EisOutFrmCur->mFrame.VFrame.mpVirAddr[0]);
                ion_freeMem(EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
                free(EisOutFrmCur);
            }
        }
        EisOutFrmCur = NULL;
    }

    if (!list_empty(&pVideoEISData->mOutValidList)) {
        list_for_each_entry_safe(EisOutFrmCur, EisOutFrmTmp, &pVideoEISData->mOutValidList, mList)
        {
            if (EisOutFrmCur) {
                aloge("Release video input frame[%d], vir addr[%p][%p].", EisOutFrmCur->mFrame.mId,
                    EisOutFrmCur->mFrame.VFrame.mpVirAddr[0], EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
                ion_freeMem(EisOutFrmCur->mFrame.VFrame.mpVirAddr[0]);
                ion_freeMem(EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
                free(EisOutFrmCur);
            }
        }
        EisOutFrmCur = NULL;    
    }

    if (!list_empty(&pVideoEISData->mOutIdleList)) {
        list_for_each_entry_safe(EisOutFrmCur, EisOutFrmTmp, &pVideoEISData->mOutIdleList, mList)
        {
            if (EisOutFrmCur) {
                aloge("Release video input frame[%d], vir addr[%p][%p].", EisOutFrmCur->mFrame.mId,
                    EisOutFrmCur->mFrame.VFrame.mpVirAddr[0], EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
                ion_freeMem(EisOutFrmCur->mFrame.VFrame.mpVirAddr[0]);
                ion_freeMem(EisOutFrmCur->mFrame.VFrame.mpVirAddr[1]);
                free(EisOutFrmCur);
            }
        }
        EisOutFrmCur = NULL;
    }
    pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);

    pthread_mutex_destroy(&pVideoEISData->mOutFrmListLock);
}

static void _EisDumpHardwareLibConfigs(EISE_CFG_PARA *pEisCfg)
{
    printf("Dump EIS hardware driver config:\r\n");
    printf("\t in_w:\t%d\r\n", pEisCfg->in_w);
    printf("\t in_h:\t%d\r\n", pEisCfg->in_h);
    printf("\t out_h:\t%d\r\n", pEisCfg->out_h);
    printf("\t out_w:\t%d\r\n", pEisCfg->out_w);
    printf("\t hor_off:\t%d\r\n", pEisCfg->hor_off);
    printf("\t vor_off:\t%d\r\n", pEisCfg->ver_off);

    if (pEisCfg->in_yuv_type == PLANE_YUV420) {
        printf("\t in_yuv_type:\tYUV420\r\n");
    } else if (pEisCfg->in_yuv_type == PLANE_YVU420) {
        printf("\t in_yuv_type:\tYVU420\r\n");
    }

    if (pEisCfg->out_yuv_type == PLANE_YUV420)
        printf("\t out_yuv_type:\tYUV420\r\n");
    else if (pEisCfg->out_yuv_type == PLANE_YVU420)
        printf("\t out_yuv_type:\tYVU420\r\n");

    printf("\t in_luma_pitch:\t%d\r\n", pEisCfg->in_luma_pitch);
    printf("\t in_chroma_pitch:\t%d\r\n", pEisCfg->in_chroma_pitch);
    printf("\t out_luma_pitch:\t%d\r\n", pEisCfg->out_luma_pitch);
    printf("\t out_chroma_pitch:\t%d\r\n", pEisCfg->out_chroma_pitch);
    printf("\t src_width:\t%d\r\n", pEisCfg->src_width);
    printf("\t src_height:\t%d\r\n", pEisCfg->src_height);
    printf("\t cut_height:\t%d\r\n", pEisCfg->cut_height);
    printf("\t rt:\t%f\r\n", pEisCfg->rt);
    printf("\t k_matrix[0]:\t%f\r\n", pEisCfg->k_matrix[0]);
    printf("\t k_matrix[1]:\t%f\r\n", pEisCfg->k_matrix[2]);
    printf("\t k_matrix[2]:\t%f\r\n", pEisCfg->k_matrix[4]);
    printf("\t k_matrix[3]:\t%f\r\n", pEisCfg->k_matrix[5]);

    printf("\t k_matrix[8]:\t%f\r\n", pEisCfg->k_matrix[8]);
    printf("\t ts:\t%f\r\n", pEisCfg->ts);
    printf("\t td:\t%f\r\n", pEisCfg->td);
    printf("\t stable_anglev[0]:\t%f\r\n", pEisCfg->stable_anglev[0]);
    printf("\t stable_anglev[1]:\t%f\r\n", pEisCfg->stable_anglev[1]);
    printf("\t stable_anglev[2]:\t%f\r\n", pEisCfg->stable_anglev[2]);

    printf("\t angle_th[0]:\t%f\r\n", pEisCfg->angle_th[0]);
    printf("\t angle_th[1]:\t%f\r\n", pEisCfg->angle_th[1]);
    printf("\t angle_th[2]:\t%f\r\n", pEisCfg->angle_th[2]);

    printf("\t radius[0]:\t%d\r\n", pEisCfg->radius[0]);
    printf("\t radius[1]:\t%d\r\n", pEisCfg->radius[1]);
    printf("\t radius[2]:\t%d\r\n", pEisCfg->radius[2]);

    printf("\t video fps:\t%d\r\n", pEisCfg->fps);
    printf("\t filter_type:\t%d\r\n", pEisCfg->filter_type);
    printf("\t xy_exchange_en:\t%d\r\n", pEisCfg->xy_exchange_en);
    printf("\t rolling_shutter:\t%d\r\n", pEisCfg->rolling_shutter);
    printf("\t rs_correction_en:\t%d\r\n", pEisCfg->rs_correction_en);
    printf("\t frame_rotation_en:\t%d\r\n", pEisCfg->frame_rotation_en);
    printf("\t fast_mode:\t%d\r\n", pEisCfg->fast_mode);
    printf("\t g_en_angle_filter:\t%d\r\n", pEisCfg->g_en_angle_filter);
    printf("\t g_interlace:\t%d\r\n", pEisCfg->g_interlace);
    printf("\t max_frm_buf:\t%d\r\n", pEisCfg->max_frm_buf);
    printf("\t max_next_frm:\t%d\r\n", pEisCfg->max_next_frm);
}

static void VideoEisSetHwCfgs(VIDEOEISDATATYPE *pVideoEISData)
{
    pVideoEISData->mEisCfg.in_w = pVideoEISData->mEISAttr.iVideoInWidth;
    pVideoEISData->mEisCfg.in_h = pVideoEISData->mEISAttr.iVideoInHeight;

    // 1080p@30fps
    pVideoEISData->mEisCfg.out_h = pVideoEISData->mEISAttr.iVideoOutHeight;
    pVideoEISData->mEisCfg.out_w = pVideoEISData->mEISAttr.iVideoOutWidth;
    pVideoEISData->mEisCfg.hor_off = (int)((float)(pVideoEISData->mEISAttr.iVideoInWidth-pVideoEISData->mEISAttr.iVideoOutWidth)/64+0.5)*32;
    pVideoEISData->mEisCfg.ver_off = (int)((float)(pVideoEISData->mEISAttr.iVideoInHeight-pVideoEISData->mEISAttr.iVideoOutHeight)/64+0.5)*32;
    pVideoEISData->mEisCfg.in_yuv_type =
        (pVideoEISData->mEISAttr.eVideoFmt == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420) ? PLANE_YUV420 : PLANE_YVU420;
    pVideoEISData->mEisCfg.out_yuv_type =
        (pVideoEISData->mEISAttr.eVideoFmt == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420) ? PLANE_YUV420 : PLANE_YVU420;
    pVideoEISData->mEisCfg.in_luma_pitch   = pVideoEISData->mEISAttr.iVideoInWidth;
    pVideoEISData->mEisCfg.in_chroma_pitch = pVideoEISData->mEISAttr.iVideoInWidth;
    pVideoEISData->mEisCfg.out_luma_pitch   = pVideoEISData->mEISAttr.iVideoOutWidth;
    pVideoEISData->mEisCfg.out_chroma_pitch = pVideoEISData->mEISAttr.iVideoOutWidth;

    switch (pVideoEISData->mEISAttr.eOperationMode) {
        /* Just all use EIS_OPR_1080P30 configuration until now. */
        case EIS_OPR_4K30: {
            pVideoEISData->mEisCfg.src_width  = 4032;
            pVideoEISData->mEisCfg.src_height = 2256;
            pVideoEISData->mEisCfg.cut_height = 2256;
            pVideoEISData->mEisCfg.rt = (float)4296 / (300*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 2478.96f;
            pVideoEISData->mEisCfg.k_matrix[2] = 1983.4f;
            pVideoEISData->mEisCfg.k_matrix[4] = 2486.28f;
            pVideoEISData->mEisCfg.k_matrix[5] = 1165.32f;
            pVideoEISData->iVideoLineTime = 30;
            pVideoEISData->mEisCfg.g_interlace = 1;
        } break;
        case EIS_OPR_2P7K30: {
            pVideoEISData->mEisCfg.src_width  = 3840;
            pVideoEISData->mEisCfg.src_height = 2160;
            pVideoEISData->mEisCfg.cut_height = 2160;
            pVideoEISData->mEisCfg.rt = (float)4296 / (300*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 1960.0f;
            pVideoEISData->mEisCfg.k_matrix[2] = 1680.0f;
            pVideoEISData->mEisCfg.k_matrix[4] = 1960.0f;
            pVideoEISData->mEisCfg.k_matrix[5] = 944.0f;
            pVideoEISData->iVideoLineTime = 30;
            pVideoEISData->mEisCfg.g_interlace = 1;
        } break;
        case EIS_OPR_VGA30: {
            pVideoEISData->mEisCfg.src_width  = 3840;
            pVideoEISData->mEisCfg.src_height = 2160;
            pVideoEISData->mEisCfg.cut_height = 2160;
            pVideoEISData->mEisCfg.rt = (float)4296 / (300*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 466.67f;
            pVideoEISData->mEisCfg.k_matrix[2] = 400.0f;
            pVideoEISData->mEisCfg.k_matrix[4] = 466.67f;
            pVideoEISData->mEisCfg.k_matrix[5] = 224.0f;
            pVideoEISData->iVideoLineTime = 30;
            pVideoEISData->mEisCfg.g_interlace = 1;
        } break;
        case EIS_OPR_1080P30: {
            pVideoEISData->mEisCfg.src_width  = 3840;
            pVideoEISData->mEisCfg.src_height = 2160;
            pVideoEISData->mEisCfg.cut_height = 2160;
            pVideoEISData->mEisCfg.rt = (float)4296 / (300*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 1400.0f;
            pVideoEISData->mEisCfg.k_matrix[2] = 1200.0f;
            pVideoEISData->mEisCfg.k_matrix[4] = 1400.0f;
            pVideoEISData->mEisCfg.k_matrix[5] = 672.0f;
            pVideoEISData->iVideoLineTime = 30;
            pVideoEISData->mEisCfg.g_interlace = 1;
        } break; /* keep default settings */
        case EIS_OPR_VGA60: {
            pVideoEISData->mEisCfg.src_width  = 2016;
            pVideoEISData->mEisCfg.src_height = 1128;
            pVideoEISData->mEisCfg.cut_height = 1128;
            pVideoEISData->mEisCfg.rt = (float)2256 / (294*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 467.82f;
            pVideoEISData->mEisCfg.k_matrix[2] = 400.0f;
            pVideoEISData->mEisCfg.k_matrix[4] = 467.82f;
            pVideoEISData->mEisCfg.k_matrix[5] = 224.0f;
            pVideoEISData->iVideoLineTime = 9;
            pVideoEISData->mEisCfg.g_interlace = 2;
        } break;
        case EIS_OPR_1080P60: {
            pVideoEISData->mEisCfg.src_width  = 2016;
            pVideoEISData->mEisCfg.src_height = 1128;
            pVideoEISData->mEisCfg.cut_height = 1128;
            pVideoEISData->mEisCfg.rt = (float)2256 / (294*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 1400.0f;
            pVideoEISData->mEisCfg.k_matrix[2] = 1008.0f;
            pVideoEISData->mEisCfg.k_matrix[4] = 1400.0f;
            pVideoEISData->mEisCfg.k_matrix[5] = 564.0f;
            pVideoEISData->iVideoLineTime = 9;
            pVideoEISData->mEisCfg.g_interlace = 2;
        } break;
        case EIS_OPR_DEBUG_RES: {
            pVideoEISData->mEisCfg.src_width  = 3840;
            pVideoEISData->mEisCfg.src_height = 2160;
            pVideoEISData->mEisCfg.cut_height = 2160;
            pVideoEISData->mEisCfg.rt = (float)4296 / (300*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 1680.0f;
            pVideoEISData->mEisCfg.k_matrix[2] = 1440.0f;
            pVideoEISData->mEisCfg.k_matrix[4] = 1680.0f;
            pVideoEISData->mEisCfg.k_matrix[5] = 816.0f;
            pVideoEISData->iVideoLineTime = 30;
        } break;
        default: {
            pVideoEISData->mEisCfg.src_width  = 3840;
            pVideoEISData->mEisCfg.src_height = 2160;
            pVideoEISData->mEisCfg.cut_height = 2160;
            pVideoEISData->mEisCfg.rt = (float)4296 / (300*1000*1000);
            pVideoEISData->mEisCfg.k_matrix[0] = 3735.99;
            pVideoEISData->mEisCfg.k_matrix[2] = 1188.06;
            pVideoEISData->mEisCfg.k_matrix[4] = 3665.66;
            pVideoEISData->mEisCfg.k_matrix[5] = 680.34;
            pVideoEISData->iVideoLineTime = 30;
            pVideoEISData->mEisCfg.g_interlace = 1;

            aloge("You set a wrong operation mode[%d], use EIS_OPR_1080P30 by default.", pVideoEISData->mEISAttr.eOperationMode);
        } break;
    }

    if (pVideoEISData->mEISAttr.bUseKmat) {
        pVideoEISData->mEisCfg.k_matrix[0] = pVideoEISData->mEISAttr.stEisKmat.KmatK1;
        pVideoEISData->mEisCfg.k_matrix[2] = pVideoEISData->mEISAttr.stEisKmat.KmatK2;
        pVideoEISData->mEisCfg.k_matrix[4] = pVideoEISData->mEISAttr.stEisKmat.KmatKx;
        pVideoEISData->mEisCfg.k_matrix[5] = pVideoEISData->mEISAttr.stEisKmat.KmatKy;
    }

    pVideoEISData->mEisCfg.k_matrix[8] = 1;
    pVideoEISData->mEisCfg.ts = (float)pVideoEISData->mEisCfg.cut_height * pVideoEISData->mEisCfg.rt;
    pVideoEISData->mEisCfg.td = 0;
//        (float)(pVideoEISData->mEisCfg.src_height - pVideoEISData->mEisCfg.cut_height)/2*pVideoEISData->mEisCfg.rt;
#if 1
    pVideoEISData->mEisCfg.stable_anglev[0] = 0.06155;
    pVideoEISData->mEisCfg.stable_anglev[1] = 0.015;
    pVideoEISData->mEisCfg.stable_anglev[2] = -0.0173;
#else
    pVideoEISData->mEisCfg.stable_anglev[0] = 0.0082;
    pVideoEISData->mEisCfg.stable_anglev[1] = -0.0052;
    pVideoEISData->mEisCfg.stable_anglev[2] = -0.0155;
#endif

#ifndef PI
#define PI          3.141592653589793
#endif

    pVideoEISData->mEisCfg.angle_th[0] = 0;
    pVideoEISData->mEisCfg.angle_th[1] = 0;
    pVideoEISData->mEisCfg.angle_th[2] = 2*PI/180;

    switch(pVideoEISData->mEisCfg.out_w)
    {
    case 2688:
    case 2304:
        pVideoEISData->mEisCfg.radius[0]         = 17;
        pVideoEISData->mEisCfg.radius[1]         = 2;
        pVideoEISData->mEisCfg.radius[2]         = 1;
        pVideoEISData->mEisCfg.max_frm_buf       = pVideoEISData->mEISAttr.iEisFilterWidth;
        pVideoEISData->mEisCfg.filter_type       = 0; // 0:EIS_LOWPASS; 1:EIS_AVERAGE 2:EIS_POLYFIT1
        pVideoEISData->mEisCfg.g_en_angle_filter = 0;
        break;
    case 1600:
    case 1920:
        //pVideoEISData->mEisCfg.radius[0]         = 17;
        //pVideoEISData->mEisCfg.radius[1]         = 2;
        //pVideoEISData->mEisCfg.radius[2]         = 1;
        //pVideoEISData->mEisCfg.max_frm_buf       = pVideoEISData->mEISAttr.iEisFilterWidth;
        //pVideoEISData->mEisCfg.filter_type       = 0; // 0:EIS_LOWPASS; 1:EIS_AVERAGE 2:EIS_POLYFIT1
        //pVideoEISData->mEisCfg.g_en_angle_filter = 0;
		pVideoEISData->mEisCfg.radius[1] = 2;
        pVideoEISData->mEisCfg.radius[2] = 1;
		pVideoEISData->mEisCfg.radius[0] = pVideoEISData->mEISAttr.iEisFilterWidth
                                            -pVideoEISData->mEisCfg.radius[1]-pVideoEISData->mEisCfg.radius[2];
        if (pVideoEISData->mEisCfg.radius[0] < pVideoEISData->mEisCfg.radius[1]) {
            aloge("You set a wrong input buffer[%d], use [%d] by default.", pVideoEISData->mEisCfg.radius[1]);
            pVideoEISData->mEisCfg.radius[0] = pVideoEISData->mEisCfg.radius[1];
		}
        break;
    case 640:
    default:
        pVideoEISData->mEisCfg.radius[0]         = 17;
        pVideoEISData->mEisCfg.radius[1]         = 1;
        pVideoEISData->mEisCfg.radius[2]         = 0;
        pVideoEISData->mEisCfg.max_frm_buf       = pVideoEISData->mEISAttr.iEisFilterWidth > 4 ? 4 : pVideoEISData->mEISAttr.iEisFilterWidth;
        pVideoEISData->mEisCfg.filter_type       = 0; // 0:EIS_LOWPASS; 1:EIS_AVERAGE 2:EIS_POLYFIT1
        pVideoEISData->mEisCfg.g_en_angle_filter = 0;
        break;
    }

    if (pVideoEISData->mEISAttr.iInputBufNum < pVideoEISData->mEISAttr.iEisFilterWidth)
        pVideoEISData->mEISAttr.bRetInFrmFast = 1;

    if (pVideoEISData->mEISAttr.bVerticalFlip && pVideoEISData->mEISAttr.bHorizFlip) {
        pVideoEISData->mEisPrivAttr.fGyroAxScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroAyScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroAzScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor = 1;
    } else if (!pVideoEISData->mEISAttr.bVerticalFlip && pVideoEISData->mEISAttr.bHorizFlip) {
        pVideoEISData->mEisPrivAttr.fGyroAxScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroAyScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroAzScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor = -1;
    } else if (pVideoEISData->mEISAttr.bVerticalFlip && !pVideoEISData->mEISAttr.bHorizFlip) {
        pVideoEISData->mEisPrivAttr.fGyroAxScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroAyScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroAzScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor = -1;
        pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor = -1;
    } else if (!pVideoEISData->mEISAttr.bVerticalFlip && !pVideoEISData->mEISAttr.bHorizFlip) {
        pVideoEISData->mEisPrivAttr.fGyroAxScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroAyScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroAzScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor = 1;
        pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor = 1;
    }

    pVideoEISData->mEisCfg.fps = pVideoEISData->mEISAttr.iVideoFps;
    pVideoEISData->mEisCfg.xy_exchange_en   = 1;
    pVideoEISData->mEisCfg.rolling_shutter  = 0;
    pVideoEISData->mEisCfg.fast_mode = 0;
    if (pVideoEISData->mEISAttr.eOperationMode == EIS_OPR_VGA30 || pVideoEISData->mEISAttr.eOperationMode == EIS_OPR_VGA60) {
        pVideoEISData->mEisCfg.filter_type = 0;
        pVideoEISData->mEisCfg.g_en_angle_filter = 0;
        pVideoEISData->mEisCfg.max_frm_buf = 1 + pVideoEISData->mEisCfg.radius[1] + pVideoEISData->mEisCfg.radius[2];
        pVideoEISData->mEisCfg.max_next_frm = pVideoEISData->mEisCfg.max_frm_buf - pVideoEISData->mEisCfg.radius[1] - pVideoEISData->mEisCfg.radius[2];
    } else if(pVideoEISData->mEISAttr.eOperationMode == EIS_OPR_1080P30 || pVideoEISData->mEISAttr.eOperationMode == EIS_OPR_1080P60){
	    pVideoEISData->mEisCfg.filter_type = 0;
        pVideoEISData->mEisCfg.g_en_angle_filter = 0;
		pVideoEISData->mEisCfg.max_frm_buf = pVideoEISData->mEisCfg.radius[0] + pVideoEISData->mEisCfg.radius[1] + pVideoEISData->mEisCfg.radius[2];
        pVideoEISData->mEisCfg.max_next_frm = pVideoEISData->mEisCfg.max_frm_buf - pVideoEISData->mEisCfg.radius[1] - pVideoEISData->mEisCfg.radius[2];
	}else {
        pVideoEISData->mEisCfg.max_next_frm = pVideoEISData->mEisCfg.max_frm_buf - pVideoEISData->mEisCfg.radius[1] - pVideoEISData->mEisCfg.radius[2];
}

    pVideoEISData->mEisCfg.rs_correction_en  = 1; /* Only when frame_rotation_en == 1, it make sense. */
    pVideoEISData->mEisCfg.frame_rotation_en = 1;
    pVideoEISData->mEisCfg.old_radius_th0 = 3;
    pVideoEISData->mEisCfg.old_radius_th1 = 5;

    _EisDumpHardwareLibConfigs(&pVideoEISData->mEisCfg);
}
#if 0
static void _EisDumpSoftwareLibConfigs(EISE_CFG_PARA *pEisCfg)
{
    printf("Dump EIS hardware driver config:\r\n");
    printf("\t in_w:\t%d\r\n", pEisCfg->in_w);
    printf("\t in_h:\t%d\r\n", pEisCfg->in_h);
    printf("\t out_h:\t%d\r\n", pEisCfg->out_h[0]);
    printf("\t out_w:\t%d\r\n", pEisCfg->out_w[0]);

    if (pEisCfg->in_yuv_type == YUV420) {
        printf("\t in_yuv_type:\tYUV420\r\n");
    } else if (pEisCfg->in_yuv_type == YVU420) {
        printf("\t in_yuv_type:\tYVU420\r\n");
    }

    if (pEisCfg->out_yuv_type == YUV420) {
        printf("\t out_yuv_type:\tYUV420\r\n");
    } else if (pEisCfg->out_yuv_type == YVU420) {
        printf("\t out_yuv_type:\tYVU420\r\n");
    }
    printf("\t in_luma_pitch:\t%d\r\n", pEisCfg->in_luma_pitch);
    printf("\t in_chroma_pitch:\t%d\r\n", pEisCfg->in_chroma_pitch);
    printf("\t out_luma_pitch:\t%d\r\n", pEisCfg->out_luma_pitch[0]);
    printf("\t out_chroma_pitch:\t%d\r\n", pEisCfg->out_chroma_pitch[0]);
    printf("\t input_width:\t%d\r\n", pEisCfg->input_width);
    printf("\t output_width:\t%d\r\n", pEisCfg->output_width);
    printf("\t output_height:\t%d\r\n", pEisCfg->output_height);
    printf("\t src_width:\t%d\r\n", pEisCfg->src_width);
    printf("\t src_height:\t%d\r\n", pEisCfg->src_height);
    printf("\t cut_height:\t%d\r\n", pEisCfg->cut_height);
    printf("\t rt:\t%f\r\n", pEisCfg->rt);
    printf("\t k_matrix[0]:\t%f\r\n", pEisCfg->k_matrix[0]);
    printf("\t k_matrix[1]:\t%f\r\n", pEisCfg->k_matrix[1]);
    printf("\t k_matrix[2]:\t%f\r\n", pEisCfg->k_matrix[2]);
    printf("\t k_matrix[3]:\t%f\r\n", pEisCfg->k_matrix[3]);

    printf("\t k_matrix[8]:\t%f\r\n", pEisCfg->k_matrix[8]);
    printf("\t ts:\t%f\r\n", pEisCfg->ts);
    printf("\t td:\t%f\r\n", pEisCfg->td);
    printf("\t stable_anglev[0]:\t%f\r\n", pEisCfg->stable_anglev[0]);
    printf("\t stable_anglev[1]:\t%f\r\n", pEisCfg->stable_anglev[1]);
    printf("\t stable_anglev[2]:\t%f\r\n", pEisCfg->stable_anglev[2]);

    printf("\t angle_th[0]:\t%f\r\n", pEisCfg->angle_th[0]);
    printf("\t angle_th[1]:\t%f\r\n", pEisCfg->angle_th[1]);
    printf("\t angle_th[2]:\t%f\r\n", pEisCfg->angle_th[2]);

    printf("\t radius[0]:\t%d\r\n", pEisCfg->radius[0]);
    printf("\t radius[1]:\t%d\r\n", pEisCfg->radius[1]);
    printf("\t radius[2]:\t%d\r\n", pEisCfg->radius[2]);

    printf("\t filter_type:\t%d\r\n", pEisCfg->filter_type);
    printf("\t xy_exchange_en:\t%d\r\n", pEisCfg->xy_exchange_en);
    printf("\t rolling_shutter:\t%d\r\n", pEisCfg->rolling_shutter);
}

static void VideoEisSetSwCfgs(VIDEOEISDATATYPE *pVideoEISData)
{
    float KmatK1 = 1680.0f;
    float KmatK2 = 1583.6f;
    float KmatKx = 1502.2f;
    float KmatKy = 863.9f;

    pVideoEISData->mSwEISCfg.frame_width              = 2496;
    pVideoEISData->mSwEISCfg.frame_height             = 1408;
    pVideoEISData->mSwEISCfg.frame_out2_width         = 0;
    pVideoEISData->mSwEISCfg.frame_out2_height        = 0;
    pVideoEISData->mSwEISCfg.frame_width_stride       = 2496;
    pVideoEISData->mSwEISCfg.frame_height_stride      = 1408;
	pVideoEISData->mSwEISCfg.max_yaw_degrees          = 8.5f;
	pVideoEISData->mSwEISCfg.max_pitch_degrees        = 4.5f;
	pVideoEISData->mSwEISCfg.max_roll_degrees         = 1.6f;
	pVideoEISData->mSwEISCfg.output_scale             = 1.0;
	pVideoEISData->mSwEISCfg.gyro_frequency_in_Hz     = (float)pVideoEISData->mEISAttr.iGyroFreq;
	pVideoEISData->mSwEISCfg.target_fps               = 30.0f;
	pVideoEISData->mSwEISCfg.number_of_input_buffers  = 5;
	pVideoEISData->mSwEISCfg.number_of_output_buffers = pVideoEISData->mEISAttr.iOutputBufBum;
	pVideoEISData->mSwEISCfg.operation_mode           = 3;
    pVideoEISData->mSwEISCfg.style                    = 0; //0:normal 1:motion-less

    switch (pVideoEISData->mEISAttr.iOperationMode) {
        case 3:
        case 13: {
            pVideoEISData->mSwEISCfg.frame_width              = 2496;
            pVideoEISData->mSwEISCfg.frame_height             = 1408;
            pVideoEISData->mSwEISCfg.frame_width_stride       = 2496;
            pVideoEISData->mSwEISCfg.frame_height_stride      = 1408;
        	pVideoEISData->mSwEISCfg.max_yaw_degrees          = 8.5f;
        	pVideoEISData->mSwEISCfg.max_pitch_degrees        = 4.5f;
        	pVideoEISData->mSwEISCfg.max_roll_degrees         = 1.6f;
        	pVideoEISData->mSwEISCfg.target_fps               = 30.0f;
        	pVideoEISData->mSwEISCfg.number_of_input_buffers  =
        	    (pVideoEISData->mEISAttr.iOperationMode == 3) ? 5 : 27;
        	pVideoEISData->mSwEISCfg.operation_mode = pVideoEISData->mEISAttr.iOperationMode;
        } break;
    }

    pVideoEISData->mSwEISCfg.frame_height

    _EisDumpSoftwareLibConfigs(&pVideoEISData->mEisCfg);
}
#endif

//#define DEBUG_GYRO_BUFFER
#ifdef DEBUG_GYRO_BUFFER
    FILE *pBufferInfo[2] = {NULL, NULL};
#endif

ERRORTYPE _VideoEisCreate(VIDEOEISDATATYPE *pVideoEISData)
{
    ERRORTYPE eError = SUCCESS;
    int iRet = 0;
    int i = 0;

    if (pVideoEISData->mEisHd && (EIS_ALGO_MODE_BP != pVideoEISData->mEISAttr.eEisAlgoMode)) {
        alogd("You have already create the EIS handle.");
        return SUCCESS;
    }

#ifdef DEBUG_GYRO_BUFFER
    /* create one file to store buffer information */
    char pBufferTmp[4096];
    char *pTmp = &pBufferTmp[0];

    iRet = sprintf(pTmp, "/mnt/extsd/gyro_buffer_info_%dx%d.txt",
        pVideoEISData->mEISAttr.iVideoInWidth, pVideoEISData->mEISAttr.iVideoInHeight);
    pTmp += iRet; *pTmp++ = '\0';

    pBufferInfo[pVideoEISData->mMppChnInfo.mChnId] = fopen(pBufferTmp, "w+");
    if (pBufferInfo[pVideoEISData->mMppChnInfo.mChnId] == NULL) {
        aloge("open file /mnt/extsd/gyro_buffer_info[%d].txt failed!!", pVideoEISData->mMppChnInfo.mChnId);
        return eError;
    }
#endif

    pVideoEISData->pInputBufMgr = VideoBufMgrCreate(pVideoEISData->mEISAttr.iInputBufNum, 0);
    if (NULL == pVideoEISData->pInputBufMgr) {
        aloge("Create EIS video input buffer manager failed.");
        goto ECrtInput;
    }
    eError = _EisCreateOutputBufPool(pVideoEISData);
    if (SUCCESS != eError) {
        aloge("Create EIS video output buffer pool failed.");
        goto ECrtOutputPool;
    }

    switch (pVideoEISData->mEISAttr.eEisAlgoMode) {
        case EIS_ALGO_MODE_SW: {
            aloge("You choice the software EIS, but it can't be use now, so use hardware EIS process.");
        }
        case EIS_ALGO_MODE_HW: {
            pVideoEISData->pstEISPkt = malloc(pVideoEISData->mEISAttr.iInputBufNum*sizeof(EISE_FrameData));
            if (NULL == pVideoEISData->pstEISPkt) {
                aloge("Alloc EIS process packet failed, errno %d.", errno);
                goto EAllocPkts;
            }

            memset(pVideoEISData->pstEISPkt, 0, pVideoEISData->mEISAttr.iInputBufNum*sizeof(EISE_FrameData));
            /* All packets has not be used */
            pVideoEISData->mEisPktMap = 0;
            pVideoEISData->pLastUsedEISPkt = NULL;
            for (i = 0; i < pVideoEISData->mEISAttr.iInputBufNum; i++)
                pVideoEISData->pstEISPkt[i].fid = i; // Not accumulate.
            VideoEisSetHwCfgs(pVideoEISData);
            iRet = EIS_Create(&pVideoEISData->mEisCfg, &pVideoEISData->mEisHd);
            if (iRet < 0) {
                aloge("Create EIS hardware handle failed. ret %d.", iRet);
                eError = ERR_EIS_FAILED_NOTENABLE;
                goto EEisCrt;
            }

            /* If there has no gyro hardware device, then only use unit matrix. */
            if (!pVideoEISData->bHasGyroDev && !pVideoEISData->mEISAttr.bSimuOffline)
                EIS_Set_UnitMatrix_Flag(&pVideoEISData->mEisHd, 0);

//            EIS_SetFrq(pVideoEISData->iEisHwFreq, &pVideoEISData->mEisHd);
        } break;

        case EIS_ALGO_MODE_BP: {
            char pBufferTmp[4096];
            char *pTmp = &pBufferTmp[0];
            static int iBPTestCnt = 0;

            alogw("You choice the by pass EIS mode, I will output the origin gyro and video buffer with no process.");
            pVideoEISData->bByPassMode = 1;
            pVideoEISData->pBPDataSavePath = pVideoEISData->mEISAttr.pBPDataSavePath;
            iRet = sprintf(pTmp, "%s/EISGyroDataCache%dx%dCnt%d.txt", pVideoEISData->pBPDataSavePath,
                    pVideoEISData->mEISAttr.iVideoOutWidth, pVideoEISData->mEISAttr.iVideoOutHeight, iBPTestCnt);
            pTmp += iRet; *pTmp++ = '\0';
            pVideoEISData->pGyroPtsDataFd = fopen(pBufferTmp, "w+");

            pTmp = &pBufferTmp[0];
            iRet = sprintf(pTmp, "%s/EISVideoPtsCache%dx%dCnt%d.txt", pVideoEISData->pBPDataSavePath,
                    pVideoEISData->mEISAttr.iVideoOutWidth, pVideoEISData->mEISAttr.iVideoOutHeight, iBPTestCnt);
            pTmp += iRet; *pTmp++ = '\0';
            pVideoEISData->pVideoPtsFd = fopen(pBufferTmp, "w+");

            if (pVideoEISData->mEISAttr.bSaveYUV) {
                pTmp = &pBufferTmp[0];
                iRet = sprintf(pTmp, "%s/EISpVideoDataCache%dx%dCnt%d.nv12", pVideoEISData->pBPDataSavePath,
                    pVideoEISData->mEISAttr.iVideoOutWidth, pVideoEISData->mEISAttr.iVideoOutHeight, iBPTestCnt);
                pTmp += iRet; *pTmp++ = '\0';
                pVideoEISData->pVideoDataFd = fopen(pBufferTmp, "w+");
            }
            if (!pVideoEISData->pGyroPtsDataFd || !pVideoEISData->pVideoPtsFd
                    || (!pVideoEISData->pVideoDataFd && pVideoEISData->mEISAttr.bSaveYUV)) {
                aloge("Open %s/xxx cache files failed, check if you have enough space or this path.\r\n", pVideoEISData->pBPDataSavePath);
                eError = ERR_EIS_FAILED_NOTENABLE;
                goto EOSaveFd;
            }
            if (pVideoEISData->mEISAttr.iVideoOutWidth == 800 && pVideoEISData->mEISAttr.iVideoOutHeight == 448)
                iBPTestCnt++;
        } break;

        default: {
            aloge("Wrong EIS algoram mode[%d], return.", pVideoEISData->mEISAttr.eEisAlgoMode);
            eError = ERR_EIS_INVALID_PARA;
        } break;
    }

    return eError;

EOSaveFd:
    if (pVideoEISData->pGyroPtsDataFd)
        fclose(pVideoEISData->pGyroPtsDataFd);
    if (pVideoEISData->pVideoPtsFd)
        fclose(pVideoEISData->pVideoPtsFd);
    if (pVideoEISData->pVideoDataFd)
        fclose(pVideoEISData->pVideoDataFd);
EEisCrt:
    _EisDestroyOutputBufPool(pVideoEISData);
ECrtOutputPool:
    free(pVideoEISData->pstEISPkt);
EAllocPkts:
    VideoBufMgrDestroy(pVideoEISData->pInputBufMgr);
ECrtInput:

    return eError;
}

ERRORTYPE _VideoEisDestroy(VIDEOEISDATATYPE *pVideoEISData)
{
    ERRORTYPE eError = SUCCESS;
    int iRet = 0;

    if (!pVideoEISData->mEisHd && (EIS_ALGO_MODE_BP != pVideoEISData->mEISAttr.eEisAlgoMode)) {
        alogd("You have already destroy the EIS handle.");
        return SUCCESS;
    }

    VideoBufMgrWaitUsingEmpty(pVideoEISData->pInputBufMgr);
    _EisDestroyOutputBufPool(pVideoEISData);
    switch (pVideoEISData->mEISAttr.eEisAlgoMode) {
        case EIS_ALGO_MODE_SW: // Until now, there has no real software EIS process mode.
        case EIS_ALGO_MODE_HW: {
            free(pVideoEISData->pstEISPkt);
            VideoBufMgrDestroy(pVideoEISData->pInputBufMgr);

            iRet = EIS_Destroy(&pVideoEISData->mEisHd);
            if (iRet < 0) {
                aloge("Destroy EIS hardware handle failed. ret %d.", iRet);
                eError = ERR_EIS_FAILED_NOTDISABLE;
            }
            pVideoEISData->mEisHd = NULL;
        } break;

        case EIS_ALGO_MODE_BP: {
            alogw("You choice the bypass EIS mode, and I save all useful datas.");
            pVideoEISData->bByPassMode = 0;
            pVideoEISData->pBPDataSavePath = NULL;
            if (pVideoEISData->pGyroPtsDataFd)
                fclose(pVideoEISData->pGyroPtsDataFd);
            if (pVideoEISData->pVideoPtsFd)
                fclose(pVideoEISData->pVideoPtsFd);
            if (pVideoEISData->pVideoDataFd)
                fclose(pVideoEISData->pVideoDataFd);
        } break;

        default: {
            aloge("Wrong EIS algoram mode[%d], return.", pVideoEISData->mEISAttr.eEisAlgoMode);
            eError = ERR_EIS_INVALID_PARA;
        } break;
    }

#ifdef DEBUG_GYRO_BUFFER
    fclose(pBufferInfo[pVideoEISData->mMppChnInfo.mChnId]);
#endif

    return eError;
}

static void _EisInitAttrs(VIDEOEISDATATYPE* pVideoEISData)
{
    pVideoEISData->iEisHwFreq = 696;
    pVideoEISData->mEISAttr.iGyroFreq     = 1000;
    pVideoEISData->mEISAttr.iGyroPoolSize = 1000 / pVideoEISData->mEISAttr.iGyroFreq * 1000;
    pVideoEISData->mEISAttr.iGyroAxiNum   = 3;
    pVideoEISData->mEISAttr.iVideoInWidth   = 1920;
    pVideoEISData->mEISAttr.iVideoOutHeight = 1080;
    pVideoEISData->mEISAttr.iVideoInWidthStride  = 1920;
    pVideoEISData->mEISAttr.iVideoInHeightStride = 1080;
    pVideoEISData->mEISAttr.iVideoOutWidth  = 1920;
    pVideoEISData->mEISAttr.iVideoOutHeight = 1080;
    pVideoEISData->mEISAttr.iVideoFps = 30;
    pVideoEISData->mEISAttr.eVideoFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pVideoEISData->mEISAttr.iInputBufNum  = 11;
    pVideoEISData->mEISAttr.iOutputBufBum = 10;
    pVideoEISData->mEISAttr.iEisFilterWidth = 8;
    pVideoEISData->mEISAttr.bRetInFrmFast = 0;
    pVideoEISData->mEISAttr.iDelayTimeMs  = 1000/pVideoEISData->mEISAttr.iVideoFps;
    pVideoEISData->mEISAttr.iSyncErrTolerance = 5; /* ms */
    pVideoEISData->mEISAttr.bUseKmat = 0;
    pVideoEISData->mEISAttr.stEisKmat.KmatK1 = 1283.8f;
    pVideoEISData->mEISAttr.stEisKmat.KmatK2 = 1208.2f;
    pVideoEISData->mEISAttr.stEisKmat.KmatKx = 1266.9f;
    pVideoEISData->mEISAttr.stEisKmat.KmatKy = 754.5f;

    pVideoEISData->mGyroAttr.dev_name = GYRO_DEV_NAME;
    pVideoEISData->mGyroAttr.dev_dir_path = GYRO_DEV_DIR_PATH;
    pVideoEISData->mGyroAttr.kfifo_len = 200;
    pVideoEISData->mGyroAttr.sample_freq = pVideoEISData->mEISAttr.iGyroFreq;
    pVideoEISData->mGyroAttr.axi_num = pVideoEISData->mEISAttr.iGyroAxiNum;
    pVideoEISData->mGyroAttr.force_open = 0;
    pVideoEISData->mGyroAttr.proc_mth = TS_AVERAGE_PROC;
    pVideoEISData->mGyroAttr.rb_len = pVideoEISData->mEISAttr.iGyroPoolSize;
}

ERRORTYPE VideoEISComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    int i = 0;
    ERRORTYPE eError = SUCCESS;

    MM_COMPONENTTYPE *pComp = (MM_COMPONENTTYPE *)hComponent;
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)malloc(sizeof(VIDEOEISDATATYPE));

    memset(pVideoEISData, 0x0, sizeof(VIDEOEISDATATYPE));
    pComp->pComponentPrivate = (void *)pVideoEISData;
    pVideoEISData->state = COMP_StateLoaded;
    pthread_mutex_init(&pVideoEISData->mStateLock, NULL);
    pthread_mutex_init(&pVideoEISData->mEISLock, NULL);
    pthread_mutex_init(&pVideoEISData->mInputFrmLock, NULL);
    pVideoEISData->hSelf = hComponent;

    cdx_sem_init(&pVideoEISData->mSemWaitOutFrame, 0);
    /* Fill callback function pointers */
    pComp->SetCallbacks = VideoEISSetCallbacks;
    pComp->SendCommand = VideoEISSendCommand;
    pComp->GetConfig = VideoEISGetConfig;
    pComp->SetConfig = VideoEISSetConfig;
    pComp->GetState = VideoEISGetState;
    pComp->ComponentTunnelRequest = VideoEISComponentTunnelRequest;
    pComp->ComponentDeInit = VideoEISComponentDeInit;
    pComp->EmptyThisBuffer = VideoEISEmptyThisBuffer;
    pComp->FillThisBuffer = VideoEISFillThisBuffer;

    /* Initialize component data structures to default values */
    pVideoEISData->sPortParam.nPorts = 0;
    pVideoEISData->sPortParam.nStartPortNumber = 0x0;

    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].bEnabled = TRUE;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].eDomain = COMP_PortDomainVideo;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].eDir = COMP_DirInput;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].format.video.cMIMEType = "YVU420";
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].format.video.nFrameWidth = 176;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].format.video.nFrameHeight = 144;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].format.video.eCompressionFormat = PT_BUTT;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_VIDEO_IN].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pVideoEISData->sPortBufSupplier[EIS_CHN_PORT_INDEX_VIDEO_IN].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortBufSupplier[EIS_CHN_PORT_INDEX_VIDEO_IN].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_VIDEO_IN].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_VIDEO_IN].eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoEISData->sPortParam.nPorts++;

    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].bEnabled = TRUE;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].eDomain = COMP_PortDomainVideo;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].eDir = COMP_DirInput;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].format.video.cMIMEType = "YVU420";
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].format.video.nFrameWidth = 176;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].format.video.nFrameHeight = 144;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].format.video.eCompressionFormat = PT_BUTT;  // YCbCr420;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_GYRO_IN].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pVideoEISData->sPortBufSupplier[EIS_CHN_PORT_INDEX_GYRO_IN].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortBufSupplier[EIS_CHN_PORT_INDEX_GYRO_IN].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_GYRO_IN].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_GYRO_IN].eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoEISData->sPortParam.nPorts++;

    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].bEnabled = TRUE;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].eDomain = COMP_PortDomainVideo;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].eDir = COMP_DirOutput;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].format.video.cMIMEType = "YVU420";
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].format.video.nFrameWidth = 176;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].format.video.nFrameHeight = 144;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].format.video.eCompressionFormat = PT_BUTT;  // YCbCr420;
    pVideoEISData->sPortDef[EIS_CHN_PORT_INDEX_OUT].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pVideoEISData->sPortBufSupplier[EIS_CHN_PORT_INDEX_OUT].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortBufSupplier[EIS_CHN_PORT_INDEX_OUT].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_OUT].nPortIndex = pVideoEISData->sPortParam.nPorts;
    pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_OUT].eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoEISData->sPortParam.nPorts++;

    /* EIS attribution initialize */
    _EisInitAttrs(pVideoEISData);

    if (message_create(&pVideoEISData->cmd_queue) < 0) {
        aloge("message error!");
        eError = ERR_VI_NOMEM;
        goto ECrtMsg;
    }

    /* Create the component thread to do video stabilization
    * the question is: should we open the gyro after create the gyro thread immediately? no.
    */
    if (pthread_create(&pVideoEISData->mEISTrd, NULL, EIS_CompStabThread, pVideoEISData)) {
        aloge("create EIS_CompStabThread fail!");
        eError = ERR_VI_NOMEM;
        goto ECrtStab;
    }

    alogv("VideoEIS component Init success!");
    return SUCCESS;

ECrtStab:
    message_destroy(&pVideoEISData->cmd_queue);
ECrtMsg:
    free(pVideoEISData);
    return eError;
}

ERRORTYPE VideoEISComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEOEISDATATYPE *pVideoEISData;
    message_t msg;
    struct list_head *pList;
    ERRORTYPE eError = SUCCESS;
    int cnt = 0;

    if (NULL == hComponent) {
        aloge("Fatel error: NULL pointer.");
        return ERR_EIS_NOT_PERM;
    }

    pVideoEISData = (VIDEOEISDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    CompInternalMsgType eCmd = Stop;
    msg.command = eCmd;
    put_message(&pVideoEISData->cmd_queue, &msg);
    pthread_join(pVideoEISData->mEISTrd, (void *)&eError);

    message_destroy(&pVideoEISData->cmd_queue);

    pthread_mutex_destroy(&pVideoEISData->mInputFrmLock);
    pthread_mutex_destroy(&pVideoEISData->mStateLock);
    pthread_mutex_destroy(&pVideoEISData->mEISLock);
    cdx_sem_deinit(&pVideoEISData->mSemWaitOutFrame);

    free(pVideoEISData);
    pVideoEISData = NULL;
    alogv("VideoEIS component exited!");

    return eError;
}

void _GyroDebugDumpAllChannelInfos(struct gyro_device_attr *gyro_attr)
{
    int i = 0;

    for (i = 0; i < gyro_attr->arry_elems; i++) {
        printf("Channel [%d]:\r\n", i);
        printf("\tName\t%s\r\n", gyro_attr->iio_chn[i].name);
        printf("\tGName\t%s\r\n", gyro_attr->iio_chn[i].generic_name);
        printf("\tScale\t%f\r\n", gyro_attr->iio_chn[i].scale);
        printf("\toffset\t%f\r\n", gyro_attr->iio_chn[i].offset);
        printf("\tindex\t%u\r\n", gyro_attr->iio_chn[i].index);
        printf("\tbytes\t%u\r\n", gyro_attr->iio_chn[i].bytes);
        printf("\tbitused\t%u\r\n", gyro_attr->iio_chn[i].bits_used);
        printf("\tshift\t%u\r\n", gyro_attr->iio_chn[i].shift);
        printf("\tbe\t%u\r\n", gyro_attr->iio_chn[i].be);
        printf("\tis_sign\t%u\r\n", gyro_attr->iio_chn[i].is_signed);
        printf("\tlocation\t%u\r\n\n", gyro_attr->iio_chn[i].location);
    }
}

static void *EIS_CompGyroThread(void *pThreadData)
{
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)pThreadData;

    int iGyroReadGrainSize = pVideoEISData->mEISAttr.iGyroFreq/pVideoEISData->mEISAttr.iVideoFps;
    int iGyroReadTimeIntervalUs = (1000/pVideoEISData->mEISAttr.iVideoFps)/2*1000;
    int iRet = 0;

    message_t cmd_msg;

    alogv("VideoEIS ComponentGyroThread start run...");
    prctl(PR_SET_NAME, "EIS_CompGyroThread", 0, 0, 0);

#if 0
    int ret = 0;
    cpu_set_t stCpuSet;
    memset(&stCpuSet, 0, sizeof(cpu_set_t));
    CPU_ZERO(&stCpuSet);
    CPU_SET(3, &stCpuSet);
    ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &stCpuSet);
    if (ret < 0) {
        aloge("sched_setaffinity failed!!\r\n");
    }
    alogv("sched_setaffinity for GyroHw_CapThread\r\n");
#endif

#if 0
    struct sched_param stSchedPara;
    stSchedPara.sched_priority = 1;
    sched_setscheduler(syscall(__NR_gettid), SCHED_RR, &stSchedPara);
//    sched_setparam(syscall(__NR_gettid), const struct sched_param *param);

    alogv("loop GyroHw_CapThread, freq:%d, one read sequence gyro number:%d.\r\n",
        pVideoEISData->mEISAttr.iGyroFreq, iGyroReadGrainSize);

//    _GyroDebugDumpAllChannelInfos(&pVideoEISData->mGyroAttr);
#endif
    while (pVideoEISData->bGyroRunFlag) {
#if 0
    	fd_set stRdFdSet;
    	struct timeval stTimeLimit;

    	FD_ZERO(&stRdFdSet);
    	FD_SET(pVideoEISData->mGyroAttr.gyro_fd, &stRdFdSet);

    	stTimeLimit.tv_sec = 2;
    	stTimeLimit.tv_usec = 0;

    	iRet = select(pVideoEISData->mGyroAttr.gyro_fd + 1, &stRdFdSet, NULL, NULL, &stTimeLimit);
    	if (iRet < 0) {
            aloge("Select wait for gyro datas failed, return %d.", -errno);
            continue;
    	} else if (0 == iRet) {
            aloge("Select wait for gyro datas timeout.");
            continue;
    	}

        iRet = _GyroHw_ReadParseRawDatas(pVideoEISData->GryoRingBufHd, &pVideoEISData->mGyroAttr,
            iGyroReadGrainSize, pVideoEISData->mEISAttr.iVideoFps);
        if (iRet == 0) {
            continue;
        }
#endif
        if (pVideoEISData->bByPassMode && pVideoEISData->state == COMP_StateExecuting) {
            int i = 0;
            char pBufferTmp[4096];
            int iStrLen = 0;

            for (i = 0; i < iGyroReadGrainSize; i++) {
                EIS_GYRO_PACKET_S stGyroPktOut;

                if (0 == ring_buffer_out(pVideoEISData->GryoRingBufHd, &stGyroPktOut, 1))
                    break;

                iStrLen = sprintf(&pBufferTmp[0], "%llu %f %f %f\n", stGyroPktOut.dTimeStamp,
                                stGyroPktOut.fAnglrVX, stGyroPktOut.fAnglrVY, stGyroPktOut.fAnglrVZ);
                pBufferTmp[iStrLen] = '\0';
                fwrite(&pBufferTmp[0], iStrLen, 1, pVideoEISData->pGyroPtsDataFd);
            }
        }

        /* Do not read too frequently */
        usleep(iGyroReadTimeIntervalUs/2);
    }

    return NULL;
}

#if 1

static inline void _DumpGyroDatas2File
    (VIDEOEISDATATYPE *pVideoEISData, void *pstGyroData, bool bFloatTs)
{
#ifdef DEBUG_GYRO_BUFFER
    int iRet = 0;
    char pBufferTmp[4096];
    char *pTmp = &pBufferTmp[0];
    EIS_GYRO_PACKET_S *pstGyroPkt = (EIS_GYRO_PACKET_S *)pstGyroData;

    if (bFloatTs)
        iRet = sprintf(pTmp, "%lf %f %f %f\n", (double)pstGyroPkt->dTimeStamp/1000000.0f,
            pstGyroPkt->fAnglrVX, pstGyroPkt->fAnglrVY, pstGyroPkt->fAnglrVZ);
    else
        iRet = sprintf(pTmp, "%llu %f %f %f\n", pstGyroPkt->dTimeStamp,
            pstGyroPkt->fAnglrVX, pstGyroPkt->fAnglrVY, pstGyroPkt->fAnglrVZ);

    fwrite(&pBufferTmp[0], 1, iRet, pBufferInfo[pVideoEISData->mMppChnInfo.mChnId]);
    pTmp = &pBufferTmp[0];
#endif
    return;
}

/*
*            Tsv            Tsv
*    Texp    /              /
* *---------*    *---------*
* |*---------*    *---------*
* | *---------*    *---------*
* |  *---------*    *---------*
* |  |
*  Pn(pack gyro data number, equal to [iVideoLineTime])
*/
static EISE_FrameData* _SyncAndPacketEisHardwareBuffer
    (VIDEOEISDATATYPE *pVideoEISData, VIDEO_FRAME_S *pstVFrm)
{
    int i = 0;
    int iCurPktIdx = 0;
    EISE_FrameData *pstEISFrmTmp = NULL;
    struct ring_buffer* pGyroRBHd = pVideoEISData->GryoRingBufHd;

#if 1
    /* We must can find one, do not worry it will be failure. */
    for (i = 0; i < pVideoEISData->mEISAttr.iInputBufNum; i++) {
        if (!(pVideoEISData->mEisPktMap & (1 << i))) {
            pstEISFrmTmp = &pVideoEISData->pstEISPkt[i];
            pVideoEISData->mEisPktMap |= (1 << i);
            iCurPktIdx = i;
            break;
        }
    }
#endif
    int iRet = 0;
    unsigned int iAbsOfGyroAndVideoTs = 0; /* Unit: ms */
    EIS_GYRO_PACKET_S stGyroPkt;
    /* Use delay time to fix video data's timestamp. */
    uint64_t dFixedVideoPts = 0; /* Unit: us */
    uint64_t dVideoFrameIntervalMs = 1000/pVideoEISData->mEISAttr.iVideoFps; /* Unit: ms */
    uint64_t dPacktEndTimeStamp = 0, dPacktStartTimeStamp = 0; /* Unit: us */
    uint64_t dGyroIntervalUs = 1000000/pVideoEISData->mGyroAttr.sample_freq;
    uint64_t dGyroDataExtendIntervalUs = 0;
    int iNeedGyroDataNum = pVideoEISData->mGyroAttr.sample_freq/pVideoEISData->mEISAttr.iVideoFps;
    int iFixedDelayTime = 0;

    if (pstVFrm->mExposureTime > dVideoFrameIntervalMs)
        pstVFrm->mExposureTime = dVideoFrameIntervalMs;

    /* We use the half of exposure time te be the end of packet node. */
    iFixedDelayTime = pVideoEISData->mEISAttr.iDelayTimeMs + pstVFrm->mExposureTime/2;

    dFixedVideoPts = (iFixedDelayTime > 0)
        ? pstVFrm->mpts - iFixedDelayTime*1000
        : pstVFrm->mpts + abs(iFixedDelayTime)*1000;

//    printf("EIS:[%u]\n", pstVFrm->mExposureTime);
    dPacktEndTimeStamp = dFixedVideoPts - dVideoFrameIntervalMs*1000
        + (uint64_t)(pVideoEISData->mEisCfg.td*1000000.0f);

    dPacktStartTimeStamp = dPacktEndTimeStamp - dVideoFrameIntervalMs*1000;

#ifdef DEBUG_GYRO_BUFFER
    char pBufferTmp[4096];
    char *pTmp = &pBufferTmp[0];

    iRet = sprintf(pTmp, "Get one video pst[%lld], fixed[%lld], exp_time %u",
                pstVFrm->mpts, dPacktStartTimeStamp, pstVFrm->mExposureTime);
    pTmp += iRet;
    *pTmp++ = '\n';
    fwrite(&pBufferTmp[0], 1, (pTmp-&pBufferTmp[0]), pBufferInfo[pVideoEISData->mMppChnInfo.mChnId]);
    pTmp = &pBufferTmp[0];
#endif

    i = 0;
    while (1) {
        /* First of all, we will past the all old gyro data packets,
        * find the nextest start one gyro data with timestamp.
        */
        if (ring_buffer_out(pGyroRBHd, &stGyroPkt, 1) <= 0)
            goto EEmptyRB;

        _DumpGyroDatas2File(pVideoEISData, &stGyroPkt, 1);

        /* The oldest gyro data value is too fresh,
        * even it is low probability, but we still catch it. */
        if (stGyroPkt.dTimeStamp >= dPacktEndTimeStamp)
            goto EEmptyRB;

        iAbsOfGyroAndVideoTs = llabs(dPacktStartTimeStamp - stGyroPkt.dTimeStamp);
        if (iAbsOfGyroAndVideoTs <= pVideoEISData->mEISAttr.iSyncErrTolerance*1000
                || stGyroPkt.dTimeStamp >= dPacktStartTimeStamp) {
            pstEISFrmTmp->gyro_data[i].time = stGyroPkt.dTimeStamp;
            pstEISFrmTmp->gyro_data[i].ax = stGyroPkt.fAccelVX*pVideoEISData->mEisPrivAttr.fGyroAxScaleFactor;
            pstEISFrmTmp->gyro_data[i].ay = stGyroPkt.fAccelVY*pVideoEISData->mEisPrivAttr.fGyroAyScaleFactor;
            pstEISFrmTmp->gyro_data[i].az = stGyroPkt.fAccelVZ*pVideoEISData->mEisPrivAttr.fGyroAzScaleFactor;
            pstEISFrmTmp->gyro_data[i].vx = stGyroPkt.fAnglrVX*pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor;
            pstEISFrmTmp->gyro_data[i].vy = stGyroPkt.fAnglrVY*pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor;
            pstEISFrmTmp->gyro_data[i].vz = stGyroPkt.fAnglrVZ*pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor;

            _DumpGyroDatas2File(pVideoEISData, &stGyroPkt, 0);

            pstEISFrmTmp->gyro_data[i].vx -= pVideoEISData->mEisCfg.stable_anglev[0]*pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor;
            pstEISFrmTmp->gyro_data[i].vy -= pVideoEISData->mEisCfg.stable_anglev[1]*pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor;
            pstEISFrmTmp->gyro_data[i].vz -= pVideoEISData->mEisCfg.stable_anglev[2]*pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor;
            break;
        }
    }
    i++;

    while (1) {
         /* If we got one gyro packet success,
         * then use it whatever, even it may not too valid. */
        if (ring_buffer_out(pGyroRBHd, &stGyroPkt, 1) <= 0) {
            aloge("Empty.");
            goto EEmptyRB;
        }

        pstEISFrmTmp->gyro_data[i].time = stGyroPkt.dTimeStamp;
        pstEISFrmTmp->gyro_data[i].ax = stGyroPkt.fAccelVX*pVideoEISData->mEisPrivAttr.fGyroAxScaleFactor;
        pstEISFrmTmp->gyro_data[i].ay = stGyroPkt.fAccelVY*pVideoEISData->mEisPrivAttr.fGyroAyScaleFactor;
        pstEISFrmTmp->gyro_data[i].az = stGyroPkt.fAccelVZ*pVideoEISData->mEisPrivAttr.fGyroAzScaleFactor;
        pstEISFrmTmp->gyro_data[i].vx = stGyroPkt.fAnglrVX*pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor;
        pstEISFrmTmp->gyro_data[i].vy = stGyroPkt.fAnglrVY*pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor;
        pstEISFrmTmp->gyro_data[i].vz = stGyroPkt.fAnglrVZ*pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor;

        _DumpGyroDatas2File(pVideoEISData, &stGyroPkt, 0);

        pstEISFrmTmp->gyro_data[i].vx -= pVideoEISData->mEisCfg.stable_anglev[0]*pVideoEISData->mEisPrivAttr.fGyroVxScaleFactor;
        pstEISFrmTmp->gyro_data[i].vy -= pVideoEISData->mEisCfg.stable_anglev[1]*pVideoEISData->mEisPrivAttr.fGyroVyScaleFactor;
        pstEISFrmTmp->gyro_data[i].vz -= pVideoEISData->mEisCfg.stable_anglev[2]*pVideoEISData->mEisPrivAttr.fGyroVzScaleFactor;
        i++;

        /* Keep one newest timestamp for the next sync */
        if (dPacktEndTimeStamp < stGyroPkt.dTimeStamp
            || (i >= sizeof(pstEISFrmTmp->gyro_data)/sizeof(pstEISFrmTmp->gyro_data[0])))
            break; // We got enough datas
    }

EEmptyRB:
    pVideoEISData->iGVSyncPktId++;
    if (i == 0) {
        if (NULL == pVideoEISData->pLastUsedEISPkt) {
            /* This mean we got an empty buffer packets, construct it. */
            aloge("Got zero gyro buffer datas. video pts[%llu], construct it.", pstVFrm->mpts);
            memset(&pstEISFrmTmp->gyro_data[0], 0, sizeof(pstEISFrmTmp->gyro_data));

            pstEISFrmTmp->gyro_data[0].time = dPacktStartTimeStamp;

            dGyroDataExtendIntervalUs = (dPacktEndTimeStamp - dPacktStartTimeStamp)/(iNeedGyroDataNum-1);
            pstEISFrmTmp->gyro_data[0].ax = 0.0f;
            pstEISFrmTmp->gyro_data[0].ay = 0.0f;
            pstEISFrmTmp->gyro_data[0].az = 0.0f;
            pstEISFrmTmp->gyro_data[0].vx = 0.0f;//-0.005f;
            pstEISFrmTmp->gyro_data[0].vy = 0.0f;//-0.002f;
            pstEISFrmTmp->gyro_data[0].vz = 0.0f;//0.004f;

            _DumpGyroDatas2File(pVideoEISData, &pstEISFrmTmp->gyro_data[0], 0);

            while (1) {
                i++;
                pstEISFrmTmp->gyro_data[i].time = pstEISFrmTmp->gyro_data[i-1].time + dGyroDataExtendIntervalUs;
                pstEISFrmTmp->gyro_data[i].ax = 0;
                pstEISFrmTmp->gyro_data[i].ay = 0;
                pstEISFrmTmp->gyro_data[i].az = 0;
                pstEISFrmTmp->gyro_data[i].vx = 0.0f;//-0.005f;
                pstEISFrmTmp->gyro_data[i].vy = 0.0f;//-0.002f;
                pstEISFrmTmp->gyro_data[i].vz = 0.0f;//0.004f;
                _DumpGyroDatas2File(pVideoEISData, &pstEISFrmTmp->gyro_data[i], 0);
                if (pstEISFrmTmp->gyro_data[i].time > dPacktEndTimeStamp)
                    break;
            }
        } else {
            if (pVideoEISData->bHasGyroDev && pVideoEISData->iGVSyncPktId % pVideoEISData->mEISAttr.iVideoFps == 0)
                /* This mean we got an empty buffer packets. then use the last gyro data one */
                aloge("Got zero gyro buffer datas. video pts[%llu], last pts[%llu], ring buffer kepp[%d].",
                    pstVFrm->mpts,
                    pVideoEISData->pLastUsedEISPkt->gyro_data[pVideoEISData->pLastUsedEISPkt->gyro_num-1].time,
                    ring_buffer_g_validnum(pGyroRBHd));

            /* Copy all old gyro buffers, and change its timestamps */
            memcpy(&pstEISFrmTmp->gyro_data[0],
                &pVideoEISData->pLastUsedEISPkt->gyro_data[0], sizeof(pstEISFrmTmp->gyro_data));

            pstEISFrmTmp->gyro_num = pVideoEISData->pLastUsedEISPkt->gyro_num;

            pstEISFrmTmp->gyro_data[0].time = dPacktStartTimeStamp;
            dGyroDataExtendIntervalUs = (dPacktEndTimeStamp - dPacktStartTimeStamp)/(pstEISFrmTmp->gyro_num-1);

            if (pVideoEISData->bHasGyroDev && pVideoEISData->iGVSyncPktId % pVideoEISData->mEISAttr.iVideoFps == 0)
                aloge("Got zero gyro buffer datas. video pts0[%llu], num[%d].",
                        pstEISFrmTmp->gyro_data[0].time, pstEISFrmTmp->gyro_num);

            /* Fix timestamps */
            while (1) {
                i++;
                pstEISFrmTmp->gyro_data[i].time = pstEISFrmTmp->gyro_data[i-1].time + dGyroDataExtendIntervalUs;
                if (pstEISFrmTmp->gyro_data[i].time > dPacktEndTimeStamp)
                    break;
            }
        }
    } else if (i < iNeedGyroDataNum/4) {
        // TODO:
        /* Too less packets, shoudle we use the last one? */
        /* Should copy the last gyro data to pad the gyro data number, but increase timestamp */
        /* Just keep the exist program, if can not support, then change it. */
        aloge("Too less gyro buffer datas %d, ring buffer keep[%d], then extend to %d datas.\n",
                i, ring_buffer_g_validnum(pGyroRBHd), iNeedGyroDataNum);
#if 0
        dGyroDataExtendIntervalUs = (dPacktEndTimeStamp - pstEISFrmTmp->gyro_data[i-1].time)/(iNeedGyroDataNum-i);
        for (; i < iNeedGyroDataNum; i++) {
            pstEISFrmTmp->gyro_data[i].time = pstEISFrmTmp->gyro_data[i-1].time + dGyroDataExtendIntervalUs;
            pstEISFrmTmp->gyro_data[i].ax = pstEISFrmTmp->gyro_data[i-1].ax;
            pstEISFrmTmp->gyro_data[i].ay = pstEISFrmTmp->gyro_data[i-1].ay;
            pstEISFrmTmp->gyro_data[i].az = pstEISFrmTmp->gyro_data[i-1].az;
            pstEISFrmTmp->gyro_data[i].vx = pstEISFrmTmp->gyro_data[i-1].vx;
            pstEISFrmTmp->gyro_data[i].vy = pstEISFrmTmp->gyro_data[i-1].vy;
            pstEISFrmTmp->gyro_data[i].vz = pstEISFrmTmp->gyro_data[i-1].vz;
        }
#endif
    }
    pVideoEISData->pLastUsedEISPkt = pstEISFrmTmp;
    pVideoEISData->dLastInputFrmPts = dPacktEndTimeStamp;

    pstEISFrmTmp->frame_stamp = dPacktEndTimeStamp;
    pstEISFrmTmp->gyro_num = i;
#ifdef DEBUG_GYRO_BUFFER
    iRet = sprintf(pTmp, "We got video timestamp[%llu], gyro_num[%d]", pstEISFrmTmp->frame_stamp, i);
    pTmp += iRet;
    *pTmp++ = '\n';
    fwrite(&pBufferTmp[0], 1, (pTmp-&pBufferTmp[0]), pBufferInfo[pVideoEISData->mMppChnInfo.mChnId]);
    pTmp = &pBufferTmp[0];

    if (i < iNeedGyroDataNum/2 || i > iNeedGyroDataNum*3/2) {
        iRet = sprintf(pTmp, "Toooooooo small or long data num[%d]", i);
        pTmp += iRet;
        *pTmp++ = '\n';
        fwrite(&pBufferTmp[0], 1, (pTmp-&pBufferTmp[0]), pBufferInfo[pVideoEISData->mMppChnInfo.mChnId]);
        pTmp = &pBufferTmp[0];
    }
#endif

    return pstEISFrmTmp;
}
#endif
static void *EIS_CompStabThread(void *pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    VIDEOEISDATATYPE *pVideoEISData = (VIDEOEISDATATYPE *)pThreadData;
    COMP_INTERNAL_TUNNELINFOTYPE *pOutputPort = &pVideoEISData->sPortTunnelInfo[EIS_CHN_PORT_INDEX_OUT];
    message_t cmd_msg;
    ERRORTYPE eError = 0;
    int iRet = 0;

#if 1
    if (pVideoEISData->mEISAttr.eOperationMode != EIS_OPR_VGA30 && pVideoEISData->mEISAttr.eOperationMode != EIS_OPR_VGA60) {
        struct sched_param stSchedPara;
        stSchedPara.sched_priority = 20;
        if (pthread_setschedparam(pthread_self(), SCHED_RR, &stSchedPara))
            aloge("Set EIS_CompStabThread into SCHED_RR failed, %s.", strerror(errno));
    }
#endif

    alogv("VideoEIS ComponentStabThread start run...");
    prctl(PR_SET_NAME, "EIS_CompStabThread", 0, 0, 0);

    while (1) {
    PROCESS_MESSAGE:
        if (get_message(&pVideoEISData->cmd_queue, &cmd_msg) == 0) {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;
            // alogv("VideoVi ComponentThread get_message cmd:%d", cmd);
            if (cmd == SetState) {
                pthread_mutex_lock(&pVideoEISData->mStateLock);
                if (pVideoEISData->state == (COMP_STATETYPE)(cmddata)) {
                    pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData, COMP_EventError,
                                                           ERR_VI_SAMESTATE, 0, NULL);
                } else {
                    switch ((COMP_STATETYPE)(cmddata)) {
                        case COMP_StateInvalid: {
                            pVideoEISData->state = COMP_StateInvalid;
                            // CompSendEvent(pVideoEISData->hSelf, pVideoEISData->pAppData, COMP_EventError, ERR_VI_INVALIDSTATE, 0);
                            pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                   COMP_EventError, ERR_VI_INVALIDSTATE, 0, NULL);
                            pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                   COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                   pVideoEISData->state, NULL);
                        } break;
                        case COMP_StateLoaded: {
                            if (pVideoEISData->state != COMP_StateIdle) {
                                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            pVideoEISData->mWaitAllFrameReleaseFlag = 1;

                            DoVideoEisReturnBackAllInputFrames(pVideoEISData);

                            if (!list_empty(&pVideoEISData->mOutUsedList)) {
                                aloge("Wait EIS component output buffer used frame list empty.");
                                while (!list_empty(&pVideoEISData->mOutUsedList)) {usleep(10*1000);};
                            }
                            VideoBufMgrWaitUsingEmpty(pVideoEISData->pInputBufMgr);
                            alogd("Wait all EIS component output using frame return done.");

                            pVideoEISData->mWaitAllFrameReleaseFlag = 0;
                            pVideoEISData->state = COMP_StateLoaded;
                            _VideoEisDestroy(pVideoEISData);
                            alogv("Set EIS OMX_StateLoaded OK");
                            pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                   COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                   pVideoEISData->state, NULL);
                        } break;
                        case COMP_StateIdle: {
                            if (pVideoEISData->state == COMP_StateLoaded) {
                                alogv("video VI: loaded->idle ...");
                                pVideoEISData->state = COMP_StateIdle;
                                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf,
                                                                pVideoEISData->pAppData,
                                                                COMP_EventCmdComplete,
                                                                COMP_CommandStateSet,
                                                                pVideoEISData->state, NULL);
                            } else if (pVideoEISData->state == COMP_StatePause ||
                                            pVideoEISData->state == COMP_StateExecuting) {
                                alogv("video vi: pause/executing[0x%x]->idle ...", pVideoEISData->state);
                                //release all frames to video input.
                                if(!VideoBufMgrUsingEmpty(pVideoEISData->pInputBufMgr)) {
                                    alogw("Fatal warning! using frame is not empty! check code!");
                                }

                                pVideoEISData->state = COMP_StateIdle;
                                alogv("Set EIS COMP_StateIdle OK");
                                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                       COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                       pVideoEISData->state, NULL);
                            } else {
                                aloge("Fatal error! current state[0x%x] can't turn to idle!", pVideoEISData->state);
                                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                        } break;
                        case COMP_StateExecuting: {  // Transition can only happen from pause or idle state
                            if (pVideoEISData->state == COMP_StateIdle || pVideoEISData->state == COMP_StatePause) {
                                eError = _VideoEisCreate(pVideoEISData);
                                if (SUCCESS != eError) {
                                    pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf,
                                                                pVideoEISData->pAppData,
                                                                COMP_EventError,
                                                                COMP_CommandStateSet,
                                                                eError, NULL);
                                    aloge("Convert status Idle->Executing failed because open hw EIS failed. ret 0x%x.", eError);
                                } else {
                                    pVideoEISData->state = COMP_StateExecuting;
                                    alogv("Set Virvi COMP_StateExecuting OK");
                                    pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf,
                                                                pVideoEISData->pAppData,
                                                                COMP_EventCmdComplete,
                                                                COMP_CommandStateSet,
                                                                pVideoEISData->state, NULL);
                                }
                            } else {
                                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                        } break;
                        case COMP_StatePause: {
                            /* Transition can only happen from idle or executing state */
                            if (pVideoEISData->state == COMP_StateIdle || pVideoEISData->state == COMP_StateExecuting) {
                                pVideoEISData->state = COMP_StatePause;
                                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                       COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                       pVideoEISData->state, NULL);
                            } else {
                                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                        } break;
                        default: break;
                    }
                }
                pthread_mutex_unlock(&pVideoEISData->mStateLock);
            } else if (cmd == Flush) {
                pVideoEISData->pCallbacks->EventHandler(pVideoEISData->hSelf, pVideoEISData->pAppData,
                                                       COMP_EventCmdComplete, COMP_CommandFlush,
                                                       0, NULL);
            } else if (cmd == Stop) {
                /* Kill thread */
                goto EXIT;
            } else if(cmd == EisComp_InputFrameAvailable) {
                alogv("(f:%s, l:%d) frame input", __FUNCTION__, __LINE__);
            } else if(cmd == EisComp_StoreFrame) {
                pVideoEISData->bStoreFrame = TRUE;
                snprintf(pVideoEISData->mDbgStoreFrameFilePath, sizeof(pVideoEISData->mDbgStoreFrameFilePath), "%s", (char*)cmd_msg.mpData);
                /* mpData is malloced in TMessageDeepCopyMessage() */
                free(cmd_msg.mpData);
                cmd_msg.mpData = NULL;
                cmd_msg.mDataSize = 0;
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        int eError;
        if (pVideoEISData->state == COMP_StateExecuting) {
            int iInputFrmId = 0;

            /* Find one input buffer from input buffer list, do gyro sync
            * and fetch one output buffer from output buffer list.
            */

            /* Get one video buffer and do one sync operation. check if sync successful.
            * 1.if there has no gyro data can be synced, then use the last one.
            */
            VIDEO_FRAME_INFO_S *pVInFrm;
            pVInFrm = VideoBufMgrGetValidFrame(pVideoEISData->pInputBufMgr);
            if (NULL == pVInFrm) {
                /* Maybe this thread was cut out, and [VideoBufMgrPushFrame] was invoked,
                * but the [pVideoEISData->bWaitingInputFrmFlag] was not set,
                * then this thread will be get into stall. so after set [bWaitingInputFrmFlag],
                * check if we haven't got valid buffer yet, if not, then just sleep, do not worry it will be stall. */
                pVideoEISData->bWaitingInputFrmFlag = TRUE;
                pVInFrm = VideoBufMgrGetValidFrame(pVideoEISData->pInputBufMgr);
                if (NULL == pVInFrm) {
                    TMessage_WaitQueueNotEmpty(&pVideoEISData->cmd_queue, 0);
                    pVideoEISData->bWaitingInputFrmFlag = FALSE;
                    goto PROCESS_MESSAGE;
                } else
                    pVideoEISData->bWaitingInputFrmFlag = FALSE;
            }

            VIDEO_FRAME_INFO_S *pVideoInBufTmp = NULL;
            VIDEO_FRAME_INFO_S stVideoInBufRetTmp;
            /* If is bypass mode, then save all datas in specific path. */
            if (pVideoEISData->bByPassMode) {
                static uint64_t dCacheFrmCnt = 0;

                EIS_GYRO_PACKET_S stGyroPkt;
                char pBufferTmp[4096];
                int iStrLen = 0;
                pVideoInBufTmp = VideoBufMgrGetSpecUsingFrameWithAddr(
                                    pVideoEISData->pInputBufMgr, pVInFrm->VFrame.mpVirAddr[0]);
                if (pVideoInBufTmp) {
                    pVideoEISData->iCacheBufferNum++;
                    iStrLen = sprintf(&pBufferTmp[0], "%llu %u %d\n", pVideoInBufTmp->VFrame.mpts,
                            pVideoInBufTmp->VFrame.mExposureTime, pVideoEISData->iCacheBufferNum);
                    pBufferTmp[iStrLen] = '\0';
                    fwrite(&pBufferTmp[0], iStrLen, 1, pVideoEISData->pVideoPtsFd);

                    if (pVideoEISData->mEISAttr.bSaveYUV) {
                        fwrite(pVideoInBufTmp->VFrame.mpVirAddr[0],
                                pVideoInBufTmp->VFrame.mHeight*pVideoInBufTmp->VFrame.mWidth, 1, pVideoEISData->pVideoDataFd);
                        fwrite(pVideoInBufTmp->VFrame.mpVirAddr[1],
                                pVideoInBufTmp->VFrame.mHeight*pVideoInBufTmp->VFrame.mWidth/2, 1, pVideoEISData->pVideoDataFd);
                    } else {
                        VideoFrameListInfo *pVFrmOutputCur;
                        pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
                        pVFrmOutputCur = list_first_entry(&pVideoEISData->mOutIdleList, VideoFrameListInfo, mList);
                        ion_flushCache(pVideoInBufTmp->VFrame.mpVirAddr[0],
                            pVFrmOutputCur->mFrame.VFrame.mWidth*pVFrmOutputCur->mFrame.VFrame.mHeight);
                        ion_flushCache(pVideoInBufTmp->VFrame.mpVirAddr[1],
                            pVFrmOutputCur->mFrame.VFrame.mWidth*pVFrmOutputCur->mFrame.VFrame.mHeight/2);
                        /* If we only have one output idle video frame, then keep it. */
                        if (pVideoEISData->iOutFrmIdleCnt > 1) {
                            list_move_tail(&pVFrmOutputCur->mList, &pVideoEISData->mOutValidList);
                            pVideoEISData->iOutFrmIdleCnt--;
                        } else
                            aloge("We has only one idle frame in EIS output frame list.");
                        pVFrmOutputCur->mFrame.VFrame.mPhyAddr[0] = pVideoInBufTmp->VFrame.mPhyAddr[0];
                        pVFrmOutputCur->mFrame.VFrame.mPhyAddr[1] = pVideoInBufTmp->VFrame.mPhyAddr[1];
                        pVFrmOutputCur->mFrame.VFrame.mpts = pVideoInBufTmp->VFrame.mpts;
                        pVFrmOutputCur->mFrame.VFrame.mExposureTime = pVideoInBufTmp->VFrame.mExposureTime;
#if 1
#define POINT_BEGINX 100
#define POINT_BEGINY 50
#define POINT_SIZE 3
#define POINT_LINE_LEN 40
#define POINT_GRP_SIZE 5
                    {
                        int i = 0, j = 0, k = 0;
                        char *pDrawPosBegin = (char *)pVideoInBufTmp->VFrame.mpVirAddr[0]
                            + pVideoInBufTmp->VFrame.mWidth*POINT_BEGINY+POINT_BEGINX;
                        char *pDrawPos = NULL;
                        for (i = 0; i < pVideoEISData->iCacheBufferNum; i++) {
                            pDrawPos = pDrawPosBegin
                                + (i/POINT_LINE_LEN)*POINT_SIZE*2*pVideoInBufTmp->VFrame.mWidth
                                + (i%POINT_LINE_LEN)*POINT_SIZE*2
                                + (i%POINT_LINE_LEN/POINT_GRP_SIZE*POINT_SIZE);

                            for (j = 0; j < POINT_SIZE; j++) {
                                for (k = 0; k < POINT_SIZE; k++) {
                                    pDrawPos[j*pVideoInBufTmp->VFrame.mWidth+k] = ~pDrawPos[j*pVideoInBufTmp->VFrame.mWidth+k];
                                }
                            }
                        }
                        ion_flushCache(pVideoInBufTmp->VFrame.mpVirAddr[0],
                            pVFrmOutputCur->mFrame.VFrame.mWidth*pVFrmOutputCur->mFrame.VFrame.mHeight);
                        ion_flushCache(pVideoInBufTmp->VFrame.mpVirAddr[1],
                            pVFrmOutputCur->mFrame.VFrame.mWidth*pVFrmOutputCur->mFrame.VFrame.mHeight/2);
                    }
#endif

                        pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);
                        if (pVideoEISData->bWaitingOutFrmFlag && pVideoEISData->iOutFrmIdleCnt >= 1)
                            cdx_sem_up(&pVideoEISData->mSemWaitOutFrame);
                    }

                    /* Return input buffer to last component, must invoke [DoVideoEisSendBackInputFrame] first */
                    stVideoInBufRetTmp = *pVideoInBufTmp;
                    VideoBufMgrReleaseFrame(pVideoEISData->pInputBufMgr, pVideoInBufTmp);
                    DoVideoEisSendBackInputFrame(pVideoEISData, &stVideoInBufRetTmp);
                }
                goto PROCESS_MESSAGE;
            }

            VideoFrameListInfo *pVFrmOutputCur;
            pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
            pVFrmOutputCur = list_first_entry(&pVideoEISData->mOutIdleList, VideoFrameListInfo, mList);
//            ion_flushCache(pVFrmOutputCur->mFrame.VFrame.mpVirAddr[0],
//                pVFrmOutputCur->mFrame.VFrame.mWidth*pVFrmOutputCur->mFrame.VFrame.mHeight);
//            ion_flushCache(pVFrmOutputCur->mFrame.VFrame.mpVirAddr[1],
//                pVFrmOutputCur->mFrame.VFrame.mWidth*pVFrmOutputCur->mFrame.VFrame.mHeight/2);
            pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);

            /* Set the next output address. */
            EIS_PROCOUT_PARA stOutputPara;
            memset(&stOutputPara, 0, sizeof(EIS_PROCOUT_PARA));
            stOutputPara.out_luma_mmu_Addr = pVFrmOutputCur->mFrame.VFrame.mpVirAddr[0];
            stOutputPara.out_luma_phy_Addr = pVFrmOutputCur->mFrame.VFrame.mPhyAddr[0];
            stOutputPara.out_chroma_u_mmu_Addr = pVFrmOutputCur->mFrame.VFrame.mpVirAddr[1];
            stOutputPara.out_chroma_u_phy_Addr = pVFrmOutputCur->mFrame.VFrame.mPhyAddr[1];
            EIS_setOutputAddr(&pVideoEISData->mEisHd, &stOutputPara);

            EISE_FrameData* pEisProcPkt;
            int iEisProcRet = 0;

            pEisProcPkt = _SyncAndPacketEisHardwareBuffer(pVideoEISData, &pVInFrm->VFrame);
//            pEisProcPkt->frame_stamp = pVInFrm->VFrame.mpts;
            pEisProcPkt->texp = (float)pVInFrm->VFrame.mExposureTime/1000.0f;
//            aloge("Send video pts: %f %f gyro_num[%d] %f %f %f %f to process.",
//                pEisProcPkt->frame_stamp, pEisProcPkt->texp, pEisProcPkt->gyro_num, pEisProcPkt->gyro_data[0].time,
//                pEisProcPkt->gyro_data[0].vx, pEisProcPkt->gyro_data[0].vy, pEisProcPkt->gyro_data[0].vz);
            pEisProcPkt->in_addr.in_luma_mmu_Addr = pVInFrm->VFrame.mpVirAddr[0];
            pEisProcPkt->in_addr.in_luma_phy_Addr = pVInFrm->VFrame.mPhyAddr[0];
            pEisProcPkt->in_addr.in_chroma_mmu_Addr = pVInFrm->VFrame.mpVirAddr[1];
            pEisProcPkt->in_addr.in_chroma_phy_Addr = pVInFrm->VFrame.mPhyAddr[1];
#if 0
            static unsigned long long iSelfProcTest = 0;
            if (iSelfProcTest >= pVideoEISData->mEISAttr.iInputBufNum-3) {
                iGetFrmId = iSelfProcTest % (pVideoEISData->mEISAttr.iInputBufNum-3);
                iEisProcRet = LIB_S_OK;
            } else {
                iEisProcRet = LIB_E_BUFFER_NOT_ENOUGH;
            }
            iSelfProcTest++;
#else
            EIS_setFrameData(&pVideoEISData->mEisHd, pEisProcPkt);
            pVideoEISData->dEisGetInputFrmCnt++;
            if (pVideoEISData->mEISAttr.bRetInFrmFast &&
                pVideoEISData->dEisGetInputFrmCnt >= pVideoEISData->mEISAttr.iInputBufNum) {
                int i = 0;

                pVideoInBufTmp = VideoBufMgrGetOldestUsingFrame(pVideoEISData->pInputBufMgr);
                if (pVideoInBufTmp) {
                    for (i = 0; i < pVideoEISData->mEISAttr.iInputBufNum; i++) {
                        if (pVideoEISData->pstEISPkt[i].in_addr.in_luma_mmu_Addr == pVideoInBufTmp->VFrame.mpVirAddr[0]) {
                            pVideoEISData->mEisPktMap &= ~(1 << i);
                            break;
                        }
                    }

                    /* Return input buffer to last component, must invoke [DoVideoEisSendBackInputFrame] first */
                    stVideoInBufRetTmp = *pVideoInBufTmp;
                    VideoBufMgrReleaseFrame(pVideoEISData->pInputBufMgr, pVideoInBufTmp);
                    DoVideoEisSendBackInputFrame(pVideoEISData, &stVideoInBufRetTmp);
                } else
                    aloge("Using frame list empty.");
            }

#ifdef PROC_TIME_TEST
            TIME_INIT_START_END(T);
            TIME_GET_START(T);
#endif

            /* Send to kernel drivers and process it. */
            iEisProcRet = EIS_Proc(&pVideoEISData->mEisHd, &iInputFrmId);

#ifdef PROC_TIME_TEST
            TIME_GET_END(T);
            printf("[%d]%f.\n", pVideoEISData->mMppChnInfo.mChnId, TIME_PRINT_DIFF(T));
#endif
#endif
            switch (iEisProcRet) {
                case LIB_S_OK: {
                    /* We got one processed buffer */
                    pEisProcPkt = &pVideoEISData->pstEISPkt[iInputFrmId];
                    COMP_BUFFERHEADERTYPE obh;
                    bool bCanGetOutBuf = 0;

                    bCanGetOutBuf = 0;

                    pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
                    /* If we only have one output idle video frame, then keep it  */
                    if (pVideoEISData->iOutFrmIdleCnt > 1) {
                        list_move_tail(&pVFrmOutputCur->mList, &pVideoEISData->mOutValidList);
                        pVideoEISData->iOutFrmIdleCnt--;
                        bCanGetOutBuf = 1;
                    } else{
                        aloge("We has only one idle frame in EIS output frame list.");
                    }
                    pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);

                    if (!pVideoEISData->mEISAttr.bRetInFrmFast) {
                        pVideoInBufTmp = VideoBufMgrGetSpecUsingFrameWithAddr(
                                            pVideoEISData->pInputBufMgr, pEisProcPkt->in_addr.in_luma_mmu_Addr);
                        if (pVideoInBufTmp) {
                            /* Return input buffer to last component, must invoke [DoVideoEisSendBackInputFrame] first */
                            stVideoInBufRetTmp = *pVideoInBufTmp;
                            VideoBufMgrReleaseFrame(pVideoEISData->pInputBufMgr, pVideoInBufTmp);
                            DoVideoEisSendBackInputFrame(pVideoEISData, &stVideoInBufRetTmp);
                        } else
                            aloge("Not find input buffer in UsingFrameList, addr=0x%x.", pEisProcPkt->in_addr.in_luma_mmu_Addr);
                        /* Now, clear using flag. */
                        pVideoEISData->mEisPktMap &= ~(1 << iInputFrmId);
                    }
                    /* If we find one processed success video frame, then push it to next component
                    * or valid frame list.
                    */
                    if (bCanGetOutBuf) {
                        if (pVideoInBufTmp)
                            pVFrmOutputCur->mFrame.VFrame.mpts = stVideoInBufRetTmp.VFrame.mpts;
                        else
                            pVFrmOutputCur->mFrame.VFrame.mpts = 0;

                        if (FALSE == pVideoEISData->bOutputPortTunnelFlag) {
                            if (pVideoEISData->bWaitingOutFrmFlag)
                                cdx_sem_up(&pVideoEISData->mSemWaitOutFrame);
                        } else if (TRUE == pVideoEISData->bOutputPortTunnelFlag && pOutputPort->hTunnel) {
                            MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)pOutputPort->hTunnel;
                            VIDEO_FRAME_INFO_S* pVOutFrm = &pVFrmOutputCur->mFrame;
                            pVOutFrm->VFrame.mOffsetTop = 0;
                            pVOutFrm->VFrame.mOffsetBottom = pVOutFrm->VFrame.mHeight;
                            pVOutFrm->VFrame.mOffsetLeft = 0;
                            pVOutFrm->VFrame.mOffsetRight = pVOutFrm->VFrame.mWidth;

                            if(pVideoEISData->bStoreFrame) {
                                DoVideoEisStoreProcessedFrm(pVideoEISData, pVOutFrm);
                                pVideoEISData->bStoreFrame = FALSE;
                            }

                            pthread_mutex_lock(&pVideoEISData->mOutFrmListLock);
                            obh.nOutputPortIndex = pOutputPort->nPortIndex;
                            obh.nInputPortIndex = pOutputPort->nTunnelPortIndex;
                            obh.pOutputPortPrivate = pVOutFrm;
                            eError = COMP_EmptyThisBuffer(pOutTunnelComp, &obh);
                            if(SUCCESS != eError) {
                                alogw("Loop EIS_CompStabThread OutTunnelComp EmptyThisBuffer failed 0x%x, return this frame", eError);
                                /* Just return */
                                list_move_tail(&pVFrmOutputCur->mList, &pVideoEISData->mOutIdleList);
                                pVideoEISData->iOutFrmIdleCnt++;
                            } else
                                list_move_tail(&pVFrmOutputCur->mList, &pVideoEISData->mOutUsedList);
                            pthread_mutex_unlock(&pVideoEISData->mOutFrmListLock);
                        }
                    } else{
                        aloge("Not find output buffer.");
                    }
                } break;
                case LIB_E_BUFFER_NOT_ENOUGH: {
                    alogw("Has not get enough buffer.");
                } break;
                default: {
                    /* Something error occurs, and we should return the oldest using frame. */
                    if (!pVideoEISData->mEISAttr.bRetInFrmFast) {
                        int i = 0;

                        pVideoInBufTmp = VideoBufMgrGetOldestUsingFrame(pVideoEISData->pInputBufMgr);
                        if (pVideoInBufTmp) {
                            for (i = 0; i < pVideoEISData->mEISAttr.iInputBufNum; i++) {
                                if (pVideoEISData->pstEISPkt[i].in_addr.in_luma_mmu_Addr == pVideoInBufTmp->VFrame.mpVirAddr[0]) {
                                    pVideoEISData->mEisPktMap &= ~(1 << i);
                                    aloge("Free used EISPkt[%d] because of hardware error.", i);
                                    break;
                                }
                            }

                            /* Return input buffer to last component, must invoke [DoVideoEisSendBackInputFrame] first */
                            stVideoInBufRetTmp = *pVideoInBufTmp;
                            VideoBufMgrReleaseFrame(pVideoEISData->pInputBufMgr, pVideoInBufTmp);
                            DoVideoEisSendBackInputFrame(pVideoEISData, &stVideoInBufRetTmp);
                        } else
                            aloge("Using frame list empty.");
                    }
                    aloge("Got one undefined error. ret 0x%x.", iEisProcRet);
                } break;
            }
        } else {
            alogv("EIS_CompStabThread not OMX_StateExecuting\n");
            TMessage_WaitQueueNotEmpty(&pVideoEISData->cmd_queue, 0);
        }
    }

EXIT:
    alogv("VideoEis ComponentThread stopped");
    return (void *)SUCCESS;
}

