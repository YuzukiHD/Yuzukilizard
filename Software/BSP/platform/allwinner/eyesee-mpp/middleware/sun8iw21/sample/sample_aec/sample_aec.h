#ifndef _SAMPLE_AEC_H_
#define _SAMPLE_AEC_H_

#include <stdbool.h>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_clock.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleAecCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAecCmdLineParam;

typedef struct SampleAecConfig
{
    char mPcmSrcPath[MAX_FILE_PATH_SIZE];
    char mPcmDstPath[MAX_FILE_PATH_SIZE];
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int mAiAecEn;
    int mAecDelayMs;
    int mAiAnsEn;
    int mAiAnsMode;
    int mAiAgcEn;
    int mAiAgcGain;
    bool mbAddWavHeader;
}SampleAecConfig;

typedef struct SampleAecFrameNode
{
    AUDIO_FRAME_S mAFrame;
    struct list_head mList;
}SampleAecFrameNode;

typedef struct SampleAecFrameManager
{
    struct list_head mIdleList; //SampleAecFrameNode
    struct list_head mUsingList;
    int mNodeCnt;
    pthread_mutex_t mLock;
    AUDIO_FRAME_S* (*PrefetchFirstIdleFrame)(void *pThiz);
    int (*UseFrame)(void *pThiz, AUDIO_FRAME_S *pFrame);
    int (*ReleaseFrame)(void *pThiz, unsigned int nFrameId);
}SampleAecFrameManager;

typedef struct SampleAecContext
{
    SampleAecCmdLineParam mCmdLinePara;
    SampleAecConfig mConfigPara;

    int nPlayChnNum;
    int nPlaySampleRate;
    int nPlayBitsPerSample;

    FILE *mFpPcmFile;
    SampleAecFrameManager mFrameManager;
    pthread_mutex_t mWaitFrameLock;
    int mbWaitFrameFlag;
    cdx_sem_t mSemFrameCome;
    cdx_sem_t mSemEofCome;

    FILE *mFpSaveWavFile;
    int mSavePcmSize;   //unit:bytes

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAIODev;
    AO_CHN mAOChn;
    AI_CHN mAIChn;
    AIO_ATTR_S mAIOAttr;
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    pthread_t mSendPcmAoTid;
    pthread_t mAiSaveDataTidPcm;
    volatile BOOL mOverFlag;    //exit flag

    int eof_flag;   //sent by ao

    //various test
    int mPauseSendTm;   //unit:ms. when reach this time, pause send pcm data.
    int mPauseDuration; //unit:ms, the duration of pause sending pcm data.
    bool mbPauseDone;   //has already pause.
}SampleAecContext;
int initSampleAecContext(SampleAecContext *pContext);
int destroySampleAecContext(SampleAecContext *pContext);

int initSampleAecFrameManager(SampleAecFrameManager *pFrameManager, int nFrameNum, SampleAecConfig *pConfigPara, SampleAecContext* pCtx);
int destroySampleAecFrameManager(SampleAecFrameManager *pFrameManager);

#endif  /* _SAMPLE_AEC_H_ */

