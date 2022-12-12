
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

#include "sample_ao_resample_mixer_config.h"
#include "sample_ao_resample_mixer.h"

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

int initSampleAOFrameManager(SampleAOFrameManager *pFrameManager, int nFrameNum, SampleAOConfig *pConfigPara)
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
        pNode->mAFrame.mLen = pConfigPara->mChannelCnt * pConfigPara->mBitWidth/8 * pConfigPara->mFrameSize;
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
    int err = pthread_mutex_init(&pContext->mWaitFrameLock[0], NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    err = cdx_sem_init(&pContext->mSemFrameCome[0], 0);
    err = cdx_sem_init(&pContext->mSemEofCome[0], 0);
    if(err!=0)
    {
        aloge("cdx sem init fail!");
    }

    err = pthread_mutex_init(&pContext->mWaitFrameLock[1], NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    err = cdx_sem_init(&pContext->mSemFrameCome[1], 0);
    err = cdx_sem_init(&pContext->mSemEofCome[1], 0);
    if(err!=0)
    {
        aloge("cdx sem init fail!");
    }
    return 0;
}

int destroySampleAOContext(SampleAOContext *pContext)
{
    pthread_mutex_destroy(&pContext->mWaitFrameLock[0]);
    cdx_sem_deinit(&pContext->mSemFrameCome[0]);
    cdx_sem_deinit(&pContext->mSemEofCome[0]);

    pthread_mutex_destroy(&pContext->mWaitFrameLock[1]);
    cdx_sem_deinit(&pContext->mSemFrameCome[1]);
    cdx_sem_deinit(&pContext->mSemEofCome[1]);
    return 0;
}

static ERRORTYPE SampleAOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAOContext *pContext = (SampleAOContext*)cookie;
    if(MOD_ID_AO == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mAOChn[0] && pChn->mChnId != pContext->mAOChn[1])
        {
            aloge("fatal error! AO chnId[%d]!=[%d]-[%d]", pChn->mChnId, pContext->mAOChn[0],pContext->mAOChn[1]);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_AUDIO_BUFFER:
            {
                AUDIO_FRAME_S *pAFrame = (AUDIO_FRAME_S*)pEventData;
                pContext->mFrameManager[pChn->mChnId].ReleaseFrame(&pContext->mFrameManager[pChn->mChnId], pAFrame->mId);
                pthread_mutex_lock(&pContext->mWaitFrameLock[pChn->mChnId]);
                if(pContext->mbWaitFrameFlag[pChn->mChnId])
                {
                    pContext->mbWaitFrameFlag[pChn->mChnId] = 0;
                    cdx_sem_up(&pContext->mSemFrameCome[pChn->mChnId]);
                }
                pthread_mutex_unlock(&pContext->mWaitFrameLock[pChn->mChnId]);
                break;
            }
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("AO channel notify APP that play complete:%d!",pChn->mChnId);
                cdx_sem_up(&pContext->mSemEofCome[pChn->mChnId]);
				aloge("zjx_ao_chl:%d signal_eof_sem",pChn->mChnId);
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
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;
	SampleAOConfig *tmp_pConfig = NULL;
	
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

	tmp_pConfig = pConfig;
	
    memset(tmp_pConfig, 0, sizeof(SampleAOConfig));
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AO_PCM_FILE_PATH, NULL);
    strncpy(tmp_pConfig->mPcmFilePath, ptr, MAX_FILE_PATH_SIZE-1);
    tmp_pConfig->mPcmFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
    tmp_pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_SAMPLE_RATE, 0);
    tmp_pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_BIT_WIDTH, 0);
    tmp_pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_CHANNEL_CNT, 0);
    tmp_pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_FRAME_SIZE, 0);
    tmp_pConfig->second_ao_chl_en = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_SECOND_CHL_EN, 0);

	if(tmp_pConfig->second_ao_chl_en)
	{
		tmp_pConfig += 1;
		ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AO_PCM_FILE_PATH_SLAVE, NULL);
		strncpy(tmp_pConfig->mPcmFilePath, ptr, MAX_FILE_PATH_SIZE-1);
		tmp_pConfig->mPcmFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
		tmp_pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_SAMPLE_RATE_SLAVE, 0);
		tmp_pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_BIT_WIDTH_SLAVE, 0);
		tmp_pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_CHANNEL_CNT_SLAVE, 0);
		tmp_pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AO_PCM_FRAME_SIZE_SLAVE, 0);
	}

    destroyConfParser(&stConfParser);

    return SUCCESS;
}

void config_AIO_ATTR_S_by_SampleAOConfig(AIO_ATTR_S *dst, SampleAOConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->u32ChnCnt = src->mChannelCnt;
    dst->enSamplerate = (AUDIO_SAMPLE_RATE_E)src->mSampleRate;
    dst->enBitwidth = (AUDIO_BIT_WIDTH_E)(src->mBitWidth/8-1);
}

void* SecondWavThread(void* argv)
{
    SampleAOContext *PstContext = (SampleAOContext *)(argv);

	usleep(3000*1000);
	aloge("zjx_to_crt_chl2");
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
	AO_CHN tmp_ao_chl = 0;
    while(tmp_ao_chl < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(PstContext->mAODev, tmp_ao_chl);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ao channel[%d] success!", tmp_ao_chl);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            alogd("ao channel[%d] exist, find next!", tmp_ao_chl);
            tmp_ao_chl++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ao not started!");
            break;
        }
        else
        {
            aloge("create ao channel[%d] fail! ret[0x%x]!", tmp_ao_chl, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        PstContext->mAOChn[1] = MM_INVALID_CHN;
        aloge("fatal error! create ao channel fail!");
    }
	else
	{
		PstContext->mAOChn[1] = tmp_ao_chl;
	}

    PstContext->mAODev = 0;
    /* config_AIO_ATTR_S_by_SampleAOConfig(&PstContext->mAIOAttr[1], &PstContext->mConfigPara[1]);
    AW_MPI_AO_SetPubAttr(PstContext->mAODev,PstContext->mAOChn[1], &PstContext->mAIOAttr[1]);

	AW_MPI_AO_Enable(PstContext->mAODev,PstContext->mAOChn[1]); */

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)PstContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAOCallbackWrapper;
    AW_MPI_AO_RegisterCallback(PstContext->mAODev, PstContext->mAOChn[1], &cbInfo);

    AW_MPI_AO_StartChn(PstContext->mAODev, PstContext->mAOChn[1]);
    uint64_t nPts = 0;   //unit:us
    int nFrameSize = PstContext->mConfigPara[1].mFrameSize;
    int nSampleRate = PstContext->mConfigPara[1].mSampleRate;
    uint64_t nFrameInterval = 1000000*nFrameSize/nSampleRate; //128000us
    AUDIO_FRAME_S *pFrameInfo;
    int nReadLen;
	while(1)
	{
		//request idle frame
        pFrameInfo = PstContext->mFrameManager[1].PrefetchFirstIdleFrame(&PstContext->mFrameManager[1]);
        if(NULL == pFrameInfo)
        {
            pthread_mutex_lock(&PstContext->mWaitFrameLock[1]);
            pFrameInfo = PstContext->mFrameManager[1].PrefetchFirstIdleFrame(&PstContext->mFrameManager[1]);
            if(pFrameInfo!=NULL)
            {
                pthread_mutex_unlock(&PstContext->mWaitFrameLock[1]);
            }
            else
            {
                PstContext->mbWaitFrameFlag[1] = 1;
                pthread_mutex_unlock(&PstContext->mWaitFrameLock[1]);
                cdx_sem_down_timedwait(&PstContext->mSemFrameCome[1], 500);
                continue;
            }
        }

        //read pcm to idle frame
        int nWantedReadLen = nFrameSize * PstContext->mConfigPara[1].mChannelCnt * (PstContext->mConfigPara[1].mBitWidth/8);
        nReadLen = fread(pFrameInfo->mpAddr, 1, nWantedReadLen, PstContext->mFpPcmFile[1]);
        if(nReadLen < nWantedReadLen)
        {
            int bEof = feof(PstContext->mFpPcmFile[1]);
            if(bEof)
            {
                alogd("read file finish!");
            }
            break;
        }
        pFrameInfo->mTimeStamp = nPts;
        nPts += nFrameInterval;
        pFrameInfo->mId = pFrameInfo->mTimeStamp / nFrameInterval;
        PstContext->mFrameManager[1].UseFrame(&PstContext->mFrameManager[1], pFrameInfo);

        //send pcm to ao
        ret = AW_MPI_AO_SendFrame(PstContext->mAODev, PstContext->mAOChn[1], pFrameInfo, 0);
        if(ret != SUCCESS)
        {
            alogd("impossible, send frameId[%d] fail?", pFrameInfo->mId);
            PstContext->mFrameManager[1].ReleaseFrame(&PstContext->mFrameManager[1], pFrameInfo->mId);
        }

	}

	aloge("zjx_second_chl_eof");
    AW_MPI_AO_SetStreamEof(PstContext->mAODev, PstContext->mAOChn[1], 1, 1);
	aloge("zjx_second_chl_thrd_waiting_end_sem");
    cdx_sem_down(&PstContext->mSemEofCome[1]);
	aloge("zjx_second_chl_thrd_waited_end_sem");
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
        while(*ptr++ != ' ');
        if (!strncmp(buf, "setvol ", 7)) {
            AW_MPI_AO_SetDevVolume(0,atoi(ptr));
        } else if (!strncmp(buf, "setmute ", 8)) {
            AW_MPI_AO_SetDevMute(0, atoi(ptr), NULL);
        } else if (!strncmp(buf, "getvol", 6)) {
            int vol;
            AW_MPI_AO_GetDevVolume(0, &vol);
            alogd("getVol: %d", vol);
        } else if (!strncmp(buf, "getmute", 7)) {
            BOOL mute;
            AW_MPI_AO_GetDevMute(0,&mute, NULL);
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
        pConfigFilePath = DEFAULT_SAMPLE_AO_CONF_PATH;
    }
    //parse config file.
    if(loadSampleAOConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    //open pcm file
    stContext.mFpPcmFile[0] = fopen(stContext.mConfigPara[0].mPcmFilePath, "rb");
    if(!stContext.mFpPcmFile[0])
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara[0].mPcmFilePath);
        result = -1;
        goto _exit;
    }
    else
    {
        fseek(stContext.mFpPcmFile[0], 44, SEEK_SET);  // 44: size(WavHeader)
    }

	if(stContext.mConfigPara[0].second_ao_chl_en)
	{
		stContext.mFpPcmFile[1] = fopen(stContext.mConfigPara[1].mPcmFilePath, "rb");
		if(!stContext.mFpPcmFile[1])
		{
			aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara[1].mPcmFilePath);
			result = -1;
			goto _exit;
		}
		else
		{
			fseek(stContext.mFpPcmFile[1], 44, SEEK_SET);  // 44: size(WavHeader)
		}
	}


    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();
    //init frame manager
    initSampleAOFrameManager(&stContext.mFrameManager[0], 5, &stContext.mConfigPara[0]);

	if(stContext.mConfigPara[0].second_ao_chl_en)
		initSampleAOFrameManager(&stContext.mFrameManager[1], 5, &stContext.mConfigPara[1]);
    //enable ao dev
    /* stContext.mAODev = 0;
    config_AIO_ATTR_S_by_SampleAOConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    AW_MPI_AO_SetPubAttr(stContext.mAODev, &stContext.mAIOAttr); */
    //embedded in AW_MPI_AO_CreateChn
    //AW_MPI_AO_Enable(stContext.mAODev);

    //create ao channel and clock channel.
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
	AO_CHN tmp_ao_chl = 0;
    while(tmp_ao_chl < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(stContext.mAODev, tmp_ao_chl);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ao channel[%d] success!", tmp_ao_chl);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            alogd("ao channel[%d] exist, find next!", tmp_ao_chl);
            tmp_ao_chl++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ao not started!");
            break;
        }
        else
        {
            aloge("create ao channel[%d] fail! ret[0x%x]!", tmp_ao_chl, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAOChn[0] = MM_INVALID_CHN;
        aloge("fatal error! create ao channel fail!");
    }
	else
	{
		stContext.mAOChn[0] = tmp_ao_chl;
	}

    stContext.mAODev = 0;
    /* config_AIO_ATTR_S_by_SampleAOConfig(&stContext.mAIOAttr[0], &stContext.mConfigPara[0]);
    AW_MPI_AO_SetPubAttr(stContext.mAODev,stContext.mAOChn[0], &stContext.mAIOAttr[0]);

	AW_MPI_AO_Enable(stContext.mAODev,stContext.mAOChn[0]); */

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAOCallbackWrapper;
    AW_MPI_AO_RegisterCallback(stContext.mAODev, stContext.mAOChn[0], &cbInfo);

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
    cbInfo.callback = (MPPCallbackFuncType)&SampleAO_CLOCKCallbackWrapper;
    AW_MPI_CLOCK_RegisterCallback(stContext.mClockChn, &cbInfo);
    //bind clock and ao
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, stContext.mClockChn};
    MPP_CHN_S AOChn = {MOD_ID_AO, stContext.mAODev, stContext.mAOChn[0]};
    AW_MPI_SYS_Bind(&ClockChn, &AOChn);

    //test ao save file api
    /* AUDIO_SAVE_FILE_INFO_S info;
    strcpy(info.mFilePath, "/mnt/extsd/");
    strcpy(info.mFileName, "SampleAo_AoSaveFile.pcm");
    AW_MPI_AO_SaveFile(0,stContext.mAOChn,&info); */

    //start ao and clock.
    AW_MPI_CLOCK_Start(stContext.mClockChn);
    AW_MPI_AO_StartChn(stContext.mAODev, stContext.mAOChn[0]);
    AW_MPI_AO_SetDevVolume(stContext.mAODev, 90);

    //read pcm from file, play pcm through mpi_ao. we set pts by stContext.mConfigPara(mSampleRate,mFrameSize).
    uint64_t nPts = 0;   //unit:us
    int nFrameSize = stContext.mConfigPara[0].mFrameSize;
    int nSampleRate = stContext.mConfigPara[0].mSampleRate;
    uint64_t nFrameInterval = 1000000*nFrameSize/nSampleRate; //128000us
    AUDIO_FRAME_S *pFrameInfo;
    int nReadLen;
    pthread_t tid;
    int exit = 1;
    pthread_create(&tid, NULL, volumeAdjustThread, &exit);

    pthread_t second_ao_id;
	if(stContext.mConfigPara[0].second_ao_chl_en)
		pthread_create(&second_ao_id, NULL, SecondWavThread, &stContext);

    while(1)
    {
        //request idle frame
        pFrameInfo = stContext.mFrameManager[0].PrefetchFirstIdleFrame(&stContext.mFrameManager[0]);
        if(NULL == pFrameInfo)
        {
            pthread_mutex_lock(&stContext.mWaitFrameLock[0]);
            pFrameInfo = stContext.mFrameManager[0].PrefetchFirstIdleFrame(&stContext.mFrameManager[0]);
            if(pFrameInfo!=NULL)
            {
                pthread_mutex_unlock(&stContext.mWaitFrameLock[0]);
            }
            else
            {
                stContext.mbWaitFrameFlag[0] = 1;
                pthread_mutex_unlock(&stContext.mWaitFrameLock[0]);
                cdx_sem_down_timedwait(&stContext.mSemFrameCome[0], 500);
                continue;
            }
        }

        //read pcm to idle frame
        int nWantedReadLen = nFrameSize * stContext.mConfigPara[0].mChannelCnt * (stContext.mConfigPara[0].mBitWidth/8);
        nReadLen = fread(pFrameInfo->mpAddr, 1, nWantedReadLen, stContext.mFpPcmFile[0]);
        if(nReadLen < nWantedReadLen)
        {
            int bEof = feof(stContext.mFpPcmFile[0]);
            if(bEof)
            {
                alogd("read file finish!");
            }
            break;
        }
        pFrameInfo->mTimeStamp = nPts;
        nPts += nFrameInterval;
        pFrameInfo->mId = pFrameInfo->mTimeStamp / nFrameInterval;
        stContext.mFrameManager[0].UseFrame(&stContext.mFrameManager[0], pFrameInfo);

        //send pcm to ao
        ret = AW_MPI_AO_SendFrame(stContext.mAODev, stContext.mAOChn[0], pFrameInfo, 0);
        if(ret != SUCCESS)
        {
            alogd("impossible, send frameId[%d] fail?", pFrameInfo->mId);
            stContext.mFrameManager[0].ReleaseFrame(&stContext.mFrameManager[0], pFrameInfo->mId);
        }
    }

	aloge("zjx_first_ao_chl_eof");
    AW_MPI_AO_SetStreamEof(stContext.mAODev, stContext.mAOChn[0], 1, 1);
	aloge("zjx_first_ao_chl_to_wt_end_sem");
    cdx_sem_down(&stContext.mSemEofCome[0]);
	aloge("zjx_first_ao_chl_wted_end_sem");
    exit = 1;
    pthread_join(tid, NULL);
	aloge("zjx_to_join_second_chl_thrd");
	if(stContext.mConfigPara[0].second_ao_chl_en)
		pthread_join(second_ao_id, NULL);

	aloge("zjx_joined_second_chl_thrd");
    //stop ao channel, clock channel
    AW_MPI_AO_StopChn(stContext.mAODev, stContext.mAOChn[0]);
	if(stContext.mConfigPara[0].second_ao_chl_en)
		AW_MPI_AO_StopChn(stContext.mAODev, stContext.mAOChn[1]);
    AW_MPI_CLOCK_Stop(stContext.mClockChn);
    AW_MPI_AO_DestroyChn(stContext.mAODev, stContext.mAOChn[0]);
	if(stContext.mConfigPara[0].second_ao_chl_en)
		AW_MPI_AO_DestroyChn(stContext.mAODev, stContext.mAOChn[1]);
    stContext.mAODev = MM_INVALID_DEV;
    stContext.mAOChn[0] = MM_INVALID_CHN;
	if(stContext.mConfigPara[0].second_ao_chl_en)
		stContext.mAOChn[1] = MM_INVALID_CHN;
    AW_MPI_CLOCK_DestroyChn(stContext.mClockChn);
    stContext.mClockChn = MM_INVALID_CHN;
    destroySampleAOFrameManager(&stContext.mFrameManager[0]);
	if(stContext.mConfigPara[0].second_ao_chl_en)
		destroySampleAOFrameManager(&stContext.mFrameManager[1]);
    //exit mpp system
    AW_MPI_SYS_Exit();
    //close pcm file
    fclose(stContext.mFpPcmFile[0]);
    stContext.mFpPcmFile[0] = NULL;

_exit:
    destroySampleAOContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
