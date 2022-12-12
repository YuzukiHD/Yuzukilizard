
#ifndef _SAMPLE_AO2AI_H_
#define _SAMPLE_AO2AI_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_clock.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleAO2AICmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAO2AICmdLineParam;

typedef struct SampleAO2AIConfig
{
    char mPcmSrcPath[MAX_FILE_PATH_SIZE];
    char mPcmDstPath[MAX_FILE_PATH_SIZE];
    char mAacDstPath[MAX_FILE_PATH_SIZE];
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int ai_aec_en;
    int aec_delay_ms;

    int ai_ans_en;
    int ai_ans_mode;
    int ai_agc_en;
    int ao_drc_en;
    int volume;
    int log_level;
}SampleAO2AIConfig;

typedef struct SampleAO2AIFrameNode
{
    AUDIO_FRAME_S mAFrame;
    struct list_head mList;
}SampleAO2AIFrameNode;

typedef struct SampleAO2AIFrameManager
{
    struct list_head mIdleList; //SampleAO2AIFrameNode
    struct list_head mUsingList;
    int mNodeCnt;
    pthread_mutex_t mLock;
    AUDIO_FRAME_S* (*PrefetchFirstIdleFrame)(void *pThiz);
    int (*UseFrame)(void *pThiz, AUDIO_FRAME_S *pFrame);
    int (*ReleaseFrame)(void *pThiz, unsigned int nFrameId);
}SampleAO2AIFrameManager;
int initSampleAO2AIFrameManager(SampleAO2AIFrameManager *pFrameManager, int nFrameNum, SampleAO2AIConfig *pConfigPara);
int destroySampleAO2AIFrameManager(SampleAO2AIFrameManager *pFrameManager);

typedef struct SampleAO2AIContext
{
    SampleAO2AICmdLineParam mCmdLinePara;
    SampleAO2AIConfig mConfigPara;

    FILE *mFpPcmFile;
    SampleAO2AIFrameManager mFrameManager;
    pthread_mutex_t mWaitFrameLock;
    int mbWaitFrameFlag;
    cdx_sem_t mSemFrameCome;
    cdx_sem_t mSemEofCome;

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAIODev;
    AO_CHN mAOChn;
    AI_CHN mAIChn;
    AI_CHN mAIChn2;
    AIO_ATTR_S mAIOAttr;
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;

    pthread_t mAiSaveDataTid;
    pthread_t mAiSaveDataTidPcm;
    volatile BOOL mOverFlag;


    
    AENC_CHN mAencChn;
    AENC_ATTR_S mAencAttr;

    int eof_flag;

}SampleAO2AIContext;
int initSampleAO2AIContext(SampleAO2AIContext *pContext);
int destroySampleAO2AIContext(SampleAO2AIContext *pContext);

#endif  /* _SAMPLE_AO2AI_H_ */

