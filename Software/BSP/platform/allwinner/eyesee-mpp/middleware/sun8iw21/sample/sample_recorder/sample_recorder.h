#ifndef _SAMPLE_RECORDER_H_
#define _SAMPLE_RECORDER_H_

#include <mm_common.h>
#include <mm_comm_video.h>
#include <mm_comm_vi.h>
#include <mm_comm_venc.h>
#include <mm_comm_mux.h>
#include <mm_comm_sys.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include "tmessage.h"

#define MAX_FILE_PATH_SIZE (256)
#define MAX_RECORDER_COUNT  (4)

typedef struct SampleRecorderCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleRecorderCmdLineParam;

typedef enum RecorderState
{
    REC_NOT_PREPARED = 0,
    REC_PREPARED,
    REC_RECORDING,
    REC_STOP,
    REC_ERROR,
}RECSTATE_E;

typedef enum SampleRecorderMsgType
{
    Rec_NeedSetNextFd = 0,
    Rec_FileDone,
    MsgQueue_Stop,
}SampleRecorderMsgType;

typedef struct
{
    char strFilePath[MAX_FILE_PATH_SIZE];
    struct list_head mList;
}FilePathNode;

typedef struct SampleRecorderCondfig
{
    int mVIDev;
    int mIspDev;
    int mCapWidth;
    int mCapHeight;
    int mCapFrmRate;
    PIXEL_FORMAT_E mCapFormat;
    int mVIBufNum;
    int mEnableWDR;

    int mEncOnlineEnable;
    int mEncOnlineShareBufNum;
    PAYLOAD_TYPE_E mEncType;
    int mEncWidth;
    int mEncHeight;
    int mEncFrmRate;
    int mEncBitRate;
    int mEncRcMode;
    int mEncppSharpAttenCoefPer;

    int mDispX;
    int mDispY;
    int mDispWidth;
    int mDispHeight;
    VO_INTF_TYPE_E mDispDev;

    int mRecDuration;
    int mRecFileCnt;
    char mOutFilePath[MAX_FILE_PATH_SIZE];
}SampleRecorderCondfig;

typedef struct SampleRecorder
{
    BOOL mbRecorderValid;
    VI_DEV mVIDev;
    VI_CHN mVIChn;
    ISP_DEV mIspDev;
    VI_ATTR_S mVIAttr;

    VENC_CHN mVeChn;
    VENC_CHN_ATTR_S mVeChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    int mEncppSharpAttenCoefPer;

    MUX_GRP mMuxGrp;
    MUX_CHN mMuxChn;
    MUX_GRP_ATTR_S mMuxGrpAttr;
    MUX_CHN_ATTR_S mMuxChnAttr;

    VO_LAYER mVOLayer;
    VO_CHN mVOChn;
    VO_VIDEO_LAYER_ATTR_S mVOLayerAttr;

    int mFileIdCounter;
    pthread_mutex_t mFilePathListLock;
    struct list_head mFilePathList; //FilePathNode
    SampleRecorderCondfig mConfig;
}SampleRecorder;

typedef struct SampleRecorderContext
{
    SampleRecorderCmdLineParam mCmdLinePara;

    int mTestDuration;
    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVODev;
    VO_LAYER mUILayer;
    BOOL mbEnableVO;
    SampleRecorder mRecorder[MAX_RECORDER_COUNT];

    pthread_t mMsgQueueThreadId;
    message_queue_t mMsgQueue;
}SampleRecorderContext;

typedef struct SampleRecorder_MessageData
{
    SampleRecorder *pRecorder;
}SampleRecorder_MessageData;

#endif
