/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_common.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/03/15
  Last Modified :
  Description   : multimedia common function for internal use.
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "vfmt_conversion"
#include <utils/plat_log.h>

//ref platform headers
#include <string.h>
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include <linux/videodev2.h>

//media api headers to app
#include "mm_common.h"
#include "mm_comm_video.h"
#include "mm_comm_venc.h"
#include <sunxi_camera_v2.h>

//media internal common headers.
//#include "media_common.h"
#include "mm_component.h"
#include "vencoder.h"
#include "vdecoder.h"

#include "cdx_list.h"

BOOL judgeAudioFileFormat(MEDIA_FILE_FORMAT_E fileFormat)
{
    switch(fileFormat)
    {
        case MEDIA_FILE_FORMAT_MP3:
        case MEDIA_FILE_FORMAT_AAC:
            return TRUE;
        default:
            return FALSE;
    }
}

PIXEL_FORMAT_E map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(int v4l2PixFmt)
{
    PIXEL_FORMAT_E nPixelFormat;
    switch(v4l2PixFmt)
    {
        case V4L2_PIX_FMT_NV21:
            nPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            break;
        case V4L2_PIX_FMT_NV12:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            break;
        case V4L2_PIX_FMT_YUV420:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            break;
        case V4L2_PIX_FMT_YVU420:
            nPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            break;
        case V4L2_PIX_FMT_YUYV:
            nPixelFormat = MM_PIXEL_FORMAT_YUYV_PACKAGE_422;
            break;
        case V4L2_PIX_FMT_NV16:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
            break;
        case V4L2_PIX_FMT_NV61:
            nPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
            break;
        case V4L2_PIX_FMT_FBC:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
            break;
        case V4L2_PIX_FMT_LBC_2_0X:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
            break;
        case V4L2_PIX_FMT_LBC_2_5X:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            break;
        case V4L2_PIX_FMT_LBC_1_5X:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
            break;
        case V4L2_PIX_FMT_LBC_1_0X:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
            break;
        case V4L2_PIX_FMT_SBGGR8:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SBGGR8;
            break;
        case V4L2_PIX_FMT_SRGGB8:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SRGGB8;
            break;
        case V4L2_PIX_FMT_SGRBG8:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SGRBG8;
            break;
        case V4L2_PIX_FMT_SGBRG8:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SGBRG8;
            break;
        case V4L2_PIX_FMT_SBGGR10:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SBGGR10;
            break;
        case V4L2_PIX_FMT_SRGGB10:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SRGGB10;
            break;
        case V4L2_PIX_FMT_SGRBG10:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SGRBG10;
            break;
        case V4L2_PIX_FMT_SGBRG10:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SGBRG10;
            break;
        case V4L2_PIX_FMT_SBGGR12:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SBGGR12;
            break;
        case V4L2_PIX_FMT_SRGGB12:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SRGGB12;
            break;
        case V4L2_PIX_FMT_SGRBG12:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SGRBG12;
            break;
        case V4L2_PIX_FMT_SGBRG12:
            nPixelFormat = MM_PIXEL_FORMAT_RAW_SGBRG12;
            break;
        case V4L2_PIX_FMT_MJPEG:
        case V4L2_PIX_FMT_JPEG:
        case V4L2_PIX_FMT_H264:
            alogd("compressed format[0x%x], default to MM_PIXEL_FORMAT_BUTT[0x%x]", v4l2PixFmt, MM_PIXEL_FORMAT_BUTT);
            nPixelFormat = MM_PIXEL_FORMAT_BUTT;
            break;
        case V4L2_PIX_FMT_NV21M:
            nPixelFormat = MM_PIXEL_FORMAT_AW_NV21M;
            break;
        case V4L2_PIX_FMT_GREY:
            nPixelFormat = MM_PIXEL_FORMAT_YUV_GREY;
            break;
        default:
            aloge("fatal error! unknown V4L2_PIX_FMT[0x%x]", v4l2PixFmt);
            nPixelFormat = MM_PIXEL_FORMAT_BUTT;
            break;
    }
    return nPixelFormat;
}

int map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(PIXEL_FORMAT_E format)
{
    switch (format) 
    {
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
            return V4L2_PIX_FMT_YUV420;
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
            return V4L2_PIX_FMT_YVU420;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            return V4L2_PIX_FMT_NV12;
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
            return V4L2_PIX_FMT_NV21;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422:
            return V4L2_PIX_FMT_NV16;
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422:
            return V4L2_PIX_FMT_NV61;
        case MM_PIXEL_FORMAT_YUYV_PACKAGE_422:
            return V4L2_PIX_FMT_YUYV;
        case MM_PIXEL_FORMAT_RGB_1555:
            return V4L2_PIX_FMT_RGB555;
        case MM_PIXEL_FORMAT_RGB_4444:
            return V4L2_PIX_FMT_RGB444;
        case MM_PIXEL_FORMAT_RGB_8888:
            return V4L2_PIX_FMT_RGB32;
        case MM_PIXEL_FORMAT_YUV_AW_AFBC:
            return V4L2_PIX_FMT_FBC;
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X:
            return V4L2_PIX_FMT_LBC_2_0X;
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X:
            return V4L2_PIX_FMT_LBC_2_5X;
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X:
            return V4L2_PIX_FMT_LBC_1_5X;
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X:
            return V4L2_PIX_FMT_LBC_1_0X;
        case MM_PIXEL_FORMAT_RAW_SBGGR8:
            return V4L2_PIX_FMT_SBGGR8;
        case MM_PIXEL_FORMAT_RAW_SRGGB8:
            return V4L2_PIX_FMT_SRGGB8;
        case MM_PIXEL_FORMAT_RAW_SGBRG8:
            return V4L2_PIX_FMT_SGBRG8;
        case MM_PIXEL_FORMAT_RAW_SGRBG8:
            return V4L2_PIX_FMT_SGRBG8;
        case MM_PIXEL_FORMAT_RAW_SBGGR10:
            return V4L2_PIX_FMT_SBGGR8;
        case MM_PIXEL_FORMAT_RAW_SRGGB10:
            return V4L2_PIX_FMT_SRGGB10;
        case MM_PIXEL_FORMAT_RAW_SGBRG10:
            return V4L2_PIX_FMT_SGBRG10;
        case MM_PIXEL_FORMAT_RAW_SGRBG10:
            return V4L2_PIX_FMT_SGRBG10;
        case MM_PIXEL_FORMAT_RAW_SBGGR12:
            return V4L2_PIX_FMT_SBGGR10;
        case MM_PIXEL_FORMAT_RAW_SRGGB12:
            return V4L2_PIX_FMT_SRGGB12;
        case MM_PIXEL_FORMAT_RAW_SGBRG12:
            return V4L2_PIX_FMT_SGBRG12;
        case MM_PIXEL_FORMAT_RAW_SGRBG12:
            return V4L2_PIX_FMT_SGRBG12;
        case MM_PIXEL_FORMAT_AW_NV21M:
            return V4L2_PIX_FMT_NV21M;
        case MM_PIXEL_FORMAT_YUV_GREY:
            return V4L2_PIX_FMT_GREY;
        default:
            aloge("Unknown format 0x%x, use YUV420 instead.", format);
            return V4L2_PIX_FMT_YUV420;
    }
}

VIDEO_FIELD_E map_V4L2_FIELD_to_VIDEO_FIELD_E(enum v4l2_field v4l2Field)
{
    VIDEO_FIELD_E nVideoField;
    switch(v4l2Field)
    {
        case V4L2_FIELD_NONE:
            nVideoField = VIDEO_FIELD_FRAME;
            break;
        case V4L2_FIELD_TOP:
            nVideoField = VIDEO_FIELD_TOP;
            break;
        case V4L2_FIELD_BOTTOM:
            nVideoField = VIDEO_FIELD_BOTTOM;
            break;
        case V4L2_FIELD_INTERLACED:
            nVideoField = VIDEO_FIELD_INTERLACED;
            break;
        default:
            aloge("fatal error! unknown V4L2_FIELD[0x%x]", v4l2Field);
            nVideoField = VIDEO_FIELD_FRAME;
            break;
    }
    return nVideoField;
}

enum v4l2_field map_VIDEO_FIELD_E_to_V4L2_FIELD(VIDEO_FIELD_E eVideoField)
{
    enum v4l2_field v4l2Field;
    switch(eVideoField)
    {
        case VIDEO_FIELD_FRAME:
            v4l2Field = V4L2_FIELD_NONE;
            break;
        case VIDEO_FIELD_TOP:
            v4l2Field = V4L2_FIELD_TOP;
            break;
        case VIDEO_FIELD_BOTTOM:
            v4l2Field = V4L2_FIELD_BOTTOM;
            break;
        case VIDEO_FIELD_INTERLACED:
            v4l2Field = V4L2_FIELD_INTERLACED;
            break;
        default:
            aloge("fatal error! unknown VIDEO_FIELD_E[0x%x]", eVideoField);
            v4l2Field = V4L2_FIELD_NONE;
            break;
    }
    return v4l2Field;
}

PIXEL_FORMAT_E map_EPIXELFORMAT_to_PIXEL_FORMAT_E(enum EPIXELFORMAT vdecPixelFormat)
{
    PIXEL_FORMAT_E dstPixelFormat;
    switch (vdecPixelFormat) {
        case PIXEL_FORMAT_YV12:
            dstPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            break;
        case PIXEL_FORMAT_YUV_PLANER_420:
            dstPixelFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            break;
        case PIXEL_FORMAT_NV21:
            dstPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            break;
        case PIXEL_FORMAT_NV12:
            dstPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            break;
        case PIXEL_FORMAT_PLANARUV_422:
            dstPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
            break;
        case PIXEL_FORMAT_PLANARVU_422:
            dstPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
            break;
        case PIXEL_FORMAT_YUYV:
            dstPixelFormat = MM_PIXEL_FORMAT_YUYV_PACKAGE_422;
            break;
        default:
            aloge("fatal error! unknown vdecPixelFormat[0x%x]", vdecPixelFormat);
            dstPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            break;
    }
    return dstPixelFormat;
}

enum EPIXELFORMAT map_PIXEL_FORMAT_E_to_EPIXELFORMAT(PIXEL_FORMAT_E pixelFormat)
{
    enum EPIXELFORMAT vdecPixelFormat = PIXEL_FORMAT_DEFAULT;
    switch (pixelFormat) {
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_AW_NV21M:
            vdecPixelFormat = PIXEL_FORMAT_NV21;
            break;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            vdecPixelFormat = PIXEL_FORMAT_NV12;
            break;
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
            vdecPixelFormat = PIXEL_FORMAT_YUV_PLANER_420;
            break;
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
            vdecPixelFormat = PIXEL_FORMAT_YV12;
            break;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422:
            vdecPixelFormat = PIXEL_FORMAT_PLANARUV_422;
            break;
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422:
            vdecPixelFormat = PIXEL_FORMAT_PLANARVU_422;
            break;
        case MM_PIXEL_FORMAT_YUYV_PACKAGE_422:
            vdecPixelFormat = PIXEL_FORMAT_YUYV;
            break;
        case MM_PIXEL_FORMAT_RGB_8888:
            vdecPixelFormat = PIXEL_FORMAT_ARGB;
            break;
        default:
            aloge("unsupported temporary! pixelForamt[0x%x]", pixelFormat);
            break;
    }
    return vdecPixelFormat;
}


