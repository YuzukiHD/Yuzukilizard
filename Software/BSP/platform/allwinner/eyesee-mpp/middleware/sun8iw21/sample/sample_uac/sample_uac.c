
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_ai"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_ai.h>

#include <confparser.h>

#include "sample_uac_config.h"
#include "sample_uac.h"

#include <cdx_list.h>

//support uac for alsa
#include <alsa/asoundlib.h>

//uac configuare
struct uac_alsa_config {
	snd_pcm_t           *uac_handle;  //调用snd_pcm_open打开PCM设备返回的文件句柄
	snd_pcm_hw_params_t *uac_params;  //设置流的硬件参数
	snd_pcm_sw_params_t *uac_swparams;
	snd_pcm_uframes_t    periodSize;
    int                  periods;
    int                  bufferSize;
    int                  bytes_by_frame;
};

static BOOL gbExitFlag = FALSE;
static void SignalHandle(int iArg)
{
    alogd("%s user want exit!", LOG_TAG);
    gbExitFlag = TRUE;
}

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

    ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_UAC_AUDIO_CARD_NAME, NULL);
    if (ptr == NULL)
    {
        aloge("parse item[%s] failed!", conf_path);
        ret = FAILURE;
    }
    else
    {
        strncpy(pConfig->mUacAudioCardName, ptr, MAX_FILE_PATH_SIZE-1);
        alogd("parse info: mUacAudioCardName[%s]", pConfig->mUacAudioCardName);
    }
    pConfig->mUacAudioCardName[MAX_FILE_PATH_SIZE-1] = '\0';
    pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_SAMPLE_RATE, 0);
    pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_BIT_WIDTH, 0);
    pConfig->mCapDuraSec = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_CAP_DURATION, 0);
    pConfig->ai_gain = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_GAIN, 0);
    //uac to pc only support stero
    pConfig->mChannelCnt = 2;
    if (pConfig->mSampleRate && pConfig->mBitWidth && pConfig->mCapDuraSec)
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
    //uac to pc only support stero
    src->mChannelCnt = 2;
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

static void uac_xrun(snd_pcm_t *handle)
{
    snd_pcm_status_t *status = NULL;
    int ret = 0;

    snd_pcm_status_alloca(&status);
    ret = snd_pcm_status(handle, status);
    if (ret < 0)
    {
        aloge("status error");
    }
    ret = snd_pcm_status_get_state(status);
    if (SND_PCM_STATE_XRUN == ret)
    {
        aloge("xrun: SND_PCM_STATE_XRUN");
        ret = snd_pcm_prepare(handle);
        if (ret < 0)
        {
            aloge("xrun: prepare error!");
        }
        goto _uac_xrun_free;
    }
    ret = snd_pcm_status_get_state(status);
    if (SND_PCM_STATE_DRAINING == ret)
    {
        aloge("SND_PCM_STATE_DRAINING");
    }
    aloge("read/writei error! status[%s]", snd_pcm_state_name(snd_pcm_status_get_state(status)));
_uac_xrun_free:
    snd_pcm_status_free(status);
    return;
}

static void uac_suspend(snd_pcm_t *handle)
{
    int ret = 0;

    aloge("uac suspend");
    while (-EAGAIN == (ret = snd_pcm_resume(handle)))
    {
        usleep(100*1000);
        if (ret < 0)
        {
            aloge("restart stream");
            ret = snd_pcm_prepare(handle);
            if (ret < 0)
            {
                aloge("snd prepare fail!");
            }
        }
    }
    aloge("done");
}

int set_uac_audio_card_params(struct uac_alsa_config *uac_cfg, SampleAIConfig *src)
{
	int rc;

	/* Open PCM device for playback */
	alogd("open uac audio card : %s", src->mUacAudioCardName);
	rc = snd_pcm_open(&uac_cfg->uac_handle, src->mUacAudioCardName, SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0)
	{
		alogd("unable to open pcm device");
		return -1;
	}

	/* Allocate a hardware parameters object */
	snd_pcm_hw_params_alloca(&uac_cfg->uac_params);

	/* Fill it in with default values. */
	rc = snd_pcm_hw_params_any(uac_cfg->uac_handle, uac_cfg->uac_params);
	if (rc < 0)
	{
		alogd("unable to Fill it in with default values.");
		goto err1;
	}

	/* Interleaved mode */
	rc = snd_pcm_hw_params_set_access(uac_cfg->uac_handle, uac_cfg->uac_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0)
	{
		alogd("unable to Interleaved mode");
		goto err1;
	}

	snd_pcm_format_t format;

	switch(src->mBitWidth) {
		case 8:
			alogd("set 8bit for uac");
			format = SND_PCM_FORMAT_U8;
			break;
		case 16:
			alogd("set 16bit for uac");
			format = SND_PCM_FORMAT_S16_LE;
			break;
		case 24:
			alogd("set 24bit for uac");
			format = SND_PCM_FORMAT_U24_LE;
			break;
		case 32:
			alogd("set 32bit for uac");
			format = SND_PCM_FORMAT_U32_LE;
			break;
		default :
			alogd("SND_PCM_FORMAT_UNKNOWN.");
			format = SND_PCM_FORMAT_UNKNOWN;
			goto err1;
	}

	/* set format */
	rc = snd_pcm_hw_params_set_format(uac_cfg->uac_handle, uac_cfg->uac_params, format);
	if (rc < 0)
	{
		alogd("unable to set format");
		goto err1;
	}

	/* set channels (stero)  uac to pc only support stero */
	snd_pcm_hw_params_set_channels(uac_cfg->uac_handle, uac_cfg->uac_params, src->mChannelCnt);
	if (rc < 0)
	{
		alogd("unable to set channels (stero)");
		goto err1;
	}

	/* set sampling rate */
	int dir;
	unsigned int rate = src->mSampleRate;
	rc = snd_pcm_hw_params_set_rate_near(uac_cfg->uac_handle, uac_cfg->uac_params, &rate, &dir);
	if (rc < 0)
	{
		alogd("unable to set sampling rate");
		goto err1;
	}

	//set periodsize = 1024/
	uac_cfg->periodSize = 1024;
	snd_pcm_hw_params_set_period_size_near(uac_cfg->uac_handle, uac_cfg->uac_params, &uac_cfg->periodSize, &dir);

    uac_cfg->bytes_by_frame = src->mChannelCnt*src->mBitWidth/8;
    uac_cfg->periods = 4;
    snd_pcm_hw_params_set_periods(uac_cfg->uac_handle, uac_cfg->uac_params, uac_cfg->periods, dir);

    uac_cfg->bufferSize = uac_cfg->periodSize * uac_cfg->periods;
    snd_pcm_hw_params_set_buffer_size(uac_cfg->uac_handle, uac_cfg->uac_params, uac_cfg->bufferSize);

	/* Write the parameters to the dirver */
	rc = snd_pcm_hw_params(uac_cfg->uac_handle, uac_cfg->uac_params);
	if (rc < 0) {
		aloge("unable to set hw parameters: %s", snd_strerror(rc));
		goto err1;
	}

	snd_pcm_hw_params_get_period_size(uac_cfg->uac_params, &uac_cfg->periodSize, &dir);
	alogd("snd get uac card periodSize :%d", uac_cfg->periodSize);

    snd_pcm_hw_params_get_periods(uac_cfg->uac_params, &uac_cfg->periods, &dir);
    alogd("snd get uac card periods: %d", uac_cfg->periods);

    snd_pcm_hw_params_get_buffer_size(uac_cfg->uac_params, &uac_cfg->bufferSize);
    alogd("snd get uac card buffer size: %d", uac_cfg->bufferSize);

    snd_pcm_sw_params_alloca(&uac_cfg->uac_swparams);
    snd_pcm_sw_params_current(uac_cfg->uac_handle, uac_cfg->uac_swparams);
    snd_pcm_sw_params_set_avail_min(uac_cfg->uac_handle, uac_cfg->uac_swparams, uac_cfg->periodSize);
    int start_threshold;
    int stop_threshold;
    start_threshold = 0;
    stop_threshold = uac_cfg->bufferSize;
    snd_pcm_sw_params_set_start_threshold(uac_cfg->uac_handle, uac_cfg->uac_swparams, start_threshold);
    snd_pcm_sw_params_set_stop_threshold(uac_cfg->uac_handle, uac_cfg->uac_swparams, stop_threshold);
    snd_pcm_sw_params(uac_cfg->uac_handle, uac_cfg->uac_swparams);

    snd_pcm_nonblock(uac_cfg->uac_handle, 1);

	return 0;

err1:
	snd_pcm_close(uac_cfg->uac_handle);
	return -1;
}

int main(int argc, char *argv[])
{
    int result = 0;
    alogd("Hello, sample_ai!");
    SampleAIContext stContext;
    memset(&stContext, 0, sizeof(SampleAIContext));

    if (SIG_ERR == signal(SIGINT, SignalHandle))
    {
        perror("signal fail: ");
    }

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
        result = -1 ;
        goto _exit;
    }
    else
    {
        alogd("sample_ai produce file: %s", stContext.mConfigPara.mPcmFilePath);
        fseek(stContext.mFpPcmFile, 44, SEEK_SET);  // 44: size(WavHeader)
    }

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

    AW_MPI_AI_SetDevVolume(stContext.mAIDev,stContext.mConfigPara.ai_gain);

    int tmp_gain = 0;
    AW_MPI_AI_GetDevVolume(stContext.mAIDev,&tmp_gain);

    aloge("sampl_ai_gain_value:%d",tmp_gain);


    /* enable uac audio-cards */
    struct uac_alsa_config uac_cfg = {0};
    ret = set_uac_audio_card_params(&uac_cfg, &stContext.mConfigPara);
    if (ret < 0)
    {
	aloge("set_hardware_params error\n");
	return -1;
    }

    //start ai dev.
    AW_MPI_AI_EnableChn(stContext.mAIDev, stContext.mAIChn);

    int nWriteLen;
    AUDIO_FRAME_S nAFrame;
    SampleAIConfig *pAiConf = &stContext.mConfigPara;
    //cap pcm for xx s
    int nMaxWantedSize = pAiConf->mSampleRate * pAiConf->mChannelCnt * pAiConf->mBitWidth/8 * pAiConf->mCapDuraSec;
    int avail_nums = 0;
    int count = 0;

    while (1)
    {
        if (gbExitFlag)
            break;

        ret = AW_MPI_AI_GetFrame(stContext.mAIDev, stContext.mAIChn, &nAFrame, NULL, -1);
        if (SUCCESS != ret)
            continue;

        avail_nums = snd_pcm_avail(uac_cfg.uac_handle);
        count = nAFrame.mLen / uac_cfg.bytes_by_frame;
        if (avail_nums >= uac_cfg.bufferSize/2)
        {
            ret = snd_pcm_writei(uac_cfg.uac_handle, nAFrame.mpAddr, count);
            if (-EAGAIN == ret)
            {
                snd_pcm_wait(uac_cfg.uac_handle, 100);
            }
            else if (-EPIPE == ret)
            {
                uac_xrun(uac_cfg.uac_handle);
                aloge("-EPOPE");
            }
            else if (-ESTRPIPE == ret)
            {
                uac_suspend(uac_cfg.uac_handle);
                aloge("-ESTRPIPE");
            }
            else if (ret < 0)
            {
                aloge("writei error[%s]!", snd_strerror(ret));
                uac_suspend(uac_cfg.uac_handle);
            }
        }
        /*nWriteLen = fwrite(nAFrame.mpAddr, 1, nAFrame.mLen, stContext.mFpPcmFile);
        stContext.mPcmSize += nWriteLen;

        //printf("get mic size:%d \n", nAFrame.mLen);
        //printf("get periodsize %d \n", uac_cfg.periodSize * 2);
        ret = snd_pcm_writei(uac_cfg.uac_handle, nAFrame.mpAddr, uac_cfg.periodSize);
        if (ret == -EPIPE)
        {
            printf("underrun occured\n");
        }*/
        ret = AW_MPI_AI_ReleaseFrame(stContext.mAIDev, stContext.mAIChn, &nAFrame, NULL);
        if (SUCCESS != ret)
        {
            aloge("release frame to ai fail! ret: %#x", ret);
        }
        /*if (stContext.mPcmSize >= nMaxWantedSize)
        {
            alogd("capture %d Bytes pcm data, finish!", stContext.mPcmSize);
            break;
        }*/
        usleep(10*1000);
    }
    PcmDataAddWaveHeader(&stContext);
    fclose(stContext.mFpPcmFile);
    stContext.mFpPcmFile = NULL;

    //stop ai chn.
    AW_MPI_AI_DisableChn(stContext.mAIDev, stContext.mAIChn);

    //reset and destroy ai chn & dev.
    AW_MPI_AI_ResetChn(stContext.mAIDev, stContext.mAIChn);
    AW_MPI_AI_DestroyChn(stContext.mAIDev, stContext.mAIChn);

    stContext.mAIDev = MM_INVALID_DEV;
    stContext.mAIChn = MM_INVALID_CHN;

    //exit mpp system
    AW_MPI_SYS_Exit();

    //destroy uac audio card
    snd_pcm_drain(uac_cfg.uac_handle);
    snd_pcm_close(uac_cfg.uac_handle);

_exit:
    if (result == 0) {
	    printf("sample_ai exit!\n");
    }
    return result;
}
