#ifndef __SAMPLE_V459FQN_H__
#define __SAMPLE_V459FQN_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

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

typedef struct venc_in_frame_s
{
    VIDEO_FRAME_INFO_S  mFrame;
    struct list_head mList;
}VENC_IN_FRAME_S, *PTR_VENC_IN_FRAME_S;


typedef enum RecordState
{
    REC_NOT_PREPARED = 0,
    REC_PREPARED,
    REC_RECORDING,
    REC_STOP,
    REC_ERROR,
}RECSTATE_E;

typedef struct Vi2Venc2Muxer_CmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
}VI2VENC2MUXER_CMDLINEPARAM_S;

typedef struct Vi2Venc2Muxer_Config
{
    char dstVideoFile[MAX_FILE_PATH_LEN];

    int srcSize;
    int dstSize;
    int srcWidth;
    int srcHeight;
    int srcPixFmt;
    int dstPixFmt;
    int dstWidth;
    int dstHeight;

    int mDevNo;

    int mVideoEncoderFmt;
    int mVideoFrameRate;
    int mVideoBitRate;
    int mMaxFileDuration;
    int mTestDuration;

    int mRcMode;
    int mEnableFastEnc;
    int mEnableRoi;

    BOOL mColor2Grey;
    BOOL m3DNR;
}VI2VENC2MUXER_CONFIG_S;


typedef struct sample_vi2venc2ce_s
{
    VI2VENC2MUXER_CONFIG_S mConfigPara;
    VI2VENC2MUXER_CMDLINEPARAM_S mCmdLinePara;

    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

    RECSTATE_E mCurrentState;

    VI_ATTR_S mViAttr;
    ISP_DEV mIspDev;
    VI_DEV mViDev;
    VI_CHN mViChn;

    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;
    pthread_t thid;
    int mEncppSharpAttenCoefPer;

    int encrypt_fail_cnt;
    int decrypt_fail_cnt;
}SAMPLE_VI2VENC2CE_S;

typedef struct awVI2Venc_PrivCap_S {
    pthread_t thid;
    VI_DEV Dev;
    VI_CHN Chn;
    AW_S32 s32MilliSec;
    PAYLOAD_TYPE_E EncoderType;
    VENC_CHN mVencChn;
} VI2Venc_Cap_S;

#endif //#define __SAMPLE_V459FQN_H__
