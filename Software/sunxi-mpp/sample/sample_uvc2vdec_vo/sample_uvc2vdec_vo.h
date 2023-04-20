#ifndef _SAMPLE_UVC2VDECVO_H_
#define _SAMPLE_UVC2VDECVO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUvc2VdecVoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleUvc2VdecVoCmdLineParam;

typedef struct SampleUvc2VdecVoConfig
{
    char mDevName[20];
    unsigned int mPicFormat;
    int mCaptureVideoBufCnt;
    int mCaptureWidth;
    int mCaptureHeight;
    int mCaptureFrameRate;

    BOOL mbDecodeSubOutEnable;
    int mDecodeSubOutWidthScalePower;
    int mDecodeSubOutHeightScalePower;
    int mDisplayMainX;
    int mDisplayMainY;
    int mDisplayMainWidth;
    int mDisplayMainHeight;
    int mDisplaySubX;
    int mDisplaySubY;
    int mDisplaySubWidth;
    int mDisplaySubHeight;
    int mTestFrameCount;
}SampleUvc2VdecVoConfig;

typedef struct VdecDoubleFrameInfo
{            
    VIDEO_FRAME_INFO_S mMainFrame;  //0: main stream; 1 : sub stream. note: sub stream just for mjepg vdec now!
    int mMainRefCnt;
    VIDEO_FRAME_INFO_S mSubFrame;
    int mSubRefCnt;
}VdecDoubleFrameInfo;

#define MAX_FRAMEPAIR_ARRAY_SIZE (30)
typedef struct SampleUvc2VdecVoContext
{
    SampleUvc2VdecVoCmdLineParam mCmdLineParam;
    SampleUvc2VdecVoConfig  mConfigParam;
    cdx_sem_t mSemExit;
    UVC_CHN mUvcChn;
    MPP_SYS_CONF_S mSysconf;
    VDEC_CHN mVdecChn;
    VO_DEV mVoDev;
    int mUILayer;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;
    VO_LAYER mSubVoLayer;
    VO_VIDEO_LAYER_ATTR_S mSubLayerAttr;
    VO_CHN mSubVOChn;

    pthread_mutex_t mFrameLock;
    int mFrameCounter;
    int mHoldFrameNum;
    VdecDoubleFrameInfo mDoubleFrameArray[MAX_FRAMEPAIR_ARRAY_SIZE];
    int mValidDoubleFrameBufferNum;
}SampleUvc2VdecVoContext;


#endif

