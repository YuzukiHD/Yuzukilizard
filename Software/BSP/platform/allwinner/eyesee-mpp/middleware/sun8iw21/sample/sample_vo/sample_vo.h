
#ifndef _SAMPLE_VO_H_
#define _SAMPLE_VO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_clock.h>
#include <mm_comm_vo.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleVOCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleVOCmdLineParam;

typedef struct SampleVOConfig
{
    char mYuvFilePath[MAX_FILE_PATH_SIZE];
    int mPicWidth;
    int mPicHeight;
    int mDisplayWidth;
    int mDisplayHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;
}SampleVOConfig;

typedef struct SampleVOFrameNode
{
    VIDEO_FRAME_INFO_S mFrame;
    struct list_head mList;
}SampleVOFrameNode;

typedef struct SampleVOFrameManager
{
    struct list_head mIdleList; //SampleVOFrameNode
    struct list_head mUsingList;
    int mNodeCnt;
    pthread_mutex_t mLock;
    VIDEO_FRAME_INFO_S* (*PrefetchFirstIdleFrame)(void *pThiz);
    int (*UseFrame)(void *pThiz, VIDEO_FRAME_INFO_S *pFrame);
    int (*ReleaseFrame)(void *pThiz, unsigned int nFrameId);
}SampleVOFrameManager;
int initSampleVOFrameManager();
int destroySampleVOFrameManager();

typedef struct SampleVOContext
{
    SampleVOCmdLineParam mCmdLinePara;
    SampleVOConfig mConfigPara;

    FILE *mFpYuvFile;
    SampleVOFrameManager mFrameManager;
    pthread_mutex_t mWaitFrameLock;
    int mbWaitFrameFlag;
    cdx_sem_t mSemFrameCome;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;
}SampleVOContext;
int initSampleVOContext();
int destroySampleVOContext();

#endif  /* _SAMPLE_VO_H_ */

