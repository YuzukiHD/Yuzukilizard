/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CameraJpegEncoder.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/15
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_CAMERA_JPEG_ENCODER_H__
#define __IPCLINUX_CAMERA_JPEG_ENCODER_H__

#include <vencoder.h>
#include <tsemaphore.h>
#include <mm_comm_video.h>
#include <mm_comm_venc.h>

#include <Errors.h>

namespace EyeseeLinux {

typedef struct CameraJpegEncConfig
{
    PIXEL_FORMAT_E mPixelFormat;
    unsigned int mSourceWidth;
    unsigned int mSourceHeight;
    unsigned int mPicWidth;
    unsigned int mPicHeight;
    unsigned int mThumbnailWidth;
    unsigned int mThumbnailHeight;
    unsigned int mThunbnailQuality;
    int mQuality;
    unsigned int nVbvBufferSize;
    unsigned int nVbvThreshSize;
} CameraJpegEncConfig;

class CameraJpegEncoder
{
public:
    CameraJpegEncoder();
    ~CameraJpegEncoder();

    status_t initialize(CameraJpegEncConfig *pConfig);
    status_t destroy();
    status_t encode(VIDEO_FRAME_INFO_S *pFrameInfo, VENC_EXIFINFO_S *pExifInfo);
    int getFrame();
    int returnFrame(VENC_STREAM_S *pVencStream);
    size_t getDataSize();
    //status_t getDataToBuffer(void *buffer);
    status_t getThumbOffset(off_t &offset, size_t &len);
    VENC_CHN getJpegVenChn(){ return mChn;}
//private:
    static ERRORTYPE VEncCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
    cdx_sem_t mSemFrameBack;

    //VideoEncoder *mpVideoEnc;
    VENC_CHN mChn;
    VENC_CHN_ATTR_S mChnAttr;
    VENC_PARAM_JPEG_S mJpegParam;
    //VENC_EXIFINFO_S mJpegExifInfo;
    //VencOutputBuffer mOutBuffer;
    VENC_STREAM_S mOutStream;
    VENC_JPEG_THUMB_BUFFER_S mJpegThumbBuf;
    //for verify frame
    unsigned int mCurFrameId;
};

}; /* namespace EyeseeLinux */

#endif /* __IPCLINUX_CAMERA_JPEG_ENCODER_H__ */

