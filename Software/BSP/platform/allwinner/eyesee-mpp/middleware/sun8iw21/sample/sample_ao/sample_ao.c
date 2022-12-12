
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_ao"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_ao.h>
#include <ClockCompPortIndex.h>

#include <confparser.h>

#include "sample_ao_config.h"
#include "sample_ao.h"

#include <cdx_list.h>

AUDIO_FRAME_S* SampleAOFrameManager_PrefetchFirstIdleFrame(void *pThiz)
{
    SampleAOFrameManager *pFrameManager = (SampleAOFrameManager*)pThiz;
    SampleAOFrameNode *pFirstNode;
    AUDIO_FRAME_S *pFrameInfo;
    pthread_mutex_lock(&pFrameManager->mLock);
    if(!list_empty(&pFrameManager->mIdleList))
    {
        pFirstNode = list_first_entry(&pFrameManager->mIdleList, SampleAOFrameNode, mList);
        pFrameInfo = &pFirstNode->mAFrame;
    }
    else
    {
        pFrameInfo = NULL;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return pFrameInfo;
}

int SampleAOFrameManager_UseFrame(void *pThiz, AUDIO_FRAME_S *pFrame)
{
    int ret = 0;
    SampleAOFrameManager *pFrameManager = (SampleAOFrameManager*)pThiz;
    if(NULL == pFrame)
    {
        aloge("fatal error! pNode == NULL!");
        return -1;
    }
    pthread_mutex_lock(&pFrameManager->mLock);
    SampleAOFrameNode *pFirstNode = list_first_entry_or_null(&pFrameManager->mIdleList, SampleAOFrameNode, mList);
    if(pFirstNode)
    {
        if(&pFirstNode->mAFrame == pFrame)
        {
            list_move_tail(&pFirstNode->mList, &pFrameManager->mUsingList);
        }
        else
        {
            aloge("fatal error! node is not match [%p]!=[%p]", pFrame, &pFirstNode->mAFrame);
            ret = -1;
        }
    }
    else
    {
        aloge("fatal error! idle list is empty");
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return ret;
}

int SampleAOFrameManager_ReleaseFrame(void *pThiz, unsigned int nFrameId)
{
    int ret = 0;
    SampleAOFrameManager *pFrameManager = (SampleAOFrameManager*)pThiz;
    pthread_mutex_lock(&pFrameManager->mLock);
    int bFindFlag = 0;
    SampleAOFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mUsingList, mList)
    {
        if(pEntry->mAFrame.mId == nFrameId)
        {
            list_move_tail(&pEntry->mList, &pFrameManager->mIdleList);
            bFindFlag = 1;
            break;
        }
    }
    if(0 == bFindFlag)
    {
        aloge("fatal error! frameId[%d] is not find", nFrameId);
        ret = -1;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return ret;
}

int initSampleAOFrameManager(SampleAOFrameManager *pFrameManager, int nFrameNum, SampleAOConfig *pConfigPara, int nBufSize)
{
    memset(pFrameManager, 0, sizeof(SampleAOFrameManager));
    int err = pthread_mutex_init(&pFrameManager->mLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    INIT_LIST_HEAD(&pFrameManager->mIdleList);
    INIT_LIST_HEAD(&pFrameManager->mUsingList);

    int i;
    SampleAOFrameNode *pNode;
    for (i=0; i<nFrameNum; i++)
    {
        pNode = malloc(sizeof(SampleAOFrameNode));
        memset(pNode, 0, sizeof(SampleAOFrameNode));
        pNode->mAFrame.mId = i;
        pNode->mAFrame.mBitwidth = (AUDIO_BIT_WIDTH_E)(pConfigPara->mBitWidth/8 - 1);
		pNode->mAFrame.mSamplerate = pConfigPara->mSampleRate; 
        pNode->mAFrame.mSoundmode = (pConfigPara->mChannelCnt==1)?AUDIO_SOUND_MODE_MONO:AUDIO_SOUND_MODE_STEREO;
        pNode->mAFrame.mLen = nBufSize>0?nBufSize:pConfigPara->mChannelCnt*pConfigPara->mBitWidth/8*pConfigPara->mFrameSize;
        pNode->mAFrame.mpAddr = malloc(pNode->mAFrame.mLen);
        list_add_tail(&pNode->mList, &pFrameManager->mIdleList);
    }
    pFrameManager->mNodeCnt = nFrameNum;

    pFrameManager->PrefetchFirstIdleFrame = SampleAOFrameManager_PrefetchFirstIdleFrame;
    pFrameManager->UseFrame = SampleAOFrameManager_UseFrame;
    pFrameManager->ReleaseFrame = SampleAOFrameManager_ReleaseFrame;
    return 0;
}

int destroySampleAOFrameManager(SampleAOFrameManager *pFrameManager)
{
    if(!list_empty(&pFrameManager->mUsingList))
    {
        aloge("fatal error! why using list is not empty");
    }
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pFrameManager->mIdleList)
    {
        cnt++;
    }
    if(cnt != pFrameManager->mNodeCnt)
    {
        aloge("fatal error! frame count is not match [%d]!=[%d]", cnt, pFrameManager->mNodeCnt);
    }
    SampleAOFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mIdleList, mList)
    {
        free(pEntry->mAFrame.mpAddr);
        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_destroy(&pFrameManager->mLock);
    return 0;
}

int initSampleAOContext(SampleAOContext *pContext)
{
    memset(pContext, 0, sizeof(SampleAOContext));
    int err = pthread_mutex_init(&pContext->mWaitFrameLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    err = cdx_sem_init(&pContext->mSemFrameCome, 0);
    err = cdx_sem_init(&pContext->mSemEofCome, 0);
    if(err!=0)
    {
        aloge("cdx sem init fail!");
    }
    return 0;
}

int destroySampleAOContext(SampleAOContext *pContext)
{
    pthread_mutex_destroy(&pContext->mWaitFrameLock);
    cdx_sem_deinit(&pContext->mSemFrameCome);
    cdx_sem_deinit(&pContext->mSemEofCome);
    return 0;
}

static ERRORTYPE SampleAOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAOContext *pContext = (SampleAOContext*)cookie;
    if(MOD_ID_AO == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mAOChn)
        {
            aloge("fatal error! AO chnId[%d]!=[%d]", pChn->mChnId, pContext->mAOChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_AUDIO_BUFFER:
            {
                AUDIO_FRAME_S *pAFrame = (AUDIO_FRAME_S*)pEventData;
                pContext->mFrameManager.ReleaseFrame(&pContext->mFrameManager, pAFrame->mId);
                pthread_mutex_lock(&pContext->mWaitFrameLock);
                if(pContext->mbWaitFrameFlag)
                {
                    pContext->mbWaitFrameFlag = 0;
                    cdx_sem_up(&pContext->mSemFrameCome);
                }
                pthread_mutex_unlock(&pContext->mWaitFrameLock);
                break;
            }
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("AO channel notify APP that play complete!");
                cdx_sem_up(&pContext->mSemEofCome);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
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

static ERRORTYPE SampleAO_CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    alogw("clock channel[%d] has some event[0x%x]", pChn->mChnId, event);
    return SUCCESS;
}

static int ParseCmdLine(int argc, char **argv, SampleAOCmdLineParam *pCmdLinePara)
{
    alogd("sample ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAOCmdLineParam));
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
                "\t-path /home/sample_ao.conf\n");
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

static ERRORTYPE loadSampleAOConfig(SampleAOConfig *pConfig, const char *conf_path)
{
    int ret = SUCCESS;

    strcpy(pConfig->mPcmFilePath, "/mnt/extsd/test.wav");
    pConfig->mSampleRate = 8000;
    pConfig->mChannelCnt = 1;
    pConfig->mBitWidth = 16;;
    pConfig->mFrameSize = 1024;
    pConfig->mAoVolume = 90;
    pConfig->mAoSaveDataFlag = false;

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
        memset(pConfig, 0, sizeof(SampleAOConfig));
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AO_PCM_FILE_PATH, NULL);
        strncpy(pConfig->mPcmFilePath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mPcmFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_SAMPLE_RATE, 0);
        pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_BIT_WIDTH, 0);
        pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_CHANNEL_CNT, 0);
        pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_FRAME_SIZE, 0);
        pConfig->mAoVolume = GetConfParaInt(&stConfParser, SAMPLE_AO_VOLUME, 0);
        pConfig->log_level = GetConfParaInt(&stConfParser, SAMPLE_AO_LOG_LEVEL, 0);
        destroyConfParser(&stConfParser);
    }
    alogd("param: pcmFilePath[%s], sampleRate[%d], channelCount[%d], bitWidth[%d], frameSize[%d], aoVolume[%d]",
        pConfig->mPcmFilePath, pConfig->mSampleRate, pConfig->mChannelCnt, pConfig->mBitWidth, pConfig->mFrameSize, pConfig->mAoVolume);
    return SUCCESS;
}

void config_AIO_ATTR_S_by_SampleAOConfig(AIO_ATTR_S *dst, SampleAOConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->u32ChnCnt = src->mChannelCnt;
    dst->enSamplerate = (AUDIO_SAMPLE_RATE_E)src->mSampleRate;
    dst->enBitwidth = (AUDIO_BIT_WIDTH_E)(src->mBitWidth/8-1);
}

void* volumeAdjustThread(void* argv)
{
    int *exitFlag = (int*)argv;
    char buf[64] = {0};
    char *ptr;
    while (!*exitFlag) {
        fgets(buf, 64, stdin);
        // setvol xx(0-100)
        ptr = buf;
        while(*ptr++ != ' ' && *ptr != '\n');
        if (!strncmp(buf, "setvol ", 7)) {
            AW_MPI_AO_SetDevVolume(0, atoi(ptr));
        } else if (!strncmp(buf, "setmute ", 8)) {
            AW_MPI_AO_SetDevMute(0, atoi(ptr), NULL);
        } else if (!strncmp(buf, "getvol", 6)) {
            int vol;
            AW_MPI_AO_GetDevVolume(0, &vol);
            alogd("getVol: %d", vol);
        } else if (!strncmp(buf, "getmute", 7)) {
            BOOL mute;
            AW_MPI_AO_GetDevMute(0, &mute, NULL);
            alogd("getMute: %d", mute);
        } else {
            alogd("unknow cmd: %s!", buf);
            alogd("help: setvol/mute x for set volume/mute");
            alogd("      getvol/mute for get volume/mute value");
        }
        memset(buf, 0, 64);
    }
    return NULL;
}
/**
 * @return: wav header size.
 */
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
int main(int argc, char *argv[])
{
    int result = 0;
    alogd("Hello, sample_ao!");
    SampleAOContext stContext;
    initSampleAOContext(&stContext);
    //parse command line param
    if(ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
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
    if(loadSampleAOConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 0,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    stGLogConfig.FLAGS_stderrthreshold = stContext.mConfigPara.log_level;
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "v833-");
    log_init(argv[0], &stGLogConfig);

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
        int nChnNum;
        int nSampleRate;
        int nBitsPerSample;
        int nHeaderSize = ParseWavHeader(stContext.mFpPcmFile, &nChnNum, &nSampleRate, &nBitsPerSample);
        alogd("parse wav header size:%d, ChnNum[%d], SampleRate[%d], BitsPerSample[%d]", nHeaderSize, nChnNum, nSampleRate, nBitsPerSample);
        //fseek(stContext.mFpPcmFile, 44, SEEK_SET);  // 44: size(WavHeader)
        stContext.mConfigPara.mSampleRate = nSampleRate;
        stContext.mConfigPara.mChannelCnt = nChnNum;
        stContext.mConfigPara.mBitWidth = nBitsPerSample;
    }
    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();
    //init frame manager
    initSampleAOFrameManager(&stContext.mFrameManager, 5, &stContext.mConfigPara, 0);

    //enable ao dev
    stContext.mAODev = 0;
    /* config_AIO_ATTR_S_by_SampleAOConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    AW_MPI_AO_SetPubAttr(stContext.mAODev, &stContext.mAIOAttr); */
    //embedded in AW_MPI_AO_CreateChn
    //AW_MPI_AO_Enable(stContext.mAODev);

    //create ao channel and clock channel.
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
    cbInfo.callback = (MPPCallbackFuncType)&SampleAOCallbackWrapper;
    AW_MPI_AO_RegisterCallback(stContext.mAODev, stContext.mAOChn, &cbInfo);
#if 0
    bSuccessFlag = FALSE;
    stContext.mClockChnAttr.nWaitMask = 0;
    stContext.mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
    stContext.mClockChn = 0;
    while(stContext.mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(stContext.mClockChn, &stContext.mClockChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", stContext.mClockChn);
            break;
        }
        else if(ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", stContext.mClockChn);
            stContext.mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", stContext.mClockChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
    }
    AO_DRC_CONFIG_S DrcCfg;
    DrcCfg.sampling_rate = 16000;
    DrcCfg.attack_time = 1;
    DrcCfg.release_time = 100;
    DrcCfg.max_gain = 6;
    DrcCfg.min_gain = -9;
    DrcCfg.noise_threshold = -45;
    DrcCfg.target_level = -3;
    AW_MPI_AO_EnableSoftDrc(stContext.mAODev, stContext.mAOChn,&DrcCfg);
    AO_AGC_CONFIG_S AgcCfg;
    AgcCfg.fSample_rate = 16;
    AgcCfg.iSample_len = 1024;
    AgcCfg.iBytePerSample = 2;
    AgcCfg.iGain_level = 1;
    AgcCfg.iChannel = 1;
    AW_MPI_AO_EnableAgc(stContext.mAODev, stContext.mAOChn,&AgcCfg);

    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAO_CLOCKCallbackWrapper;
    AW_MPI_CLOCK_RegisterCallback(stContext.mClockChn, &cbInfo);
    //bind clock and ao
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, stContext.mClockChn};
    MPP_CHN_S AOChn = {MOD_ID_AO, stContext.mAODev, stContext.mAOChn};
    AW_MPI_SYS_Bind(&ClockChn, &AOChn);
#endif
    //test ao save file api
    if(stContext.mConfigPara.mAoSaveDataFlag)
    {
        AUDIO_SAVE_FILE_INFO_S info;
        strcpy(info.mFilePath, "/mnt/extsd/");
        strcpy(info.mFileName, "SampleAo_AoSaveFile.pcm");
        AW_MPI_AO_SaveFile(0,0,&info);
    }

    //start ao and clock.
    //AW_MPI_CLOCK_Start(stContext.mClockChn);
    AW_MPI_AO_StartChn(stContext.mAODev, stContext.mAOChn);
    int nAoVolume = 0;
    AW_MPI_AO_GetDevVolume(stContext.mAODev, &nAoVolume);
    alogd("Get Ao Volume:%d, set:%d", nAoVolume, stContext.mConfigPara.mAoVolume);
    AW_MPI_AO_SetDevVolume(stContext.mAODev, stContext.mConfigPara.mAoVolume);

    //read pcm from file, play pcm through mpi_ao. we set pts by stContext.mConfigPara(mSampleRate,mFrameSize).
    uint64_t nPts = 0;   //unit:us
    int nFrameSize = stContext.mConfigPara.mFrameSize;
    int nSampleRate = stContext.mConfigPara.mSampleRate;
    uint64_t nFrameInterval = 1000000*nFrameSize/nSampleRate; //128000us
    AUDIO_FRAME_S *pFrameInfo;
    int nReadLen;
    pthread_t tid;
    int exit = 0;
    pthread_create(&tid, NULL, volumeAdjustThread, &exit);
    while(1)
    {
        //request idle frame
        pFrameInfo = stContext.mFrameManager.PrefetchFirstIdleFrame(&stContext.mFrameManager);
        if(NULL == pFrameInfo)
        {
            pthread_mutex_lock(&stContext.mWaitFrameLock);
            pFrameInfo = stContext.mFrameManager.PrefetchFirstIdleFrame(&stContext.mFrameManager);
            if(pFrameInfo!=NULL)
            {
                pthread_mutex_unlock(&stContext.mWaitFrameLock);
            }
            else
            {
                stContext.mbWaitFrameFlag = 1;
                pthread_mutex_unlock(&stContext.mWaitFrameLock);
                cdx_sem_down_timedwait(&stContext.mSemFrameCome, 500);
                continue;
            }
        }
        
        //read pcm to idle frame
        //int nWantedReadLen = nFrameSize * stContext.mConfigPara.mChannelCnt * (stContext.mConfigPara.mBitWidth/8);
        int nWantedReadLen = pFrameInfo->mLen;
        nReadLen = fread(pFrameInfo->mpAddr, 1, nWantedReadLen, stContext.mFpPcmFile);
        if(nReadLen < nWantedReadLen)
        {
            int bEof = feof(stContext.mFpPcmFile);
            if(bEof)
            {
                alogd("read file finish! last read [%d]bytes", nReadLen);
            }
            if(nReadLen > 0)
            {
                pFrameInfo->mLen = nReadLen;
                stContext.mFrameManager.UseFrame(&stContext.mFrameManager, pFrameInfo);
                ret = AW_MPI_AO_SendFrame(stContext.mAODev, stContext.mAOChn, pFrameInfo, 0);
                if(ret != SUCCESS)
                {
                    alogw("impossible, send frameId[%d] fail?", pFrameInfo->mId);
                    stContext.mFrameManager.ReleaseFrame(&stContext.mFrameManager, pFrameInfo->mId);
                }
            }
            break;
        }
        //pFrameInfo->mTimeStamp = nPts;
        //nPts += nReadLen*1000000/(stContext.mConfigPara.mChannelCnt * (stContext.mConfigPara.mBitWidth/8))/stContext.mConfigPara.mSampleRate;
        //pFrameInfo->mId = pFrameInfo->mTimeStamp / nFrameInterval;
        stContext.mFrameManager.UseFrame(&stContext.mFrameManager, pFrameInfo);

        //send pcm to ao
        ret = AW_MPI_AO_SendFrame(stContext.mAODev, stContext.mAOChn, pFrameInfo, 0);
        if(ret != SUCCESS)
        {
            alogw("impossible, send frameId[%d] fail?", pFrameInfo->mId);
            stContext.mFrameManager.ReleaseFrame(&stContext.mFrameManager, pFrameInfo->mId);
        }
    }
    AW_MPI_AO_SetStreamEof(stContext.mAODev, stContext.mAOChn, 1, 1);
    cdx_sem_down(&stContext.mSemEofCome);
    exit = 1;
    pthread_join(tid, NULL);

    //stop ao channel, clock channel
    AW_MPI_AO_StopChn(stContext.mAODev, stContext.mAOChn);
    //AW_MPI_CLOCK_Stop(stContext.mClockChn);
    AW_MPI_AO_DestroyChn(stContext.mAODev, stContext.mAOChn);
    stContext.mAODev = MM_INVALID_DEV;
    stContext.mAOChn = MM_INVALID_CHN;
    //AW_MPI_CLOCK_DestroyChn(stContext.mClockChn);
    //stContext.mClockChn = MM_INVALID_CHN;
    destroySampleAOFrameManager(&stContext.mFrameManager);
    //exit mpp system
    AW_MPI_SYS_Exit();
    //close pcm file
    fclose(stContext.mFpPcmFile);
    stContext.mFpPcmFile = NULL;

_exit:
    destroySampleAOContext(&stContext);
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
