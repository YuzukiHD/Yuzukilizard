//#define LOG_NDEBUG 0
#define LOG_TAG "g2d_format"
#include <utils/plat_log.h>

#include <PIXEL_FORMAT_E_g2d_format_convert.h>

ERRORTYPE convert_PIXEL_FORMAT_E_to_G2dFormat(PARAM_IN PIXEL_FORMAT_E ePixelFormat, PARAM_OUT g2d_data_fmt *pG2dFormat,
                                              PARAM_OUT g2d_pixel_seq *pG2dPixelSeq)
{
    ERRORTYPE ret = SUCCESS;
    switch (ePixelFormat) {
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_AW_NV21M:
            *pG2dFormat = G2D_FMT_PYUV420UVC;
            *pG2dPixelSeq = G2D_SEQ_VUVU;
            break;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            *pG2dFormat = G2D_FMT_PYUV420UVC;
            *pG2dPixelSeq = G2D_SEQ_NORMAL;
            break;
        default:
            aloge("fatal error! unsupport pixel format[0x%x]!", ePixelFormat);
            *pG2dFormat = G2D_FMT_PYUV420UVC;
            *pG2dPixelSeq = G2D_SEQ_VUVU;
            ret = FAILURE;
            break;
    }
    return ret;
}

ERRORTYPE convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(PARAM_IN PIXEL_FORMAT_E ePixelFormat, PARAM_OUT g2d_fmt_enh *pG2dFormat)
{
    ERRORTYPE ret = SUCCESS;
    switch (ePixelFormat) {
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_AW_NV21M:
            *pG2dFormat = G2D_FORMAT_YUV420UVC_V1U1V0U0;
            break;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            *pG2dFormat = G2D_FORMAT_YUV420UVC_U1V1U0V0;
            break;
        case MM_PIXEL_FORMAT_RGB_888:
            *pG2dFormat = G2D_FORMAT_RGB888;
            break;
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422:
            *pG2dFormat = G2D_FORMAT_YUV422UVC_U1V1U0V0;
            break;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422:
            *pG2dFormat = G2D_FORMAT_YUV422UVC_V1U1V0U0;
            break;
        case MM_PIXEL_FORMAT_VYUY_PACKAGE_422:
            *pG2dFormat = G2D_FORMAT_IYUV422_Y1U0Y0V0;
            break;
        case MM_PIXEL_FORMAT_UYVY_PACKAGE_422:
            *pG2dFormat = G2D_FORMAT_IYUV422_Y1V0Y0U0;
            break;
        case MM_PIXEL_FORMAT_YUYV_PACKAGE_422:
            *pG2dFormat = G2D_FORMAT_IYUV422_V0Y1U0Y0;
            break;
        case MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422:
            *pG2dFormat = G2D_FORMAT_IYUV422_U0Y1V0Y0;
            break;
        default:
            aloge("fatal error! unsupport pixel format[0x%x]!", ePixelFormat);
            *pG2dFormat = G2D_FORMAT_YUV420UVC_V1U1V0U0;
            ret = FAILURE;
            break;
    }
    return ret;
}
