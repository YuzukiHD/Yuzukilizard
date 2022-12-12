/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VideoEnc_Component.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/09
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

#ifndef __IPCLINUX_VIDEOENC_COMPONENT_H__
#define __IPCLINUX_VIDEOENC_COMPONENT_H__

//ref platform headers
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"

//media api headers to app
#include "mm_common.h"
#include "mm_comm_video.h"
#include "mm_comm_venc.h"
#include "mpi_venc.h"

//media internal common headers.
#include "media_common.h"
#include "mm_component.h"
#include "ComponentCommon.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include "vencoder.h"


//------------------------------------------------------------------------------------

//#define VIDEO_ENC_TIME_DEBUG

#define MAX_VENCODER_PORTS (2)

#define ENC_FIFO_LEVEL  (32)

#define COMPRESSED_SRC_ENC_BUF_LEN      (10*1024*1024)
#define FRAME_BEGIN_FLAG        0x55AA55AA

#define BITSTREAM_FRAME_SIZE    (256)   // ref to BITSTREAM_FRAME_FIFO_SIZE

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))


typedef enum COMP_RECORD_TYPE
{
    COMP_RECORD_TYPE_NORMAL = 0,
    COMP_RECORD_TYPE_TIMELAPSE, //selected encoded frame interval is long(0.1fps), but play as camera capture frame rate. camera capture in normal frame rate, but select only few to encode.
    COMP_RECORD_TYPE_SLOW,  //encode every input frame, but reset its pts to play as another frame rate. e.g.,frame interval is small(120fps), but play as 30fps.
} COMP_RECORD_TYPE;

typedef struct FRAMEINFOTYPE {
    int64_t     timeStamp;  //unit:us
    int         bufferId;
    int         size;
} FRAMEINFOTYPE;

typedef struct FRAMEDATATYPE {
    FRAMEINFOTYPE   info;
    char*           addrY;
} FRAMEDATATYPE;

typedef struct VIDEOENCBUFFERMANAGER {
    unsigned char           *buffer;
    unsigned int            writePos;
    unsigned int            readPos;
    unsigned int         prefetchPos;
    int         count;
    int         mUnprefetchFrameNum;
    pthread_mutex_t lock;
} VIDEOENCBUFFERMANAGER;



typedef struct bufMrgQ_t
{
    struct list_head mIdleFrameList;    //VideoFrameInfoNode
    struct list_head mReadyFrameList;
    struct list_head mUsingFrameList;
    struct list_head mUsedFrameList;
    int                 buf_unused; //ENC_FIFO_LEVEL
}bufMrgQ_t;

typedef struct SoftFrameRateCtrl
{
    BOOL enable;
    //method2: by pts.
    int64_t mBasePts;   //unit:us
    int64_t mCurrentWantedPts; //unit:us
    int64_t mFrameCounter;
} SoftFrameRateCtrl;

typedef struct CaptureTimeLapse
{
    COMP_RECORD_TYPE recType;
    double capFrameIntervalUs;     //two frame's pts interval which we select to encode.
    double videoFrameIntervalUs;   //the frame reconfig_pts interval after encode, 
    double lastCapTimeUs;          //base on last encoded frame's pts, == n*capFrameIntervalUs + first_pts.
    double lastTimeStamp;          //we set last encoded frame's reconfig_pts. unit:us
} CaptureTimeLapse;

typedef struct VEncRoiCfgNode
{
    VENC_ROI_CFG_S mROI;
    struct list_head mList;
}VEncRoiCfgNode;

typedef struct VIDEOENCDATATYPE {
    COMP_STATETYPE state;
    pthread_mutex_t mStateLock;
    COMP_CALLBACKTYPE *pCallbacks;
    void* pAppData;
    COMP_HANDLETYPE hSelf;
    
    COMP_PORT_PARAM_TYPE sPortParam;
    COMP_PARAM_PORTDEFINITIONTYPE sInPortDef;
    COMP_PARAM_PORTDEFINITIONTYPE sOutPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE sInPortTunnelInfo;
    COMP_INTERNAL_TUNNELINFOTYPE sOutPortTunnelInfo;
    
    COMP_PARAM_BUFFERSUPPLIERTYPE sPortBufSupplier[MAX_VENCODER_PORTS];
    BOOL mInputPortTunnelFlag;
    BOOL mOutputPortTunnelFlag;   //TRUE: tunnel mode; FALSE: non-tunnel mode.
    
    MPP_CHN_S mMppChnInfo;
    pthread_t thread_id;
    char mThreadName[32];
    CompInternalMsgType eTCmd;
    message_queue_t  cmd_queue;
    volatile int mFlagInputFrame;   //0: no input frame to be encoded.
    volatile int mFlagOutputStream; //0: no output stream from the encoding library.
    VideoEncoder *pCedarV;
    struct ScMemOpsS *mMemOps;
    VeOpsS *mVeOpsS;
    void *mpVeOpsSelf;
    int mVEFreq;    //unit:MHz, 0:use default value. If ve is run, keep current freq.
    int mOutCacheTime;  //unit:ms, next components will cache encoded data, so venc can decrease vbvSize.
    //VIDEOINFO_t     mSourceVideoInfo;
    VENC_CHN_ATTR_S mEncChnAttr;
    VENC_PARAM_INTRA_REFRESH_S mEncIntraRefreshParam;
    VENC_COLOR2GREY_S   mColor2GreyParam;
    s2DfilterParam m2DfilterParam;
    s3DfilterParam m3DfilterParam;
    VENC_SUPERFRAME_CFG_S mSuperFrmParam;
    BOOL    mHorizonFlipFlag;//
    BOOL    mAdaptiveIntraInPFlag;//

    pthread_mutex_t mCedarvVideoEncInitFlagLock;
    BOOL mbCedarvVideoEncInitFlag;//the falg of init  video encoder.
    //pthread_mutex_t mVencOverlayLock;
    //VencGopParam mVencGopParam;
    VencAdvancedRefParam  mRefParam;
    VencSmartFun mEncSmartPParam;
    VencBrightnessS mEncBrightness;
    pthread_mutex_t mFrameRateLock;
    VENC_FRAME_RATE_S mFrameRateInfo;
	VENC_FRAME_RATE_S mPreFrameRateInfo;
    VENC_RC_PARAM_S mRcParam;
    VENC_CROP_CFG_S mCropCfg;
    VeProcSet mProcSet;
    VencSaveBSFile mSaveBSFile;
    BOOL mbDropOverflowFrameFlag;   //if drop overflow frame directly when encoder encoding.

    VencHeaderData *mpVencHeaderData;

    int mOutputFrameNotifyPipeFds[2];

    pthread_mutex_t mRecvPicControlLock;
    VENC_RECV_PIC_PARAM_S mRecvPicParam;
    BOOL mLimitedFramesFlag;    //flag if encode limited frames
    int mCurRecvPicNum;

    
    struct list_head mRoiCfgList;   //VEncRoiCfgNode
    pthread_mutex_t mRoiLock;
    VENC_ROIBG_FRAME_RATE_S mRoiBgFrameRate;

    //struct list_head mOverlayList; //VENC_OVERLAY_INFO

    VENC_PARAM_H264_VUI_S mH264VuiParam;
    VENC_PARAM_H265_VUI_S mH265VuiParam;

    VENC_PARAM_JPEG_S mJpegParam;   //for jpeg encode.
    VENC_EXIFINFO_S mJpegExifInfo;

    BOOL csi_first_frame;
    int64_t  csi_base_time;
    int64_t mPrevInputPts;   //unit:us
    //int buf_id; //used for releasing camera frame
    pthread_mutex_t mutex_fifo_ops_lock;
    bufMrgQ_t   mBufQ; //used for managing camera frame, for uncompressed video frame.
    BOOL            mWaitInputFrameUsingEmptyFlag;  //when comp need all input orig using frames list to be return back, set this flag.
    pthread_cond_t  mInputFrameUsingEmptyCondition;

    int     is_compress_source;  /* gushiming compressed source */
    BOOL mbSendSpspps;  //when in compress mode and outputPort tunnel mode, venc component need send spspps to next component.
    //for debug
    int     mMaxCompressJpegFrameSize;

    VIDEOENCBUFFERMANAGER *buffer_manager;  //used for managing usb camera frame, for compressed video frame such as mjpeg.

    BOOL mbForbidDiscardingFrame;
    struct list_head    mIdleOutFrameList;  //ENCODER_NODE_T
    struct list_head    mReadyOutFrameList; //ENCODER_NODE_T, for non-tunnel, when mOutputPortTunnelFlag == FALSE, use it to store encoded frames. for tunnel-mode, store output frame.
    struct list_head    mUsedOutFrameList; //ENCODER_NODE_T, for non-tunnel mode. when mOutputPortTunnelFlag == FALSE, use it to store user occupied frames. for tunnel-mode, use it store output frame.
    int                 mFrameNodeNum;
    BOOL                mWaitOutFrameFullFlag;  //when comp need all out frames to be return back, set this flag.
    BOOL                mWaitOutFrameFlag;  //for non-tunnel mode, wait outFrame ready, then get it!
    BOOL                mWaitOutFrameReturnFlag;    //when encoder need idleOutFrame to be return to continue encoding, we will set this flag.
    pthread_mutex_t         mOutFrameListMutex;
    pthread_cond_t          mOutFrameFullCondition;
    pthread_cond_t          mOutFrameCondition; //for non-tunnel mode, wait outFrame coming!

    cdx_sem_t mInputFrameBufDoneSem; // for ForbidDiscardingFrame=TRUE flow, used to let the inputBufDone callback notify the venc component thread to fetch the encoding result.

    int mFlagDoNotSendOrigFrameToVencLib; // only used by ComponentThread()

    double mVbvBufTime;
    SoftFrameRateCtrl mSoftFrameRateCtrl;
    BOOL timeLapseEnable;
    pthread_mutex_t mCapTimeLapseLock;
    CaptureTimeLapse mCapTimeLapse;
    VencBaseConfig mBaseConfig;

    //statistics
    int bitStreamBufFullCnt;    //continuous bit stream full count.
    int mDbgEncodeCnt;
    int mEncodeTimeoutCnt;
    int mDiscardFrameCnt;
    int mEncodeSuccessCount;
    int64_t mTotalEncodeSuccessDuration;
    unsigned int mStatMaxFrameSize;
    int64_t mStatStreamSize;    //stat encode stream total size
    //statistics for bitRate
    int mFrameNumInKeyFrameInvertal;    //calculate frame number in keyFrameInterval, clear when meet second key frame.
    int mBytesKeyFrameInterval; //calculate frame bytes in keyFrameInterval, clear when meet second key frame.

    VencHighPassFilter mVencHighPassFilter;
    int DayOrNight;     // 0:day;1:night

    int venc_cnt_id;
    int venc_first_frm;
    //for debug
    int mbDebug_IgnoreAllFrame;

    BOOL mbEnableMotionSearch;
    VencMotionSearchResult mMotionResult;
    //QPMAP
    VencMBModeCtrl  mMBModeCtrl;
    VencMBInfo      mMBInfo;
    VencMBSumInfo   mMbSumInfo;

    //online
    int mOnlineEnableFlag;

    //int mEncppEnable;
    //unsigned int mEncppSharpAttenCoefPer;

    BOOL mbEnableWbYUV;

    BOOL bForceKeyFrameFlag;
} VIDEOENCDATATYPE;

//private interface
ERRORTYPE VideoEncGetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef);
ERRORTYPE VideoEncSetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef);
ERRORTYPE VideoEncGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier);
ERRORTYPE VideoEncSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier);
ERRORTYPE VideoEncGetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT MPP_CHN_S *pChn);
ERRORTYPE VideoEncSetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn);
ERRORTYPE VideoEncGetChannelFd(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT int *pChnFd);
ERRORTYPE VideoEncGetTunnelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo);
ERRORTYPE VideoEncGetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_CHN_ATTR_S *pChnAttr);
ERRORTYPE VideoEncSetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_CHN_ATTR_S *pChnAttr);
ERRORTYPE VideoEncGetVencChnState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_CHN_STAT_S *pChnStat);
ERRORTYPE VideoEncGetStream(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_STREAM_S *pStream,
        PARAM_IN int nMilliSec);
ERRORTYPE VideoEncReleaseStream(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_STREAM_S *pStream);
ERRORTYPE VideoEncGetStreamDuration(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT double *pTimeDuration);
ERRORTYPE VideoEncSetStreamDuration(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN double nTimeDuration);
ERRORTYPE VideoEncGetRoiCfg(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_INOUT VENC_ROI_CFG_S *pROI);
ERRORTYPE VideoEncSetRoiCfg(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_ROI_CFG_S *pRoiCfg);
ERRORTYPE VideoEncGetJpegParam(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT VENC_PARAM_JPEG_S *pJpegParam);
ERRORTYPE VideoEncSetJpegParam(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN VENC_PARAM_JPEG_S *pJpegParam);
ERRORTYPE VideoEncGetJpegExifInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_EXIFINFO_S *pJpegExifInfo);
ERRORTYPE VideoEncSetJpegExifInfo(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN VENC_EXIFINFO_S *pJpegExifInfo);
ERRORTYPE VideoEncGetJpegThumbBuffer(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT VENC_JPEG_THUMB_BUFFER_S *pJpegThumbBuffer);
ERRORTYPE VideoEncGetHighPassFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VencHighPassFilter *pHightPassFilter);
ERRORTYPE VideoEncSetHighPassFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencHighPassFilter *pHightPassFilter);
ERRORTYPE VideoEncGetDayOrNight(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT int *DayOrNight);
ERRORTYPE VideoEncSetDayOrNight(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int *DayOrNight);
ERRORTYPE VideoEncGetFrameRate(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT VENC_FRAME_RATE_S *pFrameRate);
ERRORTYPE VideoEncSetFrameRate(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_FRAME_RATE_S *pFrameRate);
ERRORTYPE VideoEncGetTimeLapse(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT int64_t* pTimeLapse);
ERRORTYPE VideoEncSetTimeLapse(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT int64_t* pTimeLapse);
ERRORTYPE VideoEncGetCropCfg(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_CROP_CFG_S *pCropCfg);
ERRORTYPE VideoEncSetCropCfg(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_CROP_CFG_S *pCropCfg);
ERRORTYPE VideoEncGetStreamBufInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_STREAM_BUF_INFO_S *pStreamBufInfo);
ERRORTYPE VideoEncExtraData(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT VencHeaderData *pVencHeaderData);
ERRORTYPE VideoEncResetChannel(PARAM_IN COMP_HANDLETYPE hComponent, BOOL bForceReleaseOutFrameInNonTunnelMode);
ERRORTYPE VideoEncSetRecvPicParam(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN VENC_RECV_PIC_PARAM_S *pRecvPicParam);
ERRORTYPE VideoEncRequestIDR(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN BOOL bInstant);
//public interface
ERRORTYPE VideoEncSendCommand(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd, 
        PARAM_IN unsigned int nParam1,
        PARAM_IN void* pCmdData);
ERRORTYPE VideoEncGetState(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT COMP_STATETYPE* pState);
ERRORTYPE VideoEncSetCallbacks(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_CALLBACKTYPE* pCallbacks, 
        PARAM_IN void* pAppData) ;
ERRORTYPE VideoEncGetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure);
ERRORTYPE VideoEncSetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure);
ERRORTYPE VideoEncComponentTunnelRequest(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  unsigned int nPort,
        PARAM_IN  COMP_HANDLETYPE hTunneledComp,
        PARAM_IN  unsigned int nTunneledPort,
        PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup);
ERRORTYPE VideoEncEmptyThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer);
ERRORTYPE VideoEncFillThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer);
ERRORTYPE VideoEncComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent);
ERRORTYPE VideoEncComponentInit(PARAM_IN COMP_HANDLETYPE hComponent);
#endif  /* __IPCLINUX_VIDEOENC_COMPONENT_H__ */

