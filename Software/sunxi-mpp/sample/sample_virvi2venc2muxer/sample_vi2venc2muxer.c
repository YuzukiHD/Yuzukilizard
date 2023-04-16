/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleVirVi2Venc2Muxer"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "plat_log.h"
#include <mm_common.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_region.h>
#include <mpi_vi_private.h>
#include "sample_vi2venc2muxer.h"
#include "sample_vi2venc2muxer_conf.h"

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)
//#define DOUBLE_ENCODER_FILE_OUT
#define ISP_RUN (1)

static SAMPLE_VI2VENC2MUXER_S *gpVi2Venc2MuxerData;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpVi2Venc2MuxerData)
    {
        cdx_sem_up(&gpVi2Venc2MuxerData->mSemExit);
    }
}

static int setOutputFileSync(SAMPLE_VI2VENC2MUXER_S *pContext, char* path, int64_t fallocateLength, int muxerId);


static ERRORTYPE InitVi2Venc2MuxerData(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }
    memset(pContext, 0, sizeof(SAMPLE_VI2VENC2MUXER_S));
    pContext->mMuxGrp = MM_INVALID_CHN;
    pContext->mVeChn = MM_INVALID_CHN;
    pContext->mViChn = MM_INVALID_CHN;
    pContext->mViDev = MM_INVALID_DEV;

    int i=0;
    for (i = 0; i < 2; i++)
    {
        INIT_LIST_HEAD(&pContext->mMuxerFileListArray[i]);
    }
    alogd("&pContext->mMuxerFileListArray[0][%p], &pContext->mMuxerFileListArray[1][%p]",
        &pContext->mMuxerFileListArray[0], &pContext->mMuxerFileListArray[1]);

    pContext->mCurrentState = REC_NOT_PREPARED;

    if (message_create(&pContext->mMsgQueue) < 0)
    {
        aloge("message create fail!");
        return FAILURE;
    }

    return SUCCESS;
}

static ERRORTYPE parseCmdLine(SAMPLE_VI2VENC2MUXER_S *pContext, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

    if(argc <= 1)
    {
        alogd("use default config.");
        return SUCCESS;
    }
    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = SUCCESS;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_vi2venc2muxer.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_VI2VENC2MUXER_S *pContext, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;

    if (conf_path != NULL)
    {
        CONFPARSER_S mConf;
        memset(&mConf, 0, sizeof(CONFPARSER_S));
        ret = createConfParser(conf_path, &mConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        pContext->mConfigPara.mVippDev = GetConfParaInt(&mConf, CFG_VIPP_DEV_ID, 0);
        pContext->mConfigPara.mVeChn = GetConfParaInt(&mConf, CFG_VENC_CH_ID, 0);
        alogd("vippDev: %d, veChn: %d", pContext->mConfigPara.mVippDev, pContext->mConfigPara.mVeChn);

        pContext->mConfigPara.srcWidth = GetConfParaInt(&mConf, CFG_SRC_WIDTH, 0);
        pContext->mConfigPara.srcHeight = GetConfParaInt(&mConf, CFG_SRC_HEIGHT, 0);
        alogd("srcWidth: %d, srcHeight: %d", pContext->mConfigPara.srcWidth, pContext->mConfigPara.srcHeight);

        pContext->mConfigPara.dstWidth = GetConfParaInt(&mConf, CFG_DST_VIDEO_WIDTH, 0);
        pContext->mConfigPara.dstHeight = GetConfParaInt(&mConf, CFG_DST_VIDEO_HEIGHT, 0);
        alogd("dstWidth: %d, dstHeight: %d", pContext->mConfigPara.dstWidth, pContext->mConfigPara.dstHeight);

        ptr = (char *)GetConfParaString(&mConf, CFG_SRC_PIXFMT, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "nv21"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yv12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            }
            else if (!strcmp(ptr, "nv12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yu12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            }
            else if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_5x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_0x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
            }
            else
            {
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pContext->mConfigPara.srcPixFmt);
            }
        }

        ptr = (char *)GetConfParaString(&mConf, CFG_COLOR_SPACE, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "jpeg"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
            }
            else if (!strcmp(ptr, "rec709"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709;
            }
            else if (!strcmp(ptr, "rec709_part_range"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
            }
            else
            {
                aloge("fatal error! wrong color space:%s", ptr);
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
            }
        }

        alogd("srcPixFmt=%d, ColorSpace=%d", pContext->mConfigPara.srcPixFmt, pContext->mConfigPara.mColorSpace);

        pContext->mConfigPara.mSaturationChange = GetConfParaInt(&mConf, CFG_SATURATION_CHANGE, 0);
        alogd("SaturationChange=%d", pContext->mConfigPara.mSaturationChange);
        
        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->mConfigPara.dstVideoFile, ptr);
        }

        pContext->mConfigPara.mbAddRepairInfo = GetConfParaInt(&mConf, CFG_ADD_REPAIR_INFO, 0);
        pContext->mConfigPara.mMaxFrmsTagInterval = GetConfParaInt(&mConf, CFG_FRMSTAG_BACKUP_INTERVAL, 0);
        pContext->mConfigPara.mDstFileMaxCnt = GetConfParaInt(&mConf, CFG_DST_FILE_MAX_CNT, 0);
        pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE, 0);
        pContext->mConfigPara.mViBufferNum = GetConfParaInt(&mConf, CFG_DST_VI_BUFFER_NUM, 0);
        pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE, 0);
        pContext->mConfigPara.mMaxFileDuration = GetConfParaInt(&mConf, CFG_DST_VIDEO_DURATION, 0);

        pContext->mConfigPara.mProductMode = GetConfParaInt(&mConf, CFG_PRODUCT_MODE, 0);
        pContext->mConfigPara.mSensorType = GetConfParaInt(&mConf, CFG_SENSOR_TYPE, 0);
        pContext->mConfigPara.mKeyFrameInterval = GetConfParaInt(&mConf, CFG_KEY_FRAME_INTERVAL, 0);
        pContext->mConfigPara.mRcMode = GetConfParaInt(&mConf, CFG_RC_MODE, 0);
        pContext->mConfigPara.mInitQp = GetConfParaInt(&mConf, CFG_INIT_QP, 0);
        pContext->mConfigPara.mMinIQp = GetConfParaInt(&mConf, CFG_MIN_I_QP, 0);
        pContext->mConfigPara.mMaxIQp = GetConfParaInt(&mConf, CFG_MAX_I_QP, 0);
        pContext->mConfigPara.mMinPQp = GetConfParaInt(&mConf, CFG_MIN_P_QP, 0);
        pContext->mConfigPara.mMaxPQp = GetConfParaInt(&mConf, CFG_MAX_P_QP, 0);
        pContext->mConfigPara.mEnMbQpLimit = GetConfParaInt(&mConf, CFG_MB_QP_LIMIT, 0);
        pContext->mConfigPara.mMovingTh = GetConfParaInt(&mConf, CFG_MOVING_TH, 0);
        pContext->mConfigPara.mQuality = GetConfParaInt(&mConf, CFG_QUALITY, 0);
        pContext->mConfigPara.mPBitsCoef = GetConfParaInt(&mConf, CFG_P_BITS_COEF, 0);
        pContext->mConfigPara.mIBitsCoef = GetConfParaInt(&mConf, CFG_I_BITS_COEF, 0);
        pContext->mConfigPara.mGopMode = GetConfParaInt(&mConf, CFG_GOP_MODE, 0);
        pContext->mConfigPara.mGopSize = GetConfParaInt(&mConf, CFG_GOP_SIZE, 0);
        pContext->mConfigPara.mAdvancedRef_Base = GetConfParaInt(&mConf, CFG_AdvancedRef_Base, 0);
        pContext->mConfigPara.mAdvancedRef_Enhance = GetConfParaInt(&mConf, CFG_AdvancedRef_Enhance, 0);
        pContext->mConfigPara.mAdvancedRef_RefBaseEn = GetConfParaInt(&mConf, CFG_AdvancedRef_RefBaseEn, 0);
        pContext->mConfigPara.mEnableFastEnc = GetConfParaInt(&mConf, CFG_FAST_ENC, 0);
        pContext->mConfigPara.mbEnableSmart = GetConfParaBoolean(&mConf, CFG_ENABLE_SMART, 0);
        pContext->mConfigPara.mSVCLayer = GetConfParaInt(&mConf, CFG_SVC_LAYER, 0);
        pContext->mConfigPara.mEncodeRotate = GetConfParaInt(&mConf, CFG_ENCODE_ROTATE, 0);

        pContext->mConfigPara.m2DnrPara.enable_2d_filter = GetConfParaInt(&mConf, CFG_2DNR_EN, 0);
        pContext->mConfigPara.m2DnrPara.filter_strength_y = GetConfParaInt(&mConf, CFG_2DNR_STRENGTH_Y, 0);
        pContext->mConfigPara.m2DnrPara.filter_strength_uv = GetConfParaInt(&mConf, CFG_2DNR_STRENGTH_C, 0);
        pContext->mConfigPara.m2DnrPara.filter_th_y = GetConfParaInt(&mConf, CFG_2DNR_THRESHOLD_Y, 0);
        pContext->mConfigPara.m2DnrPara.filter_th_uv = GetConfParaInt(&mConf, CFG_2DNR_THRESHOLD_C, 0);

        pContext->mConfigPara.m3DnrPara.enable_3d_filter = GetConfParaInt(&mConf, CFG_3DNR_EN, 0);
        pContext->mConfigPara.m3DnrPara.adjust_pix_level_enable = GetConfParaInt(&mConf, CFG_3DNR_PIX_LEVEL_EN, 0);
        pContext->mConfigPara.m3DnrPara.smooth_filter_enable = GetConfParaInt(&mConf, CFG_3DNR_SMOOTH_EN, 0);
        pContext->mConfigPara.m3DnrPara.max_pix_diff_th = GetConfParaInt(&mConf, CFG_3DNR_PIX_DIFF_TH, 0);
        pContext->mConfigPara.m3DnrPara.max_mv_th = GetConfParaInt(&mConf, CFG_3DNR_MAX_MV_TH, 0);
        pContext->mConfigPara.m3DnrPara.max_mad_th = GetConfParaInt(&mConf, CFG_3DNR_MAX_MAD_TH, 0);
        pContext->mConfigPara.m3DnrPara.min_coef = GetConfParaInt(&mConf, CFG_3DNR_MIN_COEF, 0);
        pContext->mConfigPara.m3DnrPara.max_coef = GetConfParaInt(&mConf, CFG_3DNR_MAX_COEF, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_ENCODER, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "H.264"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_H264;
                alogd("H.264");
            }
            else if (!strcmp(ptr, "H.265"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_H265;
                alogd("H.265");
            }
            else if (!strcmp(ptr, "MJPEG"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_MJPEG;
                alogd("MJPEG");
            }
            else
            {
                aloge("error conf encoder type");
            }
        }

        pContext->mConfigPara.mTestDuration = GetConfParaInt(&mConf, CFG_TEST_DURATION, 0);

        pContext->mConfigPara.mEncUseProfile = GetConfParaInt(&mConf, CFG_DST_ENCODE_PROFILE, 0);

        alogd("vipp:%d, frame rate:%d, bitrate:%d, video_duration=%d, test_time=%d, profile=%d", pContext->mConfigPara.mVippDev,\
            pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate,\
            pContext->mConfigPara.mMaxFileDuration, pContext->mConfigPara.mTestDuration,\
            pContext->mConfigPara.mEncUseProfile);

        pContext->mConfigPara.mHorizonFlipFlag = GetConfParaInt(&mConf, CFG_MIRROR, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_COLOR2GREY, NULL);
        if (ptr != NULL)
        {
            if(!strcmp(ptr, "yes"))
            {
                pContext->mConfigPara.mColor2Grey = TRUE;
            }
            else
            {
                pContext->mConfigPara.mColor2Grey = FALSE;
            }
        }

        pContext->mConfigPara.mRoiNum = GetConfParaInt(&mConf, CFG_ROI_NUM, 0);
        pContext->mConfigPara.mRoiQp = GetConfParaInt(&mConf, CFG_ROI_QP, 0);
        pContext->mConfigPara.mRoiBgFrameRateEnable = GetConfParaBoolean(&mConf, CFG_ROI_BgFrameRateEnable, 0);
        pContext->mConfigPara.mRoiBgFrameRateAttenuation = GetConfParaInt(&mConf, CFG_ROI_BgFrameRateAttenuation, 0);
        pContext->mConfigPara.mIntraRefreshBlockNum = GetConfParaInt(&mConf, CFG_IntraRefresh_BlockNum, 0);
        pContext->mConfigPara.mOrlNum = GetConfParaInt(&mConf, CFG_ORL_NUM, 0);
        pContext->mConfigPara.mVbvBufferSize = GetConfParaInt(&mConf, CFG_vbvBufferSize, 0);
        pContext->mConfigPara.mVbvThreshSize = GetConfParaInt(&mConf, CFG_vbvThreshSize, 0);

        alogd("mirror:%d, Color2Grey:%d, RoiNum:%d, RoiQp:%d, RoiBgFrameRate Enable:%d Attenuation:%d, IntraRefreshBlockNum:%d, OrlNum:%d"
            "VbvBufferSize:%d, VbvThreshSize:%d",
            pContext->mConfigPara.mHorizonFlipFlag, pContext->mConfigPara.mColor2Grey,
            pContext->mConfigPara.mRoiNum, pContext->mConfigPara.mRoiQp,
            pContext->mConfigPara.mRoiBgFrameRateEnable, pContext->mConfigPara.mRoiBgFrameRateAttenuation,
            pContext->mConfigPara.mIntraRefreshBlockNum,
            pContext->mConfigPara.mOrlNum, pContext->mConfigPara.mVbvBufferSize,
            pContext->mConfigPara.mVbvThreshSize);

        pContext->mConfigPara.mCropEnable = GetConfParaInt(&mConf, CFG_CROP_ENABLE, 0);
        pContext->mConfigPara.mCropRectX = GetConfParaInt(&mConf, CFG_CROP_RECT_X, 0);
        pContext->mConfigPara.mCropRectY = GetConfParaInt(&mConf, CFG_CROP_RECT_Y, 0);
        pContext->mConfigPara.mCropRectWidth = GetConfParaInt(&mConf, CFG_CROP_RECT_WIDTH, 0);
        pContext->mConfigPara.mCropRectHeight = GetConfParaInt(&mConf, CFG_CROP_RECT_HEIGHT, 0);

        alogd("venc crop enable:%d, X:%d, Y:%d, Width:%d, Height:%d",
            pContext->mConfigPara.mCropEnable, pContext->mConfigPara.mCropRectX,
            pContext->mConfigPara.mCropRectY, pContext->mConfigPara.mCropRectWidth,
            pContext->mConfigPara.mCropRectHeight);

        pContext->mConfigPara.mVuiTimingInfoPresentFlag = GetConfParaInt(&mConf, CFG_vui_timing_info_present_flag, 0);
        alogd("VuiTimingInfoPresentFlag:%d", pContext->mConfigPara.mVuiTimingInfoPresentFlag);

        pContext->mConfigPara.mVeFreq = GetConfParaInt(&mConf, CFG_Ve_Freq, 0);
        alogd("mVeFreq:%d MHz", pContext->mConfigPara.mVeFreq);

        pContext->mConfigPara.mOnlineEnable = GetConfParaInt(&mConf, CFG_online_en, 0);
        pContext->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&mConf, CFG_online_share_buf_num, 0);
        alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pContext->mConfigPara.mOnlineEnable,
            pContext->mConfigPara.mOnlineShareBufNum);

        if (0 == pContext->mConfigPara.mOnlineEnable)
        {
            // venc drop frame only support offline.
            pContext->mConfigPara.mViDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("ViDropFrameNum: %d", pContext->mConfigPara.mViDropFrameNum);
        }
        else
        {
            // venc drop frame support online and offline.
            pContext->mConfigPara.mVencDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("VencDropFrameNum: %d", pContext->mConfigPara.mVencDropFrameNum);
        }

        pContext->mConfigPara.wdr_en = GetConfParaInt(&mConf, CFG_WDR_EN, 0);
        alogd("wdr_en: %d", pContext->mConfigPara.wdr_en);

        pContext->mConfigPara.mEnableGdc = GetConfParaInt(&mConf, CFG_EnableGdc, 0);
        alogd("EnableGdc: %d", pContext->mConfigPara.mEnableGdc);

        pContext->mConfigPara.mEncppEnable = GetConfParaInt(&mConf, CFG_EncppEnable, 0);
        pContext->mConfigPara.mIspAndVeLinkageEnable = GetConfParaInt(&mConf, CFG_IspAndVeLinkageEnable, 0);
        alogd("EncppEnable: %d, IspAndVeLinkageEnable: %d", pContext->mConfigPara.mEncppEnable, pContext->mConfigPara.mIspAndVeLinkageEnable);

        pContext->mConfigPara.mSuperFrmMode = GetConfParaInt(&mConf, CFG_SuperFrmMode, -1);
        pContext->mConfigPara.mSuperIFrmBitsThr = GetConfParaInt(&mConf, CFG_SuperIFrmBitsThr, 0);
        pContext->mConfigPara.mSuperPFrmBitsThr = GetConfParaInt(&mConf, CFG_SuperPFrmBitsThr, 0);
        alogd("SuperFrm Mode: %d, I:%d, P:%d", pContext->mConfigPara.mSuperFrmMode, pContext->mConfigPara.mSuperIFrmBitsThr, pContext->mConfigPara.mSuperPFrmBitsThr);

        pContext->mConfigPara.mBitsClipParam.dis_default_para = GetConfParaBoolean(&mConf, CFG_BitsClipDisDefault, 0);
        pContext->mConfigPara.mBitsClipParam.mode = GetConfParaInt(&mConf, CFG_BitsClipMode, 0);
        pContext->mConfigPara.mBitsClipParam.coef_th[0][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef00, -0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[0][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef01, 0.2);
        pContext->mConfigPara.mBitsClipParam.coef_th[1][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef10, -0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[1][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef11, 0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[2][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef20, -0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[2][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef21, 0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[3][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef30, -0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[3][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef31, 0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[4][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef40, 0.4);
        pContext->mConfigPara.mBitsClipParam.coef_th[4][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef41, 0.7);

        alogd("BitsClipParam: %d %d {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}",
            pContext->mConfigPara.mBitsClipParam.dis_default_para,
            pContext->mConfigPara.mBitsClipParam.mode,
            pContext->mConfigPara.mBitsClipParam.coef_th[0][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[0][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[1][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[1][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[2][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[2][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[3][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[3][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[4][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[4][1]);

        pContext->mConfigPara.mAeDiffParam.dis_default_para = GetConfParaInt(&mConf, CFG_AeDiffDisDefault, 0);
        pContext->mConfigPara.mAeDiffParam.diff_frames_th = GetConfParaInt(&mConf, CFG_AeDiffFramesTh, 40);
        pContext->mConfigPara.mAeDiffParam.stable_frames_th[0] = GetConfParaInt(&mConf, CFG_AeStableFramesTh0, 5);
        pContext->mConfigPara.mAeDiffParam.stable_frames_th[1] = GetConfParaInt(&mConf, CFG_AeStableFramesTh1, 100);
        pContext->mConfigPara.mAeDiffParam.diff_th[0] = (float)GetConfParaDouble(&mConf, CFG_AeDiffTh0, 0.1);
        pContext->mConfigPara.mAeDiffParam.diff_th[1] = (float)GetConfParaDouble(&mConf, CFG_AeDiffTh1, 0.6);
        pContext->mConfigPara.mAeDiffParam.small_diff_qp[0] =  GetConfParaInt(&mConf, CFG_AeSmallDiffQp0, 20);
        pContext->mConfigPara.mAeDiffParam.small_diff_qp[1] =  GetConfParaInt(&mConf, CFG_AeSmallDiffQp1, 25);
        pContext->mConfigPara.mAeDiffParam.large_diff_qp[0] =  GetConfParaInt(&mConf, CFG_AeLargeDiffQp0, 35);
        pContext->mConfigPara.mAeDiffParam.large_diff_qp[1] =  GetConfParaInt(&mConf, CFG_AeLargeDiffQp1, 50);

        alogd("AeDiffParam: %d %d [%d,%d] [%.2f,%.2f], [%d,%d], [%d,%d]",
            pContext->mConfigPara.mAeDiffParam.dis_default_para,
            pContext->mConfigPara.mAeDiffParam.diff_frames_th,
            pContext->mConfigPara.mAeDiffParam.stable_frames_th[0],
            pContext->mConfigPara.mAeDiffParam.stable_frames_th[1],
            pContext->mConfigPara.mAeDiffParam.diff_th[0],
            pContext->mConfigPara.mAeDiffParam.diff_th[1],
            pContext->mConfigPara.mAeDiffParam.small_diff_qp[0],
            pContext->mConfigPara.mAeDiffParam.small_diff_qp[1],
            pContext->mConfigPara.mAeDiffParam.large_diff_qp[0],
            pContext->mConfigPara.mAeDiffParam.large_diff_qp[1]);

        destroyConfParser(&mConf);
    }

    //parse dst directory form dst file path.
    char *pLastSlash = strrchr(pContext->mConfigPara.dstVideoFile, '/');
    if(pLastSlash != NULL)
    {
        int dirLen = pLastSlash-pContext->mConfigPara.dstVideoFile;
        strncpy(pContext->mDstDir, pContext->mConfigPara.dstVideoFile, dirLen);
        pContext->mDstDir[dirLen] = '\0';
        
        char *pFileName = pLastSlash+1;
        strcpy(pContext->mFirstFileName, pFileName);
    }
    else
    {
        strcpy(pContext->mDstDir, "");
        strcpy(pContext->mFirstFileName, pContext->mConfigPara.dstVideoFile);
    }
    return SUCCESS;
}

static unsigned long long GetNowTimeUs(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

static int getFileNameByCurTime(SAMPLE_VI2VENC2MUXER_S *pContext, char *pNameBuf)
{
#if 0
    sprintf(pNameBuf, "%s", "/mnt/extsd/sample_mux/");
    sprintf(pNameBuf, "%s%llud.mp4", pNameBuf, GetNowTimeUs());
#else
    static int file_cnt = 0;
    char strStemPath[MAX_FILE_PATH_LEN] = {0};
    int len = strlen(pContext->mConfigPara.dstVideoFile);
    char *ptr = pContext->mConfigPara.dstVideoFile;
    while (*(ptr+len-1) != '.')
    {
        len--;
    }

    ++file_cnt;
    strncpy(strStemPath, pContext->mConfigPara.dstVideoFile, len-1);
    sprintf(pNameBuf, "%s_%d.mp4", strStemPath, file_cnt);
#endif
    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_VI2VENC2MUXER_S *pContext = (SAMPLE_VI2VENC2MUXER_S *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                aloge("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Vi_Timeout;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&pContext->mMsgQueue, &stCmdMsg);
                break;
            }
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                if (pIsp2VeParam)
                {
                    sEncppSharpParam *pSharpParam = &pIsp2VeParam->mSharpParam;
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pContext->mConfigPara.mVippDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mConfigPara.mVippDev, ret);
                        return -1;
                    }
                    struct enc_VencIsp2VeParam mIsp2VeParam;
                    memset(&mIsp2VeParam, 0, sizeof(struct enc_VencIsp2VeParam));
                    ret = AW_MPI_ISP_GetIsp2VeParam(mIspDev, &mIsp2VeParam);
                    if (ret)
                    {
                        aloge("fatal error, isp[%d] GetIsp2VeParam failed! ret=%d", mIspDev, ret);
                        return -1;
                    }

                    if (mIsp2VeParam.encpp_en)
                    {
                        VENC_CHN_ATTR_S stVencAttr;
                        memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
                        AW_MPI_VENC_GetChnAttr(mVEncChn, &stVencAttr);
                        if (FALSE == stVencAttr.EncppAttr.mbEncppEnable)
                        {
                            stVencAttr.EncppAttr.mbEncppEnable = TRUE;
                            AW_MPI_VENC_SetChnAttr(mVEncChn, &stVencAttr);
                        }
                        memcpy(&pSharpParam->mDynamicParam, &mIsp2VeParam.mDynamicSharpCfg,sizeof(sEncppSharpParamDynamic));
                        memcpy(&pSharpParam->mStaticParam, &mIsp2VeParam.mStaticSharpCfg, sizeof(sEncppSharpParamStatic));
                    }
                    else
                    {
                        VENC_CHN_ATTR_S stVencAttr;
                        memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
                        AW_MPI_VENC_GetChnAttr(mVEncChn, &stVencAttr);
                        if (TRUE == stVencAttr.EncppAttr.mbEncppEnable)
                        {
                            stVencAttr.EncppAttr.mbEncppEnable = FALSE;
                            AW_MPI_VENC_SetChnAttr(mVEncChn, &stVencAttr);
                        }
                    }

                    pIsp2VeParam->mEnvLv = AW_MPI_ISP_GetEnvLV(mIspDev);
                    pIsp2VeParam->mAeWeightLum = AW_MPI_ISP_GetAeWeightLum(mIspDev);
                    pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                }
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)pEventData;
                if (pVe2IspParam && pContext->mConfigPara.mIspAndVeLinkageEnable)
                {
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pContext->mConfigPara.mVippDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mConfigPara.mVippDev, ret);
                        return -1;
                    }
                    alogv("update Ve2IspParam, route Isp[%d]-Vipp[%d]-VencChn[%d]", mIspDev, pContext->mConfigPara.mVippDev, pContext->mConfigPara.mVeChn);

                    struct enc_VencVe2IspParam mIspParam;
                    memcpy(&mIspParam.mMovingLevelInfo, &pVe2IspParam->mMovingLevelInfo, sizeof(MovingLevelInfo));
                    ret = AW_MPI_ISP_SetVe2IspParam(mIspDev, &mIspParam);
                    if (ret)
                    {
                        aloge("fatal error, isp[%d] SetVe2IspParam failed! ret=%d", mIspDev, ret);
                        return -1;
                    }
                    alogv("isp[%d] d2d_level=%d, d3d_level=%d", mIspDev, pVe2IspParam->d2d_level, pVe2IspParam->d3d_level);
                    ret = AW_MPI_ISP_SetNRAttr(mIspDev, pVe2IspParam->d2d_level);
                    if (ret)
                    {
                        aloge("fatal error, isp[%d] SetNRAttr failed! ret=%d", mIspDev, ret);
                        return -1;
                    }
                    ret = AW_MPI_ISP_Set3NRAttr(mIspDev, pVe2IspParam->d3d_level);
                    if (ret)
                    {
                        aloge("fatal error, isp[%d] Set3NRAttr failed! ret=%d", mIspDev, ret);
                        return -1;
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                alogd("MuxerId[%d] record file done.", *(int*)pEventData);
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&gpVi2Venc2MuxerData->mMsgQueue, &stCmdMsg);  
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                alogd("MuxerId[%d] need next fd.", *(int*)pEventData);
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Rec_NeedSetNextFd;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&gpVi2Venc2MuxerData->mMsgQueue, &stCmdMsg);  
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return SUCCESS;
}

static ERRORTYPE configMuxGrpAttr(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    memset(&pContext->mMuxGrpAttr, 0, sizeof(MUX_GRP_ATTR_S));

    pContext->mMuxGrpAttr.mVideoAttrValidNum = 1;
    pContext->mMuxGrpAttr.mVideoAttr[0].mVideoEncodeType = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mMuxGrpAttr.mVideoAttr[0].mWidth = pContext->mConfigPara.dstWidth;
    pContext->mMuxGrpAttr.mVideoAttr[0].mHeight = pContext->mConfigPara.dstHeight;
    pContext->mMuxGrpAttr.mVideoAttr[0].mVideoFrmRate = pContext->mConfigPara.mVideoFrameRate*1000;
    pContext->mMuxGrpAttr.mVideoAttr[0].mVeChn = pContext->mVeChn;
    pContext->mMuxGrpAttr.mAudioEncodeType = PT_MAX;

    return SUCCESS;
}

static ERRORTYPE createMuxGrp(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configMuxGrpAttr(pContext);
    pContext->mMuxGrp = 0;
    while (pContext->mMuxGrp < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(pContext->mMuxGrp, &pContext->mMuxGrpAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] success!", pContext->mMuxGrp);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("mux group[%d] is exist, find next!", pContext->mMuxGrp);
            pContext->mMuxGrp++;
        }
        else
        {
            alogd("create mux group[%d] ret[0x%x], find next!", pContext->mMuxGrp, ret);
            pContext->mMuxGrp++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pContext->mMuxGrp = MM_INVALID_CHN;
        aloge("fatal error! create mux group fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pContext->mMuxGrp, &cbInfo);
        return SUCCESS;
    }
}

static int addOutputFormatAndOutputSink_l(SAMPLE_VI2VENC2MUXER_S *pContext, OUTSINKINFO_S *pSinkInfo)
{
    int retMuxerId = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;

    alogd("fmt:0x%x, fd:%d, FallocateLen:%d, callback_out_flag:%d", pSinkInfo->mOutputFormat, pSinkInfo->mOutputFd, pSinkInfo->mFallocateLen, pSinkInfo->mCallbackOutFlag);
    if(pSinkInfo->mOutputFd >= 0 && TRUE == pSinkInfo->mCallbackOutFlag)
    {
        aloge("fatal error! one muxer cannot support two sink methods!");
        return -1;
    }

    //find if the same output_format sinkInfo exist or callback out stream is exist.
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFormat == pSinkInfo->mOutputFormat)
            {
                alogd("Be careful! same outputForamt[0x%x] exist in array", pSinkInfo->mOutputFormat);
            }
//            if (pEntry->mSinkInfo.mCallbackOutFlag == pSinkInfo->mCallbackOutFlag)
//            {
//                aloge("fatal error! only support one callback out stream");
//            }
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    MUX_CHN_INFO_S *p_node = (MUX_CHN_INFO_S *)malloc(sizeof(MUX_CHN_INFO_S));
    if (p_node == NULL)
    {
        aloge("alloc mux chn info node fail");
        return -1;
    }

    memset(p_node, 0, sizeof(MUX_CHN_INFO_S));
    p_node->mSinkInfo.mMuxerId = pContext->mMuxerIdCounter;
    p_node->mSinkInfo.mOutputFormat = pSinkInfo->mOutputFormat;
    if (pSinkInfo->mOutputFd > 0)
    {
        p_node->mSinkInfo.mOutputFd = dup(pSinkInfo->mOutputFd);
    }
    else
    {
        p_node->mSinkInfo.mOutputFd = -1;
    }
    p_node->mSinkInfo.mFallocateLen = pSinkInfo->mFallocateLen;
    p_node->mSinkInfo.mCallbackOutFlag = pSinkInfo->mCallbackOutFlag;

    p_node->mMuxChnAttr.mMuxerId = p_node->mSinkInfo.mMuxerId;
    p_node->mMuxChnAttr.mMediaFileFormat = p_node->mSinkInfo.mOutputFormat;
    p_node->mMuxChnAttr.mMaxFileDuration = pContext->mConfigPara.mMaxFileDuration *1000;
    p_node->mMuxChnAttr.mFallocateLen = p_node->mSinkInfo.mFallocateLen;
    p_node->mMuxChnAttr.mCallbackOutFlag = p_node->mSinkInfo.mCallbackOutFlag;
    p_node->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    p_node->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    p_node->mMuxChnAttr.mAddRepairInfo = pContext->mConfigPara.mbAddRepairInfo;
    p_node->mMuxChnAttr.mMaxFrmsTagInterval = pContext->mConfigPara.mMaxFrmsTagInterval;

    p_node->mMuxChn = MM_INVALID_CHN;

    if ((pContext->mCurrentState == REC_PREPARED) || (pContext->mCurrentState == REC_RECORDING))
    {
        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while (nMuxChn < MUX_MAX_CHN_NUM)
        {
            ret = AW_MPI_MUX_CreateChn(pContext->mMuxGrp, nMuxChn, &p_node->mMuxChnAttr, p_node->mSinkInfo.mOutputFd);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pContext->mMuxGrp, nMuxChn, p_node->mMuxChnAttr.mMuxerId);
                break;
            }
            else if (ERR_MUX_EXIST == ret)
            {
                alogd("mux group[%d] channel[%d] is exist, find next!", pContext->mMuxGrp, nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create mux group[%d] channel[%d] fail ret[0x%x], find next!", pContext->mMuxGrp, nMuxChn, ret);
                nMuxChn++;
            }
        }

        if (nSuccessFlag)
        {
            retMuxerId = p_node->mSinkInfo.mMuxerId;
            p_node->mMuxChn = nMuxChn;
            pContext->mMuxerIdCounter++;
        }
        else
        {
            aloge("fatal error! create mux group[%d] channel fail!", pContext->mMuxGrp);
            if (p_node->mSinkInfo.mOutputFd >= 0)
            {
                close(p_node->mSinkInfo.mOutputFd);
                p_node->mSinkInfo.mOutputFd = -1;
            }

            retMuxerId = -1;
        }

        pthread_mutex_lock(&pContext->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pContext->mMuxChnList);
        pthread_mutex_unlock(&pContext->mMuxChnListLock);
    }
    else
    {
        retMuxerId = p_node->mSinkInfo.mMuxerId;
        pContext->mMuxerIdCounter++;
        pthread_mutex_lock(&pContext->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pContext->mMuxChnList);
        pthread_mutex_unlock(&pContext->mMuxChnListLock);
    }

    return retMuxerId;
}

static int addOutputFormatAndOutputSink(SAMPLE_VI2VENC2MUXER_S *pContext, char* path, MEDIA_FILE_FORMAT_E format)
{
    int muxerId = -1;
    OUTSINKINFO_S sinkInfo = {0};

    if (path != NULL)
    {
        sinkInfo.mFallocateLen = 0;
        sinkInfo.mCallbackOutFlag = FALSE;
        sinkInfo.mOutputFormat = format;
        sinkInfo.mOutputFd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (sinkInfo.mOutputFd < 0)
        {
            aloge("Failed to open %s", path);
            return -1;
        }

        muxerId = addOutputFormatAndOutputSink_l(pContext, &sinkInfo);
        close(sinkInfo.mOutputFd);
    }

    return muxerId;
}

static int setOutputFileSync_l(SAMPLE_VI2VENC2MUXER_S *pContext, int fd, int64_t fallocateLength, int muxerId)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;

    if (pContext->mCurrentState != REC_RECORDING)
    {
        aloge("must be in recording state");
        return -1;
    }

    alogv("setOutputFileSync fd=%d", fd);
    if (fd < 0)
    {
        aloge("Invalid parameter");
        return -1;
    }

    MUX_CHN muxChn = MM_INVALID_CHN;
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mMuxChnAttr.mMuxerId == muxerId)
            {
                muxChn = pEntry->mMuxChn;
                break;
            }
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    if (muxChn != MM_INVALID_CHN)
    {
        alogd("switch fd");
        AW_MPI_MUX_SwitchFd(pContext->mMuxGrp, muxChn, fd, fallocateLength);
        return 0;
    }
    else
    {
        aloge("fatal error! can't find muxChn which muxerId[%d]", muxerId);
        return -1;
    }
}

static int setOutputFileSync(SAMPLE_VI2VENC2MUXER_S *pContext, char* path, int64_t fallocateLength, int muxerId)
{
    int ret;

    if (pContext->mCurrentState != REC_RECORDING)
    {
        aloge("not in recording state");
        return -1;
    }

    if(path != NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
        {
            aloge("fail to open %s", path);
            return -1;
        }
        ret = setOutputFileSync_l(pContext, fd, fallocateLength, muxerId);
        close(fd);

        return ret;
    }
    else
    {
        return -1;
    }
}

static inline unsigned int map_H264_UserSet2Profile(int val)
{
    unsigned int profile = (unsigned int)H264_PROFILE_HIGH;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H264_PROFILE_BASE;
        break;

    case 1:
        profile = (unsigned int)H264_PROFILE_MAIN;
        break;

    case 2:
        profile = (unsigned int)H264_PROFILE_HIGH;
        break;

    default:
        break;
    }

    return profile;
}

static inline unsigned int map_H265_UserSet2Profile(int val)
{
    unsigned int profile = H265_PROFILE_MAIN;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H265_PROFILE_MAIN;
        break;

    case 1:
        profile = (unsigned int)H265_PROFILE_MAIN10;
        break;

    case 2:
        profile = (unsigned int)H265_PROFILE_STI11;
        break;

    default:
        break;
    }
    return profile;
}

static void initGdcParam(sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eWarpMode = Gdc_Warp_LDC;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;

    pGdcParam->fx = 2417.19;
    pGdcParam->fy = 2408.43;
    pGdcParam->cx = 1631.50;
    pGdcParam->cy = 1223.50;
    pGdcParam->fx_scale = 2161.82;
    pGdcParam->fy_scale = 2153.99;
    pGdcParam->cx_scale = 1631.50;
    pGdcParam->cy_scale = 1223.50;

    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    pGdcParam->distCoef_wide_ra[0] = -0.3849;
    pGdcParam->distCoef_wide_ra[1] = 0.1567;
    pGdcParam->distCoef_wide_ra[2] = -0.0030;
    pGdcParam->distCoef_wide_ta[0] = -0.00005;
    pGdcParam->distCoef_wide_ta[1] = 0.0016;

    pGdcParam->distCoef_fish_k[0]  = -0.0024;
    pGdcParam->distCoef_fish_k[1]  = 0.141;
    pGdcParam->distCoef_fish_k[2]  = -0.3;
    pGdcParam->distCoef_fish_k[3]  = 0.2328;

    pGdcParam->centerOffsetX         =      0;     //[-255,0]
    pGdcParam->centerOffsetY         =      0;     //[-255,0]
    pGdcParam->rotateAngle           =      0;     //[0,360]
    pGdcParam->radialDistortCoef     =      0;     //[-255,255]
    pGdcParam->trapezoidDistortCoef  =      0;     //[-255,255]
    pGdcParam->fanDistortCoef        =      0;     //[-255,255]
    pGdcParam->pan                   =      0;     //pano360:[0,360]; others:[-90,90]
    pGdcParam->tilt                  =      0;     //[-90,90]
    pGdcParam->zoomH                 =      100;   //[0,100]
    pGdcParam->zoomV                 =      100;   //[0,100]
    pGdcParam->scale                 =      100;   //[0,100]
    pGdcParam->innerRadius           =      0;     //[0,width/2]
    pGdcParam->roll                  =      0;     //[-90,90]
    pGdcParam->pitch                 =      0;     //[-90,90]
    pGdcParam->yaw                   =      0;     //[-90,90]

    pGdcParam->perspFunc             =    Gdc_Persp_Only;
    pGdcParam->perspectiveProjMat[0] =    1.0;
    pGdcParam->perspectiveProjMat[1] =    0.0;
    pGdcParam->perspectiveProjMat[2] =    0.0;
    pGdcParam->perspectiveProjMat[3] =    0.0;
    pGdcParam->perspectiveProjMat[4] =    1.0;
    pGdcParam->perspectiveProjMat[5] =    0.0;
    pGdcParam->perspectiveProjMat[6] =    0.0;
    pGdcParam->perspectiveProjMat[7] =    0.0;
    pGdcParam->perspectiveProjMat[8] =    1.0;

    pGdcParam->mountHeight           =      0.85; //meters
    pGdcParam->roiDist_ahead         =      4.5;  //meters
    pGdcParam->roiDist_left          =     -1.5;  //meters
    pGdcParam->roiDist_right         =      1.5;  //meters
    pGdcParam->roiDist_bottom        =      0.65; //meters

    pGdcParam->peaking_en            =      1;    //0/1
    pGdcParam->peaking_clamp         =      1;    //0/1
    pGdcParam->peak_m                =     16;    //[0,63]
    pGdcParam->th_strong_edge        =      6;    //[0,15]
    pGdcParam->peak_weights_strength =      2;    //[0,15]

    if (pGdcParam->eWarpMode == Gdc_Warp_LDC)
    {
        pGdcParam->birdsImg_width    = 768;
        pGdcParam->birdsImg_height   = 1080;
    }
}

static ERRORTYPE configVencChnAttr(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    memset(&pContext->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mVencChnAttr.VeAttr.mOnlineEnable = 1;
        pContext->mVencChnAttr.VeAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mVencChnAttr.VeAttr.Type = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mKeyFrameInterval;
    pContext->mVencChnAttr.VeAttr.SrcPicWidth  = pContext->mConfigPara.srcWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.srcHeight;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.srcPixFmt;
    pContext->mVencChnAttr.VeAttr.mColorSpace = pContext->mConfigPara.mColorSpace;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pContext->mVencChnAttr.VeAttr.PixelFormat, pContext->mVencChnAttr.VeAttr.mColorSpace);
    pContext->mVencChnAttr.VeAttr.mDropFrameNum = pContext->mConfigPara.mVencDropFrameNum;
    alogd("DropFrameNum:%d", pContext->mVencChnAttr.VeAttr.mDropFrameNum);
    pContext->mVencChnAttr.EncppAttr.mbEncppEnable = pContext->mConfigPara.mEncppEnable;
    switch(pContext->mConfigPara.mEncodeRotate)
    {
        case 90:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_90;
            break;
        case 180:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_180;
            break;
        case 270:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_270;
            break;
        default:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_NONE;
            break;
    }

    pContext->mVencRcParam.product_mode = pContext->mConfigPara.mProductMode;
    pContext->mVencRcParam.sensor_type = pContext->mConfigPara.mSensorType;

    if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.BufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->mVencRcParam.ParamH264Vbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH264Vbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH264Vbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH264Vbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH264Vbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            pContext->mVencRcParam.ParamH264Vbr.mMovingTh = pContext->mConfigPara.mMovingTh;
            pContext->mVencRcParam.ParamH264Vbr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = pContext->mConfigPara.mIBitsCoef;
            pContext->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = pContext->mConfigPara.mPBitsCoef;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = pContext->mConfigPara.mMinPQp;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mQuality = 8;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 20;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH264Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH264Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pContext->mConfigPara.mEnableFastEnc)
        {
            pContext->mVencChnAttr.VeAttr.AttrH264e.FastEncFlag = TRUE;
        }
    }
    else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->mVencRcParam.ParamH265Vbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH265Vbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH265Vbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH265Vbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH265Vbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            pContext->mVencRcParam.ParamH265Vbr.mMovingTh = pContext->mConfigPara.mMovingTh;
            pContext->mVencRcParam.ParamH265Vbr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = pContext->mConfigPara.mIBitsCoef;
            pContext->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = pContext->mConfigPara.mPBitsCoef;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = pContext->mConfigPara.mMinPQp;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencRcParam.ParamH265Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH265Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pContext->mConfigPara.mEnableFastEnc)
        {
            pContext->mVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = TRUE;
        }
    }
    else if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.dstHeight;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 0:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            break;
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 2:
        case 3:
        default:
            aloge("not support! use default cbr mode");
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            break;
        }
    }

    alogd("venc set Rcmode=%d", pContext->mVencChnAttr.RcAttr.mRcMode);

    if(0 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    }
    else if(1 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_DUALP;
    }
    else if(2 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_SMARTP;
        pContext->mVencChnAttr.GopAttr.stSmartP.mVirtualIFrameInterval = 15;
    }
    pContext->mVencChnAttr.GopAttr.mGopSize = pContext->mConfigPara.mGopSize;

    if (pContext->mConfigPara.mEnableGdc)
    {
        alogd("enable GDC and init GDC params");
        initGdcParam(&pContext->mVencChnAttr.GdcAttr);
    }

    memcpy(&pContext->mVencRcParam.mBitsClipParam, &pContext->mConfigPara.mBitsClipParam, sizeof(VencTargetBitsClipParam));
    memcpy(&pContext->mVencRcParam.mAeDiffParam, &pContext->mConfigPara.mAeDiffParam, sizeof(VencAeDiffParam));

    return SUCCESS;
}

static ERRORTYPE createVencChn(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pContext);
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mVeChn = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pContext->mVeChn = pContext->mConfigPara.mVeChn;
    }

    while (pContext->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pContext->mVeChn, &pContext->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pContext->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pContext->mVeChn);
            pContext->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pContext->mVeChn, ret);
            pContext->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pContext->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        if (0 < pContext->mConfigPara.mVeFreq)
        {
            AW_MPI_VENC_SetVEFreq(pContext->mVeChn, pContext->mConfigPara.mVeFreq);
            alogd("set VE freq %d MHz", pContext->mConfigPara.mVeFreq);
        }

        AW_MPI_VENC_SetRcParam(pContext->mVeChn, &pContext->mVencRcParam);

        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = stFrameRate.DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        alogd("set venc framerate: src %dfps, dst %dfps", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &stFrameRate);

        if (pContext->mConfigPara.mAdvancedRef_Base)
        {
            VENC_PARAM_REF_S stRefParam;
            memset(&stRefParam, 0, sizeof(VENC_PARAM_REF_S));
            stRefParam.Base = pContext->mConfigPara.mAdvancedRef_Base;
            stRefParam.Enhance = pContext->mConfigPara.mAdvancedRef_Enhance;
            stRefParam.bEnablePred = pContext->mConfigPara.mAdvancedRef_RefBaseEn;
            AW_MPI_VENC_SetRefParam(pContext->mVeChn, &stRefParam);
            alogd("set RefParam %d %d %d", stRefParam.Base, stRefParam.Enhance, stRefParam.bEnablePred);
        }

        if (0 == pContext->mConfigPara.m2DnrPara.enable_2d_filter)
        {
            AW_MPI_VENC_Set2DFilter(pContext->mVeChn, &pContext->mConfigPara.m2DnrPara);
            alogd("disable and set 2DFilter param");
        }

        if (0 == pContext->mConfigPara.m3DnrPara.enable_3d_filter)
        {
            AW_MPI_VENC_Set3DFilter(pContext->mVeChn, &pContext->mConfigPara.m3DnrPara);
            alogd("disable and set 3DFilter param");
        }

        if (pContext->mConfigPara.mColor2Grey)
        {
            VENC_COLOR2GREY_S bColor2Grey;
            memset(&bColor2Grey, 0, sizeof(VENC_COLOR2GREY_S));
            bColor2Grey.bColor2Grey = pContext->mConfigPara.mColor2Grey;
            AW_MPI_VENC_SetColor2Grey(pContext->mVeChn, &bColor2Grey);
            alogd("set Color2Grey %d", pContext->mConfigPara.mColor2Grey);
        }

        if (pContext->mConfigPara.mHorizonFlipFlag)
        {
            AW_MPI_VENC_SetHorizonFlip(pContext->mVeChn, pContext->mConfigPara.mHorizonFlipFlag);
            alogd("set HorizonFlip %d", pContext->mConfigPara.mHorizonFlipFlag);
        }

        if (pContext->mConfigPara.mCropEnable)
        {
            VENC_CROP_CFG_S stCropCfg;
            memset(&stCropCfg, 0, sizeof(VENC_CROP_CFG_S));
            stCropCfg.bEnable = pContext->mConfigPara.mCropEnable;
            stCropCfg.Rect.X = pContext->mConfigPara.mCropRectX;
            stCropCfg.Rect.Y = pContext->mConfigPara.mCropRectY;
            stCropCfg.Rect.Width = pContext->mConfigPara.mCropRectWidth;
            stCropCfg.Rect.Height = pContext->mConfigPara.mCropRectHeight;
            AW_MPI_VENC_SetCrop(pContext->mVeChn, &stCropCfg);
            alogd("set Crop %d, [%d][%d][%d][%d]", stCropCfg.bEnable, stCropCfg.Rect.X, stCropCfg.Rect.Y, stCropCfg.Rect.Width, stCropCfg.Rect.Height);
        }

        //test PIntraRefresh
        if(pContext->mConfigPara.mIntraRefreshBlockNum > 0)
        {
            VENC_PARAM_INTRA_REFRESH_S stIntraRefresh;
            memset(&stIntraRefresh, 0, sizeof(VENC_PARAM_INTRA_REFRESH_S));
            stIntraRefresh.bRefreshEnable = TRUE;
            stIntraRefresh.RefreshLineNum = pContext->mConfigPara.mIntraRefreshBlockNum;
            ret = AW_MPI_VENC_SetIntraRefresh(pContext->mVeChn, &stIntraRefresh);
            if(ret != SUCCESS)
            {
                aloge("fatal error! set roiBgFrameRate fail[0x%x]!", ret);
            }
            else
            {
                alogd("set intra refresh:%d", stIntraRefresh.RefreshLineNum);
            }
        }

        if(pContext->mConfigPara.mbEnableSmart)
        {
            VencSmartFun smartParam;
            memset(&smartParam, 0, sizeof(VencSmartFun));
            smartParam.smart_fun_en = 1;
            smartParam.img_bin_en = 1;
            smartParam.img_bin_th = 0;
            smartParam.shift_bits = 2;
            AW_MPI_VENC_SetSmartP(pContext->mVeChn, &smartParam);
        }

        if(pContext->mConfigPara.mSVCLayer > 0)
        {
            VencH264SVCSkip stSVCSkip;
            memset(&stSVCSkip, 0, sizeof(VencH264SVCSkip));
            stSVCSkip.nTemporalSVC = pContext->mConfigPara.mSVCLayer;
            AW_MPI_VENC_SetH264SVCSkip(pContext->mVeChn, &stSVCSkip);
        }

        if (pContext->mConfigPara.mVuiTimingInfoPresentFlag)
        {
            /** must be call it before AW_MPI_VENC_GetH264SpsPpsInfo(unbind) and AW_MPI_VENC_StartRecvPic. */
            if(PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H264_VUI_S H264Vui;
                memset(&H264Vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
                AW_MPI_VENC_GetH264Vui(pContext->mVeChn, &H264Vui);
                H264Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H264Vui.VuiTimeInfo.fixed_frame_rate_flag = 1;
                H264Vui.VuiTimeInfo.num_units_in_tick = 1000;
                H264Vui.VuiTimeInfo.time_scale = H264Vui.VuiTimeInfo.num_units_in_tick * pContext->mConfigPara.mVideoFrameRate * 2;
                AW_MPI_VENC_SetH264Vui(pContext->mVeChn, &H264Vui);
                alogd("VencChn[%d] fill framerate %d to H264VUI", pContext->mVeChn, pContext->mConfigPara.mVideoFrameRate);
            }
            else if(PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H265_VUI_S H265Vui;
                memset(&H265Vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
                AW_MPI_VENC_GetH265Vui(pContext->mVeChn, &H265Vui);
                H265Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H265Vui.VuiTimeInfo.num_units_in_tick = 1000;
                /* Notices: the protocol syntax states that h265 does not need to be multiplied by 2. */
                H265Vui.VuiTimeInfo.time_scale = H265Vui.VuiTimeInfo.num_units_in_tick * pContext->mConfigPara.mVideoFrameRate;
                H265Vui.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = H265Vui.VuiTimeInfo.num_units_in_tick;
                AW_MPI_VENC_SetH265Vui(pContext->mVeChn, &H265Vui);
                alogd("VencChn[%d] fill framerate %d to H265VUI", pContext->mVeChn, pContext->mConfigPara.mVideoFrameRate);
            }
        }

        if (0 <= pContext->mConfigPara.mSuperFrmMode)
        {
            VENC_SUPERFRAME_CFG_S mSuperFrmParam;
            memset(&mSuperFrmParam, 0, sizeof(VENC_SUPERFRAME_CFG_S));
            mSuperFrmParam.enSuperFrmMode = SUPERFRM_NONE;
            if (0 == pContext->mConfigPara.mSuperIFrmBitsThr || 0 == pContext->mConfigPara.mSuperPFrmBitsThr)
            {
                float cmp_bits = 1.5*1024*1024 / 20;
                float dst_bits = (float)pContext->mConfigPara.mVideoBitRate / pContext->mConfigPara.mVideoFrameRate;
                float bits_ratio = dst_bits / cmp_bits;
                mSuperFrmParam.SuperIFrmBitsThr = (unsigned int)((8.0*200*1024) * bits_ratio);
                mSuperFrmParam.SuperPFrmBitsThr = mSuperFrmParam.SuperIFrmBitsThr / 3;
            }
            else
            {
                mSuperFrmParam.SuperIFrmBitsThr = pContext->mConfigPara.mSuperIFrmBitsThr;
                mSuperFrmParam.SuperPFrmBitsThr = pContext->mConfigPara.mSuperPFrmBitsThr;
            }
            alogd("SuperFrm Mode:%d, I:%d, P:%d", mSuperFrmParam.enSuperFrmMode, mSuperFrmParam.SuperIFrmBitsThr, mSuperFrmParam.SuperPFrmBitsThr);
            AW_MPI_VENC_SetSuperFrameCfg(pContext->mVeChn, &mSuperFrmParam);
        }

        if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
        {
            VENC_PARAM_JPEG_S stJpegParam;
            memset(&stJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
            stJpegParam.Qfactor = pContext->mConfigPara.mQuality;
            AW_MPI_VENC_SetJpegParam(pContext->mVeChn, &stJpegParam);
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    ERRORTYPE ret;

    //create vi channel
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViDev = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pContext->mViDev = pContext->mConfigPara.mVippDev;
    }
    pContext->mIspDev = 0;
    pContext->mViChn = 0;

    ret = AW_MPI_VI_CreateVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
    }

    memset(&pContext->mViAttr, 0, sizeof(VI_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViAttr.mOnlineEnable = 1;
        pContext->mViAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.srcPixFmt);
    pContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mViAttr.format.colorspace = pContext->mConfigPara.mColorSpace;
    pContext->mViAttr.format.width = pContext->mConfigPara.srcWidth;
    pContext->mViAttr.format.height = pContext->mConfigPara.srcHeight;
    pContext->mViAttr.nbufs =  pContext->mConfigPara.mViBufferNum;
    alogd("vipp use %d v4l2 buffers, colorspace: 0x%x", pContext->mViAttr.nbufs, pContext->mViAttr.format.colorspace);
    pContext->mViAttr.nplanes = 2;
    pContext->mViAttr.wdr_mode = pContext->mConfigPara.wdr_en;
    alogd("wdr_mode %d", pContext->mViAttr.wdr_mode);
    pContext->mViAttr.fps = pContext->mConfigPara.mVideoFrameRate;
    pContext->mViAttr.drop_frame_num = pContext->mConfigPara.mViDropFrameNum;
    pContext->mViAttr.mbEncppEnable = pContext->mConfigPara.mEncppEnable;

    ret = AW_MPI_VI_SetVippAttr(pContext->mViDev, &pContext->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
#if ISP_RUN
    AW_MPI_ISP_Run(pContext->mIspDev);
#endif

    // Saturation change
    if (pContext->mConfigPara.mSaturationChange)
    {
        int nSaturationValue = 0;
        AW_MPI_ISP_GetSaturation(pContext->mIspDev, &nSaturationValue);
        alogd("current SaturationValue: %d", nSaturationValue);
        nSaturationValue = nSaturationValue + pContext->mConfigPara.mSaturationChange;
        AW_MPI_ISP_SetSaturation(pContext->mIspDev, nSaturationValue);
        AW_MPI_ISP_GetSaturation(pContext->mIspDev, &nSaturationValue);
        alogd("after change, SaturationValue: %d", nSaturationValue);
    }

    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = TRUE;
    ret = AW_MPI_VI_CreateVirChn(pContext->mViDev, pContext->mViChn, &stVirChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pContext->mViChn);
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VI_RegisterCallback(pContext->mViDev, &cbInfo);
    
    ret = AW_MPI_VI_EnableVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    return ret;
}

static ERRORTYPE prepare(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    BOOL nSuccessFlag;
    MUX_CHN nMuxChn;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret;
    ERRORTYPE result = FAILURE;

    if (createViChn(pContext) != SUCCESS)
    {
        aloge("create vi chn fail");
        return result;
    }

    if (createVencChn(pContext) != SUCCESS)
    {
        aloge("create venc chn fail");
        return result;
    }

    if (createMuxGrp(pContext) != SUCCESS)
    {
        aloge("create mux group fail");
        return result;
    }

    //set spspps
    if (pContext->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        memset(&H264SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &H264SpsPpsInfo);
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH264SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH264SpsPpsInfo(pContext->mMuxGrp, pContext->mVeChn, &H264SpsPpsInfo);
    }
    else if(pContext->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        memset(&H265SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &H265SpsPpsInfo);
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH265SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH265SpsPpsInfo(pContext->mMuxGrp, pContext->mVeChn, &H265SpsPpsInfo);
    }

    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            nMuxChn = 0;
            nSuccessFlag = FALSE;
            while (pEntry->mMuxChn < MUX_MAX_CHN_NUM)
            {
                ret = AW_MPI_MUX_CreateChn(pContext->mMuxGrp, nMuxChn, &pEntry->mMuxChnAttr, pEntry->mSinkInfo.mOutputFd);
                if (SUCCESS == ret)
                {
                    nSuccessFlag = TRUE;
                    alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pContext->mMuxGrp, \
                        nMuxChn, pEntry->mMuxChnAttr.mMuxerId);
                    break;
                }
                else if(ERR_MUX_EXIST == ret)
                {
                    nMuxChn++;
                    //break;
                }
                else
                {
                    nMuxChn++;
                }
            }

            if (FALSE == nSuccessFlag)
            {
                pEntry->mMuxChn = MM_INVALID_CHN;
                aloge("fatal error! create mux group[%d] channel fail!", pContext->mMuxGrp);
            }
            else
            {
                result = SUCCESS;
                pEntry->mMuxChn = nMuxChn;
            }
        }
    }
    else
    {
        aloge("maybe something wrong,mux chn list is empty");
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};

        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    if (pContext->mVeChn >= 0 && pContext->mMuxGrp >= 0)
    {
        MPP_CHN_S MuxGrp = {MOD_ID_MUX, 0, pContext->mMuxGrp};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};

        AW_MPI_SYS_Bind(&VeChn, &MuxGrp);
        pContext->mCurrentState = REC_PREPARED;
    }

    return result;
}

static ERRORTYPE start(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("start");

    ret = AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn);
    if (ret != SUCCESS)
    {
        alogd("VI enable error!");
        return FAILURE;
    }

    if (pContext->mVeChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(pContext->mVeChn);
    }

    if (pContext->mMuxGrp >= 0)
    {
        AW_MPI_MUX_StartGrp(pContext->mMuxGrp);
    }

    pContext->mCurrentState = REC_RECORDING;

    return ret;
}

static ERRORTYPE stop(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret = SUCCESS;

    alogd("stop");

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    }

    if (pContext->mMuxGrp >= 0)
    {
        alogd("stop mux grp");
        AW_MPI_MUX_StopGrp(pContext->mMuxGrp);
    }
    if (pContext->mMuxGrp >= 0)
    {
        alogd("destory mux grp");
        AW_MPI_MUX_DestroyGrp(pContext->mMuxGrp);
        pContext->mMuxGrp = MM_INVALID_CHN;
    }
    if (pContext->mVeChn >= 0)
    {
        alogd("destory venc");
        //AW_MPI_VENC_ResetChn(pContext->mVeChn);
        AW_MPI_VENC_DestroyChn(pContext->mVeChn);
        pContext->mVeChn = MM_INVALID_CHN;
    }
    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);
        AW_MPI_VI_DisableVipp(pContext->mViDev);
    #if ISP_RUN
        AW_MPI_ISP_Stop(pContext->mIspDev);
    #endif
        AW_MPI_VI_DestroyVipp(pContext->mViDev);
    }

    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        alogd("free chn list node");
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                alogd("close file");
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }

            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    return SUCCESS;
}

ERRORTYPE SampleVI2Venc2Muxer_CreateFolder(const char* pStrFolderPath)
{
    if(NULL == pStrFolderPath || 0 == strlen(pStrFolderPath))
    {
        aloge("folder path is wrong!");
        return FAILURE;
    }
    //check folder existence
    struct stat sb;
    if (stat(pStrFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pStrFolderPath, (int)sb.st_mode);
            return FAILURE;
        }
    }
    //create folder if necessary
    int ret = mkdir(pStrFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pStrFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pStrFolderPath);
        return FAILURE;
    }
}

static ERRORTYPE resetCamera(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    alogd("stop");

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    }

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        alogd("UnBind vi ve");
        AW_MPI_SYS_UnBind(&ViChn, &VeChn);
    }

    if (pContext->mViChn >= 0)
    {
        alogd("DestroyVirChn");
        AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);
        alogd("DisableVipp");
        AW_MPI_VI_DisableVipp(pContext->mViDev);
#if ISP_RUN
        alogd("ISP_Stop");
        AW_MPI_ISP_Stop(pContext->mIspDev);
#endif
        alogd("DestroyVipp");
        AW_MPI_VI_DestroyVipp(pContext->mViDev);
    }

    alogd("createViChn");
    createViChn(pContext);

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        alogd("Bind vi ve");
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    alogd("start");

    if (pContext->mViDev >= 0 && pContext->mViChn >= 0)
    {
        alogd("VI_EnableVirChn");
        AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("VENC_StartRecvPic");
        AW_MPI_VENC_StartRecvPic(pContext->mVeChn);
    }
}

static void *MsgQueueThread(void *pThreadData)
{
    SAMPLE_VI2VENC2MUXER_S *pContext = (SAMPLE_VI2VENC2MUXER_S*)pThreadData;
    message_t stCmdMsg;
    Vi2Venc2MuxerMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pContext->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Rec_NeedSetNextFd:
                {
                    int muxerId = nCmdPara;
                    char fileName[MAX_FILE_PATH_LEN] = {0};
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    if (muxerId == pMsgData->mpVi2Venc2MuxerData->mMuxId[0])
                    {
                        getFileNameByCurTime(pMsgData->mpVi2Venc2MuxerData, fileName);
                        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pFilePathNode, 0, sizeof(FilePathNode));
                        strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                        list_add_tail(&pFilePathNode->mList, &pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray[0]);
                    }
                    #ifdef DOUBLE_ENCODER_FILE_OUT
                        static int cnt = 0;
                        cnt++;
                        sprintf(fileName, "/mnt/extsd/test_%d.ts", cnt);
                        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                        list_add_tail(&pFilePathNode->mList, &pContext->mMuxerFileListArray[1]);
                    #endif
                    alogd("muxId[%d] set next fd, filepath=%s", muxerId, fileName);
                    setOutputFileSync(pContext, fileName, 0, muxerId);
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Rec_FileDone:
                {
                    int ret;
                    int muxerId = nCmdPara;
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;
                    int idx = -1;

                    if (muxerId == pMsgData->mpVi2Venc2MuxerData->mMuxId[0])
                    {
                        idx = 0;
                    }
                    #ifdef DOUBLE_ENCODER_FILE_OUT
                    else if (muxerId == pMsgData->mpVi2Venc2MuxerData->mMuxId[1])
                    {
                        idx = 1;
                    }
                    #endif
                    if (idx >= 0)
                    {
                        int cnt = 0;
                        struct list_head *pList;
                        list_for_each(pList, &pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray[idx]){cnt++;}
                        FilePathNode *pNode = NULL;
                        while (cnt > pMsgData->mpVi2Venc2MuxerData->mConfigPara.mDstFileMaxCnt)
                        {
                            pNode = list_first_entry(&pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray[idx], FilePathNode, mList);
                            if ((ret = remove(pNode->strFilePath)) != 0)
                            {
                                aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                            }
                            else
                            {
                                alogd("delete file[%s] success", pNode->strFilePath);
                            }
                            cnt--;
                            list_del(&pNode->mList);
                            free(pNode);
                        }
                    }
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Vi_Timeout:
                {
                    int ret;
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    resetCamera(pContext);
                    
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                }
                default:
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->mMsgQueue, 0);
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
}

int main(int argc, char** argv)
{
    int result = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);
    
	printf("sample_virvi2venc2muxer running!\n");
    SAMPLE_VI2VENC2MUXER_S *pContext = (SAMPLE_VI2VENC2MUXER_S* )malloc(sizeof(SAMPLE_VI2VENC2MUXER_S));

    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        result = FAILURE;
        goto _err0;
    }
    if (InitVi2Venc2MuxerData(pContext) != SUCCESS)
    {
        return -1;
    }
    gpVi2Venc2MuxerData = pContext;
    cdx_sem_init(&pContext->mSemExit, 0);

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("parse cmdline fail");
        result = FAILURE;
        goto err_out_0;
    }
    char *pConfPath = NULL;
    if(argc > 1)
    {
        pConfPath = pContext->mCmdLinePara.mConfigFilePath;
    }
        
    if (loadConfigPara(pContext, pConfPath) != SUCCESS)
    {
        aloge("load config file fail");
        result = FAILURE;
        goto err_out_0;
    }
    alogd("ViDropFrameNum=%d", pContext->mConfigPara.mViDropFrameNum);
    SampleVI2Venc2Muxer_CreateFolder(pContext->mDstDir);

    INIT_LIST_HEAD(&pContext->mMuxChnList);
    pthread_mutex_init(&pContext->mMuxChnListLock, NULL);

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    pContext->mMuxId[0] = addOutputFormatAndOutputSink(pContext, pContext->mConfigPara.dstVideoFile, MEDIA_FILE_FORMAT_MP4);
    if (pContext->mMuxId[0] < 0)
    {
        aloge("add first out file fail");
        goto err_out_1;
    }
    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
    memset(pFilePathNode, 0, sizeof(FilePathNode));
    strncpy(pFilePathNode->strFilePath, pContext->mConfigPara.dstVideoFile, MAX_FILE_PATH_LEN-1);
    list_add_tail(&pFilePathNode->mList, &pContext->mMuxerFileListArray[0]);

#ifdef DOUBLE_ENCODER_FILE_OUT
    char mov_path[MAX_FILE_PATH_LEN];
    strcpy(mov_path, "/mnt/extsd/test.ts");
    pContext->mMuxId[1] = addOutputFormatAndOutputSink(pContext, mov_path, MEDIA_FILE_FORMAT_TS);
    if (pContext->mMuxId[1] < 0)
    {
        alogd("add mMuxId[1] ts file sink fail");
    }
    else
    {
        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
        memset(pFilePathNode, 0, sizeof(FilePathNode));
        strncpy(pFilePathNode->strFilePath, mov_path, MAX_FILE_PATH_LEN-1);
        list_add_tail(&pFilePathNode->mList, &pContext->mMuxerFileListArray[1]);
    }
#endif

    if (prepare(pContext) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_2;
    }

    //create msg queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create Msg Queue Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create Msg Queue Thread success! threadId[0x%x]", &pContext->mMsgQueueThreadId);
    }

    start(pContext);

    //test roi.
    int i = 0;
    ERRORTYPE ret;
    VENC_ROI_CFG_S stMppRoiBlockInfo;
    memset(&stMppRoiBlockInfo, 0, sizeof(VENC_ROI_CFG_S));
    for(i=0; i<pContext->mConfigPara.mRoiNum; i++)
    {
        stMppRoiBlockInfo.Index = i;
        stMppRoiBlockInfo.bEnable = TRUE;
        stMppRoiBlockInfo.bAbsQp = TRUE;
        stMppRoiBlockInfo.Qp = pContext->mConfigPara.mRoiQp;
        stMppRoiBlockInfo.Rect.X = 128*i;
        stMppRoiBlockInfo.Rect.Y = 128*i;
        stMppRoiBlockInfo.Rect.Width = 128;
        stMppRoiBlockInfo.Rect.Height = 128;
        ret = AW_MPI_VENC_SetRoiCfg(pContext->mVeChn, &stMppRoiBlockInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! set roi[%d] fail[0x%x]!", i, ret);
        }
        else
        {
            alogd("set roiIndex:%d, Qp:%d-%d, Rect[%d,%d,%dx%d]", i, stMppRoiBlockInfo.bAbsQp, stMppRoiBlockInfo.Qp, 
                stMppRoiBlockInfo.Rect.X, stMppRoiBlockInfo.Rect.Y, stMppRoiBlockInfo.Rect.Width, stMppRoiBlockInfo.Rect.Height);
        }
    }
    
    if(pContext->mConfigPara.mRoiNum>0 && pContext->mConfigPara.mRoiBgFrameRateEnable)
    {
        VENC_ROIBG_FRAME_RATE_S stRoiBgFrmRate;
        ret = AW_MPI_VENC_GetRoiBgFrameRate(pContext->mVeChn, &stRoiBgFrmRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get roiBgFrameRate fail[0x%x]!", ret);
        }
        alogd("get roi bg frame rate:%d-%d", stRoiBgFrmRate.mSrcFrmRate, stRoiBgFrmRate.mDstFrmRate);
        if (pContext->mConfigPara.mRoiBgFrameRateAttenuation)
        {
            stRoiBgFrmRate.mDstFrmRate = stRoiBgFrmRate.mSrcFrmRate/pContext->mConfigPara.mRoiBgFrameRateAttenuation;
        }
        else
        {
            stRoiBgFrmRate.mDstFrmRate = stRoiBgFrmRate.mSrcFrmRate;
        }
        if(stRoiBgFrmRate.mDstFrmRate <= 0)
        {
            stRoiBgFrmRate.mDstFrmRate = 1;
        }
        ret = AW_MPI_VENC_SetRoiBgFrameRate(pContext->mVeChn, &stRoiBgFrmRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! set roiBgFrameRate fail[0x%x]!", ret);
        }
        alogd("set roi bg frame rate param:%d-%d", stRoiBgFrmRate.mSrcFrmRate, stRoiBgFrmRate.mDstFrmRate);
    }

    //test orl
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stRgnChnAttr;
    memset(&stRgnAttr, 0, sizeof(RGN_ATTR_S));
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    MPP_CHN_S viChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
    for(i=0; i<pContext->mConfigPara.mOrlNum; i++)
    {
        stRgnAttr.enType = ORL_RGN;
        ret = AW_MPI_RGN_Create(i, &stRgnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why create ORL region fail?[0x%x]", ret);
            break;
        }
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = ORL_RGN;
        stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.X = i*120;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y = i*60;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width = 100;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height = 50;
        stRgnChnAttr.unChnAttr.stOrlChn.mColor = 0xFF0000 >> ((i % 3)*8);
        stRgnChnAttr.unChnAttr.stOrlChn.mThick = 6;
        stRgnChnAttr.unChnAttr.stOrlChn.mLayer = i;
        ret = AW_MPI_RGN_AttachToChn(i, &viChn, &stRgnChnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why attach to vi channel[%d,%d] fail?", pContext->mViDev, pContext->mViChn);
        }
        
    }

    if(pContext->mConfigPara.mOrlNum > 0)
    {
        alogd("sleep 10s ...");
        sleep(10);
    }
    for(i=0; i<pContext->mConfigPara.mOrlNum; i++)
    {
        ret = AW_MPI_RGN_Destroy(i);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why destory region:%d fail?", i);
        }
    }

    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    //stop msg queue thread
    message_t stMsgCmd;
    stMsgCmd.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stMsgCmd);
    pthread_join(pContext->mMsgQueueThreadId, NULL);
    alogd("start to free res");
err_out_3:
    stop(pContext);
    result = 0;
err_out_2:
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        alogd("chn list not empty");
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }
        }

        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);
err_out_1:
    AW_MPI_SYS_Exit();

    pthread_mutex_destroy(&pContext->mMuxChnListLock);
err_out_0:
    cdx_sem_deinit(&pContext->mSemExit);
    message_destroy(&pContext->mMsgQueue);
    free(pContext);
    gpVi2Venc2MuxerData = pContext = NULL;
_err0:
	alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
