
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_ao2ai"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_ao.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>

#include <ClockCompPortIndex.h>

#include <confparser.h>

#include "sample_ao2ai_config.h"
#include "sample_ao2ai.h"

#include <cdx_list.h>

AUDIO_FRAME_S* SampleAO2AIFrameManager_PrefetchFirstIdleFrame(void *pThiz)
{
    SampleAO2AIFrameManager *pFrameManager = (SampleAO2AIFrameManager*)pThiz;
    SampleAO2AIFrameNode *pFirstNode;
    AUDIO_FRAME_S *pFrameInfo;
    pthread_mutex_lock(&pFrameManager->mLock);
    if(!list_empty(&pFrameManager->mIdleList))
    {
        pFirstNode = list_first_entry(&pFrameManager->mIdleList, SampleAO2AIFrameNode, mList);
        pFrameInfo = &pFirstNode->mAFrame;
    }
    else
    {
        pFrameInfo = NULL;
    }
    pthread_mutex_unlock(&pFrameManager->mLock);
    return pFrameInfo;
}

int SampleAO2AIFrameManager_UseFrame(void *pThiz, AUDIO_FRAME_S *pFrame)
{
    int ret = 0;
    SampleAO2AIFrameManager *pFrameManager = (SampleAO2AIFrameManager*)pThiz;
    if(NULL == pFrame)
    {
        aloge("fatal error! pNode == NULL!");
        return -1;
    }
    pthread_mutex_lock(&pFrameManager->mLock);
    SampleAO2AIFrameNode *pFirstNode = list_first_entry_or_null(&pFrameManager->mIdleList, SampleAO2AIFrameNode, mList);
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

int SampleAO2AIFrameManager_ReleaseFrame(void *pThiz, unsigned int nFrameId)
{
    int ret = 0;
    SampleAO2AIFrameManager *pFrameManager = (SampleAO2AIFrameManager*)pThiz;
    pthread_mutex_lock(&pFrameManager->mLock);
    int bFindFlag = 0;
    SampleAO2AIFrameNode *pEntry, *pTmp;
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

int initSampleAO2AIFrameManager(SampleAO2AIFrameManager *pFrameManager, int nFrameNum, SampleAO2AIConfig *pConfigPara)
{
    memset(pFrameManager, 0, sizeof(SampleAO2AIFrameManager));
    int err = pthread_mutex_init(&pFrameManager->mLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    INIT_LIST_HEAD(&pFrameManager->mIdleList);
    INIT_LIST_HEAD(&pFrameManager->mUsingList);

    int i;
    SampleAO2AIFrameNode *pNode;
    for (i=0; i<nFrameNum; i++)
    {
        pNode = malloc(sizeof(SampleAO2AIFrameNode));
        memset(pNode, 0, sizeof(SampleAO2AIFrameNode));
        pNode->mAFrame.mId = i;
        pNode->mAFrame.mBitwidth = (AUDIO_BIT_WIDTH_E)(pConfigPara->mBitWidth/8 - 1);
        pNode->mAFrame.mSoundmode = (pConfigPara->mChannelCnt==1)?AUDIO_SOUND_MODE_MONO:AUDIO_SOUND_MODE_STEREO;
        pNode->mAFrame.mLen = pConfigPara->mChannelCnt * pConfigPara->mBitWidth/8 * pConfigPara->mFrameSize;
        pNode->mAFrame.mpAddr = malloc(pNode->mAFrame.mLen);
        list_add_tail(&pNode->mList, &pFrameManager->mIdleList);
    }
    pFrameManager->mNodeCnt = nFrameNum;

    pFrameManager->PrefetchFirstIdleFrame = SampleAO2AIFrameManager_PrefetchFirstIdleFrame;
    pFrameManager->UseFrame = SampleAO2AIFrameManager_UseFrame;
    pFrameManager->ReleaseFrame = SampleAO2AIFrameManager_ReleaseFrame;
    return 0;
}

int destroySampleAO2AIFrameManager(SampleAO2AIFrameManager *pFrameManager)
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
    SampleAO2AIFrameNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pFrameManager->mIdleList, mList)
    {
        free(pEntry->mAFrame.mpAddr);
        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_destroy(&pFrameManager->mLock);
    return 0;
}

int initSampleAO2AIContext(SampleAO2AIContext *pContext)
{
    memset(pContext, 0, sizeof(SampleAO2AIContext));
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

int destroySampleAO2AIContext(SampleAO2AIContext *pContext)
{
    pthread_mutex_destroy(&pContext->mWaitFrameLock);
    cdx_sem_deinit(&pContext->mSemFrameCome);
    cdx_sem_deinit(&pContext->mSemEofCome);
    return 0;
}

static ERRORTYPE SampleAO2AI_CallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAO2AIContext *pContext = (SampleAO2AIContext*)cookie;
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
                
//                cdx_sem_signal(&pContext->mSemEofCome);
                pContext->eof_flag = 1;

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
            default:
            alogw("ai chn should not callback!");
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
            alogw("clk chn should not callback!");
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


static int ParseCmdLine(int argc, char **argv, SampleAO2AICmdLineParam *pCmdLinePara)
{
    alogd("sample ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAO2AICmdLineParam));
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
                "\t-path /home/sample_ao2ai.conf\n");
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

static ERRORTYPE loadSampleAO2AIConfig(SampleAO2AIConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleAO2AIConfig));
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AO2AI_PCM_SRC_PATH, NULL);
    strncpy(pConfig->mPcmSrcPath, ptr, MAX_FILE_PATH_SIZE);
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AO2AI_PCM_DST_PATH, NULL);
    strncpy(pConfig->mPcmDstPath, ptr, MAX_FILE_PATH_SIZE);
    
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AO2AI_AAC_DST_PATH, NULL);
    strncpy(pConfig->mAacDstPath, ptr, MAX_FILE_PATH_SIZE);
    pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_PCM_SAMPLE_RATE, 0);
    pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_PCM_BIT_WIDTH, 0);
    pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_PCM_CHANNEL_CNT, 0);
    pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_PCM_FRAME_SIZE, 0);
    pConfig->aec_delay_ms = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_AEC_DELAY_MS, 0);
    pConfig->ai_aec_en = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_AEC_ENABLE, 0);
    
    pConfig->ai_ans_en = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_ANS_ENABLE, 0);
    pConfig->ai_ans_mode = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_ANS_MODE, 0);

	pConfig->ai_agc_en = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_AGC_ENABLE, 0);
	pConfig->ao_drc_en = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_DRC_ENABLE, 0);
	pConfig->volume = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_VOLUME, 0);
	pConfig->log_level = GetConfParaInt(&stConfParser, SAMPLE_AO2AI_LOG_LEVEL, 0);
	destroyConfParser(&stConfParser);

    return SUCCESS;
}

void config_AIO_ATTR_S_by_SampleAO2AIConfig(AIO_ATTR_S *dst, SampleAO2AIConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->u32ChnCnt = src->mChannelCnt;
    dst->enSamplerate = (AUDIO_SAMPLE_RATE_E)src->mSampleRate;
    dst->enBitwidth = (AUDIO_BIT_WIDTH_E)(src->mBitWidth/8-1);
    dst->aec_delay_ms = src->aec_delay_ms;
    dst->ai_aec_en = src->ai_aec_en;
    dst->ai_ans_en = src->ai_ans_en;
    dst->ai_ans_mode = src->ai_ans_mode;
    dst->ai_agc_en = src->ai_agc_en;

	if(dst->ai_agc_en)
	{
		dst->ai_agc_cfg.fSample_rate = dst->enSamplerate;
		dst->ai_agc_cfg.iBytePerSample = src->mBitWidth/8;
		dst->ai_agc_cfg.iChannel = src->mChannelCnt;
		dst->ai_agc_cfg.iSample_len = 1024;
		dst->ai_agc_cfg.iGain_level = 2;
	}
}

void* AiSaveDataThread(void *param)
{
    SampleAO2AIContext *pCtx = (SampleAO2AIContext *)param;
    AUDIO_FRAME_S frame;
    const char* dst_file_path = pCtx->mConfigPara.mPcmDstPath;

    FILE *fp = fopen(dst_file_path, "wb");
    alogd("AiSaveDataThread cap data file: %s, dst_fp:%p", dst_file_path, fp);
    while (!pCtx->mOverFlag)
    {
        AW_MPI_AI_GetFrame(pCtx->mAIODev, pCtx->mAIChn2, &frame, NULL, -1);
        if(NULL != fp)
        {
            fwrite(frame.mpAddr, 1, frame.mLen, fp);
        }
        AW_MPI_AI_ReleaseFrame(pCtx->mAIODev, pCtx->mAIChn2, &frame, NULL);
    }
    fclose(fp);

    return NULL;
}

static void *getFrameThread(void *pThreadData)
{
    AUDIO_STREAM_S stream;
    SampleAO2AIContext *pStContext = (SampleAO2AIContext *)pThreadData;

    
    const char* dst_file_path = pStContext->mConfigPara.mAacDstPath;
    FILE *fp = fopen(dst_file_path, "wb"); 

    while (!pStContext->mOverFlag)
    {
        if (SUCCESS == AW_MPI_AENC_GetStream(pStContext->mAencChn, &stream, 10/*0*/))
        {
            //alogd("get one stream with size: [%d]", stream.mLen);
            fwrite(stream.pStream, 1, stream.mLen, fp);
            AW_MPI_AENC_ReleaseStream(pStContext->mAencChn, &stream);
        }
    }
    fclose(fp);

    return NULL;
}




static void configAencAttr(SampleAO2AIContext *pStContext)
{
    memset(&pStContext->mAencAttr, 0, sizeof(AENC_ATTR_S));

    pStContext->mAencAttr.sampleRate = pStContext->mConfigPara.mSampleRate;
    pStContext->mAencAttr.channels = pStContext->mConfigPara.mChannelCnt;
    pStContext->mAencAttr.bitRate = 0;             // usful when codec G726
    pStContext->mAencAttr.bitsPerSample = pStContext->mConfigPara.mBitWidth;
    pStContext->mAencAttr.attachAACHeader = 1;     // ADTS
    pStContext->mAencAttr.Type = PT_AAC;
} 

static ERRORTYPE createAencChn(SampleAO2AIContext *pStContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configAencAttr(pStContext);
    pStContext->mAencChn = 0;
    while (pStContext->mAencChn < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(pStContext->mAencChn, (AENC_CHN_ATTR_S*)&pStContext->mAencAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", pStContext->mAencChn);
            break;
        }
        else if(ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", pStContext->mAencChn);
            pStContext->mAencChn++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", pStContext->mAencChn, ret);
            pStContext->mAencChn++;
        }
    }

    if(FALSE == nSuccessFlag)
    {
        pStContext->mAencChn = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pStContext;
        cbInfo.callback = NULL;
        AW_MPI_AENC_RegisterCallback(pStContext->mAencChn, &cbInfo);

        if (pStContext->mAIChn >= 0)
        {
            alogd("bind ai & aenc");
            MPP_CHN_S AiChn = {MOD_ID_AI, 0, pStContext->mAIChn};
            MPP_CHN_S AencChn = {MOD_ID_AENC, 0, pStContext->mAencChn};
            AW_MPI_SYS_Bind(&AiChn, &AencChn);
        }
        return SUCCESS;
    }
}



int main(int argc, char *argv[])
{
    int result = 0;
    int raw_ao_play_frms = 0;
    alogd("Hello, sample_ao2ai!");
    SampleAO2AIContext stContext;
    initSampleAO2AIContext(&stContext);
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
        pConfigFilePath = DEFAULT_SAMPLE_AO2AI_CONF_PATH;
    }
    //parse config file.
    if(loadSampleAO2AIConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
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
    stContext.mFpPcmFile = fopen(stContext.mConfigPara.mPcmSrcPath, "rb");
    if(!stContext.mFpPcmFile)
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara.mPcmSrcPath);
        result = -1;
        goto _exit;
    }
    else
    {
        fseek(stContext.mFpPcmFile, 44, SEEK_SET);  // 44: size(WavHeader)
    }
    //init mpp system

    stContext.eof_flag = 0;
    
    if(stContext.mConfigPara.mSampleRate == 16000)
    {
        raw_ao_play_frms = 78;
    }
    else if(stContext.mConfigPara.mSampleRate == 8000)
    {
        raw_ao_play_frms = 78/2;
    }
    
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();
    //init frame manager
    initSampleAO2AIFrameManager(&stContext.mFrameManager, 5, &stContext.mConfigPara);

    //enable ao dev
    stContext.mAIODev = 0;
    /* config_AIO_ATTR_S_by_SampleAO2AIConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    AW_MPI_AO_SetPubAttr(stContext.mAIODev, &stContext.mAIOAttr);
    AW_MPI_AO_Enable(stContext.mAIODev); */

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

    config_AIO_ATTR_S_by_SampleAO2AIConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    AW_MPI_AO_SetPubAttr(stContext.mAIODev, stContext.mAOChn,&stContext.mAIOAttr);
    AW_MPI_AO_Enable(stContext.mAIODev,stContext.mAOChn);

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAO2AI_CallbackWrapper;
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
    cbInfo.callback = (MPPCallbackFuncType)&SampleAO2AI_CallbackWrapper;
    AW_MPI_CLOCK_RegisterCallback(stContext.mClockChn, &cbInfo);

    //bind clock and ao
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, stContext.mClockChn};
    MPP_CHN_S AOChn = {MOD_ID_AO, stContext.mAIODev, stContext.mAOChn};
//    AW_MPI_SYS_Bind(&ClockChn, &AOChn);  // for audio only condition,on need to bind clock component with ao component.
    if(stContext.mConfigPara.ao_drc_en)
    {
        AO_DRC_CONFIG_S DrcCfg;
        DrcCfg.sampling_rate = stContext.mConfigPara.mSampleRate;
        DrcCfg.attack_time = 1;
        DrcCfg.release_time = 100;
        DrcCfg.max_gain = 6;
        DrcCfg.min_gain = -9;
        DrcCfg.noise_threshold = -45;
        DrcCfg.target_level = -3;
        AW_MPI_AO_EnableSoftDrc(stContext.mAIODev, stContext.mAOChn,&DrcCfg);
    }
#if 0
    AW_MPI_AI_SetPubAttr(stContext.mAIODev, &stContext.mAIOAttr);
    AW_MPI_AI_Enable(stContext.mAIODev);

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


    //create ai channel
    bSuccessFlag = FALSE;
    stContext.mAIChn2 = 1;
    while(stContext.mAIChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(stContext.mAIODev, stContext.mAIChn2);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", stContext.mAIChn2);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", stContext.mAIChn2);
            stContext.mAIChn2++;
        }
        else if(ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", stContext.mAIChn2, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAIChn2 = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
    }
#endif /* 0 */

    
    //MPPCallbackInfo cbInfo;
    //cbInfo.cookie = (void*)&stContext;
    //cbInfo.callback = (MPPCallbackFuncType)&SampleAO2AI_CallbackWrapper;
//    AW_MPI_AI_RegisterCallback(stContext.mAIODev, stContext.mAIChn, &cbInfo);

//    MPP_CHN_S AIChn = {MOD_ID_AI, stContext.mAIODev, stContext.mAIChn};
//    AW_MPI_SYS_Bind(&AOChn, &AIChn);


    
//    AW_MPI_AI_EnableVqe(stContext.mAIODev, stContext.mAIChn);   // currently to enable aec feature only

    alogd("sample test goal: ao play music, ai cap sound, and ao send ref_frame to ai by tunnel mode");

#if 0
    ret = createAencChn(&stContext);
    if(0 > ret)
    {
        aloge("create_aenc_fail");
    }
#endif /* 0 */


    //start ao and clock.
//    pthread_create(&stContext.mAiSaveDataTid, NULL, getFrameThread, &stContext);
    
    AW_MPI_CLOCK_Start(stContext.mClockChn);
    AW_MPI_AO_StartChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_AO_SetDevVolume(stContext.mAIODev, stContext.mConfigPara.volume); 


//    AW_MPI_AI_EnableChn(stContext.mAIODev, stContext.mAIChn);
//    AW_MPI_AI_EnableChn(stContext.mAIODev, stContext.mAIChn2);
//    AW_MPI_AENC_StartRecvPcm(stContext.mAencChn);
//
//    pthread_create(&stContext.mAiSaveDataTidPcm, NULL, AiSaveDataThread, &stContext);

    //read pcm from file, play pcm through mpi_ao. we set pts by stContext.mConfigPara(mSampleRate,mFrameSize).
    uint64_t nPts = 0;   //unit:us

    int enabled_ai_flag = 0;
    int audio_frm_cnt = 0;
    int audio_frm_cnt2 = 0;
    int nFrameSize = stContext.mConfigPara.mFrameSize;
    int nSampleRate = stContext.mConfigPara.mSampleRate;
    uint64_t nFrameInterval = 1000000*nFrameSize/nSampleRate; //128000us
    AUDIO_FRAME_S *pFrameInfo;
    int nReadLen;
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
//                aloge("zjx_to_wait_1");
                cdx_sem_down_timedwait(&stContext.mSemFrameCome, 500);
//                aloge("zjx_waited_1");
                continue;
            }
        }

        //read pcm to idle frame
        int nWantedReadLen = nFrameSize * stContext.mConfigPara.mChannelCnt * (stContext.mConfigPara.mBitWidth/8);
        nReadLen = fread(pFrameInfo->mpAddr, 1, nWantedReadLen, stContext.mFpPcmFile);
        if(nReadLen < nWantedReadLen)
        {
            int bEof = feof(stContext.mFpPcmFile);
            if(bEof)
            {
                alogd("read file finish!");
            }
            break;
        }
        
        audio_frm_cnt2 ++;

        if(!enabled_ai_flag && audio_frm_cnt <= raw_ao_play_frms)
        {
            audio_frm_cnt ++;
        }
        else
        {
            if(!enabled_ai_flag)
            {
                aloge("zjx_ao_done,then_to_delay=============");
                usleep(5*1000000);
                audio_frm_cnt = 0;
                enabled_ai_flag = 1;
                

//                AW_MPI_AO_SetStreamEof(stContext.mAIODev, stContext.mAOChn, 1, 1);
//
//                while(!stContext.eof_flag)
//                {
//                    usleep(5000);
//                }
//
//                aloge("zjx_to_stop_ao");
//                AW_MPI_AO_StopChn(stContext.mAIODev, stContext.mAOChn);
//                AW_MPI_AO_SetStreamEof(stContext.mAIODev, stContext.mAOChn, 0, 1);



                
                AW_MPI_AI_SetPubAttr(stContext.mAIODev, &stContext.mAIOAttr);
                AW_MPI_AI_Enable(stContext.mAIODev);
                
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
                
                
                //create ai channel
                bSuccessFlag = FALSE;
                stContext.mAIChn2 = 1;
                while(stContext.mAIChn2 < AIO_MAX_CHN_NUM)
                {
                    ret = AW_MPI_AI_CreateChn(stContext.mAIODev, stContext.mAIChn2);
                    if(SUCCESS == ret)
                    {
                        bSuccessFlag = TRUE;
                        alogd("create ai channel[%d] success!", stContext.mAIChn2);
                        break;
                    }
                    else if (ERR_AI_EXIST == ret)
                    {
                        alogd("ai channel[%d] exist, find next!", stContext.mAIChn2);
                        stContext.mAIChn2++;
                    }
                    else if(ERR_AI_NOT_ENABLED == ret)
                    {
                        aloge("audio_hw_ai not started!");
                        break;
                    }
                    else
                    {
                        aloge("create ai channel[%d] fail! ret[0x%x]!", stContext.mAIChn2, ret);
                        break;
                    }
                }
                if(FALSE == bSuccessFlag)
                {
                    stContext.mAIChn2 = MM_INVALID_CHN;
                    aloge("fatal error! create ai channel fail!");
                }
                
                
//                MPP_CHN_S AIChn = {MOD_ID_AI, stContext.mAIODev, stContext.mAIChn};
//                AW_MPI_SYS_Bind(&AOChn, &AIChn); 
                
                ret = createAencChn(&stContext);
                if(0 > ret)
                {
                    aloge("create_aenc_fail");
                } 

//                usleep(3000000);

//                stContext.eof_flag = 0;
//                AW_MPI_AO_StartChn(stContext.mAIODev, stContext.mAOChn);
                
                AW_MPI_AI_EnableChn(stContext.mAIODev, stContext.mAIChn);
                AW_MPI_AI_EnableChn(stContext.mAIODev, stContext.mAIChn2); 
                
                
                AW_MPI_AENC_StartRecvPcm(stContext.mAencChn);
                
                pthread_create(&stContext.mAiSaveDataTid, NULL, getFrameThread, &stContext);
                pthread_create(&stContext.mAiSaveDataTidPcm, NULL, AiSaveDataThread, &stContext);


                aloge("zjx_enabled_ai,then to delay===================");
                usleep(5000000);
            }
        }


        
        if(enabled_ai_flag && audio_frm_cnt2 <= raw_ao_play_frms*2)
        {
            pFrameInfo->mTimeStamp = nPts;
            nPts += nFrameInterval;
            pFrameInfo->mId = pFrameInfo->mTimeStamp / nFrameInterval;
            stContext.mFrameManager.UseFrame(&stContext.mFrameManager, pFrameInfo);
            
            //send pcm to ao
            ret = AW_MPI_AO_SendFrame(stContext.mAIODev, stContext.mAOChn, pFrameInfo, 0);
            if(ret != SUCCESS)
            {
                aloge("impossible, send frameId[%d] fail?", pFrameInfo->mId);
                stContext.mFrameManager.ReleaseFrame(&stContext.mFrameManager, pFrameInfo->mId);
            } 
        }
        else if(enabled_ai_flag && raw_ao_play_frms*2 <audio_frm_cnt2 && audio_frm_cnt2 <= raw_ao_play_frms*2+1)
        {
            usleep(5*1000*1000);
        }
        else
        { 
            pFrameInfo->mTimeStamp = nPts;
            nPts += nFrameInterval;
            pFrameInfo->mId = pFrameInfo->mTimeStamp / nFrameInterval;
            stContext.mFrameManager.UseFrame(&stContext.mFrameManager, pFrameInfo);
            
            //send pcm to ao
            ret = AW_MPI_AO_SendFrame(stContext.mAIODev, stContext.mAOChn, pFrameInfo, 0);
            if(ret != SUCCESS)
            {
                aloge("impossible, send frameId[%d] fail?", pFrameInfo->mId);
                stContext.mFrameManager.ReleaseFrame(&stContext.mFrameManager, pFrameInfo->mId);
            }
        }


        
    }
    AW_MPI_AO_SetStreamEof(stContext.mAIODev, stContext.mAOChn, 1, 1);

//    aloge("zjx_to_wait_eof");
    while(!stContext.eof_flag)
    {
        usleep(5000);
    }
//    cdx_sem_wait(&stContext.mSemEofCome);
//    aloge("zjx_waited_eof");
    
    //stop ao channel, clock channel
    AW_MPI_AO_StopChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_CLOCK_Stop(stContext.mClockChn);
    AW_MPI_AO_DestroyChn(stContext.mAIODev, stContext.mAOChn);
    stContext.mAOChn = MM_INVALID_CHN;
    AW_MPI_CLOCK_DestroyChn(stContext.mClockChn);
    stContext.mClockChn = MM_INVALID_CHN;
    destroySampleAO2AIFrameManager(&stContext.mFrameManager);

    // stop ai channel
    stContext.mOverFlag = TRUE;
    pthread_join(stContext.mAiSaveDataTid, NULL);
    pthread_join(stContext.mAiSaveDataTidPcm, NULL);
    ret = AW_MPI_AI_DisableChn(stContext.mAIODev, stContext.mAIChn); 
    
    ret = AW_MPI_AI_DisableChn(stContext.mAIODev, stContext.mAIChn2); 
    ret = AW_MPI_AI_ResetChn(stContext.mAIODev, stContext.mAIChn);
    ret = AW_MPI_AI_ResetChn(stContext.mAIODev, stContext.mAIChn2);
    ret = AW_MPI_AI_DestroyChn(stContext.mAIODev, stContext.mAIChn);
    ret = AW_MPI_AI_DestroyChn(stContext.mAIODev, stContext.mAIChn2);
    AW_MPI_AENC_StopRecvPcm(stContext.mAencChn);
    //alogd("333. ret:%d", ret);

    if (stContext.mAencChn >= 0)
    {
        AW_MPI_AENC_ResetChn(stContext.mAencChn);
        AW_MPI_AENC_DestroyChn(stContext.mAencChn);
    } 

    AW_MPI_AI_Disable(stContext.mAIODev);
    AW_MPI_AO_Disable(stContext.mAIODev,stContext.mAOChn);

    //exit mpp system
    AW_MPI_SYS_Exit();
    //close pcm file
    fclose(stContext.mFpPcmFile);
    stContext.mFpPcmFile = NULL;

_exit:
    destroySampleAO2AIContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
