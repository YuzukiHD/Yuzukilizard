
//#define LOG_NDEBUG 0
#define LOG_TAG "sonud_controler"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_ai.h>

#include <confparser.h>

#include "sample_sound_controler_config.h"
#include "sample_sound_controler.h"
#include "txz_engine.h"

#include <cdx_list.h>

void* cmdInst;

static int ParseCmdLine(int argc, char **argv, SampleAICmdLineParam *pCmdLinePara)
{
    alogd("sample ai path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAICmdLineParam));
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
                "\t-path /home/sample_ai.conf\n");
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

static ERRORTYPE loadSampleAIConfig(SampleAIConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        ret = FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleAIConfig));
    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AI_PCM_FILE_PATH, NULL);
    if (ptr == NULL)
    {
        aloge("parse item[%s] failed!", conf_path);
        ret = FAILURE;
    }
    else
    {
        strncpy(pConfig->mPcmFilePath, ptr, MAX_FILE_PATH_SIZE-1);
        alogd("parse info: mPcmFilePath[%s]", pConfig->mPcmFilePath);
    }
    pConfig->mPcmFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
    pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_SAMPLE_RATE, 0);
    pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_BIT_WIDTH, 0);
    pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_CHANNEL_CNT, 0);
    pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_FRAME_SIZE, 0);
    pConfig->mCapDuraSec = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_CAP_DURATION, 0);
    if (pConfig->mSampleRate && pConfig->mBitWidth && pConfig->mChannelCnt && pConfig->mFrameSize && pConfig->mCapDuraSec)
    {
        alogd("para: mSampleRate[%d], mBitWidth[%d], mChannelCnt[%d], mFrameSize[%d], mCapTime[%d]s",
            pConfig->mSampleRate, pConfig->mBitWidth, pConfig->mChannelCnt, pConfig->mFrameSize, pConfig->mCapDuraSec);
    }
    else
    {
        aloge("parse file(%s) for some items failed!", conf_path);
        ret = FAILURE;
    }
    destroyConfParser(&stConfParser);

    return ret;
}

void config_AIO_ATTR_S_by_SampleAIConfig(AIO_ATTR_S *dst, SampleAIConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->u32ChnCnt = src->mChannelCnt;
    dst->enSamplerate = (AUDIO_SAMPLE_RATE_E)src->mSampleRate;
    dst->enBitwidth = (AUDIO_BIT_WIDTH_E)(src->mBitWidth/8-1);
}

void PcmDataAddWaveHeader(SampleAIContext *ctx)
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
    SampleAIConfig *pConf = &ctx->mConfigPara;

    memcpy(&header.riff_id, "RIFF", 4);
    header.riff_sz = ctx->mPcmSize + sizeof(struct WaveHeader) - 8;
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
    header.data_sz = ctx->mPcmSize;

    fseek(ctx->mFpPcmFile, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(struct WaveHeader), ctx->mFpPcmFile);
}
/*------------------------------*/
int cmd_setpara()
{
    /* CMD configure */
    cmd_uint16_t cmd_shift_frames = 1; // 1 (0~10)
    cmd_uint16_t cmd_smooth_frames = 1; // 1 (0~30)
    cmd_uint16_t cmd_lock_frames = 1; // 1 (0~100)
    cmd_uint16_t cmd_post_max_frames = 35; // 35 (0~100)
    cmd_float32_t cmd_threshold = 0.50; // 0.70f (0.0f~1.0f)
    //printf("cmd threshod = %f\n", cmd_threshold);

#if 0
    if (CMD_CODE_NORMAL != txzEngineSetConfig(cmdInst, cmd_shiftFrames, &cmd_shift_frames)) {
        aloge( "txzEngineSetConfig cmd_shiftFrames error.\n");
            return -1;
        }
        if (CMD_CODE_NORMAL != txzEngineSetConfig(cmdInst, cmd_smoothFrames, &cmd_smooth_frames)) {
            aloge( "txzEngineSetConfig cmd_smoothFrames error.\n");
            return -1;
        }
        if (CMD_CODE_NORMAL != txzEngineSetConfig(cmdInst, cmd_lockFrames, &cmd_lock_frames)) {
            aloge( "txzEngineSetConfig cmd_lockFrames error.\n");
            return -1;
        }
        if (CMD_CODE_NORMAL != txzEngineSetConfig(cmdInst, cmd_postMaxFrames, &cmd_post_max_frames)) {
            aloge( "txzEngineSetConfig cmd_postMaxFrames error.\n");
            return -1;
        }
        if (CMD_CODE_NORMAL != txzEngineSetConfig(cmdInst, cmd_thresHold, &cmd_threshold)) {
            aloge( "txzEngineSetConfig cmd_threshold error.\n");
            return -1;
        }
#endif /* 0 */
        alogd("txzEngineSetConfig ok\n");
        return 0;

}

int uv_cmd_init()
{
    
	int res = txzEngineCheckLicense(NULL, "bbfbc8d1c8b032ee5d9e24d3a5a4ca9cc7af8cf5", "/usr/", "/mnt/extsd/");//A code can only be used once
	if(res != 1)
	{
		printf("err ---------------------------\n");
		return -1;
	}

    txzEngineCreate(&cmdInst);
    if (NULL == cmdInst)
    {
        aloge("txzEngineCreate err \n");
        return -1;
    }
    if (CMD_CODE_NORMAL != txzEngineCmdInit(cmdInst))
    {
        aloge("txzEngineInit err \n");
        return -1;
    }
    cmd_setpara();
    alogd("txzEngineInit ok \n");
    return 0;
}

/*------------------------------*/

int main(int argc, char *argv[])
{
    int result = 0;
    cmd_uint16_t cmdIndex=0;
    const char* commands = NULL;
    float cmd_confidence=0;
    int cmdcount = 0;
    int cmdcount_mod = 0;
    void * fram_date_bak = NULL;
    alogd("Hello, sound_contorler!");
    SampleAIContext stContext;
    memset(&stContext, 0, sizeof(SampleAIContext));

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
        pConfigFilePath = DEFAULT_SAMPLE_AI_CONF_PATH;
    }

    //parse config file.
    if(loadSampleAIConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    //open pcm file
    stContext.mFpPcmFile = fopen(stContext.mConfigPara.mPcmFilePath, "wb");
    if(!stContext.mFpPcmFile)
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara.mPcmFilePath);
        result = -1;
        goto _exit;
    }
    else
    {
        alogd("sample_ai produce file: %s", stContext.mConfigPara.mPcmFilePath);
        fseek(stContext.mFpPcmFile, 44, SEEK_SET);  // 44: size(WavHeader)
    }
    /********sound contorler**************/
    result = uv_cmd_init();
    if(result!=0) 
    {
        printf("uv_cmd_init err \n");
        fclose(stContext.mFpPcmFile);
        return -1;
    }
    txzEngineReset(cmdInst);
    /**********************/

    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //enable ai dev
    stContext.mAIDev = 0;
    config_AIO_ATTR_S_by_SampleAIConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    AW_MPI_AI_SetPubAttr(stContext.mAIDev, &stContext.mAIOAttr);
    //embedded in AW_MPI_AI_CreateChn
    //AW_MPI_AI_Enable(stContext.mAIDev);

    //create ai channel.
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    stContext.mAIChn = 0;
    while(stContext.mAIChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(stContext.mAIDev, stContext.mAIChn);
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
        goto _exit;
    }

    //start ai dev.
    AW_MPI_AI_EnableChn(stContext.mAIDev, stContext.mAIChn);

    short *pDatabuf = NULL;
    short *pDatabuf_bak = NULL;
    short *pDatabuf_remain = NULL;
    unsigned int near_buff_len;
    near_buff_len = stContext.mConfigPara.mFrameSize * stContext.mConfigPara.mBitWidth/8 * stContext.mConfigPara.mChannelCnt;
    pDatabuf = (short *)malloc(near_buff_len * 2);

    int nWriteLen;
    AUDIO_FRAME_S nAFrame;
    SampleAIConfig *pAiConf = &stContext.mConfigPara;
    //cap pcm for xx s
    int nMaxWantedSize = pAiConf->mSampleRate * pAiConf->mChannelCnt * pAiConf->mBitWidth/8 * pAiConf->mCapDuraSec;
    while (1)
    {
        pDatabuf_bak = pDatabuf;
        ret = AW_MPI_AI_GetFrame(stContext.mAIDev, stContext.mAIChn, &nAFrame, NULL, -1);
        memcpy(pDatabuf + cmdcount_mod/2,nAFrame.mpAddr,nAFrame.mLen);
        cmdcount = (nAFrame.mLen + cmdcount_mod)/320;//每次需要传入320个字节
        cmdcount_mod = (nAFrame.mLen + cmdcount_mod)%320;//取余数，保证所有数据都会传入
        pDatabuf_remain = pDatabuf + 160 * cmdcount;
        for(cmdcount; cmdcount > 0; cmdcount--)
        {
            //alogd("cmdcount:%d,cmdcount_mod:%d,pDatabuf_bak:%p",
            //        cmdcount,cmdcount_mod,pDatabuf_bak);
            result = txzEngineProcess(cmdInst, pDatabuf_bak, 160, &cmdIndex,&cmd_confidence);
            pDatabuf_bak = pDatabuf_bak + 160;//地址后移320个字节
            if(result!= CMD_CODE_NORMAL) 
            {
                printf("txzEngineProcess error, ret=%d \n",ret);
                txzEngineReset(cmdInst);
            }
            else
            {
                if (cmdIndex > 0) 
                {
                    txzEngineGetName(cmdIndex, &commands);
                    printf(" cmdIndex=%d , command:<%s> ,confidence=%f\n", cmdIndex,commands,cmd_confidence);
                    //cmdIndex=3 , command:<小志拍照> ,confidence=0.518561
                    txzEngineReset(cmdInst);
                }
            }
        }
        //alogd("pDatabuf_remain:%p pDatabuf + cmdcount_mod/2:%p",pDatabuf_remain,pDatabuf + cmdcount_mod/2);
        memcpy(pDatabuf,pDatabuf_remain,cmdcount_mod);
#if 1
        if (SUCCESS == ret)
        {
            //nAFrame.mpAddr = fram_date_bak;

            nWriteLen = fwrite(nAFrame.mpAddr, 1, nAFrame.mLen, stContext.mFpPcmFile);
            stContext.mPcmSize += nWriteLen;
            ret = AW_MPI_AI_ReleaseFrame(stContext.mAIDev, stContext.mAIChn, &nAFrame, NULL);
            if (SUCCESS != ret)
            {
                aloge("release frame to ai fail! ret: %#x", ret);
            }
        }
        else
        {
            aloge("get pcm from ai in block mode fail! ret: %#x", ret);
            break;
        }
#endif
        if (stContext.mPcmSize >= nMaxWantedSize)
        {
            alogd("capture %d Bytes pcm data, finish!", stContext.mPcmSize);
            break;
        }

    }
    printf("Delete wav header,only pcm data!!");
    //PcmDataAddWaveHeader(&stContext);
    fclose(stContext.mFpPcmFile);
    stContext.mFpPcmFile = NULL;

     free(pDatabuf);
    //stop ai chn.
    AW_MPI_AI_DisableChn(stContext.mAIDev, stContext.mAIChn);

    //reset and destroy ai chn & dev.
    AW_MPI_AI_ResetChn(stContext.mAIDev, stContext.mAIChn);
    AW_MPI_AI_DestroyChn(stContext.mAIDev, stContext.mAIChn);

    stContext.mAIDev = MM_INVALID_DEV;
    stContext.mAIChn = MM_INVALID_CHN;
    /*****Sound CTR******/
    txzEngineDesrtoy(&cmdInst);
    /**********/
    //exit mpp system
    AW_MPI_SYS_Exit();

_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
