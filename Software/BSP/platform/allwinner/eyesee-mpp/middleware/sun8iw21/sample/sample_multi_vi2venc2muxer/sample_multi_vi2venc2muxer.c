/******************************************************************************
  Copyright (C), 2020-2030, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2020/5/15
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_multi_vi2venc2muxer"
#include "plat_log.h"

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <confparser.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_sys.h>
#include <mpi_vi.h>
#include <mpi_venc.h>
#include <mpi_mux.h>
#include <mpi_isp.h>
#include <cdx_list.h>

#include "sample_multi_vi2venc2muxer.h"
#include "sample_multi_vi2venc2muxer_conf.h"

#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)
#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)

static SampleMultiVi2venc2muxerContext *gpSampleMultiVi2venc2muxerContext = NULL;
static void handle_exit()
{
    alogd("user want to exit!");
    if(NULL != gpSampleMultiVi2venc2muxerContext)
    {
        cdx_sem_up(&gpSampleMultiVi2venc2muxerContext->mSemExit);
    }
}

static int ParseCmdLine(SampleMultiVi2venc2muxerContext *pThiz, int argc, char **argv)
{
    alogd("sample_multi_vi2venc2muxer:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(&pThiz->mCmdLinePara, 0, sizeof(pThiz->mCmdLinePara));
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
            else
            {
                strcpy(pThiz->mCmdLinePara.mConfigFilePath, argv[i]);
            }
        }
        else if(!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_multi_vi2venc2muxer.conf");
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

static int loadSampleMultiVi2venc2muxerConfig(SampleMultiVi2venc2muxerContext *pThiz, const char *conf_path)
{
    int ret = 0;
    //vipp configure
    pThiz->mConfigPara.mVipp0 = HVIDEO(0, 0);
    pThiz->mConfigPara.mVipp0Format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    pThiz->mConfigPara.mVipp0CaptureWidth = 1920;
    pThiz->mConfigPara.mVipp0CaptureHeight = 1080;
    pThiz->mConfigPara.mVipp0Framerate = 60;
    pThiz->mConfigPara.mVipp1 = HVIDEO(1, 0);
    pThiz->mConfigPara.mVipp1Format = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pThiz->mConfigPara.mVipp1CaptureWidth = 640;
    pThiz->mConfigPara.mVipp1CaptureHeight = 360;
    pThiz->mConfigPara.mVipp1Framerate = 60;
    //params of every video encode line
    pThiz->mConfigPara.mVideoAVipp = pThiz->mConfigPara.mVipp0;
    strcpy(pThiz->mConfigPara.mVideoAFile, "/mnt/extsd/1080p.mp4");
    pThiz->mConfigPara.mVideoAFileCnt = 3;
    pThiz->mConfigPara.mVideoAFramerate = 30;
    pThiz->mConfigPara.mVideoABitrate = 2*1000*1000;
    pThiz->mConfigPara.mVideoAWidth = 640;
    pThiz->mConfigPara.mVideoAHeight = 360;
    pThiz->mConfigPara.mVideoAEncoderType = PT_H264;
    pThiz->mConfigPara.mVideoARcMode = 0;
    pThiz->mConfigPara.mVideoADuration = 30;
    pThiz->mConfigPara.mVideoATimelapse = -1;
    pThiz->mConfigPara.mVideoBVipp = pThiz->mConfigPara.mVipp1;
    strcpy(pThiz->mConfigPara.mVideoBFile, "/mnt/extsd/640p.mp4");
    pThiz->mConfigPara.mVideoBFileCnt = 3;
    pThiz->mConfigPara.mVideoBFramerate = 30;
    pThiz->mConfigPara.mVideoBBitrate = 500*1000;
    pThiz->mConfigPara.mVideoBWidth = 640;
    pThiz->mConfigPara.mVideoBHeight = 360;
    pThiz->mConfigPara.mVideoBEncoderType = PT_H264;
    pThiz->mConfigPara.mVideoBRcMode = 0;
    pThiz->mConfigPara.mVideoBDuration = 30;
    pThiz->mConfigPara.mVideoBTimelapse = -1;
    pThiz->mConfigPara.mVideoCVipp = pThiz->mConfigPara.mVipp1;
    strcpy(pThiz->mConfigPara.mVideoCFile, "/mnt/extsd/timelapse_first.mp4");
    pThiz->mConfigPara.mVideoCFileCnt = 3;
    pThiz->mConfigPara.mVideoCFramerate = 30;
    pThiz->mConfigPara.mVideoCBitrate = 500*1000;
    pThiz->mConfigPara.mVideoCWidth = 640;
    pThiz->mConfigPara.mVideoCHeight = 360;
    pThiz->mConfigPara.mVideoCEncoderType = PT_H264;
    pThiz->mConfigPara.mVideoCRcMode = 0;
    pThiz->mConfigPara.mVideoCDuration = 30;
    pThiz->mConfigPara.mVideoCTimelapse = 200*1000; //unit:us
    pThiz->mConfigPara.mVideoDVipp = pThiz->mConfigPara.mVipp1;
    strcpy(pThiz->mConfigPara.mVideoDFile, "/mnt/extsd/timelapse_second.mp4");
    pThiz->mConfigPara.mVideoDFileCnt = 3;
    pThiz->mConfigPara.mVideoDFramerate = 30;
    pThiz->mConfigPara.mVideoDBitrate = 500*1000;
    pThiz->mConfigPara.mVideoDWidth = 640;
    pThiz->mConfigPara.mVideoDHeight = 360;
    pThiz->mConfigPara.mVideoDEncoderType = PT_H264;
    pThiz->mConfigPara.mVideoDRcMode = 0;
    pThiz->mConfigPara.mVideoDDuration = 30;
    pThiz->mConfigPara.mVideoDTimelapse = 1000*1000;
    pThiz->mConfigPara.mVideoEVipp = pThiz->mConfigPara.mVipp0;
    strcpy(pThiz->mConfigPara.mVideoEFileJpeg, "/mnt/extsd/1080p.jpeg");
    pThiz->mConfigPara.mVideoEFileJpegCnt = 3;
    pThiz->mConfigPara.mVideoEWidth = 1920;
    pThiz->mConfigPara.mVideoEHeight = 1080;
    pThiz->mConfigPara.mVideoEEncoderType = PT_JPEG;
    pThiz->mConfigPara.mVideoEPhotoInterval = 30;
    pThiz->mConfigPara.mTestDuration = 120;
    
    if(conf_path != NULL)
    {
        char *ptr;
        CONFPARSER_S stConf;
        ret = createConfParser(conf_path, &stConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return ret;
        }
        //vipp configure
        pThiz->mConfigPara.mVipp0 = HVIDEO(GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP0, 0), 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP0_FORMAT, NULL);
        if(!strcmp(ptr, "lbc2.5"))
        {
            pThiz->mConfigPara.mVipp0Format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else if(!strcmp(ptr, "lbc2.0"))
        {
            pThiz->mConfigPara.mVipp0Format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
        }
        else if(!strcmp(ptr, "lbc1.5"))
        {
            pThiz->mConfigPara.mVipp0Format = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
        }
        else if(!strcmp(ptr, "nv12"))
        {
            pThiz->mConfigPara.mVipp0Format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if(!strcmp(ptr, "nv21"))
        {
            pThiz->mConfigPara.mVipp0Format = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else
        {
            aloge("fatal error! conf file pic_format[%s] is unsupported", ptr);
            pThiz->mConfigPara.mVipp0Format = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        pThiz->mConfigPara.mVipp0CaptureWidth = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP0_CAPTURE_WIDTH, 0);
        pThiz->mConfigPara.mVipp0CaptureHeight = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP0_CAPTURE_HEIGHT, 0);
        pThiz->mConfigPara.mVipp0Framerate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP0_FRAMERATE, 0);
        pThiz->mConfigPara.mVipp1 = HVIDEO(GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP1, 0), 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP1_FORMAT, NULL);
        if(!strcmp(ptr, "lbc2.5"))
        {
            pThiz->mConfigPara.mVipp1Format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else if(!strcmp(ptr, "lbc2.0"))
        {
            pThiz->mConfigPara.mVipp1Format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
        }
        else if(!strcmp(ptr, "lbc1.5"))
        {
            pThiz->mConfigPara.mVipp1Format = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
        }
        else if(!strcmp(ptr, "nv12"))
        {
            pThiz->mConfigPara.mVipp1Format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if(!strcmp(ptr, "nv21"))
        {
            pThiz->mConfigPara.mVipp1Format = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else
        {
            aloge("fatal error! conf file pic_format[%s] is unsupported", ptr);
            pThiz->mConfigPara.mVipp1Format = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        pThiz->mConfigPara.mVipp1CaptureWidth = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP1_CAPTURE_WIDTH, 0);
        pThiz->mConfigPara.mVipp1CaptureHeight = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP1_CAPTURE_HEIGHT, 0);
        pThiz->mConfigPara.mVipp1Framerate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIPP1_FRAMERATE, 0);
        //params of every video encode line
        pThiz->mConfigPara.mVideoAVipp = HVIDEO(GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_VIPP, 0), 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_FILE, NULL);
        strcpy(pThiz->mConfigPara.mVideoAFile, ptr);
        pThiz->mConfigPara.mVideoAFileCnt = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_FILE_CNT, 0);
        pThiz->mConfigPara.mVideoAFramerate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_FRAMERATE, 0);
        pThiz->mConfigPara.mVideoABitrate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_BITRATE, 0);
        pThiz->mConfigPara.mVideoAWidth = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_WIDTH, 0);
        pThiz->mConfigPara.mVideoAHeight = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_HEIGHT, 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_ENCODER, NULL);
        if(!strcmp(ptr, "H.264"))
        {
            pThiz->mConfigPara.mVideoAEncoderType = PT_H264;
        }
        else if(!strcmp(ptr, "H.265"))
        {
            pThiz->mConfigPara.mVideoAEncoderType = PT_H265;
        }
        else
        {
            aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
            pThiz->mConfigPara.mVideoAEncoderType = PT_H264;
        }
        pThiz->mConfigPara.mVideoARcMode = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_RC_MODE, 0);
        pThiz->mConfigPara.mVideoADuration = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_DURATION, 0);
        pThiz->mConfigPara.mVideoATimelapse = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOA_TIMELAPSE, 0);
        
        pThiz->mConfigPara.mVideoBVipp = HVIDEO(GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_VIPP, 0), 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_FILE, NULL);
        strcpy(pThiz->mConfigPara.mVideoBFile, ptr);
        pThiz->mConfigPara.mVideoBFileCnt = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_FILE_CNT, 0);
        pThiz->mConfigPara.mVideoBFramerate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_FRAMERATE, 0);
        pThiz->mConfigPara.mVideoBBitrate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_BITRATE, 0);
        pThiz->mConfigPara.mVideoBWidth = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_WIDTH, 0);
        pThiz->mConfigPara.mVideoBHeight = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_HEIGHT, 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_ENCODER, NULL);
        if(!strcmp(ptr, "H.264"))
        {
            pThiz->mConfigPara.mVideoBEncoderType = PT_H264;
        }
        else if(!strcmp(ptr, "H.265"))
        {
            pThiz->mConfigPara.mVideoBEncoderType = PT_H265;
        }
        else
        {
            aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
            pThiz->mConfigPara.mVideoBEncoderType = PT_H264;
        }
        pThiz->mConfigPara.mVideoBRcMode = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_RC_MODE, 0);
        pThiz->mConfigPara.mVideoBDuration = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_DURATION, 0);
        pThiz->mConfigPara.mVideoBTimelapse = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOB_TIMELAPSE, 0);

        pThiz->mConfigPara.mVideoCVipp = HVIDEO(GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_VIPP, 0), 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_FILE, NULL);
        strcpy(pThiz->mConfigPara.mVideoCFile, ptr);
        pThiz->mConfigPara.mVideoCFileCnt = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_FILE_CNT, 0);
        pThiz->mConfigPara.mVideoCFramerate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_FRAMERATE, 0);
        pThiz->mConfigPara.mVideoCBitrate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_BITRATE, 0);
        pThiz->mConfigPara.mVideoCWidth = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_WIDTH, 0);
        pThiz->mConfigPara.mVideoCHeight = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_HEIGHT, 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_ENCODER, NULL);
        if(!strcmp(ptr, "H.264"))
        {
            pThiz->mConfigPara.mVideoCEncoderType = PT_H264;
        }
        else if(!strcmp(ptr, "H.265"))
        {
            pThiz->mConfigPara.mVideoCEncoderType = PT_H265;
        }
        else
        {
            aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
            pThiz->mConfigPara.mVideoCEncoderType = PT_H264;
        }
        pThiz->mConfigPara.mVideoCRcMode = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_RC_MODE, 0);
        pThiz->mConfigPara.mVideoCDuration = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_DURATION, 0);
        pThiz->mConfigPara.mVideoCTimelapse = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOC_TIMELAPSE, 0);

        pThiz->mConfigPara.mVideoDVipp = HVIDEO(GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_VIPP, 0), 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_FILE, NULL);
        strcpy(pThiz->mConfigPara.mVideoDFile, ptr);
        pThiz->mConfigPara.mVideoDFileCnt = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_FILE_CNT, 0);
        pThiz->mConfigPara.mVideoDFramerate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_FRAMERATE, 0);
        pThiz->mConfigPara.mVideoDBitrate = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_BITRATE, 0);
        pThiz->mConfigPara.mVideoDWidth = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_WIDTH, 0);
        pThiz->mConfigPara.mVideoDHeight = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_HEIGHT, 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_ENCODER, NULL);
        if(!strcmp(ptr, "H.264"))
        {
            pThiz->mConfigPara.mVideoDEncoderType = PT_H264;
        }
        else if(!strcmp(ptr, "H.265"))
        {
            pThiz->mConfigPara.mVideoDEncoderType = PT_H265;
        }
        else
        {
            aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
            pThiz->mConfigPara.mVideoDEncoderType = PT_H264;
        }
        pThiz->mConfigPara.mVideoDRcMode = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_RC_MODE, 0);
        pThiz->mConfigPara.mVideoDDuration = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_DURATION, 0);
        pThiz->mConfigPara.mVideoDTimelapse = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOD_TIMELAPSE, 0);
        
        pThiz->mConfigPara.mVideoEVipp = HVIDEO(GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOE_VIPP, 0), 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOE_FILE_JPEG, NULL);
        strcpy(pThiz->mConfigPara.mVideoEFileJpeg, ptr);
        pThiz->mConfigPara.mVideoEFileJpegCnt = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOE_FILE_JPEG_CNT, 0);
        pThiz->mConfigPara.mVideoEWidth = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOE_WIDTH, 0);
        pThiz->mConfigPara.mVideoEHeight = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOE_HEIGHT, 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOE_ENCODER, NULL);
        if(!strcmp(ptr, "JPEG"))
        {
            pThiz->mConfigPara.mVideoEEncoderType = PT_JPEG;
        }
        else
        {
            aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
            pThiz->mConfigPara.mVideoEEncoderType = PT_JPEG;
        }
        pThiz->mConfigPara.mVideoEPhotoInterval = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_VIDEOE_PHOTO_INTERVAL, 0);
        pThiz->mConfigPara.mTestDuration = GetConfParaInt(&stConf, SAMPLE_MULTI_VI2VENC2MUXER_TEST_DURATION, 0);
        destroyConfParser(&stConf);
    }
    alogd("vipp0: id[%d],fmt[0x%x],size[%dx%d],frameRate[%d]", pThiz->mConfigPara.mVipp0, pThiz->mConfigPara.mVipp0Format,
        pThiz->mConfigPara.mVipp0CaptureWidth, pThiz->mConfigPara.mVipp0CaptureHeight, pThiz->mConfigPara.mVipp0Framerate);
    alogd("vipp1: id[%d],fmt[0x%x],size[%dx%d],frameRate[%d]", pThiz->mConfigPara.mVipp1, pThiz->mConfigPara.mVipp1Format,
        pThiz->mConfigPara.mVipp1CaptureWidth, pThiz->mConfigPara.mVipp1CaptureHeight, pThiz->mConfigPara.mVipp1Framerate);
    alogd("videoA: connect vipp[%d],file[%s],fileCnt[%d],frameRate[%d],bitRate[%d],size[%dx%d],encType[%d],rcMode[%d],duration[%d]s,timelapse[%d]us", 
        pThiz->mConfigPara.mVideoAVipp, pThiz->mConfigPara.mVideoAFile, pThiz->mConfigPara.mVideoAFileCnt, pThiz->mConfigPara.mVideoAFramerate,
        pThiz->mConfigPara.mVideoABitrate, pThiz->mConfigPara.mVideoAWidth, pThiz->mConfigPara.mVideoAHeight,
        pThiz->mConfigPara.mVideoAEncoderType, pThiz->mConfigPara.mVideoARcMode, pThiz->mConfigPara.mVideoADuration, 
        pThiz->mConfigPara.mVideoATimelapse);
    alogd("videoB: connect vipp[%d],file[%s],fileCnt[%d],frameRate[%d],bitRate[%d],size[%dx%d],encType[%d],rcMode[%d],duration[%d]s,timelapse[%d]us", 
        pThiz->mConfigPara.mVideoBVipp, pThiz->mConfigPara.mVideoBFile, pThiz->mConfigPara.mVideoBFileCnt, pThiz->mConfigPara.mVideoBFramerate,
        pThiz->mConfigPara.mVideoBBitrate, pThiz->mConfigPara.mVideoBWidth, pThiz->mConfigPara.mVideoBHeight,
        pThiz->mConfigPara.mVideoBEncoderType, pThiz->mConfigPara.mVideoBRcMode, pThiz->mConfigPara.mVideoBDuration, 
        pThiz->mConfigPara.mVideoBTimelapse);
    alogd("videoC: connect vipp[%d],file[%s],fileCnt[%d],frameRate[%d],bitRate[%d],size[%dx%d],encType[%d],rcMode[%d],duration[%d]s,timelapse[%d]us", 
        pThiz->mConfigPara.mVideoCVipp, pThiz->mConfigPara.mVideoCFile, pThiz->mConfigPara.mVideoCFileCnt, pThiz->mConfigPara.mVideoCFramerate,
        pThiz->mConfigPara.mVideoCBitrate, pThiz->mConfigPara.mVideoCWidth, pThiz->mConfigPara.mVideoCHeight,
        pThiz->mConfigPara.mVideoCEncoderType, pThiz->mConfigPara.mVideoCRcMode, pThiz->mConfigPara.mVideoCDuration, 
        pThiz->mConfigPara.mVideoCTimelapse);
    alogd("videoD: connect vipp[%d],file[%s],fileCnt[%d],frameRate[%d],bitRate[%d],size[%dx%d],encType[%d],rcMode[%d],duration[%d]s,timelapse[%d]us", 
        pThiz->mConfigPara.mVideoDVipp, pThiz->mConfigPara.mVideoDFile, pThiz->mConfigPara.mVideoDFileCnt, pThiz->mConfigPara.mVideoDFramerate,
        pThiz->mConfigPara.mVideoDBitrate, pThiz->mConfigPara.mVideoDWidth, pThiz->mConfigPara.mVideoDHeight,
        pThiz->mConfigPara.mVideoDEncoderType, pThiz->mConfigPara.mVideoDRcMode, pThiz->mConfigPara.mVideoDDuration, 
        pThiz->mConfigPara.mVideoDTimelapse);
    alogd("jpegE: connect vipp[%d],fileJpeg[%s],jpegCnt[%d],size[%dx%d],encType[%d],interval[%d]s", 
        pThiz->mConfigPara.mVideoEVipp, pThiz->mConfigPara.mVideoEFileJpeg, pThiz->mConfigPara.mVideoEFileJpegCnt,
        pThiz->mConfigPara.mVideoEWidth, pThiz->mConfigPara.mVideoEHeight,
        pThiz->mConfigPara.mVideoEEncoderType, pThiz->mConfigPara.mVideoEPhotoInterval);
    alogd("testDuration:[%d]s", pThiz->mConfigPara.mTestDuration);
    return SUCCESS;
}

SampleMultiVi2venc2muxerContext *constructSampleMultiVi2venc2muxerContext()
{
    SampleMultiVi2venc2muxerContext *pThiz = (SampleMultiVi2venc2muxerContext*)malloc(sizeof(*pThiz));
    if(pThiz != NULL)
    {
        memset(pThiz, 0, sizeof(*pThiz));
        int ret = cdx_sem_init(&pThiz->mSemExit, 0);
        if(ret != 0)
        {
            aloge("fatal error! cdx sem init fail[%d]", ret);
        }
    }
    else
    {
        aloge("fatal error! malloc fail!");
    }
    return pThiz;
}
void destructSampleMultiVi2venc2muxerContext(SampleMultiVi2venc2muxerContext *pThiz)
{
    if(pThiz != NULL)
    {
        cdx_sem_deinit(&pThiz->mSemExit);
        free(pThiz);
    }
}

static VippConfig* findVippConfig(SampleMultiVi2venc2muxerContext *pContext, int nVipp)
{
    int nFindFlag = 0;
    int nDst = 0;
    int i;
    for(i=0; i<MAX_VIPP_DEV_NUM; i++)
    {
        if(pContext->mVippConfigs[i].mbValid)
        {
            if(pContext->mVippConfigs[i].mViDev == nVipp)
            {
                nFindFlag++;
                nDst = i;
            }
        }
    }
    if(nFindFlag > 1)
    {
        aloge("fatal error! [%d] vippConfigs are matched! check code!", nFindFlag);
        return NULL;
    }
    else if(1 == nFindFlag)
    {
        return &pContext->mVippConfigs[nDst];
    }
    else
    {
        aloge("fatal error! no vippConfig is matched to vipp[%d]!", nVipp);
        return NULL;
    }
}

int initVippConfigs(SampleMultiVi2venc2muxerContext *pContext)
{
    if(pContext->mConfigPara.mVipp0 >= 0)
    {
        memset(&pContext->mVippConfigs[0], 0, sizeof(VippConfig));
        pContext->mVippConfigs[0].mbValid = true;
        pContext->mVippConfigs[0].mIspDev = 0;
        pContext->mVippConfigs[0].mViDev = pContext->mConfigPara.mVipp0;
        pContext->mVippConfigs[0].mPixelFormat = pContext->mConfigPara.mVipp0Format;
        pContext->mVippConfigs[0].mCaptureWidth = pContext->mConfigPara.mVipp0CaptureWidth;
        pContext->mVippConfigs[0].mCaptureHeight = pContext->mConfigPara.mVipp0CaptureHeight;
        pContext->mVippConfigs[0].mFramerate = pContext->mConfigPara.mVipp0Framerate;
    }
    if(pContext->mConfigPara.mVipp1 >= 0)
    {
        memset(&pContext->mVippConfigs[1], 0, sizeof(VippConfig));
        pContext->mVippConfigs[1].mbValid = true;
        pContext->mVippConfigs[1].mIspDev = 0;
        pContext->mVippConfigs[1].mViDev = pContext->mConfigPara.mVipp1;
        pContext->mVippConfigs[1].mPixelFormat = pContext->mConfigPara.mVipp1Format;
        pContext->mVippConfigs[1].mCaptureWidth = pContext->mConfigPara.mVipp1CaptureWidth;
        pContext->mVippConfigs[1].mCaptureHeight = pContext->mConfigPara.mVipp1CaptureHeight;
        pContext->mVippConfigs[1].mFramerate = pContext->mConfigPara.mVipp1Framerate;
    }
    return 0;    
}

static MEDIA_FILE_FORMAT_E getFileFormatByName(char *pFilePath)
{
    MEDIA_FILE_FORMAT_E eFileFormat = MEDIA_FILE_FORMAT_MP4;
    char *pFileName;
    char *pFileNameExtend;
    char *ptr = strrchr(pFilePath, '/');
    if(ptr != NULL)
    {
        pFileName = ptr+1;
    }
    else
    {
        pFileName = pFilePath;
    }
    ptr = strrchr(pFileName, '.');
    if(ptr != NULL)
    {
        pFileNameExtend = ptr + 1;
    }
    else
    {
        pFileNameExtend = NULL;
    }
    if(pFileNameExtend)
    {
        if(!strcmp(pFileNameExtend, "mp4"))
        {
            eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
        else if(!strcmp(pFileNameExtend, "ts"))
        {
            eFileFormat = MEDIA_FILE_FORMAT_TS;
        }
        else
        {
            alogw("Be careful! unknown file format:%d, default to mp4", eFileFormat);
            eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
    }
    else
    {
        alogw("Be careful! extend name is not exist, default to mp4");
        eFileFormat = MEDIA_FILE_FORMAT_MP4;
    }
    return eFileFormat;
}

static int initRecorders(SampleMultiVi2venc2muxerContext *pContext)
{
    memset(&pContext->mRecorders[0], 0, sizeof(Vi2venc2muxerContext));
    if(pContext->mConfigPara.mVideoAVipp >= 0)
    {
        pContext->mRecorders[0].mbValid = true;
        strcpy(pContext->mRecorders[0].mName, "VideoA");
        strcpy(pContext->mRecorders[0].mVideoFile, pContext->mConfigPara.mVideoAFile);
        pContext->mRecorders[0].mFileFormat         = getFileFormatByName(pContext->mRecorders[0].mVideoFile);
        pContext->mRecorders[0].mVideoFileCnt       = pContext->mConfigPara.mVideoAFileCnt;
        pContext->mRecorders[0].mVideoFramerate     = pContext->mConfigPara.mVideoAFramerate;
        pContext->mRecorders[0].mVideoBitrate       = pContext->mConfigPara.mVideoABitrate;
        pContext->mRecorders[0].mVideoWidth         = pContext->mConfigPara.mVideoAWidth;
        pContext->mRecorders[0].mVideoHeight        = pContext->mConfigPara.mVideoAHeight;
        pContext->mRecorders[0].mVideoEncoderType   = pContext->mConfigPara.mVideoAEncoderType;
        pContext->mRecorders[0].mVideoRcMode        = pContext->mConfigPara.mVideoARcMode;
        pContext->mRecorders[0].mVideoDuration      = pContext->mConfigPara.mVideoADuration;
        pContext->mRecorders[0].mVideoTimelapse     = pContext->mConfigPara.mVideoATimelapse;
        pContext->mRecorders[0].mpConnectVipp       = findVippConfig(pContext, pContext->mConfigPara.mVideoAVipp);
        pthread_mutex_init(&pContext->mRecorders[0].mFilePathListLock, NULL);
        INIT_LIST_HEAD(&pContext->mRecorders[0].mFilePathList);
    }
    memset(&pContext->mRecorders[1], 0, sizeof(Vi2venc2muxerContext));
    if(pContext->mConfigPara.mVideoBVipp >= 0)
    {
        pContext->mRecorders[1].mbValid = true;
        strcpy(pContext->mRecorders[1].mName, "VideoB");
        strcpy(pContext->mRecorders[1].mVideoFile, pContext->mConfigPara.mVideoBFile);
        pContext->mRecorders[1].mFileFormat         = getFileFormatByName(pContext->mRecorders[1].mVideoFile);
        pContext->mRecorders[1].mVideoFileCnt       = pContext->mConfigPara.mVideoBFileCnt;
        pContext->mRecorders[1].mVideoFramerate     = pContext->mConfigPara.mVideoBFramerate;
        pContext->mRecorders[1].mVideoBitrate       = pContext->mConfigPara.mVideoBBitrate;
        pContext->mRecorders[1].mVideoWidth         = pContext->mConfigPara.mVideoBWidth;
        pContext->mRecorders[1].mVideoHeight        = pContext->mConfigPara.mVideoBHeight;
        pContext->mRecorders[1].mVideoEncoderType   = pContext->mConfigPara.mVideoBEncoderType;
        pContext->mRecorders[1].mVideoRcMode        = pContext->mConfigPara.mVideoBRcMode;
        pContext->mRecorders[1].mVideoDuration      = pContext->mConfigPara.mVideoBDuration;
        pContext->mRecorders[1].mVideoTimelapse     = pContext->mConfigPara.mVideoBTimelapse;
        pContext->mRecorders[1].mpConnectVipp       = findVippConfig(pContext, pContext->mConfigPara.mVideoBVipp);
        pthread_mutex_init(&pContext->mRecorders[1].mFilePathListLock, NULL);
        INIT_LIST_HEAD(&pContext->mRecorders[1].mFilePathList);
    }
    memset(&pContext->mRecorders[2], 0, sizeof(Vi2venc2muxerContext));
    if(pContext->mConfigPara.mVideoCVipp >= 0)
    {
        pContext->mRecorders[2].mbValid = true;
        strcpy(pContext->mRecorders[2].mName, "VideoC");
        strcpy(pContext->mRecorders[2].mVideoFile, pContext->mConfigPara.mVideoCFile);
        pContext->mRecorders[2].mFileFormat         = getFileFormatByName(pContext->mRecorders[2].mVideoFile);
        pContext->mRecorders[2].mVideoFileCnt       = pContext->mConfigPara.mVideoCFileCnt;
        pContext->mRecorders[2].mVideoFramerate     = pContext->mConfigPara.mVideoCFramerate;
        pContext->mRecorders[2].mVideoBitrate       = pContext->mConfigPara.mVideoCBitrate;
        pContext->mRecorders[2].mVideoWidth         = pContext->mConfigPara.mVideoCWidth;
        pContext->mRecorders[2].mVideoHeight        = pContext->mConfigPara.mVideoCHeight;
        pContext->mRecorders[2].mVideoEncoderType   = pContext->mConfigPara.mVideoCEncoderType;
        pContext->mRecorders[2].mVideoRcMode        = pContext->mConfigPara.mVideoCRcMode;
        pContext->mRecorders[2].mVideoDuration      = pContext->mConfigPara.mVideoCDuration;
        pContext->mRecorders[2].mVideoTimelapse     = pContext->mConfigPara.mVideoCTimelapse;
        pContext->mRecorders[2].mpConnectVipp       = findVippConfig(pContext, pContext->mConfigPara.mVideoCVipp);
        pthread_mutex_init(&pContext->mRecorders[2].mFilePathListLock, NULL);
        INIT_LIST_HEAD(&pContext->mRecorders[2].mFilePathList);
    }
    memset(&pContext->mRecorders[3], 0, sizeof(Vi2venc2muxerContext));
    if(pContext->mConfigPara.mVideoDVipp >= 0)
    {
        pContext->mRecorders[3].mbValid = true;
        strcpy(pContext->mRecorders[3].mName, "VideoD");
        strcpy(pContext->mRecorders[3].mVideoFile, pContext->mConfigPara.mVideoDFile);
        pContext->mRecorders[3].mFileFormat         = getFileFormatByName(pContext->mRecorders[3].mVideoFile);
        pContext->mRecorders[3].mVideoFileCnt       = pContext->mConfigPara.mVideoDFileCnt;
        pContext->mRecorders[3].mVideoFramerate     = pContext->mConfigPara.mVideoDFramerate;
        pContext->mRecorders[3].mVideoBitrate       = pContext->mConfigPara.mVideoDBitrate;
        pContext->mRecorders[3].mVideoWidth         = pContext->mConfigPara.mVideoDWidth;
        pContext->mRecorders[3].mVideoHeight        = pContext->mConfigPara.mVideoDHeight;
        pContext->mRecorders[3].mVideoEncoderType   = pContext->mConfigPara.mVideoDEncoderType;
        pContext->mRecorders[3].mVideoRcMode        = pContext->mConfigPara.mVideoDRcMode;
        pContext->mRecorders[3].mVideoDuration      = pContext->mConfigPara.mVideoDDuration;
        pContext->mRecorders[3].mVideoTimelapse     = pContext->mConfigPara.mVideoDTimelapse;
        pContext->mRecorders[3].mpConnectVipp       = findVippConfig(pContext, pContext->mConfigPara.mVideoDVipp);
        pthread_mutex_init(&pContext->mRecorders[3].mFilePathListLock, NULL);
        INIT_LIST_HEAD(&pContext->mRecorders[3].mFilePathList);
    }
    memset(&pContext->mTakeJpegCtx, 0, sizeof(TakeJpegContext));
    if(pContext->mConfigPara.mVideoEVipp >= 0)
    {
        pContext->mTakeJpegCtx.mbValid = true;
        strcpy(pContext->mTakeJpegCtx.mName, "VideoEJpeg");
        strcpy(pContext->mTakeJpegCtx.mJpegFile, pContext->mConfigPara.mVideoEFileJpeg);
        pContext->mTakeJpegCtx.mJpegFileCnt         = pContext->mConfigPara.mVideoEFileJpegCnt;
        pContext->mTakeJpegCtx.mJpegWidth           = pContext->mConfigPara.mVideoEWidth;
        pContext->mTakeJpegCtx.mJpegHeight          = pContext->mConfigPara.mVideoEHeight;
        pContext->mTakeJpegCtx.mVideoEncoderType    = pContext->mConfigPara.mVideoEEncoderType;
        pContext->mTakeJpegCtx.mPhotoInterval       = pContext->mConfigPara.mVideoEPhotoInterval;
        pContext->mTakeJpegCtx.mpConnectVipp        = findVippConfig(pContext, pContext->mConfigPara.mVideoEVipp);
        pthread_mutex_init(&pContext->mTakeJpegCtx.mFilePathListLock, NULL);
        INIT_LIST_HEAD(&pContext->mTakeJpegCtx.mFilePathList);
    }
    return 0;
}

static int destroyRecorders(SampleMultiVi2venc2muxerContext *pContext)
{
    int i;
    FilePathNode *pEntry, *pTmp;
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pContext->mRecorders[i].mbValid)
        {
            pthread_mutex_lock(&pContext->mRecorders[i].mFilePathListLock);
            list_for_each_entry_safe(pEntry, pTmp, &pContext->mRecorders[i].mFilePathList, mList)
            {
                list_del(&pEntry->mList);
                free(pEntry);
            }
            pthread_mutex_unlock(&pContext->mRecorders[i].mFilePathListLock);
            pthread_mutex_destroy(&pContext->mRecorders[i].mFilePathListLock);
        }
    }
    if(pContext->mTakeJpegCtx.mbValid)
    {
        pthread_mutex_lock(&pContext->mTakeJpegCtx.mFilePathListLock);
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mTakeJpegCtx.mFilePathList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
        pthread_mutex_unlock(&pContext->mTakeJpegCtx.mFilePathListLock);
        pthread_mutex_destroy(&pContext->mTakeJpegCtx.mFilePathListLock);
    }
    return 0;
}

static int createViChn(Vi2venc2muxerContext *pRecCtx)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = TRUE;
    //create vi channel
    pRecCtx->mViChn = 0;
    while(1)
    {
        ret = AW_MPI_VI_CreateVirChn(pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn, &stVirChnAttr);
        if(SUCCESS == ret)
        {
            break;
        }
        else if(ERR_VI_EXIST == ret)
        {
            pRecCtx->mViChn++;
            continue;
        }
        else
        {
            aloge("fatal error! createVirChn[%d] fail[0x%x]!", pRecCtx->mViChn, ret);
            result = -1;
            break;
        }
    }
    return result;
}

static int startVipp(VippConfig *pVipp, bool bForceRef, SampleMultiVi2venc2muxerContext *pContext)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_VI_CreateVipp(pVipp->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp[%d] failed", pVipp->mViDev);
        result = -1;
        goto _err0;
    }

    memset(&pVipp->mViAttr, 0, sizeof(VI_ATTR_S));
    pVipp->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pVipp->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pVipp->mViAttr.format.width = pVipp->mCaptureWidth;
    pVipp->mViAttr.format.height = pVipp->mCaptureHeight;
    pVipp->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pVipp->mPixelFormat);
    pVipp->mViAttr.format.field = V4L2_FIELD_NONE;
    pVipp->mViAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    pVipp->mViAttr.nbufs = 3;//5;
    pVipp->mViAttr.nplanes = 2;
    pVipp->mViAttr.fps = pVipp->mFramerate;
    pVipp->mViAttr.use_current_win = bForceRef?0:1; // update configuration anyway, do not use current configuration
    pVipp->mViAttr.wdr_mode = 0;
    pVipp->mViAttr.capturemode = V4L2_MODE_VIDEO; /* V4L2_MODE_VIDEO; V4L2_MODE_IMAGE; V4L2_MODE_PREVIEW */
    pVipp->mViAttr.drop_frame_num = 0;
    pVipp->mViAttr.mbEncppEnable = TRUE;
    ret = AW_MPI_VI_SetVippAttr(pVipp->mViDev, &pVipp->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed[0x%x]", ret);
        result = -1;
        goto _err1;
    }
    ret = AW_MPI_ISP_Run(pVipp->mIspDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! isp run failed[0x%x]", ret);
        result = -1;
        goto _err1;
    }
    int i;
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pVipp == pContext->mRecorders[i].mpConnectVipp)
        {
            if (createViChn(&pContext->mRecorders[i]) != 0)
            {
                aloge("fatal error! create vi chn fail");
                result = -1;
                goto _err1;
            }
        }
    }
    
    ret = AW_MPI_VI_EnableVipp(pVipp->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail[0x%x]!", ret);
        result = -1;
        goto _err1;
    }
    pVipp->mbEnable = true;
    return result;

_err1:
    ret = AW_MPI_VI_DestroyVipp(pVipp->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI destroy vipp[%d] failed[0x%x]", pVipp->mViDev, ret);
    }
_err0:
    return result;
}

static int stopVipp(VippConfig *pVipp)
{
    int result = 0;
    ERRORTYPE ret;
    if(!pVipp->mbEnable)
    {
        return result;
    }
    ret = AW_MPI_VI_DisableVipp(pVipp->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI disable vipp[%d] failed[0x%x]", pVipp->mViDev, ret);
    }
    ret = AW_MPI_VI_DestroyVipp(pVipp->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI destroy vipp[%d] failed[0x%x]", pVipp->mViDev, ret);
    }
    ret = AW_MPI_ISP_Stop(pVipp->mIspDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! isp[%d] stop failed[0x%x]", pVipp->mIspDev, ret);
    }
    return result;
}

static int generateNextFileName(char *pNameBuf, Vi2venc2muxerContext *pRecCtx)
{
    pRecCtx->mFileIdCounter++;

    char tmpBuf[256] = {0};
    char *pFileName;
    char *ptr = strrchr(pRecCtx->mVideoFile, '/');
    if(ptr != NULL)
    {
        pFileName = ptr+1;
    }
    else
    {
        pFileName = pRecCtx->mVideoFile;
    }
    ptr = strrchr(pFileName, '.');
    if(ptr != NULL)
    {
        strncpy(tmpBuf, pRecCtx->mVideoFile, ptr - pRecCtx->mVideoFile);
        sprintf(pNameBuf, "%s_%d%s", tmpBuf, pRecCtx->mFileIdCounter, ptr);
    }
    else
    {
        sprintf(pNameBuf, "%s_%d", pRecCtx->mVideoFile, pRecCtx->mFileIdCounter);
    }
    return 0;
}

static int setNextFileToMuxer(Vi2venc2muxerContext *pRecCtx, char* path, int64_t fallocateLength, int muxerId)
{
    int result = 0;
    ERRORTYPE ret;
    if(path != NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT, 0666);
        if (fd < 0)
        {
            aloge("fatal error! fail to open %s", path);
            return -1;
        }

        if (pRecCtx->mMuxChnAttr.mMuxerId == muxerId)
        {
            ret = AW_MPI_MUX_SwitchFd(pRecCtx->mMuxGrp, pRecCtx->mMuxChn, fd, (int)fallocateLength);
            if(ret != SUCCESS)
            {
                aloge("fatal error! muxer[%d,%d] switch fd[%d] fail[0x%x]!", pRecCtx->mMuxGrp, pRecCtx->mMuxChn, fd, ret);
                result = -1;
            }
        }
        else
        {
            aloge("fatal error! muxerId is not match:[0x%x!=0x%x]", pRecCtx->mMuxChnAttr.mMuxerId, muxerId);
            result = -1;
        }
        
        close(fd);

        return result;
    }
    else
    {
        return -1;
    }
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    int ret;
    ERRORTYPE eRet = SUCCESS;
    Vi2venc2muxerContext *pRecCtx = (Vi2venc2muxerContext *)cookie;
    if(MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                break;
            }
            case MPP_EVENT_VENC_TIMEOUT:
            {
                uint64_t framePts = *(uint64_t*)pEventData;
                alogw("Be careful! detect encode timeout, pts[%lld]us", framePts);
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                alogw("Be careful! detect venc buffer full");
                break;
            }
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                if (pIsp2VeParam)
                {
                    sEncppSharpParam *pSharpParam = &pIsp2VeParam->mSharpParam;
                    ISP_DEV mIspDev = 0;
                    ret = AW_MPI_VI_GetIspDev(pRecCtx->mViChn, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pRecCtx->mViChn, ret);
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
                            mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren * pRecCtx->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren * pRecCtx->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren * pRecCtx->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren * pRecCtx->mEncppSharpAttenCoefPer / 100;
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
                aloge("fatal error! unknown event[%d]", event);
                break;
            }
        }
    }
    else if(MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                MultiVi2Venc2Muxer_MessageData stMsgData;

                alogd("MuxerId[%d] record file done.", *(int*)pEventData);
                stMsgData.pRecCtx = (Vi2venc2muxerContext*)cookie;
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(stMsgData);
                stCmdMsg.mpData = &stMsgData;

                putMessageWithData(&gpSampleMultiVi2venc2muxerContext->mMsgQueue, &stCmdMsg);
                /*
                int muxerId = *(int*)pEventData;
                alogd("[%s]file done, mux_id=0x%x", pRecCtx->mName, muxerId);
                pthread_mutex_lock(&pRecCtx->mFilePathListLock);
                int cnt = 0;
                struct list_head *pList;
                list_for_each(pList, &pRecCtx->mFilePathList)
                {
                    cnt++;
                }
                while(cnt > pRecCtx->mVideoFileCnt && pRecCtx->mVideoFileCnt > 0)
                {
                    FilePathNode *pNode = list_first_entry(&pRecCtx->mFilePathList, FilePathNode, mList);
                    list_del(&pNode->mList);
                    cnt--;
                    if ((ret = remove(pNode->strFilePath)) != 0)
                    {
                        aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                    }
                    else
                    {
                        alogd("delete file[%s] success", pNode->strFilePath);
                    }
                    free(pNode);
                }
                pthread_mutex_unlock(&pRecCtx->mFilePathListLock);
                */
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stMsgCmd;
                InitMessage(&stMsgCmd);
                MultiVi2Venc2Muxer_MessageData stMessageData;

                alogd("MuxerId[%d] need next fd.", *(int*)pEventData);
                stMessageData.pRecCtx = (Vi2venc2muxerContext*)cookie;
                stMsgCmd.command = Rec_NeedSetNextFd;
                stMsgCmd.para0 = *(int *)pEventData;
                stMsgCmd.mDataSize = sizeof(MultiVi2Venc2Muxer_MessageData);
                stMsgCmd.mpData = &stMessageData;

                putMessageWithData(&gpSampleMultiVi2venc2muxerContext->mMsgQueue, &stMsgCmd);
                /*
                int muxerId = *(int*)pEventData;
                char fileName[256] = {0};
                if(muxerId != pRecCtx->mMuxChnAttr.mMuxerId)
                {
                    aloge("fatal error! muxerId is not match:[0x%x!=0x%x]", muxerId, pRecCtx->mMuxChnAttr.mMuxerId);
                }
                generateNextFileName(fileName, pRecCtx);
                alogd("[%s]mux set next fd, filepath=%s", pRecCtx->mName, fileName);
                ret = setNextFileToMuxer(pRecCtx, fileName, 0, muxerId);                
                if(0 == ret)
                {
                    FilePathNode *pNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                    memset(pNode, 0, sizeof(*pNode));
                    strcpy(pNode->strFilePath, fileName);
                    pthread_mutex_lock(&pRecCtx->mFilePathListLock);
                    list_add_tail(&pNode->mList, &pRecCtx->mFilePathList);
                    pthread_mutex_unlock(&pRecCtx->mFilePathListLock);
                }
                */
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x]", event);
                break;
            }
        }
    }
    else
    {
         aloge("fatal error! unknown chn[%d,%d,%d]", pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return eRet;
}

static int configVencChnAttr(Vi2venc2muxerContext *pRecCtx)
{
    memset(&pRecCtx->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

    pRecCtx->mVencChnAttr.VeAttr.Type = pRecCtx->mVideoEncoderType;
    switch(pRecCtx->mVencChnAttr.VeAttr.Type)
    {
        case PT_H264:
        {
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = ALIGN((pRecCtx->mVideoWidth*pRecCtx->mVideoHeight*3/2)/3, 1024);
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.BufSize = ALIGN(pRecCtx->mVideoBitrate*4/8 + pRecCtx->mVencChnAttr.VeAttr.AttrH264e.mThreshSize, 1024);
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.Profile = 2;//0:base 1:main 2:high
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pRecCtx->mVideoWidth;
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pRecCtx->mVideoHeight;
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.FastEncFlag = FALSE;
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.IQpOffset = 0;
            pRecCtx->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_H265:
        {
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = ALIGN((pRecCtx->mVideoWidth*pRecCtx->mVideoHeight*3/2)/3, 1024);
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mBufSize = ALIGN(pRecCtx->mVideoBitrate*4/8 + pRecCtx->mVencChnAttr.VeAttr.AttrH264e.mThreshSize, 1024);
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mProfile = 0; //0:main 1:main10 2:sti11
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pRecCtx->mVideoWidth;
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pRecCtx->mVideoHeight;
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = FALSE;
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.IQpOffset = 0;
            pRecCtx->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_MJPEG:
        {
            pRecCtx->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
            pRecCtx->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pRecCtx->mVideoWidth;
            pRecCtx->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pRecCtx->mVideoHeight;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pRecCtx->mVencChnAttr.VeAttr.Type);
            break;
        }
    }
    pRecCtx->mVencChnAttr.VeAttr.SrcPicWidth = pRecCtx->mpConnectVipp->mCaptureWidth;
    pRecCtx->mVencChnAttr.VeAttr.SrcPicHeight = pRecCtx->mpConnectVipp->mCaptureHeight;
    pRecCtx->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pRecCtx->mVencChnAttr.VeAttr.PixelFormat = pRecCtx->mpConnectVipp->mPixelFormat;
    pRecCtx->mVencChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    pRecCtx->mVencChnAttr.VeAttr.Rotate = ROTATE_NONE;

    pRecCtx->mVencRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
    pRecCtx->mVencRcParam.sensor_type = VENC_ST_EN_WDR;

    pRecCtx->mVencChnAttr.EncppAttr.mbEncppEnable = TRUE;
    pRecCtx->mEncppSharpAttenCoefPer = 100 * pRecCtx->mpConnectVipp->mCaptureWidth / gpSampleMultiVi2venc2muxerContext->mConfigPara.mVipp0CaptureWidth;
    alogd("EncppSharpAttenCoefPer=%d", pRecCtx->mEncppSharpAttenCoefPer);

    switch(pRecCtx->mVencChnAttr.VeAttr.Type)
    {
        case PT_H264:
        {
            switch (pRecCtx->mVideoRcMode)
            {
                case 1:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pRecCtx->mVideoBitrate;
                    pRecCtx->mVencRcParam.ParamH264Vbr.mMaxQp = 51;
                    pRecCtx->mVencRcParam.ParamH264Vbr.mMinQp = 10;
                    pRecCtx->mVencRcParam.ParamH264Vbr.mMaxPqp = 50;
                    pRecCtx->mVencRcParam.ParamH264Vbr.mMinPqp = 10;
                    pRecCtx->mVencRcParam.ParamH264Vbr.mQpInit = 38;
                    pRecCtx->mVencRcParam.ParamH264Vbr.mMovingTh = 20;
                    pRecCtx->mVencRcParam.ParamH264Vbr.mQuality = 10;
                    break;
                }
                case 2:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 28;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 28;
                    break;
                }
                case 3:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = pRecCtx->mVideoBitrate;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Abr.mQuality = 8;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 20;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxQp = 51;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pRecCtx->mVideoBitrate;
                    pRecCtx->mVencRcParam.ParamH264Cbr.mMaxQp = 51;
                    pRecCtx->mVencRcParam.ParamH264Cbr.mMinQp = 10;
                    pRecCtx->mVencRcParam.ParamH264Cbr.mMaxPqp = 50;
                    pRecCtx->mVencRcParam.ParamH264Cbr.mMinPqp = 10;
                    pRecCtx->mVencRcParam.ParamH264Cbr.mQpInit = 38;
                    break;
                }
            }
            break;
        }
        case PT_H265:
        {
            switch (pRecCtx->mVideoRcMode)
            {
                case 1:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pRecCtx->mVideoBitrate;
                    pRecCtx->mVencRcParam.ParamH265Vbr.mMaxQp = 51;
                    pRecCtx->mVencRcParam.ParamH265Vbr.mMinQp = 10;
                    pRecCtx->mVencRcParam.ParamH265Vbr.mMaxPqp = 50;
                    pRecCtx->mVencRcParam.ParamH265Vbr.mMinPqp = 10;
                    pRecCtx->mVencRcParam.ParamH265Vbr.mQpInit = 38;
                    pRecCtx->mVencRcParam.ParamH265Vbr.mMovingTh = 20;
                    pRecCtx->mVencRcParam.ParamH265Vbr.mQuality = 10;
                    break;
                }
                case 2:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 28;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 28;
                    break;
                }
                case 3:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = pRecCtx->mVideoBitrate;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Abr.mQuality = 8;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Abr.mMinIQp = 20;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxQp = 51;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                    pRecCtx->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pRecCtx->mVideoBitrate;
                    pRecCtx->mVencRcParam.ParamH265Cbr.mMaxQp = 51;
                    pRecCtx->mVencRcParam.ParamH265Cbr.mMinQp = 1;
                    pRecCtx->mVencRcParam.ParamH265Cbr.mMaxPqp = 50;
                    pRecCtx->mVencRcParam.ParamH265Cbr.mMinPqp = 10;
                    pRecCtx->mVencRcParam.ParamH265Cbr.mQpInit = 38;
                    break;
                }
            }
            break;
        }
        case PT_MJPEG:
        {
            if(pRecCtx->mVideoRcMode != 0)
            {
                aloge("fatal error! mjpeg don't support rcMode[%d]!", pRecCtx->mVideoRcMode);
            }
            pRecCtx->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pRecCtx->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pRecCtx->mVideoBitrate;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pRecCtx->mVencChnAttr.VeAttr.Type);
            break;
        }
    }
    pRecCtx->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    alogd("venc ste Rcmode=%d", pRecCtx->mVencChnAttr.RcAttr.mRcMode);

    return 0;
}

static int createVencChn(Vi2venc2muxerContext *pRecCtx)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pRecCtx);
    pRecCtx->mVeChn = 0;
    while (pRecCtx->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pRecCtx->mVeChn, &pRecCtx->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pRecCtx->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            //alogd("venc channel[%d] is exist, find next!", pRecCtx->mVeChn);
            pRecCtx->mVeChn++;
        }
        else
        {
            aloge("fatal error! create venc channel[%d] ret[0x%x], find next!", pRecCtx->mVeChn, ret);
            pRecCtx->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pRecCtx->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return -1;
    }
    else
    {
        AW_MPI_VENC_SetRcParam(pRecCtx->mVeChn, &pRecCtx->mVencRcParam);

        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = pRecCtx->mpConnectVipp->mFramerate;
        stFrameRate.DstFrmRate = pRecCtx->mVideoFramerate;
        alogd("[%s]set srcFrameRate:%d, venc framerate:%d", pRecCtx->mName, stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        ret = AW_MPI_VENC_SetFrameRate(pRecCtx->mVeChn, &stFrameRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! venc set framerate fail[0x%x]!", ret);
        }
        alogd("set venc timelapse:[%d]us", pRecCtx->mVideoTimelapse);
        ret = AW_MPI_VENC_SetTimeLapse(pRecCtx->mVeChn, (int64_t)pRecCtx->mVideoTimelapse);
        if(ret != SUCCESS)
        {
            aloge("fatal error! venc set timeLapse[%d] fail[0x%x]!", pRecCtx->mVideoTimelapse, ret);
        }

        VeProcSet stVeProcSet;
        memset(&stVeProcSet, 0, sizeof(VeProcSet));
        stVeProcSet.bProcEnable = 1;
        stVeProcSet.nProcFreq = 30;
        AW_MPI_VENC_SetProcSet(pRecCtx->mVeChn, &stVeProcSet);
        
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pRecCtx;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pRecCtx->mVeChn, &cbInfo);

        return result;
    }
}

static ERRORTYPE configMuxGrpAttr(Vi2venc2muxerContext *pRecCtx)
{
    memset(&pRecCtx->mMuxGrpAttr, 0, sizeof(MUX_GRP_ATTR_S));
    pRecCtx->mMuxGrpAttr.mVideoAttrValidNum = 1;
    pRecCtx->mMuxGrpAttr.mVideoAttr[0].mHeight = pRecCtx->mVideoHeight;
    pRecCtx->mMuxGrpAttr.mVideoAttr[0].mWidth = pRecCtx->mVideoWidth;
    pRecCtx->mMuxGrpAttr.mVideoAttr[0].mVideoFrmRate = pRecCtx->mVideoFramerate*1000;
    pRecCtx->mMuxGrpAttr.mVideoAttr[0].mVideoEncodeType = pRecCtx->mVideoEncoderType;
    pRecCtx->mMuxGrpAttr.mVideoAttr[0].mVeChn = pRecCtx->mVeChn;
    pRecCtx->mMuxGrpAttr.mAudioEncodeType = PT_MAX;

    return SUCCESS;
}

static int createMuxGrp(Vi2venc2muxerContext *pRecCtx)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configMuxGrpAttr(pRecCtx);
    pRecCtx->mMuxGrp = 0;
    while (pRecCtx->mMuxGrp < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(pRecCtx->mMuxGrp, &pRecCtx->mMuxGrpAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] success!", pRecCtx->mMuxGrp);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            //alogd("mux group[%d] is exist, find next!", pRecCtx->mMuxGrp);
            pRecCtx->mMuxGrp++;
        }
        else
        {
            alogd("create mux group[%d] ret[0x%x], find next!", pRecCtx->mMuxGrp, ret);
            pRecCtx->mMuxGrp++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pRecCtx->mMuxGrp = MM_INVALID_CHN;
        aloge("fatal error! create mux group fail!");
        return -1;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pRecCtx;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pRecCtx->mMuxGrp, &cbInfo);
        return 0;
    }
}

static int createMuxChn(Vi2venc2muxerContext *pRecCtx)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;
    memset(&pRecCtx->mMuxChnAttr, 0, sizeof(pRecCtx->mMuxChnAttr));
    pRecCtx->mMuxChnAttr.mMuxerId = pRecCtx->mMuxGrp << 16 | 0;
    //pRecCtx->mMuxChnAttr.mMuxerGrpId = pRecCtx->mMuxGrp;
    pRecCtx->mMuxChnAttr.mMediaFileFormat = pRecCtx->mFileFormat;
    pRecCtx->mMuxChnAttr.mMaxFileDuration = pRecCtx->mVideoDuration *1000;
    pRecCtx->mMuxChnAttr.mMaxFileSizeBytes = 0;
    pRecCtx->mMuxChnAttr.mFallocateLen = 0;
    pRecCtx->mMuxChnAttr.mCallbackOutFlag = FALSE;
    pRecCtx->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pRecCtx->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    pRecCtx->mMuxChnAttr.bBufFromCacheFlag = FALSE;

    int nFd = open(pRecCtx->mVideoFile, O_RDWR | O_CREAT, 0666);
    if (nFd < 0)
    {
        aloge("fatal error! Failed to open %s", pRecCtx->mVideoFile);
        return -1;
    }

    pRecCtx->mMuxChn = 0;
    nSuccessFlag = FALSE;
    while (pRecCtx->mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pRecCtx->mMuxGrp, pRecCtx->mMuxChn, &pRecCtx->mMuxChnAttr, nFd);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pRecCtx->mMuxGrp, pRecCtx->mMuxChn, pRecCtx->mMuxChnAttr.mMuxerId);
            break;
        }
        else if(ERR_MUX_EXIST == ret)
        {
            pRecCtx->mMuxChn++;
        }
        else
        {
            aloge("fatal error! create mux chn fail[0x%x]!", ret);
            pRecCtx->mMuxChn++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        pRecCtx->mMuxChn = MM_INVALID_CHN;
        aloge("fatal error! create muxChannel in muxGroup[%d] fail!", pRecCtx->mMuxGrp);
        result = -1;
    }
    if(nFd >= 0)
    {
        close(nFd);
        nFd = -1;
    }
    return result;
}

static int prepareRecord(Vi2venc2muxerContext *pRecCtx)
{
    ERRORTYPE eRet = SUCCESS;
    int result = 0;

    alogd("recorder[%s] begin to prepare", pRecCtx->mName);
    /*if (createViChn(pRecCtx) != 0)
    {
        aloge("fatal error! create vi chn fail");
        result = -1;
        goto _err0;
    }*/
    if (createVencChn(pRecCtx) != 0)
    {
        aloge("[%s] create venc chn fail", pRecCtx->mName);
        result = -1;
        goto _err1;
    }
    if (createMuxGrp(pRecCtx) != 0)
    {
        aloge("[%s]create mux group fail", pRecCtx->mName);
        result = -1;
        goto _err2;
    }
    //set spspps
    if (pRecCtx->mVideoEncoderType == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        AW_MPI_VENC_GetH264SpsPpsInfo(pRecCtx->mVeChn, &H264SpsPpsInfo);
        AW_MPI_MUX_SetH264SpsPpsInfo(pRecCtx->mMuxGrp, pRecCtx->mVeChn, &H264SpsPpsInfo);
    }
    else if(pRecCtx->mVideoEncoderType == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        AW_MPI_VENC_GetH265SpsPpsInfo(pRecCtx->mVeChn, &H265SpsPpsInfo);
        AW_MPI_MUX_SetH265SpsPpsInfo(pRecCtx->mMuxGrp, pRecCtx->mVeChn, &H265SpsPpsInfo);
    }
    else
    {
        alogd("don't need set spspps for encodeType[%d]", pRecCtx->mVideoEncoderType);
    }
    if(createMuxChn(pRecCtx) != 0)
    {
        aloge("[%s]create mux channel fail", pRecCtx->mName);
        result = -1;
        goto _err3;
    }
    FilePathNode *pNode = (FilePathNode*)malloc(sizeof(FilePathNode));
    memset(pNode, 0, sizeof(*pNode));
    strcpy(pNode->strFilePath, pRecCtx->mVideoFile);
    pthread_mutex_lock(&pRecCtx->mFilePathListLock);
    list_add_tail(&pNode->mList, &pRecCtx->mFilePathList);
    pthread_mutex_unlock(&pRecCtx->mFilePathListLock);

    if ((pRecCtx->mpConnectVipp->mViDev >= 0 && pRecCtx->mViChn >= 0) && pRecCtx->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pRecCtx->mVeChn};
        eRet = AW_MPI_SYS_Bind(&ViChn, &VeChn);
        if(eRet!=SUCCESS)
        {
            aloge("fatal error! [%s] sys bind fail[0x%x]", pRecCtx->mName, eRet);
        }
    }
    if (pRecCtx->mVeChn >= 0 && pRecCtx->mMuxGrp >= 0)
    {
        MPP_CHN_S MuxGrp = {MOD_ID_MUX, 0, pRecCtx->mMuxGrp};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pRecCtx->mVeChn};
        eRet = AW_MPI_SYS_Bind(&VeChn, &MuxGrp);
        if(eRet!=SUCCESS)
        {
            aloge("fatal error! [%s] sys bind fail[0x%x]", pRecCtx->mName, eRet);
        }
    }
    return result;

_err3:
    eRet = AW_MPI_MUX_DestroyGrp(pRecCtx->mMuxGrp);
    if(eRet!=SUCCESS)
    {
        aloge("fatal error! [%s] destroy muxGroup[%d] fail[0x%x]", pRecCtx->mName, pRecCtx->mMuxGrp, eRet);
    }
_err2:
    eRet = AW_MPI_VENC_DestroyChn(pRecCtx->mVeChn);
    if(eRet!=SUCCESS)
    {
        aloge("fatal error! [%s] destroy vencChn[%d] fail[0x%x]", pRecCtx->mName, pRecCtx->mVeChn, eRet);
    }
    pRecCtx->mVeChn = MM_INVALID_CHN;
_err1:
    eRet = AW_MPI_VI_DestroyVirChn(pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn);
    if(eRet!=SUCCESS)
    {
        aloge("fatal error! [%s] destroy viChn[%d] fail[0x%x]", pRecCtx->mName, pRecCtx->mViChn, eRet);
    }
    pRecCtx->mViChn = MM_INVALID_CHN;
_err0:
    return result;
}

static int startRecord(Vi2venc2muxerContext *pRecCtx)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_VI_EnableVirChn(pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! viChn[%d,%d] enable error[0x%x]!", pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn, ret);
    }

    if (pRecCtx->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StartRecvPic(pRecCtx->mVeChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] start error[0x%x]!", pRecCtx->mVeChn, ret);
        }
    }

    if (pRecCtx->mMuxGrp >= 0)
    {
        ret = AW_MPI_MUX_StartGrp(pRecCtx->mMuxGrp);
        if (ret != SUCCESS)
        {
            aloge("fatal error! muxGroup[%d] start error[0x%x]!", pRecCtx->mMuxGrp, ret);
        }
    }
    return result;
}
static int stopRecord(Vi2venc2muxerContext *pRecCtx)
{
    ERRORTYPE ret = SUCCESS;
    if (pRecCtx->mViChn >= 0)
    {
        ret = AW_MPI_VI_DisableVirChn(pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! [%s] vipp[%d]chn[%d] disabled fail[0x%x]", pRecCtx->mName, pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn, ret);
        }
    }
    if (pRecCtx->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StopRecvPic(pRecCtx->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! [%s] veChn[%d] stop fail[0x%x]", pRecCtx->mName, pRecCtx->mVeChn, ret);
        }
    }
    if (pRecCtx->mMuxGrp >= 0)
    {
        ret = AW_MPI_MUX_StopGrp(pRecCtx->mMuxGrp);
        if(ret != SUCCESS)
        {
            aloge("fatal error! [%s] muxGrp[%d] stop fail[0x%x]", pRecCtx->mName, pRecCtx->mMuxGrp, ret);
        }
    }
    return 0;
}

static int releaseRecord(Vi2venc2muxerContext *pRecCtx)
{
    ERRORTYPE ret = SUCCESS;
    if (pRecCtx->mMuxGrp >= 0)
    {
        ret = AW_MPI_MUX_DestroyGrp(pRecCtx->mMuxGrp);
        if(ret != SUCCESS)
        {
            aloge("fatal error! [%s] muxGrp[%d] destroy fail[0x%x]", pRecCtx->mName, pRecCtx->mMuxGrp, ret);
        }
        pRecCtx->mMuxGrp = MM_INVALID_CHN;
    }
    if (pRecCtx->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_ResetChn(pRecCtx->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! [%s] veChn[%d] stop fail[0x%x]", pRecCtx->mName, pRecCtx->mVeChn, ret);
        }
        ret = AW_MPI_VENC_DestroyChn(pRecCtx->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! [%s] veChn[%d] destroy fail[0x%x]", pRecCtx->mName, pRecCtx->mVeChn, ret);
        }
        pRecCtx->mVeChn = MM_INVALID_CHN;
    }
    if (pRecCtx->mViChn >= 0)
    {
        ret = AW_MPI_VI_DestroyVirChn(pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! [%s] vipp[%d]Chn[%d] stop fail[0x%x]", pRecCtx->mName, pRecCtx->mpConnectVipp->mViDev, pRecCtx->mViChn, ret);
        }
        pRecCtx->mViChn = MM_INVALID_CHN;
    }
    return 0;
}

static ERRORTYPE MPPCallbackWrapper_Jpeg(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    TakeJpegContext *pJpegCtx = (TakeJpegContext *)cookie;
    ERRORTYPE ret = 0;
    if(MOD_ID_VENC == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                if(NULL != pVideoFrameInfo)
                {
                    ret = AW_MPI_VI_ReleaseFrame(pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, pVideoFrameInfo);
                    if(ret != SUCCESS)
                    {
                       aloge("fatal error! AW_MPI_VI ReleaseFrame vipp[%d]Chn[%d] failed[0x%x]", pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, ret);
                    }
                }
                break;
            }
            default:
            {
                aloge("fatal error! unknown jpeg encoder event[%d]", event);
                break;
            }
        }
    }

    return SUCCESS;
}

static int prepareJpeg(TakeJpegContext *pJpegCtx)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    bool bSuccessFlag = false;
    //create vi channel
    pJpegCtx->mViChn = 0;
    while(1)
    {
        ret = AW_MPI_VI_CreateVirChn(pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, NULL);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            break;
        }
        else if(ERR_VI_EXIST == ret)
        {
            pJpegCtx->mViChn++;
        }
        else
        {
            aloge("fatal error! createVirChn[%d] fail[0x%x]!", pJpegCtx->mViChn, ret);
            break;
        }
    }
    if(false == bSuccessFlag)
    {
        pJpegCtx->mViChn = MM_INVALID_CHN;
        result = -1;
        goto _err0;
    }
    
    //create venc channel
    memset(&pJpegCtx->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pJpegCtx->mVencChnAttr.VeAttr.Type = PT_JPEG;
    pJpegCtx->mVencChnAttr.VeAttr.AttrJpeg.MaxPicWidth = 0;
    pJpegCtx->mVencChnAttr.VeAttr.AttrJpeg.MaxPicHeight = 0;
    pJpegCtx->mVencChnAttr.VeAttr.AttrJpeg.BufSize = ALIGN(pJpegCtx->mJpegWidth*pJpegCtx->mJpegHeight*3/2, 1024);
    pJpegCtx->mVencChnAttr.VeAttr.AttrJpeg.bByFrame = TRUE;
    pJpegCtx->mVencChnAttr.VeAttr.AttrJpeg.PicWidth = pJpegCtx->mJpegWidth;
    pJpegCtx->mVencChnAttr.VeAttr.AttrJpeg.PicHeight = pJpegCtx->mJpegHeight;
    pJpegCtx->mVencChnAttr.VeAttr.AttrJpeg.bSupportDCF = FALSE;
    pJpegCtx->mVencChnAttr.VeAttr.MaxKeyInterval = 1;
    pJpegCtx->mVencChnAttr.VeAttr.SrcPicWidth = pJpegCtx->mpConnectVipp->mCaptureWidth;
    pJpegCtx->mVencChnAttr.VeAttr.SrcPicHeight = pJpegCtx->mpConnectVipp->mCaptureHeight;
    pJpegCtx->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pJpegCtx->mVencChnAttr.VeAttr.PixelFormat = pJpegCtx->mpConnectVipp->mPixelFormat;
    pJpegCtx->mVencChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    pJpegCtx->mVencChnAttr.VeAttr.Rotate = ROTATE_NONE;

    bSuccessFlag = false;
    pJpegCtx->mVeChn = 0;
    while (pJpegCtx->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pJpegCtx->mVeChn, &pJpegCtx->mVencChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("[%s]create venc channel[%d] success!", pJpegCtx->mName, pJpegCtx->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            pJpegCtx->mVeChn++;
        }
        else
        {
            aloge("fatal error! create venc channel[%d] ret[0x%x], find next!", pJpegCtx->mVeChn, ret);
            pJpegCtx->mVeChn++;
        }
    }
    if (bSuccessFlag == false)
    {
        pJpegCtx->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        result = -1;
        goto _err1;
    }
    VeProcSet stVeProcSet;
    memset(&stVeProcSet, 0, sizeof(VeProcSet));
    stVeProcSet.bProcEnable = 1;
    stVeProcSet.nProcFreq = 30;
    AW_MPI_VENC_SetProcSet(pJpegCtx->mVeChn, &stVeProcSet);

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pJpegCtx;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper_Jpeg;
    AW_MPI_VENC_RegisterCallback(pJpegCtx->mVeChn, &cbInfo);
    VENC_PARAM_JPEG_S mJpegParam;
    memset(&mJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
    mJpegParam.Qfactor = 100;
    AW_MPI_VENC_SetJpegParam(pJpegCtx->mVeChn, &mJpegParam);
    AW_MPI_VENC_ForbidDiscardingFrame(pJpegCtx->mVeChn, TRUE);

    return result;

_err1:
    ret = AW_MPI_VI_DestroyVirChn(pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Vi destroy virVhn fail[0x%x]!", ret);
    }
_err0:
    return result;
}

static ERRORTYPE startJpeg(TakeJpegContext *pJpegCtx)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_VI_EnableVirChn(pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn);
    if (ret != SUCCESS)
    {
        alogd("Vipp[%d]viChn[%d] enable fail[0x%x]!", pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, ret);
        return -1;
    }
    if (pJpegCtx->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StartRecvPic(pJpegCtx->mVeChn);
        if (ret != SUCCESS)
        {
            alogd("veChn[%d] start fail[0x%x]!", pJpegCtx->mVeChn, ret);
            return -1;
        }
    }
    return result;
}

static int generatePictureName(char *pNameBuf, TakeJpegContext *pJpegCtx)
{
    if(0 == pJpegCtx->mJpegCounter)
    {
        strcpy(pNameBuf, pJpegCtx->mJpegFile);
        pJpegCtx->mJpegCounter++;
        return 0;
    }

    char tmpBuf[256] = {0};
    char *pFileName;
    char *ptr = strrchr(pJpegCtx->mJpegFile, '/');
    if(ptr != NULL)
    {
        pFileName = ptr+1;
    }
    else
    {
        pFileName = pJpegCtx->mJpegFile;
    }
    ptr = strrchr(pFileName, '.');
    if(ptr != NULL)
    {
        strncpy(tmpBuf, pJpegCtx->mJpegFile, ptr - pJpegCtx->mJpegFile);
        sprintf(pNameBuf, "%s_%d%s", tmpBuf, pJpegCtx->mJpegCounter, ptr);
    }
    else
    {
        sprintf(pNameBuf, "%s_%d", pJpegCtx->mJpegFile, pJpegCtx->mJpegCounter);
    }
    pJpegCtx->mJpegCounter++;
    return 0;
}

int takePicture(TakeJpegContext *pJpegCtx)
{
    int result = 0;
    ERRORTYPE ret;
    //static int file_cnt = 0;
    VIDEO_FRAME_INFO_S stFrame;
    memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    if ((ret = AW_MPI_VI_GetFrame(pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, &stFrame, 5000)) != 0)
    {
        aloge("fatal error! vipp[%d]chn[%d] Get Frame failed[0x%x]!", pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, ret);
        return -1;
    }
    else
    {
        ret = AW_MPI_VENC_SendFrame(pJpegCtx->mVeChn, &stFrame, 0);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] send frame fail[0x%x]", pJpegCtx->mVeChn, ret);
        }
    }

    VENC_STREAM_S stVencStream;
    VENC_PACK_S stPack;
    memset(&stVencStream, 0, sizeof(VENC_STREAM_S));
    memset(&stPack, 0, sizeof(VENC_PACK_S));
    stVencStream.mpPack = &stPack;
    stVencStream.mPackCount = 1;
    if(SUCCESS == AW_MPI_VENC_GetStream(pJpegCtx->mVeChn, &stVencStream, 5000))
    {
        unsigned int thumboffset = 0;
        unsigned int thumblen = 0;
        VENC_JPEG_THUMB_BUFFER_S stJpegThumbBuf;
        memset(&stJpegThumbBuf, 0, sizeof(VENC_JPEG_THUMB_BUFFER_S));
        ret = AW_MPI_VENC_GetJpegThumbBuffer(pJpegCtx->mVeChn, &stJpegThumbBuf);
        if(ret != SUCCESS)
        {
            aloge("fatal error! AW_MPI_VENC_GetJpegThumbBuffer failed");
        }
        if(stJpegThumbBuf.ThumbLen > 0)
        {
            thumblen = stJpegThumbBuf.ThumbLen;
            //deduce thumb picture buffer in mOutStream
            if(NULL == stVencStream.mpPack[0].mpAddr0 || NULL == stVencStream.mpPack[0].mpAddr1 
                || 0 == stVencStream.mpPack[0].mLen0 || 0 == stVencStream.mpPack[0].mLen1)
            {
                aloge("fatal error! check code!");
            }
            if(stJpegThumbBuf.ThumbAddrVir >= stVencStream.mpPack[0].mpAddr0)
            {
                if(stJpegThumbBuf.ThumbAddrVir >= stVencStream.mpPack[0].mpAddr0 + stVencStream.mpPack[0].mLen0)
                {
                    aloge("fatal error! check code![%p][%p][%d]", stJpegThumbBuf.ThumbAddrVir, stVencStream.mpPack[0].mpAddr0, stVencStream.mpPack[0].mLen0);
                }
                thumboffset = stJpegThumbBuf.ThumbAddrVir - stVencStream.mpPack[0].mpAddr0;
            }
            else if(stJpegThumbBuf.ThumbAddrVir>= stVencStream.mpPack[0].mpAddr1)
            {
                if(stJpegThumbBuf.ThumbAddrVir >= stVencStream.mpPack[0].mpAddr1 + stVencStream.mpPack[0].mLen1)
                {
                    aloge("fatal error! check code![%p][%p][%d]", stJpegThumbBuf.ThumbAddrVir, stVencStream.mpPack[0].mpAddr1, stVencStream.mpPack[0].mLen1);
                }
                thumboffset = stVencStream.mpPack[0].mLen0 + (stJpegThumbBuf.ThumbAddrVir - stVencStream.mpPack[0].mpAddr1);
            }
            else
            {
                aloge("fatal error! check code![%p][%p][%d][%p][%d]", stJpegThumbBuf.ThumbAddrVir, 
                    stVencStream.mpPack[0].mpAddr0, stVencStream.mpPack[0].mLen0, 
                    stVencStream.mpPack[0].mpAddr1, stVencStream.mpPack[0].mLen1);
            }
        }
        else
        {
            alogd("jpeg has no thumb picture");
            thumboffset = 0;
            thumblen = 0;
        }
        PictureBuffer stPictureBuffer;
        memset(&stPictureBuffer, 0, sizeof(PictureBuffer));
        stPictureBuffer.mpData0 = stVencStream.mpPack[0].mpAddr0;
        stPictureBuffer.mpData1 = stVencStream.mpPack[0].mpAddr1;
        stPictureBuffer.mpData2 = stVencStream.mpPack[0].mpAddr2;
        stPictureBuffer.mLen0 = stVencStream.mpPack[0].mLen0;
        stPictureBuffer.mLen1 = stVencStream.mpPack[0].mLen1;
        stPictureBuffer.mLen2 = stVencStream.mpPack[0].mLen2;
        stPictureBuffer.mThumbOffset = thumboffset;
        stPictureBuffer.mThumbLen = thumblen;
        stPictureBuffer.mDataSize = stPictureBuffer.mLen0+stPictureBuffer.mLen1+stPictureBuffer.mLen2;
        char strNameBuf[MAX_FILE_PATH_SIZE] = {0};
        generatePictureName(strNameBuf, pJpegCtx);
        alogd("-----take pictrue file %s-----", strNameBuf);
        FILE* pFile = fopen(strNameBuf, "wb");
        if (pFile != NULL)
        {
            if(stPictureBuffer.mpData0!=NULL)
            {
                fwrite(stPictureBuffer.mpData0, 1, stPictureBuffer.mLen0, pFile);
            }
            if(stPictureBuffer.mpData1!=NULL)
            {
                fwrite(stPictureBuffer.mpData1, 1, stPictureBuffer.mLen1, pFile);
            }
            if(stPictureBuffer.mpData2!=NULL)
            {
                fwrite(stPictureBuffer.mpData2, 1, stPictureBuffer.mLen2, pFile);
            }
            fwrite(&stPictureBuffer.mThumbOffset, 1, sizeof(stPictureBuffer.mThumbOffset), pFile);
            fwrite(&stPictureBuffer.mThumbLen, 1, sizeof(stPictureBuffer.mThumbLen), pFile);
            fwrite(&stPictureBuffer.mDataSize, 1, sizeof(stPictureBuffer.mDataSize), pFile);
            fclose(pFile);

            pthread_mutex_lock(&pJpegCtx->mFilePathListLock);
            FilePathNode *pNode = (FilePathNode*)malloc(sizeof(FilePathNode));
            memset(pNode, 0, sizeof(*pNode));
            strcpy(pNode->strFilePath, strNameBuf);
            list_add_tail(&pNode->mList, &pJpegCtx->mFilePathList);
            int cnt = 0;
            struct list_head *pList;
            list_for_each(pList, &pJpegCtx->mFilePathList)
            {
                cnt++;
            }
            while(cnt > pJpegCtx->mJpegFileCnt && pJpegCtx->mJpegFileCnt > 0)
            {
                FilePathNode *pNode = list_first_entry(&pJpegCtx->mFilePathList, FilePathNode, mList);
                list_del(&pNode->mList);
                cnt--;
                if ((ret = remove(pNode->strFilePath)) != 0)
                {
                    aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                }
                else
                {
                    alogd("delete file[%s] success", pNode->strFilePath);
                }
                free(pNode);
            }
            pthread_mutex_unlock(&pJpegCtx->mFilePathListLock);
        }
        else
        {
            aloge("fatal error: cannot create out file");
        }
        ret = AW_MPI_VENC_ReleaseStream(pJpegCtx->mVeChn, &stVencStream);
        if(ret != SUCCESS)
        {
            aloge("fatal error! AW_MPI_VENC ReleaseStream failed[0x%x]", ret);
        }
    }
    else
    {
        aloge("fatal error! get picture fail");
        result = -1;
    }
    alogd("take picture end!");
    return result;
}

static int stopJpeg(TakeJpegContext *pJpegCtx)
{
    ERRORTYPE ret = SUCCESS;
    if (pJpegCtx->mViChn >= 0)
    {
        ret = AW_MPI_VI_DisableVirChn(pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d] disable virChn[%d] fail[0x%x].", pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, ret);
        }
    }
    if (pJpegCtx->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StopRecvPic(pJpegCtx->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! stop veChn[%d] fail[0x%x].", pJpegCtx->mVeChn, ret);
        }
    }
    return 0;
}

static int releaseJpeg(TakeJpegContext *pJpegCtx)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    if (pJpegCtx->mViChn >= 0)
    {
        ret = AW_MPI_VI_DestroyVirChn(pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d] destroy virChn[%d] fail[0x%x].", pJpegCtx->mpConnectVipp->mViDev, pJpegCtx->mViChn, ret);
        }
        pJpegCtx->mViChn = MM_INVALID_CHN;
    }

    if (pJpegCtx->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_DestroyChn(pJpegCtx->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! destroy veChn[%d] fail[0x%x].", pJpegCtx->mVeChn, ret);
        }
        pJpegCtx->mVeChn = MM_INVALID_CHN;
    }
    return result;
}

static void *TakePictureThread(void *pThreadData)
{
    int ret = 0;
    TakeJpegContext *pJpegCtx = (TakeJpegContext*)pThreadData;
    int nInterval = pJpegCtx->mPhotoInterval;
    if(nInterval <= 0)
    {
        nInterval = 30; //photo interval is default 30s.
    }
    int count = nInterval;
    while(1)
    {
        if(pJpegCtx->mbExitFlag)
        {
            alogd("detect take picture thread exit flag!");
            break;
        }
        if(count == nInterval)
        {
            ret = prepareJpeg(pJpegCtx);
            if (ret != 0)
            {
                aloge("prepare jpeg fail!");
                count = 0;
                continue;
            }
            ret = startJpeg(pJpegCtx);
            if(ret != 0)
            {
                aloge("fatal error! start jpeg fail");
            }
            ret = takePicture(pJpegCtx);
            if(ret != 0)
            {
                aloge("fatal error! take picture fail");
            }
            ret = stopJpeg(pJpegCtx);
            if(ret != 0)
            {
                aloge("fatal error! stop jpeg fail");
            }
            ret = releaseJpeg(pJpegCtx);
            if(ret != 0)
            {
                aloge("fatal error! release jpeg fail");
            }
            count = 0;
        }
        sleep(1);
        count++ ;
    }
    return NULL;
}

void *MsgQueueThread(void *pThreadData)
{
    SampleMultiVi2venc2muxerContext *pContext = (SampleMultiVi2venc2muxerContext*)pThreadData;
    message_t stCmdMsg;
    MultiVi2Venc2MuxerMsgType cmd;
    int nCmdPara;

    alogd("message queue thread start.");
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
                    MultiVi2Venc2Muxer_MessageData *pMsgData = stCmdMsg.mpData;
                    int muxerId = nCmdPara;
                    char fileName[MAX_FILE_PATH_SIZE] = {0};
                    int ret;

                    if (muxerId != pMsgData->pRecCtx->mMuxChnAttr.mMuxerId)
                    {
                        alogd("fatal error! muxerId is not match:[0x%x!=0x%x]",
                                muxerId, pMsgData->pRecCtx->mMuxChnAttr.mMuxerId);
                    }
                    generateNextFileName(fileName, pMsgData->pRecCtx);
                    alogd("[%s]mux[%d] set next fd, filepath=%s", pMsgData->pRecCtx->mName, muxerId, fileName);
                    ret = setNextFileToMuxer(pMsgData->pRecCtx, fileName, 0, muxerId);
                    if (0 == ret)
                    {
                        FilePathNode *pNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pNode, 0, sizeof(FilePathNode));
                        strncpy(pNode->strFilePath, fileName, MAX_FILE_PATH_SIZE-1);
                        pthread_mutex_lock(&pMsgData->pRecCtx->mFilePathListLock);
                        list_add_tail(&pNode->mList, &pMsgData->pRecCtx->mFilePathList);
                        pthread_mutex_unlock(&pMsgData->pRecCtx->mFilePathListLock);
                    }
                    //free message mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Rec_FileDone:
                {
                    MultiVi2Venc2Muxer_MessageData *pMsgData = stCmdMsg.mpData;
                    int muxerId = nCmdPara;
                    int ret;

                    pthread_mutex_lock(&pMsgData->pRecCtx->mFilePathListLock);
                    int cnt = 0;
                    struct list_head *pList;
                    list_for_each(pList, &pMsgData->pRecCtx->mFilePathList)
                    {
                        cnt++;
                    }
                    while(cnt > pMsgData->pRecCtx->mVideoFileCnt && pMsgData->pRecCtx->mVideoFileCnt > 0)
                    {
                        FilePathNode *pNode = list_first_entry(&pMsgData->pRecCtx->mFilePathList, FilePathNode, mList);
                        list_del(&pNode->mList);
                        cnt--;
                        if ((ret = remove(pNode->strFilePath)) != 0)
                        {
                            aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                        }
                        else
                        {
                            alogd("delete file[%s] success", pNode->strFilePath);
                        }
                        free(pNode);
                    }
                    pthread_mutex_unlock(&pMsgData->pRecCtx->mFilePathListLock);
                    //free message mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;

                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                    break;
                }
                default :
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
    alogd("message queue thread stop.");
    return NULL;
}

int main(int argc, char** argv)
{
    int result = 0;
    ERRORTYPE eRet = SUCCESS;
    void *pValue = NULL;
    message_t stCmdMsg;
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

    alogd("hello, sample_multi_vi2venc2muxer");
//    if(FILE_EXIST("/mnt/extsd/video"))
//    {
//        system("rm /mnt/extsd/video -rf");
//    }
//    system("mkdir /mnt/extsd/video");
    SampleMultiVi2venc2muxerContext *pContext = constructSampleMultiVi2venc2muxerContext();
    if(NULL == pContext)
    {
        result = -1;
        goto _err0;
    }
    gpSampleMultiVi2venc2muxerContext = pContext;
    
    char *pConfigFilePath;
    if(ParseCmdLine(pContext, argc, argv) != 0)
    {
        result = -1;
        goto _err1;
    }
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    /* parse config file. */
    if(loadSampleMultiVi2venc2muxerConfig(pContext, pConfigFilePath) != 0)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _err1;
    }

    // create msg queue
    if (message_create(&pContext->mMsgQueue) < 0)
    {
        aloge("fatal error! create message queue fail!");
        goto _err1;
    }
    
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }

    initVippConfigs(pContext);
    initRecorders(pContext);
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    eRet = AW_MPI_SYS_Init();
    if (eRet < 0)
    {
        aloge("sys Init failed!");
        result = -1;
        goto _err1_1;
    }

    //create message queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create message queue thread fail[%d]!", result);
        goto _err1_1;
    }
    else
    {
        alogd("create message queue success threadId[0x%x].", &pContext->mMsgQueueThreadId);
    }

    int i;
    //start vipp
    bool bForceRef = true;
    for(i=0; i<MAX_VIPP_DEV_NUM; i++)
    {
        if(pContext->mVippConfigs[i].mbValid)
        {
            result = startVipp(&pContext->mVippConfigs[i], bForceRef, pContext);
            if (result != 0)
            {
                aloge("fatal error! start vipp[%d] fail", pContext->mVippConfigs[i].mViDev);
                result = -1;
                goto _err2;
            }
            bForceRef = false;
        }
    }
    //start every recorder
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pContext->mRecorders[i].mbValid)
        {
            result = prepareRecord(&pContext->mRecorders[i]);
            if(result != 0)
            {
                aloge("fatal error! recorder[%s] prepare fail[%d]", pContext->mRecorders[i].mName, result);
                goto _err3;
            }
        }
    }
    alogd("prepareRecord success!");
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pContext->mRecorders[i].mbValid)
        {
            result = startRecord(&pContext->mRecorders[i]);
            if(result != 0)
            {
                aloge("fatal error! recorder[%s] start fail[%d]", pContext->mRecorders[i].mName, result);
                goto _err4;
            }
        }
    }
    alogd("startRecord success!");
    if (pContext->mConfigPara.mVideoEVipp >= 0)
    {
        result = pthread_create(&pContext->mTakeJpegCtx.mTakePictureThreadId, NULL, TakePictureThread, &pContext->mTakeJpegCtx);
        if (result != 0)
        {
            aloge("fatal error! create Take picture thread fail[%d]", result);
        }
        else
        {
            alogd("take picture threadId:0x%x", pContext->mTakeJpegCtx.mTakePictureThreadId);
        }
    }

    //wait exit.
    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }
    //notify takePicture thread to exit.
    pContext->mTakeJpegCtx.mbExitFlag = true;
    //stop message queue thread
    stCmdMsg.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stCmdMsg);
    int ret;
    pthread_join(pContext->mMsgQueueThreadId, (void*)&ret);
    //stop and release record.
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pContext->mRecorders[i].mbValid)
        {
            result = stopRecord(&pContext->mRecorders[i]);
            if(result != 0)
            {
                aloge("fatal error! recorder[%s] stop fail[%d]", pContext->mRecorders[i].mName, result);
            }
        }
    }
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pContext->mRecorders[i].mbValid)
        {
            result = releaseRecord(&pContext->mRecorders[i]);
            if(result != 0)
            {
                aloge("fatal error! recorder[%s] release fail[%d]", pContext->mRecorders[i].mName, result);
            }
        }
    }

    if (pContext->mConfigPara.mVideoEVipp >= 0)
    {
        result = pthread_join(pContext->mTakeJpegCtx.mTakePictureThreadId, (void**)&pValue);
        alogd("take picture pthread[0x%x] join ret[%d], pValue[%p]", pContext->mTakeJpegCtx.mTakePictureThreadId, result, pValue);
    }
    //stop vipp
    for(i=0; i<MAX_VIPP_DEV_NUM; i++)
    {
        if(pContext->mVippConfigs[i].mbValid)
        {
            result = stopVipp(&pContext->mVippConfigs[i]);
            if (result != 0)
            {
                aloge("fatal error! sstop vipp[%d] fail", pContext->mVippConfigs[i].mViDev);
            }
        }
    }

    eRet = AW_MPI_SYS_Exit();
    if (eRet != SUCCESS)
    {
        aloge("sys exit failed[0x%x]!", eRet);
    }
    destroyRecorders(pContext);
    destructSampleMultiVi2venc2muxerContext(pContext);
    pContext = NULL;
    gpSampleMultiVi2venc2muxerContext = NULL;

    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;

_err4:
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pContext->mRecorders[i].mbValid)
        {
            result = stopRecord(&pContext->mRecorders[i]);
            if(result != 0)
            {
                aloge("fatal error! recorder[%s] stop fail[%d]", pContext->mRecorders[i].mName, result);
            }
        }
    }
_err3:
    for(i=0; i<MAX_RECORDER_COUNT; i++)
    {
        if(pContext->mRecorders[i].mbValid)
        {
            result = releaseRecord(&pContext->mRecorders[i]);
            if(result != 0)
            {
                aloge("fatal error! recorder[%s] release fail[%d]", pContext->mRecorders[i].mName, result);
            }
        }
    }
_err2:
    //stop message queue thread
    stCmdMsg.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stCmdMsg);
    pthread_join(pContext->mMsgQueueThreadId, NULL);
    //stop vipp
    for(i=0; i<MAX_VIPP_DEV_NUM; i++)
    {
        if(pContext->mVippConfigs[i].mbValid)
        {
            if(stopVipp(&pContext->mVippConfigs[i]) != 0)
            {
                aloge("fatal error! stop vipp[%d] fail", pContext->mVippConfigs[i].mViDev);
            }
        }
    }
    eRet = AW_MPI_SYS_Exit();
    if (eRet < 0)
    {
        aloge("sys exit failed!");
    }
_err1_1:
    destroyRecorders(pContext);
    message_destroy(&pContext->mMsgQueue);
_err1:
    destructSampleMultiVi2venc2muxerContext(pContext);
    pContext = NULL;
    gpSampleMultiVi2venc2muxerContext = NULL;
_err0:
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
