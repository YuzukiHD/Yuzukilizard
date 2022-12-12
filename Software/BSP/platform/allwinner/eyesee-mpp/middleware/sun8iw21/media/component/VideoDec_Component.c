/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//#define LOG_NDEBUG 0
#define LOG_TAG "VideoDec_Component"
#include <utils/plat_log.h>

#include <errno.h>
//#include <ion/ion.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>

#include <mm_component.h>
#include <tmessage.h>
#include <tsemaphore.h>

#include <CDX_MediaFormat.h>
#include <cedarx_demux.h>
#include <memoryAdapter.h>
#include <vdecoder.h>
#include <cdc_config.h>
#include <iniparserapi.h>
//#include <CDX_FormatConvert.h>
#include <EncodedStream.h>
#include <DemuxCompStream.h>
#include <SystemBase.h>
#include <VdecStream.h>
#include "VideoDec_Component.h"
#include "VideoDec_DataType.h"
#include "VideoDec_InputThread.h"
#include <sys_linux_ioctl.h>
#include <mpi_videoformat_conversion.h>
#include <EPIXELFORMAT_g2d_format_convert.h>
#include <media_common_vcodec.h>
#include <ion_memmanager.h>
#include <linux/g2d_driver.h>
//#include <ConfigOption.h>
#include <cdx_list.h>

#define VIDEO_DEC_TIME_DEBUG 0

//#define CDX_COMP_PRIV_FLAGS_ABORT_WAIT_VBV_SEM (1<<16)
#define MAX_YUV_PLANNER_MODE_OUTPUT_WIDTH 1288  //to make 1280x720 pixel by pixel
#define MAX_YUV_PLANNER_MODE_OUTPUT_HEIGHT 728

//#define USE_HARDWARE_FORMAT_CONVERT
#define VDEC_FRAME_COUNT (32)
#define VDEC_COMP_FRAME_COUNT (4)

//#define __WRITEOUT_FRAMEBUFFER

//------------------------------------------------------------------------------------
//extern void*        cedar_sys_phymalloc_map(unsigned int size, int align);
//extern void         cedar_sys_phyfree_map(void *buf);
//extern unsigned int cedarv_address_phy2vir(void *addr);
//extern unsigned int cedarv_address_vir2phy(void *addr);
//extern void         cedarv_set_ve_freq(int freq); //MHz
//extern void cedarx_cache_op(long start, long end, int flag);

static void *ComponentThread(void *pThreadData);
static void *VideoDecCompFrameBufferThread(void *pThreadData);
//ERRORTYPE VideoDecRequstBuffer(COMP_HANDLETYPE hComponent, unsigned int nPortIndex, COMP_BUFFERHEADERTYPE* pBuffer);
//ERRORTYPE VideoDecReleaseBuffer(COMP_HANDLETYPE hComponent, unsigned int nPortIndex, COMP_BUFFERHEADERTYPE* pBuffer);

typedef struct {
    void *mpFrameBuf;    //memory address, virtual address.
    int mIonUserHandle;  //ion_user_handle_t
    //BOOL mbIonUserHandleValid;  //1:ion_user_handle_t is valid, 0:ion_user_handle_t is free.
    struct list_head mList;
} VDANWBuffer;

typedef struct {
    VideoStreamInfo mStreamInfo;
    struct list_head mList;
} VDStreamInfo;

//static void cb_release_frame_buffer_sem(void *cookie)
//{
//  VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)cookie;
//  cdx_sem_up(&pVideoDecData->sem_frame_buffer);
//    return;
//}
//
//static void cb_free_vbs_buffer_sem(void *cookie)
//{
//  VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)cookie;
//  cdx_sem_signal(&pVideoDecData->sem_vdec_2_dmx); TODO: del it??
//    return;
//}

static enum EPIXELFORMAT convertPixelFormatCdx2Vdec(int format)
{
    if (format == CDX_PIXEL_FORMAT_AW_MB422) {
        return PIXEL_FORMAT_YUV_MB32_422;
    } else if (format == CDX_PIXEL_FORMAT_AW_MB420) {
        return PIXEL_FORMAT_YUV_MB32_420;
    } else if (format == CDX_PIXEL_FORMAT_YV12) {
        return PIXEL_FORMAT_YV12;
    } else if (format == CDX_PIXEL_FORMAT_YCrCb_420_SP) {
        return PIXEL_FORMAT_NV21;
    } else if (format == CDX_PIXEL_FORMAT_AW_NV12) {
        return PIXEL_FORMAT_NV12;
    } else {
        alogw("fatal error! format=0x%x", format);
        return PIXEL_FORMAT_YUV_MB32_420;
    }
}

ROTATE_E map_cedarv_rotation_to_ROTATE_E(int cedarv_rotation)
{
    ROTATE_E nRotate;
    //clock wise. 0: no rotate, 1: 90 degree (clock wise), 2: 180, 3: 270, 4: horizon flip, 5: vertical flip; implicitly means picture rotate anti-clockwise.
    switch (cedarv_rotation) {
        case 0:
            nRotate = ROTATE_NONE;
            break;
        case 1:
            nRotate = ROTATE_90;
            break;
        case 2:
            nRotate = ROTATE_180;
            break;
        case 3:
            nRotate = ROTATE_270;
            break;
        default:
            aloge("fatal error! not support other rotation[%d]", cedarv_rotation);
            nRotate = ROTATE_NONE;
            break;
    }
    return nRotate;
}

int map_ROTATE_E_to_cedarv_rotation(ROTATE_E eRotate)
{
    //clock wise. 0: no rotate, 1: 90 degree (clock wise), 2: 180, 3: 270, 4: horizon flip, 5: vertical flip; implicitly means picture rotate anti-clockwise.
    int cedarvRotation;
    switch (eRotate) {
        case ROTATE_NONE:
            cedarvRotation = 0;
            break;
        case ROTATE_90:
            cedarvRotation = 1;
            break;
        case ROTATE_180:
            cedarvRotation = 2;
            break;
        case ROTATE_270:
            cedarvRotation = 3;
            break;
        default:
            aloge("fatal error! unknown rotate[%d]", eRotate);
            cedarvRotation = 0;
            break;
    }
    return cedarvRotation;
}

ERRORTYPE config_VIDEO_FRAME_INFO_S_by_VideoPicture(VIDEO_FRAME_INFO_S *pFrameInfo, VideoPicture *pVideoPicture,
                                                    VIDEODECDATATYPE *pVideoDecData)
{
    memset(pFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));
    pFrameInfo->mId = pVideoPicture->nID;
    pFrameInfo->VFrame.mWidth = pVideoPicture->nWidth;
    pFrameInfo->VFrame.mHeight = pVideoPicture->nHeight;
    if (pVideoPicture->bIsProgressive) {
        pFrameInfo->VFrame.mField = VIDEO_FIELD_FRAME;
    } else {
        pFrameInfo->VFrame.mField = VIDEO_FIELD_INTERLACED;
    }
    pFrameInfo->VFrame.mPixelFormat = map_EPIXELFORMAT_to_PIXEL_FORMAT_E(pVideoPicture->ePixelFormat);
    pFrameInfo->VFrame.mVideoFormat = VIDEO_FORMAT_LINEAR;
    pFrameInfo->VFrame.mCompressMode = COMPRESS_MODE_NONE;
//    pFrameInfo->VFrame.mPhyAddr[0] = (unsigned int)CdcMemGetPhysicAddressCpu(pVideoDecData->mMemOps, pVideoPicture->pData0);
//    pFrameInfo->VFrame.mPhyAddr[1] = (unsigned int)CdcMemGetPhysicAddressCpu(pVideoDecData->mMemOps, pVideoPicture->pData1);
//    pFrameInfo->VFrame.mPhyAddr[2] = (unsigned int)CdcMemGetPhysicAddressCpu(pVideoDecData->mMemOps, pVideoPicture->pData2);
    pFrameInfo->VFrame.mPhyAddr[0] = (unsigned int)pVideoPicture->phyYBufAddr;
    pFrameInfo->VFrame.mPhyAddr[1] = (unsigned int)pVideoPicture->phyCBufAddr;
    pFrameInfo->VFrame.mPhyAddr[2] = pVideoPicture->pData2!=NULL?(unsigned int)pVideoPicture->phyCBufAddr+pVideoPicture->nWidth*pVideoPicture->nHeight/4:0;
    pFrameInfo->VFrame.mpVirAddr[0] = pVideoPicture->pData0;
    pFrameInfo->VFrame.mpVirAddr[1] = pVideoPicture->pData1;
    pFrameInfo->VFrame.mpVirAddr[2] = pVideoPicture->pData2;
    pFrameInfo->VFrame.mStride[0] = pVideoPicture->nLineStride;
    pFrameInfo->VFrame.mStride[1] = pVideoPicture->nLineStride;
    pFrameInfo->VFrame.mStride[2] = pVideoPicture->nLineStride;
    pFrameInfo->VFrame.mOffsetTop = pVideoPicture->nTopOffset;
    pFrameInfo->VFrame.mOffsetBottom = pVideoPicture->nBottomOffset;
    pFrameInfo->VFrame.mOffsetLeft = pVideoPicture->nLeftOffset;
    pFrameInfo->VFrame.mOffsetRight = pVideoPicture->nRightOffset;
    pFrameInfo->VFrame.mpts = pVideoPicture->nPts;
    return SUCCESS;
}

/**
 * nRotation: vdeclib should static rotate, clock wise. 0: no rotate, 1: 90 degree (clock wise), 2: 180, 3: 270, 
 *              4: horizon flip, 5: vertical flip; implicitly means picture rotate anti-clockwise.
 * we will malloc one buffer contain y,u,v data.
 */
static ERRORTYPE CompFrameInit(VDecCompOutputFrame *pCompFrame, VideoPicture *pPicture, int nRotation)
{
    static int sgCompFrameIdCounter = 0;
    ERRORTYPE ret = SUCCESS;
    if(nRotation < 0 || nRotation > 3)
    {
        aloge("fatal error! wrong rotation[%d].", nRotation);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    if(0 == nRotation)
    {
        alogw("fatal error! rotation is 0");
    }
    if(pCompFrame->mpPicture)
    {
        aloge("fatal error! mpPicture[%p] is not NULL.", pCompFrame->mpPicture);
        return SUCCESS;
    }
    pCompFrame->mpPicture = (VideoPicture*)calloc(sizeof(VideoPicture), 1);
    if(NULL == pCompFrame->mpPicture)
    {
        aloge("fatal error! malloc fail!");
        return ERR_VDEC_NOMEM;
    }
    *pCompFrame->mpPicture = *pPicture;
    pCompFrame->mpPicture->nID = sgCompFrameIdCounter++;
    if(1 == nRotation)
    {
        pCompFrame->mpPicture->nWidth = pPicture->nHeight;
        pCompFrame->mpPicture->nHeight = pPicture->nWidth;
        pCompFrame->mpPicture->nLineStride = pPicture->nHeight;
        pCompFrame->mpPicture->nTopOffset = pPicture->nLeftOffset;
        pCompFrame->mpPicture->nLeftOffset = pPicture->nHeight - pPicture->nBottomOffset;
        pCompFrame->mpPicture->nBottomOffset = pPicture->nRightOffset;
        pCompFrame->mpPicture->nRightOffset = pPicture->nHeight - pPicture->nTopOffset;
    }
    else if(2 == nRotation)
    {
        pCompFrame->mpPicture->nWidth = pPicture->nWidth;
        pCompFrame->mpPicture->nHeight = pPicture->nHeight;
        pCompFrame->mpPicture->nLineStride = pPicture->nWidth;
        pCompFrame->mpPicture->nTopOffset = pPicture->nHeight - pPicture->nBottomOffset;
        pCompFrame->mpPicture->nLeftOffset = pPicture->nWidth - pPicture->nRightOffset;
        pCompFrame->mpPicture->nBottomOffset = pPicture->nHeight - pPicture->nTopOffset;
        pCompFrame->mpPicture->nRightOffset = pPicture->nWidth - pPicture->nLeftOffset;
    }
    else if(3 == nRotation)
    {
        pCompFrame->mpPicture->nWidth = pPicture->nHeight;
        pCompFrame->mpPicture->nHeight = pPicture->nWidth;
        pCompFrame->mpPicture->nLineStride = pPicture->nHeight;
        pCompFrame->mpPicture->nTopOffset = pPicture->nWidth - pPicture->nRightOffset;
        pCompFrame->mpPicture->nLeftOffset = pPicture->nTopOffset;
        pCompFrame->mpPicture->nBottomOffset = pPicture->nWidth - pPicture->nLeftOffset;
        pCompFrame->mpPicture->nRightOffset = pPicture->nBottomOffset;
    }
    else
    {
        aloge("fatal error! rotation[%d] is not support!", nRotation);
    }

    int nYSize = 0;
    int nUSize = 0;
    int nVSize = 0;
    if(PIXEL_FORMAT_YV12 == pCompFrame->mpPicture->ePixelFormat)
    {
        nYSize = pCompFrame->mpPicture->nWidth * pCompFrame->mpPicture->nHeight;
        nUSize = pCompFrame->mpPicture->nWidth * pCompFrame->mpPicture->nHeight/4;
        nVSize = nUSize;
        IonAllocAttr stAttr;
        memset(&stAttr, 0, sizeof(IonAllocAttr));
        stAttr.mLen = nYSize + nUSize + nVSize;
        stAttr.mAlign = 0;
        stAttr.mIonHeapType = IonHeapType_IOMMU;
        stAttr.mbSupportCache = 0;
        char *pVirBuf = (char*)ion_allocMem_extend(&stAttr);
        if(NULL == pVirBuf)
        {
            aloge("fatal error! ion malloc size[%d] fail", nYSize + nUSize + nVSize);
            ret = ERR_VDEC_NOMEM;
            goto _err0;
        }
        uintptr_t nPhyBuf = (uintptr_t)ion_getMemPhyAddr(pVirBuf);
        pCompFrame->mpPicture->pData0 = pVirBuf;
        pCompFrame->mpPicture->pData1 = pVirBuf + nYSize;
        pCompFrame->mpPicture->pData2 = pVirBuf + nYSize + nUSize;
        pCompFrame->mpPicture->pData3 = NULL;
        pCompFrame->mpPicture->phyYBufAddr = nPhyBuf;
        pCompFrame->mpPicture->phyCBufAddr = nPhyBuf + nYSize;
    }
    else if(PIXEL_FORMAT_NV21 == pCompFrame->mpPicture->ePixelFormat 
        || PIXEL_FORMAT_NV12 == pCompFrame->mpPicture->ePixelFormat)
    {
        nYSize = pCompFrame->mpPicture->nWidth * pCompFrame->mpPicture->nHeight;
        nUSize = pCompFrame->mpPicture->nWidth * pCompFrame->mpPicture->nHeight/2;
        nVSize = 0;
        IonAllocAttr stAttr;
        stAttr.mLen = nYSize + nUSize + nVSize;
        stAttr.mAlign = 0;
        stAttr.mIonHeapType = IonHeapType_IOMMU;
        stAttr.mbSupportCache = 0;
        char *pVirBuf = (char*)ion_allocMem_extend(&stAttr);
        if(NULL == pVirBuf)
        {
            aloge("fatal error! ion malloc size[%d] fail", nYSize + nUSize + nVSize);
            ret = ERR_VDEC_NOMEM;
            goto _err0;
        }
        uintptr_t nPhyBuf = (uintptr_t)ion_getMemPhyAddr(pVirBuf);
        pCompFrame->mpPicture->pData0 = pVirBuf;
        pCompFrame->mpPicture->pData1 = pVirBuf + nYSize;
        pCompFrame->mpPicture->pData2 = NULL;
        pCompFrame->mpPicture->pData3 = NULL;
        pCompFrame->mpPicture->phyYBufAddr = nPhyBuf;
        pCompFrame->mpPicture->phyCBufAddr = nPhyBuf + nYSize;
    }
    else
    {
        aloge("fatal error! pixelFormat[0x%x] is not support!", pCompFrame->mpPicture->ePixelFormat);
        ret = ERR_VDEC_ILLEGAL_PARAM;
        goto _err0;
    }
    alogd("comp frame init, rotation[%d], size[%dx%d, %d,%d,%d,%d]", 
        nRotation, pCompFrame->mpPicture->nWidth, pCompFrame->mpPicture->nHeight, 
        pCompFrame->mpPicture->nTopOffset, pCompFrame->mpPicture->nLeftOffset, 
        pCompFrame->mpPicture->nBottomOffset, pCompFrame->mpPicture->nRightOffset);
    return ret;

_err0:
    free(pCompFrame->mpPicture);
    pCompFrame->mpPicture = NULL;
    return ret;
    
}

static ERRORTYPE CompFrameDestroy(VDecCompOutputFrame *pCompFrame)
{
    if(pCompFrame->mpPicture)
    {
        if(pCompFrame->mpPicture->pData0)
        {
            ion_freeMem(pCompFrame->mpPicture->pData0);
            pCompFrame->mpPicture->pData0 = NULL;
        }
        if(pCompFrame->mpPicture->pData1)
        {
            //ion_freeMem(pCompFrame->mpPicture->pData1);
            pCompFrame->mpPicture->pData1 = NULL;
        }
        if(pCompFrame->mpPicture->pData2)
        {
            //ion_freeMem(pCompFrame->mpPicture->pData2);
            pCompFrame->mpPicture->pData2 = NULL;
        }
        if(pCompFrame->mpPicture->pData3)
        {
            //ion_freeMem(pCompFrame->mpPicture->pData3);
            pCompFrame->mpPicture->pData3 = NULL;
        }
        free(pCompFrame->mpPicture);
        pCompFrame->mpPicture = NULL;
    }
    return SUCCESS;
}

int decideMaxScaleRatio(int maxRatio, VideoStreamInfo *pStreamInfo)
{
#if (AWCHIP == AW_V5 || AWCHIP == AW_V316 || AWCHIP == AW_V459 || AWCHIP == AW_V853)
    switch(pStreamInfo->eCodecFormat)
    {
        case VIDEO_CODEC_FORMAT_H264:
        case VIDEO_CODEC_FORMAT_H265:
        {
            if (maxRatio > 2) 
            {
                alogd("limit ScaleDownRatio[%d]->[2] of CodecFormat[0x%x]", maxRatio, pStreamInfo->eCodecFormat);
                maxRatio = 2;
            }
            break;
        }
        case VIDEO_CODEC_FORMAT_MJPEG:
        {
            if (maxRatio > 3) 
            {
                alogd("limit ScaleDownRatio[%d]->[3] of CodecFormat[0x%x]", maxRatio, pStreamInfo->eCodecFormat);
                maxRatio = 3;
            }
            break;
        }
        default:
        {
            aloge("fatal error! unknown CodecFormat[0x%x]", pStreamInfo->eCodecFormat);
            if (maxRatio > 2) 
            {
                alogd("limit ScaleDownRatio[%d]->[2] of CodecFormat[0x%x]", maxRatio, pStreamInfo->eCodecFormat);
                maxRatio = 2;
            }
            break;
        }
    }
#else
    aloge("fatal error! unknown chip");
    if (maxRatio > 2) 
    {
        alogd("limit ScaleDownRatio[%d]->[2] of CodecFormat[0x%x]", maxRatio, pStreamInfo->eCodecFormat);
        maxRatio = 2;
    }
#endif
    return maxRatio;
}

ERRORTYPE VideoDecDecideCompFrameBufferMode(VIDEODECDATATYPE *pVideoDecData)
{
    VideoStreamInfo *pVideoStreamInfo = (VideoStreamInfo *)pVideoDecData->sInPortExtraDef[VDEC_PORT_SUFFIX_DEMUX].pVendorInfo;
    if(0 == pVideoDecData->cedarv_rotation)
    {
        pVideoDecData->mbUseCompFrame = FALSE;
    }
    else
    {
#if 0
        if(VIDEO_CODEC_FORMAT_H265 == pVideoStreamInfo->eCodecFormat 
            ||VIDEO_CODEC_FORMAT_MJPEG == pVideoStreamInfo->eCodecFormat
            ||VIDEO_CODEC_FORMAT_H264 == pVideoStreamInfo->eCodecFormat)
        {
            pVideoDecData->mbUseCompFrame = TRUE;
        }
        else
        {
            pVideoDecData->mbUseCompFrame = FALSE;
        }
#else
        pVideoDecData->mbUseCompFrame = TRUE;
#endif
    }
    if(pVideoDecData->mbUseCompFrame)
    {
        if(pVideoDecData->mG2DHandle < 0)
        {
            pVideoDecData->mG2DHandle = open("/dev/g2d", O_RDWR, 0);
            if (pVideoDecData->mG2DHandle < 0)
            {
                aloge("fatal error! open /dev/g2d failed");
            }
            alogv("open /dev/g2d OK");
        }
        else
        {
            aloge("fatal error! why g2dDriver[%d] is open?", pVideoDecData->mG2DHandle);
        }
        if(pVideoDecData->mG2DHandle >= 0)
        {
            if(0 == pVideoDecData->mCompFrameBufferThreadId)
            {
                pVideoDecData->mCompFBThreadState = COMP_StateLoaded;
                int err = pthread_create(&pVideoDecData->mCompFrameBufferThreadId, NULL, VideoDecCompFrameBufferThread, pVideoDecData);
                if (err || !pVideoDecData->mCompFrameBufferThreadId)
                {
                    aloge("fatal error! create thread fail![%d], threadId[%d]", err, (int)pVideoDecData->mCompFrameBufferThreadId);
                }
                message_t msg;
                msg.command = SetState;
                msg.para0 = COMP_StateIdle;
                put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
                pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                while(pVideoDecData->mCompFBThreadState != COMP_StateIdle)
                {
                    pthread_cond_wait(&pVideoDecData->mCondCompFBThreadState, &pVideoDecData->mCompFBThreadStateLock);
                }
                pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
            }
            else
            {
                aloge("fatal error! comp FBThreadId[%d] is exist!", (int)pVideoDecData->mCompFrameBufferThreadId);
            }
        }
        else
        {
            pVideoDecData->mbUseCompFrame = FALSE;
        }
    }
    return SUCCESS;
}

int CedarvCodecInit(VIDEODECDATATYPE *pVideoDecData)
{
    int ret;
    VideoDecoder *pCedarV;

    if (pVideoDecData->pCedarV) {
        alogd("VideoDecoder already exist!");
        return SUCCESS;
    }

    VideoStreamInfo *pVideoStreamInfo, tmpStreamInfo;
    if (pVideoDecData->mInputPortTunnelFlag)
    {
        pVideoStreamInfo = (VideoStreamInfo *)pVideoDecData->sInPortExtraDef[VDEC_PORT_SUFFIX_DEMUX].pVendorInfo;
        if (NULL == pVideoStreamInfo) {
            alogd("video stream info is not got, can't init vdeclib now.");
            return ERR_VDEC_ILLEGAL_PARAM;
        }
    }
    else
    {
        alogd("JPEG or H264 decoder init begin!");
        pVideoStreamInfo = &tmpStreamInfo;
        memset(pVideoStreamInfo, 0, sizeof(VideoStreamInfo));
        memcpy(pVideoStreamInfo, &pVideoDecData->stVideoStreamInfo, sizeof(VideoStreamInfo));
        pVideoStreamInfo->eCodecFormat = map_PAYLOAD_TYPE_E_to_EVIDEOCODECFORMAT(pVideoDecData->mChnAttr.mType);
        pVideoStreamInfo->bIsFramePackage = 1;
    }
    if(pVideoDecData->mbForceFramePackage)
    {
        if(0 == pVideoStreamInfo->bIsFramePackage)
        {
            alogd("parser set bIsFramePackage to 0, but vdec force to 1");
            pVideoStreamInfo->bIsFramePackage = 1;
        }
    }
    alogd("video_format: %d", pVideoStreamInfo->eCodecFormat);
    if (!(pVideoStreamInfo->eCodecFormat >= VIDEO_CODEC_FORMAT_MIN &&
          pVideoStreamInfo->eCodecFormat <= VIDEO_CODEC_FORMAT_MAX)) {
        aloge("!!!!can't find cedar codecs!, eCodecFormat(%d)\n", pVideoStreamInfo->eCodecFormat);
        return -1;
    }

    //pCedarV = libcedarv_init(&ret);
    pCedarV = CreateVideoDecoder();
    if (pCedarV == NULL) {
        aloge("libcedarv_init error!");
        return -1;
    }

    if (pVideoDecData->max_resolution) 
    {
        int nMaxCapabilityWidth = pVideoDecData->max_resolution >> 16;
        int nMaxCapabilityHeight = pVideoDecData->max_resolution & 0xffff;
        if(pVideoDecData->cedarv_max_width > nMaxCapabilityWidth)
        {
            alogd("Be careful! user setting decode width[%d] > Capability[%d]", pVideoDecData->cedarv_max_width, nMaxCapabilityWidth);
            pVideoDecData->cedarv_max_width = nMaxCapabilityWidth;
        }
        if(pVideoDecData->cedarv_max_height > nMaxCapabilityHeight)
        {
            alogd("Be careful! user setting decode height[%d] > Capability[%d]", pVideoDecData->cedarv_max_height, nMaxCapabilityHeight);
            pVideoDecData->cedarv_max_height = nMaxCapabilityHeight;
        }
    }
    memset(&pVideoDecData->mVConfig, 0, sizeof(VConfig));
    //config scale param
    if (pVideoDecData->cedarv_max_width != 0 && pVideoDecData->cedarv_max_height != 0 &&
        pVideoStreamInfo->nWidth != 0 && pVideoStreamInfo->nHeight != 0) 
    {
        int i;
        int nOutput, nActual;
        //may be need scale.
        if (pVideoStreamInfo->nWidth > pVideoDecData->cedarv_max_width) {
            pVideoDecData->mVConfig.bScaleDownEn = 1;
            nOutput = pVideoDecData->cedarv_max_width;
            nActual = pVideoStreamInfo->nWidth;
            for (i = 0; nOutput < nActual; i++) {
                nOutput *= 2;
            }
            pVideoDecData->mVConfig.nHorizonScaleDownRatio = i;
        }
        if (pVideoStreamInfo->nHeight > pVideoDecData->cedarv_max_height) {
            pVideoDecData->mVConfig.bScaleDownEn = 1;
            nOutput = pVideoDecData->cedarv_max_height;
            nActual = pVideoStreamInfo->nHeight;
            for (i = 0; nOutput < nActual; i++) {
                nOutput *= 2;
            }
            pVideoDecData->mVConfig.nVerticalScaleDownRatio = i;
        }
        int maxRatio = pVideoDecData->mVConfig.nHorizonScaleDownRatio > pVideoDecData->mVConfig.nVerticalScaleDownRatio
                         ? pVideoDecData->mVConfig.nHorizonScaleDownRatio
                         : pVideoDecData->mVConfig.nVerticalScaleDownRatio;
        maxRatio = decideMaxScaleRatio(maxRatio, pVideoStreamInfo);
        pVideoDecData->mVConfig.nHorizonScaleDownRatio = pVideoDecData->mVConfig.nVerticalScaleDownRatio = maxRatio;
        if (pVideoDecData->mVConfig.bScaleDownEn) 
        {
            alogd("ScaleDownRatio[%dx%d], max_output_resolution[%dx%d], stream_resolution[%dx%d]",
                  pVideoDecData->mVConfig.nHorizonScaleDownRatio, pVideoDecData->mVConfig.nVerticalScaleDownRatio,
                  pVideoDecData->cedarv_max_width, pVideoDecData->cedarv_max_height, pVideoStreamInfo->nWidth,
                  pVideoStreamInfo->nHeight);
            if (pVideoDecData->mVConfig.nHorizonScaleDownRatio > pVideoDecData->mVConfig.nVerticalScaleDownRatio) 
            {
                pVideoDecData->mVConfig.nVerticalScaleDownRatio = pVideoDecData->mVConfig.nHorizonScaleDownRatio;
            } 
            else 
            {
                pVideoDecData->mVConfig.nHorizonScaleDownRatio = pVideoDecData->mVConfig.nVerticalScaleDownRatio;
            }
            alogd("final, ScaleDownRatio[%dx%d]", pVideoDecData->mVConfig.nHorizonScaleDownRatio, pVideoDecData->mVConfig.nVerticalScaleDownRatio);
        }
    }

    if (0 == pVideoDecData->cedarv_rotation) 
    {
        pVideoDecData->mVConfig.bRotationEn = 0;
        pVideoDecData->mVConfig.nRotateDegree = 0;
    } 
    else 
    {
        if(VIDEO_CODEC_FORMAT_H265 == pVideoStreamInfo->eCodecFormat
            || VIDEO_CODEC_FORMAT_MJPEG == pVideoStreamInfo->eCodecFormat
            || VIDEO_CODEC_FORMAT_H264 == pVideoStreamInfo->eCodecFormat)
        {
            alogd("don't use vdeclib to rotate[%d] for vdecFormat[0x%x], we use g2d!", pVideoDecData->cedarv_rotation, pVideoStreamInfo->eCodecFormat);
            pVideoDecData->mVConfig.bRotationEn = 0;
            pVideoDecData->mVConfig.nRotateDegree = 0;
        }
        else
        {
            pVideoDecData->mVConfig.bRotationEn = 1;
            pVideoDecData->mVConfig.nRotateDegree = pVideoDecData->cedarv_rotation;
        }
    }
    pVideoDecData->mVConfig.bThumbnailMode = 0;
    if (pVideoDecData->cedarv_output_setting == PIXEL_FORMAT_DEFAULT) {
        pVideoDecData->mVConfig.eOutputPixelFormat = PIXEL_FORMAT_YV12;  //PIXEL_FORMAT_YUV_MB32_420;
        alogd("vdec output format default to PIXEL_FORMAT_YV12");
    } else {
        pVideoDecData->mVConfig.eOutputPixelFormat = pVideoDecData->cedarv_output_setting;
        alogd("vdec output format set to [0x%x]", pVideoDecData->mVConfig.eOutputPixelFormat);
    }
    pVideoDecData->mVConfig.bNoBFrames = 0;
    if (pVideoDecData->disable_3d) {
        pVideoDecData->mVConfig.bDisable3D = 1;
    } else {
        pVideoDecData->mVConfig.bDisable3D = 0;
    }

    pVideoDecData->mVConfig.bSupportMaf = 0;
    pVideoDecData->mVConfig.bDispErrorFrame = 0;
    if(pVideoDecData->config_vbv_size%1024 != 0)
    {
        int alignSize = ALIGN(pVideoDecData->config_vbv_size, 1024);
        aloge("fatal error! vbvSize[%d] must 1024 align! we will extend it to [%d]", pVideoDecData->config_vbv_size, alignSize);
        pVideoDecData->config_vbv_size = alignSize;
    }
    pVideoDecData->mVConfig.nVbvBufferSize = pVideoDecData->config_vbv_size;
    alogd("config vbvBufferSize[%d]KB", pVideoDecData->mVConfig.nVbvBufferSize / 1024);
    pVideoDecData->mVConfig.nAlignStride = 32;
    if (0 == pVideoDecData->nDisplayFrameRequestMode) {
        pVideoDecData->mVConfig.bGpuBufValid = 0;
    } else {
        pVideoDecData->mVConfig.bGpuBufValid = 1;
    }
    pVideoDecData->mVConfig.nDisplayHoldingFrameBufferNum = GetConfigParamterInt("pic_4list_num", NUM_OF_PICTURES_KEEP_IN_LIST) + pVideoDecData->mVdecExtraFrameNum;
    pVideoDecData->mVConfig.nDeInterlaceHoldingFrameBufferNum = GetConfigParamterInt("pic_4di_num", 0);  //NUM_OF_PICTURES_KEEPPED_BY_DEINTERLACE;
    pVideoDecData->mVConfig.nRotateHoldingFrameBufferNum = GetConfigParamterInt("pic_4rotate_num", 0);       //NUM_OF_PICTURES_KEEPPED_BY_ROTATE;
    pVideoDecData->mVConfig.nDecodeSmoothFrameBufferNum = GetConfigParamterInt("pic_4smooth_num", 0);        //NUM_OF_PICTURES_FOR_EXTRA_SMOOTH;
    pVideoDecData->mVConfig.nFrameBufferNum = pVideoDecData->mChnAttr.mnFrameBufferNum;
    pVideoDecData->mVConfig.memops = pVideoDecData->mMemOps;

    // for jpeg decode, reinit VConfig!
    if (pVideoDecData->mChnAttr.mType == PT_JPEG)
    {
        //memset(&pVideoDecData->mVConfig, 0, sizeof(VConfig));
        //if(pVideoDecData->config_vbv_size > 0)
        //{
        //    pVideoDecData->mVConfig.nVbvBufferSize = ALIGN(pVideoDecData->config_vbv_size, 1024);
        //}
        //else
        //{
        //    pVideoDecData->mVConfig.nVbvBufferSize = 4*1024*1024;
        //}
        pVideoDecData->mVConfig.bThumbnailMode = 1;
        //pVideoDecData->mVConfig.nAlignStride = 32;
        //pVideoDecData->mVConfig.eOutputPixelFormat = pVideoDecData->cedarv_output_setting;
    }

    //open proc to decoder,as defualt
    pVideoDecData->mVConfig.bSetProcInfoEnable = 1;
    if(0 == pVideoStreamInfo->nFrameRate)
    {
        pVideoDecData->mVConfig.nSetProcInfoFreq = 25;
    }
    else
    {
        pVideoDecData->mVConfig.nSetProcInfoFreq = pVideoStreamInfo->nFrameRate / 1000;
        if(0 == pVideoDecData->mVConfig.nSetProcInfoFreq)
        {
            pVideoDecData->mVConfig.nSetProcInfoFreq = 25;
        } 
    }
    pVideoDecData->mVConfig.nChannelNum = pVideoDecData->mMppChnInfo.mChnId;
    
    //init sub output pic output channel
    if(pVideoDecData->cedarv_second_output_en)
    {
        if(PT_MJPEG == pVideoDecData->mChnAttr.mType)
        {
            pVideoDecData->mVConfig.bSecOutputEn = 1;
            pVideoDecData->mVConfig.eSecOutputPixelFormat = pVideoDecData->cedarv_second_output_setting;
            pVideoDecData->mVConfig.nSecHorizonScaleDownRatio = pVideoDecData->cedarv_second_horizon_scale_down_ratio;
            pVideoDecData->mVConfig.nSecVerticalScaleDownRatio = pVideoDecData->cedarv_second_vertical_scale_down_ratio;
        }
        else
        {
            aloge("fatal error, the [%d] decode type do support second output!", pVideoDecData->mChnAttr.mType);
            pVideoDecData->mVConfig.bSecOutputEn = 0;
            pVideoDecData->cedarv_second_output_en = 0;
        }
    }

    //ret = pCedarV->open(pCedarV);
    ret = InitializeVideoDecoder(pCedarV, pVideoStreamInfo, &pVideoDecData->mVConfig);
    alogd("video stream info size[%dx%d]", pVideoStreamInfo->nWidth, pVideoStreamInfo->nHeight);
    if (ret < 0) {
        aloge("CedarV open error! ret=%d", ret);
        return -1;
    }
    pVideoDecData->mbConfigVdecFrameBuffers = FALSE;
    //pCedarV->ioctrl(pCedarV, CEDARV_COMMAND_PLAY, 0);
    pVideoDecData->pCedarV = pCedarV;
    if(pVideoDecData->mVEFreq)
    {
        alogd("vdec init VE freq to [%d]MHz", pVideoDecData->mVEFreq);
        VideoDecoderSetFreq(pVideoDecData->pCedarV, pVideoDecData->mVEFreq);
    }
    alogv("pVideoDecData->pCedarV:%p", pVideoDecData->pCedarV);

    return 0;
}

ERRORTYPE VideoDecTunnel_SendVDecCompOutputFrame(VIDEODECDATATYPE *pVideoDecData, VDecCompOutputFrame *pOutFrame)
{
    ERRORTYPE ret;
    if(FALSE == pVideoDecData->mbUseCompFrame)
    {
        ERRORTYPE omxRet;
        int releaseRet;
        COMP_BUFFERHEADERTYPE obh;
        obh.nOutputPortIndex = pVideoDecData->sOutPortTunnelInfo.nPortIndex;
        obh.nInputPortIndex = pVideoDecData->sOutPortTunnelInfo.nTunnelPortIndex;
        VIDEO_FRAME_INFO_S stFrameInfo;
        config_VIDEO_FRAME_INFO_S_by_VideoPicture(&stFrameInfo, pOutFrame->mpPicture, pVideoDecData);
        obh.pOutputPortPrivate = (void *)&stFrameInfo;
        omxRet = COMP_EmptyThisBuffer(pVideoDecData->sOutPortTunnelInfo.hTunnel, &obh);
        if (SUCCESS == omxRet) 
        {
            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
            list_add_tail(&pOutFrame->mList, &pVideoDecData->mUsedOutFrameList);
            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
            ret = SUCCESS;
        } 
        else 
        {
            int commonErrCode = omxRet&0x1FFF;
            if (EN_ERR_INCORRECT_STATE_OPERATION == commonErrCode) 
            {
                alogd("Be careful! VDec output frame fail[0x%x], maybe next component status is Loaded, return frame!", omxRet);
            }
            else if(EN_ERR_SYS_NOTREADY == commonErrCode)
            {
                alogv("frame is ignored.");
            }
            else
            {
                aloge("fatal error! errCode[0x%x]", omxRet);
            }
            releaseRet = ReturnPicture(pVideoDecData->pCedarV, (VideoPicture *)pOutFrame->mpPicture);
            if (releaseRet != 0) 
            {
                aloge("fatal error! Return Picture() fail ret[%d]", releaseRet);
            }
            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
            list_add(&pOutFrame->mList, &pVideoDecData->mIdleOutFrameList);
            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
            ret = ERR_VDEC_INCORRECT_STATE_OPERATION;
        }
        return ret;
    }
    else
    {
        pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
        list_add_tail(&pOutFrame->mList, &pVideoDecData->mUsedOutFrameList);
        if(pVideoDecData->mbCompFBThreadWaitVdecFrameInput)
        {
            pVideoDecData->mbCompFBThreadWaitVdecFrameInput = FALSE;
            message_t msg;
            msg.command = VDecComp_CompFrameBufferThread_InputFrameAvailable;
            put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
        }
        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
        return SUCCESS;
    }
}

ERRORTYPE VideoDecReturnVDecCompOutputFrameToIdleList(VIDEODECDATATYPE *pVideoDecData, int nFrameId)
{
    VDecCompOutputFrame *pOutFrame = NULL;
    pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
    VDecCompOutputFrame *pEntry;
    BOOL bFindFlag = FALSE;
    list_for_each_entry(pEntry, &pVideoDecData->mUsedOutFrameList, mList)
    {
        if (pEntry->mpPicture->nID == nFrameId) 
        {
            if(!bFindFlag)
            {
                bFindFlag = TRUE;
                pOutFrame = pEntry;
                //break;
            }
            else
            {
                aloge("fatal error! find frameId[0x%x] again!", pEntry->mpPicture->nID);
            }
        }
    }
    if(!bFindFlag)
    {
        aloge("fatal error! can't find frameId[%d], check code!", nFrameId);
        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    int ret = ReturnPicture(pVideoDecData->pCedarV, (VideoPicture *)pOutFrame->mpPicture);
    if (ret != 0) 
    {
        aloge("fatal error! Return Picture() fail ret[%d]", ret);
    }
    list_move_tail(&pOutFrame->mList, &pVideoDecData->mIdleOutFrameList);
    if (pVideoDecData->mWaitOutFrameFlag) 
    {
        pVideoDecData->mWaitOutFrameFlag = FALSE;
        message_t msg;
        msg.command = VDecComp_OutFrameAvailable;
        put_message(&pVideoDecData->cmd_queue, &msg);
    }
    if (pVideoDecData->mWaitOutFrameFullFlag) 
    {
        int cnt = 0;
        struct list_head *pList;
        list_for_each(pList, &pVideoDecData->mIdleOutFrameList) 
        { 
            cnt++; 
        }
        if (cnt >= pVideoDecData->mFrameNodeNum) 
        {
            pthread_cond_signal(&pVideoDecData->mOutFrameFullCondition);
        }
    }
    pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
    return SUCCESS;
}

ERRORTYPE VideoDecTunnel_ReturnVDecCompOutputFrame(VIDEODECDATATYPE *pVideoDecData, int nFrameId)
{
    if(FALSE == pVideoDecData->mbUseCompFrame)
    {
        return VideoDecReturnVDecCompOutputFrameToIdleList(pVideoDecData, nFrameId);
    }
    else
    {
        //return it to VideoDec CompFrameBufferThread
        VDecCompOutputFrame *pOutFrame = NULL;
        pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
        VDecCompOutputFrame *pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pVideoDecData->mCompUsedOutFrameList, mList)
        {
            if (pEntry->mpPicture->nID == nFrameId) 
            {
                if(!bFindFlag)
                {
                    bFindFlag = TRUE;
                    pOutFrame = pEntry;
                    //break;
                }
                else
                {
                    aloge("fatal error! find frameId[0x%x] again!", pEntry->mpPicture->nID);
                }
            }
        }
        if(!bFindFlag)
        {
            aloge("fatal error! can't find frameId[%d], check code!", nFrameId);
            pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
            return ERR_VDEC_ILLEGAL_PARAM;
        }
        list_move_tail(&pOutFrame->mList, &pVideoDecData->mCompIdleOutFrameList);
        if (pVideoDecData->mbCompFBThreadWaitOutFrame) 
        {
            pVideoDecData->mbCompFBThreadWaitOutFrame = FALSE;
            message_t msg;
            msg.command = VDecComp_CompFrameBufferThread_OutFrameAvailable;
            put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
        }
        if (pVideoDecData->mbCompFBThreadWaitOutFrameFull) 
        {
            int cnt = 0;
            struct list_head *pList;
            list_for_each(pList, &pVideoDecData->mCompIdleOutFrameList) 
            { 
                cnt++; 
            }
            if (cnt >= VDEC_COMP_FRAME_COUNT) 
            {
                pthread_cond_signal(&pVideoDecData->mCompFBThreadOutFrameFullCondition);
            }
        }
        pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
        return SUCCESS;
    }
    
}

ERRORTYPE VideoDecNonTunnel_ReadyVDecCompOutputFrame(VIDEODECDATATYPE *pVideoDecData, VDecCompOutputFrame *pOutFrame)
{
    ERRORTYPE ret;
    if(FALSE == pVideoDecData->mbUseCompFrame)
    {
        pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
        list_add_tail(&pOutFrame->mList, &pVideoDecData->mReadyOutFrameList);
        if (pVideoDecData->mWaitReadyFrameFlag)
        {
            pthread_cond_signal(&pVideoDecData->mReadyFrameCondition);
        }
        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
        return SUCCESS;
    }
    else
    {
        //send to VideoDec CompFrameBufferThread, rotate at there.
        pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
        list_add_tail(&pOutFrame->mList, &pVideoDecData->mUsedOutFrameList);
        if(pVideoDecData->mbCompFBThreadWaitVdecFrameInput)
        {
            pVideoDecData->mbCompFBThreadWaitVdecFrameInput = FALSE;
            message_t msg;
            msg.command = VDecComp_CompFrameBufferThread_InputFrameAvailable;
            put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
        }
        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
        return SUCCESS;
    }
}

ERRORTYPE VideoDecNonTunnel_GetVDecCompOutputFrame(
    PARAM_IN VIDEODECDATATYPE *pVideoDecData, 
    PARAM_OUT VIDEO_FRAME_INFO_S *pFrameInfo, 
    PARAM_OUT VIDEO_FRAME_INFO_S *pSubFrameInfo,
    PARAM_IN int nMilliSec)
{
    ERRORTYPE eError;
    if(FALSE == pVideoDecData->mbUseCompFrame)
    {
        pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
    _TryToGetOutFrame:
        if (!list_empty(&pVideoDecData->mReadyOutFrameList)) 
        {
            VDecCompOutputFrame *pOutFrame = list_first_entry(&pVideoDecData->mReadyOutFrameList, VDecCompOutputFrame, mList);
            config_VIDEO_FRAME_INFO_S_by_VideoPicture(pFrameInfo, pOutFrame->mpPicture, pVideoDecData);
            if(pVideoDecData->mVConfig.bSecOutputEn)
            {
                if(pSubFrameInfo && pOutFrame->mpSubPicture)
                {
                    if(pOutFrame->mpPicture->nID != pOutFrame->mpSubPicture->nID)
                    {
                        aloge("fatal error! why frame pair id is not same?[%d]!=[%d]", pOutFrame->mpPicture->nID, pOutFrame->mpSubPicture->nID);
                    }
                    config_VIDEO_FRAME_INFO_S_by_VideoPicture(pSubFrameInfo, pOutFrame->mpSubPicture, pVideoDecData);
                }
                else
                {
                    aloge("fatal error!,if you want get sub portout, why not get stream?");
                }

            }
            list_move_tail(&pOutFrame->mList, &pVideoDecData->mUsedOutFrameList);
            eError = SUCCESS;
        }
        else
        {
            if (0 == nMilliSec)
            {
                eError = ERR_VDEC_NOBUF;
            }
            else if (nMilliSec < 0) 
            {
                pVideoDecData->mWaitReadyFrameFlag = TRUE;
                while (list_empty(&pVideoDecData->mReadyOutFrameList))
                {
                    pthread_cond_wait(&pVideoDecData->mReadyFrameCondition, &pVideoDecData->mOutFrameListMutex);
                }
                pVideoDecData->mWaitReadyFrameFlag = FALSE;
                goto _TryToGetOutFrame;
            }
            else
            {
                pVideoDecData->mWaitReadyFrameFlag = TRUE;
                int ret = pthread_cond_wait_timeout(&pVideoDecData->mReadyFrameCondition, &pVideoDecData->mOutFrameListMutex, nMilliSec);
                if (ETIMEDOUT == ret) 
                {
                    alogv("wait output frame timeout[%d]ms, ret[%d]", nMilliSec, ret);
                    eError = ERR_VDEC_NOBUF;
                    pVideoDecData->mWaitReadyFrameFlag = FALSE;
                } 
                else if (0 == ret) 
                {
                    pVideoDecData->mWaitReadyFrameFlag = FALSE;
                    goto _TryToGetOutFrame;
                } 
                else 
                {
                    aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                    eError = ERR_VDEC_NOBUF;
                    pVideoDecData->mWaitReadyFrameFlag = FALSE;
                }
            }
        }
        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
        return eError;
    }
    else
    {
        pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
    _TryToGetCompOutFrame:
        if (!list_empty(&pVideoDecData->mCompReadyOutFrameList)) 
        {
            VDecCompOutputFrame *pOutFrame = list_first_entry(&pVideoDecData->mCompReadyOutFrameList, VDecCompOutputFrame, mList);
            config_VIDEO_FRAME_INFO_S_by_VideoPicture(pFrameInfo, pOutFrame->mpPicture, pVideoDecData);

            if(pVideoDecData->mVConfig.bSecOutputEn)
            {
                if(pSubFrameInfo && pOutFrame->mpSubPicture)
                {
                    config_VIDEO_FRAME_INFO_S_by_VideoPicture(pSubFrameInfo, pOutFrame->mpSubPicture, pVideoDecData);
                }
                else
                {
                    aloge("fatal error!,if you want get sub portout, why not get stream?");
                }
            }

            list_move_tail(&pOutFrame->mList, &pVideoDecData->mCompUsedOutFrameList);
            eError = SUCCESS;
        }
        else
        {
            if (0 == nMilliSec)
            {
                eError = ERR_VDEC_NOBUF;
            }
            else if (nMilliSec < 0) 
            {
                pVideoDecData->mbCompWaitReadyFrame = TRUE;
                while (list_empty(&pVideoDecData->mCompReadyOutFrameList))
                {
                    pthread_cond_wait(&pVideoDecData->mCompReadyFrameCondition, &pVideoDecData->mCompOutFramesLock);
                }
                pVideoDecData->mbCompWaitReadyFrame = FALSE;
                goto _TryToGetCompOutFrame;
            }
            else
            {
                pVideoDecData->mbCompWaitReadyFrame = TRUE;
                int ret = pthread_cond_wait_timeout(&pVideoDecData->mCompReadyFrameCondition, &pVideoDecData->mCompOutFramesLock, nMilliSec);
                if (ETIMEDOUT == ret) 
                {
                    alogv("wait output frame timeout[%d]ms, ret[%d]", nMilliSec, ret);
                    eError = ERR_VDEC_NOBUF;
                    pVideoDecData->mbCompWaitReadyFrame = FALSE;
                } 
                else if (0 == ret) 
                {
                    pVideoDecData->mbCompWaitReadyFrame = FALSE;
                    goto _TryToGetCompOutFrame;
                } 
                else 
                {
                    aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                    eError = ERR_VDEC_NOBUF;
                    pVideoDecData->mbCompWaitReadyFrame = FALSE;
                }
            }
        }
        pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
        return eError;
    }
}

ERRORTYPE VideoDecNonTunnel_ReleaseVDecCompOutputFrame(
    PARAM_IN VIDEODECDATATYPE *pVideoDecData, 
    PARAM_IN VIDEO_FRAME_INFO_S *pFrameInfo,
    PARAM_IN VIDEO_FRAME_INFO_S *pSubFrameInfo)
{
    ERRORTYPE eError = SUCCESS;
    if(FALSE == pVideoDecData->mbUseCompFrame)
    {
        pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
        if (!list_empty(&pVideoDecData->mUsedOutFrameList)) 
        {
            int ret;  //EVDECODERESULT
            int nFindFlag = 0;
            VDecCompOutputFrame *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pVideoDecData->mUsedOutFrameList, mList)
            {
                if (pEntry->mpPicture->nID == pFrameInfo->mId) 
                {
                    ret = ReturnPicture(pVideoDecData->pCedarV, pEntry->mpPicture);
                    if (ret != 0) 
                    {
                        aloge("fatal error! Return Picture() fail ret[%d]", ret);
                    }

                    if(pVideoDecData->mVConfig.bSecOutputEn)
                    {
                        if(pEntry->mpSubPicture)
                        {
                            if(pEntry->mpSubPicture->nID != pSubFrameInfo->mId)
                            {
                                aloge("fatal error! why subPicture Id is not match?[%d]!=[%d]", pEntry->mpSubPicture->nID, pSubFrameInfo->mId);
                            }
                            ret = ReturnPicture(pVideoDecData->pCedarV, pEntry->mpSubPicture); // must return sub frame to vdecoder, do not check id.
                            if(ret != 0)
                            {
                                aloge("fatal error! Return Picture() fail ret[%d]", ret);
                            }
                        }
                        else
                        {
                            aloge("fatal error! if you want get sub portout, why not get stream?");
                        }
                    }

                    list_move_tail(&pEntry->mList, &pVideoDecData->mIdleOutFrameList);
                    if (pVideoDecData->mWaitOutFrameFlag) 
                    {
                        pVideoDecData->mWaitOutFrameFlag = FALSE;
                        message_t msg;
                        msg.command = VDecComp_OutFrameAvailable;
                        put_message(&pVideoDecData->cmd_queue, &msg);
                    }
                    if (pVideoDecData->mWaitOutFrameFullFlag) 
                    {
                        int cnt = 0;
                        struct list_head *pList;
                        list_for_each(pList, &pVideoDecData->mIdleOutFrameList) { cnt++; }
                        if (cnt >= pVideoDecData->mFrameNodeNum) 
                        {
                            pthread_cond_signal(&pVideoDecData->mOutFrameFullCondition);
                        }
                    }
                    nFindFlag = 1;
                    eError = SUCCESS;
                    break;
                }
            }
            if (0 == nFindFlag) 
            {
                aloge("fatal error! vdec frameId[0x%x] is not match UsedOutFrameList", pFrameInfo->mId);
                eError = ERR_VDEC_ILLEGAL_PARAM;
            }
        }
        else 
        {
            aloge("fatal error! vdec frameId[0x%x] is not find in UsedOutFrameList", pFrameInfo->mId);
            eError = ERR_VDEC_ILLEGAL_PARAM;
        }
        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
        return eError;
    }
    else
    {
        //return it to VideoDec CompFrameBufferThread
        pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
        if (!list_empty(&pVideoDecData->mCompUsedOutFrameList))
        {
            int nFindFlag = 0;
            VDecCompOutputFrame *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pVideoDecData->mCompUsedOutFrameList, mList)
            {
                if (pEntry->mpPicture->nID == pFrameInfo->mId)
                {
                    list_move_tail(&pEntry->mList, &pVideoDecData->mCompIdleOutFrameList);
                    if(pVideoDecData->mbCompFBThreadWaitOutFrame)
                    {
                        pVideoDecData->mbCompFBThreadWaitOutFrame = FALSE;
                        message_t msg;
                        msg.command = VDecComp_CompFrameBufferThread_OutFrameAvailable;
                        put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
                    }
                    if (pVideoDecData->mbCompFBThreadWaitOutFrameFull) 
                    {
                        int cnt = 0;
                        struct list_head *pList;
                        list_for_each(pList, &pVideoDecData->mCompIdleOutFrameList) { cnt++; }
                        if (cnt >= VDEC_COMP_FRAME_COUNT)
                        {
                            pthread_cond_signal(&pVideoDecData->mCompFBThreadOutFrameFullCondition);
                        }
                    }
                    nFindFlag = 1;
                    eError = SUCCESS;
                    break;
                }
            }
            if(0 == nFindFlag)
            {
                aloge("fatal error! comp frameId[0x%x] is not match CompUsedOutFrameList", pFrameInfo->mId);
                eError = ERR_VDEC_ILLEGAL_PARAM;
            }
        }
        else 
        {
            aloge("fatal error! comp frameId[0x%x] is not find in CompUsedOutFrameList", pFrameInfo->mId);
            eError = ERR_VDEC_ILLEGAL_PARAM;
        }
        pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
        return eError;
    }
}

VDANWBuffer *searchVDANWBufferListByVideoPicture(struct list_head *pVdAnbList, VideoPicture *pPicture)
{
    int num = 0;
    VDANWBuffer *pDes = NULL;
    VDANWBuffer *pEntry;
    list_for_each_entry(pEntry, pVdAnbList, mList)
    {
        if (pEntry->mIonUserHandle == (int)(uintptr_t)pPicture->pPrivate &&
            pEntry->mpFrameBuf == (void *)pPicture->pData0) {
            if (0 == num) {
                pDes = pEntry;
                num++;
            } else {
                aloge("fatal error! same ion_user_handle_t[%d][%p]?", pEntry->mIonUserHandle, pEntry->mpFrameBuf);
                num++;
            }
        }
    }
    return pDes;
}

ERRORTYPE VideoDecDestroyVDANWBufferList(VIDEODECDATATYPE *pVideoDecData, struct list_head *pVdAnbList)
{
    if (!list_empty(pVdAnbList)) {
        alogd("free ANWBuffers ion_user_handle_t!");
        VDANWBuffer *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, pVdAnbList, mList)
        {
            if (pEntry->mIonUserHandle > 0) {
                struct ion_handle_data handleData = {
                    .handle = pEntry->mIonUserHandle,
                };
                ioctl(pVideoDecData->mIonFd, ION_IOC_FREE, &handleData);
                pEntry->mIonUserHandle = 0;
            }
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    return SUCCESS;
}

/*****************************************************************************/
/*******************************************************************************
Function name: VideoDecAddChangedStreamInfos
Description: 
    
Parameters: 
    
Return: 
    return entry which match the first element of pVideoInfo.
Time: 2015/9/21
*******************************************************************************/
VDStreamInfo *VideoDecAddChangedStreamInfos(VIDEODECDATATYPE *pVideoDecData, struct VideoInfo *pVideoinfo)
{
    VDStreamInfo *pDesEntry = NULL;
    int i;
    VDStreamInfo *pVDStreamInfo;
    for (i = 0; i < pVideoinfo->videoNum; i++) {
        pVDStreamInfo = malloc(sizeof(VDStreamInfo));
        if (NULL == pVDStreamInfo) {
            aloge("fatal error! malloc video stream info fail!");
            break;
        }
        memset(pVDStreamInfo, 0, sizeof(VDStreamInfo));
        memcpy(&pVDStreamInfo->mStreamInfo, &pVideoinfo->video[i], sizeof(VideoStreamInfo));
        if (pVideoinfo->video[i].pCodecSpecificData) {
            pVDStreamInfo->mStreamInfo.pCodecSpecificData = (char *)malloc(pVideoinfo->video[i].nCodecSpecificDataLen);
            if (pVDStreamInfo->mStreamInfo.pCodecSpecificData == NULL) {
                aloge("fatal error! malloc video specific data fail!");
                free(pVDStreamInfo);
                break;
            }
            memcpy(pVDStreamInfo->mStreamInfo.pCodecSpecificData, pVideoinfo->video[i].pCodecSpecificData,
                   pVideoinfo->video[i].nCodecSpecificDataLen);
            pVDStreamInfo->mStreamInfo.nCodecSpecificDataLen = pVideoinfo->video[i].nCodecSpecificDataLen;
        }
        list_add_tail(&pVDStreamInfo->mList, &pVideoDecData->mChangedStreamList);
        if (NULL == pDesEntry) {
            pDesEntry = pVDStreamInfo;
        }
    }
    return pDesEntry;
}

ERRORTYPE VideoDecDeleteChangedStreamInfo(VIDEODECDATATYPE *pVideoDecData, VideoStreamInfo *pVideoStreamInfo)
{
    VDStreamInfo *pDesEntry = NULL;
    VDStreamInfo *pEntry;
    list_for_each_entry(pEntry, &pVideoDecData->mChangedStreamList, mList)
    {
        if (&pEntry->mStreamInfo == pVideoStreamInfo) {
            pDesEntry = pEntry;
        }
    }
    if (pDesEntry) {
        list_del(&pDesEntry->mList);
        if (pDesEntry->mStreamInfo.pCodecSpecificData) {
            free(pDesEntry->mStreamInfo.pCodecSpecificData);
            pDesEntry->mStreamInfo.pCodecSpecificData = NULL;
        }
        free(pDesEntry);
        return SUCCESS;
    } else {
        aloge("fatal error! not find stream[%p] in changedStreamList", pVideoStreamInfo);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
}

ERRORTYPE VideoDecDestroyChangedStreamInfos(VIDEODECDATATYPE *pVideoDecData)
{
    if (!list_empty(&pVideoDecData->mChangedStreamList)) {
        VDStreamInfo *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoDecData->mChangedStreamList, mList)
        {
            if (pEntry->mStreamInfo.pCodecSpecificData) {
                free(pEntry->mStreamInfo.pCodecSpecificData);
                pEntry->mStreamInfo.pCodecSpecificData = NULL;
            }
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    return SUCCESS;
}

ERRORTYPE VideoDecGetMPPChannelInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT MPP_CHN_S *pChn)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    copy_MPP_CHN_S(pChn, &pVideoDecData->mMppChnInfo);
    return eError;
}

ERRORTYPE VideoDecGetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if (pPortDef->nPortIndex == pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].nPortIndex)
        memcpy(pPortDef, &pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pVideoDecData->sOutPortDef.nPortIndex)
        memcpy(pPortDef, &pVideoDecData->sOutPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK].nPortIndex)
        memcpy(pPortDef, &pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else
        eError = ERR_VDEC_ILLEGAL_PARAM;
    return eError;
}

ERRORTYPE VideoDecGetCompBufferSupplier(PARAM_IN COMP_HANDLETYPE hComponent,
                                        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for (i = 0; i < MAX_VDECODER_PORTS; i++) {
        if (pVideoDecData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pVideoDecData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if (bFindFlag) {
        eError = SUCCESS;
    } else {
        eError = ERR_VDEC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE VideoDecGetBufferState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT COMP_BUFFERSTATE *pBufferState)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    VideoDecoder *pCedarV = pVideoDecData->pCedarV;
    int nStreamBufIndex;
    if (pBufferState->video_stream_type == VIDEO_TYPE_MINOR)  //CDX_VIDEO_STREAM_MINOR
    {
        nStreamBufIndex = 1;
    } else {
        nStreamBufIndex = 0;
    }
    int nVbsBufferSize = VideoStreamBufferSize(pCedarV, nStreamBufIndex);
    int nVbsDataSize = VideoStreamDataSize(pCedarV, nStreamBufIndex);
    int nVbsFrameNum = VideoStreamFrameNum(pCedarV, nStreamBufIndex);
    pBufferState->nElementCounter = nVbsFrameNum;
    pBufferState->nValidSizePercent = nVbsDataSize * 100 / nVbsBufferSize;
    return SUCCESS;
}

ERRORTYPE VideoDecGetFbmBufInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT FbmBufInfo *pFbmBufInfo)
{
    ERRORTYPE eError;
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    FbmBufInfo *pVdeclibFbmBufInfo = GetVideoFbmBufInfo(pVideoDecData->pCedarV);
    if (pFbmBufInfo) {
        memcpy(pFbmBufInfo, pVdeclibFbmBufInfo, sizeof(FbmBufInfo));
        eError = SUCCESS;

    } else {
        eError = ERR_VDEC_SYS_NOTREADY;
    }
    return eError;
}

ERRORTYPE VideoDecGetChnAttr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT VDEC_CHN_ATTR_S *pChnAttr)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    memcpy(pChnAttr, &pVideoDecData->mChnAttr, sizeof(VDEC_CHN_ATTR_S));
    return SUCCESS;
}

ERRORTYPE VideoDecGetChnState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT VDEC_CHN_STAT_S *pChnStat)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    VideoStreamInfo videoInfo;
    memset(&videoInfo, 0, sizeof(VideoStreamInfo));
    GetVideoStreamInfo(pVideoDecData->pCedarV, &videoInfo);
    pChnStat->mType = map_EVIDEOCODECFORMAT_to_PAYLOAD_TYPE_E(videoInfo.eCodecFormat);
    pChnStat->mLeftStreamBytes = VideoStreamDataSize(pVideoDecData->pCedarV, 0);
    pChnStat->mLeftStreamFrames = VideoStreamFrameNum(pVideoDecData->pCedarV, 0);
    pChnStat->mLeftPics = ValidPictureNum(pVideoDecData->pCedarV, 0);
    if (COMP_StateExecuting == pVideoDecData->state || COMP_StatePause == pVideoDecData->state) {
        pChnStat->mbStartRecvStream = TRUE;
    } else {
        pChnStat->mbStartRecvStream = FALSE;
    }
    
    pthread_mutex_lock(&pVideoDecData->mDecodeFramesControlLock);
    if(pVideoDecData->mLimitedDecodeFramesFlag)
    {
        pChnStat->mLeftDecodeStreamFrames = pVideoDecData->mDecodeFramesParam.mDecodeFrameNum - pVideoDecData->mCurDecodeFramesNum;
    }
    else
    {
        pChnStat->mLeftDecodeStreamFrames = 0;
    }
    pthread_mutex_unlock(&pVideoDecData->mDecodeFramesControlLock);
    
    
    return SUCCESS;
}

ERRORTYPE VideoDecGetParam(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT VDEC_CHN_PARAM_S *pChnParam)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    memcpy(pChnParam, &pVideoDecData->mChnParam, sizeof(VDEC_CHN_PARAM_S));
    return SUCCESS;
}

ERRORTYPE VideoDecGetProtocolParam(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT VDEC_PRTCL_PARAM_S *pProtocolParam)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    memcpy(pProtocolParam, &pVideoDecData->mPrtclParam, sizeof(VDEC_PRTCL_PARAM_S));
    return SUCCESS;
}

/** 
 * get frame, used in non-tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent vdec component.
 * @param pFrameInfo store frame info, caller malloc.
 * @param nMilliSec 0:return immediately, <0:wait forever, >0:wait some time.
 */
ERRORTYPE VideoDecGetFrame(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT VIDEO_FRAME_INFO_S *pFrameInfo, PARAM_OUT VIDEO_FRAME_INFO_S *pSubFrameInfo, PARAM_IN int nMilliSec)
{
    ERRORTYPE eError;
    int ret;
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (COMP_StateIdle != pVideoDecData->state && COMP_StateExecuting != pVideoDecData->state && COMP_StatePause != pVideoDecData->state) 
    {
        alogw("call getStream in wrong state[0x%x]", pVideoDecData->state);
        return ERR_VDEC_NOT_PERM;
    }
    if (pVideoDecData->mOutputPortTunnelFlag) 
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_VDEC_NOT_PERM;
    }
    eError = VideoDecNonTunnel_GetVDecCompOutputFrame(pVideoDecData, pFrameInfo, pSubFrameInfo, nMilliSec);
    
    return eError;
}

ERRORTYPE VideoDecGetRotate(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT ROTATE_E *pRotate)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    *pRotate = map_cedarv_rotation_to_ROTATE_E(pVideoDecData->cedarv_rotation);
    return SUCCESS;
}

ERRORTYPE VideoDecSetPortDefinition(PARAM_IN COMP_HANDLETYPE hComponent,
                                    PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if (pPortDef->nPortIndex == pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].nPortIndex)
        memcpy(&pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK].nPortIndex)
        memcpy(&pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else if (pPortDef->nPortIndex == pVideoDecData->sOutPortDef.nPortIndex)
        memcpy(&pVideoDecData->sOutPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    else
        eError = ERR_VDEC_ILLEGAL_PARAM;
    return eError;
}

ERRORTYPE VideoDecSetCompBufferSupplier(PARAM_IN COMP_HANDLETYPE hComponent,
                                        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for (i = 0; i < MAX_VDECODER_PORTS; i++) {
        if (pVideoDecData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
            bFindFlag = TRUE;
            memcpy(&pVideoDecData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if (bFindFlag) {
        eError = SUCCESS;
    } else {
        eError = ERR_VDEC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE VideoDecSetIonFd(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nIonFd)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (pVideoDecData->mIonFd >= 0) {
        aloge("fatal error! ionFd[%d] >= 0?", pVideoDecData->mIonFd);
        close(pVideoDecData->mIonFd);
        pVideoDecData->mIonFd = -1;
    }
    pVideoDecData->mIonFd = dup(nIonFd);
    return SUCCESS;
}
ERRORTYPE VideoDecSeek(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    int cnt = 0;

    if(pVideoDecData->pCedarV)
    {
        if(pVideoDecData->mbUseCompFrame)   // to release node cached in 
        { 
            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
            if (!list_empty(&pVideoDecData->mUsedOutFrameList))
            { 
                pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex); 
                VDecCompOutputFrame *pEntry, *pTmp; 
                
                list_for_each_entry_safe(pEntry, pTmp, &pVideoDecData->mUsedOutFrameList, mList)
                {
                    VideoDecReturnVDecCompOutputFrameToIdleList(pVideoDecData, pEntry->mpPicture->nID);
                    cnt++;
                }
            }
            else
            {
                pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex); 
            }

            alogd("vdec_out_used_frm_list_node_cnt:%d_when_seek",cnt);
        }
        ResetVideoDecoder(pVideoDecData->pCedarV);
    }
    else
    {
        alogw("the vdecor do not create, it means had seek!!!");
    }
    pVideoDecData->mbEof = FALSE;
    return SUCCESS;
}

ERRORTYPE VideoDecSetStreamEof(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    alogv("vdec end flag is set");
    pVideoDecData->priv_flag |= CDX_comp_PRIV_FLAGS_STREAMEOF;
    message_t msg;
    msg.command = VDecComp_VbsAvailable;
    put_message(&pVideoDecData->cmd_queue, &msg);
    return SUCCESS;
}

ERRORTYPE VideoDecClearStreamEof(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    alogv("vdec end flag is clear");
    pVideoDecData->priv_flag &= ~(CDX_comp_PRIV_FLAGS_STREAMEOF);
    return SUCCESS;
}

ERRORTYPE VideoDecSetRotate(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN ROTATE_E eRotate)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    pVideoDecData->cedarv_rotation = map_ROTATE_E_to_cedarv_rotation(eRotate);
    return eError;
}

ERRORTYPE VideoDecSetOutputPixelFormat(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN enum EPIXELFORMAT eVdecPixelFormat)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    pVideoDecData->cedarv_output_setting = eVdecPixelFormat;
    return eError;
}

ERRORTYPE VideoDecSetDisplayFrameRequestMode(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nFrameRequest)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    pVideoDecData->nDisplayFrameRequestMode = nFrameRequest;
    return SUCCESS;
}

ERRORTYPE VideoDecSetMPPChannelInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN MPP_CHN_S *pChn)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pVideoDecData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE VideoDecSetChnAttr(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN VDEC_CHN_ATTR_S *pChnAttr)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    memcpy(&pVideoDecData->mChnAttr, pChnAttr, sizeof(VDEC_CHN_ATTR_S));
    pVideoDecData->cedarv_max_width = pVideoDecData->mChnAttr.mPicWidth;
    pVideoDecData->cedarv_max_height = pVideoDecData->mChnAttr.mPicHeight;
    pVideoDecData->cedarv_output_setting = map_PIXEL_FORMAT_E_to_EPIXELFORMAT(pVideoDecData->mChnAttr.mOutputPixelFormat);
    pVideoDecData->cedarv_rotation = map_ROTATE_E_to_cedarv_rotation(pVideoDecData->mChnAttr.mInitRotation);
    pVideoDecData->config_vbv_size = pVideoDecData->mChnAttr.mBufSize;
    pVideoDecData->mVdecExtraFrameNum = pVideoDecData->mChnAttr.mExtraFrameNum;
    pVideoDecData->cedarv_second_output_en = pVideoDecData->mChnAttr.mSubPicEnable;
    if(pVideoDecData->cedarv_second_output_en)
    {
        pVideoDecData->cedarv_second_output_setting = map_PIXEL_FORMAT_E_to_EPIXELFORMAT(pVideoDecData->mChnAttr.mSubOutputPixelFormat);
        pVideoDecData->cedarv_second_horizon_scale_down_ratio = pVideoDecData->mChnAttr.mSubPicWidthRatio;
        pVideoDecData->cedarv_second_vertical_scale_down_ratio = pVideoDecData->mChnAttr.mSubPicHeightRatio;
        if(pVideoDecData->cedarv_second_horizon_scale_down_ratio >= 5 
            || pVideoDecData->cedarv_second_horizon_scale_down_ratio < 0
            || pVideoDecData->cedarv_second_vertical_scale_down_ratio >= 5
            || pVideoDecData->cedarv_second_vertical_scale_down_ratio < 0)
        {
            aloge("fatal error! sub ratio wrong! [%d],[%d]", pVideoDecData->cedarv_second_horizon_scale_down_ratio, pVideoDecData->cedarv_second_vertical_scale_down_ratio);
            pVideoDecData->cedarv_second_output_en = 0;
        }
    }
    return eError;
}

ERRORTYPE VideoDecResetChannel(PARAM_IN COMP_HANDLETYPE hComponent)
{
    //ERRORTYPE eError;
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (pVideoDecData->state != COMP_StateIdle) {
        aloge("fatal error! must reset channel in stateIdle!");
        return ERR_VDEC_NOT_PERM;
    }

    int cnt;
    struct list_head *pList;
    alogd("wait VDec idleOutFrameList full");
    pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
    pVideoDecData->mWaitOutFrameFullFlag = TRUE;
    //wait all outFrame return.
    while (1) {
        cnt = 0;
        list_for_each(pList, &pVideoDecData->mIdleOutFrameList) { cnt++; }
        if (cnt < pVideoDecData->mFrameNodeNum) {
            alogd("wait idleOutFrameList [%d]nodes to home", pVideoDecData->mFrameNodeNum - cnt);
            pthread_cond_wait(&pVideoDecData->mOutFrameFullCondition, &pVideoDecData->mOutFrameListMutex);
        } else {
            break;
        }
    }
    pVideoDecData->mWaitOutFrameFullFlag = FALSE;
    pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
    //free ion_user_handle_t
    VideoDecDestroyVDANWBufferList(pVideoDecData, &pVideoDecData->mPreviousANWBuffersList);
    VideoDecDestroyVDANWBufferList(pVideoDecData, &pVideoDecData->mANWBuffersList);

    pthread_mutex_lock(&pVideoDecData->mDecodeFramesControlLock);
    pVideoDecData->mLimitedDecodeFramesFlag = FALSE;
    pVideoDecData->mDecodeFramesParam.mDecodeFrameNum = 0;
    pVideoDecData->mCurDecodeFramesNum = 0;
    pthread_mutex_unlock(&pVideoDecData->mDecodeFramesControlLock);
    
    alogd("wait VDec idleOutFrameList full done");
    return SUCCESS;
}

ERRORTYPE VideoDecSetChnParam(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN VDEC_CHN_PARAM_S *pChnParam)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    memcpy(&pVideoDecData->mChnParam, pChnParam, sizeof(VDEC_CHN_PARAM_S));

    if (pVideoDecData->mChnParam.mDecMode != 0) {
        pVideoDecData->drop_B_frame = 1;
    } else {
        pVideoDecData->drop_B_frame = 0;
    }
    return eError;
}

ERRORTYPE VideoDecSetProtocolParam(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN VDEC_PRTCL_PARAM_S *pPrtclParam)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    memcpy(&pVideoDecData->mPrtclParam, pPrtclParam, sizeof(VDEC_PRTCL_PARAM_S));
    return eError;
}

/** 
 * release frame, used in non-tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent vdec component.
 * @param pFrameInfo frame info, caller malloc.
 */
ERRORTYPE VideoDecReleaseFrame(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN VIDEO_FRAME_INFO_S *pFrameInfo, PARAM_IN VIDEO_FRAME_INFO_S *pSubFrameInfo)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if (COMP_StateIdle != pVideoDecData->state && COMP_StateExecuting != pVideoDecData->state && COMP_StatePause != pVideoDecData->state) 
    {
        alogw("call getStream in wrong state[0x%x]", pVideoDecData->state);
        return ERR_VDEC_NOT_PERM;
    }
    if (pVideoDecData->mOutputPortTunnelFlag) 
    {
        aloge("fatal error! can't call releaseFrame() in tunnel mode!");
        return ERR_VDEC_NOT_PERM;
    }
    eError = VideoDecNonTunnel_ReleaseVDecCompOutputFrame(pVideoDecData, pFrameInfo, pSubFrameInfo);
    return eError;
}

ERRORTYPE VideoDecSetFrameBuffersToVdecLib(PARAM_IN COMP_HANDLETYPE hComponent,
                                           PARAM_IN VDecCompFrameBuffersParam *pParam)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    CdxANWBuffersInfo *pAnwBuffersInfo = pParam->mpANWBuffersInfo;
    struct list_head *pOutFrameList = &pParam->mFramesOwnedByANW;  //VDecCompOutputFrame
    INIT_LIST_HEAD(pOutFrameList);
    pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
    //set gpu buffers to vdeclib.
    int i;
    VideoPicture tmpVP;
    VideoPicture *pVdecPic;
    memset(&tmpVP, 0, sizeof(VideoPicture));
    for (i = 0; i < pAnwBuffersInfo->mnBufNum; i++) {
        // set the buffer address, bufId, ion_user_handle_t, pixelFormat, sizeParam.
        tmpVP.pData0 = (char *)pAnwBuffersInfo->mANWBuffers[i].dst;
        tmpVP.pData1 = tmpVP.pData0 + (pAnwBuffersInfo->mANWBuffers[i].height * pAnwBuffersInfo->mANWBuffers[i].stride);
        tmpVP.pData2 =
          tmpVP.pData1 + (pAnwBuffersInfo->mANWBuffers[i].height * pAnwBuffersInfo->mANWBuffers[i].stride) / 4;
        tmpVP.phyYBufAddr = (size_addr)pAnwBuffersInfo->mANWBuffers[i].dstPhy;
        tmpVP.phyCBufAddr = (size_addr)tmpVP.phyYBufAddr +
                            (pAnwBuffersInfo->mANWBuffers[i].height * pAnwBuffersInfo->mANWBuffers[i].stride);
        tmpVP.nBufId = i;
        tmpVP.pPrivate = (void *)(uintptr_t)pAnwBuffersInfo->mANWBuffers[i].mIonUserHandle;
        tmpVP.ePixelFormat = convertPixelFormatCdx2Vdec(pAnwBuffersInfo->mANWBuffers[i].format);
        tmpVP.nWidth = pAnwBuffersInfo->mANWBuffers[i].stride;
        tmpVP.nHeight = pAnwBuffersInfo->mANWBuffers[i].height;
        tmpVP.nLineStride = pAnwBuffersInfo->mANWBuffers[i].stride;
        if (pAnwBuffersInfo->mANWBuffers[i].mbOccupyFlag) {
            if (FALSE == pVideoDecData->mbConfigVdecFrameBuffers) {
                SetVideoFbmBufAddress(pVideoDecData->pCedarV, &tmpVP, 0);
            } else {
                ReturnRelasePicture(pVideoDecData->pCedarV, &tmpVP, 0);
            }

        } else {
            if (FALSE == pVideoDecData->mbConfigVdecFrameBuffers) {
                pVdecPic = SetVideoFbmBufAddress(pVideoDecData->pCedarV, &tmpVP, 1);
            } else {
                pVdecPic = ReturnRelasePicture(pVideoDecData->pCedarV, &tmpVP, 1);
            }
            if (list_empty(&pVideoDecData->mIdleOutFrameList)) {
                aloge("fatal error! idle out frame list can't be empty in state[%d]", pVideoDecData->state);
                continue;
            }
            VDecCompOutputFrame *pOutFrame =
              list_first_entry(&pVideoDecData->mIdleOutFrameList, VDecCompOutputFrame, mList);
            pOutFrame->mpPicture = pVdecPic;
            list_move_tail(&pOutFrame->mList, pOutFrameList);
        }
    }
    pVideoDecData->mbConfigVdecFrameBuffers = TRUE;
    //store ANWBuffers in VDANWBuffer list.
    if (!list_empty(&pVideoDecData->mANWBuffersList)) {
        aloge("fatal error! why ANWBuffers is not empty?");
        abort();
    }
    if (!list_empty(&pVideoDecData->mPreviousANWBuffersList)) {
        alogw("Low probability! previousANWBuffersList is not empty");
    }
    for (i = 0; i < pAnwBuffersInfo->mnBufNum; i++) {
        VDANWBuffer *pANWBuf = (VDANWBuffer *)malloc(sizeof(VDANWBuffer));
        if (NULL == pANWBuf) {
            aloge("fatal error! malloc fail!");
            break;
        }
        memset(pANWBuf, 0, sizeof(VDANWBuffer));
        pANWBuf->mpFrameBuf = (void *)pAnwBuffersInfo->mANWBuffers[i].dst;
        pANWBuf->mIonUserHandle = pAnwBuffersInfo->mANWBuffers[i].mIonUserHandle;
        list_add_tail(&pANWBuf->mList, &pVideoDecData->mANWBuffersList);
    }
    //print ANativeWindow buffers info
    alogd("set gpu buffers num[%d], state[%d]", pAnwBuffersInfo->mnBufNum, pVideoDecData->state);
    for (i = 0; i < pAnwBuffersInfo->mnBufNum; i++) {
        alogd("buffer[%d]: [%dx%d][%d][0x%x][0x%x][%p][%p][%p],[%d]", i, pAnwBuffersInfo->mANWBuffers[i].width,
              pAnwBuffersInfo->mANWBuffers[i].height, pAnwBuffersInfo->mANWBuffers[i].stride,
              pAnwBuffersInfo->mANWBuffers[i].format, pAnwBuffersInfo->mANWBuffers[i].usage,
              pAnwBuffersInfo->mANWBuffers[i].dst, pAnwBuffersInfo->mANWBuffers[i].dstPhy,
              pAnwBuffersInfo->mANWBuffers[i].pObjANativeWindowBuffer, pAnwBuffersInfo->mANWBuffers[i].mbOccupyFlag);
    }
    //notify vdec thread if necessary
    if (pVideoDecData->mWaitOutFrameFlag) {
        pVideoDecData->mWaitOutFrameFlag = FALSE;
        message_t msg;
        msg.command = VDecComp_OutFrameAvailable;
        put_message(&pVideoDecData->cmd_queue, &msg);
    }
    pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
    return SUCCESS;
}

ERRORTYPE VideoDecReopenVideoEngine(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError;
    int ret;
    VideoStreamInfo streamInfo;
    VideoStreamInfo *pVideoInfo = (VideoStreamInfo *)VideoStreamDataInfoPointer(pVideoDecData->pCedarV, 0);
    if (pVideoInfo != NULL) {
        memcpy(&streamInfo, pVideoInfo, sizeof(VideoStreamInfo));
    } else {
        //*if resolustionChange was detected by decoder, we should not send the
        //* specific data to decoder. or decoder will appear error.
        memset(&streamInfo, 0, sizeof(VideoStreamInfo));
        if (pVideoDecData->mInputPortTunnelFlag)
        {
            VideoStreamInfo *pVideoStreamInfo =
              (VideoStreamInfo *)pVideoDecData->sInPortExtraDef[VDEC_PORT_SUFFIX_DEMUX].pVendorInfo;
            memcpy(&streamInfo, pVideoStreamInfo, sizeof(VideoStreamInfo));
        }
        else
        {
            streamInfo.eCodecFormat = map_PAYLOAD_TYPE_E_to_EVIDEOCODECFORMAT(pVideoDecData->mChnAttr.mType);
        }
        //streamInfo.pCodecSpecificData = NULL;
        //streamInfo.nCodecSpecificDataLen = 0;
    }
    ret = ReopenVideoEngine(pVideoDecData->pCedarV, &pVideoDecData->mVConfig, &streamInfo);
    pVideoDecData->mbConfigVdecFrameBuffers = FALSE;
    if (pVideoInfo) {
        VideoDecDeleteChangedStreamInfo(pVideoDecData, pVideoInfo);
    }
    VideoDecDestroyVDANWBufferList(pVideoDecData, &pVideoDecData->mANWBuffersList);
    message_t msg;
    msg.command = VDecComp_ReopenVideoEngine;
    put_message(&pVideoDecData->cmd_queue, &msg);
    if (0 == ret) {
        eError = SUCCESS;
    } else {
        eError = ERR_VDEC_SYS_NOTREADY;
    }
    return eError;
}

static ERRORTYPE VideoDecSetVEFreq(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nFreq)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    pVideoDecData->mVEFreq = nFreq;
    if(nFreq > 0)
    {
        if(pVideoDecData->pCedarV)
        {
            alogd("vdec set VE freq to [%d]MHz", nFreq);
            VideoDecoderSetFreq(pVideoDecData->pCedarV, nFreq);
        }
    }
    return SUCCESS;
}

static ERRORTYPE VideoDecSetDecodeFrame(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN VDEC_DECODE_FRAME_PARAM_S *pDecodeParam)
{    
    ERRORTYPE  eError = SUCCESS;
    VIDEODECDATATYPE  *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pVideoDecData->mDecodeFramesControlLock);
    if(pDecodeParam && pDecodeParam->mDecodeFrameNum > 0)
    {
        pVideoDecData->mLimitedDecodeFramesFlag = TRUE;
        pVideoDecData->mDecodeFramesParam = *pDecodeParam;
    }
    else
    {        
        pVideoDecData->mLimitedDecodeFramesFlag = FALSE;
        pVideoDecData->mDecodeFramesParam.mDecodeFrameNum = 0;
    }
    pVideoDecData->mCurDecodeFramesNum = 0;
    pthread_mutex_unlock(&pVideoDecData->mDecodeFramesControlLock);
    
    return eError;
}

static ERRORTYPE VideoDecSetVideoStreamInfo(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN VideoStreamInfo *pVideoStreamInfo)
{
    ERRORTYPE  eError = SUCCESS;
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (NULL == pVideoStreamInfo)
    {
        aloge("fatal error! Set VideoStreamInfo is NULL!");
        return ERR_VDEC_NULL_PTR;
    }
    memcpy(&pVideoDecData->stVideoStreamInfo, pVideoStreamInfo, sizeof(VideoStreamInfo));
    if (pVideoStreamInfo->nCodecSpecificDataLen > 0 && pVideoStreamInfo->pCodecSpecificData)
    {
        char* tmpBuf = (char*)malloc(pVideoStreamInfo->nCodecSpecificDataLen);
        if (NULL == tmpBuf)
        {
            aloge("fatal error! malloc fail!");
            return ERR_VDEC_NOMEM;
        }
        memcpy(tmpBuf, pVideoStreamInfo->pCodecSpecificData, pVideoStreamInfo->nCodecSpecificDataLen);
        pVideoDecData->stVideoStreamInfo.pCodecSpecificData = tmpBuf;
    }
    return SUCCESS;
}

static ERRORTYPE VideoDecForceFramePackage(PARAM_IN COMP_HANDLETYPE hComponent, BOOL bFlag)
{
    ERRORTYPE  eError = SUCCESS;
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    pVideoDecData->mbForceFramePackage = bFlag;
    return SUCCESS;
}

ERRORTYPE VideoDecSendCommand(COMP_HANDLETYPE hComponent, COMP_COMMANDTYPE Cmd, unsigned int nParam1, void *pCmdData)
{
    VIDEODECDATATYPE *pVideoDecData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError;
    message_t msg;

    eError = SUCCESS;
    pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (pVideoDecData->state == COMP_StateInvalid) {
        eError = ERR_VDEC_INVALIDSTATE;
        goto OMX_CONF_CMD_BAIL;
    }

    switch (Cmd) {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        case COMP_CommandPortDisable:
            eCmd = StopPort;
            break;

        case COMP_CommandPortEnable:
            eCmd = RestartPort;
            break;

        case COMP_CommandVendorChangeANativeWindow: {
            eCmd = VDecComp_ChangeGraphicBufferProducer;
            break;
        }
        default:
            eCmd = -1;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pVideoDecData->cmd_queue, &msg);

OMX_CONF_CMD_BAIL:
    return eError;
}

/*****************************************************************************/
ERRORTYPE VideoDecGetState(COMP_HANDLETYPE hComponent, COMP_STATETYPE *pState)
{
    VIDEODECDATATYPE *pVideoDecData;
    ERRORTYPE eError;

    eError = SUCCESS;

    pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    *pState = pVideoDecData->state;
    return eError;
}

/*****************************************************************************/
ERRORTYPE VideoDecSetCallbacks(COMP_HANDLETYPE hComponent, COMP_CALLBACKTYPE *pCallbacks, void *pAppData)
{
    VIDEODECDATATYPE *pVideoDecData;
    ERRORTYPE eError;

    eError = SUCCESS;
    pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pVideoDecData->pCallbacks = pCallbacks;
    pVideoDecData->pAppData = pAppData;

OMX_CONF_CMD_BAIL:
    return eError;
}

ERRORTYPE VideoDecGetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                            PARAM_INOUT void *pComponentConfigStructure)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;

    switch (nIndex) {
        case COMP_IndexVendorMPPChannelInfo: {
            eError = VideoDecGetMPPChannelInfo(hComponent, (MPP_CHN_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamPortDefinition: {
            eError = VideoDecGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier: {
            eError =
              VideoDecGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorBufferState: {
            eError = VideoDecGetBufferState(hComponent, (COMP_BUFFERSTATE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorFbmBufInfo: {
            eError = VideoDecGetFbmBufInfo(hComponent, (FbmBufInfo *)pComponentConfigStructure);
            break;
        }
//        case COMP_IndexVendorConfigInputBuffer: {
//            COMP_BUFFERHEADERTYPE *pBuffer = (COMP_BUFFERHEADERTYPE *)pComponentConfigStructure;
//            eError = VideoDecRequstBuffer(hComponent, pBuffer->nInputPortIndex, pBuffer);
//            break;
//        }
        case COMP_IndexVendorVdecChnAttr: {
            eError = VideoDecGetChnAttr(hComponent, (VDEC_CHN_ATTR_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecChnState: {
            eError = VideoDecGetChnState(hComponent, (VDEC_CHN_STAT_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecParam: {
            eError = VideoDecGetParam(hComponent, (VDEC_CHN_PARAM_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecProtocolParam: {
            eError = VideoDecGetProtocolParam(hComponent, (VDEC_PRTCL_PARAM_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecGetFrame: {
            VdecOutFrame *pFrame = (VdecOutFrame *)pComponentConfigStructure;
            eError = VideoDecGetFrame(hComponent, pFrame->pFrameInfo, pFrame->pSubFrameInfo, pFrame->nMilliSec);
            break;
        }
        case COMP_IndexVendorVdecRotate: {
            eError = VideoDecGetRotate(hComponent, (ROTATE_E *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecLuma: {
            aloge("unsupported temporary");
            eError = FAILURE;
            break;
        }
        default: {
            aloge("fatal error! unknown nIndex[0x%x] in state[%d]", nIndex, pVideoDecData->state);
            eError = ERR_VDEC_NOT_SURPPORT;
            break;
        }
    }
    return eError;
}

ERRORTYPE VideoDecSetConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_INDEXTYPE nIndex,
                            PARAM_IN void *pComponentConfigStructure)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;

    switch (nIndex) {
        case COMP_IndexParamPortDefinition:
            eError = VideoDecSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE *)pComponentConfigStructure);
            break;
        case COMP_IndexParamCompBufferSupplier: {
            eError =
              VideoDecSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorIonFd:
            eError = VideoDecSetIonFd(hComponent, *(int *)pComponentConfigStructure);
            break;

        case COMP_IndexVendorSeekToPosition:
            eError = VideoDecSeek(hComponent);
            break;

        case COMP_IndexVendorSetStreamEof: {
            eError = VideoDecSetStreamEof(hComponent);
            break;
        }
        case COMP_IndexVendorClearStreamEof:
            eError = VideoDecClearStreamEof(hComponent);
            break;

        case COMP_IndexVendorVdecRotate:
            eError = VideoDecSetRotate(hComponent, *(ROTATE_E *)pComponentConfigStructure);
            break;
        case COMP_IndexVendorCedarvOutputSetting:
            eError = VideoDecSetOutputPixelFormat(hComponent, *(enum EPIXELFORMAT *)pComponentConfigStructure);
            break;

        case COMP_IndexVendorDisplayFrameRequestMode:
            eError = VideoDecSetDisplayFrameRequestMode(hComponent, *(int *)pComponentConfigStructure);
            break;
        case COMP_IndexVendorMPPChannelInfo: {
            eError = VideoDecSetMPPChannelInfo(hComponent, (MPP_CHN_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecChnAttr: {
            eError = VideoDecSetChnAttr(hComponent, (VDEC_CHN_ATTR_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecResetChannel: {
            eError = VideoDecResetChannel(hComponent);
            break;
        }
        case COMP_IndexVendorVdecParam: {
            eError = VideoDecSetChnParam(hComponent, (VDEC_CHN_PARAM_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecProtocolParam: {
            eError = VideoDecSetProtocolParam(hComponent, (VDEC_PRTCL_PARAM_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecReleaseFrame: {
            VdecOutFrame *pFrame = (VdecOutFrame *)pComponentConfigStructure;
            eError = VideoDecReleaseFrame(hComponent, pFrame->pFrameInfo, pFrame->pSubFrameInfo);
            break;
        }
//        case COMP_IndexVendorConfigInputBuffer: {
//            COMP_BUFFERHEADERTYPE *pBuffer = (COMP_BUFFERHEADERTYPE *)pComponentConfigStructure;
//            eError = VideoDecReleaseBuffer(hComponent, pBuffer->nInputPortIndex, pBuffer);
//            break;
//        }
        case COMP_IndexVendorFrameBuffers: {
            eError =
              VideoDecSetFrameBuffersToVdecLib(hComponent, (VDecCompFrameBuffersParam *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorReopenVideoEngine: {
            eError = VideoDecReopenVideoEngine(hComponent);
            break;
        }
        case COMP_IndexVendorVEFreq:
        {
            eError = VideoDecSetVEFreq(hComponent, *(int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecDecodeFrameParam:
        {
            eError = VideoDecSetDecodeFrame(hComponent, (VDEC_DECODE_FRAME_PARAM_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorExtraData:
        {
            eError = VideoDecSetVideoStreamInfo(hComponent, (VideoStreamInfo *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVdecForceFramePackage:
        {
            eError = VideoDecForceFramePackage(hComponent, *(BOOL*)pComponentConfigStructure);
            break;
        }
        default: {
            aloge("fatal error! unknown nIndex[0x%x] in state[%d]", nIndex, pVideoDecData->state);
            eError = ERR_VDEC_ILLEGAL_PARAM;
            break;
        }
    }
    return eError;
}

ERRORTYPE VideoDecComponentTunnelRequest(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN unsigned int nPort,
                                         PARAM_IN COMP_HANDLETYPE hTunneledComp, PARAM_IN unsigned int nTunneledPort,
                                         PARAM_INOUT COMP_TUNNELSETUPTYPE *pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    if (pVideoDecData->state == COMP_StateExecuting) {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    } else if (pVideoDecData->state != COMP_StateIdle) {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pVideoDecData->state);
        eError = ERR_VDEC_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }
    COMP_PARAM_PORTDEFINITIONTYPE *pPortDef;
    COMP_PARAM_PORTEXTRADEFINITIONTYPE *pPortExtraDef = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier;
    BOOL bFindFlag;
    int i;
    bFindFlag = FALSE;
    for (i = 0; i < MAX_VDECODER_IN_PORTS; i++) {
        if (pVideoDecData->sInPortDef[i].nPortIndex == nPort) {
            pPortDef = &pVideoDecData->sInPortDef[i];
            pPortExtraDef = &pVideoDecData->sInPortExtraDef[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if (FALSE == bFindFlag) {
        if (pVideoDecData->sOutPortDef.nPortIndex == nPort) {
            pPortDef = &pVideoDecData->sOutPortDef;
            bFindFlag = TRUE;
        }
    }
    if (FALSE == bFindFlag) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_VDEC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for (i = 0; i < MAX_VDECODER_IN_PORTS; i++) {
        if (pVideoDecData->sInPortTunnelInfo[i].nPortIndex == nPort) {
            pPortTunnelInfo = &pVideoDecData->sInPortTunnelInfo[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if (FALSE == bFindFlag) {
        if (pVideoDecData->sOutPortTunnelInfo.nPortIndex == nPort) {
            pPortTunnelInfo = &pVideoDecData->sOutPortTunnelInfo;
            bFindFlag = TRUE;
        }
    }
    if (FALSE == bFindFlag) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_VDEC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for (i = 0; i < MAX_VDECODER_PORTS; i++) {
        if (pVideoDecData->sPortBufSupplier[i].nPortIndex == nPort) {
            pPortBufSupplier = &pVideoDecData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if (FALSE == bFindFlag) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_VDEC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if (NULL == hTunneledComp && 0 == nTunneledPort && NULL == pTunnelSetup) 
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        if (pPortDef->eDir == COMP_DirOutput)
        {
            pVideoDecData->mOutputPortTunnelFlag = FALSE;
        }
        else
        {
            pVideoDecData->mInputPortTunnelFlag = FALSE;
        }
        eError = SUCCESS;
        goto COMP_CMD_FAIL;
    }
    if (pPortDef->eDir == COMP_DirOutput) {
        if (pVideoDecData->mOutputPortTunnelFlag) {
            aloge("VDec_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pVideoDecData->mOutputPortTunnelFlag = TRUE;
    } else {
        if (pVideoDecData->mInputPortTunnelFlag) {
            aloge("VDec_Comp inport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE *)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if (out_port_def.eDir != COMP_DirOutput) {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_VDEC_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;
        //pPortDef->pVendorInfo = out_port_def.pVendorInfo;
        COMP_PARAM_PORTEXTRADEFINITIONTYPE out_port_extra_def;
        memset(&out_port_extra_def, 0, sizeof(COMP_PARAM_PORTEXTRADEFINITIONTYPE));
        out_port_extra_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE *)hTunneledComp)
          ->GetConfig(hTunneledComp, COMP_IndexVendorParamPortExtraDefinition, &out_port_extra_def);
        pPortExtraDef->pVendorInfo = out_port_extra_def.pVendorInfo;

        //The component B informs component A about the final result of negotiation.
        if (pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier) {
            alogw("Low probability! input portIndex[%d] buffer supplier[%d] is not same as output portIndex[%d] buffer supplier[%d], respect output port decision!", 
                nPort, pPortBufSupplier->eBufferSupplier, nTunneledPort, pTunnelSetup->eSupplier);
             pPortBufSupplier->eBufferSupplier = pTunnelSetup->eSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE *)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE *)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pVideoDecData->mInputPortTunnelFlag = TRUE;
    }

COMP_CMD_FAIL:
    return eError;
}

#if 0
ERRORTYPE VideoDecRequstBuffer(COMP_HANDLETYPE hComponent, unsigned int nPortIndex, COMP_BUFFERHEADERTYPE *pBuffer)
{
    alogw("Be careful! old method, should not use now.");
    ERRORTYPE eError = SUCCESS;

    // Get component private data
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    EncodedStream *pDmxOutBuf = (EncodedStream *)pBuffer->pOutputPortPrivate;
    
    // Check Video Decoder
    if (NULL == pVideoDecData->pCedarV) 
    {
        alogw("vdecLib is not create, can't request buffer.");
        return ERR_VDEC_SYS_NOTREADY;
    }
    
    //alogd("requestBuf len[%d]", pDmxOutBuf->nTobeFillLen);
    if (nPortIndex == pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].nPortIndex) 
    {// inport tunnel mode
        int nStreamBufIndex = (VIDEO_TYPE_MINOR == pDmxOutBuf->video_stream_type) ? 1 : 0;
        int ret = RequestVideoStreamBuffer(pVideoDecData->pCedarV, pDmxOutBuf->nTobeFillLen, (char **)&pDmxOutBuf->pBuffer,
                                       (int *)&pDmxOutBuf->nBufferLen, (char **)&pDmxOutBuf->pBufferExtra,
                                       (int *)&pDmxOutBuf->nBufferExtraLen, nStreamBufIndex);
        //alogd("requestBuf ret[0x%x], len[%d+%d]", ret, pDmxOutBuf->nBufferLen, pDmxOutBuf->nBufferExtraLen);
        if (ret < 0) 
        {
            eError = ERR_VDEC_BUF_FULL;
        }
    } 
    else 
    {// inport non-tunnel mode
        aloge("fatal error! wrong portIndex[%d]", nPortIndex);
        eError = ERR_VDEC_ILLEGAL_PARAM;
    }

    return eError;
}

/** 
 * update vbs data to VdecLib, used in tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent vdec component.
 * @param nPortIndex indicate vbs inputPort index.
 * @param pBuffer contain vbs data struct.
 */
ERRORTYPE VideoDecReleaseBuffer(COMP_HANDLETYPE hComponent, unsigned int nPortIndex, COMP_BUFFERHEADERTYPE *pBuffer)
{
    alogw("Be careful! old method, should not use now.");
    VIDEODECDATATYPE *pVideoDecData;
    ERRORTYPE eError = SUCCESS;
    int ret;

    pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    EncodedStream *pDmxOutBuf = (EncodedStream *)pBuffer->pOutputPortPrivate;
    
    //alogd("releaseBuf len[%d]", pDmxOutBuf->nFilledLen);
    if (nPortIndex == pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].nPortIndex) 
    {// inport tunnel mode
        pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
        VideoStreamDataInfo dataInfo;
        memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
        
        //if demux detect stream info change.
        if (pDmxOutBuf->infoVersion != pVideoDecData->mVideoInfoVersion) 
        {
            if (pDmxOutBuf->pChangedStreamsInfo) 
            {
                struct VideoInfo *pVideoinfo = (struct VideoInfo *)pDmxOutBuf->pChangedStreamsInfo;
                if (pVideoinfo->videoNum != 1) 
                {
                    aloge("fatal error! the videoNum is not 1");
                    abort();
                }
                VDStreamInfo *pVDStreamInfo = VideoDecAddChangedStreamInfos(pVideoDecData, pVideoinfo);
                dataInfo.pVideoInfo = (void *)&pVDStreamInfo->mStreamInfo;
                dataInfo.bVideoInfoFlag = 1;
            }
            pVideoDecData->mVideoInfoVersion = pDmxOutBuf->infoVersion;
        }
        dataInfo.pData = (char *)pDmxOutBuf->pBuffer;
        dataInfo.nLength = pDmxOutBuf->nFilledLen;
        if (pDmxOutBuf->nFlags & CEDARV_FLAG_PTS_VALID)
        {
            dataInfo.nPts = pDmxOutBuf->nTimeStamp;
        } 
        else 
        {
            dataInfo.nPts = -1;
        }
        dataInfo.nPcr = -1;
        if (pDmxOutBuf->nFlags & CEDARV_FLAG_FIRST_PART)
        {
            dataInfo.bIsFirstPart = 1;
        }
        if (pDmxOutBuf->nFlags & CEDARV_FLAG_LAST_PART)
        {
            dataInfo.bIsLastPart = 1;
            alogv("last part found!");
        }
        if (pDmxOutBuf->video_stream_type == VIDEO_TYPE_MINOR) //CDX_VIDEO_STREAM_MINOR
        {
            dataInfo.nStreamIndex = 1;
        } 
        else 
        {
            dataInfo.nStreamIndex = 0;
        }
        
        // Send Vbs data to Decoder Lib
        SubmitVideoStreamData(pVideoDecData->pCedarV, &dataInfo, dataInfo.nStreamIndex);
        
        if (pVideoDecData->mWaitVbsInputFlag) 
        {
            pVideoDecData->mWaitVbsInputFlag = FALSE;
            message_t msg;
            msg.command = VDecComp_VbsAvailable;
            put_message(&pVideoDecData->cmd_queue, &msg);
        }
        
        pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
    } 
    else 
    {// inport non-tunnel mode
        aloge("fatal error! wrong portIndex[%d]", nPortIndex);
        eError = ERR_VDEC_ILLEGAL_PARAM;
    }

    return eError;
}
#endif

ERRORTYPE VideoDecTunnel_EmptyThisBuffer_BufferSupplyInput(COMP_HANDLETYPE hComponent, COMP_BUFFERHEADERTYPE *pBuffer)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    VIDEODEC_INPUT_DATA* pInputData = pVideoDecData->pInputData;
    // Get buffer pointer from demux component
    EncodedStream* pDmxOutBuf = (EncodedStream*)pBuffer->pOutputPortPrivate;
    ERRORTYPE eError = SUCCESS;
    DMXPKT_NODE_T* pEntry = NULL;

    // Check it is the same buffer in mUsingAbsList
    pthread_mutex_lock(&pInputData->mVbsListLock);
    
    if (list_empty(&pInputData->mUsingVbsList))
    {
        pthread_mutex_unlock(&pInputData->mVbsListLock);
        aloge("fatal error! Calling EmptyThisBuffer while mUsingVbsList is empty!");
        return FAILURE;
    }
    
    pEntry = list_first_entry(&pInputData->mUsingVbsList, DMXPKT_NODE_T, mList);
    if (  (pEntry->stEncodedStream.pBuffer != pDmxOutBuf->pBuffer)
       || (pEntry->stEncodedStream.pBufferExtra != pDmxOutBuf->pBufferExtra)
       )
    {
       pthread_mutex_unlock(&pInputData->mVbsListLock);
       aloge("fatal error! the buffer in EmptyThisBuffer param is not same as in mUsingVbsList");
       return FAILURE;
    }
    
    if (-1 == pDmxOutBuf->nFilledLen)
    {// buffer is not enough for component A
        
        alogv("vdec lib buffer is not enough for component A (nTobeFillLen = %d)", pDmxOutBuf->nTobeFillLen);
        pInputData->nRequestLen = pDmxOutBuf->nTobeFillLen;

        // Move node from mUsingAbsList to mIdleAbsList
        if (!list_empty(&pInputData->mUsingVbsList))
        {// moving from mReadyAbsList to mIdleAbsList
            pEntry = list_first_entry(&pInputData->mUsingVbsList, DMXPKT_NODE_T, mList);
            memset(&pEntry->stEncodedStream, 0, sizeof(EncodedStream));
            list_move_tail(&pEntry->mList, &pInputData->mIdleVbsList);
        }
            
        // Send Abs Valid message to input thread
        if (pInputData->mWaitVbsValidFlag)
        {
            pInputData->mWaitVbsValidFlag = FALSE;
            message_t msg;
            msg.command = VDecComp_VbsAvailable;
            put_message(&pInputData->cmd_queue, &msg);
        }
        // signal when all vbs return to idleList
        if(pInputData->mbWaitVbsFullFlag)
        {
            struct list_head *pList;
            int cnt = 0;
            list_for_each(pList, &pInputData->mIdleVbsList) { cnt++; }
            if (cnt >= pInputData->mNodeNum) 
            {
                pInputData->mbWaitVbsFullFlag = FALSE;
                pthread_cond_signal(&pInputData->mVbsFullCond);
            }
        }
        pthread_mutex_unlock(&pInputData->mVbsListLock);
        return eError;
    }
    
    // Prepare datainfo for decode lib
    VideoStreamDataInfo dataInfo;
    memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
    if(-1 == pVideoDecData->mVideoInfoVersion)
    {
        pVideoDecData->mVideoInfoVersion = pDmxOutBuf->infoVersion;
        alogd("video info init version is [%d]!", pVideoDecData->mVideoInfoVersion);
    }
    if (pDmxOutBuf->infoVersion != pVideoDecData->mVideoInfoVersion) 
    {
        alogd("Be careful! demux detect video info version change [%d]->[%d]!", pDmxOutBuf->infoVersion, pVideoDecData->mVideoInfoVersion);
        if (pDmxOutBuf->pChangedStreamsInfo) 
        {
            struct VideoInfo *pVideoinfo = (struct VideoInfo *)pDmxOutBuf->pChangedStreamsInfo;
            if (pVideoinfo->videoNum != 1) 
            {
                aloge("fatal error! the videoNum is not 1");
                abort();
            }
            VDStreamInfo *pVDStreamInfo = VideoDecAddChangedStreamInfos(pVideoDecData, pVideoinfo);
            dataInfo.pVideoInfo = (void *)&pVDStreamInfo->mStreamInfo;
            dataInfo.bVideoInfoFlag = 1;
        }
        pVideoDecData->mVideoInfoVersion = pDmxOutBuf->infoVersion;
    }
    dataInfo.pData = (char*)pDmxOutBuf->pBuffer;
    dataInfo.nLength = pDmxOutBuf->nFilledLen;
    if (pDmxOutBuf->nFlags & CEDARV_FLAG_PTS_VALID)
    {
        dataInfo.nPts = pDmxOutBuf->nTimeStamp;
    } 
    else 
    {
        dataInfo.nPts = -1;
    }
    dataInfo.nPcr = -1;
    if (pDmxOutBuf->nFlags & CEDARV_FLAG_FIRST_PART)
    {
        dataInfo.bIsFirstPart = 1;
    }
    if (pDmxOutBuf->nFlags & CEDARV_FLAG_LAST_PART)
    {
        dataInfo.bIsLastPart = 1;
        alogv("last part found!");
    }
    if (pDmxOutBuf->video_stream_type == VIDEO_TYPE_MINOR)  //CDX_VIDEO_STREAM_MINOR
    {
        dataInfo.nStreamIndex = 1;
    } 
    else 
    {
        dataInfo.nStreamIndex = 0;
    }
    
    // Send Vbs data to Decoder Lib
    pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
    int vdecRet = SubmitVideoStreamData(pVideoDecData->pCedarV, &dataInfo, dataInfo.nStreamIndex);
    pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
    if (0 == vdecRet)
    {// Send buf to decode lib success
        eError = SUCCESS;
        if (!list_empty(&pInputData->mUsingVbsList))
        {
            // Move node from mUsingVbsList to mIdleVbsList
            pEntry = list_first_entry(&pInputData->mUsingVbsList, DMXPKT_NODE_T, mList);
            list_move_tail(&pEntry->mList, &pInputData->mIdleVbsList);
            
            // Send Vbs Valid message to input thread
            if (pInputData->mWaitVbsValidFlag)
            {
                pInputData->mWaitVbsValidFlag = FALSE;
                message_t msg;
                msg.command = VDecComp_VbsAvailable;
                put_message(&pInputData->cmd_queue, &msg);
            }
            // signal when all vbs return to idleList
            if(pInputData->mbWaitVbsFullFlag)
            {
                struct list_head *pList;
                int cnt = 0;
                list_for_each(pList, &pInputData->mIdleVbsList) { cnt++; }
                if (cnt >= pInputData->mNodeNum) 
                {
                    pInputData->mbWaitVbsFullFlag = FALSE;
                    pthread_cond_signal(&pInputData->mVbsFullCond);
                }
            }
            pthread_mutex_unlock(&pInputData->mVbsListLock);
            
            // Send Vbs Valid message to work thread
            if (pVideoDecData->mWaitVbsInputFlag) 
            {
                pVideoDecData->mWaitVbsInputFlag = FALSE;
                message_t msg;
                msg.command = VDecComp_VbsAvailable;
                put_message(&pVideoDecData->cmd_queue, &msg);
            }
        }
        else
        {
            pthread_mutex_unlock(&pInputData->mVbsListLock);
            aloge("fatal error! No Using Vbs node while calling EmptyThisBuffer!");
            return FAILURE;
        }
    }
    else
    {
        aloge("fatal error! submit data fail, check code!");
        pthread_mutex_unlock(&pInputData->mVbsListLock);
        eError = FAILURE;
    }
    return eError;
}


ERRORTYPE VideoDecTunnel_EmptyThisBuffer_BufferSupplyOutput(COMP_HANDLETYPE hComponent, COMP_BUFFERHEADERTYPE *pBuffer)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    VIDEODEC_INPUT_DATA* pInputData = pVideoDecData->pInputData;
    // Get buffer pointer from demux component
    EncodedStream* pStream = (EncodedStream*)pBuffer->pOutputPortPrivate;
    ERRORTYPE eError = SUCCESS;
    DMXPKT_NODE_T* pEntry = NULL;

    if(pStream->pBufferExtra!=NULL || pStream->nBufferExtraLen!=0)
    {
        aloge("fatal error! we only process pBuffer! BufferExtra[%p][%d] will be ignore!", pStream->pBufferExtra, pStream->nBufferExtraLen);
    }
    // Check it is the same buffer in mUsingAbsList
    pthread_mutex_lock(&pInputData->mVbsListLock);

    //make sure there is an idle node.
    if(list_empty(&pInputData->mIdleVbsList))
    {
        alogw("Be careful! not enough idle node, malloc more!");
        DMXPKT_NODE_T *pNode = (DMXPKT_NODE_T*)malloc(sizeof(DMXPKT_NODE_T));
        if(pNode != NULL)
        {
            memset(pNode, 0, sizeof(DMXPKT_NODE_T));
            list_add_tail(&pNode->mList, &pInputData->mIdleVbsList);
            pInputData->mNodeNum++;
        }
        else
        {
            aloge("fatal error! malloc fail!");
            pthread_mutex_unlock(&pInputData->mVbsListLock);
            return ERR_VDEC_NOMEM;
        }
    }
    
    //put to readylist
    pEntry = list_first_entry(&pInputData->mIdleVbsList, DMXPKT_NODE_T, mList);
    pEntry->stEncodedStream = *pStream;
    list_move_tail(&pEntry->mList, &pInputData->mReadyVbsList);
    pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
    if (pVideoDecData->mWaitVbsInputFlag)
    {
        pVideoDecData->mWaitVbsInputFlag = FALSE;
        message_t msg;
        msg.command = VDecComp_VbsAvailable;
        put_message(&pVideoDecData->cmd_queue, &msg);
    }
    pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
    pthread_mutex_unlock(&pInputData->mVbsListLock);
    return eError;
}


ERRORTYPE VideoDecNonTunnel_EmptyThisBuffer(COMP_HANDLETYPE hComponent, COMP_BUFFERHEADERTYPE *pBuffer)
{
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    /*---------- Non-tunnel mode ----------*/
    // Get Vbs Data
    VdecInputStream *pInputStream = (VdecInputStream *)pBuffer->pAppPrivate;
    VDEC_STREAM_S *pStream = pInputStream->pStream;
    int nMilliSec = pInputStream->nMilliSec;
    if (0 == pStream->mLen && pStream->mbEndOfStream) 
    {
        alogd("indicate EndOfStream");
        VideoDecSetStreamEof(hComponent);
        goto ERROR;
    }
    
    pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
    
    //send stream to vdeclib.
    EncodedStream DmxOutBuf;
    memset(&DmxOutBuf, 0, sizeof(EncodedStream));
    
_TryToRequestVbs:
    DmxOutBuf.nTobeFillLen = pStream->mLen;
    int ret = RequestVideoStreamBuffer(pVideoDecData->pCedarV, DmxOutBuf.nTobeFillLen, (char **)&DmxOutBuf.pBuffer,
                                       (int *)&DmxOutBuf.nBufferLen, (char **)&DmxOutBuf.pBufferExtra,
                                       (int *)&DmxOutBuf.nBufferExtraLen, 0);
    if (0 == ret) 
    {// request buffer success

        // Copy Vbs data to ringbuffer
        if (pStream->mLen > DmxOutBuf.nBufferLen) 
        {
            memcpy(DmxOutBuf.pBuffer, pStream->pAddr, DmxOutBuf.nBufferLen);
            memcpy(DmxOutBuf.pBufferExtra, pStream->pAddr + DmxOutBuf.nBufferLen, pStream->mLen - DmxOutBuf.nBufferLen);
        } 
        else 
        {
            memcpy(DmxOutBuf.pBuffer, pStream->pAddr, pStream->mLen);
        }

        // Prepare Vbs data for video Decoder lib
        VideoStreamDataInfo dataInfo;
        memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
        dataInfo.pData        = (char *)DmxOutBuf.pBuffer;
        dataInfo.nLength      = pStream->mLen;
        dataInfo.nPts         = pStream->mPTS;
        dataInfo.nPcr         = -1;
        dataInfo.bIsFirstPart = 1;
        if (pStream->mbEndOfFrame) 
        {
            dataInfo.bIsLastPart = 1;
        } 
        else 
        {
            alogw("Be careful! maybe one frame divided to several parts");
            dataInfo.bIsLastPart = 0;
        }
        dataInfo.nStreamIndex = 0;

        // Send Vbs data to Decoder lib
        SubmitVideoStreamData(pVideoDecData->pCedarV, &dataInfo, dataInfo.nStreamIndex);
        if (pVideoDecData->mWaitVbsInputFlag)
        {
            pVideoDecData->mWaitVbsInputFlag = FALSE;
            message_t msg;
            msg.command = VDecComp_VbsAvailable;
            put_message(&pVideoDecData->cmd_queue, &msg);
        }
        
        if (pStream->mbEndOfStream) 
        {
            alogd("indicate EndOfStream");
            VideoDecSetStreamEof(hComponent);
        }
        eError = SUCCESS;
    }
    else 
    {// request buffer failed, wait timeout
        if (0 == nMilliSec) 
        {
            eError = ERR_VDEC_BUF_FULL;
        } 
        else if (nMilliSec < 0) 
        {
            pVideoDecData->mWaitEmptyVbsFlag = TRUE;
            pVideoDecData->mRequestVbsLen = pStream->mLen;
            pthread_cond_wait(&pVideoDecData->mEmptyVbsCondition, &pVideoDecData->mVbsInputMutex);
            pVideoDecData->mWaitEmptyVbsFlag = FALSE;
            goto _TryToRequestVbs;
        } 
        else 
        {
            pVideoDecData->mWaitEmptyVbsFlag = TRUE;
            pVideoDecData->mRequestVbsLen = pStream->mLen;
            int waitRet = pthread_cond_wait_timeout(&pVideoDecData->mEmptyVbsCondition, &pVideoDecData->mVbsInputMutex, nMilliSec);
            if (ETIMEDOUT == waitRet)
            {
                alogv("wait empty vbs buffer timeout[%d]ms, ret[%d]", nMilliSec, waitRet);
                eError = ERR_VDEC_BUF_FULL;
                pVideoDecData->mWaitEmptyVbsFlag = FALSE;
            } 
            else if (0 == waitRet) 
            {
                pVideoDecData->mWaitEmptyVbsFlag = FALSE;
                goto _TryToRequestVbs;
            } 
            else 
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", waitRet);
                eError = ERR_VDEC_BUF_FULL;
                pVideoDecData->mWaitEmptyVbsFlag = FALSE;
            }
        }
    }
    
    pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);

ERROR:
    return eError;
}

/** 
 * send stream to vdeclib, can used in non-tunnel mode.
 *
 * @return SUCCESS
 * @param hComponent vdecComp handle.
 * @param pBuffer stream info.
 */
ERRORTYPE VideoDecEmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    DMXPKT_NODE_T* pEntry = NULL;
    
    // get component private data
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    VIDEODEC_INPUT_DATA* pInputData = pVideoDecData->pInputData;

    pthread_mutex_lock(&pVideoDecData->mStateLock);

    if (pVideoDecData->state != COMP_StateExecuting && pVideoDecData->state != COMP_StatePause) 
    {
        alogw("send stream when vdec state[0x%x] is not executing/pause", pVideoDecData->state);
    }
    if(NULL == pVideoDecData->pCedarV)
    {
        alogw("Be careful! vdeclib is not create! can't send data!");
        pthread_mutex_unlock(&pVideoDecData->mStateLock);
        return ERR_VDEC_NOT_PERM;
    }
    if (pVideoDecData->mInputPortTunnelFlag) 
    {// inport tunnel mode
        // check port index
        if (pBuffer->nInputPortIndex == pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_DEMUX].nPortIndex)
        {
            COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufferSupplier = &pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX];
            if(COMP_BufferSupplyInput == pPortBufferSupplier->eBufferSupplier)
            {
                eError = VideoDecTunnel_EmptyThisBuffer_BufferSupplyInput(hComponent, pBuffer);
            }
            else if(COMP_BufferSupplyOutput == pPortBufferSupplier->eBufferSupplier)
            {
                eError = VideoDecTunnel_EmptyThisBuffer_BufferSupplyOutput(hComponent, pBuffer);
            }
            else
            {
                aloge("fatal error! wrong bufferSupplier[0x%x]!", pPortBufferSupplier->eBufferSupplier);
                eError = FAILURE;
            }
        }
        else
        {
            aloge("fatal error! PortIndex[%u]!=[%u]!", pBuffer->nInputPortIndex, pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_DEMUX].nPortIndex);
            eError = FAILURE;
        }
    }
    else
    {
        eError = VideoDecNonTunnel_EmptyThisBuffer(hComponent, pBuffer);
    }
    pthread_mutex_unlock(&pVideoDecData->mStateLock);
    return eError;
}

/** 
 * return frame to vdeclib in tunnel mode.
 *
 * @return SUCCESS
 * @param hComponent vdecComp handle.
 * @param pBuffer frame buffer info.
 */
ERRORTYPE VideoDecFillThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE *pBuffer)
{
    VIDEODECDATATYPE *pVideoDecData;
    ERRORTYPE eError = SUCCESS;
    int ret;

    pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    if (pBuffer->nOutputPortIndex == pVideoDecData->sOutPortDef.nPortIndex) 
    {
        VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S*)pBuffer->pOutputPortPrivate;
        eError = VideoDecTunnel_ReturnVDecCompOutputFrame(pVideoDecData, pFrameInfo->mId);
    } 
    else 
    {
        aloge("fatal error! outPortIndex[%d]!=[%d]", pBuffer->nOutputPortIndex, pVideoDecData->sOutPortDef.nPortIndex);
    }

ERROR:
    return eError;
}

//ERRORTYPE VideoDecQueryBufferState(COMP_HANDLETYPE hComponent, unsigned int nPortIndex, COMP_BUFFERSTATE* pBufferState)
//{
//  VIDEODECDATATYPE* pVideoDecData;
//  ERRORTYPE     eError;
//
//  eError = SUCCESS;
//
//  pVideoDecData = (VIDEODECDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
//
//  if(nPortIndex == pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].nPortIndex)
//  {
//      cedarv_quality_t  vq;
//      VideoDecoder* pCedarV;
//
//      pCedarV = pVideoDecData->pCedarV;
//      pCedarV->video_stream_type = pBufferState->video_stream_type;
//      pCedarV->query_quality(pCedarV,&vq);
//      pBufferState->nElementCounter   = vq.frame_num_in_vbv;
//      pBufferState->nValidSizePercent = vq.vbv_buffer_usage;
//
//        int nStreamBufIndex;
//        if(pBufferState->video_stream_type == VIDEO_TYPE_MINOR) //CDX_VIDEO_STREAM_MINOR
//        {
//            nStreamBufIndex = 1;
//        }
//        else
//        {
//            nStreamBufIndex = 0;
//        }
//        int nVbsBufferSize = VideoStreamBufferSize(pCedarV, nStreamBufIndex);
//        int nVbsDataSize = VideoStreamDataSize(pCedarV, nStreamBufIndex);
//        int nVbsFrameNum = VideoStreamFrameNum(pCedarV, nStreamBufIndex);
//        pBufferState->nElementCounter = nVbsFrameNum;
//        pBufferState->nValidSizePercent = nVbsDataSize*100/nVbsBufferSize;
//  }
//
//  return eError;
//}

/*****************************************************************************/
ERRORTYPE VideoDecComponentDeInit(COMP_HANDLETYPE hComponent)
{
    VIDEODECDATATYPE *pVideoDecData;
    VideoDecoder *pCedarV;
    ERRORTYPE eError;
    CompInternalMsgType eCmd;
    message_t msg;

    eError = SUCCESS;
    eCmd = Stop;

    pVideoDecData = (VIDEODECDATATYPE *)(((MM_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    msg.command = eCmd;
    put_message(&pVideoDecData->cmd_queue, &msg);

    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pVideoDecData->thread_id, (void*)&eError);
    VideoDec_InputDataDestroy(pVideoDecData->pInputData);

    pCedarV = pVideoDecData->pCedarV;
    if (pCedarV) 
    {
        DestroyVideoDecoder(pCedarV);
        pCedarV = NULL;
    }

    message_destroy(&pVideoDecData->cmd_queue);

    pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
    if (!list_empty(&pVideoDecData->mUsedOutFrameList)) {
        aloge("fatal error! outUsedFrame must be 0!");
    }
    if (!list_empty(&pVideoDecData->mReadyOutFrameList)) {
        aloge("fatal error! outReadyFrame must be 0!");
    }
    int nodeNum = 0;
    if (!list_empty(&pVideoDecData->mIdleOutFrameList)) {
        VDecCompOutputFrame *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoDecData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            nodeNum++;
        }
    }
    if (nodeNum != pVideoDecData->mFrameNodeNum) 
    {
        aloge("fatal error! frame node number is not match[%d][%d]", nodeNum, pVideoDecData->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
    //if VideoDec CompFrameBufferThread exist, stop it
    if(pVideoDecData->mCompFrameBufferThreadId != 0)
    {
        msg.command = Stop;
        put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
        pthread_join(pVideoDecData->mCompFrameBufferThreadId, (void *)&eError);
        pVideoDecData->mCompFrameBufferThreadId = 0;
    }
    message_destroy(&pVideoDecData->mCompFrameBufferThreadMessageQueue);
    if (!list_empty(&pVideoDecData->mCompUsedOutFrameList))
    {
        aloge("fatal error! CompUsedOutFrame must be 0!");
        list_splice_tail_init(&pVideoDecData->mCompUsedOutFrameList, &pVideoDecData->mCompIdleOutFrameList);
    }
    if (!list_empty(&pVideoDecData->mCompReadyOutFrameList))
    {
        aloge("fatal error! CompReadyOutFrame must be 0!");
        list_splice_tail_init(&pVideoDecData->mCompReadyOutFrameList, &pVideoDecData->mCompIdleOutFrameList);
    }
    if(!list_empty(&pVideoDecData->mCompIdleOutFrameList))
    {
        nodeNum = 0;
        VDecCompOutputFrame *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoDecData->mCompIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            CompFrameDestroy(pEntry);
            free(pEntry);
            nodeNum++;
        }
    }
    if (nodeNum != VDEC_COMP_FRAME_COUNT)
    {
        aloge("fatal error! frame node number is not match[%d][%d]", nodeNum, VDEC_COMP_FRAME_COUNT);
    }
    
    pthread_cond_destroy(&pVideoDecData->mOutFrameFullCondition);
    pthread_cond_destroy(&pVideoDecData->mReadyFrameCondition);
    pthread_cond_destroy(&pVideoDecData->mEmptyVbsCondition);
    pthread_cond_destroy(&pVideoDecData->mCompFBThreadOutFrameFullCondition);
    pthread_cond_destroy(&pVideoDecData->mCompReadyFrameCondition);
    pthread_cond_destroy(&pVideoDecData->mCondCompFBThreadState);
    pthread_mutex_destroy(&pVideoDecData->mOutFrameListMutex);
    pthread_mutex_destroy(&pVideoDecData->mVbsInputMutex);
    pthread_mutex_destroy(&pVideoDecData->mCompOutFramesLock);
    pthread_mutex_destroy(&pVideoDecData->mCompFBThreadStateLock);
    pthread_mutex_destroy(&pVideoDecData->mStateLock);
    pthread_mutex_destroy(&pVideoDecData->mDecodeFramesControlLock);
    if (!list_empty(&pVideoDecData->mANWBuffersList) || !list_empty(&pVideoDecData->mPreviousANWBuffersList)) {
        aloge("fatal error! why ANWBuffers is not empty?");
    }
    if (pVideoDecData->mIonFd >= 0) {
        close(pVideoDecData->mIonFd);
        pVideoDecData->mIonFd = -1;
    }
    if (!list_empty(&pVideoDecData->mChangedStreamList)) {
        alogw("Low probability! changedStream is not processed done by vdeclib.");
        VideoDecDestroyChangedStreamInfos(pVideoDecData);
    }
    if(pVideoDecData->mG2DHandle >= 0)
    {
        close(pVideoDecData->mG2DHandle);
        pVideoDecData->mG2DHandle = -1;
    }
#if VIDEO_DEC_TIME_DEBUG
    //print statistics info
    alogd("DecFrameCount[%d], timeDuration[%lld]us, averageDecDuration[%lld]us", pVideoDecData->mDecodeFrameCount, pVideoDecData->mTotalDecodeDuration, pVideoDecData->mTotalDecodeDuration/pVideoDecData->mDecodeFrameCount);
#endif

    if (pVideoDecData->pInputData)
    {
        free(pVideoDecData->pInputData);
    }
    if (pVideoDecData->stVideoStreamInfo.pCodecSpecificData)
    {
        free(pVideoDecData->stVideoStreamInfo.pCodecSpecificData);
    }
    if (pVideoDecData) 
    {
        free(pVideoDecData);
    }

    alogd("VideoDec component exited!");
    
    return eError;
}

/*****************************************************************************/
ERRORTYPE VideoDecComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    VIDEODECDATATYPE *pVideoDecData;
    ERRORTYPE eError;
    unsigned int err;
    int i;

    eError = SUCCESS;

    pComp = (MM_COMPONENTTYPE *)hComponent;

    // Create private data
    pVideoDecData = (VIDEODECDATATYPE *)malloc(sizeof(VIDEODECDATATYPE));
    memset(pVideoDecData, 0x0, sizeof(VIDEODECDATATYPE));
    pComp->pComponentPrivate = (void *)pVideoDecData;
    pVideoDecData->state = COMP_StateLoaded;
    pthread_mutex_init(&pVideoDecData->mStateLock, NULL);
    
    pVideoDecData->hSelf = hComponent;
    pVideoDecData->mVideoInfoVersion = -1;
    pVideoDecData->mG2DHandle = -1;

    INIT_LIST_HEAD(&pVideoDecData->mChangedStreamList);
    INIT_LIST_HEAD(&pVideoDecData->mIdleOutFrameList);
    INIT_LIST_HEAD(&pVideoDecData->mReadyOutFrameList);
    INIT_LIST_HEAD(&pVideoDecData->mUsedOutFrameList);
    INIT_LIST_HEAD(&pVideoDecData->mCompIdleOutFrameList);
    INIT_LIST_HEAD(&pVideoDecData->mCompReadyOutFrameList);
    INIT_LIST_HEAD(&pVideoDecData->mCompUsedOutFrameList);
    for (i = 0; i < VDEC_FRAME_COUNT; i++) {
        VDecCompOutputFrame *pNode = (VDecCompOutputFrame *)malloc(sizeof(VDecCompOutputFrame));
        if (NULL == pNode) {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        //memset(pNode, 0, sizeof(VDecCompOutputFrame));
        list_add_tail(&pNode->mList, &pVideoDecData->mIdleOutFrameList);
        pVideoDecData->mFrameNodeNum++;
    }
    for (i = 0; i < VDEC_COMP_FRAME_COUNT; i++) 
    {
        VDecCompOutputFrame *pNode = (VDecCompOutputFrame *)malloc(sizeof(VDecCompOutputFrame));
        if (NULL == pNode) 
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        memset(pNode, 0, sizeof(VDecCompOutputFrame));
        list_add_tail(&pNode->mList, &pVideoDecData->mCompIdleOutFrameList);
    }
    INIT_LIST_HEAD(&pVideoDecData->mANWBuffersList);
    INIT_LIST_HEAD(&pVideoDecData->mPreviousANWBuffersList);
    pVideoDecData->mIonFd = -1;

    
    err = pthread_mutex_init(&pVideoDecData->mDecodeFramesControlLock, NULL);
    if (err != 0) 
    {
        aloge("pthread mutex init fail!");
        eError = ERR_VDEC_SYS_NOTREADY;
        goto EXIT0;
    }
    pVideoDecData->mCurDecodeFramesNum = 0;
    pVideoDecData->mLimitedDecodeFramesFlag = FALSE;
    pVideoDecData->mDecodeFramesParam.mDecodeFrameNum = 0;
    
    err = pthread_mutex_init(&pVideoDecData->mVbsInputMutex, NULL);
    if (err != 0) {
        aloge("pthread mutex init fail!");
        eError = ERR_VDEC_SYS_NOTREADY;
        goto EXIT0;
    }
    err = pthread_mutex_init(&pVideoDecData->mOutFrameListMutex, NULL);
    if (err != 0) {
        aloge("pthread mutex init fail!");
        eError = ERR_VDEC_SYS_NOTREADY;
        goto EXIT1;
    }
    err = pthread_mutex_init(&pVideoDecData->mCompOutFramesLock, NULL);
    if (err != 0) 
    {
        aloge("pthread mutex init fail!");
    }
    err = pthread_mutex_init(&pVideoDecData->mCompFBThreadStateLock, NULL);
    if (err != 0) 
    {
        aloge("pthread mutex init fail!");
    }
        
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    err = pthread_cond_init(&pVideoDecData->mOutFrameFullCondition, &condAttr);
    if (err != 0) {
        aloge("pthread cond init fail!");
        eError = ERR_VDEC_SYS_NOTREADY;
        goto EXIT2;
    }
    pthread_cond_init(&pVideoDecData->mReadyFrameCondition, &condAttr);
    pthread_cond_init(&pVideoDecData->mEmptyVbsCondition, &condAttr);
    pthread_cond_init(&pVideoDecData->mCompFBThreadOutFrameFullCondition, &condAttr);
    pthread_cond_init(&pVideoDecData->mCompReadyFrameCondition, &condAttr);
    pthread_cond_init(&pVideoDecData->mCondCompFBThreadState, &condAttr);
    // Fill in function pointers
    pComp->SetCallbacks = VideoDecSetCallbacks;
    pComp->SendCommand = VideoDecSendCommand;
    pComp->GetConfig = VideoDecGetConfig;
    pComp->SetConfig = VideoDecSetConfig;
    pComp->GetState = VideoDecGetState;
    pComp->ComponentTunnelRequest = VideoDecComponentTunnelRequest;
    pComp->ComponentDeInit = VideoDecComponentDeInit;
    pComp->EmptyThisBuffer = VideoDecEmptyThisBuffer;
    pComp->FillThisBuffer = VideoDecFillThisBuffer;
    // Initialize component data structures to default values
    pVideoDecData->sPortParam.nPorts = 0x2;
    pVideoDecData->sPortParam.nStartPortNumber = 0x0;

    // Initialize the video parameters for input port
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].nPortIndex =
    pVideoDecData->sInPortExtraDef[VDEC_PORT_SUFFIX_DEMUX].nPortIndex = VDEC_PORT_INDEX_DEMUX;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].bEnabled = TRUE;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].eDomain = COMP_PortDomainVideo;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].format.video.cMIMEType = "MPEG2";
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].format.video.nFrameWidth = 176;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].format.video.nFrameHeight = 144;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].eDir = COMP_DirInput;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX].format.video.eCompressionFormat = PT_MPEG2VIDEO;

    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK].nPortIndex =
    pVideoDecData->sInPortExtraDef[VDEC_PORT_SUFFIX_CLOCK].nPortIndex = VDEC_PORT_INDEX_CLOCK;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK].bEnabled = TRUE;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK].eDomain = COMP_PortDomainOther;
    pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_CLOCK].eDir = COMP_DirInput;

    // Initialize the video parameters for output port
    pVideoDecData->sOutPortDef.nPortIndex = VDEC_OUT_PORT_INDEX_VRENDER;
    pVideoDecData->sOutPortDef.bEnabled = TRUE;
    pVideoDecData->sOutPortDef.eDomain = COMP_PortDomainVideo;
    pVideoDecData->sOutPortDef.format.video.cMIMEType = "YUV420";
    pVideoDecData->sOutPortDef.format.video.nFrameWidth = 176;
    pVideoDecData->sOutPortDef.format.video.nFrameHeight = 144;
    pVideoDecData->sOutPortDef.eDir = COMP_DirOutput;
    pVideoDecData->sOutPortDef.format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;

    pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX].nPortIndex = VDEC_PORT_INDEX_DEMUX;
    pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX].eBufferSupplier = COMP_BufferSupplyInput;
    pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_CLOCK].nPortIndex = VDEC_PORT_INDEX_CLOCK;
    pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_CLOCK].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoDecData->sPortBufSupplier[2].nPortIndex = VDEC_OUT_PORT_INDEX_VRENDER;
    pVideoDecData->sPortBufSupplier[2].eBufferSupplier = COMP_BufferSupplyOutput;

    pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_DEMUX].nPortIndex = VDEC_PORT_INDEX_DEMUX;
    pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_DEMUX].eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_CLOCK].nPortIndex = VDEC_PORT_INDEX_CLOCK;
    pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_CLOCK].eTunnelType = TUNNEL_TYPE_CLOCK;
    pVideoDecData->sOutPortTunnelInfo.nPortIndex = VDEC_OUT_PORT_INDEX_VRENDER;
    pVideoDecData->sOutPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;

    if (message_create(&pVideoDecData->cmd_queue) < 0) 
    {
        aloge("message error!");
        eError = ERR_VDEC_NOMEM;
        goto EXIT3;
    }
    if (message_create(&pVideoDecData->mCompFrameBufferThreadMessageQueue) < 0)
    {
        aloge("fatal error! msg queue init fail!");
    }
    pVideoDecData->mMemOps = MemAdapterGetOpsS();

    // Create input thread data
    VIDEODEC_INPUT_DATA* pInputData = (VIDEODEC_INPUT_DATA*) malloc(sizeof(VIDEODEC_INPUT_DATA));
    if (pInputData == NULL)
    {
        aloge("create input data error!");
        eError = ERR_VDEC_NOMEM;
        goto EXIT4;
    }
    pVideoDecData->pInputData = pInputData;    
    // Create the component thread
    err = pthread_create(&pVideoDecData->thread_id, NULL, ComponentThread, pVideoDecData);
    if (err || !pVideoDecData->thread_id) {
        eError = ERR_VDEC_NOMEM;
        goto EXIT5;
    }

    if((eError = VideoDec_InputDataInit(pInputData, pVideoDecData)) != SUCCESS)
    {
        aloge("fatal error! VideoDec InputData init fail");
        goto EXIT5;
    }
    pthread_condattr_destroy(&condAttr);
    return eError;

EXIT5:
    free(pInputData);
    pVideoDecData->pInputData = NULL;
EXIT4:
    message_destroy(&pVideoDecData->cmd_queue);
    message_destroy(&pVideoDecData->mCompFrameBufferThreadMessageQueue);
EXIT3:
    pthread_cond_destroy(&pVideoDecData->mOutFrameFullCondition);
EXIT2:
    pthread_mutex_destroy(&pVideoDecData->mOutFrameListMutex);
EXIT1:
    pthread_mutex_destroy(&pVideoDecData->mVbsInputMutex);
EXIT0:
    return eError;
}

static ERRORTYPE ReturnAllVideoStreams_Tunnel_BufferSupplyOutput(VIDEODECDATATYPE* pVideoDecData)
{
    VIDEODEC_INPUT_DATA* pInputData = pVideoDecData->pInputData;
    COMP_PARAM_PORTDEFINITIONTYPE *pPortDef            = &pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX];
    COMP_INTERNAL_TUNNELINFOTYPE  *pPortTunnelInfo     = &pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_DEMUX];
    COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufferSupplier = &pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX];

    int num = 0;
    pthread_mutex_lock(&pInputData->mVbsListLock);
    if(!list_empty(&pInputData->mReadyVbsList))
    {
        ERRORTYPE eError;
        COMP_BUFFERHEADERTYPE obh;
        memset(&obh, 0, sizeof(COMP_BUFFERHEADERTYPE));
        DMXPKT_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mReadyVbsList, mList)
        {
            obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
            obh.nInputPortIndex    = pPortDef->nPortIndex;
            obh.nOutputPortIndex   = pPortTunnelInfo->nTunnelPortIndex;
            eError = COMP_FillThisBuffer(pPortTunnelInfo->hTunnel, &obh);
            if(SUCCESS == eError)
            {
                list_move_tail(&pEntry->mList, &pInputData->mIdleVbsList);
                num++;
            }
            else
            {
                aloge("fatal error! why return encoded stream fail[0x%x]?", eError);
            }
        }
    }
    if(num > 0)
    {
        alogd("VideoDec input thread, Tunnel_BufferSupplyOutput: return [%d]frames when state[%d] turning to idle!", num, pInputData->state);
    }
    pthread_mutex_unlock(&pInputData->mVbsListLock);
    return SUCCESS;
}

static ERRORTYPE SendAllVideoStreamsToVencLib_Tunnel_BufferSupplyOutput(VIDEODECDATATYPE *pVideoDecData)
{
    VIDEODEC_INPUT_DATA* pInputData = pVideoDecData->pInputData;
    COMP_PARAM_PORTDEFINITIONTYPE *pPortDef            = &pVideoDecData->sInPortDef[VDEC_PORT_SUFFIX_DEMUX];
    COMP_INTERNAL_TUNNELINFOTYPE  *pPortTunnelInfo     = &pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_DEMUX];
    COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufferSupplier = &pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX];

    pthread_mutex_lock(&pInputData->mVbsListLock);
    if(!list_empty(&pInputData->mReadyVbsList))
    {
        ERRORTYPE eError;
        COMP_BUFFERHEADERTYPE obh;
        memset(&obh, 0, sizeof(obh));
        DMXPKT_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pInputData->mReadyVbsList, mList)
        {
            pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
            //send stream to vdeclib.
            EncodedStream DmxOutBuf;
            memset(&DmxOutBuf, 0, sizeof(EncodedStream));
            EncodedStream *pStream = &pEntry->stEncodedStream;
            DmxOutBuf.nTobeFillLen = pStream->nFilledLen;
            int ret = RequestVideoStreamBuffer(pVideoDecData->pCedarV, DmxOutBuf.nTobeFillLen, (char **)&DmxOutBuf.pBuffer,
                                               (int *)&DmxOutBuf.nBufferLen, (char **)&DmxOutBuf.pBufferExtra,
                                               (int *)&DmxOutBuf.nBufferExtraLen, 0);
            if (0 == ret) 
            {// request buffer success
                // Copy Vbs data to ringbuffer
                if (pStream->nFilledLen > DmxOutBuf.nBufferLen) 
                {
                    memcpy(DmxOutBuf.pBuffer, pStream->pBuffer, DmxOutBuf.nBufferLen);
                    memcpy(DmxOutBuf.pBufferExtra, pStream->pBuffer + DmxOutBuf.nBufferLen, pStream->nFilledLen - DmxOutBuf.nBufferLen);
                } 
                else 
                {
                    memcpy(DmxOutBuf.pBuffer, pStream->pBuffer, pStream->nFilledLen);
                }

                // Prepare Vbs data for video Decoder lib
                VideoStreamDataInfo dataInfo;
                memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
                if(-1 == pVideoDecData->mVideoInfoVersion)
                {
                    pVideoDecData->mVideoInfoVersion = pStream->infoVersion;
                    alogd("video info init version is [%d]!", pVideoDecData->mVideoInfoVersion);
                }
                if (pStream->infoVersion != pVideoDecData->mVideoInfoVersion) 
                {
                    alogd("Be careful! demux detect video info version change [%d]->[%d]!", pStream->infoVersion, pVideoDecData->mVideoInfoVersion);
                    if (pStream->pChangedStreamsInfo) 
                    {
                        struct VideoInfo *pVideoinfo = (struct VideoInfo *)pStream->pChangedStreamsInfo;
                        if (pVideoinfo->videoNum != 1) 
                        {
                            aloge("fatal error! the videoNum is not 1");
                            abort();
                        }
                        VDStreamInfo *pVDStreamInfo = VideoDecAddChangedStreamInfos(pVideoDecData, pVideoinfo);
                        dataInfo.pVideoInfo = (void *)&pVDStreamInfo->mStreamInfo;
                        dataInfo.bVideoInfoFlag = 1;
                    }
                    pVideoDecData->mVideoInfoVersion = pStream->infoVersion;
                }
                dataInfo.pData = (char*)DmxOutBuf.pBuffer;
                dataInfo.nLength = pStream->nFilledLen;
                if (pStream->nFlags & CEDARV_FLAG_PTS_VALID)
                {
                    dataInfo.nPts = pStream->nTimeStamp;
                } 
                else 
                {
                    dataInfo.nPts = -1;
                }
                //alogd("input encoded frame pts [%lld]us!", dataInfo.nPts);
                dataInfo.nPcr = -1;
                if (pStream->nFlags & CEDARV_FLAG_FIRST_PART)
                {
                    dataInfo.bIsFirstPart = 1;
                }
                if (pStream->nFlags & CEDARV_FLAG_LAST_PART)
                {
                    dataInfo.bIsLastPart = 1;
                    alogv("last part found!");
                }
                if (pStream->video_stream_type == VIDEO_TYPE_MINOR)  //CDX_VIDEO_STREAM_MINOR
                {
                    dataInfo.nStreamIndex = 1;
                } 
                else 
                {
                    dataInfo.nStreamIndex = 0;
                }

                // Send Vbs data to Decoder lib
                int submitRet = SubmitVideoStreamData(pVideoDecData->pCedarV, &dataInfo, dataInfo.nStreamIndex);
                if(submitRet != 0)
                {
                    aloge("fatal error! check vdeclib, ret[%d]", submitRet);
                }
                pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
                //return video stream
                obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
                obh.nInputPortIndex    = pPortDef->nPortIndex;
                obh.nOutputPortIndex   = pPortTunnelInfo->nTunnelPortIndex;
                eError = COMP_FillThisBuffer(pPortTunnelInfo->hTunnel, &obh);
                if(eError != SUCCESS)
                {
                    aloge("fatal error! why return video stream fail[0x%x]?", eError);
                }
                list_move_tail(&pEntry->mList, &pInputData->mIdleVbsList);
            }
            else 
            {// request buffer failed, maybe vbv buffer is full.
                alogd("VdecChn[%d] request videoStreambuffer failed! maybe vbv is full, stop sending videoStream, start to decode.", pVideoDecData->mMppChnInfo.mChnId);
                pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
                break;
            }
        }
    }
    pthread_mutex_unlock(&pInputData->mVbsListLock);

    return SUCCESS;
}

/*
 *  Component Thread 
 *    The ComponentThread function is exeuted in a separate pThread and
 *    is used to implement the actual component functions. 
 */
/*****************************************************************************/
static void *ComponentThread(void *pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    message_t cmd_msg;
    
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)pThreadData;
    VIDEODEC_INPUT_DATA* pInputData = pVideoDecData->pInputData;
    
    alogv("VideoDecoder ComponentThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"MPP_VDec", 0, 0, 0);

    while (1) 
    {
PROCESS_MESSAGE:
        if (get_message(&pVideoDecData->cmd_queue, &cmd_msg) == 0) 
        {// pump message from message queue
            
            cmd     = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;

            // State transition command
            if (cmd == SetState) 
            {
                pthread_mutex_lock(&pVideoDecData->mStateLock);
                if (pVideoDecData->state == (COMP_STATETYPE)(cmddata))
                {// If the parameter states a transition to the same state, raise a same state transition error.
                    pVideoDecData->pCallbacks->EventHandler
                        (pVideoDecData->hSelf
                        ,pVideoDecData->pAppData
                        ,COMP_EventError
                        ,ERR_VDEC_SAMESTATE
                        ,pVideoDecData->state
                        ,NULL
                        );
                }
                else 
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE)(cmddata)) 
                    {
                        case COMP_StateInvalid:
                            pVideoDecData->state = COMP_StateInvalid;
                            pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                    COMP_EventError, ERR_VDEC_INVALIDSTATE, 0, NULL);
                            pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                    COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                    pVideoDecData->state, NULL);
                            break;

                        case COMP_StateLoaded: 
                        {
                            if (pVideoDecData->state != COMP_StateIdle) 
                            {
                                aloge("fatal error! VideoDec incorrect state transition [0x%x]->Loaded!", pVideoDecData->state);
                                pVideoDecData->pCallbacks->EventHandler
                                    (pVideoDecData->hSelf
                                    ,pVideoDecData->pAppData
                                    ,COMP_EventError
                                    ,ERR_VDEC_INCORRECT_STATE_TRANSITION
                                    ,0
                                    ,NULL
                                    );
                            }
                            
                            if (pVideoDecData->mbUseCompFrame)
                            {
                                // Send message to FrameBufferThread
                                message_t msg;
                                msg.command = SetState;
                                msg.para0   = COMP_StateLoaded;
                                put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
                                
                                // Sync FrameBuffer state
                                pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                                while (pVideoDecData->mCompFBThreadState != COMP_StateLoaded)
                                {
                                    pthread_cond_wait(&pVideoDecData->mCondCompFBThreadState, &pVideoDecData->mCompFBThreadStateLock);
                                }
                                pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                            }
                            
                            // wait all outFrame return.
                            struct list_head *pList;
                            alogd("wait VDec idleOutFrameList full");
                            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                            pVideoDecData->mWaitOutFrameFullFlag = TRUE;
                            while (1) 
                            {
                                int cnt = 0;
                                list_for_each(pList, &pVideoDecData->mIdleOutFrameList) { cnt++; }
                                list_for_each(pList, &pVideoDecData->mReadyOutFrameList) { cnt++; }
                                if (cnt < pVideoDecData->mFrameNodeNum) 
                                {
                                    alogd("wait idleOutFrameList [%d]nodes to home", pVideoDecData->mFrameNodeNum - cnt);
                                    pthread_cond_wait(&pVideoDecData->mOutFrameFullCondition, &pVideoDecData->mOutFrameListMutex);
                                } 
                                else 
                                {
                                    break;
                                }
                            }
                            pVideoDecData->mWaitOutFrameFullFlag = FALSE;
                            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                            alogd("wait VDec idleOutFrameList full done");
                            
                            //free ion_user_handle_t
                            VideoDecDestroyVDANWBufferList(pVideoDecData, &pVideoDecData->mPreviousANWBuffersList);
                            VideoDecDestroyVDANWBufferList(pVideoDecData, &pVideoDecData->mANWBuffersList);

                            //if inputPort is tunnel_BufferSupplyOutput, then return all input Video Stream.
                            if(pVideoDecData->mInputPortTunnelFlag)
                            {
                                if(pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX].eBufferSupplier == COMP_BufferSupplyOutput)
                                {
                                    ReturnAllVideoStreams_Tunnel_BufferSupplyOutput(pVideoDecData);
                                }
                            }
                            // send message to input_thread
                            message_t InputThreadMsg;
                            InputThreadMsg.command = SetState;
                            InputThreadMsg.para0   = COMP_StateLoaded;
                            put_message(&pInputData->cmd_queue, &InputThreadMsg);

                            // Sync input thread state
                            pthread_mutex_lock(&pInputData->mStateLock);
                            while (pInputData->state != COMP_StateLoaded)
                            {
                                pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                            }
                            pthread_mutex_unlock(&pInputData->mStateLock);
                            
                            pVideoDecData->state = COMP_StateLoaded;
                            pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                    COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                    pVideoDecData->state, NULL);
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if (pVideoDecData->state == COMP_StateInvalid)
                            {
                                pVideoDecData->pCallbacks->EventHandler
                                    (pVideoDecData->hSelf
                                    ,pVideoDecData->pAppData
                                    ,COMP_EventError
                                    ,ERR_VDEC_INCORRECT_STATE_TRANSITION
                                    ,0
                                    ,NULL
                                    );
                            }
                            
                            else if (pVideoDecData->state == COMP_StateLoaded) 
                            {
                                pVideoDecData->state = COMP_StateIdle;
                                pVideoDecData->pCallbacks->EventHandler(
                                        pVideoDecData->hSelf, pVideoDecData->pAppData, COMP_EventCmdComplete,
                                        COMP_CommandStateSet, pVideoDecData->state, NULL);
                            }
                            else 
                            {
                                alogd("VideoDec state[0x%x]->Idle!", pVideoDecData->state);
                                if (pVideoDecData->mbUseCompFrame)
                                {
                                    // Send message to FrameBufferThread
                                    message_t msg;
                                    msg.command = SetState;
                                    msg.para0   = COMP_StateIdle;
                                    put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
                                    
                                    // Sync FrameBufferThread state
                                    pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                                    while (pVideoDecData->mCompFBThreadState != COMP_StateIdle)
                                    {
                                        pthread_cond_wait(&pVideoDecData->mCondCompFBThreadState, &pVideoDecData->mCompFBThreadStateLock);
                                    }
                                    pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                                }

                                if(pVideoDecData->mInputPortTunnelFlag)
                                {
                                    if(pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX].eBufferSupplier == COMP_BufferSupplyOutput)
                                    {
                                        ReturnAllVideoStreams_Tunnel_BufferSupplyOutput(pVideoDecData);
                                    }
                                }
                                pVideoDecData->state = COMP_StateIdle;
                                pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                        pVideoDecData->state, NULL);
                            }

                            // send message to input_thread
                            message_t InputThreadMsg;
                            InputThreadMsg.command = SetState;
                            InputThreadMsg.para0   = COMP_StateIdle;
                            put_message(&pInputData->cmd_queue, &InputThreadMsg);

                            // Sync input thread state
                            pthread_mutex_lock(&pInputData->mStateLock);
                            while (pInputData->state != COMP_StateIdle)
                            {
                                pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                            }
                            pthread_mutex_unlock(&pInputData->mStateLock);
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pVideoDecData->state == COMP_StateIdle || pVideoDecData->state == COMP_StatePause) 
                            {
                                // Do some Init work when trans from Idle to Exec
                                if (pVideoDecData->state == COMP_StateIdle) 
                                {
                                    // Initial Video Decoder
                                    if (NULL == pVideoDecData->pCedarV) 
                                    {
                                        if (CedarvCodecInit(pVideoDecData) == SUCCESS)
                                        {
                                            VideoDecDecideCompFrameBufferMode(pVideoDecData);
                                        }
                                        else
                                        {
                                            aloge("fatal error! check why vdecLib init fail!"); 
                                            pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                                    COMP_EventError,
                                                                                    ERR_VDEC_INCORRECT_STATE_TRANSITION, 0, NULL); 
                                            break;
                                        }
                                    }
                                    
                                    if (pVideoDecData->mbUseCompFrame)
                                    {
                                        // Send message to FrameBufferThread
                                        message_t msg;
                                        msg.command = SetState;
                                        msg.para0   = COMP_StateExecuting;
                                        put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
                                        
                                        // Sync FrameBufferThread state
                                        pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                                        while(pVideoDecData->mCompFBThreadState != COMP_StateExecuting)
                                        {
                                            pthread_cond_wait(&pVideoDecData->mCondCompFBThreadState, &pVideoDecData->mCompFBThreadStateLock);
                                        }
                                        pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                                    }
                                }/*Idle to Exec*/
                                else if(pVideoDecData->state == COMP_StatePause)
                                {
                                    if (pVideoDecData->mbUseCompFrame)
                                    {
                                        // Send message to FrameBufferThread
                                        message_t msg;
                                        msg.command = SetState;
                                        msg.para0   = COMP_StateExecuting;
                                        put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
                                        
                                        // Sync FrameBufferThread state
                                        pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                                        while(pVideoDecData->mCompFBThreadState != COMP_StateExecuting)
                                        {
                                            pthread_cond_wait(&pVideoDecData->mCondCompFBThreadState, &pVideoDecData->mCompFBThreadStateLock);
                                        }
                                        pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                                    }
                                }
                                //hnd_clock_comp = (MM_COMPONENTTYPE *)(pVideoDecData->sInPortTunnelInfo[VDEC_PORT_SUFFIX_CLOCK].hTunnel);
                                pVideoDecData->state = COMP_StateExecuting;
                                pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                        pVideoDecData->state, NULL);

                                // send message to input_thread
                                message_t InputThreadMsg;
                                InputThreadMsg.command = SetState;
                                InputThreadMsg.para0   = COMP_StateExecuting;
                                put_message(&pInputData->cmd_queue, &InputThreadMsg);

                                // Sync input thread state
                                pthread_mutex_lock(&pInputData->mStateLock);
                                while (pInputData->state != COMP_StateExecuting)
                                {
                                    pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                                }
                                pthread_mutex_unlock(&pInputData->mStateLock); 
                            } 
                            else
                            {
                                pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                        COMP_EventError,
                                                                        ERR_VDEC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pVideoDecData->state == COMP_StateIdle || pVideoDecData->state == COMP_StateExecuting) 
                            {
                                pVideoDecData->state = COMP_StatePause;
                                pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                        COMP_EventCmdComplete, COMP_CommandStateSet,
                                                                        pVideoDecData->state, NULL);

                                if (pVideoDecData->mbUseCompFrame)
                                {
                                    // Send message to FrameBufferThread
                                    message_t msg;
                                    msg.command = SetState;
                                    msg.para0   = COMP_StatePause;
                                    put_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &msg);
                                    
                                    // Sync FrameBufferThread state
                                    pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                                    while(pVideoDecData->mCompFBThreadState != COMP_StatePause)
                                    {
                                        pthread_cond_wait(&pVideoDecData->mCondCompFBThreadState, &pVideoDecData->mCompFBThreadStateLock);
                                    }
                                    pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                                }

                                // send message to input_thread
                                message_t InputThreadMsg;
                                InputThreadMsg.command = SetState;
                                InputThreadMsg.para0   = COMP_StatePause;
                                put_message(&pInputData->cmd_queue, &InputThreadMsg);

                                // Sync input thread state
                                pthread_mutex_lock(&pInputData->mStateLock);
                                while (pInputData->state != COMP_StatePause)
                                {
                                    pthread_cond_wait(&pInputData->mStateCond, &pInputData->mStateLock);
                                }
                                pthread_mutex_unlock(&pInputData->mStateLock); 
                            } 
                            else
                            {
                                pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                                        COMP_EventError,
                                                                        ERR_VDEC_INCORRECT_STATE_TRANSITION, 0, NULL);
                            }
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&pVideoDecData->mStateLock);
            } 
            else if (cmd == Stop) 
            {
                // send message to input_thread
                message_t InputThreadMsg;
                InputThreadMsg.command = Stop;
                InputThreadMsg.para0   = 0;
                put_message(&pInputData->cmd_queue, &InputThreadMsg);
                
                // Kill thread
                goto EXIT;
            } 
            else if (cmd == VDecComp_VbsAvailable) 
            {
                alogv("Vdec vbs available");
            } 
            else if (cmd == VDecComp_OutFrameAvailable) 
            {
                alogv("Vdec frame available");
            } 
            else if (cmd == VDecComp_ChangeGraphicBufferProducer) 
            {
                //state is pause here in common
                if (pVideoDecData->state != COMP_StatePause) 
                {
                    aloge("fatal error! state[%d] is not pause", pVideoDecData->state);
                }
                alogd("receive message: change GraphicBufferProducer, displayFrameRequestMode[%d]", pVideoDecData->nDisplayFrameRequestMode);
                if (pVideoDecData->nDisplayFrameRequestMode) 
                {
                    pVideoDecData->mbChangeGraphicBufferProducer = TRUE;
                    if (!list_empty(&pVideoDecData->mPreviousANWBuffersList)) 
                    {
                        aloge("fatal error! previous GraphicBufferProducer's frames still remain!");
                    }
                    list_splice_tail_init(&pVideoDecData->mANWBuffersList, &pVideoDecData->mPreviousANWBuffersList);
                    //set vdeclib release flag, get release frames as many as possible, and free its ionUserHandle.
                    SetVideoFbmBufRelease(pVideoDecData->pCedarV);
                    VideoPicture *pReleasePicture = NULL;
                    VDANWBuffer *pVdAnb;
                    int nNeedReleaseBufferNum = 0;
                    while (1) {
                        pReleasePicture = RequestReleasePicture(pVideoDecData->pCedarV);
                        if (NULL == pReleasePicture) {
                            struct list_head *pList;
                            list_for_each(pList, &pVideoDecData->mPreviousANWBuffersList) { nNeedReleaseBufferNum++; }
                            if (nNeedReleaseBufferNum > 0) {
                                alogw("Low probability, not request all releasePictures, left [%d]frames",
                                      nNeedReleaseBufferNum);
                            }
                            break;
                        }
                        pVdAnb = searchVDANWBufferListByVideoPicture(&pVideoDecData->mPreviousANWBuffersList, pReleasePicture);
                        if (NULL == pVdAnb) {
                            aloge("fatal error! not find release VideoPicture!");
                            abort();
                        }
                        alogd("ion_free: handle_ion[%p]", pReleasePicture->pPrivate);
                        //ion_free(pVideoDecData->mIonFd, pVdAnb->mIonUserHandle);
                        struct ion_handle_data handleData = {
                            .handle = pVdAnb->mIonUserHandle,
                        };
                        ioctl(pVideoDecData->mIonFd, ION_IOC_FREE, &handleData);
                        list_del(&pVdAnb->mList);
                        free(pVdAnb);
                        pReleasePicture->pPrivate = NULL;
                    }
                    if (nNeedReleaseBufferNum <= 0) {
                        pVideoDecData->mbChangeGraphicBufferProducer = FALSE;
                    }
                }
                pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData,
                                                        COMP_EventCmdComplete, COMP_CommandVendorChangeANativeWindow, 0,
                                                        NULL);
            } 
            else if (cmd == VDecComp_ReopenVideoEngine) 
            {
                alogd("resolution change, reopen VideoEngine done.");
                pVideoDecData->mResolutionChangeFlag = FALSE;
            }
            
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (pVideoDecData->mbEof)
        {
            alogd("VDec EOF!");
            TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 0);
        }
        else if (pVideoDecData->state == COMP_StateExecuting) 
        {
            int ret;
            ERRORTYPE omxRet;
            int bStreamEOF = 0;

            // Get Video Decoder
            VideoDecoder *pCedarV = pVideoDecData->pCedarV;
            
#if VIDEO_DEC_TIME_DEBUG
            int64_t start_time = CDX_GetSysTimeUsMonotonic();
            int64_t end_time;
#endif

            // Check EOF Flag
            pthread_mutex_lock(&pVideoDecData->mDecodeFramesControlLock);
            if(pVideoDecData->mLimitedDecodeFramesFlag && pVideoDecData->mCurDecodeFramesNum >= pVideoDecData->mDecodeFramesParam.mDecodeFrameNum)
            {
                alogd("the vdec channel[%d] had decode %d frame, it is enough!",pVideoDecData->mMppChnInfo.mChnId, pVideoDecData->mCurDecodeFramesNum);
                pthread_mutex_unlock(&pVideoDecData->mDecodeFramesControlLock);
                TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 0);
                goto PROCESS_MESSAGE;
            }            
            pthread_mutex_unlock(&pVideoDecData->mDecodeFramesControlLock);
            
            if (pVideoDecData->priv_flag & CDX_comp_PRIV_FLAGS_STREAMEOF) 
            {
                bStreamEOF = 1;
            } 
            else 
            {
                bStreamEOF = 0;
            }
            
            // Decode one frame
            if (!pVideoDecData->mResolutionChangeFlag) 
            {
                // if inputPort is tunnel_BufferSupplyOutput, need send all video streams to vencLib.
                if(pVideoDecData->mInputPortTunnelFlag)
                {
                    if(pVideoDecData->sPortBufSupplier[VDEC_PORT_SUFFIX_DEMUX].eBufferSupplier == COMP_BufferSupplyOutput)
                    {
                        SendAllVideoStreamsToVencLib_Tunnel_BufferSupplyOutput(pVideoDecData);
                    }
                }
                ret = DecodeVideoStream(pCedarV, bStreamEOF /*eof*/, 0 /*key frame only*/, pVideoDecData->drop_B_frame /*drop b frame*/, 0 /*current time*/);
                //alogd("decoder ret:0x%x, priv_flag:0x%x", ret, pVideoDecData->priv_flag);
            } 
            else 
            {
                aloge("fatal error! resolution change is not process done, can't decode!");
                TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 0);
                goto PROCESS_MESSAGE;
            }

            if (ret == VDECODE_RESULT_NO_BITSTREAM) 
            {
                pVideoDecData->mNoBitstreamCounter++;
            } 
            else 
            {
                pVideoDecData->mNoBitstreamCounter = 0;
            }

            pthread_mutex_lock(&pVideoDecData->mDecodeFramesControlLock);
            if(pVideoDecData->mLimitedDecodeFramesFlag && (VDECODE_RESULT_FRAME_DECODED == ret || VDECODE_RESULT_KEYFRAME_DECODED == ret))
            {
                pVideoDecData->mCurDecodeFramesNum++;
            }
            pthread_mutex_unlock(&pVideoDecData->mDecodeFramesControlLock);
            
#if VIDEO_DEC_TIME_DEBUG
            end_time = CDX_GetSysTimeUsMonotonic();
            if (  ret == VDECODE_RESULT_OK 
               || ret == VDECODE_RESULT_CONTINUE 
               || ret == VDECODE_RESULT_FRAME_DECODED 
               || ret == VDECODE_RESULT_KEYFRAME_DECODED
               )
            {
                if (end_time - start_time > 200 * 1000)
                {
                    alogd("Be careful! vdecChn[%d], decodeType[%d] too long to [%lld]us, ret[0x%x]",
                        pVideoDecData->mMppChnInfo.mChnId, pVideoDecData->mChnAttr.mType, end_time - start_time, ret);
                }
                pVideoDecData->mTotalDecodeDuration += end_time - start_time;
                if (ret == VDECODE_RESULT_FRAME_DECODED || ret == VDECODE_RESULT_KEYFRAME_DECODED)
                {
                    pVideoDecData->mDecodeFrameCount++;
                }
            }
#endif

            if (pVideoDecData->mbChangeGraphicBufferProducer) 
            {
                alogd("vdeclib ret[%d] when changeGraphicBufferProducer!", ret);
                
                // continue get release frames as many as possible, and free its ionUserHandle.
                VDANWBuffer *pVdAnb;
                int nNeedReleaseBufferNum = 0;
                VideoPicture *pReleasePicture = NULL;
                while (1) 
                {
                    pReleasePicture = RequestReleasePicture(pVideoDecData->pCedarV);
                    if (NULL == pReleasePicture) 
                    {
                        struct list_head *pList;
                        list_for_each(pList, &pVideoDecData->mPreviousANWBuffersList) 
                        { 
                            nNeedReleaseBufferNum++; 
                        }
                        if (nNeedReleaseBufferNum > 0) 
                        {
                            alogw("Low probability, not request all releasePictures, left [%d]frames", nNeedReleaseBufferNum);
                        }
                        break;
                    }
                    pVdAnb = searchVDANWBufferListByVideoPicture(&pVideoDecData->mPreviousANWBuffersList, pReleasePicture);
                    if (NULL == pVdAnb) 
                    {
                        aloge("fatal error! not find release VideoPicture!");
                        abort();
                    }
                    alogd("ion_free: handle_ion[%p]", pReleasePicture->pPrivate);
                    //ion_free(pVideoDecData->mIonFd, pVdAnb->mIonUserHandle);
                    struct ion_handle_data handleData = {
                        .handle = pVdAnb->mIonUserHandle,
                    };
                    ioctl(pVideoDecData->mIonFd, ION_IOC_FREE, &handleData);
                    list_del(&pVdAnb->mList);
                    free(pVdAnb);
                    pReleasePicture->pPrivate = NULL;
                }
                if (nNeedReleaseBufferNum <= 0) 
                {
                    pVideoDecData->mbChangeGraphicBufferProducer = FALSE;
                }
            }

            if ((VDECODE_RESULT_KEYFRAME_DECODED == ret || VDECODE_RESULT_FRAME_DECODED == ret)) 
            {// Get one frame
                
                //alogv("cedar dec ret:%d totalframe:%d",ret,pVideoDecData->total_dec_frames++);
                
                //check if need signal emptyVbs ready.
                if (FALSE == pVideoDecData->mInputPortTunnelFlag) 
                {
                    pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
                    if (pVideoDecData->mWaitEmptyVbsFlag) 
                    {
                        int nEmptyVbsSize = VideoStreamBufferSize(pVideoDecData->pCedarV, 0) - VideoStreamDataSize(pVideoDecData->pCedarV, 0);
                        if (nEmptyVbsSize >= pVideoDecData->mRequestVbsLen) 
                        {
                            pthread_cond_signal(&pVideoDecData->mEmptyVbsCondition);
                        }
                    }
                    pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
                }

                //request frame
                int releaseRet;
                while (1) 
                {
                    pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                    if (list_empty(&pVideoDecData->mIdleOutFrameList)) 
                    {// Output buffer full, malloc new block
                        aloge("fatal error! idle out frame list is empty, wait");
                        VDecCompOutputFrame *pNewNode = malloc(sizeof(VDecCompOutputFrame));
                        if (NULL == pNewNode) 
                        {
                            aloge("fatal error! malloc fail! turn to stateInvalid!");
                            pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData, COMP_EventError, ERR_VDEC_NOMEM, 0, NULL);
                            pVideoDecData->state = COMP_StateInvalid;
                            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                            goto PROCESS_MESSAGE;
                        }
                        list_add_tail(&pNewNode->mList, &pVideoDecData->mIdleOutFrameList);
                    }
                    
                    VDecCompOutputFrame *pOutFrame = list_first_entry(&pVideoDecData->mIdleOutFrameList, VDecCompOutputFrame, mList);
                    
                    //don't support double stream now. we don't need it.
                    pOutFrame->mpPicture = RequestPicture(pCedarV, 0 /*the major stream*/);
                    if (pOutFrame->mpPicture != NULL) 
                    {
                        if(pVideoDecData->mVConfig.bSecOutputEn)
                        {
                            //and now , support double stream ,just for mjpeg no-tunnl...
                            pOutFrame->mpSubPicture = RequestPicture(pCedarV, 1 /*the sub stream*/);
                            if(NULL == pOutFrame->mpSubPicture)
                            {
                                aloge("fatal error! why the sub output can not get stream?");
                                pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                                break;
                            }
//                            alogd("main[%d,%d,%dx%d], sub[%d,%d,%dx%d]",
//                                pOutFrame->mpPicture->nLeftOffset, pOutFrame->mpPicture->nTopOffset,
//                                pOutFrame->mpPicture->nRightOffset - pOutFrame->mpPicture->nLeftOffset, 
//                                pOutFrame->mpPicture->nBottomOffset - pOutFrame->mpPicture->nTopOffset,
//                                pOutFrame->mpSubPicture->nLeftOffset, pOutFrame->mpSubPicture->nTopOffset,
//                                pOutFrame->mpSubPicture->nRightOffset - pOutFrame->mpSubPicture->nLeftOffset, 
//                                pOutFrame->mpSubPicture->nBottomOffset - pOutFrame->mpSubPicture->nTopOffset);
                        }
                        list_del(&pOutFrame->mList);
                        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                        
#ifdef __WRITEOUT_FRAMEBUFFER
                        static int pic_id = 0;
                        if (pic_id % 30 == 0) 
                        {
                            FILE *fp_yuv = fopen("/mnt/extsd/yuv.dat", "a+");
                            alogd("pict->nWidth(%d), pict->nHeight(%d)", pOutFrame->mpPicture->nWidth,
                                  pOutFrame->mpPicture->nHeight);
                            fwrite(pOutFrame->mpPicture->pData0, 1,
                                   pOutFrame->mpPicture->nWidth * pOutFrame->mpPicture->nHeight, fp_yuv);
                            fwrite(pOutFrame->mpPicture->pData1, 1,
                                   pOutFrame->mpPicture->nWidth * pOutFrame->mpPicture->nHeight / 2, fp_yuv);
                            fclose(fp_yuv);
                        }
                        pic_id++;
#endif

                        if (pVideoDecData->mOutputPortTunnelFlag) 
                        {// out port tunnel mode
                            VideoDecTunnel_SendVDecCompOutputFrame(pVideoDecData, pOutFrame);
                        } 
                        else
                        {// out port non-tunnel mode
                            VideoDecNonTunnel_ReadyVDecCompOutputFrame(pVideoDecData, pOutFrame);
                        }
                    }
                    else 
                    {
                        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                        break;
                    }
                }
            } 
            else if (ret == VDECODE_RESULT_OK || ret == VDECODE_RESULT_CONTINUE) 
            {
            } 
            else if (VDECODE_RESULT_NO_FRAME_BUFFER == ret) 
            {
                pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                int emptyFrameNum = EmptyPictureBufferNum(pCedarV, 0);
                if (emptyFrameNum <= 0) 
                {
                    pVideoDecData->mWaitOutFrameFlag = TRUE;
                    pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);

                    //if non-tunnel mode and no-rotate, then send cb msg to user, user will return frame,
                    //now, EyeseeUSBCamera need receive no frame buffer message, then return frame.
                    if (!pVideoDecData->mOutputPortTunnelFlag && !pVideoDecData->mbUseCompFrame)
                    {
                        pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData, COMP_EventMoreBuffer, 0, 0, NULL);
                    }
                    TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 0);
                } 
                else 
                {
                    //alogd("Low probability! vdecLib has empty frame[%d] after decode_no_frame!", emptyFrameNum);
                    pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                }
                goto PROCESS_MESSAGE;
            } 
            else if (VDECODE_RESULT_NO_BITSTREAM == ret) 
            {// No Vbs data
                if (pVideoDecData->priv_flag & CDX_comp_PRIV_FLAGS_STREAMEOF) 
                {
                    usleep(5 * 1000);

                    if (!pVideoDecData->exit_counter) 
                    {
                        pVideoDecData->exit_counter++;
                        continue;
                    }
                    alogd("VideoDec notify EOF");
                    pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData, COMP_EventBufferFlag, 0, 0, NULL);
                    pVideoDecData->mbEof = TRUE;
                    continue;
                }
                pthread_mutex_lock(&pVideoDecData->mVbsInputMutex);
                int streamSize = VideoStreamDataSize(pCedarV, 0);
                if (streamSize > 0 && pVideoDecData->mNoBitstreamCounter < 2) 
                {
                    //alogd("Low probability! vdecLib has bitstream[%d]!", streamSize);
                    pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
                } 
                else 
                {// wait Vbs data come
                    pVideoDecData->mWaitVbsInputFlag = TRUE;
                    pthread_mutex_unlock(&pVideoDecData->mVbsInputMutex);
                    TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 0);
                }
                goto PROCESS_MESSAGE;
            } 
            else if (ret <= VDECODE_RESULT_UNSUPPORTED) 
            {
                //* CXC, unsupported stream, may be stream format not supported or memory allocation for frame buffers fail.
                aloge("fatal error! cedarv dec error 0 ret[%d]", ret);
                pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData, COMP_EventError, ERR_VDEC_INVALIDSTATE, 0, NULL);
                pVideoDecData->state = COMP_StateInvalid;
            } 
            else if (ret == VDECODE_RESULT_RESOLUTION_CHANGE) 
            {
                alogd("detect video picture resolution change!");
                //request and send all frames to videoRender.
                pVideoDecData->mResolutionChangeFlag = TRUE;
                int releaseRet;
                while (1) 
                {
                    pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                    if (list_empty(&pVideoDecData->mIdleOutFrameList)) 
                    {
                        aloge("fatal error! idle out frame list is empty, wait");
                        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                        if (TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 200) > 0) 
                        {
                            goto PROCESS_MESSAGE;
                        } 
                        else 
                        {
                            continue;
                        }
                    }
                    VDecCompOutputFrame *pOutFrame = list_first_entry(&pVideoDecData->mIdleOutFrameList, VDecCompOutputFrame, mList);
                    //don't support double stream now. we don't need it.
                    pOutFrame->mpPicture = RequestPicture(pCedarV, 0 /*the major stream*/);
                    if (pOutFrame->mpPicture != NULL) 
                    {
                        list_del(&pOutFrame->mList);
                        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                        if (pVideoDecData->mOutputPortTunnelFlag) 
                        {
                            COMP_BUFFERHEADERTYPE obh;
                            obh.nOutputPortIndex = pVideoDecData->sOutPortTunnelInfo.nPortIndex;
                            obh.nInputPortIndex = pVideoDecData->sOutPortTunnelInfo.nTunnelPortIndex;
                            VIDEO_FRAME_INFO_S stFrameInfo;
                            config_VIDEO_FRAME_INFO_S_by_VideoPicture(&stFrameInfo, pOutFrame->mpPicture, pVideoDecData);
                            obh.pOutputPortPrivate = (void*)&stFrameInfo;
                            omxRet = COMP_EmptyThisBuffer(pVideoDecData->sOutPortTunnelInfo.hTunnel, &obh);
                            if (SUCCESS == omxRet) 
                            {
                                pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                                list_add_tail(&pOutFrame->mList, &pVideoDecData->mUsedOutFrameList);
                                pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                            } 
                            else 
                            {
                                if (ERR_VDEC_INCORRECT_STATE_OPERATION == omxRet) 
                                {
                                    alogd(
                                      "Be careful! VDec output frame fail[0x%x], maybe next component status is "
                                      "Loaded, return frame!",
                                      omxRet);
                                }
                                releaseRet = ReturnPicture(pCedarV, (VideoPicture *)pOutFrame->mpPicture);
                                if (releaseRet != 0) 
                                {
                                    aloge("fatal error! Return Picture() fail ret[%d]", ret);
                                }
                                pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                                list_add(&pOutFrame->mList, &pVideoDecData->mIdleOutFrameList);
                                pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                            }
                        } 
                        else 
                        {
                            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                            list_add_tail(&pOutFrame->mList, &pVideoDecData->mReadyOutFrameList);
                            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                        }
                    } 
                    else 
                    {
                        pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                        break;
                    }
                }
                //notify videorender resolution change.
                if (pVideoDecData->mOutputPortTunnelFlag) 
                {
                    COMP_SetConfig(pVideoDecData->sOutPortTunnelInfo.hTunnel, COMP_IndexVendorResolutionChange, NULL);
                } 
                else 
                {
                    pVideoDecData->pCallbacks->EventHandler(pVideoDecData->hSelf, pVideoDecData->pAppData, COMP_EventMultiPixelExit, 0, 0, NULL);
                }
                //wait ReopenVideoEngine().
                TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 0);
                goto PROCESS_MESSAGE;
            } 
            else 
            {
                aloge("fatal error! ret[%d]", ret);
            }
        } 
        else 
        {// not Exec state, wait message
            TMessage_WaitQueueNotEmpty(&pVideoDecData->cmd_queue, 0);
        }
    }

EXIT:
    alogv("VideoDecoder ComponentThread stopped");
    return (void *)SUCCESS;
}

void setCompVideoPictureInfo(VideoPicture *pDst, VideoPicture *pSrc)
{
//    int     nID;
    pDst->nStreamIndex = pSrc->nStreamIndex;
    pDst->ePixelFormat = pSrc->ePixelFormat;
//    int     nWidth;
//    int     nHeight;
//    int     nLineStride;
//    int     nTopOffset;
//    int     nLeftOffset;
//    int     nBottomOffset;
//    int     nRightOffset;
    pDst->nFrameRate = pSrc->nFrameRate;
    pDst->nAspectRatio = pSrc->nAspectRatio;
    pDst->bIsProgressive = pSrc->bIsProgressive;
    pDst->bTopFieldFirst = pSrc->bTopFieldFirst;
    pDst->bRepeatTopField = pSrc->bRepeatTopField;
    pDst->nPts = pSrc->nPts;
    pDst->nPcr = pSrc->nPcr;
//    char*   pData0;
//    char*   pData1;
//    char*   pData2;
//    char*   pData3;
//    int     bMafValid;
//    char*   pMafData;
//    int     nMafFlagStride;
//    int     bPreFrmValid;
//    int     nBufId;
//    size_addr phyYBufAddr;
//    size_addr phyCBufAddr;
//    void*   pPrivate;
//    int     nBufFd;
//    int     nBufStatus;
//    int     bTopFieldError;
//    int        bBottomFieldError;
//    int     nColorPrimary;
//    int     bFrameErrorFlag;

    //* to save hdr info and afbc header info
//    void*   pMetaData;

    //*display related parameter
//    VIDEO_FULL_RANGE_FLAG   video_full_range_flag;
//    VIDEO_TRANSFER          transfer_characteristics;
//    VIDEO_MATRIX_COEFFS     matrix_coeffs;
//    u8      colour_primaries;
    //*end of display related parameter defined
    //size_addr    nLower2BitPhyAddr;
//    int          nLower2BitBufSize;
//    int          nLower2BitBufOffset;
//    int          nLower2BitBufStride;
//    int          b10BitPicFlag;
//    int          bEnableAfbcFlag;
//
//    int     nBufSize;
//    int     nAfbcSize;
//    int     nDebugCount;
}

ERRORTYPE VideoDecRotateFrame(const VideoPicture *pSrc, VideoPicture *pDst, VIDEODECDATATYPE *pVideoDecData)
{
    int nRotation;  //anti-clock wise
    switch(pVideoDecData->cedarv_rotation)
    {
        case 0:
            nRotation = 0;
            break;
        case 1:
            nRotation = 90;
            break;
        case 2:
            nRotation = 180;
            break;
        case 3:
            nRotation = 270;
            break;
        default:
            aloge("fatal error! cedarv rotation[%d] is not support!", pVideoDecData->cedarv_rotation);
            nRotation = 0;
            break;
    }
  #if 0
    g2d_blt     blit_para;
    int         err;
    ERRORTYPE ret = SUCCESS;
    if (nRotation != 90 && nRotation != 180 && nRotation != 270)
    {
        aloge("fatal error! rotation[%d] is invalid!", nRotation);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    if(pVideoDecData->mG2DHandle < 0)
    {
        aloge("fatal error! g2d driver[%d] is not valid, can't rotate!", pVideoDecData->mG2DHandle);
        return ERR_VDEC_SYS_NOTREADY;
    }
    g2d_data_fmt    eSrcFormat, eDstFormat;
    g2d_pixel_seq   eSrcPixelSeq, eDstPixelSeq;
    ERRORTYPE eError;
    eError = convert_EPIXELFORMAT_to_G2dFormat((enum EPIXELFORMAT)pSrc->ePixelFormat, &eSrcFormat, &eSrcPixelSeq);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrc->ePixelFormat);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    eError = convert_EPIXELFORMAT_to_G2dFormat((enum EPIXELFORMAT)pDst->ePixelFormat, &eDstFormat, &eDstPixelSeq);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDst->ePixelFormat);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    memset(&blit_para, 0, sizeof(g2d_blt));
    blit_para.src_image.addr[0]      = pSrc->phyYBufAddr;
    blit_para.src_image.addr[1]      = pSrc->phyCBufAddr;
    blit_para.src_image.w            = pSrc->nWidth;         /* src buffer width in pixel units */
    blit_para.src_image.h            = pSrc->nHeight;        /* src buffer height in pixel units */
    blit_para.src_image.format       = eSrcFormat;
    blit_para.src_image.pixel_seq    = eSrcPixelSeq;//G2D_SEQ_VUVU;          /*  */
    blit_para.src_rect.x             = 0;                       /* src rect->x in pixel */
    blit_para.src_rect.y             = 0;                       /* src rect->y in pixel */
    blit_para.src_rect.w             = pSrc->nWidth;           /* src rect->w in pixel */
    blit_para.src_rect.h             = pSrc->nHeight;          /* src rect->h in pixel */

    blit_para.dst_image.addr[0]      = pDst->phyYBufAddr;
    blit_para.dst_image.addr[1]      = pDst->phyCBufAddr;
    blit_para.dst_image.w            = pDst->nWidth;         /* dst buffer width in pixel units */
    blit_para.dst_image.h            = pDst->nHeight;        /* dst buffer height in pixel units */
    blit_para.dst_image.format       = eDstFormat;
    blit_para.dst_image.pixel_seq    = eDstPixelSeq;
    blit_para.dst_x                  = 0;                   /* dst rect->x in pixel */
    blit_para.dst_y                  = 0;                   /* dst rect->y in pixel */
    blit_para.color                  = 0xff;                /* fix me*/
    blit_para.alpha                  = 0xff;                /* globe alpha */

    switch(nRotation)
    {
        case 90:
            blit_para.flag = G2D_BLT_ROTATE90;
            break;
        case 180:
            blit_para.flag = G2D_BLT_ROTATE180;
            break;
        case 270:
            blit_para.flag = G2D_BLT_ROTATE270;
            break;
        default:
            aloge("fatal error! rotation[%d] is invalid!", nRotation);
            blit_para.flag = G2D_BLT_NONE;
            break;
    }

    err = ioctl(pVideoDecData->mG2DHandle, G2D_CMD_BITBLT, (unsigned long)&blit_para);
    if(err < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed");
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
        ret = ERR_VDEC_SYS_NOTREADY;
    }
    return ret;

  #else
    g2d_blt_h   blit;
    int         err;
    ERRORTYPE    ret = SUCCESS;
    if (nRotation != 90 && nRotation != 180 && nRotation != 270 && nRotation != 360)
    {
        aloge("fatal error! rotation[%d] is invalid!", nRotation);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    if(pVideoDecData->mG2DHandle < 0)
    {
        aloge("fatal error! g2d driver[%d] is not valid, can't rotate!", pVideoDecData->mG2DHandle);
        return ERR_VDEC_SYS_NOTREADY;
    }
    g2d_fmt_enh eSrcFormat, eDstFormat;
    ERRORTYPE eError;
    eError = convert_EPIXELFORMAT_to_g2d_fmt_enh(pSrc->ePixelFormat, &eSrcFormat);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrc->ePixelFormat);
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    eError = convert_EPIXELFORMAT_to_g2d_fmt_enh(pDst->ePixelFormat, &eDstFormat);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDst->ePixelFormat);
        return ERR_VDEC_ILLEGAL_PARAM;
    }

    //config blit
    memset(&blit, 0, sizeof(g2d_blt_h));
    switch(nRotation)
    {
        case 90:
            blit.flag_h = G2D_ROT_90;
            break;
        case 180:
            blit.flag_h = G2D_ROT_180;
            break;
        case 270:
            blit.flag_h = G2D_ROT_270;
            break;
        default:
            aloge("fatal error! rotation[%d] is invalid!", nRotation);
            blit.flag_h = G2D_BLT_NONE_H;
            break;
    }
    blit.src_image_h.bbuff = 1;
    blit.src_image_h.use_phy_addr = 1;
    blit.src_image_h.color = 0xff;
    blit.src_image_h.format = eSrcFormat;
    blit.src_image_h.laddr[0] = pSrc->phyYBufAddr;
    blit.src_image_h.laddr[1] = pSrc->phyCBufAddr;
	if(pSrc->ePixelFormat == PIXEL_FORMAT_YUV_PLANER_420)
	{
		blit.src_image_h.laddr[2] = pSrc->phyCBufAddr+pSrc->nHeight*pSrc->nLineStride/4;
	}
	else if(pSrc->ePixelFormat == PIXEL_FORMAT_YV12)
	{
		blit.src_image_h.laddr[1] = pSrc->phyCBufAddr+pSrc->nHeight*pSrc->nLineStride/4;
		blit.src_image_h.laddr[2] = pSrc->phyCBufAddr;
	}
    blit.src_image_h.width = pSrc->nWidth;
    blit.src_image_h.height = pSrc->nHeight;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    blit.src_image_h.clip_rect.x = 0;
    blit.src_image_h.clip_rect.y = 0;
    blit.src_image_h.clip_rect.w = pSrc->nWidth;
    blit.src_image_h.clip_rect.h = pSrc->nHeight;
    blit.src_image_h.gamut = G2D_BT709;
    blit.src_image_h.bpremul = 0;
    blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_GLOBAL_ALPHA;

    blit.dst_image_h.bbuff = 1;
    blit.dst_image_h.use_phy_addr = 1;
    blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = eDstFormat;
    blit.dst_image_h.laddr[0] = pDst->phyYBufAddr;
    blit.dst_image_h.laddr[1] = pDst->phyCBufAddr;
	if(pDst->ePixelFormat == PIXEL_FORMAT_YUV_PLANER_420)
	{
		blit.dst_image_h.laddr[2] = pDst->phyCBufAddr+pDst->nHeight*pDst->nLineStride/4;
	}
	else if(pDst->ePixelFormat == PIXEL_FORMAT_YV12)
	{
		blit.dst_image_h.laddr[1] = pDst->phyCBufAddr+pDst->nHeight*pDst->nLineStride/4;
		blit.dst_image_h.laddr[2] = pDst->phyCBufAddr;
	}
    blit.dst_image_h.width = pDst->nWidth;
    blit.dst_image_h.height = pDst->nHeight;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;
    blit.dst_image_h.clip_rect.x = 0;
    blit.dst_image_h.clip_rect.y = 0;
    blit.dst_image_h.clip_rect.w = pDst->nWidth;
    blit.dst_image_h.clip_rect.h = pDst->nHeight;
    blit.dst_image_h.gamut = G2D_BT709;
    blit.dst_image_h.bpremul = 0;
    blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_GLOBAL_ALPHA;

//    blit.color = 0xff;
//    blit.alpha = 0xff;

    err = ioctl(pVideoDecData->mG2DHandle, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(err < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed");
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
        ret = ERR_VDEC_SYS_NOTREADY;
    }

    //alogd("debug g2d[0x%x]: virAddr[0x%x][0x%x], phyAddr[0x%x][0x%x], size[%dx%d]", pVideoDecData->mG2DHandle,
    //    pDst->pData0, pDst->pData1, pDst->phyYBufAddr, pDst->phyCBufAddr,
    //    pDst->nWidth, pDst->nHeight);
    //memset(pDes->VFrame.mpVirAddr[0], 0xff, pDes->VFrame.mWidth * pDes->VFrame.mHeight);
    //memset(pDes->VFrame.mpVirAddr[1], 0xff, pDes->VFrame.mWidth * pDes->VFrame.mHeight/2);
    //memcpy(pDes->VFrame.mpVirAddr[0], pSrc->VFrame.mpVirAddr[0], pDes->VFrame.mWidth * pDes->VFrame.mHeight);
    //memcpy(pDes->VFrame.mpVirAddr[1], pSrc->VFrame.mpVirAddr[1], pDes->VFrame.mWidth * pDes->VFrame.mHeight/2);
    return ret;
  #endif
}


static void *VideoDecCompFrameBufferThread(void *pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    message_t cmd_msg;
    
    VIDEODECDATATYPE *pVideoDecData = (VIDEODECDATATYPE *)pThreadData;

    alogv("VideoDecoder CompFrameBufferThread start run...");
    prctl(PR_SET_NAME, (unsigned long)"MPP_VDecCompFrameBuffer", 0, 0, 0);

    while (1) 
    {
PROCESS_MESSAGE:
        if (get_message(&pVideoDecData->mCompFrameBufferThreadMessageQueue, &cmd_msg) == 0) 
        {// pump message from message queue
            
            cmd     = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;
            
            if (cmd == SetState) 
            {// State transition command
                if (pVideoDecData->mCompFBThreadState == (COMP_STATETYPE)(cmddata))
                {
                    alogd("comp FBThread same state[0x%x]", pVideoDecData->mCompFBThreadState);
                }
                else 
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE)(cmddata)) 
                    {
                        case COMP_StateLoaded:
                        {
                            if (pVideoDecData->mCompFBThreadState != COMP_StateIdle)
                            {
                                aloge("fatal error! VDecCompFBThread incorrect state transition [0x%x]->Loaded!", pVideoDecData->mCompFBThreadState);
                            }
                            alogd("return all VDec used frames");
                            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
                            if (!list_empty(&pVideoDecData->mUsedOutFrameList))
                            {
                                int cnt = 0;
                                VDecCompOutputFrame *pEntry, *pTmp;
                                list_for_each_entry_safe(pEntry, pTmp, &pVideoDecData->mUsedOutFrameList, mList)
                                {
                                    int ret = ReturnPicture(pVideoDecData->pCedarV, (VideoPicture *)pEntry->mpPicture);
                                    if (ret != 0) 
                                    {
                                        aloge("fatal error! Return Picture() fail ret[%d]", ret);
                                    }
                                    list_move_tail(&pEntry->mList, &pVideoDecData->mIdleOutFrameList);
                                    
                                    if (pVideoDecData->mWaitOutFrameFlag) 
                                    {
                                        pVideoDecData->mWaitOutFrameFlag = FALSE;
                                        message_t msg;
                                        msg.command = VDecComp_OutFrameAvailable;
                                        put_message(&pVideoDecData->cmd_queue, &msg);
                                    }
                                    cnt++;
                                }
                                alogd("VDec CompFBThread release [%d]used frames!", cnt);
                                
                                if (pVideoDecData->mWaitOutFrameFullFlag) 
                                {
                                    int cnt = 0;
                                    struct list_head *pList;
                                    list_for_each(pList, &pVideoDecData->mIdleOutFrameList) { cnt++; }
                                    if (cnt >= pVideoDecData->mFrameNodeNum) 
                                    {
                                        pthread_cond_signal(&pVideoDecData->mOutFrameFullCondition);
                                    }
                                }
                            }
                            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                            
                            alogd("wait VDec CompIdleOutFrameList full");
                            pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
                            
                            pVideoDecData->mbCompFBThreadWaitOutFrameFull = TRUE;
                            //wait all outFrame return.
                            while (1) 
                            {
                                int cnt = 0;
                                struct list_head *pList;
                                list_for_each(pList, &pVideoDecData->mCompIdleOutFrameList) { cnt++; }
                                list_for_each(pList, &pVideoDecData->mCompReadyOutFrameList) { cnt++; }
                                if (cnt < VDEC_COMP_FRAME_COUNT)
                                {
                                    alogd("wait CompIdleOutFrameList [%d]nodes to home", VDEC_COMP_FRAME_COUNT - cnt);
                                    pthread_cond_wait(&pVideoDecData->mCompFBThreadOutFrameFullCondition, &pVideoDecData->mCompOutFramesLock);
                                } 
                                else 
                                {
                                    break;
                                }
                            }
                            pVideoDecData->mbCompFBThreadWaitOutFrameFull = FALSE;
                            pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
                            alogd("wait VDec CompIdleOutFrameList full done");

                            pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                            pVideoDecData->mCompFBThreadState = COMP_StateLoaded;
                            pthread_cond_signal(&pVideoDecData->mCondCompFBThreadState);
                            pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            alogd("VDec CompFBThread state[0x%x]->Idle!", pVideoDecData->mCompFBThreadState);
                            pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                            pVideoDecData->mCompFBThreadState = COMP_StateIdle;
                            pthread_cond_signal(&pVideoDecData->mCondCompFBThreadState);
                            pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pVideoDecData->mCompFBThreadState == COMP_StateIdle || 
                                        pVideoDecData->mCompFBThreadState == COMP_StatePause) 
                            {
                                pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                                pVideoDecData->mCompFBThreadState = COMP_StateExecuting;
                                pthread_cond_signal(&pVideoDecData->mCondCompFBThreadState);
                                pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                            } 
                            else
                            {
                                aloge("fatal error! wrong state[0x%x]->executing", pVideoDecData->mCompFBThreadState);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            if (pVideoDecData->mCompFBThreadState == COMP_StateIdle || 
                                        pVideoDecData->mCompFBThreadState == COMP_StateExecuting) 
                            {
                                pthread_mutex_lock(&pVideoDecData->mCompFBThreadStateLock);
                                pVideoDecData->mCompFBThreadState = COMP_StatePause;
                                pthread_cond_signal(&pVideoDecData->mCondCompFBThreadState);
                                pthread_mutex_unlock(&pVideoDecData->mCompFBThreadStateLock);
                            } 
                            else
                            {
                                aloge("fatal error! wrong state[0x%x]->executing", pVideoDecData->mCompFBThreadState);
                            }
                            break;
                        }
                        default:
                        {
                            aloge("fatal error! wrong dst state[0x%x]", cmddata);
                            break;
                        }
                    }
                }
            }
            else if (cmd == Stop) 
            {
                // Kill thread
                goto EXIT;
            } 
            else if (cmd == VDecComp_CompFrameBufferThread_InputFrameAvailable)
            {
                alogv("VDec CompFBThread input frame available");
            } 
            else if (cmd == VDecComp_CompFrameBufferThread_OutFrameAvailable)
            {
                alogv("VDec CompFBThread out frame available");
            } 
            //precede to process message
            goto PROCESS_MESSAGE;
        }
        
        if (!pVideoDecData->mbUseCompFrame)
        {
            aloge("fatal error! useCompFrame is false");
            TMessage_WaitQueueNotEmpty(&pVideoDecData->mCompFrameBufferThreadMessageQueue, 0);
            goto PROCESS_MESSAGE;
        }
        
        if (pVideoDecData->mCompFBThreadState == COMP_StateExecuting)
        {
            ERRORTYPE ret;
            VDecCompOutputFrame *pInputFrame;
            VDecCompOutputFrame *pOutputFrame;
            
            //confirm inputFrame exist
            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
            if (list_empty(&pVideoDecData->mUsedOutFrameList))
            {
                pVideoDecData->mbCompFBThreadWaitVdecFrameInput = TRUE;
                pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
                TMessage_WaitQueueNotEmpty(&pVideoDecData->mCompFrameBufferThreadMessageQueue, 0);
                goto PROCESS_MESSAGE;
            }
            pInputFrame = list_first_entry(&pVideoDecData->mUsedOutFrameList, VDecCompOutputFrame, mList);
            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);

            //confirm CompIdleOutFrame exist
            pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
            if (list_empty(&pVideoDecData->mCompIdleOutFrameList))
            {
                pVideoDecData->mbCompFBThreadWaitOutFrame = TRUE;
                pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
                TMessage_WaitQueueNotEmpty(&pVideoDecData->mCompFrameBufferThreadMessageQueue, 0);
                goto PROCESS_MESSAGE;
            }
            pOutputFrame = list_first_entry(&pVideoDecData->mCompIdleOutFrameList, VDecCompOutputFrame, mList);
            if (NULL == pOutputFrame->mpPicture)
            {
                alogd("need init compFrame!");
                ret = CompFrameInit(pOutputFrame, pInputFrame->mpPicture, pVideoDecData->cedarv_rotation);
                if(ret != SUCCESS)
                {
                    pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
                    goto _SelfFrameErr0;
                }
            }
            pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
            
            //rotate to output frame
            VideoDecRotateFrame(pInputFrame->mpPicture, pOutputFrame->mpPicture, pVideoDecData);
            setCompVideoPictureInfo(pOutputFrame->mpPicture, pInputFrame->mpPicture);
            
            //return input frame to vdeclib
            VideoDecReturnVDecCompOutputFrameToIdleList(pVideoDecData, pInputFrame->mpPicture->nID);
            
            //send output frame
            if (pVideoDecData->mOutputPortTunnelFlag)
            {
                pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
                list_move_tail(&pOutputFrame->mList, &pVideoDecData->mCompUsedOutFrameList);
                pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
                COMP_BUFFERHEADERTYPE obh;
                obh.nOutputPortIndex = pVideoDecData->sOutPortTunnelInfo.nPortIndex;
                obh.nInputPortIndex = pVideoDecData->sOutPortTunnelInfo.nTunnelPortIndex;
                VIDEO_FRAME_INFO_S stFrameInfo;
                config_VIDEO_FRAME_INFO_S_by_VideoPicture(&stFrameInfo, pOutputFrame->mpPicture, pVideoDecData);
                obh.pOutputPortPrivate = (void*)&stFrameInfo;
                ERRORTYPE omxRet = COMP_EmptyThisBuffer(pVideoDecData->sOutPortTunnelInfo.hTunnel, &obh);
                if (SUCCESS == omxRet) 
                {
                } 
                else 
                {
                    int commonErrCode = omxRet&0x1FFF;
                    if (EN_ERR_INCORRECT_STATE_OPERATION == commonErrCode) 
                    {
                        alogd("Be careful! VDec output frame fail[0x%x], maybe next component status is Loaded, return frame!", omxRet);
                    }
                    else if(EN_ERR_SYS_NOTREADY == commonErrCode)
                    {
                        alogv("frame is ignored.");
                    }
                    else
                    {
                        aloge("fatal error! errCode[0x%x]", omxRet);
                    }
                    pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
                    list_move(&pOutputFrame->mList, &pVideoDecData->mCompIdleOutFrameList);
                    pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
                }
            }
            else
            {
                pthread_mutex_lock(&pVideoDecData->mCompOutFramesLock);
                list_move_tail(&pOutputFrame->mList, &pVideoDecData->mCompReadyOutFrameList);
                if (pVideoDecData->mbCompWaitReadyFrame)
                {
                    pthread_cond_signal(&pVideoDecData->mCompReadyFrameCondition);
                }
                pthread_mutex_unlock(&pVideoDecData->mCompOutFramesLock);
            }
            goto PROCESS_MESSAGE;

_SelfFrameErr0:
            //return all frames
            pthread_mutex_lock(&pVideoDecData->mOutFrameListMutex);
            if(!list_empty(&pVideoDecData->mUsedOutFrameList))
            {
                VDecCompOutputFrame *pEntry;
                list_for_each_entry(pEntry, &pVideoDecData->mUsedOutFrameList, mList)
                {
                    int releaseRet = ReturnPicture(pVideoDecData->pCedarV, (VideoPicture *)pEntry->mpPicture);
                    if (releaseRet != 0)
                    {
                        aloge("fatal error! Return Picture() fail ret[%d]", releaseRet);
                    }
                }
                list_splice_tail_init(&pVideoDecData->mUsedOutFrameList, &pVideoDecData->mIdleOutFrameList);
            }
            pthread_mutex_unlock(&pVideoDecData->mOutFrameListMutex);
            goto PROCESS_MESSAGE;
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pVideoDecData->mCompFrameBufferThreadMessageQueue, 0);
        }
    }

EXIT:
    alogv("VideoDec CompFrameBufferThread stopped");
    return (void *)SUCCESS;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
