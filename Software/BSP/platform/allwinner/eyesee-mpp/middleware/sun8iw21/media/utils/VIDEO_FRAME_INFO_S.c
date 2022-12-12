//#define LOG_NDEBUG 0
#define LOG_TAG "video_frame_info"
#include <utils/plat_log.h>

#include "plat_errno.h"
#include "plat_type.h"

#include <VIDEO_FRAME_INFO_S.h>

ERRORTYPE getVideoFrameBufferSizeInfo(PARAM_IN VIDEO_FRAME_INFO_S *pFrame, PARAM_OUT VideoFrameBufferSizeInfo *pSizeInfo)
{
    if(NULL == pFrame || NULL == pSizeInfo)
    {
        aloge("fatal error! pFrame[%p], pSizeInfo[%p]", pFrame, pSizeInfo);
        return FAILURE;
    }
    ERRORTYPE ret = SUCCESS;
    switch(pFrame->VFrame.mPixelFormat)
    {
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_AW_NV21M:
            pSizeInfo->mYSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight;
            pSizeInfo->mUSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight/2;
            pSizeInfo->mVSize = 0;
            break;
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
            pSizeInfo->mYSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight;
            pSizeInfo->mUSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight/4;
            pSizeInfo->mVSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight/4;
            break;
        case MM_PIXEL_FORMAT_YUV_AW_AFBC:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X:
            pSizeInfo->mYSize = pFrame->VFrame.mStride[0];
            pSizeInfo->mUSize = pFrame->VFrame.mStride[1];
            pSizeInfo->mVSize = pFrame->VFrame.mStride[2];
            break;
        case MM_PIXEL_FORMAT_RAW_SBGGR8:
        case MM_PIXEL_FORMAT_RAW_SGBRG8:
        case MM_PIXEL_FORMAT_RAW_SGRBG8:
        case MM_PIXEL_FORMAT_RAW_SRGGB8:
            pSizeInfo->mYSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight;
            pSizeInfo->mUSize = 0;
            pSizeInfo->mVSize = 0;
            break;
        case MM_PIXEL_FORMAT_RAW_SBGGR10:
        case MM_PIXEL_FORMAT_RAW_SGBRG10:
        case MM_PIXEL_FORMAT_RAW_SGRBG10:
        case MM_PIXEL_FORMAT_RAW_SRGGB10:
            pSizeInfo->mYSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight*10/8;
            pSizeInfo->mUSize = 0;
            pSizeInfo->mVSize = 0;
            break;
        case MM_PIXEL_FORMAT_RAW_SBGGR12:
        case MM_PIXEL_FORMAT_RAW_SGBRG12:
        case MM_PIXEL_FORMAT_RAW_SGRBG12:
        case MM_PIXEL_FORMAT_RAW_SRGGB12:
            pSizeInfo->mYSize = pFrame->VFrame.mWidth*pFrame->VFrame.mHeight*12/8;
            pSizeInfo->mUSize = 0;
            pSizeInfo->mVSize = 0;
            break;
        default:
            aloge("fatal error! not support pixel format[0x%x]", pFrame->VFrame.mPixelFormat);
            ret = FAILURE;
            break;
    }
    return ret;
}

