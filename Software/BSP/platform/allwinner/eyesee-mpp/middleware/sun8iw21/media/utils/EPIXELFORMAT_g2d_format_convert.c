//#define LOG_NDEBUG 0
#define LOG_TAG "vdeclib_2_g2d_format"
#include <utils/plat_log.h>

#include <EPIXELFORMAT_g2d_format_convert.h>

ERRORTYPE convert_EPIXELFORMAT_to_G2dFormat(
    PARAM_IN enum EPIXELFORMAT ePixelFormat, 
    PARAM_OUT g2d_data_fmt *pG2dFormat, 
    PARAM_OUT g2d_pixel_seq *pG2dPixelSeq)
{
    ERRORTYPE ret = SUCCESS;
    switch(ePixelFormat)
    {
        case PIXEL_FORMAT_NV21:
            *pG2dFormat = G2D_FMT_PYUV420UVC;
            *pG2dPixelSeq = G2D_SEQ_VUVU;
            break;
        default:
            aloge("fatal error! unsupport EPIXELFORMAT[0x%x]!", ePixelFormat);
            ret = FAILURE;
            break;
    }
    return ret;
}

ERRORTYPE convert_EPIXELFORMAT_to_g2d_fmt_enh(
    PARAM_IN enum EPIXELFORMAT ePixelFormat, 
    PARAM_OUT g2d_fmt_enh *pG2dFormat)
{
    ERRORTYPE ret = SUCCESS;
    switch(ePixelFormat)
    {
        case PIXEL_FORMAT_NV21:
            *pG2dFormat = G2D_FORMAT_YUV420UVC_V1U1V0U0;
            break;
        default:
            aloge("fatal error! unsupport EPIXELFORMAT[0x%x]!", ePixelFormat);
            ret = FAILURE;
            break;
    }
    return ret;
}


