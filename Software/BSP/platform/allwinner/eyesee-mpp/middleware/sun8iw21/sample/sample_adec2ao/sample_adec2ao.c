
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_adec"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_adec.h>
#include <mpi_ao.h>
#include <ClockCompPortIndex.h>

#include <confparser.h>

#include "sample_adec2ao_config.h"
#include "sample_adec2ao.h"

#include <cdx_list.h>

AUDIO_STREAM_S* SampleADecStreamManager_PrefetchFirstIdleStream(void *pThiz)
{
	// Init output frame buffer
	AUDIO_STREAM_S *pStreamInfo = NULL;
	
	// Get handle of StreamManager
    SampleADecStreamManager *pStreamManager = (SampleADecStreamManager*)pThiz;
    
	// Prefetch frame buffer
    pthread_mutex_lock(&pStreamManager->mLock);
    if (!list_empty(&pStreamManager->mIdleList))
    {// if mIdleList has node, get the first one
        SampleADecStreamNode* pFirstNode = list_first_entry(&pStreamManager->mIdleList, SampleADecStreamNode, mList);
        pStreamInfo = &pFirstNode->mAStream;  //only prefetch, don't change status
    }
    else
    {
        pStreamInfo = NULL;
    }
    pthread_mutex_unlock(&pStreamManager->mLock);
	
    return pStreamInfo;
}

int SampleADecStreamManager_UseStream(void *pThiz, AUDIO_STREAM_S *pStream)
{
    int ret = 0;
    SampleADecStreamManager *pStreamManager = (SampleADecStreamManager*)pThiz;
    if (NULL == pStream)
    {
        aloge("fatal error! pNode == NULL!");
        return -1;
    }
	
    pthread_mutex_lock(&pStreamManager->mLock);
    SampleADecStreamNode *pFirstNode = list_first_entry_or_null(&pStreamManager->mIdleList, SampleADecStreamNode, mList);
    if (pFirstNode)
    {
        if (&pFirstNode->mAStream == pStream)
        {
            list_move_tail(&pFirstNode->mList, &pStreamManager->mUsingList);
        }
        else
        {
            aloge("fatal error! node is not match [%p]!=[%p]", pStream, &pFirstNode->mAStream);
            ret = -1;
        }
    }
    else
    {
        aloge("fatal error! idle list is empty");
        ret = -1;
    }
    pthread_mutex_unlock(&pStreamManager->mLock);
    return ret;
}

int SampleADecStreamManager_ReleaseStream(void *pThiz, unsigned int nStreamId)
{
    int ret = 0;
    SampleADecStreamManager *pStreamManager = (SampleADecStreamManager*)pThiz;
	
    pthread_mutex_lock(&pStreamManager->mLock);
    int bFindFlag = 0;
    SampleADecStreamNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pStreamManager->mUsingList, mList)
    {
        if(pEntry->mAStream.mId == nStreamId)
        {
            list_move_tail(&pEntry->mList, &pStreamManager->mIdleList);
            bFindFlag = 1;
            break;
        }
    }
    if(0 == bFindFlag)
    {
        aloge("fatal error! StreamId[%d] is not find", nStreamId);
        ret = -1;
    }
    pthread_mutex_unlock(&pStreamManager->mLock);
    return ret;
}

int initSampleADecStreamManager(SampleADecStreamManager *pStreamManager, int nStreamNum)
{
    memset(pStreamManager, 0, sizeof(SampleADecStreamManager));
	
    int err = pthread_mutex_init(&pStreamManager->mLock, NULL);
    if (err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
	
    INIT_LIST_HEAD(&pStreamManager->mIdleList);
    INIT_LIST_HEAD(&pStreamManager->mUsingList);

	// Create node and malloc buffer
    int i;
    SampleADecStreamNode *pNode = NULL;
    for (i = 0; i < nStreamNum; i++)
    {
        pNode = malloc(sizeof(SampleADecStreamNode));
        memset(pNode, 0, sizeof(SampleADecStreamNode));
        pNode->mAStream.mId     = i;
        pNode->mAStream.mLen    = 4096;    // max size
        pNode->mAStream.pStream = malloc(pNode->mAStream.mLen);
        list_add_tail(&pNode->mList, &pStreamManager->mIdleList);
    }
    pStreamManager->mNodeCnt = nStreamNum;

	// Attach options 
    pStreamManager->PrefetchFirstIdleStream = SampleADecStreamManager_PrefetchFirstIdleStream;
    pStreamManager->UseStream               = SampleADecStreamManager_UseStream;
    pStreamManager->ReleaseStream           = SampleADecStreamManager_ReleaseStream;
	
    return 0;
}

int destroySampleADecStreamManager(SampleADecStreamManager *pStreamManager)
{
	// Check, mUsingList should be empty
    if (!list_empty(&pStreamManager->mUsingList))
    {
        aloge("fatal error! why using list is not empty");
    }
	
	// Check, All node should locates in mIdleList
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pStreamManager->mIdleList)
    {
        cnt++;
    }
    if(cnt != pStreamManager->mNodeCnt)
    {
        aloge("fatal error! Stream count is not match [%d]!=[%d]", cnt, pStreamManager->mNodeCnt);
    }
	
	// Release buffers
    SampleADecStreamNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pStreamManager->mIdleList, mList)
    {
        free(pEntry->mAStream.pStream);
        list_del(&pEntry->mList);
        free(pEntry);
    }
	
	// Release other resouces
    pthread_mutex_destroy(&pStreamManager->mLock);
	
    return 0;
}

int initSampleADecContext(SampleADecContext *pContext)
{
    memset(pContext, 0, sizeof(SampleADecContext));
    int err = pthread_mutex_init(&pContext->mWaitStreamLock, NULL);
    if (err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
	
	err = 0;
    err += cdx_sem_init(&pContext->mSemStreamCome, 0);
    err += cdx_sem_init(&pContext->mSemEofCome, 0);
    if (err!=0)
    {
        aloge("cdx sem init fail!");
    }
    return 0;
}

int destroySampleADecContext(SampleADecContext *pContext)
{
    pthread_mutex_destroy(&pContext->mWaitStreamLock);
    cdx_sem_deinit(&pContext->mSemStreamCome);
    cdx_sem_deinit(&pContext->mSemEofCome);
    return 0;
}

extern int parseAudioFile(SampleADecConfig *pConf, FILE *fp);
static ERRORTYPE SampleADecCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleADecContext *pContext = (SampleADecContext*)cookie;
    if (MOD_ID_ADEC == pChn->mModId)
    {
        if (pChn->mChnId != pContext->mADecChn)
        {
            aloge("fatal error! ADec chnId[%d]!=[%d]", pChn->mChnId, pContext->mADecChn);
        }
        switch (event)
        {
            case MPP_EVENT_RELEASE_AUDIO_BUFFER:
            {
                AUDIO_STREAM_S *pAStream = (AUDIO_STREAM_S*)pEventData;
                pContext->mStreamManager.ReleaseStream(&pContext->mStreamManager, pAStream->mId);
				
                pthread_mutex_lock(&pContext->mWaitStreamLock);
                if (pContext->mbWaitStreamFlag)
                {
                    pContext->mbWaitStreamFlag = 0;
                    cdx_sem_up(&pContext->mSemStreamCome);
                }
                pthread_mutex_unlock(&pContext->mWaitStreamLock);
                break;
            }
            case MPP_EVENT_NOTIFY_EOF:
            {
                aloge("ADec channel notify APP that decode complete!");

                cdx_sem_signal(&pContext->mSemEofCome); 
                
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_ADEC_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else if(MOD_ID_AO == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mAOChn)
        {
            aloge("fatal error! AO chnId[%d]!=[%d]", pChn->mChnId, pContext->mAOChn);
        }
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("AO channel notify APP that play complete!");
                
                pContext->ao_eof_flag = 1;

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

static ERRORTYPE SampleADec_CLOCKCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    alogw("clock channel[%d] has some event[0x%x]", pChn->mChnId, event);
    return SUCCESS;
}

static int ParseCmdLine(int argc, char **argv, SampleADecCmdLineParam *pCmdLinePara)
{
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleADecCmdLineParam));
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
                "\t-path /home/sample_adec.conf\n");
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

static ERRORTYPE loadSampleADecConfig(SampleADecConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail!");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleADecConfig));
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_ADEC_AUDIO_FILE_PATH, NULL);
    strncpy(pConfig->mCompressAudioFilePath, ptr, MAX_FILE_PATH_SIZE-1);
    pConfig->mCompressAudioFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
//    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_ADEC_PCM_FILE_PATH, NULL);
//    strncpy(pConfig->mDecompressAudioFilePath, ptr, MAX_FILE_PATH_SIZE-1);
//    pConfig->mDecompressAudioFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
	pConfig->mType = GetConfParaInt(&stConfParser, SAMPLE_ADEC_DATA_TYPE, 0);
    pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_ADEC_PCM_SAMPLE_RATE, 0);
    pConfig->mBitWidth = 16;
    //pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_ADEC_PCM_BIT_WIDTH, 0);
    pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_ADEC_PCM_CHANNEL_CNT, 0);
    //pConfig->mStreamSize = GetConfParaInt(&stConfParser, SAMPLE_ADEC_PCM_Stream_SIZE, 0);
    destroyConfParser(&stConfParser);

    return SUCCESS;
}

int map_FreqIdx_to_SampleRate(int idx)
{
    switch (idx)
    {
    case 0:
        return 96000;
    case 1:
        return 88200;
    case 2:
        return 64000;
    case 3:
        return 48000;
    case 4:
        return 44100;
    case 5:
        return 32000;
    case 6:
        return 24000;
    case 7:
        return 22050;
    case 8:
        return 16000;
    case 9:
        return 12000;
    case 10:
        return 11025;
    case 11:
        return 8000;
    case 12:
        return 7370;
    case 13:
    case 14:
    case 15:
    default:
        aloge("wrong frequence idx [%d]", idx);
        return 8000;
    }
}

void parseADTSHeader(SampleADecConfig *pConf, ADTSHeader *header)
{
    pConf->mType = PT_AAC;
    unsigned char *ptr = (unsigned char*)header;
    pConf->mHeaderLen = ((ptr[1]&0x01)==1) ? 7:9;
    pConf->mBitWidth = 16;
    pConf->mChannelCnt = (ptr[2]&0x1)<<3 | (ptr[3]&0xc0)>>6;
    int idx = (ptr[2]&0x3c)>>2;
    pConf->mSampleRate = map_FreqIdx_to_SampleRate(idx);
    pConf->mFirstPktLen = ((ptr[3]&0x03)<<11) | (ptr[4]<<3) | ((ptr[5]&0xe0)>>5);
}

int parseAudioFile(SampleADecConfig *pConf, FILE *fp)
{
    int ret = 0;
    unsigned char buf[4];
	if(pConf->mType == PT_G711U || pConf->mType == PT_G711A)
    {
		pConf->mHeaderLen = 1024;
       fseek(fp, 0, SEEK_SET);
	}
	else if(pConf->mType == PT_G726 || pConf->mType == PT_G726U)
	{
		pConf->mHeaderLen = 512;
       fseek(fp, 0, SEEK_SET);
	}
	else
	{
		fseek(fp, 0, SEEK_SET);
		fread(buf, 1, 4, fp);
		if ((buf[0] == 0xff) && (buf[1]>>4 == 0xf))
		{
			alogd("audio file type is AAC!");
			fseek(fp, 0, SEEK_SET);
			ADTSHeader header;
			fread(&header, 1, 9, fp);
			parseADTSHeader(pConf, &header);
			fseek(fp, pConf->mFirstPktLen, SEEK_SET);

			fseek(fp, 0, SEEK_SET);
		}
		else
		{
			aloge("unknown audio file! can not decode other type now!");
			ret = -1;
		}
	}

    return ret;
}

int extractStreamPacket(AUDIO_STREAM_S *pStreamInfo, FILE *fp, SampleADecConfig *pConf)
{
    static int id;
    static long long pts;
    int read_len, pkt_sz,bs_sz;
    ADTSHeader header; 

	if(pConf->mType == PT_G711U || pConf->mType == PT_G711A)
	{
		bs_sz = 1024;// pkt_sz - pConf->mHeaderLen;
	}
	else if(pConf->mType == PT_G726 || pConf->mType == PT_G726U)
	{
		bs_sz = 512;// pkt_sz - pConf->mHeaderLen;
	}
	else
	{
		read_len = fread(&header, 1, pConf->mHeaderLen, fp);
		if (0 == read_len)
		{
			aloge("zjx_err:%d",pConf->mHeaderLen);
			return 0;
		}
		unsigned char *ptr = (unsigned char *)&header;
		pkt_sz = ((ptr[3]&0x03)<<11) | (ptr[4]<<3) | ((ptr[5]&0xe0)>>5);
		bs_sz = pkt_sz - pConf->mHeaderLen;
	}
    read_len = fread(pStreamInfo->pStream, 1, bs_sz, fp);

//    aloge("zjx_frm:%d-%d",bs_sz,read_len);
    pStreamInfo->mLen = read_len;
    pStreamInfo->mId  = id++;
    pStreamInfo->mTimeStamp = pts;
    pts += 23;
    return read_len;
}

void writeWaveHeader(SampleADecConfig *pConf, FILE *fp, int pcm_size)
{
    WaveHeader header;
    memcpy(&header.riff_id, "RIFF", 4);
    header.riff_sz = pcm_size + sizeof(WaveHeader) - 8;
    memcpy(&header.riff_fmt, "WAVE", 4);
    memcpy(&header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // s16le
    header.num_chn = pConf->mChannelCnt;
    header.sample_rate = pConf->mSampleRate;
    header.byte_rate = pConf->mSampleRate * pConf->mChannelCnt * 2;
    header.block_align = pConf->mChannelCnt * 2;
    header.bits_per_sample = 16;
    memcpy(&header.data_id, "data", 4);
    header.data_sz = pcm_size;
    fseek(fp, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(WaveHeader), fp);
}

int main(int argc, char *argv[])
{
    int result = 0;
    alogd("Hello, sample_adec!");
	
    SampleADecContext stContext;
    initSampleADecContext(&stContext);
	
    // parse command line param
    if (ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
	
    char *pConfigFilePath;
    if (strlen(stContext.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = DEFAULT_SAMPLE_ADEC_CONF_PATH;
    }
	
    // parse config file.
    if (loadSampleADecConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
	
    // open aac file
    stContext.mFpAudioFile = fopen(stContext.mConfigPara.mCompressAudioFilePath, "rb");
    if (!stContext.mFpAudioFile)
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara.mCompressAudioFilePath);
        result = -1;
        goto _exit;
    }
    else
    {
        //parse file type and set param by audio file
        result = parseAudioFile(&stContext.mConfigPara, stContext.mFpAudioFile);
        if (result)
        {
            aloge("some wang happened! need exit!!!");
            goto _exit;
        }
    }
	
	// open pcm file
//    stContext.mFpPcmFile = fopen(stContext.mConfigPara.mDecompressAudioFilePath, "wb");
//    WaveHeader tmpHeader;
//    memset(&tmpHeader, 0x55, sizeof(WaveHeader));
//    fwrite(&tmpHeader, 1, sizeof(WaveHeader), stContext.mFpPcmFile);

    // init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    // init Stream manager
    initSampleADecStreamManager(&stContext.mStreamManager, 5);
    
    memset(&stContext.mAIOAttr, 0, sizeof(AIO_ATTR_S));
	stContext.mAIOAttr.u32ChnCnt = stContext.mConfigPara.mChannelCnt;
	stContext.mAIOAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)stContext.mConfigPara.mSampleRate;
	stContext.mAIOAttr.enBitwidth = (AUDIO_BIT_WIDTH_E)(stContext.mConfigPara.mBitWidth/8-1);
    stContext.mAIODev = 0; 
    AW_MPI_AO_SetPubAttr(stContext.mAIODev,stContext.mAOChn, &stContext.mAIOAttr);
    AW_MPI_AO_Enable(stContext.mAIODev,stContext.mAOChn); 
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
        goto _exit;
    }

    //register vo callback
    MPPCallbackInfo voCallback = {&stContext, SampleADecCallbackWrapper};
    AW_MPI_AO_RegisterCallback(stContext.mAIODev, stContext.mAOChn, &voCallback); 
    
	AW_MPI_AO_SetDevVolume(stContext.mAIODev,100);
    //config adec chn attr.
    stContext.mADecAttr.mType         = stContext.mConfigPara.mType;
    stContext.mADecAttr.sampleRate    = stContext.mConfigPara.mSampleRate;
	stContext.mADecAttr.channels      = stContext.mConfigPara.mChannelCnt;
	stContext.mADecAttr.bitsPerSample = stContext.mConfigPara.mBitWidth;
    if(stContext.mConfigPara.mType == PT_G726 || stContext.mConfigPara.mType == PT_G726U)
	{
		//stContext.mADecAttr.g726_src_enc_type = 1;
		stContext.mADecAttr.bitRate = 32000;
	}

    //create ADec channel.
    stContext.mADecChn = 0;
    while (stContext.mADecChn < ADEC_MAX_CHN_NUM)
    {
        ret = AW_MPI_ADEC_CreateChn(stContext.mADecChn, &stContext.mADecAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ADec channel[%d] success!", stContext.mADecChn);
            break;
        }
        else if (ERR_ADEC_EXIST == ret)
        {
            alogd("ADec channel[%d] exist, find next!", stContext.mADecChn);
            stContext.mADecChn++;
        }
        else
        {
            aloge("create ADec channel[%d] fail! ret[0x%x]!", stContext.mADecChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mADecChn = MM_INVALID_CHN;
        aloge("fatal error! create ADec channel fail!");
    }
	
	// Set callback functions
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleADecCallbackWrapper;
    AW_MPI_ADEC_RegisterCallback(stContext.mADecChn, &cbInfo);


    MPP_CHN_S AdecChn = {MOD_ID_ADEC, stContext.mAIODev, stContext.mADecChn};
    MPP_CHN_S AOChn = {MOD_ID_AO, stContext.mAIODev, stContext.mAOChn};

    AW_MPI_SYS_Bind(&AdecChn, &AOChn); 
	
    // Start ADec chn.
    AW_MPI_ADEC_StartRecvStream(stContext.mADecChn);

    bSuccessFlag = FALSE;
    stContext.mClockChnAttr.nWaitMask = 0;
    stContext.mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
    stContext.mClkChn = 0;
    while(stContext.mClkChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(stContext.mClkChn, &stContext.mClockChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", stContext.mClkChn);
            break;
        }
        else if(ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", stContext.mClkChn);
            stContext.mClkChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", stContext.mClkChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mClkChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
    }

    //bind clock and ao
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, stContext.mClkChn};
//    AW_MPI_SYS_Bind(&ClockChn, &AOChn);

    //start clk chn.
    AW_MPI_CLOCK_Start(stContext.mClkChn);
    //start ao chn.
    AW_MPI_AO_StartChn(stContext.mAIODev, stContext.mAOChn); 
    
    int file_end = 0;
    stContext.loop_cnt = 5;
    // Read stream from file.
    int nPcmDataSize = 0;
    int nStreamSize;
    AUDIO_STREAM_S nStreamInfo;
    AUDIO_FRAME_S  nFrameInfo;
    nStreamInfo.pStream = malloc(4096); // max size
    if(NULL == nStreamInfo.pStream)
    {
        aloge("malloc_4k_failed");
        goto _exit_2;
    }
    while (1)
    {
        // get stream from file
        nStreamSize = extractStreamPacket(&nStreamInfo, stContext.mFpAudioFile, &stContext.mConfigPara);
        if (0 == nStreamSize)
        {// if no available audio data, clean output buffer and set eof flag
//            while (AW_MPI_ADEC_GetFrame(stContext.mADecChn, &nFrameInfo, 100) != ERR_ADEC_BUF_EMPTY)
//            {
//                fwrite(nFrameInfo.mpAddr, 1, nFrameInfo.mLen, stContext.mFpPcmFile);
//                AW_MPI_ADEC_ReleaseFrame(stContext.mADecChn, &nFrameInfo);
//                nPcmDataSize += nFrameInfo.mLen;
//            }
			
            if (feof(stContext.mFpAudioFile))
            {
                alogd("read file finish!");
            }

            aloge("zjx_eof:%d",stContext.loop_cnt);
            AW_MPI_ADEC_SetStreamEof(stContext.mADecChn, 1);
            
            aloge("zjx_w_eof");
            cdx_sem_wait(&stContext.mSemEofCome);
            aloge("zjx_wed_eof");
            
            AW_MPI_AO_SetStreamEof(stContext.mAIODev, stContext.mAOChn, 1, 1);

            while(!stContext.ao_eof_flag)
            {
                usleep(5000);
            }

            stContext.ao_eof_flag = 0; 
            file_end = 1;
            stContext.loop_cnt--; 

            AW_MPI_ADEC_SetStreamEof(stContext.mADecChn, 0);
            AW_MPI_ADEC_Seek(stContext.mADecChn);
            AW_MPI_ADEC_StopRecvStream(stContext.mADecChn); //added to stop chal and release audio dec libary
            AW_MPI_ADEC_StartRecvStream(stContext.mADecChn); 
            
//            AW_MPI_CLOCK_Seek(stContext.mClkChn);

            AW_MPI_AO_SetStreamEof(stContext.mAIODev, stContext.mAOChn, 0, 0);
            AW_MPI_AO_StopChn(stContext.mAIODev, stContext.mAOChn); 
            AW_MPI_AO_StartChn(stContext.mAIODev, stContext.mAOChn); 
            
            int result = parseAudioFile(&stContext.mConfigPara, stContext.mFpAudioFile);
            if (result)
            {
                aloge("some wang happened! need exit!!!");
            }
            if(0 == stContext.loop_cnt)
            {
                break;
            }
//            break;
        }

        if(!file_end)
        {
            // send stream to adec
            int nWaitTimeMs = 5*1000;
            ret = AW_MPI_ADEC_SendStream(stContext.mADecChn, &nStreamInfo, nWaitTimeMs);
            if (ret != SUCCESS)
            {
                alogw("send StreamId[%d] with [%d]ms timeout fail?!", nStreamInfo.mId, nWaitTimeMs);
            }
        }
        else
        {
            file_end = 0;
        }


        // get frame from adec
//        while (AW_MPI_ADEC_GetFrame(stContext.mADecChn, &nFrameInfo, 0) != ERR_ADEC_BUF_EMPTY)
//        {
//            fwrite(nFrameInfo.mpAddr, 1, nFrameInfo.mLen, stContext.mFpPcmFile);
//            AW_MPI_ADEC_ReleaseFrame(stContext.mADecChn, &nFrameInfo);
//            nPcmDataSize += nFrameInfo.mLen;
//        }
    }
    free(nStreamInfo.pStream);
	
	// Write pcm header
    aloge("decompress audio file and keep pcm file finish! then next write WAVE header!");
//    writeWaveHeader(&stContext.mConfigPara, stContext.mFpPcmFile, nPcmDataSize);

_exit_2:	
    // Stop ADec channel.
    AW_MPI_ADEC_StopRecvStream(stContext.mADecChn);
    AW_MPI_ADEC_DestroyChn(stContext.mADecChn);


    AW_MPI_CLOCK_Stop(stContext.mClkChn);
    AW_MPI_CLOCK_DestroyChn(stContext.mClkChn);


    AW_MPI_AO_StopChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_AO_DestroyChn(stContext.mAIODev, stContext.mAOChn);

    
    stContext.mADecChn = MM_INVALID_CHN;
    destroySampleADecStreamManager(&stContext.mStreamManager);
	
    // Exit mpp system
    AW_MPI_SYS_Exit();
	
    // Close aac/pcm file
    fclose(stContext.mFpAudioFile);
//    fclose(stContext.mFpPcmFile);
    stContext.mFpAudioFile = NULL;
	stContext.mFpPcmFile = NULL;

_exit:
    destroySampleADecContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
