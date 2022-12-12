#ifndef _SAMPLE_UVC_VO_H_
#define _SAMPLE_UVC_VO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUvcVoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleUvcVoCmdLineParam;

typedef struct SampleUvcVoConfig
{
    char mDevName[20];
    unsigned int mPicFormat;    //UVC_MJPEG
    int mCaptureVideoBufCnt;
    int mCaptureWidth;
    int mCaptureHeight;
    int mCaptureRate;

    int mDisplayWidth;
    int mDisplayHeight;
    int mTestFrameCount;
    int mBrightness;
    int mContrast;
    int mHue;
    int mSaturation;
    int mSharpness;
    
}SampleUvcVoConfig;

typedef struct SampleUvcVoContext
{
    SampleUvcVoCmdLineParam mCmdLineParam;
    SampleUvcVoConfig  mConfigParam;
    cdx_sem_t mSemExit;
    UVC_CHN mUvcChn;
    MPP_SYS_CONF_S mSysconf;
    
    VO_DEV mVoDev;
    int mUILayer;
    VO_LAYER mVoLayer;    
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;    
    VO_CHN mVOChn;    

    pthread_mutex_t mFrameCountLock;
    int mFrameCount;
    int mHoldFrameNum;  //the frame number which is hold by app.
}SampleUvcVoContext;


#endif
