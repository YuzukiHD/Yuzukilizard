/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_demux.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   : sample_demux module
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleDemux"

#include <unistd.h>
#include <fcntl.h>
#include "plat_log.h"
#include <time.h>
#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_demux.h>

#include "sample_demux.h"
#include "sample_demux_config.h"

#define DEMUX_DEFAULT_SRC_FILE   "/mnt/extsd/two_video.mp4"
#define DEMUX_DEFAULT_DST_VIDEO_FILE      "/mnt/extsd/video.bin"
#define DEMUX_DEFAULT_DST_AUDIO_FILE      "/mnt/extsd/audio.bin"
#define DEMUX_DEFAULT_DST_SUBTITLE_FILE   "/mnt/extsd/subtitle.bin"

static int parseCmdLine(SampleDemuxContext *pDemuxData, int argc, char** argv)
{
    int ret = -1;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = 0;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pDemuxData->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pDemuxData->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_demux.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SampleDemuxContext *pDemuxData, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConf;

    strcpy(pDemuxData->mConfigPara.mSrcFile, DEMUX_DEFAULT_SRC_FILE);
    strcpy(pDemuxData->mConfigPara.mDstVideoFile, DEMUX_DEFAULT_DST_VIDEO_FILE);
    strcpy(pDemuxData->mConfigPara.mDstAudioFile, DEMUX_DEFAULT_DST_AUDIO_FILE);
    strcpy(pDemuxData->mConfigPara.mDstSubtileFile, DEMUX_DEFAULT_DST_SUBTITLE_FILE);
    pDemuxData->mConfigPara.mSelectVideoIndex = 0;
    pDemuxData->mConfigPara.mSeekTime = 0;
    pDemuxData->mConfigPara.mTestDuration = 0;

    if(NULL == conf_path)
    {
        alogd("user do not set config file. use default test parameter!");
        return SUCCESS;
    }
    ret = createConfParser(conf_path, &stConf);
    if (ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    ptr = (char *)GetConfParaString(&stConf, DEMUX_CFG_SRC_FILE_STR, NULL);
    strcpy(pDemuxData->mConfigPara.mSrcFile, ptr);
    ptr = (char *)GetConfParaString(&stConf, DEMUX_CFG_DST_VIDEO_FILE_STR, NULL);
    strcpy(pDemuxData->mConfigPara.mDstVideoFile, ptr);
    ptr = (char *)GetConfParaString(&stConf, DEMUX_CFG_DST_AUDIO_FILE_STR, NULL);
    strcpy(pDemuxData->mConfigPara.mDstAudioFile, ptr);
    ptr = (char *)GetConfParaString(&stConf, DEMUX_CFG_DST_SUBTITLE_FILE_STR, NULL);
    strcpy(pDemuxData->mConfigPara.mDstSubtileFile, ptr);
    pDemuxData->mConfigPara.mSelectVideoIndex = GetConfParaInt(&stConf, DEMUX_CFG_SELECT_VIDEO_INDEX, 0);
    pDemuxData->mConfigPara.mSeekTime = GetConfParaInt(&stConf, DEMUX_CFG_SEEK_POSITION, 0);
    pDemuxData->mConfigPara.mTestDuration = GetConfParaInt(&stConf, DEMUX_CFG_TEST_DURATION, 0);

    destroyConfParser(&stConf);
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleDemuxContext *pDemuxData = (SampleDemuxContext*)cookie;

    switch(event)
    {
    case MPP_EVENT_NOTIFY_EOF:
        alogd("get demux eof message");
        cdx_sem_up(&pDemuxData->mSemExit);
        break;
    default:
        alogd("get demux other message[0x%x]", event);
        break;
    }

    return SUCCESS;
}

static void *getDmxBufThread(void *pThreadData)
{
    ERRORTYPE ret;
    int cmd;
    unsigned int cmddata;
    EncodedStream demuxOutBuf;
    SampleDemuxContext *pDemuxData = (SampleDemuxContext*)pThreadData;

    lseek(pDemuxData->mDstVideoFd, 0, SEEK_SET);
    lseek(pDemuxData->mDstAudioFd, 0, SEEK_SET);
    lseek(pDemuxData->mDstSubFd, 0, SEEK_SET);

    while (!pDemuxData->mOverFlag)
    {
        if ((ret=AW_MPI_DEMUX_getDmxOutPutBuf(pDemuxData->mDmxChn, &demuxOutBuf, 100)) == SUCCESS)
        {
            if (demuxOutBuf.media_type == CDX_PacketVideo)
            {
               //alogd("write video");
               write(pDemuxData->mDstVideoFd, demuxOutBuf.pBuffer, demuxOutBuf.nFilledLen);
            }
            else if (demuxOutBuf.media_type == CDX_PacketAudio)
            {
               //alogd("write audio");
               write(pDemuxData->mDstAudioFd, demuxOutBuf.pBuffer, demuxOutBuf.nFilledLen);
            }
            else if (demuxOutBuf.media_type == CDX_PacketSubtitle)
            {
               //alogd("write subtitle");
               write(pDemuxData->mDstSubFd, demuxOutBuf.pBuffer, demuxOutBuf.nFilledLen);
            }
            ret = AW_MPI_DEMUX_releaseDmxBuf(pDemuxData->mDmxChn, &demuxOutBuf);
            if (ret != SUCCESS)
            {
                aloge("dmxoutbuf get, but can not release, maybe something wrong in middleware");
            }
        }
        else
        {
            alogd("get dmx buf fail[0x%x]", ret);
        }
    }

    alogd("DmxBufThread exit!");
    return NULL;
}

SampleDemuxContext *constructSampleDemuxContext()
{
    int ret;
    SampleDemuxContext *pThiz = (SampleDemuxContext*)malloc(sizeof(SampleDemuxContext));
    if(NULL == pThiz)
    {
        aloge("fatal error! malloc fail!");
        return NULL;
    }
    memset(pThiz, 0, sizeof(SampleDemuxContext));
    pThiz->mSrcFd = -1;
    pThiz->mDstVideoFd = -1;
    pThiz->mDstAudioFd = -1;
    pThiz->mDstSubFd = -1;
    ret = cdx_sem_init(&pThiz->mSemExit, 0);
    if(ret != 0)
    {
        aloge("fatal error! cdx sem init fail[%d]", ret);
    }
    return pThiz;
}

void destructSampleDemuxContext(SampleDemuxContext *pThiz)
{
    if(pThiz)
    {
        if(pThiz->mSrcFd >= 0)
        {
            close(pThiz->mSrcFd);
            pThiz->mSrcFd = -1;
        }
        if(pThiz->mDstVideoFd >= 0)
        {
            close(pThiz->mDstVideoFd);
            pThiz->mDstVideoFd = -1;
        }
        if(pThiz->mDstAudioFd >= 0)
        {
            close(pThiz->mDstAudioFd);
            pThiz->mDstAudioFd = -1;
        }
        if(pThiz->mDstSubFd >= 0)
        {
            close(pThiz->mDstSubFd);
            pThiz->mDstSubFd = -1;
        }
        cdx_sem_deinit(&pThiz->mSemExit);
        free(pThiz);
    }
}

int main(int argc, char** argv)
{
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

    int result = 0;
    ERRORTYPE ret = SUCCESS;
    

    SampleDemuxContext *pDemuxData = constructSampleDemuxContext();
    if(NULL == pDemuxData)
    {
        aloge("fatal error! malloc fail");
        return -1;
    }

    if (parseCmdLine(pDemuxData, argc, argv) != 0)
    {
        goto err_out_0;
    }

    if (loadConfigPara(pDemuxData, pDemuxData->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        goto err_out_0;
    }

    pDemuxData->mSrcFd = open(pDemuxData->mConfigPara.mSrcFile, O_RDONLY);
    if (pDemuxData->mSrcFd < 0)
    {
        aloge("ERROR: cannot open video src file");
        goto err_out_0;
    }

    pDemuxData->mDstVideoFd = open(pDemuxData->mConfigPara.mDstVideoFile, O_CREAT|O_RDWR);
    if (pDemuxData->mDstVideoFd < 0)
    {
        aloge("ERROR: cannot create video dst file");
        goto err_out_1;
    }

    pDemuxData->mDstAudioFd = open(pDemuxData->mConfigPara.mDstAudioFile, O_CREAT|O_RDWR);
    if (pDemuxData->mDstAudioFd < 0)
    {
        aloge("ERROR: cannot create audio dst file");
        goto err_out_2;
    }

    pDemuxData->mDstSubFd = open(pDemuxData->mConfigPara.mDstSubtileFile, O_CREAT|O_RDWR);
    if (pDemuxData->mDstSubFd < 0)
    {
        aloge("ERROR: cannot create subtitle dst file");
        goto err_out_3;
    }

    pDemuxData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pDemuxData->mSysConf);
    AW_MPI_SYS_Init();

    //we do not need subtitle
    pDemuxData->mTrackDisFlag = DEMUX_DISABLE_SUBTITLE_TRACK;

    pDemuxData->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pDemuxData->mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pDemuxData->mDmxChnAttr.mSourceUrl = NULL;
    pDemuxData->mDmxChnAttr.mFd = pDemuxData->mSrcFd;
    pDemuxData->mDmxChnAttr.mDemuxDisableTrack = pDemuxData->mTrackDisFlag;

    pDemuxData->mDmxChn = 0;
    BOOL bSuccessFlag = FALSE;
    while (pDemuxData->mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(pDemuxData->mDmxChn, &pDemuxData->mDmxChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", pDemuxData->mDmxChn);
            break;
        }
        else if (ERR_DEMUX_EXIST == ret)
        {
            //alogd("demux channel[%d] is exist, find next!", pDemuxData->mDmxChn);
            pDemuxData->mDmxChn++;
        }
        else
        {
            aloge("fatal error! create demux channel[%d] ret[0x%x]!", pDemuxData->mDmxChn, ret);
            break;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pDemuxData->mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        goto err_out_4;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pDemuxData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_DEMUX_RegisterCallback(pDemuxData->mDmxChn, &cbInfo);
    }

    ret = AW_MPI_DEMUX_GetMediaInfo(pDemuxData->mDmxChn, &pDemuxData->mMediaInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! get media info fail[0x%x]!", ret);
        goto err_out_5;
    }
    alogd("default select v[%d-%d],a[%d-%d],s[%d-%d]", pDemuxData->mMediaInfo.mVideoIndex, pDemuxData->mMediaInfo.mVideoNum, 
        pDemuxData->mMediaInfo.mAudioIndex, pDemuxData->mMediaInfo.mAudioNum, 
        pDemuxData->mMediaInfo.mSubtitleIndex, pDemuxData->mMediaInfo.mSubtitleNum);
    ret = AW_MPI_DEMUX_SelectVideoStream(pDemuxData->mDmxChn, pDemuxData->mConfigPara.mSelectVideoIndex);
    if(ret != SUCCESS)
    {
        aloge("fatal error! select video stream[%d] fail[0x%x].", pDemuxData->mConfigPara.mSelectVideoIndex, ret);
    }
    ret = AW_MPI_DEMUX_GetMediaInfo(pDemuxData->mDmxChn, &pDemuxData->mMediaInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! get media info fail[0x%x]!", ret);
        goto err_out_5;
    }
    alogd("select done! v[%d-%d],a[%d-%d],s[%d-%d]", pDemuxData->mMediaInfo.mVideoIndex, pDemuxData->mMediaInfo.mVideoNum, 
        pDemuxData->mMediaInfo.mAudioIndex, pDemuxData->mMediaInfo.mAudioNum, 
        pDemuxData->mMediaInfo.mSubtitleIndex, pDemuxData->mMediaInfo.mSubtitleNum);

    if(pDemuxData->mConfigPara.mSeekTime > 0)
    {
        ret = AW_MPI_DEMUX_Seek(pDemuxData->mDmxChn, pDemuxData->mConfigPara.mSeekTime);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why seek[%d]ms fail?", pDemuxData->mConfigPara.mSeekTime);
        }
    }

    ret = AW_MPI_DEMUX_Start(pDemuxData->mDmxChn);
    if(ret != SUCCESS)
    {
        aloge("start play fail");
        goto err_out_5;
    }

    pDemuxData->mOverFlag = FALSE;
    result = pthread_create(&pDemuxData->mThdId, NULL, getDmxBufThread, pDemuxData);
    if (result != 0)
    {
        aloge("fatal error! create thread fail[%d]", result);
        goto err_out_6;
    }

    if (pDemuxData->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pDemuxData->mSemExit, pDemuxData->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pDemuxData->mSemExit);
    }

    pDemuxData->mOverFlag = TRUE;

err_out_7:
    pthread_join(pDemuxData->mThdId, NULL);
err_out_6:
    ret = AW_MPI_DEMUX_Stop(pDemuxData->mDmxChn);
    if (ret != SUCCESS)
    {
        alogw("demux stop fail[0x%x]", ret);
    }
err_out_5:
    AW_MPI_DEMUX_DestroyChn(pDemuxData->mDmxChn);
err_out_4:
    //exit mpp system
    AW_MPI_SYS_Exit();
err_out_3:
err_out_2:
err_out_1:
err_out_0:
    destructSampleDemuxContext(pDemuxData);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
