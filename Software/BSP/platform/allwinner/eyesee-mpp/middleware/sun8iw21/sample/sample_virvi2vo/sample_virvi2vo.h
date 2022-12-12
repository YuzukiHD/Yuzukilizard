
#ifndef _SAMPLE_VIRVI2VO_H_
#define _SAMPLE_VIRVI2VO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_isp.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleVIRVI2VOCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleVIRVI2VOCmdLineParam;

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
}VIPP2VOConfig;
typedef struct VIPP2VOLinkInfo
{
    VI_DEV mVIDev;
    VI_CHN mVIChn;

    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;
}VIPP2VOLinkInfo;

#define VIPP2VO_NUM (2)
typedef struct SampleVIRVI2VOConfig
{
    VIPP2VOConfig mVIPP2VOConfigArray[VIPP2VO_NUM];
    BOOL mbAddUILayer;
    VO_LAYER mTestUILayer;
    int mUIDisplayWidth;
    int mUIDisplayHeight;
    
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mTestDuration;  //unit:s, 0 mean infinite
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;

    ISP_CFG_MODE mIspWdrSetting;
}SampleVIRVI2VOConfig;

//a framebuffer device structure;
typedef struct fbdev{
    int mFbFd;
    unsigned long mFbMemOffset;
    void *mFbMem;
    struct fb_fix_screeninfo mFbFix;
    struct fb_var_screeninfo mFbVar;
    char mDev[20];   //"/ dev/fb0"
} FBDEV, *PFBDEV;

typedef struct SampleVIRVI2VOContext
{
    SampleVIRVI2VOCmdLineParam mCmdLinePara;
    SampleVIRVI2VOConfig mConfigPara;

    cdx_sem_t mSemExit;
    VO_LAYER mUILayer;
    VO_VIDEO_LAYER_ATTR_S mUILayerAttr;
    FBDEV mFbDev;
    VO_LAYER mTestUILayer;
    VO_CHN mTestVOChn;
    VO_VIDEO_LAYER_ATTR_S mTestUILayerAttr;
    VIDEO_FRAME_INFO_S mTestUIFrame;

    MPP_SYS_CONF_S mSysConf;
    //ISP_DEV mIspDev;
    VO_DEV mVoDev;
    
#if 0
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;
#endif
    VIPP2VOLinkInfo mLinkInfoArray[VIPP2VO_NUM];
}SampleVIRVI2VOContext;
int initSampleVIRVI2VOContext();
int destroySampleVIRVI2VOContext();

#endif  /* _SAMPLE_VIRVI2VO_H_ */

