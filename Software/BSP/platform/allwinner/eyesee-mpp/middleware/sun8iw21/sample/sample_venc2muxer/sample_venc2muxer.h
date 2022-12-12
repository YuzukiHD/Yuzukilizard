#ifndef __SAMPLE_VENC2MUXER_H__
#define __SAMPLE_VENC2MUXER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cdx_list.h>

#include <pthread.h>

#include "mm_comm_sys.h"
#include "mpi_sys.h"

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


#define IN_FRAME_BUF_NUM  (6)

#define MAX_FILE_PATH_LEN  (128)

typedef enum RecordState
{
    REC_NOT_PREPARED = 0,
    REC_PREPARED,
    REC_RECORDING,
    REC_STOP,
    REC_ERROR,
}RECSTATE_E;

typedef enum Venc2MuxerMsgType
{
    Rec_NeedSetNextFd = 0,
    Rec_FileDone,
    MsgQueue_Stop,
}Venc2MuxerMsgType;

typedef struct output_sink_info_s
{
    int mMuxerId;
    MEDIA_FILE_FORMAT_E mOutputFormat;
    int mOutputFd;
    int mFallocateLen;
    BOOL mCallbackOutFlag;
}OUTSINKINFO_S, *PTR_OUTSINKINFO_S;

typedef struct mux_chn_info_s
{
    OUTSINKINFO_S mSinkInfo;
    MUX_CHN_ATTR_S mMuxChnAttr;
    MUX_CHN mMuxChn;
    struct list_head mList;
}MUX_CHN_INFO_S, *PTR_MUX_CHN_INFO_S;


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

typedef struct IN_FRAME_NODE_S
{
    VIDEO_FRAME_INFO_S  mFrame;
    struct list_head mList;
}IN_FRAME_NODE_S;

typedef struct INPUT_BUF_Q
{
    int mFrameNum;

    struct list_head mIdleList;
    struct list_head mReadyList;
    struct list_head mUseList;
    pthread_mutex_t mIdleListLock;
    pthread_mutex_t mReadyListLock;
    pthread_mutex_t mUseListLock;
}INPUT_BUF_Q;

typedef struct Venc2Muxer_CmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}VENC2MUXER_CMDLINEPARAM_S;

typedef struct Venc2Muxer_Config
{
    FILE *srcYUVFd;

    char srcYUVFile[MAX_FILE_PATH_LEN];
    char dstVideoFile[MAX_FILE_PATH_LEN];

    int srcSize;
    int srcWidth;
    int srcHeight;
    int srcPixFmt;
    int mSrcRegionX;
    int mSrcRegionY;
    int mSrcRegionW;
    int mSrcRegionH;

    int dstSize;
    int dstPixFmt;
    int dstWidth;
    int dstHeight;

    int mField;
    PAYLOAD_TYPE_E mVideoEncoderFmt;
    MEDIA_FILE_FORMAT_E mMediaFileFmt;
    int mVideoFrameRate;
    int mVideoBitRate;
    int mMaxFileDuration;

    int mEncUseProfile;
    int mRcMode;

    int mTestDuration;
}VENC2MUXER_CONFIG_S;


typedef struct sample_venc2muxer_s
{
    VENC2MUXER_CMDLINEPARAM_S mCmdLinePara;
    VENC2MUXER_CONFIG_S mConfigPara;

    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

    RECSTATE_E mCurRecState;

    int mMuxerIdCounter;

    MUX_GRP_ATTR_S mMuxGrpAttr;
    MUX_GRP mMuxGrp;

    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;

    pthread_mutex_t mChnListLock;
    struct list_head mMuxChnList;

    char *tmpBuf;

    INPUT_BUF_Q mInBuf_Q;
    pthread_t mReadThdId;

    BOOL mOverFlag;

    pthread_t mMsgQueueThreadId;
    message_queue_t mMsgQueue;
}SAMPLE_VENC2MUXER_S;

typedef struct Venc2Muxer_MessageData
{
    SAMPLE_VENC2MUXER_S *pVenc2MuxerData;
}Venc2Muxer_MessageData;

#endif //#define __SAMPLE_VENC2MUXER_H__
