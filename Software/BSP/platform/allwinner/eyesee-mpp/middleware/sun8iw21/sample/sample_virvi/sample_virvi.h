#ifndef _SAMPLE_VIRVI_H_
#define _SAMPLE_VIRVI_H_

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE  (256)
#define MAX_CAPTURE_NUM     (4)

typedef struct SampleVirViCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
} SampleVirViCmdLineParam;

typedef struct SampleVirViConfig
{
    int AutoTestCount;
    int GetFrameCount;
    int DevNum;
    int mIspDevNum;
    int PicWidth;
    int PicHeight;
    int FrameRate;
    PIXEL_FORMAT_E PicFormat; //V4L2_PIX_FMT_NV21M
    int mEnableWDRMode;
    enum v4l2_colorspace mColorSpace;
    int mViDropFrmCnt;
} SampleVirViConfig;

typedef struct SampleVirviCap
{
    BOOL mbCapValid;
    BOOL mbExitFlag;
    BOOL mbTrdRunning;
    pthread_t thid;
    VI_DEV Dev;
    ISP_DEV mIspDev;
    VI_CHN Chn;
    int s32MilliSec;
    VIDEO_FRAME_INFO_S pstFrameInfo;
    int mRawStoreNum;   //current save raw picture num
    void *mpContext;    //SampleVirViContext*

    SampleVirViConfig mConfig;
} SampleVirviCap;

typedef struct SampleVirviSaveBufNode
{
    int mId;
    int mFrmCnt;
    int mDataLen;
    unsigned int mDataPhyAddr;
    void *mpDataVirAddr;
    int mFrmLen;
    SIZE_S mFrmSize;
    PIXEL_FORMAT_E mFrmFmt;
    void *mpCap;    //SampleVirViContext*

    struct list_head mList;
}SampleVirviSaveBufNode;

typedef struct SampleVirviSaveBufMgrConfig
{
    VI_DEV mSavePicDev;
    int mYuvFrameCount;
    char mYuvFile[MAX_FILE_PATH_SIZE];

    int mRawStoreCount; //the picture number of storing. 0 means don't store pictures.
    int mRawStoreInterval; //n: store one picture of n pictures.
    char mStoreDirectory[MAX_FILE_PATH_SIZE];   //e.g.: /mnt/extsd

    int mSavePicBufferNum;
    int mSavePicBufferLen;
}SampleVirviSaveBufMgrConfig;

typedef struct SampleVirviSaveBufMgr
{
    BOOL mbTrdRunningFlag;
    struct list_head mIdleList;     //SampleVirviSaveBufNode
    struct list_head mReadyList;    //SampleVirviSaveBufNode
    pthread_mutex_t mIdleListLock;
    pthread_mutex_t mReadyListLock;

    SampleVirviSaveBufMgrConfig mConfig;
}SampleVirviSaveBufMgr;

typedef struct SampleVirViContext
{
    SampleVirViCmdLineParam mCmdLinePara;
    SampleVirviCap mCaps[MAX_CAPTURE_NUM];

    int mTestDuration;
    cdx_sem_t mSemExit;
    BOOL mbSaveCsiTrdExitFlag;
    SampleVirviSaveBufMgrConfig mSaveBufMgrConfig;
    SampleVirviSaveBufMgr *mpSaveBufMgr;
} SampleVirViContext;

#endif  /* _SAMPLE_VIRVI_H_ */
