
#ifndef _SAMPLE_VIRVI_H_
#define _SAMPLE_VIRVI_H_

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleViResetCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleViResetCmdLineParam;

typedef struct SampleViResetConfig
{
    int mTestCount;
    int mFrameCountStep1;   //capture this number of frames, then reset vi.
    bool mbRunIsp;
    ISP_DEV mIspDev;
    VI_DEV mVippStart;    //vipp0~3
    VI_DEV mVippEnd;    //vipp0~3
    int mPicWidth;
    int mPicHeight;
    int mSubPicWidth;
    int mSubPicHeight;
    int mFrameRate;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
}SampleViResetConfig;

typedef struct VirViChnInfo 
{
    pthread_t mThid;
    VI_DEV mVipp;
    VI_CHN mVirChn;
    AW_S32 mMilliSec;
    VIDEO_FRAME_INFO_S mFrameInfo;
    int mCaptureFrameCount;    //capture frame number.
} VirViChnInfo;

typedef struct SampleViResetContext
{
    SampleViResetCmdLineParam mCmdLinePara;
    SampleViResetConfig mConfigPara;

    VI_ATTR_S mViAttr;
    VI_CHN mVirChn;
    VirViChnInfo mVirViChnArray[MAX_VIPP_DEV_NUM][MAX_VIR_CHN_NUM];
    int mCaptureNum;
    bool mFirstVippRunFlag; //if vipp lined with isp is running, param is set to front.
    bool mbIspRunningFlag;
    int mTestNum;
    //VIDEO_FRAME_INFO_S mFrameInfo;
}SampleViResetContext;

#endif  /* _SAMPLE_VIRVI_H_ */

