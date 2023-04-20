
#ifndef _SAMPLE_AO_H_
#define _SAMPLE_AO_H_

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
	
	int second_ao_chl_en;
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
    SampleAOConfig mConfigPara[2];

    FILE *mFpPcmFile[2];
    SampleAOFrameManager mFrameManager[2];
    pthread_mutex_t mWaitFrameLock[2];
    int mbWaitFrameFlag[2];
    cdx_sem_t mSemFrameCome[2];
    cdx_sem_t mSemEofCome[2];

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAODev;
    AO_CHN mAOChn[2];
    AIO_ATTR_S mAIOAttr[2];
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;
}SampleAOContext;
int initSampleAOContext();
int destroySampleAOContext();

#endif  /* _SAMPLE_VO_H_ */

