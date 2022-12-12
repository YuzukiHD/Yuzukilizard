#ifndef _SAMPLE_MULTI_VI2VENC2MUXER_H_
#define _SAMPLE_MULTI_VI2VENC2MUXER_H_

#include <mm_common.h>
#include <mm_comm_video.h>
#include <mm_comm_vi.h>
#include <mm_comm_venc.h>
#include <mm_comm_mux.h>
#include <mm_comm_sys.h>
#include <tsemaphore.h>
#include "tmessage.h"

#define MAX_FILE_PATH_SIZE (256)
#define MAX_RECORDER_COUNT  (4)

typedef struct SampleMultiVi2venc2muxerCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleMultiVi2venc2muxerCmdLineParam;

typedef enum RecordState
{
    REC_NOT_PREPARED = 0,
    REC_PREPARED,
    REC_RECORDING,
    REC_STOP,
    REC_ERROR,
}RECSTATE_E;

typedef enum MultiVi2Venc2MuxerMsgType
{
    Rec_NeedSetNextFd = 0,
    Rec_FileDone,
    MsgQueue_Stop,
}MultiVi2Venc2MuxerMsgType;

typedef struct SampleMultiVi2venc2muxerConfig
{
    //vipp configure
    int mVipp0;
    PIXEL_FORMAT_E mVipp0Format;
    int mVipp0CaptureWidth;
    int mVipp0CaptureHeight;
    int mVipp0Framerate;
    int mVipp1;
    PIXEL_FORMAT_E mVipp1Format;
    int mVipp1CaptureWidth;
    int mVipp1CaptureHeight;
    int mVipp1Framerate;

    //params of every video encode line
    int mVideoAVipp;    //vipp to connect, -1 means disable.
    char mVideoAFile[MAX_FILE_PATH_SIZE];
    int mVideoAFileCnt;
    int mVideoAFramerate;
    int mVideoABitrate;
    int mVideoAWidth;
    int mVideoAHeight;
    PAYLOAD_TYPE_E mVideoAEncoderType;
    int mVideoARcMode;      //rc_mode for H264/H265 0:CBR  1:VBR  2:FIXQP  3:QPMAP
    int mVideoADuration;    //per output media file time len (s)
    int mVideoATimelapse;   // -1:disable timelapse; 0:enable slow photograph; >0:enable timelapse, interval's unit is us

    int mVideoBVipp;
    char mVideoBFile[MAX_FILE_PATH_SIZE];
    int mVideoBFileCnt;
    int mVideoBFramerate;
    int mVideoBBitrate;
    int mVideoBWidth;
    int mVideoBHeight;
    PAYLOAD_TYPE_E mVideoBEncoderType;
    int mVideoBRcMode;
    int mVideoBDuration;
    int mVideoBTimelapse;

    int mVideoCVipp;
    char mVideoCFile[MAX_FILE_PATH_SIZE];
    int mVideoCFileCnt;
    int mVideoCFramerate;
    int mVideoCBitrate;
    int mVideoCWidth;
    int mVideoCHeight;
    PAYLOAD_TYPE_E mVideoCEncoderType;
    int mVideoCRcMode;
    int mVideoCDuration;
    int mVideoCTimelapse;

    int mVideoDVipp;
    char mVideoDFile[MAX_FILE_PATH_SIZE];
    int mVideoDFileCnt;
    int mVideoDFramerate;
    int mVideoDBitrate;
    int mVideoDWidth;
    int mVideoDHeight;
    PAYLOAD_TYPE_E mVideoDEncoderType;
    int mVideoDRcMode;
    int mVideoDDuration;
    int mVideoDTimelapse;

    int mVideoEVipp;
    char mVideoEFileJpeg[MAX_FILE_PATH_SIZE];
    int mVideoEFileJpegCnt;
    int mVideoEWidth;
    int mVideoEHeight;
    PAYLOAD_TYPE_E mVideoEEncoderType;
    int mVideoEPhotoInterval;   //unit:s, take picture every interval seconds.

    int mTestDuration;  //unit:s
}SampleMultiVi2venc2muxerConfig;

typedef struct VippConfig
{
    bool    mbValid;
    ISP_DEV mIspDev;
    VI_DEV  mViDev;
    VI_ATTR_S mViAttr;

    //config param
    PIXEL_FORMAT_E mPixelFormat;
    int mCaptureWidth;
    int mCaptureHeight;
    int mFramerate;

    bool mbEnable;  //indicate if vipp is enable.
}VippConfig;

typedef struct 
{
    char strFilePath[MAX_FILE_PATH_SIZE];
    struct list_head mList;
}FilePathNode;


typedef struct Vi2venc2muxerContext
{
    bool mbValid;
    char mName[MAX_FILE_PATH_SIZE]; //e.g.:"VideoA"

    //config params
    char mVideoFile[MAX_FILE_PATH_SIZE];
    MEDIA_FILE_FORMAT_E mFileFormat;
    int mVideoFileCnt;  //0:means don't remove files.
    int mVideoFramerate;
    int mVideoBitrate;
    int mVideoWidth;    //encode dst width.
    int mVideoHeight;
    PAYLOAD_TYPE_E mVideoEncoderType;
    int mVideoRcMode;   //0:CBR  1:VBR  2:FIXQP  3:ABR
    int mVideoDuration; //unit:s
    int mVideoTimelapse;    //unit:us, -1:disable timelapse; 0:enable slow photograph; >0:enable timelapse

    //component info
    VippConfig *mpConnectVipp;
    VI_CHN mViChn;

    VENC_CHN mVeChn;
    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    int mEncppSharpAttenCoefPer;

    MUX_GRP mMuxGrp;
    MUX_GRP_ATTR_S mMuxGrpAttr;
    MUX_CHN mMuxChn;
    MUX_CHN_ATTR_S mMuxChnAttr;

    int mFileIdCounter;
    pthread_mutex_t mFilePathListLock;
    struct list_head mFilePathList; //FilePathNode
}Vi2venc2muxerContext;

typedef struct PictureBuffer
{
    unsigned char *mpData0;
    unsigned char *mpData1;
    unsigned char *mpData2;
    unsigned int mLen0;
    unsigned int mLen1;
    unsigned int mLen2;
    unsigned int mThumbOffset;
    unsigned int mThumbLen;
    unsigned int mDataSize;
}PictureBuffer;
typedef struct TakeJpegContext
{
    bool mbValid;
    char mName[MAX_FILE_PATH_SIZE]; //e.g.:"VideoEJpeg"

    //config params
    char mJpegFile[MAX_FILE_PATH_SIZE];
    int mJpegFileCnt;
    int mJpegWidth;
    int mJpegHeight;
    PAYLOAD_TYPE_E mVideoEncoderType;
    int mPhotoInterval; //unit:s

    //component info
    VippConfig *mpConnectVipp;
    VI_CHN mViChn;

    VENC_CHN mVeChn;
    VENC_CHN_ATTR_S mVencChnAttr;

    //PictureBuffer mJpegBuffer;
    int mJpegCounter;
    pthread_mutex_t mFilePathListLock;
    struct list_head mFilePathList; //FilePathNode
    
    pthread_t mTakePictureThreadId;
    bool mbExitFlag;
}TakeJpegContext;

typedef struct SampleMultiVi2venc2muxerContext
{
    SampleMultiVi2venc2muxerCmdLineParam mCmdLinePara;
    SampleMultiVi2venc2muxerConfig mConfigPara;

    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;
    VippConfig mVippConfigs[MAX_VIPP_DEV_NUM];
    Vi2venc2muxerContext mRecorders[MAX_RECORDER_COUNT];
    TakeJpegContext mTakeJpegCtx;

    pthread_t mMsgQueueThreadId;
    message_queue_t mMsgQueue;

}SampleMultiVi2venc2muxerContext;

typedef struct MultiVi2Venc2Muxer_MessageData
{
    Vi2venc2muxerContext *pRecCtx;
}MultiVi2Venc2Muxer_MessageData;

#endif  /* _SAMPLE_MULTI_VI2VENC2MUXER_H_ */

