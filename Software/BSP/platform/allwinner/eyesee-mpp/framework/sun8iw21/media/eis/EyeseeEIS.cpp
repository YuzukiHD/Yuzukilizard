

#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeEIS"
#include <utils/plat_log.h>

#include "EISDevice.h"
#include "EyeseeEIS.h"


using namespace std;

namespace EyeseeLinux {

class EyeseeEIS;

class EyeseeEISContext : public DataListener, public NotifyListener
{
public:
	EyeseeEISContext(EyeseeEIS *pEIS) : mpEIS(pEIS) {}
	~EyeseeEISContext(){}

	virtual void postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize);
	virtual void notify(int msgType, int chnId, int ext1, int ext2);

private:
	EyeseeEIS *mpEIS;
	Mutex mLock;
};

//	data listener for eyeseeEIS, will be registered into EIS component layer		
void EyeseeEISContext::postData(int msgType, int chnId, const std::shared_ptr<CMediaMemory>& dataPtr, size_t dataSize)
{
	EyeseeEIS::postEventFromNative(mpEIS, msgType, chnId, (int)dataSize, 0, &dataPtr);
}

// info listener for eyeseeEIS, will be registered into EIS component layer 		
void EyeseeEISContext::notify(int msgType, int chnId, int ext1, int ext2)
{
	EyeseeEIS::postEventFromNative(mpEIS, msgType, chnId, ext1, ext2, NULL);
}

// used to post the msg sent by EIS component to msg queue,and wait to be dispatched	
void EyeseeEIS::postEventFromNative(EyeseeEIS *pEIS, int what, int arg1, int arg2, int arg3, 
										const std::shared_ptr<CMediaMemory>* pDataPtr)
{
	CallbackMessage msg;

	msg.what = what;
	msg.arg1 = arg1;
	msg.arg2 = arg2;
	msg.extArgs.push_back(arg3);
	if(pDataPtr)
	{
		msg.mDataPtr = *pDataPtr;
	}
	pEIS->mpEventHandler->post(msg);
} 

// will be called by msg dsipather,and process the msg,then call the listener registered by app samuel.zhou
void EyeseeEIS::EventHandler::handleMessage(const CallbackMessage &msg) 		// notify app something happened this layer
{
	switch (msg.what) {
		case CAMERA_MSG_SHUTTER:
		{
			AutoMutex lock(mpEIS->mSetTakePictureCallbackLock);
			int chnId = msg.arg1;
			std::map<int, EyeseeEIS::TakePictureCallback>::iterator it = mpEIS->mTakePictureCallback.find(chnId);
			if(it == mpEIS->mTakePictureCallback.end())
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
			AutoMutex lock(mpEIS->mSetTakePictureCallbackLock);
			int chnId = msg.arg1;
			std::map<int, EyeseeEIS::TakePictureCallback>::iterator it = mpEIS->mTakePictureCallback.find(chnId);
			if(it == mpEIS->mTakePictureCallback.end())
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
						it->second.mpRawImageCallback->onPictureTaken(chnId, pData, nSize, mpEIS);
					}
				}
			}
            mpEIS->mpEISDevice->notifyPictureRelease(chnId);
			return;
		}

		case CAMERA_MSG_COMPRESSED_IMAGE:
		{
			AutoMutex lock(mpEIS->mSetTakePictureCallbackLock);
			int chnId = msg.arg1;
			std::map<int, EyeseeEIS::TakePictureCallback>::iterator it = mpEIS->mTakePictureCallback.find(chnId);
			if(it == mpEIS->mTakePictureCallback.end())
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
						it->second.mpJpegCallback->onPictureTaken(chnId, pData, nSize, mpEIS);
					}
				}
			}
            mpEIS->mpEISDevice->notifyPictureRelease(chnId);
			return;
		}

		case CAMERA_MSG_PREVIEW_FRAME:
		{
			if (mpEIS->mpPreviewCallback != NULL) 
			{
				alogw("only for test, for fun. Don't use it now");
				if(msg.mDataPtr)
				{
					if(msg.mDataPtr->getPointer() != NULL)
					{
						int chnId = msg.arg1;
						const void *pData = msg.mDataPtr->getPointer();
						int nSize = msg.arg2;
						mpEIS->mpPreviewCallback->onPreviewFrame(pData, nSize, mpEIS);
					}
				}
			}
			return;
		}

		case CAMERA_MSG_POSTVIEW_FRAME:
		{
            int chnId = msg.arg1;
			if (mpEIS->mpPostviewCallback != NULL)
			{
				if(msg.mDataPtr)
				{
					if(msg.mDataPtr->getPointer() != NULL)
					{
						const void *pData = msg.mDataPtr->getPointer();
						int nSize = msg.arg2;
						mpEIS->mpPostviewCallback->onPictureTaken(chnId, pData, nSize, mpEIS);
						//mpEIS->postDataCompleted(msg.arg1, msg.obj, msg.arg2);
					}
				}
			}
            mpEIS->mpEISDevice->notifyPictureRelease(chnId);
			return;
		}

		case CAMERA_MSG_ERROR :
		{
			aloge("Channel %d error %d", msg.arg1, msg.arg2);
			if (mpEIS->mpErrorCallback != NULL) {
				mpEIS->mpErrorCallback->onError(msg.arg1, msg.arg2, mpEIS);
			}
			return;
		}

		case CAMERA_MSG_INFO:
		{
			alogd("vo notify EyeseeEIS start preview");
			int chnId = msg.arg1;
			CameraMsgInfoType what = (CameraMsgInfoType)msg.arg2;
			int extra = 0;
			if(msg.extArgs.size() > 0)
			{
				extra = msg.extArgs[0];
			}
			if (mpEIS->mpInfoCallback != NULL) {
				mpEIS->mpInfoCallback->onInfo(chnId, what, extra, mpEIS);
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


unsigned int EyeseeEIS::gEISIdCounter = EISIdPrefixMark|0x00;

EyeseeEIS::EyeseeEIS() : mEISId(gEISIdCounter++) , mpShutterCallback(NULL) , 
						 mpRawImageCallback(NULL) , mpJpegCallback(NULL) , 
						 mpPreviewCallback(NULL) , mpPostviewCallback(NULL) , mpErrorCallback(NULL)
{
	alogd("EyeseeEIS constructing!");
	mpEISDevice = new EISDevice(mEISId);
	mpEventHandler = new EventHandler(this);
	mpNativeContext = new EyeseeEISContext(this);
}

EyeseeEIS::~EyeseeEIS()
{
	alogd("EyeseeEIS destorying!");
	if (mpNativeContext)
	{
		delete mpNativeContext;
	}
	if (mpEISDevice)
	{
		delete mpEISDevice;
	}
	if (mpEventHandler)
	{
		delete mpEventHandler;
	}
}

EyeseeEIS *EyeseeEIS::open()
{
	return new EyeseeEIS();
}

void EyeseeEIS::close(EyeseeEIS *peis)
{
	delete peis;
}

status_t EyeseeEIS::prepareDevice(EIS_ATTR_S *pEisChnAttr)
{
	return mpEISDevice->prepare(pEisChnAttr);
}

status_t EyeseeEIS::releaseDevice()
{
	return mpEISDevice->release();
}

status_t EyeseeEIS::startDevice()
{
    return mpEISDevice->start();
}

status_t EyeseeEIS::stopDevice()
{
    return mpEISDevice->stop();
} 

EIS_CHN EyeseeEIS::openChannel(EIS_ATTR_S *pChnAttr)
{
    EISDevice *pEISDevice = mpEISDevice;
	EyeseeEISContext *pContext = mpNativeContext;
	
    EIS_CHN nChnId = pEISDevice->openChannel(pChnAttr);
    if (nChnId == MM_INVALID_CHN) 
    {
        aloge("Failed to open channel");
        return nChnId;
    }
	
    pEISDevice->setDataListener(nChnId, pContext);
    pEISDevice->setNotifyListener(nChnId, pContext);
	
    return nChnId;
}

status_t EyeseeEIS::startChannel(int chnId)
{
    return mpEISDevice->startChannel(chnId);
}

status_t EyeseeEIS::closeChannel(EIS_CHN chnId)
{
    return mpEISDevice->closeChannel(chnId);
} 


status_t EyeseeEIS::setParameters(int chnId, CameraParameters &param)
{
    return mpEISDevice->setParameters(chnId, param);
}

status_t EyeseeEIS::getParameters(int chnId, CameraParameters &param)
{
    return mpEISDevice->getParameters(chnId, param);
}

status_t EyeseeEIS::EISProxy::getParameters(int chnId, CameraParameters &param)
{
	return mpEIS->getParameters(chnId, param);
}

status_t EyeseeEIS::EISProxy::setParameters(int chnId, CameraParameters &param)
{
	return mpEIS->setParameters(chnId, param);
}


status_t EyeseeEIS::setChannelDisplay(int chnId, int hlay)
{
    return mpEISDevice->setChannelDisplay(chnId, hlay);
}

status_t EyeseeEIS::EISProxy::setChannelDisplay(int chnId, int hlay)
{
	return mpEIS->setChannelDisplay(chnId, hlay);
}


bool EyeseeEIS::previewEnabled(int chnId)
{
    return mpEISDevice->previewEnabled(chnId);
} 


status_t EyeseeEIS::startRender(int chnId)
{
    return mpEISDevice->startRender(chnId);
}

status_t EyeseeEIS::stopRender(int chnId)
{
    return mpEISDevice->stopRender(chnId);
}

status_t EyeseeEIS::setEISChnAttr(EIS_ATTR_S *pAttr)
{
    return mpEISDevice->setEISChnAttr(pAttr);
} 

//Used to set proxy of providing source data
status_t EyeseeEIS::setCamera(std::vector<EIS_CameraChannelInfo>& cameraChannels)
{
	return mpEISDevice->setCamera(cameraChannels);
}

// recorder get proxy for return yuv after encode one frame
CameraRecordingProxy* EyeseeEIS::getRecordingProxy()
{
    alogd("getProxy");
    return new EISProxy(this);
}


// recording frm: processed frm by component			
void EyeseeEIS::releaseRecordingFrame(int chnId, uint32_t index)
{
	mpEISDevice->releaseRecordingFrame(chnId, index);
}

void EyeseeEIS::EISProxy::releaseRecordingFrame(int chnId, uint32_t frameIndex)
{
	return mpEIS->releaseRecordingFrame(chnId, frameIndex);
} 

// will register proxy listener defined in other module to EIS
status_t EyeseeEIS::startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
	return mpEISDevice->startRecording(chnId, pCb, recorderId);
}

status_t EyeseeEIS::stopRecording(int chnId, int recorderId)
{
	return mpEISDevice->stopRecording(chnId, recorderId);
}

	// CameraRecordingProxy interface
status_t EyeseeEIS::EISProxy::startRecording(int chnId, CameraRecordingProxyListener *pListener, int recorderId)
{
	return mpEIS->startRecording(chnId, pListener, recorderId);
}

status_t EyeseeEIS::EISProxy::stopRecording(int chnId, int recorderId)
{
	return mpEIS->stopRecording(chnId, recorderId);
}


void EyeseeEIS::setErrorCallback(ErrorCallback *pCb)
{
	mpErrorCallback = pCb;
}

void EyeseeEIS::setInfoCallback(InfoCallback *pCb)
{
	mpInfoCallback = pCb;
}

void EyeseeEIS::setPreviewCallback(PreviewCallback *pCb)
{
	mpPreviewCallback = pCb;
}

status_t EyeseeEIS::takePicture(int chnId, ShutterCallback *pShutter, PictureCallback *pRaw,
										PictureCallback *pJpeg, PictureRegionCallback *pPicReg)
{
	return takePicture(chnId, pShutter, pRaw, NULL, pJpeg, pPicReg);
}

status_t EyeseeEIS::takePicture(int chnId, ShutterCallback *pShutter,
									PictureCallback *pRaw, PictureCallback *pPostview, 
									PictureCallback *pJpeg, PictureRegionCallback *pPicReg)
{
	
	unsigned int msgType = 0;
	status_t ret = NO_ERROR;
	EyeseeEISContext *context = (EyeseeEISContext*)mpNativeContext; 
	EISDevice *pEISDevice = mpEISDevice;
	
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

	ret = pEISDevice->takePicture(chnId, msgType, pPicReg);
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

status_t EyeseeEIS::cancelContinuousPicture(int chnId)
{
	status_t ret = mpEISDevice->cancelContinuousPicture(chnId);
	if (ret != NO_ERROR)
	{
		aloge("cancelContinuousPicture failed[0x%x]", ret);
	}
	return ret;
} 


};


