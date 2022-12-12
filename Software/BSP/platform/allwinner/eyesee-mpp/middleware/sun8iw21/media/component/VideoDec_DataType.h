/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VideoDec_Component.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/19
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :

******************************************************************************/
#ifndef _VIDEODEC_DATATYPE_H_
#define _VIDEODEC_DATATYPE_H_

//ref platform headers
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"

#include "VideoDec_InputThread.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

#define MAX_VDECODER_IN_PORTS 2
#define MAX_VDECODER_PORTS (MAX_VDECODER_IN_PORTS + 1)

typedef enum VIDEODECODER_PORT_SUFFIX_DEFINITION {
    VDEC_PORT_SUFFIX_DEMUX = 0,
    VDEC_PORT_SUFFIX_CLOCK = 1,
} VIDEODECODER_PORT_SUFFIX_DEFINITION;

typedef struct VIDEODECDATATYPE 
{
    COMP_STATETYPE state;
    pthread_mutex_t mStateLock;
        
    COMP_CALLBACKTYPE *pCallbacks;
    void *pAppData;
    COMP_HANDLETYPE hSelf;
    COMP_PORT_PARAM_TYPE sPortParam;
    COMP_PARAM_PORTDEFINITIONTYPE sInPortDef[MAX_VDECODER_IN_PORTS];
    COMP_PARAM_PORTEXTRADEFINITIONTYPE sInPortExtraDef[MAX_VDECODER_IN_PORTS];
    COMP_PARAM_PORTDEFINITIONTYPE sOutPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE sInPortTunnelInfo[MAX_VDECODER_IN_PORTS];
    COMP_INTERNAL_TUNNELINFOTYPE sOutPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE sPortBufSupplier[MAX_VDECODER_PORTS];
    BOOL mInputPortTunnelFlag;
    BOOL mOutputPortTunnelFlag;  //TRUE: tunnel mode; FALSE: non-tunnel mode.
    MPP_CHN_S mMppChnInfo;
    VDEC_CHN_ATTR_S mChnAttr;
    VDEC_CHN_PARAM_S mChnParam;
    VDEC_PRTCL_PARAM_S mPrtclParam;

    pthread_t thread_id;
    CompInternalMsgType eTCmd;
    message_queue_t cmd_queue;
    //int                        demux_type;  //
    int priv_flag;
    BOOL mbEof;

    //int                        soft_chip_version;
    unsigned int max_resolution;  //chip max decode resolution capability. eg, 1288<<16 | 1288
    int cedarv_max_width;  //user set vdeclib max output width and height.
    int cedarv_max_height;
    int cedarv_output_setting;  //PIXEL_FORMAT_YUV_MB32_420, CEDARX_OUTPUT_SETTING_MODE_PLANNER, vdeclib output frame format.
    int cedarv_rotation;     //vdeclib should static rotate, clock wise. 0: no rotate, 1: 90 degree (clock wise), 2: 180, 3: 270, 4: horizon flip, 5: vertical flip; implicitly means picture rotate anti-clockwise.
    int cedarv_second_output_en;
    int cedarv_second_horizon_scale_down_ratio;
    int cedarv_second_vertical_scale_down_ratio;
    int cedarv_second_output_setting;
    int mVdecExtraFrameNum; //let vdeclib increase output frame number.

    BOOL mbUseCompFrame;    //true: use component_malloc frame to transport, false:use vdeclib buffer to transport
    int mG2DHandle;

    pthread_mutex_t mVbsInputMutex;
    volatile BOOL mWaitVbsInputFlag;
    BOOL mWaitEmptyVbsFlag;  //wait empty vbs buffer flag.
    int mRequestVbsLen;      //record need how many vbsLen.
    pthread_cond_t mEmptyVbsCondition;
    int mVideoInfoVersion;                //stream video info change, e.g., resolution.
    struct list_head mChangedStreamList;  //VDStreamInfo
    struct ScMemOpsS *mMemOps;
    VideoDecoder *pCedarV;
    BOOL mbForceFramePackage; //TRUE: consider every package contains whole frames.
    int mVEFreq;    //unit:MHz, 0:use default value. If ve is run, keep current freq.
    BOOL mbConfigVdecFrameBuffers;  //used in VideoRender_GUI mode, indicate if outer config vdecFrame to vdeclib.
    VConfig mVConfig;
    int config_vbv_size;
    
    struct list_head mIdleOutFrameList;  //VDecCompOutputFrame, VDEC_FRAME_COUNT
    struct list_head mReadyOutFrameList;  //VDecCompOutputFrame, for non-tunnel mode. when mOutputPortTunnelFlag == FALSE, use it to store decoded frames.
    struct list_head mUsedOutFrameList;  //VDecCompOutputFrame, use it to store user occupied frames or out frames.
    int mFrameNodeNum;  //number of mIdleOutFrameList nodes
    BOOL mWaitOutFrameFullFlag;
    BOOL mWaitOutFrameFlag;  //wait empty frame return
    pthread_mutex_t mOutFrameListMutex;
    pthread_cond_t mOutFrameFullCondition;
    BOOL mWaitReadyFrameFlag;  //wait frame ready, so can get it.
    pthread_cond_t mReadyFrameCondition;

    pthread_mutex_t mDecodeFramesControlLock;
    VDEC_DECODE_FRAME_PARAM_S mDecodeFramesParam;
    BOOL mLimitedDecodeFramesFlag;    //flag if decode limited frames
    int mCurDecodeFramesNum;
        
    pthread_t mCompFrameBufferThreadId;
    
    message_queue_t mCompFrameBufferThreadMessageQueue;
    struct list_head mCompIdleOutFrameList;  //VDecCompOutputFrame, component malloc buffer to copy, for mbUseCompBuffer mode. VDEC_COMP_FRAME_COUNT
    struct list_head mCompReadyOutFrameList;  //VDecCompOutputFrame, component malloc buffer to copy, for mbUseCompBuffer mode. for non-tunnel mode. when mOutputPortTunnelFlag == FALSE, use it to store decoded frames.
    struct list_head mCompUsedOutFrameList;  //VDecCompOutputFrame, component malloc buffer to copy, for mbUseCompBuffer mode. use it to store user occupied frames or out frames.
    pthread_mutex_t mCompOutFramesLock;
    BOOL mbCompFBThreadWaitVdecFrameInput;
    BOOL mbCompFBThreadWaitOutFrame;
    BOOL mbCompFBThreadWaitOutFrameFull;
    pthread_cond_t mCompFBThreadOutFrameFullCondition;
    BOOL mbCompWaitReadyFrame;  //wait frame ready, so can get it.
    pthread_cond_t mCompReadyFrameCondition;
    pthread_mutex_t mCompFBThreadStateLock;
    pthread_cond_t mCondCompFBThreadState;
    COMP_STATETYPE mCompFBThreadState;  //invalid, loaded, idle, executing
    
    unsigned int exit_counter;

    unsigned int disable_3d;
    unsigned int
      _3d_mode;  //same as cdx_ctx->_3d_mode, 1:3d double stream; 0:common stream, maybe 3d or not, cedarv_3d_mode_e
    unsigned int drop_B_frame;
    //int                         av_sync;
    BOOL mResolutionChangeFlag;

    int nDisplayFrameRequestMode;  //0:malloc self; 1:from gui.
    //for VideoRender_GUI mode.
    BOOL mbChangeGraphicBufferProducer;        //if displayFrame is from gui, vdec need process bufferProducer change.
    int mIonFd;                                //need dupfd.
    struct list_head mANWBuffersList;          //VDANWBuffer, for VideoRender_GUI mode,
    struct list_head mPreviousANWBuffersList;  //VDANWBuffer, for VideoRender_GUI mode, change native window.

    int mNoBitstreamCounter;

    //statistics
    int64_t mTotalDecodeDuration;   //unit:us
    int mDecodeFrameCount;
    
    struct VIDEODEC_INPUT_DATA* pInputData;

    VideoStreamInfo stVideoStreamInfo;
}VIDEODECDATATYPE;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* _VIDEODEC_DATATYPE_H_ */

