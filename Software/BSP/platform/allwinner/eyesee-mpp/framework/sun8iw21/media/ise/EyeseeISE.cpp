/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeISE.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/05
  Last Modified :
  Description   : ISE module.
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeISE"
#include <utils/plat_log.h>

#include "ISEDevice.h"
#include "EyeseeISE.h"


using namespace std;

namespace EyeseeLinux {

class EyeseeISE;

class EyeseeISEContext : public DataListener, public NotifyListener
{
public:
    EyeseeISEContext(EyeseeISE *pISE) : mpISE(pISE) {}
    ~EyeseeISEContext(){}

    virtual void postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize);
    virtual void notify(int msgType, int chnId, int ext1, int ext2);

private:
    EyeseeISE *mpISE;
    Mutex mLock;
};

void EyeseeISEContext::postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize)
{
    EyeseeISE::postEventFromNative(mpISE, msgType, chnId, (int)dataSize, 0, &dataPtr);
}

void EyeseeISEContext::notify(int msgType, int chnId, int ext1, int ext2)
{
    EyeseeISE::postEventFromNative(mpISE, msgType, chnId, ext1, ext2, NULL);
}


void EyeseeISE::EventHandler::handleMessage(const CallbackMessage &msg)
{
    switch (msg.what) {
        case CAMERA_MSG_SHUTTER:
        {
            //if (mpISE->mpShutterCallback != NULL)
            //{
            //    mpISE->mpShutterCallback->onShutter(msg.arg1);
            //}
            AutoMutex lock(mpISE->mSetTakePictureCallbackLock);
            int chnId = msg.arg1;
            std::map<int, EyeseeISE::TakePictureCallback>::iterator it = mpISE->mTakePictureCallback.find(chnId);
            if(it == mpISE->mTakePictureCallback.end())
            {
                aloge("fatal error, why not find chnId[%d]", chnId);
                return;
            }

            if(it->second.mpShutterCallback)
            {
               it->second.mpShutterCallback->onShutter(chnId);
            }
            return;
        }
        case CAMERA_MSG_RAW_IMAGE:
        {
            AutoMutex lock(mpISE->mSetTakePictureCallbackLock);
            int chnId = msg.arg1;
            std::map<int, EyeseeISE::TakePictureCallback>::iterator it = mpISE->mTakePictureCallback.find(chnId);
            if(it == mpISE->mTakePictureCallback.end())
            {
                aloge("fatal error, why not find chnId[%d]", chnId);
                return;
            }

            if (it->second.mpRawImageCallback)
            {
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL)
                    {
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        it->second.mpRawImageCallback->onPictureTaken(chnId, pData, nSize, mpISE);
                    }
                }
            }
            mpISE->mpISEDevice->notifyPictureRelease(chnId);
            return;
        }

        case CAMERA_MSG_COMPRESSED_IMAGE:
        {
            AutoMutex lock(mpISE->mSetTakePictureCallbackLock);
            int chnId = msg.arg1;
            std::map<int, EyeseeISE::TakePictureCallback>::iterator it = mpISE->mTakePictureCallback.find(chnId);
            if(it == mpISE->mTakePictureCallback.end())
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
                        it->second.mpJpegCallback->onPictureTaken(chnId, pData, nSize, mpISE);
                    }
                }
            }
            mpISE->mpISEDevice->notifyPictureRelease(chnId);
            return;
        }

        case CAMERA_MSG_PREVIEW_FRAME:
        {
            if (mpISE->mpPreviewCallback != NULL) 
            {
                alogw("only for test, for fun. Don't use it now");
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL)
                    {
                        int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        mpISE->mpPreviewCallback->onPreviewFrame(pData, nSize, mpISE);
                    }
                }
            }
            return;
        }

        case CAMERA_MSG_POSTVIEW_FRAME:
        {
            int chnId = msg.arg1;
            if (mpISE->mpPostviewCallback != NULL)
            {
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL)
                    {
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        mpISE->mpPostviewCallback->onPictureTaken(chnId, pData, nSize, mpISE);
                        //mpISE->postDataCompleted(msg.arg1, msg.obj, msg.arg2);
                    }
                }
            }
            mpISE->mpISEDevice->notifyPictureRelease(chnId);
            return;
        }

        case CAMERA_MSG_ERROR :
        {
            aloge("Channel %d error %d", msg.arg1, msg.arg2);
            if (mpISE->mpErrorCallback != NULL) {
                mpISE->mpErrorCallback->onError(msg.arg1, msg.arg2, mpISE);
            }
            return;
        }

        case CAMERA_MSG_INFO:
        {
            alogd("vo notify EyeseeISE start preview");
            int chnId = msg.arg1;
            CameraMsgInfoType what = (CameraMsgInfoType)msg.arg2;
            int extra = 0;
            if(msg.extArgs.size() > 0)
            {
                extra = msg.extArgs[0];
            }
            if (mpISE->mpInfoCallback != NULL) {
                mpISE->mpInfoCallback->onInfo(chnId, what, extra, mpISE);
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


void EyeseeISE::postEventFromNative(EyeseeISE *pISE, int what, int arg1, int arg2, int arg3, const std::shared_ptr<CMediaMemory>* pDataPtr)
{
    CallbackMessage msg;

    msg.what = what;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    msg.extArgs.push_back(arg3);
    //msg.obj = obj;
    if(pDataPtr)
    {
        msg.mDataPtr = *pDataPtr;
    }
    pISE->mpEventHandler->post(msg);
}

unsigned int EyeseeISE::gISEIdCounter = ISEIdPrefixMark | 0x00;
EyeseeISE::EyeseeISE()
    : mISEId(gISEIdCounter++)
    , mpShutterCallback(NULL)
    , mpRawImageCallback(NULL)
    , mpJpegCallback(NULL)
    , mpPreviewCallback(NULL)
    , mpPostviewCallback(NULL)
    , mpErrorCallback(NULL)
{
    alogd("EyeseeISE ctor!");
    mpISEDevice = new ISEDevice(mISEId);
    mpEventHandler = new EventHandler(this);
    mpNativeContext = new EyeseeISEContext(this);
}

EyeseeISE::~EyeseeISE()
{
    alogd("EyeseeISE dtor!");
    if (mpNativeContext)
    {
        delete mpNativeContext;
    }
    if (mpISEDevice)
    {
        delete mpISEDevice;
    }
    if (mpEventHandler)
    {
        delete mpEventHandler;
    }
}

EyeseeISE *EyeseeISE::open()
{
    return new EyeseeISE();
}

void EyeseeISE::close(EyeseeISE *pIse)
{
    delete pIse;
}

status_t EyeseeISE::prepareDevice(ISE_MODE_E mode,PIXEL_FORMAT_E pixelformat)
{
    return mpISEDevice->prepare(mode,pixelformat);
}

status_t EyeseeISE::releaseDevice()
{
    return mpISEDevice->release();
}

//status_t EyeseeISE::setCamera(EyeseeCamera *pCam0, int nCam0Chn, EyeseeCamera *pCam1, int nCam1Chn)
//{
//    return mpISEDevice->setCamera(pCam0, nCam0Chn, pCam1, nCam1Chn);
//}

status_t EyeseeISE::setCamera(std::vector<CameraChannelInfo>& cameraChannels)
{
    return mpISEDevice->setCamera(cameraChannels);
}

status_t EyeseeISE::startDevice()
{
    return mpISEDevice->start();
}

status_t EyeseeISE::stopDevice()
{
    return mpISEDevice->stop();
}

ISE_CHN EyeseeISE::openChannel(ISE_CHN_ATTR_S *pChnAttr)
{
    ISEDevice *pISEDevice = (ISEDevice*)mpISEDevice;
    ISE_CHN nChnId = pISEDevice->openChannel(pChnAttr);
    if (nChnId == MM_INVALID_CHN) 
    {
        aloge("Failed to open channel");
        return nChnId;
    }
    EyeseeISEContext *pContext = (EyeseeISEContext*)mpNativeContext;
    pISEDevice->setDataListener(nChnId, pContext);
    pISEDevice->setNotifyListener(nChnId, pContext);
    return nChnId;
}

status_t EyeseeISE::startChannel(int chnId)
{
    return mpISEDevice->startChannel(chnId);
}

status_t EyeseeISE::closeChannel(ISE_CHN chnId)
{
    return mpISEDevice->closeChannel(chnId);
}

status_t EyeseeISE::setParameters(int chnId, CameraParameters &param)
{
    return mpISEDevice->setParameters(chnId, param);
}

status_t EyeseeISE::getParameters(int chnId, CameraParameters &param)
{
    return mpISEDevice->getParameters(chnId, param);
}

status_t EyeseeISE::setChannelDisplay(int chnId, int hlay)
{
    return mpISEDevice->setChannelDisplay(chnId, hlay);
}

bool EyeseeISE::previewEnabled(int chnId)
{
    return mpISEDevice->previewEnabled(chnId);
}

status_t EyeseeISE::startRender(int chnId)
{
    return mpISEDevice->startRender(chnId);
}

status_t EyeseeISE::stopRender(int chnId)
{
    return mpISEDevice->stopRender(chnId);
}

status_t EyeseeISE::setMoPortAttr(ISE_CHN_ATTR_S *pAttr)
{
    return mpISEDevice->setMoPortAttr(pAttr);
}

void EyeseeISE::releaseRecordingFrame(int chnId, uint32_t index)
{
    mpISEDevice->releaseRecordingFrame(chnId, index);
}

status_t EyeseeISE::startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
    return mpISEDevice->startRecording(chnId, pCb, recorderId);
}

status_t EyeseeISE::stopRecording(int chnId, int recorderId)
{
    return mpISEDevice->stopRecording(chnId, recorderId);
}

void EyeseeISE::setErrorCallback(ErrorCallback *pCb)
{
    mpErrorCallback = pCb;
}

void EyeseeISE::setInfoCallback(InfoCallback *pCb)
{
    mpInfoCallback = pCb;
}

void EyeseeISE::setPreviewCallback(PreviewCallback *pCb)
{
    mpPreviewCallback = pCb;
}

status_t EyeseeISE::takePicture(int chnId, ShutterCallback *pShutter, PictureCallback *pRaw,
        PictureCallback *pJpeg, PictureRegionCallback *pPicReg)
{
    return takePicture(chnId, pShutter, pRaw, NULL, pJpeg, pPicReg);
}

status_t EyeseeISE::takePicture(int chnId, ShutterCallback *pShutter, PictureCallback *pRaw,
        PictureCallback *pPostview, PictureCallback *pJpeg, PictureRegionCallback *pPicReg)
{
    AutoMutex lock(mSetTakePictureCallbackLock);

    alogv("takePicture");
    mpShutterCallback = pShutter;
    mpRawImageCallback = pRaw;
    mpPostviewCallback = pPostview;
    mpJpegCallback = pJpeg;

    ShutterCallback *pOldShutterCallback = NULL;
    PictureCallback *pOldRawImageCallback = NULL;
    PictureCallback *pOldJpegCallback = NULL;
    PictureCallback *pOldPostviewCallback = NULL;
    PictureRegionCallback *pOldPictureRegionCallback = NULL;

    std::map<int, TakePictureCallback>::iterator it = mTakePictureCallback.find(chnId);
    if(it == mTakePictureCallback.end())
    {
        TakePictureCallback Temp;
        Temp.mpShutterCallback = pShutter;
        Temp.mpRawImageCallback = pRaw;
        Temp.mpPostviewCallback = pPostview;
        Temp.mpJpegCallback = pJpeg;
        Temp.mpPictureRegionCallback = pPicReg;
        mTakePictureCallback.insert(make_pair(chnId, Temp));
        if((it = mTakePictureCallback.find(chnId)) == mTakePictureCallback.end())
        {
            aloge("fatal error!");
            return UNKNOWN_ERROR;
        }
    }
    else
    {
        pOldShutterCallback = it->second.mpShutterCallback;
        pOldRawImageCallback = it->second.mpRawImageCallback;
        pOldPostviewCallback = it->second.mpPostviewCallback;
        pOldPostviewCallback = it->second.mpJpegCallback;
        pOldPictureRegionCallback = it->second.mpPictureRegionCallback;

        it->second.mpShutterCallback = pShutter;
        it->second.mpRawImageCallback = pRaw;
        it->second.mpPostviewCallback = pPostview;
        it->second.mpJpegCallback = pJpeg;
        it->second.mpPictureRegionCallback = pPicReg;
    }

    // If callback is not set, do not send me callbacks.
    unsigned int msgType = 0;
    status_t ret = NO_ERROR;
    EyeseeISEContext *context = (EyeseeISEContext*)mpNativeContext;

    if (mpShutterCallback != NULL) {
        msgType |= CAMERA_MSG_SHUTTER;
    }
    if (mpRawImageCallback != NULL) {
        msgType |= CAMERA_MSG_RAW_IMAGE;
    }
    if (mpPostviewCallback != NULL) {
        msgType |= CAMERA_MSG_POSTVIEW_FRAME;
    }
    if (mpJpegCallback != NULL) {
        msgType |= CAMERA_MSG_COMPRESSED_IMAGE;
    }

    ISEDevice *pISEDevice = (ISEDevice*)mpISEDevice;
    ret = pISEDevice->takePicture(chnId, msgType, pPicReg);
    if (ret != NO_ERROR) {
        aloge("takePicture failed");
        it->second.mpShutterCallback = pOldShutterCallback;
        it->second.mpRawImageCallback = pOldRawImageCallback;
        it->second.mpPostviewCallback = pOldPostviewCallback;
        it->second.mpJpegCallback = pOldPostviewCallback;
        it->second.mpPictureRegionCallback = pOldPictureRegionCallback;
        return ret;
    }

    return NO_ERROR;
}

status_t EyeseeISE::cancelContinuousPicture(int chnId)
{
    status_t ret = mpISEDevice->cancelContinuousPicture(chnId);
    if (ret != NO_ERROR)
    {
        aloge("cancelContinuousPicture failed[0x%x]", ret);
    }
    return ret;
}

/*
void EyeseeISE::postDataCompleted(int chnId, const void *pData, int size)
{
    return mpISEDevice->postDataCompleted(chnId, pData, size);
}
*/

// recorder get proxy for return yuv after encode one frame
CameraRecordingProxy* EyeseeISE::getRecordingProxy()
{
    alogd("getProxy");
    return new ISEProxy(this);
}

// CameraRecordingProxy interface
status_t EyeseeISE::ISEProxy::startRecording(int chnId, CameraRecordingProxyListener *pListener, int recorderId)
{
    return mpISE->startRecording(chnId, pListener, recorderId);
}

status_t EyeseeISE::ISEProxy::stopRecording(int chnId, int recorderId)
{
    return mpISE->stopRecording(chnId, recorderId);
}

void EyeseeISE::ISEProxy::releaseRecordingFrame(int chnId, uint32_t frameIndex)
{
    return mpISE->releaseRecordingFrame(chnId, frameIndex);
}

status_t EyeseeISE::ISEProxy::setChannelDisplay(int chnId, int hlay)
{
    return mpISE->setChannelDisplay(chnId, hlay);
}

status_t EyeseeISE::ISEProxy::getParameters(int chnId, CameraParameters &param)
{
    return mpISE->getParameters(chnId, param);
}

status_t EyeseeISE::ISEProxy::setParameters(int chnId, CameraParameters &param)
{
    return mpISE->setParameters(chnId, param);
}

}; /* namespace EyeseeLinux */

