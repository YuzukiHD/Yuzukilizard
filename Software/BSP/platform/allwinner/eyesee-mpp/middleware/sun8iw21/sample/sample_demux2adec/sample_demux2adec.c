//#define LOG_NDEBUG 0
#define LOG_TAG "SampleDemux2Adec"
#include "plat_log.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <mpi_demux.h>
#include <mpi_adec.h>
#include <mpi_ao.h>
#include <mpi_clock.h>
#include <ClockCompPortIndex.h>

#include "sample_demux2adec.h"
#include "sample_demux2adec_conf.h"

#define DMX2ADEC_DEFAULT_SRC_FILE   "/mnt/extsd/sample_demux2adec/test.mp4"
#define DMX2ADEC_DEFAULT_DST_FILE   "/mnt/extsd/sample_demux2adec/output.wav"

static SAMPLE_DEMUX2ADEC_S *pDemux2AdecData;

static ERRORTYPE InitDemux2AdecData(void)
{
    pDemux2AdecData = (SAMPLE_DEMUX2ADEC_S* )malloc(sizeof(SAMPLE_DEMUX2ADEC_S));
    if (pDemux2AdecData == NULL)
    {
        aloge("malloc sample_demux2adec context fail");
        return FAILURE;
    }

    memset(pDemux2AdecData, 0, sizeof(SAMPLE_DEMUX2ADEC_S));

    pDemux2AdecData->mDmxChn   = MM_INVALID_CHN;
    pDemux2AdecData->mAdecChn  = MM_INVALID_CHN;
    pDemux2AdecData->mClockChn = MM_INVALID_CHN;

    strcpy(pDemux2AdecData->srcFile, DMX2ADEC_DEFAULT_SRC_FILE);
    strcpy(pDemux2AdecData->dstFile, DMX2ADEC_DEFAULT_DST_FILE);

    return SUCCESS;
}

static int parseCmdLine(SAMPLE_DEMUX2ADEC_S *pSampleData, int argc, char** argv)
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


static ERRORTYPE loadConfigPara(SAMPLE_DEMUX2ADEC_S *pSampleData, const char *conf_path)
{
    int ret = createConfParser(conf_path, &pSampleData->mConf);
    if (ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    //pSampleData->seekTime = GetConfParaInt(&pSampleData->mConf, DMX2ADEC_CFG_SEEK_POSITION, 0);

    char* ptr = NULL;
    ptr = (char*)GetConfParaString(&pSampleData->mConf, DMX2ADEC_CFG_SRC_FILE_PATH, NULL);
    strcpy(pSampleData->srcFile, ptr);
    ptr = (char*)GetConfParaString(&pSampleData->mConf, DMX2ADEC_CFG_DST_FILE_PATH, NULL);
    strcpy(pSampleData->dstFile, ptr);

    destroyConfParser(&pSampleData->mConf);
    return SUCCESS;
}


ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_DEMUX2ADEC_S *pSampleData = (SAMPLE_DEMUX2ADEC_S *)cookie;

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
            pSampleData->overFlag = 1;
            break;
        default:
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE createClockChn(SAMPLE_DEMUX2ADEC_S *pSampleData)
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


static ERRORTYPE createDemuxChn(SAMPLE_DEMUX2ADEC_S *pSampleData)
{
    int ret;
    BOOL nSuccessFlag = FALSE;

    // Config demux channel
    pSampleData->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pSampleData->mDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pSampleData->mDmxChnAttr.mSourceUrl  = NULL;
    pSampleData->mDmxChnAttr.mFd         = pSampleData->srcFd;
    pSampleData->mDmxChnAttr.mDemuxDisableTrack = DEMUX_DISABLE_SUBTITLE_TRACK;
    pSampleData->mTrackDisableFlag       = DEMUX_DISABLE_SUBTITLE_TRACK;

    // Create demux channel
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
        cbInfo.cookie   = (void*)pSampleData;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_DEMUX_RegisterCallback(pSampleData->mDmxChn, &cbInfo);
    }
    return SUCCESS;
}


static ERRORTYPE createAdecChn(SAMPLE_DEMUX2ADEC_S *pSampleData)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    if (  (pSampleData->mDemuxMediaInfo.mAudioNum > 0)
       && !(pSampleData->mDmxChnAttr.mDemuxDisableTrack & DEMUX_DISABLE_AUDIO_TRACK)
       )
    {
        // Get Audio Stream Info
        DEMUX_MEDIA_INFO_S* pMediaInfo = &pSampleData->mDemuxMediaInfo;
        DEMUX_AUDIO_STREAM_INFO_S *pAudioStreamInfo = &pMediaInfo->mAudioStreamInfo[pMediaInfo->mAudioIndex];
            
        // Config adec channel
        memset(&pSampleData->mAdecChnAttr, 0, sizeof(ADEC_CHN_ATTR_S));
        pSampleData->mAdecChnAttr.mType         = pAudioStreamInfo->mCodecType;
        pSampleData->mAdecChnAttr.sampleRate    = pAudioStreamInfo->mSampleRate;
        pSampleData->mAdecChnAttr.channels      = pAudioStreamInfo->mChannelNum;
        pSampleData->mAdecChnAttr.bitsPerSample = pAudioStreamInfo->mBitsPerSample;
        
        // Create adec channel
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
            MPPCallbackInfo cbInfo;
            cbInfo.cookie   = (void*)pSampleData;
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


static ERRORTYPE prepare(SAMPLE_DEMUX2ADEC_S *pSampleData)
{
    ERRORTYPE ret;
    
    if (createDemuxChn(pDemux2AdecData) != SUCCESS)
    {
        aloge("create demuxchn fail");
        return FAILURE;
    }
    
    // get media info from demux component
    DEMUX_MEDIA_INFO_S DemuxMediaInfo;
    if (AW_MPI_DEMUX_GetMediaInfo(pSampleData->mDmxChn, &DemuxMediaInfo) != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
        return FAILURE;
    }

    if (  (DemuxMediaInfo.mVideoNum > 0 && DemuxMediaInfo.mVideoIndex >= DemuxMediaInfo.mVideoNum)
       || (DemuxMediaInfo.mAudioNum > 0 && DemuxMediaInfo.mAudioIndex >= DemuxMediaInfo.mAudioNum)
       || (DemuxMediaInfo.mSubtitleNum > 0 && DemuxMediaInfo.mSubtitleIndex >= DemuxMediaInfo.mSubtitleNum)
       )
    {
        aloge("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
               DemuxMediaInfo.mVideoNum, DemuxMediaInfo.mVideoIndex,
               DemuxMediaInfo.mAudioNum, DemuxMediaInfo.mAudioIndex,
               DemuxMediaInfo.mSubtitleNum, DemuxMediaInfo.mSubtitleIndex);
        return FAILURE;
    }

    // save media info
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
        // Print audio track info
        DEMUX_AUDIO_STREAM_INFO_S* pAudioStreamInfo = &DemuxMediaInfo.mAudioStreamInfo[DemuxMediaInfo.mAudioIndex];
        alogd("Audio Info: ChannelNum = %d BitsPerSample = %d SampleRate = %d"
             ,pAudioStreamInfo->mChannelNum, pAudioStreamInfo->mBitsPerSample, pAudioStreamInfo->mSampleRate);
        
        ret = createAdecChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind demux & adec");
            MPP_CHN_S DmxChn  = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
            MPP_CHN_S AdecChn = {MOD_ID_ADEC, 0, pSampleData->mAdecChn};
            AW_MPI_SYS_Bind(&DmxChn, &AdecChn);
        }

        ret = createClockChn(pSampleData);
        if (ret == SUCCESS)
        {
            alogd("bind clock & demux");
            MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pSampleData->mClockChn};
            MPP_CHN_S DmxChn   = {MOD_ID_DEMUX, 0, pSampleData->mDmxChn};
            AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
        }

        return ret;
    }
    else
    {
        return FAILURE;
    }
}

static void writeWaveHeader(DEMUX_MEDIA_INFO_S *pMediaInfo, FILE *fp, int pcm_size)
{
    DEMUX_AUDIO_STREAM_INFO_S* pAudioStreamInfo = &pMediaInfo->mAudioStreamInfo[pMediaInfo->mAudioIndex];

    WaveHeader header;
    memcpy(&header.riff_id, "RIFF", 4);
    header.riff_sz = pcm_size + sizeof(WaveHeader) - 8;
    memcpy(&header.riff_fmt, "WAVE", 4);
    memcpy(&header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // s16le
    header.num_chn = pAudioStreamInfo->mChannelNum;
    header.sample_rate = pAudioStreamInfo->mSampleRate;
    header.byte_rate = pAudioStreamInfo->mSampleRate * pAudioStreamInfo->mChannelNum * 2;
    header.block_align = pAudioStreamInfo->mChannelNum * 2;
    header.bits_per_sample = 16;
    memcpy(&header.data_id, "data", 4);
    header.data_sz = pcm_size;
    fseek(fp, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(WaveHeader), fp);
}

int main(int argc, char** argv)
{
    int result = 0;
    ERRORTYPE ret = 0;

    if (InitDemux2AdecData() != SUCCESS)
    {
        goto exit;
    }

    if (parseCmdLine(pDemux2AdecData, argc, argv) != 0)
    {
        aloge("parseCmdLine fail!");
        result = -1;
        goto exit;
    }

    if (loadConfigPara(pDemux2AdecData, pDemux2AdecData->confFilePath) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        result = -1;
        goto exit;
    }

    // Open src and dst file
    pDemux2AdecData->srcFd = open(pDemux2AdecData->srcFile, O_RDONLY);
    if (pDemux2AdecData->srcFd < 0)
    {
        aloge("ERROR: cannot open mp4 src file (%s)", pDemux2AdecData->srcFile);
        result = -1;
        goto exit;
    }
    
    pDemux2AdecData->dstFp = fopen(pDemux2AdecData->dstFile, "wb");
    if (pDemux2AdecData->dstFp == NULL)
    {
        aloge("ERROR: cannot open wav dst file (%s)", pDemux2AdecData->dstFile);
        result = -1;
        goto exit;
    }
    fseek(pDemux2AdecData->dstFp, sizeof(WaveHeader), SEEK_SET);

    // Init mpp
    pDemux2AdecData->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pDemux2AdecData->mSysConf);
    AW_MPI_SYS_Init();

    // Create demux and adec channel and bind them
    if (prepare(pDemux2AdecData) != SUCCESS)
    {
        aloge("prepare failed");
    }
    
    // Start mpp
    AW_MPI_CLOCK_Start(pDemux2AdecData->mClockChn);

    if (pDemux2AdecData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_StartRecvStream(pDemux2AdecData->mAdecChn);
    }
    
    ret = AW_MPI_DEMUX_Start(pDemux2AdecData->mDmxChn);
    if (ret != SUCCESS)
    {
        aloge("start play fail");
    }
    
    // wait until encount eof
    alogd("wait until app receive EOF from DMX...");
         
    int nPcmDataSize = 0;
    AUDIO_FRAME_S nFrameInfo;
    while (!pDemux2AdecData->overFlag)
    {
        // get frame from adec
        while (AW_MPI_ADEC_GetFrame(pDemux2AdecData->mAdecChn, &nFrameInfo, 100) != ERR_ADEC_BUF_EMPTY)
        {
            alogd("@ Got decoded data, mLen = %d TimeStamp = %d mId = %d"
                ,nFrameInfo.mLen, nFrameInfo.mTimeStamp, nFrameInfo.mId);
            fwrite(nFrameInfo.mpAddr, 1, nFrameInfo.mLen, pDemux2AdecData->dstFp);
            
            AW_MPI_ADEC_ReleaseFrame(pDemux2AdecData->mAdecChn, &nFrameInfo);
            nPcmDataSize += nFrameInfo.mLen;
        }
    }
    
    writeWaveHeader(&pDemux2AdecData->mDemuxMediaInfo, pDemux2AdecData->dstFp, nPcmDataSize);
    fclose(pDemux2AdecData->dstFp);
    alogd("decode finished!");
    
    // Stop mpp
    ret = AW_MPI_DEMUX_Stop(pDemux2AdecData->mDmxChn);
    
    if (pDemux2AdecData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_StopRecvStream(pDemux2AdecData->mAdecChn);
    }
    
    AW_MPI_CLOCK_Stop(pDemux2AdecData->mClockChn);
    
    if (ret != SUCCESS)
    {
        alogw("stop fail");
    }

    // Destroy channel
    if (pDemux2AdecData->mClockChn >= 0)
    {
        AW_MPI_CLOCK_DestroyChn(pDemux2AdecData->mClockChn);
    }
    if (pDemux2AdecData->mAdecChn >= 0)
    {
        AW_MPI_ADEC_DestroyChn(pDemux2AdecData->mAdecChn);
    }
    if (pDemux2AdecData->mDmxChn >= 0)
    {
        AW_MPI_DEMUX_DestroyChn(pDemux2AdecData->mDmxChn);
    }
    
    // exit mpp system
    AW_MPI_SYS_Exit();
    
    close(pDemux2AdecData->srcFd);
    free(pDemux2AdecData);
    pDemux2AdecData = NULL;

exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
