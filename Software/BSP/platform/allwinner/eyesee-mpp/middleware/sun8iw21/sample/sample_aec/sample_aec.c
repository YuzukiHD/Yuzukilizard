
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_aec"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <mm_common.h>
#include <media_common_aio.h>
#include <mpi_sys.h>
#include <mpi_ao.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>

#include <ClockCompPortIndex.h>

#include <confparser.h>

#include "sample_aec_config.h"
#include "sample_aec.h"

#include <cdx_list.h>

static SampleAecContext *gpSampleAecContext = NULL;

static AUDIO_FRAME_S* SampleAecFrameManager_PrefetchFirstIdleFrame(void *pThiz)
{
    SampleAecFrameManager *pFrameManager = (SampleAecFrameManager*)pThiz;
    SampleAecFrameNode *pFirstNode;
    AUDIO_FRAME_S *pFrameInfo;
    pthread_mutex_lock(&pFrameManager->mLock);
    if(!list_empty(&pFrameManager->mIdleList))
    {
        pFirstNode = list_first_entry(&pFrameManager->mIdleList, SampleAecFrameNode, mList);
        pFrameInfo = &pFirstNode->mAFrame;
    }
    else
    {
        pFrameInfo = NULL;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return pFrameInfo;
}

static int SampleAecFrameManager_UseFrame(void *pThiz, AUDIO_FRAME_S *pFrame)
{
    int ret = 0;
    SampleAecFrameManager *pFrameManager = (SampleAecFrameManager*)pThiz;
    if(NULL == pFrame)
    {
        aloge("fatal error! pNode == NULL!");
        return -1;
    }
    pthread_mutex_lock(&pFrameManager->mLock);
    SampleAecFrameNode *pFirstNode = list_first_entry_or_null(&pFrameManager->mIdleList, SampleAecFrameNode, mList);
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

static int SampleAecFrameManager_ReleaseFrame(void *pThiz, unsigned int nFrameId)
{
    int ret = 0;
    SampleAecFrameManager *pFrameManager = (SampleAecFrameManager*)pThiz;
    pthread_mutex_lock(&pFrameManager->mLock);
    int bFindFlag = 0;
    SampleAecFrameNode *pEntry, *pTmp;
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

int initSampleAecFrameManager(SampleAecFrameManager *pFrameManager, int nFrameNum, SampleAecConfig *pConfigPara, SampleAecContext* pCtx)
{
    memset(pFrameManager, 0, sizeof(SampleAecFrameManager));
    int err = pthread_mutex_init(&pFrameManager->mLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    INIT_LIST_HEAD(&pFrameManager->mIdleList);
    INIT_LIST_HEAD(&pFrameManager->mUsingList);

    int i;
    SampleAecFrameNode *pNode;
    for (i=0; i<nFrameNum; i++)
    {
        pNode = malloc(sizeof(SampleAecFrameNode));
        memset(pNode, 0, sizeof(SampleAecFrameNode));
        pNode->mAFrame.mId = i;
        pNode->mAFrame.mSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pCtx->nPlaySampleRate);
        pNode->mAFrame.mBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(pCtx->nPlayBitsPerSample);
        pNode->mAFrame.mSoundmode = (pCtx->nPlayChnNum==1)?AUDIO_SOUND_MODE_MONO:AUDIO_SOUND_MODE_STEREO;
        pNode->mAFrame.mLen = pCtx->nPlayChnNum * pCtx->nPlayBitsPerSample/8 * pConfigPara->mFrameSize;
        pNode->mAFrame.mpAddr = malloc(pNode->mAFrame.mLen);
        list_add_tail(&pNode->mList, &pFrameManager->mIdleList);
    }
    pFrameManager->mNodeCnt = nFrameNum;

    pFrameManager->PrefetchFirstIdleFrame = SampleAecFrameManager_PrefetchFirstIdleFrame;
    pFrameManager->UseFrame = SampleAecFrameManager_UseFrame;
    pFrameManager->ReleaseFrame = SampleAecFrameManager_ReleaseFrame;
    return 0;
}

int destroySampleAecFrameManager(SampleAecFrameManager *pFrameManager)
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
    SampleAecFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mIdleList, mList)
    {
        free(pEntry->mAFrame.mpAddr);
        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_destroy(&pFrameManager->mLock);
    return 0;
}

int initSampleAecContext(SampleAecContext *pContext)
{
    memset(pContext, 0, sizeof(SampleAecContext));
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

int destroySampleAecContext(SampleAecContext *pContext)
{
    pthread_mutex_destroy(&pContext->mWaitFrameLock);
    cdx_sem_deinit(&pContext->mSemFrameCome);
    cdx_sem_deinit(&pContext->mSemEofCome);
    return 0;
}

static ERRORTYPE SampleAec_CallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAecContext *pContext = (SampleAecContext*)cookie;
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
                pContext->eof_flag = 1;
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
    else if (MOD_ID_AI == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mAIChn)
        {
            aloge("fatal error! AI chnId[%d]!=[%d]", pChn->mChnId, pContext->mAIChn);
        }
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo *pDataInfo = (AISendDataInfo*)pEventData;
                //alogd("AIChannel transport pcm:%d bytes, ignore:%d, pts:%lldms", pDataInfo->mLen, pDataInfo->mbIgnore, pDataInfo->mPts/1000);
                break;
            }
            default:
            alogw("ai chn should not send callback event[0x%x]!", event);
            break;
        }
    }
    else if (MOD_ID_CLOCK == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mClockChn)
        {
            aloge("fatal error! CLK chnId[%d]!=[%d]", pChn->mChnId, pContext->mClockChn);
        }
        switch(event)
        {
            default:
            alogw("clk chn should not send callback event[0x%x]!", event);
            break;
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x] callback?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}


static int ParseCmdLine(int argc, char **argv, SampleAecCmdLineParam *pCmdLinePara)
{
    alogd("sample ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAecCmdLineParam));
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
                "\t-path /home/sample_aec.conf\n");
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

static ERRORTYPE loadSampleAecConfig(SampleAecConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    strcpy(pConfig->mPcmSrcPath, "/mnt/extsd/sample_aoref_8000_ch1_bit16_aec_30s.wav");
    strcpy(pConfig->mPcmDstPath, "/mnt/extsd/ai_cap.wav");
    pConfig->mSampleRate = 8000;
    pConfig->mChannelCnt = 1;
    pConfig->mBitWidth = 16;
    pConfig->mFrameSize = 1024;
    pConfig->mAiAecEn = 1;
    pConfig->mAecDelayMs = 0;
    pConfig->mAiAnsEn = 0;
    pConfig->mAiAnsMode = 0;
    pConfig->mAiAgcEn = 0;
    pConfig->mAiAgcGain = 0;
    pConfig->mbAddWavHeader = true;

    if(conf_path != NULL)
    {
        char *ptr;
        CONFPARSER_S stConfParser;

        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        memset(pConfig, 0, sizeof(SampleAecConfig));
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AEC_PCM_SRC_PATH, NULL);
        strncpy(pConfig->mPcmSrcPath, ptr, MAX_FILE_PATH_SIZE-1);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AEC_PCM_DST_PATH, NULL);
        strncpy(pConfig->mPcmDstPath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_SAMPLE_RATE, 0);
        pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_BIT_WIDTH, 0);
        pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_CHANNEL_CNT, 0);
        pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_FRAME_SIZE, 0);
        pConfig->mAecDelayMs = GetConfParaInt(&stConfParser, SAMPLE_AEC_AEC_DELAY_MS, 0);
        pConfig->mAiAecEn = GetConfParaInt(&stConfParser, SAMPLE_AEC_AEC_ENABLE, 0);
        pConfig->mAiAnsEn = GetConfParaInt(&stConfParser, SAMPLE_AEC_ANS_ENABLE, 0);
        pConfig->mAiAnsMode = GetConfParaInt(&stConfParser, SAMPLE_AEC_ANS_MODE, 0);
        pConfig->mAiAgcEn = GetConfParaInt(&stConfParser, SAMPLE_AEC_AGC_ENABLE, 0);
        pConfig->mAiAgcGain = GetConfParaInt(&stConfParser, SAMPLE_AEC_AGC_GAIN, 0);
        pConfig->mbAddWavHeader = (bool)GetConfParaInt(&stConfParser, SAMPLE_AEC_ADD_WAV_HEADER, 0);
        destroyConfParser(&stConfParser);
    }
    alogd("config:%s-%d-%d-%d-%d-%d", pConfig->mPcmSrcPath, pConfig->mSampleRate, pConfig->mChannelCnt,
        pConfig->mAiAecEn, pConfig->mAiAnsEn, pConfig->mAiAgcEn);
    return SUCCESS;
}

void config_AIO_ATTR_S_by_SampleAecConfig(AIO_ATTR_S *dst, SampleAecConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->u32ChnCnt = src->mChannelCnt;
    dst->enSamplerate = (AUDIO_SAMPLE_RATE_E)src->mSampleRate;
    dst->enBitwidth = (AUDIO_BIT_WIDTH_E)(src->mBitWidth/8-1);
    dst->aec_delay_ms = src->mAecDelayMs;
    dst->ai_aec_en = src->mAiAecEn;
    dst->ai_ans_en = src->mAiAnsEn;
    dst->ai_ans_mode = src->mAiAnsMode;
    dst->ai_agc_en = src->mAiAgcEn;
    if(dst->ai_agc_en)
    {
        dst->ai_agc_cfg.fSample_rate = dst->enSamplerate;
        dst->ai_agc_cfg.iBytePerSample = src->mBitWidth/8;
        dst->ai_agc_cfg.iChannel = src->mChannelCnt;
        dst->ai_agc_cfg.iSample_len = 1024;
        dst->ai_agc_cfg.iGain_level = src->mAiAgcGain;
    }
}

static void PcmDataAddWaveHeader(SampleAecContext *pContext)
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
        int data_id;
        int data_sz;
    } header;
    SampleAecConfig *pConf = &pContext->mConfigPara;

    memcpy(&header.riff_id, "RIFF", 4);
    header.riff_sz = pContext->mSavePcmSize + sizeof(struct WaveHeader) - 8;
    memcpy(&header.riff_fmt, "WAVE", 4);
    memcpy(&header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // s16le
    header.num_chn = pConf->mChannelCnt;
    header.sample_rate = pConf->mSampleRate;
    header.byte_rate = pConf->mSampleRate * pConf->mChannelCnt * pConf->mBitWidth/8;
    header.block_align = pConf->mChannelCnt * pConf->mBitWidth/8;
    header.bits_per_sample = pConf->mBitWidth;
    memcpy(&header.data_id, "data", 4);
    header.data_sz = pContext->mSavePcmSize;

    fseek(pContext->mFpSaveWavFile, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(struct WaveHeader), pContext->mFpSaveWavFile);
}

static void* AiSaveDataThread(void *param)
{
    SampleAecContext *pCtx = (SampleAecContext*)param;
    AUDIO_FRAME_S frame;
    ERRORTYPE ret = SUCCESS;
    int nWriteLen;
    pCtx->mFpSaveWavFile = fopen(pCtx->mConfigPara.mPcmDstPath, "wb");
    alogd("AiSaveDataThread cap data file: %s, dst_fp:%p", pCtx->mConfigPara.mPcmDstPath, pCtx->mFpSaveWavFile);
    if(pCtx->mConfigPara.mbAddWavHeader)
    {
        fseek(pCtx->mFpSaveWavFile, 44, SEEK_SET);  // 44: size(WavHeader)
    }
    while (!pCtx->mOverFlag)
    {
        ret = AW_MPI_AI_GetFrame(pCtx->mAIODev, pCtx->mAIChn, &frame, NULL, 500);
        if(SUCCESS == ret)
        {
            if(NULL != pCtx->mFpSaveWavFile)
            {
                nWriteLen = fwrite(frame.mpAddr, 1, frame.mLen, pCtx->mFpSaveWavFile);
                if(nWriteLen != frame.mLen)
                {
                    aloge("fatal error! fwrite[%d]!=[%d]", frame.mLen, nWriteLen);
                }
                pCtx->mSavePcmSize += nWriteLen;
            }
            AW_MPI_AI_ReleaseFrame(pCtx->mAIODev, pCtx->mAIChn, &frame, NULL);
        }
        else if(ERR_AI_BUF_EMPTY == ret)
        {
            alogw("Be careful! ai getFrame timeout?");
        }
        else
        {
            aloge("fatal error! ai getFrame fail[0x%x]", ret);
        }
    }
    if(pCtx->mConfigPara.mbAddWavHeader)
    {
        PcmDataAddWaveHeader(pCtx);
    }
    fclose(pCtx->mFpSaveWavFile);
    pCtx->mFpSaveWavFile = NULL;

    return NULL;
}

static void *SendPcmAoThread(void *pThreadData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAecContext *pContext = (SampleAecContext *)pThreadData;
    //read pcm from file, play pcm through mpi_ao. we set pts by stContext.mConfigPara(mSampleRate,mFrameSize).
    uint64_t nPts = 0;   //unit:us

    AUDIO_FRAME_S *pFrameInfo = NULL;
    int nReadLen = 0;
    int nReadTotalLen = 0;
    int nWantedReadLen = pContext->mConfigPara.mFrameSize * pContext->mConfigPara.mChannelCnt * (pContext->mConfigPara.mBitWidth/8);
    unsigned int nIdCounter = 0;
    if(pContext->mPauseDuration > 0)
    {
        alogd("wait [%d]ms before send pcm!", pContext->mPauseDuration);
        usleep(pContext->mPauseDuration*1000);
    }
    while(1)
    {
        if(pContext->mOverFlag)
        {
            alogd("send_pcm_ao_thread receive exit flag!");
            break;
        }
        //request idle frame
        pFrameInfo = pContext->mFrameManager.PrefetchFirstIdleFrame(&pContext->mFrameManager);
        if(NULL == pFrameInfo)
        {
            pthread_mutex_lock(&pContext->mWaitFrameLock);
            pFrameInfo = pContext->mFrameManager.PrefetchFirstIdleFrame(&pContext->mFrameManager);
            if(pFrameInfo!=NULL)
            {
                pthread_mutex_unlock(&pContext->mWaitFrameLock);
            }
            else
            {
                pContext->mbWaitFrameFlag = 1;
                pthread_mutex_unlock(&pContext->mWaitFrameLock);
                cdx_sem_down_timedwait(&pContext->mSemFrameCome, 500);
                continue;
            }
        }

        //read pcm to idle frame
        nReadLen = fread(pFrameInfo->mpAddr, 1, nWantedReadLen, pContext->mFpPcmFile);
        if(nReadLen < nWantedReadLen)
        {
            int bEof = feof(pContext->mFpPcmFile);
            if(bEof)
            {
                alogd("read file finish!");
            }
            break;
        }
        nReadTotalLen += nReadLen;
        int nReadTotalSample = nReadTotalLen/(pContext->mConfigPara.mChannelCnt * pContext->mConfigPara.mBitWidth/8);
        pFrameInfo->mTimeStamp = nPts;
        nPts = (uint64_t)nReadTotalSample*1000*1000/(pContext->mConfigPara.mSampleRate);
        pFrameInfo->mId = nIdCounter++;
        pContext->mFrameManager.UseFrame(&pContext->mFrameManager, pFrameInfo);

        //send pcm to ao
        ret = AW_MPI_AO_SendFrame(pContext->mAIODev, pContext->mAOChn, pFrameInfo, 0);
        if(ret != SUCCESS)
        {
            aloge("impossible, send frameId[%d] fail?", pFrameInfo->mId);
            pContext->mFrameManager.ReleaseFrame(&pContext->mFrameManager, pFrameInfo->mId);
        }
        if(false==pContext->mbPauseDone && pContext->mPauseDuration>0 && nPts/1000>=pContext->mPauseSendTm)
        {
            pContext->mbPauseDone = true;
            alogd("pause sending pcm for [%d]ms", pContext->mPauseDuration);
            usleep(pContext->mPauseDuration*1000);
        }
    }
    AW_MPI_AO_SetStreamEof(pContext->mAIODev, pContext->mAOChn, TRUE, TRUE);

    while(!pContext->eof_flag)
    {
        usleep(100*1000);
    }
    pContext->mOverFlag = TRUE;
    return (void*)ret;
}

static void handle_exit()
{
    alogd("user want to exit!");
    if(NULL != gpSampleAecContext)
    {
        gpSampleAecContext->mOverFlag = TRUE;
        cdx_sem_up(&gpSampleAecContext->mSemEofCome);
    }
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
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);

    alogd("Hello, sample_aec!");
    SampleAecContext stContext;
    initSampleAecContext(&stContext);
    gpSampleAecContext = &stContext;
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
    if(loadSampleAecConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    stContext.mPauseSendTm = 5000;
    stContext.mPauseDuration = 0;
    stContext.mbPauseDone = true;
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }
    //open pcm file
    stContext.mFpPcmFile = fopen(stContext.mConfigPara.mPcmSrcPath, "rb");
    if(!stContext.mFpPcmFile)
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara.mPcmSrcPath);
        result = -1;
        goto _exit;
    }
    else
    {
        int nHeaderSize = ParseWavHeader(stContext.mFpPcmFile, &stContext.nPlayChnNum, &stContext.nPlaySampleRate, &stContext.nPlayBitsPerSample);
        alogd("parse wav header size:%d, ChnNum[%d], SampleRate[%d], BitsPerSample[%d]", nHeaderSize, stContext.nPlayChnNum, stContext.nPlaySampleRate, stContext.nPlayBitsPerSample);
        if(stContext.mConfigPara.mSampleRate != stContext.nPlaySampleRate
            || stContext.mConfigPara.mChannelCnt != stContext.nPlayChnNum
            || stContext.mConfigPara.mBitWidth != stContext.nPlayBitsPerSample)
        {
            alogw("Be careful! sample aec param [%d-%d-%d] is not match playParam[%d-%d-%d]!", 
                stContext.mConfigPara.mChannelCnt, stContext.mConfigPara.mSampleRate, stContext.mConfigPara.mBitWidth,
                stContext.nPlayChnNum, stContext.nPlaySampleRate, stContext.nPlayBitsPerSample);
        }
    }
    //init mpp system

    stContext.eof_flag = 0;

    
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();
    //init frame manager
    initSampleAecFrameManager(&stContext.mFrameManager, 5, &stContext.mConfigPara, &stContext);

    //enable ao dev
    stContext.mAIODev = 0;
    config_AIO_ATTR_S_by_SampleAecConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    //AW_MPI_AO_SetPubAttr(stContext.mAIODev, stContext.mAOChn, &stContext.mAIOAttr);
    //AW_MPI_AO_Enable(stContext.mAIODev, stContext.mAOChn);

    //create ao channel and clock channel.
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    stContext.mAOChn = 0;
    while(stContext.mAOChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(stContext.mAIODev, stContext.mAOChn);
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
    cbInfo.callback = (MPPCallbackFuncType)&SampleAec_CallbackWrapper;
    AW_MPI_AO_RegisterCallback(stContext.mAIODev, stContext.mAOChn, &cbInfo);

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
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAec_CallbackWrapper;
    AW_MPI_CLOCK_RegisterCallback(stContext.mClockChn, &cbInfo);

    //bind clock and ao
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, stContext.mClockChn};
    MPP_CHN_S AOChn = {MOD_ID_AO, stContext.mAIODev, stContext.mAOChn};
//    AW_MPI_SYS_Bind(&ClockChn, &AOChn);  // for audio only condition,no need to bind clock component with ao component.

    AO_DRC_CONFIG_S DrcCfg;
    DrcCfg.sampling_rate = stContext.mConfigPara.mSampleRate;   //8000
    DrcCfg.attack_time = 1;
    DrcCfg.release_time = 100;
    DrcCfg.max_gain = 6;
    DrcCfg.min_gain = -9;
    DrcCfg.noise_threshold = -45;
    DrcCfg.target_level = -3;
    //AW_MPI_AO_EnableSoftDrc(stContext.mAIODev, stContext.mAOChn,&DrcCfg);

    //start ao and clock.
//    pthread_create(&stContext.mAiSaveDataTid, NULL, getFrameThread, &stContext);
    
    AW_MPI_CLOCK_Start(stContext.mClockChn);
    AW_MPI_AO_StartChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_AO_SetDevVolume(stContext.mAIODev, 90);

    AW_MPI_AI_SetPubAttr(stContext.mAIODev, &stContext.mAIOAttr);
    AW_MPI_AI_Enable(stContext.mAIODev);
    //AW_MPI_AI_SetDevVolume(stContext.mAIODev, 100);
    
    //create ai channel
    bSuccessFlag = FALSE;
    stContext.mAIChn = 0;
    while(stContext.mAIChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(stContext.mAIODev, stContext.mAIChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", stContext.mAIChn);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", stContext.mAIChn);
            stContext.mAIChn++;
        }
        else if(ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", stContext.mAIChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAIChn = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
    }
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAec_CallbackWrapper;
    AW_MPI_AI_RegisterCallback(stContext.mAIODev, stContext.mAIChn, &cbInfo);

    AW_MPI_AI_EnableChn(stContext.mAIODev, stContext.mAIChn);

    result = pthread_create(&stContext.mSendPcmAoTid, NULL, SendPcmAoThread, &stContext);
    if(0 == result)
    {
        alogd("pthread create send_pcm_ao_thread[0x%x]!", stContext.mSendPcmAoTid);
    }
    else
    {
        aloge("fatal error! pthread create fail[%d]!", result);
    }
    
    result = pthread_create(&stContext.mAiSaveDataTidPcm, NULL, AiSaveDataThread, &stContext);
    if(0 == result)
    {
        alogd("pthread create send_pcm_ao_thread[0x%x]!", stContext.mSendPcmAoTid);
    }
    else
    {
        aloge("fatal error! pthread create fail[%d]!", result);
    }

    //wait pcm sent over.
    cdx_sem_down(&stContext.mSemEofCome);

    //clean before exit.
    result = pthread_join(stContext.mSendPcmAoTid, NULL);
    if(0 == result)
    {
        alogd("sendPcmAoTid[0x%x] is joined!", stContext.mSendPcmAoTid);
    }
    else
    {
        aloge("fatal error! sendPcmAoTid[0x%x] join fail[%d]!", stContext.mSendPcmAoTid, result);
    }
    result = pthread_join(stContext.mAiSaveDataTidPcm, NULL);
    if(0 == result)
    {
        alogd("aiSaveDataTidPcm[0x%x] is joined!", stContext.mAiSaveDataTidPcm);
    }
    else
    {
        aloge("fatal error! aiSaveDataTidPcm[0x%x] join fail[%d]!", stContext.mAiSaveDataTidPcm, result);
    }
    
    //stop ao channel, clock channel
    AW_MPI_AO_StopChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_CLOCK_Stop(stContext.mClockChn);
    AW_MPI_AO_DestroyChn(stContext.mAIODev, stContext.mAOChn);
    stContext.mAOChn = MM_INVALID_CHN;
    AW_MPI_CLOCK_DestroyChn(stContext.mClockChn);
    stContext.mClockChn = MM_INVALID_CHN;
    destroySampleAecFrameManager(&stContext.mFrameManager);

    ret = AW_MPI_AI_DisableChn(stContext.mAIODev, stContext.mAIChn); 
    ret = AW_MPI_AI_ResetChn(stContext.mAIODev, stContext.mAIChn);
    ret = AW_MPI_AI_DestroyChn(stContext.mAIODev, stContext.mAIChn);
    AW_MPI_AI_Disable(stContext.mAIODev);
    AW_MPI_AO_Disable(stContext.mAIODev, stContext.mAOChn);

    //exit mpp system
    AW_MPI_SYS_Exit();
    //close pcm file
    fclose(stContext.mFpPcmFile);
    stContext.mFpPcmFile = NULL;

_exit:
    destroySampleAecContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
