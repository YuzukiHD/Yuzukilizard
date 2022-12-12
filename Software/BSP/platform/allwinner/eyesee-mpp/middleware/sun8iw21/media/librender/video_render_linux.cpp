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

//#define LOG_NDEBUG 0
#define LOG_TAG "video_render_linux"
#include <utils/plat_log.h>

#include <stdio.h>
#include <assert.h> 
#include <string.h>
#include <CdxANativeWindowBuffer.h>
//#include <CDX_CallbackType.h>
#include <memoryAdapter.h>
#include <vdecoder.h>
#include <vencoder.h>
#include "video_render.h"
#include <CedarXNativeRenderer.h>
#include <hwdisplay.h>
#include <CDX_ErrorType.h>
#include <MetaData.h>
#include <CedarXMetaData.h>
#include <mm_comm_video.h>
#include <ConfigOption.h>

using namespace EyeseeLinux;

class VR4LData
{
public:
    CedarXRenderer *mVideoRenderer;
    bool mbFirstFrame;  //wait first frame.
    bool mbShowFlag;
    struct ScMemOpsS *mMemOps;
    VR4LData()
    {
        mVideoRenderer = NULL;
        mbFirstFrame = true;
        mbShowFlag = true;
        mMemOps = MemAdapterGetOpsS();
    };
};

CdxPixelFormat convertPixelFormatVdec2Cdx(EPIXELFORMAT format)
{
    if(format == PIXEL_FORMAT_YUV_MB32_422)
    {
        return CDX_PIXEL_FORMAT_AW_MB422;  //VIRTUAL_HWC_FORMAT_MBYUV422
    }
    else if (format == PIXEL_FORMAT_YUV_MB32_420)
    {
        return CDX_PIXEL_FORMAT_AW_MB420;  //VIRTUAL_HWC_FORMAT_MBYUV420
    }
    else if(format == PIXEL_FORMAT_YV12)
    {
        return CDX_PIXEL_FORMAT_YV12;//(int32_t)HAL_PIXEL_FORMAT_YV12; VIRTUAL_HWC_FORMAT_YUV420PLANAR
    }
    else if(format == PIXEL_FORMAT_YUV_PLANER_420)
    {
        return CDX_PIXEL_FORMAT_AW_YUV_PLANNER420;
    }
    else if(format == PIXEL_FORMAT_NV21)
    {
        return CDX_PIXEL_FORMAT_YCrCb_420_SP;//(int32_t)HAL_PIXEL_FORMAT_YV12; VIRTUAL_HWC_FORMAT_YUV420VUC
    }
    else if(format == PIXEL_FORMAT_NV12)
    {
        return CDX_PIXEL_FORMAT_AW_NV12;
    }
    else if(format == PIXEL_FORMAT_PLANARUV_422)
    {
        return CDX_PIXEL_FORMAT_YCbCr_422_SP;
    }
    else if(format == PIXEL_FORMAT_PLANARVU_422)
    {
        return CDX_PIXEL_FORMAT_AW_NV61;
    }
    else if(format == PIXEL_FORMAT_YUYV)
    {
        return CDX_PIXEL_FORMAT_YCbCr_422_I;
    }
    else if(format == PIXEL_FORMAT_ARGB)
    {
        return CDX_PIXEL_FORMAT_AW_ARGB_8888;
    }
    else
    {
        alogw("fatal error! format=0x%x", format);
        return CDX_PIXEL_FORMAT_AW_MB420;    //* format should be CEDARV_PIXEL_FORMAT_MB_UV_COMBINED_YUV420, VIRTUAL_HWC_FORMAT_MBYUV420
    }
}

int config_libhwclayerpara_t(libhwclayerpara_t *pHwcPara, VIDEO_FRAME_INFO_S *pVPic, struct ScMemOpsS *pMemOps)
{
    memset(pHwcPara, 0, sizeof(libhwclayerpara_t));
    //ALOGD("(f:%s, l:%d) printVideoPicture:[%d][%p][%p][%p][%p]", __FUNCTION__, __LINE__, pVPic->ePixelFormat, pVPic->pData0, pVPic->pData1, pVPic->pData2, pVPic->pData3);
    pHwcPara->number = pVPic->mId;
//    pHwcPara->top_y  = (unsigned long)(intptr_t)CdcMemGetPhysicAddressCpu(pMemOps, pVPic->pData0);
//    pHwcPara->top_c  = (unsigned long)(intptr_t)CdcMemGetPhysicAddressCpu(pMemOps, pVPic->pData1);
//    if(pVPic->pData2)
//    {
//        pHwcPara->bottom_y = (unsigned long)(intptr_t)CdcMemGetPhysicAddressCpu(pMemOps, pVPic->pData2);
//    }
//    if(pVPic->pData3)
//    {
//        pHwcPara->bottom_c = (unsigned long)(intptr_t)CdcMemGetPhysicAddressCpu(pMemOps, pVPic->pData2);
//    }
    pHwcPara->top_y = pVPic->VFrame.mPhyAddr[0];
    pHwcPara->top_c = pVPic->VFrame.mPhyAddr[1];
    pHwcPara->bottom_y = pVPic->VFrame.mPhyAddr[2];
    if (VIDEO_FIELD_FRAME == pVPic->VFrame.mField) 
    {
        pHwcPara->bProgressiveSrc = 1;
    } 
    else 
    {
        pHwcPara->bProgressiveSrc = 0;
    }
    pHwcPara->bTopFieldFirst = 1;
#if 1 //deinterlace support
//    pHwcPara->flag_addr              = (unsigned long)(intptr_t)pVPic->pMafData;
//    pHwcPara->flag_stride            = pVPic->nMafFlagStride;
//    pHwcPara->maf_valid              = pVPic->bMafValid;
//    pHwcPara->pre_frame_valid        = pVPic->bPreFrmValid;
    pHwcPara->flag_addr              = 0;
    pHwcPara->flag_stride            = 0;
    pHwcPara->maf_valid              = 0;
    pHwcPara->pre_frame_valid        = 0;
#endif
    return 0;
}

static int vr4l_init(struct CDX_VideoRenderHAL *handle, int nRenderMode, void *pIn, void *pOut)
{
    if(handle->mPriv != NULL)
    {
        aloge("fatal error! CDX_VideoRenderHAL->mPriv[%p] is not null!", handle->mPriv);
        delete (VR4LData*)handle->mPriv;
        handle->mPriv = NULL;
    }
    VR4LData *pVR4LData = new VR4LData;
    handle->mPriv = (void*)pVR4LData;

    int nDisplayTopX = 0;
    int nDisplayTopY = 0;
    int nDisplayWidth;
    int nDisplayHeight;
    int nFrameBufWidth;
    int nFrameBufHeight;
    int nFrameCount = -1;
    int nGUIInitRotation = 0;
    CdxPixelFormat nDisplayFormat; //CDX_PIXEL_FORMAT_YV12
    VENC_COLOR_SPACE nColorSpace = VENC_BT601;
    VO_LAYER nVideoLayerId = -1;
    //int nVideoScalingMode;
    if(nRenderMode == VideoRender_HW)
    {
        CdxVRHwcLayerInitPara   *pHwInitPara = (CdxVRHwcLayerInitPara*)pIn;
        nDisplayTopX    = pHwInitPara->mnDisplayTopX;
        nDisplayTopY    = pHwInitPara->mnDisplayTopY;
        nDisplayWidth   = pHwInitPara->mnDisplayWidth;
        nDisplayHeight  = pHwInitPara->mnDisplayHeight;
        nFrameBufWidth  = pHwInitPara->mnBufWidth;
        nFrameBufHeight = pHwInitPara->mnBufHeight;
        nDisplayFormat  = convertPixelFormatVdec2Cdx((EPIXELFORMAT)pHwInitPara->mePixelFormat);
        nColorSpace = (VENC_COLOR_SPACE)pHwInitPara->mColorSpace;
        nVideoLayerId = pHwInitPara->mVoLayer;
    }
    else if(nRenderMode == VideoRender_SW)
    {
        CdxVRSWANWInitPara   *pSwInitPara = (CdxVRSWANWInitPara*)pIn;
        nDisplayWidth   = pSwInitPara->mnDisplayWidth;
        nDisplayHeight  = pSwInitPara->mnDisplayHeight;
        nFrameBufWidth  = pSwInitPara->mnBufWidth;
        nFrameBufHeight = pSwInitPara->mnBufHeight;
        nDisplayFormat  = convertPixelFormatVdec2Cdx((EPIXELFORMAT)pSwInitPara->mePixelFormat);
    }
    else if(nRenderMode == VideoRender_GUI)
    {
        CdxVRANWInitPara *pAnwInitPara = (CdxVRANWInitPara*)pIn;
        nDisplayWidth = -1;
        nDisplayHeight = -1;
        nFrameBufWidth  = pAnwInitPara->mnBufWidth;
        nFrameBufHeight = pAnwInitPara->mnBufHeight;
        nFrameCount = pAnwInitPara->mnBufNum;
        nDisplayFormat = convertPixelFormatVdec2Cdx((EPIXELFORMAT)pAnwInitPara->mePixelFormat);
    }
    else
    {
        aloge("fatal error! what render mode[%d]?", nRenderMode);
        return CDX_ERROR;
    }

    MetaData *meta = new MetaData;
    meta->setInt32(kKeyColorFormat, nDisplayFormat);
    meta->setInt32(kKeyWidth, nFrameBufWidth);
    meta->setInt32(kKeyHeight, nFrameBufHeight);
    meta->setInt32(kCedarXKeyDisplayTopX, nDisplayTopX);
    meta->setInt32(kCedarXKeyDisplayTopY, nDisplayTopY);
    meta->setInt32(kKeyDisplayWidth, nDisplayWidth);
    meta->setInt32(kKeyDisplayHeight, nDisplayHeight);
    meta->setInt32(kKeyRotation, nGUIInitRotation);
    //meta->setInt32(kKeyIsDRM, mIsDRMMedia);
    meta->setInt32(kCedarXKeyFrameCount, nFrameCount);
    meta->setInt32(kCedarXKeyColorSpace, nColorSpace);
    meta->setInt32(kCedarXKeyVideoLayer, nVideoLayerId);
    if(nRenderMode == VideoRender_HW)//HWC_FORMAT_YUV420PLANAR, HAL_PIXEL_FORMAT_YV12)
    {
#if (CDXCFG_HW_DISPLAY == OPTION_HW_DISPLAY_ENABLE)
        alogd("mDisplayFormat[0x%x], new CedarXNativeRenderer", nDisplayFormat);
        pVR4LData->mVideoRenderer = new CedarXNativeRenderer(nVideoLayerId, meta);
#else
        aloge("fatal error! renderMode is HW, why CDXCFG HW DISPLAY is not OPTION_HW_DISPLAY_ENABLE?");
        assert(0);
#endif
        
    }
    else if(nRenderMode == VideoRender_SW)
    {
#if (CDXCFG_HW_DISPLAY == OPTION_HW_DISPLAY_DISABLE)
        alogd("mDisplayFormat[0x%x], new CedarXSoftwareRenderer", nDisplayFormat);
        pVR4LData->mVideoRenderer = new CedarXSoftwareRenderer(mNativeWindow, meta);
#else
        aloge("fatal error! renderMode is SW, why CDXCFG HW DISPLAY is not OPTION_HW_DISPLAY_DISABLE?");
        assert(0);
#endif
        
    }
    else if(nRenderMode == VideoRender_GUI)
    {
#if (CDXCFG_HW_DISPLAY == OPTION_HW_DISPLAY_DISABLE)
        alogd("mDisplayFormat[0x%x], new CedarXNativeWindowRenderer", mDisplayFormat);
        pVR4LData->mVideoRenderer = new CedarXNativeWindowRenderer(mNativeWindow, meta, mIonFd);
        pVR4LData->mVideoRenderer->control(VIDEORENDER_CMD_GET_ANATIVEWINDOWBUFFERS, 0, pOut);
#else
        aloge("fatal error! renderMode is GUI, why CDXCFG HW DISPLAY is not OPTION_HW_DISPLAY_DISABLE?");
        assert(0);
#endif
    }
    else
    {
        aloge("fatal error! unknown renderMode[%d]", nRenderMode);
        assert(0);
    }
    //pVR4LData->mVideoRenderer->control(VIDEORENDER_CMD_SET_SCALING_MODE, nVideoScalingMode, NULL);
    return CDX_OK;
}

static void vr4l_exit(struct CDX_VideoRenderHAL *handle) 
{
    if(handle->mPriv)
    {
        VR4LData *pVR4LData = (VR4LData*)handle->mPriv;
        pVR4LData->mVideoRenderer->control(VIDEORENDER_CMD_SHOW, 0, NULL);
        delete pVR4LData->mVideoRenderer;
        pVR4LData->mVideoRenderer = NULL;
        delete pVR4LData;
        handle->mPriv = NULL;
    }
    else
    {
        alogd("VideoRenderHal not init when exit.");
    }
}

static int vr4l_render(struct CDX_VideoRenderHAL *handle, void *frame_info, int frame_id)
{
    VR4LData *pVR4LData = (VR4LData*)handle->mPriv;
#if (CDXCFG_HW_DISPLAY == OPTION_HW_DISPLAY_ENABLE)
    if(pVR4LData->mVideoRenderer != NULL)
    {
        VIDEO_FRAME_INFO_S *frm_inf = (VIDEO_FRAME_INFO_S*) frame_info;
        libhwclayerpara_t hwcLayerPara;
        config_libhwclayerpara_t(&hwcLayerPara, frm_inf, pVR4LData->mMemOps);
        pVR4LData->mVideoRenderer->render(&hwcLayerPara, 0);
        if(pVR4LData->mbFirstFrame)
        {
            if(pVR4LData->mbShowFlag)
            {
                pVR4LData->mVideoRenderer->control(VIDEORENDER_CMD_SHOW, 1, NULL);
            }
            pVR4LData->mbFirstFrame = false;
        }
    }
    else
    {
        alogd("mVideoRenderer=NULL");
    }
#else
    //#error "renderMode is GUI, why call StagefrightVideoRenderData()?"
    aloge("fatal error! renderMode is GUI, why call StagefrightVideoRenderData()?");
#endif


    return 0;
}

static int vr4l_set_showflag(struct CDX_VideoRenderHAL *handle, int bShowFlag)
{
    VR4LData *pVR4LData = (VR4LData*)handle->mPriv;
    pVR4LData->mbShowFlag = bShowFlag!=0?true:false;
#if (CDXCFG_HW_DISPLAY == OPTION_HW_DISPLAY_ENABLE)
    if(pVR4LData->mVideoRenderer != NULL)
    {
        if(false == pVR4LData->mbFirstFrame)
        {
            pVR4LData->mVideoRenderer->control(VIDEORENDER_CMD_SHOW, bShowFlag, NULL);
        }
    }
    else
    {
        alogd("mVideoRenderer=NULL");
    }
#else
    //#error "renderMode is GUI, why call StagefrightVideoRenderData()?"
    aloge("fatal error! renderMode is GUI, why call StagefrightVideoRenderData()?");
#endif


    return 0;
}

static int vr4l_update_display_size(struct CDX_VideoRenderHAL *handle, void *pFrameInfo)
{
    VR4LData *pVR4LData = (VR4LData*)handle->mPriv;
#if (CDXCFG_HW_DISPLAY == OPTION_HW_DISPLAY_ENABLE)
    if(pVR4LData->mVideoRenderer != NULL)
    {
        CdxVRFrameInfo *pDisplayRect = (CdxVRFrameInfo*)pFrameInfo;
        //RECT_S crop = {pDisplayRect->mnDisplayTopX, pDisplayRect->mnDisplayTopY, (unsigned int)pDisplayRect->mnDisplayWidth, (unsigned int)pDisplayRect->mnDisplayHeight};
        pVR4LData->mVideoRenderer->control(VIDEORENDER_CMD_SET_CROP, 0, pDisplayRect);
    }
    else
    {
        alogd("mVideoRenderer=NULL");
    }
#else
    aloge("fatal error! renderMode is GUI, not implement");
#endif
    return CDX_OK;
}


//extern "C" {

CDX_VideoRenderHAL video_render_linux_hal = {
    info: "video render hal for linux",
    callback_info: NULL,
    mPriv: NULL,
    init: vr4l_init,
    exit: vr4l_exit,
    render: vr4l_render,
    get_disp_frame_id: NULL,
    set_showflag: vr4l_set_showflag,

    dequeue_frame: NULL,    //request frame from gpu
    enqueue_frame: NULL,    //release frame to gpu
    cancel_frame: NULL,
    update_display_size: vr4l_update_display_size,
};

//}

