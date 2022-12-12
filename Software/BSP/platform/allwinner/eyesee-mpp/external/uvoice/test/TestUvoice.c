//#define LOG_NDEBUG 0
#define LOG_TAG "TestUvoice"
#include <utils/plat_log.h>

#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <confparser.h>
#include <cdx_list.h>
#include "TestUvoice_config.h"
#include "TestUvoice.h"

TestUvoiceContext *gpTestUvoiceContext = NULL;

typedef struct
{
    uvoiceCommand eCmd;
    const char *pCmdStr;
    //const char CameraParameters::KEY_FOCUS_DISTANCES[] = "focus-distances";
}UvoiceCommandMap;
    
static UvoiceCommandMap gUvoiceCommandMapAttry[] =
{
    {uvoiceCmd_XiaozhiStartRecord,     "XiaozhiStartRecord"},
    {uvoiceCmd_XiaozhiStopRecord,      "XiaozhiStopRecord"},
    {uvoiceCmd_XiaozhiCapture,         "XiaozhiCapture"},
    {uvoiceCmd_XiaozhiContinueCapture, "XiaozhiContinueCapture"},
    {uvoiceCmd_XiaozhiShutdown,        "XiaozhiShutdown"},
};

static const char* GetStringFromUvoiceCommand(uvoiceCommand eCmd)
{
    int nArraySize = sizeof(gUvoiceCommandMapAttry)/sizeof(gUvoiceCommandMapAttry[0]);
    int i;
    for(i=0; i<nArraySize; i++)
    {
        if(eCmd == gUvoiceCommandMapAttry[i].eCmd)
        {
            return gUvoiceCommandMapAttry[i].pCmdStr;
        }
    }
    aloge("fatal error! uvoiceCommand[0x%x] is not found in list!", eCmd);
    return NULL;
}

typedef enum
{
    TestUvoice_Exit = 0,
} TestUvoiceMessage;

int initTestUvoiceContext(TestUvoiceContext *pContext)
{
    int ret;
    memset(pContext, 0, sizeof(TestUvoiceContext));
    ret = message_create(&pContext->mMessageQueue);
    if(ret != 0)
    {
        aloge("fatal error! message create fail[0x%x]", ret);
    }
    ret = pthread_mutex_init(&pContext->mUvoiceListLock, NULL);
    if(ret != 0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    INIT_LIST_HEAD(&pContext->mUvoiceResultList);
    return 0;
}

int destroyTestUvoiceContext(TestUvoiceContext *pContext)
{
    if(pContext->mpSaveWavFile)
    {
        aloge("fatal error! why SaveWavFile not destruct?");
    }
    if(pContext->mpUvoice)
    {
        aloge("fatal error! why Uvoice not destruct?");
    }
    message_destroy(&pContext->mMessageQueue);
    pthread_mutex_lock(&pContext->mUvoiceListLock);
    if(!list_empty(&pContext->mUvoiceResultList))
    {
        TestUvoiceResult *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mUvoiceResultList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pContext->mUvoiceListLock);
    pthread_mutex_destroy(&pContext->mUvoiceListLock);
    return 0;
}

TestUvoiceContext* constructTestUvoiceContext()
{
    TestUvoiceContext *pContext = (TestUvoiceContext*)malloc(sizeof(TestUvoiceContext));
    if(pContext != NULL)
    {
        initTestUvoiceContext(pContext);
    }
    return pContext;
}

void destructTestUvoiceContext(TestUvoiceContext *pContext)
{
    if(pContext)
    {
        destroyTestUvoiceContext(pContext);
        free(pContext);
        pContext = NULL;
    }
}

static int ParseCmdLine(int argc, char **argv, TestUvoiceCmdLineParam *pCmdLinePara)
{
    alogd("TestUvoice path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(TestUvoiceCmdLineParam));
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
            printf("CmdLine param example:\n"
                "\t run -path /home/TestUvoice.conf\n");
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

static ERRORTYPE loadTestUvoiceConfig(TestUvoiceConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    if(NULL == conf_path)
    {
        alogd("user not set config file. use default test parameter!");
        strcpy(pConfig->mDirPath, "/mnt/extsd/TestUvoice_Files");
        strcpy(pConfig->mFileName, "TestUvoice.wav");
        pConfig->mSampleRate = 44100;
        pConfig->mChannelCount = 1;
        pConfig->mBitWidth = 16;
        pConfig->mCapureDuration = 0;
        pConfig->mUvoiceDataInterval = 160;
        pConfig->mbSaveWav = 1;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(TestUvoiceConfig));
    char *pStrDirPath = (char*)GetConfParaString(&stConfParser, TESTUVOICE_KEY_SAVE_DIR_PATH, NULL);
    if(pStrDirPath != NULL)
    {
        strncpy(pConfig->mDirPath, pStrDirPath, MAX_FILE_PATH_SIZE);
    }
    char *pStrFileName = (char*)GetConfParaString(&stConfParser, TESTUVOICE_KEY_SAVE_FILE_NAME, NULL);
    if(pStrFileName != NULL)
    {
        strncpy(pConfig->mFileName, pStrFileName, MAX_FILE_PATH_SIZE);
    }
    pConfig->mSampleRate = (unsigned int)GetConfParaInt(&stConfParser, TESTUVOICE_KEY_PCM_SAMPLE_RATE, 0);
    pConfig->mChannelCount = (unsigned int)GetConfParaInt(&stConfParser, TESTUVOICE_KEY_PCM_CHANNEL_CNT, 0);
    pConfig->mBitWidth = (unsigned int)GetConfParaInt(&stConfParser, TESTUVOICE_KEY_PCM_BIT_WIDTH, 0);
    pConfig->mCapureDuration = GetConfParaInt(&stConfParser, TESTUVOICE_KEY_PCM_CAP_DURATION, 0);
    pConfig->mUvoiceDataInterval = GetConfParaInt(&stConfParser, TESTUVOICE_KEY_UVOICE_DATA_INTERVAL, 0);
    pConfig->mbSaveWav = GetConfParaInt(&stConfParser, TESTUVOICE_KEY_SAVE_WAV, 0);
    alogd("dirPath[%s], fileName[%s], sampleRate[%d], channelCount[%d], bitWidth[%d], uvoiceDataInterval[%d]ms", 
        pConfig->mDirPath, pConfig->mFileName, pConfig->mSampleRate, pConfig->mChannelCount, pConfig->mBitWidth, pConfig->mUvoiceDataInterval);
    alogd("captureDuration[%d]s, bSaveWav[%d]", pConfig->mCapureDuration, pConfig->mbSaveWav);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpTestUvoiceContext)
    {
        message_t msg;
        msg.command = TestUvoice_Exit;
        put_message(&gpTestUvoiceContext->mMessageQueue, &msg);
    }
}

static void config_AIO_ATTR_S_by_AMicRecordConfig(AIO_ATTR_S *pAttr, TestUvoiceConfig *pConfig)
{
    memset(pAttr, 0, sizeof(AIO_ATTR_S));
    pAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)pConfig->mSampleRate;
    pAttr->enBitwidth = (AUDIO_BIT_WIDTH_E)(pConfig->mBitWidth/8-1);
    pAttr->enWorkmode = AIO_MODE_I2S_MASTER;
    if(pConfig->mChannelCount > 1)
    {
        pAttr->enSoundmode = AUDIO_SOUND_MODE_STEREO;
    }
    else
    {
        pAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
    }
    pAttr->u32ChnCnt = pConfig->mChannelCount;
    pAttr->u32ClkSel = 0;
    pAttr->mPcmCardId = PCM_CARD_TYPE_AUDIOCODEC;
}

int TestUvoice_CreateDirectory(const char* strDir)
{
    if(NULL == strDir)
    {
        aloge("fatal error! Null string pointer!");
        return -1;
    }
    //check folder existence
    struct stat sb;
    if (stat(strDir, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return 0;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", strDir, sb.st_mode);
            return -1;
        }
    }
    //create folder if necessary
    int ret = mkdir(strDir, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", strDir);
        return 0;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", strDir);
        return -1;
    }
}

static int TestUvoice_UvoiceCallback(void *cookie, uvoiceResult *pResultData)
{
    TestUvoiceContext *pContext = (TestUvoiceContext*)cookie;
    pthread_mutex_lock(&pContext->mUvoiceListLock);
    if(pResultData != NULL)
    {
        TestUvoiceResult *pNode = (TestUvoiceResult*)malloc(sizeof(TestUvoiceResult));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
        }
        memset(pNode, 0, sizeof(TestUvoiceResult));
        pNode->eCmd = pResultData->eCmd;
        const char *pString = GetStringFromUvoiceCommand(pNode->eCmd);
        if(pString!=NULL)
        {
            strncpy(pNode->strUvoiceCommand, pString, 128);
        }
        list_add_tail(&pNode->mList, &pContext->mUvoiceResultList);
        alogd("app receive uvoice result: {id:%d, string:%s}", pNode->eCmd, pNode->strUvoiceCommand);
    }
    else
    {
        aloge("fatal error! why uvoice return invalid result?");
    }
    pthread_mutex_unlock(&pContext->mUvoiceListLock);
    return 0;
}

/**
 * uvoice data time interval must be 160ms, equal to pContext->mAudioValidSize bytes.
 */
static int TestUvoice_SendDataToUvoice(TestUvoiceContext *pContext, char *pData, unsigned int nSize)
{
    int nUvoiceRet;
    #if 0
    if(pContext->mAudioValidSize > 0)
    {
        if(pContext->mAudioValidSize + nSize < pContext->mAudioBufferSize)
        {
            //only copy.
            memcpy(pContext->mpAudioBuffer+pContext->mAudioValidSize, pData, nSize);
            pContext->mAudioValidSize += nSize;
            return 0;
        }
        else
        {
            //copy and send.
            unsigned int nCurDataSendSize = pContext->mAudioBufferSize - pContext->mAudioValidSize;
            memcpy(pContext->mpAudioBuffer+pContext->mAudioValidSize, pData, nCurDataSendSize);
            pContext->mAudioValidSize += nCurDataSendSize;
            nUvoiceRet = uvoice_SendData(pContext->mpUvoice, pContext->mpAudioBuffer, pContext->mAudioValidSize);
            //alogd("[2]uvoice sendData[%p][%d], ret[%d]", pContext->mpAudioBuffer, pContext->mAudioValidSize, nUvoiceRet);
            pData += nCurDataSendSize;
            nSize -= nCurDataSendSize;
            pContext->mAudioValidSize = 0;
            if(nSize <= 0)
            {
                return 0;
            }
        }
    }
    if(0 == pContext->mAudioValidSize)
    {
        unsigned int nSendSize = 0;
        unsigned int nBlockSize = pContext->mAudioBufferSize;
        while(nSize - nSendSize >= nBlockSize)
        {
            nUvoiceRet = uvoice_SendData(pContext->mpUvoice, pData+nSendSize, nBlockSize);
            //alogd("uvoice sendData[%p][%d], ret[%d]", pData+nSendSize, nBlockSize, nUvoiceRet);
            nSendSize += nBlockSize;
        }
        unsigned int nLeftSize = nSize - nSendSize;
        if(nLeftSize > 0)
        {
            //copy to audiobuffer.
            memcpy(pContext->mpAudioBuffer, pData+nSendSize, nLeftSize);
            pContext->mAudioValidSize += nLeftSize;
        }
    }
    #else
    nUvoiceRet = uvoice_SendData(pContext->mpUvoice, pData, nSize);
    #endif
    return 0;
}

int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
	printf("TestUvoice running!\n");	
    TestUvoiceContext *pContext = constructTestUvoiceContext();
    gpTestUvoiceContext = pContext;
    if(NULL == pContext)
    {
        printf("why construct fail?\n");
        return -1;
    }
    /* parse command line param */
    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _err_1;
    }
    char *pConfigFilePath;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }

    /* parse config file. */
    if(loadTestUvoiceConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _err_1;
    }
    //check folder existence, create folder if necessary
    if(0 != TestUvoice_CreateDirectory(pContext->mConfigPara.mDirPath))
    {
        return -1;
    }
    int nBytesPerSecond = pContext->mConfigPara.mSampleRate*pContext->mConfigPara.mChannelCount*(pContext->mConfigPara.mBitWidth/8);
    if((nBytesPerSecond*pContext->mConfigPara.mUvoiceDataInterval)%1000 != 0)
    {
        aloge("fatal error! dataBlockSize which is need by uvoicelib is not integer.[%d][%d]", nBytesPerSecond, pContext->mConfigPara.mUvoiceDataInterval);
    }
    pContext->mAudioBufferSize = nBytesPerSecond*pContext->mConfigPara.mUvoiceDataInterval/1000;
    pContext->mpAudioBuffer = (char*)malloc(pContext->mAudioBufferSize);
    if(NULL == pContext->mpAudioBuffer)
    {
        aloge("fatal error! malloc [%d]bytes fail!", pContext->mAudioBufferSize);
    }
    
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    ERRORTYPE ret;
    /* init mpp system */
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_SYS_Init failed");
        goto _err_1;
    }
    //config ai attr
    pContext->mAIDev = 0;
    config_AIO_ATTR_S_by_AMicRecordConfig(&pContext->mAIOAttr, &pContext->mConfigPara);
    AW_MPI_AI_SetPubAttr(pContext->mAIDev, &pContext->mAIOAttr);
    alogd("AI Attr is: sampleRate[%d] bitWidth[%d], workMode[%d], Soundmode[%d], ChnCnt[%d], CldSel[%d], pcmCardId[%d]", 
        pContext->mAIOAttr.enSamplerate,
        pContext->mAIOAttr.enBitwidth,
        pContext->mAIOAttr.enWorkmode,
        pContext->mAIOAttr.enSoundmode,
        pContext->mAIOAttr.u32ChnCnt,
        pContext->mAIOAttr.u32ClkSel,
        pContext->mAIOAttr.mPcmCardId);
    //create ai channel.
    BOOL bSuccessFlag = FALSE;
    pContext->mAIChn = 0;
    while(pContext->mAIChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(pContext->mAIDev, pContext->mAIChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", pContext->mAIChn);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", pContext->mAIChn);
            pContext->mAIChn++;
        }
        else if(ERR_AI_NOT_ENABLED == ret)
        {
            aloge("fatal error! audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("fatal error! create ai channel[%d] fail! ret[0x%x]!", pContext->mAIChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        pContext->mAIChn = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        goto _err_2;
    }

    //start ai dev.
    ret = AW_MPI_AI_EnableChn(pContext->mAIDev, pContext->mAIChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enable ai channel fail!");
    }

    //create SaveWavFile if necessary.
    if(pContext->mConfigPara.mbSaveWav)
    {
        char strFilePath[MAX_FILE_PATH_SIZE];
        snprintf(strFilePath, MAX_FILE_PATH_SIZE, "%s/%s", pContext->mConfigPara.mDirPath, pContext->mConfigPara.mFileName);
        int nBufferDuration = 30;
        int nBufferSize = pContext->mConfigPara.mChannelCount*(pContext->mConfigPara.mBitWidth/8)*pContext->mConfigPara.mSampleRate*nBufferDuration;
        pContext->mpSaveWavFile = constructSaveWavFile(strFilePath, nBufferSize, 1, pContext->mConfigPara.mSampleRate, pContext->mConfigPara.mChannelCount, pContext->mConfigPara.mBitWidth);
        if(NULL == pContext->mpSaveWavFile)
        {
            aloge("fatal error! construct save wav file fail!");
        }
    }

    //create uvoice
    int nUvoiceRet;
    PcmConfig uvoicePcmConfig = {pContext->mConfigPara.mSampleRate, pContext->mConfigPara.mChannelCount, pContext->mConfigPara.mBitWidth};
    printf("#####  111   ###\n");
    pContext->mpUvoice = constructUvoiceInstance(&uvoicePcmConfig);
    printf("#####  2222   ###\n");
    destructUvoiceInstance(pContext->mpUvoice);
    PcmConfig uvoicePcmConfig2 = {pContext->mConfigPara.mSampleRate, pContext->mConfigPara.mChannelCount, pContext->mConfigPara.mBitWidth};
    printf("#####  3333   ###\n");
    pContext->mpUvoice = constructUvoiceInstance(&uvoicePcmConfig2);
    printf("#####  44456   ###\n");
    if(NULL == pContext->mpUvoice)
    {
        aloge("fatal error! construct uvoice fail!");
    }

    uvoiceCallbackInfo stCallbackInfo = {(void*)pContext, TestUvoice_UvoiceCallback};
    nUvoiceRet = uvoice_RegisterCallback(pContext->mpUvoice, &stCallbackInfo);
    if(nUvoiceRet != 0)
    {
        aloge("fatal error! why uvoice register callback fail[%d]?", nUvoiceRet);
    }
    
    int nSaveWavFileRet;
    AUDIO_FRAME_S stAFrame;
    memset(&stAFrame, 0, sizeof(AUDIO_FRAME_S));
    message_t stMsg;
    while (1)
    {
    PROCESS_MESSAGE:
        if(get_message(&pContext->mMessageQueue, &stMsg) == 0) 
        {
            alogv("TestUvoice thread get message cmd:%d", stMsg.command);
            if (TestUvoice_Exit == stMsg.command)
            {
                alogd("receive usr exit message!");
                break;
            }
            else
            {
                aloge("fatal error! unknown cmd[0x%x]", stMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }
        if(0 == stAFrame.mLen)
        {
            ret = AW_MPI_AI_GetFrame(pContext->mAIDev, pContext->mAIChn, &stAFrame, NULL, 2000);
            if(SUCCESS == ret)
            {
                pContext->mPcmSize += stAFrame.mLen;
                pContext->mPcmDurationMs = (int64_t)pContext->mPcmSize*1000/(pContext->mConfigPara.mChannelCount*(pContext->mConfigPara.mBitWidth/8)*pContext->mConfigPara.mSampleRate);
            }
        }
        else
        {
            alogd("Be careful! audio frame is not processed?");
            ret = SUCCESS;
        }
        if (SUCCESS == ret)
        {
            if(pContext->mConfigPara.mbSaveWav)
            {
            _sendPcmData:
                nSaveWavFileRet = pContext->mpSaveWavFile->SendPcmData((void*)pContext->mpSaveWavFile, stAFrame.mpAddr, stAFrame.mLen, 500);
                if(nSaveWavFileRet != 0)
                {
                    if(ETIMEDOUT == nSaveWavFileRet)
                    {
                        alogd("send pcm data timeout? continue to send!");
                        if(get_message_count(&pContext->mMessageQueue) > 0)
                        {
                            alogd("receive usr message2!");
                            goto PROCESS_MESSAGE;
                        }
                        goto _sendPcmData;
                    }
                    else
                    {
                        aloge("send pcm data fail, ignore pcm data.");
                    }
                }
            }
            TestUvoice_SendDataToUvoice(pContext, stAFrame.mpAddr, stAFrame.mLen);
            ret = AW_MPI_AI_ReleaseFrame(pContext->mAIDev, pContext->mAIChn, &stAFrame, NULL);
            if (SUCCESS != ret)
            {
                aloge("fatal error! release frame to ai fail! ret: %#x", ret);
            }
            memset(&stAFrame, 0, sizeof(AUDIO_FRAME_S));
            if(pContext->mConfigPara.mCapureDuration != 0)
            {
                if(pContext->mPcmDurationMs/1000 >= pContext->mConfigPara.mCapureDuration)
                {
                    alogd("time is over! exit!");
                    goto _exit;
                }
            }
        }
        else if(ERR_AI_BUF_EMPTY == ret)
        {
            alogd("timeout? continue!");
        }
        else
        {
            aloge("get pcm from ai in block mode fail! ret: %#x", ret);
            break;
        }
    }
_exit:
    //stop, reset and destroy ai chn & dev.
    AW_MPI_AI_DisableChn(pContext->mAIDev, pContext->mAIChn);
    AW_MPI_AI_ResetChn(pContext->mAIDev, pContext->mAIChn);
    AW_MPI_AI_DestroyChn(pContext->mAIDev, pContext->mAIChn);
    pContext->mAIChn = MM_INVALID_CHN;
    pContext->mAIDev = MM_INVALID_DEV;
    /* exit mpp system */
    AW_MPI_SYS_Exit();

    if(pContext->mConfigPara.mbSaveWav)
    {
        destructSaveWavFile(pContext->mpSaveWavFile);
        pContext->mpSaveWavFile = NULL;
    }
    if(pContext->mpUvoice)
    {
        //alogd("before destruct uvoice lib");
        destructUvoiceInstance(pContext->mpUvoice);
        //alogd("after destruct uvoice lib");
        pContext->mpUvoice = NULL;
    }

    //print all uvoice result!
    pthread_mutex_lock(&pContext->mUvoiceListLock);
    int nCnt = 0;
    if(!list_empty(&pContext->mUvoiceResultList))
    {
        TestUvoiceResult *pEntry;
        list_for_each_entry(pEntry, &pContext->mUvoiceResultList, mList)
        {
            alogd("entry[%d]:%s", nCnt, pEntry->strUvoiceCommand);
            nCnt++;
        }
    }
    alogd("uvoice total result count:%d", nCnt);
    pthread_mutex_unlock(&pContext->mUvoiceListLock);
    
    destructTestUvoiceContext(pContext);
    pContext = NULL;
    gpTestUvoiceContext = NULL;
    printf("TestUvoice exit success!\n");
    return 0;

_err_2:
    /* exit mpp system */
    AW_MPI_SYS_Exit();
_err_1:
    destructTestUvoiceContext(pContext);
    printf("TestUvoice exit!\n");
    return result;
}

