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
#define LOG_TAG "Sampletwinchnvirvi2venc2ve"

#include <unistd.h>
#include "plat_log.h"
#include <time.h>
#include <mm_common.h>

#include <openssl/rsa.h>
#include <openssl/ossl_typ.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/hmac.h>

#include "sample_twinchn_virvi2venc2ce.h"
#include "sample_twinchn_virvi2venc2ce_conf.h"


#define DEFAULT_SRC_SIZE   1080
#define DEFAULT_DST_VIDEO_FILE      "/mnt/extsd/video/1080p.mp4"

#define DEFAULT_SRC_SIZE   1080

#define DEFAULT_MAX_DURATION  60*1000
#define DEFAULT_DST_VIDEO_FRAMERATE 30
#define DEFAULT_DST_VIDEO_BITRATE 12*1000*1000

#define DEFAULT_SRC_PIXFMT   MM_PIXEL_FORMAT_YUV_PLANAR_420
#define DEFAULT_DST_PIXFMT   MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420
#define DEFAULT_ENCODER   PT_H264

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (4*1024)

static SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData_main;
static SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData_sub;

#define u32 unsigned int
#define u16 unsigned short
#define u8	unsigned char
#define s32 int
#define s16 short
#define s8  char
#define AES_PER_BLK (1024)
#define AES128_KEY_LENGTH (16)
#define AES_KEY_SIZE_256	32

unsigned char g_key[AES_KEY_SIZE_256] = {
	0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
	0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

static ERRORTYPE InitVi2Venc2CeData(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData)
{
    pVi2Venc2CeData->mConfigPara.srcSize = pVi2Venc2CeData->mConfigPara.dstSize = DEFAULT_SRC_SIZE;
    if (pVi2Venc2CeData->mConfigPara.srcSize == 3840)
    {
        pVi2Venc2CeData->mConfigPara.srcWidth = 3840;
        pVi2Venc2CeData->mConfigPara.srcHeight = 2160;
    }
    else if (pVi2Venc2CeData->mConfigPara.srcSize == 1080)
    {
        pVi2Venc2CeData->mConfigPara.srcWidth = 1920;
        pVi2Venc2CeData->mConfigPara.srcHeight = 1080;
    }
    else if (pVi2Venc2CeData->mConfigPara.srcSize == 720)
    {
        pVi2Venc2CeData->mConfigPara.srcWidth = 1280;
        pVi2Venc2CeData->mConfigPara.srcHeight = 720;
    }
    else if (pVi2Venc2CeData->mConfigPara.srcSize == 640)
    {
        pVi2Venc2CeData->mConfigPara.srcWidth = 640;
        pVi2Venc2CeData->mConfigPara.srcHeight = 360;
    }

    if (pVi2Venc2CeData->mConfigPara.dstSize == 3840)
    {
        pVi2Venc2CeData->mConfigPara.dstWidth = 3840;
        pVi2Venc2CeData->mConfigPara.dstHeight = 2160;
    }
    else if (pVi2Venc2CeData->mConfigPara.dstSize == 1080)
    {
        pVi2Venc2CeData->mConfigPara.dstWidth = 1920;
        pVi2Venc2CeData->mConfigPara.dstHeight = 1080;
    }
    else if (pVi2Venc2CeData->mConfigPara.dstSize == 720)
    {
        pVi2Venc2CeData->mConfigPara.dstWidth = 1280;
        pVi2Venc2CeData->mConfigPara.dstHeight = 720;
    }    
    else if (pVi2Venc2CeData->mConfigPara.srcSize == 640)
    {
        pVi2Venc2CeData->mConfigPara.srcWidth = 640;
        pVi2Venc2CeData->mConfigPara.srcHeight = 360;
    }

    pVi2Venc2CeData->mConfigPara.mMaxFileDuration = DEFAULT_MAX_DURATION;
    pVi2Venc2CeData->mConfigPara.mVideoFrameRate = DEFAULT_DST_VIDEO_FRAMERATE;
    pVi2Venc2CeData->mConfigPara.mVideoBitRate = DEFAULT_DST_VIDEO_BITRATE;

    pVi2Venc2CeData->mConfigPara.srcPixFmt = DEFAULT_SRC_PIXFMT;
    pVi2Venc2CeData->mConfigPara.dstPixFmt = DEFAULT_DST_PIXFMT;
    pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt = DEFAULT_ENCODER;

    pVi2Venc2CeData->mConfigPara.mColor2Grey = FALSE;
    pVi2Venc2CeData->mConfigPara.m3DNR = FALSE;

    pVi2Venc2CeData->mVeChn = MM_INVALID_CHN;
    pVi2Venc2CeData->mViChn = MM_INVALID_CHN;
    pVi2Venc2CeData->mViDev = MM_INVALID_DEV;

    strcpy(pVi2Venc2CeData->mConfigPara.dstVideoFile, DEFAULT_DST_VIDEO_FILE);

    pVi2Venc2CeData->mCurrentState = REC_NOT_PREPARED;

    return SUCCESS;
}

static ERRORTYPE parseCmdLine(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

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

              strncpy(pVi2Venc2CeData->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pVi2Venc2CeData->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_v459_twinchn_virvi2venc2ce.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData, const char *conf_path, int vi_node)
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

        if(vi_node == 0){
            pVi2Venc2CeData->mConfigPara.mDevNo = GetConfParaInt(&mConf, CFG_SRC_DEV_NODE_MAIN, 0);    
            pVi2Venc2CeData->mConfigPara.srcSize = GetConfParaInt(&mConf, CFG_SRC_SIZE_MAIN, 0);    
            pVi2Venc2CeData->mConfigPara.dstSize = GetConfParaInt(&mConf, CFG_DST_VIDEO_SIZE_MAIN, 0);
            aloge("%d,%d",pVi2Venc2CeData->mConfigPara.srcSize,GetConfParaInt(&mConf, CFG_SRC_SIZE_MAIN, 0));
            ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR_MAIN, NULL);
            strcpy(pVi2Venc2CeData->mConfigPara.dstVideoFile, ptr);
            
            pVi2Venc2CeData->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE_MAIN, 0);
            pVi2Venc2CeData->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE_MAIN, 0);
            pVi2Venc2CeData->mConfigPara.mMaxFileDuration = GetConfParaInt(&mConf, CFG_DST_VIDEO_DURATION_MAIN, 0);
            
            pVi2Venc2CeData->mConfigPara.mRcMode = GetConfParaInt(&mConf, CFG_RC_MODE_MAIN, 0);
            pVi2Venc2CeData->mConfigPara.mEnableFastEnc = GetConfParaInt(&mConf, CFG_FAST_ENC_MAIN, 0);
            pVi2Venc2CeData->mConfigPara.mEnableRoi = GetConfParaInt(&mConf, CFG_ROI_MAIN, 0);
            
            ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_ENCODER_MAIN, NULL);
        }else if(vi_node == 1){
            pVi2Venc2CeData->mConfigPara.mDevNo = GetConfParaInt(&mConf, CFG_SRC_DEV_NODE_SUB, 0);        
            pVi2Venc2CeData->mConfigPara.srcSize = GetConfParaInt(&mConf, CFG_SRC_SIZE_SUB, 0);
            pVi2Venc2CeData->mConfigPara.dstSize = GetConfParaInt(&mConf, CFG_DST_VIDEO_SIZE_SUB, 0);
            
            ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR_SUB, NULL);
            strcpy(pVi2Venc2CeData->mConfigPara.dstVideoFile, ptr);
            
            pVi2Venc2CeData->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE_SUB, 0);
            pVi2Venc2CeData->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE_SUB, 0);
            pVi2Venc2CeData->mConfigPara.mMaxFileDuration = GetConfParaInt(&mConf, CFG_DST_VIDEO_DURATION_SUB, 0);
            
            pVi2Venc2CeData->mConfigPara.mRcMode = GetConfParaInt(&mConf, CFG_RC_MODE_SUB, 0);
            pVi2Venc2CeData->mConfigPara.mEnableFastEnc = GetConfParaInt(&mConf, CFG_FAST_ENC_SUB, 0);
            pVi2Venc2CeData->mConfigPara.mEnableRoi = GetConfParaInt(&mConf, CFG_ROI_SUB, 0);
            
            ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_ENCODER_SUB, NULL);
        }
        if (pVi2Venc2CeData->mConfigPara.srcSize == 3840)
        {
            pVi2Venc2CeData->mConfigPara.srcWidth = 3840;
            pVi2Venc2CeData->mConfigPara.srcHeight = 2160;
        }
        else if (pVi2Venc2CeData->mConfigPara.srcSize == 1080)
        {
            pVi2Venc2CeData->mConfigPara.srcWidth = 1920;
            pVi2Venc2CeData->mConfigPara.srcHeight = 1080;
        }
        else if (pVi2Venc2CeData->mConfigPara.srcSize == 720)
        {
            pVi2Venc2CeData->mConfigPara.srcWidth = 1280;
            pVi2Venc2CeData->mConfigPara.srcHeight = 720;
        }    
        else if (pVi2Venc2CeData->mConfigPara.srcSize == 640)
        {
            pVi2Venc2CeData->mConfigPara.srcWidth = 640;
            pVi2Venc2CeData->mConfigPara.srcHeight = 360;
        }
        if (pVi2Venc2CeData->mConfigPara.dstSize == 3840)
        {
            pVi2Venc2CeData->mConfigPara.dstWidth = 3840;
            pVi2Venc2CeData->mConfigPara.dstHeight = 2160;
        }
        else if (pVi2Venc2CeData->mConfigPara.dstSize == 1080)
        {
            pVi2Venc2CeData->mConfigPara.dstWidth = 1920;
            pVi2Venc2CeData->mConfigPara.dstHeight = 1080;
        }
        else if (pVi2Venc2CeData->mConfigPara.dstSize == 720)
        {
            pVi2Venc2CeData->mConfigPara.dstWidth = 1280;
            pVi2Venc2CeData->mConfigPara.dstHeight = 720;
        }
        else if (pVi2Venc2CeData->mConfigPara.dstSize == 640)
        {
            pVi2Venc2CeData->mConfigPara.dstWidth = 640;
            pVi2Venc2CeData->mConfigPara.dstHeight = 360;
        }
        if (!strcmp(ptr, "H.264"))
        {
            pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt = PT_H264;
        }
        else if (!strcmp(ptr, "H.265"))
        {
            pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt = PT_H265;
        }
        else if (!strcmp(ptr, "MJPEG"))
        {
            pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt = PT_MJPEG;
        }
        else if (!strcmp(ptr, "JPEG"))
        {
            pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt = PT_JPEG;
        }
        else
        {
            aloge("error conf encoder type");
        }

        pVi2Venc2CeData->mConfigPara.mTestDuration = GetConfParaInt(&mConf, CFG_TEST_DURATION, 0);

        alogd("dev_node:%d, frame rate:%d, bitrate:%d, video_duration=%d, test_time=%d", pVi2Venc2CeData->mConfigPara.mDevNo,\
            pVi2Venc2CeData->mConfigPara.mVideoFrameRate, pVi2Venc2CeData->mConfigPara.mVideoBitRate,\
            pVi2Venc2CeData->mConfigPara.mMaxFileDuration, pVi2Venc2CeData->mConfigPara.mTestDuration);

        ptr	= (char *)GetConfParaString(&mConf, CFG_COLOR2GREY, NULL);
        if(!strcmp(ptr, "yes"))
        {
            pVi2Venc2CeData->mConfigPara.mColor2Grey = TRUE;
        }
        else
        {
            pVi2Venc2CeData->mConfigPara.mColor2Grey = FALSE;
        }

        ptr	= (char *)GetConfParaString(&mConf, CFG_3DNR, NULL);
        if(!strcmp(ptr, "yes"))
        {
            pVi2Venc2CeData->mConfigPara.m3DNR = TRUE;
        }
        else
        {
            pVi2Venc2CeData->mConfigPara.m3DNR = FALSE;
        }
     
        destroyConfParser(&mConf);
    }
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_VI2VENC2CE_S *pContext = (SAMPLE_VI2VENC2CE_S *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VENC == pChn->mModId)
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
                    ret = AW_MPI_VI_GetIspDev(pContext->mViDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mViDev, ret);
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
                        if (0 != mVEncChn)
                        {
                            mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren * pContext->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren * pContext->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren * pContext->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren * pContext->mEncppSharpAttenCoefPer / 100;
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
            default:
            {
                break;
            }
        }
    }
    return SUCCESS;
}

static ERRORTYPE configVencChnAttr(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData)
{
    memset(&pVi2Venc2CeData->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

    pVi2Venc2CeData->mVencChnAttr.VeAttr.Type = pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt;
    pVi2Venc2CeData->mVencChnAttr.VeAttr.SrcPicWidth  = pVi2Venc2CeData->mConfigPara.srcWidth;
    pVi2Venc2CeData->mVencChnAttr.VeAttr.SrcPicHeight = pVi2Venc2CeData->mConfigPara.srcHeight;
    pVi2Venc2CeData->mVencChnAttr.VeAttr.PixelFormat = pVi2Venc2CeData->mConfigPara.dstPixFmt;
    pVi2Venc2CeData->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pVi2Venc2CeData->mVencChnAttr.EncppAttr.mbEncppEnable = TRUE;
    pVi2Venc2CeData->mEncppSharpAttenCoefPer = 100 * pVi2Venc2CeData_sub->mConfigPara.srcWidth / pVi2Venc2CeData_main->mConfigPara.srcWidth;
    alogd("EncppSharpAttenCoefPer=%d", pVi2Venc2CeData->mEncppSharpAttenCoefPer);

    if (PT_H264 == pVi2Venc2CeData->mVencChnAttr.VeAttr.Type)
    {
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH264e.Profile = 2;//0:base 1:main 2:high
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pVi2Venc2CeData->mConfigPara.dstWidth;
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pVi2Venc2CeData->mConfigPara.dstHeight;
        if(pVi2Venc2CeData->mViDev == 1){
            pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH264e.BufSize = 6 * 1024 * 1024;
        }
        switch (pVi2Venc2CeData->mConfigPara.mRcMode)
        {
        case 1:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pVi2Venc2CeData->mVencRcParam.ParamH264Vbr.mMinQp = 10;
            pVi2Venc2CeData->mVencRcParam.ParamH264Vbr.mMaxQp = 52;
            break;
        case 2:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 28;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 28;
            break;
        case 3:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264QPMAP;
            break;
        case 0:
        default:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pVi2Venc2CeData->mConfigPara.mVideoFrameRate;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pVi2Venc2CeData->mConfigPara.mVideoBitRate;
            break;
        }
        if (pVi2Venc2CeData->mConfigPara.mEnableFastEnc)
        {
            pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH264e.FastEncFlag = TRUE;
        }
    }
    else if (PT_H265 == pVi2Venc2CeData->mVencChnAttr.VeAttr.Type)
    {
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH265e.mProfile = 1;//1:main 2:main10 3:sti11
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pVi2Venc2CeData->mConfigPara.dstWidth;
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pVi2Venc2CeData->mConfigPara.dstHeight;
        pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pVi2Venc2CeData->mConfigPara.mVideoBitRate;
        switch (pVi2Venc2CeData->mConfigPara.mRcMode)
        {
        case 1:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pVi2Venc2CeData->mVencRcParam.ParamH265Vbr.mMinQp = 10;
            pVi2Venc2CeData->mVencRcParam.ParamH265Vbr.mMaxQp = 52;
            break;
        case 2:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 28;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 28;
            break;
        case 3:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264QPMAP;
            break;
        case 0:
        default:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pVi2Venc2CeData->mConfigPara.mVideoFrameRate;
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pVi2Venc2CeData->mConfigPara.mVideoBitRate;
            break;
        }
        if (pVi2Venc2CeData->mConfigPara.mEnableFastEnc)
        {
            pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = TRUE;
        }
    }
    else if (PT_MJPEG == pVi2Venc2CeData->mVencChnAttr.VeAttr.Type)
    {
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pVi2Venc2CeData->mConfigPara.dstWidth;
        pVi2Venc2CeData->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pVi2Venc2CeData->mConfigPara.dstHeight;
        switch (pVi2Venc2CeData->mConfigPara.mRcMode)
        {
        case 0:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        case 1:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            break;
        case 2:
        case 3:
            aloge("not support! use default cbr mode");
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        default:
            pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        }
        pVi2Venc2CeData->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pVi2Venc2CeData->mConfigPara.mVideoBitRate;
    }

    alogd("venc ste Rcmode=%d", pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode);

    return SUCCESS;
}

int openssl_encrypt(u8 *in, u8 *out, u8 *key, u32 key_len, u32 data_len)
{

    EVP_CIPHER_CTX *ctx;
	int outl=0,outltmp=0;
	int rest = data_len, per_len = 0;
	unsigned char *inp = in, *outp = out;
	int ret = 0;
    int block_size;
    // ENGINE_load_builtin_engines(); // There is no such function in libcrypto 
    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL){
        aloge("EVP_CIPHER_CTX_new: ctx == NULL ?");
        EVP_CIPHER_CTX_free(ctx);
    }

	EVP_CIPHER_CTX_init(ctx);
    
	if( key_len == AES128_KEY_LENGTH ){
		aloge("aes 128\n");
		ret = EVP_EncryptInit_ex(ctx,EVP_aes_128_cbc(),NULL,key,NULL);
		if( ret != 1 ){
			aloge(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}else{
		ret = EVP_EncryptInit_ex(ctx,EVP_aes_128_cbc(),NULL,key,NULL);
		if( ret != 1 ){
			aloge(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}

	EVP_CIPHER_CTX_set_padding(ctx, 1);

    block_size = EVP_CIPHER_CTX_block_size(ctx);
    ret = EVP_EncryptUpdate(ctx, outp, &outltmp, (const unsigned char *)inp, data_len);

    outl += outltmp;

	if(0 < EVP_EncryptFinal_ex(ctx, outp+outltmp,&outltmp)){
        outl += outltmp;

    }else{
        aloge("data not align!");
    }
    
    EVP_CIPHER_CTX_cleanup(ctx);

//	aloge("enc ciphertext update length: %d, outltmp %d outl %d\n", data_len, outltmp, outl);
	return outl;
}

int openssl_decrypt(u8 *in, u8 *out, u8 *key, u32 key_len, u32 data_len)
{

	EVP_CIPHER_CTX *ctx;

	int outl=0,outltmp=0;;
	int ret = 0 ;
	int rest = data_len, per_len = 0;
	unsigned char *inp = in, *outp = out;
    int block_size;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL){
        aloge("EVP_CIPHER_CTX_new: ctx == NULL ?");
        EVP_CIPHER_CTX_free(ctx);
    }

	EVP_CIPHER_CTX_init(ctx);
	if( key_len == AES128_KEY_LENGTH ){
		aloge("aes 128\n");
		ret = EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(),NULL,key,NULL);
		if( ret != 1 ){
			aloge(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}else{
		ret = EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(),NULL,key,NULL);
		if( ret != 1 ){
			aloge(" Encryption init aes cbc 128 fail\n");
			return -1 ;
		}
	}

	EVP_CIPHER_CTX_set_padding(ctx, 1);

    block_size = EVP_CIPHER_block_size(ctx);


	ret = EVP_DecryptUpdate(ctx, outp, &outltmp, (const unsigned char *)inp, data_len);
	if( ret != 1 ) {
			aloge(" Encryption update fail\n");
			return -1 ;
	} else {
            outl += outltmp;
	}
	

	if(0 < EVP_DecryptFinal_ex(ctx, outp +outl, &outltmp)){
        outl += outltmp;
    }else
        aloge("data not align!");
        
	EVP_CIPHER_CTX_cleanup(ctx);    
//	aloge("dec ciphertext update length: %d, outltmp %d outl %d\n", data_len, outltmp, outl);
	return outl;
}


static void *GetEncoderFrameThread(void *pArg)
{
    int ret = 0;
    int count = 0;

    SAMPLE_VI2VENC2CE_S *pCap = (SAMPLE_VI2VENC2CE_S *)pArg;
    VI_DEV nViDev = pCap->mViDev;
    VI_CHN nViChn = pCap->mViChn;
    VENC_CHN nVencChn = pCap->mVeChn;
    VENC_STREAM_S VencFrame;
    VENC_PACK_S venc_pack;
    VencFrame.mPackCount = 1;
    VencFrame.mpPack = &venc_pack;
    alogd("Cap threadid=0x%lx, ViDev = %d, ViCh = %d\n", pCap->thid, nViDev, nViChn);

    int clen,plen,num;
    int total_frame_cnt = 0;
    int encrypt_success_cnt = 0;
    int encrypt_fail_cnt = 0;
    int decrypt_success_cnt = 0;
    int decrypt_fail_cnt = 0;

    while(count != 500)
    {
        count++;
        if((ret = AW_MPI_VENC_GetStream(nVencChn, &VencFrame, 4000)) < 0) //6000(25fps) 4000(30fps)
        {
            alogd("get first frmae failed!\n");
            continue;
        }
        else
        {
            total_frame_cnt++;
            int encrypt_fail_flag = 0;
            int decrypt_fail_flag = 0;
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen0)
            {
//                unsigned char in[17] = "ahsdiuwhqhwdhadsh";
//                unsigned char enout[64+EVP_MAX_BLOCK_LENGTH];
//                unsigned char decout[32];
//                aloge("in:%s,src_len:%d",in,strlen(in));
//                int enc_len = openssl_encrypt(in, enout, g_key, AES_KEY_SIZE_256, 17);
//                aloge("enout:%s,enc_len:%d",enout,enc_len);
//                int dec_len = openssl_decrypt(enout, decout, g_key, AES_KEY_SIZE_256, enc_len);
//                decout[17] = '\0';
//                aloge("decout:%s,dec_len:%d",decout,dec_len);

                int venc_frame_len = VencFrame.mpPack->mLen0;
                unsigned char *encout = malloc(venc_frame_len + EVP_MAX_BLOCK_LENGTH);
                unsigned char *decout = malloc(venc_frame_len + EVP_MAX_BLOCK_LENGTH);                
                int encout_len = 0;
                int decout_len = 0;
                if (encout != NULL && decout != NULL) {
                    encout_len = openssl_encrypt(VencFrame.mpPack->mpAddr0, encout, g_key, AES_KEY_SIZE_256, venc_frame_len);
                    if(encout_len > 0){
                        decout_len = openssl_decrypt(encout, decout, g_key, AES_KEY_SIZE_256, encout_len);                    
                        if(decout_len <= 0){
                            aloge("openssl dec failed!!!");
                            decrypt_fail_flag = 1;
                        }
                    }else{
                        aloge("openssl enc failed!!!");
                        encrypt_fail_flag = 1;
                    }
                    if(decout_len != venc_frame_len){
                        aloge("vipp[%d]chn[%d] openssl dec test fail! %d, %d",nViDev, nViChn, decout_len, venc_frame_len);
                        decrypt_fail_flag = 1;
                    }
                    free(encout);
                    free(decout);
                }
                //fwrite(VencFrame.mpPack->mpAddr0,1,VencFrame.mpPack->mLen0, gpSampleVirvi2VencContext->mOutputFileFp);
            }
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen1)
            {                
                int venc_frame_len = VencFrame.mpPack->mLen1;
                unsigned char *encout = malloc(venc_frame_len + EVP_MAX_BLOCK_LENGTH);
                unsigned char *decout = malloc(venc_frame_len + EVP_MAX_BLOCK_LENGTH);                
                int encout_len = 0;
                int decout_len = 0;
                if (encout != NULL && decout != NULL) {
                    encout_len = openssl_encrypt(VencFrame.mpPack->mpAddr1, encout, g_key, AES_KEY_SIZE_256, venc_frame_len);
                    if(encout_len > 0){
                        decout_len = openssl_decrypt(encout, decout, g_key, AES_KEY_SIZE_256, encout_len);                    
                        if(decout_len <= 0){
                            aloge("openssl dec failed!!!");
                            decrypt_fail_flag = 1;
                        }
                    }else{
                        aloge("openssl enc failed!!!");
                        encrypt_fail_flag = 1;
                    }
                    if(decout_len != venc_frame_len){
                        aloge("vipp[%d]chn[%d] openssl dec test fail! %d, %d",nViDev, nViChn, decout_len, venc_frame_len);
                        decrypt_fail_flag = 1;
                    }
                    free(encout);
                    free(decout);
                }
                //fwrite(VencFrame.mpPack->mpAddr1,1,VencFrame.mpPack->mLen1, gpSampleVirvi2VencContext->mOutputFileFp);
            }
            ret = AW_MPI_VENC_ReleaseStream(nVencChn, &VencFrame);
            if(ret < 0)
            {
                alogd("falied error,release failed!!!\n");
            }
            // check enc and dec result
            if (encrypt_fail_flag)
                encrypt_fail_cnt++;
            else
                encrypt_success_cnt++;
            if (decrypt_fail_flag)
                decrypt_fail_cnt++;
            else
                decrypt_success_cnt++;
         }
    }
    pCap->encrypt_fail_cnt = encrypt_fail_cnt;
    pCap->decrypt_fail_cnt = decrypt_fail_cnt;
    alogd("vipp[%d]chn[%d] total_frame_cnt: %d, encrypt_success_cnt:%d, encrypt_fail_cnt:%d, decrypt_success_cnt:%d, decrypt_fail_cnt:%d",
        nViDev, nViChn, total_frame_cnt, encrypt_success_cnt, encrypt_fail_cnt, decrypt_success_cnt, decrypt_fail_cnt);
    return NULL;
}


static ERRORTYPE createVencChn(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pVi2Venc2CeData);
    pVi2Venc2CeData->mVeChn = pVi2Venc2CeData->mViDev;
    while (pVi2Venc2CeData->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pVi2Venc2CeData->mVeChn, &pVi2Venc2CeData->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pVi2Venc2CeData->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pVi2Venc2CeData->mVeChn);
            pVi2Venc2CeData->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pVi2Venc2CeData->mVeChn, ret);
            pVi2Venc2CeData->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pVi2Venc2CeData->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = stFrameRate.DstFrmRate = pVi2Venc2CeData->mConfigPara.mVideoFrameRate;
        alogd("set venc framerate:%d", stFrameRate.DstFrmRate);
        AW_MPI_VENC_SetFrameRate(pVi2Venc2CeData->mVeChn, &stFrameRate);

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pVi2Venc2CeData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pVi2Venc2CeData->mVeChn, &cbInfo);

        if ( ((PT_H264 == pVi2Venc2CeData->mVencChnAttr.VeAttr.Type) || (PT_H265 == pVi2Venc2CeData->mVencChnAttr.VeAttr.Type))
            && ((VENC_RC_MODE_H264QPMAP == pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode) || (VENC_RC_MODE_H265QPMAP == pVi2Venc2CeData->mVencChnAttr.RcAttr.mRcMode))
           )
        {
            aloge("fatal error! not support qpmap currently!");
        }

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData)
{
    ERRORTYPE ret;

    //create vi channel
    pVi2Venc2CeData->mViDev = pVi2Venc2CeData->mConfigPara.mDevNo;
    pVi2Venc2CeData->mIspDev = 0;
    pVi2Venc2CeData->mViChn = 0;

    ret = AW_MPI_VI_CreateVipp(pVi2Venc2CeData->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
    }

    memset(&pVi2Venc2CeData->mViAttr, 0, sizeof(VI_ATTR_S));
    pVi2Venc2CeData->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pVi2Venc2CeData->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pVi2Venc2CeData->mViAttr.format.pixelformat = V4L2_PIX_FMT_NV21M;
    pVi2Venc2CeData->mViAttr.format.field = V4L2_FIELD_NONE;
    pVi2Venc2CeData->mViAttr.format.width = pVi2Venc2CeData->mConfigPara.srcWidth;
    pVi2Venc2CeData->mViAttr.format.height = pVi2Venc2CeData->mConfigPara.srcHeight;
    pVi2Venc2CeData->mViAttr.nbufs = 3; //5
    aloge("use %d v4l2 buffers!!!", pVi2Venc2CeData->mViAttr.nbufs);
    pVi2Venc2CeData->mViAttr.nplanes = 2;
    pVi2Venc2CeData->mViAttr.fps = pVi2Venc2CeData->mConfigPara.mVideoFrameRate;
    pVi2Venc2CeData->mViAttr.mbEncppEnable = TRUE;

    ret = AW_MPI_VI_SetVippAttr(pVi2Venc2CeData->mViDev, &pVi2Venc2CeData->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }

    ret = AW_MPI_VI_EnableVipp(pVi2Venc2CeData->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    AW_MPI_ISP_Run(pVi2Venc2CeData->mIspDev);

    ret = AW_MPI_VI_CreateVirChn(pVi2Venc2CeData->mViDev, pVi2Venc2CeData->mViChn, NULL);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pVi2Venc2CeData->mViChn);
    }

    return ret;
}

static ERRORTYPE prepare(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData)
{
    BOOL nSuccessFlag;
    MUX_CHN nMuxChn;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret;
    ERRORTYPE result = FAILURE;

    if (createViChn(pVi2Venc2CeData) != SUCCESS)
    {
        aloge("create vi chn fail");
        return result;
    }

    if (createVencChn(pVi2Venc2CeData) != SUCCESS)
    {
        aloge("create venc chn fail");
        return result;
    }

    //set spspps
    if (pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        AW_MPI_VENC_GetH264SpsPpsInfo(pVi2Venc2CeData->mVeChn, &H264SpsPpsInfo);
    }
    else if(pVi2Venc2CeData->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        AW_MPI_VENC_GetH265SpsPpsInfo(pVi2Venc2CeData->mVeChn, &H265SpsPpsInfo);
    }

    if (pVi2Venc2CeData->mConfigPara.mEnableRoi)
    {
        alogd("------------------ROI begin-----------------------");
        VENC_ROI_CFG_S VencRoiCfg;

        VencRoiCfg.bEnable = TRUE;
        VencRoiCfg.Index = 0;
        VencRoiCfg.Qp = 10;
        VencRoiCfg.bAbsQp = 0;
        VencRoiCfg.Rect.X = 20;
        VencRoiCfg.Rect.Y = 0;
        VencRoiCfg.Rect.Width = 1280;
        VencRoiCfg.Rect.Height = 320;
        AW_MPI_VENC_SetRoiCfg(pVi2Venc2CeData->mVeChn, &VencRoiCfg);

        VencRoiCfg.Index = 1;
        VencRoiCfg.Rect.X = 200;
        VencRoiCfg.Rect.Y = 600;
        VencRoiCfg.Rect.Width = 1000;
        VencRoiCfg.Rect.Height = 200;
        AW_MPI_VENC_SetRoiCfg(pVi2Venc2CeData->mVeChn, &VencRoiCfg);

        VencRoiCfg.Index = 2;
        VencRoiCfg.Rect.X = 640;
        VencRoiCfg.Rect.Y = 384;
        VencRoiCfg.Rect.Width = 640;
        VencRoiCfg.Rect.Height = 360;
        VencRoiCfg.Qp = 40;
        VencRoiCfg.bAbsQp = 0;
        AW_MPI_VENC_SetRoiCfg(pVi2Venc2CeData->mVeChn, &VencRoiCfg);

        alogd("------------------ROI end-----------------------");
    }
    if ((pVi2Venc2CeData->mViDev >= 0 && pVi2Venc2CeData->mViChn >= 0) && pVi2Venc2CeData->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pVi2Venc2CeData->mViDev, pVi2Venc2CeData->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pVi2Venc2CeData->mVeChn};

        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }
    return SUCCESS;
}

static ERRORTYPE start(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData)
{
    ERRORTYPE ret = SUCCESS;

    alogd("start");

    ret = AW_MPI_VI_EnableVirChn(pVi2Venc2CeData->mViDev, pVi2Venc2CeData->mViChn);
    if (ret != SUCCESS)
    {
        alogd("VI enable error!");
        return FAILURE;
    }

    if (pVi2Venc2CeData->mVeChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(pVi2Venc2CeData->mVeChn);
    }

    pVi2Venc2CeData->mCurrentState = REC_RECORDING;

    return ret;
}

static ERRORTYPE stop(SAMPLE_VI2VENC2CE_S *pVi2Venc2CeData)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret = SUCCESS;

    alogd("stop");

    if (pVi2Venc2CeData->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pVi2Venc2CeData->mViDev, pVi2Venc2CeData->mViChn);
    }

    if (pVi2Venc2CeData->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pVi2Venc2CeData->mVeChn);
    }


    if (pVi2Venc2CeData->mViChn >= 0)
    {
        AW_MPI_VI_DestroyVirChn(pVi2Venc2CeData->mViDev, pVi2Venc2CeData->mViChn);
        AW_MPI_VI_DisableVipp(pVi2Venc2CeData->mViDev);
        AW_MPI_VI_DestroyVipp(pVi2Venc2CeData->mViDev);
        AW_MPI_ISP_Stop(pVi2Venc2CeData->mIspDev);
    }

    if (pVi2Venc2CeData->mVeChn >= 0)
    {
        alogd("destory venc");
        AW_MPI_VENC_ResetChn(pVi2Venc2CeData->mVeChn);
        AW_MPI_VENC_DestroyChn(pVi2Venc2CeData->mVeChn);
        pVi2Venc2CeData->mVeChn = MM_INVALID_CHN;
    }

    return SUCCESS;
}

int main(int argc, char** argv)
{
    int result = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;

    pVi2Venc2CeData_main = (SAMPLE_VI2VENC2CE_S* )malloc(sizeof(SAMPLE_VI2VENC2CE_S));    
    pVi2Venc2CeData_sub = (SAMPLE_VI2VENC2CE_S* )malloc(sizeof(SAMPLE_VI2VENC2CE_S));
    if (pVi2Venc2CeData_main == NULL || pVi2Venc2CeData_sub == NULL)
    {
        aloge("malloc struct fail\n");
        return FAILURE;
    }
    memset(pVi2Venc2CeData_main, 0, sizeof(SAMPLE_VI2VENC2CE_S));
    memset(pVi2Venc2CeData_sub, 0, sizeof(SAMPLE_VI2VENC2CE_S));
	printf("sample_twinchn_virvi2venc2ce running!\n");
    if (InitVi2Venc2CeData(pVi2Venc2CeData_main) != SUCCESS)
    {
        return -1;
    }
    if(InitVi2Venc2CeData(pVi2Venc2CeData_sub) != SUCCESS){
        return -1;
    }
    if (parseCmdLine(pVi2Venc2CeData_main, argc, argv) != SUCCESS)
    {
        aloge("parse cmdline fail");
        goto err_out_0;
    }
    if (loadConfigPara(pVi2Venc2CeData_main, pVi2Venc2CeData_main->mCmdLinePara.mConfigFilePath, 0) != SUCCESS)
    {
        aloge("load main config file fail");
        goto err_out_0;
    }
    if (loadConfigPara(pVi2Venc2CeData_sub, pVi2Venc2CeData_main->mCmdLinePara.mConfigFilePath, 1) != SUCCESS)
    {
        aloge("load sub config file fail");
        goto err_out_0;
    }
    pVi2Venc2CeData_main->mSysConf.nAlignWidth = 32;    
    AW_MPI_SYS_SetConf(&pVi2Venc2CeData_main->mSysConf);
    AW_MPI_SYS_Init();

    if (prepare(pVi2Venc2CeData_main) != SUCCESS)
    {
        aloge("prepare main fail!");
    }
    if (prepare(pVi2Venc2CeData_sub) != SUCCESS)
    {
        aloge("prepare sub fail!");
    }

    start(pVi2Venc2CeData_main);
    start(pVi2Venc2CeData_sub);
    result = pthread_create(&pVi2Venc2CeData_main->thid, NULL, GetEncoderFrameThread, (void *)pVi2Venc2CeData_main);
    if (result < 0)
    {
        alogd("pthread_create failed! \n");
    }    
    result = pthread_create(&pVi2Venc2CeData_sub->thid, NULL, GetEncoderFrameThread, (void *)pVi2Venc2CeData_sub);
    if (result < 0)
    {
        alogd("pthread_create failed! \n");
    }
    pthread_join(pVi2Venc2CeData_main->thid, NULL);
    pthread_join(pVi2Venc2CeData_sub->thid, NULL);
    stop(pVi2Venc2CeData_main);
    stop(pVi2Venc2CeData_sub);

    if (pVi2Venc2CeData_main->decrypt_fail_cnt || pVi2Venc2CeData_main->encrypt_fail_cnt ||
        pVi2Venc2CeData_sub->decrypt_fail_cnt || pVi2Venc2CeData_sub->encrypt_fail_cnt)
    {
        result = -1;
        aloge("fatal error, main encrypt_fail_cnt:%d, decrypt_fail_cnt:%d, sub encrypt_fail_cnt:%d, decrypt_fail_cnt:%d",
            pVi2Venc2CeData_main->decrypt_fail_cnt, pVi2Venc2CeData_main->encrypt_fail_cnt,
            pVi2Venc2CeData_sub->decrypt_fail_cnt, pVi2Venc2CeData_sub->encrypt_fail_cnt);
    }
    else
    {
        result = 0;
    }
    alogd("start to free res");
err_out_1:
    AW_MPI_SYS_Exit();

err_out_0:
    free(pVi2Venc2CeData_main);    
    free(pVi2Venc2CeData_sub);    
    pVi2Venc2CeData_main = NULL;
    pVi2Venc2CeData_sub = NULL;
	alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
