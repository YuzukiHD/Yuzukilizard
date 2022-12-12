/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CallbackNotifier.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/03
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "CallbackNotifier"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <memory.h>
#include <type_camera.h>
#include <mpi_region.h>

#include <memoryAdapter.h>

#include "CallbackNotifier.h"
#include "CameraJpegEncoder.h"
#include <ModData.h>
#include <AdasData.h>
#include "VIDEO_FRAME_INFO_S.h"

//#include <ion_memmanager.h>

#define PictureThumbQuality_DEFAULT 60

using namespace std;

namespace EyeseeLinux {

static int getMeminfo(const char* pattern)
{
  FILE* fp = fopen("/proc/meminfo", "r");
  if (fp == NULL) {
    return -1;
  }

  int result = -1;
  char buf[256];
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    if (sscanf(buf, pattern, &result) == 1) {
      break;
    }
  }
  fclose(fp);
  return result;
}

static int getTotalMemory(void)
{
  return getMeminfo("MemTotal: %d kB");
}

static int getFreeMemory(void)
{
  return getMeminfo("MemFree: %d kB");
}

static int getCachedMemory(void)
{
  return getMeminfo("Cached: %d kB");
}

static int getBuffersMemory(void)
{
  return getMeminfo("Buffers: %d kB");
}

CallbackNotifier::DoSavePictureThread::DoSavePictureThread(CallbackNotifier *pCallbackNotifier)
    : mpCallbackNotifier(pCallbackNotifier)
{
    mbWaitPicture = false;
    mbWaitReleasePicture = false;
}

status_t CallbackNotifier::DoSavePictureThread::startThread()
{
    status_t ret = run("SavePictureTh");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void CallbackNotifier::DoSavePictureThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeSavePic_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t CallbackNotifier::DoSavePictureThread::notifyNewPictureCome()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitPicture)
    {
        mbWaitPicture = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeSavePic_InputPictureAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

status_t CallbackNotifier::DoSavePictureThread::notifyPictureRelease()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitReleasePicture)
    {
        mbWaitReleasePicture = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeSavePic_ReleasePicture;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

bool CallbackNotifier::DoSavePictureThread::threadLoop()
{
    if(!exitPending())
    {
        return mpCallbackNotifier->savePictureThread();
    } 
    else
    {
        return false;
    }
}

CallbackNotifier::CallbackNotifier(int chnId, CameraBufferReference *pCbr)
    : mChnId(chnId)
    , mpCamBufRef(pCbr)
    , mpDataCallback(NULL)
    , mpNotifyCallback(NULL)
    , mPictureWidth(640)
    , mPictureHeight(480)
    , mPictureThumbWidth(320)
    , mPictureThumbHeight(240)
    , mPictureThumbQuality(PictureThumbQuality_DEFAULT)
    , mJpegQuality(90)
    , mJpegRotate(0)
    , mGpsLatitude(0.0)
    , mGpsLongitude(0.0)
    , mGpsAltitude(0.0)
    , mGpsTimestamp(0)
    , mpGpsProcessingMethod(NULL)
    , mMessageEnabler(0)
    , mpFolderPath(NULL)
    , mpSnapPath(NULL)
    , mSavePictureCnt(0)
    , mpPictureRegionCallback(NULL)
{
    alogv("CallbackNotifier construct");
    mPictureNum = 1;
    mpJpegenc = NULL;
    mbWaitPicBufListEmpty = false;
    memset(&mJpegEncConfig, 0, sizeof(CameraJpegEncConfig));
    mpSavePicThread = new DoSavePictureThread(this);
    mpSavePicThread->startThread();
}

CallbackNotifier::~CallbackNotifier()
{
    alogv("CallbackNotifier destruct");

    if (mpSavePicThread != NULL) 
    {
        mpSavePicThread->stopThread();
        delete mpSavePicThread;
    }
    if (mpGpsProcessingMethod != NULL) {
        free(mpGpsProcessingMethod);
        mpGpsProcessingMethod = NULL;
    }
    if (mpFolderPath != NULL) {
        free(mpFolderPath);
        mpFolderPath = NULL;
    }
    if (mpSnapPath != NULL) {
        free(mpSnapPath);
        mpSnapPath = NULL;
    }

    mPicBufLock.lock();
    if(!mPicBufList.empty())
    {
        alogw("impossible! some pictureBuf not send, release them!");
//        for (std::list<PictureBuffer>::iterator it = mPicBufList.begin(); it != mPicBufList.end(); ++it) 
//        {
//            delete[] it->mpData;
//        }
        
    }
    if(!mWaitReleasePicBufList.empty())
    {
        alogw("impossible! some pictureBuf not return, release them!");
//        for (std::list<PictureBuffer>::iterator it = mWaitReleasePicBufList.begin(); it != mWaitReleasePicBufList.end(); ++it) 
//        {
//            delete[] it->mpData;
//        }
    }
    mPicBufLock.unlock();

    if(mpJpegenc)
    {
        alogw("jpeg encoder is exist, maybe user sets keeping picture encoder? destroy now!");
        destoryPictureRegion();
        mpJpegenc->destroy();
        delete mpJpegenc;
        mpJpegenc = NULL;
    }
}

status_t CallbackNotifier::NotifyRenderStart()
{
    unsigned int nCameraMsgType = CAMERA_MSG_INFO;  //CAMERA_MSG_COMPRESSED_IMAGE;
    CameraMsgInfoType eMsgInfoType = CAMERA_INFO_RENDERING_START;
    mpNotifyCallback->notify(nCameraMsgType, mChnId, eMsgInfoType, 0);
    return NO_ERROR;
}

status_t CallbackNotifier::NotifyCameraDeviceTimeout()
{
    unsigned int nCameraMsgType = CAMERA_MSG_ERROR;
    CameraMsgErrorType eMsgInfoType = CAMERA_ERROR_SELECT_TIMEOUT;
    mpNotifyCallback->notify(nCameraMsgType, mChnId, eMsgInfoType, 0);
    return NO_ERROR;
}
status_t CallbackNotifier::NotifyG2DTimeout()
{
    unsigned int nCameraMsgType = CAMERA_MSG_ERROR;
    CameraMsgErrorType eMsgInfoType = CAMERA_ERROR_G2D_TIMEOUT;
    mpNotifyCallback->notify(nCameraMsgType, mChnId, eMsgInfoType, 0);
    return NO_ERROR;
}

status_t CallbackNotifier::NotifyCameraTakePicFail()
{
    unsigned int nCameraMsgType = CAMERA_MSG_ERROR;
    CameraMsgErrorType eMsgInfoType = CAMERA_ERROR_TAKE_PIC_FAIL;
    mpNotifyCallback->notify(nCameraMsgType, mChnId, eMsgInfoType, 0);
    return NO_ERROR;
}


int CallbackNotifier::onNextFrameAvailable(void* frame, int chnId)
{
    int ret = 0;

    AutoMutex lock(mRecListenerLock);
    for (vector<RecordingProxyListener>::iterator it = mRecListener.begin(); it != mRecListener.end(); ++it)
    {
        if(it->mInputChnId == chnId)
        {
            mpCamBufRef->increaseBufRef((VIDEO_FRAME_BUFFER_S*)frame);
            it->mpListener->dataCallbackTimestamp(frame);
        }
        else
        {
            alogw("Be careful! impossible chnId[%d]!=[%d]", it->mInputChnId, chnId);
        }
    }

    return ret;
}

status_t CallbackNotifier::postMODData(std::shared_ptr<CMediaMemory>& spData)
{
    unsigned int nCameraMsgType = 0;  //CAMERA_MSG_COMPRESSED_IMAGE
    nCameraMsgType |= CAMERA_MSG_MOD_DATA;
    mpDataCallback->postData(nCameraMsgType, mChnId, spData, sizeof(MotionDetectResult));
    return NO_ERROR;
}

status_t CallbackNotifier::postAdasData(std::shared_ptr<CMediaMemory>& spData)
{
    unsigned int nCameraMsgType = 0;  //CAMERA_MSG_COMPRESSED_IMAGE
    nCameraMsgType |= CAMERA_MSG_ADAS_DATA;
    mpDataCallback->postData(nCameraMsgType, mChnId, spData, sizeof(AW_AI_ADAS_DETECT_R_));
    return NO_ERROR;
}


status_t CallbackNotifier::postFaceDetectData(std::shared_ptr<CMediaMemory>& spData)
{
    unsigned int nCameraMsgType = 0;  //CAMERA_MSG_COMPRESSED_IMAGE
    nCameraMsgType |= CAMERA_MSG_FACEDETECT_DATA;
    //mpDataCallback->postData(nCameraMsgType, mChnId, spData, sizeof(AW_AI_EVE_EVENT_RESULT_S));
    return NO_ERROR;
}

status_t CallbackNotifier::postVLPRData(std::shared_ptr<CMediaMemory>& spData)
{
    unsigned int nCameraMsgType = 0;  //CAMERA_MSG_COMPRESSED_IMAGE
    nCameraMsgType |= CAMERA_MSG_VLPR_DATA;
    //mpDataCallback->postData(nCameraMsgType, mChnId, spData, sizeof(AW_AI_CVE_VLPR_RULT_S));
    return NO_ERROR;
}

void CallbackNotifier::setPictureNum(int num)
{
    mPictureNum = num;
}

status_t CallbackNotifier::startRecording(CameraRecordingProxyListener *pCb, int recorderId, int chnId)
{
    RecordingProxyListener listener;
    listener.mRecorderId = recorderId;
    listener.mInputChnId = chnId;
    listener.mpListener = pCb;
    AutoMutex lock(mRecListenerLock);
    mRecListener.push_back(listener);
    return NO_ERROR;
}

status_t CallbackNotifier::stopRecording(int recorderId)
{
    AutoMutex lock(mRecListenerLock);
    for (vector<RecordingProxyListener>::iterator it = mRecListener.begin(); it != mRecListener.end(); ++it) 
    {
        if (it->mRecorderId == recorderId) 
        {
            delete it->mpListener;
            mRecListener.erase(it);
            break;
        }
    }
    return NO_ERROR;
}

void CallbackNotifier::setDataListener(DataListener *pCb)
{
    AutoMutex lock(mLock);
    mpDataCallback = pCb;
}

void CallbackNotifier::setNotifyListener(NotifyListener *pCb)
{
    AutoMutex lock(mLock);
    mpNotifyCallback = pCb;
}

void CallbackNotifier::enableMessage(unsigned int msg_type)
{
    alogv("msg_type = 0x%x", msg_type);
    AutoMutex lock(&mLock);
    mMessageEnabler |= msg_type;
    alogv("**** Currently enabled messages:");
}

void CallbackNotifier::disableMessage(unsigned int msg_type)
{
    alogv("msg_type = 0x%x", msg_type);

    AutoMutex lock(&mLock);
    mMessageEnabler &= ~msg_type;
    alogv("**** Currently enabled messages:");
}

void CallbackNotifier::getCurrentDateTime(char *buf, int bufsize)
{
    time_t t;
    struct tm *tm_t;
    time(&t);
    tm_t = localtime(&t);
    snprintf(buf, bufsize, "%4d:%02d:%02d %02d:%02d:%02d",
        tm_t->tm_year+1900, tm_t->tm_mon+1, tm_t->tm_mday,
        tm_t->tm_hour, tm_t->tm_min, tm_t->tm_sec);
}

void CallbackNotifier::setGPSMethod(const char *gpsMethod)
{
    if (mpGpsProcessingMethod != NULL) {
        free(mpGpsProcessingMethod);
    }
    if (gpsMethod == NULL) {
        mpGpsProcessingMethod = NULL;
    } else {
        mpGpsProcessingMethod = strdup(gpsMethod);
    }
}
/*
void CallbackNotifier::setSaveFolderPath(const char *str)
{
    if (mpFolderPath != NULL) {
        free(mpFolderPath);
    }
    if (str == NULL) {
        mpFolderPath = NULL;
    } else {
        mpFolderPath = strdup(str);
    }
}

void CallbackNotifier::setSnapPath(const char *str)
{
    if (mpSnapPath != NULL) {
        free(mpSnapPath);
    }
    if (str == NULL) {
        mpSnapPath = NULL;
    } else {
        mpSnapPath = strdup(str);
    }
}
*/
/*
void CallbackNotifier::postDataCompleted(const void *pData, int size)
{
    AutoMutex lock(mPicBufLock);
    int bFind = 0;
    std::list<PictureBuffer>::iterator DstIt;
    for(std::list<PictureBuffer>::iterator it=mWaitReleasePicBufList.begin(); it!=mWaitReleasePicBufList.end(); ++it)
    {
        if(it->mpData == pData)
        {
            if(it->mDataSize != (size_t)size)
            {
                aloge("fatal error! release PictureBuffer's dataSize is not match[%d]!=[%d]", it->mDataSize, size);
            }
            if(0 == bFind)
            {
                DstIt = it;
                delete[] (const char*)pData;
            }
            else
            {
                aloge("fatal error! repeat PictureBuffer in list! check code!");
            }
            bFind++;
        }
    }
    if(bFind > 0)
    {
        mWaitReleasePicBufList.erase(DstIt);
    }
}
*/

void CallbackNotifier::setExifMake(char *make, int size)
{
}

void CallbackNotifier::setExifModel(char *model, int size)
{
}

void CallbackNotifier::setExifCameraSerialNum(char *model, int size)
{
}

bool CallbackNotifier::takePicture(const void *frame,
                    bool isContinuous,
                    bool isThumbnailPic,
                    const struct isp_exif_attribute* pIspExif)
{
    bool bRet = true;
    VIDEO_FRAME_BUFFER_S *pbuf = (VIDEO_FRAME_BUFFER_S*)frame;
    PIXEL_FORMAT_E src_format = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    unsigned char *src_addr_phyY = NULL;
    unsigned char *src_addr_phyC = NULL;
    unsigned int src_width = 0;
    unsigned int src_height = 0;
    unsigned int picWidth = 0, picHeight = 0;
    int thumbnailWidth, thumbnailHeight;
    int jpegQuality, thumbnailQuality;
    int msgType = CAMERA_MSG_COMPRESSED_IMAGE;
    size_t jpegsize = 0, bufsize = 0;
    off_t thumboffset = 0;
    size_t thumblen = 0;
    status_t ret;
    PictureBuffer buf;

    if (isMessageEnabled(CAMERA_MSG_RAW_IMAGE) && (!isThumbnailPic))
    {
        msgType = CAMERA_MSG_RAW_IMAGE;
        VideoFrameBufferSizeInfo FrameSizeInfo;
        getVideoFrameBufferSizeInfo(&pbuf->mFrameBuf, &FrameSizeInfo);
        int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
        bufsize = FrameSizeInfo.mYSize + FrameSizeInfo.mUSize + FrameSizeInfo.mVSize;
        alogd("takePicture: Raw size %dx%d", bufsize);

        buf.mbSharedMemPrepared = true;
        buf.mMsgType      = msgType;
        buf.mIsContinuous = true;
        buf.mPicBuf       = std::make_shared<CMediaMemory>(bufsize);
        buf.mDataSize     = bufsize;

        char *p = (char*)buf.mPicBuf->getPointer();
        for (int i=0; i<3; i++)
        {
            if(pbuf->mFrameBuf.VFrame.mpVirAddr[i] != NULL)
            {
                memcpy(p , pbuf->mFrameBuf.VFrame.mpVirAddr[i], yuvSize[i]);
                p += yuvSize[i];
                alogv("virAddr[%d]=[%p], length=[%d]", i, pbuf->mFrameBuf.VFrame.mpVirAddr[i], yuvSize[i]);
            }
        }

        mPicBufLock.lock();
        mPicBufList.push_back(buf);
        mpSavePicThread->notifyNewPictureCome();
        mPicBufLock.unlock();
        return true;
    }

    if (isThumbnailPic)
    {
        if (!isMessageEnabled(CAMERA_MSG_POSTVIEW_FRAME)) 
        {
            return false;
        }
        picWidth = mPictureThumbWidth;
        picHeight = mPictureThumbHeight;
        jpegQuality = mPictureThumbQuality;
        thumbnailWidth = 0;
        thumbnailHeight = 0;
        thumbnailQuality = PictureThumbQuality_DEFAULT;
        msgType = CAMERA_MSG_POSTVIEW_FRAME;
    }
    else
    {
        if (!isMessageEnabled(CAMERA_MSG_COMPRESSED_IMAGE)) 
        {
            return false;
        }
        picWidth = mPictureWidth;
        picHeight = mPictureHeight;
        jpegQuality = mJpegQuality;
        thumbnailWidth = mPictureThumbWidth;
        thumbnailHeight = mPictureThumbHeight;
        if(mPictureThumbQuality < 20 || mPictureThumbQuality > 100)
        {
            aloge("the picture thumbnailQuality must be in [20, 100],and it will be set as default");
            mPictureThumbQuality = PictureThumbQuality_DEFAULT;
        }
        thumbnailQuality = mPictureThumbQuality;
                
        msgType = CAMERA_MSG_COMPRESSED_IMAGE;
    }

//    if ((pbuf->mIsThumbAvailable == 1) && (pbuf->mThumbUsedForPhoto == 1 || isThumbnailPic))
//    {
//        aloge("can't take thumb nail picture!");
//        return false;
//    }
//    else
    {
        src_format          = pbuf->mFrameBuf.VFrame.mPixelFormat;
        src_addr_phyY       = (unsigned char*)pbuf->mFrameBuf.VFrame.mPhyAddr[0];
        src_addr_phyC       = (unsigned char*)pbuf->mFrameBuf.VFrame.mPhyAddr[1];
        src_width           = pbuf->mFrameBuf.VFrame.mWidth;
        src_height          = pbuf->mFrameBuf.VFrame.mHeight;
    }

    if(MM_PIXEL_FORMAT_BUTT == src_format)
    {
        alogd("frame buf may contain compressed frame, need not encode.");
        jpegsize = pbuf->mFrameBuf.VFrame.mStride[0];
        bufsize = jpegsize + sizeof(off_t) + sizeof(size_t) + sizeof(size_t);
        thumboffset = 0;
        thumblen = 0;
        
        buf.mbSharedMemPrepared = true;
        buf.mMsgType = msgType;
        buf.mIsContinuous = isContinuous;
        buf.mPicBuf = std::make_shared<CMediaMemory>(bufsize);
        buf.mDataSize = bufsize;
        memcpy(buf.mPicBuf->getPointer(), pbuf->mFrameBuf.VFrame.mpVirAddr[0], jpegsize);
        char *p = (char*)buf.mPicBuf->getPointer() + jpegsize;
        memcpy(p, &thumboffset, sizeof(off_t));
        p += sizeof(off_t);
        memcpy(p, &thumblen, sizeof(size_t));
        p += sizeof(size_t);
        memcpy(p, &jpegsize, sizeof(size_t));

        mPicBufLock.lock();
        mPicBufList.push_back(buf);
        mpSavePicThread->notifyNewPictureCome();
        mPicBufLock.unlock();
        goto JPEG_INIT_ERR;
    }
    if(mpJpegenc)
    {
        if(mJpegEncConfig.mSourceWidth != src_width || mJpegEncConfig.mSourceHeight != src_height
            || mJpegEncConfig.mPicWidth != picWidth || mJpegEncConfig.mPicHeight != picHeight)
        {
            alogd("srcSize-dstSize change [%dx%d,%dx%d]->[%dx%d,%dx%d], need reset encoder", 
                mJpegEncConfig.mSourceWidth, mJpegEncConfig.mSourceHeight, mJpegEncConfig.mPicWidth, mJpegEncConfig.mPicHeight,
                src_width, src_height, picWidth, picHeight);
            mpJpegenc->destroy();
            delete mpJpegenc;
            mpJpegenc = NULL;
        }
    }
    if(NULL == mpJpegenc)
    {
        memset(&mJpegEncConfig, 0, sizeof(CameraJpegEncConfig));
        mJpegEncConfig.mPixelFormat = src_format;
        mJpegEncConfig.mSourceWidth = src_width;
        mJpegEncConfig.mSourceHeight = src_height;
        mJpegEncConfig.mPicWidth = picWidth;
        mJpegEncConfig.mPicHeight = picHeight;
        mJpegEncConfig.mThumbnailWidth = thumbnailWidth;
        mJpegEncConfig.mThumbnailHeight = thumbnailHeight;
        mJpegEncConfig.mThunbnailQuality = thumbnailQuality; 
        mJpegEncConfig.mQuality = jpegQuality;
        //ion_memGetTotalSize();
        unsigned int minVbvBufSize = picWidth * picHeight * 3/2;
        unsigned int vbvThreshSize = picWidth*picHeight;
        unsigned int vbvBufSize = (picWidth * picHeight * 3/2 /10 * mPictureNum) + vbvThreshSize;
        if(vbvBufSize < minVbvBufSize)
        {
            vbvBufSize = minVbvBufSize;
        }
        if(vbvBufSize > 16*1024*1024)
        {
            alogd("Be careful! vbvSize[%d]MB is too large, decrease to threshSize[%d]MB + 1MB", vbvBufSize/(1024*1024), vbvThreshSize/(1024*1024));
            vbvBufSize = vbvThreshSize + 1*1024*1024;
        }
        mJpegEncConfig.nVbvBufferSize = ALIGN(vbvBufSize, 1024);
        mJpegEncConfig.nVbvThreshSize = vbvThreshSize;
        mpJpegenc = new CameraJpegEncoder();
        ret = mpJpegenc->initialize(&mJpegEncConfig);
        if (ret != NO_ERROR) 
        {
            aloge("CameraJpegEncoder initialize error!");
    		goto JPEG_INIT_ERR;
        }
    }
    alogd("takePicture: Picture size %dx%d, Thumb size %dx%d, vbv buffer size %u", picWidth, picHeight, thumbnailWidth, thumbnailHeight, mJpegEncConfig.nVbvBufferSize);
    //every picture need set exifInfo.
    VENC_EXIFINFO_S stExifInfo;
    memset(&stExifInfo, 0, sizeof(VENC_EXIFINFO_S));
    setExifMake((char*)stExifInfo.CameraMake, MM_INFO_LENGTH);
    setExifModel((char*)stExifInfo.CameraModel, MM_INFO_LENGTH);
    getCurrentDateTime((char*)stExifInfo.DateTime, MM_DATA_TIME_LENGTH);
    stExifInfo.ThumbWidth = thumbnailWidth;
    stExifInfo.ThumbHeight = thumbnailHeight;
    stExifInfo.thumb_quality = thumbnailQuality;
    stExifInfo.Orientation = mJpegRotate;
//    if (pIspExif != NULL) 
//    {
//        stExifInfo.fr32ExposureTime = FRACTION32(pIspExif->exposure_time.denominator, pIspExif->exposure_time.numerator);
//    }
    stExifInfo.fr32FNumber = FRACTION32(10, 26);
    if(pIspExif != NULL)
    {
        stExifInfo.ISOSpeed = (short)pIspExif->iso_speed;
        stExifInfo.ExposureBiasValueNum = pIspExif->exposure_bias;
    }
    stExifInfo.MeteringMode = AVERAGE_AW_EXIF;
    stExifInfo.fr32FocalLength = FRACTION32(100, 228);
    stExifInfo.WhiteBalance = 0;
    if (mpGpsProcessingMethod != NULL) 
    {
        stExifInfo.enableGpsInfo = 1;
        stExifInfo.gps_latitude = mGpsLatitude;
        stExifInfo.gps_longitude = mGpsLongitude;
        stExifInfo.gps_altitude = mGpsAltitude;
        stExifInfo.gps_timestamp = mGpsTimestamp;
        strcpy((char*)stExifInfo.gpsProcessingMethod, mpGpsProcessingMethod);
    } 
    setExifCameraSerialNum((char*)stExifInfo.CameraSerialNum, MM_INFO_LENGTH);
    stExifInfo.FocalLengthIn35mmFilm = 18;
    strcpy((char*)stExifInfo.ImageName, "aw-photo");
//    strcpy((char*)stExifInfo.ImageDescription, "This photo is taken by AllWinner");   // not need any more

    if(!isThumbnailPic)
    {
        createPictureRegion();
    }
    
    ret = mpJpegenc->encode(&pbuf->mFrameBuf, &stExifInfo);

    if(!isThumbnailPic)
    {
        destoryPictureRegion();
    }
    
    if (ret != NO_ERROR) 
    {
        aloge("CameraJpegEncoder encode error!");
        goto JPEG_ENCODE_ERR;
    }
    if (mpJpegenc->getFrame() != 0) 
    {
        aloge("CameraJpegEncoder getFrame error!");
        goto JPEG_GET_FRAME_ERR;
    }
    mpJpegenc->getThumbOffset(thumboffset, thumblen);

    buf.mbSharedMemPrepared = false;
    buf.mMsgType = msgType;
    buf.mIsContinuous = isContinuous;
    buf.mpData0 = mpJpegenc->mOutStream.mpPack[0].mpAddr0;
    buf.mpData1 = mpJpegenc->mOutStream.mpPack[0].mpAddr1;
    buf.mpData2 = mpJpegenc->mOutStream.mpPack[0].mpAddr2;
    buf.mLen0 = mpJpegenc->mOutStream.mpPack[0].mLen0;
    buf.mLen1 = mpJpegenc->mOutStream.mpPack[0].mLen1;
    buf.mLen2 = mpJpegenc->mOutStream.mpPack[0].mLen2;
    buf.mThumbOffset = thumboffset;
    buf.mThumbLen = thumblen;
    buf.mDataSize = buf.mLen0+buf.mLen1+buf.mLen2;
    
    mPicBufLock.lock();
    mPicBufList.push_back(buf);
    mpSavePicThread->notifyNewPictureCome();
    mPicBufLock.unlock();
    

    //mpJpegenc->returnFrame();
    //pJpegenc->destroy();
    //delete pJpegenc;

    alogv("take one photo end");
    return bRet;

JPEG_GET_FRAME_ERR:
JPEG_ENCODE_ERR:
    mpJpegenc->destroy();
JPEG_INIT_ERR:
    if(mpJpegenc)
    {
        delete mpJpegenc;
        mpJpegenc = NULL;
    }
    return bRet;
}

void CallbackNotifier::takePictureEnd()
{
    int64_t nWaitInterval = 200;   //unit:ms
    status_t ret;
    mPicBufLock.lock();
    while(!mPicBufList.empty())
    {
        alogw("Be careful! Before destroy jpegEnc, PicBufList has [%d] pictures to send!", mPicBufList.size());
        mbWaitPicBufListEmpty = true;
        ret = mCondWaitPicBufListEmpty.waitRelative(mPicBufLock, nWaitInterval*1000*1000);
        if(ret != NO_ERROR)
        {
            alogw("wait ret[0x%x], wait [%lld]ms, still has some pictures to send!", ret, nWaitInterval);
        }
    }
    mPicBufLock.unlock();
    if(mpJpegenc)
    {
        mpJpegenc->destroy();
        delete mpJpegenc;
        mpJpegenc = NULL;
    }
    alogv("take picture end.");
}
void CallbackNotifier::setPictureRegionCallback(PictureRegionCallback *pCb)
{
    AutoMutex lock(mPictureRegionLock);
    mpPictureRegionCallback = pCb;
}

void CallbackNotifier::createPictureRegion()
{
    //status_t ret = NO_ERROR;
    AutoMutex lock(mPictureRegionLock);

    if(!mpPictureRegionCallback)
    {
        return ;
    }

    mpPictureRegionCallback->addPictureRegion(mPictureRegionList);
    
    if(!mPictureRegionList.empty())
    {
        alogd("had %d picture regions input!" , mPictureRegionList.size());
        mPictureRegionHandleList.clear();
        
        int regionId = 0; //just for debug info
        MPP_CHN_S PictureVeChn = {MOD_ID_VENC, 0, mpJpegenc->getJpegVenChn()};

        for(std::list<PictureRegionType>::iterator it = mPictureRegionList.begin(); it != mPictureRegionList.end(); ++it, ++regionId )
        {
            RGN_ATTR_S stRegion;
            RGN_CHN_ATTR_S stRgnChnAttr;

            if(OVERLAY_RGN == it->mType)
            {
                memset(&stRegion, 0, sizeof(RGN_ATTR_S));
                stRegion.enType = OVERLAY_RGN;

                if(MM_PIXEL_FORMAT_RGB_8888 == it->mInfo.mOverlay.mPixFmt || MM_PIXEL_FORMAT_RGB_1555 == it->mInfo.mOverlay.mPixFmt)
                {
                    
                   stRegion.unAttr.stOverlay.mPixelFmt = it->mInfo.mOverlay.mPixFmt;//??
                }
                else
                {
                    aloge("fatal error! [ %d ] PictureRegion had error PixFmt ,and it will be ignored!", regionId);
                    mPictureRegionHandleList.push_back(RGN_HANDLE_MAX); // invaild handle.
                    continue;
                }
                
                stRegion.unAttr.stOverlay.mSize.Width = it->mRect.Width;
                stRegion.unAttr.stOverlay.mSize.Height = it->mRect.Height;
                
            }
            else if(COVER_RGN == it->mType )
            {
                memset(&stRegion, 0, sizeof(RGN_ATTR_S));
                stRegion.enType = COVER_RGN;
            }
            else
            {
                aloge("fatal error! [ %d ] PictureRegion had error Type , and it will be ignored!", regionId);
                mPictureRegionHandleList.push_back(RGN_HANDLE_MAX); // invaild handle.
                continue;     
            }
            
            bool bSuccess = false;
            int handle = 0;
            while(handle < RGN_HANDLE_MAX)
            {             
                ERRORTYPE Ret = AW_MPI_RGN_Create(handle, &stRegion);
                if(SUCCESS == Ret)
                {
                    alogd("[ %d ] PictureRegion had create, the region handle is %d", regionId, handle);
                    bSuccess = true;
                    mPictureRegionHandleList.push_back(handle);
                    break;   
                }
                else if(ERR_RGN_EXIST == Ret)
                {
                    //alogv("region[%d] is exist, find next!", handle);            
                    handle++;
                }
                else
                {
                    aloge("fatal error! create [ %d ] PictureRegion,  ret[0x%x]!", regionId, Ret);
                    bSuccess = false;
                    mPictureRegionHandleList.push_back(RGN_HANDLE_MAX); // invaild handle.
                    break;
                }   
            }
            
            if(bSuccess)
            {           
                memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
                if(OVERLAY_RGN == stRegion.enType)
                {
                    BITMAP_S stBmp;        
                    memset(&stBmp, 0, sizeof(BITMAP_S));
                    stBmp.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
                    stBmp.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
                    stBmp.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
                    stBmp.mpData = it->mInfo.mOverlay.mBitmap;
                    if(AW_MPI_RGN_SetBitMap(handle, &stBmp) != SUCCESS)
                    {
                        aloge("fatal error! create [ %d ] PictureRegion will be ignored!", regionId);
                        AW_MPI_RGN_Destroy(handle);
                        mPictureRegionHandleList.pop_back();
                        mPictureRegionHandleList.push_back(RGN_HANDLE_MAX); 
                        continue;
                    }
                    free(stBmp.mpData);
                    it->mInfo.mOverlay.mBitmap = NULL;
                    stBmp.mpData = NULL;

                    stRgnChnAttr.bShow = TRUE;
                    stRgnChnAttr.enType = stRegion.enType;
                    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = it->mRect.X;
                    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = it->mRect.Y;
                    stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = it->mInfo.mOverlay.mPriority;
					stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
                    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
                    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
                    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
                    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TRUE;

                    if(it->mInfo.mOverlay.mbInvColEn)
                    {
                        aloge("Sorry, This version not support color invert in [ %d ] PictureRegion ", regionId);
                    }

                    if(AW_MPI_RGN_AttachToChn(handle, &PictureVeChn, &stRgnChnAttr) != SUCCESS)
                    {
                        aloge("fatal error! create [ %d ] PictureRegion will be ignored!", regionId);
                        AW_MPI_RGN_Destroy(handle);
                        mPictureRegionHandleList.pop_back();
                        mPictureRegionHandleList.push_back(RGN_HANDLE_MAX); 
                        continue;          
                     }        
                }
                else if(COVER_RGN == stRegion.enType)
                {
                    stRgnChnAttr.bShow = TRUE;
                    stRgnChnAttr.enType = stRegion.enType;
                    stRgnChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
                    stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = it->mRect.X;
                    stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = it->mRect.Y;
                    stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width = it->mRect.Width;
                    stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height = it->mRect.Height;
                    stRgnChnAttr.unChnAttr.stCoverChn.mColor = it->mInfo.mCover.mChromaKey;
                    stRgnChnAttr.unChnAttr.stCoverChn.mLayer = it->mInfo.mCover.mPriority;

                    if(AW_MPI_RGN_AttachToChn(handle, &PictureVeChn, &stRgnChnAttr) != SUCCESS)
                    {
                        aloge("fatal error! create [ %d ] PictureRegion will be ignored!", regionId);
                        AW_MPI_RGN_Destroy(handle);
                        mPictureRegionHandleList.pop_back();
                        mPictureRegionHandleList.push_back(RGN_HANDLE_MAX); 
                        continue;                        
                    }                    
                }    
            }
        }
        alogd("had %d picture region work!" , mPictureRegionHandleList.size());
    }
}

void CallbackNotifier::destoryPictureRegion()
{
    AutoMutex lock(mPictureRegionLock);

    if(!mPictureRegionHandleList.empty())
    {
        MPP_CHN_S PictureVeChn = {MOD_ID_VENC, 0, mpJpegenc->getJpegVenChn()};
        for(std::list<RGN_HANDLE>::iterator it = mPictureRegionHandleList.begin(); it != mPictureRegionHandleList.end(); ++it)
        {
            if(*it < RGN_HANDLE_MAX)
            {
                AW_MPI_RGN_DetachFromChn(*it, &PictureVeChn);
                AW_MPI_RGN_Destroy(*it);                
            }
        }
        mPictureRegionHandleList.clear();
        alogd("the pictureregionhandlelist had clear!");
    }

    if(!mPictureRegionList.empty())
    {
        for(std::list<PictureRegionType>::iterator it = mPictureRegionList.begin(); it != mPictureRegionList.end(); ++it)
        {
            if(OVERLAY_RGN == it->mType)
            {
                if(it->mInfo.mOverlay.mBitmap)
                {
                    free(it->mInfo.mOverlay.mBitmap);
                    it->mInfo.mOverlay.mBitmap = NULL;
                }
            }
        }
        mPictureRegionList.clear();
        alogd("the pictureregionlist had clear!");
    }

    //alogd("the pictureregionlist and pictureregionhandlelist had clear!!!");
    
}

status_t CallbackNotifier::notifyPictureRelease()
{
    mPicBufLock.lock();
    if(!mWaitReleasePicBufList.empty())
    {
        //alogd("notify waitReleasePicBufList to pop!");
        mWaitReleasePicBufList.pop_front();
    }
    else
    {
        aloge("fatal error! why waitReleasePicBufList is empty?");
    }
    mpSavePicThread->notifyPictureRelease();
    mPicBufLock.unlock();
    return NO_ERROR;
}

bool CallbackNotifier::savePictureThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    PROCESS_MESSAGE:
    getMsgRet = mpSavePicThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoSavePictureThread::MsgTypeSavePic_InputPictureAvailable == msg.mMsgType)
        {
        }
        else if(DoSavePictureThread::MsgTypeSavePic_ReleasePicture == msg.mMsgType)
        {
        }
        else if(DoSavePictureThread::MsgTypeSavePic_Exit == msg.mMsgType)
        {
            bRunningFlag = false; 
            alogw("save_pic_thrd_force_rls_when_exit:%d",mPicBufList.size());
            mPicBufLock.lock();
            while(!mPicBufList.empty())
            { 
                VENC_STREAM_S tmpVencStream;
                VENC_PACK_S tmpVencPack;
                memset(&tmpVencStream, 0, sizeof(VENC_STREAM_S));
                memset(&tmpVencPack, 0, sizeof(VENC_PACK_S));
                
                PictureBuffer pbuf = mPicBufList.front();
                mPicBufList.pop_front();
                tmpVencStream.mpPack = &tmpVencPack;
                tmpVencStream.mPackCount = 1;
                tmpVencStream.mpPack[0].mpAddr0 = pbuf.mpData0;
                tmpVencStream.mpPack[0].mpAddr1 = pbuf.mpData1;
                tmpVencStream.mpPack[0].mpAddr2 = pbuf.mpData2;
                tmpVencStream.mpPack[0].mLen0 = pbuf.mLen0;
                tmpVencStream.mpPack[0].mLen1 = pbuf.mLen1;
                tmpVencStream.mpPack[0].mLen2 = pbuf.mLen2;
                mpJpegenc->returnFrame(&tmpVencStream);
            }
            mPicBufLock.unlock();
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }

    while(1)
    {
        mPicBufLock.lock();
        if (mPicBufList.empty()) 
        {
            alogv("wait for picture to save");
            mpSavePicThread->mWaitLock.lock();
            mpSavePicThread->mbWaitPicture = true;
            mpSavePicThread->mWaitLock.unlock();
            if(mbWaitPicBufListEmpty)
            {
                alogd("need notify picture thread that all pictures have been sent.");
                mbWaitPicBufListEmpty = false;
                mCondWaitPicBufListEmpty.signal();
            }
            mPicBufLock.unlock();
            mpSavePicThread->mMsgQueue.waitMessage();
            goto PROCESS_MESSAGE;
        }
        if(mWaitReleasePicBufList.size() > 0)
        {
            alogd("wait user to notify to release PicBuffer");
            mpSavePicThread->mWaitLock.lock();
            mpSavePicThread->mbWaitReleasePicture = true;
            mpSavePicThread->mWaitLock.unlock();
            mPicBufLock.unlock();
            mpSavePicThread->mMsgQueue.waitMessage();
            goto PROCESS_MESSAGE;
        }
        mWaitReleasePicBufList.splice(mWaitReleasePicBufList.end(), mPicBufList, mPicBufList.begin());
        PictureBuffer *pbuf = &mWaitReleasePicBufList.back();
        size_t bufsize = pbuf->mDataSize;
        if(false == pbuf->mbSharedMemPrepared)
        {
            size_t jpegsize = pbuf->mDataSize;
            bufsize = jpegsize + sizeof(off_t) + sizeof(size_t) + sizeof(size_t);

//            int freeMemKb = getFreeMemory();
//            int buffersMemKb = getBuffersMemory();
//            int cachedMemKb = getCachedMemory();
//            if ((unsigned int)(freeMemKb/*+buffersMemKb+cachedMemKb*/) < (bufsize+2*1024*1024)>>10) 
//            {
//                alogw("Be careful! Free memory too small! bufsize=%dKB, MemTotal=%dKB, MemFree=%dKB, Buffers=%dKB, Cached=%dKB",
//                    bufsize/1024, getTotalMemory(), freeMemKb, buffersMemKb, cachedMemKb);
//                //goto ALLOC_MEM_ERR;
//            }
            pbuf->mPicBuf = std::make_shared<CMediaMemory>(bufsize);
            char *p = (char*)pbuf->mPicBuf->getPointer();
            if(NULL == p)
            {
                aloge("fatal error! malloc fail!");
            }
            if(pbuf->mLen0 > 0)
            {
                memcpy(p, pbuf->mpData0, pbuf->mLen0);
                p += pbuf->mLen0;
            }
            if(pbuf->mLen1 > 0)
            {
                memcpy(p, pbuf->mpData1, pbuf->mLen1);
                p += pbuf->mLen1;
            }
            if(pbuf->mLen2 > 0)
            {
                memcpy(p, pbuf->mpData2, pbuf->mLen2);
                p += pbuf->mLen2;
            }
            off_t thumboffset = pbuf->mThumbOffset;
            size_t thumblen = pbuf->mThumbLen;
            memcpy(p, &thumboffset, sizeof(off_t));
            p += sizeof(off_t);
            memcpy(p, &thumblen, sizeof(size_t));
            p += sizeof(size_t);
            memcpy(p, &jpegsize, sizeof(size_t));
            //after finish sharedMem, return venc frame now.
            VENC_STREAM_S tmpVencStream;
            VENC_PACK_S tmpVencPack;
            memset(&tmpVencStream, 0, sizeof(VENC_STREAM_S));
            memset(&tmpVencPack, 0, sizeof(VENC_PACK_S));
            tmpVencStream.mpPack = &tmpVencPack;
            tmpVencStream.mPackCount = 1;
            tmpVencStream.mpPack[0].mpAddr0 = pbuf->mpData0;
            tmpVencStream.mpPack[0].mpAddr1 = pbuf->mpData1;
            tmpVencStream.mpPack[0].mpAddr2 = pbuf->mpData2;
            tmpVencStream.mpPack[0].mLen0 = pbuf->mLen0;
            tmpVencStream.mpPack[0].mLen1 = pbuf->mLen1;
            tmpVencStream.mpPack[0].mLen2 = pbuf->mLen2;
            mpJpegenc->returnFrame(&tmpVencStream);
        }
        mPicBufLock.unlock();
        //mpDataCallback->postData(pbuf->mMsgType, mMppChnInfo.mChnId, pbuf->mpData, pbuf->mDataSize);
        mpDataCallback->postData(pbuf->mMsgType, mChnId, pbuf->mPicBuf, bufsize);
        //mWaitReleasePicBufList.pop_front();
    }
    return true;

_exit0:
    return bRunningFlag;
}

}; /* namespace EyeseeLinux */
