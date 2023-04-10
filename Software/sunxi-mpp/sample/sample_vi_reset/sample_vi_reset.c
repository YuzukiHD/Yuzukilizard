/******************************************************************************
  Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_virvi.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/1/5
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

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

#include <utils/plat_log.h>
#include "media/mpi_sys.h"
#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include <mpi_videoformat_conversion.h>

#include <confparser.h>
#include "sample_vi_reset.h"
#include "sample_vi_reset_config.h"

#define TEST_FLIP (0)
int hal_virvi_start(VI_DEV ViDev, VI_CHN ViCh, void *pAttr)
{
    ERRORTYPE ret = -1;

    ret = AW_MPI_VI_CreateVirChn(ViDev, ViCh, pAttr);
    if(ret < 0)
    {
        aloge("Create VI Chn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    ret = AW_MPI_VI_SetVirChnAttr(ViDev, ViCh, pAttr);
    if(ret < 0)
    {
        aloge("Set VI ChnAttr failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    ret = AW_MPI_VI_EnableVirChn(ViDev, ViCh);
    if(ret < 0)
    {
        aloge("VI Enable VirChn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }

    return 0;
}

int hal_virvi_end(VI_DEV ViDev, VI_CHN ViCh)
{
    int ret = -1;
    ret = AW_MPI_VI_DisableVirChn(ViDev, ViCh);
    if(ret < 0)
    {
        aloge("Disable VI Chn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    ret = AW_MPI_VI_DestroyVirChn(ViDev, ViCh);
    if(ret < 0)
    {
        aloge("Destory VI Chn failed,VIDev = %d,VIChn = %d",ViDev,ViCh);
        return ret ;
    }
    return 0;
}

static int parseCmdLine(SampleViResetContext *pContext, int argc, char** argv)
{
    int ret = -1;

    alogd("argc=%d", argc);

    if (argc != 3)
    {
        printf("CmdLine param:\n"
                "\t-path ./sample_CodecParallel.conf\n");
        return -1;
    }

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
                ret = 0;
                if (strlen(*argv) >= MAX_FILE_PATH_SIZE)
                {
                    aloge("fatal error! file path[%s] too long:!", *argv);
                }

                if (pContext)
                {
                    strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_SIZE-1);
                    pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
                }
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path ./sample_CodecParallel.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE LoadSampleViResetConfig(SampleViResetConfig *pConfig, const char *conf_path)
{
    if (NULL == pConfig)
    {
        aloge("pConfig is NULL!");
        return FAILURE;
    }

    if (NULL == conf_path)
    {
        aloge("user not set config file!");
        return FAILURE;
    }

    pConfig->mTestCount = 0;
    pConfig->mFrameCountStep1 = 300;
    pConfig->mbRunIsp = true;
    pConfig->mIspDev = 0;
    pConfig->mVippStart = 0;
    pConfig->mVippEnd = 0;
    pConfig->mPicWidth = 1920;
    pConfig->mPicHeight = 1080;
    pConfig->mSubPicWidth = 640;
    pConfig->mSubPicHeight = 360;
    pConfig->mFrameRate = 20;
    pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    CONFPARSER_S stConfParser;
    if(0 > createConfParser(conf_path, &stConfParser))
    {
        aloge("load conf fail!");
        return FAILURE;
    }
    pConfig->mTestCount = GetConfParaInt(&stConfParser, SAMPLE_VI_RESET_TEST_COUNT, 0);
    alogd("mTestCount=%d", pConfig->mTestCount);

    pConfig->mVippStart = GetConfParaInt(&stConfParser, SAMPLE_VI_RESET_VIPP_ID_START, 0);
    pConfig->mVippEnd = GetConfParaInt(&stConfParser, SAMPLE_VI_RESET_VIPP_ID_END, 0);

    alogd("vi config: vipp scope=[%d,%d], captureSize:[%dx%d,%dx%d] frameRate:%d, pixelFormat:0x%x",
       pConfig->mVippStart, pConfig->mVippEnd, pConfig->mPicWidth, pConfig->mPicHeight, 
       pConfig->mSubPicWidth, pConfig->mSubPicHeight, pConfig->mFrameRate, pConfig->mPicFormat);

    destroyConfParser(&stConfParser);

    return SUCCESS;
}

static void *GetCSIFrameThread(void *pArg)
{
    VI_DEV ViDev;
    VI_CHN ViCh;
    int ret = 0;
    int i = 0, j = 0;

    VirViChnInfo *pCap = (VirViChnInfo*)pArg;
    ViDev = pCap->mVipp;
    ViCh = pCap->mVirChn;
    alogd("Cap threadid=0x%lx, ViDev = %d, ViCh = %d", pCap->mThid, ViDev, ViCh);
    int nFlipValue = 0;
    int nFlipTestCnt = 0;
    while (pCap->mCaptureFrameCount > j)
    {
        if ((ret = AW_MPI_VI_GetFrame(ViDev, ViCh, &pCap->mFrameInfo, pCap->mMilliSec)) != 0)
        {
            alogw("Vipp[%d,%d] Get Frame failed!", ViDev, ViCh);
            continue;
        }
        i++;
        if (i % 20 == 0)
        {
            time_t now;
            struct tm *timenow;
            time(&now);
            timenow = localtime(&now);
            alogd("Cap threadid=0x%lx, ViDev=%d, VirVi=%d, mpts=%lld; local time is %s", pCap->mThid, ViDev, ViCh, pCap->mFrameInfo.VFrame.mpts, asctime(timenow));
#if 0
            FILE *fd;
            char filename[128];
            sprintf(filename, "/tmp/%dx%d_%d.yuv",
                pCap->pstFrameInfo.VFrame.mWidth,
                pCap->pstFrameInfo.VFrame.mHeight,
                i);
            fd = fopen(filename, "wb+");
            fwrite(pCap->pstFrameInfo.VFrame.mpVirAddr[0],
                    pCap->pstFrameInfo.VFrame.mWidth * pCap->pstFrameInfo.VFrame.mHeight,
                    1, fd);
            fwrite(pCap->pstFrameInfo.VFrame.mpVirAddr[1],
                    pCap->pstFrameInfo.VFrame.mWidth * pCap->pstFrameInfo.VFrame.mHeight >> 1,
                    1, fd);
            fclose(fd);
#endif
        }
        AW_MPI_VI_ReleaseFrame(ViDev, ViCh, &pCap->mFrameInfo);
        j++;
#if TEST_FLIP
        if(j == pCap->mCaptureFrameCount)
        {
            j = 0;
            if(0 == ViDev)
            {
                nFlipValue = !nFlipValue;
                nFlipTestCnt++;
                AW_MPI_VI_SetVippMirror(ViDev, nFlipValue);
                AW_MPI_VI_SetVippFlip(ViDev, nFlipValue);
                alogd("vipp[%d] mirror and flip:%d, test count:%d begin.", ViDev, nFlipValue, nFlipTestCnt);
            }
        }
#endif
    }
    return NULL;
}
int RunOneVipp(VI_DEV nVippIndex, SampleViResetContext *pContext)
{
    ERRORTYPE ret = SUCCESS;
    /*Set VI Channel Attribute*/
    memset(&pContext->mViAttr, 0, sizeof(VI_ATTR_S));
    pContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mPicFormat);
    pContext->mViAttr.format.field = V4L2_FIELD_NONE;
    alogd("nVippIndex=%d", nVippIndex);
    if(0 == nVippIndex)
    {
        pContext->mViAttr.format.width = pContext->mConfigPara.mPicWidth;
        pContext->mViAttr.format.height = pContext->mConfigPara.mPicHeight;
    }
    else
    {
        pContext->mViAttr.format.width = pContext->mConfigPara.mSubPicWidth;
        pContext->mViAttr.format.height = pContext->mConfigPara.mSubPicHeight;
    }
    pContext->mViAttr.nbufs = 3;//5;
    pContext->mViAttr.nplanes = 2;
    pContext->mViAttr.fps = pContext->mConfigPara.mFrameRate;
    /* update configuration anyway, do not use current configuration */
    if(false == pContext->mFirstVippRunFlag)
    {
        pContext->mViAttr.use_current_win = 0;
        pContext->mFirstVippRunFlag = true;
    }
    else
    {
        pContext->mViAttr.use_current_win = 1;
    }
    pContext->mViAttr.wdr_mode = 0;
    pContext->mViAttr.capturemode = V4L2_MODE_VIDEO;
    pContext->mViAttr.drop_frame_num = 0;
    ret = AW_MPI_VI_CreateVipp(nVippIndex);
    if(ret != SUCCESS)
    {
        aloge("fatal error! create vipp[%d] fail[0x%x]", nVippIndex, ret);
    }
    ret = AW_MPI_VI_SetVippAttr(nVippIndex, &pContext->mViAttr);
    if(ret != SUCCESS)
    {
        aloge("fatal error! set vipp attr[%d] fail[0x%x]", nVippIndex, ret);
    }
    ret = AW_MPI_VI_EnableVipp(nVippIndex);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enable vipp[%d] fail[0x%x]", nVippIndex, ret);
    }
    if(pContext->mConfigPara.mbRunIsp && false == pContext->mbIspRunningFlag)
    {
        ret = AW_MPI_ISP_Run(pContext->mConfigPara.mIspDev);
        if(ret != SUCCESS)
        {
            aloge("fatal error! isp[%d] run fail[0x%x]", pContext->mConfigPara.mIspDev, ret);
        }
        pContext->mbIspRunningFlag = true;
    }
    VI_CHN virvi_chn = 0;
    for (virvi_chn = 0; virvi_chn < 1 /*MAX_VIR_CHN_NUM*/; virvi_chn++)
    {
        VirViChnInfo *pChnInfo = &pContext->mVirViChnArray[nVippIndex][virvi_chn];
        memset(pChnInfo, 0, sizeof(VirViChnInfo));
        pChnInfo->mVipp = nVippIndex;
        pChnInfo->mVirChn = virvi_chn;
        pChnInfo->mMilliSec = 5000;  // 2000;
        pChnInfo->mCaptureFrameCount = pContext->mConfigPara.mFrameCountStep1;
        ret = hal_virvi_start(nVippIndex, virvi_chn, NULL);
        if(ret != 0)
        {
            aloge("virvi start failed!");
        }
        pChnInfo->mThid = 0;
        ret = pthread_create(&pChnInfo->mThid, NULL, GetCSIFrameThread, (void *)pChnInfo);
        if(0 == ret)
        {
            alogd("pthread create success, vipp[%d], virChn[%d]", pChnInfo->mVipp, pChnInfo->mVirChn);
        }
        else
        {
            aloge("fatal error! pthread_create failed, vipp[%d], virChn[%d]", pChnInfo->mVipp, pChnInfo->mVirChn);
        }
    }
    return ret;
}

int DestroyOneVipp(VI_DEV nVippIndex, SampleViResetContext *pContext)
{
    int ret = 0;
    VI_CHN virvi_chn = 0;
    for (virvi_chn = 0; virvi_chn < 1; virvi_chn++)
    {
        ret = hal_virvi_end(nVippIndex, virvi_chn);
        if(ret != 0)
        {
            aloge("fatal error! virvi[%d,%d] end failed!", nVippIndex, virvi_chn);
        }
    }
    ret = AW_MPI_VI_DisableVipp(nVippIndex);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disable vipp[%d] fail", nVippIndex);
    }
    ret = AW_MPI_VI_DestroyVipp(nVippIndex);
    if(ret != SUCCESS)
    {
        aloge("fatal error! destroy vipp[%d] fail", nVippIndex);
    }
    alogd("vipp[%d] is destroyed!", nVippIndex);
    return ret;
}

int main(int argc, char *argv[])
{
    int result = 0;
    ERRORTYPE ret;
    int virvi_chn;

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
    
	alogd("app:[%s] begin! time=%s, %s", argv[0], __DATE__, __TIME__);

    SampleViResetContext *pContext = (SampleViResetContext*)malloc(sizeof(SampleViResetContext));
    if(NULL == pContext)
    {
        aloge("fatal error! malloc fail!");
        return -1;
    }
    memset(pContext, 0, sizeof(SampleViResetContext));

    /* parse command line param */
    char *pConfigFilePath = NULL;
    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("fatal error! parse cmd line fail");
        result = -1;
        goto _exit;
    }
    pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;

    /* parse config file. */
    if(LoadSampleViResetConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    MPP_SYS_CONF_S stSysConf;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret != SUCCESS)
    {
        aloge("sys Init failed!");
        goto _exit;
    }

    while (pContext->mTestNum < pContext->mConfigPara.mTestCount || 0 == pContext->mConfigPara.mTestCount)
    {
        alogd("======================================");
        alogd("Auto Test count : %d. (MaxCount==%d)", pContext->mTestNum, pContext->mConfigPara.mTestCount);
        //system("cat /proc/meminfo | grep Committed_AS");
        alogd("======================================");
        VI_DEV nVippIndex;
        for(nVippIndex = pContext->mConfigPara.mVippStart; nVippIndex <= pContext->mConfigPara.mVippEnd; nVippIndex++)
        {
            RunOneVipp(HVIDEO(nVippIndex, 0), pContext);
        }
        
        for(nVippIndex = pContext->mConfigPara.mVippStart; nVippIndex <= pContext->mConfigPara.mVippEnd; nVippIndex++)
        {
            for (virvi_chn = 0; virvi_chn < 1; virvi_chn++)
            {
                pthread_join(pContext->mVirViChnArray[HVIDEO(nVippIndex, 0)][virvi_chn].mThid, NULL);
                alogd("vipp[%d]virChn[%d] capture thread is exit!", HVIDEO(nVippIndex, 0), virvi_chn);
            }
        }
        for(nVippIndex = pContext->mConfigPara.mVippStart; nVippIndex <= pContext->mConfigPara.mVippEnd; nVippIndex++)
        {
            DestroyOneVipp(HVIDEO(nVippIndex, 0), pContext);
        }
        if(pContext->mbIspRunningFlag)
        {
            AW_MPI_ISP_Stop(pContext->mConfigPara.mIspDev);
            alogd("isp[%d] is stopped!", pContext->mConfigPara.mIspDev);
            pContext->mbIspRunningFlag = false;
        }
        pContext->mTestNum++;
        pContext->mFirstVippRunFlag = false;
        alogd("[%d] test time is done!", pContext->mTestNum);
    }

_exit:
    free(pContext);
    alogd("%s test result: %s", argv[0], ((0==result) ? "success" : "fail"));
    return result;
}
