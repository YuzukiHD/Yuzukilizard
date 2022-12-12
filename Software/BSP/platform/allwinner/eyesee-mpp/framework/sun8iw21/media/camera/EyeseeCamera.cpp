/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeCamera.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/01
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeCamera"
#include <utils/plat_log.h>

#include <vector>

#include <EyeseeCamera.h>

#include "VIDevice.h"

using namespace std;
namespace EyeseeLinux
{

std::map<int, CameraInfo> EyeseeCamera::mCameraInfos;
Mutex EyeseeCamera::mCameraInfosLock;
    
class EyeseeCamera;

class EyeseeCameraContext : public DataListener
                                    , public NotifyListener
{
public:
    EyeseeCameraContext(EyeseeCamera *pCamera);
    ~EyeseeCameraContext();

    virtual void postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize);
    virtual void notify(int msgType, int chnId, int ext1, int ext2);
    void addCallbackBuffer(int msgType, CMediaMemory &cbb);
    bool isRawImageCallbackBufferAvailable() const;

private:
    EyeseeCamera *mpCamera;
    vector<CMediaMemory> mRawImageCallbackBuffers;
    vector<CMediaMemory> mCallbackBuffers;
    Mutex mLock;
};

EyeseeCameraContext::EyeseeCameraContext(EyeseeCamera *pCamera)
    : mpCamera(pCamera)
{
}

EyeseeCameraContext::~EyeseeCameraContext()
{
}

void EyeseeCameraContext::postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize)
{
    EyeseeCamera::postEventFromNative(mpCamera, msgType, chnId, (int)dataSize, 0, &dataPtr);
}

void EyeseeCameraContext::notify(int msgType, int chnId, int ext1, int ext2)
{
    EyeseeCamera::postEventFromNative(mpCamera, msgType, chnId, ext1, ext2, NULL);
}

void EyeseeCameraContext::addCallbackBuffer(int msgType, CMediaMemory &cbb)
{
    alogw("addCallbackBuffer is not implement now!");
}

bool EyeseeCameraContext::isRawImageCallbackBufferAvailable() const
{
    return !mRawImageCallbackBuffers.empty();
}


void EyeseeCamera::postEventFromNative(EyeseeCamera *pCam, int what, int arg1, int arg2, int arg3, const std::shared_ptr<CMediaMemory>* pDataPtr)
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


EyeseeCamera::EventHandler::EventHandler(EyeseeCamera *pCam)
    : mpCamera(pCam)
{
}

void EyeseeCamera::EventHandler::handleMessage(const CallbackMessage &msg)
{
    switch (msg.what) {
        case CAMERA_MSG_SHUTTER:
        {
            //if (mpCamera->mpShutterCallback != NULL)
            //{
            //    mpCamera->mpShutterCallback->onShutter(msg.arg1);
            //}
            AutoMutex lock(mpCamera->mSetTakePictureCallbackLock);
            int chnId = msg.arg1;
            std::map<int, EyeseeCamera::TakePictureCallback>::iterator it = mpCamera->mTakePictureCallback.find(chnId);
            if(it == mpCamera->mTakePictureCallback.end())
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
            AutoMutex lock(mpCamera->mSetTakePictureCallbackLock);
            int chnId = msg.arg1;
            std::map<int, EyeseeCamera::TakePictureCallback>::iterator it = mpCamera->mTakePictureCallback.find(chnId);
            if(it == mpCamera->mTakePictureCallback.end())
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
                        //int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        it->second.mpRawImageCallback->onPictureTaken(chnId, pData, nSize, mpCamera);
                    }
                }
            }
            mpCamera->mpVIDevice->notifyPictureRelease(chnId);
            return;
        }

        case CAMERA_MSG_COMPRESSED_IMAGE:
        {
            AutoMutex lock(mpCamera->mSetTakePictureCallbackLock);
            int chnId = msg.arg1;
            std::map<int, EyeseeCamera::TakePictureCallback>::iterator it = mpCamera->mTakePictureCallback.find(chnId);
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
                        //int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        it->second.mpJpegCallback->onPictureTaken(chnId, pData, nSize, mpCamera);
                        //mpCamera->postDataCompleted(msg.arg1, msg.obj, msg.arg2);
                    }
                }
            }
            mpCamera->mpVIDevice->notifyPictureRelease(chnId);
            return;
        }

        case CAMERA_MSG_PREVIEW_FRAME:
        {
            PreviewCallback *pCb = mpCamera->mpPreviewCallback;
            if (pCb != NULL) 
            {
                alogw("only for test, for fun. Don't use it now");
                if (mpCamera->mOneShot) 
                {
                    // Clear the callback variable before the callback
                    // in case the app calls setPreviewCallback from
                    // the callback function
                    mpCamera->mpPreviewCallback = NULL;
                } 
                else if (!mpCamera->mWithBuffer) 
                {
                    // We're faking the camera preview mode to prevent
                    // the app from being flooded with preview frames.
                    // Set to oneshot mode again.
                    mpCamera->setHasPreviewCallback(true, false);
                }
                //pCb.onPreviewFrame((byte[])msg.obj, mCamera);
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL)
                    {
                        int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        pCb->onPreviewFrame(pData, nSize, mpCamera);
                    }
                }
            }
            return;
        }

        case CAMERA_MSG_POSTVIEW_FRAME:
        {
            AutoMutex lock(mpCamera->mSetTakePictureCallbackLock);
            int chnId = msg.arg1;
            std::map<int, EyeseeCamera::TakePictureCallback>::iterator it = mpCamera->mTakePictureCallback.find(chnId);
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
            mpCamera->mpVIDevice->notifyPictureRelease(chnId);
            return;
        }
        case CAMERA_MSG_MOD_DATA:
        {
            if(mpCamera->mpMODDataCallback != NULL)
            {
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL) 
                    {
                        int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        mpCamera->mpMODDataCallback->onMODData(chnId, (MotionDetectResult*)pData, mpCamera);
                    }
                }
            }
            break;
        }
        case CAMERA_MSG_ADAS_DATA:
        {
            if(mpCamera->mpAdasDataCallback != NULL)
            {
                if(msg.mDataPtr)
                {
                    if(msg.mDataPtr->getPointer() != NULL)
                    {
                        int chnId = msg.arg1;
                        const void *pData = msg.mDataPtr->getPointer();
                        int nSize = msg.arg2;
                        #ifdef DEF_MPPCFG_ADAS_DETECT
                        mpCamera->mpAdasDataCallback->onAdasOutData(chnId, (AW_AI_ADAS_DETECT_R_*)pData, mpCamera);
                        #endif
                        #ifdef DEF_MPPCFG_ADAS_DETECT_V2
                        mpCamera->mpAdasDataCallback->onAdasOutData_v2(chnId, (AW_AI_ADAS_DETECT_R__v2*)pData, mpCamera);
                        #endif
                     }
                }
            }
            break;
        }
        case CAMERA_MSG_ERROR:
            aloge("Channel %d error %d", msg.arg1, msg.arg2);
            if (mpCamera->mpErrorCallback != NULL) {
                mpCamera->mpErrorCallback->onError(msg.arg1, msg.arg2, mpCamera);
            }
            return;
        case CAMERA_MSG_INFO:
        {
            int chnId = msg.arg1;
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

EyeseeCamera::EyeseeCamera(int cameraId)
    : mCameraId(cameraId)
    , mpNativeContext(NULL)
    , mpEventHandler(NULL)
    , mpPreviewCallback(NULL)
    , mpErrorCallback(NULL)
    , mpInfoCallback(NULL)
    , mOneShot(false)
    , mWithBuffer(false)
    , mpVIDevice(NULL)
{
    initCamerasConfiguration();
    CameraInfo stCameraInfo;
    if(NO_ERROR!=getCameraInfo(cameraId, &stCameraInfo))
    {
        return;
    }
    mpMODDataCallback = NULL;
    mpAdasDataCallback = NULL;
    mpEventHandler = new EventHandler(this);
    VIDevice *pVIDevice = new VIDevice(cameraId, &stCameraInfo);
    mpVIDevice = pVIDevice;
    EyeseeCameraContext *pContext = new EyeseeCameraContext(this);
    mpNativeContext = (void*)pContext;
}

EyeseeCamera::~EyeseeCamera()
{
    if (mpNativeContext != NULL) {
        EyeseeCameraContext *pContext = (EyeseeCameraContext*)mpNativeContext;
        delete pContext;
    }
    if (mpVIDevice != NULL) {
        VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
        delete pVIDevice;
    }
    if (mpEventHandler != NULL) {
        delete mpEventHandler;
    }
}

EyeseeCamera *EyeseeCamera::open(int cameraId)
{
    initCamerasConfiguration();
    if(NO_ERROR!=getCameraInfo(cameraId, NULL))
    {
        return NULL;
    }
    EyeseeCamera *pCam = new EyeseeCamera(cameraId);
    return pCam;
}

void EyeseeCamera::close(EyeseeCamera* pEyeseeCam)
{
    delete pEyeseeCam;
}

status_t EyeseeCamera::openChannel(int chnId, bool bForceRef)
{
    status_t ret;
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;

    ret = pVIDevice->openChannel(chnId, bForceRef);
    if (ret != NO_ERROR) {
        aloge("Failed to open channel %d", chnId);
        return ret;
    }
    EyeseeCameraContext *pContext = (EyeseeCameraContext*)mpNativeContext;
    pVIDevice->setDataListener(chnId, pContext);
    pVIDevice->setNotifyListener(chnId, pContext);
    return NO_ERROR;
}

status_t EyeseeCamera::closeChannel(int chnId)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->closeChannel(chnId);
}

/*status_t EyeseeCamera::setDeviceAttr(VI_DEV_ATTR_S *devAttr)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->setDeviceAttr(devAttr);
}

status_t EyeseeCamera::getDeviceAttr(VI_DEV_ATTR_S *devAttr)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->getDeviceAttr(devAttr);
}*/

status_t EyeseeCamera::setChannelDisplay(int chnId, int hlay)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->setChannelDisplay(chnId, hlay);
}

status_t EyeseeCamera::changeChannelDisplay(int chnId, int hlay)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->changeChannelDisplay(chnId, hlay);
}

status_t EyeseeCamera::prepareDevice()
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->prepareDevice();
}

status_t EyeseeCamera::releaseDevice()
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->releaseDevice();
}

status_t EyeseeCamera::prepareChannel(int chnId)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->prepareChannel(chnId);
}

status_t EyeseeCamera::releaseChannel(int chnId)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->releaseChannel(chnId);
}

status_t EyeseeCamera::startDevice()
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->startDevice();
}

status_t EyeseeCamera::stopDevice()
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->stopDevice();
}

status_t EyeseeCamera::startChannel(int chnId)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->startChannel(chnId);
}

status_t EyeseeCamera::stopChannel(int chnId)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->stopChannel(chnId);
}

status_t EyeseeCamera::getMODParams(int chnId, MOTION_DETECT_ATTR_S *pParamMD)
{
    return mpVIDevice->getMODParams(chnId, pParamMD);
}

status_t EyeseeCamera::setMODParams(int chnId,MOTION_DETECT_ATTR_S pParamMD)
{
    return mpVIDevice->setMODParams(chnId, pParamMD);
}

status_t EyeseeCamera::startMODDetect(int chnId)
{
    return mpVIDevice->startMODDetect(chnId);
}

status_t EyeseeCamera::stopMODDetect(int chnId)
{
    return mpVIDevice->stopMODDetect(chnId);
}

status_t EyeseeCamera::getAdasParams(int chnId, AdasDetectParam *pParamADAS)
{
    return mpVIDevice->getAdasParams(chnId, pParamADAS);
}
status_t EyeseeCamera::setAdasParams(int chnId, AdasDetectParam pParamADAS)
{
    return mpVIDevice->setAdasParams(chnId, pParamADAS);
}
status_t EyeseeCamera::getAdasInParams(int chnId, AdasInParam *pParamADAS)
{
    return mpVIDevice->getAdasInParams(chnId, pParamADAS);
}
status_t EyeseeCamera::setAdasInParams(int chnId, AdasInParam pParamADAS)
{
    return mpVIDevice->setAdasInParams(chnId, pParamADAS);
}
status_t EyeseeCamera::startAdasDetect(int chnId)
{
    return mpVIDevice->startAdasDetect(chnId);
}
status_t EyeseeCamera::stopAdasDetect(int chnId)
{
    return mpVIDevice->stopAdasDetect(chnId);
}
//==========adas-v2===========
status_t EyeseeCamera::getAdasParams_v2(int chnId, AdasDetectParam_v2 *pParamADAS_v2)
{
    return mpVIDevice->getAdasParams_v2(chnId, pParamADAS_v2);
}
status_t EyeseeCamera::setAdasParams_v2(int chnId, AdasDetectParam_v2 pParamADAS_v2)
{
    return mpVIDevice->setAdasParams_v2(chnId, pParamADAS_v2);
}
status_t EyeseeCamera::getAdasInParams_v2(int chnId, AdasInParam_v2 *pParamADAS_v2)
{
    return mpVIDevice->getAdasInParams_v2(chnId, pParamADAS_v2);
}
status_t EyeseeCamera::setAdasInParams_v2(int chnId, AdasInParam_v2 pParamADAS_v2)
{
    return mpVIDevice->setAdasInParams_v2(chnId, pParamADAS_v2);
}
status_t EyeseeCamera::startAdasDetect_v2(int chnId)
{
    return mpVIDevice->startAdasDetect_v2(chnId);
}
status_t EyeseeCamera::stopAdasDetect_v2(int chnId)
{
    return mpVIDevice->stopAdasDetect_v2(chnId);
}



void EyeseeCamera::releaseRecordingFrame(int chnId, uint32_t index)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    pVIDevice->releaseRecordingFrame(chnId, index);
}

status_t EyeseeCamera::startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->startRecording(chnId, pCb, recorderId);
}

status_t EyeseeCamera::stopRecording(int chnId, int recorderId)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->stopRecording(chnId, recorderId);
}

status_t EyeseeCamera::setParameters(int chnId, CameraParameters &param)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->setParameters(chnId, param);
}

void EyeseeCamera::increaseBufRef(int chnId, VIDEO_FRAME_BUFFER_S *pBuf)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->increaseBufRef(chnId, pBuf);
}

status_t EyeseeCamera::setPicCapMode(int chnId, uint32_t cap_mode_en)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->setPicCapMode(chnId, cap_mode_en);
}

status_t EyeseeCamera::setFrmDrpThrForPicCapMode(int chnId,uint32_t frm_cnt)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->setFrmDrpThrForPicCapMode(chnId, frm_cnt);
} 


status_t EyeseeCamera::getParameters(int chnId, CameraParameters &param)
{
    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    return pVIDevice->getParameters(chnId, param);
}

status_t EyeseeCamera::setISPParameters(CameraParameters &param)
{
    return mpVIDevice->setISPParameters(param);
}

status_t EyeseeCamera::getISPParameters(CameraParameters &param)
{
    return mpVIDevice->getISPParameters(param);
}

status_t EyeseeCamera::configCameraWithMPPModules(int cameraId, CameraInfo *pCameraInfo)
{
    std::map<int, CameraInfo>::iterator it = mCameraInfos.find(cameraId);
    if (it == mCameraInfos.end())
    {
        mCameraInfos[cameraId] = *pCameraInfo;
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! cameraId[%d] has already exist, refuse reconfig!", cameraId);
        return UNKNOWN_ERROR;
    }
}

status_t EyeseeCamera::initCamerasConfiguration()
{
    AutoMutex lock(mCameraInfosLock);
    if(mCameraInfos.size() > 0)
    {
        return NO_ERROR;
    }
    alogw("warning: we don't init cameraInfos according to real system situation. It it not ready. So we use default for example.");
    int cameraId;
    CameraInfo cameraInfo;
    VI_CHN viChn;
    ISP_DEV ispDev;
    ISPGeometry stISPGeometry;

    cameraId = 0;
    cameraInfo.facing = CAMERA_FACING_BACK;
    cameraInfo.orientation = 0;
    cameraInfo.canDisableShutterSound = true;
    cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
    cameraInfo.mMPPGeometry.mCSIChn = 0;
    stISPGeometry.mISPDev = 1;
    stISPGeometry.mScalerOutChns.clear();
    viChn = 0;
    stISPGeometry.mScalerOutChns.push_back(viChn);
    viChn = 2;
    stISPGeometry.mScalerOutChns.push_back(viChn);
    cameraInfo.mMPPGeometry.mISPGeometrys.push_back(stISPGeometry);
    configCameraWithMPPModules(cameraId, &cameraInfo);

    CameraInfo cameraInfo1;
    cameraId = 1;
    cameraInfo1.facing = CAMERA_FACING_BACK;
    cameraInfo1.orientation = 0;
    cameraInfo1.canDisableShutterSound = true;
    cameraInfo1.mCameraDeviceType = CameraInfo::CAMERA_CSI;
    cameraInfo1.mMPPGeometry.mCSIChn = 1;
    stISPGeometry.mISPDev = 0;
    stISPGeometry.mScalerOutChns.clear();
    viChn = 1;
    stISPGeometry.mScalerOutChns.push_back(viChn);
    viChn = 3;
    stISPGeometry.mScalerOutChns.push_back(viChn);
    cameraInfo1.mMPPGeometry.mISPGeometrys.push_back(stISPGeometry);
    configCameraWithMPPModules(cameraId, &cameraInfo1);

    return NO_ERROR;
}

status_t EyeseeCamera::clearCamerasConfiguration()
{
    initCamerasConfiguration();
    AutoMutex lock(mCameraInfosLock);
    alogw("warning: I don't know how to operate lower v4l2 drivers, so it take no effect.");
    mCameraInfos.clear();
    return NO_ERROR;
}

status_t EyeseeCamera::getCameraInfo(int cameraId, CameraInfo *pCameraInfo)
{
    initCamerasConfiguration();
    AutoMutex lock(mCameraInfosLock);
    std::map<int, CameraInfo>::iterator it = mCameraInfos.find(cameraId);
    if (it == mCameraInfos.end())
    {
        aloge("fatal error! cameraId[%d] not exist", cameraId);
        return UNKNOWN_ERROR;
    }
    else
    {
        if(pCameraInfo)
        {
            *pCameraInfo = it->second;
        }
        return NO_ERROR;
    }
}

status_t EyeseeCamera::getCameraInfos(std::map<int, CameraInfo>& cameraInfos)
{
    initCamerasConfiguration();
    AutoMutex lock(mCameraInfosLock);
    cameraInfos = mCameraInfos;
    return NO_ERROR;
}

bool EyeseeCamera::previewEnabled(int chnId)
{
    return mpVIDevice->previewEnabled(chnId);
}

status_t EyeseeCamera::startRender(int chnId)
{
    return mpVIDevice->startRender(chnId);
}
status_t EyeseeCamera::stopRender(int chnId)
{
    return mpVIDevice->stopRender(chnId);
}
status_t EyeseeCamera::pauseRender(int chnId)
{
    return mpVIDevice->pauseRender(chnId);
}
status_t EyeseeCamera::resumeRender(int chnId)
{
    return mpVIDevice->resumeRender(chnId);
}

status_t EyeseeCamera::storeDisplayFrame(int chnId, uint64_t framePts)
{
    return mpVIDevice->storeDisplayFrame(chnId, framePts);
}

status_t EyeseeCamera::takePicture(int chnId, ShutterCallback *pShutter, PictureCallback *pRaw,
        PictureCallback *pJpeg, PictureRegionCallback *pPicReg)
{
    return takePicture(chnId, pShutter, pRaw, NULL, pJpeg, pPicReg);
}

status_t EyeseeCamera::takePicture(int chnId, ShutterCallback *pShutter, PictureCallback *pRaw,
        PictureCallback *pPostview, PictureCallback *pJpeg, PictureRegionCallback *pPicReg)
{
    AutoMutex lock(mSetTakePictureCallbackLock);
    
    alogv("takePicture");

    ShutterCallback *pOldShutterCallback = NULL;
    PictureCallback *pOldRawImageCallback = NULL;  
    PictureCallback *pOldJpegCallback = NULL;  
    PictureCallback *pOldPostviewCallback = NULL;
    PictureRegionCallback *pOldPictureRegionCallback = NULL;		// a serious of callback function
    
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
    EyeseeCameraContext *context = (EyeseeCameraContext*)mpNativeContext;

    if (pShutter != NULL) {
        msgType |= CAMERA_MSG_SHUTTER;
    }
    if (pRaw != NULL) {
        msgType |= CAMERA_MSG_RAW_IMAGE;
    }
    if (pPostview != NULL) {
        msgType |= CAMERA_MSG_POSTVIEW_FRAME;
    }
    if (pJpeg != NULL) {
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

    VIDevice *pVIDevice = (VIDevice*)mpVIDevice;
    ret = pVIDevice->takePicture(chnId, msgType, pPicReg);
    if (ret != NO_ERROR) {
        aloge("takePicture failed");
        it->second.mpShutterCallback = pOldShutterCallback;
        it->second.mpRawImageCallback = pOldRawImageCallback;
        it->second.mpPostviewCallback = pOldPostviewCallback ;
        it->second.mpJpegCallback = pOldPostviewCallback;
        it->second.mpPictureRegionCallback = pOldPictureRegionCallback;
        return ret;
    }
    
    return NO_ERROR;
}

status_t EyeseeCamera::cancelContinuousPicture(int chnId)
{
    status_t ret = mpVIDevice->cancelContinuousPicture(chnId);
    if (ret != NO_ERROR)
    {
        aloge("cancelContinuousPicture failed[0x%x]", ret);
    }
    return ret;
}

status_t EyeseeCamera::KeepPictureEncoder(int chnId, bool bKeep)
{
    return mpVIDevice->KeepPictureEncoder(chnId, bKeep);
}

status_t EyeseeCamera::releasePictureEncoder(int chnId)
{
    return mpVIDevice->releasePictureEncoder(chnId);
}

//add by jason
/*
status_t EyeseeCamera::getISPDMsg(int * exp, int * exp_line, int * gain, int * lv_idx, int * color_temp, int * rgain, int * bgain)
{
	int t1,t2,t3,t4,t5,t6,t7;
    mpVIDevice->getISPDMsg(&t1, &t2, &t3, &t4, &t5, &t6, &t7);
	*exp		 	= t1; 
	*exp_line 		= t2;  
	*gain 			= t3;  
	*lv_idx 		= t4;  
	*color_temp 	= t5; 
	*rgain 			= t6;  
	*bgain 			= t7; 
	return 0;
}
*/

//add by alien
status_t EyeseeCamera::getISPDMsg(int * exp, int * exp_line, int * gain, int * lv_idx, int * color_temp, int * rgain, int * bgain, int * grgain, int * gbgain)
{
	int t1,t2,t3,t4,t5,t6,t7,t8,t9;
    mpVIDevice->getISPDMsg(&t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8, &t9);
	*exp		 	= t1; 
	*exp_line 		= t2;  
	*gain 			= t3;  
	*lv_idx 		= t4;  
	*color_temp 	= t5; 
	*rgain 			= t6;  
	*bgain 			= t7; 
	*grgain         = t8;
	*gbgain         = t9;
	
	return 0;
}



void EyeseeCamera::addCallbackBuffer(CMediaMemory &callbackBuffer)
{
    alogw("addCallbackBuffer is not implement now!");
    // todo
}

void EyeseeCamera::addRawImageCallbackBuffer(CMediaMemory &callbackBuffer)
{
    alogw("addRawImageCallbackBuffer is not implement now!");
    // todo
}

void EyeseeCamera::addCallbackBuffer(CMediaMemory &callbackBuffer, int msgType)
{
    // todo
}

void EyeseeCamera::_addCallbackBuffer(CMediaMemory &callbackBuffer, int msgType)
{
    // todo
}

void EyeseeCamera::setPreviewCallback(PreviewCallback *pCb)
{
    // todo
}

void EyeseeCamera::setOneShotPreviewCallback(PreviewCallback *pCb)
{
    // todo
}

void EyeseeCamera::setPreviewCallbackWithBuffer(PreviewCallback *pCb)
{
    // todo
}

void EyeseeCamera::setHasPreviewCallback(bool installed, bool manualBuffer)
{
    // todo
}

void EyeseeCamera::setMODDataCallback(MODDataCallback *pCb)
{
    mpMODDataCallback = pCb;
}
void EyeseeCamera::setAdasDataCallback(AdasDataCallback *pCb)
{
    mpAdasDataCallback = pCb;
}
/*status_t  EyeseeCamera::setWaterMark(int enable, const char *str)
{
    // todo
    return NO_ERROR;
}

status_t EyeseeCamera::setOSDRects(int chnId, std::list<OSDRectInfo> &rects)
{
    return mpVIDevice->setOSDRects(chnId, rects);
}

status_t EyeseeCamera::getOSDRects(int chnId, std::list<OSDRectInfo> **ppRects)
{
    return mpVIDevice->getOSDRects(chnId, ppRects);
}

status_t EyeseeCamera::OSDOnOff(int chnId, bool bOnOff)
{
    return mpVIDevice->OSDOnOff(chnId, bOnOff);
}*/

RGN_HANDLE EyeseeCamera::createRegion(const RGN_ATTR_S *pstRegion)
{
    return mpVIDevice->createRegion(pstRegion);
}
status_t EyeseeCamera::getRegionAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRgnAttr)
{
    return mpVIDevice->getRegionAttr(Handle, pstRgnAttr);
}
status_t EyeseeCamera::setRegionBitmap(RGN_HANDLE Handle, const BITMAP_S *pBitmap)
{
    return mpVIDevice->setRegionBitmap(Handle, pBitmap);
}
status_t EyeseeCamera::attachRegionToChannel(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr)
{
    return mpVIDevice->attachRegionToChannel(Handle, chnId, pstChnAttr);
}
status_t EyeseeCamera::detachRegionFromChannel(RGN_HANDLE Handle, int chnId)
{
    return mpVIDevice->detachRegionFromChannel(Handle, chnId);
}
status_t EyeseeCamera::setRegionDisplayAttr(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr)
{
    return mpVIDevice->setRegionDisplayAttr(Handle, chnId, pstChnAttr);
}
status_t EyeseeCamera::getRegionDisplayAttr(RGN_HANDLE Handle, int chnId, RGN_CHN_ATTR_S *pstChnAttr)
{
    return mpVIDevice->getRegionDisplayAttr(Handle, chnId, pstChnAttr);
}
status_t EyeseeCamera::destroyRegion(RGN_HANDLE Handle)
{
    return mpVIDevice->destroyRegion(Handle);
}

bool EyeseeCamera::enableShutterSound(bool enabled)
{
    // todo
    return true;
}

void EyeseeCamera::setErrorCallback(ErrorCallback *pCb)
{
    mpErrorCallback = pCb;
}

void EyeseeCamera::setInfoCallback(InfoCallback *pCb)
{
    mpInfoCallback = pCb;
}

CameraRecordingProxy* EyeseeCamera::getRecordingProxy()
{
    alogv("getProxy");
    return new CameraProxy(this);
}

EyeseeCamera::CameraProxy::CameraProxy(EyeseeCamera *pCamera)
{
    mpCamera = pCamera;
}

// CameraRecordingProxy interface
status_t EyeseeCamera::CameraProxy::startRecording(int chnId, CameraRecordingProxyListener *pListener, int recorderId)
{
    return mpCamera->startRecording(chnId, pListener, recorderId);
}

status_t EyeseeCamera::CameraProxy::stopRecording(int chnId, int recorderId)
{
    return mpCamera->stopRecording(chnId, recorderId);
}

void EyeseeCamera::CameraProxy::releaseRecordingFrame(int chnId, uint32_t frameIndex)
{
    return mpCamera->releaseRecordingFrame(chnId, frameIndex);
}

status_t EyeseeCamera::CameraProxy::setChannelDisplay(int chnId, int hlay)
{
    return mpCamera->setChannelDisplay(chnId, hlay);
}

status_t EyeseeCamera::CameraProxy::getParameters(int chnId, CameraParameters &param)
{
    return mpCamera->getParameters(chnId, param);
}

status_t EyeseeCamera::CameraProxy::setParameters(int chnId, CameraParameters &param)
{
    return mpCamera->setParameters(chnId, param);
}

void EyeseeCamera::CameraProxy::increaseBufRef(int chnId, VIDEO_FRAME_BUFFER_S *pBuf)
{
    return mpCamera->increaseBufRef(chnId, pBuf);
}

}; /* namespace EyeseeLinux */

