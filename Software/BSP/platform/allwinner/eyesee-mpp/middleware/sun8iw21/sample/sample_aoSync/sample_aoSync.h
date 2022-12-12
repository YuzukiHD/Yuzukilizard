
#ifndef _SAMPLE_AO_SYNC_H_
#define _SAMPLE_AO_SYNC_H_

#include <stdbool.h>

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleAOSyncCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAOSyncCmdLineParam;

typedef struct SampleAOSyncConfig
{
    char mPcmFilePath[MAX_FILE_PATH_SIZE];
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int mAoVolume;
    bool mbDrcEnable;
    int mTestDuration;
    int mParseWavHeaderEnable;
}SampleAOSyncConfig;

typedef struct SampleAOSyncContext
{
    SampleAOSyncCmdLineParam mCmdLinePara;
    SampleAOSyncConfig mConfigPara;

    FILE *mFpPcmFile;
    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAODev;
    AO_CHN mAOChn;
    AIO_ATTR_S mAIOAttr;

    pthread_t mSendThdId;
    pthread_t mVolumeAdjustThdId;
    BOOL mOverFlag;
}SampleAOSyncContext;

#endif  /* _SAMPLE_AO_SYNC_H_ */

