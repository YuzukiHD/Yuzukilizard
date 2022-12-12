
#ifndef _SAMPLE_TAKE_PICTURE_H_
#define _SAMPLE_TAKE_PICTURE_H_

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct TakePicture_Cap_S {
    pthread_t thid;
    VI_DEV Dev;
    VI_CHN Chn;
    int s32MilliSec;
    VIDEO_FRAME_INFO_S pstFrameInfo;
    void *mpContext;    //SampleTakePictureContext*

    int mRawStoreNum;
    int mJpegStoreNum;

    VENC_CHN mChn;
    VENC_CHN_ATTR_S mChnAttr;
    VENC_PARAM_JPEG_S mJpegParam;
    VENC_EXIFINFO_S mExifInfo;
    VENC_STREAM_S mOutStream;
    VENC_JPEG_THUMB_BUFFER_S mJpegThumbBuf;
    cdx_sem_t mSemFrameBack;
    unsigned int mCurFrameId;
} TakePicture_Cap_S;

typedef struct SampleTakePictureCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleTakePictureCmdLineParam;

typedef struct SampleTakePictureConfig
{
    int DevNum;
    int SrcWidth;
    int SrcHeight;
    int FrameRate;
    unsigned int SrcFormat; //V4L2_PIX_FMT_NV21M
    enum v4l2_colorspace mColorSpace;
    int mViDropFrmCnt;

    int mTakePictureMode;
    int mTakePictureNum;
    int mTakePictureInterval;
    int mJpegWidth;
    int mJpegHeight;
    char mStoreDirectory[MAX_FILE_PATH_SIZE];   //e.g.: /mnt/extsd

    int mEncodeRotate;
    int mHorizonFlipFlag;

    BOOL mCropEnable;
    int mCropRectX;
    int mCropRectY;
    int mCropRectWidth;
    int mCropRectHeight;
}SampleTakePictureConfig;

enum TAKE_PICTURE_MODE_E
{
    TAKE_PICTURE_MODE_NULL = 0,
    TAKE_PICTURE_MODE_NORMAL,   // stream off -> stream on -> take picture -> stream off -> stream on
    TAKE_PICTURE_MODE_FAST,     // same as normal mode but do not need to stream off/on, use preview capture setting.
    TAKE_PICTURE_MODE_CONTINUOUS, //fast mode, take picture more times, need set PictureInterval(if 0, encode frame by frame), need set PictureNumber.
};

typedef struct SampleTakePictureContext
{
    SampleTakePictureCmdLineParam mCmdLinePara;
    SampleTakePictureConfig mConfigPara;
    enum TAKE_PICTURE_MODE_E mTakePicMode;

    TakePicture_Cap_S privCap[MAX_VIPP_DEV_NUM][MAX_VIR_CHN_NUM];
}SampleTakePictureContext;

#endif  /* _SAMPLE_TAKE_PICTURE_H_ */
