
#ifndef _SAMPLE_VIRVI2VO_ZOOM_H_
#define _SAMPLE_VIRVI2VO_ZOOM_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_vo.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleVIRVI2VOZoomCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleVIRVI2VOZoomCmdLineParam;

typedef struct SampleVIRVI2VOZoomConfig
{
    int mDevNum;
    int mCaptureWidth;
    int mCaptureHeight;
    int mDisplayWidth;
    int mDisplayHeight;
    PIXEL_FORMAT_E mPicFormat;
    int mFrameRate;
    int mTestDuration;  //unit:s, 0 mean infinite
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;

    int mZoomSpeed;
    int mZoomMaxCnt;
}SampleVIRVI2VOZoomConfig;

typedef struct SampleVIRVI2VOZoomContext
{
    SampleVIRVI2VOZoomCmdLineParam mCmdLinePara;
    SampleVIRVI2VOZoomConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;

    VI_DEV mVIDev;
    VI_CHN mVIChn;

    int mActualCaptureWidth;
    int mActualCaptureHeight;
    RECT_S mLastRect;

    int mOverFlag;
}SampleVIRVI2VOZoomContext;

#endif  /* _SAMPLE_VIRVI2VO_ZOOM_H_ */

