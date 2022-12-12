//#define LOG_NDEBUG 0
#define LOG_TAG "mm_comm_venc"
#include <utils/plat_log.h>

#include "mm_comm_venc.h"
#include <mm_comm_venc_op.h>

ERRORTYPE getPicSizeFromVENC_ATTR_S(VENC_ATTR_S *pVencAttr, SIZE_S *pPicSize)
{
    ERRORTYPE ret = SUCCESS;
    switch(pVencAttr->Type)
    {
        case PT_H264:
            pPicSize->Width = pVencAttr->AttrH264e.PicWidth;
            pPicSize->Height = pVencAttr->AttrH264e.PicHeight;
            break;
        case PT_MJPEG:
            pPicSize->Width = pVencAttr->AttrMjpeg.mPicWidth;
            pPicSize->Height = pVencAttr->AttrMjpeg.mPicHeight;
            break;
        case PT_JPEG:
            pPicSize->Width = pVencAttr->AttrJpeg.PicWidth;
            pPicSize->Height = pVencAttr->AttrJpeg.PicHeight;
            break;
        case PT_H265:
            pPicSize->Width = pVencAttr->AttrH265e.mPicWidth;
            pPicSize->Height = pVencAttr->AttrH265e.mPicHeight;
            break;
        default:
            aloge("fatal error! unsupport venc type[%d]", pVencAttr->Type);
            ret = FAILURE;
            break;
    }
    return ret;
}

