
#ifndef _SAMPLE_AO_H_
#define _SAMPLE_AO_H_

#include <stdbool.h>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_clock.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleAOCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAOCmdLineParam;

typedef struct SampleAOConfig
{
    char mPcmFilePath[MAX_FILE_PATH_SIZE];
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int mAoVolume;
    bool mAoSaveDataFlag;
    int log_level;
}SampleAOConfig;

typedef struct SampleAOFrameNode
{
    AUDIO_FRAME_S mAFrame;
    struct list_head mList;
}SampleAOFrameNode;

typedef struct SampleAOFrameManager
{
    struct list_head mIdleList; //SampleAOFrameNode
    struct list_head mUsingList;
    int mNodeCnt;
    pthread_mutex_t mLock;
    AUDIO_FRAME_S* (*PrefetchFirstIdleFrame)(void *pThiz);
    int (*UseFrame)(void *pThiz, AUDIO_FRAME_S *pFrame);
    int (*ReleaseFrame)(void *pThiz, unsigned int nFrameId);
}SampleAOFrameManager;
int initSampleAOFrameManager();
int destroySampleAOFrameManager();

typedef struct SampleAOContext
{
    SampleAOCmdLineParam mCmdLinePara;
    SampleAOConfig mConfigPara;

    FILE *mFpPcmFile;
    SampleAOFrameManager mFrameManager;
    pthread_mutex_t mWaitFrameLock;
    int mbWaitFrameFlag;
    cdx_sem_t mSemFrameCome;
    cdx_sem_t mSemEofCome;

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAODev;
    AO_CHN mAOChn;
    AIO_ATTR_S mAIOAttr;
    //CLOCK_CHN mClockChn;
    //CLOCK_CHN_ATTR_S mClockChnAttr;
}SampleAOContext;
int initSampleAOContext();
int destroySampleAOContext();

#endif  /* _SAMPLE_VO_H_ */

