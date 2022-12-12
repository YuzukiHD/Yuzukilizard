#ifndef _SAMPLE_SMART_IPC_DEMO_H_
#define _SAMPLE_SMART_IPC_DEMO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_region.h>
#include "rgb_ctrl.h"

#define MAX_FILE_PATH_SIZE  (256)

#define INVALID_OVERLAY_HANDLE  (-1)

typedef enum ISP_DEV_TYPE_E
{
    ISP_DEV_MAIN = 0,
    ISP_DEV_SUB  = 1
} ISP_DEV_TYPE_E;

typedef enum VI_DEV_TYPE_E
{
    VI_DEV_MAIN = 0,
    VI_DEV_SUB  = 1,
    VI_DEV_JPEG = 2,
    VI_DEV_TYPE_MAX
} VI_DEV_TYPE_E;

typedef enum VENC_CHN_TYPE_E
{
    VENC_CHN_MAIN_STREAM      = 0,
    VENC_CHN_SUB_STREAM       = 1,
    VENC_CHN_TYPE_MAX
} VENC_CHN_TYPE_E;

typedef struct SampleSmartIPCDemoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleSmartIPCDemoCmdLineParam;

typedef struct SampleSmartIPCDemoConfig
{
    // rtsp
    int mRtspNetType;
    int mRtspBufNum;
    // common params
    int mProductMode;
    // main stream
    int mMainEnable;
    int mMainRtspID;
    ISP_DEV mMainIsp;
    VI_DEV mMainVipp;
    VI_CHN mMainViChn;
    int mMainSrcWidth;
    int mMainSrcHeight;
    PIXEL_FORMAT_E mMainPixelFormat;
    int mMainWdrEnable;
    int mMainViBufNum;
    int mMainSrcFrameRate;
    VENC_CHN mMainVEncChn;
    PAYLOAD_TYPE_E mMainEncodeType;
    int mMainEncodeWidth;
    int mMainEncodeHeight;
    int mMainEncodeFrameRate;
    int mMainEncodeBitrate;
    int mMainOnlineEnable;
    int mMainOnlineShareBufNum;
    BOOL mMainEncppEnable;
    char mMainFilePath[MAX_FILE_PATH_SIZE];
    int mMainSaveOneFileDuration;
    int mMainSaveMaxFileCnt;
    char mMainDrawOSDText[MAX_FILE_PATH_SIZE];
    int mMainNnNbgType;
    ISP_DEV mMainNnIsp;
    VI_DEV mMainNnVipp;
    int mMainNnViBufNum;
    int mMainNnSrcFrameRate;
    char mMainNnNbgFilePath[MAX_FILE_PATH_SIZE];
    int mMainNnDrawOrlEnable;
    // sub stream
    int mSubEnable;
    int mSubRtspID;
    ISP_DEV mSubIsp;
    VI_DEV mSubVipp;
    VI_CHN mSubViChn;
    int mSubSrcWidth;
    int mSubSrcHeight;
    PIXEL_FORMAT_E mSubPixelFormat;
    int mSubWdrEnable;
    int mSubViBufNum;
    int mSubSrcFrameRate;
    VENC_CHN mSubVEncChn;
    PAYLOAD_TYPE_E mSubEncodeType;
    int mSubEncodeWidth;
    int mSubEncodeHeight;
    int mSubEncodeFrameRate;
    int mSubEncodeBitrate;
    int mSubEncppSharpAttenCoefPer;
    BOOL mSubEncppEnable;
    char mSubFilePath[MAX_FILE_PATH_SIZE];
    int mSubSaveOneFileDuration;
    int mSubSaveMaxFileCnt;
    char mSubDrawOSDText[MAX_FILE_PATH_SIZE];
    int mSubNnNbgType;
    ISP_DEV mSubNnIsp;
    VI_DEV mSubNnVipp;
    int mSubNnViBufNum;
    int mSubNnSrcFrameRate;
    char mSubNnNbgFilePath[MAX_FILE_PATH_SIZE];
    int mSubNnDrawOrlEnable;
    BOOL mSubVippCropEnable;
    int mSubVippCropRectX;
    int mSubVippCropRectY;
    int mSubVippCropRectWidth;
    int mSubVippCropRectHeight;
    // isp and ve linkage
    BOOL mIspAndVeLinkageEnable;
    VENC_CHN_TYPE_E mIspAndVeLinkageStreamChn;
    // wb yuv
    int mWbYuvEnable;
    int mWbYuvBufNum;
    int mWbYuvStartIndex;
    int mWbYuvTotalCnt;
    int mWbYuvStreamChn;
    char mWbYuvFilePath[MAX_FILE_PATH_SIZE];
    // others
    int mTestDuration;  //unit:s, 0 means infinite.
}SampleSmartIPCDemoConfig;

typedef struct VencStreamContext
{
    pthread_t mStreamThreadId;
    ISP_DEV mIsp;
    VI_DEV mVipp;
    VI_CHN mViChn;
    VI_ATTR_S mViAttr;
    VENC_CHN mVEncChn;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVEncRcParam;
    VENC_FRAME_RATE_S mVEncFrameRateConfig;
    int mStreamDataCnt;
    FILE* mFile;
    void *priv;
    int mEncppSharpAttenCoefPer;
    int mRecordHandler;
}VencStreamContext;

typedef struct VencOsdContext
{
    pthread_t mStreamThreadId;
    RGN_HANDLE mOverlayHandle[VENC_MAX_CHN_NUM];
    PIXEL_FORMAT_E mPixelFormat;
    void *priv;
}VencOsdContext;

typedef struct SampleSmartIPCDemoContext
{
    SampleSmartIPCDemoCmdLineParam mCmdLinePara;
    SampleSmartIPCDemoConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mbExitFlag;

    VencStreamContext mMainStream;
    VencStreamContext mSubStream;

    RGN_HANDLE mOverlayDrawStreamOSDBase;
    RGB_PIC_S mRgbPic[16];

    pthread_t mWbYuvThreadId;
}SampleSmartIPCDemoContext;

#endif  /* _SAMPLE_SMART_IPC_DEMO_H_ */

