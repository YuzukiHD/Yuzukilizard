
#ifndef _SAMPLE_AI2AO_H_
#define _SAMPLE_AI2AO_H_

#include <plat_type.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleAI2AOCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAI2AOCmdLineParam;

typedef struct SampleAI2AOConfig
{
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    BOOL mTunnelMode;       // tunnel or non-tunnel
    int mCapDurationSec;
}SampleAI2AOConfig;

typedef struct pcmFrameNode {
    AUDIO_FRAME_S frame;
    struct list_head mList;
} pcmFrameNode;

typedef struct SampleAI2AOContext
{
    SampleAI2AOCmdLineParam mCmdLinePara;
    SampleAI2AOConfig mConfigPara;

    FILE *mFpPcmFile;
    int mPcmSize;

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAIODev;
    AI_CHN mAIChn;
    AO_CHN mAOChn;
    CLOCK_CHN mClkChn;
    AIO_ATTR_S mAIOAttr;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    pthread_mutex_t mPcmlock;
    struct list_head mPcmIdleList;   // AUDIO_FRAME_INFO_S
    struct list_head mPcmUsingList;   // AUDIO_FRAME_INFO_S

    BOOL mOverFlag;
}SampleAI2AOContext;

#endif  /* _SAMPLE_AI2AO_H_ */

