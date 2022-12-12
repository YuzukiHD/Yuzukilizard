
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_aoSync"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_ao.h>

#include <confparser.h>

#include "sample_aoSync_config.h"
#include "sample_aoSync.h"

#include <cdx_list.h>


static int initSampleAOSyncContext(SampleAOSyncContext *pContext)
{
    memset(pContext, 0, sizeof(SampleAOSyncContext));
    int err = cdx_sem_init(&pContext->mSemExit, 0);
    if(err!=0)
    {
        aloge("cdx mSem Exit init fail!");
    }
    return 0;
}

static int destroySampleAOSyncContext(SampleAOSyncContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemExit);
    return 0;
}

static ERRORTYPE SampleAOSyncCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAOSyncContext *pContext = (SampleAOSyncContext*)cookie;
    if (MOD_ID_AO == pChn->mModId)
    {
        if (pChn->mChnId != pContext->mAOChn)
        {
            aloge("fatal error! AO chnId[%d]!=[%d]", pChn->mChnId, pContext->mAOChn);
        }

        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("AO channel notify APP that play complete!");
                cdx_sem_up(&pContext->mSemExit);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_AO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x]?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

static int ParseCmdLine(int argc, char **argv, SampleAOSyncCmdLineParam *pCmdLinePara)
{
    alogd("sample ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAOSyncCmdLineParam));
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
                "\t-path /home/sample_aoSync.conf\n");
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

static ERRORTYPE loadSampleAOSyncConfig(SampleAOSyncConfig *pConfig, const char *conf_path)
{
    int ret = SUCCESS;

    strcpy(pConfig->mPcmFilePath, "/mnt/E/test.wav");
    pConfig->mSampleRate = 8000;
    pConfig->mChannelCnt = 1;
    pConfig->mBitWidth = 16;;
    pConfig->mFrameSize = 1024;
    pConfig->mAoVolume = 90;
    pConfig->mbDrcEnable = false;

    if(conf_path)
    {
        char *ptr;
        CONFPARSER_S stConfParser;

        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        memset(pConfig, 0, sizeof(SampleAOSyncConfig));
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AO_SYNC_PCM_FILE_PATH, NULL);
        strncpy(pConfig->mPcmFilePath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mPcmFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_PCM_SAMPLE_RATE, 0);
        pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_PCM_BIT_WIDTH, 0);
        pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_PCM_CHANNEL_CNT, 0);
        pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_PCM_FRAME_SIZE, 0);
        pConfig->mAoVolume = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_VOLUME, 0);
        pConfig->mbDrcEnable = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_DRC_ENABLE, 0);
        pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_TEST_DURATION, 0);
        pConfig->mParseWavHeaderEnable = GetConfParaInt(&stConfParser, SAMPLE_AO_SYNC_PARSE_WAV_HEADER_ENABLE, 0);
        destroyConfParser(&stConfParser);
    }
    alogd("param: pcmFilePath[%s], sampleRate[%d], channelCount[%d], bitWidth[%d], frameSize[%d], aoVolume[%d], drcEn[%d], TestDuration[%d], ParseWavHeaderEnable[%d]",
        pConfig->mPcmFilePath, pConfig->mSampleRate, pConfig->mChannelCnt, pConfig->mBitWidth, pConfig->mFrameSize, pConfig->mAoVolume, pConfig->mbDrcEnable,
        pConfig->mTestDuration, pConfig->mParseWavHeaderEnable);
    return SUCCESS;
}

static void config_AIO_ATTR_S_by_SampleAOSyncConfig(AIO_ATTR_S *dst, SampleAOSyncConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->u32ChnCnt = src->mChannelCnt;
    dst->enSamplerate = (AUDIO_SAMPLE_RATE_E)src->mSampleRate;
    dst->enBitwidth = (AUDIO_BIT_WIDTH_E)(src->mBitWidth/8-1);
}

static int ParseWavHeader(FILE *pWavFile, int *pChnNum, int *pSampleRate, int *pBitsPerSample)
{
    struct WaveHeader{
        int riff_id;
        int riff_sz;
        int riff_fmt;
        int fmt_id;
        int fmt_sz;
        short audio_fmt;
        short num_chn;
        int sample_rate;
        int byte_rate;
        short block_align;
        short bits_per_sample;
//        int data_id;
//        int data_sz;
    } header;
    struct RIFFSubChunkHeader
    {
        int mSubChunkId;
        int mSubChunkSize;
    } stSubChunkHeader;

    int nHeaderSize;
    int nRdLen = fread(&header, 1, sizeof(struct WaveHeader), pWavFile);
    if(nRdLen != sizeof(struct WaveHeader))
    {
        aloge("fatal error! read file wrong!");
    }
    if(header.fmt_sz != 16)
    {
        alogd("Be careful! wav header fmt_sz[%d]!= 16, need adjust filepos by fseek [%d]bytes", header.fmt_sz, header.fmt_sz - 16);
        fseek(pWavFile, header.fmt_sz - 16, SEEK_CUR);
    }
    fread(&stSubChunkHeader, 1, sizeof(struct RIFFSubChunkHeader), pWavFile);
    //find data sub-chunk
    while(1)
    {
        if(0 != strncmp((char*)&stSubChunkHeader.mSubChunkId, "data", 4))
        {
            alogd("Be careful! wav header has chunk:[0x%x], size:%d, skip", stSubChunkHeader.mSubChunkId, stSubChunkHeader.mSubChunkSize);
            fseek(pWavFile, stSubChunkHeader.mSubChunkSize, SEEK_CUR);
            //read next sub-chunk
            fread(&stSubChunkHeader, 1, sizeof(struct RIFFSubChunkHeader), pWavFile);
        }
        else
        {
            break;
        }
    }
    if(pChnNum)
    {
        *pChnNum = header.num_chn;
    }
    if(pSampleRate)
    {
        *pSampleRate = header.sample_rate;
    }
    if(pBitsPerSample)
    {
        *pBitsPerSample = header.bits_per_sample;
    }
    nHeaderSize = ftell(pWavFile);
    return nHeaderSize;
}

static void* volumeAdjustThread(void* argv)
{
    SampleAOSyncContext *pContext = (SampleAOSyncContext*)argv;
    char buf[64] = {0};
    char *ptr;
    int num = 0;
    while (!pContext->mOverFlag)
    {
        fgets(buf, 64, stdin);
        if(pContext->mOverFlag)
        {
            alogd("receive exit flag!");
            break;
        }
        // setvol xx(0-100)
        ptr = buf;
        while(*ptr++ != ' ' && *ptr != '\n');
        if (!strncmp(buf, "setvol ", 7))
        {
            num = atoi(ptr);
            alogd("set volume: %d", num);
            AW_MPI_AO_SetDevVolume(0, num);
        }
        else if (!strncmp(buf, "setmute ", 8))
        {
            num = atoi(ptr);
            alogd("set mute: %d", num);
            AW_MPI_AO_SetDevMute(0, num, NULL);
        }
        else if (!strncmp(buf, "getvol", 6))
        {
            int vol;
            AW_MPI_AO_GetDevVolume(0, &vol);
            alogd("getVol: %d", vol);
        }
        else if (!strncmp(buf, "getmute", 7))
        {
            BOOL mute;
            AW_MPI_AO_GetDevMute(0, &mute, NULL);
            alogd("getMute: %d", mute);
        }
        else
        {
            alogd("unknow cmd: %s!", buf);
            alogd("help: setvol/mute x for set volume/mute");
            alogd("      getvol/mute for get volume/mute value");
        }
        memset(buf, 0, 64);
    }
    return NULL;
}

static void *sendAudioFrameThread(void *pThreadData)
{
    SampleAOSyncContext *pContext = (SampleAOSyncContext *)pThreadData;
    ERRORTYPE ret = SUCCESS;
    int nReadLen = 0;
    AUDIO_FRAME_S frameInfo;

    memset(&frameInfo, 0, sizeof(AUDIO_FRAME_S));
    // frameInfo.mId = 0;  /* mId is useless for usr in sync mode */

    /* They do not need to be set again if they have been set by AW_MPI_AO_SetPubAttr() previously. */
    //frameInfo.mSamplerate = (AUDIO_SAMPLE_RATE_E)pContext->mConfigPara.mSampleRate;
    //frameInfo.mBitwidth = (AUDIO_BIT_WIDTH_E)(pContext->mConfigPara.mBitWidth / 8 - 1);

    frameInfo.mBitwidth = pContext->mConfigPara.mBitWidth;  // key: need to set info for every frm
    frameInfo.mSamplerate = pContext->mConfigPara.mSampleRate;

    frameInfo.mSoundmode = (pContext->mConfigPara.mChannelCnt==1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    frameInfo.mLen = pContext->mConfigPara.mChannelCnt * pContext->mConfigPara.mBitWidth / 8 * pContext->mConfigPara.mFrameSize;
    if (0 == frameInfo.mLen)
    {
        aloge("invalid frameInfo.mLen: %d ", frameInfo.mLen);
        return NULL;
    }

    frameInfo.mpAddr = malloc(frameInfo.mLen);
    if (NULL == frameInfo.mpAddr)
    {
        aloge("malloc audio frame buf fail, size: %d ", frameInfo.mLen);
        return NULL;
    }

    while (!pContext->mOverFlag)
    {
        nReadLen = fread(frameInfo.mpAddr, 1, frameInfo.mLen, pContext->mFpPcmFile);
        if (nReadLen < frameInfo.mLen)
        {
            int bEof = feof(pContext->mFpPcmFile);
            if (bEof)
            {
                alogd("read file finish! last read [%d]bytes", nReadLen);
            }
            if (nReadLen > 0)
            {
                frameInfo.mLen = nReadLen;
                ret = AW_MPI_AO_SendFrameSync(pContext->mAODev, pContext->mAOChn, &frameInfo);
                if(ret != SUCCESS)
                {
                    alogw("impossible, send frameId[%d] fail? ret=%d ", frameInfo.mId, ret);
                }
            }
            break;
        }

        /** NOTICE: when testing, do not add any printing, because it will affect the play. */
        // alogd("frameInfo.mLen=%d,%d,%d, %p,%d,%d, %lld \n",
        //     frameInfo.mLen, frameInfo.mBitwidth, frameInfo.mId,
        //     frameInfo.mpAddr, frameInfo.mSamplerate, frameInfo.mSoundmode, frameInfo.mTimeStamp);

        ret = AW_MPI_AO_SendFrameSync(pContext->mAODev, pContext->mAOChn, &frameInfo);
        if (ret != SUCCESS)
        {
            alogw("impossible, send frameId[%d] fail? ret=%d ", frameInfo.mId, ret);
            break;
        }
    }

    if (frameInfo.mpAddr)
    {
        free(frameInfo.mpAddr);
        frameInfo.mpAddr = NULL;
    }

    AW_MPI_AO_SetStreamEof(pContext->mAODev, pContext->mAOChn, 1, 1);
    alogd("exit");

    return NULL;
}

int main(int argc, char *argv[])
{
    int result = 0;
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
    strcpy(stGLogConfig.LogFileNameExtension, "v833-");
    log_init(argv[0], &stGLogConfig);

    alogd("Hello, sample_aoSync!");
    SampleAOSyncContext stContext;
    initSampleAOSyncContext(&stContext);

    //parse command line param
    if (ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(stContext.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }

    //parse config file.
    if(loadSampleAOSyncConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    //open pcm file
    stContext.mFpPcmFile = fopen(stContext.mConfigPara.mPcmFilePath, "rb");
    if(!stContext.mFpPcmFile)
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara.mPcmFilePath);
        result = -1;
        goto _exit;
    }
    else
    {
        if (stContext.mConfigPara.mParseWavHeaderEnable)
        {
            int nChnNum;
            int nSampleRate;
            int nBitsPerSample;
            int nHeaderSize = ParseWavHeader(stContext.mFpPcmFile, &nChnNum, &nSampleRate, &nBitsPerSample);
            alogd("parse wav header size:%d, ChnNum[%d], SampleRate[%d], BitsPerSample[%d]", nHeaderSize, nChnNum, nSampleRate, nBitsPerSample);
            stContext.mConfigPara.mSampleRate = nSampleRate;
            stContext.mConfigPara.mChannelCnt = nChnNum;
            stContext.mConfigPara.mBitWidth = nBitsPerSample;
        }
    }

    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //enable ao dev
    stContext.mAODev = 0;
    //config_AIO_ATTR_S_by_SampleAOSyncConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    //AW_MPI_AO_SetPubAttr(stContext.mAODev, &stContext.mAIOAttr);

    AW_MPI_AO_SetDevVolume(stContext.mAODev, stContext.mConfigPara.mAoVolume);
    alogd("Set Ao Volume:%d", stContext.mConfigPara.mAoVolume);

    //embedded in AW_MPI_AO_EnableChn
    //AW_MPI_AO_Enable(stContext.mAODev);

    //create ao channel
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    stContext.mAOChn = 0;
    while(stContext.mAOChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(stContext.mAODev, stContext.mAOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ao channel[%d] success!", stContext.mAOChn);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            alogd("ao channel[%d] exist, find next!", stContext.mAOChn);
            stContext.mAOChn++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ao not started!");
            break;
        }
        else
        {
            aloge("create ao channel[%d] fail! ret[0x%x]!", stContext.mAOChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAOChn = MM_INVALID_CHN;
        aloge("fatal error! create ao channel fail!");
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAOSyncCallbackWrapper;
    AW_MPI_AO_RegisterCallback(stContext.mAODev, stContext.mAOChn, &cbInfo);

    if(stContext.mConfigPara.mbDrcEnable)
    {
        AO_DRC_CONFIG_S DrcCfg;
        memset(&DrcCfg, 0, sizeof(DrcCfg));
        DrcCfg.sampling_rate = (AUDIO_SAMPLE_RATE_E)stContext.mConfigPara.mSampleRate;
        DrcCfg.attack_time = 1;
        DrcCfg.release_time = 100;
        DrcCfg.max_gain = 6;
        DrcCfg.min_gain = -9;
        DrcCfg.noise_threshold = -45;
        DrcCfg.target_level = -3;
        AW_MPI_AO_EnableSoftDrc(stContext.mAODev, stContext.mAOChn, &DrcCfg);
    }
            
    AW_MPI_AO_StartChn(stContext.mAODev, stContext.mAOChn);

    /* create send audio frame thread */
    stContext.mOverFlag = FALSE;
    int nRet = pthread_create(&stContext.mSendThdId, NULL, sendAudioFrameThread, &stContext);
    if (nRet || !stContext.mSendThdId)
    {
        aloge("create send audio frame thread fail!");
        goto _exit_1;
    }
    nRet = pthread_create(&stContext.mVolumeAdjustThdId, NULL, volumeAdjustThread, &stContext);
    if (nRet || !stContext.mVolumeAdjustThdId)
    {
        aloge("fatal error! pthread create fail");
    }

    if (stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }
    stContext.mOverFlag = TRUE;

    alogd("over, start to free res");
    pthread_join(stContext.mSendThdId, (void*) &nRet);
    pthread_join(stContext.mVolumeAdjustThdId, (void*) &nRet);

_exit_1:
    //stop ao channel
    AW_MPI_AO_StopChn(stContext.mAODev, stContext.mAOChn);
    AW_MPI_AO_DestroyChn(stContext.mAODev, stContext.mAOChn);

    //embedded in AW_MPI_AO_DisableChn
    //AW_MPI_AO_Disable(stContext.mAODev);

    stContext.mAODev = MM_INVALID_DEV;
    stContext.mAOChn = MM_INVALID_CHN;

    AW_MPI_SYS_Exit();

    if (stContext.mFpPcmFile)
    {
        fclose(stContext.mFpPcmFile);
        stContext.mFpPcmFile = NULL;
    }

_exit:
    destroySampleAOSyncContext(&stContext);
    if (result == 0) {
        printf("sample_aoSync exit!\n");
    }
    log_quit();
    return result;
}

