/*
********************************************************************************
*                           eyesee framework module
*
*          (c) Copyright 2010-2018, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : EyeseeUSBCamera.cpp
* Version: V1.0
* By     : 
* Date   : 2018-01-11
* Description:
********************************************************************************
*/
#include <plat_log.h>

#include <mpi_uvc.h>
#include <uvcInput.h>
#include <mpi_vdec.h>
#include <mpi_sys.h>

#include <cmath>

#include <mpi_videoformat_conversion.h>
#include "EyeseeUSBCamera.h"
#include "VdecFrameManager.h"
#include "UvcChannel.h"

namespace EyeseeLinux {

int EyeseeUSBCamera::mCameraCounter = 0;

class UvcChannel;

static bool JudgeCompressedCaptureFormat(UVC_CAPTURE_FORMAT format)
{
    bool bCompressed = false;   //if capture format is compressed, e.g mjpeg, h264, set true;
    switch(format)
    {
        case UVC_YUY2:
        case UVC_NV12:
            bCompressed = false;
            break;
        case UVC_H264:
        case UVC_MJPEG:
            bCompressed = true;
            break;
        default:
            aloge("fatal error! unknown capture format[0x%x]!", format);
            bCompressed = false;
            break;
    }
    return bCompressed;
}

static int decideMaxScaleRatio(int maxRatio, PAYLOAD_TYPE_E eCodecFormat)
{
#if (defined(MPPCFG_CHIP_AW1721) || defined(MPPCFG_CHIP_AW1816) || defined(MPPCFG_CHIP_AW1886))
    switch(eCodecFormat)
    {
        case PT_H264:
        case PT_H265:
        {
            if (maxRatio > 2) 
            {
                alogd("limit ScaleDownRatio[%d]->[2] of CodecFormat[0x%x]", maxRatio, eCodecFormat);
                maxRatio = 2;
            }
            break;
        }
        case PT_JPEG:
        case PT_MJPEG:
        {
            if (maxRatio > 5) 
            {
                alogd("limit ScaleDownRatio[%d]->[5] of CodecFormat[0x%x]", maxRatio, eCodecFormat);
                maxRatio = 5;
            }
            break;
        }
        default:
        {
            aloge("fatal error! unknown CodecFormat[0x%x]", eCodecFormat);
            if (maxRatio > 2) 
            {
                alogd("limit ScaleDownRatio[%d]->[2] of CodecFormat[0x%x]", maxRatio, eCodecFormat);
                maxRatio = 2;
            }
            break;
        }
    }
#else
    aloge("fatal error! unknown chip");
    if (maxRatio > 2) 
    {
        alogd("limit ScaleDownRatio[%d]->[2] of CodecFormat[0x%x]", maxRatio, eCodecFormat);
        maxRatio = 2;
    }
#endif
    return maxRatio;
}

class EyeseeUSBCameraContext : public DataListener
                                    , public NotifyListener
{
public:
    EyeseeUSBCameraContext(EyeseeUSBCamera *pCamera);
    ~EyeseeUSBCameraContext();

    virtual void postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize);
    virtual void notify(int msgType, int chnId, int ext1, int ext2);

private:
    EyeseeUSBCamera *mpCamera;
    Mutex mLock;
};

EyeseeUSBCameraContext::EyeseeUSBCameraContext(EyeseeUSBCamera *pCamera)
    : mpCamera(pCamera)
{
}

EyeseeUSBCameraContext::~EyeseeUSBCameraContext()
{
}

void EyeseeUSBCameraContext::postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize)
{
    EyeseeUSBCamera::postEventFromNative(mpCamera, msgType, chnId, (int)dataSize, 0, &dataPtr);
}

void EyeseeUSBCameraContext::notify(int msgType, int chnId, int ext1, int ext2)
{
    EyeseeUSBCamera::postEventFromNative(mpCamera, msgType, chnId, ext1, ext2, NULL);
}

void EyeseeUSBCamera::postEventFromNative(EyeseeUSBCamera *pCam, int what, int arg1, int arg2, int arg3, const std::shared_ptr<CMediaMemory>* pDataPtr)
{
    if (pCam == NULL)
    {
        aloge("fatal error! pCam == NULL");
        return;
    }
    CallbackMessage msg;

    msg.what = what;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    msg.extArgs.push_back(arg3);
    //msg.obj = obj;
    if(pDataPtr)
    {
        msg.mDataPtr = std::const_pointer_cast<const CMediaMemory>(*pDataPtr);
    }
    pCam->mpEventHandler->post(msg);
}

EyeseeUSBCamera::EventHandler::EventHandler(EyeseeUSBCamera *pCam)
    : mpCamera(pCam)
{
}

void EyeseeUSBCamera::EventHandler::handleMessage(const CallbackMessage &msg)
{
    switch (msg.what) 
    {
        case CAMERA_MSG_COMPRESSED_IMAGE:
        {
            AutoMutex lock(mpCamera->mSetTakePictureCallbackLock);
            UvcChn chnId = (UvcChn)msg.arg1;
            std::map<UvcChn, EyeseeUSBCamera::TakePictureCallback>::iterator it = mpCamera->mTakePictureCallback.find(chnId);
            if(it == mpCamera->mTakePictureCallback.end())
            {
                aloge("fatal error, why not find chnId[%d]", chnId);
                return;
            }
            
            if (it->second.mpJpegCallback) 
            {
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL) 
                    {
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        it->second.mpJpegCallback->onPictureTaken(chnId, pData, nSize, mpCamera);
                    }
                }
            }
            return;
        }
        case CAMERA_MSG_POSTVIEW_FRAME:
        {
            AutoMutex lock(mpCamera->mSetTakePictureCallbackLock);
            UvcChn chnId = (UvcChn)msg.arg1;
            std::map<UvcChn, EyeseeUSBCamera::TakePictureCallback>::iterator it = mpCamera->mTakePictureCallback.find(chnId);
            if(it == mpCamera->mTakePictureCallback.end())
            {
                aloge("fatal error, why not find chnId[%d]", chnId);
                return;
            }
            
            if (it->second.mpPostviewCallback) 
            {
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL) 
                    {
                        //int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        it->second.mpPostviewCallback->onPictureTaken(chnId, pData, nSize, mpCamera);
                    }
                }
            }
            return;
        }
        case CAMERA_MSG_ERROR:
            aloge("Channel %d error %d", msg.arg1, msg.arg2);
            if (mpCamera->mpErrorCallback != NULL) 
            {
                mpCamera->mpErrorCallback->onError((UvcChn)msg.arg1, (CameraMsgErrorType)msg.arg2, mpCamera);
            }
            return;
        case CAMERA_MSG_INFO:
        {
            UvcChn chnId = (UvcChn)msg.arg1;
            CameraMsgInfoType what = (CameraMsgInfoType)msg.arg2;
            int extra = 0;
            if(msg.extArgs.size() > 0)
            {
                extra = msg.extArgs[0];
            }
            if (mpCamera->mpInfoCallback != NULL) 
            {
                mpCamera->mpInfoCallback->onInfo(chnId, what, extra, mpCamera);
            }
            return;
        }
        default:
        {
            aloge("Channel %d Unknown message type %d", msg.arg1, msg.what);
            return;
        }
    }
}

EyeseeUSBCamera::USBCameraProxy::USBCameraProxy(EyeseeUSBCamera *pUSBCameraParam)
{
    mpUSBCamera = pUSBCameraParam;
}
status_t EyeseeUSBCamera::USBCameraProxy::startRecording(int chnId, CameraRecordingProxyListener *pListener, int recorderId)
{
    return mpUSBCamera->startRecording((UvcChn)chnId, pListener, recorderId);
}
status_t EyeseeUSBCamera::USBCameraProxy::stopRecording(int chnId, int recorderId)
{
    return mpUSBCamera->stopRecording((UvcChn)chnId, recorderId);
}
void EyeseeUSBCamera::USBCameraProxy::releaseRecordingFrame(int chnId, uint32_t frameIndex)
{
    mpUSBCamera->releaseRecordingFrame((UvcChn)chnId, frameIndex);
}
status_t EyeseeUSBCamera::USBCameraProxy::setChannelDisplay(int chnId, int hlay)
{
    aloge("this is what, is funy???");
    return UNKNOWN_ERROR;
}
status_t EyeseeUSBCamera::USBCameraProxy::getParameters(int chnId, CameraParameters &param)
{
    return mpUSBCamera->getParameters((UvcChn)chnId, param);
}
status_t EyeseeUSBCamera::USBCameraProxy::setParameters(int chnId, CameraParameters &param)
{
    return mpUSBCamera->setParameters((UvcChn)chnId, param);
}

EyeseeUSBCamera::EyeseeUSBCamera()
    :mCameraId(mCameraCounter++)
{
    clearConfig();
    mpErrorCallback = NULL;
    mpInfoCallback = NULL;
    mTakePictureCallback.clear();
    mUvcChnList.clear();

    mpEventHandler = new EventHandler(this);
    mpNativeContext = new EyeseeUSBCameraContext(this);
    mCurrentState = State::IDLE;
}
EyeseeUSBCamera::~EyeseeUSBCamera()
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::IDLE)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
    }
    if (mpNativeContext != NULL) 
    {
        delete mpNativeContext;
        mpNativeContext = NULL;
    }
    if(!mUvcChnList.empty())
    {
        aloge("fatal error! uvcChn list is not empty!");
    }
    if (mpEventHandler != NULL) 
    {
        delete mpEventHandler;
        mpEventHandler = NULL;
    }
    
    clearConfig();
}

status_t EyeseeUSBCamera::getUSBCameraCaptureParam(USBCameraCaptureParam &CaptureParam) const
{
    CaptureParam = mCaptureParam;
    return NO_ERROR;
}

status_t EyeseeUSBCamera::setUSBCameraCaptureParam(const USBCameraCaptureParam &CaptureParam)
{
    AutoMutex autoMutex(mLock);
    if(State::IDLE != mCurrentState)
    {
        alogw("Be careful! usb camera param can only be set in idle state!");
        return PERMISSION_DENIED;
    }
    mCaptureParam = CaptureParam;
    mbCaptureFlag = true;
    return NO_ERROR;
}

status_t EyeseeUSBCamera::getUSBCameraVdecParam(USBCameraVdecParam &VdecParam) const
{
    VdecParam = mVdecParam;
    return NO_ERROR;
}

status_t EyeseeUSBCamera::setUSBCameraVdecParam(const USBCameraVdecParam &VdecParam)
{
    AutoMutex autoMutex(mLock);
    if(State::IDLE != mCurrentState)
    {
        alogw("Be careful! usb camera param can only be set in idle state!");
        return PERMISSION_DENIED;
    }
    mVdecParam = VdecParam;
    mbVdecFlag =  true;
    return NO_ERROR;
}

CameraRecordingProxy* EyeseeUSBCamera::getRecordingProxy()
{
    return new USBCameraProxy(this);
}

status_t EyeseeUSBCamera::prepareDevice()
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::IDLE)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    
    if(!mbCaptureFlag)
    {
        aloge("error,you must set capture param!!!");
        return INVALID_OPERATION;
    }

    int Ret = NO_ERROR;
    Ret = openUSBCamera();
    if(Ret != NO_ERROR)
    {
        aloge("fatal error, can not open %s usbcamera, 0x%x", mCaptureParam.mpUsbCam_DevName, Ret);
        return Ret;
    }
    if(mbVdecFlag && JudgeCompressedCaptureFormat(mCaptureParam.mUsbCam_CapPixelformat))
    {
        //create uvc virtual channel.
        mVirtualUvcChnForVdec = 0;
        while(mVirtualUvcChnForVdec < UVC_MAX_CHN_NUM)
        {
            Ret = AW_MPI_UVC_CreateVirChn((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChnForVdec);
            if(SUCCESS == Ret)
            {
                break;
            }
            else if(ERR_UVC_EXIST == Ret)
            {
                mVirtualUvcChnForVdec++;
            }
            else
            {
                aloge("the %s usbcamera fail to create virchn[%d], 0x%x", mUvcDev, mVirtualUvcChnForVdec, Ret);
                mVirtualUvcChnForVdec++;
            }
        }
        if(UVC_MAX_CHN_NUM == mVirtualUvcChnForVdec)
        {
            aloge("fatal error! the %s usbcamera fail to create virchn.", mUvcDev);
            return UNKNOWN_ERROR;
        }
        Ret = openVdecDevice();
        if(Ret != NO_ERROR)
        {
            aloge("fatal error, can not create %s usbcamera vdecchn[%d], 0x%x", mCaptureParam.mpUsbCam_DevName, mVdecChn, Ret);
            return Ret;
        }
        
        MPP_CHN_S UvcChn = {MOD_ID_UVC, (int)mUvcDev.c_str(), mVirtualUvcChnForVdec};
        MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, mVdecChn};
        Ret = AW_MPI_SYS_Bind(&UvcChn, &VdecChn);           
        if(Ret != SUCCESS)
        {
            aloge("can not bind uvcchn[%d] and vdecchn[%d], 0x%x", mVirtualUvcChnForVdec, mVdecChn, Ret);
            return UNKNOWN_ERROR;
        }

        //create vdec frame manager
        mpVdecFrameManager = new VdecFrameManager(mVdecChn, mbVdecSubOutputFlag);
        if(NULL == mpVdecFrameManager)
        {
            aloge("fatal error! malloc fail!");
        }
    }
    
    mCurrentState = State::PREPARED;
    return NO_ERROR;
    
}


status_t EyeseeUSBCamera::startDevice()
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::PREPARED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    int Ret = NO_ERROR;
    if(mbVdecFlag && JudgeCompressedCaptureFormat(mCaptureParam.mUsbCam_CapPixelformat))
    {
        Ret = AW_MPI_VDEC_StartRecvStream(mVdecChn);
        if(Ret != SUCCESS)
        {
            aloge("fatal error! the %s usbcamera can not start vdecchn[%d], 0x%x", mUvcDev, mVdecChn, Ret);
            return UNKNOWN_ERROR;
        }
        Ret = AW_MPI_UVC_StartRecvPic((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChnForVdec);
        if(Ret != SUCCESS)
        {
            aloge("fatal error! the %s usbcamera can not start uvcchn[%d], 0x%x", mUvcDev, mVirtualUvcChnForVdec, Ret);
            return UNKNOWN_ERROR;
        }
        status_t eRet;
        eRet = mpVdecFrameManager->start();
        if(eRet != NO_ERROR)
        {
            aloge("fatal error! check code!");
        }
    }
    Ret = AW_MPI_UVC_EnableDevice((UVC_DEV)mUvcDev.c_str());
    if(Ret != SUCCESS)
    {
        aloge("the %s usbcamera can not enable, 0x%x", mUvcDev, Ret);
        return Ret;
    }
    
    mCurrentState = State::STARTED;
    return NO_ERROR;
    
}

status_t EyeseeUSBCamera::stopDevice()
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::STARTED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    
    int Ret = SUCCESS;
    if(mbVdecFlag && JudgeCompressedCaptureFormat(mCaptureParam.mUsbCam_CapPixelformat))
    {
        status_t eRet = mpVdecFrameManager->stop();
        if(eRet != NO_ERROR)
        {
            aloge("fatal error! check code!");
        }
        Ret = AW_MPI_UVC_StopRecvPic((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChnForVdec);
        if(Ret != SUCCESS)
        {
            aloge("%s usbcamera can not stop uvcchn[%d], 0x%x", mUvcDev.c_str(), mVirtualUvcChnForVdec, Ret);
            return UNKNOWN_ERROR;
        }
        Ret = AW_MPI_VDEC_StopRecvStream(mVdecChn);
        if(Ret != SUCCESS)
        {
            aloge("%s usbcamera can not stop vdecchn[%d], 0x%x", mUvcDev, mVdecChn, Ret);
            return UNKNOWN_ERROR;
        }
    }
    mCurrentState = State::PREPARED;
    return NO_ERROR;
}

status_t EyeseeUSBCamera::releaseDevice()
{
    AutoMutex autoMutex(mLock);

    if(mCurrentState != State::PREPARED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    
    if(mbVdecFlag && JudgeCompressedCaptureFormat(mCaptureParam.mUsbCam_CapPixelformat))
    {
        if(mpVdecFrameManager)
        {
            delete mpVdecFrameManager;
            mpVdecFrameManager = NULL;
        }
        AW_MPI_VDEC_DestroyChn(mVdecChn);
        mVdecChn = MM_INVALID_CHN;
        AW_MPI_UVC_DestroyVirChn((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChnForVdec);
        mVirtualUvcChnForVdec = MM_INVALID_CHN;
    }
    ERRORTYPE Ret = AW_MPI_UVC_DisableDevice((UVC_DEV)mUvcDev.c_str());
    if(Ret != NO_ERROR)
    {
        aloge("fatal error! check code!");
    }
    Ret = AW_MPI_UVC_DestroyDevice((UVC_DEV)mUvcDev.c_str());
    if(Ret != NO_ERROR)
    {
        aloge("fatal error! check code!");
    }
    mUvcDev = "";

    clearConfig();

    mCurrentState = State::IDLE;
    
    return NO_ERROR;        
}

UvcChannel *EyeseeUSBCamera::searchUvcChannel_l(UvcChn chnId)
{
    for (auto& i : mUvcChnList)
    {
        if (i.mChnId == chnId)
        {
            return i.mpChannel;
        }
    }
    return NULL;
}

UvcChannel *EyeseeUSBCamera::searchUvcChannel(UvcChn chnId)
{
    AutoMutex lock(mUvcChnListLock);
    return searchUvcChannel_l(chnId);
}

status_t EyeseeUSBCamera::openChannel(UvcChn chnId)
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::STARTED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel != NULL)
    {
        aloge("channel %d is opened!", chnId);
        return ALREADY_EXISTS;
    }
    //decide if permit create uvcChn.
    bool bPermit = false;
    if(UvcChn::UvcMainChn == chnId)
    {
        bPermit = true;
    }
    else if(UvcChn::VdecMainChn == chnId)
    {
        if(mbVdecFlag && JudgeCompressedCaptureFormat(mCaptureParam.mUsbCam_CapPixelformat))
        {
            bPermit = true;
        }
    }
    else if(UvcChn::VdecSubChn == chnId)
    {
        if(mbVdecFlag && JudgeCompressedCaptureFormat(mCaptureParam.mUsbCam_CapPixelformat))
        {
            if(mbVdecSubOutputFlag)
            {
                bPermit = true;
            }
        }
    }
    else
    {
        aloge("fatal error! wrong uvcChn[%d]!", chnId);
    }
    if(false == bPermit)
    {
        return PERMISSION_DENIED;
    }
    AutoMutex lock(mUvcChnListLock);
    UvcChannelInfo chnInfo;
    chnInfo.mpChannel = new UvcChannel(mUvcDev, mCameraId, chnId, mpVdecFrameManager);
    chnInfo.mChnId = chnId;
    mUvcChnList.push_back(chnInfo);

    CameraParameters chnParams;
    chnInfo.mpChannel->getParameters(chnParams);
    if(UvcChn::UvcMainChn == chnId)
    {
        SIZE_S videoSize = {mUvcDevAttr.mUvcVideo_Width, mUvcDevAttr.mUvcVideo_Height};
        chnParams.setVideoSize(videoSize);
        alogd("uvcChn[%d] videoSize[%dx%d]", chnId, videoSize.Width, videoSize.Height);
        chnParams.setPreviewFrameRate(mUvcDevAttr.mUvcVideo_Fps);
        chnParams.setPreviewFormat(map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(mUvcDevAttr.mPixelformat));
        chnParams.setVideoBufferNumber(mUvcDevAttr.mUvcVideo_BufCnt);
    }
    else if(UvcChn::VdecMainChn == chnId)
    {
        SIZE_S videoSize = {mVdecChnAttr.mPicWidth, mVdecChnAttr.mPicHeight};
        chnParams.setVideoSize(videoSize);
        SIZE_S videoBufSize = {ALIGN(mVdecChnAttr.mPicWidth, 32), ALIGN(mVdecChnAttr.mPicHeight, 32)};
        chnParams.setVideoBufSizeOut(videoBufSize);
        alogd("uvcChn[%d] videoSize[%dx%d], bufSize[%dx%d]", chnId, videoSize.Width, videoSize.Height, videoBufSize.Width, videoBufSize.Height);
        chnParams.setPreviewFrameRate(mUvcDevAttr.mUvcVideo_Fps);
        chnParams.setPreviewFormat(mVdecChnAttr.mOutputPixelFormat);
        chnParams.setVideoBufferNumber(5);
    }
    else if(UvcChn::VdecSubChn == chnId)
    {
        unsigned int mSubPicWidth = ALIGN((unsigned int)ceil((float)mVdecChnAttr.mPicWidth/pow(2, mVdecChnAttr.mSubPicWidthRatio)), 2);
        unsigned int mSubPicHeight = ALIGN((unsigned int)ceil((float)mVdecChnAttr.mPicHeight/pow(2, mVdecChnAttr.mSubPicHeightRatio)), 2);
        SIZE_S videoSize = {mSubPicWidth, mSubPicHeight};
        chnParams.setVideoSize(videoSize);
        SIZE_S videoBufSize = {ALIGN(mSubPicWidth, 32), ALIGN(mSubPicHeight, 32)};
        chnParams.setVideoBufSizeOut(videoBufSize);
        alogd("uvcChn[%d], videoSize[%dx%d], bufSize[%dx%d]", chnId, videoSize.Width, videoSize.Height, videoBufSize.Width, videoBufSize.Height);
        chnParams.setPreviewFrameRate(mUvcDevAttr.mUvcVideo_Fps);
        chnParams.setPreviewFormat(mVdecChnAttr.mSubOutputPixelFormat);
        chnParams.setVideoBufferNumber(5);
    }
    chnParams.setColorSpace(V4L2_COLORSPACE_JPEG);
    chnParams.setPreviewRotation(0);
    chnParams.setZoomSupported(true);
    chnParams.setZoom(0);
    chnParams.setMaxZoom(10);
    chnInfo.mpChannel->setParameters(chnParams);

    chnInfo.mpChannel->setDataListener(mpNativeContext);
    chnInfo.mpChannel->setNotifyListener(mpNativeContext);
    return NO_ERROR;
}

status_t EyeseeUSBCamera::prepareChannel(UvcChn chnId)
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::STARTED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    status_t ret = pChannel->prepare();
    if (ret != NO_ERROR)
    {
        aloge("prepare channel error!");
        return ret;
    }
    return NO_ERROR;
}
status_t EyeseeUSBCamera::startChannel(UvcChn chnId)
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::STARTED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) 
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    status_t ret = pChannel->startChannel();
    if(ret != NO_ERROR)
    {
        aloge("fatal error! uvcChn[%d] start fail[0x%x]", chnId, ret);
    }
    return ret;
}
status_t EyeseeUSBCamera::stopChannel(UvcChn chnId)
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::STARTED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopChannel();
}
status_t EyeseeUSBCamera::releaseChannel(UvcChn chnId)
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::STARTED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    AutoMutex lock(mUvcChnListLock);
    status_t ret = pChannel->release();
    return ret;
}
status_t EyeseeUSBCamera::closeChannel(UvcChn chnId)
{
    AutoMutex autoMutex(mLock);
    if(mCurrentState != State::STARTED)
    {
        aloge("called in wrong state 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    bool found = false;
    {
        AutoMutex lock(mUvcChnListLock);
        for (auto it = mUvcChnList.begin(); it != mUvcChnList.end(); ++it)
        {
            if (it->mChnId == chnId) 
            {
                delete it->mpChannel;
                mUvcChnList.erase(it);
                found = true;
                break;
            }
        }
    }
    if (!found)
    {
        aloge("channel %d is not exist!", chnId);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t EyeseeUSBCamera::setParameters(UvcChn chnId, CameraParameters &param)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) 
    {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    return pChannel->setParameters(param);
}

status_t EyeseeUSBCamera::getParameters(UvcChn chnId, CameraParameters &param)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) 
    {
        alogd("Be careful! channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getParameters(param);
}

status_t EyeseeUSBCamera::setChannelDisplay(UvcChn chnId, VO_LAYER VoLayer)
{
    AutoMutex autoMutex(mLock);
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) 
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setPreviewDisplay(VoLayer);
}

bool EyeseeUSBCamera::previewEnabled(UvcChn chnId)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return false;
    }
    return pChannel->isPreviewEnabled();
}

status_t EyeseeUSBCamera::startRender(UvcChn chnId)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startRender();
}

status_t EyeseeUSBCamera::stopRender(UvcChn chnId)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopRender();
}

void EyeseeUSBCamera::releaseRecordingFrame(UvcChn chnId, uint32_t index)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) 
    {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->releaseFrame(index);
}

status_t EyeseeUSBCamera::startRecording(UvcChn chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startRecording(pCb, recorderId);
}

status_t EyeseeUSBCamera::stopRecording(UvcChn chnId, int recorderId)
{
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopRecording(recorderId);
}

void EyeseeUSBCamera::setErrorCallback(ErrorCallback *pCb)
{
    mpErrorCallback = pCb;
}

void EyeseeUSBCamera::setInfoCallback(InfoCallback *pCb)
{
    mpInfoCallback = pCb;
}

status_t EyeseeUSBCamera::takePicture(UvcChn chnId, PictureCallback *pPostview, PictureCallback *pJpeg, PictureRegionCallback *pPicRegion)
{
    AutoMutex lock(mSetTakePictureCallbackLock);
    alogv("takePicture");
    std::map<UvcChn, TakePictureCallback>::iterator it = mTakePictureCallback.find(chnId);
    if(it == mTakePictureCallback.end())
    {
        TakePictureCallback Temp;
        Temp.mpPostviewCallback = pPostview;
        Temp.mpJpegCallback = pJpeg;
        Temp.mpPictureRegionCallback = pPicRegion;
        std::pair<std::map<UvcChn, TakePictureCallback>::iterator, bool> insertRet = mTakePictureCallback.insert(std::make_pair(chnId, Temp));
        if(insertRet.second != true)
        {
            aloge("fatal error! insert std::pair fail!");
            return UNKNOWN_ERROR;
        }
        /*if((it = mTakePictureCallback.find(chnId)) == mTakePictureCallback.end())
        {
            aloge("fatal error!");
            return UNKNOWN_ERROR;
        }*/
    }
    else
    {
        it->second.mpPostviewCallback = pPostview;
        it->second.mpJpegCallback = pJpeg;
        it->second.mpPictureRegionCallback = pPicRegion;
    }
    
    // If callback is not set, do not send me callbacks.
    unsigned int msgType = 0;
    status_t ret = NO_ERROR;
    //EyeseeCameraContext *context = (EyeseeCameraContext*)mpNativeContext;

    if (pPostview != NULL) 
    {
        msgType |= CAMERA_MSG_POSTVIEW_FRAME;
    }
    if (pJpeg != NULL) 
    {
        msgType |= CAMERA_MSG_COMPRESSED_IMAGE;
    }

#if 0
    /*
     * When CAMERA_MSG_RAW_IMAGE is requested, if the raw image callback
     * buffer is available, CAMERA_MSG_RAW_IMAGE is enabled to get the
     * notification _and_ the data; otherwise, CAMERA_MSG_RAW_IMAGE_NOTIFY
     * is enabled to receive the callback notification but no data.
     */
    if (msgType & CAMERA_MSG_RAW_IMAGE) {
        alogv("Enable raw image callback buffer");
        if (!context->isRawImageCallbackBufferAvailable()) {
            alogv("Enable raw image notification, since no callback buffer exists");
            msgType &= ~CAMERA_MSG_RAW_IMAGE;
            msgType |= CAMERA_MSG_RAW_IMAGE_NOTIFY;
        }
    }
#endif
    UvcChannel *pChannel = searchUvcChannel(chnId);
    if (pChannel == NULL) 
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    bool bCanTakePic = true;
    if(pChannel->getChannelId() == UvcChn::UvcMainChn)
    {
        if(JudgeCompressedCaptureFormat(mUvcDevAttr.mPixelformat))
        {
            if(pPostview!=NULL)
            {
                aloge("fatal error! compressed format can't generate postview picture!");
                bCanTakePic = false;
            }
        }
    }
    if(bCanTakePic)
    {
        return pChannel->takePicture(msgType, pPicRegion);
    }
    else
    {
        return BAD_VALUE;
    }
}

status_t EyeseeUSBCamera::openUSBCamera()
{
    mUvcDev = mCaptureParam.mpUsbCam_DevName;
    int Ret = AW_MPI_UVC_CreateDevice((UVC_DEV)mUvcDev.c_str());
    if(Ret != SUCCESS)
    {
        aloge("the %s usbcamera device can not create!", mUvcDev);
        return UNKNOWN_ERROR;
    }

    AW_MPI_UVC_GetDeviceAttr((UVC_DEV)mUvcDev.c_str(), &mUvcDevAttr);
    mUvcDevAttr.mPixelformat = mCaptureParam.mUsbCam_CapPixelformat;
    mUvcDevAttr.mUvcVideo_Width = mCaptureParam.mUsbCam_CapWidth;
    mUvcDevAttr.mUvcVideo_Height = mCaptureParam.mUsbCam_CapHeight;
    mUvcDevAttr.mUvcVideo_Fps = mCaptureParam.mUsbCam_CapFps;
    mUvcDevAttr.mUvcVideo_BufCnt = mCaptureParam.mUsbCam_CapBufCnt;
    Ret = AW_MPI_UVC_SetDeviceAttr((UVC_DEV)mUvcDev.c_str(), &mUvcDevAttr);
    if(Ret != SUCCESS)
    {
        aloge("the %s usbcamera device can not set device attr!", mUvcDev);
        return UNKNOWN_ERROR;
    }
    //get final UVC_ATTR_S.
    AW_MPI_UVC_GetDeviceAttr((UVC_DEV)mUvcDev.c_str(), &mUvcDevAttr);
    alogd("uvc capture final format[0x%x],[%dx%d],fps[%d]", mUvcDevAttr.mPixelformat, mUvcDevAttr.mUvcVideo_Width, mUvcDevAttr.mUvcVideo_Height, mUvcDevAttr.mUvcVideo_Fps);
    return NO_ERROR;
}

status_t EyeseeUSBCamera::openVdecDevice()
{
    if(mUvcDevAttr.mPixelformat != UVC_MJPEG && mUvcDevAttr.mPixelformat != UVC_H264)
    {
        aloge("the %s usbcamera capture pixelformat do not support vdec!", mUvcDev);
        return INVALID_OPERATION;
    }

    if(mUvcDevAttr.mPixelformat == UVC_MJPEG)
    {
        mVdecChnAttr.mType = PT_MJPEG;
    }
    else if(mUvcDevAttr.mPixelformat == UVC_H264)
    {
        mVdecChnAttr.mType = PT_H264;
    }

    mVdecChnAttr.mBufSize = mVdecParam.mUsbCam_VdecBufSize;
    mVdecChnAttr.mPriority = mVdecParam.mUsbCam_VdecPriority;
    mVdecChnAttr.mPicWidth = mVdecParam.mUsbCam_VdecPicWidth;
    mVdecChnAttr.mPicHeight = mVdecParam.mUsbCam_VdecPicHeight;
    mVdecChnAttr.mInitRotation = mVdecParam.mUsbCam_VdecInitRotation;
    mVdecChnAttr.mOutputPixelFormat = mVdecParam.mUsbCam_VdecOutputPixelFormat;
    mVdecChnAttr.mExtraFrameNum = mVdecParam.mUsbCam_VdecExtraFrameNum;
    mVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    mVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;
    mVdecChnAttr.mSubPicEnable = mVdecParam.mUsbCam_VdecSubPicEnable;
    mVdecChnAttr.mSubOutputPixelFormat = mVdecParam.mUsbCam_VdecSubOutputPixelFormat;
    if(mVdecChnAttr.mSubPicEnable)
    {
        if(mVdecParam.mUsbCam_VdecSubPicWidth > 0 && mVdecParam.mUsbCam_VdecSubPicHeight > 0)
        {
            // ratio = log2(Main / Sub) = (log10(Main)) / (log10(Sub)) : old version std C++ before C++11
            // log2() : C++11
            int SubRatio = ceil(log2(static_cast<float>(mUvcDevAttr.mUvcVideo_Width) / mVdecParam.mUsbCam_VdecSubPicWidth));
            mVdecChnAttr.mSubPicWidthRatio = decideMaxScaleRatio(SubRatio, mVdecChnAttr.mType);
            SubRatio = ceil(log2(static_cast<float>(mUvcDevAttr.mUvcVideo_Height) / mVdecParam.mUsbCam_VdecSubPicHeight));
            mVdecChnAttr.mSubPicHeightRatio = decideMaxScaleRatio(SubRatio, mVdecChnAttr.mType);
            if(mVdecChnAttr.mSubPicWidthRatio > mVdecChnAttr.mSubPicHeightRatio)
            {
                mVdecChnAttr.mSubPicHeightRatio = mVdecChnAttr.mSubPicWidthRatio;
            }
            else
            {
                mVdecChnAttr.mSubPicWidthRatio = mVdecChnAttr.mSubPicHeightRatio;
            }
            alogd("sub picture scale ratio w[%d]h[%d]", mVdecChnAttr.mSubPicWidthRatio, mVdecChnAttr.mSubPicHeightRatio);
            if(mVdecChnAttr.mSubPicWidthRatio <= 0)
            {
                alogd("Be careful! why VdecSubPicSize[%dx%d] < uvcVideoSize[%dx%d]", 
                    mVdecParam.mUsbCam_VdecSubPicWidth, mVdecParam.mUsbCam_VdecSubPicHeight, 
                    mUvcDevAttr.mUvcVideo_Width, mUvcDevAttr.mUvcVideo_Height);
                mVdecChnAttr.mSubPicEnable = FALSE;
            }
        }
        else
        {
            aloge("fatal error, the vdec subwidth[%d], subheight[%d]", mVdecParam.mUsbCam_VdecSubPicWidth, mVdecParam.mUsbCam_VdecSubPicHeight);
            mVdecChnAttr.mSubPicWidthRatio = 0;
            mVdecChnAttr.mSubPicHeightRatio = 0;
            mVdecChnAttr.mSubPicEnable = FALSE;
        }
    }
    else
    {
        mVdecChnAttr.mSubPicWidthRatio = 0;
        mVdecChnAttr.mSubPicHeightRatio = 0;
    }
    if(mVdecChnAttr.mSubPicEnable)
    {
        mbVdecSubOutputFlag = true;
    }
    else
    {
        mbVdecSubOutputFlag = false;
    }
    //verify param, make vencChnAttr picwidth and picHeight equal to vdec output buffer size.
    if(mVdecChnAttr.mSubPicEnable)
    {
        mVdecChnAttr.mPicWidth = mUvcDevAttr.mUvcVideo_Width;
        mVdecChnAttr.mPicHeight = mUvcDevAttr.mUvcVideo_Height;
    }
    else
    {
        if(0 == mVdecChnAttr.mPicWidth || 0 == mVdecChnAttr.mPicHeight)
        {
            mVdecChnAttr.mPicWidth = mUvcDevAttr.mUvcVideo_Width;
            mVdecChnAttr.mPicHeight = mUvcDevAttr.mUvcVideo_Height;
        }
        int SubRatioW = 0;
        if(mVdecChnAttr.mPicWidth < mUvcDevAttr.mUvcVideo_Width)
        {
            SubRatioW = ceil(log2(static_cast<float>(mUvcDevAttr.mUvcVideo_Width)/mVdecChnAttr.mPicWidth));
        }
        int SubRatioH = 0;
        if(mVdecChnAttr.mPicHeight < mUvcDevAttr.mUvcVideo_Height)
        {
            SubRatioH = ceil(log2(static_cast<float>(mUvcDevAttr.mUvcVideo_Height)/mVdecChnAttr.mPicHeight));
        }
        int SubRatio = SubRatioW>SubRatioH?SubRatioW:SubRatioH;
        mVdecChnAttr.mPicWidth = ALIGN((unsigned int)ceil((float)mUvcDevAttr.mUvcVideo_Width/pow(2, SubRatio)), 32);
        mVdecChnAttr.mPicHeight = ALIGN((unsigned int)ceil((float)mUvcDevAttr.mUvcVideo_Height/pow(2, SubRatio)), 32);
    }

    mVdecChn = 0;
    while(mVdecChn < VDEC_MAX_CHN_NUM)
    {
        int Ret = AW_MPI_VDEC_CreateChn(mVdecChn, &mVdecChnAttr);
        if(SUCCESS == Ret)
        {
            break;
        }
        else if(ERR_VDEC_EXIST == Ret)
        {
            mVdecChn++;
        }
        else
        {
            aloge("the %s usbcamera fail to create vdecchn[%d], 0x%x", mUvcDev, mVdecChn, Ret);
            mVdecChn++;
        }
    }
    if(VDEC_MAX_CHN_NUM == mVdecChn)
    {
        aloge("the %s usbcamera fail to create virchn.", mUvcDev);
        return UNKNOWN_ERROR;
    }

    //AW_MPI_VDEC_GetChnAttr(mVdecChn, &mVdecChnAttr); //feedback vdecattr

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&MPPVdecCallback;
    AW_MPI_VDEC_RegisterCallback(mVdecChn, &cbInfo);

    return NO_ERROR;
    
}
void EyeseeUSBCamera::clearConfig()
{
    mCaptureParam.mpUsbCam_DevName.clear();
    mCaptureParam.mUsbCam_CapPixelformat = UVC_YUY2;
    mCaptureParam.mUsbCam_CapWidth = 0;
    mCaptureParam.mUsbCam_CapHeight = 0;
    mCaptureParam.mUsbCam_CapFps = 0;
    mCaptureParam.mUsbCam_CapBufCnt = 0;
        
    mbCaptureFlag = false;
    mUvcDev = "";
    memset(&mUvcDevAttr, 0, sizeof mUvcDevAttr);
    mVirtualUvcChnForVdec = MM_INVALID_CHN;

    mVdecParam.mUsbCam_VdecBufSize = 0;
    mVdecParam.mUsbCam_VdecPriority = 0;
    mVdecParam.mUsbCam_VdecPicWidth = 0;
    mVdecParam.mUsbCam_VdecPicHeight = 0;
    mVdecParam.mUsbCam_VdecInitRotation = ROTATE_NONE;
    mVdecParam.mUsbCam_VdecOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    mVdecParam.mUsbCam_VdecSubPicEnable = false;
    mVdecParam.mUsbCam_VdecSubPicWidth = 0;
    mVdecParam.mUsbCam_VdecSubPicHeight = 0;
    mVdecParam.mUsbCam_VdecSubOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    mVdecParam.mUsbCam_VdecExtraFrameNum = 0;
        
    mbVdecFlag = false;
    mbVdecSubOutputFlag = false;
    mVdecChn = MM_INVALID_CHN;
    memset(&mVdecChnAttr, 0, sizeof mVdecChnAttr);
}

ERRORTYPE EyeseeUSBCamera::MPPVdecCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    if(MOD_ID_VDEC != pChn->mModId)
    {
        aloge("fatal error! need implement!");
        return ERR_VDEC_ILLEGAL_PARAM;
    }
    EyeseeUSBCamera *pCamera = (EyeseeUSBCamera*)cookie;
    switch(event)
    {
        case MPP_EVENT_VDEC_NOTIFY_NO_FRAME_BUFFER:
        {
            status_t eRet = pCamera->mpVdecFrameManager->VdecNotifyNoFrameBuffer();
            if(eRet != NO_ERROR)
            {
                //alogd("Be careful! vdec frame manager has no frame to return!");
                ret = ERR_VDEC_BUF_EMPTY;
            }
            else
            {
                ret = SUCCESS;
            }
            break;
        }
        default:
        {
            alogd("ignore event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
            break;
        }
    }
    return ret;
}


};




