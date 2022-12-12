/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
//#define LOG_NDEBUG 0
#define LOG_TAG "CedarXNativeRenderer"
#include <utils/plat_log.h>

#include <stdio.h>
#include <string.h>
#include <hwdisplay.h>

#include "CedarXNativeRenderer.h"
//#include <media/stagefright/foundation/ADebug.h>
#include <MetaData.h>
//#include <system/window.h>
//#include <CDX_Common.h>
#include <CedarXMetaData.h>
#include <vencoder.h>
#include <CDX_ErrorType.h>

namespace EyeseeLinux {

enum e_hwc_format convertPixelFormatCdx2Hwc(CdxPixelFormat format)
{
    if(format == CDX_PIXEL_FORMAT_AW_MB422)
    {
	    return HWC_FORMAT_MBYUV422;  //VIRTUAL_HWC_FORMAT_MBYUV422
    }
	else if (format == CDX_PIXEL_FORMAT_AW_MB420)
	{
		return HWC_FORMAT_MBYUV420;  //VIRTUAL_HWC_FORMAT_MBYUV420
	}
	else if(format == CDX_PIXEL_FORMAT_YV12)
	{
	    return HWC_FORMAT_YUV420PLANAR;//(int32_t)HAL_PIXEL_FORMAT_YV12; VIRTUAL_HWC_FORMAT_YUV420PLANAR
	}
    else if(format == CDX_PIXEL_FORMAT_AW_YUV_PLANNER420)
    {
        return HWC_FORMAT_YUV420PLANAR;
    }
	else if(format == CDX_PIXEL_FORMAT_YCrCb_420_SP)
	{
        return HWC_FORMAT_YUV420VUC;//(int32_t)HAL_PIXEL_FORMAT_YCrCb_420_SP; VIRTUAL_HWC_FORMAT_YUV420VUC
	}
    else if(format == CDX_PIXEL_FORMAT_AW_NV12)
	{
        return HWC_FORMAT_YUV420UVC;
	}
    else if(format == CDX_PIXEL_FORMAT_YCbCr_422_SP)
    {
        return HWC_FORMAT_YUV422UVC;
    }
    else if(format == CDX_PIXEL_FORMAT_AW_NV61)
    {
        return HWC_FORMAT_YUV422VUC;
    }
    else if(format == CDX_PIXEL_FORMAT_YCbCr_422_I)
    {
        return HWC_FORMAT_YCbYCr_422_I;
    }
    else if(format == CDX_PIXEL_FORMAT_AW_ARGB_8888)
    {
        return HWC_FORMAT_ARGB_8888;
    }
	else
    {
        alogw("fatal error! format=0x%x", format);
	    return HWC_FORMAT_MBYUV420;    //* format should be CEDARV_PIXEL_FORMAT_MB_UV_COMBINED_YUV420, VIRTUAL_HWC_FORMAT_MBYUV420
	}
}

static disp_color_space convertColorSpaceCdx2Hwc(int nColorSpace)
{
    disp_color_space hwcColorSpace;
    switch(nColorSpace)
    {
        case VENC_BT601:
            hwcColorSpace = DISP_BT601;
            break;
        case VENC_YCC:
            hwcColorSpace = DISP_BT601_F;
            break;
        default:
            aloge("fatal error! unknown cdx color space[0x%x]", nColorSpace);
            hwcColorSpace = DISP_BT601;
            break;
    }
    return hwcColorSpace;
}

CedarXNativeRenderer::CedarXNativeRenderer(const int hlay, MetaData *meta)
    : mVideoLayerId(hlay)
{
    int32_t halFormat; //halFormat = CdxPixelFormat

    if(!meta->findInt32(kKeyColorFormat, &halFormat))
    {
        aloge("not find kKeyColorFormat");
    }
    if(!meta->findInt32(kKeyWidth, &mWidth))   //width and height is bufWidth and bufHeight.
    {
        aloge("not find kKeyWidth");
    }
    if(!meta->findInt32(kKeyHeight, &mHeight))
    {
        aloge("not find kKeyHeight");
    }
    if(!meta->findInt32(kCedarXKeyDisplayTopX, &mCropLeft)) //displayTopX and displayTopY.
    {
        aloge("not find kCedarXKeyDisplayTopX");
    }
    if(!meta->findInt32(kCedarXKeyDisplayTopY, &mCropTop))
    {
        aloge("not find kCedarXKeyDisplayTopY");
    }
    if(!meta->findInt32(kKeyDisplayWidth, &mCropWidth)) //width and height is display_width and display_height.
    {
        aloge("not find kKeyDisplayWidth");
    }
    if(!meta->findInt32(kKeyDisplayHeight, &mCropHeight))
    {
        aloge("not find kKeyDisplayHeight");
    }
    mCropRight = mCropWidth;
    mCropBottom = mCropHeight;
    mCdxPixelFormat = (CdxPixelFormat)halFormat;
    mPixelFormat = (int32_t)convertPixelFormatCdx2Hwc((CdxPixelFormat)halFormat);

    //int32_t rotationDegrees = 0;
    int32_t nVdecInitRotation;  //clock wise.
    if (!meta->findInt32(kKeyRotation, &nVdecInitRotation))
    {
        alogw("vdec already rotate [%d] clock-wise", nVdecInitRotation);
    }
    if(!meta->findInt32(kCedarXKeyColorSpace, &mColorSpace))
    {
        aloge("not find kCedarXKeyColorSpace");
    }

    //when YV12: vdec_buffer's Y now is 32 pixel align in width, 16 lines align in height. (16*16)
    //when MB32: one Y_MB is 32pixel * 32 lines. spec is 16*16, but hw decoder extend to 32*32!
    if(mWidth != mCropWidth)
    {
        alogw("bufWidth[%d]!=display_width[%d]", mWidth, mCropWidth);
    }
    if(mHeight != mCropHeight)
    {
        alogw("bufHeight[%d]!=display_height[%d]", mHeight, mCropHeight);
    }

    if(mVideoLayerId >= 0)
    {
        struct src_info src;
        memset(&src, 0, sizeof(struct src_info));
    	src.w = mWidth;   //ALIGN32(mWidth);
    	src.h = mHeight;  //ALIGN32(mHeight);
        src.crop_x = mCropLeft;
        src.crop_y = mCropTop;
        src.crop_w = mCropWidth;
    	src.crop_h = mCropHeight;
    	src.format = mPixelFormat;
        src.color_space = (int)convertColorSpaceCdx2Hwc(mColorSpace);
        alogd("hwc disp fmt[0x%x], color space:%d", src.format, src.color_space);
        hwd_layer_set_src(mVideoLayerId, &src);
    }
	else
	{
        alogd("Be careful! video layer id[%d] < 0", mVideoLayerId);
	}
    mLayerShowed = 0;
}

CedarXNativeRenderer::~CedarXNativeRenderer()
{
}

int CedarXNativeRenderer::control(int cmd, int para, void *pData)
{
    int ret = CDX_OK;
    switch(cmd)
    {
        case VIDEORENDER_CMD_SHOW:
        {
            if(para == mLayerShowed)
            {
                ret = CDX_OK;
                break;
            }
            if(mVideoLayerId < 0)
            {
                alogd("Be careful! video layer id[%d] invalid, not show", mVideoLayerId);
                ret = CDX_OK;
                break;
            }
            if(para == 0)
            {
                if(RET_OK == hwd_layer_close(mVideoLayerId))
                {
                    ret = CDX_OK;
                    mLayerShowed = 0;
                }
                else
                {
                    alogw("fatal error! hwd layer close fail!");
                    ret = CDX_ERROR_UNKNOWN;
                }
            }
            else
            {
                if(RET_OK == hwd_layer_open(mVideoLayerId))
                {
                    ret = CDX_OK;
                    mLayerShowed = 1;
                }
                else
                {
                    alogw("fatal error! hwd layer open fail!");
                    ret = CDX_ERROR_UNKNOWN;
                }
            }
            break;
        }
        case VIDEORENDER_CMD_SET_CROP:
        {
            CdxVRFrameInfo *pCrop = (CdxVRFrameInfo*)pData;
            mCropLeft = pCrop->mnDisplayTopX;
            mCropTop = pCrop->mnDisplayTopY;
            mCropWidth = pCrop->mnDisplayWidth;
            mCropHeight = pCrop->mnDisplayHeight;
            mWidth = pCrop->mnBufWidth;
            mHeight = pCrop->mnBufHeight;

            if(mVideoLayerId >= 0)
            {
                struct src_info src;
                memset(&src, 0, sizeof(struct src_info));
            	src.w = mWidth; //ALIGN16(mWidth);
            	src.h = mHeight;    //ALIGN16(mHeight);
                src.crop_x = mCropLeft;
                src.crop_y = mCropTop;
                src.crop_w = mCropWidth;
            	src.crop_h = mCropHeight;
            	src.format = mPixelFormat;
                src.color_space = (int)convertColorSpaceCdx2Hwc(mColorSpace);
            	if(RET_OK == hwd_layer_set_src(mVideoLayerId, &src))
            	{
                    ret = CDX_OK;
            	}
                else
                {
                    ret = CDX_ERROR_UNKNOWN;
                }
            }
            else
            {
                alogd("Be careful! video layer id[%d] invalid, not set crop", mVideoLayerId);
                ret = CDX_OK;
            }
            break;
        }
        case VIDEORENDER_CMD_SET_SCALING_MODE:
        {
            if (mVideoLayerId >= 0) 
            {
                alogd("not support window videoScaling mode tmp");
            }
            ret = CDX_OK;
            break;
        }
        default:
            alogw("undefined command[0x%x]!", cmd);
            ret = CDX_ERROR_UNKNOWN;
            break;
    }
    return ret;
}
//extern int ion_alloc_phy2vir(void * pbuf);
void CedarXNativeRenderer::render(const void *data, size_t size)
{
    libhwclayerpara_t *plibhwclayerpara = (libhwclayerpara_t*)data;
    if(CDX_PIXEL_FORMAT_YV12 == mCdxPixelFormat && HWC_FORMAT_YUV420PLANAR == mPixelFormat)
    {
        unsigned long tmp = plibhwclayerpara->top_c;
        plibhwclayerpara->top_c = plibhwclayerpara->bottom_y;
        plibhwclayerpara->bottom_y = tmp;
    }
    if(mVideoLayerId >= 0)
    {
	    hwd_layer_render(mVideoLayerId, plibhwclayerpara);
    }
    else
    {
        alogv("Be careful! video layer id[%d] is invalid, not render", mVideoLayerId);
    }
}

int CedarXNativeRenderer::dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    return CDX_ERROR_UNKNOWN;
}

int CedarXNativeRenderer::enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    return CDX_ERROR_UNKNOWN;
}

int CedarXNativeRenderer::cancelFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    return CDX_ERROR_UNKNOWN;
}

}  // namespace android
