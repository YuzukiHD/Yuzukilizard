/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_MotionDetect.c
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2021/09/13
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_MotionDetect"
#include <utils/plat_log.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include "media/mpi_venc.h"
#include "media/mpi_sys.h"
#include "mm_common.h"
#include "mm_comm_venc.h"
#include "mm_comm_rc.h"
#include <mpi_videoformat_conversion.h>

#include <confparser.h>
#include "sample_MotionDetect.h"
#include "sample_MotionDetect_config.h"


SampleMotionDetectContext *gpSampleMotionDetectContext = NULL;

static int ParseCmdLine(int argc, char **argv, SampleMotionDetectCmdLineParam *pCmdLinePara)
{
    alogd("sample MotionDetect:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleMotionDetectCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                aloge("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_MotionDetect.conf\n");
            ret = 1;
            break;
        }
        else
        {
            alogd("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }
    return ret;
}

static ERRORTYPE loadSampleMotionDetectConfig(SampleMotionDetectConfig *pConfig, const char *pConfPath)
{
    int ret = SUCCESS;
    
    pConfig->mEncoderCount = 1800;
    pConfig->mDevNum = 0;
    pConfig->mSrcPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    pConfig->mSrcWidth = 1920;
    pConfig->mSrcHeight = 1080;
    pConfig->mSrcFrameRate = 30;
    pConfig->mEncoderType = PT_H265;
    pConfig->mDstWidth = 1920;
    pConfig->mDstHeight = 1080;
    pConfig->mDstFrameRate = 30;
    pConfig->mDstBitRate = 2000000;
    strcpy(pConfig->mOutputFilePath, "/mnt/extsd/md.h265");
    pConfig->mIspAndVeLinkageEnable = 1;

    if(pConfPath != NULL)
    {
        char *ptr;
        CONFPARSER_S stConfParser;
        ret = createConfParser(pConfPath, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        memset(pConfig, 0, sizeof(SampleMotionDetectConfig));
        pConfig->mEncoderCount = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_EncoderCount, 0);
        pConfig->mDevNum = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_DevNum, 0);
        char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_MOTIONDETECT_SrcPicFormat, NULL);
        if(!strcmp(pStrPixelFormat, "nv21"))
        {
            pConfig->mSrcPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "lbc25"))
        {
            pConfig->mSrcPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else
        { 
            aloge("fatal error! use nv21 as default");
            pConfig->mSrcPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        pConfig->mSrcWidth = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_SrcWidth, 0);
        pConfig->mSrcHeight = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_SrcHeight, 0);
        pConfig->mSrcFrameRate = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_SrcFrameRate, 0);
        char *EncoderType = (char*)GetConfParaString(&stConfParser, SAMPLE_MOTIONDETECT_DstEncoderType, NULL);
        if(!strcmp(EncoderType, "H.264"))
        {
            pConfig->mEncoderType = PT_H264;
        }
        else if(!strcmp(EncoderType, "H.265"))
        {
            pConfig->mEncoderType = PT_H265;
        }
        else if(!strcmp(EncoderType, "MJPEG"))
        {
            pConfig->mEncoderType = PT_MJPEG;
        }
        else
        {
            alogw("unsupported venc type:%p,encoder type turn to H.264!",EncoderType);
            pConfig->mEncoderType = PT_H264;
        }
        pConfig->mDstWidth = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_DstWidth, 0);
        pConfig->mDstHeight = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_DstHeight, 0);
        pConfig->mDstFrameRate = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_DstFrameRate, 0);
        pConfig->mDstBitRate = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_DstBitRate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_MOTIONDETECT_OutputFilePath, NULL);
        strncpy(pConfig->mOutputFilePath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mOutputFilePath[MAX_FILE_PATH_SIZE-1] = '\0';

        pConfig->mOnlineEnable = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_OnlineEnable, 0);
        pConfig->mOnlineShareBufNum = GetConfParaInt(&stConfParser, SAMPLE_MOTIONDETECT_OnlineShareBufNum, 0);
        alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pConfig->mOnlineEnable,
            pConfig->mOnlineShareBufNum);
        
        destroyConfParser(&stConfParser);
    }
    alogd("vipp=%d, src[%dx%d,%d], dst[%dx%d,%d], dstBitRate=%d, outputFile[%s]",
           pConfig->mDevNum, pConfig->mSrcWidth, pConfig->mSrcHeight,pConfig->mSrcFrameRate,
           pConfig->mDstWidth, pConfig->mDstHeight, pConfig->mDstFrameRate,pConfig->mDstBitRate,
           pConfig->mOutputFilePath);
    return ret;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleMotionDetectContext *pContext = (SampleMotionDetectContext *)cookie;
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
                    ret = AW_MPI_VI_GetIspDev(pContext->mViDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pContext->mViDev, ret);
                        return -1;
                    }
                    alogv("update Ve2IspParam, route Isp[%d]-Vipp[%d]-VencChn[%d]", mIspDev, pContext->mViDev, pContext->mVeChn);

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

    return SUCCESS;
}

int main(int argc, char *argv[])
{
    int result = 0;
    ERRORTYPE ret;

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

    SampleMotionDetectContext *pContext = (SampleMotionDetectContext*)malloc(sizeof(SampleMotionDetectContext));
    gpSampleMotionDetectContext = pContext;
    memset(pContext, 0, sizeof(SampleMotionDetectContext));
    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    /* parse config file. */
    if(loadSampleMotionDetectConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    system("cat /proc/meminfo | grep Committed_AS");

    MPP_SYS_CONF_S stSysConf;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret < 0)
    {
        aloge("sys Init failed!");
        return -1;
    }
    pContext->mIspDev = 0;
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViDev = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pContext->mViDev = pContext->mConfigPara.mDevNum;
    }
    pContext->mViChn = 0;
    pContext->mVeChn = 0;

    /*Set VI Channel Attribute*/
    memset(&pContext->mViAttr, 0, sizeof(VI_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViAttr.mOnlineEnable = 1;
        pContext->mViAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mSrcPicFormat);
    pContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mViAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    pContext->mViAttr.format.width = pContext->mConfigPara.mSrcWidth;
    pContext->mViAttr.format.height = pContext->mConfigPara.mSrcHeight;
    pContext->mViAttr.fps = pContext->mConfigPara.mSrcFrameRate;
    pContext->mViAttr.use_current_win = 0;
    pContext->mViAttr.nbufs = 3;//5;
    pContext->mViAttr.nplanes = 2;
    pContext->mViAttr.mbEncppEnable = TRUE;

    /* venc chn attr */
    memset(&pContext->mVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mVEncChnAttr.VeAttr.mOnlineEnable = 1;
        pContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mVEncChnAttr.VeAttr.Type = pContext->mConfigPara.mEncoderType;
    pContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mDstFrameRate;
    pContext->mVEncChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.mSrcWidth;
    pContext->mVEncChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mSrcHeight;
    pContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVEncChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mSrcPicFormat;
    pContext->mVEncChnAttr.VeAttr.mColorSpace = pContext->mViAttr.format.colorspace;
    pContext->mVEncChnAttr.EncppAttr.mbEncppEnable = TRUE;
    pContext->mVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
    pContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;
    if(PT_H264 == pContext->mVEncChnAttr.VeAttr.Type)
    {
        pContext->mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        pContext->mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVEncChnAttr.VeAttr.AttrH264e.PicWidth = pContext->mConfigPara.mDstWidth;
        pContext->mVEncChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mDstHeight;
        pContext->mVEncChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
        pContext->mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        pContext->mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mDstBitRate;
        pContext->mVEncRcParam.ParamH264Vbr.mMaxQp = 51;
        pContext->mVEncRcParam.ParamH264Vbr.mMinQp = 1;
        pContext->mVEncRcParam.ParamH264Vbr.mMaxPqp = 51;
        pContext->mVEncRcParam.ParamH264Vbr.mMinPqp = 1;
        pContext->mVEncRcParam.ParamH264Vbr.mQpInit = 30;
        pContext->mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
        pContext->mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->mVEncRcParam.ParamH264Vbr.mQuality = 5;
        pContext->mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        pContext->mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_H265 == pContext->mVEncChnAttr.VeAttr.Type)
    {
        pContext->mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mDstWidth;
        pContext->mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mDstHeight;
        pContext->mVEncChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
        pContext->mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mDstBitRate;
        pContext->mVEncRcParam.ParamH265Vbr.mMaxQp = 51;
        pContext->mVEncRcParam.ParamH265Vbr.mMinQp = 1;
        pContext->mVEncRcParam.ParamH265Vbr.mMaxPqp = 51;
        pContext->mVEncRcParam.ParamH265Vbr.mMinPqp = 1;
        pContext->mVEncRcParam.ParamH265Vbr.mQpInit = 30;
        pContext->mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        pContext->mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->mVEncRcParam.ParamH265Vbr.mQuality = 5;
        pContext->mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        pContext->mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_MJPEG == pContext->mVEncChnAttr.VeAttr.Type)
    {
        pContext->mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= pContext->mConfigPara.mDstWidth;
        pContext->mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mDstHeight;
        pContext->mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mDstBitRate;
    }
    pContext->mVencFrameRateConfig.SrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
    pContext->mVencFrameRateConfig.DstFrmRate = pContext->mConfigPara.mDstFrameRate;

    AW_MPI_VI_CreateVipp(pContext->mViDev);
    AW_MPI_VI_SetVippAttr(pContext->mViDev, &pContext->mViAttr);

    AW_MPI_ISP_Run(pContext->mIspDev);

    AW_MPI_VI_EnableVipp(pContext->mViDev);

    ret = AW_MPI_VI_CreateVirChn(pContext->mViDev, pContext->mViChn, NULL);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Create ViChn[%d-%d] failed", pContext->mViDev, pContext->mViChn);
    }
    ret = AW_MPI_VENC_CreateChn(pContext->mVeChn, &pContext->mVEncChnAttr);
    if(ret != SUCCESS)
    {
        aloge("fatal error! create venc channel[%d] falied!\n", pContext->mVeChn);
    }
    AW_MPI_VENC_SetRcParam(pContext->mVeChn, &pContext->mVEncRcParam);
    AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &pContext->mVencFrameRateConfig);

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);

    MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
    ret = AW_MPI_SYS_Bind(&ViChn,&VeChn);
    if(ret !=SUCCESS)
    {
        alogd("fatal error! vi can not bind venc!!!");
    }
    ret = AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn);
    if (ret != SUCCESS) 
    {
        alogd("fatal error! viChn[%d-%d] enable error!", pContext->mViDev, pContext->mViChn);
    }
    ret = AW_MPI_VENC_StartRecvPic(pContext->mVeChn);
    if (ret != SUCCESS) 
    {
        alogd("VencChn[%d] Start RecvPic error!", pContext->mVeChn);
    }

    // motion search must be enabled when VE is running.
    VencMotionSearchParam mMotionParam;
    memset(&mMotionParam, 0, sizeof(VencMotionSearchParam));
    mMotionParam.en_motion_search = 1;
    mMotionParam.dis_default_para = 1;
    mMotionParam.hor_region_num = 10;
    mMotionParam.ver_region_num = 5;
    mMotionParam.large_mv_th = 20;
    mMotionParam.large_mv_ratio_th = 12.0f;
    mMotionParam.non_zero_mv_ratio_th = 20.0f;
    mMotionParam.large_sad_ratio_th = 30.0f;
    AW_MPI_VENC_SetMotionSearchParam(pContext->mVeChn, &mMotionParam);
    pContext->bMotionSearchEnable = TRUE;
    alogd("enable MotionSearch");
    //AW_MPI_VENC_GetMotionSearchParam(pContext->mVeChn, &mMotionParam);
    unsigned int size = mMotionParam.hor_region_num * mMotionParam.ver_region_num * sizeof(VencMotionSearchRegion);
    pContext->mMotionResult.region = (VencMotionSearchRegion *)malloc(size);
    if (NULL == pContext->mMotionResult.region)
    {
        aloge("fatal error! malloc region failed! size=%d", size);
    }
    else
    {
        memset(pContext->mMotionResult.region, 0, size);
    }

    //open output file
    if(strlen(pContext->mConfigPara.mOutputFilePath) > 0)
    {
        pContext->mOutputFileFp = fopen(pContext->mConfigPara.mOutputFilePath, "wb+");
        if(!pContext->mOutputFileFp)
        {
            aloge("fatal error! can't open file[%s]", pContext->mConfigPara.mOutputFilePath);
        }
    }
    
    VencHeaderData stVencHeader;
    if(PT_H264 == pContext->mVEncChnAttr.VeAttr.Type)
    {
        result = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &stVencHeader);
        if (SUCCESS == result)
        {
            if(stVencHeader.nLength)
            {
                if(pContext->mOutputFileFp != NULL)
                {
                    fwrite(stVencHeader.pBuffer, stVencHeader.nLength, 1, pContext->mOutputFileFp);
                }
            }
        }
        else
        {
            alogd("fatal error! AW_MPI_VENC GetH264SpsPpsInfo failed!");
        }
    }
    else if(PT_H265 == pContext->mVEncChnAttr.VeAttr.Type)
    {
        result = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &stVencHeader);
        if (SUCCESS == result)
        {
            if(stVencHeader.nLength)
            {
                if(pContext->mOutputFileFp != NULL)
                {
                    fwrite(stVencHeader.pBuffer, stVencHeader.nLength, 1, pContext->mOutputFileFp);
                }
            }
        }
        else
        {
            alogd("fatal error! AW_MPI_VENC GetH265SpsPpsInfo failed!");
        }
    }
    int count = 0;
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;
    while(count < pContext->mConfigPara.mEncoderCount)
    {
        if(pContext->mbExitFlag)
        {
            alogd("receive exit flag, exit!");
            break;
        }
        if((ret = AW_MPI_VENC_GetStream(pContext->mVeChn, &stVencStream, 1000)) < 0)
        {
            aloge("fatal error! get venc stream failed!");
            continue;
        }
        else
        {
            count++;
            if(pContext->mOutputFileFp)
            {
                if(stVencStream.mpPack != NULL && stVencStream.mpPack->mLen0)
                {
                    fwrite(stVencStream.mpPack->mpAddr0,1,stVencStream.mpPack->mLen0, pContext->mOutputFileFp);
                }
                if(stVencStream.mpPack != NULL && stVencStream.mpPack->mLen1)
                {
                    fwrite(stVencStream.mpPack->mpAddr1,1,stVencStream.mpPack->mLen1, pContext->mOutputFileFp);
                }
            }
            if (pContext->bMotionSearchEnable)
            {
                ret = AW_MPI_VENC_GetMotionSearchResult(pContext->mVeChn, &pContext->mMotionResult);
                if (SUCCESS == ret)
                {
                    int num = 0;
                    int i = 0;
                    for(i=0; i<pContext->mMotionResult.total_region_num; i++)
                    {
                        if(pContext->mMotionResult.region[i].is_motion)
                        {
                            alogd("area_%d:[(%d,%d),(%d,%d)]", i,
                                pContext->mMotionResult.region[i].pix_x_bgn, pContext->mMotionResult.region[i].pix_y_bgn,
                                pContext->mMotionResult.region[i].pix_x_end, pContext->mMotionResult.region[i].pix_y_end);
                            num++;
                        }
                    }
                    alogd("detect motion: total area:%d", num);
                }
            }
            
            ret = AW_MPI_VENC_ReleaseStream(pContext->mVeChn, &stVencStream);
            if(ret < 0)
            {
                aloge("fatal error! release stream failed.");
            }
         }
    }

    ret = AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    if(ret < 0)
    {
        aloge("fatal error! Disable ViChn[%d-%d] failed", pContext->mViDev, pContext->mViChn);
    }

    memset(&mMotionParam, 0, sizeof(VencMotionSearchParam));
    mMotionParam.en_motion_search = 0;
    AW_MPI_VENC_SetMotionSearchParam(pContext->mVeChn, &mMotionParam);
    pContext->bMotionSearchEnable = FALSE;
    alogd("disable MotionSearch");

    ret = AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! VencChn[%d] Stop Receive Picture error!", pContext->mVeChn);
    }
    ret = AW_MPI_VENC_ResetChn(pContext->mVeChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! VENC Reset Chn error!");
    }
    ret = AW_MPI_VENC_DestroyChn(pContext->mVeChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! VencChn[%d] Destroy Chn error!", pContext->mVeChn);
    }
    ret = AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);
    if(ret != SUCCESS)
    {
        aloge("Destory ViChn[%d-%d] failed", pContext->mViDev, pContext->mViChn);
    }
    AW_MPI_VI_DisableVipp(pContext->mViDev);
    AW_MPI_ISP_Stop(pContext->mIspDev);
    AW_MPI_VI_DestroyVipp(pContext->mViDev);
    /* exit mpp systerm */
    ret = AW_MPI_SYS_Exit();
    if (ret != SUCCESS)
    {
        aloge("fatal error! sys exit failed!");
    }
    if(pContext->mOutputFileFp)
    {
        fclose(pContext->mOutputFileFp);
        pContext->mOutputFileFp = NULL;
    }
    if (pContext->mMotionResult.region)
    {
        aloge("free pContext->mMotionResult.region");
        free(pContext->mMotionResult.region);
        pContext->mMotionResult.region = NULL;
    }

_exit:
    if(pContext!=NULL)
    {
        free(pContext);
        pContext = NULL;
    }
    gpSampleMotionDetectContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}

