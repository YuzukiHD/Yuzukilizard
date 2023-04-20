#ifndef __SAMPLE_AI2AENC2MUXER_H__
#define __SAMPLE_AI2AENC2MUXER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <confparser.h>

#define MAX_FILE_PATH_LEN  (128)

#define DST_FILE_PATH       "dst_file"
#define CODEC_TYPE          "codec_type"
#define CAPTURE_DURATION    "cap_dura"
#define CHANNEL_COUNT       "chn_cnt"
#define BIT_WIDTH           "bit_width"
#define SAMPLE_RATE         "smp_rate"
#define KEY_BITRATE         "bitRate"

typedef struct SAMPLE_AI2AENC2MUXER_S
{
    char confFilePath[MAX_FILE_PATH_LEN];

    FILE *mFpDst;

    CONFPARSER_S mConfParser;
    MPP_SYS_CONF_S mSysConf;

    char mConfDstFile[MAX_FILE_PATH_LEN];
    char mConfCodecType[16];
    int mConfCapDuration;   // unit: s
    int mConfChnCnt;
    int mConfBitWidth;
    int mConfSampleRate;
    int mConfBitRate;
    bool mConfAISaveFileFlag;

    MPP_CHN_S mAiChn;
    MPP_CHN_S mAEncChn;

    AUDIO_DEV mAIDevId;
    AI_CHN mAIChnId;
    AIO_ATTR_S mAioAttr;
    AENC_CHN mAEncChnId;
    AENC_CHN_ATTR_S mAEncAttr;

    AUDIO_SAVE_FILE_INFO_S mSaveFileInfo;
}SAMPLE_AI2AENC2MUXER_S;

#endif //#define __SAMPLE_AI2AENC2MUXER_H__

