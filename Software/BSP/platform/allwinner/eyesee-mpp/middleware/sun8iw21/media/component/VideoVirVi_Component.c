/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VideoVi_Component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/09
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "VideoVirVi_Component"
#include <utils/plat_log.h>

//ref platform headers
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"
#include "cdx_list.h"

#include "SystemBase.h"
#include <VIDEO_FRAME_INFO_S.h>
#include "mm_comm_vi.h"
#include "mm_comm_video.h"
#include <mm_comm_venc.h>
#include "mm_common.h"
#include "mpi_vi.h"

#include "ComponentCommon.h"
#include "media_common.h"
#include "mm_component.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include "vencoder.h"

// #include "../LIBRARY/libisp/include/device/csi_subdev.h"
#include "VideoVirVi_Component.h"
#include "isp_dev.h"
#include "video.h"
#include "video_buffer_manager.h"
#include <sys/prctl.h>
#include <pthread.h>

#include "../videoIn/videoInputHw.h"
//#define VIDEO_ENC_TIME_DEBUG

typedef struct awVIDEOVIDATATYPE{
    COMP_STATETYPE state;
    pthread_mutex_t mStateLock;
    pthread_mutex_t mLock;
    COMP_CALLBACKTYPE* pCallbacks;
    void* pAppData;
    COMP_HANDLETYPE hSelf;

    COMP_PORT_PARAM_TYPE sPortParam;
    COMP_PARAM_PORTDEFINITIONTYPE sPortDef[VI_CHN_MAX_PORTS];
    COMP_INTERNAL_TUNNELINFOTYPE sPortTunnelInfo[VI_CHN_MAX_PORTS];
    COMP_PARAM_BUFFERSUPPLIERTYPE sPortBufSupplier[VI_CHN_MAX_PORTS];
    bool mInputPortTunnelFlag;
    bool mOutputPortTunnelFlag;  //TRUE: tunnel mode; FALSE: non-tunnel mode.

    MPP_CHN_S mMppChnInfo;
    VI_ATTR_S mViAttr;
    ViVirChnAttrS mViVirChnAttr;
    pthread_t thread_id;
    // CompInternalMsgType eTCmd;
    message_queue_t cmd_queue;

	volatile int mWaitingCapDataFlag;
	cdx_sem_t mAllFrameRelSem;
    volatile int mWaitAllFrameReleaseFlag;

    VideoBufferManager* mpCapMgr;
    pthread_mutex_t mFrameLock;
    cdx_sem_t mSemWaitInputFrame;

    VI_SHUTTIME_CFG_S mLongExp;
    int mSetLongExpCnt;
    int mLongExpFrameDelay;
    int mLongExpFrameCnt;
    pthread_mutex_t mLongExpLock;

    int mStoreFrameCount;
    char mDbgStoreFrameFilePath[VI_MAXPATHSIZE];
    uint64_t mLastFrmPtsCache;
    unsigned int mLastFrmCnt;
    bool mbStoreFrame;
} VIDEOVIDATATYPE;

/* Used for async long exposure mode. */
enum awVI_LONGEXP_EVENT {
    E_VI_LONGEXP_SET, /* Set to long exposure mode. */
    E_VI_LONGEXP_RESET, /* Reset from long exposure mode. */
};

static void *Vi_ComponentThread(void *pThreadData);

static ERRORTYPE DoVideoViSendBackInputFrame(VIDEOVIDATATYPE *pVideoViData, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    COMP_BUFFERHEADERTYPE BufferHeader;
    BufferHeader.pOutputPortPrivate = pFrameInfo;
    if (FALSE == pVideoViData->mInputPortTunnelFlag) {
        pVideoViData->pCallbacks->EmptyBufferDone(pVideoViData->hSelf, pVideoViData->pAppData, &BufferHeader);
    } else {
        alogw("unsupported temporary: vda return input frame in tunnel mode!");
        MM_COMPONENTTYPE *pTunnelComp = (MM_COMPONENTTYPE *)pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_CAP_IN].hTunnel;
        pTunnelComp->FillThisBuffer(pTunnelComp, &BufferHeader);
    }
    alogd("release input FrameId[%d]", pFrameInfo->mId);
    return SUCCESS;
}

ERRORTYPE DoVideoViGetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                     PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int i;
    for(i = 0; i < VI_CHN_MAX_PORTS; i++) {
        if (pPortDef->nPortIndex == pVideoViData->sPortDef[i].nPortIndex) {
            memcpy(pPortDef, &pVideoViData->sPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
        }
    }
    if (i == VI_CHN_MAX_PORTS) {
        eError = FAILURE;
    }

    return eError;
}
ERRORTYPE DoVideoViSetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                     PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int i;
    for(i = 0; i < VI_CHN_MAX_PORTS; i++) {
        if (pPortDef->nPortIndex == pVideoViData->sPortDef[i].nPortIndex) {
            memcpy(&pVideoViData->sPortDef[i], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
        }
    }
    if (i == VI_CHN_MAX_PORTS) {
        eError = FAILURE;
    }
    return eError;
}

ERRORTYPE DoVideoViGetCompBufferSupplier(PARAM_IN COMP_HANDLETYPE hComponent,
                                         PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int i;
    for(i=0; i<VI_CHN_MAX_PORTS; i++) {
        if(pVideoViData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
            memcpy(pPortBufSupplier, &pVideoViData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(i == VI_CHN_MAX_PORTS) {
        eError = FAILURE;
    }

    return eError;
}
ERRORTYPE DoVideoViSetCompBufferSupplier(PARAM_IN COMP_HANDLETYPE hComponent,
                                         PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int i;
    for(i=0; i<VI_CHN_MAX_PORTS; i++) {
        if(pVideoViData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
            memcpy(&pVideoViData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(i == VI_CHN_MAX_PORTS) {
        eError = FAILURE;
    }

    return eError;
}

ERRORTYPE DoVideoViGetMPPChannelInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT MPP_CHN_S *pChn)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(pChn, &pVideoViData->mMppChnInfo);
    return SUCCESS;
}

ERRORTYPE DoVideoViSetMPPChannelInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN MPP_CHN_S *pChn)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pVideoViData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE DoVideoViGetTunnelInfo(PARAM_IN COMP_HANDLETYPE hComponent,
                                 PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = ERR_VI_UNEXIST;
    int i;
    for(i=0; i<VI_CHN_MAX_PORTS; i++) {
        if(pVideoViData->sPortBufSupplier[i].nPortIndex == pTunnelInfo->nPortIndex) {
            memcpy(pTunnelInfo, &pVideoViData->sPortTunnelInfo[i], sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
            break;
        }
    }
    if(i == VI_CHN_MAX_PORTS) {
        eError = FAILURE;
    }

    return eError;
}

ERRORTYPE DoVideoViGetData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT VI_Params *pstParams, PARAM_IN int nMilliSec)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoViData->state && COMP_StateExecuting != pVideoViData->state) {
        alogw("call getStream in wrong state[0x%x]", pVideoViData->state);
        return ERR_VI_NOT_PERM;
    }
    int ret;

    /* not support get data in Tunnel mode for app */
    if (TRUE == pVideoViData->mOutputPortTunnelFlag) {
        return ERR_VI_NOT_PERM;
    }

    /* for online, does not support get/release vipp data by user. */
    if (pVideoViData->mViAttr.mOnlineEnable) {
        return ERR_VI_NOT_PERM;
    }

    VIDEO_FRAME_INFO_S *pstFrameInfo = pstParams->pstFrameInfo;
    VIDEO_FRAME_INFO_S *tmp = NULL;
_TryToGetFrame:
    tmp = VideoBufMgrGetValidFrame(pVideoViData->mpCapMgr);
    if (NULL != tmp)
        goto GetFrmSuccess;

    if (0 == nMilliSec)
        return FAILURE;
    else if (nMilliSec < 0) {
        pVideoViData->mWaitingCapDataFlag = TRUE;
        tmp = VideoBufMgrGetValidFrame(pVideoViData->mpCapMgr);
        if (tmp != NULL) {
            pVideoViData->mWaitingCapDataFlag = FALSE;
            goto GetFrmSuccess;
        } else {
            cdx_sem_down(&pVideoViData->mSemWaitInputFrame);
            pVideoViData->mWaitingCapDataFlag = FALSE;
            goto _TryToGetFrame;
        }
    } else {
        pVideoViData->mWaitingCapDataFlag = TRUE;
        tmp = VideoBufMgrGetValidFrame(pVideoViData->mpCapMgr);
        if (tmp != NULL) {
            pVideoViData->mWaitingCapDataFlag = FALSE;
            goto GetFrmSuccess;
        } else {
            ret = cdx_sem_down_timedwait(&pVideoViData->mSemWaitInputFrame, nMilliSec);
            if(ETIMEDOUT == ret) {
                alogv("wait input frame timeout[%d]ms, ret[%d]", nMilliSec, ret);
                pVideoViData->mWaitingCapDataFlag = FALSE;
                return FAILURE;
            } else if(0 == ret) {
                pVideoViData->mWaitingCapDataFlag = FALSE;
                goto _TryToGetFrame;
            } else {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                pVideoViData->mWaitingCapDataFlag = FALSE;
                return FAILURE;
            }
        }
    }

GetFrmSuccess:
    memcpy(&pstFrameInfo->VFrame, &tmp->VFrame, sizeof(VIDEO_FRAME_INFO_S));
    pstFrameInfo->mId = tmp->mId;
    //pstFrameInfo->VFrame.mOffsetTop = 0;
    //pstFrameInfo->VFrame.mOffsetBottom = pstFrameInfo->VFrame.mHeight;
    //pstFrameInfo->VFrame.mOffsetLeft = 0;
    //pstFrameInfo->VFrame.mOffsetRight = pstFrameInfo->VFrame.mWidth;
    alogv("DoVideoViGetData addr = %p.\r\n", pstFrameInfo->VFrame.mpVirAddr[0]);
    if(pVideoViData->mbStoreFrame) {
        char strPixFmt[16];
        if(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pstFrameInfo->VFrame.mPixelFormat) {
            strcpy(strPixFmt, "nv21");
        }else if(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == pstFrameInfo->VFrame.mPixelFormat) {
            strcpy(strPixFmt, "nv12");
        }else if(MM_PIXEL_FORMAT_YUV_AW_AFBC == pstFrameInfo->VFrame.mPixelFormat) {
            strcpy(strPixFmt, "afbc");
        }else if(MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X == pstFrameInfo->VFrame.mPixelFormat) {
            strcpy(strPixFmt, "lbc20x");
        }else if(MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X == pstFrameInfo->VFrame.mPixelFormat) {
            strcpy(strPixFmt, "lbc25x");
        }else if(MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X == pstFrameInfo->VFrame.mPixelFormat) {
            strcpy(strPixFmt, "lbc15x");
        }else if(MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X == pstFrameInfo->VFrame.mPixelFormat) {
            strcpy(strPixFmt, "lbc10x");
        } else {
            strcpy(strPixFmt, "unknown");
        }
        int nPos = strlen(pVideoViData->mDbgStoreFrameFilePath);
        snprintf(pVideoViData->mDbgStoreFrameFilePath+nPos, VI_MAXPATHSIZE-nPos,
                "/vipp[%d]virChn[%d]pic[%d].%s", pVideoViData->mMppChnInfo.mDevId,
                pVideoViData->mMppChnInfo.mChnId, pVideoViData->mStoreFrameCount++, strPixFmt);
        FILE *dbgFp = fopen(pVideoViData->mDbgStoreFrameFilePath, "wb");
        VideoFrameBufferSizeInfo FrameSizeInfo;
        getVideoFrameBufferSizeInfo(tmp, &FrameSizeInfo);
        int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
        for(int i=0; i<3; i++) {
            if(tmp->VFrame.mpVirAddr[i] != NULL) {
                fwrite(tmp->VFrame.mpVirAddr[i], 1, yuvSize[i], dbgFp);
                alogd("virAddr[%d]=[%p], length=[%d]", i, tmp->VFrame.mpVirAddr[i], yuvSize[i]);
            }
        }
        fclose(dbgFp);
        pVideoViData->mbStoreFrame = FALSE;
        alogd("store VI frame in file[%s], non-tunnel mode", pVideoViData->mDbgStoreFrameFilePath);
    }

    return SUCCESS;
}

ERRORTYPE DoVideoViReleaseData(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN VI_Params *pstParams)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoViData->state && COMP_StateExecuting != pVideoViData->state) {
        alogw("call releaseStream in wrong state[0x%x], but still release it", pVideoViData->state);
        //return ERR_VI_NOT_PERM;
    }
    if (TRUE == pVideoViData->mOutputPortTunnelFlag) { /* Tunnel mode */
        return ERR_VI_NOT_PERM;
    }

    /* for online, does not support get/release vipp data by user. */
    if (pVideoViData->mViAttr.mOnlineEnable) {
        return ERR_VI_NOT_PERM;
    }

    int vipp_id = pstParams->mDev;
    VIDEO_FRAME_INFO_S *pstFrameInfo = pstParams->pstFrameInfo;
    videoInputHw_RefsReduceAndRleaseData(vipp_id, pstFrameInfo);
    VideoBufMgrReleaseFrame(pVideoViData->mpCapMgr, pstFrameInfo);
    alogv("DoVideoViReleaseData addr = %p.\r\n", pstFrameInfo->VFrame.mpVirAddr[0]);

    return SUCCESS;
}

static ERRORTYPE DoVideoViReturnAllValidFrames(PARAM_IN VIDEOVIDATATYPE *pVideoViData)
{
    int nFrameNum = 0;
    VIDEO_FRAME_INFO_S *pFrame;
    while(1)
    {
        pFrame = VideoBufMgrGetValidFrame(pVideoViData->mpCapMgr);
        if(pFrame) {
            videoInputHw_RefsReduceAndRleaseData(pVideoViData->mMppChnInfo.mDevId, pFrame);
            VideoBufMgrReleaseFrame(pVideoViData->mpCapMgr, pFrame);
            nFrameNum++;
        } else {
            break;
        }
    }
    if(nFrameNum > 0)
    {
        alogv("release [%d]validFrames", nFrameNum);
    }
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pVideoViData->mpCapMgr->mUsingFrmList)
    {
        cnt++;
    }            
    if(cnt > 0)
    {
        alogw("Be careful! remain [%d] usingFrames after return all valid frames", cnt);
    }
    return SUCCESS;
}
/**
 * pDirPath: /mnt/extsd/VIDebug,
 */
ERRORTYPE VideoViStoreFrame(PARAM_IN COMP_HANDLETYPE hComponent, const char* pDirPath)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (!pVideoViData->mOutputPortTunnelFlag) {
        alogw("in non-tunnel mode, you should store frame by your self, we don't do it\n");
        return FAILURE;
    }

    message_t msg;
    InitMessage(&msg);
    msg.command = VViComp_StoreFrame;
    msg.mpData = (void*)pDirPath;
    msg.mDataSize = strlen(pDirPath)+1;
    putMessageWithData(&pVideoViData->cmd_queue, &msg);

    return SUCCESS;
}

/**
 * pstViShutter: see mm_common_vi.h -> awVI_SHUTTIME_CFG_S
 */
ERRORTYPE VideoViSetLongExpMode(PARAM_IN COMP_HANDLETYPE hComponent, VI_SHUTTIME_CFG_S *pstViShutter)
{
    ERRORTYPE eErr = SUCCESS;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pVideoViData->mLongExpLock);
    switch (pstViShutter->eShutterMode) {
        case VI_SHUTTIME_MODE_AUTO: break;
        case VI_SHUTTIME_MODE_PREVIEW: {
            aloge("not support this value until now, eShutterMode[%d]", pstViShutter->eShutterMode);
            eErr = ERR_VI_INVALID_PARA;
            goto exit;
        } break;
        case VI_SHUTTIME_MODE_NIGHT_VIEW: {
            if (pstViShutter->iTime > 0) {
                aloge("not support this value until now, iTime[%d]", pstViShutter->iTime);
                eErr = ERR_VI_INVALID_PARA;
                goto exit;
            }
        } break;
        default: {
            aloge("not support this value until now, eShutterMode[%d]", pstViShutter->eShutterMode);
            eErr = ERR_VI_INVALID_PARA;
            goto exit;
        } break;
    }

    if (pVideoViData->mSetLongExpCnt >= 1) {
        aloge("the long exposure mode has been set, do not set it again before returns normal.");
        eErr = ERR_VI_INVALID_PARA;
        goto exit;
    } else if (pstViShutter->eShutterMode == VI_SHUTTIME_MODE_AUTO) {
        alogd("The shutter mode now is auto already, return.");
        eErr = SUCCESS;
        goto exit;
    }

    memcpy(&pVideoViData->mLongExp, pstViShutter, sizeof(VI_SHUTTIME_CFG_S));;
    pVideoViData->mSetLongExpCnt++;

    message_t msg;
    memset(&msg, 0, sizeof(msg));
    if (pVideoViData->mLongExp.eShutterMode == VI_SHUTTIME_MODE_NIGHT_VIEW) {
        msg.command = VViComp_LongExpEvent;
        msg.para0 = E_VI_LONGEXP_SET;
        msg.para1 = COMP_IndexVendorViSetLongExp;
        put_message(&pVideoViData->cmd_queue, &msg);
    }

    if (pVideoViData->mLongExp.eResetMode == VI_SHUTTIME_RESET_AUTO_NOW) {
        msg.command = VViComp_LongExpEvent;
        msg.para0 = E_VI_LONGEXP_RESET;
        msg.para1 = FF_LONGEXP;
        put_message(&pVideoViData->cmd_queue, &msg);
    }

exit:
    pthread_mutex_unlock(&pVideoViData->mLongExpLock);
    /* we should return all buffers in valid frame list. */
    DoVideoViReturnAllValidFrames(pVideoViData);

    return eErr;
}

/**
 * pstViAttr: see mm_common_vi.h -> awVI_ATTR_S
 */
ERRORTYPE VideoViGetViDevAttr(PARAM_IN COMP_HANDLETYPE hComponent, VI_ATTR_S *pstViAttr)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    memcpy(pstViAttr, &pVideoViData->mViAttr, sizeof(VI_ATTR_S));

    return SUCCESS;
}

/**
 * pstViAttr: see mm_common_vi.h -> awVI_ATTR_S
 */
ERRORTYPE VideoViSetViDevAttr(PARAM_IN COMP_HANDLETYPE hComponent, VI_ATTR_S *pstViAttr)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    memcpy(&pVideoViData->mViAttr, pstViAttr, sizeof(VI_ATTR_S));
    //aloge("fps %d nbufs %d", pVideoViData->mViAttr.fps, pVideoViData->mViAttr.nbufs);

    return SUCCESS;
}

ERRORTYPE VideoViGetViChnAttr(PARAM_IN COMP_HANDLETYPE hComponent, ViVirChnAttrS *pViVirChnAttr)
{
    ERRORTYPE ret = SUCCESS;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    pthread_mutex_lock(&pVideoViData->mLock);
    if(pViVirChnAttr)
    {
        memcpy(pViVirChnAttr, &pVideoViData->mViVirChnAttr, sizeof(ViVirChnAttrS));
    }
    else
    {
        ret = ERR_VI_INVALID_NULL_PTR;
    }
    pthread_mutex_unlock(&pVideoViData->mLock);
    return ret;
}

ERRORTYPE VideoViSetViChnAttr(PARAM_IN COMP_HANDLETYPE hComponent, ViVirChnAttrS *pViVirChnAttr)
{
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pVideoViData->mLock);
    if(pViVirChnAttr != NULL)
    {
        memcpy(&pVideoViData->mViVirChnAttr, pViVirChnAttr, sizeof(ViVirChnAttrS));
    }
    pthread_mutex_unlock(&pVideoViData->mLock);
    return SUCCESS;
}

ERRORTYPE VideoViSendCommand(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_COMMANDTYPE Cmd,
                             PARAM_IN unsigned int nParam1, PARAM_IN void *pCmdData)
{
    VIDEOVIDATATYPE *pVideoViData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    alogv("VideoViSendCommand: %d", Cmd);

    pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (!pVideoViData) {
        eError = ERR_VI_INVALID_PARA;
        goto COMP_CONF_CMD_FAIL;
    }

    if (pVideoViData->state == COMP_StateInvalid) {
        eError = ERR_VI_SYS_NOTREADY;
        goto COMP_CONF_CMD_FAIL;
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
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pVideoViData->cmd_queue, &msg);

COMP_CONF_CMD_FAIL:
    return eError;
}

ERRORTYPE VideoViGetState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT COMP_STATETYPE *pState)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    *pState = pVideoViData->state;

    return eError;
}

ERRORTYPE VideoViSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_CALLBACKTYPE *pCallbacks,
                              PARAM_IN void *pAppData)
{
    VIDEOVIDATATYPE *pVideoViData;
    ERRORTYPE eError = SUCCESS;

    pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (!pVideoViData || !pCallbacks || !pAppData) {
        eError = ERR_VI_INVALID_PARA;
        goto COMP_CONF_CMD_FAIL;
    }

    pVideoViData->pCallbacks = pCallbacks;
    pVideoViData->pAppData = pAppData;

COMP_CONF_CMD_FAIL:
    return eError;
}

ERRORTYPE VideoViGetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                           PARAM_IN void *pComponentConfigStructure)
{
    // VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    switch (nIndex) {
        case COMP_IndexParamPortDefinition: {
            eError = DoVideoViGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
        } break;
        case COMP_IndexParamCompBufferSupplier: {
            eError =
              DoVideoViGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorMPPChannelInfo: {
            eError = DoVideoViGetMPPChannelInfo(hComponent, (MPP_CHN_S *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViGetFrame: {
            VI_Params *pData = (VI_Params *)pComponentConfigStructure;
            eError = DoVideoViGetData(hComponent, pData, pData->s32MilliSec);
        } break;
        case COMP_IndexVendorTunnelInfo: {
            eError = DoVideoViGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViDevAttr: {
            eError = VideoViGetViDevAttr(hComponent, (VI_ATTR_S *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViChnAttr: 
        {
            eError = VideoViGetViChnAttr(hComponent, (ViVirChnAttrS*)pComponentConfigStructure);
            break;
        }
        default: {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_VI_NOT_SUPPORT;
        } break;
    }
    return eError;
}

ERRORTYPE VideoViSetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                           PARAM_IN void *pComponentConfigStructure)
{
    // VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    switch (nIndex) {
        case COMP_IndexParamPortDefinition: {
            eError = DoVideoViSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
        } break;
        case COMP_IndexParamCompBufferSupplier: {
            eError =
              DoVideoViSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorMPPChannelInfo: {
            eError = DoVideoViSetMPPChannelInfo(hComponent, (MPP_CHN_S *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViReleaseFrame: {
            eError = DoVideoViReleaseData(hComponent, (VI_Params *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViStoreFrame: {
            eError = VideoViStoreFrame(hComponent, (const char*)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViSetLongExp: {
            eError = VideoViSetLongExpMode(hComponent, (VI_SHUTTIME_CFG_S *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViDevAttr: {
            eError = VideoViSetViDevAttr(hComponent, (VI_ATTR_S *)pComponentConfigStructure);
        } break;
        case COMP_IndexVendorViChnAttr:
        {
            eError = VideoViSetViChnAttr(hComponent, (ViVirChnAttrS*)pComponentConfigStructure);
            break;
        }
        default: {
            aloge("(f:%s, l:%d) unknown Index[0x%x]", __FUNCTION__, __LINE__, nIndex);
            eError = ERR_VI_INVALID_PARA;
        } break;
    }

    return eError;
}

ERRORTYPE VideoViComponentTunnelRequest(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN unsigned int nPort,
                                        PARAM_IN COMP_HANDLETYPE hTunneledComp, PARAM_IN unsigned int nTunneledPort,
                                        PARAM_INOUT COMP_TUNNELSETUPTYPE *pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    COMP_PARAM_PORTDEFINITIONTYPE   *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE    *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE   *pPortBufSupplier;
    int i;

    if (pVideoViData->state == COMP_StateExecuting) {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    } else if (pVideoViData->state != COMP_StateIdle) {
        aloge("fatal error! tunnel request can't be in state[0x%x]",pVideoViData->state);
        eError = ERR_VI_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < VI_CHN_MAX_PORTS; ++i) {
        if (pVideoViData->sPortDef[i].nPortIndex == nPort) {
            pPortDef = &pVideoViData->sPortDef[i];
            break;
        }
    }
    if (i == VI_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < VI_CHN_MAX_PORTS; ++i) {
        if (pVideoViData->sPortTunnelInfo[i].nPortIndex == nPort) {
            pPortTunnelInfo = &pVideoViData->sPortTunnelInfo[i];
            break;
        }
    }
    if (i == VI_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < VI_CHN_MAX_PORTS; ++i) {
        if (pVideoViData->sPortBufSupplier[i].nPortIndex == nPort) {
            pPortBufSupplier = &pVideoViData->sPortBufSupplier[i];
            break;
        }
    }
    if (i == VI_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }


    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        if(pPortDef->eDir == COMP_DirOutput)
        {
            pVideoViData->mOutputPortTunnelFlag = FALSE;
        }
        else
        {
            pVideoViData->mInputPortTunnelFlag = FALSE;
        }
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        if (pVideoViData->mOutputPortTunnelFlag) {
            aloge("VirVi_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pVideoViData->mOutputPortTunnelFlag = TRUE;
    }
    else
    {
        if (pVideoViData->mInputPortTunnelFlag) {
            aloge("VirVi_Comp inport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput)
        {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_VI_INVALID_PARA;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;

        //The component B informs component A about the final result of negotiation.
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
        {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pVideoViData->mInputPortTunnelFlag = TRUE;
    }

COMP_CMD_FAIL:
    return eError;
}

ERRORTYPE VideoViEmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    ERRORTYPE eError    = SUCCESS;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    VIDEO_FRAME_INFO_S *pFrm = (VIDEO_FRAME_INFO_S *)pBuffer->pOutputPortPrivate;
    uint64_t FrmIntervalMs = (pFrm->VFrame.mpts-pVideoViData->mLastFrmPtsCache)/1000;
    unsigned int FrmCntInterval = pFrm->VFrame.mFramecnt-pVideoViData->mLastFrmCnt;
    int iGetLongExpFlag = 0;

    /* long exposure mode should be 1S and lager time interval,
    * if the time interval is less than 1S, it may be regarded as normal frame.
    */
    if (0 != pVideoViData->mLastFrmPtsCache && (FrmIntervalMs>1000/pVideoViData->mViAttr.fps*FrmCntInterval+500)) {
#if 0
        struct timespec flag_ts;
        clock_gettime(CLOCK_MONOTONIC, &flag_ts);
        pFrm->VFrame.mFlagPts = ((uint64_t)flag_ts.tv_sec)*1000000+flag_ts.tv_nsec/1000;
        pFrm->VFrame.mFrmFlag = FrmIntervalMs<<16 | FF_LONGEXP;
        pFrm->VFrame.mWhoSetFlag = (pVideoViData->mMppChnInfo.mModId<<16) | (pVideoViData->mMppChnInfo.mDevId<<8)
                                    | (pVideoViData->mMppChnInfo.mChnId);
        alogw("Find one long exposure frame, exp-time(%x %x), cnt_lost(%d)\r\n",
            pFrm->VFrame.mFrmFlag, pFrm->VFrame.mWhoSetFlag, FrmCntInterval);
#endif
        iGetLongExpFlag = 1;
    }
    pVideoViData->mLastFrmPtsCache = pFrm->VFrame.mpts;
    pVideoViData->mLastFrmCnt      = pFrm->VFrame.mFramecnt;

    pthread_mutex_lock(&pVideoViData->mStateLock);
    bool bDiscardFlag = false;
    if(COMP_StateExecuting == pVideoViData->state || COMP_StatePause == pVideoViData->state)
    {
        bDiscardFlag = false;
    }
    else if(COMP_StateIdle == pVideoViData->state)
    {
        BOOL bRecv = pVideoViData->mViVirChnAttr.mbRecvInIdleState;
        if(bRecv)
        {
            bDiscardFlag = false;
        }
        else
        {
            bDiscardFlag = true;
        }
    }
    else
    {
        bDiscardFlag = true;
    }
    if(bDiscardFlag)
    {
        //alogw("vipp[%d,%d] send frameId[%d,%d] in vi state[0x%x], discard!", pVideoViData->mMppChnInfo.mDevId, 
        //    pVideoViData->mMppChnInfo.mChnId, pFrm->mId, pFrm->VFrame.mFramecnt, pVideoViData->state);
        eError = FAILURE;
        goto err_state;
    }

    pthread_mutex_lock(&pVideoViData->mLongExpLock);
    if (pVideoViData->mLongExp.eShutterMode == VI_SHUTTIME_MODE_AUTO) {
        /* normal mode, but still should check if is long exposure frame */
        if (pVideoViData->mSetLongExpCnt == 0) {
            /* do nothing */;
        } else if (pVideoViData->mLongExpFrameCnt == pFrm->VFrame.mFramecnt) {
            pVideoViData->mSetLongExpCnt = 0;
        } else {
            pthread_mutex_unlock(&pVideoViData->mLongExpLock);
            eError = FAILURE;
            aloge("FrmIntervalMs %llu, not a long exposure video frame", FrmIntervalMs);
            goto err_state;
        }
    } else if (pVideoViData->mLongExp.eShutterMode == VI_SHUTTIME_MODE_NIGHT_VIEW) {
        if (iGetLongExpFlag) {
            pVideoViData->mLongExpFrameCnt = pFrm->VFrame.mFramecnt + pVideoViData->mLongExpFrameDelay;
            /* if it is not reset manual, I resume to AUTO mode by myself,.
            * else, if pVideoViData->mSetLongExpCnt == 1; it says somebody set 
            * VI_SHUTTIME_MODE_AUTO, so we should set to VI_SHUTTIME_MODE_AUTO
            */
            pVideoViData->mLongExp.eShutterMode = VI_SHUTTIME_MODE_AUTO;
            if (pVideoViData->mLongExp.eResetMode == VI_SHUTTIME_RESET_AUTO_DELAY) {
                message_t msg;
                msg.command = VViComp_LongExpEvent;
                msg.para0 = E_VI_LONGEXP_RESET;
                msg.para1 = FF_LONGEXP;
                put_message(&pVideoViData->cmd_queue, &msg);
            }
            aloge("FrmIntervalMs %llu, mode %d, cnt %d", FrmIntervalMs, pVideoViData->mLongExp.eShutterMode, pVideoViData->mSetLongExpCnt);
        }

        if (pFrm->VFrame.mFramecnt >= pVideoViData->mLongExpFrameCnt) {
            pVideoViData->mSetLongExpCnt = 0;
        } else {
            pthread_mutex_unlock(&pVideoViData->mLongExpLock);
            eError = FAILURE;
            aloge("FrmIntervalMs %llu, not a long exposure video frame", FrmIntervalMs);
            aloge("Real long exposure frame count %d, this frame count %d", pVideoViData->mLongExpFrameCnt, pFrm->VFrame.mFramecnt);
            goto err_state;
        }
    }
    pthread_mutex_unlock(&pVideoViData->mLongExpLock);


    /* put data ptr to CapMgr_queue for tunnel or user mpi_get\release */
    if (pBuffer->nInputPortIndex == pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].nPortIndex)
    {
        do
        {
            if(pVideoViData->mViVirChnAttr.mCacheFrameNum > 0)
            {
                if (0 == pVideoViData->mViVirChnAttr.mCachePolicy)
                {
                    int nFrameNum = pVideoViData->mpCapMgr->GetValidUsingFrameNum(pVideoViData->mpCapMgr);
                    if(nFrameNum >= pVideoViData->mViVirChnAttr.mCacheFrameNum)
                    {
                        //cache frame number is enough, discard current frame.
                        eError = FAILURE;
                        break;
                    }
                }
                else if (1 == pVideoViData->mViVirChnAttr.mCachePolicy)
                {
                    int nFrameNum = pVideoViData->mpCapMgr->GetValidUsingFrameNum(pVideoViData->mpCapMgr);
                    if(nFrameNum >= pVideoViData->mViVirChnAttr.mCacheFrameNum)
                    {
                        //cache frame number is enough, discard old frame.
                        message_t msg;
                        msg.command = VViComp_DropFrame;
                        put_message(&pVideoViData->cmd_queue, &msg);
                        alogv("nFrameNum=%d, VViComp DropFrame", nFrameNum);
                    }
                }
                else
                {
                    aloge("fatal error! Cache Policy %d is NOT supported!", pVideoViData->mViVirChnAttr.mCachePolicy);
                }
            }
            pthread_mutex_lock(&pVideoViData->mFrameLock);
            eError = VideoBufMgrPushFrame(pVideoViData->mpCapMgr, pFrm);
            if (eError != SUCCESS)
            {
                pthread_mutex_unlock(&pVideoViData->mFrameLock);
                aloge("failed to push this frame, it will be droped\n");
                eError = FAILURE;
                goto push_fail;
            }

            alogv("VideoViEmptyThisBuffer pushFrame process, %p.\r\n", pFrm->VFrame.mpVirAddr[0]);
            if (pVideoViData->mWaitingCapDataFlag)
            {
                if (pVideoViData->mOutputPortTunnelFlag)
                {
                    message_t msg;
                    msg.command = VViComp_InputFrameAvailable;
                    put_message(&pVideoViData->cmd_queue, &msg);
                }
                else
                {
                    cdx_sem_up(&pVideoViData->mSemWaitInputFrame);
                }
                pVideoViData->mWaitingCapDataFlag = FALSE;
            }
            pthread_mutex_unlock(&pVideoViData->mFrameLock);
        }while(0);
    }
    else if (pBuffer->nOutputPortIndex == pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].nPortIndex)
    {
    }
    else
    {
        aloge("fatal error! inputPortIndex[%d] match nothing!", pBuffer->nOutputPortIndex);
    }

push_fail:
err_state:
    pthread_mutex_unlock(&pVideoViData->mStateLock);

    return eError;
}

ERRORTYPE VideoViFillThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    VIDEOVIDATATYPE *pVideoViData;
    ERRORTYPE eError = SUCCESS;
    int ret;

    pVideoViData = (VIDEOVIDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoViData->state && COMP_StateExecuting != pVideoViData->state) 
    {
        alogw("call fillThisBuffer in wrong state[0x%x], but still release it", pVideoViData->state);
    }
    if (pBuffer->nOutputPortIndex == pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].nPortIndex) {
        VIDEO_FRAME_INFO_S *pFrm = pBuffer->pOutputPortPrivate;
        videoInputHw_RefsReduceAndRleaseData(pVideoViData->mMppChnInfo.mDevId, pFrm);
        VideoBufMgrReleaseFrame(pVideoViData->mpCapMgr, pFrm);
    }
    else {
        aloge("fatal error! outputPortIndex[%d]!=[%d]", pBuffer->nOutputPortIndex, pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].nPortIndex);
    }

    return 0;
}

ERRORTYPE VideoViComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEOVIDATATYPE *pVideoViData = NULL;
    message_t msg;
    struct list_head *pList;
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    int cnt = 0;

    pVideoViData = (VIDEOVIDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
	if(NULL == pVideoViData)
	{
		aloge("pVideoViData is NULL!!!\n");
		return FAILURE;
	}
    msg.command = eCmd;
    put_message(&pVideoViData->cmd_queue, &msg);
	if(NULL != pVideoViData)
	{
		pthread_join(pVideoViData->thread_id, (void *)&eError);
	}
    message_destroy(&pVideoViData->cmd_queue);
    pthread_mutex_destroy(&pVideoViData->mLongExpLock);
    pthread_mutex_destroy(&pVideoViData->mStateLock);
    pthread_mutex_destroy(&pVideoViData->mFrameLock);
    pthread_mutex_destroy(&pVideoViData->mLock);
	if(NULL != pVideoViData)
	{
		VideoBufMgrDestroy(pVideoViData->mpCapMgr);
	}
    cdx_sem_deinit(&pVideoViData->mSemWaitInputFrame);
	if(NULL != pVideoViData)
	{
		pVideoViData->mpCapMgr = NULL;

	}
    if (pVideoViData) {
        free(pVideoViData);
        pVideoViData = NULL;
    }
    alogd("VideoVirvi component exited!");
    return eError;
}

ERRORTYPE VideoViComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    ERRORTYPE eError = SUCCESS;
    int i = 0;

    MM_COMPONENTTYPE *pComp = (MM_COMPONENTTYPE *)hComponent;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)malloc(sizeof(VIDEOVIDATATYPE));
    if (NULL == pVideoViData)
    {
        aloge("fatal error, malloc pVideoViData fail!");
        return ERR_VI_NOMEM;
    }
    memset(pVideoViData, 0x0, sizeof(VIDEOVIDATATYPE));
    pComp->pComponentPrivate = (void *)pVideoViData;
    pVideoViData->state = COMP_StateLoaded;
    pthread_mutex_init(&pVideoViData->mStateLock, NULL);
    pthread_mutex_init(&pVideoViData->mFrameLock, NULL);
    pthread_mutex_init(&pVideoViData->mLock, NULL);
    pthread_mutex_init(&pVideoViData->mLongExpLock, NULL);
    pVideoViData->mLongExpFrameDelay = 1;
    pVideoViData->hSelf = hComponent;

    cdx_sem_init(&pVideoViData->mSemWaitInputFrame, 0);
    pVideoViData->mpCapMgr = VideoBufMgrCreate(VI_FIFO_LEVEL, 0);
    if (pVideoViData->mpCapMgr == NULL) {
        aloge("videoInputBufMgrCreate error!");
        return FAILURE;
    }
    // Fill in function pointers
    pComp->SetCallbacks = VideoViSetCallbacks;
    pComp->SendCommand = VideoViSendCommand;
    pComp->GetConfig = VideoViGetConfig;
    pComp->SetConfig = VideoViSetConfig;
    pComp->GetState = VideoViGetState;
    pComp->ComponentTunnelRequest = VideoViComponentTunnelRequest;
    pComp->ComponentDeInit = VideoViComponentDeInit;
    pComp->EmptyThisBuffer = VideoViEmptyThisBuffer;
    pComp->FillThisBuffer = VideoViFillThisBuffer;

    // Initialize component data structures to default values
    pVideoViData->sPortParam.nPorts = 0;
    pVideoViData->sPortParam.nStartPortNumber = 0x0;

    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].bEnabled = TRUE;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].eDomain = COMP_PortDomainVideo;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].eDir = COMP_DirInput;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].format.video.cMIMEType = "YVU420";
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].format.video.nFrameWidth = 176;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].format.video.nFrameHeight = 144;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].format.video.eCompressionFormat = PT_BUTT;  // YCbCr420;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_CAP_IN].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pVideoViData->sPortBufSupplier[VI_CHN_PORT_INDEX_CAP_IN].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortBufSupplier[VI_CHN_PORT_INDEX_CAP_IN].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_CAP_IN].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_CAP_IN].eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoViData->sPortParam.nPorts++;

    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].bEnabled = TRUE;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].eDomain = COMP_PortDomainVideo;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].eDir = COMP_DirInput;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].format.video.cMIMEType = "YVU420";
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].format.video.nFrameWidth = 176;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].format.video.nFrameHeight = 144;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].format.video.eCompressionFormat = PT_BUTT;  // YCbCr420;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_FILE_IN].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pVideoViData->sPortBufSupplier[VI_CHN_PORT_INDEX_FILE_IN].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortBufSupplier[VI_CHN_PORT_INDEX_FILE_IN].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_FILE_IN].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_FILE_IN].eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoViData->sPortParam.nPorts++;

    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].bEnabled = TRUE;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].eDomain = COMP_PortDomainVideo;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].eDir = COMP_DirOutput;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].format.video.cMIMEType = "YVU420";
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].format.video.nFrameWidth = 176;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].format.video.nFrameHeight = 144;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].format.video.eCompressionFormat = PT_BUTT;  // YCbCr420;
    pVideoViData->sPortDef[VI_CHN_PORT_INDEX_OUT].format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pVideoViData->sPortBufSupplier[VI_CHN_PORT_INDEX_OUT].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortBufSupplier[VI_CHN_PORT_INDEX_OUT].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_OUT].nPortIndex = pVideoViData->sPortParam.nPorts;
    pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_OUT].eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoViData->sPortParam.nPorts++;

    if (message_create(&pVideoViData->cmd_queue) < 0) {
        aloge("message error!");
        eError = ERR_VI_NOMEM;
        goto EXIT;
    }

    // Create the component thread
    if (pthread_create(&pVideoViData->thread_id, NULL, Vi_ComponentThread, pVideoViData) /*|| !pVideoViData->thread_id*/) {
        aloge("(f:%s, l:%d) create Vi_Component Thread fail!\r\n", __FUNCTION__, __LINE__);
        eError = ERR_VI_NOMEM;
        goto EXIT;
    }
    alogd("VideoVirvi component Init! thread_id[0x%x]", pVideoViData->thread_id);
EXIT:
    return eError;
}

static void *Vi_ComponentThread(void *pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    VIDEOVIDATATYPE *pVideoViData = (VIDEOVIDATATYPE *)pThreadData;
    message_t cmd_msg;

    alogv("VideoVi ComponentThread start run...");
    prctl(PR_SET_NAME, "ViComponentThread", 0, 0, 0);

    while (1) {
    PROCESS_MESSAGE:
        if (get_message(&pVideoViData->cmd_queue, &cmd_msg) == 0) {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;
            // alogv("VideoVi ComponentThread get_message cmd:%d", cmd);
            if (cmd == SetState) {
                pthread_mutex_lock(&pVideoViData->mStateLock);
                if (pVideoViData->state == (COMP_STATETYPE)(cmddata)) {
                    pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData, COMP_EventError,
                                                           ERR_VI_SAMESTATE, 0, NULL);
                } else {
                    switch ((COMP_STATETYPE)(cmddata)) {
                        case COMP_StateInvalid: {
                            pVideoViData->state = COMP_StateInvalid;
                            // CompSendEvent(pVideoViData->hSelf, pVideoViData->pAppData, COMP_EventError, ERR_VI_INVALIDSTATE, 0);
                            pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                   COMP_EventError, ERR_VI_INVALIDSTATE, 0, NULL);
                            pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                   COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                   pVideoViData->state, NULL);
                            break;
                        }
                        case COMP_StateLoaded: {
                            if (pVideoViData->state != COMP_StateIdle) {
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            pVideoViData->mWaitAllFrameReleaseFlag = 1;

                            alogd("vipp[%d]virChn[%d]:begin to wait using frame return", pVideoViData->mMppChnInfo.mDevId, pVideoViData->mMppChnInfo.mChnId);
                            VideoBufMgrWaitUsingEmpty(pVideoViData->mpCapMgr);
                            alogv("viChn[%d-%d] wait using frame return done", pVideoViData->mMppChnInfo.mDevId, pVideoViData->mMppChnInfo.mChnId);

                            pVideoViData->mWaitAllFrameReleaseFlag = 0;

                            //return all valid frames to videoInput
                            DoVideoViReturnAllValidFrames(pVideoViData);
                            pVideoViData->state = COMP_StateLoaded;
                            alogv("viChn[%d-%d] Set Virvi OMX_StateLoaded OK", pVideoViData->mMppChnInfo.mDevId, pVideoViData->mMppChnInfo.mChnId);
                            pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                   COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                   pVideoViData->state, NULL);
                            break;
                        }
                        case COMP_StateIdle: {
                            if (pVideoViData->state == COMP_StateLoaded) {
                                alogv("video VI: loaded->idle ...");
                                pVideoViData->state = COMP_StateIdle;
                                // callback sem++
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                       pVideoViData->state, NULL);
                            } else if (pVideoViData->state == COMP_StatePause ||
                            pVideoViData->state == COMP_StateExecuting) {
                                alogv("video vi: pause/executing[0x%x]->idle ...", pVideoViData->state);
                                //release all frames to video input.
                                if(!VideoBufMgrUsingEmpty(pVideoViData->mpCapMgr))
                                {
                                    // aloge("fatal error! using frame is not empty! check code!");
                                    alogw("Be careful! virChn[%d-%d] using frame is not empty!", pVideoViData->mMppChnInfo.mDevId, pVideoViData->mMppChnInfo.mChnId);
                                }
                                //return all valid frames to videoInput
                                DoVideoViReturnAllValidFrames(pVideoViData);
                                pVideoViData->state = COMP_StateIdle;
                                alogv("Set Virvi COMP_StateIdle OK");
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                       pVideoViData->state, NULL);
                            } else {
                                aloge("fatal error! current state[0x%x] can't turn to idle!", pVideoViData->state);
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        case COMP_StateExecuting: {  // Transition can only happen from pause or idle state
                            if (pVideoViData->state == COMP_StateIdle || pVideoViData->state == COMP_StatePause) {
                                pVideoViData->state = COMP_StateExecuting;
                                alogv("Set Virvi COMP_StateExecuting OK");
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                       pVideoViData->state, NULL);
                            } else {
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        case COMP_StatePause: {
                            // Transition can only happen from idle or executing state
                            if (pVideoViData->state == COMP_StateIdle || pVideoViData->state == COMP_StateExecuting) {
                                pVideoViData->state = COMP_StatePause;
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                       pVideoViData->state, NULL);
                            } else {
                                pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                                                                       COMP_EventError,
                                                                       ERR_VI_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&pVideoViData->mStateLock);
            } else if (cmd == Flush) {
                alogv("Receive command Flush.");
            } else if (cmd == Stop) {
                goto EXIT; /* Kill thread */
            } else if (cmd == VViComp_InputFrameAvailable) {
                alogv("(f:%s, l:%d) frame input", __FUNCTION__, __LINE__);
            } else if (cmd == VViComp_DropFrame) {
                alogv("frame drop");
            } else if (cmd == VViComp_LongExpEvent) {
                if (E_VI_LONGEXP_SET == cmd_msg.para0) {
                    pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                        COMP_EventBufferFlag, cmd_msg.para1, pVideoViData->mMppChnInfo.mDevId, NULL);
                } else if (E_VI_LONGEXP_RESET == cmd_msg.para0) {
                    VI_SHUTTIME_CFG_S mShutter;

                    mShutter.eShutterMode = VI_SHUTTIME_MODE_AUTO;
                    pVideoViData->pCallbacks->EventHandler(pVideoViData->hSelf, pVideoViData->pAppData,
                        COMP_EventBufferFlag, cmd_msg.para1, pVideoViData->mMppChnInfo.mDevId, (void *)&mShutter);

                }
            } else if (cmd == VViComp_StoreFrame) {
                if(FALSE == pVideoViData->mbStoreFrame)
                {
                    pVideoViData->mbStoreFrame = TRUE;
                }
                else
                {
                    alogw("Be careful! storeFrame is already true! maybe process slow.");
                }
                snprintf(pVideoViData->mDbgStoreFrameFilePath, sizeof(pVideoViData->mDbgStoreFrameFilePath), "%s", (char*)cmd_msg.mpData);
                /* mpData is malloced in TMessageDeepCopyMessage() */
                free(cmd_msg.mpData);
                cmd_msg.mpData = NULL;
                cmd_msg.mDataSize = 0;
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (pVideoViData->state == COMP_StateExecuting)
        {
            /* For online, csi&ve driver transfer data directly, and virvi channel does not need to process data. */
            if (0 != pVideoViData->mViAttr.mOnlineEnable)
            {
                TMessage_WaitQueueNotEmpty(&pVideoViData->cmd_queue, 0);
                goto PROCESS_MESSAGE;
            }

            /* For offline. */
            if (TRUE == pVideoViData->mOutputPortTunnelFlag)
            {
                /* Tunnel */
                //alogw("Tunnel virvi debug.\r\n");
                COMP_INTERNAL_TUNNELINFOTYPE *pPortTunnelInfo = &pVideoViData->sPortTunnelInfo[VI_CHN_PORT_INDEX_OUT];
                MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)pPortTunnelInfo->hTunnel;
                if (pOutTunnelComp != NULL)
                {
                    COMP_BUFFERHEADERTYPE obh;
                    pthread_mutex_lock(&pVideoViData->mFrameLock);
                    VIDEO_FRAME_INFO_S *pFrm = VideoBufMgrGetValidFrame(pVideoViData->mpCapMgr);
                    if (pFrm == NULL)
                    {
                        pVideoViData->mWaitingCapDataFlag = TRUE;
                        pthread_mutex_unlock(&pVideoViData->mFrameLock);
                        int msgNum = TMessage_WaitQueueNotEmpty(&pVideoViData->cmd_queue, 2000);
                        if(0 == msgNum)
                        {
                            alogd("virvi[%d-%d] has no frame input?", pVideoViData->mMppChnInfo.mDevId, pVideoViData->mMppChnInfo.mChnId);
                        }
                        goto PROCESS_MESSAGE;
                    }
                    pthread_mutex_unlock(&pVideoViData->mFrameLock);
                    if(pVideoViData->mbStoreFrame)
                    {
                        char strPixFmt[16];
                        if(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pFrm->VFrame.mPixelFormat)
                        {
                            strcpy(strPixFmt, "nv21");
                        }
                        else if(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == pFrm->VFrame.mPixelFormat)
                        {
                            strcpy(strPixFmt, "nv12");
                        }
                        else if(MM_PIXEL_FORMAT_YUV_AW_AFBC == pFrm->VFrame.mPixelFormat)
                        {
                            strcpy(strPixFmt, "afbc");
                        }
                        else if(MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X == pFrm->VFrame.mPixelFormat)
                        {
                            strcpy(strPixFmt, "lbc20x");
                        }
                        else if(MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X == pFrm->VFrame.mPixelFormat)
                        {
                            strcpy(strPixFmt, "lbc25x");
                        }
                        else if(MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X == pFrm->VFrame.mPixelFormat)
                        {
                            strcpy(strPixFmt, "lbc15x");
                        }
                        else if(MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X == pFrm->VFrame.mPixelFormat)
                        {
                            strcpy(strPixFmt, "lbc10x");
                        }
                        else
                        {
                            strcpy(strPixFmt, "unknown");
                        }
                        int nPos = strlen(pVideoViData->mDbgStoreFrameFilePath);
                        snprintf(pVideoViData->mDbgStoreFrameFilePath+nPos, VI_MAXPATHSIZE-nPos,
                            "/vipp[%d]virChn[%d]frmId[%d]pic[%d].%s", pVideoViData->mMppChnInfo.mDevId,
                            pVideoViData->mMppChnInfo.mChnId, pFrm->mId, pVideoViData->mStoreFrameCount++, strPixFmt);
                        FILE *dbgFp = fopen(pVideoViData->mDbgStoreFrameFilePath, "wb");
                        VideoFrameBufferSizeInfo FrameSizeInfo;
                        getVideoFrameBufferSizeInfo(pFrm, &FrameSizeInfo);
                        int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
                        for(int i=0; i<3; i++)
                        {
                            if(pFrm->VFrame.mpVirAddr[i] != NULL)
                            {
                                fwrite(pFrm->VFrame.mpVirAddr[i], 1, yuvSize[i], dbgFp);
                                alogd("virAddr[%d]=[%p], length=[%d]", i, pFrm->VFrame.mpVirAddr[i], yuvSize[i]);
                            }
                        }
                        fclose(dbgFp);
                        pVideoViData->mbStoreFrame = FALSE;
                        alogd("store VI frame in file[%s]", pVideoViData->mDbgStoreFrameFilePath);
                    }

                    obh.nOutputPortIndex = pPortTunnelInfo->nPortIndex;
                    obh.nInputPortIndex = pPortTunnelInfo->nTunnelPortIndex;
                    //obh.pOutputPortPrivate = pFrm;
                    obh.pOutputPortPrivate = pFrm;
                    int eError = pOutTunnelComp->EmptyThisBuffer(pOutTunnelComp, &obh);
                    if(SUCCESS != eError) 
                    {
                        if(eError == ERR_VENC_BUF_FULL)
                        {
                            alogd("VencComp refuse to receive frame because limited frame number, [0x%x].", eError);
                        }
                        else
                        {
                            alogw("Loop Vi_Component Thread OutTunnelComp EmptyThisBuffer failed %x, return this frame", eError);
                        }
                        videoInputHw_RefsReduceAndRleaseData(pVideoViData->mMppChnInfo.mDevId, pFrm);
                        VideoBufMgrReleaseFrame(pVideoViData->mpCapMgr, pFrm);
                    }
                }
                else
                {
                    TMessage_WaitQueueNotEmpty(&pVideoViData->cmd_queue, 0);
                    goto PROCESS_MESSAGE;
                }
            }
            else
            {
                /* non-Tunnel */
                if(pVideoViData->mViVirChnAttr.mCacheFrameNum > 0)
                {
                    if (1 == pVideoViData->mViVirChnAttr.mCachePolicy)
                    {
                        int nFrameNum = 0;
                        do
                        {
                            nFrameNum = pVideoViData->mpCapMgr->GetValidUsingFrameNum(pVideoViData->mpCapMgr);
                            alogv("nFrameNum=%d", nFrameNum);
                            if(nFrameNum <= pVideoViData->mViVirChnAttr.mCacheFrameNum)
                            {
                                break;
                            }
                            VIDEO_FRAME_INFO_S *pOldFrm = VideoBufMgrGetValidFrame(pVideoViData->mpCapMgr);
                            if (pOldFrm)
                            {
                                videoInputHw_RefsReduceAndRleaseData(pVideoViData->mMppChnInfo.mDevId, pOldFrm);
                                VideoBufMgrReleaseFrame(pVideoViData->mpCapMgr, pOldFrm);
                            }
                        } while(1);
                    }
                }
                TMessage_WaitQueueNotEmpty(&pVideoViData->cmd_queue, 200);
            }
        }
        else
        {
            alogv("VI ComponentThread not OMX_StateExecuting\n");
            TMessage_WaitQueueNotEmpty(&pVideoViData->cmd_queue, 0);
        }
    }

EXIT:
    alogv("VideoVirvi ComponentThread stopped");
    return (void *)SUCCESS;
}

