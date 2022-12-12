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
#define LOG_TAG "sample_recorder"
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
#include <vo/hwdisplay.h>
#include <mpi_vo.h>
#include <cdx_list.h>

#include "sample_recorder.h"
#include "sample_recorder_conf.h"

#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)
#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)

static SampleRecorderContext *gpSampleRecorderContext = NULL;
static void handle_exit()
{
    alogd("user want to exit!");
    if(NULL != gpSampleRecorderContext)
    {
        cdx_sem_up(&gpSampleRecorderContext->mSemExit);
    }
}

static int ParseCmdLine(SampleRecorderContext *pContext, int argc, char **argv)
{
    alogd("sample_multi_vi2venc2muxer:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(&pContext->mCmdLinePara, 0, sizeof(pContext->mCmdLinePara));
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
                strcpy(pContext->mCmdLinePara.mConfigFilePath, argv[i]);
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

static void judgeCaptureFormat(char *pFormatConf, PIXEL_FORMAT_E *pCapFromat)
{
    if (!strcmp(pFormatConf, "nv21"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pFormatConf, "yv12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pFormatConf, "nv12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pFormatConf, "yu12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pFormatConf, "aw_afbc"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_2_0x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_2_5x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_1_5x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_1_0x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        aloge("fatal error! wrong src pixfmt:%s use default nv21", pFormatConf);
    }
}

static void judgeEncoderType(char *pFormatConf, PAYLOAD_TYPE_E *pEncoderType)
{
    if (!strcmp(pFormatConf, "H.264"))
    {
        *pEncoderType = PT_H264;
    }
    else if (!strcmp(pFormatConf, "H.265"))
    {
        *pEncoderType = PT_H265;
    }
    else if (!strcmp(pFormatConf, "MJPEG"))
    {
        *pEncoderType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! unsupport encoder type[%s] use default H.264", pFormatConf);
        *pEncoderType = PT_H264;
    }
}

static int loadSampleRecordConfig(SampleRecorderContext *pContext, const char *conf_path)
{
    int ret = 0;
    SampleRecorderCondfig *pRecConfig = NULL;
    char *pTmp = NULL;

    if(conf_path != NULL)
    {
        CONFPARSER_S stConf;
        ret = createConfParser(conf_path, &stConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return ret;
        }

        pRecConfig = &pContext->mRecorder[0].mConfig;
        pRecConfig->mVIDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_VIPP_DEV, 0);
        pRecConfig->mIspDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ISP_DEV, 0);
        pRecConfig->mCapWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_CAP_WIDTH, 0);
        pRecConfig->mCapHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_CAP_HEIGHT, 0);
        pRecConfig->mCapFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_CAP_RATE, 0);
        pTmp = (char *)(char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER1_CAP_FORMAT, NULL);
        judgeCaptureFormat(pTmp, &pRecConfig->mCapFormat);
        pRecConfig->mVIBufNum = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_VI_BUFNUM, 0);
        pRecConfig->mEnableWDR = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENABLEWDR, 0);
        pRecConfig->mEncOnlineEnable = GetConfParaInt(&stConf,SAMPLE_RECORD_KEY_RECORDER1_ENC_ONLINE, 0);
        pRecConfig->mEncOnlineShareBufNum = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENC_ONLINE_SHARE_BFUNUM, 0);
        pTmp = (char *)(char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENC_TYPE, NULL);
        judgeEncoderType(pTmp, &pRecConfig->mEncType);
        pRecConfig->mEncWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENC_WIDTH, 0);
        pRecConfig->mEncHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENC_HEIGHT, 0);
        pRecConfig->mEncFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENC_RATE, 0);
        pRecConfig->mEncBitRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENC_BITRATE, 0);
        pRecConfig->mEncRcMode = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_ENC_RCMODE, 0);
        pRecConfig->mEncppSharpAttenCoefPer = 100;
        pRecConfig->mDispX = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_DISP_X, 0);
        pRecConfig->mDispY = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_DISP_Y, 0);
        pRecConfig->mDispWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_DISP_WIDTH, 0);
        pRecConfig->mDispHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_DISP_HEIGHT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER1_DISP_DEV, NULL);
        if (!strcmp(pTmp, "lcd"))
        {
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        else
        {
            aloge("fatal error! unsupport display device[%s] use dafault lcd", pTmp);
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        pRecConfig->mRecDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_REC_DURATION, 0);
        pRecConfig->mRecFileCnt = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER1_REC_FILECNT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER1_REC_FILE, NULL);
        if (pTmp)
        {
            strncpy(pRecConfig->mOutFilePath, pTmp, MAX_FILE_PATH_SIZE-1);
        }

        pRecConfig = &pContext->mRecorder[1].mConfig;
        pRecConfig->mVIDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_VIPP_DEV, 0);
        pRecConfig->mIspDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ISP_DEV, 0);
        pRecConfig->mCapWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_CAP_WIDTH, 0);
        pRecConfig->mCapHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_CAP_HEIGHT, 0);
        pRecConfig->mCapFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_CAP_RATE, 0);
        pRecConfig->mVIBufNum = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_VI_BUFNUM, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER2_CAP_FORMAT, NULL);
        judgeCaptureFormat(pTmp, &pRecConfig->mCapFormat);
        pRecConfig->mEnableWDR = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ENABLEWDR, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ENC_TYPE, NULL);
        judgeEncoderType(pTmp, &pRecConfig->mEncType);
        pRecConfig->mEncWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ENC_WIDTH, 0);
        pRecConfig->mEncHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ENC_HEIGHT, 0);
        pRecConfig->mEncFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ENC_RATE, 0);
        pRecConfig->mEncBitRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ENC_BITRATE, 0);
        pRecConfig->mEncRcMode = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_ENC_RCMODE, 0);
        pRecConfig->mEncppSharpAttenCoefPer = 100 * pContext->mRecorder[1].mConfig.mCapWidth / pContext->mRecorder[0].mConfig.mCapWidth;
        pRecConfig->mDispX = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_DISP_X, 0);
        pRecConfig->mDispY = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_DISP_Y, 0);
        pRecConfig->mDispWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_DISP_WIDTH, 0);
        pRecConfig->mDispHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_DISP_HEIGHT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER2_DISP_DEV, NULL);
        if (!strcmp(pTmp, "lcd"))
        {
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        else
        {
            aloge("fatal error! unsupport display device[%s] use dafault lcd", pTmp);
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        pRecConfig->mRecDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_REC_DURATION, 0);
        pRecConfig->mRecFileCnt = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_REC_FILECNT, 0);
        pRecConfig->mRecDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_REC_DURATION, 0);
        pRecConfig->mRecFileCnt = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER2_REC_FILECNT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER2_REC_FILE, NULL);
        if (pTmp)
        {
            strncpy(pRecConfig->mOutFilePath, pTmp, MAX_FILE_PATH_SIZE-1);
        }

        pRecConfig = &pContext->mRecorder[2].mConfig;
        pRecConfig->mVIDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_VIPP_DEV, 0);
        pRecConfig->mIspDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ISP_DEV, 0);
        pRecConfig->mCapWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_CAP_WIDTH, 0);
        pRecConfig->mCapHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_CAP_HEIGHT, 0);
        pRecConfig->mCapFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_CAP_RATE, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER3_CAP_FORMAT, NULL);
        judgeCaptureFormat(pTmp, &pRecConfig->mCapFormat);
        pRecConfig->mVIBufNum = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_VI_BUFNUM, 0);
        pRecConfig->mEnableWDR = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ENABLEWDR, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ENC_TYPE, NULL);
        judgeEncoderType(pTmp, &pRecConfig->mEncType);
        pRecConfig->mEncWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ENC_WIDTH, 0);
        pRecConfig->mEncHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ENC_HEIGHT, 0);
        pRecConfig->mEncFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ENC_RATE, 0);
        pRecConfig->mEncBitRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ENC_BITRATE, 0);
        pRecConfig->mEncRcMode = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_ENC_RCMODE, 0);
        pRecConfig->mEncppSharpAttenCoefPer = 100 * pContext->mRecorder[2].mConfig.mCapWidth / pContext->mRecorder[0].mConfig.mCapWidth;
        pRecConfig->mDispX = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_DISP_X, 0);
        pRecConfig->mDispY = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_DISP_Y, 0);
        pRecConfig->mDispWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_DISP_WIDTH, 0);
        pRecConfig->mDispHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_DISP_HEIGHT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER3_DISP_DEV, NULL);
        if (!strcmp(pTmp, "lcd"))
        {
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        else
        {
            aloge("fatal error! unsupport display device[%s] use dafault lcd", pTmp);
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        pRecConfig->mRecDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_REC_DURATION, 0);
        pRecConfig->mRecFileCnt = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_REC_FILECNT, 0);
        pRecConfig->mRecDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_REC_DURATION, 0);
        pRecConfig->mRecFileCnt = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER3_REC_FILECNT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER3_REC_FILE, NULL);
        if (pTmp)
        {
            strncpy(pRecConfig->mOutFilePath, pTmp, MAX_FILE_PATH_SIZE-1);
        }

        pRecConfig = &pContext->mRecorder[3].mConfig;
        pRecConfig->mVIDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_VIPP_DEV, 0);
        pRecConfig->mIspDev = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ISP_DEV, 0);
        pRecConfig->mCapWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_CAP_WIDTH, 0);
        pRecConfig->mCapHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_CAP_HEIGHT, 0);
        pRecConfig->mCapFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_CAP_RATE, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER4_CAP_FORMAT, NULL);
        judgeCaptureFormat(pTmp, &pRecConfig->mCapFormat);
        pRecConfig->mVIBufNum = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_VI_BUFNUM, 0);
        pRecConfig->mEnableWDR = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ENABLEWDR, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ENC_TYPE, NULL);
        judgeEncoderType(pTmp, &pRecConfig->mEncType);
        pRecConfig->mEncWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ENC_WIDTH, 0);
        pRecConfig->mEncHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ENC_HEIGHT, 0);
        pRecConfig->mEncFrmRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ENC_RATE, 0);
        pRecConfig->mEncBitRate = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ENC_BITRATE, 0);
        pRecConfig->mEncRcMode = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_ENC_RCMODE, 0);
        pRecConfig->mEncppSharpAttenCoefPer = 100 * pContext->mRecorder[3].mConfig.mCapWidth / pContext->mRecorder[0].mConfig.mCapWidth;
        pRecConfig->mDispX = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_DISP_X, 0);
        pRecConfig->mDispY = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_DISP_Y, 0);
        pRecConfig->mDispWidth = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_DISP_WIDTH, 0);
        pRecConfig->mDispHeight = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_DISP_HEIGHT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER1_DISP_DEV, NULL);
        if (!strcmp(pTmp, "lcd"))
        {
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        else
        {
            aloge("fatal error! unsupport display device[%s] use dafault lcd", pTmp);
            pRecConfig->mDispDev = VO_INTF_LCD;
        }
        pRecConfig->mRecDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_REC_DURATION, 0);
        pRecConfig->mRecFileCnt = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_REC_FILECNT, 0);
        pRecConfig->mRecDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_REC_DURATION, 0);
        pRecConfig->mRecFileCnt = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORDER4_REC_FILECNT, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_RECORD_KEY_RECORDER4_REC_FILE, NULL);
        if (pTmp)
        {
            strncpy(pRecConfig->mOutFilePath, pTmp, MAX_FILE_PATH_SIZE-1);
        }

        pContext->mTestDuration = GetConfParaInt(&stConf, SAMPLE_RECORD_KEY_RECORD_TEST_DURATION, 0);
        destroyConfParser(&stConf);
    }

    for (int i = 0; i < MAX_RECORDER_COUNT; i++)
    {
        pRecConfig = &pContext->mRecorder[i].mConfig;
        alogd("recorder[%d] vi dev[%d] capture size[%d-%d] format[%d] rate[%d] vi buf num[%d] wdr[%d]", \
            i+1, pRecConfig->mVIDev, pRecConfig->mCapWidth, pRecConfig->mCapHeight, \
            pRecConfig->mCapFormat, pRecConfig->mCapFrmRate, pRecConfig->mVIBufNum, pRecConfig->mEnableWDR);
        alogd("recorder[%d] encodr type[%d] size[%d-%d] frmRate[%d] bitRate[%d] rcMode[%d]", \
            i+1, pRecConfig->mEncType, pRecConfig->mEncWidth, pRecConfig->mEncHeight, \
            pRecConfig->mEncFrmRate, pRecConfig->mEncBitRate, pRecConfig->mEncRcMode);
        alogd("recorder[%d] display size[%d-%d-%d-%d] display device[%d]", \
            i+1, pRecConfig->mDispX, pRecConfig->mDispY, pRecConfig->mDispWidth, \
            pRecConfig->mDispHeight, pRecConfig->mDispHeight, pRecConfig->mDispDev);
        alogd("recorder[%d] record file[%s] record duration[%d] record max file cnt[%d]", \
            i+1, pRecConfig->mOutFilePath, pRecConfig->mRecDuration, pRecConfig->mRecFileCnt);
    }
    alogd("sample record test duration[%d]", pContext->mTestDuration);

    return SUCCESS;
}

SampleRecorderContext *constructSampleRecorderContext()
{
    SampleRecorderContext *pContext = (SampleRecorderContext*)malloc(sizeof(*pContext));
    if(pContext != NULL)
    {
        memset(pContext, 0, sizeof(*pContext));
        int ret = cdx_sem_init(&pContext->mSemExit, 0);
        if(ret != 0)
        {
            aloge("fatal error! cdx sem init fail[%d]", ret);
        }
    }
    else
    {
        aloge("fatal error! malloc fail!");
    }
    return pContext;
}

static int initRecorder(SampleRecorderContext *pContext)
{
    SampleRecorder *pRecorder = NULL;
    SampleRecorderCondfig *pRecConfig = NULL;

    for (int i = 0; i < MAX_RECORDER_COUNT; i++)
    {
        pRecorder = &pContext->mRecorder[i];
        pRecConfig = &pRecorder->mConfig;
        pRecorder->mVIDev = pRecConfig->mVIDev;
        pRecorder->mVIChn = MM_INVALID_CHN;
        pRecorder->mIspDev = MM_INVALID_DEV;
        pRecorder->mVeChn = MM_INVALID_CHN;
        pRecorder->mVOLayer = MM_INVALID_LAYER;
        pRecorder->mVOChn = MM_INVALID_CHN;
        pRecorder->mMuxGrp = MM_INVALID_DEV;
        pRecorder->mMuxChn = MM_INVALID_CHN;

        if (pRecorder->mVIDev < 0)
        {
            pRecorder->mbRecorderValid = FALSE;
            alogd("recorder[%d] vi dev[%d] set invalid", i+1, pRecorder->mVIDev);
            continue;
        }
        pRecorder->mbRecorderValid = TRUE;
        pthread_mutex_init(&pRecorder->mFilePathListLock, NULL);
        INIT_LIST_HEAD(&pRecorder->mFilePathList);
        if (0 != pRecConfig->mDispWidth && 0 != pRecConfig->mDispHeight)
        {
            pContext->mbEnableVO = TRUE;
        }
    }
    pContext->mVODev = MM_INVALID_DEV;
    pContext->mUILayer = MM_INVALID_LAYER;

    return 0;
}

static int deinitRecorder(SampleRecorderContext *pContext)
{
    FilePathNode *pEntry, *pTmp;
    SampleRecorder *pRecorder = NULL;

    for (int i = 0; i < MAX_RECORDER_COUNT; i++)
    {
        pRecorder = &pContext->mRecorder[i];
        pRecorder->mVIDev = MM_INVALID_DEV;
        pRecorder->mVIChn = MM_INVALID_CHN;
        pRecorder->mIspDev = MM_INVALID_DEV;
        pRecorder->mVeChn = MM_INVALID_CHN;
        pRecorder->mVOLayer = MM_INVALID_LAYER;
        pRecorder->mVOChn = MM_INVALID_CHN;

        if (pRecorder->mbRecorderValid)
        {
            pthread_mutex_lock(&pRecorder->mFilePathListLock);
            list_for_each_entry_safe(pEntry, pTmp, &pRecorder->mFilePathList, mList)
            {
                list_del(&pEntry->mList);
                if (pEntry)
                {
                    free(pEntry);
                    pEntry = NULL;
                }
            }
            pthread_mutex_unlock(&pRecorder->mFilePathListLock);
            pthread_mutex_destroy(&pRecorder->mFilePathListLock);
        }
        pRecorder->mbRecorderValid = FALSE;
    }
    pContext->mVODev = MM_INVALID_DEV;
    pContext->mUILayer = MM_INVALID_LAYER;
    pContext->mbEnableVO = FALSE;

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

static int generateNextFileName(char *pNameBuf, SampleRecorder *precorder)
{
    precorder->mFileIdCounter++;

    char tmpBuf[MAX_FILE_PATH_SIZE] = {0};
    char *pFileName;
    char *ptr = strrchr(precorder->mConfig.mOutFilePath, '/');
    if(ptr != NULL)
    {
        pFileName = ptr+1;
    }
    else
    {
        pFileName = precorder->mConfig.mOutFilePath;
    }
    ptr = strrchr(pFileName, '.');
    if(ptr != NULL)
    {
        strncpy(tmpBuf, precorder->mConfig.mOutFilePath, MAX_FILE_PATH_SIZE-1);
        sprintf(pNameBuf, "%s_%d%s", tmpBuf, precorder->mFileIdCounter, ptr);
    }
    else
    {
        sprintf(pNameBuf, "%s_%d", precorder->mConfig.mOutFilePath, precorder->mFileIdCounter);
    }
    return 0;
}

static int setNextFileToMuxer(SampleRecorder *pRecorder, char* path, int64_t fallocateLength, int muxerId)
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

        if (pRecorder->mMuxChnAttr.mMuxerId == muxerId)
        {
            ret = AW_MPI_MUX_SwitchFd(pRecorder->mMuxGrp, pRecorder->mMuxChn, fd, (int)fallocateLength);
            if(ret != SUCCESS)
            {
                aloge("fatal error! muxer[%d,%d] switch fd[%d] fail[0x%x]!", pRecorder->mMuxGrp, pRecorder->mMuxChn, fd, ret);
                result = -1;
            }
        }
        else
        {
            aloge("fatal error! muxerId is not match:[0x%x!=0x%x]", pRecorder->mMuxChnAttr.mMuxerId, muxerId);
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
    SampleRecorder *pRecorder = (SampleRecorder *)cookie;

    if (MOD_ID_VENC == pChn->mModId)
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
                    ret = AW_MPI_VI_GetIspDev(pRecorder->mVIDev, &mIspDev);
                    if (ret)
                    {
                        aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", pRecorder->mVIDev, ret);
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
                            mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren * pRecorder->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren * pRecorder->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren * pRecorder->mEncppSharpAttenCoefPer / 100;
                            mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren * pRecorder->mEncppSharpAttenCoefPer / 100;
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
                SampleRecorder_MessageData stMsgData;

                alogd("MuxerId[%d] record file done.", *(int*)pEventData);
                stMsgData.pRecorder = (SampleRecorder*)cookie;
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(SampleRecorder_MessageData);
                stCmdMsg.mpData = &stMsgData;

                putMessageWithData(&gpSampleRecorderContext->mMsgQueue, &stCmdMsg);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stMsgCmd;
                InitMessage(&stMsgCmd);
                SampleRecorder_MessageData stMessageData;

                alogd("MuxerId[%d] need next fd.", *(int*)pEventData);
                stMessageData.pRecorder = (SampleRecorder*)cookie;
                stMsgCmd.command = Rec_NeedSetNextFd;
                stMsgCmd.para0 = *(int *)pEventData;
                stMsgCmd.mDataSize = sizeof(SampleRecorder_MessageData);
                stMsgCmd.mpData = &stMessageData;

                putMessageWithData(&gpSampleRecorderContext->mMsgQueue, &stMsgCmd);
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
    else if (MOD_ID_VOU == pChn->mModId)
    {
        switch (event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("vo report video display size[%dx%d]", pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo report rendering start");
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
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

static void configViAttr(SampleRecorderCondfig *pRecConfig, VI_ATTR_S *pViAttr)
{
    if (pRecConfig->mEncOnlineEnable)
    {
        pViAttr->mOnlineEnable = 1;
        pViAttr->mOnlineShareBufNum = pRecConfig->mEncOnlineShareBufNum;
    }
    pViAttr->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pViAttr->memtype = V4L2_MEMORY_MMAP;
    pViAttr->format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pRecConfig->mCapFormat);
    pViAttr->format.field = V4L2_FIELD_NONE;
    pViAttr->format.colorspace = V4L2_COLORSPACE_JPEG;
    pViAttr->format.width = pRecConfig->mCapWidth;
    pViAttr->format.height = pRecConfig->mCapHeight;
    pViAttr->nbufs = pRecConfig->mVIBufNum;
    pViAttr->nplanes = 2;
    pViAttr->fps = pRecConfig->mCapFrmRate;
    pViAttr->use_current_win = 0;
    pViAttr->wdr_mode = pRecConfig->mEnableWDR;
    pViAttr->capturemode = V4L2_MODE_VIDEO;
    pViAttr->drop_frame_num = 0;
    pViAttr->mbEncppEnable = TRUE;
}

static int createVipp(SampleRecorder *pRecorder)
{
    ERRORTYPE eRet = SUCCESS;
    SampleRecorderCondfig *pRecConfig = &pRecorder->mConfig;

    pRecorder->mVIDev = pRecConfig->mVIDev;
    eRet = AW_MPI_VI_CreateVipp(pRecorder->mVIDev);
    if (SUCCESS != eRet)
    {
        pRecorder->mVIDev = MM_INVALID_DEV;
        aloge("fatal error! vi dev[%d] create fail!", pRecorder->mVIDev);
        return -1;
    }

    memset(&pRecorder->mVIAttr, 0, sizeof(VI_ATTR_S));
    AW_MPI_VI_GetVippAttr(pRecorder->mVIDev, &pRecorder->mVIAttr);
    configViAttr(pRecConfig, &pRecorder->mVIAttr);
    eRet = AW_MPI_VI_SetVippAttr(pRecorder->mVIDev, &pRecorder->mVIAttr);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! vi dev[%d] set attr fail!", pRecorder->mVIDev);
        return -1;
    }

    pRecorder->mIspDev = pRecConfig->mIspDev;
    if (pRecorder->mIspDev >= 0)
    {
        eRet = AW_MPI_ISP_Run(pRecorder->mIspDev);
        if (SUCCESS != eRet)
        {
            aloge("fatal error! ISP[%d] init fail!", pRecorder->mIspDev);
            return -1;
        }
    }

    eRet = AW_MPI_VI_EnableVipp(pRecorder->mVIDev);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! vi dev[%d] enable fail!", pRecorder->mVIDev);
        return -1;
    }

    pRecorder->mVIChn = 0;
    eRet = AW_MPI_VI_CreateVirChn(pRecorder->mVIDev, pRecorder->mVIChn, NULL);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! vi chn[%d] create fail!", pRecorder->mVIChn);
        return -1;
    }

    alogd("create vi dev[%d] vi chn[%d] success!", pRecorder->mVIDev, pRecorder->mVIChn);
    return 0;
}

static int configVencChnAttr(SampleRecorderCondfig *pRecConfig, VENC_CHN_ATTR_S *pVencChnAttr, VENC_RC_PARAM_S *pVencRcParam)
{
    pVencChnAttr->VeAttr.Type = pRecConfig->mEncType;
    if (pRecConfig->mEncOnlineEnable)
    {
        pVencChnAttr->VeAttr.mOnlineEnable = pRecConfig->mEncOnlineEnable;
        pVencChnAttr->VeAttr.mOnlineShareBufNum = pRecConfig->mEncOnlineShareBufNum;
    }
    switch(pVencChnAttr->VeAttr.Type)
    {
        case PT_H264:
        {
            pVencChnAttr->VeAttr.AttrH264e.mThreshSize = ALIGN((pRecConfig->mEncWidth*pRecConfig->mEncHeight*3/2)/3, 1024);
            pVencChnAttr->VeAttr.AttrH264e.BufSize = ALIGN(pRecConfig->mEncBitRate*4/8 + pVencChnAttr->VeAttr.AttrH264e.mThreshSize, 1024);
            pVencChnAttr->VeAttr.AttrH264e.Profile = 2;//0:base 1:main 2:high
            pVencChnAttr->VeAttr.AttrH264e.bByFrame = TRUE;
            pVencChnAttr->VeAttr.AttrH264e.PicWidth  = pRecConfig->mEncWidth;
            pVencChnAttr->VeAttr.AttrH264e.PicHeight = pRecConfig->mEncHeight;
            pVencChnAttr->VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
            pVencChnAttr->VeAttr.AttrH264e.FastEncFlag = FALSE;
            pVencChnAttr->VeAttr.AttrH264e.IQpOffset = 0;
            pVencChnAttr->VeAttr.AttrH264e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_H265:
        {
            pVencChnAttr->VeAttr.AttrH265e.mThreshSize = ALIGN((pRecConfig->mEncWidth*pRecConfig->mEncHeight*3/2)/3, 1024);
            pVencChnAttr->VeAttr.AttrH265e.mBufSize = ALIGN(pRecConfig->mEncBitRate*4/8 + pVencChnAttr->VeAttr.AttrH264e.mThreshSize, 1024);
            pVencChnAttr->VeAttr.AttrH265e.mProfile = 0; //0:main 1:main10 2:sti11
            pVencChnAttr->VeAttr.AttrH265e.mbByFrame = TRUE;
            pVencChnAttr->VeAttr.AttrH265e.mPicWidth = pRecConfig->mEncWidth;
            pVencChnAttr->VeAttr.AttrH265e.mPicHeight = pRecConfig->mEncHeight;
            pVencChnAttr->VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
            pVencChnAttr->VeAttr.AttrH265e.mFastEncFlag = FALSE;
            pVencChnAttr->VeAttr.AttrH265e.IQpOffset = 0;
            pVencChnAttr->VeAttr.AttrH265e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_MJPEG:
        {
            pVencChnAttr->VeAttr.AttrMjpeg.mbByFrame = TRUE;
            pVencChnAttr->VeAttr.AttrMjpeg.mPicWidth = pRecConfig->mEncWidth;
            pVencChnAttr->VeAttr.AttrMjpeg.mPicHeight = pRecConfig->mEncHeight;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pVencChnAttr->VeAttr.Type);
            break;
        }
    }
    pVencChnAttr->VeAttr.SrcPicWidth = pRecConfig->mCapWidth;
    pVencChnAttr->VeAttr.SrcPicHeight = pRecConfig->mCapHeight;
    pVencChnAttr->VeAttr.Field = VIDEO_FIELD_FRAME;
    pVencChnAttr->VeAttr.PixelFormat = pRecConfig->mCapFormat;
    pVencChnAttr->VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    pVencChnAttr->VeAttr.Rotate = ROTATE_NONE;

    pVencChnAttr->EncppAttr.mbEncppEnable = TRUE;

    switch(pVencChnAttr->VeAttr.Type)
    {
        case PT_H264:
        {
            switch (pRecConfig->mEncRcMode)
            {
                case 1:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                    pVencChnAttr->RcAttr.mAttrH264Vbr.mMaxBitRate = pRecConfig->mEncBitRate;
                    pVencRcParam->ParamH264Vbr.mMaxQp = 51;
                    pVencRcParam->ParamH264Vbr.mMinQp = 10;
                    pVencRcParam->ParamH264Vbr.mMaxPqp = 50;
                    pVencRcParam->ParamH264Vbr.mMinPqp = 10;
                    pVencRcParam->ParamH264Vbr.mQpInit = 38;
                    pVencRcParam->ParamH264Vbr.mMovingTh = 20;
                    pVencRcParam->ParamH264Vbr.mQuality = 10;
                    break;
                }
                case 2:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
                    pVencChnAttr->RcAttr.mAttrH264FixQp.mIQp = 28;
                    pVencChnAttr->RcAttr.mAttrH264FixQp.mPQp = 28;
                    break;
                }
                case 3:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
                    pVencChnAttr->RcAttr.mAttrH264Abr.mMaxBitRate = pRecConfig->mEncBitRate;
                    pVencChnAttr->RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
                    pVencChnAttr->RcAttr.mAttrH264Abr.mQuality = 8;
                    pVencChnAttr->RcAttr.mAttrH264Abr.mMinIQp = 20;
                    pVencChnAttr->RcAttr.mAttrH264Abr.mMaxQp = 51;
                    pVencChnAttr->RcAttr.mAttrH264Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                    pVencChnAttr->RcAttr.mAttrH264Cbr.mBitRate = pRecConfig->mEncBitRate;
                    pVencRcParam->ParamH264Cbr.mMaxQp = 51;
                    pVencRcParam->ParamH264Cbr.mMinQp = 10;
                    pVencRcParam->ParamH264Cbr.mMaxPqp = 50;
                    pVencRcParam->ParamH264Cbr.mMinPqp = 10;
                    pVencRcParam->ParamH264Cbr.mQpInit = 38;
                    break;
                }
            }
            break;
        }
        case PT_H265:
        {
            switch (pRecConfig->mEncRcMode)
            {
                case 1:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                    pVencChnAttr->RcAttr.mAttrH265Vbr.mMaxBitRate = pRecConfig->mEncBitRate;
                    pVencRcParam->ParamH265Vbr.mMaxQp = 51;
                    pVencRcParam->ParamH265Vbr.mMinQp = 10;
                    pVencRcParam->ParamH265Vbr.mMaxPqp = 50;
                    pVencRcParam->ParamH265Vbr.mMinPqp = 10;
                    pVencRcParam->ParamH265Vbr.mQpInit = 38;
                    pVencRcParam->ParamH265Vbr.mMovingTh = 20;
                    pVencRcParam->ParamH265Vbr.mQuality = 10;
                    break;
                }
                case 2:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
                    pVencChnAttr->RcAttr.mAttrH265FixQp.mIQp = 28;
                    pVencChnAttr->RcAttr.mAttrH265FixQp.mPQp = 28;
                    break;
                }
                case 3:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
                    pVencChnAttr->RcAttr.mAttrH265Abr.mMaxBitRate = pRecConfig->mEncBitRate;
                    pVencChnAttr->RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
                    pVencChnAttr->RcAttr.mAttrH265Abr.mQuality = 8;
                    pVencChnAttr->RcAttr.mAttrH265Abr.mMinIQp = 20;
                    pVencChnAttr->RcAttr.mAttrH265Abr.mMaxQp = 51;
                    pVencChnAttr->RcAttr.mAttrH265Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                    pVencChnAttr->RcAttr.mAttrH265Cbr.mBitRate = pRecConfig->mEncBitRate;
                    pVencRcParam->ParamH265Cbr.mMaxQp = 51;
                    pVencRcParam->ParamH265Cbr.mMinQp = 1;
                    pVencRcParam->ParamH265Cbr.mMaxPqp = 50;
                    pVencRcParam->ParamH265Cbr.mMinPqp = 10;
                    pVencRcParam->ParamH265Cbr.mQpInit = 38;
                    break;
                }
            }
            break;
        }
        case PT_MJPEG:
        {
            if(pRecConfig->mEncRcMode != 0)
            {
                aloge("fatal error! mjpeg don't support rcMode[%d]!", pRecConfig->mEncBitRate);
            }
            pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pVencChnAttr->RcAttr.mAttrMjpegeCbr.mBitRate = pRecConfig->mEncBitRate;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pVencChnAttr->VeAttr.Type);
            break;
        }
    }
    pVencChnAttr->GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    alogd("venc ste Rcmode=%d", pVencChnAttr->RcAttr.mRcMode);

    return 0;
}

static int createVencChn(SampleRecorder *pRecorder)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;
    SampleRecorderCondfig *pRecConfig = &pRecorder->mConfig;

    memset(&pRecorder->mVeChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&pRecorder->mVencRcParam, 0, sizeof(VENC_RC_PARAM_S));
    configVencChnAttr(pRecConfig, &pRecorder->mVeChnAttr, &pRecorder->mVencRcParam);
    pRecorder->mEncppSharpAttenCoefPer = pRecConfig->mEncppSharpAttenCoefPer;
    alogd("encpp SharpAttenCoefPer=%d", pRecorder->mEncppSharpAttenCoefPer);
    pRecorder->mVeChn = 0;
    while (pRecorder->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pRecorder->mVeChn, &pRecorder->mVeChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pRecorder->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            //alogd("venc channel[%d] is exist, find next!", pRecorder->mVeChn);
            pRecorder->mVeChn++;
        }
        else
        {
            aloge("fatal error! create venc channel[%d] ret[0x%x], find next!", pRecorder->mVeChn, ret);
            pRecorder->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pRecorder->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return -1;
    }
    else
    {
        AW_MPI_VENC_SetRcParam(pRecorder->mVeChn, &pRecorder->mVencRcParam);
        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = pRecConfig->mCapFrmRate;
        stFrameRate.DstFrmRate = pRecConfig->mEncFrmRate;
        alogd("set srcFrameRate:%d, venc framerate:%d", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        ret = AW_MPI_VENC_SetFrameRate(pRecorder->mVeChn, &stFrameRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! venc set framerate fail[0x%x]!", ret);
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pRecorder;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pRecorder->mVeChn, &cbInfo);

        alogd("create venc chn[%d] success!", pRecorder->mVeChn);
        return result;
    }
}

static void configMuxGrpAttr(SampleRecorder *pRecorder, MUX_GRP_ATTR_S *pMuxGrpAttr)
{
    pMuxGrpAttr->mVideoAttrValidNum = 1;
    pMuxGrpAttr->mVideoAttr[0].mWidth = pRecorder->mConfig.mEncWidth;
    pMuxGrpAttr->mVideoAttr[0].mHeight = pRecorder->mConfig.mEncHeight;
    pMuxGrpAttr->mVideoAttr[0].mVideoFrmRate = pRecorder->mConfig.mEncFrmRate*1000;
    pMuxGrpAttr->mVideoAttr[0].mVideoEncodeType = pRecorder->mConfig.mEncType;
    pMuxGrpAttr->mVideoAttr[0].mVeChn = pRecorder->mVeChn;
    pMuxGrpAttr->mAudioEncodeType = PT_MAX;
}

static int createMuxGrp(SampleRecorder *pRecorder)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    memset(&pRecorder->mMuxGrpAttr, 0, sizeof(MUX_GRP_ATTR_S));
    configMuxGrpAttr(pRecorder, &pRecorder->mMuxGrpAttr);
    pRecorder->mMuxGrp = 0;
    while (pRecorder->mMuxGrp < MUX_MAX_GRP_NUM)
    {
        ret = AW_MPI_MUX_CreateGrp(pRecorder->mMuxGrp, &pRecorder->mMuxGrpAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] success!", pRecorder->mMuxGrp);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            //alogd("mux group[%d] is exist, find next!", pRecorder->mMuxGrp);
            pRecorder->mMuxGrp++;
        }
        else
        {
            alogd("create mux group[%d] ret[0x%x], find next!", pRecorder->mMuxGrp, ret);
            pRecorder->mMuxGrp++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pRecorder->mMuxGrp = MM_INVALID_CHN;
        aloge("fatal error! create mux group fail!");
        return -1;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pRecorder;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pRecorder->mMuxGrp, &cbInfo);
        return 0;
    }

    alogd("create mux grp[%d] success!", pRecorder->mMuxGrp);
    return 0;
}

static int createMuxChn(SampleRecorder *pRecorder)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;
    memset(&pRecorder->mMuxChnAttr, 0, sizeof(pRecorder->mMuxChnAttr));
    pRecorder->mMuxChnAttr.mMuxerId = pRecorder->mMuxGrp << 16 | 0;
    pRecorder->mMuxChnAttr.mMediaFileFormat = getFileFormatByName(pRecorder->mConfig.mOutFilePath);
    pRecorder->mMuxChnAttr.mMaxFileDuration = pRecorder->mConfig.mRecDuration*1000;
    pRecorder->mMuxChnAttr.mMaxFileSizeBytes = 0;
    pRecorder->mMuxChnAttr.mFallocateLen = 0;
    pRecorder->mMuxChnAttr.mCallbackOutFlag = FALSE;
    pRecorder->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pRecorder->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    pRecorder->mMuxChnAttr.bBufFromCacheFlag = FALSE;

    int nFd = open(pRecorder->mConfig.mOutFilePath, O_RDWR | O_CREAT, 0666);
    if (nFd < 0)
    {
        aloge("fatal error! Failed to open %s", pRecorder->mConfig.mOutFilePath);
        return -1;
    }

    pRecorder->mMuxChn = 0;
    nSuccessFlag = FALSE;
    while (pRecorder->mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pRecorder->mMuxGrp, pRecorder->mMuxChn, &pRecorder->mMuxChnAttr, nFd);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pRecorder->mMuxGrp, pRecorder->mMuxChn, pRecorder->mMuxChnAttr.mMuxerId);
            break;
        }
        else if(ERR_MUX_EXIST == ret)
        {
            pRecorder->mMuxChn++;
        }
        else
        {
            aloge("fatal error! create mux chn fail[0x%x]!", ret);
            pRecorder->mMuxChn++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        pRecorder->mMuxChn = MM_INVALID_CHN;
        aloge("fatal error! create muxChannel in muxGroup[%d] fail!", pRecorder->mMuxGrp);
        result = -1;
    }
    if(nFd >= 0)
    {
        close(nFd);
        nFd = -1;
    }

    alogd("create mux chn[%d] file format[%d]", pRecorder->mMuxChn, pRecorder->mMuxChnAttr.mMediaFileFormat);
    return result;
}

static int createVOChn(SampleRecorder *pRecorder)
{
    pRecorder->mVOLayer = 0;
    while(pRecorder->mVOLayer < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(pRecorder->mVOLayer))
        {
            break;
        }
        pRecorder->mVOLayer++;
    }
    if(pRecorder->mVOLayer >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
    }
    memset(&pRecorder->mVOLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
    AW_MPI_VO_GetVideoLayerAttr(pRecorder->mVOLayer, &pRecorder->mVOLayerAttr);
    pRecorder->mVOLayerAttr.stDispRect.X = pRecorder->mConfig.mDispX;
    pRecorder->mVOLayerAttr.stDispRect.Y = pRecorder->mConfig.mDispY;
    pRecorder->mVOLayerAttr.stDispRect.Width = pRecorder->mConfig.mDispWidth;
    pRecorder->mVOLayerAttr.stDispRect.Height = pRecorder->mConfig.mDispHeight;
    AW_MPI_VO_SetVideoLayerAttr(pRecorder->mVOLayer, &pRecorder->mVOLayerAttr);

    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    pRecorder->mVOChn = 0;
    while(pRecorder->mVOChn < VO_MAX_CHN_NUM)
    {
        ret = AW_MPI_VO_CreateChn(pRecorder->mVOLayer, pRecorder->mVOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", pRecorder->mVOChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == ret)
        {
            alogd("vo channel[%d] is exist, find next!", pRecorder->mVOChn);
            pRecorder->mVOChn++;
        }
        else
        {
            aloge("fatal error! create vo channel[%d] ret[0x%x]!", pRecorder->mVOChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        pRecorder->mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&pRecorder;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VO_RegisterCallback(pRecorder->mVOLayer, pRecorder->mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pRecorder->mVOLayer, pRecorder->mVOChn, 2);

    alogd("create vo layer[%d] create vo chn[%d] success", pRecorder->mVOLayer, pRecorder->mVOChn);
    return 0;
}

static int prepareRecord(SampleRecorder *pRecorder)
{
    ERRORTYPE eRet = SUCCESS;
    int result = 0;
    SampleRecorderCondfig *pRecConfig = &pRecorder->mConfig;

    if (!pRecorder->mbRecorderValid)
    {
        return 0;
    }

    if (createVipp(pRecorder) != 0)
    {
        aloge("fatal eorror! create vipp fail!");
        return -1;
    }

    if ((0 != pRecConfig->mEncWidth && 0 != pRecConfig->mEncHeight) && \
        (0 == pRecConfig->mDispWidth || 0 == pRecConfig->mDispHeight))
    {
        if (createVencChn(pRecorder) != 0)
        {
            aloge("fatal error! create venc fail!");
            return -1;
        }
        if (createMuxGrp(pRecorder) != 0)
        {
            aloge("fatal error! create mux grp fail!");
            return -1;
        }
        //set spspps
        if (pRecorder->mVeChnAttr.VeAttr.Type == PT_H264)
        {
            VencHeaderData H264SpsPpsInfo;
            AW_MPI_VENC_GetH264SpsPpsInfo(pRecorder->mVeChn, &H264SpsPpsInfo);
            AW_MPI_MUX_SetH264SpsPpsInfo(pRecorder->mMuxGrp, pRecorder->mVeChn, &H264SpsPpsInfo);
        }
        else if(pRecorder->mVeChnAttr.VeAttr.Type == PT_H265)
        {
            VencHeaderData H265SpsPpsInfo;
            AW_MPI_VENC_GetH265SpsPpsInfo(pRecorder->mVeChn, &H265SpsPpsInfo);
            AW_MPI_MUX_SetH265SpsPpsInfo(pRecorder->mMuxGrp, pRecorder->mVeChn, &H265SpsPpsInfo);
        }
        else
        {
            alogd("don't need set spspps for encodeType[%d]", pRecorder->mVeChnAttr.VeAttr.Type);
        }
        if(createMuxChn(pRecorder) != 0)
        {
            aloge("create mux channel fail");
            return -1;
        }
        FilePathNode *pNode = (FilePathNode*)malloc(sizeof(FilePathNode));
        memset(pNode, 0, sizeof(*pNode));
        strncpy(pNode->strFilePath, pRecConfig->mOutFilePath, MAX_FILE_PATH_SIZE-1);
        pthread_mutex_lock(&pRecorder->mFilePathListLock);
        list_add_tail(&pNode->mList, &pRecorder->mFilePathList);
        pthread_mutex_unlock(&pRecorder->mFilePathListLock);

        MPP_CHN_S ViChn = {MOD_ID_VIU, pRecorder->mVIDev, pRecorder->mVIChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pRecorder->mVeChn};
        eRet = AW_MPI_SYS_Bind(&ViChn, &VeChn);
        if(eRet!=SUCCESS)
        {
            aloge("fatal error! bind vi[%d-%d] ve chn[%d] fail!", \
                pRecorder->mVIDev, pRecorder->mVIChn, pRecorder->mVeChn);
            return -1;
        }
        MPP_CHN_S MuxGrp = {MOD_ID_MUX, 0, pRecorder->mMuxGrp};
        eRet = AW_MPI_SYS_Bind(&VeChn, &MuxGrp);
        if(eRet!=SUCCESS)
        {
            aloge("fatal error! bind ve chn[%d] mux grp[%d] fail!", \
                pRecorder->mVeChn, pRecorder->mMuxGrp);
            return -1;
        }
    }
    if ((0 != pRecConfig->mDispWidth && 0 != pRecConfig->mDispHeight) && \
        (0 == pRecConfig->mEncWidth || 0 == pRecConfig->mEncHeight))
    {
        if (createVOChn(pRecorder) != 0)
        {
            aloge("fatal error! create vo chn fail!");
            return -1;
        }
        MPP_CHN_S VIChn = {MOD_ID_VIU, pRecorder->mVIDev, pRecorder->mVIChn};
        MPP_CHN_S VOChn = {MOD_ID_VOU, pRecorder->mVOLayer, pRecorder->mVOChn};
        eRet = AW_MPI_SYS_Bind(&VIChn, &VOChn);
        if(eRet!=SUCCESS)
        {
            aloge("fatal error! bind vi [%d-%d] vo[%d-%d] fail!", \
                pRecorder->mVIDev, pRecorder->mVIChn, pRecorder->mVOLayer, pRecorder->mVOChn);
            return -1;
        }
    }
    return result;
}

static int startRecord(SampleRecorder *pRecorder)
{
    if (!pRecorder->mbRecorderValid)
    {
        return 0;
    }

    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_VI_EnableVirChn(pRecorder->mVIDev, pRecorder->mVIChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! viChn[%d,%d] enable error[0x%x]!", pRecorder->mVIDev, pRecorder->mVIChn, ret);
    }

    if (pRecorder->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StartRecvPic(pRecorder->mVeChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] start error[0x%x]!", pRecorder->mVeChn, ret);
        }
    }

    if (pRecorder->mMuxGrp >= 0)
    {
        ret = AW_MPI_MUX_StartGrp(pRecorder->mMuxGrp);
        if (ret != SUCCESS)
        {
            aloge("fatal error! muxGroup[%d] start error[0x%x]!", pRecorder->mMuxGrp, ret);
        }
    }

    if (pRecorder->mVOLayer >= 0 && pRecorder->mVOChn >= 0)
    {
        ret = AW_MPI_VO_StartChn(pRecorder->mVOLayer, pRecorder->mVOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vo layer[%d] vo chn[%d] start fail!", pRecorder->mVOLayer, pRecorder->mVOChn);
        }
    }
    return result;
}
static int stopRecord(SampleRecorder *pRecorder)
{
    ERRORTYPE ret = SUCCESS;

    if (!pRecorder->mbRecorderValid)
    {
        return 0;
    }

    if (pRecorder->mVIChn >= 0)
    {
        ret = AW_MPI_VI_DisableVirChn(pRecorder->mVIDev, pRecorder->mVIChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d]chn[%d] disabled fail[0x%x]", pRecorder->mVIDev, pRecorder->mVIChn, ret);
        }
    }
    if (pRecorder->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StopRecvPic(pRecorder->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] stop fail[0x%x]", pRecorder->mVeChn, ret);
        }
    }
    if (pRecorder->mMuxGrp >= 0)
    {
        ret = AW_MPI_MUX_StopGrp(pRecorder->mMuxGrp);
        if(ret != SUCCESS)
        {
            aloge("fatal error! muxGrp[%d] stop fail[0x%x]", pRecorder->mMuxGrp, ret);
        }
    }
    if (pRecorder->mVOLayer >= 0 && pRecorder->mVOChn >= 0)
    {
        ret = AW_MPI_VO_StopChn(pRecorder->mVOLayer, pRecorder->mVOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vo layer[%d] vo chn[%d] stop fail!", pRecorder->mVOLayer, pRecorder->mVOChn);
        }
    }
    return 0;
}

static int releaseRecord(SampleRecorder *pRecorder)
{
    ERRORTYPE ret = SUCCESS;
    if (pRecorder->mMuxGrp >= 0)
    {
        ret = AW_MPI_MUX_DestroyGrp(pRecorder->mMuxGrp);
        if(ret != SUCCESS)
        {
            aloge("fatal error! muxGrp[%d] destroy fail[0x%x]", pRecorder->mMuxGrp, ret);
        }
        pRecorder->mMuxGrp = MM_INVALID_CHN;
    }
    if (pRecorder->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_ResetChn(pRecorder->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] stop fail[0x%x]", pRecorder->mVeChn, ret);
        }
        ret = AW_MPI_VENC_DestroyChn(pRecorder->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] destroy fail[0x%x]", pRecorder->mVeChn, ret);
        }
        pRecorder->mVeChn = MM_INVALID_CHN;
    }
    if (pRecorder->mVOLayer >= 0 && pRecorder->mVOChn >= 0)
    {
        ret = AW_MPI_VO_DestroyChn(pRecorder->mVOLayer, pRecorder->mVOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vo layer[%d] vo chn[%d] destroy fail!", pRecorder->mVOLayer, pRecorder->mVOChn);
        }
        ret = AW_MPI_VO_DisableVideoLayer(pRecorder->mVOLayer);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vo layer[%d] disable fail!", pRecorder->mVOLayer);
        }
        pRecorder->mVOChn = MM_INVALID_CHN;
        pRecorder->mVOLayer = MM_INVALID_LAYER;
    }
    if (pRecorder->mVIChn >= 0)
    {
        ret = AW_MPI_VI_DestroyVirChn(pRecorder->mVIDev, pRecorder->mVIChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d]Chn[%d] stop fail[0x%x]", pRecorder->mVIDev, pRecorder->mVIChn, ret);
        }
        pRecorder->mVeChn = MM_INVALID_CHN;
    }
    if (pRecorder->mVIDev >= 0)
    {
        ret = AW_MPI_VI_DisableVipp(pRecorder->mVIDev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vi dev[%d] disable fail!", pRecorder->mVIDev);
        }
    }
    if (pRecorder->mIspDev >= 0)
    {
        ret = AW_MPI_ISP_Stop(pRecorder->mIspDev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! isp[%d] stop fail!", pRecorder->mIspDev);
        }
    }
    if (pRecorder->mVIDev >= 0)
    {
        ret = AW_MPI_VI_DestroyVipp(pRecorder->mVIDev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vi dev[%d] destroy fail!", pRecorder->mVIDev);
        }
    }
    return 0;
}

void *MsgQueueThread(void *pThreadData)
{
    SampleRecorderContext *pContext = (SampleRecorderContext*)pThreadData;
    message_t stCmdMsg;
    SampleRecorderMsgType cmd;
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
                    SampleRecorder_MessageData *pMsgData = stCmdMsg.mpData;
                    int muxerId = nCmdPara;
                    char fileName[MAX_FILE_PATH_SIZE] = {0};
                    int ret;

                    if (muxerId != pMsgData->pRecorder->mMuxChnAttr.mMuxerId)
                    {
                        alogd("fatal error! muxerId is not match:[0x%x!=0x%x]",
                                muxerId, pMsgData->pRecorder->mMuxChnAttr.mMuxerId);
                    }
                    generateNextFileName(fileName, pMsgData->pRecorder);
                    alogd("mux[%d] set next fd, filepath=%s", muxerId, fileName);
                    ret = setNextFileToMuxer(pMsgData->pRecorder, fileName, 0, muxerId);
                    if (0 == ret)
                    {
                        FilePathNode *pNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pNode, 0, sizeof(FilePathNode));
                        strncpy(pNode->strFilePath, fileName, MAX_FILE_PATH_SIZE-1);
                        pthread_mutex_lock(&pMsgData->pRecorder->mFilePathListLock);
                        list_add_tail(&pNode->mList, &pMsgData->pRecorder->mFilePathList);
                        pthread_mutex_unlock(&pMsgData->pRecorder->mFilePathListLock);
                    }
                    //free message mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Rec_FileDone:
                {
                    SampleRecorder_MessageData *pMsgData = stCmdMsg.mpData;
                    //int muxerId = nCmdPara;
                    int ret;

                    pthread_mutex_lock(&pMsgData->pRecorder->mFilePathListLock);
                    int cnt = 0;
                    struct list_head *pList;
                    list_for_each(pList, &pMsgData->pRecorder->mFilePathList)
                    {
                        cnt++;
                    }
                    while(cnt > pMsgData->pRecorder->mConfig.mRecFileCnt && pMsgData->pRecorder->mConfig.mRecFileCnt > 0)
                    {
                        FilePathNode *pNode = list_first_entry(&pMsgData->pRecorder->mFilePathList, FilePathNode, mList);
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
                    pthread_mutex_unlock(&pMsgData->pRecorder->mFilePathListLock);
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
//    void *pValue = NULL;
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
    SampleRecorderContext *pContext = constructSampleRecorderContext();
    if(NULL == pContext)
    {
        result = -1;
        goto _exit;
    }
    gpSampleRecorderContext = pContext;

    char *pConfigFilePath;
    if(ParseCmdLine(pContext, argc, argv) != 0)
    {
        result = -1;
        goto _destroy_context;
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
    if(loadSampleRecordConfig(pContext, pConfigFilePath) != 0)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _destroy_context;
    }

    // create msg queue
    if (message_create(&pContext->mMsgQueue) < 0)
    {
        aloge("fatal error! create message queue fail!");
        goto _destroy_context;
    }

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }

    initRecorder(pContext);
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    eRet = AW_MPI_SYS_Init();
    if (eRet < 0)
    {
        aloge("sys Init failed!");
        result = -1;
        goto _destroy_recorders;
    }

    //create message queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create message queue thread fail[%d]!", result);
        goto _mpp_exit;
    }
    else
    {
        alogd("create message queue success threadId[0x%x].", &pContext->mMsgQueueThreadId);
    }

    if (pContext->mbEnableVO)
    {
        pContext->mVODev = 0;
        eRet = AW_MPI_VO_Enable(pContext->mVODev);
        if (SUCCESS != eRet)
        {
            aloge("fatal error! vo dev[%d] enable fail!", pContext->mVODev);
            goto _stop_msgque;
        }
        pContext->mUILayer = HLAY(2, 0);
        AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
        AW_MPI_VO_CloseVideoLayer(pContext->mUILayer); /* close ui layer. */
        VO_PUB_ATTR_S spPubAttr;
        AW_MPI_VO_GetPubAttr(pContext->mVODev, &spPubAttr);
        spPubAttr.enIntfType = VO_INTF_LCD;
        spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
        AW_MPI_VO_SetPubAttr(pContext->mVODev, &spPubAttr);
    }

    for (int i = 0; i < MAX_RECORDER_COUNT; i++)
    {
        SampleRecorder *pRecorder = &pContext->mRecorder[i];
        result = prepareRecord(pRecorder);
        if (result)
        {
            aloge("fatal errpr! recorder[%d] prepare fail!", i+1);
            pRecorder->mbRecorderValid = FALSE;
        }
    }

    for (int i = 0; i < MAX_RECORDER_COUNT; i++)
    {
        SampleRecorder *pRecorder = &pContext->mRecorder[i];
        result = startRecord(pRecorder);
        if (result)
        {
            aloge("fatal error! recorder[%d] start fail!", i+1);
            pRecorder->mbRecorderValid = FALSE;
        }
    }

    //wait exit.
    if (pContext->mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    //stop and release record.
    for (int i = 0; i < MAX_RECORDER_COUNT; i++)
    {
        SampleRecorder *pRecorder = &pContext->mRecorder[i];
        result = stopRecord(pRecorder);
        if (result)
        {
            aloge("fatal error! recorder[%d] start fail!", i+1);
            pRecorder->mbRecorderValid = FALSE;
        }
    }

    for (int i = 0; i < MAX_RECORDER_COUNT; i++)
    {
        SampleRecorder *pRecorder = &pContext->mRecorder[i];
        result = releaseRecord(pRecorder);
        if (result)
        {
            aloge("fatal error! recorder[%d] start fail!", i+1);
            pRecorder->mbRecorderValid = FALSE;
        }
    }

//_destroy_vo:
    if (pContext->mVODev >= 0)
    {
        AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);
        AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
        AW_MPI_VO_Disable(pContext->mVODev);
        pContext->mUILayer = MM_INVALID_LAYER;
        pContext->mVODev = MM_INVALID_DEV;
    }
_stop_msgque:
    memset(&stCmdMsg, 0, sizeof(message_t));
    stCmdMsg.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stCmdMsg);
    int ret;
    pthread_join(pContext->mMsgQueueThreadId, (void*)&ret);
_mpp_exit:
    AW_MPI_SYS_Exit();
_destroy_recorders:
    deinitRecorder(pContext);
//_destroy_msgque:
    message_destroy(&pContext->mMsgQueue);
_destroy_context:
    cdx_sem_deinit(&pContext->mSemExit);
    if (pContext != NULL)
    {
        free(pContext);
        pContext = NULL;
        gpSampleRecorderContext = NULL;
    }
_exit:
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
