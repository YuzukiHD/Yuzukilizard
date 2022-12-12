#ifndef _SAMPLE_ISPOSD_H_
#define _SAMPLE_ISPOSD_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_region.h>
#include <mm_comm_venc.h>
#include "rgb_ctrl.h"


#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleIspOsdCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleIspOsdCmdLineParam;

typedef struct SampleIspOsdConfig{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mTestDuration;  //unit:s, 0 mean infinite

    PIXEL_FORMAT_E mBitmapFormat;
    int overlay_x;
    int overlay_y;
    int overlay_w;
    int overlay_h;

    bool mbAttachToVi;
    //venc
    bool add_venc_channel;
    int encoder_count;
    int bit_rate;
    PAYLOAD_TYPE_E EncoderType;
    char OutputFilePath[MAX_FILE_PATH_SIZE];
}SampleIspOsdConfig;

typedef struct SampleIspOsdContext{
    SampleIspOsdCmdLineParam mCmdLinePara;
    SampleIspOsdConfig mConfigPara;
    cdx_sem_t mSemExit;
    int mUILayer;
    MPP_SYS_CONF_S mSysConf;
    CLOCK_CHN mClockChn;
    CLOCK_CHN_ATTR_S mClockChnAttr;
    VI_DEV mVIDev;
    VI_CHN mVIChn;
    VI_CHN mVI2VEncChn;

    //VENC_
    VENC_CHN mVEChn;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    volatile BOOL mbEncThreadExitFlag;
    pthread_t mEncThreadId;
    pthread_t ispDebugThreadId;
    pthread_t ispDebugRgbThreadId;

    RGN_HANDLE mOverlayHandle;
    RGN_HANDLE mCoverHandle;
    RGB_PIC_S  ispDebugRgb;

    FILE* mOutputFileFp;
}SampleIspOsdContext;



#endif
