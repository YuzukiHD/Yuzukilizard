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
#include <mm_comm_aio.h>

#include <alsa/asoundlib.h>

#define MAX_FILE_PATH_SIZE (256)
#define MAX_FORMAT_SUPPORT_NUM      (3)
#define MAX_FRAME_SUPPORT_NUM       (3)
#define MAX_AUDIO_CARD_NAME_LEN     (16)

typedef enum SampleUsbCameraUVCOutBufDataType{
    BUF_DATA_TYPE_JPEG_STREAM = 0,
    BUF_DATA_TYPE_MJPEG_STREAM,
    BUF_DATA_TYPE_H264_HEADER,
    BUF_DATA_TYPE_H264_STREAM,
    BUF_DATA_TYPE_YUV2_STREAM,
    BUF_DATA_TYPE_BUTT,
}SampleUsbCameraUVCOutBufDataType;

typedef struct SampleUsbCameraUVCFormat {
    unsigned int iWidth;
    unsigned int iHeight;
    unsigned int iFormat;
    unsigned int iInterval; // units:100ns
} SampleUsbCameraUVCFormat;

typedef struct SampleUsbCameraUVCFrame {
    void *pVirAddr;
    void *pPhyAddr;
    unsigned int iBufLen;
} SampleUsbCameraUVCFrame;

typedef struct SampleUsbCameraUVCDevice {
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

    SampleUsbCameraUVCFrame *pstFrames;
    int iBufsNum;

    void *pPrivite;
} SampleUsbCameraUVCDevice;

typedef struct SampleUsbCameraCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];

} SampleUsbCameraCmdLineParam;

typedef struct SampleUsbCameraUVCOutBuf {
    unsigned char *pcData;
    SampleUsbCameraUVCOutBufDataType eDataType;
    unsigned int iDataBufSize;
    unsigned int iDataSize0;
    unsigned int iDataSize1;
    unsigned int iDataSize2;
    struct list_head mList;
} SampleUsbCameraUVCOutBuf;

typedef struct SampleUsbCameraUVCInfo {
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
} SampleUsbCameraUVCInfo;

typedef struct SampleUsbCameraUACAlsaConfig
{
    snd_pcm_t *mpUACHandle;
    snd_pcm_hw_params_t *mpUAChwParam;
    snd_pcm_sw_params_t *mpUACswParam;
    snd_pcm_uframes_t mPeriodSize;
    snd_pcm_uframes_t mBufferSize;
    unsigned int mPeriods;
    int mBytesPerFrame;
}SampleUsbCameraUACAlsaConfig;

typedef struct SampleUsbCameraUACConfig
{
    AUDIO_DEV mAIDev;
    AI_CHN mAIChn;
    AIO_ATTR_S mAIOAttr;

    BOOL mbTaskProcRunning;
    BOOL mbTaskProcExit;
    pthread_t mUACTaskProcTrd;
    SampleUsbCameraUACAlsaConfig mUACAlsaCfg;
}SampleUsbCameraUACConfig;

typedef struct SampleUsbCameraConfig
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

    BOOL mbEnableUAC;
    char mUACAudioCard[MAX_AUDIO_CARD_NAME_LEN];
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mAIGain;
}SampleUsbCameraConfig;

typedef struct SampleUsbCameraContext
{
    SampleUsbCameraUVCInfo mUVCInfo;
    SampleUsbCameraUVCDevice mUVCDev;
    SampleUsbCameraUACConfig mUACInfo;
    SampleUsbCameraConfig mConfig;
    SampleUsbCameraCmdLineParam mCmdLinePara;
    int iBulkMode;
} SampleUsbCameraContext;

int initSampleUsbCameraContext();
int destroySampleUsbCameraContext();

#endif  /* _SAMPLE_UVC_H_ */
