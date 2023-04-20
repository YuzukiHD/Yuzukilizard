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
#define LOG_TAG "SampleDemux2Vdec2Vo"

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include "plat_log.h"

#include <mpi_demux.h>
#include <mpi_vdec.h>
#include <mpi_clock.h>

#include "hwdisplay.h"

#include "sample_demux2vdec2vo.h"
#include "sample_demux2vdec2vo_common.h"


#define DMX2VDEC2VO_DEFAULT_SRC_FILE   "/mnt/extsd/sample_demux2vdec2vo/test.mp4"


static SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData;


static ERRORTYPE InitDemux2Vdec2VoData(void)
{
    pDemux2Vdec2VoData = (SAMPLE_DEMUX2VDEC2VO_S* )malloc(sizeof(SAMPLE_DEMUX2VDEC2VO_S));
    if (pDemux2Vdec2VoData == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }

    memset(pDemux2Vdec2VoData, 0, sizeof(SAMPLE_DEMUX2VDEC2VO_S));

    pDemux2Vdec2VoData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    //pDemux2Vdec2VoData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputWidth = 1920;
    pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputHeight= 1080;

    pDemux2Vdec2VoData->mDmxChn = MM_INVALID_CHN;
    pDemux2Vdec2VoData->mVdecChn = MM_INVALID_CHN;
    pDemux2Vdec2VoData->mClockChn = MM_INVALID_CHN;

    pDemux2Vdec2VoData->mVoDev = MM_INVALID_DEV;
    pDemux2Vdec2VoData->mVoLayer = MM_INVALID_CHN;
    pDemux2Vdec2VoData->mVoChn = MM_INVALID_CHN;
    pDemux2Vdec2VoData->mUILayer = MM_INVALID_CHN;

    return SUCCESS;
}

static int parseCmdLine(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData, int argc, char** argv)
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

              strncpy(pDemux2Vdec2VoData->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pDemux2Vdec2VoData->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_demux2vdec2vo.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S mConf;

    strcpy(pDemux2Vdec2VoData->mConfigPara.srcFile, "/mnt/extsd/test.mp4");
    pDemux2Vdec2VoData->mConfigPara.seekTime = 0;
    pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputWidth = 1920;
    pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputHeight= 1080;
    pDemux2Vdec2VoData->mConfigPara.mInitRotation = 0;
    pDemux2Vdec2VoData->mConfigPara.mUserSetPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;   //MM_PIXEL_FORMAT_YVU_PLANAR_420
    pDemux2Vdec2VoData->mConfigPara.mTestDuration = 0;
    pDemux2Vdec2VoData->mConfigPara.mDisplayX = 0;
    pDemux2Vdec2VoData->mConfigPara.mDisplayY = 0;
    pDemux2Vdec2VoData->mConfigPara.mDisplayWidth = 480;
    pDemux2Vdec2VoData->mConfigPara.mDisplayHeight = 640;

    pDemux2Vdec2VoData->mConfigPara.mbForceFramePackage = FALSE;

    if(conf_path != NULL)
    {
        ret = createConfParser(conf_path, &mConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        ptr = (char *)GetConfParaString(&mConf, DMX2VDEC2VO_CFG_SRC_FILE_STR, NULL);
        strcpy(pDemux2Vdec2VoData->mConfigPara.srcFile, ptr);

        pDemux2Vdec2VoData->mConfigPara.seekTime = GetConfParaInt(&mConf, DMX2VDEC2VO_CFG_SEEK_POSITION, 0);
        pDemux2Vdec2VoData->mConfigPara.mTestDuration = GetConfParaInt(&mConf, DMX2VDEC2VO_CFG_TEST_DURATION, 0);
        pDemux2Vdec2VoData->mConfigPara.mDisplayX = GetConfParaInt(&mConf, DMX2VDEC2VO_CFG_DISPLAY_X, 0);
        pDemux2Vdec2VoData->mConfigPara.mDisplayY = GetConfParaInt(&mConf, DMX2VDEC2VO_CFG_DISPLAY_Y, 0);
        pDemux2Vdec2VoData->mConfigPara.mDisplayWidth = GetConfParaInt(&mConf, DMX2VDEC2VO_CFG_DISPLAY_WIDTH, 0);
        pDemux2Vdec2VoData->mConfigPara.mDisplayHeight = GetConfParaInt(&mConf, DMX2VDEC2VO_CFG_DISPLAY_HEIGHT, 0);
        pDemux2Vdec2VoData->mConfigPara.mVeFreq = GetConfParaInt(&mConf, DMX2VDEC2VO_CFG_VE_FREQ, 0);

        alogd("src_file:%s, seek_time:%d, test_time=%d", pDemux2Vdec2VoData->mConfigPara.srcFile,\
            pDemux2Vdec2VoData->mConfigPara.seekTime, pDemux2Vdec2VoData->mConfigPara.mTestDuration);

        destroyConfParser(&mConf);
    }
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData = (SAMPLE_DEMUX2VDEC2VO_S *)cookie;

    if (pChn->mModId == MOD_ID_DEMUX)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("demux to end of file");
            if (pDemux2Vdec2VoData->mVdecChn >= 0)
            {
                AW_MPI_VDEC_SetStreamEof(pDemux2Vdec2VoData->mVdecChn, 1);
            }
            //cdx_sem_up(&pDemux2Vdec2VoData->mSemExit);
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
            alogd("vdec to the end of file");
            if (pDemux2Vdec2VoData->mVoChn >= 0)
            {
                AW_MPI_VO_SetStreamEof(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn, 1);
            }
            //cdx_sem_up(&pDemux2Vdec2VoData->mSemExit);
            break;

        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_VOU)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vo to the end of file");
            cdx_sem_up(&pDemux2Vdec2VoData->mSemExit);
            break;

        case MPP_EVENT_RENDERING_START:
            alogd("vo start to rendering");
            break;

        default:
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE createClockChn(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;

    pDemux2Vdec2VoData->mClockChn = 0;
    while (pDemux2Vdec2VoData->mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(pDemux2Vdec2VoData->mClockChn, &pDemux2Vdec2VoData->mClockChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", pDemux2Vdec2VoData->mClockChn);
            break;
        }
        else if (ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", pDemux2Vdec2VoData->mClockChn);
            pDemux2Vdec2VoData->mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", pDemux2Vdec2VoData->mClockChn, ret);
            break;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pDemux2Vdec2VoData->mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
        return FAILURE;
    }
    else
    {
        return SUCCESS;
    }
}

static ERRORTYPE configDmxChnAttr(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    pDemux2Vdec2VoData->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pDemux2Vdec2VoData->mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pDemux2Vdec2VoData->mDmxChnAttr.mSourceUrl = NULL;
    pDemux2Vdec2VoData->mDmxChnAttr.mFd = pDemux2Vdec2VoData->srcFd;
    pDemux2Vdec2VoData->mDmxChnAttr.mDemuxDisableTrack = pDemux2Vdec2VoData->mTrackDisFlag = DEMUX_DISABLE_SUBTITLE_TRACK|DEMUX_DISABLE_AUDIO_TRACK;

    return SUCCESS;
}

static ERRORTYPE createDemuxChn(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configDmxChnAttr(pDemux2Vdec2VoData);

    pDemux2Vdec2VoData->mDmxChn = 0;
    while (pDemux2Vdec2VoData->mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(pDemux2Vdec2VoData->mDmxChn, &pDemux2Vdec2VoData->mDmxChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", pDemux2Vdec2VoData->mDmxChn);
            break;
        }
        else if (ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, find next!", pDemux2Vdec2VoData->mDmxChn);
            pDemux2Vdec2VoData->mDmxChn++;
        }
        else
        {
            alogd("create demux channel[%d] ret[0x%x]!", pDemux2Vdec2VoData->mDmxChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pDemux2Vdec2VoData->mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pDemux2Vdec2VoData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_DEMUX_RegisterCallback(pDemux2Vdec2VoData->mDmxChn, &cbInfo);
        return SUCCESS;
    }
}

static ERRORTYPE ConfigVdecChnAttr(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData, DEMUX_VIDEO_STREAM_INFO_S *pStreamInfo)
{
    memset(&pDemux2Vdec2VoData->mVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    pDemux2Vdec2VoData->mVdecChnAttr.mPicWidth = pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputWidth;
    pDemux2Vdec2VoData->mVdecChnAttr.mPicHeight = pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputHeight;
    pDemux2Vdec2VoData->mVdecChnAttr.mInitRotation = pDemux2Vdec2VoData->mConfigPara.mInitRotation;
    pDemux2Vdec2VoData->mVdecChnAttr.mOutputPixelFormat = pDemux2Vdec2VoData->mConfigPara.mUserSetPixelFormat;
    pDemux2Vdec2VoData->mVdecChnAttr.mType = pStreamInfo->mCodecType;
    pDemux2Vdec2VoData->mVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 0; //1
    pDemux2Vdec2VoData->mVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;

    return SUCCESS;
}

static ERRORTYPE createVdecChn(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    if ((pDemux2Vdec2VoData->mDemuxMediaInfo.mVideoNum >0 ) && !(pDemux2Vdec2VoData->mDmxChnAttr.mDemuxDisableTrack & DEMUX_DISABLE_VIDEO_TRACK))
    {
        ConfigVdecChnAttr(pDemux2Vdec2VoData, &pDemux2Vdec2VoData->mDemuxMediaInfo.mVideoStreamInfo[pDemux2Vdec2VoData->mDemuxMediaInfo.mVideoIndex]);
        pDemux2Vdec2VoData->mVdecChn = 0;
        while (pDemux2Vdec2VoData->mVdecChn < VDEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VDEC_CreateChn(pDemux2Vdec2VoData->mVdecChn, &pDemux2Vdec2VoData->mVdecChnAttr);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create vdec channel[%d] success!", pDemux2Vdec2VoData->mVdecChn);
                break;
            }
            else if (ERR_VDEC_EXIST == ret)
            {
                alogd("vdec channel[%d] is exist, find next!", pDemux2Vdec2VoData->mVdecChn);
                pDemux2Vdec2VoData->mVdecChn++;
            }
            else
            {
                alogd("create vdec channel[%d] ret[0x%x]!", pDemux2Vdec2VoData->mVdecChn, ret);
                break;
            }
        }

        if (FALSE == nSuccessFlag)
        {
            pDemux2Vdec2VoData->mVdecChn = MM_INVALID_CHN;
            aloge("fatal error! create vdec channel fail!");
            return FAILURE;
        }
        else
        {
            if (pDemux2Vdec2VoData->mConfigPara.mVeFreq)
            {
                alogd("vdec set ve freq %d MHz", pDemux2Vdec2VoData->mConfigPara.mVeFreq);
                AW_MPI_VDEC_SetVEFreq(pDemux2Vdec2VoData->mVdecChn, pDemux2Vdec2VoData->mConfigPara.mVeFreq);
            }
            alogd("add call back");
            MPPCallbackInfo cbInfo;
            cbInfo.cookie = (void*)pDemux2Vdec2VoData;
            cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
            AW_MPI_VDEC_RegisterCallback(pDemux2Vdec2VoData->mVdecChn, &cbInfo);
            AW_MPI_VDEC_ForceFramePackage(pDemux2Vdec2VoData->mVdecChn, pDemux2Vdec2VoData->mConfigPara.mbForceFramePackage);
            return SUCCESS;
        }
    }
    else
    {
        return FAILURE;
    }
}

static ERRORTYPE createVoChn(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    pDemux2Vdec2VoData->mVoDev = 0;
    pDemux2Vdec2VoData->mUILayer = HLAY(2, 0);

    AW_MPI_VO_Enable(pDemux2Vdec2VoData->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pDemux2Vdec2VoData->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pDemux2Vdec2VoData->mUILayer);//close ui layer.
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pDemux2Vdec2VoData->mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pDemux2Vdec2VoData->mVoDev, &spPubAttr);

   //enable vo layer
    int hlay0 = 0;
    while (hlay0 < VO_MAX_LAYER_NUM)
    {
        if (SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            break;
        }
        hlay0++;
    }

    if (hlay0 >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
        pDemux2Vdec2VoData->mVoLayer = MM_INVALID_DEV;
        AW_MPI_VO_RemoveOutsideVideoLayer(pDemux2Vdec2VoData->mUILayer);
        AW_MPI_VO_Disable(pDemux2Vdec2VoData->mVoDev);
        return FAILURE;
    }

    pDemux2Vdec2VoData->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(pDemux2Vdec2VoData->mVoLayer, &pDemux2Vdec2VoData->mVoLayerAttr);

    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.X = pDemux2Vdec2VoData->mConfigPara.mDisplayX;
    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.Y = pDemux2Vdec2VoData->mConfigPara.mDisplayY;
    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.Width = pDemux2Vdec2VoData->mConfigPara.mDisplayWidth;
    pDemux2Vdec2VoData->mVoLayerAttr.stDispRect.Height = pDemux2Vdec2VoData->mConfigPara.mDisplayHeight;
    pDemux2Vdec2VoData->mVoLayerAttr.enPixFormat = pDemux2Vdec2VoData->mConfigPara.mUserSetPixelFormat;
    AW_MPI_VO_SetVideoLayerAttr(pDemux2Vdec2VoData->mVoLayer, &pDemux2Vdec2VoData->mVoLayerAttr);


    pDemux2Vdec2VoData->mVoChn = 0;
    while (pDemux2Vdec2VoData->mVoChn < VO_MAX_CHN_NUM)
    {
        ret = AW_MPI_VO_CreateChn(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", pDemux2Vdec2VoData->mVoChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == ret)
        {
            alogd("vo channel[%d] is exist, find next!", pDemux2Vdec2VoData->mVoChn);
            pDemux2Vdec2VoData->mVoChn++;
        }
        else
        {
            alogd("create vo channel[%d] ret[0x%x]!", pDemux2Vdec2VoData->mVoChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pDemux2Vdec2VoData->mVoChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pDemux2Vdec2VoData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VO_RegisterCallback(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn, 2);
        return SUCCESS;
    }
}

static ERRORTYPE prepare(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;
    DEMUX_MEDIA_INFO_S DemuxMediaInfo = {0};

    if (createDemuxChn(pDemux2Vdec2VoData) != SUCCESS)
    {
        aloge("create demux chn fail");
        return FAILURE;
    }

    alogd("prepare");
    if (AW_MPI_DEMUX_GetMediaInfo(pDemux2Vdec2VoData->mDmxChn, &DemuxMediaInfo) != SUCCESS)
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

    memcpy(&pDemux2Vdec2VoData->mDemuxMediaInfo, &DemuxMediaInfo, sizeof(DEMUX_MEDIA_INFO_S));

    pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputWidth = DemuxMediaInfo.mVideoStreamInfo[0].mWidth;
    pDemux2Vdec2VoData->mConfigPara.mMaxVdecOutputHeight = DemuxMediaInfo.mVideoStreamInfo[0].mHeight;

    if (DemuxMediaInfo.mVideoNum >0)
    {
        ret = createVdecChn(pDemux2Vdec2VoData);
        if (ret == SUCCESS)
        {
            alogd("bind demux & vdec");
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pDemux2Vdec2VoData->mDmxChn};
            MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pDemux2Vdec2VoData->mVdecChn};
            AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
        }
        else
        {
            alogd("create vdec chn fail");
            return FAILURE;
        }

        ret = createVoChn(pDemux2Vdec2VoData);
        if (ret == SUCCESS)
        {
            alogd("bind vdec & vo");
            MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pDemux2Vdec2VoData->mVdecChn};
            MPP_CHN_S VoChn = {MOD_ID_VOU, pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn};

            AW_MPI_SYS_Bind(&VdecChn, &VoChn);
        }
        else
        {
            alogd("create vo chn fail");
            return FAILURE;
        }

        pDemux2Vdec2VoData->mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_VIDEO; //becareful this is too important!!!
        ret = createClockChn(pDemux2Vdec2VoData);
        if (ret == SUCCESS)
        {
            alogd("bind clock & demux");
            MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pDemux2Vdec2VoData->mClockChn};
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pDemux2Vdec2VoData->mDmxChn};
            MPP_CHN_S VoChn = {MOD_ID_VOU, pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn};

            AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
            AW_MPI_SYS_Bind(&ClockChn, &VoChn);
        }
        else
        {
            alogd("create clock chn fail");
            return FAILURE;
        }


        return SUCCESS;
    }
    else
    {
        return FAILURE;
    }
}

static ERRORTYPE start(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;

    alogd("start stream");
    AW_MPI_CLOCK_Start(pDemux2Vdec2VoData->mClockChn);

    if (pDemux2Vdec2VoData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_StartRecvStream(pDemux2Vdec2VoData->mVdecChn);
    }

    if ((pDemux2Vdec2VoData->mVoLayer >= 0) && (pDemux2Vdec2VoData->mVoChn >= 0))
    {
        AW_MPI_VO_StartChn(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn);
    }

    ret = AW_MPI_DEMUX_Start(pDemux2Vdec2VoData->mDmxChn);

    return ret;
}

static ERRORTYPE pauseTranse(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    if (pDemux2Vdec2VoData->mDmxChn >= 0)
    {
        AW_MPI_DEMUX_Pause(pDemux2Vdec2VoData->mDmxChn);
    }

    if (pDemux2Vdec2VoData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_Pause(pDemux2Vdec2VoData->mVdecChn);
    }

    if (pDemux2Vdec2VoData->mClockChn >= 0)
    {
        AW_MPI_CLOCK_Pause(pDemux2Vdec2VoData->mClockChn);
    }

    if ((pDemux2Vdec2VoData->mVoLayer >= 0) && (pDemux2Vdec2VoData->mVoChn >= 0))
    {
        AW_MPI_VO_PauseChn(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn);
    }

    return SUCCESS;
}

static ERRORTYPE stop(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;

//vo stop must before vdec stop (when vo stop, will return buffer to vdec, just all buffer sync)
    if ((pDemux2Vdec2VoData->mVoLayer >=0) && (pDemux2Vdec2VoData->mVoChn >= 0))
    {
        alogd("stop vo chn");
        AW_MPI_VO_StopChn(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn);
    }
    if (pDemux2Vdec2VoData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_StopRecvStream(pDemux2Vdec2VoData->mVdecChn);
    }
    ret = AW_MPI_DEMUX_Stop(pDemux2Vdec2VoData->mDmxChn);
    
    AW_MPI_CLOCK_Stop(pDemux2Vdec2VoData->mClockChn);
    return ret;
}

static ERRORTYPE seekto(SAMPLE_DEMUX2VDEC2VO_S *pDemux2Vdec2VoData)
{
    ERRORTYPE ret;

    alogd("seek to");
    ret = AW_MPI_DEMUX_Seek(pDemux2Vdec2VoData->mDmxChn, pDemux2Vdec2VoData->mConfigPara.seekTime);

    if (pDemux2Vdec2VoData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_SetStreamEof(pDemux2Vdec2VoData->mVdecChn, 0);
    }
    if ((pDemux2Vdec2VoData->mVoLayer >= 0) && (pDemux2Vdec2VoData->mVoChn >= 0))
    {
        AW_MPI_VO_SetStreamEof(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn, 0);
    }


    if (pDemux2Vdec2VoData->mVdecChn >= 0)
    {
        AW_MPI_VDEC_Seek(pDemux2Vdec2VoData->mVdecChn);
    }
    if ((pDemux2Vdec2VoData->mVoLayer >= 0) && (pDemux2Vdec2VoData->mVoChn >= 0))
    {
        AW_MPI_VO_Seek(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn);
    }

    return ret;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != pDemux2Vdec2VoData)
    {
        cdx_sem_up(&pDemux2Vdec2VoData->mSemExit);
    }
}

int main(int argc, char** argv)
{
    int result = -1;
    int ret = 0;

    if (InitDemux2Vdec2VoData() != SUCCESS)
    {
        return -1;
    }

    cdx_sem_init(&pDemux2Vdec2VoData->mSemExit, 0);
    char *pConfFilePath = NULL;
    if(argc > 1)
    {
        if (parseCmdLine(pDemux2Vdec2VoData, argc, argv) != 0)
        {
            goto err_out_0;
        }
        pConfFilePath = pDemux2Vdec2VoData->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfFilePath = NULL;
    }

    if (loadConfigPara(pDemux2Vdec2VoData, pConfFilePath) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        goto err_out_0;
    }

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        aloge("can't catch SIGSEGV");
    
    pDemux2Vdec2VoData->srcFd = open(pDemux2Vdec2VoData->mConfigPara.srcFile, O_RDONLY);
    if (pDemux2Vdec2VoData->srcFd < 0)
    {
        aloge("ERROR: cannot open video src file");
        goto err_out_0;
    }

    pDemux2Vdec2VoData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pDemux2Vdec2VoData->mSysConf);
    AW_MPI_SYS_Init();

    if (prepare(pDemux2Vdec2VoData) !=  SUCCESS)
    {
        aloge("prepare failed");
        goto err_out_1;
    }

    if (pDemux2Vdec2VoData->mConfigPara.seekTime > 0)
    {
        seekto(pDemux2Vdec2VoData);
    }
    if (start(pDemux2Vdec2VoData) != SUCCESS)
    {
        aloge("start play fail");
        goto err_out_2;
    }

    if (pDemux2Vdec2VoData->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pDemux2Vdec2VoData->mSemExit, pDemux2Vdec2VoData->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pDemux2Vdec2VoData->mSemExit);
    }
    pDemux2Vdec2VoData->mOverFlag = TRUE;

    alogw("over");

    if (stop(pDemux2Vdec2VoData) != SUCCESS)
    {
        alogw("stop fail");
    }
    result = 0;

err_out_2:
    //vo stop/destroy must before vdec stop (when vo destroy, will return buffer to vdec(2 frames), just all buffer sync)
    if (pDemux2Vdec2VoData->mVoLayer >= 0)
    {
        if (pDemux2Vdec2VoData->mVoChn >= 0)
        {
            AW_MPI_VO_DestroyChn(pDemux2Vdec2VoData->mVoLayer, pDemux2Vdec2VoData->mVoChn);
            pDemux2Vdec2VoData->mVoChn = MM_INVALID_CHN;
        }

        AW_MPI_VO_DisableVideoLayer(pDemux2Vdec2VoData->mVoLayer);
        //wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
        usleep(50*1000);
        pDemux2Vdec2VoData->mVoLayer = MM_INVALID_CHN;
        AW_MPI_VO_RemoveOutsideVideoLayer(pDemux2Vdec2VoData->mUILayer);
        AW_MPI_VO_Disable(pDemux2Vdec2VoData->mVoDev);
        pDemux2Vdec2VoData->mVoDev = MM_INVALID_DEV;
    }
    if (pDemux2Vdec2VoData->mVdecChn >= 0)
    {
        int ret = AW_MPI_VDEC_DestroyChn(pDemux2Vdec2VoData->mVdecChn);
        alogd("ret = %d", ret);
    }
    if (pDemux2Vdec2VoData->mDmxChn >= 0)
    {
        AW_MPI_DEMUX_DestroyChn(pDemux2Vdec2VoData->mDmxChn);
    }
    if (pDemux2Vdec2VoData->mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(pDemux2Vdec2VoData->mClockChn);
    }
err_out_1:
    //exit mpp system
    AW_MPI_SYS_Exit();

    close(pDemux2Vdec2VoData->srcFd);
err_out_0:
    cdx_sem_deinit(&pDemux2Vdec2VoData->mSemExit);
    free(pDemux2Vdec2VoData);
    pDemux2Vdec2VoData = NULL;

    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
