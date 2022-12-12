/*
 * Copyright (C) 2008-2015 Allwinner Technology Co. Ltd.
 * Author: Ning Fang <fangning@allwinnertech.com>
 *         Caoyuan Yang <yangcaoyuan@allwinnertech.com>
 *
 * This software is confidential and proprietary and may be used
 * only as expressly authorized by a licensing agreement from
 * Softwinner Products.
 *
 * The entire notice above must be reproduced on all copies
 * and should not be removed.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//#define LOG_TAG "venc"

#include "cdc_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "vencoder.h"
#include "FrameBufferManager.h"
#include "venc_device.h"
#include "EncAdapter.h"
#include "veAdapter.h"
#include "memoryAdapter.h"
#include "CdcUtil.h"
#include "cdc_version.h"
#include "CdcIniparserapi.h"
#define MAX_FRAME_BUFFER_NUM 32

#define AW_ENCODER_SHOW_SPEED_INFO (0)

#define MAX_VENCODER_CHANNEL_NUM (16)

void* OnlineThreadProcess(void* param);
void* OfflineThreadProcess(void* param);

extern void* EncppCreate();
extern void EncppDestroy(void* encpp);
extern int EncppFunction(void* encpp,
                         VencEncppBufferInfo* pInBuffer,
                         VencEncppBufferInfo* pOutBuffer,
                         VencEncppFuncParam* pEncppFunction);

typedef enum VencoderCmd
{
    VENCODER_CMD_SET_STATE_INIT          = 0,
    VENCODER_CMD_SET_STATE_EXECUT        = 1,//  sync cmd
    VENCODER_CMD_SET_STATE_PAUSE         = 2,//  sync cmd
    VENCODER_CMD_RESET                   = 3,//  sync cmd
    VENCODER_CMD_EXIT                    = 4,//  sync cmd
    VENCODER_CMD_InputFrameAvailable     = 5,// async cmd
    VENCODER_CMD_OutputStreamAvailable   = 6,// async cmd
}VencoderCmd;

#define VENC_SEM_WAIT_REPLY_NUM (VENCODER_CMD_EXIT + 1)

typedef enum VencoderState
{
	VENCODER_STATE_IDLE     = 0,
	VENCODER_STATE_INITED   = 1,
	VENCODER_STATE_EXCUTING = 2,
	VENCODER_STATE_PAUSE    = 3,
}VencoderState;

typedef struct ShowSpeedInfoCtx
{
    int bEnable;

    int64_t nCurFrameStartTime;
    int64_t nCurFrameEndTime;

    int64_t nPeriodStartTime;
    int64_t nPeriodEndTime;
    int64_t nPeriodCostTotalTime;
    int64_t nPeriodFrameNum;

    int64_t nPeriodMaxTime;
    int64_t nPeriodMinTime;

}ShowSpeedInfoCtx;

typedef struct OverlayParam
{
    unsigned char*   dataBuf;
    unsigned int     dataSize;
    VencOverlayInfoS mOverlayInfo;
    pthread_mutex_t  mutex;
    unsigned int     bNeedUpdateFlag;
}OverlayParam;

typedef struct VencContext
{
    unsigned int         mDbgVbvFullContiguousCnt;
    int                  nChannelId;

    OverlayParam         mOverlayParam;
    VENC_DEVICE*         pVEncDevice;
    void*                pEncoderHandle;
    FrameBufferManager*  pFBM;
    VencBaseConfig        baseConfig;
    VencHeaderData        headerData;
    VencInputBuffer      curEncInputbuffer;
    VENC_CODEC_TYPE         codecType;
    unsigned int         ICVersion;
    int                  bInit;
    struct ScMemOpsS    *memops;
    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;
    ShowSpeedInfoCtx  mShowSpeedInfoCtx;
    pthread_t         processThread;
    VencoderState     eState;
    CdcMessageQueue*  mq;
    sem_t             reply_sem[VENC_SEM_WAIT_REPLY_NUM];

    unsigned int      bNoInputFrameFlag;
    pthread_mutex_t   mInputFrameLock;
    unsigned int      bNoOutputStreamBufFlag;
    pthread_mutex_t   mOutputStreamLock;
    VENC_RC_MODE      eRcMode;
    unsigned int      bNeedUpdateMbStatInfoFlag;
    unsigned int      bNeedUpdateSharpParamFlag;

    VencCbType*      pCallbacks;
    void*            pAppData;
}VencContext;

typedef enum OnlineBufNumInfo
{
    Had_No_Online_Channel,
    Had_Online_Channel_One_Buf,
    Had_Online_Channel_Two_Buf
}OnlineBufNumInfo;

typedef struct OnlineVencContext
{
    VencContext *pVencCxt[MAX_VENCODER_CHANNEL_NUM];
    pthread_t    onlineThread;
    int          nValidChannelNum;
    unsigned int      bNoInputFrameFlag;
    pthread_mutex_t   mInputFrameLock;
    unsigned int      bNoOutputStreamBufFlag;
    pthread_mutex_t   mOutputStreamLock;
    CdcMessageQueue*  mq;
    int               nOnlineChannelIndex;
    OnlineBufNumInfo  eOnlineBufNum;
}OnlineVencContext;

static OnlineVencContext *pOnlineCxt = NULL;
static pthread_mutex_t   OnlineMutex = PTHREAD_MUTEX_INITIALIZER;

extern struct ScMemOpsS* MemAdapterGetOpsS();

extern VENC_DEVICE video_encoder_h264_ver1;
extern VENC_DEVICE video_encoder_jpeg;
extern VENC_DEVICE video_encoder_h264_ver2;
extern VENC_DEVICE video_encoder_h265;
extern VENC_DEVICE video_encoder_vp8;

VENC_DEVICE* video_encoder_devices[] =
{
    &video_encoder_h264_ver1,
    &video_encoder_jpeg,
    &video_encoder_h264_ver2,
    &video_encoder_h265,
    //&video_encoder_vp8,  // not any product use vp8 encoder now. (bz)
    0
};

static int postMessgaeAndWait(VencContext* venc_ctx, int cmd,  unsigned char* param1, unsigned char* param2)
{
	CdcMessage msg;
	memset(&msg, 0, sizeof(CdcMessage));
	msg.messageId  = cmd;
	msg.params[0]  = (uintptr_t)(&venc_ctx->reply_sem[cmd]);
    msg.params[1]  = (uintptr_t)venc_ctx;
    msg.params[2]  = (uintptr_t)param1;
    msg.params[3]  = (uintptr_t)param2;

    if(venc_ctx->baseConfig.bOnlineMode == 0)
    	CdcMessageQueuePostMessage(venc_ctx->mq, &msg);
    else
        CdcMessageQueuePostMessage(pOnlineCxt->mq, &msg);

	logv("********* cmd = %d, wait reply start", cmd);
	SemTimedWait(&venc_ctx->reply_sem[cmd], -1);
	logv("********* cmd = %d, wait reply finish", cmd);

	return 0;
}


static inline int64_t getCurrentTime(void)
{
    struct timeval tv;
    int64_t time;
    gettimeofday(&tv,NULL);
    time = tv.tv_sec*1000000 + tv.tv_usec;
    return time;
}


static VENC_DEVICE *vencoderDeviceCreate(VENC_CODEC_TYPE type)
{
    VENC_DEVICE *vencoder_device_handle;

    vencoder_device_handle = (VENC_DEVICE *)malloc(sizeof(VENC_DEVICE));
    if(vencoder_device_handle == NULL)
    {
        return NULL;
    }

    memcpy(vencoder_device_handle, video_encoder_devices[type], sizeof(VENC_DEVICE));

    return vencoder_device_handle;
}
static void vencoderDeviceDestroy(void *handle)
{
    if (handle != NULL) {
        free(handle);
    }
}

static int updateSpeedInfo(VencContext* venc_ctx)
{
    ShowSpeedInfoCtx* pSpeedCtx = &venc_ctx->mShowSpeedInfoCtx;

    pSpeedCtx->nPeriodFrameNum++;

    pSpeedCtx->nCurFrameEndTime = getCurrentTime();
    pSpeedCtx->nPeriodEndTime = getCurrentTime();

    int64_t nCurFrameCostTime = pSpeedCtx->nCurFrameEndTime - pSpeedCtx->nCurFrameStartTime;
    pSpeedCtx->nPeriodCostTotalTime += nCurFrameCostTime;

    if(pSpeedCtx->nPeriodMaxTime < nCurFrameCostTime || pSpeedCtx->nPeriodMaxTime == 0)
        pSpeedCtx->nPeriodMaxTime = nCurFrameCostTime;

    if(pSpeedCtx->nPeriodMinTime > nCurFrameCostTime || pSpeedCtx->nPeriodMinTime == 0)
        pSpeedCtx->nPeriodMinTime = nCurFrameCostTime;

    int64_t nPeriodTime = pSpeedCtx->nPeriodEndTime - pSpeedCtx->nPeriodStartTime;
    if(nPeriodTime > 1*1000*1000) //* 1 second
    {
        int64_t nHardwareSpeed = pSpeedCtx->nPeriodCostTotalTime/pSpeedCtx->nPeriodFrameNum;
        int64_t nRealSpeed = nPeriodTime/pSpeedCtx->nPeriodFrameNum;
        logd(" hardware_speed = %0.2f fps, real_speed = %0.2f fps, maxTime = %0.2f ms, minTime = %0.2f ms",
            (float)1000*1000/nHardwareSpeed, (float)1000*1000/nRealSpeed,
            (float)pSpeedCtx->nPeriodMaxTime/1000,
            (float)pSpeedCtx->nPeriodMinTime/1000);

        pSpeedCtx->nCurFrameStartTime = 0;
        pSpeedCtx->nCurFrameEndTime   = 0;

        pSpeedCtx->nPeriodStartTime     = 0;
        pSpeedCtx->nPeriodEndTime       = 0;
        pSpeedCtx->nPeriodCostTotalTime = 0;
        pSpeedCtx->nPeriodFrameNum      = 0;
        pSpeedCtx->nPeriodMaxTime       = 0;
        pSpeedCtx->nPeriodMinTime       = 0;
    }

    return 0;
}

static int encodeOneFrame(VideoEncoder* pEncoder)
{
    int result = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;
    VencCbInputBufferDoneInfo mCbDoneInfo;
    memset(&mCbDoneInfo, 0 , sizeof(VencCbInputBufferDoneInfo));
    VencMBModeCtrl mMbModeCtl;
    memset(&mMbModeCtl, 0, sizeof(VencMBModeCtrl));
    int bBSBufIsFull = 0;

    if(!venc_ctx) {
        return -1;
    }

    pthread_mutex_lock(&venc_ctx->mOverlayParam.mutex);
    if(venc_ctx->mOverlayParam.bNeedUpdateFlag)
    {
        venc_ctx->pVEncDevice->SetParameter(venc_ctx->pEncoderHandle, VENC_IndexParamSetOverlay, (void*)&venc_ctx->mOverlayParam.mOverlayInfo);
        venc_ctx->mOverlayParam.bNeedUpdateFlag = 0;
    }
    pthread_mutex_unlock(&venc_ctx->mOverlayParam.mutex);

    struct ScMemOpsS *_memops = venc_ctx->baseConfig.memops;
    unsigned int phyOffset = EncAdapterGetVeAddrOffset();

    if(venc_ctx->baseConfig.bOnlineChannel == 0)
    {
        if(VencFbmRequestValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer) != 0)
        {
            return VENC_RESULT_NO_FRAME_BUFFER;
        }
    }
    else
    {
        memset(&venc_ctx->curEncInputbuffer, 0, sizeof(VencInputBuffer));
    }

    VencGetParameter((VideoEncoder* )venc_ctx, VENC_IndexParamBSbufIsFull, &bBSBufIsFull);

    if(bBSBufIsFull == 1)
    {
        if(venc_ctx->mDbgVbvFullContiguousCnt == 0)
        {
            logw("bitstream empty buffer is full: id = %d", venc_ctx->nChannelId);
        }
        venc_ctx->mDbgVbvFullContiguousCnt++;
    }
    else if(venc_ctx->mDbgVbvFullContiguousCnt > 0)
    {
        logw("bitstream empty buffer is enough: id = %d", venc_ctx->nChannelId);
        venc_ctx->mDbgVbvFullContiguousCnt = 0;
    }

    if(bBSBufIsFull == 1)
    {
        if(venc_ctx->baseConfig.bOnlineChannel == 0)
        {
            VencFbmReturnValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer);
        }
        if(venc_ctx->pCallbacks->InputBufferDone)
        {
            result = VENC_RESULT_BITSTREAM_IS_FULL;
            mCbDoneInfo.nResult = result;
            mCbDoneInfo.pInputBuffer = &venc_ctx->curEncInputbuffer;
            venc_ctx->pCallbacks->InputBufferDone(pEncoder, venc_ctx->pAppData, &mCbDoneInfo);
        }
        return result;
    }

    venc_ctx->curEncInputbuffer.pAddrPhyY -= phyOffset;
    venc_ctx->curEncInputbuffer.pAddrPhyC -= phyOffset;

    if(venc_ctx->mShowSpeedInfoCtx.bEnable == 1)
    {
        venc_ctx->mShowSpeedInfoCtx.nCurFrameStartTime = getCurrentTime();
        if(venc_ctx->mShowSpeedInfoCtx.nPeriodStartTime == 0)
           venc_ctx->mShowSpeedInfoCtx.nPeriodStartTime = getCurrentTime();
    }

    if(venc_ctx->curEncInputbuffer.bEnableCorp == 1)
    {
        if(venc_ctx->baseConfig.bOnlineChannel == 1 || venc_ctx->baseConfig.eInputFormat == VENC_PIXEL_LBC_AW)
        {
            logw("online-mode and lbc-format are not support crop function, online = %d, inputformat = %d",
                venc_ctx->baseConfig.bOnlineChannel, venc_ctx->baseConfig.eInputFormat);
            venc_ctx->curEncInputbuffer.bEnableCorp = 0;
        }
    }

    //* update encpp sharp param before do encode frame
    if(venc_ctx->pCallbacks->EventHandler)
    {
        VencIsp2VeParam mIsp2VeParam = {0};

        int ret = venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                                      VencEvent_UpdateIspToVeParam, 0, 0, &mIsp2VeParam);
        if(ret == 0)
        {
            VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamEnCameraMove, &mIsp2VeParam.mEnCameraMove);
            VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamEnvLv, &mIsp2VeParam.mEnvLv);
            VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamAeWeightLum, &mIsp2VeParam.mAeWeightLum);
            if(venc_ctx->bNeedUpdateSharpParamFlag == 1)
                VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamSharpConfig, &mIsp2VeParam.mSharpParam);
        }

        if(venc_ctx->eRcMode == AW_QPMAP)
        {
            venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                                VencEvent_UpdateMbModeInfo, 0, 0, &mMbModeCtl);
            if(mMbModeCtl.mode_ctrl_en == 1)
                VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamMBModeCtrl, &mMbModeCtl);
        }

        venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                            VencEvent_UpdateIspMotionParam, 0, 0, NULL);
    }

    logv("pAddrPhyY = %p, pAddrPhyC = %p", venc_ctx->curEncInputbuffer.pAddrPhyY, venc_ctx->curEncInputbuffer.pAddrPhyC);
    CdcVeLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    result = venc_ctx->pVEncDevice->encode(venc_ctx->pEncoderHandle, &venc_ctx->curEncInputbuffer);
    CdcVeUnLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);

    if(venc_ctx->pCallbacks->EventHandler)
    {
        if(venc_ctx->bNeedUpdateMbStatInfoFlag == 1)
        {
            venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                                VencEvent_UpdateMbStatInfo, 0, 0, NULL);
        }

        if(venc_ctx->codecType == VENC_CODEC_H265 || venc_ctx->codecType == VENC_CODEC_H264)
        {
            VencVe2IspParam mVe2IspParam;
            result = venc_ctx->pVEncDevice->GetParameter(venc_ctx->pEncoderHandle, VENC_IndexParamVe2IspParam, &mVe2IspParam);
            if(result == 0)
            {
                venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                                    VencEvent_UpdateVeToIspParam, 0, 0, &mVe2IspParam);
            }
        }
    }

    if(venc_ctx->baseConfig.bOnlineChannel == 0)
    {
        VencFbmReturnValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer);
        mCbDoneInfo.nResult = result;
        mCbDoneInfo.pInputBuffer = &venc_ctx->curEncInputbuffer;
        venc_ctx->pCallbacks->InputBufferDone(pEncoder, venc_ctx->pAppData, &mCbDoneInfo);
    }
    else
    {
        if(venc_ctx->pCallbacks && venc_ctx->pCallbacks->InputBufferDone)
        {
            mCbDoneInfo.nResult = result;
            mCbDoneInfo.pInputBuffer = &venc_ctx->curEncInputbuffer;
            venc_ctx->pCallbacks->InputBufferDone(pEncoder, venc_ctx->pAppData, &mCbDoneInfo);
        }
    }

    if(venc_ctx->mShowSpeedInfoCtx.bEnable == 1 && result == VENC_RESULT_OK)
    {
        updateSpeedInfo(venc_ctx);
    }

    return result;
}

VideoEncoder* VencCreate(VENC_CODEC_TYPE eCodecType)
{
    VencContext* venc_ctx = NULL;
    int i = 0;
    int loglevel;

    const char* loglevelFilePath = CdcGetConfigParamterString("cdc_log_level_file_path", NULL);
    loglevel = CdcGetSpecifyFilePathLogLevel(loglevelFilePath);
    if(loglevel == 0)
    {
        loglevel = CdcGetConfigParamterInt("cdc_log_level", 3);
    }
    if(loglevel < 2 || 6 < loglevel)
    {
        loglevel = 3;
    }

    logd("now cedarc log level:%d", loglevel);
    cdc_log_set_level(loglevel);

    LogVersionInfo();

    venc_ctx = (VencContext*)malloc(sizeof(VencContext));
    if(!venc_ctx){
        loge("malloc VencContext fail!");
        return NULL;
    }

    memset(venc_ctx, 0,sizeof(VencContext));

    venc_ctx->codecType = eCodecType;
    venc_ctx->bInit = 0;

    if(pthread_mutex_init(&venc_ctx->mInputFrameLock, NULL) != 0)
    {
        loge("init mutex failed");
        free(venc_ctx);
        return NULL;
    }
    if(pthread_mutex_init(&venc_ctx->mOutputStreamLock, NULL) != 0)
    {
        loge("init mutex failed");
        free(venc_ctx);
        return NULL;
    }
    if(pthread_mutex_init(&venc_ctx->mOverlayParam.mutex, NULL) != 0)
    {
        loge("init mutex failed");
        free(venc_ctx);
        return NULL;
    }

    int type = VE_OPS_TYPE_NORMAL;
    venc_ctx->veOpsS = GetVeOpsS(type);
    if(venc_ctx->veOpsS == NULL)
    {
        loge("get ve ops failed , type = %d",type);
        free(venc_ctx);
        return NULL;
    }

    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 0;
    mVeConfig.nEncoderFlag = 1;
    mVeConfig.nEnableAfbcFlag = 0;
    mVeConfig.nFormat = eCodecType;
    mVeConfig.nWidth = 0;
    mVeConfig.nResetVeMode = 0;
    venc_ctx->pVeOpsSelf = CdcVeInit(venc_ctx->veOpsS, &mVeConfig);
    if(venc_ctx->pVeOpsSelf == NULL)
    {
        loge("init ve ops failed");
        CdcVeRelease(venc_ctx->veOpsS,venc_ctx->pVeOpsSelf);
        free(venc_ctx);
        return NULL;
    }

    char* baseAddr = CdcVeGetGroupRegAddr(venc_ctx->veOpsS,
                                          venc_ctx->pVeOpsSelf,
                                          REG_GROUP_VETOP);
    CdcVeLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    venc_ctx->ICVersion = EncAdapterGetICVersion(baseAddr);
    CdcVeUnLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);

    //for h264 enc use new driver from ic_1708
    if(eCodecType == VENC_CODEC_H264 && venc_ctx->ICVersion >= 0x1708)
        eCodecType = VENC_CODEC_H264_VER2;

    venc_ctx->pVEncDevice = vencoderDeviceCreate(eCodecType);
    if(venc_ctx->pVEncDevice == NULL)
    {
        loge("VencoderDeviceCreate failed\n");
        free(venc_ctx);
        return NULL;
    }

    venc_ctx->memops = MemAdapterGetOpsS();
    if (venc_ctx->memops == NULL)
    {
        loge("MemAdapterGetOpsS failed\n");
        free(venc_ctx);
        return NULL;
    }

    if(EncAdapterInitializeMem(venc_ctx->memops) != 0)
      {
        loge("can not set up memory runtime environment.");
        free(venc_ctx);
        return NULL;
    }

    venc_ctx->baseConfig.veOpsS = venc_ctx->veOpsS;
    venc_ctx->baseConfig.pVeOpsSelf = venc_ctx->pVeOpsSelf;
    venc_ctx->baseConfig.memops = venc_ctx->memops;

    venc_ctx->pEncoderHandle =
                    venc_ctx->pVEncDevice->open(&venc_ctx->baseConfig, venc_ctx->ICVersion);
    if(!venc_ctx->pEncoderHandle)
    {
        logd("open VEncDevice error\n");
        vencoderDeviceDestroy(venc_ctx->pVEncDevice);
        venc_ctx->pVEncDevice = NULL;
        free(venc_ctx);
        return NULL;
    }

    venc_ctx->mq = CdcMessageQueueCreate(16, "venc_thread");
    for(i = 0; i < VENC_SEM_WAIT_REPLY_NUM; i++)
    {
        sem_init(&venc_ctx->reply_sem[i], 0, 0);
    }

#if(AW_ENCODER_SHOW_SPEED_INFO)
    venc_ctx->mShowSpeedInfoCtx.bEnable = 1;
#endif

    venc_ctx->eState = VENCODER_STATE_IDLE;

    return (VideoEncoder*)venc_ctx;
}

void VencDestroy(VideoEncoder* pEncoder)
{
    int i = 0;
    int ret = 0;
    int nRealValidChannelNum = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx->baseConfig.bOnlineMode == 0)
    {
        postMessgaeAndWait(venc_ctx, VENCODER_CMD_EXIT, NULL, NULL);
        pthread_join(venc_ctx->processThread, (void**)&ret);
    }
    else
    {
        pthread_mutex_lock(&OnlineMutex);
        if(pOnlineCxt)
        {
            postMessgaeAndWait(venc_ctx, VENCODER_CMD_EXIT, NULL, NULL);
            PRINTF_CODE_POS
            for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
            {
                if(pOnlineCxt->pVencCxt[i])
                    nRealValidChannelNum++;
            }
            PRINTF_CODE_POS
            logd("nRealValidChannelNum = %d", nRealValidChannelNum);
            if(nRealValidChannelNum == 0)
            {
                pthread_join(pOnlineCxt->onlineThread, (void**)&ret);

                if(pOnlineCxt->mq)
                    CdcMessageQueueDestroy(pOnlineCxt->mq);

                pthread_mutex_destroy(&pOnlineCxt->mInputFrameLock);
                pthread_mutex_destroy(&pOnlineCxt->mOutputStreamLock);

                free(pOnlineCxt);
                pOnlineCxt = NULL;
            }
        }
        pthread_mutex_unlock(&OnlineMutex);
    }

    venc_ctx->pVEncDevice->uninit(venc_ctx->pEncoderHandle);

    if(venc_ctx->ICVersion == 0x1639)
    {
        if(venc_ctx->baseConfig.nDstWidth >= 3840 || venc_ctx->baseConfig.nDstHeight >= 2160)
        {
            CdcVeUnInitEncoderPerformance(venc_ctx->baseConfig.veOpsS, venc_ctx->baseConfig.pVeOpsSelf, 1);
        }
        else
        {
            CdcVeUnInitEncoderPerformance(venc_ctx->baseConfig.veOpsS, venc_ctx->baseConfig.pVeOpsSelf, 0);
            logd("VeUninitEncoderPerformance");
        }
    }

    if(venc_ctx->pFBM)
    {
        VencFbmDestroy(venc_ctx->pFBM);
        venc_ctx->pFBM = NULL;
    }

    if(venc_ctx->mq)
        CdcMessageQueueDestroy(venc_ctx->mq);

    for(i = 0; i < VENC_SEM_WAIT_REPLY_NUM; i++)
    {
        sem_destroy(&venc_ctx->reply_sem[i]);
    }
    pthread_mutex_destroy(&venc_ctx->mInputFrameLock);
    pthread_mutex_destroy(&venc_ctx->mOutputStreamLock);
    pthread_mutex_destroy(&venc_ctx->mOverlayParam.mutex);

    if(venc_ctx->mOverlayParam.dataBuf)
        free(venc_ctx->mOverlayParam.dataBuf);

    if(venc_ctx->pVEncDevice)
    {
        venc_ctx->pVEncDevice->close(venc_ctx->pEncoderHandle);
        vencoderDeviceDestroy(venc_ctx->pVEncDevice);
        venc_ctx->pVEncDevice = NULL;
        venc_ctx->pEncoderHandle = NULL;
    }

    EncAdpaterRelease(venc_ctx->memops);

    if(venc_ctx->veOpsS)
    {
        CdcVeRelease(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    }

    free(venc_ctx);
}

int VencInit(VideoEncoder* pEncoder, VencBaseConfig* pConfig)
{
    int i = 0;
    int result = 0;
    int bHadOnlineChannel = 0;
    int bHadOfflineChannel = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;
    PRINTF_CODE_POS

    if(pEncoder == NULL || pConfig == NULL || venc_ctx->bInit)
    {
        loge("InitVideoEncoder, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->eState != VENCODER_STATE_IDLE)
    {
        logw("the state[%d] is not idle when call init", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }

    logd("bOnlineMode = %d, bOnlineChannel = %d", pConfig->bOnlineMode, pConfig->bOnlineChannel);
    logd("lbc2.5 = %d, 2.0 = %d, 1.5 = %d, format = %d",
        pConfig->bLbcLossyComEnFlag2_5x,
        pConfig->bLbcLossyComEnFlag2x, pConfig->bLbcLossyComEnFlag1_5x, pConfig->eInputFormat);

    pConfig->memops = venc_ctx->memops;
    pConfig->veOpsS = venc_ctx->veOpsS;
    pConfig->pVeOpsSelf = venc_ctx->pVeOpsSelf;

    venc_ctx->pFBM = VencFbmCreate(MAX_FRAME_BUFFER_NUM, pConfig->memops,
                                    (void *)venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);

    if(venc_ctx->pFBM == NULL)
    {

        loge("venc_ctx->pFBM == NULL");
        return VENC_RESULT_NO_MEMORY;
    }

    //logd("(f:%s, l:%d)", __FUNCTION__, __LINE__);

    if(venc_ctx->ICVersion == 0x1639)
    {
        if(pConfig->nDstWidth >= 3840 || pConfig->nDstHeight>= 2160)
        {
            CdcVeInitEncoderPerformance(pConfig->veOpsS, pConfig->pVeOpsSelf, 1);
        }
        else
        {
            CdcVeInitEncoderPerformance(pConfig->veOpsS, pConfig->pVeOpsSelf, 0);
            logd("VeInitEncoderPerformance");
        }
    }

    //logd("(f:%s, l:%d)", __FUNCTION__, __LINE__);

    memcpy(&venc_ctx->baseConfig, pConfig, sizeof(VencBaseConfig));

    venc_ctx->baseConfig.veOpsS = venc_ctx->veOpsS;
    venc_ctx->baseConfig.pVeOpsSelf = venc_ctx->pVeOpsSelf;

    result = venc_ctx->pVEncDevice->init(venc_ctx->pEncoderHandle, &venc_ctx->baseConfig);
    if(VENC_RESULT_OK == result)
    {
        venc_ctx->bInit = 1;
    }
    else
    {
        loge("venc_init_fail");
    }

    if(pConfig->bOnlineMode == 1)
    {
        int bNeedCreateThreadFlag = 0;
        pthread_mutex_lock(&OnlineMutex);
        if(pOnlineCxt == NULL)
        {
            pOnlineCxt = calloc(1, sizeof(OnlineVencContext));
            if(pOnlineCxt == NULL)
            {
                loge("calloc failed, size = %d", sizeof(OnlineVencContext));
            }
            pOnlineCxt->nOnlineChannelIndex = -1;
            pOnlineCxt->eOnlineBufNum = Had_No_Online_Channel;

            pOnlineCxt->mq = CdcMessageQueueCreate(32, "vencOn_thread");

            if(pthread_mutex_init(&pOnlineCxt->mInputFrameLock, NULL) != 0)
            {
                loge("init mutex failed");
                free(pOnlineCxt);
                pthread_mutex_unlock(&OnlineMutex);
                return VENC_RESULT_ERROR;
            }
            if(pthread_mutex_init(&pOnlineCxt->mOutputStreamLock, NULL) != 0)
            {
                loge("init mutex failed");
                free(pOnlineCxt);
                pthread_mutex_unlock(&OnlineMutex);
                return VENC_RESULT_ERROR;
            }
            bNeedCreateThreadFlag = 1;
        }

        if(pOnlineCxt)
        {
            for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
            {
                if(pOnlineCxt->pVencCxt[i] == NULL)
                {
                    venc_ctx->nChannelId = i;
                    pOnlineCxt->pVencCxt[i] = venc_ctx;
                    pOnlineCxt->nValidChannelNum++;

                    if(venc_ctx->baseConfig.bOnlineChannel == 1)
                        pOnlineCxt->nOnlineChannelIndex = i;

                    break;
                }
            }
            if(i >= MAX_VENCODER_CHANNEL_NUM)
            {
                loge("cannot find empty pVencCxt: %d", i);
                return -1;
            }
        }

        if(pConfig->bOnlineChannel)
        {
            if(pConfig->nOnlineShareBufNum == 1)
                pOnlineCxt->eOnlineBufNum = Had_Online_Channel_One_Buf;
            if(pConfig->nOnlineShareBufNum == 2)
                pOnlineCxt->eOnlineBufNum = Had_Online_Channel_Two_Buf;
        }
        logd("pOnlineCxt->eOnlineBufNum = %d", pOnlineCxt->eOnlineBufNum);

        bHadOnlineChannel  = 0;
        bHadOfflineChannel = 0;
        int OnlineIndex = 0;
        for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
        {
            if(pOnlineCxt->pVencCxt[i])
            {
                if(pOnlineCxt->pVencCxt[i]->baseConfig.bOnlineChannel == 0)
                    bHadOfflineChannel = 1;
                else if(pOnlineCxt->pVencCxt[i]->baseConfig.bOnlineChannel == 1)
                {
                    bHadOnlineChannel = 1;
                    OnlineIndex = i;
                }
            }
        }

        //* we need check online status when had onlineChannel and offlineChannel
        if(bHadOnlineChannel == 1 && bHadOfflineChannel == 1)
        {
            unsigned int bEnableCheckOnlineStatus = 1;
            VencSetParameter((VideoEncoder*)pOnlineCxt->pVencCxt[OnlineIndex],
                              VENC_IndexParamEnableCheckOnlineStatus,
                              &bEnableCheckOnlineStatus);
        }
        if(bNeedCreateThreadFlag == 1)
            pthread_create(&pOnlineCxt->onlineThread, NULL, OnlineThreadProcess, NULL);

        pthread_mutex_unlock(&OnlineMutex);

    }
    else
    {
        pthread_create(&venc_ctx->processThread, NULL, OfflineThreadProcess, (void*)venc_ctx);
    }

    PRINTF_CODE_POS

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_SET_STATE_INIT, NULL, NULL);
    return result;
}

int VencStart(VideoEncoder* pEncoder)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    if(venc_ctx == NULL)
    {
        loge("VencoderStart, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->eState != VENCODER_STATE_INITED && venc_ctx->eState != VENCODER_STATE_PAUSE)
    {
        logw("the state[%d] is not inited or pause when call start", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }
    PRINTF_CODE_POS

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_SET_STATE_EXECUT, NULL, NULL);
    return VENC_RESULT_OK;
}

int VencPause(VideoEncoder* pEncoder)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    if(venc_ctx == NULL)
    {
        loge("VencoderPause, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->eState != VENCODER_STATE_EXCUTING)
    {
        logw("the state[%d] is not excuting when call pause", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_SET_STATE_PAUSE, NULL, NULL);
    return VENC_RESULT_OK;
}

int VencReset(VideoEncoder* pEncoder)
{
    int result = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL)
    {
        loge("VencReset, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }

    if(venc_ctx->eState != VENCODER_STATE_EXCUTING && venc_ctx->eState != VENCODER_STATE_PAUSE)
    {
        logw("the state[%d] is not excuting or pause when call reset", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_RESET, NULL, NULL);
    return result;
}

static int updateOverlayParam(VencContext* venc_ctx, VencOverlayInfoS *pSrcOverlayInfo)
{
    int i = 0;
    int totalOverlayDataLen = 0;
    OverlayParam* pDstOverlay = &venc_ctx->mOverlayParam;
    unsigned char* pCurBuf = NULL;


    pthread_mutex_lock(&pDstOverlay->mutex);

    for(i = 0; i < pSrcOverlayInfo->blk_num; i++)
        totalOverlayDataLen += pSrcOverlayInfo->overlayHeaderList[i].bitmap_size;

    if(totalOverlayDataLen > pDstOverlay->dataSize)
    {
        if(pDstOverlay->dataBuf)
            free(pDstOverlay->dataBuf);
        pDstOverlay->dataBuf = malloc(totalOverlayDataLen);
        if(pDstOverlay->dataBuf == NULL)
        {
            loge("malloc failed, size = %d", totalOverlayDataLen);
            pthread_mutex_unlock(&pDstOverlay->mutex);
            return -1;
        }
        memset(pDstOverlay->dataBuf, 0, totalOverlayDataLen);
        pDstOverlay->dataSize = totalOverlayDataLen;
    }

    memcpy(&pDstOverlay->mOverlayInfo, pSrcOverlayInfo, sizeof(VencOverlayInfoS));
    pCurBuf = pDstOverlay->dataBuf;
    for(i = 0; i < pSrcOverlayInfo->blk_num; i++)
    {
        memcpy(pCurBuf, pSrcOverlayInfo->overlayHeaderList[i].overlay_blk_addr, pSrcOverlayInfo->overlayHeaderList[i].bitmap_size);
        pDstOverlay->mOverlayInfo.overlayHeaderList[i].overlay_blk_addr = pCurBuf;
        pDstOverlay->mOverlayInfo.overlayHeaderList[i].bitmap_size      = pSrcOverlayInfo->overlayHeaderList[i].bitmap_size;
        pCurBuf += pSrcOverlayInfo->overlayHeaderList[i].bitmap_size;
    }
    pDstOverlay->bNeedUpdateFlag = 1;

    pthread_mutex_unlock(&pDstOverlay->mutex);
    return 0;
}

int VencGetParameter(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData)
{
    CEDARC_UNUSE(VencGetParameter);
    VencContext* venc_ctx = (VencContext*)pEncoder;
    return venc_ctx->pVEncDevice->GetParameter(venc_ctx->pEncoderHandle, indexType, paramData);
}

int VencSetParameter(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData)
{
    int ret = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;
    int setParamInThread = 0;
    if(indexType == VENC_IndexParamH264Param)
    {
        VencH264Param *pH264Param = (VencH264Param *)paramData;
        venc_ctx->eRcMode = pH264Param->sRcParam.eRcMode;
    }
    else if(indexType == VENC_IndexParamH265Param)
    {
        VencH265Param *pH265Param = (VencH265Param *)paramData;
        venc_ctx->eRcMode = pH265Param->sRcParam.eRcMode;
    }
    else if(indexType == VENC_IndexParamMBInfoOutput)
    {
        VencMBInfo *pMBInfo = (VencMBInfo *)paramData;
        if(pMBInfo->num_mb > 0 && pMBInfo->p_para)
            venc_ctx->bNeedUpdateMbStatInfoFlag = 1;
    }
    else if(indexType == VENC_IndexParamEnableEncppSharp)
    {
        venc_ctx->bNeedUpdateSharpParamFlag = *((unsigned int*)paramData);
    }

    if(indexType == VENC_IndexParamSetOverlay)
    {
        return updateOverlayParam(venc_ctx, (VencOverlayInfoS *)paramData);
    }

    logv("indexType = %d, gdcIndex = %d", indexType, VENC_IndexParamGdcConfig);
    //* sync mutex in sub-encoder when gdc and sharp config, as it will call every frame
    //* and if sync by CdcVeLock, it cost too much time
    if(indexType == VENC_IndexParamGdcConfig || indexType == VENC_IndexParamSharpConfig)
    {
        ret = venc_ctx->pVEncDevice->SetParameter(venc_ctx->pEncoderHandle, indexType, paramData);
    }
    else
    {
        CdcVeLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
        ret = venc_ctx->pVEncDevice->SetParameter(venc_ctx->pEncoderHandle, indexType, paramData);
        CdcVeUnLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    }

    return ret;
}

int VencGetValidOutputBufNum(VideoEncoder* pEncoder)
{
    CEDARC_UNUSE(VencGetValidOutputBufNum);
    VencContext* venc_ctx = (VencContext*)pEncoder;
    return venc_ctx->pVEncDevice->ValidBitStreamFrameNum(venc_ctx->pEncoderHandle);
}

int VencDequeueOutputBuf(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(!venc_ctx) {
        return -1;
    }

    if(venc_ctx->pVEncDevice->GetOneBitStreamFrame(venc_ctx->pEncoderHandle, pBuffer)!=0)
    {
        return VENC_RESULT_BITSTREAM_IS_EMPTY;
    }

    return 0;
}

int VencQueueOutputBuf(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(!venc_ctx || (venc_ctx->baseConfig.bOnlineMode && !pOnlineCxt)) {
        return -1;
    }

    if(venc_ctx->pVEncDevice->FreeOneBitStreamFrame(venc_ctx->pEncoderHandle, pBuffer)!=0)
    {
        return -1;
    }

    if(venc_ctx->baseConfig.bOnlineMode == 0)
    {
        pthread_mutex_lock(&venc_ctx->mOutputStreamLock);
        if(venc_ctx->bNoOutputStreamBufFlag == 1)
        {
            CdcMessage msg;
            memset(&msg, 0, sizeof(CdcMessage));
            msg.messageId  = VENCODER_CMD_OutputStreamAvailable;
            CdcMessageQueuePostMessage(venc_ctx->mq, &msg);
        }
        pthread_mutex_unlock(&venc_ctx->mOutputStreamLock);
    }
    else
    {
        //* just send msg for online channel; not for offline channel.
        //* if you do not understand, please look func of checkIsReady_online1Buf()
        pthread_mutex_lock(&pOnlineCxt->mOutputStreamLock);
        if(venc_ctx->baseConfig.bOnlineChannel == 1 && pOnlineCxt->bNoOutputStreamBufFlag == 1)
        {
            CdcMessage msg;
            memset(&msg, 0, sizeof(CdcMessage));
            msg.messageId  = VENCODER_CMD_OutputStreamAvailable;
            CdcMessageQueuePostMessage(pOnlineCxt->mq, &msg);
        }
        pthread_mutex_unlock(&pOnlineCxt->mOutputStreamLock);
    }

    return 0;
}

int VencAllocateInputBuf(VideoEncoder* pEncoder, VencAllocateBufferParam *pBufferParam, VencInputBuffer* dst_inputBuf)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(pEncoder == NULL || pBufferParam == NULL)
    {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->pFBM == NULL)
    {
        loge("venc_ctx->pFBM == NULL, must call InitVideoEncoder firstly");
        return VENC_RESULT_NO_RESOURCE;
    }
    PRINTF_CODE_POS

    if(VencFbmAllocateBuffer(venc_ctx->pFBM, pBufferParam, dst_inputBuf)!=0)
    {
        loge("allocat inputbuffer failed");
        return VENC_RESULT_NO_MEMORY;
    }

    return 0;
}

int VencQueueInputBuf(VideoEncoder* pEncoder, VENC_IN VencInputBuffer *inputbuffer)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    int nSemCnt = 0;

    if(pEncoder == NULL || inputbuffer == NULL || (venc_ctx->baseConfig.bOnlineMode && !pOnlineCxt))
    {
        loge("param is NULL: pEncoder = %p, inputbuffer = %p", pEncoder, inputbuffer);
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->pFBM == NULL)
    {

        loge("venc_ctx->pFBM == NULL, must call InitVideoEncoder firstly");
        return VENC_RESULT_NO_RESOURCE;
    }

    if(VencFbmAddValidBuffer(venc_ctx->pFBM, inputbuffer) != 0)
    {
        logv("AddValidBuffer failed");
        return -1;
    }

    if(venc_ctx->baseConfig.bOnlineMode == 0)
    {
        pthread_mutex_lock(&venc_ctx->mInputFrameLock);
        if(venc_ctx->bNoInputFrameFlag == 1)
        {
            venc_ctx->bNoInputFrameFlag = 0;
            CdcMessage msg;
            memset(&msg, 0, sizeof(CdcMessage));
            msg.messageId  = VENCODER_CMD_InputFrameAvailable;
            CdcMessageQueuePostMessage(venc_ctx->mq, &msg);
        }
        pthread_mutex_unlock(&venc_ctx->mInputFrameLock);
    }
    else
    {
        pthread_mutex_lock(&pOnlineCxt->mInputFrameLock);
        if(pOnlineCxt->bNoInputFrameFlag == 1)
        {
            pOnlineCxt->bNoInputFrameFlag = 0;
            CdcMessage msg;
            memset(&msg, 0, sizeof(CdcMessage));
            msg.messageId  = VENCODER_CMD_InputFrameAvailable;
            CdcMessageQueuePostMessage(pOnlineCxt->mq, &msg);
        }
        pthread_mutex_unlock(&pOnlineCxt->mInputFrameLock);
    }

    return 0;
}

int VencGetValidInputBufNum(VideoEncoder* pEncoder)
{
    CEDARC_UNUSE(VencGetValidInputBufNum);

    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(!venc_ctx) {
        return -1;
    }
    return VencFbmGetValidBufferNum(venc_ctx->pFBM);
}

void VencGetVeIommuAddr(VideoEncoder* pEncoder, struct user_iommu_param *pIommuBuf)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    CdcVeGetIommuAddr(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, pIommuBuf);
    return;
}

void VencFreeVeIommuAddr(VideoEncoder* pEncoder, struct user_iommu_param *pIommuBuf)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    CdcVeFreeIommuAddr(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, pIommuBuf);
    return;
}

int VencSetFreq(VideoEncoder* pEncoder, int nVeFreq)
{
    CEDARC_UNUSE(VencSetFreq);
    VencContext* venc_ctx = (VencContext*)pEncoder;
    int ret;
    ret = CdcVeSetSpeed(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, nVeFreq);
    if(ret < 0)
        loge("VideoEncoderSetFreq %d error, ret is %d\n", nVeFreq, ret);

    return ret;
}

void VencSetDdrMode(VideoEncoder* pEncoder, int nDdrType)
{
    CEDARC_UNUSE(VencSetDdrMode);
    VencContext* venc_ctx = (VencContext*)pEncoder;

    CdcVeSetDdrMode(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, nDdrType);

    return;
}

int VencSetCallbacks(VideoEncoder* pEncoder, VencCbType* pCallbacks, void* pAppData)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(pEncoder == NULL || pCallbacks == NULL)
    {
        loge("VencSetCallbacks, param is NULL: %p, %p", pEncoder, pCallbacks);
        return VENC_RESULT_NULL_PTR;
    }

    venc_ctx->pCallbacks = pCallbacks;
    venc_ctx->pAppData   = pAppData;

    return 0;
}

int VencJpegEnc(JpegEncInfo* pJpegInfo, EXIFInfo* pExifInfo, void* pOutBuffer, int* pOutBufferSize)
{
    CEDARC_UNUSE(VencJpegEnc);

    VencAllocateBufferParam bufferParam;
    VideoEncoder* pVideoEnc = NULL;
    VencInputBuffer inputBuffer;
    VencOutputBuffer outputBuffer;
    int result = 0;
    struct user_iommu_param iommu_buffer;
    int ion_fd = -1;
    aw_ion_user_handle_t  handle_ion = ION_USER_HANDLE_INIT_VALUE;
    int                   nBufFd = -1;
    unsigned char         memory_type;

    memory_type = CdcIonGetMemType();
    memset(&inputBuffer, 0, sizeof(VencInputBuffer));

    logd("memory_type[%d](0:normal, 1:iommu), pAddrPhyY[%p], pAddrPhyC[%p],\
    quality[%d], nShareBufFd[%d], cropFlag[%d]\n",
                                                        memory_type,
                                                        pJpegInfo->pAddrPhyY,
                                                        pJpegInfo->pAddrPhyC,
                                                        pJpegInfo->quality,
                                                        pJpegInfo->nShareBufFd,
                                                        pJpegInfo->bEnableCorp);
    logd("input_size[%dx%d], output_size[%dx%d], stride[%d], color_format[%d]\n",
                                                        pJpegInfo->sBaseInfo.nInputWidth,
                                                        pJpegInfo->sBaseInfo.nInputHeight,
                                                        pJpegInfo->sBaseInfo.nDstWidth,
                                                        pJpegInfo->sBaseInfo.nDstHeight,
                                                        pJpegInfo->sBaseInfo.nStride,
                                                        pJpegInfo->sBaseInfo.eInputFormat);
    logd("thumb_size[%dx%d]\n",
                                                        pExifInfo->ThumbWidth,
                                                        pExifInfo->ThumbHeight);

    pVideoEnc = VencCreate(VENC_CODEC_JPEG);

    VencSetParameter(pVideoEnc, VENC_IndexParamJpegExifInfo, pExifInfo);
    VencSetParameter(pVideoEnc, VENC_IndexParamJpegQuality, &pJpegInfo->quality);

    if(VencInit(pVideoEnc, &pJpegInfo->sBaseInfo) != 0)
    {
        result = -1;
        goto ERROR;
    }

    if(pJpegInfo->bNoUseAddrPhy)
    {
        bufferParam.nSizeY = pJpegInfo->sBaseInfo.nStride*pJpegInfo->sBaseInfo.nInputHeight;
        bufferParam.nSizeC = bufferParam.nSizeY>>1;

        if(VencAllocateInputBuf(pVideoEnc, &bufferParam, &inputBuffer)<0)
        {
            result = -1;
            goto ERROR;
        }
    }
    else
    {
        //get ion fd
        ion_fd = CdcIonOpen();
        if (ion_fd < 0)
        {
            loge("open ion fd failed \n");
            result = -1;
            goto ERROR;
        }

        //import ion buffer handle
        result = CdcIonImport(ion_fd, pJpegInfo->nShareBufFd, &handle_ion);
        if (result < 0)
        {
            loge("CdcIonImport get handle failed\n");
            result = -1;
            goto ERROR;
        }

        //get ion buffer fd
        nBufFd = CdcIonGetFd(ion_fd, (uintptr_t)handle_ion);
        if (nBufFd < 0)
        {
            loge("CdcIonGetFd get fd failed\n");
            result = -1;
            goto ERROR;
        }

        //get iommu addr
        if(memory_type == MEMORY_IOMMU)
        {
            iommu_buffer.fd = nBufFd;
            VencGetVeIommuAddr(pVideoEnc, &iommu_buffer);
            inputBuffer.pAddrPhyY = (unsigned char*)(uintptr_t)iommu_buffer.iommu_addr;
            inputBuffer.pAddrPhyC = inputBuffer.pAddrPhyY +
                pJpegInfo->sBaseInfo.nStride * ((pJpegInfo->sBaseInfo.nInputHeight + 15) & (~15));
        }
        else
        {
            inputBuffer.pAddrPhyY = (unsigned char *) CdcIonGetPhyAdr(ion_fd,
                                                            (uintptr_t)handle_ion);
            inputBuffer.pAddrPhyC = inputBuffer.pAddrPhyY +
                pJpegInfo->sBaseInfo.nStride * pJpegInfo->sBaseInfo.nInputHeight;
        }
    }

    VencStart(pVideoEnc);

    inputBuffer.bEnableCorp         = pJpegInfo->bEnableCorp;
    inputBuffer.sCropInfo.nLeft     =  pJpegInfo->sCropInfo.nLeft;
    inputBuffer.sCropInfo.nTop        =  pJpegInfo->sCropInfo.nTop;
    inputBuffer.sCropInfo.nWidth    =  pJpegInfo->sCropInfo.nWidth;
    inputBuffer.sCropInfo.nHeight   =  pJpegInfo->sCropInfo.nHeight;

    if(pJpegInfo->bNoUseAddrPhy)
    {
        memcpy(inputBuffer.pAddrVirY, pJpegInfo->pAddrPhyY, bufferParam.nSizeY);
        memcpy(inputBuffer.pAddrVirC, pJpegInfo->pAddrPhyC, bufferParam.nSizeC);
        inputBuffer.bNeedFlushCache = 1;
    }

    VencQueueInputBuf(pVideoEnc, &inputBuffer);

    result = -1;
    int timeoutNum = 400;
    while(result != 0)
    {
        result = VencDequeueOutputBuf(pVideoEnc, &outputBuffer);
        usleep(5*1000);
        timeoutNum--;
        if(timeoutNum <= 0)
        {
            loge("dequeue out buffer timeout");
            break;
        }
    }

    if(result == 0)
    {
        memcpy(pOutBuffer, outputBuffer.pData0, outputBuffer.nSize0);
        if(outputBuffer.nSize1)
        {
            memcpy(((unsigned char*)pOutBuffer + outputBuffer.nSize0), \
                outputBuffer.pData1, outputBuffer.nSize1);
            *pOutBufferSize = outputBuffer.nSize0 + outputBuffer.nSize1;
        }
        else
        {
            *pOutBufferSize = outputBuffer.nSize0;
        }

        VencQueueOutputBuf(pVideoEnc, &outputBuffer);
    }

    //deattach buf fd from ve
    if(memory_type == MEMORY_IOMMU)
        VencFreeVeIommuAddr(pVideoEnc, &iommu_buffer);

    //close buf fd
    if(nBufFd != -1)
    {
        result = CdcIonClose(nBufFd);
        if(result < 0)
        {
            loge("CdcIonClose close buf fd error\n");
            result = -1;
            goto ERROR;
        }
    }

    //free ion handle
    if(handle_ion)
    {
        result = CdcIonFree(ion_fd, handle_ion);
        if(result < 0)
        {
            loge("CdcIonFree free ion_handle error\n");
            goto ERROR;
        }
    }

    if (ion_fd != -1)
    {
        result = CdcIonClose(ion_fd);
        if(result < 0)
        {
            loge("CdcIonClose ion fd error\n");
            goto ERROR;
        }
        ion_fd = -1;
    }

ERROR:

    if(pVideoEnc)
    {
        VencPause(pVideoEnc);
        VencDestroy(pVideoEnc);
    }

    return result;
}

VideoEncoderEncpp* VencEncppCreate()
{
    int loglevel;

    const char* loglevelFilePath = CdcGetConfigParamterString("cdc_log_level_file_path", NULL);
    loglevel = CdcGetSpecifyFilePathLogLevel(loglevelFilePath);
    if(loglevel == 0)
    {
        loglevel = CdcGetConfigParamterInt("cdc_log_level", 3);
    }
    if(loglevel < 2 || 6 < loglevel)
    {
        loglevel = 3;
    }

    logd("now cedarc log level:%d", loglevel);
    cdc_log_set_level(loglevel);

    LogVersionInfo();

    return (VideoEncoderEncpp*)EncppCreate();
}

void VencEncppDestroy(VideoEncoderEncpp* pEncpp)
{
    EncppDestroy((void*)pEncpp);
}

int VencEncppFunction(VideoEncoderEncpp* pEncpp,
                              VencEncppBufferInfo* pInBuffer,
                              VencEncppBufferInfo* pOutBuffer,
                              VencEncppFuncParam* pEncppFunction)
{

    int ret = EncppFunction((void*)pEncpp, pInBuffer, pOutBuffer, pEncppFunction);

    return ret;
}

static int doReset(VencContext *pVencCxt)
{
    int ret = 0;
    //reset input buffer
    ret = VencFbmReset(pVencCxt->pFBM);
    if(ret)
        return -1;

    //reset output buffer
    ret = pVencCxt->pVEncDevice->ResetBitStreamFrame(pVencCxt->pEncoderHandle);
    if(ret)
        return -1;

    return ret;
}

/*
online-2-buf & all offline-channel:
1. check in-frame of all channel, if had, continue encode;
2. no need check out-frame;
*/
static void checkIsReady_online2Buf()
{
    int i = 0;
    VencContext *pCurVencCxt = NULL;
    unsigned int bOnlineChannelInFrameIsReady = 0;
    unsigned int bNeedContinueDoEncode = 0;

    //*check whether need wait for in-frame
    pthread_mutex_lock(&pOnlineCxt->mInputFrameLock);
    for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
    {
        if(pOnlineCxt->pVencCxt[i])
        {
            pCurVencCxt = pOnlineCxt->pVencCxt[i];
            if(pCurVencCxt->baseConfig.bOnlineChannel == 0)
            {
                if(VencGetValidInputBufNum((VideoEncoder*)pCurVencCxt) > 0)
                    bNeedContinueDoEncode = 1;
            }
            else
            {
                VencGetParameter((VideoEncoder* )pCurVencCxt, VENC_IndexParamCheckOnlineStatus, &bOnlineChannelInFrameIsReady);
                if(bOnlineChannelInFrameIsReady == 1)
                    bNeedContinueDoEncode = 1;
            }
        }
    }
    if(bNeedContinueDoEncode == 0)
    {
        logv("** need wait for inputframe");
        pOnlineCxt->bNoInputFrameFlag = 1;
    }
    pthread_mutex_unlock(&pOnlineCxt->mInputFrameLock);

    if(bNeedContinueDoEncode == 0)
        CdcMessageQueueWaitMessage(pOnlineCxt->mq, 200);

    return ;
}

/*
online-1-buf:
1. check in-frame of all offline-channel, if had, continue encode; -- (bOfflineChannelHadInFrame = 1)
2. check out-frame of online-channle, if had, continue encode; -- (bOnlineChannelHadOutFrame = 1)
3. if(bOfflineChannelHadInFrame == 0 && bOnlineChannelHadOutFrame == 0), wait for it
*/
static void checkIsReady_online1Buf()
{
    int i = 0;
    VencContext *pCurVencCxt = NULL;
    //*check whether need wait for in-frame
    unsigned int bOfflineChannelHadInFrame = 0;
    unsigned int bOnlineChannelHadOutFrame = 0;
    unsigned int bBSBufIsFull = 0;
    unsigned int bNeedWaitForMsg = 0;

    pOnlineCxt->bNoInputFrameFlag = 0;
    pOnlineCxt->bNoOutputStreamBufFlag = 0;

    pthread_mutex_lock(&pOnlineCxt->mInputFrameLock);
    pthread_mutex_lock(&pOnlineCxt->mOutputStreamLock);
    for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
    {
        if(pOnlineCxt->pVencCxt[i])
        {
            pCurVencCxt = pOnlineCxt->pVencCxt[i];
            //* check in-frame of offline channel
            //* check out-frame of online channel
            if(pCurVencCxt->baseConfig.bOnlineChannel == 0)
            {
                if(VencGetValidInputBufNum((VideoEncoder*)pCurVencCxt) > 0)
                    bOfflineChannelHadInFrame = 1;
            }
            else
            {
                VencGetParameter((VideoEncoder* )pCurVencCxt, VENC_IndexParamBSbufIsFull, &bBSBufIsFull);
                if(bBSBufIsFull == 0)
                    bOnlineChannelHadOutFrame = 1;
            }
        }
    }
    if(bOfflineChannelHadInFrame == 0 && bOnlineChannelHadOutFrame == 0)
    {
        logv("** need wait for inputframe");
        pOnlineCxt->bNoInputFrameFlag = 1;
        pOnlineCxt->bNoOutputStreamBufFlag = 1;
        bNeedWaitForMsg = 1;
    }
    pthread_mutex_unlock(&pOnlineCxt->mOutputStreamLock);
    pthread_mutex_unlock(&pOnlineCxt->mInputFrameLock);

    if(bNeedWaitForMsg == 1)
        CdcMessageQueueWaitMessage(pOnlineCxt->mq, 200);

    return ;
}


void* OnlineThreadProcess(void* param)
{
    int i = 0;
    int ret = 0;
    VencContext *pCurVencCxt = NULL;
    VencInputBuffer mInputBuffer;
    memset(&mInputBuffer, 0, sizeof(VencInputBuffer));
    CdcMessage msg;
    memset(&msg, 0, sizeof(CdcMessage));
    sem_t* pReplySem = NULL;
    int bHadNoOfflineChannel = 0;
    int bHadOnlineChannel = 0;
    int bNeedProcessMsg = 0;
    int nRealValidChannelNum = 0;
    int bNeedWaitForInputFrame = 0;
    int bThreadExcutingState = 0;
    int bBSBufIsFull = 0;
    int bNeedWaitForOutputBuf = 0;

    int bOfflineChannelHadNoInFrame = 0;
    int tmp_cnt = 0;
    int maxLoopEncodeCnt = 10;

	logd("online thread start");
    while(1)
    {
        //* process message
        tmp_cnt++;
        if(CdcMessageQueueTryGetMessage(pOnlineCxt->mq, &msg, 0) == 0)
        {
            pCurVencCxt = (VencContext *)msg.params[1];

            if(msg.messageId == VENCODER_CMD_SET_STATE_INIT)
            {
                pCurVencCxt->eState = VENCODER_STATE_INITED;
            }
            else if(msg.messageId == VENCODER_CMD_SET_STATE_EXECUT)
            {
                pCurVencCxt->eState = VENCODER_STATE_EXCUTING;
                if(pCurVencCxt->baseConfig.bOnlineChannel == 1)
                    bHadOnlineChannel = 1;
            }
            else if(msg.messageId == VENCODER_CMD_SET_STATE_PAUSE)
            {
                pCurVencCxt->eState = VENCODER_STATE_PAUSE;
            }
            else if(msg.messageId == VENCODER_CMD_RESET)
            {
                doReset(pCurVencCxt);
            }
            else if(msg.messageId == VENCODER_CMD_EXIT)
            {
                for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
                {
                    if(pOnlineCxt->pVencCxt[i] == pCurVencCxt)
                    {
                        pOnlineCxt->pVencCxt[i] = NULL;
                        pOnlineCxt->nValidChannelNum--;
                        break;
                    }
                }
            }
            else if(msg.messageId == VENCODER_CMD_InputFrameAvailable)
            {
                pOnlineCxt->bNoInputFrameFlag = 0;
            }
            else if(msg.messageId == VENCODER_CMD_OutputStreamAvailable)
            {
                pOnlineCxt->bNoOutputStreamBufFlag = 0;
            }
            pReplySem = (sem_t*)msg.params[0];
            logv("pReplySem = %p, pCurVencCxt = %p, cmd = %d, cnt = %d", pReplySem, pCurVencCxt, msg.messageId, tmp_cnt);
            if(pReplySem)
                sem_post(pReplySem);
        }

        nRealValidChannelNum = 0;
        bThreadExcutingState = 0;
        for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
        {
            if(pOnlineCxt->pVencCxt[i])
                nRealValidChannelNum++;

            if(pOnlineCxt->pVencCxt[i] && pOnlineCxt->pVencCxt[i]->eState == VENCODER_STATE_EXCUTING)
                bThreadExcutingState = 1;
        }
        logv("nRealValidChannelNum = %d", nRealValidChannelNum);

        if(nRealValidChannelNum <= 0)
        {
            logd("exit the loop process, nRealValidChannelNum = %d",nRealValidChannelNum);
            break;
        }

        if(bThreadExcutingState == 1)
        {
            //* do encode frame of offline-channel first;
            for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
            {
                if(pOnlineCxt->nOnlineChannelIndex == i)
                    continue;

                if(pOnlineCxt->pVencCxt[i])
                {
                    pCurVencCxt = pOnlineCxt->pVencCxt[i];
                    logv("pCurVencCxt = %p, i  = %d", pCurVencCxt, i);
                    if(pCurVencCxt->eState == VENCODER_STATE_EXCUTING)
                    {
                        //* encode all in-frame of every offline channel
                        ret = VENC_RESULT_OK;
                        maxLoopEncodeCnt = 10;
                        while(ret == VENC_RESULT_OK)
                        {
                            int64_t start_t = getCurrentTime();
                            ret = encodeOneFrame((VideoEncoder*)pCurVencCxt);
                            int64_t end_t = getCurrentTime();
                            if (ret == VENC_RESULT_OK)
                                logv("offline[%d] encoder, ret = %d, time = %lld ms, cnt = %d",i, ret, (end_t - start_t)/1000, tmp_cnt);
                            maxLoopEncodeCnt--;
                            if(maxLoopEncodeCnt <= 0)
                            {
                                logw("loop do encode cnt exceed maxCnt");
                                break;
                            }
                        }
                    }
                }
            }

            //* do encode frame of online-channel;
            if(pOnlineCxt->nOnlineChannelIndex >= 0)
            {
                pCurVencCxt = pOnlineCxt->pVencCxt[pOnlineCxt->nOnlineChannelIndex];
                if(pCurVencCxt)
                {
                    if(pCurVencCxt->eState == VENCODER_STATE_EXCUTING)
                    {
                        int64_t start_t = getCurrentTime();
                        ret = encodeOneFrame((VideoEncoder*)pCurVencCxt);
                        int64_t end_t = getCurrentTime();
                        if (ret == 0)
                        {
                            logv("online[%d] encoder, ret = %d, time = %lld ms, cnt = %d, ",
                                pOnlineCxt->nOnlineChannelIndex, ret, (end_t - start_t)/1000, tmp_cnt);
                        }
                    }
                }
            }

            logv("pOnlineCxt->eOnlineBufNum = %d", pOnlineCxt->eOnlineBufNum);
            if(pOnlineCxt->eOnlineBufNum == Had_Online_Channel_One_Buf)
                checkIsReady_online1Buf();
            else if(pOnlineCxt->eOnlineBufNum == Had_Online_Channel_Two_Buf || pOnlineCxt->eOnlineBufNum == Had_No_Online_Channel)
                checkIsReady_online2Buf();
            else
                loge("eOnlineBufNum[%d] is error", pOnlineCxt->eOnlineBufNum);

        }
        else
        {
            CdcMessageQueueWaitMessage(pOnlineCxt->mq, 200);
        }
    }

    logd("exit the thread");
    return 0;
}

void* OfflineThreadProcess(void* param)
{
    int ret = 0;
    VencContext *pVencCxt = (VencContext *)param;
    sem_t* pReplySem = NULL;
    VencInputBuffer mInputBuffer;
    memset(&mInputBuffer, 0, sizeof(VencInputBuffer));
    CdcMessage msg;
    memset(&msg, 0, sizeof(CdcMessage));
    int bExitFlag = 0;
    int bNeedWaitMsg = 0;

	logd("offline thread start");
    while(1)
    {
        //* process message
        if(CdcMessageQueueTryGetMessage(pVencCxt->mq, &msg, 0) == 0)
        {
            if(msg.messageId == VENCODER_CMD_SET_STATE_INIT)
                pVencCxt->eState = VENCODER_STATE_INITED;
            else if(msg.messageId == VENCODER_CMD_SET_STATE_EXECUT)
                pVencCxt->eState = VENCODER_STATE_EXCUTING;
            else if(msg.messageId == VENCODER_CMD_SET_STATE_PAUSE)
                pVencCxt->eState = VENCODER_STATE_PAUSE;
            else if(msg.messageId == VENCODER_CMD_RESET)
            {
                doReset(pVencCxt);
            }
            else if(msg.messageId == VENCODER_CMD_EXIT)
            {
                bExitFlag = 1;
            }
            else if(msg.messageId == VENCODER_CMD_InputFrameAvailable)
            {
                pVencCxt->bNoInputFrameFlag = 0;
            }
            else if(msg.messageId == VENCODER_CMD_OutputStreamAvailable)
            {
                pVencCxt->bNoOutputStreamBufFlag = 0;
            }

            pReplySem = (sem_t*)msg.params[0];
            logv("pReplySem = %p, cmd = %d",pReplySem, msg.messageId);
            if(pReplySem)
                sem_post(pReplySem);
        }

        if(bExitFlag == 1)
        {
            logd("exit the loop process");
            break;
        }

        //* do encode frame
        if(pVencCxt->eState == VENCODER_STATE_EXCUTING)
        {
            ret = encodeOneFrame((VideoEncoder*)pVencCxt);
            if(ret == VENC_RESULT_NO_FRAME_BUFFER)
            {
                bNeedWaitMsg = 0;
                pthread_mutex_lock(&pVencCxt->mInputFrameLock);
                if(VencFbmGetValidBufferNum(pVencCxt->pFBM) <= 0)
                {
                    pVencCxt->bNoInputFrameFlag = 1;
                    bNeedWaitMsg = 1;
                }
                pthread_mutex_unlock(&pVencCxt->mInputFrameLock);

                if(bNeedWaitMsg == 1)
                    CdcMessageQueueWaitMessage(pVencCxt->mq, 200);

                continue;
            }
            else if(ret == VENC_RESULT_BITSTREAM_IS_FULL)
            {
                int bBSBufIsFull = 0;
                bNeedWaitMsg = 0;
                pthread_mutex_lock(&pVencCxt->mOutputStreamLock);
                VencGetParameter((VideoEncoder* )pVencCxt, VENC_IndexParamBSbufIsFull, &bBSBufIsFull);
                if(bBSBufIsFull == 1)
                {
                    pVencCxt->bNoOutputStreamBufFlag = 1;
                    bNeedWaitMsg = 1;
                }
                pthread_mutex_unlock(&pVencCxt->mOutputStreamLock);

                if(bNeedWaitMsg == 1)
                    CdcMessageQueueWaitMessage(pVencCxt->mq, 200);

                continue;
            }

            //if (ret == 0)
            //    logv("encoder, ret = %d, time = %lld ms",ret, (end_t - start_t)/1000);
        }
        else
        {
            CdcMessageQueueWaitMessage(pVencCxt->mq, 200);
        }
    }

    logd("exit the thread");
    return 0;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

