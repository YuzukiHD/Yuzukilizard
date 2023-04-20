#ifndef _SAMPLE_UVC_H_
#define _SAMPLE_UVC_H_

#include <pthread.h>
#include "./include/video.h"

#include <tsemaphore.h>
#include <plat_type.h>
#include <mm_comm_vo.h>
#include <mm_common.h>
#include <mm_comm_video.h>
#include <mm_comm_venc.h>

#define MAX_FILE_PATH_SIZE (256)
#define MAX_FORMAT_SUPPORT_NUM      (3)
#define MAX_FRAME_SUPPORT_NUM       (3)

typedef enum SampleUvcOutBufDataType{
    BUF_DATA_TYPE_JPEG_STREAM = 0,
    BUF_DATA_TYPE_MJPEG_STREAM,
    BUF_DATA_TYPE_H264_HEADER,
    BUF_DATA_TYPE_H264_STREAM,
    BUF_DATA_TYPE_YUV2_STREAM,
    BUF_DATA_TYPE_BUTT,
}SampleUvcOutBufDataType;

typedef struct SampleUVCFormat {
    unsigned int iWidth;
    unsigned int iHeight;
    unsigned int iFormat;
    unsigned int iInterval; // units:100ns
} SampleUVCFormat;

typedef struct SampleUVCFrame {
    void *pVirAddr;
    void *pPhyAddr;
    unsigned int iBufLen;
} SampleUVCFrame;

typedef struct SampleUVCDevice {
    int iDev; // must be set before open video device

    int iFd;

    int bIsStreaming;
    int bIsBulkMode;

    int iCtrlSetCur;
    struct uvc_streaming_control stProbe;
    struct uvc_streaming_control stCommit;

    unsigned int iWidth;
    unsigned int iHeight;
    unsigned int iFormat;
    unsigned int iInterval; // units:100ns

    SampleUVCFrame *pstFrames;
    int iBufsNum;

    void *pPrivite;
} SampleUVCDevice;

typedef struct SampleUVCCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];

} SampleUVCCmdLineParam;

typedef struct SampleUVCOutBuf {
    unsigned char *pcData;
    SampleUvcOutBufDataType eDataType;
    unsigned int iDataBufSize;
    unsigned int iDataSize0;
    unsigned int iDataSize1;
    unsigned int iDataSize2;
    struct list_head mList;
} SampleUVCOutBuf;

typedef struct SampleUVCInfo {
    BOOL mbCapRunningFlag;
    BOOL mbCapExitFlag;
    pthread_t mCaptureTrd;

    int iVippDev;
    int iVippChn;
    int iVencChn;
    int iIspDev;

    int mG2dFd;
    VIDEO_FRAME_INFO_S mG2dProcFrm;

    struct list_head mIdleFrm;
    struct list_head mValidFrm;
    struct list_head mUsedFrm;
    pthread_mutex_t mFrmLock;
} SampleUVCInfo;

typedef struct SampleUVCConfig
{
    int iUVCDev;
    int iCapDev;
    int iCapWidth;
    int iCapHeight;
    int iCapFrameRate;
    PIXEL_FORMAT_E eCapFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420

    PAYLOAD_TYPE_E eEncoderType;
    int iEncWidth;
    int iEncHeight;
    int iEncBitRate;
    int iEncFrameRate;
    int iEncQuality;
}SampleUVCConfig;

typedef struct SampleUVCContext
{
    SampleUVCInfo mUVCInfo;
    SampleUVCDevice mUVCDev;
    SampleUVCConfig mUVCConfig;
    SampleUVCCmdLineParam mCmdLinePara;
    int iBulkMode;
} SampleUVCContext;

int initSampleUVCContext();
int destroySampleUVCContext();

#endif  /* _SAMPLE_UVC_H_ */
