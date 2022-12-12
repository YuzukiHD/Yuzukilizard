//#define LOG_NDEBUG 0
#define LOG_TAG "SampleDemux2Adec2AO"

#include <unistd.h>
#include <fcntl.h>
#include "plat_log.h"
//#include <utils/plat_log.h>
#include <time.h>

#include <mpi_demux.h>
#include <mpi_adec.h>
#include <mpi_ao.h>
#include <mpi_clock.h>
#include <ClockCompPortIndex.h>

#include "sample_demux2adec2ao.h"
#include "sample_demux2adec2ao_common.h"

#define DMX2ADEC2AO_DEFAULT_SRC_FILE        "/mnt/extsd/sample_demux2adec2ao/test.mp4"
#define PCM_FILE_OUTPUT


static SAMPLE_DEMUX2ADEC2AO_S *pDemux2Adec2AOData;

static ERRORTYPE InitDemux2Adec2AOData(void)
{
    pDemux2Adec2AOData = (SAMPLE_DEMUX2ADEC2AO_S* )malloc(sizeof(SAMPLE_DEMUX2ADEC2AO_S));
    if (pDemux2Adec2AOData == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }

    memset(pDemux2Adec2AOData, 0, sizeof(SAMPLE_DEMUX2ADEC2AO_S));

    pDemux2Adec2AOData->mDmxChn = MM_INVALID_CHN;
    pDemux2Adec2AOData->mAdecChn = MM_INVALID_CHN;
    pDemux2Adec2AOData->mAOChn = MM_INVALID_CHN;
    pDemux2Adec2AOData->mClockChn = MM_INVALID_CHN;

    strcpy(pDemux2Adec2AOData->srcFile, DMX2ADEC2AO_DEFAULT_SRC_FILE);

    return SUCCESS;
}

static int parseCmdLine(SAMPLE_DEMUX2ADEC2AO_S *pSampleData, int argc, char** argv)
{
    int ret = 0;

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

              strncpy(pSampleData->confFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pSampleData->confFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_demux2adec2ao.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}


static ERRORTYPE loadConfigPara(SAMPLE_DEMUX2ADEC2AO_S *pSampleData, const char *conf_path)
{
    int ret;
    pSampleData->seekTime = 0;
    strcpy(pSampleData->srcFile, "/mnt/extsd/test.aac");

    if(conf_path)
    {
        char *ptr;

        ret = createConfParser(conf_path, &pSampleData->mConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        pSampleData->seekTime = GetConfParaInt(&pSampleData->mConf, DMX2ADEC2AO_CFG_SEEK_POSITION, 0);

        ptr = (char*)GetConfParaString(&pSampleData->mConf, DMX2ADEC2AO_CFG_SRC_FILE_STR, NULL);
        strcpy(pSampleData->srcFile, ptr);

        destroyConfParser(&pSampleData->mConf);
    }
    return SUCCESS;
}

ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_DEMUX2ADEC2AO_S *pSampleData = (SAMPLE_DEMUX2ADEC2AO_S *)cookie;

    if (pChn->mModId == MOD_ID_DEMUX)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("demux get EOF flag");
            AW_MPI_ADEC_SetStreamEof(pSampleData->mAdecChn, 1);
            break;
        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_ADEC)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("adec get EOF flag");
            AW_MPI_AO_SetStreamEof(pSampleData->mAODevId, pSampleData->mAOChn, TRUE, TRUE);
            break;
        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_AO)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("ao get EOF flag");
            cdx_sem_up(&pSampleData->mWaitEof);
            break;
        default:
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE createClockChn(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;

    pSampleData->mClockChn = 0;
    pSampleData->mClockChnAttr.nWaitMask = 0;
    pSampleData->mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
    while (pSampleData->mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(pSampleData->mClockChn, &pSampleData->mClockChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", pSampleData->mClockChn);
            break;
        }
        else if (ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", pSampleData->mClockChn);
            pSampleData->mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", pSampleData->mClockChn, ret);
            break;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pSampleData->mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
        return FAILURE;
    }
    else
    {
        return SUCCESS;
    }
}

static ERRORTYPE ConfigDmxChnAttr(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    pSampleData->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pSampleData->mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pSampleData->mDmxChnAttr.mSourceUrl = NULL;
    pSampleData->mDmxChnAttr.mFd = pSampleData->srcFd;
    alogd("fd=%d", pSampleData->mDmxChnAttr.mFd);
    pSampleData->mDmxChnAttr.mDemuxDisableTrack = pSampleData->mTrackDisableFlag = DEMUX_DISABLE_SUBTITLE_TRACK | DEMUX_DISABLE_VIDEO_TRACK;

    return SUCCESS;
}

static ERRORTYPE createDemuxChn(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    int ret;
    BOOL nSuccessFlag = FALSE;

    ConfigDmxChnAttr(pSampleData);

    pSampleData->mDmxChn = 0;
    while (pSampleData->mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", pSampleData->mDmxChn);
            break;
        }
        else if (ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, find next!", pSampleData->mDmxChn);
            pSampleData->mDmxChn++;
        }
        else
        {
            alogd("create demux channel[%d] ret[0x%x]!", pSampleData->mDmxChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pSampleData->mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pSampleData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_DEMUX_RegisterCallback(pSampleData->mDmxChn, &cbInfo);
    }
    return SUCCESS;
}

static ERRORTYPE ConfigAdecChnAttr(SAMPLE_DEMUX2ADEC2AO_S *pSampleData, DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo)
{
    memset(&pSampleData->mAdecChnAttr, 0, sizeof(ADEC_CHN_ATTR_S));
    pSampleData->mAdecChnAttr.mType = pStreamInfo->mCodecType;
    pSampleData->mAdecChnAttr.sampleRate = pStreamInfo->mSampleRate;
    pSampleData->mAdecChnAttr.channels = pStreamInfo->mChannelNum;
    pSampleData->mAdecChnAttr.bitsPerSample = pStreamInfo->mBitsPerSample;

    return SUCCESS;
}

static ERRORTYPE createAdecChn(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    if (pSampleData->mDemuxMediaInfo.mAudioNum>0 && !(pSampleData->mDmxChnAttr.mDemuxDisableTrack&DEMUX_DISABLE_AUDIO_TRACK))
    {
        alogd("try to get adec chn");
        DEMUX_AUDIO_STREAM_INFO_S *pAudioStreamInfo =
            &(pSampleData->mDemuxMediaInfo.mAudioStreamInfo[pSampleData->mDemuxMediaInfo.mAudioIndex]);
        ConfigAdecChnAttr(pSampleData, pAudioStreamInfo);
        pSampleData->mAdecChn = 0;
        while (pSampleData->mAdecChn < ADEC_MAX_CHN_NUM)
        {
            ret = AW_MPI_ADEC_CreateChn(pSampleData->mAdecChn, &pSampleData->mAdecChnAttr);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create adec channel[%d] success!", pSampleData->mAdecChn);
                break;
            }
            else if (ERR_ADEC_EXIST == ret)
            {
                alogd("adec channel[%d] is exist, find next!", pSampleData->mAdecChn);
                pSampleData->mAdecChn++;
            }
            else
            {
                alogd("create adec channel[%d] ret[0x%x]!", pSampleData->mAdecChn, ret);
                break;
            }
        }

        if (FALSE == nSuccessFlag)
        {
            pSampleData->mAdecChn = MM_INVALID_CHN;
            aloge("fatal error! create adec channel fail!");
            return FAILURE;
        }
        else
        {
            alogd("add call back");
            MPPCallbackInfo cbInfo;
            cbInfo.cookie = (void*)pSampleData;
            cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
            AW_MPI_ADEC_RegisterCallback(pSampleData->mAdecChn, &cbInfo);
            return SUCCESS;
        }
    }
    else
    {
        return FAILURE;
    }
}

void config_AIO_ATTR_S_by_DEMUX_AUDIO_STREAM_INFO_S(AIO_ATTR_S *pAioAttr, DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo)
{
    memset(pAioAttr, 0, sizeof(AIO_ATTR_S));
    pAioAttr->u32ChnCnt = pStreamInfo->mChannelNum;
    pAioAttr->enBitwidth = (AUDIO_BIT_WIDTH_E)(pStreamInfo->mBitsPerSample/8 - 1);
    pAioAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)pStreamInfo->mSampleRate;
}

static ERRORTYPE createAOChn(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    ERRORTYPE ret;

    pSampleData->mAODevId = 0;
    DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo =
        &(pSampleData->mDemuxMediaInfo.mAudioStreamInfo[pSampleData->mDemuxMediaInfo.mAudioIndex]);
    config_AIO_ATTR_S_by_DEMUX_AUDIO_STREAM_INFO_S(&pSampleData->mAioAttr, pStreamInfo);
    //AW_MPI_AO_SetPubAttr(pSampleData->mAODevId, &pSampleData->mAioAttr);

    //enable audio_hw_ao
    //ret = AW_MPI_AO_Enable(pSampleData->mAODevId);

    //create ao channel
    BOOL bSuccessFlag = FALSE;
    pSampleData->mAOChn = 0;
    while(pSampleData->mAOChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(pSampleData->mAODevId, pSampleData->mAOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ao channel[%d] success!", pSampleData->mAOChn);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            alogd("ao channel[%d] exist, find next!", pSampleData->mAOChn);
            pSampleData->mAOChn++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ao not started!");
            break;
        }
        else
        {
            aloge("create ao channel[%d] fail! ret[0x%x]!", pSampleData->mAOChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        pSampleData->mAOChn = MM_INVALID_CHN;
        aloge("fatal error! create ao channel fail!");
        ret = FAILURE;
    }
    else
    {
        alogd("add call back");
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pSampleData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_AO_RegisterCallback(pSampleData->mAODevId, pSampleData->mAOChn, &cbInfo);
    }

    return ret;
}

static ERRORTYPE prepare(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    ERRORTYPE ret;
    DEMUX_MEDIA_INFO_S DemuxMediaInfo;

    if (AW_MPI_DEMUX_GetMediaInfo(pSampleData->mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return FAILURE;
    }

    if ((DemuxMediaInfo.mVideoNum >0 && DemuxMediaInfo.mVideoIndex>=DemuxMediaInfo.mVideoNum)
        || (DemuxMediaInfo.mAudioNum >0 && DemuxMediaInfo.mAudioIndex>=DemuxMediaInfo.mAudioNum)
        || (DemuxMediaInfo.mSubtitleNum >0 && DemuxMediaInfo.mSubtitleIndex>=DemuxMediaInfo.mSubtitleNum))
    {
        aloge("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
               DemuxMediaInfo.mVideoNum, DemuxMediaInfo.mVideoIndex,
               DemuxMediaInfo.mAudioNum, DemuxMediaInfo.mAudioIndex,
               DemuxMediaInfo.mSubtitleNum, DemuxMediaInfo.mSubtitleIndex);
        return FAILURE;
    }

    memcpy(&pSampleData->mDemuxMediaInfo, &DemuxMediaInfo, sizeof(DEMUX_MEDIA_INFO_S));

    if (DemuxMediaInfo.mSubtitleNum > 0)
    {
        AW_MPI_DEMUX_GetChnAttr(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
        pSampleData->mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_SUBTITLE_TRACK;
        AW_MPI_DEMUX_SetChnAttr(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
    }

    if (DemuxMediaInfo.mVideoNum > 0)
    {
        AW_MPI_DEMUX_GetChnAttr(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
        pSampleData->mDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_VIDEO_TRACK;
        AW_MPI_DEMUX_SetChnAttr(pSampleData->mDmxChn, &pSampleData->mDmxChnAttr);
    }

    if (DemuxMediaInfo.mAudioNum > 0)
    {
        ret = createAdecChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind demux & adec");
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
            MPP_CHN_S AdecChn = {MOD_ID_ADEC, 0, pSampleData->mAdecChn};
            AW_MPI_SYS_Bind(&DmxChn, &AdecChn);
        }

        ret = createClockChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind clock & demux");
            MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pSampleData->mClockChn};
            MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
            AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
        }

        ret = createAOChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind adec & ao");
            MPP_CHN_S AdecChn = {MOD_ID_ADEC, 0, pSampleData->mAdecChn};
            MPP_CHN_S AoChn = {MOD_ID_AO, 0, pSampleData->mAOChn};
            AW_MPI_SYS_Bind(&AdecChn, &AoChn);

            alogd("bind clock & ao");
            MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pSampleData->mClockChn};
            AW_MPI_SYS_Bind(&ClockChn, &AoChn);
        }

        return ret;
    }
    else
    {
        return FAILURE;
    }
}

static ERRORTYPE start(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    ERRORTYPE ret;

    alogd("start stream");
    AW_MPI_CLOCK_Start(pSampleData->mClockChn);

    if (pSampleData->mAOChn >= 0)
    {
        AW_MPI_AO_StartChn(pSampleData->mAODevId, pSampleData->mAOChn);
    }
    if (pSampleData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_StartRecvStream(pSampleData->mAdecChn);
    }
    ret = AW_MPI_DEMUX_Start(pSampleData->mDmxChn);

    return ret;
}

static ERRORTYPE stop(SAMPLE_DEMUX2ADEC2AO_S *pSampleData)
{
    ERRORTYPE ret;
    ret = AW_MPI_DEMUX_Stop(pSampleData->mDmxChn);
    if (pSampleData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_StopRecvStream(pSampleData->mAdecChn);
    }
    if (pSampleData->mAOChn >= 0)
    {
        AW_MPI_AO_StopChn(pSampleData->mAODevId, pSampleData->mAOChn);
    }
    AW_MPI_CLOCK_Stop(pSampleData->mClockChn);

    return ret;
}

static ERRORTYPE sendMsg(message_queue_t *msgQue, int cmd, int param0)
{
    ERRORTYPE eError = SUCCESS;
    message_t msg = {0};

    if (msgQue == NULL)
    {
        eError = FAILURE;
        return eError;
    }

    msg.command = cmd;
    msg.para0 = param0;
    put_message(msgQue, &msg);
    return eError;
}

static ERRORTYPE constructComp(void)
{
    pDemux2Adec2AOData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pDemux2Adec2AOData->mSysConf);
    AW_MPI_SYS_Init();
    return SUCCESS;
}


int main(int argc, char** argv)
{
    int ret = 0;

    if (InitDemux2Adec2AOData() != SUCCESS)
    {
        ret = -1;
        goto exit;
    }

    if (parseCmdLine(pDemux2Adec2AOData, argc, argv) != 0)
    {
        aloge("parseCmdLine fail!");
        ret = -1;
    }
    char *pConfFilePath = NULL;
    if(strlen(pDemux2Adec2AOData->confFilePath) > 0)
    {
        pConfFilePath =  pDemux2Adec2AOData->confFilePath;
    }
    if (loadConfigPara(pDemux2Adec2AOData, pConfFilePath) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        ret = -1;
    }

    pDemux2Adec2AOData->srcFd = open(pDemux2Adec2AOData->srcFile, O_RDONLY);
    if (pDemux2Adec2AOData->srcFd < 0)
    {
        aloge("ERROR: cannot open mp4 src file");
        ret = -1;
    }
    else
    {
        alogd("mp4 src file fd=%d", pDemux2Adec2AOData->srcFd);
    }

    cdx_sem_init(&pDemux2Adec2AOData->mWaitEof, 0);

    if (constructComp() != SUCCESS)
    {
        aloge("ERROR: demux construct fail");
        ret = -1;
    }

    if (createDemuxChn(pDemux2Adec2AOData) != SUCCESS)
    {
        aloge("create demuxchn fail");
        ret = -1;
    }

    if (prepare(pDemux2Adec2AOData) != SUCCESS)
    {
        aloge("prepare failed");
        ret = -1;
    }

    if (start(pDemux2Adec2AOData) != SUCCESS)
    {
        aloge("start play fail");
        ret = -1;
    }

    AW_MPI_AO_SetDevVolume(pDemux2Adec2AOData->mAODevId, 90);

    alogd("wait until app receive EOF from DMX...");
    cdx_sem_down(&pDemux2Adec2AOData->mWaitEof);
    alogd("play finished!");

    if (stop(pDemux2Adec2AOData) != SUCCESS)
    {
        alogw("stop fail");
        ret = -1;
    }

    if (pDemux2Adec2AOData->mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(pDemux2Adec2AOData->mClockChn);
    }
    if (pDemux2Adec2AOData->mAOChn >= 0)
    {
        AW_MPI_AO_DestroyChn(pDemux2Adec2AOData->mAODevId, pDemux2Adec2AOData->mAOChn);
    }
    if (pDemux2Adec2AOData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_DestroyChn(pDemux2Adec2AOData->mAdecChn);
    }
    AW_MPI_DEMUX_DestroyChn(pDemux2Adec2AOData->mDmxChn);


   //exit mpp system
    AW_MPI_SYS_Exit();
    cdx_sem_deinit(&pDemux2Adec2AOData->mWaitEof);
    close(pDemux2Adec2AOData->srcFd);
    free(pDemux2Adec2AOData);
    pDemux2Adec2AOData = NULL;

exit:
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}
