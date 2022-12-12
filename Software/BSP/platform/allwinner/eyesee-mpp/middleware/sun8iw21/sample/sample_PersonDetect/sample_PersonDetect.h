#ifndef _SAMPLE_PERSONDETECT_H_
#define _SAMPLE_PERSONDETECT_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <awnn_det.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct SamplePersonDetectCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SamplePersonDetectCmdLineParam;

typedef struct SamplePersonDetectConfig
{
    VI_DEV mMainVipp;
    int mMainSrcWidth;
    int mMainSrcHeight;
    PIXEL_FORMAT_E mMainPixelFormat;    //MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420
    VI_DEV mSubVipp;
    int mSubSrcWidth;
    int mSubSrcHeight;
    PIXEL_FORMAT_E mSubPixelFormat;
    int mSrcFrameRate;
    
    int mOrlColor;
    int mbEnableMainVippOrl;    //permit draw orl on main vipp
    int mbEnableSubVippOrl;     //permit draw orl on sub vipp

    int mPreviewVipp;
    int mPreviewWidth;
    int mPreviewHeight;
    
    PAYLOAD_TYPE_E mMainEncodeType;
    int mMainEncodeWidth;
    int mMainEncodeHeight;
    int mMainEncodeFrameRate;
    int mMainEncodeBitrate;
    char mMainFilePath[MAX_FILE_PATH_SIZE];
    PAYLOAD_TYPE_E mSubEncodeType;
    int mSubEncodeWidth;
    int mSubEncodeHeight;
    int mSubEncodeFrameRate;
    int mSubEncodeBitrate;
    char mSubFilePath[MAX_FILE_PATH_SIZE];

    char mNnaNbgFilePath[MAX_FILE_PATH_SIZE];
    
    int mTestDuration;  //unit:s, 0 means infinite.
}SamplePersonDetectConfig;

typedef struct SamplePDStreamContext
{
    VI_DEV mVipp;
    VI_CHN mViChn;
    VENC_CHN mVEncChn;
    VI_ATTR_S mViAttr;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVEncRcParam;
    VENC_FRAME_RATE_S mVEncFrameRateConfig;
    FILE* mFile;
    pthread_t mStreamThreadId;

    void *mpSamplePersonDetectContext;  //SamplePersonDetectContext*
}SamplePDStreamContext;

#define MAX_ORL_HANDLE_NUM (20)
typedef struct SamplePersonDetectContext
{
    SamplePersonDetectCmdLineParam mCmdLinePara;
    SamplePersonDetectConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mbExitFlag;

    ISP_DEV mIspDev;
    int mbPreviewFlag;
    VI_CHN mPreviewViChn;
    VI_CHN mPersonDetectViChn;

    VO_DEV mVoDev;
    VO_LAYER mUILayer;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVoChn;

    SamplePDStreamContext mMainStream;
    SamplePDStreamContext mSubStream;

    pthread_t mPDThreadId;
    Awnn_Det_Handle mAiModelHandle;

    int mValidOrlHandleNum;
    RGN_HANDLE mOrlHandles[MAX_ORL_HANDLE_NUM];
}SamplePersonDetectContext;

#endif  /* _SAMPLE_PERSONDETECT_H_ */

