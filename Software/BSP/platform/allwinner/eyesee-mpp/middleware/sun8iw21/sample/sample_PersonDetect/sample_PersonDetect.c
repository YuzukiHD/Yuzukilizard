/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_PersonDetect.c
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2021/09/13
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_PersonDetect"
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
#include <sys/prctl.h>

#include <hwdisplay.h>

#include "media/mpi_sys.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include "media/mpi_venc.h"
#include <mpi_vo.h>
#include <mpi_region.h>
#include <mpi_videoformat_conversion.h>
#include <SystemBase.h>

#include <confparser.h>
#include "sample_PersonDetect.h"
#include "sample_PersonDetect_config.h"

#define IE_FRAME_INTERVAL           (5)
#define ENABLE_PDETLIBS            (1)

#define PDET_SCORE_LEVEL (500)

SamplePersonDetectContext *gpSamplePersonDetectContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSamplePersonDetectContext)
    {
        cdx_sem_up(&gpSamplePersonDetectContext->mSemExit);
    }
}

static int ParseCmdLine(int argc, char **argv, SamplePersonDetectCmdLineParam *pCmdLinePara)
{
    alogd("sample PersonDetect:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SamplePersonDetectCmdLineParam));
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
                "\t-path /mnt/extsd/sample_PersonDetect.conf\n");
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

static PIXEL_FORMAT_E get_PIXEL_FORMAT_E_FromString(char *pStrPixelFormat)
{
    PIXEL_FORMAT_E ePixelFormat;
    if(NULL == pStrPixelFormat)
    {
        aloge("fatal error! NULL pointer, use nv21 as default.");
        return MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    if(!strcmp(pStrPixelFormat, "nv21"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "lbc25"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else
    { 
        aloge("fatal error! use nv21 as default");
        ePixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    return ePixelFormat;
}

static PAYLOAD_TYPE_E get_PAYLOAD_TYPE_E_FromString(char *pStrPayloadType)
{
    PAYLOAD_TYPE_E ePayloadType;
    if(NULL == pStrPayloadType)
    {
        aloge("fatal error! NULL pointer, use h264 as default.");
        return PT_H264;
    }
    if(!strcmp(pStrPayloadType, "H.264"))
    {
        ePayloadType = PT_H264;
    }
    else if(!strcmp(pStrPayloadType, "H.265"))
    {
        ePayloadType = PT_H265;
    }
    else if(!strcmp(pStrPayloadType, "MJPEG"))
    {
        ePayloadType = PT_MJPEG;
    }
    else
    {
        alogw("unsupported venc type:%s,encoder type turn to H.264!", pStrPayloadType);
        ePayloadType = PT_H264;
    }
    return ePayloadType;
}

static ERRORTYPE loadSamplePersonDetectConfig(SamplePersonDetectConfig *pConfig, const char *pConfPath)
{
    ERRORTYPE eError = SUCCESS;
    
    pConfig->mMainVipp = 0;
    pConfig->mMainSrcWidth = 1920;
    pConfig->mMainSrcHeight = 1080;
    pConfig->mMainPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;    //MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420
    pConfig->mSubVipp = 1;
    pConfig->mSubSrcWidth = 352;
    pConfig->mSubSrcHeight = 288;
    pConfig->mSubPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pConfig->mSrcFrameRate = 30;
    
    pConfig->mOrlColor = 0xFF0000;
    pConfig->mbEnableMainVippOrl = 1;
    pConfig->mbEnableSubVippOrl = 1;

    pConfig->mPreviewVipp = 1;
    pConfig->mPreviewWidth = 352;
    pConfig->mPreviewHeight = 288;
    
    pConfig->mMainEncodeType = PT_H265;
    pConfig->mMainEncodeWidth = 1920;
    pConfig->mMainEncodeHeight = 1080;
    pConfig->mMainEncodeFrameRate = 30;
    pConfig->mMainEncodeBitrate = 2000000;
    strcpy(pConfig->mMainFilePath, "mainStreamPd.raw");
    pConfig->mSubEncodeType = PT_H265;
    pConfig->mSubEncodeWidth = 352;
    pConfig->mSubEncodeHeight = 288;
    pConfig->mSubEncodeFrameRate = 30;
    pConfig->mSubEncodeBitrate = 512000;
    strcpy(pConfig->mSubFilePath, "subStreamPd.raw");
    pConfig->mTestDuration = 60;

    if(pConfPath != NULL)
    {
        char *ptr = NULL;
        CONFPARSER_S stConfParser;
        int ret = createConfParser(pConfPath, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        memset(pConfig, 0, sizeof(SamplePersonDetectConfig));
        pConfig->mMainVipp = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_MainVipp, 0);
        pConfig->mMainSrcWidth = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_MainSrcWidth, 0);
        pConfig->mMainSrcHeight = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_MainSrcHeight, 0);
        pConfig->mMainPixelFormat = get_PIXEL_FORMAT_E_FromString((char*)GetConfParaString(&stConfParser, SAMPLE_PERSONDETECT_MainPixelFormat, NULL));    //MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420
        pConfig->mSubVipp = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SubVipp, 0);
        pConfig->mSubSrcWidth = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SubSrcWidth, 0);
        pConfig->mSubSrcHeight = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SubSrcHeight, 0);
        pConfig->mSubPixelFormat = get_PIXEL_FORMAT_E_FromString((char*)GetConfParaString(&stConfParser, SAMPLE_PERSONDETECT_SubPixelFormat, NULL));
        pConfig->mSrcFrameRate = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SrcFrameRate, 0);
        pConfig->mOrlColor = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_OrlColor, 0);
        pConfig->mbEnableMainVippOrl = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_EnableMainVippOrl, 0);
        pConfig->mbEnableSubVippOrl = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_EnableSubVippOrl, 0);
    
        pConfig->mPreviewVipp = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_PreviewVipp, 0);
        pConfig->mPreviewWidth = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_PreviewWidth, 0);
        pConfig->mPreviewHeight = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_PreviewHeight, 0);
        
        pConfig->mMainEncodeType = get_PAYLOAD_TYPE_E_FromString((char*)GetConfParaString(&stConfParser, SAMPLE_PERSONDETECT_MainEncodeType, NULL));
        pConfig->mMainEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_MainEncodeWidth, 0);
        pConfig->mMainEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_MainEncodeHeight, 0);
        pConfig->mMainEncodeFrameRate = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_MainEncodeFrameRate, 0);
        pConfig->mMainEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_MainEncodeBitrate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_PERSONDETECT_MainFilePath, NULL);
        strncpy(pConfig->mMainFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubEncodeType = get_PAYLOAD_TYPE_E_FromString((char*)GetConfParaString(&stConfParser, SAMPLE_PERSONDETECT_SubEncodeType, NULL));
        pConfig->mSubEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SubEncodeWidth, 0);
        pConfig->mSubEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SubEncodeHeight, 0);
        pConfig->mSubEncodeFrameRate = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SubEncodeFrameRate, 0);
        pConfig->mSubEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_SubEncodeBitrate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_PERSONDETECT_SubFilePath, NULL);
        strncpy(pConfig->mSubFilePath, ptr, MAX_FILE_PATH_SIZE);

        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_PERSONDETECT_NnaNbgFilePath, NULL);
        strncpy(pConfig->mNnaNbgFilePath, ptr, MAX_FILE_PATH_SIZE);

        pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_PERSONDETECT_TestDuration, 0);
    }
    return eError;
}

static void configMainStream(SamplePersonDetectContext *pContext)
{
    pContext->mMainStream.mpSamplePersonDetectContext = (void*)pContext;
    pContext->mMainStream.mVipp = pContext->mConfigPara.mMainVipp;
    pContext->mMainStream.mViChn = 0;
    pContext->mMainStream.mVEncChn = 0;
    pContext->mMainStream.mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mMainStream.mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mMainStream.mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mMainPixelFormat);
    pContext->mMainStream.mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mMainStream.mViAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    pContext->mMainStream.mViAttr.format.width = pContext->mConfigPara.mMainSrcWidth;
    pContext->mMainStream.mViAttr.format.height = pContext->mConfigPara.mMainSrcHeight;
    pContext->mMainStream.mViAttr.fps = pContext->mConfigPara.mSrcFrameRate;
    pContext->mMainStream.mViAttr.use_current_win = 0;
    pContext->mMainStream.mViAttr.nbufs = 3;//5;
    pContext->mMainStream.mViAttr.nplanes = 2;
    pContext->mMainStream.mVEncChnAttr.VeAttr.Type = pContext->mConfigPara.mMainEncodeType;
    pContext->mMainStream.mVEncChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mMainEncodeFrameRate;
    pContext->mMainStream.mVEncChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.mMainSrcWidth;
    pContext->mMainStream.mVEncChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mMainSrcHeight;
    pContext->mMainStream.mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mMainStream.mVEncChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mMainPixelFormat;
    pContext->mMainStream.mVEncChnAttr.VeAttr.mColorSpace = pContext->mMainStream.mViAttr.format.colorspace;
    pContext->mMainStream.mVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
    pContext->mMainStream.mVEncRcParam.sensor_type = VENC_ST_EN_WDR;
    if(PT_H264 == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.PicWidth = pContext->mConfigPara.mMainEncodeWidth;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mMainEncodeHeight;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mMainEncodeBitrate;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMaxQp = 51;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMinQp = 1;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMaxPqp = 51;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMinPqp = 1;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mQpInit = 30;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mQuality = 5;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        pContext->mMainStream.mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_H265 == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mMainEncodeWidth;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mMainEncodeHeight;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mMainEncodeBitrate;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMaxQp = 51;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMinQp = 1;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMaxPqp = 51;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMinPqp = 1;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mQpInit = 30;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mQuality = 5;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        pContext->mMainStream.mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_MJPEG == pContext->mMainStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= pContext->mConfigPara.mMainEncodeWidth;
        pContext->mMainStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mMainEncodeHeight;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mMainStream.mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mMainEncodeBitrate;
    }
    pContext->mMainStream.mVEncFrameRateConfig.SrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
    pContext->mMainStream.mVEncFrameRateConfig.DstFrmRate = pContext->mConfigPara.mMainEncodeFrameRate;
}

static void configSubStream(SamplePersonDetectContext *pContext)
{
    pContext->mSubStream.mpSamplePersonDetectContext = (void*)pContext;
    pContext->mSubStream.mVipp = pContext->mConfigPara.mSubVipp;
    pContext->mSubStream.mViChn = 0;
    pContext->mSubStream.mVEncChn = 1;
    pContext->mSubStream.mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mSubStream.mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mSubStream.mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mSubPixelFormat);
    pContext->mSubStream.mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mSubStream.mViAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    pContext->mSubStream.mViAttr.format.width = pContext->mConfigPara.mSubSrcWidth;
    pContext->mSubStream.mViAttr.format.height = pContext->mConfigPara.mSubSrcHeight;
    pContext->mSubStream.mViAttr.fps = pContext->mConfigPara.mSrcFrameRate;
    pContext->mSubStream.mViAttr.use_current_win = 1;
    pContext->mSubStream.mViAttr.nbufs = 3;//5;
    pContext->mSubStream.mViAttr.nplanes = 2;
    pContext->mSubStream.mVEncChnAttr.VeAttr.Type = pContext->mConfigPara.mSubEncodeType;
    pContext->mSubStream.mVEncChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mSubEncodeFrameRate;
    pContext->mSubStream.mVEncChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.mSubSrcWidth;
    pContext->mSubStream.mVEncChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mSubSrcHeight;
    pContext->mSubStream.mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mSubStream.mVEncChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mSubPixelFormat;
    pContext->mSubStream.mVEncChnAttr.VeAttr.mColorSpace = pContext->mSubStream.mViAttr.format.colorspace;
    pContext->mSubStream.mVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
    pContext->mSubStream.mVEncRcParam.sensor_type = VENC_ST_EN_WDR;
    if(PT_H264 == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.PicWidth = pContext->mConfigPara.mSubEncodeWidth;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mSubEncodeHeight;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mSubEncodeBitrate;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMaxQp = 51;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMinQp = 1;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMaxPqp = 51;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMinPqp = 1;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mQpInit = 30;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mQuality = 5;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        pContext->mSubStream.mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_H265 == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mSubEncodeWidth;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mSubEncodeHeight;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mSubEncodeBitrate;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMaxQp = 51;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMinQp = 1;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMaxPqp = 51;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMinPqp = 1;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mQpInit = 30;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mQuality = 5;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        pContext->mSubStream.mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else if(PT_MJPEG == pContext->mSubStream.mVEncChnAttr.VeAttr.Type)
    {
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= pContext->mConfigPara.mSubEncodeWidth;
        pContext->mSubStream.mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mSubEncodeHeight;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mSubStream.mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mSubEncodeBitrate;
    }
    pContext->mSubStream.mVEncFrameRateConfig.SrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
    pContext->mSubStream.mVEncFrameRateConfig.DstFrmRate = pContext->mConfigPara.mSubEncodeFrameRate;
}

int initPDLib(SamplePersonDetectContext *pContext)
{
    Awnn_Det_Handle mAiModelHandle = NULL;
    Awnn_Init_t init_param;
    memset(&init_param, 0, sizeof(Awnn_Init_t));
    init_param.human_nb = pContext->mConfigPara.mNnaNbgFilePath;
    init_param.face_nb = NULL;
    awnn_detect_init(&mAiModelHandle, &init_param);
    if (NULL == mAiModelHandle)
    {
        aloge("fatal error, open model file failed!");
        return -1;
    }
    pContext->mAiModelHandle = mAiModelHandle;
    return 0;
}

int destroyPDLib(SamplePersonDetectContext *pContext)
{
    if(pContext->mAiModelHandle)
    {
        awnn_detect_exit(pContext->mAiModelHandle);
    }
    else
    {
        aloge("fatal error! why null?[%p]", pContext->mAiModelHandle);
    }
    return 0;
}

static int deduceDstRect(RECT_S *pDstRect, SIZE_S *pDstSize, RECT_S *pSrcRect, SIZE_S *pSrcSize)
{
    //aw vipp prefers to keep proportion rather than keep picture content.
    //all vipps accord to isp's output proportion, maybe it is also vipp0's proportion. 
    //For keeping proportion, vipp will cut picture before output to memory!
    //To simplify calculation, we assume dstSize has right proportion, then judge if vipp cut width or height.
    RECT_S stOrigSrcRect;
    SIZE_S stOrigSrcSize;
    if(pDstSize->Width*pSrcSize->Height == pSrcSize->Width*pDstSize->Height)
    { //dstW/dstH == srcW/srcH
        stOrigSrcSize = *pSrcSize;
        stOrigSrcRect = *pSrcRect;
    }
    else
    {
        if(pDstSize->Width*pSrcSize->Height > pSrcSize->Width*pDstSize->Height)
        { //dstW/dstH > srcW/srcH, judge that vipp cut src width
            stOrigSrcSize.Width = pDstSize->Width*pSrcSize->Height/pDstSize->Height;
            stOrigSrcSize.Height = pSrcSize->Height;
            //convert srcRect positon according to new OrigSrcSize.
            stOrigSrcRect.X = pSrcRect->X + (stOrigSrcSize.Width-pSrcSize->Width)/2;
            stOrigSrcRect.Y = pSrcRect->Y;
        }
        else
        { //dstW/dstH < srcW/srcH, judge that vipp cut src height
            stOrigSrcSize.Width = pSrcSize->Width;
            stOrigSrcSize.Height = pDstSize->Height*pSrcSize->Width/pDstSize->Width;
            //convert srcRect positon according to new OrigSrcSize.
            stOrigSrcRect.X = pSrcRect->X;
            stOrigSrcRect.Y = pSrcRect->Y + (stOrigSrcSize.Height-pSrcSize->Height)/2;;
        }
        stOrigSrcRect.Width = pSrcRect->Width;
        stOrigSrcRect.Height = pSrcRect->Height;
    }
    pDstRect->X = stOrigSrcRect.X*pDstSize->Width/stOrigSrcSize.Width;
    pDstRect->Y = stOrigSrcRect.Y*pDstSize->Height/stOrigSrcSize.Height;
    pDstRect->Width = stOrigSrcRect.Width*pDstSize->Width/stOrigSrcSize.Width;
    pDstRect->Height = stOrigSrcRect.Height*pDstSize->Height/stOrigSrcSize.Height;
    return 0;
}

/**
 * According to PersonDetect results, draw orl on vipp.
 */
static void processPDResults(SamplePersonDetectContext *pContext, BBoxResults_t *pPDResults)
{
    //delete all region, because we will delete region from all vipps, so we can call AW_MPI_RGN_Destroy() directly,
    //no need to call AW_MPI_RGN_DetachFromChn().
    ERRORTYPE ret;
    int i;
    for(i=0; i<pContext->mValidOrlHandleNum; i++)
    {
        ret = AW_MPI_RGN_Destroy(pContext->mOrlHandles[i]);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why destroy orlHandle:%d-%d fail[0x%x]?", i, pContext->mOrlHandles[i], ret);
        }
        pContext->mOrlHandles[i] = -1;
    }
    pContext->mValidOrlHandleNum = 0;

    //draw orl on all vipps.
    RGN_ATTR_S stRgnAttr;
    memset(&stRgnAttr, 0, sizeof(stRgnAttr));
    stRgnAttr.enType = ORL_RGN;
    RGN_CHN_ATTR_S stRgnChnAttr;
    if(pPDResults && pPDResults->valid_cnt > 0)
    {
        int i,nPdCnt;
        for (i=0,nPdCnt=0; i<pPDResults->valid_cnt; i++)
        {
            alogd("person detect:[%d-%d], [%d,%d,%d,%d], label:%d, score:%f", 
                    i, pPDResults->valid_cnt,
                    pPDResults->boxes[i].xmin,
                    pPDResults->boxes[i].ymin,
                    pPDResults->boxes[i].xmax,
                    pPDResults->boxes[i].ymax,
                    pPDResults->boxes[i].label,
                    pPDResults->boxes[i].score);
            //label = 1: person; =0: other.
            if(1==pPDResults->boxes[i].label && pPDResults->boxes[i].score*1000 >= PDET_SCORE_LEVEL)
            {
                nPdCnt++;
                if(pContext->mValidOrlHandleNum < MAX_ORL_HANDLE_NUM)
                {
                    pContext->mOrlHandles[pContext->mValidOrlHandleNum] = pContext->mValidOrlHandleNum;
                    ret = AW_MPI_RGN_Create(pContext->mOrlHandles[pContext->mValidOrlHandleNum], &stRgnAttr);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! why destroy orlHandle:%d-%d fail[0x%x]?", i, pContext->mOrlHandles[i], ret);
                    }
                    if(pContext->mConfigPara.mbEnableMainVippOrl)
                    {
                        memset(&stRgnChnAttr, 0, sizeof(stRgnChnAttr));
                        stRgnChnAttr.bShow = TRUE;
                        stRgnChnAttr.enType = ORL_RGN;
                        stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
                        SIZE_S dstSize = {pContext->mConfigPara.mMainSrcWidth, pContext->mConfigPara.mMainSrcHeight};
                        SIZE_S srcSize = {pContext->mConfigPara.mSubSrcWidth, pContext->mConfigPara.mSubSrcHeight};
                        RECT_S srcRect;
                        srcRect.X = pPDResults->boxes[i].xmin;
                        srcRect.Y = pPDResults->boxes[i].ymin;
                        srcRect.Width = pPDResults->boxes[i].xmax - pPDResults->boxes[i].xmin + 1;
                        srcRect.Height = pPDResults->boxes[i].ymax - pPDResults->boxes[i].ymin + 1;
                        deduceDstRect(&stRgnChnAttr.unChnAttr.stOrlChn.stRect, &dstSize, &srcRect, &srcSize);
                        stRgnChnAttr.unChnAttr.stOrlChn.mColor = pContext->mConfigPara.mOrlColor;
                        stRgnChnAttr.unChnAttr.stOrlChn.mThick = 2;
                        stRgnChnAttr.unChnAttr.stOrlChn.mLayer = pContext->mValidOrlHandleNum;
                        MPP_CHN_S stVIChn = {MOD_ID_VIU, pContext->mMainStream.mVipp, pContext->mMainStream.mViChn};
                        ret = AW_MPI_RGN_AttachToChn(pContext->mOrlHandles[pContext->mValidOrlHandleNum], &stVIChn, &stRgnChnAttr);
                        if(ret != SUCCESS)
                        {
                            aloge("fatal error! rgn[%d-%d] attach to viChn[%d-%d] fail[0x%x]!", 
                                pContext->mValidOrlHandleNum, pContext->mOrlHandles[pContext->mValidOrlHandleNum],
                                stVIChn.mDevId, stVIChn.mChnId, ret);
                        }
                    }
                    if(pContext->mConfigPara.mbEnableSubVippOrl)
                    {
                        memset(&stRgnChnAttr, 0, sizeof(stRgnChnAttr));
                        stRgnChnAttr.bShow = TRUE;
                        stRgnChnAttr.enType = ORL_RGN;
                        stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
                        RECT_S srcRect;
                        srcRect.X = pPDResults->boxes[i].xmin;
                        srcRect.Y = pPDResults->boxes[i].ymin;
                        srcRect.Width = pPDResults->boxes[i].xmax - pPDResults->boxes[i].xmin + 1;
                        srcRect.Height = pPDResults->boxes[i].ymax - pPDResults->boxes[i].ymin + 1;
                        stRgnChnAttr.unChnAttr.stOrlChn.stRect = srcRect;
                        stRgnChnAttr.unChnAttr.stOrlChn.mColor = pContext->mConfigPara.mOrlColor;
                        stRgnChnAttr.unChnAttr.stOrlChn.mThick = 1;
                        stRgnChnAttr.unChnAttr.stOrlChn.mLayer = pContext->mValidOrlHandleNum;
                        MPP_CHN_S stVIChn = {MOD_ID_VIU, pContext->mSubStream.mVipp, pContext->mSubStream.mViChn};
                        ret = AW_MPI_RGN_AttachToChn(pContext->mOrlHandles[pContext->mValidOrlHandleNum], &stVIChn, &stRgnChnAttr);
                        if(ret != SUCCESS)
                        {
                            aloge("fatal error! rgn[%d-%d] attach to viChn[%d-%d] fail[0x%x]!", 
                                pContext->mValidOrlHandleNum, pContext->mOrlHandles[pContext->mValidOrlHandleNum],
                                stVIChn.mDevId, stVIChn.mChnId, ret);
                        }
                    }
                    pContext->mValidOrlHandleNum++;
                }
                else
                {
                    alogw("fatal error! person number:%d is more than %d", nPdCnt, MAX_ORL_HANDLE_NUM);
                }
            }
            else
            {
                //alogd("person detect ignore:[%d], score:%f", i, pPDResults->persons[i].prob);
            }
        }
    }
}

static void* PersonDetectThread(void *pThreadData)
{
    int result = 0;
    ERRORTYPE ret;
    SamplePersonDetectContext *pContext = (SamplePersonDetectContext*)pThreadData;
    prctl(PR_SET_NAME, (unsigned long)"PersonDetectThread", 0, 0, 0);

    int sleep_time = 0; //unit:us
    int64_t nStartTimeStamp;    //unit:us
    int64_t nEndTimeStamp;      //unit:us
    int64_t getInterval = (1000000 / IE_FRAME_INTERVAL);    //unit:us
    //int64_t nBeginPts;
    //int64_t nEndPts;
    //int64_t nTotalRunTime = 0;  //unit:us
    //int64_t nTotalRunCount = 0;
    VIDEO_FRAME_INFO_S stFrame;

    Awnn_Run_t run_param;
    awnn_create_run(&run_param, AWNN_HUMAN_DET);

    while(!pContext->mbExitFlag)
    {
        nStartTimeStamp = CDX_GetSysTimeUsMonotonic();
        ret = AW_MPI_VI_GetFrame(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn, &stFrame, 1000);
        if(ret != SUCCESS)
        {
            alogw("Be careful! get frame fail!");
            continue;
        }
        //nBeginPts = CDX_GetSysTimeUsMonotonic();
        #if (1==ENABLE_PDETLIBS)
        unsigned char *input_buffers[2];
        input_buffers[0] = stFrame.VFrame.mpVirAddr[0];
        input_buffers[1] = stFrame.VFrame.mpVirAddr[1];

        run_param.input_buffers = input_buffers;
        run_param.thresh = 0.0f;
        BBoxResults_t *pPDResults = awnn_detect_run(pContext->mAiModelHandle, &run_param);
        #endif
        ret = AW_MPI_VI_ReleaseFrame(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn, &stFrame);
        if(ret != SUCCESS)
        {
            aloge("fatal error! release frame fail[0x%x]!", ret);
        }
        //nEndPts = CDX_GetSysTimeUsMonotonic();
//        {
//            nTotalRunTime += (nEndPts-nBeginPts);
//            nTotalRunCount++;
//            if(0 == nTotalRunCount%100)
//            {
//                alogd("pDet:%lld/%lld = %lld us", nTotalRunTime, nTotalRunCount, nTotalRunTime/nTotalRunCount);
//            }
//        }
        processPDResults(pContext, pPDResults);
        //calculate sleep_time to keep detectint speed to dst_fps.
        nEndTimeStamp = CDX_GetSysTimeUsMonotonic();
        if(getInterval > (nEndTimeStamp - nStartTimeStamp))
        {
            sleep_time = (int)(getInterval - (nEndTimeStamp - nStartTimeStamp));
            //disable virchn to avoid to occupy frames. let other virChn can get frame and release frame normally.
            AW_MPI_VI_DisableVirChn(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
            usleep(sleep_time);
            AW_MPI_VI_EnableVirChn(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
        }
        //To make sure to get latest frame, get all ready frames and discard them.
        while(1)
        {
            ret = AW_MPI_VI_GetFrame(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn, &stFrame, 0);
            if(ret != SUCCESS)
            {
                break;
            }
            ret = AW_MPI_VI_ReleaseFrame(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn, &stFrame);
            if(ret != SUCCESS)
            {
                aloge("fatal error! release frame fail[0x%x]!", ret);
            }
        }
    }

    awnn_destroy_run(&run_param);

    //before exit, must destroy all orl region.
    int i;
    for(i=0; i<pContext->mValidOrlHandleNum; i++)
    {
        ret = AW_MPI_RGN_Destroy(pContext->mOrlHandles[i]);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why destroy orlHandle:%d-%d fail[0x%x]?", i, pContext->mOrlHandles[i], ret);
        }
        pContext->mOrlHandles[i] = -1;
    }
    pContext->mValidOrlHandleNum = 0;
    alogd("PersonDetect thread exit!!!!!!!!");
    return (void*)result;
}

static void* PersonDetect_GetVEncStreamThread(void *pThreadData)
{
    SamplePDStreamContext *pStreamContext = (SamplePDStreamContext*)pThreadData;
    SamplePersonDetectContext *pContext = (SamplePersonDetectContext*)pStreamContext->mpSamplePersonDetectContext;
    char strThreadName[32];
    sprintf(strThreadName, "venc%d-stream", pStreamContext->mVEncChn);
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    ERRORTYPE ret = SUCCESS;
    int result = 0;
    unsigned int nStreamLen = 0;
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;
    //get spspps from venc.
    VencHeaderData stSpsPpsInfo;
    memset(&stSpsPpsInfo, 0, sizeof(stSpsPpsInfo));
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pStreamContext->mVEncChn, &stSpsPpsInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get spspps fail[0x%x]!", ret);
        }
    }
    else if(PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pStreamContext->mVEncChn, &stSpsPpsInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get spspps fail[0x%x]!", ret);
        }
    }
    else
    {
        aloge("fatal error! vencType:0x%x is wrong!", pStreamContext->mVEncChnAttr.VeAttr.Type);
    }
    if(pStreamContext->mFile)
    {
        if(stSpsPpsInfo.nLength > 0)
        {
            fwrite(stSpsPpsInfo.pBuffer, 1, stSpsPpsInfo.nLength, pStreamContext->mFile);
        }
    }
    while (!pContext->mbExitFlag)
    {
        memset(stVencStream.mpPack, 0, sizeof(VENC_PACK_S));
        ret = AW_MPI_VENC_GetStream(pStreamContext->mVEncChn, &stVencStream, 1000);
        if(SUCCESS == ret)
        {
            nStreamLen = stVencStream.mpPack[0].mLen0 + stVencStream.mpPack[0].mLen1 + stVencStream.mpPack[0].mLen2;
            if(nStreamLen <= 0)
            {
                aloge("fatal error! VencStream length error,[%d,%d,%d]!", stVencStream.mpPack[0].mLen0, stVencStream.mpPack[0].mLen1, stVencStream.mpPack[0].mLen2);
            }
            if(pStreamContext->mFile)
            {
                if(stVencStream.mpPack[0].mLen0 > 0)
                {
                    fwrite(stVencStream.mpPack[0].mpAddr0, 1, stVencStream.mpPack[0].mLen0, pStreamContext->mFile);
                }
                if(stVencStream.mpPack[0].mLen1 > 0)
                {
                    fwrite(stVencStream.mpPack[0].mpAddr1, 1, stVencStream.mpPack[0].mLen1, pStreamContext->mFile);
                }
                if(stVencStream.mpPack[0].mLen2 > 0)
                {
                    fwrite(stVencStream.mpPack[0].mpAddr2, 1, stVencStream.mpPack[0].mLen2, pStreamContext->mFile);
                }
            }
            ret = AW_MPI_VENC_ReleaseStream(pStreamContext->mVEncChn, &stVencStream);
            if(ret != SUCCESS)
            {
                aloge("fatal error! venc_chn[%d] releaseStream fail", pStreamContext->mVEncChn);
            }                
        }
        else
        {
            alogw("fatal error! vencChn[%d] get frame failed! check code!", pStreamContext->mVEncChn);
            continue;
        }
    }

    return (void*)result;
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

    SamplePersonDetectContext *pContext = (SamplePersonDetectContext*)malloc(sizeof(SamplePersonDetectContext));
    gpSamplePersonDetectContext = pContext;
    memset(pContext, 0, sizeof(SamplePersonDetectContext));
    cdx_sem_init(&pContext->mSemExit, 0);

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

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
    if(loadSamplePersonDetectConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
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
    if(pContext->mConfigPara.mPreviewWidth>0 || pContext->mConfigPara.mPreviewHeight>0)
    {
        pContext->mbPreviewFlag = 1;
    }
    else
    {
        pContext->mbPreviewFlag = 0;
    }
    pContext->mPreviewViChn = 1;        //vipp1, virChn1.
    pContext->mPersonDetectViChn = 2;   //vipp1, virChn2.
    pContext->mVoDev = 0;
    pContext->mUILayer = HLAY(2, 0);
    pContext->mVoLayer = HLAY(0, 0);
    pContext->mVoChn = 0;
    configMainStream(pContext);
    configSubStream(pContext);

    //start vipp0 and vipp1
    AW_MPI_VI_CreateVipp(pContext->mMainStream.mVipp);
    AW_MPI_VI_SetVippAttr(pContext->mMainStream.mVipp, &pContext->mMainStream.mViAttr);
    AW_MPI_ISP_Run(pContext->mIspDev);
    AW_MPI_VI_EnableVipp(pContext->mMainStream.mVipp);

    AW_MPI_VI_CreateVipp(pContext->mSubStream.mVipp);
    AW_MPI_VI_SetVippAttr(pContext->mSubStream.mVipp, &pContext->mSubStream.mViAttr);
    AW_MPI_VI_EnableVipp(pContext->mSubStream.mVipp);

    //vipp1-virChn1 preview.
    if(pContext->mbPreviewFlag)
    {
        /* enable vo dev */
        AW_MPI_VO_Enable(pContext->mVoDev);
        AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
        AW_MPI_VO_CloseVideoLayer(pContext->mUILayer); /* close ui layer. */
        VO_PUB_ATTR_S spPubAttr;
        AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
        spPubAttr.enIntfType = VO_INTF_LCD;
        spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
        AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);

        ret = AW_MPI_VI_CreateVirChn(pContext->mSubStream.mVipp, pContext->mPreviewViChn, NULL);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Create ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mPreviewViChn);
        }
        if(AW_MPI_VO_EnableVideoLayer(pContext->mVoLayer) != SUCCESS)
        {
            aloge("fatal error! enable video layer[%d] fail!", pContext->mVoLayer);
        }
        AW_MPI_VO_GetVideoLayerAttr(pContext->mVoLayer, &pContext->mLayerAttr);
        pContext->mLayerAttr.stDispRect.X = 0;
        pContext->mLayerAttr.stDispRect.Y = 0;
        pContext->mLayerAttr.stDispRect.Width = pContext->mConfigPara.mPreviewWidth;
        pContext->mLayerAttr.stDispRect.Height = pContext->mConfigPara.mPreviewHeight;
        AW_MPI_VO_SetVideoLayerAttr(pContext->mVoLayer, &pContext->mLayerAttr);
        ret = AW_MPI_VO_CreateChn(pContext->mVoLayer, pContext->mVoChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! create vo channel[%d-%d] fail!", pContext->mVoLayer, pContext->mVoChn);
        }
        AW_MPI_VO_SetChnDispBufNum(pContext->mVoLayer, pContext->mVoChn, 0);
        MPP_CHN_S VOChn = {MOD_ID_VOU, pContext->mVoLayer, pContext->mVoChn};
        MPP_CHN_S VIChn = {MOD_ID_VIU, pContext->mSubStream.mVipp, pContext->mPreviewViChn};
        AW_MPI_SYS_Bind(&VIChn, &VOChn);

        /* start vo, vi_channel. */
        ret = AW_MPI_VI_EnableVirChn(pContext->mSubStream.mVipp, pContext->mPreviewViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! enableVirChn[%d-%d] fail!", pContext->mSubStream.mVipp, pContext->mPreviewViChn);
        }
        AW_MPI_VO_StartChn(pContext->mVoLayer, pContext->mVoChn);
    }
    //vipp1-virChn2 person detect.
    initPDLib(pContext);

    ret = AW_MPI_VI_CreateVirChn(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn, NULL);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Create ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
    }
    ret = AW_MPI_VI_EnableVirChn(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! why enable viChn[%d-%d] fail?", pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
    }
    result = pthread_create(&pContext->mPDThreadId, NULL, PersonDetectThread, (void*)pContext);
    if (result != 0)
    {
        aloge("fatal error! pthread create fail[%d]", result);
    }
    //vipp0-virChn0 encode, vipp1-virChn0 encode
    if(strlen(pContext->mConfigPara.mMainFilePath) > 0)
    {
        pContext->mMainStream.mFile = fopen(pContext->mConfigPara.mMainFilePath, "wb");
        if(NULL == pContext->mMainStream.mFile)
        {
            aloge("fatal error! why open file[%s] fail?", pContext->mConfigPara.mMainFilePath);
        }
        ret = AW_MPI_VI_CreateVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn, NULL);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Create ViChn[%d-%d] failed", pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        }
        ret = AW_MPI_VENC_CreateChn(pContext->mMainStream.mVEncChn, &pContext->mMainStream.mVEncChnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! create venc channel[%d] falied!", pContext->mMainStream.mVEncChn);
        }
        AW_MPI_VENC_SetRcParam(pContext->mMainStream.mVEncChn, &pContext->mMainStream.mVEncRcParam);
        AW_MPI_VENC_SetFrameRate(pContext->mMainStream.mVEncChn, &pContext->mMainStream.mVEncFrameRateConfig);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mMainStream.mVipp, pContext->mMainStream.mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mMainStream.mVEncChn};
        ret = AW_MPI_SYS_Bind(&ViChn,&VeChn);
        if(ret !=SUCCESS)
        {
            aloge("fatal error! vi can not bind venc!!!");
        }
        ret = AW_MPI_VI_EnableVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        if (ret != SUCCESS) 
        {
            alogd("fatal error! viChn[%d-%d] enable error!", pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        }
        ret = AW_MPI_VENC_StartRecvPic(pContext->mMainStream.mVEncChn);
        if (ret != SUCCESS) 
        {
            alogd("VencChn[%d] Start RecvPic error!", pContext->mMainStream.mVEncChn);
        }
        result = pthread_create(&pContext->mMainStream.mStreamThreadId, NULL, PersonDetect_GetVEncStreamThread, (void*)&pContext->mMainStream);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }
    if(strlen(pContext->mConfigPara.mSubFilePath) > 0)
    {
        pContext->mSubStream.mFile = fopen(pContext->mConfigPara.mSubFilePath, "wb");
        if(NULL == pContext->mSubStream.mFile)
        {
            aloge("fatal error! why open file[%s] fail?", pContext->mConfigPara.mSubFilePath);
        }
        ret = AW_MPI_VI_CreateVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn, NULL);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Create ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        }
        ret = AW_MPI_VENC_CreateChn(pContext->mSubStream.mVEncChn, &pContext->mSubStream.mVEncChnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! create venc channel[%d] falied!", pContext->mSubStream.mVEncChn);
        }
        AW_MPI_VENC_SetRcParam(pContext->mSubStream.mVEncChn, &pContext->mSubStream.mVEncRcParam);
        AW_MPI_VENC_SetFrameRate(pContext->mSubStream.mVEncChn, &pContext->mSubStream.mVEncFrameRateConfig);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mSubStream.mVipp, pContext->mSubStream.mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mSubStream.mVEncChn};
        ret = AW_MPI_SYS_Bind(&ViChn, &VeChn);
        if(ret !=SUCCESS)
        {
            aloge("fatal error! vi can not bind venc!!!");
        }
        ret = AW_MPI_VI_EnableVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        if (ret != SUCCESS) 
        {
            alogd("fatal error! viChn[%d-%d] enable error!", pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        }
        ret = AW_MPI_VENC_StartRecvPic(pContext->mSubStream.mVEncChn);
        if (ret != SUCCESS) 
        {
            alogd("VencChn[%d] Start RecvPic error!", pContext->mSubStream.mVEncChn);
        }
        result = pthread_create(&pContext->mSubStream.mStreamThreadId, NULL, PersonDetect_GetVEncStreamThread, (void*)&pContext->mSubStream);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    if(pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }
    pContext->mbExitFlag = 1;

    void *pRetVal = NULL;
    //stop encode
    if(strlen(pContext->mConfigPara.mMainFilePath) > 0)
    {
        pthread_join(pContext->mMainStream.mStreamThreadId, &pRetVal);
        alogd("mainStream pRetVal=%p", pRetVal);
        ret = AW_MPI_VI_DisableVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Disable ViChn[%d-%d] failed", pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        }
        ret = AW_MPI_VENC_StopRecvPic(pContext->mMainStream.mVEncChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! VencChn[%d] Stop Receive Picture error!", pContext->mMainStream.mVEncChn);
        }
        ret = AW_MPI_VENC_ResetChn(pContext->mMainStream.mVEncChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! VENC Reset Chn error!");
        }
        ret = AW_MPI_VENC_DestroyChn(pContext->mMainStream.mVEncChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! VencChn[%d] Destroy Chn error!", pContext->mMainStream.mVEncChn);
        }
        ret = AW_MPI_VI_DestroyVirChn(pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Destory ViChn[%d-%d] failed", pContext->mMainStream.mVipp, pContext->mMainStream.mViChn);
        }
        if(pContext->mMainStream.mFile)
        {
            fclose(pContext->mMainStream.mFile);
            pContext->mMainStream.mFile = NULL;
        }
    }
    if(strlen(pContext->mConfigPara.mSubFilePath) > 0)
    {
        pthread_join(pContext->mSubStream.mStreamThreadId, &pRetVal);
        alogd("subStream pRetVal=%p", pRetVal);
        ret = AW_MPI_VI_DisableVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Disable ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        }
        ret = AW_MPI_VENC_StopRecvPic(pContext->mSubStream.mVEncChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! VencChn[%d] Stop Receive Picture error!", pContext->mSubStream.mVEncChn);
        }
        ret = AW_MPI_VENC_ResetChn(pContext->mSubStream.mVEncChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! VENC Reset Chn error!");
        }
        ret = AW_MPI_VENC_DestroyChn(pContext->mSubStream.mVEncChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! VencChn[%d] Destroy Chn error!", pContext->mSubStream.mVEncChn);
        }
        ret = AW_MPI_VI_DestroyVirChn(pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Destory ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mSubStream.mViChn);
        }
        if(pContext->mSubStream.mFile)
        {
            fclose(pContext->mSubStream.mFile);
            pContext->mSubStream.mFile = NULL;
        }
    }

    //stop person detect
    pthread_join(pContext->mPDThreadId, &pRetVal);
    alogd("PDThread pRetVal=%p", pRetVal);
    ret = AW_MPI_VI_DisableVirChn(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Disable ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
    }
    ret = AW_MPI_VI_DestroyVirChn(pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Destory ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mPersonDetectViChn);
    }
    destroyPDLib(pContext);

    //stop preview.
    if(pContext->mbPreviewFlag)
    {
        ret = AW_MPI_VI_DisableVirChn(pContext->mSubStream.mVipp, pContext->mPreviewViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! Disable ViChn[%d-%d] failed", pContext->mSubStream.mVipp, pContext->mPreviewViChn);
        }
        AW_MPI_VO_StopChn(pContext->mVoLayer, pContext->mVoChn);
        ret = AW_MPI_VO_DestroyChn(pContext->mVoLayer, pContext->mVoChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! VO DisableChn[%d-%d] fail!", pContext->mVoLayer, pContext->mVoChn);
        }
        /* disable vo layer */
        ret = AW_MPI_VO_DisableVideoLayer(pContext->mVoLayer);
        if(ret != SUCCESS)
        {
            aloge("fatal error! VO DisableVideoLayer[%d] fail!", pContext->mVoLayer);
        }
        //wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
        usleep(50*1000);
        ret = AW_MPI_VI_DestroyVirChn(pContext->mSubStream.mVipp, pContext->mPreviewViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! VI DestoryVirChn[%d-%d] fail!", pContext->mSubStream.mVipp, pContext->mPreviewViChn);
        }
        AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
        /* disable vo dev */
        AW_MPI_VO_Disable(pContext->mVoDev);
    }

    //stop vipp0 and vipp1
    AW_MPI_VI_DisableVipp(pContext->mSubStream.mVipp);
    AW_MPI_VI_DestroyVipp(pContext->mSubStream.mVipp);
    AW_MPI_VI_DisableVipp(pContext->mMainStream.mVipp);
    AW_MPI_ISP_Stop(pContext->mIspDev);
    AW_MPI_VI_DestroyVipp(pContext->mMainStream.mVipp);

    /* exit mpp systerm */
    ret = AW_MPI_SYS_Exit();
    if (ret != SUCCESS)
    {
        aloge("fatal error! sys exit failed!");
    }
    cdx_sem_deinit(&pContext->mSemExit);
_exit:
    if(pContext!=NULL)
    {
        free(pContext);
        pContext = NULL;
    }
    gpSamplePersonDetectContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}

