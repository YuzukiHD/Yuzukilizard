/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : MediaStructConvert.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/12/13
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "MediaStructConvert"
#include <utils/plat_log.h>

#include <mpi_videoformat_conversion.h>
#include "MediaStructConvert.h"

namespace EyeseeLinux {

void config_VIDEO_FRAME_INFO_S_by_VIDEO_FRAME_BUFFER_S(VIDEO_FRAME_INFO_S *pEncBuf, VIDEO_FRAME_BUFFER_S *pCameraFrameInfo)
{
    *pEncBuf = pCameraFrameInfo->mFrameBuf;
    /*
    pEncBuf->mId = pCameraFrameInfo->mFrameBuf.u32PoolId;
    pEncBuf->VFrame.mWidth = pCameraFrameInfo->mFrameBuf.u32Width;
    pEncBuf->VFrame.mHeight = pCameraFrameInfo->mFrameBuf.u32Height;
    pEncBuf->VFrame.mField = map_V4L2_FIELD_to_VIDEO_FIELD_E((enum v4l2_field)pCameraFrameInfo->mFrameBuf.u32field);
    pEncBuf->VFrame.mPixelFormat = map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(pCameraFrameInfo->mFrameBuf.u32PixelFormat);
    pEncBuf->VFrame.mVideoFormat = VIDEO_FORMAT_LINEAR;
    pEncBuf->VFrame.mCompressMode = COMPRESS_MODE_NONE;
    pEncBuf->VFrame.mPhyAddr[0] = pCameraFrameInfo->mFrameBuf.u32PhyAddr[0];
    pEncBuf->VFrame.mPhyAddr[1] = pCameraFrameInfo->mFrameBuf.u32PhyAddr[1];
    pEncBuf->VFrame.mPhyAddr[2] = pCameraFrameInfo->mFrameBuf.u32PhyAddr[2];
    pEncBuf->VFrame.mpVirAddr[0] = pCameraFrameInfo->mFrameBuf.pVirAddr[0];
    pEncBuf->VFrame.mpVirAddr[1] = pCameraFrameInfo->mFrameBuf.pVirAddr[1];
    pEncBuf->VFrame.mpVirAddr[2] = pCameraFrameInfo->mFrameBuf.pVirAddr[2];
    pEncBuf->VFrame.mStride[0] = pCameraFrameInfo->mFrameBuf.u32Stride[0];
    pEncBuf->VFrame.mStride[1] = pCameraFrameInfo->mFrameBuf.u32Stride[1];
    pEncBuf->VFrame.mStride[2] = pCameraFrameInfo->mFrameBuf.u32Stride[2];
    pEncBuf->VFrame.mOffsetTop = pCameraFrameInfo->mCrop.Y;
    pEncBuf->VFrame.mOffsetBottom = pCameraFrameInfo->mCrop.Y + pCameraFrameInfo->mCrop.Height;
    pEncBuf->VFrame.mOffsetLeft = pCameraFrameInfo->mCrop.X;
    pEncBuf->VFrame.mOffsetRight = pCameraFrameInfo->mCrop.X + pCameraFrameInfo->mCrop.Width;
    pEncBuf->VFrame.mpts = (int64_t)pCameraFrameInfo->mFrameBuf.stTimeStamp.tv_sec*1000*1000 + pCameraFrameInfo->mFrameBuf.stTimeStamp.tv_usec;
    */
    return;
}

}

