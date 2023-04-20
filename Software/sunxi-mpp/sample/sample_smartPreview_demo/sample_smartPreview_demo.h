#ifndef _SAMPLE_SMARTPREVIEW_DEMO_H_
#define _SAMPLE_SMARTPREVIEW_DEMO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_isp.h>

#define MAX_FILE_PATH_SIZE (256)

#define VIPP2VO_NUM (2)

typedef struct SampleSmartPreviewDemoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleSmartPreviewDemoCmdLineParam;

typedef struct VIPP2VOConfig
{
    int mIspDev;
    int mVippDev;
    int mCaptureWidth;
    int mCaptureHeight;
    int mDisplayX;
    int mDisplayY;
    int mDisplayWidth;
    int mDisplayHeight;
    int mLayerNum;

    int mNnNbgType;
    ISP_DEV mNnIsp;
    VI_DEV mNnVipp;
    int mNnViBufNum;
    int mNnSrcFrameRate;
    char mNnNbgFilePath[MAX_FILE_PATH_SIZE];
    int mNnDrawOrlEnable;
}VIPP2VOConfig;
typedef struct VIPP2VOLinkInfo
{
    VI_DEV mVIDev;
    VI_CHN mVIChn;

    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;
}VIPP2VOLinkInfo;

typedef struct SampleSmartPreviewDemoConfig
{
    VIPP2VOConfig mVIPP2VOConfigArray[VIPP2VO_NUM];
    
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mTestDuration;  //unit:s, 0 mean infinite
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;
}SampleSmartPreviewDemoConfig;

typedef struct SampleSmartPreviewDemoContext
{
    SampleSmartPreviewDemoCmdLineParam mCmdLinePara;
    SampleSmartPreviewDemoConfig mConfigPara;

    cdx_sem_t mSemExit;
    VO_LAYER mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;

    VIPP2VOLinkInfo mLinkInfoArray[VIPP2VO_NUM];
}SampleSmartPreviewDemoContext;

#endif  /* _SAMPLE_SMARTPREVIEW_DEMO_H_ */

