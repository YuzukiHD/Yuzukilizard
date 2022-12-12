/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : ISEDevice.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/02
  Last Modified :
  Description   : ise device.
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "ISEDevice"
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

#include "ISEDevice.h"
#include "ISEChannel.h"

#include <mpi_ise.h>

using namespace std;

namespace EyeseeLinux {

status_t ISEDevice::CameraProxyInfo::SetCameraChannelInfo(CameraChannelInfo *pCameraChnInfo)
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

status_t ISEDevice::CameraProxyInfo::addReleaseFrame(VIDEO_FRAME_INFO_S *pVIFrame)
{
    AutoMutex lock(mFrameBufListLock);
    bool bFindFlag = false;
    int matchCount = 0;
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
            aloge("fatal error! MPP-ISE release frame is not first one of mUsedFrameBufList!");
        }
        if(matchCount > 1)
        {
            aloge("fatal error! find [%d] matched frames in mUsedFrameBufList!", matchCount);
        }
        mWaitReleaseFrameBufList.splice(mWaitReleaseFrameBufList.end(), mUsedFrameBufList, firstOne);
    }
    return NO_ERROR;
}

ISEDevice::CameraProxyInfo::CameraProxyInfo()
{
    mpCameraProxy = NULL;
    mnCameraChannel = 0;;
    mIdleFrameBufList.resize(10);
}

ISEDevice::CameraProxyInfo::CameraProxyInfo(const CameraProxyInfo& ref)
{
    mpCameraProxy       = ref.mpCameraProxy;
    mnCameraChannel     = ref.mnCameraChannel;
    mIdleFrameBufList   = ref.mIdleFrameBufList;
    mReadyFrameBufList  = ref.mReadyFrameBufList;
    mUsedFrameBufList   = ref.mUsedFrameBufList;
    mWaitReleaseFrameBufList = ref.mWaitReleaseFrameBufList;
    mReleasingFrameBufList = ref.mReleasingFrameBufList;
}

ISEDevice::CameraProxyInfo::~CameraProxyInfo()
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

ISEDevice::CameraProxyListener::CameraProxyListener(ISEDevice *pIse, int nCameraIndex)
{
    mpIse = pIse;
    mCameraIndex = nCameraIndex;
}
void ISEDevice::CameraProxyListener::dataCallbackTimestamp(const void *pdata)
{
    mpIse->dataCallbackTimestamp(mCameraIndex, (const VIDEO_FRAME_BUFFER_S*)pdata);
}

ISEDevice::DoSendPicThread::DoSendPicThread(ISEDevice *pIseDev)
    :mpIseDev(pIseDev)
{
    mbWaitInputFrameBuf = false;
    mSendPicThreadState = ISE_SENDPIC_STATE_LOADED;
    mbThreadOK = true;
    if(NO_ERROR != run("ISESendPicThread"))
    {
        aloge("fatal error! create thread fail");;
        mbThreadOK = false;
    }
}
ISEDevice::DoSendPicThread::~DoSendPicThread()
{
    SendPicThreadReset();
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeSendPic_Exit;
    mSendPicMsgQueue.queueMessage(&msg);
    requestExitAndWait();
    mSendPicMsgQueue.flushMessage();
}

status_t ISEDevice::DoSendPicThread::SendPicThreadStart()
{
    AutoMutex autoLock(mLock);
    AutoMutex autoLock2(mStateLock);
    if(mSendPicThreadState == ISE_SENDPIC_STATE_STARTED)
    {
        alogd("already in started");
        return NO_ERROR;
    }
    if(mSendPicThreadState != ISE_SENDPIC_STATE_LOADED && mSendPicThreadState != ISE_SENDPIC_STATE_PAUSED)
    {
        aloge("fatal error! can't call in state[0x%x]", mSendPicThreadState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeSendPic_SetState;
    msg.mPara0 = ISE_SENDPIC_STATE_STARTED;
    mSendPicMsgQueue.queueMessage(&msg);
    while(ISE_SENDPIC_STATE_STARTED != mSendPicThreadState)
    {
    	mStartCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

status_t ISEDevice::DoSendPicThread::SendPicThreadPause()
{
    AutoMutex autoLock(mLock);
    AutoMutex autoLock2(mStateLock);
    if(mSendPicThreadState == ISE_SENDPIC_STATE_PAUSED)
    {
        alogd("already in paused");
        return NO_ERROR;
    }
    if(mSendPicThreadState != ISE_SENDPIC_STATE_STARTED)
    {
        aloge("fatal error! can't call in state[0x%x]", mSendPicThreadState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeSendPic_SetState;
    msg.mPara0 = ISE_SENDPIC_STATE_PAUSED;
    mSendPicMsgQueue.queueMessage(&msg);
    while(ISE_SENDPIC_STATE_PAUSED != mSendPicThreadState)
    {
    	mPauseCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}
status_t ISEDevice::DoSendPicThread::SendPicThreadReset()
{
    AutoMutex autoLock(mLock);
    {
        AutoMutex autoLock2(mStateLock);
        if(mSendPicThreadState != ISE_SENDPIC_STATE_LOADED)
        {
            EyeseeMessage msg;
            msg.mMsgType = MsgTypeSendPic_SetState;
            msg.mPara0 = ISE_SENDPIC_STATE_LOADED;
            mSendPicMsgQueue.queueMessage(&msg);
            while(ISE_SENDPIC_STATE_LOADED != mSendPicThreadState)
            {
            	mLoadedCompleteCond.wait(mStateLock);
            }
        }
    }
    mbWaitInputFrameBuf = false;
    return NO_ERROR;
}

status_t ISEDevice::DoSendPicThread::SendMessage_InputFrameReady()
{
    mbWaitInputFrameBuf = false;
    EyeseeMessage msg;
    msg.mMsgType = DoSendPicThread::MsgTypeSendPic_InputFrameAvailable;
    mSendPicMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

bool ISEDevice::DoSendPicThread::threadLoop()
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

bool ISEDevice::DoSendPicThread::sendPicThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
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
                else if(ISE_SENDPIC_STATE_LOADED == msg.mPara0)
                {
                    alogd("sendPic thread state[0x%x]->loaded", mSendPicThreadState);
                    mSendPicThreadState = ISE_SENDPIC_STATE_LOADED;
                    mLoadedCompleteCond.signal();
                }
                else if(ISE_SENDPIC_STATE_PAUSED == msg.mPara0)
                {
                    alogd("sendPic thread state[0x%x]->paused", mSendPicThreadState);
                    mSendPicThreadState = ISE_SENDPIC_STATE_PAUSED;
                    mPauseCompleteCond.signal();
                }
                else if(ISE_SENDPIC_STATE_STARTED == msg.mPara0)
                {
                    alogd("sendPic thread state[0x%x]->started", mSendPicThreadState);
                    mSendPicThreadState = ISE_SENDPIC_STATE_STARTED;
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
        if(ISE_SENDPIC_STATE_STARTED == mSendPicThreadState)
        {
            for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
            {
                it->mFrameBufListLock.lock();
            }

            unsigned int minFrameNum = (unsigned int)-1;
            unsigned int maxFrameNum = 0;
            unsigned int sendFrameNum = 0;
            for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
            {
                if(it->mReadyFrameBufList.size() < minFrameNum)
                {
                    minFrameNum = it->mReadyFrameBufList.size();
                }
                if(it->mReadyFrameBufList.size() > maxFrameNum)
                {
                    maxFrameNum = it->mReadyFrameBufList.size();
                }
            }
            sendFrameNum = (ISEMODE_ONE_FISHEYE == mpIseDev->mIseAttr.iseMode) ? maxFrameNum : minFrameNum;

            VIDEO_FRAME_INFO_S *pbuf0;
            VIDEO_FRAME_INFO_S *pbuf1;
            for(unsigned int i=0; i<sendFrameNum; i++)
            {
                if(mpIseDev->mIseAttr.iseMode == ISEMODE_ONE_FISHEYE)
                {
                    pbuf0 = &mpIseDev->mCameraProxies[0].mReadyFrameBufList.begin()->mFrameBuf;
                    pbuf1 = NULL;
                }
                else
                {
                    pbuf0 = &mpIseDev->mCameraProxies[0].mReadyFrameBufList.begin()->mFrameBuf;
                    pbuf1 = &mpIseDev->mCameraProxies[1].mReadyFrameBufList.begin()->mFrameBuf;
                }
                ret = AW_MPI_ISE_SendPic(mpIseDev->mGroupId, pbuf0, pbuf1, -1);
                if (ret != SUCCESS) 								// if send frm fail,move frm to wailRlsList directly.
                { 
					if(ISEMODE_ONE_FISHEYE == mpIseDev->mIseAttr.iseMode )	// process for failing in mode:ISEMODE_ONE_FISHEYE
					{
						mpIseDev->mCameraProxies[0].mWaitReleaseFrameBufList.splice(mpIseDev->mCameraProxies[0].mWaitReleaseFrameBufList.end(), 
																						mpIseDev->mCameraProxies[0].mReadyFrameBufList,
																						mpIseDev->mCameraProxies[0].mReadyFrameBufList.begin());
					}
					else													// process for failing in other mode
					{
						for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
						{
							it->mWaitReleaseFrameBufList.splice(it->mWaitReleaseFrameBufList.end(), 
																	it->mReadyFrameBufList, it->mReadyFrameBufList.begin());
						}
					}
                    aloge("fatal error! why MPP-ISE can't accept new frame! drop it directly");
                }
				else												// if send frm suc,mov frm to UsedFrmList	
				{
					for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
					{
						it->mUsedFrameBufList.splice(it->mUsedFrameBufList.end(), it->mReadyFrameBufList, it->mReadyFrameBufList.begin());
					}
				}
            }
            mbWaitInputFrameBuf = true;
            for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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

ISEDevice::DoReturnPicThread::DoReturnPicThread(ISEDevice *pIseDev)
    :mpIseDev(pIseDev)
{
    mbWaitInputFrameBuf = false;
    mState = ISE_RETURNPIC_STATE_LOADED;
    mbThreadOK = true;
    if(NO_ERROR != run("ISEReturnPic"))
    {
        aloge("fatal error! create thread fail");
        mbThreadOK = false;
    }
}

ISEDevice::DoReturnPicThread::~DoReturnPicThread()
{
    Reset();
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeReturnPic_Exit;
    mMsgQueue.queueMessage(&msg);
    requestExitAndWait();
    mMsgQueue.flushMessage();
}

status_t ISEDevice::DoReturnPicThread::Start()
{
    AutoMutex autoLock(mLock);
    if(mState == ISE_RETURNPIC_STATE_STARTED)
    {
        alogd("already in started");
        return NO_ERROR;
    }
    if(mState != ISE_RETURNPIC_STATE_LOADED)
    {
        aloge("fatal error! can't call in state[0x%x]", mState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeReturnPic_SetState;
    msg.mPara0 = ISE_RETURNPIC_STATE_STARTED;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t ISEDevice::DoReturnPicThread::Reset()
{
    AutoMutex autoLock(mLock);
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeReturnPic_ReturnAllFrames;
    mMsgQueue.queueMessage(&msg);
    
    msg.reset();
    msg.mMsgType = MsgTypeReturnPic_SetState;
    msg.mPara0 = ISE_RETURNPIC_STATE_LOADED;
    mMsgQueue.queueMessage(&msg);

    AutoMutex autoLock2(mStateLock);
    while(mState != ISE_RETURNPIC_STATE_LOADED)
    {
        mStateCond.wait(mStateLock);
    }

    mbWaitInputFrameBuf = false;
    return NO_ERROR;
}

status_t ISEDevice::DoReturnPicThread::notifyReturnPictureCome()
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

status_t ISEDevice::DoReturnPicThread::SendCommand_ReturnAllFrames()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeReturnPic_ReturnAllFrames;
    mMsgQueue.queueMessage(&msg);
    
    return NO_ERROR;
}

bool ISEDevice::DoReturnPicThread::threadLoop()
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

status_t ISEDevice::DoReturnPicThread::returnAllFrames()
{
    bool bHasFrame;
    // return all waitReleaseFrames
    while(1)
    {
        bHasFrame = false;
        for(std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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
        for(std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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
        for(std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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
    for(std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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

bool ISEDevice::DoReturnPicThread::returnPicThread()
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
                else if(ISE_RETURNPIC_STATE_LOADED == msg.mPara0)
                {
                    //returnAllFrames();
                    alogd("returnPic thread state[0x%x]->loaded", mState);
                    mState = ISE_RETURNPIC_STATE_LOADED;
                }
                else if(ISE_RETURNPIC_STATE_STARTED == msg.mPara0)
                {
                    alogd("returnPic thread state[0x%x]->started", mState);
                    mState = ISE_RETURNPIC_STATE_STARTED;
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
        if(ISE_RETURNPIC_STATE_STARTED == mState)
        {
            bool bHasFrame;
            while(1)
            {
                bHasFrame = false;
                for(std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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
                    //if all cameraproxies has no frame, check them again!
                    for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
                    {
                        it->mFrameBufListLock.lock();
                    }
                    for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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
                    for (std::vector<CameraProxyInfo>::iterator it = mpIseDev->mCameraProxies.begin(); it!=mpIseDev->mCameraProxies.end(); ++it)
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
        
ISEDevice::DoCaptureThread::DoCaptureThread(ISEDevice *pIseDev)
    : mpIseDev(pIseDev)
{
    mCapThreadState = ISE_CAPTURE_STATE_LOADED;
    mbThreadOK = true;
    if(NO_ERROR != run("ISECaptureThread"))
    {
        aloge("fatal error! create thread fail");;
        mbThreadOK = false;
    }
}

ISEDevice::DoCaptureThread::~DoCaptureThread()
{
    CaptureThreadReset();
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_Exit;
    mCaptureMsgQueue.queueMessage(&msg);
    requestExitAndWait();

    mCaptureMsgQueue.flushMessage();
}

status_t ISEDevice::DoCaptureThread::CaptureThreadStart()
{
    AutoMutex autoLock(mLock);
    AutoMutex autoLock2(mStateLock);
    if(mCapThreadState == ISE_CAPTURE_STATE_STARTED)
    {
        alogd("already in started");
        return NO_ERROR;
    }
    if(mCapThreadState != ISE_CAPTURE_STATE_LOADED && mCapThreadState != ISE_CAPTURE_STATE_PAUSED)
    {
        aloge("fatal error! can't call in state[0x%x]", mCapThreadState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_SetState;
    msg.mPara0 = ISE_CAPTURE_STATE_STARTED;
    mCaptureMsgQueue.queueMessage(&msg);
    while(ISE_CAPTURE_STATE_STARTED != mCapThreadState)
    {
    	mStartCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

status_t ISEDevice::DoCaptureThread::CaptureThreadPause()
{
    AutoMutex autoLock(mLock);
    AutoMutex autoLock2(mStateLock);
    if(mCapThreadState == ISE_CAPTURE_STATE_PAUSED)
    {
        alogd("already in paused");
        return NO_ERROR;
    }
    if(mCapThreadState != ISE_CAPTURE_STATE_STARTED)
    {
        aloge("fatal error! can't call in state[0x%x]", mCapThreadState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_SetState;
    msg.mPara0 = ISE_CAPTURE_STATE_PAUSED;
    mCaptureMsgQueue.queueMessage(&msg);
    while(ISE_CAPTURE_STATE_PAUSED != mCapThreadState)
    {
    	mPauseCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

status_t ISEDevice::DoCaptureThread::CaptureThreadReset()
{
    AutoMutex autoLock(mLock);
    AutoMutex autoLock2(mStateLock);
    if(mCapThreadState != ISE_CAPTURE_STATE_LOADED)
    {
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeCapture_SetState;
        msg.mPara0 = ISE_CAPTURE_STATE_LOADED;
        mCaptureMsgQueue.queueMessage(&msg);
        while(ISE_CAPTURE_STATE_LOADED != mCapThreadState)
        {
        	mLoadedCompleteCond.wait(mStateLock);
        }
    }
    return NO_ERROR;
}

bool ISEDevice::DoCaptureThread::threadLoop()
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

bool ISEDevice::DoCaptureThread::captureThread()
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
                else if(ISE_CAPTURE_STATE_LOADED == msg.mPara0)
                {
                    alogd("capture thread state[0x%x]->loaded", mCapThreadState);
                    mCapThreadState = ISE_CAPTURE_STATE_LOADED;
                    mLoadedCompleteCond.signal();
                }
                else if(ISE_CAPTURE_STATE_PAUSED == msg.mPara0)
                {
                    alogd("capture thread state[0x%x]->paused", mCapThreadState);
                    mCapThreadState = ISE_CAPTURE_STATE_PAUSED;
                    mPauseCompleteCond.signal();
                }
                else if(ISE_CAPTURE_STATE_STARTED == msg.mPara0)
                {
                    alogd("capture thread state[0x%x]->started", mCapThreadState);
                    mCapThreadState = ISE_CAPTURE_STATE_STARTED;
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
        if(ISE_CAPTURE_STATE_STARTED == mCapThreadState)
        {
            //AutoMutex autoLock(mpIseDev->mISEChannelVectorLock);
            for (vector<ISEChannelInfo>::iterator it = mpIseDev->mISEChannelVector.begin(); it != mpIseDev->mISEChannelVector.end(); ++it)
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

ISEDevice::ISEDevice(unsigned int nISEId)
    : mISEId(nISEId)
    , mDevState(ISE_STATE_CONSTRUCTED)
    , mpSendPicThread(NULL)
    , mpCapThread(NULL)
{
    alogd("Construct");
    memset(&mIseAttr, 0, sizeof(mIseAttr));

    mpSendPicThread = new DoSendPicThread(this);
    mpReturnPicThread = new DoReturnPicThread(this);
    mpCapThread = new DoCaptureThread(this);
    mColorSpace = V4L2_COLORSPACE_JPEG;
    //MPP components
//    mISEGroup.mModId = MOD_ID_ISE;
//    mISEGroup.mDevId = MM_INVALID_DEV;
//    mISEGroup.mChnId = MM_INVALID_CHN;
    mGroupId = MM_INVALID_DEV;
}

ISEDevice::~ISEDevice()
{
    alogd("Destruct");
    reset();
    delete mpSendPicThread;
    delete mpReturnPicThread;
    delete mpCapThread;
}

ISEChannel *ISEDevice::searchISEChannel(int chnId)
{
    AutoMutex lock(mISEChannelVectorLock);
    for (vector<ISEChannelInfo>::iterator it = mISEChannelVector.begin(); it != mISEChannelVector.end(); ++it) {
        if (it->mChnId== chnId) {
            return it->mpChannel;
        }
    }
    return NULL;
}

status_t ISEDevice::prepare(ISE_MODE_E mode,PIXEL_FORMAT_E pixelformat)
{
    AutoMutex lock(mLock);
    status_t result;
    if(mDevState != ISE_STATE_CONSTRUCTED) 
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

    if(ISE_MODE_ONE_FISHEYE == mode)
    {
        mIseAttr.iseMode = ISEMODE_ONE_FISHEYE;
    }
    else if(ISE_MODE_TWO_FISHEYE == mode)
    {
        mIseAttr.iseMode = ISEMODE_TWO_FISHEYE;
    }
    else
    {
        mIseAttr.iseMode = ISEMODE_TWO_ISE;
    }
    mIseAttr.mPixelFormat = pixelformat;
    ERRORTYPE ret;
    bool bSuccessFlag = false;
    //mISEGroup.mDevId = 0;
    mGroupId = 0;
    while(mGroupId < ISE_MAX_GRP_NUM)
    {
        ret = AW_MPI_ISE_CreateGroup(mGroupId, &mIseAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create ISE group[%d] success!", mGroupId);
            break;
        }
        else if(ERR_ISE_EXIST == ret)
        {
            alogd("ISE group[%d] is exist, find next!", mGroupId);
            mGroupId++;
        }
        else
        {
            alogd("create ISE group[%d] ret[0x%x], find next!", mGroupId, ret);
            mGroupId++;
        }
    }
    if(false == bSuccessFlag)
    {
        mGroupId = MM_INVALID_DEV;
        aloge("fatal error! create ISE group fail!");
        return UNKNOWN_ERROR;
    }
    alogd("IseDev CreateGroup with GrpId=%d", mGroupId);

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)this;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    ret = AW_MPI_ISE_RegisterCallback(mGroupId, &cbInfo);

    mDevState = ISE_STATE_PREPARED;

    return NO_ERROR;
_err0:
    return UNKNOWN_ERROR;
    
}

status_t ISEDevice::release_l()
{
    if (mDevState != ISE_STATE_PREPARED)
    {
        aloge("release in error state %d", mDevState);
        return INVALID_OPERATION;
    }
    mpSendPicThread->SendPicThreadReset();
    mpReturnPicThread->Reset();
    mpCapThread->CaptureThreadReset();
    ERRORTYPE ret = AW_MPI_ISE_DestroyGroup(mGroupId);
    if (ret != SUCCESS)
    {
        aloge("AW_MPI_ISE_DestroyGroup error!");
        return UNKNOWN_ERROR;
    }
    mGroupId = MM_INVALID_DEV;

    mDevState = ISE_STATE_CONSTRUCTED;

    return NO_ERROR;
}
status_t ISEDevice::release()
{
    AutoMutex lock(mLock);
    return release_l();
}

status_t ISEDevice::start()
{
    AutoMutex lock(mLock);
    if(mDevState == ISE_STATE_STARTED)
    {
        alogd("already in state started");
        return NO_ERROR;
    }
    if (mDevState != ISE_STATE_PREPARED) 
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
    if((mIseAttr.iseMode == ISEMODE_TWO_FISHEYE) || (mIseAttr.iseMode == ISEMODE_TWO_ISE))
    {
        if(mCameraProxies.size() != 2)
        {
            aloge("fatal error! two_fisheye mode need two cameras!");
            return INVALID_OPERATION;
        }
    }
    else
    {
        if(mCameraProxies.size() != 1)
        {
            aloge("fatal error! only one camera is permit under iseMode[0x%x]!", mIseAttr.iseMode);
        }
    }

    ERRORTYPE ret = AW_MPI_ISE_Start(mGroupId);
    if (ret != SUCCESS)
    {
        aloge("AW_MPI_ISE_Start error!");
        return UNKNOWN_ERROR;
    }
    mDevState = ISE_STATE_STARTED;
    //start sendPic_thread and capture_thread.
    mpSendPicThread->SendPicThreadStart();
    mpReturnPicThread->Start();
    mpCapThread->CaptureThreadStart();

    for(unsigned int i=0; i<mCameraProxies.size(); i++)
    {
        mCameraProxies[i].mpCameraProxy->startRecording(
            mCameraProxies[i].mnCameraChannel,
            new CameraProxyListener(this, i),
            (int)mISEId);
    }

    CameraParameters camParam;
    getParameters(0, camParam);

    return NO_ERROR;
}

status_t ISEDevice::stop_l()
{
    if (mDevState != ISE_STATE_STARTED) 
    {
        aloge("stop in error state %d", mDevState);
        return INVALID_OPERATION;
    }
    //pause capture thread
    mpCapThread->CaptureThreadPause();
    //stop camera sending frame here.
    for(unsigned int i=0; i<mCameraProxies.size(); i++)
    {
        mCameraProxies[i].mpCameraProxy->stopRecording(
            mCameraProxies[i].mnCameraChannel, 
            (int)mISEId);
    }
    //pause sendPic thread
    mpSendPicThread->SendPicThreadPause();

    {
        AutoMutex lock(mISEChannelVectorLock);
        for (vector<ISEChannelInfo>::iterator it = mISEChannelVector.begin(); it != mISEChannelVector.end(); ++it) 
        {
          #if 0 //by andy
            it->mpChannel->stopPreview();
            it->mpChannel->releaseAllISEFrames();
          #else
            it->mpChannel->stopChannel();
          #endif
        }
    }
    //stop ISE 
    ERRORTYPE ret = AW_MPI_ISE_Stop(mGroupId);
    if (ret != SUCCESS) 
    {
        aloge("AW_MPI_ISE_Stop error!");
        //return UNKNOWN_ERROR;
    }
    //let return Pic thread return all frames.
    mpReturnPicThread->SendCommand_ReturnAllFrames();
    mDevState = ISE_STATE_PREPARED;
    return NO_ERROR;
}

status_t ISEDevice::stop()
{
    AutoMutex lock(mLock);
    return stop_l();
}

status_t ISEDevice::reset()
{
    status_t ret;
    AutoMutex lock(mLock);
    alogd("reset");
    if(ISE_STATE_CONSTRUCTED == mDevState)
    {
        return NO_ERROR;
    }
    if(ISE_STATE_STARTED == mDevState)
    {
        ret = stop_l();
    }
    if(ISE_STATE_PREPARED == mDevState)
    {
        ret = release_l();
    }
    return NO_ERROR;
}

ISE_CHN ISEDevice::openChannel(ISE_CHN_ATTR_S *pChnAttr)
{
    AutoMutex lock(mLock);

    if(mDevState != ISE_STATE_PREPARED)
    {
        aloge("open channel in error state %d", mDevState);
        return INVALID_OPERATION;
    }
    ISEChannel *pChannel = new ISEChannel(mGroupId, mIseAttr.iseMode, pChnAttr);
    if(pChannel->getChannelId() != MM_INVALID_CHN)
    {
        //CameraParameters IseChnParam;
        //pChannel->getParameters(IseChnParam);
        //IseChnParam.setColorSpace(mColorSpace);
        //pChannel->setParameters(IseChnParam);
        ISEChannelInfo iseChn;
        iseChn.mChnId = pChannel->getChannelId();
        iseChn.mpChannel = pChannel;
        AutoMutex lock(mISEChannelVectorLock);
        mISEChannelVector.push_back(iseChn);
        return iseChn.mChnId;
    }
    else
    {
        delete pChannel;
        pChannel = NULL;
        return MM_INVALID_CHN;
    }
}

status_t ISEDevice::closeChannel(ISE_CHN chnId)
{
    AutoMutex lock(mLock);
    if(mDevState != ISE_STATE_PREPARED) 
    {
        aloge("close channel in error state %d", mDevState);
        return INVALID_OPERATION;
    }
    
    bool found = false;
    vector<ISEChannelInfo>::iterator it;
    AutoMutex lock2(mISEChannelVectorLock);
    for (it = mISEChannelVector.begin(); it != mISEChannelVector.end(); ++it) 
    {
        if (it->mChnId == chnId) 
        {
            found = true;
            break;
        }
    }

    if (!found) 
    {
        aloge("ISE scaler channel %d is not exist!", chnId);
        return INVALID_OPERATION;
    }

    delete it->mpChannel;
    mISEChannelVector.erase(it);

    return NO_ERROR;
}

status_t ISEDevice::startChannel(ISE_CHN chnId)
{
    AutoMutex lock(mLock);
    if(mDevState != ISE_STATE_PREPARED)
    {
        aloge("close channel in error state %d", mDevState);
        return INVALID_OPERATION;
    }

    bool found = false;
    vector<ISEChannelInfo>::iterator it;
    AutoMutex lock2(mISEChannelVectorLock);
    for (it = mISEChannelVector.begin(); it != mISEChannelVector.end(); ++it)
    {
        if (it->mChnId == chnId)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        aloge("ISE scaler channel %d is not exist!", chnId);
        return INVALID_OPERATION;
    }

    return it->mpChannel->startChannel();
}

status_t ISEDevice::setParameters(int chnId, CameraParameters &param)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) {
        aloge("channel %d is not exit!", chnId);
        return INVALID_OPERATION;
    }
    return pIseChn->setParameters(param);
}

status_t ISEDevice::getParameters(int chnId, CameraParameters &param)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) {
        aloge("channel %d is not exit!", chnId);
        return INVALID_OPERATION;
    }
    return pIseChn->getParameters(param);
}

status_t ISEDevice::setChannelDisplay(int chnId, int hlay)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) {
        aloge("channel %d is not exit!", chnId);
        return INVALID_OPERATION;
    }
    return pIseChn->setChannelDisplay(hlay);
}

bool ISEDevice::previewEnabled(int chnId)
{
    ISEChannel *pChannel = searchISEChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return false;
    }
    return pChannel->isPreviewEnabled();
}

status_t ISEDevice::startRender(int chnId)
{
    ISEChannel *pChannel = searchISEChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startRender();
}
status_t ISEDevice::stopRender(int chnId)
{
    ISEChannel *pChannel = searchISEChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopRender();
}

status_t ISEDevice::setMoPortAttr(ISE_CHN_ATTR_S *pAttr)
{
    if (mGroupId != MM_INVALID_DEV)
    {
        bool found = false;
        vector<ISEChannelInfo>::iterator it;
        ISE_CHN chnId = 0;
        AutoMutex lock2(mISEChannelVectorLock);
        for (it = mISEChannelVector.begin(); it != mISEChannelVector.end(); ++it)
        {
            if (it->mChnId == chnId)
            {
                found = true;
                break;
            }
        }
        if ((true == found) && (mIseAttr.iseMode == ISEMODE_ONE_FISHEYE))
        {
            //only can update main_chn(portId=0) attr, do not update scale_chn!
            return AW_MPI_ISE_SetPortAttr(mGroupId, chnId, pAttr);
        }
        else
        {
            aloge("set MoPortAttr fail! Because ise chn has not create!");
            return UNKNOWN_ERROR;
        }
    }
    else {

        return UNKNOWN_ERROR;
    }
}

void ISEDevice::releaseRecordingFrame(int chnId, uint32_t index)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) 
    {
        aloge("ISE channel %d is not exist!", chnId);
        return;
    }
    pIseChn->releaseRecordingFrame(index);
}

status_t ISEDevice::startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) 
    {
        aloge("ISE channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pIseChn->startRecording(pCb, recorderId);
}

status_t ISEDevice::stopRecording(int chnId, int recorderId)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) 
    {
        aloge("ISE channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pIseChn->stopRecording(recorderId);
}

void ISEDevice::setDataListener(int chnId, DataListener *pCb)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL)
    {
        aloge("ISE channel %d is not exist!", chnId);
        return;
    }
    pIseChn->setDataListener(pCb);
}

void ISEDevice::setNotifyListener(int chnId, NotifyListener *pCb)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) 
    {
        aloge("ISE channel %d is not exist!", chnId);
        return;
    }
    pIseChn->setNotifyListener(pCb);
}

status_t ISEDevice::takePicture(int chnId, unsigned int msgType, PictureRegionCallback *pPicReg)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) {
        aloge("channel %d is not exit!", chnId);
        return INVALID_OPERATION;
    }
    return pIseChn->takePicture(msgType, pPicReg);
}

status_t ISEDevice::notifyPictureRelease(int chnId)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) {
        aloge("channel %d is not exist!", chnId);
        return INVALID_OPERATION;
    }
    return pIseChn->notifyPictureRelease();
}

status_t ISEDevice::cancelContinuousPicture(int chnId)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL) {
        aloge("channel %d is not exit!", chnId);
        return INVALID_OPERATION;
    }
    return pIseChn->cancelContinuousPicture();
}

/*
void ISEDevice::postDataCompleted(int chnId, const void *pData, int size)
{
    ISEChannel *pIseChn = searchISEChannel(chnId);
    if (pIseChn == NULL)
    {
        aloge("ISE channel %d is not exist!", chnId);
        return;
    }
    pIseChn->postDataCompleted(pData, size);
}
*/
//status_t ISEDevice::setCamera(EyeseeCamera *pCam0, int nCam0Chn, EyeseeCamera *pCam1, int nCam1Chn)
status_t ISEDevice::setCamera(std::vector<CameraChannelInfo>& cameraChannels)
{
    if(cameraChannels.size() <= 0)
    {
        aloge("fatal error! none camera is set!");
        return BAD_VALUE;
    }

    if (mDevState != ISE_STATE_PREPARED) 
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
    if(mIseAttr.iseMode == ISEMODE_ONE_FISHEYE && cameraChannels.size() > 1)
    {
        aloge("fatal error! one fishEye mode can't set two or more cameras!");
    }
    return NO_ERROR;
}

void ISEDevice::dataCallbackTimestamp(int nCameraIndex, const VIDEO_FRAME_BUFFER_S *pCameraFrameInfo)
{
    //AutoMutex lock(mLock);
    if(NULL == pCameraFrameInfo)
    {
        aloge("fatal error! camera frame is NULL?");
        return;
    }
    if (mDevState != ISE_STATE_STARTED) 
    {
        alogd("Be careful! camera[%d] call dataCallbackTimestamp when ise device state is [0x%x]", nCameraIndex, mDevState);
        //mCameraProxies[nCameraIndex].mpCameraProxy->releaseRecordingFrame(mCameraProxies[nCameraIndex].mnCameraChannel, pCameraFrameInfo->mFrameBuf.mId);
        //return;
    }
    
    CameraProxyInfo& CameraProxy = mCameraProxies[nCameraIndex];
    {
        AutoMutex lock(CameraProxy.mFrameBufListLock);
        if(CameraProxy.mIdleFrameBufList.empty())
        {
            alogd("Be careful! ISE input IdleFrameBuf list is empty, increase!");
            VIDEO_FRAME_BUFFER_S elem;
            memset(&elem, 0, sizeof(VIDEO_FRAME_BUFFER_S));
            CameraProxy.mIdleFrameBufList.insert(CameraProxy.mIdleFrameBufList.end(), 5, elem);
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

void ISEDevice::notify(MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    if(MOD_ID_ISE == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            { // vo callback, not use!
                VI_FRAME_BUF_INFOS *pbufs = (VI_FRAME_BUF_INFOS*)pEventData;
                VIDEO_FRAME_INFO_S *pbuf0 = pbufs->pbuf0;
                VIDEO_FRAME_INFO_S *pbuf1 = pbufs->pbuf1;
                if(pbuf0 != NULL)
                {
                    mCameraProxies[0].addReleaseFrame(pbuf0);
                }
                if(pbuf1 != NULL)
                {
                    mCameraProxies[1].addReleaseFrame(pbuf1);
                }
                mpReturnPicThread->notifyReturnPictureCome();
                break;
            }
            case MPP_EVENT_RELEASE_ISE_VIDEO_BUFFER0:
            {
                //ise callback to notify return frame to camera
                VIDEO_FRAME_INFO_S *pbuf0 = (VIDEO_FRAME_INFO_S *)pEventData;
                if(pbuf0 != NULL)
                {
                    mCameraProxies[0].addReleaseFrame(pbuf0);
                }
                mpReturnPicThread->notifyReturnPictureCome();
                break;
            }
            case MPP_EVENT_RELEASE_ISE_VIDEO_BUFFER1:
            {
                VIDEO_FRAME_INFO_S *pbuf1 = (VIDEO_FRAME_INFO_S *)pEventData;
                if(pbuf1 != NULL)
                {
                    mCameraProxies[1].addReleaseFrame(pbuf1);
                }
                mpReturnPicThread->notifyReturnPictureCome();
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from ISE group[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! another MPP MOD?");
    }
}
ERRORTYPE ISEDevice::MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ((ISEDevice*)cookie)->notify(pChn, event, pEventData);
    return SUCCESS;
}

}; /* namespace EyeseeLinux */
