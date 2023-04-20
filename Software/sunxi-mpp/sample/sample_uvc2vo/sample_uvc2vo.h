#ifndef _SAMPLE_UVC2VO_H_
#define _SAMPLE_UVC2VO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUvc2VoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleUvc2VoCmdLineParam;

typedef struct SampleUvc2VoConfig
{
    char mDevName[20];
    unsigned int mPicFormat;    //V4L2_PIX_FMT_MJPEG
    int mCaptureVideoBufCnt;
    int mCaptureWidth;
    int mCaptureHeight;
    int mCaptureRate;

    int mDisplayWidth;
    int mDisplayHeight;
    int mTestDuration;

    int mBrightness;
    int mContrast;
    int mHue;
    int mSaturation;
    int mSharpness;
    
}SampleUvc2VoConfig;

typedef struct SampleUvc2VoContext
{
    SampleUvc2VoCmdLineParam mCmdLineParam;
    SampleUvc2VoConfig  mConfigParam;
    cdx_sem_t mSemExit;
    UVC_CHN mUvcChn;
    MPP_SYS_CONF_S mSysconf;
    
    VO_DEV mVoDev;
    int mUILayer;
    VO_LAYER mVoLayer;    
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;    
    VO_CHN mVOChn;    
}SampleUvc2VoContext;


#endif
