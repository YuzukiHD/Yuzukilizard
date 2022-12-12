#ifndef _SAMPLE_MOTIONDETECT_H_
#define _SAMPLE_MOTIONDETECT_H_

#include <plat_type.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct SampleMotionDetectCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleMotionDetectCmdLineParam;

typedef struct SampleMotionDetectConfig
{
    int mEncoderCount;
    int mDevNum;
    PIXEL_FORMAT_E mSrcPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mSrcWidth;
    int mSrcHeight;
    int mSrcFrameRate;
    PAYLOAD_TYPE_E mEncoderType;
    int mDstWidth;
    int mDstHeight;
    int mDstFrameRate;
    int mDstBitRate;
    int mOnlineEnable;
    int mOnlineShareBufNum;
    char mOutputFilePath[MAX_FILE_PATH_SIZE];
    int mIspAndVeLinkageEnable;
}SampleMotionDetectConfig;

typedef struct SampleMotionDetectContext
{
    SampleMotionDetectCmdLineParam mCmdLinePara;
    SampleMotionDetectConfig mConfigPara;

    ISP_DEV mIspDev;
    VI_DEV mViDev;
    VI_CHN mViChn;
    VENC_CHN mVeChn;

    VI_ATTR_S mViAttr;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVEncRcParam;
    VENC_FRAME_RATE_S mVencFrameRateConfig;

    FILE* mOutputFileFp;

    int mbExitFlag;
    BOOL bMotionSearchEnable;
    VencMotionSearchResult mMotionResult;
}SampleMotionDetectContext;

#endif  /* _SAMPLE_MOTIONDETECT_H_ */

