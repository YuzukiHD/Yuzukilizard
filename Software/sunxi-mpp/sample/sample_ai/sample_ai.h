
#ifndef _SAMPLE_AI_H_
#define _SAMPLE_AI_H_

#include <plat_type.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleAICmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAICmdLineParam;

typedef struct SampleAIConfig
{
    char mPcmFilePath[MAX_FILE_PATH_SIZE];
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int mCapDuraSec;
    int ai_gain;
}SampleAIConfig;

typedef struct SampleAIContext
{
    SampleAICmdLineParam mCmdLinePara;
    SampleAIConfig mConfigPara;

    FILE *mFpPcmFile;
    int mPcmSize;

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAIDev;
    AI_CHN mAIChn;
    AIO_ATTR_S mAIOAttr;
}SampleAIContext;

#endif  /* _SAMPLE_AI_H_ */

