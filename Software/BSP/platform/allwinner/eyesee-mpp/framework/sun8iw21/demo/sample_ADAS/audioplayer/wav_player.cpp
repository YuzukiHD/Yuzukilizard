/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    wav_player.cpp
 * @brief   wav文件播放
 * @author  id:826
 * @version v0.3
 * @date    2017-09-21
 */

#include "wav_player.h"
#include "common/app_log.h"

#include <string.h>
#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_ao.h>
#include <mpi_ai.h>
using namespace std;
using namespace EyeseeLinux;

int WavParser::GetPcmParam(FILE *file, PcmWaveParam &param)
{
    WaveHeader header;

    if (file == NULL) {
        db_error("file is NULL");
        return -1;
    }

    if (ParseHeader(file, header) < 0) {
        db_error("parse wav header failed");
        return -1;
    }

    param.trackCnt = header.num_chn;
    param.bitWidth = header.bits_per_sample;
    param.sampleRate = header.sample_rate;

    db_info("audio file format(%d, %d, %d)", param.trackCnt, param.bitWidth, param.sampleRate);

    return 0;
}

int WavParser::GetPcmDataSize(FILE *file)
{
    if (file == NULL) {
        db_error("file is NULL");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    return ftell(file) - sizeof(WaveHeader);
}

int WavParser::GetPcmData(FILE *file, void *pcm_buf, int pcm_size)
{
    if (file == NULL) {
        db_error("file is NULL");
        return -1;
    }

    if (pcm_buf == NULL) {
        db_error("pcm_buf is NULL");
        return -1;
    }

    fseek(file, sizeof(WaveHeader), SEEK_SET);

    return fread(pcm_buf, 1, pcm_size, file);
}

int WavParser::ParseHeader(FILE *file, WaveHeader &header)
{
    int ret;

    if (file == NULL) {
        db_error("file is NULL");
        return -1;
    }

    fread(&header, 1, sizeof(header), file);
    if ((ret=strncmp("WAVE", (char*)&header.riff_fmt, 4))) {
        char *ptr = (char*)&header.riff_fmt;
        db_error("audio file is not wav file! exit Parser!(%d)(%s)(%c%c%c%c)",
                ret, "WAVE", ptr[0], ptr[1], ptr[2], ptr[3]);
        return -1;
    }

    return 0;
}

WavPlayer::WavPlayer()
    : ao_dev_(0)
    , ao_chn_(0)
    , callback_cnt_(0)
    , chunk_cnt_(0)
{
    // cdx_sem_init(&pcm_callback_sem_, 0);
    cdx_sem_init(&eof_callback_sem_, 0);
}

WavPlayer::~WavPlayer()
{
    db_debug("WavPlayer destruct");
    // cdx_sem_deinit(&pcm_callback_sem_);
    cdx_sem_deinit(&eof_callback_sem_);
    AW_MPI_AO_StopChn(ao_dev_, ao_chn_);
    AW_MPI_AO_DestroyChn(ao_dev_, ao_chn_);
}

int WavPlayer::AOChannelInit(int ao_card)
{
    //init mpp system  
    MPP_SYS_CONF_S mSysConf;
    PcmWaveParam param;
   
    param.trackCnt = 1;
    param.sampleRate = 44100;//44100;
    param.bitWidth = 16;

    switch (ao_card) {
        case 0:
            param.aoCardType = PCM_CARD_TYPE_AUDIOCODEC;
            break;
        case 1:
            param.aoCardType = PCM_CARD_TYPE_SNDHDMI;
            break;
        default:
            param.aoCardType = PCM_CARD_TYPE_AUDIOCODEC;
            break;
    }

    db_debug("param dump: ");
    db_debug("trackCnt: %d, sampleRate: %d, bitwidth: %d, aoCardType: %d",
            param.trackCnt, param.sampleRate, param.bitWidth, param.aoCardType);

    return AOChannelInit(param);
}

int WavPlayer::AOChannelInit(const PcmWaveParam &param)
{
    int ret;

    lock_guard<mutex> lock(mutex_);

    AIO_ATTR_S mAIOAttr;
    AW_MPI_AO_GetPubAttr(ao_dev_, ao_chn_, &mAIOAttr);
    db_debug("enWorkmode:%d enSoundmode:%d u32EXFlag:%d u32FrmNum:%d u32PtNumPerFrm:%d u32ChnCnt:%d u32ClkSel:%d",
		mAIOAttr.enWorkmode,mAIOAttr.enSoundmode,mAIOAttr.u32EXFlag, mAIOAttr.u32FrmNum,mAIOAttr.u32PtNumPerFrm,mAIOAttr.u32ChnCnt,mAIOAttr.u32ClkSel);

    memset(&mAIOAttr, 0, sizeof(AIO_ATTR_S));
    mAIOAttr.u32ChnCnt = param.trackCnt;
    mAIOAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)param.sampleRate;
    mAIOAttr.enBitwidth = (AUDIO_BIT_WIDTH_E)(param.bitWidth / 8 - 1);
    mAIOAttr.mPcmCardId = (PCM_CARD_TYPE_E)param.aoCardType;

    db_debug("AIO_ATTR_S: [%d, %d, %d]",
            mAIOAttr.u32ChnCnt, mAIOAttr.enSamplerate, mAIOAttr.enBitwidth);

    AW_MPI_AO_SetPubAttr(ao_dev_, ao_chn_, &mAIOAttr);

    while(ao_chn_ < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(ao_dev_, ao_chn_);
        if(SUCCESS == ret)
        {
            db_info("create ao channel[%d] success!", ao_chn_);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            db_info("ao channel[%d] exist, find next!", ao_chn_);
            ao_chn_++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            db_error("audio_hw_ao not started!");
            break;
        }
        else
        {
            db_error("create ao channel[%d] fail! ret[0x%x]!", ao_chn_, ret);
            break;
        }
    }
    if(SUCCESS != ret)
    {
        ao_chn_ = MM_INVALID_CHN;
        db_error("fatal error! create ao channel fail!");
        return -1;
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = this;
    cbInfo.callback = (MPPCallbackFuncType)&MPPAOCallback;
    AW_MPI_AO_RegisterCallback(ao_dev_, ao_chn_, &cbInfo);
       AW_MPI_AO_StartChn(ao_dev_, ao_chn_);
       //AW_MPI_AO_SetVolume(ao_dev_, 100);
    return 0;
}

void WavPlayer::AdjustBeepToneVolume(int val)
{
    AW_MPI_AO_SetDevVolume(ao_dev_, val);
}


int WavPlayer::Play(const PcmWaveData &pcm_data)
{
    lock_guard<mutex> lock(mutex_);

    int rdId = 0;

    if (pcm_data.buffer == NULL || pcm_data.size == 0) {
        db_error("pcm buffer is NULL");
        return -1;
    }

    int chunk_size = pcm_data.param.trackCnt * 2 * 1024;
    chunk_cnt_ = (pcm_data.size + chunk_size - 1) / chunk_size;

    // db_info("pcmSize:%d, chunkSize: %d, chunkTotalCnt:%d", pcm_data.size, chunk_size, chunk_cnt_);

    AUDIO_FRAME_S frmInfo;
    bzero(&frmInfo, sizeof(frmInfo));

    while (1) {
        frmInfo.mpAddr = pcm_data.buffer + chunk_size * rdId++;
        if (rdId != chunk_cnt_) {
            frmInfo.mLen = chunk_size;
        } else {
            frmInfo.mLen = chunk_size - (chunk_cnt_ * chunk_size - pcm_data.size);
        }

        AW_MPI_AO_SendFrame(ao_dev_, ao_chn_, &frmInfo, 0);

        if (rdId == chunk_cnt_) {
            if (callback_cnt_ != chunk_cnt_) {
                if (ETIMEDOUT == cdx_sem_down_timedwait(&eof_callback_sem_, 3000)) {
                    db_warn("cdx_sem_down_timedwait, 3000ms");
                }
            }
            AW_MPI_AO_SetStreamEof(ao_dev_, ao_chn_, TRUE, TRUE);
            usleep(50*1000);
            rdId = 0;
            callback_cnt_ = 0;
            AW_MPI_AO_SetStreamEof(ao_dev_, ao_chn_, FALSE, FALSE);
            AW_MPI_AO_StartChn(ao_dev_, ao_chn_);
            break;
        }
    }

    return 0;
}

ERRORTYPE WavPlayer::MPPAOCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    WavPlayer *self = (WavPlayer*)cookie;

    if (event == MPP_EVENT_RELEASE_AUDIO_BUFFER) {
        self->callback_cnt_++;
        // db_debug("notify release data! callback_cnt_: %d, chunk_cnt_: %d", self->callback_cnt_, self->chunk_cnt_);
        if (self->callback_cnt_ == self->chunk_cnt_)
        {
            db_debug("AO callback_cnt_:%d, find EOF, pcm play END!", self->callback_cnt_);
            cdx_sem_up(&self->eof_callback_sem_);
        }
    } else if (event == MPP_EVENT_NOTIFY_EOF) {
        return 0;
    } else {
        db_error("why AO nontify event:%d?!", event);
    }

    return 0;
}

