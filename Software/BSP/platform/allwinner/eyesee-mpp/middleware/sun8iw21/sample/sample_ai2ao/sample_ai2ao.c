
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_ai2ao"
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
#include <mpi_ai.h>
#include <mpi_ao.h>
#include <mpi_clock.h>
#include <ClockCompPortIndex.h>
#include "tsemaphore.h"

#include <confparser.h>

#include "sample_ai2ao_config.h"
#include "sample_ai2ao.h"

#include <cdx_list.h>

static int ParseCmdLine(int argc, char **argv, SampleAI2AOCmdLineParam *pCmdLinePara)
{
    alogd("sample_ai2ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAI2AOCmdLineParam));
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
                "\t-path /home/sample_ai2ao.conf\n");
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

static ERRORTYPE loadSampleAI2AOConfig(SampleAI2AOConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    pConfig->mSampleRate = 44100;
    pConfig->mBitWidth = 16;
    pConfig->mChannelCnt = 2;
    pConfig->mFrameSize = 1024;
    pConfig->mTunnelMode = 1;
    pConfig->mCapDurationSec = 10;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleAI2AOConfig));
    pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AI2AO_PCM_SAMPLE_RATE, 0);
    pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AI2AO_PCM_BIT_WIDTH, 0);
    pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AI2AO_PCM_CHANNEL_CNT, 0);
    pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AI2AO_PCM_FRAME_SIZE, 0);
    pConfig->mTunnelMode = GetConfParaInt(&stConfParser, SAMPLE_AI2AO_TUNNEL_MODE, 0);
    pConfig->mCapDurationSec = GetConfParaInt(&stConfParser, SAMPLE_AI2AO_CAP_DURATION, 0);
    alogd("            mSampleRate[%d], mBitWidth[%d], mChannelCnt[%d], mFrameSize[%d], mTunnleMode[%d], mCapTime[%d]",
        pConfig->mSampleRate, pConfig->mBitWidth, pConfig->mChannelCnt, pConfig->mFrameSize, pConfig->mTunnelMode, pConfig->mCapDurationSec);
    destroyConfParser(&stConfParser);

    return SUCCESS;
}

void config_AIO_ATTR_S_by_SampleAI2AOConfig(AIO_ATTR_S *dst, SampleAI2AOConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->u32ChnCnt = src->mChannelCnt;
    dst->enSamplerate = (AUDIO_SAMPLE_RATE_E)src->mSampleRate;
    dst->enBitwidth = (AUDIO_BIT_WIDTH_E)(src->mBitWidth/8-1);
}

void PcmDataAddWaveHeader(SampleAI2AOContext *ctx)
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
    SampleAI2AOConfig *pConf = &ctx->mConfigPara;

    memcpy(&header.riff_id, "RIFF ", 4);
    header.riff_sz = ctx->mPcmSize + sizeof(struct WaveHeader) - 8;
    memcpy(&header.riff_fmt, "WAVE ", 4);
    memcpy(&header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // s16le
    header.num_chn = pConf->mChannelCnt;
    header.sample_rate = pConf->mSampleRate;
    header.byte_rate = pConf->mSampleRate * pConf->mChannelCnt * pConf->mBitWidth/8;
    header.block_align = pConf->mChannelCnt * pConf->mBitWidth/8;
    header.bits_per_sample = pConf->mBitWidth;
    memcpy(&header.data_id, "data", 4);
    header.data_sz = ctx->mPcmSize;

    fseek(ctx->mFpPcmFile, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(struct WaveHeader), ctx->mFpPcmFile);
}

ERRORTYPE aoDataListener(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleAI2AOContext *pCtx = (SampleAI2AOContext*)cookie;
    int ret;
    pcmFrameNode *pNode;
    switch (event)
    {
    case MPP_EVENT_RELEASE_AUDIO_BUFFER:
        pthread_mutex_lock(&pCtx->mPcmlock);
        pNode = list_first_entry(&pCtx->mPcmUsingList, pcmFrameNode, mList);
        pthread_mutex_unlock(&pCtx->mPcmlock);
        ret = AW_MPI_AI_ReleaseFrame(pCtx->mAIODev, pCtx->mAIChn, &pNode->frame, NULL);
        if (SUCCESS != ret)
        {
            aloge("release frame to ai fail! ret: %#x", ret);
        }
        else
        {
            pthread_mutex_lock(&pCtx->mPcmlock);
            list_move_tail(&pNode->mList, &pCtx->mPcmIdleList);
            pthread_mutex_unlock(&pCtx->mPcmlock);
        }
        break;
    default:
        alogd("unknown event: [%d]", event);
        break;
    }

    return 0;
}

static SampleAI2AOContext *gp_Ctx;

void signalHandler(int signo)
{
    alogd("detect signal, ready to finish test...");
    if (gp_Ctx != NULL)
    {
        gp_Ctx->mOverFlag = TRUE;
    }
    else
    {
        aloge("Are you kidding me?! you must kill it by youself!");
    }
}

#define TUNNEL_MODE
//#define NON_TUNNEL_MODE

int main(int argc, char *argv[])
{
    int result = 0;
    alogd("Hello, sample_ai2ao!");
    SampleAI2AOContext stContext;
    memset(&stContext, 0, sizeof(SampleAI2AOContext));
    gp_Ctx = &stContext;
    int disp_fd = -1;

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
        pConfigFilePath = DEFAULT_SAMPLE_AI2AO_CONF_PATH;
    }

    //parse config file.
    if(loadSampleAI2AOConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    result = AW_MPI_SYS_Init();
    if (SUCCESS != result)
    {
        goto _exit;
    }

    //enable ai&ao dev
    stContext.mAIODev = 0;
    config_AIO_ATTR_S_by_SampleAI2AOConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    AW_MPI_AI_SetPubAttr(stContext.mAIODev, &stContext.mAIOAttr);
    //AW_MPI_AI_Enable(stContext.mAIODev);

    //create ai channel.
    //ERRORTYPE result;
    BOOL bSuccessFlag = FALSE;
    stContext.mAIChn = 0;
    while(stContext.mAIChn < AIO_MAX_CHN_NUM)
    {
        result = AW_MPI_AI_CreateChn(stContext.mAIODev, stContext.mAIChn);
        if(SUCCESS == result)
        {
            bSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", stContext.mAIChn);
            break;
        }
        else if (ERR_AI_EXIST == result)
        {
            alogd("ai channel[%d] exist, find next!", stContext.mAIChn);
            stContext.mAIChn++;
        }
        else if(ERR_AI_NOT_ENABLED == result)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! result[0x%x]!", stContext.mAIChn, result);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAIChn = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        goto _exit;
    }

    //create ao channel.
    bSuccessFlag = FALSE;
    stContext.mAOChn = 0;
    while(stContext.mAOChn < AIO_MAX_CHN_NUM)
    {
        result = AW_MPI_AO_CreateChn(stContext.mAIODev, stContext.mAOChn);
        if(SUCCESS == result)
        {
            bSuccessFlag = TRUE;
            alogd("create ao channel[%d] success!", stContext.mAOChn);
            break;
        }
        else if (ERR_AO_EXIST == result)
        {
            alogd("ao channel[%d] exist, find next!", stContext.mAOChn);
            stContext.mAOChn++;
        }
        else if(ERR_AO_NOT_ENABLED == result)
        {
            aloge("audio_hw_ao not started!");
            break;
        }
        else
        {
            aloge("create ao channel[%d] fail! result[0x%x]!", stContext.mAOChn, result);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAOChn = MM_INVALID_CHN;
        aloge("fatal error! create ao channel fail!");
        goto _exit;
    }
    AIO_ATTR_S stAIOAttr;
    memset(&stAIOAttr, 0, sizeof(AIO_ATTR_S));
    memcpy(&stAIOAttr, &stContext.mAIOAttr, sizeof(AIO_ATTR_S));
    stAIOAttr.mPcmCardId = PCM_CARD_TYPE_AUDIOCODEC;
    AW_MPI_AO_SetPubAttr(stContext.mAIODev, stContext.mAOChn, &stContext.mAIOAttr);
    AW_MPI_AO_Enable(stContext.mAIODev, stContext.mAOChn);

    //register vo callback
    MPPCallbackInfo voCallback = {&stContext, aoDataListener};
    AW_MPI_AO_RegisterCallback(stContext.mAIODev, stContext.mAOChn, &voCallback);

    //bind ai and ao
    MPP_CHN_S AIChn = {MOD_ID_AI, stContext.mAIODev, stContext.mAIChn};
    MPP_CHN_S AOChn = {MOD_ID_AO, stContext.mAIODev, stContext.mAOChn};
    if (stContext.mConfigPara.mTunnelMode)
    {
        AW_MPI_SYS_Bind(&AIChn, &AOChn);
    }

    bSuccessFlag = FALSE;
    stContext.mClockChnAttr.nWaitMask = 0;
    stContext.mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
    stContext.mClkChn = 0;
    while(stContext.mClkChn < CLOCK_MAX_CHN_NUM)
    {
        result = AW_MPI_CLOCK_CreateChn(stContext.mClkChn, &stContext.mClockChnAttr);
        if(SUCCESS == result)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", stContext.mClkChn);
            break;
        }
        else if(ERR_CLOCK_EXIST == result)
        {
            alogd("clock channel[%d] is exist, find next!", stContext.mClkChn);
            stContext.mClkChn++;
        }
        else
        {
            alogd("create clock channel[%d] result[0x%x]!", stContext.mClkChn, result);
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
    AW_MPI_SYS_Bind(&ClockChn, &AOChn);

    //start clk chn.
    AW_MPI_CLOCK_Start(stContext.mClkChn);
    //start ai chn.
    AW_MPI_AI_EnableChn(stContext.mAIODev, stContext.mAIChn);
    //start ao chn.
    AW_MPI_AO_StartChn(stContext.mAIODev, stContext.mAOChn);

    INIT_LIST_HEAD(&stContext.mPcmIdleList);
    INIT_LIST_HEAD(&stContext.mPcmUsingList);

    AW_MPI_AI_SetDevVolume(stContext.mAIODev, 95);
    AW_MPI_AO_SetDevVolume(stContext.mAIODev, 95);

    cdx_sem_t SemExit;
    cdx_sem_init(&SemExit, 0);
    if (stContext.mConfigPara.mCapDurationSec < 0)
    {
        cdx_sem_down(&SemExit);
    }
    else
    {
        cdx_sem_down_timedwait(&SemExit, stContext.mConfigPara.mCapDurationSec*1000);
    }

_deinit_ao:
    //stop ai chn
    AW_MPI_AI_DisableChn(stContext.mAIODev, stContext.mAIChn);

    //stop and reset ao chn, destroy ao dev
    AW_MPI_AO_StopChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_CLOCK_Stop(stContext.mClkChn);
    AW_MPI_AO_DestroyChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_CLOCK_DestroyChn(stContext.mClkChn);

    alogd("------------ai cap finish! ready write wave header!----------------");

_deinit_ai:
    //reset ai chn and destroy ai dev
    AW_MPI_AI_ResetChn(stContext.mAIODev, stContext.mAIChn);
    AW_MPI_AI_DestroyChn(stContext.mAIODev, stContext.mAIChn);

    AW_MPI_AI_Disable(stContext.mAIODev);
    AW_MPI_AO_Disable(stContext.mAIODev, stContext.mAOChn);

_deinit_clk:
    // stop and destroy clock chn
    stContext.mAIODev = MM_INVALID_DEV;
    stContext.mAOChn = MM_INVALID_CHN;
    stContext.mAIChn = MM_INVALID_CHN;
    stContext.mClkChn = MM_INVALID_CHN;

    //exit mpp system
    AW_MPI_SYS_Exit();

_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
