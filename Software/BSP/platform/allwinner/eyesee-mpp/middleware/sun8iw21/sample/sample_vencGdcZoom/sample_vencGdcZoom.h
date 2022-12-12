#ifndef __SAMPLE_VENC_GDCZOOM_H__
#define __SAMPLE_VENC_GDCZOOM_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cdx_list.h>

#include <pthread.h>

#include "mm_comm_sys.h"
#include "mpi_sys.h"

#include "mm_comm_vi.h"
#include "mpi_vi.h"
#include <mpi_isp.h>

#include "vencoder.h"
#include "mpi_venc.h"
#include "mm_comm_video.h"

#include "mm_comm_mux.h"
#include "mpi_mux.h"

#include "tmessage.h"
#include "tsemaphore.h"

#include <memoryAdapter.h>
#include "sc_interface.h"

#include <confparser.h>


#define MAX_FILE_PATH_LEN  (128)

typedef enum H264_PROFILE_E
{
   H264_PROFILE_BASE = 0,
   H264_PROFILE_MAIN,
   H264_PROFILE_HIGH,
}H264_PROFILE_E;

typedef enum H265_PROFILE_E
{
   H265_PROFILE_MAIN = 0,
   H265_PROFILE_MAIN10,
   H265_PROFILE_STI11,
}H265_PROFILE_E;

typedef struct CmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}CMDLINEPARAM_S;

typedef struct VencGdcZoom_Config
{
    char dstVideoFile[MAX_FILE_PATH_LEN];

    int srcWidth;
    int srcHeight;
    int srcPixFmt;
    enum v4l2_colorspace mColorSpace;
    int dstWidth;
    int dstHeight;
    int mViDropFrameNum;   // for offline
    int mVencDropFrameNum; // for online
    int mViBufferNum;
    int mSaturationChange;

    int mVippDev;
    int mVeChn;

    int mVideoEncoderFmt;
    int mVideoFrameRate;
    int mVideoBitRate;
    int mTestDuration;

    int mVeFreq; // MHz

    int mProductMode;
    int mSensorType;
    int mKeyFrameInterval;
    int mRcMode;

    int mInitQp;
    int mMinIQp;
    int mMaxIQp;
    int mMinPQp;
    int mMaxPQp;
    int mEnMbQpLimit;

    int mMovingTh;
    int mQuality;
    int mPBitsCoef;
    int mIBitsCoef;

    int mGopMode;
    int mGopSize;
    int mAdvancedRef_Base;
    int mAdvancedRef_Enhance;
    int mAdvancedRef_RefBaseEn;
    int mEnableFastEnc;
    BOOL mbEnableSmart;
    int mSVCLayer;  //0, 2, 3, 4
    int mEncodeRotate;  //clockwise.
    int mEncUseProfile;

    BOOL mColor2Grey;
    s2DfilterParam m2DnrPara;
    s3DfilterParam m3DnrPara;
    int mRoiNum;
    int mRoiQp;
    BOOL mRoiBgFrameRateEnable;
    int mRoiBgFrameRateAttenuation;
    int mIntraRefreshBlockNum;
    int mOrlNum;
    BOOL mHorizonFlipFlag; // mirror

    int mVbvBufferSize;  //unit:Byte
    int mVbvThreshSize;  //unit:Byte

    /* crop params */
    BOOL mCropEnable;
    int mCropRectX;
    int mCropRectY;
    int mCropRectWidth;
    int mCropRectHeight;

    int mVuiTimingInfoPresentFlag;

    int mOnlineEnable;
    int mOnlineShareBufNum;

    int wdr_en;
    int mEnableGdc;
    int mEnableGdcZoom;
}VENC_GDCZOOM_CONFIG_S;

typedef struct SAMPLE_VENC_GDCZOOM_S
{
    VENC_GDCZOOM_CONFIG_S mConfigPara;
    CMDLINEPARAM_S mCmdLinePara;

    FILE* mOutputFileFp;

    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

    VI_ATTR_S mViAttr;
    ISP_DEV mIspDev;
    VI_DEV mViDev;
    VI_CHN mViChn;

    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;

    pthread_t mThreadId;
    int mExitFlag;
}SAMPLE_VENC_GDCZOOM_S;

#endif //#define __SAMPLE_VENC_GDCZOOM_H__
