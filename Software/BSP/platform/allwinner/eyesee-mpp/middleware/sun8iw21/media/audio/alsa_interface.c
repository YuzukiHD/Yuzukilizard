/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : alsa_interface.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/04/19
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "alsa_interface"
#include <utils/plat_log.h>
#include <SystemBase.h>

#include <alsa_interface.h>

//static long gVolume;
static char aioDebugfsPath[256] = {0};
static unsigned int updateCnt = 0;

#define UPDATE_AIO_DEBUGFS_INFO(update_flag, item_flag, item, type, buf, path)  \
    do {                                                                        \
        if (update_flag & item_flag) {                                          \
            update_flag &= ~item_flag;                                          \
            memset(buf, 0, sizeof(buf));                                        \
            sprintf(buf, "echo %x %d > %s\n", type, item, path);                \
            system(buf);                                                        \
        }                                                                       \
    } while(0)

enum {
    aiDEBit = 0,
    aiCCBit = 1,
    aiCTBit = 2,
    aiSRBit = 3,
    aiBWBit = 4,
    aiTCBit = 5,
    aiBMBit = 6,
    aiVOLBit = 7,

    aoDEBit = 8,
    aoCCBit = 9,
    aoCTBit = 10,
    aoSRBit = 11,
    aoBWBit = 12,
    aoTCBit = 13,
    aoBMBit = 14,
    aoVOLBit = 15,
};

// 0-none item need update; !0-some items need update.
static unsigned int aioDebugUpdataFlag = 0;
enum {
    aiDevEnableFlag = 1<<aiDEBit,     //ai dev updata falg
    aiChnCntFlag = 1<<aiCCBit,
    aiCardTypeFlag = 1<<aiCTBit,
    aiSampleRateFlag = 1<<aiSRBit,
    aiBitWidthFlag = 1<<aiBWBit,
    aiTrackCntFlag = 1<<aiTCBit,
    aibMuteFlag = 1<<aiBMBit,
    aiVolumeFlag = 1<<aiVOLBit,
    aoDevEnableFlag = 1<<aoDEBit,     //ao dev updata falg
    aoChnCntFlag = 1<<aoCCBit,
    aoCardTypeFlag = 1<<aoCTBit,
    aoSampleRateFlag = 1<<aoSRBit,
    aoBitWidthFlag = 1<<aoBWBit,
    aoTrackCntFlag = 1<<aoTCBit,
    aobMuteFlag = 1<<aoBMBit,
    aoVolumeFlag = 1<<aoVOLBit,
};

//
//These params about DevEnable, CardType, SampleRate, BitWidth,
//will be auto updated by alsa kernel.So we should not update it at here.
//
static int aiDevEnable;
static int aiChnCnt;
static int aiCardType;     //0-codec; 1-linein
static int aiSampleRate;
static int aiBitWidth;
static int aiTrackCnt;
static int aibMute;
static int aiVolume;

static int aoDevEnable;
static int aoChnCnt;
static int aoCardType;     //0-codec; 1-hdmi
static int aoSampleRate;
static int aoBitWidth;
static int aoTrackCnt;
static int aobMute;
static int aoVolume;
static int aoPALevel;   //0-low; 1-high

int UpdateDebugfsInfo()
{
    //system("cat /proc/mounts | grep debugfs | awk '{print $2}'");
    // /proc/sys/debug/mpp/sunxi-aio
    if (aioDebugUpdataFlag)
    {
        if (updateCnt++ % 100 == 0)
        {
            FILE *stream;
            char *cmd = "cat /proc/mounts | grep debugfs | awk '{print $2}'";
            int nbytes;

            stream = popen(cmd, "r");
            if (stream == NULL)
                return -1;

            nbytes = fread(aioDebugfsPath, 1, sizeof(aioDebugfsPath), stream);
            if (feof(stream) && nbytes > 0) {

                /*
                 * Fixing the tailing whitespace.
                 */
                for(; nbytes > 0; nbytes--) {
                    if ((aioDebugfsPath[nbytes - 1] == 0x0a) ||
                            (aioDebugfsPath[nbytes - 1] == 0x20))
                        aioDebugfsPath[nbytes - 1] = '\0';
                    else
                        break;
                }

                strncat(aioDebugfsPath, "/mpp/sunxi-aio", sizeof(aioDebugfsPath)- nbytes);
            }
            pclose(stream);
        }

        if (access(aioDebugfsPath, F_OK) != 0) {
            //alogv("sunxi-aio debugfs path not find!");
            return -1;
        }

        char buf[256] = {0};

        // format: echo X YZ > Path, here X must match code with aio.c in kernel
        // ai dev info
        //UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aiDevEnableFlag, aiDevEnable, aiDEBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aiChnCntFlag, aiChnCnt, aiCCBit, buf, aioDebugfsPath);
        //UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aiCardTypeFlag, aiCardType, aiCTBit, buf, aioDebugfsPath);
        //UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aiSampleRateFlag, aiSampleRate, aiSRBit, buf, aioDebugfsPath);
        //UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aiBitWidthFlag, aiBitWidth, aiBWBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aiTrackCntFlag, aiTrackCnt, aiTCBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aibMuteFlag, aibMute, aiBMBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aiVolumeFlag, aiVolume, aiVOLBit, buf, aioDebugfsPath);

        // ao dev info
        //UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aoDevEnableFlag, aoDevEnable, aoDEBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aoChnCntFlag, aoChnCnt, aoCCBit, buf, aioDebugfsPath);
        //UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aoCardTypeFlag, aoCardType, aoCTBit, buf, aioDebugfsPath);
        //UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aoSampleRateFlag, aoSampleRate, aoSRBit, buf, aioDebugfsPath);
       // UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aoBitWidthFlag, aoBitWidth, aoBWBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aoTrackCntFlag, aoTrackCnt, aoTCBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aobMuteFlag, aobMute, aoBMBit, buf, aioDebugfsPath);
        UPDATE_AIO_DEBUGFS_INFO(aioDebugUpdataFlag, aoVolumeFlag, aoVolume, aoVOLBit, buf, aioDebugfsPath);

        aioDebugUpdataFlag = 0;
/*
        // format: echo X YZ > Path, here X must match code with aio.c
        // ai dev info
        if (aioDebugUpdataFlag & aiDevEnableFlag)
        {
            aioDebugUpdataFlag &= ~aiDevEnableFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 0 %d > %s\n", aiDevEnable, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aiChnCntFlag)
        {
            aioDebugUpdataFlag &= ~aiChnCntFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 1 %d > %s\n", aiChnCnt, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aiCardTypeFlag)
        {
            aioDebugUpdataFlag &= ~aiCardTypeFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 2 %d > %s\n", aiCardType, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aiSampleRateFlag)
        {
            aioDebugUpdataFlag &= ~aiSampleRateFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 3 %d > %s\n", aiSampleRate, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aiBitWidthFlag)
        {
            aioDebugUpdataFlag &= ~aiBitWidthFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 4 %d > %s\n", aiBitWidth, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aiTrackCntFlag)
        {
            aioDebugUpdataFlag &= ~aiTrackCntFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 5 %d > %s\n", aiTrackCnt, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aibMuteFlag)
        {
            aioDebugUpdataFlag &= ~aibMuteFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 6 %d > %s\n", aibMute, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aiVolumeFlag)
        {
            aioDebugUpdataFlag &= ~aiVolumeFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 7 %d > %s\n", aiVolume, aioDebugfsPath);
            system(buf);
        }

        // ao dev info
        if (aioDebugUpdataFlag & aoDevEnableFlag)
        {
            aioDebugUpdataFlag &= ~aoDevEnableFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 8 %d > %s\n", aoDevEnable, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aoChnCntFlag)
        {
            aioDebugUpdataFlag &= ~aoChnCntFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo 9 %d > %s\n", aoChnCnt, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aoCardTypeFlag)
        {
            aioDebugUpdataFlag &= ~aoCardTypeFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo a %d > %s\n", aoCardType, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aoSampleRateFlag)
        {
            aioDebugUpdataFlag &= ~aoSampleRateFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo b %d > %s\n", aoSampleRate, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aoBitWidthFlag)
        {
            aioDebugUpdataFlag &= ~aoBitWidthFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo c %d > %s\n", aoBitWidth, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aoTrackCntFlag)
        {
            aioDebugUpdataFlag &= ~aoTrackCntFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo d %d > %s\n", aoTrackCnt, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aobMuteFlag)
        {
            aioDebugUpdataFlag &= ~aobMuteFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo e %d > %s\n", aibMute, aioDebugfsPath);
            system(buf);
        }
        if (aioDebugUpdataFlag & aoVolumeFlag)
        {
            aioDebugUpdataFlag &= ~aoVolumeFlag;
            memset(buf, 0, sizeof(buf));
            sprintf(buf, "echo f %d > %s\n", aoVolume, aioDebugfsPath);
            system(buf);
        }
        aioDebugUpdataFlag = 0;
*/
    }

    return 0;
}

int clearDebugfsInfoByHwDev(int pcmFlag)
{
    if (pcmFlag == 0)
    {
        aiDevEnable = 0;
        aiChnCnt = 0;
        aiCardType = 0;     //0-codec; 1-linein
        aiSampleRate = 0;
        aiBitWidth = 0;
        aiTrackCnt = 0;
        aibMute = 0;
        aiVolume = 0;
        aioDebugUpdataFlag |= 0xff;
    }
    else
    {
        aoDevEnable = 0;
        aoChnCnt = 0;
        aoCardType = 0;     //0-codec; 1-hdmi
        aoSampleRate = 0;
        aoBitWidth = 0;
        aoTrackCnt = 0;
        aobMute = 0;
        aoVolume = 0;
        aioDebugUpdataFlag |= 0xff00;
    }
    UpdateDebugfsInfo();

    return 0;
}

// pcmFlag: 0-cap update; 1-play update
void updateDebugfsByChnCnt(int pcmFlag, int cnt)
{
    if (pcmFlag == 0)
    {
        aiChnCnt = cnt;
        aioDebugUpdataFlag |= aiChnCntFlag;
    }
    else
    {
        aoChnCnt = cnt;
        aioDebugUpdataFlag |= aoChnCntFlag;
    }
}

int alsaSetPcmParams(PCM_CONFIG_S *pcmCfg)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *sw_params;
    int err;
    unsigned int rate, bufTime, periodTime;
//    snd_pcm_uframes_t startThreshold, stopThreshold;
    int dir = 0;

    if (pcmCfg->handle == NULL) {
        aloge("PCM is not open yet!");
        return -1;
    }
    alogd("set pcm params");

    snd_pcm_hw_params_alloca(&params);
//    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(pcmCfg->handle, params);
    if (err < 0) {
        aloge("Broken configuration for this PCM: no configurations available");
        return -1;
    }

    err = snd_pcm_hw_params_set_access(pcmCfg->handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        aloge("Access type not available");
        return -1;
    }

    err = snd_pcm_hw_params_set_format(pcmCfg->handle, params, pcmCfg->format);
    if (err < 0) {
        aloge("Sample format not available");
        return -1;
    }

    err = snd_pcm_hw_params_set_channels(pcmCfg->handle, params, pcmCfg->chnCnt);
    if (err < 0) {
        aloge("Channels count not available");
        return -1;
    }

    rate = pcmCfg->sampleRate;
    err = snd_pcm_hw_params_set_rate_near(pcmCfg->handle, params, &pcmCfg->sampleRate, NULL);
    if (err < 0) {
        aloge("set_rate_near error!");
        return -1;
    }
    if (rate != pcmCfg->sampleRate) {
        alogd("required sample_rate %d is not supported, use %d instead", rate, pcmCfg->sampleRate);
    }

    snd_pcm_uframes_t periodSize = 1024;
    snd_pcm_uframes_t prePeriodSize = periodSize;
    err = snd_pcm_hw_params_set_period_size_near(pcmCfg->handle, params, &periodSize, &dir);
    if (err < 0) {
        aloge("set_period_size_near error!");
        return -1;
    }
    if(prePeriodSize != periodSize)
    {
        alogw("Be careful! periodSize change:%d->%d", prePeriodSize, periodSize);
    }

    // double 1024-sample capacity -> 4
    snd_pcm_uframes_t bufferSize = periodSize * pcmCfg->chnCnt * snd_pcm_format_physical_width(pcmCfg->format) / 2;
    snd_pcm_uframes_t preBufferSize = bufferSize;
    err = snd_pcm_hw_params_set_buffer_size_near(pcmCfg->handle, params, &bufferSize);
    if (err < 0) {
        aloge("set_buffer_size_near error!");
        return -1;
    }
    if(preBufferSize != bufferSize)
    {
        alogw("Be careful! bufferSize change:%d->%d", preBufferSize, bufferSize);
    }

    err = snd_pcm_hw_params(pcmCfg->handle, params);
    if (err < 0) {
        aloge("Unable to install hw params");
        return -1;
    }

    snd_pcm_hw_params_get_period_size(params, &pcmCfg->chunkSize, 0);
    snd_pcm_hw_params_get_buffer_size(params, &pcmCfg->bufferSize);
    if (pcmCfg->chunkSize == pcmCfg->bufferSize) {
        aloge("Can't use period equal to buffer size (%lu == %lu)", pcmCfg->chunkSize, pcmCfg->bufferSize);
        return -1;
    }

    pcmCfg->bitsPerSample = snd_pcm_format_physical_width(pcmCfg->format);
    pcmCfg->significantBitsPerSample = snd_pcm_format_width(pcmCfg->format);
    pcmCfg->bitsPerFrame = pcmCfg->bitsPerSample * pcmCfg->chnCnt;
    pcmCfg->chunkBytes = pcmCfg->chunkSize * pcmCfg->bitsPerFrame / 8;

    alogd("----------------ALSA setting----------------");
    alogd(">>Channels:   %4d, BitWidth:  %4d,phsical_w:%4d, SampRate:   %4d", pcmCfg->chnCnt, pcmCfg->significantBitsPerSample, pcmCfg->bitsPerSample,pcmCfg->sampleRate);
    alogd(">>ChunkBytes: %4d, ChunkSize: %4d, BufferSize: %4d", pcmCfg->chunkBytes, (int)pcmCfg->chunkSize, (int)pcmCfg->bufferSize);

    /* SW params */
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(pcmCfg->handle, sw_params);
    if (snd_pcm_stream(pcmCfg->handle) == SND_PCM_STREAM_CAPTURE) {
        snd_pcm_sw_params_set_start_threshold(pcmCfg->handle, sw_params, 1);
    } else {
        snd_pcm_uframes_t boundary = 0;
        snd_pcm_sw_params_get_boundary(sw_params, &boundary);
        snd_pcm_sw_params_set_start_threshold(pcmCfg->handle, sw_params, pcmCfg->bufferSize);
        /* set silence size, in order to fill silence data into ringbuffer */
        snd_pcm_sw_params_set_silence_size(pcmCfg->handle, sw_params, boundary);
        //alogd("params: boundary:%ld, buffer_size:%ld, period_size:%ld", boundary, pcmCfg->bufferSize, pcmCfg->chunkSize);
    }
    snd_pcm_sw_params_set_stop_threshold(pcmCfg->handle, sw_params, pcmCfg->bufferSize);
    snd_pcm_sw_params_set_avail_min(pcmCfg->handle, sw_params, pcmCfg->chunkSize);
    err = snd_pcm_sw_params(pcmCfg->handle, sw_params);
    if (err < 0) {
        printf("Unable to install sw prams!\n");
        return err;
    }
    return 0;
}

int alsaOpenPcm(PCM_CONFIG_S *pcmCfg, const char *card, int pcmFlag)
{
    snd_pcm_info_t *info;
    snd_pcm_stream_t stream;
    int err;

    int open_mode = 0;

    if (pcmCfg->handle != NULL) {
        alogw("PCM is opened already!");
        return 0;
    }
    alogd("open pcm! card:[%s], pcmFlag:[%d](0-cap;1-play)", card, pcmFlag);

    // 0-cap; 1-play
    stream = (pcmFlag == 0) ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK;
    memset(pcmCfg->cardName, 0, sizeof(pcmCfg->cardName));
    strncpy(pcmCfg->cardName, card, sizeof(pcmCfg->cardName));

    snd_pcm_info_alloca(&info);

    // open_mode |= SND_PCM_NO_AUTO_RESAMPLE;  // not to used the auto resample
    err = snd_pcm_open(&pcmCfg->handle, card, stream, open_mode);
    if (err < 0) {
        aloge("fatal error! card[%s] audio open error: %s", card, snd_strerror(err));
        char *pTestMem = malloc(8*1024);
        if(pTestMem)
        {
            free(pTestMem);
            pTestMem = NULL;
        }
        else
        {
            aloge("fatal error! malloc fail, no memory now!");
        }
//        system("cat /proc/meminfo")
        return -1;
    }
    if ((err = snd_pcm_info(pcmCfg->handle, info)) < 0) {
        aloge("snd_pcm_info error: %s", snd_strerror(err));
        return -1;
    }

    if(0 == pcmCfg->snd_card_id)
    {
        if (stream == SND_PCM_STREAM_CAPTURE) {
            aiDevEnable = 1;
            aiCardType = 0;
            aiSampleRate = pcmCfg->sampleRate;
            aiBitWidth = pcmCfg->bitsPerSample;
            aiTrackCnt = pcmCfg->chnCnt;
            aioDebugUpdataFlag |= aiDevEnableFlag | aiCardTypeFlag | aiSampleRateFlag | aiBitWidthFlag | aiTrackCntFlag | aiVolumeFlag | aibMuteFlag;
        } else if (stream == SND_PCM_STREAM_PLAYBACK) {
            aoDevEnable = 1;
            aoCardType = strncmp(card, "default", 7)==0?0:1;
            aoSampleRate = pcmCfg->sampleRate;
            aoBitWidth = pcmCfg->bitsPerSample;
            aoTrackCnt= pcmCfg->chnCnt;
            aioDebugUpdataFlag |= aoDevEnableFlag | aoCardTypeFlag | aoSampleRateFlag | aoBitWidthFlag | aoTrackCntFlag | aoVolumeFlag | aobMuteFlag;
        }
    }

    return 0;
}

void alsaClosePcm(PCM_CONFIG_S *pcmCfg, int pcmFlag)
{
    //alogd("close pcm");
    clearDebugfsInfoByHwDev(pcmFlag);

    if (pcmCfg->handle == NULL) {
        aloge("PCM is not open yet!");
        return;
    }
    snd_pcm_close(pcmCfg->handle);
    pcmCfg->handle = NULL;
}

void alsaPreparePcm(PCM_CONFIG_S *pcmCfg)
{
    alogd("prepare pcm");

    if (pcmCfg->handle == NULL) {
        aloge("PCM is not open yet!");
        return;
    }
    snd_pcm_prepare(pcmCfg->handle);
}


ssize_t alsaReadPcm(PCM_CONFIG_S *pcmCfg, void *data, size_t rcount)
{
    ssize_t ret;
    ssize_t result = 0;
    int err = 0;
    char cardName[sizeof(pcmCfg->cardName)] = {0};

    if (rcount != pcmCfg->chunkSize)
        rcount = pcmCfg->chunkSize;

    if (pcmCfg == NULL || data == NULL) {
        aloge("invalid input parameter(pcmCfg=%p, data=%p)!", pcmCfg, data);
        return -1;
    }

    while (rcount > 0) {
        /* if(0 == pcmCfg->snd_card_id) // bug fixing,consume too much time to excute shell command,when debugfs is not mounted.
        {
            UpdateDebugfsInfo();
        } */
        ret = snd_pcm_readi(pcmCfg->handle, data, rcount);
        if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < rcount)) {
            snd_pcm_wait(pcmCfg->handle, 100);
        } else if (ret == -EPIPE) {
            aloge("aec_alsa_overflow_xrun:%d-%lld-(%s)!", pcmCfg->snd_card_id, CDX_GetSysTimeUsMonotonic(), strerror(errno));
            snd_pcm_prepare(pcmCfg->handle);
            if(pcmCfg->read_pcm_aec)    // for aec condition,need to return directly and re-trigger cap dma again
            {
                aloge("aec_rtn_drtly");
                return ret;
            }
        } else if (ret == -ESTRPIPE) {
            alogd("need recover(%s)!", strerror(errno));
            snd_pcm_recover(pcmCfg->handle, ret, 0);
        } else if (ret < 0) {
            aloge("read error: %s", snd_strerror(ret));
            return -1;
        }

        if (ret > 0) {
            result += ret;
            rcount -= ret;
            data += ret * pcmCfg->bitsPerFrame / 8;
        }
    }

    return result;
}

ssize_t alsaWritePcm(PCM_CONFIG_S *pcmCfg, void *data, size_t wcount)
{
    ssize_t ret;
    ssize_t result = 0;
    int err = 0;
    char cardName[sizeof(pcmCfg->cardName)] = {0};

    if (snd_pcm_state(pcmCfg->handle) == SND_PCM_STATE_SUSPENDED) {
         while ((err = snd_pcm_resume(pcmCfg->handle)) == -EAGAIN) {
             aloge("snd_pcm_resume again!");
             sleep(1);
        }
        switch(snd_pcm_state(pcmCfg->handle))
        {
            case SND_PCM_STATE_XRUN:
            {
                snd_pcm_drop(pcmCfg->handle);
                break;
            }
            case SND_PCM_STATE_SETUP:
                break;
            default:
            {
                alogw("pcm_lib_state:%s",snd_pcm_state_name(snd_pcm_state(pcmCfg->handle)));
                snd_pcm_prepare(pcmCfg->handle);
                break;
            }
        }
    alsaSetPcmParams(pcmCfg);
    }

    while (wcount > 0) {
//        if(0 == pcmCfg->snd_card_id)
//        {
//            UpdateDebugfsInfo();
//        }
        if (snd_pcm_state(pcmCfg->handle) == SND_PCM_STATE_SETUP) {
            snd_pcm_prepare(pcmCfg->handle);
        }
        ret = snd_pcm_writei(pcmCfg->handle, data, wcount);
        if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < wcount)) {
            snd_pcm_wait(pcmCfg->handle, 100);
        } else if (ret == -EPIPE) {
            //alogv("xrun!");
            snd_pcm_prepare(pcmCfg->handle);
        } else if (ret == -EBADFD) {
            //alogw("careful! current pcm state: %d", snd_pcm_state(pcmCfg->handle));
            snd_pcm_prepare(pcmCfg->handle);
        } else if (ret == -ESTRPIPE) {
            aloge("need recover!");
            snd_pcm_recover(pcmCfg->handle, ret, 0);
        } else if (ret < 0) {
            aloge("write error! ret:%d, %s", ret, snd_strerror(ret));
            //0-cap; 1-play
            alsaClosePcm(pcmCfg, 1);
            //FIXME: reopen
            aloge("cardName:[%s], pcmFlag:[play]", pcmCfg->cardName);
            strncpy(cardName, pcmCfg->cardName, sizeof(pcmCfg->cardName));
            ret = alsaOpenPcm(pcmCfg, cardName, 1);
            if (ret < 0) {
                aloge("alsaOpenPcm failed!");
                return ret;
            }
            ret = alsaSetPcmParams(pcmCfg);
            if (ret < 0) {
                aloge("alsaSetPcmParams failed!");
                return ret;
            }
            if (pcmCfg->handle != NULL) {
                snd_pcm_reset(pcmCfg->handle);
                snd_pcm_prepare(pcmCfg->handle);
                snd_pcm_start(pcmCfg->handle);
            }
            alogd("set pcm prepare finished!");
            return ret;
        }

        if (ret > 0) {
            result += ret;
            wcount -= ret;
            data += ret * pcmCfg->bitsPerFrame / 8;
        }
    }

    return result;
}

int alsaDrainPcm(PCM_CONFIG_S *pcmCfg)
{
    int err = 0;

    err = snd_pcm_drain(pcmCfg->handle);
    if (err != 0){
        aloge("drain pcm err! err=%d", err);
    }

    return err;
}

int alsaOpenMixer(AIO_MIXER_S *mixer, const char *card)
{
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    int err = 0;

    if (mixer->handle != NULL) {
        return 0;
    }
    alogd("open mixer:%s",card);

    snd_mixer_selem_id_alloca(&sid);

    err = snd_mixer_open(&mixer->handle, 0);
    if (err < 0) {
        aloge("Mixer %s open error: %s\n", card, snd_strerror(err));
        return err;
    }

    err = snd_mixer_attach(mixer->handle, card);
    if (err < 0) {
        aloge("Mixer %s attach error: %s\n", card, snd_strerror(err));
        goto ERROR;
    }

    err = snd_mixer_selem_register(mixer->handle, NULL, NULL);
    if (err < 0) {
        aloge("Mixer %s register error: %s\n", card, snd_strerror(err));
        goto ERROR;
    }

    err = snd_mixer_load(mixer->handle);
    if (err < 0) {
        aloge("Mixer %s load error: %s\n", card, snd_strerror(err));
        goto ERROR;
    } 

    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) 
    {
        snd_mixer_selem_get_id(elem, sid);
        //snd_mixer_selem_set_playback_volume_range(elem, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
        //snd_mixer_selem_set_capture_volume_range(elem, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
        // open lineout and mic switch
        const char *elem_name = snd_mixer_selem_get_name(elem);
       alogv("alsa_elem:%s",elem_name);

        if ( !strcmp(elem_name, AUDIO_ADC_MIC1_SWITCH) ) 
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 1);
        } 
        else if ( !strcmp(elem_name, AUDIO_ADC_MIC2_SWITCH) )
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 0);// disable mic2 by default
        }
        else if(!strcmp(elem_name, AUDIO_LINEIN_SWITCH))
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 0);
        }
        else if (!strcmp(elem_name, AUDIO_LINEOUT_VOL)) 
        {
            // lineout volume. 0x1f~0x02 : 0dB~-43.5dB, 1.5dB/step. 27 : -6dB.
            // user had better not change this ctrls, nor will cause wave distort!
            long vol_val = 27;
            snd_mixer_selem_set_playback_volume(elem, 0, vol_val);
            alogd("set playback vol_val to value: %ld", vol_val);
            aoVolume = 100*vol_val/AUDIO_VOLUME_MAX;
            if (aiDevEnable) {
                aioDebugUpdataFlag |= aoVolumeFlag;
            }
        }  
        else if (!strcmp(elem_name, AUDIO_LINEOUT_SWITCH))
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 1);
        } 
        else if (!strcmp(elem_name, AUDIO_LINEOUT_MUX))
        {
            snd_mixer_selem_set_enum_item(elem, 0, 1);    // increase play volume when amplifier differential input
        } 
        else if(!strcmp(elem_name, AUDIO_PA_SWITCH)){
            snd_mixer_selem_set_playback_switch(elem, 0, 1);
            aoPALevel = 0;
        }
    }


    return err;

ERROR:
    snd_mixer_close(mixer->handle);
    mixer->handle = NULL;
    return err;
}

void alsaCloseMixer(AIO_MIXER_S *mixer)
{
    if (mixer->handle == NULL) {
        return;
    }
    alogd("close mixer");

    snd_mixer_close(mixer->handle);
    mixer->handle = NULL;
}

int alsaMixerSetMic2Enable(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_ADC_MIC2_SWITCH))
        {
            aloge("aec_elem_mic2_switch:%s-%d",elem_name,value);
            err = snd_mixer_selem_set_playback_switch(elem, 0, value);
            break;
        } 
    }
    return err;
}

int alsaMixerSetLineInEnable(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_LINEIN_SWITCH))
        {
            aloge("aec_elem_linein_switch:%s-%d",elem_name,value);
            err = snd_mixer_selem_set_playback_switch(elem, 0, value);
            break;
        } 
    }
    return err;
}
int alsaMixerSetCapPlaySyncMode(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_CAP_PLAY_SYNC_MODE))
        {
            aloge("aec_elem_sync_mode_switch:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);   
            
            break;
        } 
    }
    return err;
}
/* to set drc function of dac */
int alsaMixerSetAudioCodecDacDrc(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem); 

       if(!strcmp(elem_name, AUDIO_DACDRC_EN))
        {
            aloge("audio_codec_elem_dac_drc_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);   
            
            break;
        } 
    }
    return err;
}
/* to set drc function of adc */
int alsaMixerSetAudioCodecAdcDrc(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem); 

       if(!strcmp(elem_name, AUDIO_ADCDRC_EN))
        {
            aloge("audio_codec_elem_adc_drc_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);   
            
            break;
        } 
    }
    return err;
}

/* to set hpf function of dac */
int alsaMixerSetAudioCodecDacHpf(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem); 

       if(!strcmp(elem_name, AUDIO_DACHPF_EN))
        {
            aloge("audio_codec_elem_dac_hpf_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);   
            
            break;
        } 
    }
    return err;
}

/* to set hpf function of adc */
int alsaMixerSetAudioCodecAdcHpf(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem); 

       if(!strcmp(elem_name, AUDIO_ADCHPF_EN))
        {
            aloge("audio_codec_elem_adc_hpf_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);   
            
            break;
        } 
    }
    return err;
}

int alsaMixerSetAudioCodecHubMode(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem); 

       if(!strcmp(elem_name, AUDIO_CODEC_HUB_MODE))
        {
            aloge("aec_elem_audio_codec_hub_mode:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);   
            
            break;
        } 
    }
    return err;
}

int alsaMixerSetDAudio0HubMode(AIO_MIXER_S *mixer,  int value)
{
    int err = 0; 

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem); 

       if(!strcmp(elem_name, DAUDIo0_HUB_MODE))
        {
            aloge("aec_elem_daudio0_hub_mode:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);   
            
            break;
        } 
    }
    return err;
}

int alsaMixerSetDAudio0LoopBackEn(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem); 

       if(!strcmp(elem_name, DAUDIo0_LOOPBACK_EN))
        {
            aloge("aec_elem_daudio0_loopback_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_playback_switch(elem, 0, value);
            
            break;
        } 
    }
    return err;
}

int alsaMixerSetVolume(AIO_MIXER_S *mixer, int playFlag, long value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }
    if (value < 0 || value > 100) {
        aloge("want to setAIOVol[0,100], playFlag[%d], but usr value=%ld is invalid!", playFlag, value);
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_LINEOUT_VOL)) {
            long realVol = value*AUDIO_VOLUME_MAX/100;
            err = snd_mixer_selem_set_playback_volume(elem, 0, realVol);
            alogd("playback setVolume:%ld, err:%d", realVol, err);
            aoVolume = value;
            if (aoDevEnable) {
                aioDebugUpdataFlag |= aoVolumeFlag;
            }
            break;
        } 
        else if(!playFlag && !strcmp(elem_name, AUDIO_MIC1_MAIN_GAIN))
        {
            long realVol = value*AUDIO_VOLUME_MAX/100;
            err = snd_mixer_selem_set_playback_volume(elem, 0, realVol);
            alogd("set_ai_main_gain:%d", realVol);
            
        }
        /* else if(!playFlag && !strcmp(elem_name, AUDIO_MIC2_MAIN_GAIN))
         * {
         *     long realVol = value*AUDIO_VOLUME_MAX/100;
         *     err = snd_mixer_selem_set_playback_volume(elem, 0, realVol);
         *     alogd("set_ai_main_gain:%d", realVol);
         *
         * } */
    }
    return err;
}

int alsaMixerGetVolume(AIO_MIXER_S *mixer, int playFlag, long *value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_LINEOUT_VOL)) {
            long realVal;
            err = snd_mixer_selem_get_playback_volume(elem, 0, &realVal);
            // scale from AUDIO_VOLUME_MAX to 100
            *value = realVal * 100 / AUDIO_VOLUME_MAX;
            alogd("playback getVolume:%ld, dst:%ld, err:%d", realVal, *value, err);
            aoVolume = *value;
            if (aoDevEnable) {
                aioDebugUpdataFlag |= aoVolumeFlag;
            }
            break;
        } 
        else if(!playFlag && !strcmp(elem_name, AUDIO_MIC1_MAIN_GAIN))
        {
            long realVal;
            err = snd_mixer_selem_get_playback_volume(elem, 0, &realVal);

            // scale from AUDIO_VOLUME_MAX to 100
            *value = realVal * 100 / AUDIO_VOLUME_MAX;
            alogd("get_ai_main_gain:%ld, dst:%ld, err:%d", realVal, *value, err);
        }
    }
    return err;
}

int alsaMixerSetMute(AIO_MIXER_S *mixer, int playFlag, int bEnable)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_PA_SWITCH/*AUDIO_LINEOUT_SWITCH*/)) {
            alogd("set player master-volume switch state: %d", bEnable);
            if (bEnable) {
                err = snd_mixer_selem_set_playback_switch(elem, 0, 0);
            } else {
                err = snd_mixer_selem_set_playback_switch(elem, 0, 1);
            }
            aobMute = bEnable;
            if (aoDevEnable) {
                aioDebugUpdataFlag |= aobMuteFlag;
            }
            break;
        } 
    }
    return err;
}

int alsaMixerGetMute(AIO_MIXER_S *mixer, int playFlag, int *pVolVal)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_PA_SWITCH)) {
            err = snd_mixer_selem_get_playback_switch(elem, 0, pVolVal);
            alogd("get master-volume (0-mute; 1-unmute) switch state: %d", *pVolVal);
            aobMute = (*pVolVal==0?1:0);
            if (aoDevEnable) {
                aioDebugUpdataFlag |= aobMuteFlag;
            }
            break;
        } 
    }
    return err;
}

int alsaMixerSetPlayBackPA(AIO_MIXER_S *mixer, int bHighLevel)
{
    int err = 0;

    if (mixer->handle == NULL) 
    {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) 
    {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (!strcmp(elem_name, AUDIO_PA_SWITCH)) 
        {
            int ival;
            bHighLevel = bHighLevel?1:0;
            if(snd_mixer_selem_has_playback_switch(elem))
            {
                snd_mixer_selem_get_playback_switch(elem, 0, &ival);
                if (snd_mixer_selem_set_playback_switch(elem, 0, bHighLevel) >= 0)
                {
                }
                else
                {
                    aloge("fatal error! set value of playback switch control of a mixer simple element fail[%d]!", err);
                }
            }
            else
            {
                aloge("fatal error! playback switch control is not present!");
            }
            break;
        }
    }
    return err;
}

int alsaMixerGetPlayBackPA(AIO_MIXER_S *mixer, int *pbHighLevel)
{
    int err = 0;

    if (mixer->handle == NULL) 
    {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) 
    {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (!strcmp(elem_name, AUDIO_PA_SWITCH)) 
        {
            err = snd_mixer_selem_get_playback_switch(elem, 0, pbHighLevel);
            if(err!=0)
            {
                aloge("fatal error! get player pa wrong[0x%x]", err);
            }
            break;
        }
    }
    return err;
}

