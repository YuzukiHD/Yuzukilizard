#ifndef _SAMPLE_ONLINEVENC_H_
#define _SAMPLE_ONLINEVENC_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include "tmessage.h"

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
    VENC_CHN_SUB_LAPSE_STREAM = 2,
    VENC_CHN_TYPE_MAX
} VENC_CHN_TYPE_E;

typedef enum OnlineVencMsgType
{
    Vi_Timeout,
    MsgQueue_Stop,
}OnlineVencMsgType;

typedef struct SampleOnlineVencCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleOnlineVencCmdLineParam;

typedef struct SampleOnlineVencConfig
{
    // main stream
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
    char mMainFilePath[MAX_FILE_PATH_SIZE];
    int mMainOnlineEnable;
    int mMainOnlineShareBufNum;
    BOOL mMainEncppEnable;

    // sub stream
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
    char mSubFilePath[MAX_FILE_PATH_SIZE];
    int mSubEncppSharpAttenCoefPer;
    BOOL mSubVippCropEnable;
    int mSubVippCropRectX;
    int mSubVippCropRectY;
    int mSubVippCropRectWidth;
    int mSubVippCropRectHeight;
    BOOL mSubEncppEnable;

    // sub lapse stream
    VI_DEV mSubLapseVipp;
    VI_CHN mSubLapseViChn;
    int mSubLapseSrcWidth;
    int mSubLapseSrcHeight;
    PIXEL_FORMAT_E mSubLapsePixelFormat;
    int mSubLapseWdrEnable;
    int mSubLapseViBufNum;
    int mSubLapseSrcFrameRate;
    VENC_CHN mSubLapseVEncChn;
    PAYLOAD_TYPE_E mSubLapseEncodeType;
    int mSubLapseEncodeWidth;
    int mSubLapseEncodeHeight;
    int mSubLapseEncodeFrameRate;
    int mSubLapseEncodeBitrate;
    char mSubLapseFilePath[MAX_FILE_PATH_SIZE];
    int mSubLapseEncppSharpAttenCoefPer;
    int mSubLapseTime;
    BOOL mSubLapseEncppEnable;

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
}SampleOnlineVencConfig;

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
    BOOL mbSubLapseEnable;
    int mStreamDataCnt;
    FILE* mFile;
    void *priv;
    int mEncppSharpAttenCoefPer;
}VencStreamContext;

typedef struct VencOsdContext
{
    pthread_t mStreamThreadId;
    RGN_HANDLE mOverlayHandle[VENC_MAX_CHN_NUM];
    PIXEL_FORMAT_E mPixelFormat;
    void *priv;
}VencOsdContext;

typedef struct SampleOnlineVencContext
{
    SampleOnlineVencCmdLineParam mCmdLinePara;
    SampleOnlineVencConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mbExitFlag;

    BOOL mMainIspRunFlag;
    BOOL mSubIspRunFlag;

    VencStreamContext mMainStream;
    VencStreamContext mSubStream;
    VencStreamContext mSubLapseStream;

    pthread_t mWbYuvThreadId;

    pthread_t mMsgQueueThreadId;
    message_queue_t mMsgQueue;
}SampleOnlineVencContext;

typedef struct OnlineVenc_MessageData
{
    SampleOnlineVencContext *mpOnlineVencData;
}OnlineVenc_MessageData;

#endif  /* _SAMPLE_ONLINEVENC_H_ */

