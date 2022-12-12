
#define LOG_NDEBUG 0
#define LOG_TAG "EISDevice"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>

#include <type_camera.h>
#include <memoryAdapter.h>
#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>

#include "EISDevice.h"
#include "EISChannel.h"

#include <mpi_eis.h>


using namespace std;

namespace EyeseeLinux{

// register proxy of other module to EIS
status_t EISDevice::CameraProxyInfo::SetCameraChannelInfo(EIS_CameraChannelInfo *pCameraChnInfo)
{
	if (mpCameraProxy != NULL) 
	{
		delete mpCameraProxy;
		mpCameraProxy = NULL;
	}
	if(pCameraChnInfo->mpCameraRecordingProxy)
	{
		mpCameraProxy = pCameraChnInfo->mpCameraRecordingProxy;
		mnCameraChannel = pCameraChnInfo->mnCameraChannel;
	}
	else
	{
		aloge("fatal error! camera == NULL");
	}
	return NO_ERROR;
}


// move frm node from used_frame_list to waiting_release_list
// function calling will happen when the input frame sent to EIS component has been processed.
status_t EISDevice::CameraProxyInfo::addReleaseFrame(VIDEO_FRAME_INFO_S *pVIFrame)
{
	
	bool bFindFlag = false;
	int matchCount = 0;
	
	AutoMutex lock(mFrameBufListLock);
	
	std::list<VIDEO_FRAME_BUFFER_S>::iterator firstOne;
	for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mUsedFrameBufList.begin(); it!=mUsedFrameBufList.end(); ++it)
	{
		if(it->mFrameBuf.mId == pVIFrame->mId)
		{
			matchCount++;
			if(!bFindFlag)
			{
				bFindFlag = true;
				firstOne = it;
			}
		}
	}
	if(0 == matchCount)
	{
		aloge("fatal error! not find frameId[0x%x]!", pVIFrame->mId);
	}
	else
	{
		if(firstOne != mUsedFrameBufList.begin())
		{
			aloge("fatal error! MPP-EIS release frame is not first one of mUsedFrameBufList!");
		}
		if(matchCount > 1)
		{
			aloge("fatal error! find [%d] matched frames in mUsedFrameBufList!", matchCount);
		}
		mWaitReleaseFrameBufList.splice(mWaitReleaseFrameBufList.end(), mUsedFrameBufList, firstOne);
	}
	return NO_ERROR;
}


EISDevice::CameraProxyInfo::CameraProxyInfo()
{
	mpCameraProxy = NULL;
	mnCameraChannel = 0;;
	mIdleFrameBufList.resize(12);	// add more nodes, since eis need 8 frm at least.
}

EISDevice::CameraProxyInfo::CameraProxyInfo(const CameraProxyInfo& ref)
{
	mpCameraProxy		= ref.mpCameraProxy;
	mnCameraChannel 	= ref.mnCameraChannel;
	mIdleFrameBufList	= ref.mIdleFrameBufList;
	mReadyFrameBufList	= ref.mReadyFrameBufList;
	mUsedFrameBufList	= ref.mUsedFrameBufList;
	mWaitReleaseFrameBufList = ref.mWaitReleaseFrameBufList;
	mReleasingFrameBufList = ref.mReleasingFrameBufList;
}

EISDevice::CameraProxyInfo::~CameraProxyInfo()
{
	if(mReadyFrameBufList.size()>0 || mUsedFrameBufList.size()>0 ||mWaitReleaseFrameBufList.size()>0 || mReleasingFrameBufList.size()>0)
	{
		aloge("fatal error! why cameraProxy[%p] vipp[%d] still has [%d][%d][%d][%d]frames?", 
															mpCameraProxy, mnCameraChannel,
															mReadyFrameBufList.size(),
															mUsedFrameBufList.size(),
															mWaitReleaseFrameBufList.size(),
															mReleasingFrameBufList.size());
	}
	
	if (mpCameraProxy != NULL)
	{
		delete mpCameraProxy;
		mpCameraProxy = NULL;
	}
}

/* EIS listener for proxy use */
EISDevice::CameraProxyListener::CameraProxyListener(EISDevice *pEis, int nCameraIndex)
{
	mpEis = pEis;
	mCameraIndex = nCameraIndex;
}

void EISDevice::CameraProxyListener::dataCallbackTimestamp(const void *pdata)
{
	mpEis->dataCallbackTimestamp(mCameraIndex, (const VIDEO_FRAME_BUFFER_S*)pdata);
}

/* Threads defined for input frame process */

//SendPicThread start --->
EISDevice::DoSendPicThread::DoSendPicThread(EISDevice *pEisDev) :mpEisDev(pEisDev)
{
	mbWaitInputFrameBuf = false;
	mSendPicThreadState = EIS_SENDPIC_STATE_LOADED;
	mbThreadOK = true;
	if(NO_ERROR != run("EISSendPicThread"))
	{
		aloge("fatal error! create thread fail");;
		mbThreadOK = false;
	}
}

EISDevice::DoSendPicThread::~DoSendPicThread()
{
	SendPicThreadReset();
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeSendPic_Exit;
	mSendPicMsgQueue.queueMessage(&msg);
	requestExitAndWait();
	mSendPicMsgQueue.flushMessage();
}

status_t EISDevice::DoSendPicThread::SendPicThreadStart()
{
    AutoMutex autoLock(mLock);
	
    AutoMutex autoLock2(mStateLock);
    if(mSendPicThreadState == EIS_SENDPIC_STATE_STARTED)
    {
        alogd("already in started");
        return NO_ERROR;
    }
    if(mSendPicThreadState != EIS_SENDPIC_STATE_LOADED && mSendPicThreadState != EIS_SENDPIC_STATE_PAUSED)
    {
        aloge("fatal error! can't call in state[0x%x]", mSendPicThreadState);
        return INVALID_OPERATION;
    }
	
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeSendPic_SetState;
    msg.mPara0 = EIS_SENDPIC_STATE_STARTED;
    mSendPicMsgQueue.queueMessage(&msg);
	
    while(EIS_SENDPIC_STATE_STARTED != mSendPicThreadState)
    {
    	mStartCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

status_t EISDevice::DoSendPicThread::SendPicThreadPause()
{
	AutoMutex autoLock(mLock);
	
	AutoMutex autoLock2(mStateLock);
	if(mSendPicThreadState == EIS_SENDPIC_STATE_PAUSED)
	{
		alogd("already in paused");
		return NO_ERROR;
	}
	if(mSendPicThreadState != EIS_SENDPIC_STATE_STARTED)
	{
		aloge("fatal error! can't call in state[0x%x]", mSendPicThreadState);
		return INVALID_OPERATION;
	}
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeSendPic_SetState;
	msg.mPara0 = EIS_SENDPIC_STATE_PAUSED;
	mSendPicMsgQueue.queueMessage(&msg);
	while(EIS_SENDPIC_STATE_PAUSED != mSendPicThreadState)
	{
		mPauseCompleteCond.wait(mStateLock);
	}
	return NO_ERROR;
}

status_t EISDevice::DoSendPicThread::SendPicThreadReset()
{
    AutoMutex autoLock(mLock);
	
    {
        AutoMutex autoLock2(mStateLock);
        if(mSendPicThreadState != EIS_SENDPIC_STATE_LOADED)
        {
            EyeseeMessage msg;
            msg.mMsgType = MsgTypeSendPic_SetState;
            msg.mPara0 = EIS_SENDPIC_STATE_LOADED;
            mSendPicMsgQueue.queueMessage(&msg);
            while(EIS_SENDPIC_STATE_LOADED != mSendPicThreadState)
            {
            	mLoadedCompleteCond.wait(mStateLock);
            }
        }
    }
	
    mbWaitInputFrameBuf = false;
    return NO_ERROR;
}

status_t EISDevice::DoSendPicThread::SendMessage_InputFrameReady()
{
    mbWaitInputFrameBuf = false;
	
    EyeseeMessage msg;
	
    msg.mMsgType = DoSendPicThread::MsgTypeSendPic_InputFrameAvailable;
    mSendPicMsgQueue.queueMessage(&msg);
	
    return NO_ERROR;
}

bool EISDevice::DoSendPicThread::threadLoop()
{
	if(!exitPending()) 
	{
		return sendPicThread();
	} 
	else
	{
		return false;
	}
}

bool EISDevice::DoSendPicThread::sendPicThread()		// send,means send current frm got from ready list,to EIS component
{
	bool bRunningFlag = true;
	status_t getMsgRet;
	ERRORTYPE ret;
	EyeseeMessage msg;
	
	while(1)	
	{
	PROCESS_MESSAGE:
		getMsgRet = mSendPicMsgQueue.dequeueMessage(&msg);
		if(getMsgRet == NO_ERROR)
		{
			if(MsgTypeSendPic_SetState == msg.mMsgType)
			{
				AutoMutex autoLock(mStateLock);
				if(msg.mPara0 == mSendPicThreadState)
				{
					aloge("fatal error! same state[0x%x]", mSendPicThreadState);
				}
				else if(EIS_SENDPIC_STATE_LOADED == msg.mPara0)
				{
					alogd("sendPic thread state[0x%x]->loaded", mSendPicThreadState);
					mSendPicThreadState = EIS_SENDPIC_STATE_LOADED;
					mLoadedCompleteCond.signal();
				}
				else if(EIS_SENDPIC_STATE_PAUSED == msg.mPara0)
				{
					alogd("sendPic thread state[0x%x]->paused", mSendPicThreadState);
					mSendPicThreadState = EIS_SENDPIC_STATE_PAUSED;
					mPauseCompleteCond.signal();
				}
				else if(EIS_SENDPIC_STATE_STARTED == msg.mPara0)
				{
					alogd("sendPic thread state[0x%x]->started", mSendPicThreadState);
					mSendPicThreadState = EIS_SENDPIC_STATE_STARTED;
					mStartCompleteCond.signal();
				}
				else
				{
					aloge("fatal error! check code!");
				}
			}
			else if(MsgTypeSendPic_InputFrameAvailable == msg.mMsgType)
			{
				//alogv("input frame come");
			}
			else if(MsgTypeSendPic_Exit == msg.mMsgType)
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

		if(EIS_SENDPIC_STATE_STARTED == mSendPicThreadState)
		{
			for (std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
			{
				it->mFrameBufListLock.lock();
			}

			unsigned int minFrameNum = (unsigned int)-1;
			unsigned int maxFrameNum = 0;
			unsigned int sendFrameNum = 0;
			for (std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
			{
				if(it->mReadyFrameBufList.size() < minFrameNum)
				{
					minFrameNum = it->mReadyFrameBufList.size();
				}
				if(it->mReadyFrameBufList.size() > maxFrameNum)
				{
					maxFrameNum = it->mReadyFrameBufList.size();
				}
				
				sendFrameNum = maxFrameNum;//(EISMODE_ONE_FISHEYE == mpEisDev->mEisAttr.iseMode) ? maxFrameNum : minFrameNum;
				
				VIDEO_FRAME_INFO_S *pbuf0;
				VIDEO_FRAME_INFO_S *pbuf1;
				for(unsigned int i=0; i<sendFrameNum; i++)
				{
					pbuf0 = &it->mReadyFrameBufList.begin()->mFrameBuf;
					pbuf1 = NULL; 
					
					ret = AW_MPI_EIS_SendPic(mpEisDev->mGroupId, pbuf0, -1);
					if (ret != SUCCESS) 
					{
						// move to waitReleaseList directly
						it->mWaitReleaseFrameBufList.splice(it->mWaitReleaseFrameBufList.end(), it->mReadyFrameBufList, it->mReadyFrameBufList.begin()); 
						aloge("fatal error! why MPP-EIS can't accept new frame! check code!");
					}
					else
					{
						it->mUsedFrameBufList.splice(it->mUsedFrameBufList.end(), it->mReadyFrameBufList, it->mReadyFrameBufList.begin()); 
					} 
				}
			}
			
			
			mbWaitInputFrameBuf = true;
			for (std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
			{
				it->mFrameBufListLock.unlock();
			}
			
			mSendPicMsgQueue.waitMessage();
		}
		else
		{
			mSendPicMsgQueue.waitMessage();
		}
	}
	
	return true;
_exit0:
	return bRunningFlag;
}
// doSendPicThread  end <---

// DoReturnPicThread	start --> 
EISDevice::DoReturnPicThread::DoReturnPicThread(EISDevice *pEisDev) :mpEisDev(pEisDev)
{
	mbWaitInputFrameBuf = false;
	mState = EIS_RETURNPIC_STATE_LOADED;
	mbThreadOK = true;
	
	if(NO_ERROR != run("EISReturnPic"))
	{
		aloge("fatal error! create thread fail");
		mbThreadOK = false;
	}
}

EISDevice::DoReturnPicThread::~DoReturnPicThread()
{
	Reset();
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeReturnPic_Exit;
	mMsgQueue.queueMessage(&msg);
	requestExitAndWait();
	mMsgQueue.flushMessage();
}

status_t EISDevice::DoReturnPicThread::Start()
{
	AutoMutex autoLock(mLock);
	
	if(mState == EIS_RETURNPIC_STATE_STARTED)
	{
		alogd("already in started");
		return NO_ERROR;
	}
	if(mState != EIS_RETURNPIC_STATE_LOADED)
	{
		aloge("fatal error! can't call in state[0x%x]", mState);
		return INVALID_OPERATION;
	}
	
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeReturnPic_SetState;
	msg.mPara0 = EIS_RETURNPIC_STATE_STARTED;
	
	mMsgQueue.queueMessage(&msg);
	return NO_ERROR;
}

status_t EISDevice::DoReturnPicThread::Reset()
{
    AutoMutex autoLock(mLock);
	
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeReturnPic_ReturnAllFrames;
    mMsgQueue.queueMessage(&msg);
    
    msg.reset();
    msg.mMsgType = MsgTypeReturnPic_SetState;
    msg.mPara0 = EIS_RETURNPIC_STATE_LOADED;
    mMsgQueue.queueMessage(&msg);

    AutoMutex autoLock2(mStateLock);
    while(mState != EIS_RETURNPIC_STATE_LOADED)
    {
        mStateCond.wait(mStateLock);
    }

    mbWaitInputFrameBuf = false;
    return NO_ERROR;
}

status_t EISDevice::DoReturnPicThread::notifyReturnPictureCome()
{
	AutoMutex lock(mInputFrameLock);
	
	if(mbWaitInputFrameBuf)
	{
		mbWaitInputFrameBuf = false;
		
		EyeseeMessage msg;
		msg.mMsgType = MsgTypeReturnPic_InputFrameAvailable;
		
		mMsgQueue.queueMessage(&msg);
	}
	return NO_ERROR;
}

status_t EISDevice::DoReturnPicThread::SendCommand_ReturnAllFrames()
{
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeReturnPic_ReturnAllFrames;
	mMsgQueue.queueMessage(&msg);
	
	return NO_ERROR;
}

bool EISDevice::DoReturnPicThread::threadLoop()
{
	if(!exitPending()) 
	{
		return returnPicThread();
	} 
	else
	{
		return false;
	}
}

status_t EISDevice::DoReturnPicThread::returnAllFrames()
{
	bool bHasFrame;
	// return all waitReleaseFrames
	while(1)
	{
		bHasFrame = false;
		for(std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
		{
			it->mFrameBufListLock.lock();
			
			if(!it->mWaitReleaseFrameBufList.empty())
			{
				bHasFrame = true;
				if(!it->mReleasingFrameBufList.empty())
				{
					aloge("fatal error! check code!");
				}
				
				it->mReleasingFrameBufList.splice(it->mReleasingFrameBufList.end(), it->mWaitReleaseFrameBufList, it->mWaitReleaseFrameBufList.begin());
				it->mFrameBufListLock.unlock();
				
				it->mpCameraProxy->releaseRecordingFrame(it->mnCameraChannel, it->mReleasingFrameBufList.begin()->mFrameBuf.mId);
				
				it->mFrameBufListLock.lock();
				it->mIdleFrameBufList.splice(it->mIdleFrameBufList.end(), it->mReleasingFrameBufList);
				it->mFrameBufListLock.unlock();
			}
			else
			{
				it->mFrameBufListLock.unlock();
			}
		}
		
		if(!bHasFrame)
		{
			break;
		}
	}

	// return all readyFrames
	while(1)
	{
		bHasFrame = false;
		for(std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
		{
			it->mFrameBufListLock.lock();
			if(!it->mReadyFrameBufList.empty())
			{
				bHasFrame = true;
				if(!it->mReleasingFrameBufList.empty())
				{
					aloge("fatal error! check code!");
				}
				it->mReleasingFrameBufList.splice(it->mReleasingFrameBufList.end(), it->mReadyFrameBufList, it->mReadyFrameBufList.begin());
				it->mFrameBufListLock.unlock();
				
				it->mpCameraProxy->releaseRecordingFrame(it->mnCameraChannel, it->mReleasingFrameBufList.begin()->mFrameBuf.mId);
				
				it->mFrameBufListLock.lock();
				it->mIdleFrameBufList.splice(it->mIdleFrameBufList.end(), it->mReleasingFrameBufList);
				it->mFrameBufListLock.unlock();
			}
			else
			{
				it->mFrameBufListLock.unlock();
			}
		}
		
		if(!bHasFrame)
		{
			break;
		}
	}

	// return all frm in mUsedFrameBufList
	while(1)
	{
		bHasFrame = false;
		for(std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
		{
			it->mFrameBufListLock.lock(); 
			if(!it->mUsedFrameBufList.empty())
			{
				bHasFrame = true;
				if(!it->mReleasingFrameBufList.empty())
				{
					aloge("fatal error! check code!");
				}
				it->mReleasingFrameBufList.splice(it->mReleasingFrameBufList.end(), it->mUsedFrameBufList, it->mUsedFrameBufList.begin());
				it->mFrameBufListLock.unlock();
				
				it->mpCameraProxy->releaseRecordingFrame(it->mnCameraChannel, it->mReleasingFrameBufList.begin()->mFrameBuf.mId); 
				
				it->mFrameBufListLock.lock(); 
				it->mIdleFrameBufList.splice(it->mIdleFrameBufList.end(), it->mReleasingFrameBufList); 
				it->mFrameBufListLock.unlock();
			}
			else
			{
				it->mFrameBufListLock.unlock();
			}
		}
		
		if(!bHasFrame)
		{
			break;
		}
	}


	//check again
	int nCameraIndex = 0;
	for(std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
	{
		it->mFrameBufListLock.lock();
		
		if(it->mReadyFrameBufList.size()>0 || it->mUsedFrameBufList.size()>0 || it->mWaitReleaseFrameBufList.size()>0 || it->mReleasingFrameBufList.size()>0)
		{
			aloge("fatal error! why cameraProxy[%p] camIdx[%d] vipp[%d] still has [%d][%d][%d][%d]frames?", 
													it->mpCameraProxy, nCameraIndex, it->mnCameraChannel,
													it->mReadyFrameBufList.size(),
													it->mUsedFrameBufList.size(),
													it->mWaitReleaseFrameBufList.size(),
													it->mReleasingFrameBufList.size());
		}
		it->mFrameBufListLock.unlock();
		nCameraIndex++;
	}
	return NO_ERROR;
}


// this thread process waiting_release_frm_list ,releasingFrmList,and call function to release frm used done	samuel.zhou
bool EISDevice::DoReturnPicThread::returnPicThread()
{
	bool bRunningFlag = true;
	EyeseeMessage msg;
	status_t getMsgRet;
	ERRORTYPE ret;
	
	while(1)	
	{
	PROCESS_MESSAGE:
		getMsgRet = mMsgQueue.dequeueMessage(&msg);
		if(getMsgRet == NO_ERROR)
		{
			if(MsgTypeReturnPic_SetState == msg.mMsgType)
			{
				AutoMutex autoLock(mStateLock);
				
				if(msg.mPara0 == mState)
				{
					alogd("same state[0x%x]", mState);
				}
				else if(EIS_RETURNPIC_STATE_LOADED == msg.mPara0)
				{
					//returnAllFrames();
					alogd("returnPic thread state[0x%x]->loaded", mState);
					mState = EIS_RETURNPIC_STATE_LOADED;
				}
				else if(EIS_RETURNPIC_STATE_STARTED == msg.mPara0)
				{
					alogd("returnPic thread state[0x%x]->started", mState);
					mState = EIS_RETURNPIC_STATE_STARTED;
				}
				else
				{
					aloge("fatal error! wrong state[0x%x]!", msg.mPara0);
				}
				mStateCond.signal();
			}
			else if(MsgTypeReturnPic_InputFrameAvailable == msg.mMsgType)
			{
				//alogv("input frame come");
			}
			else if(MsgTypeReturnPic_ReturnAllFrames == msg.mMsgType)
			{
				returnAllFrames();
			}
			else if(MsgTypeReturnPic_Exit == msg.mMsgType)
			{
				returnAllFrames();
				bRunningFlag = false;
				goto _exit0;
			}
			else
			{
				aloge("unknown msg[0x%x]!", msg.mMsgType);
			}
			goto PROCESS_MESSAGE;
		}
		
		if(EIS_RETURNPIC_STATE_STARTED == mState)
		{
			bool bHasFrame;
			while(1)
			{
				bHasFrame = false;
				for(std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
				{
					it->mFrameBufListLock.lock();
					
					if(!it->mWaitReleaseFrameBufList.empty())		// frame has been moved to this mWaitReleaseFrameBufList   samuel.zhou
					{
						bHasFrame = true;
						if(!it->mReleasingFrameBufList.empty())
						{
							aloge("fatal error! check code!");
						}
						
						it->mReleasingFrameBufList.splice(it->mReleasingFrameBufList.end(), it->mWaitReleaseFrameBufList, it->mWaitReleaseFrameBufList.begin());
						it->mFrameBufListLock.unlock();
						
						it->mpCameraProxy->releaseRecordingFrame(it->mnCameraChannel, it->mReleasingFrameBufList.begin()->mFrameBuf.mId); 
									// releaseRecordingFrame() function of camara proxy 	samuel.zhou
						it->mFrameBufListLock.lock(); 
						it->mIdleFrameBufList.splice(it->mIdleFrameBufList.end(), it->mReleasingFrameBufList);
						it->mFrameBufListLock.unlock();
					}
					else
					{
						it->mFrameBufListLock.unlock();
					}
				}

				
				if(!bHasFrame)
				{
					//if all cameraproxies has no frame, check them again!
					for (std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
					{
						it->mFrameBufListLock.lock();
					}
					
					for (std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
					{
						if(!it->mWaitReleaseFrameBufList.empty())
						{
							bHasFrame = true;
							break;
						}
					}
					if(!bHasFrame)
					{
						mInputFrameLock.lock();
						mbWaitInputFrameBuf = true;
						mInputFrameLock.unlock();
					}
					
					for (std::vector<CameraProxyInfo>::iterator it = mpEisDev->mCameraProxies.begin(); it!=mpEisDev->mCameraProxies.end(); ++it)
					{
						it->mFrameBufListLock.unlock();
					}
					if(!bHasFrame)
					{
						break;
					}
				}
			}
			
			mMsgQueue.waitMessage();
		}
		else
		{
			mMsgQueue.waitMessage();
		}
	}
	
	return true;
_exit0:
	return bRunningFlag;
}

//DoReturnPicThread end <--

//DoCaptureThread  start --> 
EISDevice::DoCaptureThread::DoCaptureThread(EISDevice *pEisDev) : mpEisDev(pEisDev)
{
	mCapThreadState = EIS_CAPTURE_STATE_LOADED;
	mbThreadOK = true;
	
	if(NO_ERROR != run("EISCaptureThread"))
	{
		aloge("fatal error! create thread fail");;
		mbThreadOK = false;
	}
}

EISDevice::DoCaptureThread::~DoCaptureThread()
{
	CaptureThreadReset();
	
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeCapture_Exit;
	
	mCaptureMsgQueue.queueMessage(&msg); 
	requestExitAndWait();

	mCaptureMsgQueue.flushMessage();
}

status_t EISDevice::DoCaptureThread::CaptureThreadStart()
{
	AutoMutex autoLock(mLock);
	
	AutoMutex autoLock2(mStateLock);
	if(mCapThreadState == EIS_CAPTURE_STATE_STARTED)
	{
		alogd("already in started");
		return NO_ERROR;
	}
	
	if(mCapThreadState != EIS_CAPTURE_STATE_LOADED && mCapThreadState != EIS_CAPTURE_STATE_PAUSED)
	{
		aloge("fatal error! can't call in state[0x%x]", mCapThreadState);
		return INVALID_OPERATION;
	}
	
	EyeseeMessage msg;
	msg.mMsgType = MsgTypeCapture_SetState;
	msg.mPara0 = EIS_CAPTURE_STATE_STARTED;

	mCaptureMsgQueue.queueMessage(&msg);

	while(EIS_CAPTURE_STATE_STARTED != mCapThreadState)
	{
		mStartCompleteCond.wait(mStateLock);
	}
	return NO_ERROR;
}

status_t EISDevice::DoCaptureThread::CaptureThreadPause()
{
	AutoMutex autoLock(mLock);
	
	AutoMutex autoLock2(mStateLock);
	if(mCapThreadState == EIS_CAPTURE_STATE_PAUSED)
	{
		alogd("already in paused");
		return NO_ERROR;
	}
	if(mCapThreadState != EIS_CAPTURE_STATE_STARTED)
	{
		aloge("fatal error! can't call in state[0x%x]", mCapThreadState);
		return INVALID_OPERATION;
	}
	
	EyeseeMessage msg;

	msg.mMsgType = MsgTypeCapture_SetState;
	msg.mPara0 = EIS_CAPTURE_STATE_PAUSED;

	mCaptureMsgQueue.queueMessage(&msg);

	while(EIS_CAPTURE_STATE_PAUSED != mCapThreadState)
	{
		mPauseCompleteCond.wait(mStateLock);
	}
	return NO_ERROR;
}

status_t EISDevice::DoCaptureThread::CaptureThreadReset()
{
	AutoMutex autoLock(mLock);
	
	AutoMutex autoLock2(mStateLock);
	if(mCapThreadState != EIS_CAPTURE_STATE_LOADED)
	{
		EyeseeMessage msg;
		msg.mMsgType = MsgTypeCapture_SetState;
		msg.mPara0 = EIS_CAPTURE_STATE_LOADED;
		
		mCaptureMsgQueue.queueMessage(&msg);
		
		while(EIS_CAPTURE_STATE_LOADED != mCapThreadState)
		{
			mLoadedCompleteCond.wait(mStateLock);
		}
	}
	return NO_ERROR;
}

bool EISDevice::DoCaptureThread::threadLoop()
{
	if(!exitPending()) 
	{
		return captureThread();
	} 
	else
	{
		return false;
	}
}

bool EISDevice::DoCaptureThread::captureThread()
{
	bool bRunningFlag = true;
	status_t getMsgRet;
	int ret;
	EyeseeMessage msg;
	
	while(1)
	{
	PROCESS_MESSAGE:
		getMsgRet = mCaptureMsgQueue.dequeueMessage(&msg);
		if(getMsgRet == NO_ERROR)
		{
			if(MsgTypeCapture_SetState == msg.mMsgType)
			{
				AutoMutex autoLock(mStateLock);
				if(msg.mPara0 == mCapThreadState)
				{
					aloge("fatal error! same state[0x%x]", mCapThreadState);
				}
				else if(EIS_CAPTURE_STATE_LOADED == msg.mPara0)
				{
					alogd("capture thread state[0x%x]->loaded", mCapThreadState);
					mCapThreadState = EIS_CAPTURE_STATE_LOADED;
					mLoadedCompleteCond.signal();
				}
				else if(EIS_CAPTURE_STATE_PAUSED == msg.mPara0)
				{
					alogd("capture thread state[0x%x]->paused", mCapThreadState);
					mCapThreadState = EIS_CAPTURE_STATE_PAUSED;
					mPauseCompleteCond.signal();
				}
				else if(EIS_CAPTURE_STATE_STARTED == msg.mPara0)
				{
					alogd("capture thread state[0x%x]->started", mCapThreadState);
					mCapThreadState = EIS_CAPTURE_STATE_STARTED;
					mStartCompleteCond.signal();
				}
				else
				{
					aloge("fatal error! check code!");
				}
			}
			else if(MsgTypeCapture_Exit == msg.mMsgType)
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

		if(EIS_CAPTURE_STATE_STARTED == mCapThreadState)
		{
			//AutoMutex autoLock(mpEisDev->mEISChannelVectorLock);
			for (vector<EISChannelInfo>::iterator it = mpEisDev->mEISChannelVector.begin(); it != mpEisDev->mEISChannelVector.end(); ++it)
			{
				it->mpChannel->process();
			}
		}
		else
		{
			mCaptureMsgQueue.waitMessage();
		}
	}

_exit0:
	return bRunningFlag;
}
// DoCaptureThread end <---





EISDevice::EISDevice(unsigned int nEISId) : mEISId(nEISId) , mDevState(EIS_STATE_CONSTRUCTED) ,
											mpSendPicThread(NULL) , mpCapThread(NULL)
{
	alogd("Construct");
//	memset(&mIseAttr, 0, sizeof(mIseAttr));

	mpSendPicThread = new DoSendPicThread(this);
	mpReturnPicThread = new DoReturnPicThread(this);
	mpCapThread = new DoCaptureThread(this);

	memset(&mEisChnAttr,0,sizeof(mEisChnAttr));
	
	mColorSpace = V4L2_COLORSPACE_JPEG;
	//MPP components
//	  mEISGroup.mModId = MOD_ID_EIS;
//	  mEISGroup.mDevId = MM_INVALID_DEV;
//	  mEISGroup.mChnId = MM_INVALID_CHN;
	mGroupId = MM_INVALID_DEV;
}

EISDevice::~EISDevice()
{
	alogd("Destruct");
	reset();
	delete mpSendPicThread;
	delete mpReturnPicThread;
	delete mpCapThread;
}

EISChannel *EISDevice::searchEISChannel(int chnId)
{
	AutoMutex lock(mEISChannelVectorLock);
	
	for (vector<EISChannelInfo>::iterator it = mEISChannelVector.begin(); it != mEISChannelVector.end(); ++it) {
		if (it->mChnId== chnId) {
			return it->mpChannel;
		}
	}
	return NULL;
}

status_t EISDevice::prepare(EIS_ATTR_S *pEisChnAttr)
{
	AutoMutex lock(mLock);
	
	status_t result;
	if(mDevState != EIS_STATE_CONSTRUCTED) 
	{
		aloge("prepare in error state %d", mDevState);
		return INVALID_OPERATION;
	}
	if(mCameraProxies.size()>0)
	{
		CameraParameters CameraChannelParam;
		
		mCameraProxies[0].mpCameraProxy->getParameters(mCameraProxies[0].mnCameraChannel, CameraChannelParam);
		mColorSpace = CameraChannelParam.getColorSpace();
	}
	else
	{
		alogd("no camera set?");
	} 

	memcpy(&mEisChnAttr,pEisChnAttr,sizeof(EIS_ATTR_S));
    ERRORTYPE ret;
    bool bSuccessFlag = false;
    //mISEGroup.mDevId = 0;
    mGroupId = 0;
    while(mGroupId < MAX_EIS_CHN_NUM)
    {
        ret = AW_MPI_EIS_CreateChn(mGroupId, &mEisChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create EIS chl[%d] success!", mGroupId);
            break;
        }
        else if(ERR_EIS_EXIST == ret)
        {
            alogd("EIS chl[%d] is exist, find next!", mGroupId);
            mGroupId++;
        }
        else
        {
            alogd("create EIS chl[%d] ret[0x%x], find next!", mGroupId, ret);
            mGroupId++;
        }
    }
    if(false == bSuccessFlag)
    {
        mGroupId = MM_INVALID_DEV;
        aloge("fatal error! create ISE chl fail!");
        return UNKNOWN_ERROR;
    } 
	
	MPPCallbackInfo cbInfo; 
	cbInfo.cookie = (void*)this;
	cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper; 
	ret = AW_MPI_EIS_RegisterCallback(mGroupId, &cbInfo);
	if(SUCCESS != ret)
	{
		aloge("register callback to mpi_eis fail");
	} 

	mDevState = EIS_STATE_PREPARED;

	return NO_ERROR; 
}

status_t EISDevice::release_l()
{
	if (mDevState != EIS_STATE_PREPARED)
	{
		aloge("release in error state %d", mDevState);
		return INVALID_OPERATION;
	}
	
	mpSendPicThread->SendPicThreadReset();
	mpReturnPicThread->Reset();
	mpCapThread->CaptureThreadReset(); 
	
	ERRORTYPE ret = AW_MPI_EIS_DestroyChn(mGroupId);
	if (ret != SUCCESS)
	{
		aloge("fatal error! AW_MPI_EIS_DestroyChn error!");
	}

	mGroupId = MM_INVALID_DEV; 
	mDevState = EIS_STATE_CONSTRUCTED;

	return NO_ERROR;
}

status_t EISDevice::release()
{
	AutoMutex lock(mLock);
	
	return release_l();
}

status_t EISDevice::start()
{
	AutoMutex lock(mLock);
	
	if(mDevState == EIS_STATE_STARTED)
	{
		alogd("already in state started");
		return NO_ERROR;
	}
	if (mDevState != EIS_STATE_PREPARED) 
	{
		aloge("start in error state %d", mDevState);
		return INVALID_OPERATION;
	}
	
	//judge if camera is set successful.
	{
		bool bCameraOK = true;
		if(mCameraProxies.size() > 0)
		{
			for(std::vector<CameraProxyInfo>::iterator it=mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
			{
				if(NULL == it->mpCameraProxy)
				{
					aloge("Camera is not set yet?");
					bCameraOK = false;
				}
			}
		}
		else
		{
			aloge("no camera set?");
			bCameraOK = false;
		}
		if(!bCameraOK)
		{
			return INVALID_OPERATION;
		}
	} 
	
	ERRORTYPE ret = AW_MPI_EIS_StartChn(mGroupId);
	if (ret != SUCCESS)
	{
		aloge("AW_MPI_EIS_Start error!");
		return UNKNOWN_ERROR;
	}
	 
	mDevState = EIS_STATE_STARTED;
	
	//start sendPic_thread and capture_thread.
	mpSendPicThread->SendPicThreadStart(); 
	mpReturnPicThread->Start(); 
	mpCapThread->CaptureThreadStart();

	for(unsigned int i=0; i<mCameraProxies.size(); i++)
	{
		mCameraProxies[i].mpCameraProxy->startRecording( mCameraProxies[i].mnCameraChannel, 
															new CameraProxyListener(this, i), (int)mEISId);
	}

//	CameraParameters camParam;
//	getParameters(0, camParam);

	return NO_ERROR;
}

status_t EISDevice::stop_l()
{
	if (mDevState != EIS_STATE_STARTED) 
	{
		aloge("stop in error state %d", mDevState);
		return INVALID_OPERATION;
	}
	//pause capture thread
	mpCapThread->CaptureThreadPause();
	//stop camera sending frame here.
	for(unsigned int i=0; i<mCameraProxies.size(); i++)
	{
		mCameraProxies[i].mpCameraProxy->stopRecording(mCameraProxies[i].mnCameraChannel, (int)mEISId);
	}
	
	//pause sendPic thread
	mpSendPicThread->SendPicThreadPause();

	{
		AutoMutex lock(mEISChannelVectorLock);
		for (vector<EISChannelInfo>::iterator it = mEISChannelVector.begin(); it != mEISChannelVector.end(); ++it) 
		{
      #if 0 //by andy
			it->mpChannel->stopPreview();
			it->mpChannel->releaseAllEISFrames();
      #else
			it->mpChannel->stopChannel();
      #endif
		}
	}
	
	//stop EIS 
	AW_MPI_EIS_StopChn(mGroupId);

	//let return Pic thread return all frames.
	mpReturnPicThread->SendCommand_ReturnAllFrames(); 
	
	mDevState = EIS_STATE_PREPARED;
	return NO_ERROR;
}

status_t EISDevice::stop()
{
	AutoMutex lock(mLock);
	
	return stop_l();
}

status_t EISDevice::reset()
{
	status_t ret;
	
	AutoMutex lock(mLock); 
	alogd("reset");
	if(EIS_STATE_CONSTRUCTED == mDevState)
	{
		return NO_ERROR;
	}
	if(EIS_STATE_STARTED == mDevState)
	{
		ret = stop_l();
	}
	if(EIS_STATE_PREPARED == mDevState)
	{
		ret = release_l();
	}
	
	return NO_ERROR;
}

EIS_CHN EISDevice::openChannel(EIS_ATTR_S *pChnAttr)
{
	AutoMutex lock(mLock);
	
	ERRORTYPE ret;
	EISChannelInfo eisChn;
	MPPCallbackInfo cbInfo; 			// need to move to the position after channel opening

	if(mDevState != EIS_STATE_PREPARED)
	{
		aloge("open channel in error state %d", mDevState);
		return INVALID_OPERATION;
	}
	
	EISChannel *pChannel = new EISChannel(mGroupId, pChnAttr);
	if(NULL != pChannel)
	{
		eisChn.mChnId = pChannel->getChannelId();
		eisChn.mpChannel = pChannel; 
		
		AutoMutex lock(mEISChannelVectorLock);
		mEISChannelVector.push_back(eisChn); 
		
		return eisChn.mChnId;
	}
	else
	{
		return MM_INVALID_CHN;
	}
	
}


status_t EISDevice::closeChannel(EIS_CHN chnId)
{
	AutoMutex lock(mLock);
	
	if(mDevState != EIS_STATE_PREPARED) 
	{
		aloge("close channel in error state %d", mDevState);
		return INVALID_OPERATION;
	}
	
	bool found = false;
	vector<EISChannelInfo>::iterator it;
	
	AutoMutex lock2(mEISChannelVectorLock);
	for (it = mEISChannelVector.begin(); it != mEISChannelVector.end(); ++it) 
	{
		if (it->mChnId == chnId) 
		{
			found = true;
			break;
		}
	}

	if (!found) 
	{
		aloge("EIS scaler channel %d is not exist!", chnId);
		return INVALID_OPERATION;
	}

	delete it->mpChannel;
	mEISChannelVector.erase(it);

	return NO_ERROR;
}

status_t EISDevice::startChannel(EIS_CHN chnId)
{
	AutoMutex lock(mLock);
	
	if(mDevState != EIS_STATE_PREPARED)
	{
		aloge("close channel in error state %d", mDevState);
		return INVALID_OPERATION;
	}

	bool found = false;
	vector<EISChannelInfo>::iterator it;
	
	AutoMutex lock2(mEISChannelVectorLock);
	for (it = mEISChannelVector.begin(); it != mEISChannelVector.end(); ++it)
	{
		if (it->mChnId == chnId)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		aloge("EIS scaler channel %d is not exist!", chnId);
		return INVALID_OPERATION;
	}

	return it->mpChannel->startChannel();
}

status_t EISDevice::setParameters(int chnId, CameraParameters &param)
{
	EISChannel *pEisChn = searchEISChannel(chnId);
	
	if (pEisChn == NULL) {
		aloge("channel %d is not exit!", chnId);
		return INVALID_OPERATION;
	}
	return pEisChn->setParameters(param);
}

status_t EISDevice::getParameters(int chnId, CameraParameters &param)
{
	EISChannel *pEisChn = searchEISChannel(chnId);
	
	if (pEisChn == NULL) {
		aloge("channel %d is not exit!", chnId);
		return INVALID_OPERATION;
	}
	return pEisChn->getParameters(param);
}

status_t EISDevice::setChannelDisplay(int chnId, int hlay)
{
	EISChannel *pEisChn = searchEISChannel(chnId);
	
	if (pEisChn == NULL) {
		aloge("channel %d is not exit!", chnId);
		return INVALID_OPERATION;
	}
	return pEisChn->setChannelDisplay(hlay);
}

bool EISDevice::previewEnabled(int chnId)
{
	EISChannel *pChannel = searchEISChannel(chnId);
	
	if (pChannel == NULL)
	{
		aloge("channel %d is not exist!", chnId);
		return false;
	}
	return pChannel->isPreviewEnabled();
}

status_t EISDevice::startRender(int chnId)
{
	EISChannel *pChannel = searchEISChannel(chnId);
	
	if (pChannel == NULL)
	{
		aloge("channel %d is not exist!", chnId);
		return NO_INIT;
	}
	return pChannel->startRender();
}

status_t EISDevice::stopRender(int chnId)
{
	EISChannel *pChannel = searchEISChannel(chnId);
	
	if (pChannel == NULL)
	{
		aloge("channel %d is not exist!", chnId);
		return NO_INIT;
	}
	return pChannel->stopRender();
}

status_t EISDevice::setEISChnAttr(EIS_ATTR_S *pAttr)
{
	bool found = false;
	vector<EISChannelInfo>::iterator it;
	
	EIS_CHN chnId = 0;
	
	AutoMutex lock2(mEISChannelVectorLock);
	for (it = mEISChannelVector.begin(); it != mEISChannelVector.end(); ++it)
	{
		if (it->mChnId == chnId)
		{
			found = true;
			break;
		}
	}
	if (true == found)
	{
		//only can update main_chn(portId=0) attr, do not update scale_chn!
		return AW_MPI_EIS_SetChnAttr(chnId, pAttr);
	}
	else
	{
		aloge("set EIS_SetChnAttr fail!");
		return UNKNOWN_ERROR;
	} 
}

void EISDevice::releaseRecordingFrame(int chnId, uint32_t index)		// index: frm id		samuel.zhou
{
	EISChannel *pChn = searchEISChannel(chnId);
	
	if (pChn == NULL) 
	{
		aloge("EIS channel %d is not exist!", chnId);
		return;
	}
	pChn->releaseRecordingFrame(index);
}

status_t EISDevice::startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
	EISChannel *pChn = searchEISChannel(chnId);
	
	if (pChn == NULL) 
	{
		aloge("EIS channel %d is not exist!", chnId);
		return NO_INIT;
	}
	return pChn->startRecording(pCb, recorderId);
}

status_t EISDevice::stopRecording(int chnId, int recorderId)
{
	EISChannel *pChn = searchEISChannel(chnId);
	
	if (pChn == NULL) 
	{
		aloge("EIS channel %d is not exist!", chnId);
		return NO_INIT;
	}
	return pChn->stopRecording(recorderId);
}

void EISDevice::setDataListener(int chnId, DataListener *pCb)
{
	EISChannel *pChn = searchEISChannel(chnId);
	
	if (pChn == NULL)
	{
		aloge("EIS channel %d is not exist!", chnId);
		return;
	}
	pChn->setDataListener(pCb);
}

void EISDevice::setNotifyListener(int chnId, NotifyListener *pCb)
{
	EISChannel *pChn = searchEISChannel(chnId);
	if (pChn == NULL) 
	{
		aloge("EIS channel %d is not exist!", chnId);
		return;
	}
	pChn->setNotifyListener(pCb);
}

status_t EISDevice::takePicture(int chnId, unsigned int msgType, PictureRegionCallback *pPicReg)
{
	EISChannel *pChn = searchEISChannel(chnId);
	
	if (pChn == NULL) {
		aloge("channel %d is not exit!", chnId);
		return INVALID_OPERATION;
	}
	return pChn->takePicture(msgType, pPicReg);
}

status_t EISDevice::notifyPictureRelease(int chnId)
{
    EISChannel *pChn = searchEISChannel(chnId);
	
	if (pChn == NULL) {
		aloge("channel %d is not exit!", chnId);
		return INVALID_OPERATION;
	}
	return pChn->notifyPictureRelease();
}

status_t EISDevice::cancelContinuousPicture(int chnId)
{
	EISChannel *pChn = searchEISChannel(chnId);
	
	if (pChn == NULL) {
		aloge("channel %d is not exit!", chnId);
		return INVALID_OPERATION;
	}
	return pChn->cancelContinuousPicture();
}

status_t EISDevice::setCamera(std::vector<EIS_CameraChannelInfo>& cameraChannels)
{
	if(cameraChannels.size() <= 0)
	{
		aloge("fatal error! none camera is set!");
		return BAD_VALUE;
	}

	if (mDevState != EIS_STATE_PREPARED) 
	{
		aloge("setCamera in error state %d", mDevState);
		return INVALID_OPERATION;
	}
	
	mCameraProxies.clear();
	mCameraProxies.resize(cameraChannels.size());
	
	int i = 0;
	for(std::vector<CameraProxyInfo>::iterator it = mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
	{
		it->SetCameraChannelInfo(&cameraChannels[i++]);
	} 

	return NO_ERROR;
}

// will be called by fram sender such as camara 		samuel.zhou
void EISDevice::dataCallbackTimestamp(int nCameraIndex, const VIDEO_FRAME_BUFFER_S *pCameraFrameInfo)
{
	//AutoMutex lock(mLock);
	if(NULL == pCameraFrameInfo)
	{
		aloge("fatal error! camera frame is NULL?");
		return;
	}
	if (mDevState != EIS_STATE_STARTED) 
	{
		alogd("Be careful! camera[%d] call dataCallbackTimestamp when eis device state is [0x%x]", nCameraIndex, mDevState);
		//mCameraProxies[nCameraIndex].mpCameraProxy->releaseRecordingFrame(mCameraProxies[nCameraIndex].mnCameraChannel, pCameraFrameInfo->mFrameBuf.mId);
		//return;
	}
	
	CameraProxyInfo& CameraProxy = mCameraProxies[nCameraIndex];
	{
		AutoMutex lock(CameraProxy.mFrameBufListLock);
		
		if(CameraProxy.mIdleFrameBufList.empty())
		{
			alogd("Be careful! EIS input IdleFrameBuf list is empty, increase!");
			VIDEO_FRAME_BUFFER_S elem;
			memset(&elem, 0, sizeof(VIDEO_FRAME_BUFFER_S));
			CameraProxy.mIdleFrameBufList.insert(CameraProxy.mIdleFrameBufList.end(), 12, elem);
		}
		memcpy(&CameraProxy.mIdleFrameBufList.front(), pCameraFrameInfo, sizeof(VIDEO_FRAME_BUFFER_S));
		
		CameraProxy.mReadyFrameBufList.splice(CameraProxy.mReadyFrameBufList.end(), CameraProxy.mIdleFrameBufList, CameraProxy.mIdleFrameBufList.begin());
		
		if(false == mpSendPicThread->mbWaitInputFrameBuf)
		{
			return;
		}
	}
	
	for (std::vector<CameraProxyInfo>::iterator it = mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
	{
		it->mFrameBufListLock.lock();
	}
	
	bool bFrameReady = true;
	for (std::vector<CameraProxyInfo>::iterator it = mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
	{
		if(it->mReadyFrameBufList.empty())
		{
			bFrameReady = false;
			break;
		}
	}
	
	if(bFrameReady)
	{
		mpSendPicThread->SendMessage_InputFrameReady();
	}
	
	for (std::vector<CameraProxyInfo>::iterator it = mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
	{
		it->mFrameBufListLock.unlock();
	}
}

// will be called by mpi_eis
void EISDevice::notify(MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
	if(MOD_ID_EIS == pChn->mModId)
	{
		switch(event)
		{
			case MPP_EVENT_RELEASE_VIDEO_BUFFER:
			{
                //ise callback to notify return frame to camera
                VIDEO_FRAME_INFO_S *pbuf = (VIDEO_FRAME_INFO_S *)pEventData;
                if(pbuf != NULL)
                {
                    mCameraProxies[0].addReleaseFrame(pbuf);
                }
                mpReturnPicThread->notifyReturnPictureCome();
                break;
            }
			default:
			{
				//postEventFromNative(this, event, 0, 0, pEventData);
				aloge("fatal error! unknown event[0x%x] from EIS group[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
				break;
			}
		}
	}
	else
	{
		aloge("fatal error! another MPP MOD?");
	}
}

ERRORTYPE EISDevice::MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
	((EISDevice*)cookie)->notify(pChn, event, pEventData);
	
	return SUCCESS;
}






};



