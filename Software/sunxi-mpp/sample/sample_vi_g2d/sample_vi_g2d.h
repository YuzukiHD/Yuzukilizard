#ifndef _SAMPLE_VI_G2D_H_
#define _SAMPLE_VI_G2D_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct G2dConvertParam
{
    int mSrcRectX;
    int mSrcRectY;
    int mSrcRectW;
    int mSrcRectH;
    int mDstRotate; //0, 90, 180, 270, clock-wise.
    int mDstWidth;
    int mDstHeight;
    int mDstRectX;
    int mDstRectY;
    int mDstRectW;
    int mDstRectH;
}G2dConvertParam;

typedef struct SampleViG2d_G2dConvert SampleViG2d_G2dConvert;
typedef struct SampleViG2d_G2dConvert
{
    int mG2dFd;
    G2dConvertParam mConvertParam;
    int (*G2dOpen)();
    int (*G2dClose)();
    int (*G2dSetConvertParam)(SampleViG2d_G2dConvert *pThiz, G2dConvertParam *pConvertParam);
    int (*G2dConvertVideoFrame)(SampleViG2d_G2dConvert *pThiz, VIDEO_FRAME_INFO_S* pSrcFrameInfo, VIDEO_FRAME_INFO_S* pDstFrameInfo);
} SampleViG2d_G2dConvert;
SampleViG2d_G2dConvert *constructSampleViG2d_G2dConvert();
void destructSampleViG2d_G2dConvert(SampleViG2d_G2dConvert *pThiz);

typedef struct SampleViG2d_VideoFrameManager SampleViG2d_VideoFrameManager;
typedef struct SampleViG2d_VideoFrameManager
{
    struct list_head mIdleFrameList;    //VideoFrameInfoNode
    struct list_head mFillingFrameList;
    struct list_head mReadyFrameList;
    struct list_head mUsingFrameList;
    bool mWaitIdleFrameFlag;
    bool mWaitReadyFrameFlag;
    
    pthread_mutex_t mLock;
    pthread_cond_t mIdleFrameCond;
    pthread_cond_t mReadyFrameCond;
    int mFrameNodeNum;
    PIXEL_FORMAT_E mPixelFormat;
    int mBufWidth;
    int mBufHeight;

    VIDEO_FRAME_INFO_S* (*GetIdleFrame)(SampleViG2d_VideoFrameManager *pThiz, int nTimeout); //unit:ms
    int                 (*FillingFrameDone)(SampleViG2d_VideoFrameManager *pThiz, VIDEO_FRAME_INFO_S *pFrame);
    int                 (*FillingFrameFail)(SampleViG2d_VideoFrameManager *pThiz, VIDEO_FRAME_INFO_S *pFrame);
    VIDEO_FRAME_INFO_S* (*GetReadyFrame)(SampleViG2d_VideoFrameManager *pThiz, int nTimeout); //unit:ms
    int                 (*UsingFrameDone)(SampleViG2d_VideoFrameManager *pThiz, VIDEO_FRAME_INFO_S *pFrame);
    
    int (*QueryIdleFrameNum)(SampleViG2d_VideoFrameManager *pThiz);
    int (*QueryFillingFrameNum)(SampleViG2d_VideoFrameManager *pThiz);
    int (*QueryReadyFrameNum)(SampleViG2d_VideoFrameManager *pThiz);
    int (*QueryUsingFrameNum)(SampleViG2d_VideoFrameManager *pThiz);
} SampleViG2d_VideoFrameManager;
SampleViG2d_VideoFrameManager *constructSampleViG2d_VideoFrameManager(int nFrameNum, PIXEL_FORMAT_E nPixelFormat, int nBufWidth, int nBufHeight);
void destructSampleViG2d_VideoFrameManager(SampleViG2d_VideoFrameManager *pThiz);

typedef struct SampleViCapS {
    bool mbThreadExistFlag;
    pthread_t mThreadId;
    VI_DEV mDev;
    VI_CHN mChn;
    VI_ATTR_S mViAttr;
    ISP_DEV mIspDev;
    int mTimeout;   //unit:ms
    //VIDEO_FRAME_INFO_S mFrameInfo;
    void *mpContext;    //SampleViG2dContext*

    int mRawStoreNum;

    //cdx_sem_t mSemFrameBack;
    //unsigned int mCurFrameId;

    SampleViG2d_G2dConvert *mpG2dConvert;
} SampleViCapS;
int initSampleViCapS(SampleViCapS *pThiz);
int destroySampleViCapS(SampleViCapS *pThiz);

typedef struct SampleVoDisplayS {
    bool mbThreadExistFlag;
    pthread_t mThreadId;
    VO_DEV mVoDev;
    VO_PUB_ATTR_S mPubAttr;
    VO_LAYER mUILayer;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;
    void *mpContext;    //SampleViG2dContext*
} SampleVoDisplayS;
int initSampleVoDisplayS(SampleVoDisplayS *pThiz);
int destroySampleVoDisplayS(SampleVoDisplayS *pThiz);

typedef struct SampleViG2dCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleViG2dCmdLineParam;

typedef struct SampleViG2dConfig
{
    int mDevNum;
    int mFrameRate;
    PIXEL_FORMAT_E  mPicFormat;
    int mDropFrameNum;
    int mSrcWidth;
    int mSrcHeight;
    int mSrcRectX;
    int mSrcRectY;
    int mSrcRectW;
    int mSrcRectH;
    int mDstRotate; //0, 90, 180, 270, clock-wise.
    int mDstWidth;
    int mDstHeight;
    int mDstRectX;
    int mDstRectY;
    int mDstRectW;
    int mDstRectH;
    int mDstStoreCount;
    int mDstStoreInterval;
    char mStoreDir[MAX_FILE_PATH_SIZE];
    bool mDisplayFlag;
    int mDisplayX;
    int mDisplayY;
    int mDisplayW;
    int mDisplayH;
    int mTestDuration;  //unit:s
}SampleViG2dConfig;

typedef struct SampleViG2dContext
{
    SampleViG2dCmdLineParam mCmdLinePara;
    SampleViG2dConfig mConfigPara;

    MPP_SYS_CONF_S mSysConf;
    SampleViCapS mViCapCtx;
    SampleVoDisplayS mVoDisplayCtx;
    SampleViG2d_VideoFrameManager *mpFrameManager;

    bool mbExitFlag;
    cdx_sem_t mSemExit;
}SampleViG2dContext;
SampleViG2dContext *constructSampleViG2dContext();
void destructSampleViG2dContext(SampleViG2dContext *pThiz);

#endif  /* _SAMPLE_VI_G2D_H_ */

