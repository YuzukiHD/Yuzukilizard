/******************************************************************************
  Copyright (C), 2001-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_debug.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2022/03/26
  Last Modified :
  Description   : media debug functions implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "media_debug"
#include <utils/plat_log.h>

#include <mpi_videoformat_conversion.h>

#include <iniparser.h>
#include <media_debug.h>

#define MEDIA_DEBUG_CONF_PATH_LEN    (64)
#define MEDIA_DEBUG_CONF_PATH_CNT    (5)
#define MEDIA_DEBUG_CONF_PATH_1      "/mnt/extsd/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_2      "/tmp/sd/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_3      "/tmp/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_4      "/data/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_5      "/mnt/disk_0/media_debug.conf"
#define MEDIA_DEBUG_ENV_NAME         "MPP_DEDIA_DEBUG_FILE_PATH"

#define MPP_VIPP_PARAM_CNT_MAX       (16)
#define MPP_VENC_PARAM_CNT_MAX       (16)
#define MPP_MUX_PARAM_CNT_MAX        (8)

static dictionary *getDictByConfPath(const char *pConfPath)
{
    dictionary *pDict = NULL;
    char strPath[MEDIA_DEBUG_CONF_PATH_LEN];

    if (NULL == pConfPath)
    {
        int is_found = 0;
        // check env first
        char *env = getenv(MEDIA_DEBUG_ENV_NAME);
        alogd("%s=%s", MEDIA_DEBUG_ENV_NAME, env);
        if (env && strlen(env))
        {
            if ((access(env, F_OK)) != -1)
            {
                if (access(env, R_OK) != -1)
                {
                    is_found = 1;
                    snprintf(strPath, sizeof(strPath), "%s", env);
                }
            }
        }
        // if env is not found, check the known path
        if (0 == is_found)
        {
            char strTotalPath[MEDIA_DEBUG_CONF_PATH_CNT][MEDIA_DEBUG_CONF_PATH_LEN] = {
                MEDIA_DEBUG_CONF_PATH_1, MEDIA_DEBUG_CONF_PATH_2, MEDIA_DEBUG_CONF_PATH_3,
                MEDIA_DEBUG_CONF_PATH_4, MEDIA_DEBUG_CONF_PATH_5
            };
            for (int i = 0; i < MEDIA_DEBUG_CONF_PATH_CNT; i++)
            {
                snprintf(strPath, sizeof(strPath), "%s", strTotalPath[i]);
                if ((access(strPath, F_OK)) != -1)
                {
                    if (access(strPath, R_OK) != -1)
                    {
                        is_found = 1;
                        break;
                    }
                }
            }
        }
        if (0 == is_found)
        {
            alogv("The file media_debug.conf was not found!");
            return NULL;
        }
    }
    else
    {
        if ((access(pConfPath, F_OK)) == -1)
        {
            alogd("%s is not exist", pConfPath);
            return NULL;
        }
        if (access(pConfPath, R_OK) == -1)
        {
            alogd("%s can not read", pConfPath);
            return NULL;
        }
        if (MEDIA_DEBUG_CONF_PATH_LEN < strlen(pConfPath))
        {
            alogw("%s len is too long, it should be less %d bytes.", pConfPath, MEDIA_DEBUG_CONF_PATH_LEN);
            return NULL;
        }
        snprintf(strPath, sizeof(strPath), "%s", pConfPath);
    }

    pDict = iniparser_load(strPath);
    if (NULL == pDict)
    {
        aloge("fatal error! load conf %s fail!", strPath);
        return NULL;
    }
    alogw("load %s success", strPath);

    return pDict;
}

static int findMppVippParamIndex(dictionary *pDict, int mVippDev)
{
    char *str = NULL;
    char paramkey[128];
    int index = -1;

    if (NULL == pDict)
    {
        aloge("fatal error, invalid params! %p", pDict);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "vipp_params:vipp_id");

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VIPP_PARAM_CNT_MAX > i))
        {
            if (mVippDev == (int)(atoi(strPart)))
            {
                index = i;
                alogd("vipp_params index:%d, vipp_id %d", index, mVippDev);
                break;
            }
            strPart = strtok(NULL, ",");
            i++;
        }
    }
    else
    {
        alogw("vipp_params, key vipp_id is not found.");
        index = -1;
    }

    return index;
}

static int findMppVippIntParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    int paramint[MPP_VIPP_PARAM_CNT_MAX];
    int result = 0;

    if ((NULL == pDict) || (MPP_VIPP_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "vipp_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VIPP_PARAM_CNT_MAX > i))
        {
            paramint[i++] = atoi(strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramint[index];
        alogv("vipp_params index:%d, key %s result %d", index, key, result);
    }
    else
    {
        alogv("vipp_params index:%d, key %s is not found.", index, key);
        result = -1;
    }

    return result;
}

static char* findMppVippStringParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    char paramstr[MPP_VIPP_PARAM_CNT_MAX][64];
    char *result = NULL;

    if ((NULL == pDict) || (MPP_VIPP_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return NULL;
    }

    snprintf(paramkey, sizeof(paramkey), "vipp_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VIPP_PARAM_CNT_MAX > i))
        {
            strcpy(paramstr[i++], strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramstr[index];
        alogv("vipp_params index:%d, key %s result %s", index, key, result);
    }
    else
    {
        alogv("vipp_params index:%d, key %s is not found.", index, key);
        result = NULL;
    }

    return result;
}

void loadMppViParams(viChnManager *pVipp, const char *pConfPath)
{
    char *ptr = NULL;
    dictionary *pDict = NULL;

    if (NULL == pVipp)
    {
        aloge("fatal error, invalid params! %p", pVipp);
        return;
    }

    pDict = getDictByConfPath(pConfPath);
    if (NULL == pDict)
    {
        //aloge("fatal error! get Dict By MediaDebugConfPath fail!");
        return;
    }

    int index = findMppVippParamIndex(pDict, pVipp->vipp_dev_id);
    if (-1 == index)
    {
        alogw("vippDev: %d is not found.", pVipp->vipp_dev_id);
        iniparser_freedict(pDict);
        return;
    }
    int mVippDev = pVipp->vipp_dev_id;

    int mOnlineEnable = findMppVippIntParam(pDict, "online_en", index);
    int mOnlineShareBufNum = findMppVippIntParam(pDict, "online_share_buf_num", index);
    int mViBufferNum = findMppVippIntParam(pDict, "vi_buffer_num", index);
    int mWidth = findMppVippIntParam(pDict, "src_width", index);
    int mHeight = findMppVippIntParam(pDict, "src_height", index);
    int mFrameRate = findMppVippIntParam(pDict, "src_framerate", index);

    int mPixFmt = -1;
    ptr = (char *)findMppVippStringParam(pDict, "pixfmt", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "nv21"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
        }
        else if (!strcmp(ptr, "yv12"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YVU_PLANAR_420);
        }
        else if (!strcmp(ptr, "nv12"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420);
        }
        else if (!strcmp(ptr, "yu12"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_PLANAR_420);
        }
        else if (!strcmp(ptr, "lbc_2_0x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X);
        }
        else if (!strcmp(ptr, "lbc_2_5x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X);
        }
        else if (!strcmp(ptr, "lbc_1_5x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X);
        }
        else if (!strcmp(ptr, "lbc_1_0x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X);
        }
        else
        {
            mPixFmt = -1;
            alogw("fatal error! wrong src pixfmt:%s", ptr);
        }
    }
    else
    {
        mPixFmt = -1;
    }

    int mColorSpace = -1;
    ptr = (char *)findMppVippStringParam(pDict, "color_space", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "jpeg"))
        {
            mColorSpace = V4L2_COLORSPACE_JPEG;
        }
        else if (!strcmp(ptr, "rec709"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709;
        }
        else if (!strcmp(ptr, "rec709_part_range"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
        }
        else
        {
            mColorSpace = -1;
            alogw("fatal error! wrong color space:%s", ptr);
        }
    }
    else
    {
        mColorSpace = -1;
    }

    int mViDropFrameNum = findMppVippIntParam(pDict, "drop_frm_num", index);
    int mWdrEn = findMppVippIntParam(pDict, "wdr_en", index);
    int mEncppEnable = findMppVippIntParam(pDict, "encpp_enable", index);

    alogw("**********************************************************");
    alogw("mpp vipp debug mode                                       ");
    alogw("**********************************************************");
    alogw("[params_name]              [user]               [debug]   ");
    alogw("vipp_id                    %-10d           %-10d          ", pVipp->vipp_dev_id, mVippDev);
    if (-1 != mOnlineEnable)
    {
        alogw("online_en                  %-10d           %-10d          ", pVipp->mstAttr.mOnlineEnable, mOnlineEnable);
        pVipp->mstAttr.mOnlineEnable = mOnlineEnable;
    }
    if (-1 != mOnlineShareBufNum)
    {
        alogw("online_share_buf_num       %-10d           %-10d          ", pVipp->mstAttr.mOnlineShareBufNum, mOnlineShareBufNum);
        pVipp->mstAttr.mOnlineShareBufNum = mOnlineShareBufNum;
    }
    if (-1 != mViBufferNum)
    {
        alogw("vi_buffer_num              %-10d           %-10d          ", pVipp->mstAttr.nbufs, mViBufferNum);
        pVipp->mstAttr.nbufs = mViBufferNum;
    }
    if (-1 != mWidth)
    {
        alogw("src_width                  %-10d           %-10d          ", pVipp->mstAttr.format.width, mWidth);
        pVipp->mstAttr.format.width = mWidth;
    }
    if (-1 != mHeight)
    {
        alogw("src_height                 %-10d           %-10d          ", pVipp->mstAttr.format.height, mHeight);
        pVipp->mstAttr.format.height = mHeight;
    }
    if (-1 != mFrameRate)
    {
        alogw("src_framerate              %-10d           %-10d          ", pVipp->mstAttr.fps, mFrameRate);
        pVipp->mstAttr.fps = mFrameRate;
    }
    if (-1 != mPixFmt)
    {
        alogw("pixfmt                     %-10d           %-10d          ", pVipp->mstAttr.format.pixelformat, mPixFmt);
        pVipp->mstAttr.format.pixelformat = mPixFmt;
    }
    if (-1 != mColorSpace)
    {
        alogw("color_space                %-10d           %-10d          ", pVipp->mstAttr.format.colorspace, mColorSpace);
        pVipp->mstAttr.format.colorspace = mColorSpace;
    }
    if (-1 != mViDropFrameNum)
    {
        alogw("drop_frm_num               %-10d           %-10d          ", pVipp->mstAttr.drop_frame_num, mViDropFrameNum);
        pVipp->mstAttr.drop_frame_num = mViDropFrameNum;
    }
    if (-1 != mWdrEn)
    {
        alogw("wdr_en                     %-10d           %-10d          ", pVipp->mstAttr.wdr_mode, mWdrEn);
        pVipp->mstAttr.wdr_mode = mWdrEn;
    }
    if (-1 != mEncppEnable)
    {
        alogw("encpp_enable               %-10d           %-10d          ", pVipp->mstAttr.mbEncppEnable, mEncppEnable);
        pVipp->mstAttr.mbEncppEnable = mEncppEnable;
    }
    alogw("**********************************************************");

    iniparser_freedict(pDict);
}

static int findMppVencParamIndex(dictionary *pDict, int mVencChn)
{
    char *str = NULL;
    char paramkey[128];
    int index = -1;

    if (NULL == pDict)
    {
        aloge("fatal error, invalid params! %p", pDict);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "venc_params:venc_ch_id");

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VENC_PARAM_CNT_MAX > i))
        {
            if (mVencChn == (int)(atoi(strPart)))
            {
                index = i;
                alogd("venc_params index:%d, venc_ch_id %d", index, mVencChn);
                break;
            }
            strPart = strtok(NULL, ",");
            i++;
        }
    }
    else
    {
        alogw("venc_params, key venc_ch_id is not found.");
        index = -1;
    }

    return index;
}

static int findMppVencIntParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    int paramint[MPP_VENC_PARAM_CNT_MAX];
    int result = 0;

    if ((NULL == pDict) || (MPP_VENC_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "venc_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VENC_PARAM_CNT_MAX > i))
        {
            paramint[i++] = atoi(strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramint[index];
        alogv("venc_params index:%d, key %s result %d", index, key, result);
    }
    else
    {
        alogv("venc_params index:%d, key %s is not found.", index, key);
        result = -1;
    }

    return result;
}

static char* findMppVencStringParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    char paramstr[MPP_VENC_PARAM_CNT_MAX][64];
    char *result = NULL;

    if ((NULL == pDict) || (MPP_VENC_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return NULL;
    }

    snprintf(paramkey, sizeof(paramkey), "venc_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VENC_PARAM_CNT_MAX > i))
        {
            strcpy(paramstr[i++], strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramstr[index];
        alogv("venc_params index:%d, key %s result %s", index, key, result);
    }
    else
    {
        alogv("venc_params index:%d, key %s is not found.", index, key);
        result = NULL;
    }

    return result;
}

static double findMppVencDoubleParam(dictionary *pDict, const char *key, int index)
{
    char parameterkey[128];

    if ((NULL == pDict) || (MPP_VENC_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return (double)0.0;
    }

    sprintf(parameterkey, "venc_params:%s", key);
    return iniparser_getdouble(pDict, parameterkey, -1.0);
}

void loadMppVencParams(VIDEOENCDATATYPE *pVideoEncData, const char *pConfPath)
{
    char *ptr = NULL;
    dictionary *pDict = NULL;

    if (NULL == pVideoEncData)
    {
        aloge("fatal error, invalid params! %p", pVideoEncData);
        return;
    }

    pDict = getDictByConfPath(pConfPath);
    if (NULL == pDict)
    {
        //aloge("fatal error! get Dict By MediaDebugConfPath fail!");
        return;
    }

    int index = findMppVencParamIndex(pDict, pVideoEncData->mMppChnInfo.mChnId);
    if (-1 == index)
    {
        alogw("VencChn: %d is not found.", pVideoEncData->mMppChnInfo.mChnId);
        iniparser_freedict(pDict);
        return;
    }
    int mVeChn = pVideoEncData->mMppChnInfo.mChnId;

    int mOnlineEnable = findMppVencIntParam(pDict, "online_en", index);
    int mOnlineShareBufNum = findMppVencIntParam(pDict, "online_share_buf_num", index);

    int mVideoEncoderFmt = -1;
    ptr = (char *)findMppVencStringParam(pDict, "video_encoder", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "H264"))
        {
            mVideoEncoderFmt = PT_H264;
        }
        else if (!strcmp(ptr, "H265"))
        {
            mVideoEncoderFmt = PT_H265;
        }
        else if (!strcmp(ptr, "MJPEG"))
        {
            mVideoEncoderFmt = PT_MJPEG;
        }
        else if (!strcmp(ptr, "JPEG"))
        {
            mVideoEncoderFmt = PT_JPEG;
        }
        else
        {
            mVideoEncoderFmt = -1;
            alogw("fatal error! wrong encoder type:%s", ptr);
        }
    }
    else
    {
        mVideoEncoderFmt = -1;
    }

    int mEncUseProfile = findMppVencIntParam(pDict, "profile", index);
    int mEncLevel = findMppVencIntParam(pDict, "level", index);

    int mPixFmt = -1;
    ptr = (char *)findMppVencStringParam(pDict, "pixfmt", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "nv21"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "yv12"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
        }
        else if (!strcmp(ptr, "nv12"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "yu12"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
        }
        else if (!strcmp(ptr, "lbc_2_0x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
        }
        else if (!strcmp(ptr, "lbc_2_5x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else if (!strcmp(ptr, "lbc_1_5x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
        }
        else if (!strcmp(ptr, "lbc_1_0x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
        }
        else
        {
            mPixFmt = -1;
            alogw("fatal error! wrong src pixfmt:%s", ptr);
        }
    }
    else
    {
        mPixFmt = -1;
    }

    int mColorSpace = -1;
    ptr = (char *)findMppVencStringParam(pDict, "color_space", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "jpeg"))
        {
            mColorSpace = V4L2_COLORSPACE_JPEG;
        }
        else if (!strcmp(ptr, "rec709"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709;
        }
        else if (!strcmp(ptr, "rec709_part_range"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
        }
        else
        {
            mColorSpace = -1;
            alogw("fatal error! wrong color space:%s", ptr);
        }
    }
    else
    {
        mColorSpace = -1;
    }

    int mEncppEnable = findMppVencIntParam(pDict, "encpp_enable", index);
    //int mEncppSharpAttenCoefPer = findMppVencIntParam(pDict, "encppSharpAttenCoefPer", index);

    int mSrcWidth = findMppVencIntParam(pDict, "src_width", index);
    int mSrcHeight = findMppVencIntParam(pDict, "src_height", index);
#if (AWCHIP == AW_V853)
    if (0 == mEncppEnable)
    {
        mSrcWidth = ALIGN(mSrcWidth, 16);
        mSrcHeight = ALIGN(mSrcHeight, 16);
    }
#endif

    int mVideoWidth = findMppVencIntParam(pDict, "video_width", index);
    int mVideoHeight = findMppVencIntParam(pDict, "video_height", index);

    int mProductMode = findMppVencIntParam(pDict, "product_mode", index);
    int mSensorType = findMppVencIntParam(pDict, "sensor_type", index);
    int mKeyFrameInterval = findMppVencIntParam(pDict, "key_frame_interval", index);
    int mDropFrameNum = findMppVencIntParam(pDict, "drop_frm_num", index);

    int mInitQp = findMppVencIntParam(pDict, "init_qp", index);
    int mMinIQp = findMppVencIntParam(pDict, "min_i_qp", index);
    int mMaxIQp = findMppVencIntParam(pDict, "max_i_qp", index);
    int mMinPQp = findMppVencIntParam(pDict, "min_p_qp", index);
    int mMaxPQp = findMppVencIntParam(pDict, "max_p_qp", index);
    int mEnMbQpLimit = findMppVencIntParam(pDict, "mb_qp_limit_en", index);
    int mMovingTh = findMppVencIntParam(pDict, "moving_th", index);
    int mQuality = findMppVencIntParam(pDict, "quality", index);
    int mIBitsCoef = findMppVencIntParam(pDict, "i_bits_coef", index);
    int mPBitsCoef = findMppVencIntParam(pDict, "p_bits_coef", index);

    int mSrcFrameRate = findMppVencIntParam(pDict, "src_framerate", index);
    int mVideoFrameRate = findMppVencIntParam(pDict, "video_framerate", index);
    int mVideoBitRate = findMppVencIntParam(pDict, "video_bitrate", index);

    int mPFrameIntraEn = findMppVencIntParam(pDict, "p_frame_intra_en", index);
    int mEnableFastEnc = findMppVencIntParam(pDict, "enable_fast_enc", index);

    int mGopMode = findMppVencIntParam(pDict, "gop_mode", index);
    int mGopSize = findMppVencIntParam(pDict, "gop_size", index);
    
    int m2DnrEnable = findMppVencIntParam(pDict, "2dnr_en", index);
    int m2DnrStrengthY = findMppVencIntParam(pDict, "2dnr_strength_y", index);
    int m2DnrStrengthUV = findMppVencIntParam(pDict, "2dnr_strength_c", index);
    int m2DnrThY = findMppVencIntParam(pDict, "2dnr_threshold_y", index);
    int m2DnrThUV = findMppVencIntParam(pDict, "2dnr_threshold_c", index);

    int m3DnrEnable = findMppVencIntParam(pDict, "3dnr_en", index);
    int m3DnrAdjustPixLevelEnable = findMppVencIntParam(pDict, "3dnr_pix_level_en", index);
    int m3DnrSmoothFilterEnable = findMppVencIntParam(pDict, "3dnr_smooth_en", index);
    int m3DnrMaxPixDiffTh = findMppVencIntParam(pDict, "3dnr_pix_diff_th", index);
    int m3DnrMaxMvTh = findMppVencIntParam(pDict, "3dnr_max_mv_th", index);
    int m3DnrMaxMadTh = findMppVencIntParam(pDict, "3dnr_max_mad_th", index);
    int m3DnrMinCoef = findMppVencIntParam(pDict, "3dnr_min_coef", index);
    int m3DnrMaxCoef = findMppVencIntParam(pDict, "3dnr_max_coef", index);

    int mCropEnable = findMppVencIntParam(pDict, "crop_en", index);
    int mCropRectX = findMppVencIntParam(pDict, "crop_rect_x", index);
    int mCropRectY = findMppVencIntParam(pDict, "crop_rect_y", index);
    int mCropRectWidth = findMppVencIntParam(pDict, "crop_rect_w", index);
    int mCropRectHeight = findMppVencIntParam(pDict, "crop_rect_h", index);

    int mSuperFrmMode = findMppVencIntParam(pDict, "super_frm_mode", index);
    int mSuperIFrmBitsThr = findMppVencIntParam(pDict, "super_i_frm_bits_thr", index);
    int mSuperPFrmBitsThr = findMppVencIntParam(pDict, "super_p_frm_bits_thr", index);

    int mEncodeRotate = findMppVencIntParam(pDict, "encode_rotate", index);
    switch(mEncodeRotate)
    {
    case 0:
        mEncodeRotate = ROTATE_NONE;
        break;
    case 90:
        mEncodeRotate = ROTATE_90;
        break;
    case 180:
        mEncodeRotate = ROTATE_180;
        break;
    case 270:
        mEncodeRotate = ROTATE_270;
        break;
    default:
        mEncodeRotate = -1;
        break;
    }

    int mHorizonFlipFlag = findMppVencIntParam(pDict, "mirror", index);
    int mColor2Grey = findMppVencIntParam(pDict, "color2grey", index);

    int mVbvBufferSize = findMppVencIntParam(pDict, "vbvBufferSize", index);
    int mVbvThreshSize = findMppVencIntParam(pDict, "vbvThreshSize", index);

    int mVeRefFrameLbcMode = -1;
    ptr = (char *)findMppVencStringParam(pDict, "ve_ref_frame_lbc_mode", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "lbc_disable"))
        {
            mVeRefFrameLbcMode = LBC_MODE_DISABLE;
        }
        else if (!strcmp(ptr, "lbc_1_5x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_1_5X;
        }
        else if (!strcmp(ptr, "lbc_2_0x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_2_0X;
        }
        else if (!strcmp(ptr, "lbc_2_5x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_2_5X;
        }
        else if (!strcmp(ptr, "lbc_1_0x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_NO_LOSSY;
        }
        else
        {
            mVeRefFrameLbcMode = -1;
            alogw("fatal error! wrong ve ref frame lbc mode:%s", ptr);
        }
    }
    else
    {
        mVeRefFrameLbcMode = -1;
    }

    VencTargetBitsClipParam mBitsClipParam;
    memset(&mBitsClipParam, 0, sizeof(VencTargetBitsClipParam));
    mBitsClipParam.dis_default_para = findMppVencIntParam(pDict, "bits_clip_dis_default", index);
    mBitsClipParam.mode = findMppVencIntParam(pDict, "bits_clip_mode", index);
    mBitsClipParam.coef_th[0][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[0][0]", index);
    mBitsClipParam.coef_th[0][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[0][1]", index);
    mBitsClipParam.coef_th[1][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[1][0]", index);
    mBitsClipParam.coef_th[1][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[1][1]", index);
    mBitsClipParam.coef_th[2][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[2][0]", index);
    mBitsClipParam.coef_th[2][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[2][1]", index);
    mBitsClipParam.coef_th[3][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[3][0]", index);
    mBitsClipParam.coef_th[3][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[3][1]", index);
    mBitsClipParam.coef_th[4][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[4][0]", index);
    mBitsClipParam.coef_th[4][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[4][1]", index);

    VencAeDiffParam mAeDiffParam;
    memset(&mAeDiffParam, 0, sizeof(VencAeDiffParam));
    mAeDiffParam.dis_default_para = findMppVencIntParam(pDict, "ae_diff_dis_default", index);
    mAeDiffParam.diff_frames_th = findMppVencIntParam(pDict, "ae_diff_frames_th", index);
    mAeDiffParam.stable_frames_th[0] = findMppVencIntParam(pDict, "ae_stable_frames_th[0]", index);
    mAeDiffParam.stable_frames_th[1] = findMppVencIntParam(pDict, "ae_stable_frames_th[1]", index);
    mAeDiffParam.diff_th[0] = (float)findMppVencDoubleParam(pDict, "ae_diff_th[0]", index);
    mAeDiffParam.diff_th[1] = (float)findMppVencDoubleParam(pDict, "ae_diff_th[1]", index);
    mAeDiffParam.small_diff_qp[0] = findMppVencIntParam(pDict, "ae_small_diff_qp[0]", index);
    mAeDiffParam.small_diff_qp[1] = findMppVencIntParam(pDict, "ae_small_diff_qp[1]", index);
    mAeDiffParam.large_diff_qp[0] = findMppVencIntParam(pDict, "ae_large_diff_qp[0]", index);
    mAeDiffParam.large_diff_qp[1] = findMppVencIntParam(pDict, "ae_large_diff_qp[1]", index);


    alogw("**********************************************************");
    alogw("mpp venc debug mode                                       ");
    alogw("**********************************************************");
    alogw("[params_name]              [user]               [debug]   ");
    alogw("venc_ch_id                 %-10d           %-10d          ", pVideoEncData->mMppChnInfo.mChnId, mVeChn);
    if (-1 != mOnlineEnable)
    {
        alogw("online_en                  %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mOnlineEnable, mOnlineEnable);
        pVideoEncData->mEncChnAttr.VeAttr.mOnlineEnable = mOnlineEnable;
    }
    if (-1 != mOnlineShareBufNum)
    {
        alogw("online_share_buf_num       %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mOnlineShareBufNum, mOnlineShareBufNum);
        pVideoEncData->mEncChnAttr.VeAttr.mOnlineShareBufNum = mOnlineShareBufNum;
    }
    if (-1 != mVideoEncoderFmt)
    {
        alogw("video_encoder              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.Type, mVideoEncoderFmt);
        pVideoEncData->mEncChnAttr.VeAttr.Type = mVideoEncoderFmt;
    }
    // update rc mode for other parameters
    mVideoEncoderFmt = pVideoEncData->mEncChnAttr.VeAttr.Type;

    if (-1 != mEncUseProfile)
    {
        if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("profile                    %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.Profile, mEncUseProfile);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.Profile = mEncUseProfile;
        }
        else if (PT_H265 == mVideoEncoderFmt)
        {
            alogw("profile                    %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mProfile, mEncUseProfile);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mProfile = mEncUseProfile;
        }
    }
    if (-1 != mEncLevel)
    {
        if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("level                      %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mLevel, mEncLevel);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mLevel = mEncLevel;
        }
        else if (PT_H265 == mVideoEncoderFmt)
        {
            alogw("level                      %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mLevel, mEncLevel);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mLevel = mEncLevel;
        }
    }
    if (-1 != mPixFmt)
    {
        alogw("pixfmt                     %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.PixelFormat, mPixFmt);
        pVideoEncData->mEncChnAttr.VeAttr.PixelFormat = mPixFmt;
    }
    if (-1 != mColorSpace)
    {
        alogw("color_space                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mColorSpace, mColorSpace);
        pVideoEncData->mEncChnAttr.VeAttr.mColorSpace = mColorSpace;
    }

    if (-1 != mEncppEnable)
    {
        alogw("encpp_enable               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.EncppAttr.mbEncppEnable, mEncppEnable);
        pVideoEncData->mEncChnAttr.EncppAttr.mbEncppEnable = mEncppEnable;
    }
    /*if (-1 != mEncppSharpAttenCoefPer)
    {
        alogw("encppSharpAttenCoefPer     %-10d           %-10d          ", pVideoEncData->mEncChnAttr.EncppAttr.mEncppSharpAttenCoefPer, mEncppSharpAttenCoefPer);
        pVideoEncData->mEncChnAttr.EncppAttr.mEncppSharpAttenCoefPer = mEncppSharpAttenCoefPer;
    }*/

    if (-1 != mSrcWidth)
    {
        alogw("src_width                  %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, mSrcWidth);
        pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth  = mSrcWidth;
    }
    if (-1 != mSrcHeight)
    {
        alogw("src_width                  %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight, mSrcHeight);
        pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight = mSrcHeight;
    }
    if (-1 != mVideoWidth)
    {
        if (PT_JPEG == mVideoEncoderFmt)
        {
            alogw("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicWidth = mVideoWidth;
        }
        else if (PT_MJPEG == mVideoEncoderFmt)
        {
            alogw("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth = mVideoWidth;
        }
        else if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth = mVideoWidth;
        }
        else if (PT_H265 == mVideoEncoderFmt)
        {
            alogw("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth = mVideoWidth;
        }
    }
    if (-1 != mVideoHeight)
    {
        if (PT_JPEG == mVideoEncoderFmt)
        {
            alogw("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicHeight = mVideoHeight;
        }
        else if (PT_MJPEG == mVideoEncoderFmt)
        {
            alogw("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = mVideoHeight;
        }
        else if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight = mVideoHeight;
        }
        else if (PT_H265 == mVideoEncoderFmt)
        {
            alogw("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight = mVideoHeight;
        }
    }

    if (-1 != mProductMode)
    {
        alogw("product_mode               %-10d           %-10d          ", pVideoEncData->mRcParam.product_mode, mProductMode);
        pVideoEncData->mRcParam.product_mode = mProductMode;
    }
    if (-1 != mSensorType)
    {
        alogw("sensor_type                %-10d           %-10d          ", pVideoEncData->mRcParam.sensor_type, mSensorType);
        pVideoEncData->mRcParam.sensor_type = mSensorType;
    }
    if (-1 != mKeyFrameInterval)
    {
        alogw("key_frame_interval         %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval, mKeyFrameInterval);
        pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval = mKeyFrameInterval;
    }
    if (-1 != mDropFrameNum)
    {
        alogw("drop_frm_num               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mDropFrameNum, mDropFrameNum);
        pVideoEncData->mEncChnAttr.VeAttr.mDropFrameNum = mDropFrameNum;
    }

    // rc mode depends on the video encoding type
    int mRcMode = findMppVencIntParam(pDict, "rc_mode", index);
    if (PT_H264 == mVideoEncoderFmt)
    {
        switch (mRcMode)
        {
        case 0:
            mRcMode = VENC_RC_MODE_H264CBR;
            break;
        case 1:
            mRcMode = VENC_RC_MODE_H264VBR;
            break;
        case 2:
            mRcMode = VENC_RC_MODE_H264FIXQP;
            break;
        case 3:
            mRcMode = VENC_RC_MODE_H264QPMAP;
            break;
        default:
            mRcMode = -1;
            break;
        }
    }
    else if (PT_H265 == mVideoEncoderFmt)
    {
        switch (mRcMode)
        {
        case 0:
            mRcMode = VENC_RC_MODE_H265CBR;
            break;
        case 1:
            mRcMode = VENC_RC_MODE_H265VBR;
            break;
        case 2:
            mRcMode = VENC_RC_MODE_H265FIXQP;
            break;
        case 3:
            mRcMode = VENC_RC_MODE_H265QPMAP;
            break;
        default:
            mRcMode = -1;
            break;
        }
    }
    else if (PT_MJPEG == mVideoEncoderFmt)
    {
        switch (mRcMode)
        {
        case 0:
            mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        case 2:
            mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            break;
        default:
            mRcMode = -1;
            break;
        }
    }
    else
    {
        mRcMode = -1;
    }

    if (-1 != mRcMode)
    {
        alogw("rc_mode                    %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mRcMode, mRcMode);
        pVideoEncData->mEncChnAttr.RcAttr.mRcMode = mRcMode;
    }
    // update rc mode for other parameters
    mRcMode = pVideoEncData->mEncChnAttr.RcAttr.mRcMode;

    if (VENC_RC_MODE_H264CBR == mRcMode)
    {
        if (-1 != mInitQp)
        {
            alogw("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mQpInit = mInitQp;
        }
        if (-1 != mMinIQp)
        {
            alogw("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMinQp = mMinIQp;
        }
        if (-1 != mMaxIQp)
        {
            alogw("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp = mMaxIQp;
        }
        if (-1 != mMinPQp)
        {
            alogw("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMinPqp = mMinPQp;
        }
        if (-1 != mMaxPQp)
        {
            alogw("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMaxPqp = mMaxPQp;
        }
        if (-1 != mEnMbQpLimit)
        {
            alogw("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH264Cbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
    }
    else if (VENC_RC_MODE_H264VBR == mRcMode)
    {
        if (-1 != mInitQp)
        {
            alogw("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mQpInit = mInitQp;
        }
        if (-1 != mMinIQp)
        {
            alogw("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMinQp = mMinIQp;
        }
        if (-1 != mMaxIQp)
        {
            alogw("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp = mMaxIQp;
        }
        if (-1 != mMinPQp)
        {
            alogw("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMinPqp = mMinPQp;
        }
        if (-1 != mMaxPQp)
        {
            alogw("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMaxPqp = mMaxPQp;
        }
        if (-1 != mEnMbQpLimit)
        {
            alogw("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH264Vbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (-1 != mMovingTh)
        {
            alogw("moving_th                  %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMovingTh, mMovingTh);
            pVideoEncData->mRcParam.ParamH264Vbr.mMovingTh = mMovingTh;
        }
        if (-1 != mQuality)
        {
            alogw("quality                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mQuality, mQuality);
            pVideoEncData->mRcParam.ParamH264Vbr.mQuality = mQuality;
        }
        if (-1 != mIBitsCoef)
        {
            alogw("i_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mIFrmBitsCoef, mIBitsCoef);
            pVideoEncData->mRcParam.ParamH264Vbr.mIFrmBitsCoef = mIBitsCoef;
        }
        if (-1 != mPBitsCoef)
        {
            alogw("p_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mPFrmBitsCoef, mPBitsCoef);
            pVideoEncData->mRcParam.ParamH264Vbr.mPFrmBitsCoef = mPBitsCoef;
        }
    }
    else if (VENC_RC_MODE_H264QPMAP == mRcMode)
    {
        if (-1 != mInitQp)
        {
            alogw("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mQpInit = mInitQp;
        }
        if (-1 != mMinIQp)
        {
            alogw("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMinQp = mMinIQp;
        }
        if (-1 != mMaxIQp)
        {
            alogw("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp = mMaxIQp;
        }
        if (-1 != mMinPQp)
        {
            alogw("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMinPqp = mMinPQp;
        }
        if (-1 != mMaxPQp)
        {
            alogw("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMaxPqp = mMaxPQp;
        }
        if (-1 != mEnMbQpLimit)
        {
            alogw("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH264QpMap.mbEnMbQpLimit = mEnMbQpLimit;
        }
    }
    else if (VENC_RC_MODE_H265CBR == mRcMode)
    {
        if (-1 != mInitQp)
        {
            alogw("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mQpInit = mInitQp;
        }
        if (-1 != mMinIQp)
        {
            alogw("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMinQp = mMinIQp;
        }
        if (-1 != mMaxIQp)
        {
            alogw("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp = mMaxIQp;
        }
        if (-1 != mMinPQp)
        {
            alogw("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMinPqp = mMinPQp;
        }
        if (-1 != mMaxPQp)
        {
            alogw("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMaxPqp = mMaxPQp;
        }
        if (-1 != mEnMbQpLimit)
        {
            alogw("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH265Cbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
    }
    else if (VENC_RC_MODE_H265VBR == mRcMode)
    {
        if (-1 != mInitQp)
        {
            alogw("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mQpInit = mInitQp;
        }
        if (-1 != mMinIQp)
        {
            alogw("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMinQp = mMinIQp;
        }
        if (-1 != mMaxIQp)
        {
            alogw("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp = mMaxIQp;
        }
        if (-1 != mMinPQp)
        {
            alogw("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMinPqp = mMinPQp;
        }
        if (-1 != mMaxPQp)
        {
            alogw("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMaxPqp = mMaxPQp;
        }
        if (-1 != mEnMbQpLimit)
        {
            alogw("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH265Vbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (-1 != mMovingTh)
        {
            alogw("moving_th                  %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMovingTh, mMovingTh);
            pVideoEncData->mRcParam.ParamH265Vbr.mMovingTh = mMovingTh;
        }
        if (-1 != mQuality)
        {
            alogw("quality                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mQuality, mQuality);
            pVideoEncData->mRcParam.ParamH265Vbr.mQuality = mQuality;
        }
        if (-1 != mIBitsCoef)
        {
            alogw("i_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mIFrmBitsCoef, mIBitsCoef);
            pVideoEncData->mRcParam.ParamH265Vbr.mIFrmBitsCoef = mIBitsCoef;
        }
        if (-1 != mPBitsCoef)
        {
            alogw("p_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mPFrmBitsCoef, mPBitsCoef);
            pVideoEncData->mRcParam.ParamH265Vbr.mPFrmBitsCoef = mPBitsCoef;
        }
    }
    else if (VENC_RC_MODE_H265QPMAP == mRcMode)
    {
        if (-1 != mInitQp)
        {
            alogw("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mQpInit = mInitQp;
        }
        if (-1 != mMinIQp)
        {
            alogw("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMinQp = mMinIQp;
        }
        if (-1 != mMaxIQp)
        {
            alogw("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp = mMaxIQp;
        }
        if (-1 != mMinPQp)
        {
            alogw("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMinPqp = mMinPQp;
        }
        if (-1 != mMaxPQp)
        {
            alogw("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMaxPqp = mMaxPQp;
        }
        if (-1 != mEnMbQpLimit)
        {
            alogw("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH265QpMap.mbEnMbQpLimit = mEnMbQpLimit;
        }
    }

    if (-1 != mSrcFrameRate)
    {
        alogw("src_framerate              %-10d           %-10d          ", pVideoEncData->mFrameRateInfo.SrcFrmRate, mSrcFrameRate);
        pVideoEncData->mFrameRateInfo.SrcFrmRate = mSrcFrameRate;
    }
    if (-1 != mVideoFrameRate)
    {
        alogw("dst_framerate              %-10d           %-10d          ", pVideoEncData->mFrameRateInfo.DstFrmRate, mVideoFrameRate);
        pVideoEncData->mFrameRateInfo.DstFrmRate = mVideoFrameRate;
    }
    if (-1 != mVideoBitRate)
    {
        if (PT_JPEG != mVideoEncoderFmt)
        {
            if (VENC_RC_MODE_H264CBR == mRcMode)
            {
                alogw("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate, mVideoBitRate);
                pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = mVideoBitRate;
            }
            else if (VENC_RC_MODE_H264VBR == mRcMode)
            {
                alogw("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate, mVideoBitRate);
                pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = mVideoBitRate;
            }
            else if (VENC_RC_MODE_H264QPMAP == mRcMode)
            {
                alogw("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264QpMap.mMaxBitRate, mVideoBitRate);
                pVideoEncData->mEncChnAttr.RcAttr.mAttrH264QpMap.mMaxBitRate = mVideoBitRate;
            }
            else if (VENC_RC_MODE_H265CBR == mRcMode)
            {
                alogw("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate, mVideoBitRate);
                pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate = mVideoBitRate;
            }
            else if (VENC_RC_MODE_H265VBR == mRcMode)
            {
                alogw("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate, mVideoBitRate);
                pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = mVideoBitRate;
            }
            else if (VENC_RC_MODE_H265QPMAP == mRcMode)
            {
                alogw("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265QpMap.mMaxBitRate, mVideoBitRate);
                pVideoEncData->mEncChnAttr.RcAttr.mAttrH265QpMap.mMaxBitRate = mVideoBitRate;
            }
            else if (VENC_RC_MODE_MJPEGCBR == mRcMode)
            {
                alogw("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate, mVideoBitRate);
                pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = mVideoBitRate;
            }
        }
    }

    if (-1 != mPFrameIntraEn)
    {
        if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("p_frame_intra_en           %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable, mPFrameIntraEn);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = mPFrameIntraEn;
        }
        else if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("p_frame_intra_en           %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable, mPFrameIntraEn);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = mPFrameIntraEn;
        }
    }
    if (-1 != mEnableFastEnc)
    {
        if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("enable_fast_enc            %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.FastEncFlag, mEnableFastEnc);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.FastEncFlag = mEnableFastEnc;
        }
        else if (PT_H265 == mVideoEncoderFmt)
        {
            alogw("enable_fast_enc            %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mFastEncFlag, mEnableFastEnc);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mFastEncFlag = mEnableFastEnc;
        }
    }

    if (-1 != mGopMode)
    {
        alogw("gop_mode                   %-10d           %-10d          ", pVideoEncData->mEncChnAttr.GopAttr.enGopMode, mGopMode);
        pVideoEncData->mEncChnAttr.GopAttr.enGopMode = mGopMode;    
    }
    if (-1 != mGopSize)
    {
        alogw("gop_size                   %-10d           %-10d          ", pVideoEncData->mEncChnAttr.GopAttr.mGopSize, mGopSize);
        pVideoEncData->mEncChnAttr.GopAttr.mGopSize = mGopSize;
    }

    if ((PT_H264 == mVideoEncoderFmt) || (PT_H265 == mVideoEncoderFmt))
    {
        if (-1 != m2DnrEnable)
        {
            alogw("2dnr_en                    %-10d           %-10d          ", pVideoEncData->m2DfilterParam.enable_2d_filter, m2DnrEnable);
            pVideoEncData->m2DfilterParam.enable_2d_filter = m2DnrEnable;
        }
        if (0 < m2DnrEnable)
        {
            if (-1 != m2DnrStrengthY)
            {
                alogw("2dnr_strength_y            %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_strength_y, m2DnrStrengthY);
                pVideoEncData->m2DfilterParam.filter_strength_y = m2DnrStrengthY;
            }
            if (-1 != m2DnrStrengthUV)
            {
                alogw("2dnr_strength_c            %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_strength_uv, m2DnrStrengthUV);
                pVideoEncData->m2DfilterParam.filter_strength_uv = m2DnrStrengthUV;
            }
            if (-1 != m2DnrThY)
            {
                alogw("2dnr_threshold_y           %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_th_y, m2DnrThY);
                pVideoEncData->m2DfilterParam.filter_th_y = m2DnrThY;
            }
            if (-1 != m2DnrThUV)
            {
                alogw("2dnr_threshold_c           %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_th_uv, m2DnrThUV);
                pVideoEncData->m2DfilterParam.filter_th_uv = m2DnrThUV;
            }
        }

        if (-1 != m3DnrEnable)
        {
            alogw("3dnr_en                    %-10d           %-10d          ", pVideoEncData->m3DfilterParam.enable_3d_filter, m3DnrEnable);
            pVideoEncData->m3DfilterParam.enable_3d_filter = m3DnrEnable;
        }
        if (0 < m3DnrEnable)
        {
            if (-1 != m3DnrAdjustPixLevelEnable)
            {
                alogw("3dnr_pix_level_en          %-10d           %-10d          ", pVideoEncData->m3DfilterParam.adjust_pix_level_enable, m3DnrAdjustPixLevelEnable);
                pVideoEncData->m3DfilterParam.adjust_pix_level_enable = m3DnrAdjustPixLevelEnable;
            }
            if (-1 != m3DnrSmoothFilterEnable)
            {
                alogw("3dnr_smooth_en             %-10d           %-10d          ", pVideoEncData->m3DfilterParam.smooth_filter_enable, m3DnrSmoothFilterEnable);
                pVideoEncData->m3DfilterParam.smooth_filter_enable = m3DnrSmoothFilterEnable;
            }
            if (-1 != m3DnrMaxPixDiffTh)
            {
                alogw("3dnr_pix_diff_th           %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_pix_diff_th, m3DnrMaxPixDiffTh);
                pVideoEncData->m3DfilterParam.max_pix_diff_th = m3DnrMaxPixDiffTh;
            }
            if (-1 != m3DnrMaxMvTh)
            {
                alogw("3dnr_max_mv_th             %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_mv_th, m3DnrMaxMvTh);
                pVideoEncData->m3DfilterParam.max_mv_th = m3DnrMaxMvTh;
            }
            if (-1 != m3DnrMaxMadTh)
            {
                alogw("3dnr_max_mad_th            %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_mad_th, m3DnrMaxMadTh);
                pVideoEncData->m3DfilterParam.max_mad_th = m3DnrMaxMadTh;
            }
            if (-1 != m3DnrMinCoef)
            {
                alogw("3dnr_min_coef              %-10d           %-10d          ", pVideoEncData->m3DfilterParam.min_coef, m3DnrMinCoef);
                pVideoEncData->m3DfilterParam.min_coef = m3DnrMinCoef;
            }
            if (-1 != m3DnrMaxCoef)
            {
                alogw("3dnr_max_coef              %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_coef, m3DnrMaxCoef);
                pVideoEncData->m3DfilterParam.max_coef = m3DnrMaxCoef;
            }
        }
    }

    if (-1 != mCropEnable)
    {
        alogw("crop_en                    %-10d           %-10d          ", pVideoEncData->mCropCfg.bEnable, mCropEnable);
        pVideoEncData->mCropCfg.bEnable = mCropEnable;
    }
    if (0 < mCropEnable)
    {
        if (-1 != mCropRectX)
        {
            alogw("crop_rect_x                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.X, mCropRectX);
            pVideoEncData->mCropCfg.Rect.X = mCropRectX;
        }
        if (-1 != mCropRectY)
        {
            alogw("crop_rect_y                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.Y, mCropRectY);
            pVideoEncData->mCropCfg.Rect.Y = mCropRectY;
        }
        if (-1 != mCropRectWidth)
        {
            alogw("crop_rect_w                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.Width, mCropRectWidth);
            pVideoEncData->mCropCfg.Rect.Width = mCropRectWidth;
        }
        if (-1 != mCropRectHeight)
        {
            alogw("crop_rect_h                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.Height, mCropRectHeight);
            pVideoEncData->mCropCfg.Rect.Height = mCropRectHeight;
        }
    }

    if (0 <= mSuperFrmMode)
    {
        if ((pVideoEncData->mSuperFrmParam.enSuperFrmMode != mSuperFrmMode) ||
            (pVideoEncData->mSuperFrmParam.SuperIFrmBitsThr != mSuperIFrmBitsThr) ||
            (pVideoEncData->mSuperFrmParam.SuperPFrmBitsThr != mSuperPFrmBitsThr))
        {
            VencSuperFrameConfig stSuperFrameConfig;
            memset(&stSuperFrameConfig, 0, sizeof(VencSuperFrameConfig));
            switch (mSuperFrmMode)
            {
                case SUPERFRM_NONE:
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_NONE;
                    break;
                case SUPERFRM_DISCARD:
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_DISCARD;
                    break;
                case SUPERFRM_REENCODE:
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_REENCODE;
                    break;
                default:
                    aloge("fatal error! wrong superFrmMode[0x%x]", mSuperFrmMode);
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_NONE;
                    break;
            }
            stSuperFrameConfig.nMaxIFrameBits = mSuperIFrmBitsThr;
            stSuperFrameConfig.nMaxPFrameBits = mSuperPFrmBitsThr;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSuperFrameConfig, (void*)&stSuperFrameConfig);
        }
    }
    if (-1 != mSuperFrmMode)
    {
        alogw("super_frm_mode             %-10d           %-10d          ", pVideoEncData->mSuperFrmParam.enSuperFrmMode, mSuperFrmMode);
        pVideoEncData->mSuperFrmParam.enSuperFrmMode = mSuperFrmMode;
    }
    if (0 <= mSuperFrmMode)
    {
        if (-1 != mSuperIFrmBitsThr)
        {
            alogw("super_i_frm_bits_thr       %-10d           %-10d          ", pVideoEncData->mSuperFrmParam.SuperIFrmBitsThr, mSuperIFrmBitsThr);
            pVideoEncData->mSuperFrmParam.SuperIFrmBitsThr = mSuperIFrmBitsThr;
        }
        if (-1 != mSuperPFrmBitsThr)
        {
            alogw("super_p_frm_bits_thr       %-10d           %-10d          ", pVideoEncData->mSuperFrmParam.SuperPFrmBitsThr, mSuperPFrmBitsThr);
            pVideoEncData->mSuperFrmParam.SuperPFrmBitsThr = mSuperPFrmBitsThr;
        }
    }

    if (-1 != mEncodeRotate)
    {
        alogw("encode_rotate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.Rotate, mEncodeRotate);
        pVideoEncData->mEncChnAttr.VeAttr.Rotate = mEncodeRotate;
    }
    if (-1 != mHorizonFlipFlag)
    {
        alogw("mirror                     %-10d           %-10d          ", pVideoEncData->mHorizonFlipFlag, mHorizonFlipFlag);
        if (pVideoEncData->mHorizonFlipFlag != mHorizonFlipFlag)
        {
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamHorizonFlip, (void*)&mHorizonFlipFlag);
        }
        pVideoEncData->mHorizonFlipFlag = mHorizonFlipFlag;
    }

    if (-1 != mColor2Grey)
    {
        if ((PT_H264 == mVideoEncoderFmt) || (PT_H265 == mVideoEncoderFmt))
        {
            alogw("color2grey                 %-10d           %-10d          ", pVideoEncData->mColor2GreyParam.bColor2Grey, mColor2Grey);
            pVideoEncData->mColor2GreyParam.bColor2Grey = mColor2Grey;
        }
    }

    if (-1 != mVbvBufferSize)
    {
        if (PT_JPEG == mVideoEncoderFmt)
        {
            alogw("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize = mVbvBufferSize;
        }
        else if (PT_MJPEG == mVideoEncoderFmt)
        {
            alogw("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mBufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mBufSize = mVbvBufferSize;
        }
        else if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize = mVbvBufferSize;
        }
        else if (PT_H265 == mVideoEncoderFmt)
        {
            alogw("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mBufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mBufSize = mVbvBufferSize;
        }
    }
    if (-1 != mVbvThreshSize)
    {
        if (PT_JPEG == mVideoEncoderFmt)
        {
            alogw("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize = mVbvThreshSize;
        }
        else if (PT_MJPEG == mVideoEncoderFmt)
        {
            alogw("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mThreshSize = mVbvThreshSize;
        }
        else if (PT_H264 == mVideoEncoderFmt)
        {
            alogw("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize = mVbvThreshSize;
        }
        else if (PT_H265 == mVideoEncoderFmt)
        {
            alogw("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mThreshSize = mVbvThreshSize;
        }
    }

    if (-1 != mVeRefFrameLbcMode)
    {
        alogw("ve_ref_frame_lbc_mode      %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mVeRefFrameLbcMode, mVeRefFrameLbcMode);
        pVideoEncData->mEncChnAttr.VeAttr.mVeRefFrameLbcMode = mVeRefFrameLbcMode;
    }

    if (-1 != mBitsClipParam.dis_default_para)
    {
        alogw("bits_clip_dis_default      %-10d           %-10d          ", pVideoEncData->mRcParam.mBitsClipParam.dis_default_para, mBitsClipParam.dis_default_para);
        pVideoEncData->mRcParam.mBitsClipParam.dis_default_para = mBitsClipParam.dis_default_para;
    }
    if (-1 != mBitsClipParam.mode)
    {
        alogw("mode                       %-10d           %-10d          ", pVideoEncData->mRcParam.mBitsClipParam.mode, mBitsClipParam.mode);
        pVideoEncData->mRcParam.mBitsClipParam.mode = mBitsClipParam.mode;
    }
    if (-1 != mBitsClipParam.coef_th[0][0])
    {
        alogw("bits_clip_coef[0][0]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][0], mBitsClipParam.coef_th[0][0]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][0] = mBitsClipParam.coef_th[0][0];
    }
    if (-1 != mBitsClipParam.coef_th[0][1])
    {
        alogw("bits_clip_coef[0][1]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][1], mBitsClipParam.coef_th[0][1]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][1] = mBitsClipParam.coef_th[0][1];
    }
    if (-1 != mBitsClipParam.coef_th[1][0])
    {
        alogw("bits_clip_coef[1][0]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][0], mBitsClipParam.coef_th[1][0]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][0] = mBitsClipParam.coef_th[1][0];
    }
    if (-1 != mBitsClipParam.coef_th[1][1])
    {
        alogw("bits_clip_coef[1][1]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][1], mBitsClipParam.coef_th[1][1]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][1] = mBitsClipParam.coef_th[1][1];
    }
    if (-1 != mBitsClipParam.coef_th[2][0])
    {
        alogw("bits_clip_coef[2][1]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][0], mBitsClipParam.coef_th[2][0]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][0] = mBitsClipParam.coef_th[2][0];
    }
    if (-1 != mBitsClipParam.coef_th[2][1])
    {
        alogw("bits_clip_coef[2][1]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][1], mBitsClipParam.coef_th[2][1]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][1] = mBitsClipParam.coef_th[2][1];
    }
    if (-1 != mBitsClipParam.coef_th[3][0])
    {
        alogw("bits_clip_coef[3][1]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][0], mBitsClipParam.coef_th[3][0]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][0] = mBitsClipParam.coef_th[3][0];
    }
    if (-1 != mBitsClipParam.coef_th[3][1])
    {
        alogw("bits_clip_coef[3][1]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][1], mBitsClipParam.coef_th[3][1]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][1] = mBitsClipParam.coef_th[3][1];
    }
    if (-1 != mBitsClipParam.coef_th[4][0])
    {
        alogw("bits_clip_coef[4][0]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][0], mBitsClipParam.coef_th[4][0]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][0] = mBitsClipParam.coef_th[4][0];
    }
    if (-1 != mBitsClipParam.coef_th[4][1])
    {
        alogw("bits_clip_coef[4][1]       %-10f           %-10f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][1], mBitsClipParam.coef_th[4][1]);
        pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][1] = mBitsClipParam.coef_th[4][1];
    }

    if (-1 != mAeDiffParam.dis_default_para)
    {
        alogw("ae_diff_dis_default        %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.dis_default_para, mAeDiffParam.dis_default_para);
        pVideoEncData->mRcParam.mAeDiffParam.dis_default_para = mAeDiffParam.dis_default_para;
    }
    if (-1 != mAeDiffParam.diff_frames_th)
    {
        alogw("ae_diff_frames_th          %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.diff_frames_th, mAeDiffParam.diff_frames_th);
        pVideoEncData->mRcParam.mAeDiffParam.diff_frames_th = mAeDiffParam.diff_frames_th;
    }
    if (-1 != mAeDiffParam.stable_frames_th[0])
    {
        alogw("ae_stable_frames_th[0]     %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.stable_frames_th[0], mAeDiffParam.stable_frames_th[0]);
        pVideoEncData->mRcParam.mAeDiffParam.stable_frames_th[0] = mAeDiffParam.stable_frames_th[0];
    }
    if (-1 != mAeDiffParam.stable_frames_th[1])
    {
        alogw("ae_stable_frames_th[1]     %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.stable_frames_th[1], mAeDiffParam.stable_frames_th[1]);
        pVideoEncData->mRcParam.mAeDiffParam.stable_frames_th[1] = mAeDiffParam.stable_frames_th[1];
    }
    if (-1 != mAeDiffParam.diff_th[0])
    {
        alogw("ae_diff_th[0]              %-10f           %-10f          ", pVideoEncData->mRcParam.mAeDiffParam.diff_th[0], mAeDiffParam.diff_th[0]);
        pVideoEncData->mRcParam.mAeDiffParam.diff_th[0] = mAeDiffParam.diff_th[0];
    }
    if (-1 != mAeDiffParam.diff_th[1])
    {
        alogw("ae_diff_th[1]              %-10f           %-10f          ", pVideoEncData->mRcParam.mAeDiffParam.diff_th[1], mAeDiffParam.diff_th[1]);
        pVideoEncData->mRcParam.mAeDiffParam.diff_th[1] = mAeDiffParam.diff_th[1];
    }
    if (-1 != mAeDiffParam.small_diff_qp[0])
    {
        alogw("ae_small_diff_qp[0]        %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.small_diff_qp[0], mAeDiffParam.small_diff_qp[0]);
        pVideoEncData->mRcParam.mAeDiffParam.small_diff_qp[0] = mAeDiffParam.small_diff_qp[0];
    }
    if (-1 != mAeDiffParam.small_diff_qp[1])
    {
        alogw("ae_small_diff_qp[1]        %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.small_diff_qp[1], mAeDiffParam.small_diff_qp[1]);
        pVideoEncData->mRcParam.mAeDiffParam.small_diff_qp[1] = mAeDiffParam.small_diff_qp[1];
    }
    if (-1 != mAeDiffParam.large_diff_qp[0])
    {
        alogw("ae_large_diff_qp[0]        %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.large_diff_qp[0], mAeDiffParam.large_diff_qp[0]);
        pVideoEncData->mRcParam.mAeDiffParam.large_diff_qp[0] = mAeDiffParam.large_diff_qp[0];
    }
    if (-1 != mAeDiffParam.large_diff_qp[1])
    {
        alogw("ae_large_diff_qp[1]        %-10d           %-10d          ", pVideoEncData->mRcParam.mAeDiffParam.large_diff_qp[1], mAeDiffParam.large_diff_qp[1]);
        pVideoEncData->mRcParam.mAeDiffParam.large_diff_qp[1] = mAeDiffParam.large_diff_qp[1];
    }

    alogw("**********************************************************");

    iniparser_freedict(pDict);
}

static int findMppMuxParamIndex(dictionary *pDict, int mMuxGrp)
{
    char *str = NULL;
    char paramkey[128];
    int index = -1;

    if (NULL == pDict)
    {
        aloge("fatal error, invalid params! %p", pDict);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "mux_params:mux_group_id");

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_MUX_PARAM_CNT_MAX > i))
        {
            if (mMuxGrp == (int)(atoi(strPart)))
            {
                index = i;
                alogd("mux_params index:%d, mux_group_id %d", index, mMuxGrp);
                break;
            }
            strPart = strtok(NULL, ",");
            i++;
        }
    }
    else
    {
        alogw("mux_params, key mux_group_id is not found.");
        index = -1;
    }

    return index;
}

static int findMppMuxIntParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    int paramint[MPP_MUX_PARAM_CNT_MAX];
    int result = 0;

    if ((NULL == pDict) || (MPP_MUX_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "mux_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_MUX_PARAM_CNT_MAX > i))
        {
            paramint[i++] = atoi(strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramint[index];
        alogv("mux_params index:%d, key %s result %d", index, key, result);
    }
    else
    {
        alogv("mux_params index:%d, key %s is not found.", index, key);
        result = -1;
    }

    return result;
}

static char* findMppMuxStringParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    char paramstr[MPP_MUX_PARAM_CNT_MAX][64];
    char *result = NULL;

    if ((NULL == pDict) || (MPP_MUX_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return NULL;
    }

    snprintf(paramkey, sizeof(paramkey), "mux_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_MUX_PARAM_CNT_MAX > i))
        {
            strcpy(paramstr[i++], strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramstr[index];
        alogv("mux_params index:%d, key %s result %s", index, key, result);
    }
    else
    {
        alogv("mux_params index:%d, key %s is not found.", index, key);
        result = NULL;
    }

    return result;
}

void loadMppMuxParams(RECRENDERDATATYPE *pRecRenderData, const char *pConfPath)
{
    char *ptr = NULL;
    dictionary *pDict = NULL;

    if (NULL == pRecRenderData)
    {
        aloge("fatal error, invalid params! %p", pRecRenderData);
        return;
    }

    pDict = getDictByConfPath(pConfPath);
    if (NULL == pDict)
    {
        //aloge("fatal error! get Dict By MediaDebugConfPath fail!");
        return;
    }

    int index = findMppMuxParamIndex(pDict, pRecRenderData->mMppChnInfo.mChnId);
    if (-1 == index)
    {
        alogw("mMuxGrp: %d is not found.", pRecRenderData->mMppChnInfo.mChnId);
        iniparser_freedict(pDict);
        return;
    }
    int mMuxGrp = pRecRenderData->mMppChnInfo.mChnId;
    int mVideoAttrValidNum = 1;//findMppMuxIntParam(pDict, "video_attr_valid_num", index);    

    int mVideoEncoderFmt = -1;
    ptr = (char *)findMppMuxStringParam(pDict, "video_encoder", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "H264"))
        {
            mVideoEncoderFmt = PT_H264;
        }
        else if (!strcmp(ptr, "H265"))
        {
            mVideoEncoderFmt = PT_H265;
        }
        else if (!strcmp(ptr, "MJPEG"))
        {
            mVideoEncoderFmt = PT_MJPEG;
        }
        else if (!strcmp(ptr, "JPEG"))
        {
            mVideoEncoderFmt = PT_JPEG;
        }
        else
        {
            mVideoEncoderFmt = -1;
            alogw("fatal error! wrong encoder type:%s", ptr);
        }
    }
    else
    {
        mVideoEncoderFmt = -1;
    }

    int mVideoWidth = findMppMuxIntParam(pDict, "video_width", index);
    int mVideoHeight = findMppMuxIntParam(pDict, "video_height", index);
    int mVideoFrameRate = findMppMuxIntParam(pDict, "video_framerate", index);
    int mVeChn = findMppMuxIntParam(pDict, "venc_ch_id", index);

    if (pRecRenderData->mGroupAttr.mVideoAttrValidNum != mVideoAttrValidNum)
    {
        alogw("update VideoAttrValidNum from %d to %d", pRecRenderData->mGroupAttr.mVideoAttrValidNum, mVideoAttrValidNum);
        pRecRenderData->mGroupAttr.mVideoAttrValidNum = mVideoAttrValidNum;
    }

    int mVideoInfoIndex = 0;

    alogw("**********************************************************");
    alogw("mpp mux debug mode                                        ");
    alogw("**********************************************************");
    alogw("[params_name]              [user]               [debug]   ");
    alogw("mux_group_id               %-10d           %-10d          ", pRecRenderData->mMppChnInfo.mChnId, mMuxGrp);
    if (-1 != mVideoEncoderFmt)
    {
        alogw("video_encoder              %-10d           %-10d          ", pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mVideoEncodeType, mVideoEncoderFmt);
        pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mVideoEncodeType = mVideoEncoderFmt;
    }
    if (-1 != mVideoWidth)
    {
        alogw("video_width                %-10d           %-10d          ", pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mWidth, mVideoWidth);
        pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mWidth = mVideoWidth;
    }
    if (-1 != mVideoHeight)
    {
        alogw("video_height               %-10d           %-10d          ", pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mHeight, mVideoHeight);
        pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mHeight = mVideoHeight;
    }
    if (-1 != mVideoFrameRate)
    {
        alogw("video_framerate            %-10d           %-10d          ", pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mVideoFrmRate/1000, mVideoFrameRate);
        pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mVideoFrmRate = mVideoFrameRate*1000;
    }
    if (-1 != mVeChn)
    {
        alogw("venc_ch_id                 %-10d           %-10d          ", pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mVeChn, mVeChn);
        pRecRenderData->mGroupAttr.mVideoAttr[mVideoInfoIndex].mVeChn = mVeChn;
    }
    alogw("**********************************************************");

    iniparser_freedict(pDict);
}


