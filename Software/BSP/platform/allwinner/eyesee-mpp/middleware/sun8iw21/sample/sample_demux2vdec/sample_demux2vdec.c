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
#define LOG_TAG "SampleDemux2Vdec"

#include <unistd.h>
#include <fcntl.h>
#include "plat_log.h"
#include <time.h>

#include <mpi_demux.h>
#include <mpi_vdec.h>
#include <mpi_clock.h>

#include "sample_demux2vdec.h"
#include "sample_demux2vdec_common.h"

#define DMX2VDEC_DEFAULT_SRC_FILE   "/mnt/extsd/sample_demux2vdec/test.mp4"
#define DMX2VDEC_DEFAULT_SRC_SIZE   1080
#define DMX2VDEC_DEFAULT_DST_Y_FILE      "/mnt/extsd/sample_demux2vdec/y.yuv"
#define DMX2VDEC_DEFAULT_DST_U_FILE      "/mnt/extsd/sample_demux2vdec/u.yuv"
#define DMX2VDEC_DEFAULT_DST_V_FILE      "/mnt/extsd/sample_demux2vdec/v.yuv"
#define DMX2VDEC_DEFAULT_DST_YUV_FILE    "/mnt/extsd/sample_demux2vdec/yuv.yuv"


static SAMPLE_DEMUX2VDEC_S *pDemux2VdecData;


static ERRORTYPE InitDemux2VdecData(void)
{
    pDemux2VdecData = (SAMPLE_DEMUX2VDEC_S* )malloc(sizeof(SAMPLE_DEMUX2VDEC_S));
    if (pDemux2VdecData == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }

    memset(pDemux2VdecData, 0, sizeof(SAMPLE_DEMUX2VDEC_S));

    pDemux2VdecData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    //pDemux2VdecData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    strcpy(pDemux2VdecData->mConfigPara.srcFile, DMX2VDEC_DEFAULT_SRC_FILE);
    strcpy(pDemux2VdecData->mConfigPara.dstYFile, DMX2VDEC_DEFAULT_DST_Y_FILE);
    strcpy(pDemux2VdecData->mConfigPara.dstUFile, DMX2VDEC_DEFAULT_DST_U_FILE);
    strcpy(pDemux2VdecData->mConfigPara.dstVFile, DMX2VDEC_DEFAULT_DST_V_FILE);
    strcpy(pDemux2VdecData->mConfigPara.dstYUVFile, DMX2VDEC_DEFAULT_DST_YUV_FILE);

    pDemux2VdecData->mDmxChn = MM_INVALID_CHN;
    pDemux2VdecData->mVdecChn = MM_INVALID_CHN;
    pDemux2VdecData->mClockChn = MM_INVALID_CHN;

    return SUCCESS;
}

static int parseCmdLine(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData, int argc, char** argv)
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

              strncpy(pDemux2VdecData->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pDemux2VdecData->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_demux2vdec.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S mConf;

    ret = createConfParser(conf_path, &mConf);
    if (ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    ptr = (char *)GetConfParaString(&mConf, DMX2VDEC_CFG_SRC_FILE_STR, NULL);
    strcpy(pDemux2VdecData->mConfigPara.srcFile, ptr);

    pDemux2VdecData->mConfigPara.seekTime = GetConfParaInt(&mConf, DMX2VDEC_CFG_SEEK_POSITION, 0);

    ptr = (char *)GetConfParaString(&mConf, DMX2VDEC_CFG_DST_Y_FILE_STR, NULL);
    strcpy(pDemux2VdecData->mConfigPara.dstYFile, ptr);

    ptr = (char *)GetConfParaString(&mConf, DMX2VDEC_CFG_DST_U_FILE_STR, NULL);
    strcpy(pDemux2VdecData->mConfigPara.dstUFile, ptr);

    ptr = (char *)GetConfParaString(&mConf, DMX2VDEC_CFG_DST_V_FILE_STR, NULL);
    strcpy(pDemux2VdecData->mConfigPara.dstVFile, ptr);

    ptr = (char *)GetConfParaString(&mConf, DMX2VDEC_CFG_DST_YUV_FILE_STR, NULL);
    strcpy(pDemux2VdecData->mConfigPara.dstYUVFile, ptr);

    pDemux2VdecData->mConfigPara.mTestDuration = GetConfParaInt(&mConf, DMX2VDEC_CFG_TEST_DURATION, 0);

    alogd("src_file:%s, seek_time:%d, dst_yuv=%s, test_time=%d", pDemux2VdecData->mConfigPara.srcFile,\
        pDemux2VdecData->mConfigPara.seekTime, pDemux2VdecData->mConfigPara.dstYUVFile,\
        pDemux2VdecData->mConfigPara.mTestDuration);


    destroyConfParser(&mConf);
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_DEMUX2VDEC_S *pDemux2VdecData = (SAMPLE_DEMUX2VDEC_S *)cookie;

    if (pChn->mModId == MOD_ID_DEMUX)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("demux to end of file");
            if (pDemux2VdecData->mVdecChn >= 0)
            {
                AW_MPI_VDEC_SetStreamEof(pDemux2VdecData->mVdecChn, 1);
            }
            //cdx_sem_up(&pDemux2VdecData->mSemExit);
            break;

        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_VDEC)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vdec get the endof file");
            cdx_sem_up(&pDemux2VdecData->mSemExit);
            break;

        default:
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE createClockChn(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;

    pDemux2VdecData->mClockChn = 0;
    while (pDemux2VdecData->mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(pDemux2VdecData->mClockChn, &pDemux2VdecData->mClockChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", pDemux2VdecData->mClockChn);
            break;
        }
        else if (ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", pDemux2VdecData->mClockChn);
            pDemux2VdecData->mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", pDemux2VdecData->mClockChn, ret);
            break;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pDemux2VdecData->mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
        return FAILURE;
    }
    else
    {
        return SUCCESS;
    }
}

static ERRORTYPE configDmxChnAttr(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    pDemux2VdecData->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pDemux2VdecData->mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pDemux2VdecData->mDmxChnAttr.mSourceUrl = NULL;
    pDemux2VdecData->mDmxChnAttr.mFd = pDemux2VdecData->mConfigPara.srcFd;
    pDemux2VdecData->mDmxChnAttr.mDemuxDisableTrack = pDemux2VdecData->mTrackDisFlag = DEMUX_DISABLE_SUBTITLE_TRACK|DEMUX_DISABLE_AUDIO_TRACK;

    return SUCCESS;
}

static ERRORTYPE createDemuxChn(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configDmxChnAttr(pDemux2VdecData);

    pDemux2VdecData->mDmxChn = 0;
    while (pDemux2VdecData->mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(pDemux2VdecData->mDmxChn, &pDemux2VdecData->mDmxChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", pDemux2VdecData->mDmxChn);
            break;
        }
        else if (ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, find next!", pDemux2VdecData->mDmxChn);
            pDemux2VdecData->mDmxChn++;
        }
        else
        {
            alogd("create demux channel[%d] ret[0x%x]!", pDemux2VdecData->mDmxChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pDemux2VdecData->mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pDemux2VdecData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_DEMUX_RegisterCallback(pDemux2VdecData->mDmxChn, &cbInfo);
        return SUCCESS;
    }
}

static ERRORTYPE ConfigVdecChnAttr(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData, DEMUX_VIDEO_STREAM_INFO_S *pStreamInfo)
{
    memset(&pDemux2VdecData->mVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    pDemux2VdecData->mVdecChnAttr.mPicWidth = pDemux2VdecData->mConfigPara.mMaxVdecOutputWidth;
    pDemux2VdecData->mVdecChnAttr.mPicHeight = pDemux2VdecData->mConfigPara.mMaxVdecOutputHeight;
    pDemux2VdecData->mVdecChnAttr.mInitRotation = pDemux2VdecData->mConfigPara.mInitRotation;
    pDemux2VdecData->mVdecChnAttr.mOutputPixelFormat = pDemux2VdecData->mConfigPara.mUserSetPixelFormat;
    pDemux2VdecData->mVdecChnAttr.mType = pStreamInfo->mCodecType;
    pDemux2VdecData->mVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1; //1
    pDemux2VdecData->mVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;

    return SUCCESS;
}

static ERRORTYPE createVdecChn(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    if ((pDemux2VdecData->mDemuxMediaInfo.mVideoNum >0 ) && !(pDemux2VdecData->mDmxChnAttr.mDemuxDisableTrack & DEMUX_DISABLE_VIDEO_TRACK))
    {
        ConfigVdecChnAttr(pDemux2VdecData, &pDemux2VdecData->mDemuxMediaInfo.mVideoStreamInfo[pDemux2VdecData->mDemuxMediaInfo.mVideoIndex]);
        pDemux2VdecData->mVdecChn = 0;
        while (pDemux2VdecData->mVdecChn < VDEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VDEC_CreateChn(pDemux2VdecData->mVdecChn, &pDemux2VdecData->mVdecChnAttr);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create vdec channel[%d] success!", pDemux2VdecData->mVdecChn);
                break;
            }
            else if (ERR_VDEC_EXIST == ret)
            {
                alogd("vdec channel[%d] is exist, find next!", pDemux2VdecData->mVdecChn);
                pDemux2VdecData->mVdecChn++;
            }
            else
            {
                alogd("create vdec channel[%d] ret[0x%x]!", pDemux2VdecData->mVdecChn, ret);
                break;
            }
        }

        if (FALSE == nSuccessFlag)
        {
            pDemux2VdecData->mVdecChn = MM_INVALID_CHN;
            aloge("fatal error! create vdec channel fail!");
            return FAILURE;
        }
        else
        {
            alogd("add call back");
            MPPCallbackInfo cbInfo;
            cbInfo.cookie = (void*)pDemux2VdecData;
            cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
            AW_MPI_VDEC_RegisterCallback(pDemux2VdecData->mVdecChn, &cbInfo);
            return SUCCESS;
        }
    }
    else
    {
        return FAILURE;
    }
}

static ERRORTYPE prepare(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;
    DEMUX_MEDIA_INFO_S DemuxMediaInfo = {0};

    if (createDemuxChn(pDemux2VdecData) != SUCCESS)
    {
        aloge("create demux chn fail");
        return FAILURE;
    }

    alogd("prepare");
    if (AW_MPI_DEMUX_GetMediaInfo(pDemux2VdecData->mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return FAILURE;
    }

    if ((DemuxMediaInfo.mVideoNum >0 && DemuxMediaInfo.mVideoIndex >= DemuxMediaInfo.mVideoNum)
        || (DemuxMediaInfo.mAudioNum >0 && DemuxMediaInfo.mAudioIndex >= DemuxMediaInfo.mAudioNum)
        || (DemuxMediaInfo.mSubtitleNum >0 && DemuxMediaInfo.mSubtitleIndex >= DemuxMediaInfo.mSubtitleNum))
    {
        alogd("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
               DemuxMediaInfo.mVideoNum, DemuxMediaInfo.mVideoIndex, DemuxMediaInfo.mAudioNum, DemuxMediaInfo.mAudioIndex, DemuxMediaInfo.mSubtitleNum, DemuxMediaInfo.mSubtitleIndex);
        return FAILURE;
    }

    memcpy(&pDemux2VdecData->mDemuxMediaInfo, &DemuxMediaInfo, sizeof(DEMUX_MEDIA_INFO_S));

    if (DemuxMediaInfo.mVideoNum >0)
    {
        ret = createVdecChn(pDemux2VdecData);
        if (ret == SUCCESS)
        {
            alogd("bind demux & vdec");
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pDemux2VdecData->mDmxChn};
            MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pDemux2VdecData->mVdecChn};
            AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
        }
        else
        {
            alogd("create vdec chn fail");
            return FAILURE;
        }

        ret = createClockChn(pDemux2VdecData);
        if (ret == SUCCESS)
        {
            alogd("bind clock & demux");
            MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pDemux2VdecData->mClockChn};
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pDemux2VdecData->mDmxChn};
            AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
        }
        return ret;
    }
    else
    {
        return FAILURE;
    }
}

static int saveVdecOutFrame(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;
    int size, width, height;
    VIDEO_FRAME_INFO_S mVideoFrameInfo;

    while (SUCCESS == AW_MPI_VDEC_GetImage(pDemux2VdecData->mVdecChn, &mVideoFrameInfo, 0))
    {
        //mVideoFrameInfo.VFrame.mWidth,mVideoFrameInfo.VFrame.mHeight is the actual use buffer size (1920*1088 (32byte align))
        // width = mVideoFrameInfo.VFrame.mOffsetRight - mVideoFrameInfo.VFrame.mOffsetLeft;
        // height = mVideoFrameInfo.VFrame.mOffsetBottom - mVideoFrameInfo.VFrame.mOffsetTop;
        width = mVideoFrameInfo.VFrame.mWidth;//mVideoFrameInfo.VFrame.mOffsetRight - mVideoFrameInfo.VFrame.mOffsetLeft;
        height =mVideoFrameInfo.VFrame.mHeight;// mVideoFrameInfo.VFrame.mOffsetBottom - mVideoFrameInfo.VFrame.mOffsetTop;        //alogd("write file,w=%d,h=%d,pix_format=%d", width, height,mVideoFrameInfo.VFrame.mPixelFormat);
        size = width * height;
        alogv("mPixelFormat=%d width=%d height=%d", mVideoFrameInfo.VFrame.mPixelFormat, width, height);

        write(pDemux2VdecData->mConfigPara.dstYFd, mVideoFrameInfo.VFrame.mpVirAddr[0], size);

        write(pDemux2VdecData->mConfigPara.dstYUVFd, mVideoFrameInfo.VFrame.mpVirAddr[0], size);

        if (MM_PIXEL_FORMAT_YVU_PLANAR_420 == mVideoFrameInfo.VFrame.mPixelFormat)
        {
#if 0
            size /= 4;
            write(pDemux2VdecData->mConfigPara.dstVFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);
            write(pDemux2VdecData->mConfigPara.dstUFd, mVideoFrameInfo.VFrame.mpVirAddr[2], size);

            write(pDemux2VdecData->mConfigPara.dstYUVFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);
            write(pDemux2VdecData->mConfigPara.dstYUVFd, mVideoFrameInfo.VFrame.mpVirAddr[2], size);
#else
            size /= 2;
            write(pDemux2VdecData->mConfigPara.dstVFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);

            write(pDemux2VdecData->mConfigPara.dstYUVFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);
#endif
        }
        else if (MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == mVideoFrameInfo.VFrame.mPixelFormat)
        {
            size /= 2;
            write(pDemux2VdecData->mConfigPara.dstVFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);

            write(pDemux2VdecData->mConfigPara.dstYUVFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);
        }
        else if (MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == mVideoFrameInfo.VFrame.mPixelFormat)
        {
            size /= 2;
            write(pDemux2VdecData->mConfigPara.dstUFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);

            write(pDemux2VdecData->mConfigPara.dstYUVFd, mVideoFrameInfo.VFrame.mpVirAddr[1], size);
        }

        ret = AW_MPI_VDEC_ReleaseImage(pDemux2VdecData->mVdecChn, &mVideoFrameInfo);
        if (ret != SUCCESS)
        {
            aloge("image get, but release fail");
        }
    }

    return 0;
}

static void *getVdecFrameThread(void *pThreadData)
{
    int cmd;
    SAMPLE_DEMUX2VDEC_S *pDemux2VdecData = (SAMPLE_DEMUX2VDEC_S *)pThreadData;

    lseek(pDemux2VdecData->mConfigPara.dstYFd, 0, SEEK_SET);
    lseek(pDemux2VdecData->mConfigPara.dstUFd, 0, SEEK_SET);
    lseek(pDemux2VdecData->mConfigPara.dstVFd, 0, SEEK_SET);
    lseek(pDemux2VdecData->mConfigPara.dstYUVFd, 0, SEEK_SET);

    do {
        saveVdecOutFrame(pDemux2VdecData);
    }while (!pDemux2VdecData->mOverFlag);

    /* in case when stop vdec chn,
       videodec_component thread will wait there for all the buffer realese.
       */

    alogd("thread exit!");
    return NULL;
}

static ERRORTYPE start(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;

    alogd("start stream");
    AW_MPI_CLOCK_Start(pDemux2VdecData->mClockChn);

    if (pDemux2VdecData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_StartRecvStream(pDemux2VdecData->mVdecChn);
    }

    ret = AW_MPI_DEMUX_Start(pDemux2VdecData->mDmxChn);

    return ret;
}

static ERRORTYPE pauseTranse(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    if (pDemux2VdecData->mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Pause(pDemux2VdecData->mDmxChn);
    }

    if (pDemux2VdecData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_Pause(pDemux2VdecData->mVdecChn);
    }

    if (pDemux2VdecData->mClockChn >= 0)
    {
        AW_MPI_CLOCK_Pause(pDemux2VdecData->mClockChn);
    }

    return SUCCESS;
}

static ERRORTYPE stop(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;

    ret = AW_MPI_DEMUX_Stop(pDemux2VdecData->mDmxChn);

    if (pDemux2VdecData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(pDemux2VdecData->mVdecChn);
    }

    AW_MPI_CLOCK_Stop(pDemux2VdecData->mClockChn);

    return ret;
}

static ERRORTYPE seekto(SAMPLE_DEMUX2VDEC_S *pDemux2VdecData)
{
    ERRORTYPE ret;

    alogd("seek to");
    ret = AW_MPI_DEMUX_Seek(pDemux2VdecData->mDmxChn, pDemux2VdecData->mConfigPara.seekTime);

    if (pDemux2VdecData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_SetStreamEof(pDemux2VdecData->mVdecChn, 0);
        AW_MPI_VDEC_Seek(pDemux2VdecData->mVdecChn);
    }

    return ret;
}


int main(int argc, char** argv)
{
    int result = -1;
    int ret = 0;

    if (InitDemux2VdecData() != SUCCESS)
    {
        return -1;
    }

    cdx_sem_init(&pDemux2VdecData->mSemExit, 0);

    if (parseCmdLine(pDemux2VdecData, argc, argv) != 0)
    {
        goto err_out_0;
    }

    if (loadConfigPara(pDemux2VdecData, pDemux2VdecData->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        goto err_out_0;
    }

    pDemux2VdecData->mConfigPara.srcFd = open(pDemux2VdecData->mConfigPara.srcFile, O_RDONLY);
    if (pDemux2VdecData->mConfigPara.srcFd < 0)
    {
        aloge("ERROR: cannot open video src file");
        goto err_out_0;
    }

    pDemux2VdecData->mConfigPara.dstYFd = open(pDemux2VdecData->mConfigPara.dstYFile, O_CREAT|O_RDWR);
    if (pDemux2VdecData->mConfigPara.dstYFd < 0)
    {
        aloge("ERROR: cannot create y dst file");
        goto err_out_1;
    }

    pDemux2VdecData->mConfigPara.dstUFd = open(pDemux2VdecData->mConfigPara.dstUFile, O_CREAT|O_RDWR);
    if (pDemux2VdecData->mConfigPara.dstUFd < 0)
    {
        aloge("ERROR: cannot create u dst file");
        goto err_out_2;
    }

    pDemux2VdecData->mConfigPara.dstVFd	= open(pDemux2VdecData->mConfigPara.dstVFile, O_CREAT|O_RDWR);
    if (pDemux2VdecData->mConfigPara.dstVFd < 0)
    {
        aloge("ERROR: cannot create v dst file");
        goto err_out_3;
    }

    pDemux2VdecData->mConfigPara.dstYUVFd = open(pDemux2VdecData->mConfigPara.dstYUVFile, O_CREAT|O_RDWR);
    if (pDemux2VdecData->mConfigPara.dstYUVFd < 0)
    {
        aloge("ERROR: cannot create yuv dst file");
        goto err_out_4;
    }

    pDemux2VdecData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pDemux2VdecData->mSysConf);
    AW_MPI_SYS_Init();

    if (prepare(pDemux2VdecData) !=  SUCCESS)
    {
        aloge("prepare failed");
        goto err_out_5;
    }

    pDemux2VdecData->mOverFlag = FALSE;
    ret = pthread_create(&pDemux2VdecData->mThdId, NULL, getVdecFrameThread, pDemux2VdecData);
    if (ret || !pDemux2VdecData->mThdId)
    {
        aloge("create thread faile");
        goto err_out_6;
    }

    if (pDemux2VdecData->mConfigPara.seekTime > 0)
    {
        seekto(pDemux2VdecData);
    }
    if (start(pDemux2VdecData) != SUCCESS)
    {
        aloge("start play fail");
        goto err_out_7;
    }

    if (pDemux2VdecData->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pDemux2VdecData->mSemExit, pDemux2VdecData->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pDemux2VdecData->mSemExit);
    }

    alogw("over");
    pauseTranse(pDemux2VdecData);
    pDemux2VdecData->mOverFlag = TRUE;
    if (stop(pDemux2VdecData) != SUCCESS)
    {
        alogw("stop fail");
    }
    result = 0;

err_out_7:
    pthread_join(pDemux2VdecData->mThdId, NULL);
err_out_6:
    if (pDemux2VdecData->mDmxChn >= 0)
    {
        AW_MPI_DEMUX_DestroyChn(pDemux2VdecData->mDmxChn);
    }
    if (pDemux2VdecData->mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(pDemux2VdecData->mClockChn);
    }
    if (pDemux2VdecData->mVdecChn >= 0)
    {
        int ret = AW_MPI_VDEC_DestroyChn(pDemux2VdecData->mVdecChn);
        alogd("ret = %d", ret);
    }
err_out_5:
    //exit mpp system
    AW_MPI_SYS_Exit();
    close(pDemux2VdecData->mConfigPara.dstYUVFd);
err_out_4:
    close(pDemux2VdecData->mConfigPara.dstVFd);
err_out_3:
    close(pDemux2VdecData->mConfigPara.dstUFd);
err_out_2:
    close(pDemux2VdecData->mConfigPara.dstYFd);
err_out_1:
    close(pDemux2VdecData->mConfigPara.srcFd);
err_out_0:
    cdx_sem_deinit(&pDemux2VdecData->mSemExit);
    free(pDemux2VdecData);
    pDemux2VdecData = NULL;

    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
