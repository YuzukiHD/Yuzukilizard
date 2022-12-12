

#define LOG_NDEBUG 0
#define LOG_TAG "EISChannel"

#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>

#include <ion_memmanager.h>
#include <memoryAdapter.h>
#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <MediaStructConvert.h>

#include <SystemBase.h>
#include <mpi_eis.h>
#include <mpi_videoformat_conversion.h>

#include "EISChannel.h"

#include <ConfigOption.h>

using namespace std;

namespace EyeseeLinux {


// DoCaptureProcess start -->
EISChannel::DoCaptureProcess::DoCaptureProcess(EISChannel *pEisChn): mpEisChn(pEisChn)
{
}

status_t EISChannel::DoCaptureProcess::SendCommand_TakePicture()
{
	EyeseeMessage msg;
	
	msg.mMsgType = MsgTypeCapture_TakePicture;
	mMsgQueue.queueMessage(&msg);
	
	return NO_ERROR;
}

status_t EISChannel::DoCaptureProcess::SendCommand_CancelContinuousPicture()
{
	EyeseeMessage msg;
	
	msg.mMsgType = MsgTypeCapture_CancelContinuousPicture;
	mMsgQueue.queueMessage(&msg);

	return NO_ERROR;
}
// DoCaptureProcess end <--

// DoPreviewThread start -->
EISChannel::DoPreviewThread::DoPreviewThread(EISChannel *pEisChn)
    : mpEisChn(pEisChn)
    , mbWaitReleaseAllFrames(false)
    , mbWaitPreviewFrame(false)
{
}

status_t EISChannel::DoPreviewThread::startThread()
{
    status_t ret = run("EISChnPreviewTh");
	
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void EISChannel::DoPreviewThread::stopThread()
{
	EyeseeMessage msg;
	
	msg.mMsgType = MsgTypePreview_Exit;
	mMsgQueue.queueMessage(&msg);

	join();
	
	mMsgQueue.flushMessage();
}

status_t EISChannel::DoPreviewThread::notifyNewFrameCome()
{
	AutoMutex lock(mWaitLock);
	
	if(mbWaitPreviewFrame)
	{
		mbWaitPreviewFrame = false;
		
		EyeseeMessage msg; 
		msg.mMsgType = MsgTypePreview_InputFrameAvailable;

		mMsgQueue.queueMessage(&msg);
	}
	return NO_ERROR;
}

status_t EISChannel::DoPreviewThread::releaseAllFrames()
{
	AutoMutex lock(mWaitLock);
	
	mbWaitReleaseAllFrames = true;

	EyeseeMessage msg;
	msg.mMsgType = MsgTypePreview_releaseAllFrames;

	mMsgQueue.queueMessage(&msg);

	while(mbWaitReleaseAllFrames)
	{
		mCondReleaseAllFramesFinished.wait(mWaitLock);
	}
	
	return NO_ERROR;
}

bool EISChannel::DoPreviewThread::threadLoop() 
{
	if(!exitPending()) 
	{
		return mpEisChn->previewThread();	// to show and encode
	} 
	else 
	{
		return false;
	}
}
//DoPreviewThread end <--

//DoPictureThread start -->
EISChannel::DoPictureThread::DoPictureThread(EISChannel *pEisChn)
    : mpEisChn(pEisChn)
    , mbWaitReleaseAllFrames(false)
    , mbWaitPictureFrame(false)
{
}

status_t EISChannel::DoPictureThread::startThread() 
{
	status_t ret = run("EISChnPictureTh");
	
	if(ret != NO_ERROR)
	{
		aloge("fatal error! run thread fail!");
	}
	return ret;
}

void EISChannel::DoPictureThread::stopThread()
{
	EyeseeMessage msg;
	
	msg.mMsgType = MsgTypePicture_Exit;
	mMsgQueue.queueMessage(&msg);
	
	join();
	
	mMsgQueue.flushMessage();
}

status_t EISChannel::DoPictureThread::notifyNewFrameCome()
{
	AutoMutex lock(mWaitLock);
	
	if(mbWaitPictureFrame)
	{
		mbWaitPictureFrame = false;
		
		EyeseeMessage msg;
		msg.mMsgType = MsgTypePicture_InputFrameAvailable;
		
		mMsgQueue.queueMessage(&msg);
	}
	return NO_ERROR;
}

status_t EISChannel::DoPictureThread::releaseAllFrames()
{
    AutoMutex lock(mWaitLock);
	
    mbWaitReleaseAllFrames = true;
	
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePicture_releaseAllFrames;
	
    mMsgQueue.queueMessage(&msg);
    while(mbWaitReleaseAllFrames)
    {
        mCondReleaseAllFramesFinished.wait(mWaitLock);
    }
    return NO_ERROR;
}

status_t EISChannel::DoPictureThread::notifyPictureEnd()
{
	EyeseeMessage msg;
	
	msg.mMsgType = MsgTypePicture_SendPictureEnd;
	mMsgQueue.queueMessage(&msg);
	
	return NO_ERROR;
}

bool EISChannel::DoPictureThread::threadLoop() 
{
	if(!exitPending()) 
	{
		return mpEisChn->pictureThread();
	} 
	else 
	{
		return false;
	}
}
// DoPictureThread  end <--

// DoCommandThread start -->
EISChannel::DoCommandThread::DoCommandThread(EISChannel *pEisChn): mpEisChn(pEisChn)
{
}

status_t EISChannel::DoCommandThread::startThread()
{
	status_t ret = run("EISChnCommand");
	
	if(ret != NO_ERROR)
	{
		aloge("fatal error! run thread fail!");
	}
	return ret;
}

void EISChannel::DoCommandThread::stopThread()
{
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeCommand_Exit;
	
	mMsgQueue.queueMessage(&msg);
	join();
	mMsgQueue.flushMessage();
}

status_t EISChannel::DoCommandThread::SendCommand_TakePicture(unsigned int msgType)
{
    EyeseeMessage msg;
	
    msg.mMsgType = MsgTypeCommand_TakePicture;
    msg.mPara0 = msgType;
	
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t EISChannel::DoCommandThread::SendCommand_CancelContinuousPicture()
{
    EyeseeMessage msg;
	
    msg.mMsgType = MsgTypeCommand_CancelContinuousPicture;
	
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
} 
	
bool EISChannel::DoCommandThread::threadLoop()
{
	if(!exitPending()) 
	{
		return mpEisChn->commandThread();
	} 
	else 
	{
		return false;
	}
}
//DoCommandThread  end <--


EISChannel::EISChannel(EIS_GRP nGroupId, EIS_ATTR_S *pChnAttr) : mpPrevThread(NULL)
																	 , mpPicThread(NULL)
																	 , mpCallbackNotifier(NULL)
																	 , mpPreviewWindow(NULL)
																	 , mpPreviewQueue(NULL)
																	 , mpPictureQueue(NULL)
																	 , mNewZoom(0)
																	 , mLastZoom(-1)
																	 , mMaxZoom(0xffffffff)
																	 , mpMemOps(NULL)
																	 , mChannelState(EIS_CHN_STATE_CONSTRUCTED)
																	 , mTakePictureMode(TAKE_PICTURE_MODE_NULL)
																	 , mbProcessTakePictureStart(false)
																	 , mContinuousPictureCnt(0)
																	 , mContinuousPictureMax(0)
																	 , mContinuousPictureStartTime(0)
																	 , mContinuousPictureLast(0)
																	 , mContinuousPictureInterval(0) 
																	 , mGroupId(nGroupId)
																	 , mFrameWidth(0)
																	 , mFrameHeight(0)
																	 , mFrameCounter(0)
{
    memset(&mRectCrop, 0, sizeof(mRectCrop));
    memset(&mPicFrmBuffer, 0, sizeof(mPicFrmBuffer));
    memcpy(&mAttr, pChnAttr, sizeof(EIS_ATTR_S));
    //mColorSpace = V4L2_COLORSPACE_JPEG;

    ERRORTYPE ret;
	mChannelId = 0;
#if 0
	ret = AW_MPI_EIS_CreateChn(mChannelId,&mAttr); 
	if(SUCCESS == ret)
	{
		alogd("create EIS scaler channel[%d] success!", mChannelId);
	}
	else
	{
		mChannelId = MM_INVALID_CHN;
		aloge("fatal error! create EIS scaler channel fail!");
	} 
#endif /* 0 */

	AW_MPI_EIS_EnableChn(mGroupId);

    //AW_MPI_EIS_SetPortAttr(mGroupId, mChannelId, &mAttr);
    if (NO_ERROR != GetChnOutFrameSize(mChannelId, &mAttr, mFrameWidth, mFrameHeight))
    {
        aloge("get eis chn out frame size error!");
    }

    mpCallbackNotifier = new CallbackNotifier(mChannelId, this);
	
    mbPreviewEnable = true;
    mpPreviewWindow = new PreviewWindow(this); 
	
    mpPreviewQueue = new OSAL_QUEUE;
    OSAL_QueueCreate(mpPreviewQueue, EIS_MAX_BUFFER_NUM);
	
    mpPictureQueue = new OSAL_QUEUE;
    OSAL_QueueCreate(mpPictureQueue, EIS_MAX_BUFFER_NUM);

    mpCapProcess = new DoCaptureProcess(this); 
    mpPrevThread = new DoPreviewThread(this); 
    mpPicThread = new DoPictureThread(this); 
    mpCommandThread = new DoCommandThread(this);
    //mpCommandThread->startThread(); 

    mTakePicMsgType = 0;
    mbTakePictureStart = false;
    mbProcessTakePictureStart = false;
    mParameters.setPictureMode(TAKE_PICTURE_MODE_FAST);
} 

EISChannel::~EISChannel()
{
	ERRORTYPE ret;
	
	if(mGroupId != MM_INVALID_DEV)
	{
		ret = AW_MPI_EIS_DisableChn(mGroupId);
		if(SUCCESS != ret)
		{
			aloge("fatal error! AW_MPI_EIS_DisableChn fail!");
		}
		
		mGroupId = MM_INVALID_DEV;
	}
	
	if (mpCommandThread != NULL)
	{
		mpCommandThread->stopThread();
		delete mpCommandThread;
	}
	if(mpCapProcess != NULL)
	{
		delete mpCapProcess;
	}
	if (mpPrevThread != NULL)
	{
		mpPrevThread->stopThread();
		delete mpPrevThread;
	}
	if (mpPicThread != NULL)
	{
		mpPicThread->stopThread();
		delete mpPicThread;
	}

	OSAL_QueueTerminate(mpPreviewQueue);
	delete mpPreviewQueue;
	mpPreviewQueue = NULL;

	OSAL_QueueTerminate(mpPictureQueue);
	delete mpPictureQueue;
	mpPictureQueue = NULL;

	if (mpPreviewWindow != NULL) {
		delete mpPreviewWindow;
	}
	if (mpCallbackNotifier != NULL) {
		delete mpCallbackNotifier;
	}
} 

status_t EISChannel::GetChnOutFrameSize(EIS_CHN chnId, EIS_ATTR_S *pAttr, int &mFrameWidth, int &mFrameHeight)
{
	if ((chnId == MM_INVALID_CHN) || (NULL == pAttr))
	{
		return BAD_VALUE;
	}
	
	mFrameWidth  = pAttr->iVideoOutWidth;
	mFrameHeight = pAttr->iVideoOutHeight;

	return NO_ERROR;
}

status_t EISChannel::startChannel()
{
	if(mbPreviewEnable)
	{
		mpPreviewWindow->startPreview();
	}
	
	mpPrevThread->startThread();
	mpPicThread->startThread();
	mpCommandThread->startThread();

	mChannelState = EIS_CHN_STATE_STARTED;

	return NO_ERROR;
}

status_t EISChannel::stopChannel()
{
    AutoMutex lock1(mLock);

    if (mChannelState != EIS_CHN_STATE_STARTED)
    {
        aloge("stopChannel in error state %d", mChannelState);
        return INVALID_OPERATION;
    }

    alogv("stopChannel");
    if (mbPreviewEnable == true)
    {
        mpPreviewWindow->stopPreview();
    }
	
    mpPrevThread->releaseAllFrames();
    mpPicThread->releaseAllFrames();

    return NO_ERROR;
} 

EIS_CHN EISChannel::getChannelId()
{
    AutoMutex lock(mLock);
    return mChannelId;
}


status_t EISChannel::setParameters(CameraParameters &param)
{
	mParameters = param;
	mColorSpace = mParameters.getColorSpace(); 
	int rotation = param.getPreviewRotation();
	
	if(rotation!=0 && rotation!=90 && rotation!=180 && rotation!=270 && rotation!=360)
	{
		aloge("fatal error! eis rotation[%d] is invalid!", rotation);
	}
	else
	{
		alogd("eis paramPreviewRotation set to %d degree", rotation);
		mpPreviewWindow->setPreviewRotation(rotation);
	}
	return NO_ERROR;
}

status_t EISChannel::getParameters(CameraParameters &param)
{
	param = mParameters;
	
	return NO_ERROR;
}

status_t EISChannel::setChannelDisplay(int hlay)
{
	return mpPreviewWindow->setPreviewWindow(hlay);
}

bool EISChannel::isPreviewEnabled()
{
	return mpPreviewWindow->isPreviewEnabled();
}

status_t EISChannel::startRender()
{
	status_t ret = NO_ERROR;
	AutoMutex autoLock(mLock);
	
	mbPreviewEnable = true;
	if (EIS_CHN_STATE_STARTED == mChannelState)
	{
		mpPreviewWindow->setDispBufferNum(2);
		ret = mpPreviewWindow->startPreview();
	}
	return ret;
}

status_t EISChannel::stopRender()
{
	status_t ret = NO_ERROR;
	
	AutoMutex autoLock(mLock);
	
	mbPreviewEnable = false;
	ret = mpPreviewWindow->stopPreview();
	return ret;
}

status_t EISChannel::releaseAllEISFrames()
{
	mpPrevThread->releaseAllFrames();
	mpPicThread->releaseAllFrames();
	
	return NO_ERROR;
}

status_t EISChannel::releaseRecordingFrame(uint32_t index)
{
	VIDEO_FRAME_BUFFER_S *pFrmbuf = NULL;
	
	mFrameBuffersLock.lock();
	for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mFrameBuffers.begin(); it!=mFrameBuffers.end(); ++it)
	{
		if(it->mFrameBuf.mId == index)
		{
			if(NULL == pFrmbuf)
			{
				pFrmbuf = &*it;
			}
			else
			{
				aloge("fatal error! MPP-EIS frame id[0x%x] is repeated!", index);
			}
		}
	}
	mFrameBuffersLock.unlock();
	
	if(NULL == pFrmbuf)
	{
		aloge("fatal error! not find frame index[0x%x]", index);
		return UNKNOWN_ERROR;
	}

	int ret;
	AutoMutex lock(mRefCntLock);

	if (pFrmbuf->mRefCnt > 0 && --pFrmbuf->mRefCnt == 0) 	// only when refcnt equal to 0, frm can be released.
	{
		ret = AW_MPI_EIS_ReleaseData(mGroupId, &pFrmbuf->mFrameBuf);
		if (ret != SUCCESS) 
		{
			aloge("AW_MPI_EIS_ReleaseFrame error! ChnId:%d, mPicId:%d", mGroupId, pFrmbuf->mFrameBuf.mId);
		}
	}
	return NO_ERROR;
}

status_t EISChannel::startRecording(CameraRecordingProxyListener *pCb, int recorderId)
{
	return mpCallbackNotifier->startRecording(pCb, recorderId);
}

status_t EISChannel::stopRecording(int recorderId)
{
	return mpCallbackNotifier->stopRecording(recorderId);
}

void EISChannel::setDataListener(DataListener *pCb)
{
	mpCallbackNotifier->setDataListener(pCb);
}

void EISChannel::setNotifyListener(NotifyListener *pCb)
{
	mpCallbackNotifier->setNotifyListener(pCb);
}


status_t EISChannel::doTakePicture(unsigned int msgType)
{
	AutoMutex lock(mLock);
	
	int jpeg_quality = mParameters.getJpegQuality(); 
	if (jpeg_quality <= 0) {
		jpeg_quality = 90;
	}
	mpCallbackNotifier->setJpegQuality(jpeg_quality);

	int jpeg_rotate = mParameters.getJpegRotation();
	if (jpeg_rotate <= 0) {
		jpeg_rotate = 0;
	}
	mpCallbackNotifier->setJpegRotate(jpeg_rotate);

	SIZE_S size;
	mParameters.getPictureSize(size); 
	mpCallbackNotifier->setPictureSize(size.Width, size.Height);
	
	mParameters.getJpegThumbnailSize(size);
	mpCallbackNotifier->setJpegThumbnailSize(size.Width, size.Height);

	char *pGpsMethod = mParameters.getGpsProcessingMethod();
	if (pGpsMethod != NULL) {
		mpCallbackNotifier->setGPSLatitude(mParameters.getGpsLatitude());
		mpCallbackNotifier->setGPSLongitude(mParameters.getGpsLongitude());
		mpCallbackNotifier->setGPSAltitude(mParameters.getGpsAltitude());
		mpCallbackNotifier->setGPSTimestamp(mParameters.getGpsTimestamp());
		mpCallbackNotifier->setGPSMethod(pGpsMethod);
	}
	
	mTakePicMsgType = msgType;
	mpCallbackNotifier->enableMessage(msgType);

	mTakePictureMode = mParameters.getPictureMode();
	mpCapProcess->SendCommand_TakePicture();

	return NO_ERROR;
}

status_t EISChannel::doCancelContinuousPicture()
{
	AutoMutex lock(mLock);
	
	mpCapProcess->SendCommand_CancelContinuousPicture();
	return NO_ERROR;
}

status_t EISChannel::takePicture(unsigned int msgType, PictureRegionCallback *pPicReg)
{
	AutoMutex lock(mLock);
	
	if(mbTakePictureStart)
	{
		aloge("fatal error! EIS During taking picture, we don't accept new takePicture command!");
		return UNKNOWN_ERROR;
	}
	else
	{
		mbTakePictureStart = true;
		
		mpCallbackNotifier->setPictureRegionCallback(pPicReg);
		mpCommandThread->SendCommand_TakePicture(msgType);
		return NO_ERROR;
	}
}

status_t EISChannel::notifyPictureRelease()
{
    return mpCallbackNotifier->notifyPictureRelease();
}

status_t EISChannel::cancelContinuousPicture()
{
	AutoMutex lock(mLock);
	
	if(!mbTakePictureStart)
	{
		aloge("fatal error! EIS not start take picture!");
		return NO_ERROR;
	}
	mpCommandThread->SendCommand_CancelContinuousPicture();
	return NO_ERROR;
}

void EISChannel::increaseBufRef(VIDEO_FRAME_BUFFER_S *pBuf)
{
	VIDEO_FRAME_BUFFER_S *pFrame = (VIDEO_FRAME_BUFFER_S*)pBuf;
	
	AutoMutex lock(mRefCntLock);
	++pFrame->mRefCnt;		// when someone want to use this frm, the counter of this frm will be increased
}

void EISChannel::decreaseBufRef(unsigned int nBufId)
{
	releaseRecordingFrame(nBufId);
}

// will be called in previewwindow			
void EISChannel::NotifyRenderStart()
{
	mpCallbackNotifier->NotifyRenderStart();
}




void EISChannel::process()			// call in capture thread	
{
	bool bRunningFlag = true;
	//bool bTakePictureStart = false;
	EyeseeMessage msg;
	status_t getMsgRet;
	ERRORTYPE ret;
	
PROCESS_MESSAGE:
	getMsgRet = mpCapProcess->mMsgQueue.dequeueMessage(&msg); 
	if(getMsgRet == NO_ERROR)
	{
		if(DoCaptureProcess::MsgTypeCapture_TakePicture == msg.mMsgType)
		{
			if(!mbProcessTakePictureStart)
			{
				mbProcessTakePictureStart = true;
			}
			else
			{
				alogd("Be careful! take picture is doing already!");
			}
			alogd("EIS take picture mode is [0x%x]", mTakePictureMode);
            //set picture number to callbackNotifier
            int nPicNum = 0;
            switch(mTakePictureMode)
            {
                case TAKE_PICTURE_MODE_FAST:
                    nPicNum = 1;
                    break;
                case TAKE_PICTURE_MODE_CONTINUOUS:
                    nPicNum = mParameters.getContinuousPictureNumber();
                    break;
                default:
                    nPicNum = 1;
                    break;
            }
            mpCallbackNotifier->setPictureNum(nPicNum);
		}
		else if(DoCaptureProcess::MsgTypeCapture_CancelContinuousPicture == msg.mMsgType)
		{
			if(mbProcessTakePictureStart)
			{
				if(TAKE_PICTURE_MODE_CONTINUOUS == mTakePictureMode)
				{
					mpPicThread->notifyPictureEnd();
					
					mContinuousPictureStartTime = 0;
					mContinuousPictureLast = 0;
					mContinuousPictureInterval = 0;
					mContinuousPictureCnt = 0;
					mContinuousPictureMax = 0;
					
					mbProcessTakePictureStart = false;
				}
				else
				{
					aloge("fatal error! take picture mode[0x%x] is not continuous!", mTakePictureMode);
				}
			}
			else
			{
				aloge("fatal error! not start take picture, mode[0x%x]!", mTakePictureMode);
			}
		}
		else
		{
			aloge("unknown msg[0x%x]!", msg.mMsgType);
		}
		goto PROCESS_MESSAGE;
	}
	
	//VI_FRAME_BUF_INFO_S buffer;
	VIDEO_FRAME_INFO_S stVideoFrameInfo;
	ret = AW_MPI_EIS_GetData(mGroupId, &stVideoFrameInfo, 2000);
	if (ret != SUCCESS) 
	{
		aloge("AW_MPI_EIS_GetData error!");
		return;
	}
	//VIDEO_FRAME_BUFFER_S *pFrmbuf = mpVideoFrmBuffer + buffer.u32PoolId;
	VIDEO_FRAME_BUFFER_S *pFrmbuf = NULL;
	
	mFrameBuffersLock.lock();
	for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mFrameBuffers.begin(); it!=mFrameBuffers.end(); ++it)
	{
		if(it->mFrameBuf.mId == stVideoFrameInfo.mId)
		{
			if(NULL == pFrmbuf)
			{
				pFrmbuf = &*it;
			}
			else
			{
				aloge("fatal error! MPP-EIS frame id[0x%x] is repeated!", stVideoFrameInfo.mId);
			}
		}
	}
	
	if(NULL == pFrmbuf)
	{
		alogd("EISChannel[%d] frame buffer array did not contain this bufferId[0x%x], add it.",  mGroupId, stVideoFrameInfo.mId);
		VIDEO_FRAME_BUFFER_S newFrame;
		
		memset(&newFrame, 0, sizeof(VIDEO_FRAME_BUFFER_S));
		mFrameBuffers.push_back(newFrame);
		
		pFrmbuf = &mFrameBuffers.back();
	}
	
	pFrmbuf->mFrameBuf = stVideoFrameInfo;
	
	if(mRectCrop.Width != 0 && mRectCrop.Height != 0)
	{
		pFrmbuf->mFrameBuf.VFrame.mOffsetTop = mRectCrop.Y;
		pFrmbuf->mFrameBuf.VFrame.mOffsetBottom = mRectCrop.Y + mRectCrop.Height;
		pFrmbuf->mFrameBuf.VFrame.mOffsetLeft = mRectCrop.X;
		pFrmbuf->mFrameBuf.VFrame.mOffsetRight = mRectCrop.X + mRectCrop.Width;
	}
	
	pFrmbuf->mColorSpace = mColorSpace;
	//pFrmbuf->mIsThumbAvailable = 0;
	//pFrmbuf->mThumbUsedForPhoto = 0;
	pFrmbuf->mRefCnt = 1;
	mFrameBuffersLock.unlock();
	
	if(false == mbProcessTakePictureStart)
	{
		{
			AutoMutex lock(mRefCntLock);
			pFrmbuf->mRefCnt++;
		}
	
		OSAL_Queue(mpPreviewQueue, pFrmbuf);
	
		mpPrevThread->notifyNewFrameCome();
	}
	else			// to process picture taken operation	samuel.zhou
	{
		if (mTakePictureMode == TAKE_PICTURE_MODE_NORMAL)
		{
		   aloge("fatal error! EIS can't support normal mode take picture !");
		   mpPicThread->notifyPictureEnd();
		   
		   mbProcessTakePictureStart = false;
		} 
		else 
		{
			mRefCntLock.lock();
			pFrmbuf->mRefCnt++;
			mRefCntLock.unlock();
			
			OSAL_Queue(mpPreviewQueue, pFrmbuf); 
			mpPrevThread->notifyNewFrameCome();

			if (mTakePictureMode == TAKE_PICTURE_MODE_FAST)
			{
				mRefCntLock.lock();
				pFrmbuf->mRefCnt++;
				mRefCntLock.unlock();
				
				mIsPicCopy = false;
				OSAL_Queue(mpPictureQueue, pFrmbuf);
				
				mpPicThread->notifyNewFrameCome();
				mpPicThread->notifyPictureEnd();
				
				mbProcessTakePictureStart = false;
			}
			else if(mTakePictureMode == TAKE_PICTURE_MODE_CONTINUOUS)
			{
				bool bPermit = false;
				if (0 == mContinuousPictureStartTime)	 //let's begin!
				{
					mContinuousPictureStartTime = CDX_GetSysTimeUsMonotonic()/1000;
					
					mContinuousPictureLast = mContinuousPictureStartTime;
					mContinuousPictureInterval = mParameters.getContinuousPictureIntervalMs();
					
					mContinuousPictureCnt = 0;
					mContinuousPictureMax = mParameters.getContinuousPictureNumber();
					
					bPermit = true;
					alogd("begin continous picture, will take [%d]pics, interval[%llu]ms!", mContinuousPictureMax, mContinuousPictureInterval);
				}
				else
				{
					if(mContinuousPictureInterval <= 0)
					{
						bPermit = true;
					}
					else
					{
						uint64_t nCurTime = CDX_GetSysTimeUsMonotonic()/1000;
						if(nCurTime > mContinuousPictureLast + mContinuousPictureInterval)
						{
							bPermit = true;
						}
					}
				}

				if(bPermit)
				{
					mRefCntLock.lock();
					pFrmbuf->mRefCnt++;
					mRefCntLock.unlock();
					
					mIsPicCopy = false;
					OSAL_Queue(mpPictureQueue, pFrmbuf);
					mpPicThread->notifyNewFrameCome();
					
					mContinuousPictureCnt++;
					
					if(mContinuousPictureCnt >= mContinuousPictureMax)
					{
						mpPicThread->notifyPictureEnd();
						
						mContinuousPictureStartTime = 0;
						mContinuousPictureLast = 0;
						mContinuousPictureInterval = 0;
						mContinuousPictureCnt = 0;
						mContinuousPictureMax = 0;
						
						mbProcessTakePictureStart = false;
					}
					else
					{
						mContinuousPictureLast = mContinuousPictureStartTime+mContinuousPictureCnt*mContinuousPictureInterval;
					}
				}
			}
			else
			{
				aloge("fatal error! any other take picture mode[0x%x]?", mTakePictureMode);
				mbProcessTakePictureStart = false;
			}
		}
	}

	mFrameCounter++; 
	releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
}


bool EISChannel::previewThread()
{
	bool bRunningFlag = true;
	EyeseeMessage msg;
	status_t getMsgRet;
	ERRORTYPE ret;
	VIDEO_FRAME_BUFFER_S *pFrmbuf;
	
PROCESS_MESSAGE:
	getMsgRet = mpPrevThread->mMsgQueue.dequeueMessage(&msg);
	if(getMsgRet == NO_ERROR)
	{
		if(DoPreviewThread::MsgTypePreview_InputFrameAvailable == msg.mMsgType)
		{
		}
		else if(DoPreviewThread::MsgTypePreview_releaseAllFrames == msg.mMsgType)
		{
			AutoMutex lock(mpPrevThread->mWaitLock);
			for(;;)
			{
				pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPreviewQueue);
				if(pFrmbuf)
				{
					releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
				}
				else
				{
					break;
				}
			}
			
			if(mpPrevThread->mbWaitReleaseAllFrames)
			{
				mpPrevThread->mbWaitReleaseAllFrames = false;
			}
			else
			{
				aloge("fatal error! check code!");
			}
			
			mpPrevThread->mCondReleaseAllFramesFinished.signal();
		}
		else if(DoPreviewThread::MsgTypePreview_Exit == msg.mMsgType)
		{
			bRunningFlag = false;
			goto _exit0;
		}
		else
		{
			aloge("unknown msg[0x%x]!", msg.mMsgType);
		}
		goto PROCESS_MESSAGE;
	}
	
	pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPreviewQueue);
	if (pFrmbuf == NULL) 
	{
        {	// key point
		AutoMutex lock(mpPrevThread->mWaitLock);
		
		if(OSAL_GetElemNum(mpPreviewQueue) > 0)
		{
			alogd("Low probability! EIS preview new frame come before check again.");
			goto PROCESS_MESSAGE;
		}
		else
		{
			mpPrevThread->mbWaitPreviewFrame = true;
		}
        }
		mpPrevThread->mMsgQueue.waitMessage();
		
		goto PROCESS_MESSAGE;
	}

	mpCallbackNotifier->onNextFrameAvailable(pFrmbuf);	// -->EyeseeRecorder	one new frame	samuel.zhou
	mpPreviewWindow->onNextFrameAvailable(pFrmbuf); 	// to show

	releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
	//return true;
	goto PROCESS_MESSAGE;
_exit0:
	return bRunningFlag;
}


bool EISChannel::pictureThread()
{
	bool bRunningFlag = true;
	EyeseeMessage msg;
	status_t getMsgRet;
	ERRORTYPE ret;
	bool bDrainPictureQueue = false;
	VIDEO_FRAME_BUFFER_S *pFrmbuf;
	
PROCESS_MESSAGE:
	getMsgRet = mpPicThread->mMsgQueue.dequeueMessage(&msg);
	if(getMsgRet == NO_ERROR)
	{
		if(DoPictureThread::MsgTypePicture_InputFrameAvailable == msg.mMsgType)
		{
		}
		else if(DoPictureThread::MsgTypePicture_SendPictureEnd == msg.mMsgType)
		{
			bDrainPictureQueue = true;
		}
		else if(DoPictureThread::MsgTypePicture_releaseAllFrames == msg.mMsgType)
		{
			AutoMutex lock(mpPicThread->mWaitLock);
			for(;;)
			{
				pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPictureQueue);
				if(pFrmbuf)
				{
					releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
				}
				else
				{
					break;
				}
			}
			
			if(mpPicThread->mbWaitReleaseAllFrames)
			{
				mpPicThread->mbWaitReleaseAllFrames = false;
			}
			else
			{
				aloge("fatal error! check code!");
			}
			
			mpPicThread->mCondReleaseAllFramesFinished.signal();
		}
		else if(DoPictureThread::MsgTypePicture_Exit == msg.mMsgType)
		{
			bRunningFlag = false;
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
		pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPictureQueue);
		if (pFrmbuf == NULL) 
		{
			if(bDrainPictureQueue)
			{
				mpCallbackNotifier->disableMessage(mTakePicMsgType);
				bDrainPictureQueue = false;
				mpCallbackNotifier->takePictureEnd();
				
				mLock.lock();
				if(!mbTakePictureStart)
				{
					aloge("fatal error! why EIS takePictureStart is false when we finish take picture?");
				}
				mbTakePictureStart = false;
				mLock.unlock();
			}
			{
				AutoMutex lock(mpPicThread->mWaitLock);
				if(OSAL_GetElemNum(mpPictureQueue) > 0)
				{
					alogd("Low probability! EIS picture new frame come before check again.");
					goto PROCESS_MESSAGE;
				}
				else
				{
					mpPicThread->mbWaitPictureFrame = true;
				}
			}
			mpPicThread->mMsgQueue.waitMessage();
			goto PROCESS_MESSAGE;
		}

		mpCallbackNotifier->takePicture(pFrmbuf);

		releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
		
		if(!bDrainPictureQueue)
		{
			break;
		}
	}
	//return true;
	goto PROCESS_MESSAGE;
_exit0:
	return bRunningFlag;
}


bool EISChannel::commandThread()
{
	bool bRunningFlag = true;
	EyeseeMessage msg;
	status_t getMsgRet;
	ERRORTYPE ret;
	
PROCESS_MESSAGE:
	getMsgRet = mpCommandThread->mMsgQueue.dequeueMessage(&msg);
	if(getMsgRet == NO_ERROR)
	{
		if(DoCommandThread::MsgTypeCommand_TakePicture == msg.mMsgType)
		{
			doTakePicture(msg.mPara0);
		}
		else if(DoCommandThread::MsgTypeCommand_CancelContinuousPicture == msg.mMsgType)
		{
			doCancelContinuousPicture();
		}
		else if(DoCommandThread::MsgTypeCommand_Exit == msg.mMsgType)
		{
			bRunningFlag = false;
			goto _exit0;
		}
		else
		{
			aloge("unknown msg[0x%x]!", msg.mMsgType);
		}
	}
	else
	{
		mpCommandThread->mMsgQueue.waitMessage();
	}
	//return true;
	goto PROCESS_MESSAGE;
_exit0:
	return bRunningFlag;
} 


};


















