/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    wav_player.h
 * @brief   wav文件播放
 * @author  id:826
 * @version v0.3
 * @date    2017-09-21
 */
#pragma once

#include "EyeseePlayer.h"

#include <mutex>
#include <tsemaphore.h>
namespace EyeseeLinux {

struct WaveHeader {
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
};

struct PcmWaveParam {
    int trackCnt;
    int sampleRate;
    int bitWidth;
    int aoCardType;
};

struct PcmWaveData {
    struct PcmWaveParam param;
    void *buffer;
    int size;
};

class WavParser
{
    public:
        int GetPcmParam(FILE *file, PcmWaveParam &param);

        int GetPcmDataSize(FILE *file);

        int GetPcmData(FILE *file, void *pcm_buf, int pcm_size);

    private:
        int ParseHeader(FILE *file, WaveHeader &header);
};

class WavPlayer
{
    public:
        WavPlayer();

        ~WavPlayer();

        int AOChannelInit(const PcmWaveParam &param);

        int AOChannelInit(int ao_card = 0);

        int Play(const PcmWaveData &pcm_data);

        void  AdjustBeepToneVolume(int val);

        static ERRORTYPE MPPAOCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
    private:
        int ao_dev_;
        int ao_chn_;
        int callback_cnt_;
        int chunk_cnt_;
        std::mutex mutex_;
        cdx_sem_t pcm_callback_sem_;
        cdx_sem_t eof_callback_sem_;
};

}
