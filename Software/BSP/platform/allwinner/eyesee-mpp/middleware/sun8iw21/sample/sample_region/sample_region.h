#ifndef _SAMPLE_REGION_H_
#define _SAMPLE_REGION_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_region.h>
#include <mm_comm_venc.h>


#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleRegionCmdLineParam
{    
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleRegionCmdLineParam;

typedef struct SampleRegionConfig{    
    int mCaptureWidth;    
    int mCaptureHeight;    
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420    
    int mFrameRate;
    int mDispWidth;
    int mDispHeight;
    int mTestDuration;  //unit:s, 0 mean infinite

    PIXEL_FORMAT_E mBitmapFormat;
    int overlay_x;
    int overlay_y;
    int overlay_w;
    int overlay_h;

    int cover_x;
    int cover_y;
    int cover_w;
    int cover_h;

    int orl_x;
    int orl_y;
    int orl_w;
    int orl_h;
    int orl_thick;

    int mOnlineEnable;
    int mOnlineShareBufNum;

    int mVippDevID;
    int mVeChnID;

    bool mbAttachToVi;
    bool mbAttachToVe;
    bool add_venc_channel;
    bool mChangeDisplayAttrEnable;
    bool mVencRoiEnable;

    int encoder_count;
    int bit_rate;
    PAYLOAD_TYPE_E EncoderType;
    char OutputFilePath[MAX_FILE_PATH_SIZE];
}SampleRegionConfig;

typedef struct SampleRegionContext{    
    SampleRegionCmdLineParam mCmdLinePara;    
    SampleRegionConfig mConfigPara;
    cdx_sem_t mSemExit;    
    int mUILayer;    
    MPP_SYS_CONF_S mSysConf;    
    VO_DEV mVoDev;    
    VO_LAYER mVoLayer;    
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;    
    VO_CHN mVOChn;    
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

    RGN_HANDLE mOverlayHandle;
    RGN_HANDLE mCoverHandle;
    RGN_HANDLE mOrlHandle;

    FILE* mOutputFileFp;
}SampleRegionContext;

//int initSampleVI2VOContext();
//int destroySampleVI2VOContext();





#endif
