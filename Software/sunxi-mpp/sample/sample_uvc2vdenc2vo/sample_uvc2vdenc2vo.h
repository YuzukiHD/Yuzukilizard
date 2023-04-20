#ifndef _SAMPLE_UVC2VDENC2VO_H_
#define _SAMPLE_UVC2VDENC2VO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUvc2Vdenc2VoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleUvc2Vdenc2VoCmdLineParam;

typedef struct SampleUvc2Vdenc2VoConfig
{
    char mDevName[20];
    unsigned int mPicFormat;
    int mCaptureVideoBufCnt;
    int mCaptureWidth;
    int mCaptureHeight;
    int mCaptureFrameRate;

    int mDecodeOutWidth;
    int mDecodeOutHeight;
    int mDisplayWidth;
    int mDisplayHeight;
    int mTestDuration;
}SampleUvc2Vdenc2VoConfig;

typedef struct SampleUvc2Vdenc2VoContext
{
    SampleUvc2Vdenc2VoCmdLineParam mCmdLineParam;
    SampleUvc2Vdenc2VoConfig  mConfigParam;
    cdx_sem_t mSemExit;
    UVC_CHN mUvcChn;
    MPP_SYS_CONF_S mSysconf;
    VDEC_CHN mVdecChn;
    VO_DEV mVoDev;
    int mUILayer;
    VO_LAYER mVoLayer;    
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;    
    VO_CHN mVOChn;    
}SampleUvc2Vdenc2VoContext;


#endif

