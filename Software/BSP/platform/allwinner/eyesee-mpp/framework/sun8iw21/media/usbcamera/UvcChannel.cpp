/******************************************************************************
  Copyright (C), 2001-2019, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : UvcChannel.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2018/12/18
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "UvcChannel"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>
#include <vector>

#include <memoryAdapter.h>
#include <SystemBase.h>
#include <VIDEO_FRAME_INFO_S.h>
#include <mpi_sys.h>
#include <mpi_uvc.h>
#include <mpi_videoformat_conversion.h>
#include "VdecFrameManager.h"

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <MediaStructConvert.h>
#include "UvcChannel.h"
//#include <ion_memmanager.h>
#include <ConfigOption.h>


using namespace std;

#define DEBUG_STORE_VI_FRAME (0)
#define DEBUG_SAVE_FRAME_TO_TMP (0)
#define DEBUG_SAVE_FRAME_NUM  (60)

namespace EyeseeLinux {

UvcChannel::DoCaptureThread::DoCaptureThread(UvcChannel *pUvcChn)
    : mpUvcChn(pUvcChn)
{
    mCapThreadState = State::IDLE;
    char threadName[64];
    sprintf(threadName, "UvcChn[%d]Capture", (int)mpUvcChn->mChnId);
    status_t ret = run(threadName);
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread[%s] fail!", threadName);
    }
}

UvcChannel::DoCaptureThread::~DoCaptureThread()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t UvcChannel::DoCaptureThread::startCapture()
{
    AutoMutex lock(mStateLock);
    if(mCapThreadState == State::STARTED)
    {
        alogd("already in started");
        return NO_ERROR;
    }
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::SetState;
    msg.mPara0 = (int)State::STARTED;
    mMsgQueue.queueMessage(&msg);
    while(State::STARTED != mCapThreadState)
    {
        mStartCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

status_t UvcChannel::DoCaptureThread::pauseCapture()
{
    AutoMutex lock(mStateLock);
    if(mCapThreadState == State::PAUSED)
    {
        alogd("already in paused");
        return NO_ERROR;
    }
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::SetState;
    msg.mPara0 = (int)State::PAUSED;
    mMsgQueue.queueMessage(&msg);
    while(State::PAUSED != mCapThreadState)
    {
        mPauseCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

bool UvcChannel::DoCaptureThread::threadLoop()
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

bool UvcChannel::DoCaptureThread::captureThread()
{
    bool bRunningFlag = true;
    bool bTakePictureStart = false;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    status_t eRet;
    VIDEO_FRAME_INFO_S buffer;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    int nGetFrameTimeout = 200; //unit:ms
PROCESS_MESSAGE:
    getMsgRet = mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(MsgType::SetState == (MsgType)msg.mMsgType)
        {
            AutoMutex autoLock(mStateLock);
            if((State)msg.mPara0 == mCapThreadState)
            {
                alogw("Be careful! same state[0x%x]", (int)mCapThreadState);
            }
            else if(State::PAUSED == (State)msg.mPara0)
            {
                alogv("Uvcchn[%d] captureThread state[0x%x]->paused", mpUvcChn->mChnId, mCapThreadState);
                mCapThreadState = State::PAUSED;
                mPauseCompleteCond.signal();
            }
            else if(State::STARTED == (State)msg.mPara0)
            {
                alogv("Uvcchn[%d] captureThread state[0x%x]->started", mpUvcChn->mChnId, mCapThreadState);
                mCapThreadState = State::STARTED;
                mStartCompleteCond.signal();
            }
            else
            {
                aloge("fatal error! check code!");
            }
        }
        else if(MsgType::TakePicture == (MsgType)msg.mMsgType)
        {
            if(!bTakePictureStart)
            {
                bTakePictureStart = true;
            }
            else
            {
                alogd("Be careful! UvcChn[%d] take picture is doing already!", mpUvcChn->mChnId);
            }
            alogd("UvcChn[%d] take picture mode is [0x%x]", mpUvcChn->mChnId, mpUvcChn->mTakePictureMode);
        }
        else if(MsgType::CancelContinuousPicture == (MsgType)msg.mMsgType)
        {
            if(!bTakePictureStart)
            {
                if(TAKE_PICTURE_MODE_CONTINUOUS == mpUvcChn->mTakePictureMode)
                {
                    mpUvcChn->mpPicThread->notifyPictureEnd();
                    mpUvcChn->mContinuousPictureStartTime = 0;
                    mpUvcChn->mContinuousPictureLast = 0;
                    mpUvcChn->mContinuousPictureInterval = 0;
                    mpUvcChn->mContinuousPictureCounter = 0;
                    mpUvcChn->mContinuousPictureMax = 0;
                    bTakePictureStart = false; 
                }
                else
                {
                    aloge("fatal error! UvcChn[%d] take picture mode[0x%x] is not continuous!", mpUvcChn->mChnId, mpUvcChn->mTakePictureMode);
                }
            }
            else
            {
                aloge("fatal error! UvcChn[%d] not start take picture, mode[0x%x]!", mpUvcChn->mChnId, mpUvcChn->mTakePictureMode);
            }
        }
        else if(MsgType::Exit == (MsgType)msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("UvcChn[%d] unknown msg[0x%x]!", mpUvcChn->mChnId, msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }
    
    if(mCapThreadState == State::PAUSED || mCapThreadState == State::IDLE)
    {
        mMsgQueue.waitMessage();
        goto PROCESS_MESSAGE;
    }
    memset(&buffer, 0, sizeof(VIDEO_FRAME_INFO_S));
    if(mpUvcChn->mChnId == UvcChn::UvcMainChn)
    {
        ret = AW_MPI_UVC_GetFrame((UVC_DEV)mpUvcChn->mUvcDev.c_str(), mpUvcChn->mVirtualUvcChn, &buffer, nGetFrameTimeout);
        if(SUCCESS == ret)
        {
            eRet = NO_ERROR;
        }
        else
        {
            eRet = UNKNOWN_ERROR;
        }
    }
    else if(mpUvcChn->mChnId == UvcChn::VdecMainChn || mpUvcChn->mChnId == UvcChn::VdecSubChn)
    {
        eRet = mpUvcChn->mpVdecFrameManager->getFrame(mpUvcChn->mChnId, &buffer, nGetFrameTimeout);
    }
    else
    {
        aloge("fatal error! check code!");
        eRet = UNKNOWN_ERROR;
    }
    if (eRet != SUCCESS)
    {
        mpUvcChn->mFrameBuffersLock.lock();
        alogd("UvcChn[%d] contain [%d] frame buffers", mpUvcChn->mChnId, mpUvcChn->mFrameBufferIdList.size());
        mpUvcChn->mFrameBuffersLock.unlock();
        int preview_num = mpUvcChn->mpPreviewQueue->GetValidElemNum();
        int picture_num = mpUvcChn->mpPictureQueue->GetValidElemNum();
        alogw("UvcChn[%d]: preview_num: %d, picture_num: %d, timeout: %dms", mpUvcChn->mChnId, preview_num, picture_num, nGetFrameTimeout);
        mpUvcChn->mCamDevTimeoutCnt++;
        if (mpUvcChn->mCamDevTimeoutCnt%20 == 0)
        {
            aloge("timeout for %d times when UvcChn[%d] GetFrame!", mpUvcChn->mCamDevTimeoutCnt, mpUvcChn->mChnId);
            mpUvcChn->mpCallbackNotifier->NotifyCameraDeviceTimeout();
        }
        goto PROCESS_MESSAGE;
    }
    mpUvcChn->mCamDevTimeoutCnt = 0;
    //buffer.VFrame.mEnvLV = AW_MPI_ISP_GetEnvLV(mIspDevId);
    //alogd("print vipp[%d]pts[%lld]ms", mChnId, buffer.VFrame.mpts/1000);
    mpUvcChn->mFrameCounter++;

    pFrmbuf = NULL;
    mpUvcChn->mFrameBuffersLock.lock();
    for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mpUvcChn->mFrameBuffers.begin(); it!=mpUvcChn->mFrameBuffers.end(); ++it)
    {
        if(it->mFrameBuf.mId == buffer.mId)
        {
            if(NULL == pFrmbuf)
            {
                pFrmbuf = &*it;
            }
            else
            {
                aloge("fatal error! UvcChn[%d] frame id[0x%x] is repeated!", mpUvcChn->mChnId, buffer.mId);
            }
        }
    }
    if(NULL == pFrmbuf)
    {
        alogv("UvcChn[%d] frame buffer array did not contain this bufferId[0x%x], add it.", mpUvcChn->mChnId, buffer.mId);
        mpUvcChn->mFrameBuffers.emplace_back();
        memset(&mpUvcChn->mFrameBuffers.back(), 0, sizeof(VIDEO_FRAME_BUFFER_S));
        pFrmbuf = &mpUvcChn->mFrameBuffers.back();
    }
    pFrmbuf->mFrameBuf = buffer;
    if (mpUvcChn->mLastZoom != mpUvcChn->mNewZoom)
    {
        calculateCrop(&mpUvcChn->mRectCrop, mpUvcChn->mNewZoom, mpUvcChn->mFrameWidthOut, mpUvcChn->mFrameHeightOut);
        mpUvcChn->mLastZoom = mpUvcChn->mNewZoom;
        alogd("zoom[%d], CROP: [%d, %d, %d, %d]", mpUvcChn->mNewZoom, mpUvcChn->mRectCrop.X, mpUvcChn->mRectCrop.Y, mpUvcChn->mRectCrop.Width, mpUvcChn->mRectCrop.Height);
    }
    pFrmbuf->mFrameBuf.VFrame.mOffsetTop = mpUvcChn->mRectCrop.Y;
    pFrmbuf->mFrameBuf.VFrame.mOffsetBottom = mpUvcChn->mRectCrop.Y + mpUvcChn->mRectCrop.Height;
    pFrmbuf->mFrameBuf.VFrame.mOffsetLeft = mpUvcChn->mRectCrop.X;
    pFrmbuf->mFrameBuf.VFrame.mOffsetRight = mpUvcChn->mRectCrop.X + mpUvcChn->mRectCrop.Width;
    pFrmbuf->mColorSpace = mpUvcChn->mColorSpace;
    //pFrmbuf->mIsThumbAvailable = 0;
    //pFrmbuf->mThumbUsedForPhoto = 0;
    pFrmbuf->mRefCnt = 1;
    mpUvcChn->mFrameBufferIdList.push_back(pFrmbuf->mFrameBuf.mId);
    mpUvcChn->mFrameBuffersLock.unlock();

    if(false == bTakePictureStart)
    {
        {
            AutoMutex lock(mpUvcChn->mRefCntLock);
            pFrmbuf->mRefCnt++;
        }
        while(1)
        {
            eRet = mpUvcChn->mpPreviewQueue->PutElemDataValid(pFrmbuf);
            if(eRet != NO_ERROR)
            {
                alogd("Be careful! preview Queue elem number is not enough, add one.", mpUvcChn->mpPreviewQueue->GetTotalElemNum());
                mpUvcChn->mpPreviewQueue->AddIdleElems(1);
            }
            else
            {
                break;
            }
        }
        mpUvcChn->mpPrevThread->notifyNewFrameCome();
    }
    else
    {
        if (mpUvcChn->mTakePictureMode == TAKE_PICTURE_MODE_NORMAL) 
        {
           aloge("fatal error! don't support normal mode take picture temporary!");
           mpUvcChn->mpPicThread->notifyPictureEnd();
           bTakePictureStart = false;
        } 
        else 
        {
            mpUvcChn->mRefCntLock.lock();
            pFrmbuf->mRefCnt++;
            mpUvcChn->mRefCntLock.unlock();
            while(1)
            {
                eRet = mpUvcChn->mpPreviewQueue->PutElemDataValid(pFrmbuf);
                if(eRet != NO_ERROR)
                {
                    alogd("Be careful! preview Queue elem number[%d] is not enough, add one.", mpUvcChn->mpPreviewQueue->GetTotalElemNum());
                    mpUvcChn->mpPreviewQueue->AddIdleElems(1);
                }
                else
                {
                    break;
                }
            }
            mpUvcChn->mpPrevThread->notifyNewFrameCome();

            if (mpUvcChn->mTakePictureMode == TAKE_PICTURE_MODE_FAST)
            {
                mpUvcChn->mRefCntLock.lock();
                pFrmbuf->mRefCnt++;
                mpUvcChn->mRefCntLock.unlock();
                while(1)
                {
                    eRet = mpUvcChn->mpPictureQueue->PutElemDataValid(pFrmbuf);
                    if(eRet != NO_ERROR)
                    {
                        alogd("Be careful! picture Queue elem number[%d] is not enough, add one.", mpUvcChn->mpPictureQueue->GetTotalElemNum());
                        mpUvcChn->mpPictureQueue->AddIdleElems(1);
                    }
                    else
                    {
                        break;
                    }
                }
                mpUvcChn->mpPicThread->notifyNewFrameCome();
                mpUvcChn->mpPicThread->notifyPictureEnd();
                bTakePictureStart = false;
            }
            else if(mpUvcChn->mTakePictureMode == TAKE_PICTURE_MODE_CONTINUOUS)
            {
                bool bPermit = false;
                if (0 == mpUvcChn->mContinuousPictureStartTime)    //let's begin!
                {
                    mpUvcChn->mContinuousPictureStartTime = CDX_GetSysTimeUsMonotonic()/1000;
                    mpUvcChn->mContinuousPictureLast = mpUvcChn->mContinuousPictureStartTime;
                    mpUvcChn->mContinuousPictureInterval = mpUvcChn->mParameters.getContinuousPictureIntervalMs();
                    mpUvcChn->mContinuousPictureCounter = 0;
                    mpUvcChn->mContinuousPictureMax = mpUvcChn->mParameters.getContinuousPictureNumber();
                    bPermit = true;
                    alogd("begin continous picture, will take [%d]pics, interval[%llu]ms, curTm[%lld]ms", mpUvcChn->mContinuousPictureMax, mpUvcChn->mContinuousPictureInterval, mpUvcChn->mContinuousPictureLast);
                }
                else
                {
                    if(mpUvcChn->mContinuousPictureInterval <= 0)
                    {
                        bPermit = true;
                    }
                    else
                    {
                        uint64_t nCurTime = CDX_GetSysTimeUsMonotonic()/1000;
                        if(nCurTime >= mpUvcChn->mContinuousPictureLast + mpUvcChn->mContinuousPictureInterval)
                        {
                            //alogd("capture picture, curTm[%lld]ms, [%lld][%lld]", nCurTime, mContinuousPictureLast, mContinuousPictureInterval);
                            bPermit = true;
                        }
                    }
                }

                if(bPermit)
                {
                    mpUvcChn->mRefCntLock.lock();
                    pFrmbuf->mRefCnt++;
                    mpUvcChn->mRefCntLock.unlock();
                    while(1)
                    {
                        eRet = mpUvcChn->mpPictureQueue->PutElemDataValid(pFrmbuf);
                        if(eRet != NO_ERROR)
                        {
                            alogd("Be careful! picture Queue elem number[%d] is not enough, add one.", mpUvcChn->mpPictureQueue->GetTotalElemNum());
                            mpUvcChn->mpPictureQueue->AddIdleElems(1);
                        }
                        else
                        {
                            break;
                        }
                    }
                    mpUvcChn->mpPicThread->notifyNewFrameCome();
                    mpUvcChn->mContinuousPictureCounter++;
                    if(mpUvcChn->mContinuousPictureCounter >= mpUvcChn->mContinuousPictureMax)
                    {
                        mpUvcChn->mpPicThread->notifyPictureEnd();
                        mpUvcChn->mContinuousPictureStartTime = 0;
                        mpUvcChn->mContinuousPictureLast = 0;
                        mpUvcChn->mContinuousPictureInterval = 0;
                        mpUvcChn->mContinuousPictureCounter = 0;
                        mpUvcChn->mContinuousPictureMax = 0;
                        bTakePictureStart = false;
                    }
                    else
                    {
                        mpUvcChn->mContinuousPictureLast = mpUvcChn->mContinuousPictureStartTime+mpUvcChn->mContinuousPictureCounter*mpUvcChn->mContinuousPictureInterval;
                    }
                }
            }
            else
            {
                aloge("fatal error! any other take picture mode[0x%x]?", mpUvcChn->mTakePictureMode);
                bTakePictureStart = false;
            }
        }
    }
    mpUvcChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

status_t UvcChannel::DoCaptureThread::SendCommand_TakePicture()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::TakePicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t UvcChannel::DoCaptureThread::SendCommand_CancelContinuousPicture()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::CancelContinuousPicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

UvcChannel::DoPreviewThread::DoPreviewThread(UvcChannel *pUvcChn)
    : mpUvcChn(pUvcChn)
{
    mbWaitReleaseAllFrames = false;
    mbWaitPreviewFrame = false;
    char threadName[64];
    sprintf(threadName, "UvcChn[%d]Preview", (int)mpUvcChn->mChnId);
    status_t ret = run(threadName);
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
}

UvcChannel::DoPreviewThread::~DoPreviewThread()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t UvcChannel::DoPreviewThread::notifyNewFrameCome()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitPreviewFrame)
    {
        mbWaitPreviewFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = (int)MsgType::InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

status_t UvcChannel::DoPreviewThread::releaseAllFrames()
{
    AutoMutex lock(mWaitLock);
    mbWaitReleaseAllFrames = true;
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::releaseAllFrames;
    mMsgQueue.queueMessage(&msg);
    while(mbWaitReleaseAllFrames)
    {
        mCondReleaseAllFramesFinished.wait(mWaitLock);
    }
    return NO_ERROR;
}

bool UvcChannel::DoPreviewThread::threadLoop()
{
    if(!exitPending())
    {
        return previewThread();
    }
    else
    {
        return false;
    }
}

bool UvcChannel::DoPreviewThread::previewThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    PROCESS_MESSAGE:
    getMsgRet = mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(MsgType::InputFrameAvailable == (MsgType)msg.mMsgType)
        {
        }
        else if(MsgType::releaseAllFrames == (MsgType)msg.mMsgType)
        {
            AutoMutex lock(mWaitLock);
            for(;;)
            {
                pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpUvcChn->mpPreviewQueue->GetValidElemData();
                if(pFrmbuf)
                {
                    mpUvcChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
                    mpUvcChn->mpPreviewQueue->ReleaseElemData(pFrmbuf);
                }
                else
                {
                    break;
                }
            }
            if(mbWaitReleaseAllFrames)
            {
                mbWaitReleaseAllFrames = false;
            }
            else
            {
                aloge("fatal error! check code!");
            }
            mCondReleaseAllFramesFinished.signal();
        }
        else if(MsgType::Exit == (MsgType)msg.mMsgType)
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
    
    pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpUvcChn->mpPreviewQueue->GetValidElemData();
    if (pFrmbuf == NULL) 
    {
        {
            AutoMutex lock(mWaitLock);
            if(mpUvcChn->mpPreviewQueue->GetValidElemNum() > 0)
            {
                alogd("Low probability! preview new frame come before check again.");
                goto PROCESS_MESSAGE;
            }
            else
            {
               mbWaitPreviewFrame = true;
            }
        }
        int nWaitTime = 0;    //unit:ms, 10*1000
        int msgNum = mMsgQueue.waitMessage(nWaitTime);
        if(msgNum <= 0)
        {
            aloge("fatal error! preview thread wait message timeout[%d]ms! msgNum[%d], bWait[%d]", nWaitTime, msgNum, mbWaitPreviewFrame);
        }
        goto PROCESS_MESSAGE;
    }

    mpUvcChn->mpCallbackNotifier->onNextFrameAvailable(pFrmbuf);
    mpUvcChn->mpPreviewWindow->onNextFrameAvailable(pFrmbuf);

    mpUvcChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
    mpUvcChn->mpPreviewQueue->ReleaseElemData(pFrmbuf);
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

UvcChannel::DoPictureThread::DoPictureThread(UvcChannel *pUvcChn)
    : mpUvcChn(pUvcChn)
{
    mbWaitReleaseAllFrames = false;
    mbWaitPictureFrame = false;
    char threadName[64];
    sprintf(threadName, "UvcChn[%d]Picture", (int)mpUvcChn->mChnId);
    status_t ret = run(threadName);
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
}

UvcChannel::DoPictureThread::~DoPictureThread()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t UvcChannel::DoPictureThread::notifyNewFrameCome()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitPictureFrame)
    {
        mbWaitPictureFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = (int)MsgType::InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

status_t UvcChannel::DoPictureThread::notifyPictureEnd()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::SendPictureEnd;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t UvcChannel::DoPictureThread::releaseAllFrames()
{
    AutoMutex lock(mWaitLock);
    mbWaitReleaseAllFrames = true;
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::releaseAllFrames;
    mMsgQueue.queueMessage(&msg);
    while(mbWaitReleaseAllFrames)
    {
        mCondReleaseAllFramesFinished.wait(mWaitLock);
    }
    return NO_ERROR;
}

bool UvcChannel::DoPictureThread::threadLoop()
{
    if(!exitPending())
    {
        return pictureThread();
    } 
    else
    {
        return false;
    }
}

bool UvcChannel::DoPictureThread::pictureThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    bool bDrainPictureQueue = false;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    PROCESS_MESSAGE:
    getMsgRet = mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(MsgType::InputFrameAvailable == (MsgType)msg.mMsgType)
        {
        }
        else if(MsgType::SendPictureEnd == (MsgType)msg.mMsgType)
        {
            bDrainPictureQueue = true;
        }
        else if(MsgType::releaseAllFrames == (MsgType)msg.mMsgType)
        {
            AutoMutex lock(mWaitLock);
            for(;;)
            {
                pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpUvcChn->mpPictureQueue->GetValidElemData();
                if(pFrmbuf)
                {
                    mpUvcChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
                    mpUvcChn->mpPictureQueue->ReleaseElemData(pFrmbuf);
                }
                else
                {
                    break;
                }
            }
            if(mbWaitReleaseAllFrames)
            {
                mbWaitReleaseAllFrames = false;
            }
            else
            {
                aloge("fatal error! check code!");
            }
            mCondReleaseAllFramesFinished.signal();
        }
        else if(MsgType::Exit == (MsgType)msg.mMsgType)
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
        pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpUvcChn->mpPictureQueue->GetValidElemData();
        if (pFrmbuf == NULL) 
        {
            if(bDrainPictureQueue)
            {
                mpUvcChn->mpCallbackNotifier->disableMessage(mpUvcChn->mTakePicMsgType);
                bDrainPictureQueue = false;
                mpUvcChn->mpCallbackNotifier->takePictureEnd();
                mpUvcChn->mLock.lock();
                if(!mpUvcChn->mbTakePictureStart)
                {
                    aloge("fatal error! why takePictureStart is false when we finish take picture?");
                }
                mpUvcChn->mbTakePictureStart = false;
                mpUvcChn->mLock.unlock();
            }
            {
                AutoMutex lock(mWaitLock);
                if(mpUvcChn->mpPictureQueue->GetValidElemNum() > 0)
                {
                    alogd("Low probability! picture new frame come before check again.");
                    goto PROCESS_MESSAGE;
                }
                else
                {
                    mbWaitPictureFrame = true;
                }
            }
            mMsgQueue.waitMessage();
            goto PROCESS_MESSAGE;
        }
#if 0
        static int index = 0;
        char str[50];
        sprintf(str, "./savepicture/4/%d.n21", index++);
        FILE *fp = fopen(str, "wb");
        if(fp)
        {
            VideoFrameBufferSizeInfo FrameSizeInfo;
            getVideoFrameBufferSizeInfo(&pFrmbuf->mFrameBuf, &FrameSizeInfo);
            int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
            for(int i=0; i<3; i++)
            {
                if(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[i] != NULL)
                {
                    fwrite(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[i], 1, yuvSize[i], fp);
                }
            }
            alogd("save a frame!");
            fclose(fp);
        }
#endif
        mpUvcChn->mpCallbackNotifier->takePicture(pFrmbuf, false, true);
        mpUvcChn->mpCallbackNotifier->takePicture(pFrmbuf, false, false);

        mpUvcChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
        mpUvcChn->mpPictureQueue->ReleaseElemData(pFrmbuf);
        if(!bDrainPictureQueue)
        {
            break;
        }
    }
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

UvcChannel::DoCommandThread::DoCommandThread(UvcChannel *pUvcChn)
    : mpUvcChn(pUvcChn)
{
    char threadName[64];
    sprintf(threadName, "UvcChn[%d]Command", (int)mpUvcChn->mChnId);
    status_t ret = run(threadName);
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
}

UvcChannel::DoCommandThread::~DoCommandThread()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

bool UvcChannel::DoCommandThread::threadLoop()
{
    if(!exitPending()) 
    {
        return commandThread();
    } 
    else 
    {
        return false;
    }
}

bool UvcChannel::DoCommandThread::commandThread()
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
            if(MsgType::TakePicture == (MsgType)msg.mMsgType)
            {
                mpUvcChn->doTakePicture(msg.mPara0);
            }
            else if(MsgType::CancelContinuousPicture == (MsgType)msg.mMsgType)
            {
                mpUvcChn->doCancelContinuousPicture();
            }
            else if(MsgType::Exit == (MsgType)msg.mMsgType)
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
            mMsgQueue.waitMessage();
        }
        goto PROCESS_MESSAGE;
    }
_exit0:
    return bRunningFlag;
}

status_t UvcChannel::DoCommandThread::SendCommand_TakePicture(unsigned int msgType)
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::TakePicture;
    msg.mPara0 = msgType;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t UvcChannel::DoCommandThread::SendCommand_CancelContinuousPicture()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::CancelContinuousPicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

UvcChannel::UvcChannel(std::string uvcName, int nCameraId, UvcChn chnId, VdecFrameManager *pManager)
    : mUvcDev(uvcName)
    , mCameraId(nCameraId)
    , mChnId(chnId)
    , mpVdecFrameManager(pManager)
{
    alogv("Construct");
    mVirtualUvcChn = MM_INVALID_CHN;
    mpPreviewQueue = NULL;
    mpPictureQueue = NULL;
    mChannelState = State::IDLE;
    mContinuousPictureCounter = 0;
    mContinuousPictureMax = 0;
    mContinuousPictureStartTime = 0;
    mContinuousPictureLast = 0;
    mContinuousPictureInterval = 0;
    mColorSpace = 0;
    mFrameWidth = 0;
    mFrameHeight = 0;
    mFrameWidthOut = 0;
    mFrameHeightOut = 0;
    mCamDevTimeoutCnt = 0;
    memset(&mRectCrop, 0, sizeof(mRectCrop));

    mLastZoom = -1;
    mNewZoom = 0;
    mMaxZoom = 10;
    mbPreviewEnable = true;

    mpPreviewQueue = new EyeseeQueue(0);
    mpPictureQueue = new EyeseeQueue(0);
    
    mpCallbackNotifier = new CallbackNotifier((int)mChnId, this);
    mpPreviewWindow = new PreviewWindow(this);

    mpCapThread = new DoCaptureThread(this);

    mpPrevThread = new DoPreviewThread(this);

    mpPicThread = new DoPictureThread(this);

    mpCommandThread = new DoCommandThread(this);

    mTakePicMsgType = 0;
    mbTakePictureStart = false;
    
    mFrameCounter = 0;

    mParameters.setPreviewFrameRate(30);
    SIZE_S videoSize{1280, 720};
    mParameters.setVideoSize(videoSize);
    mParameters.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    mParameters.setColorSpace(V4L2_COLORSPACE_JPEG);
    mParameters.setVideoBufferNumber(5);
    //preview rotation setting
    mParameters.setPreviewRotation(0);
    //digital zoom setting
    mParameters.setZoomSupported(true);
    mParameters.setZoom(mNewZoom);
    mParameters.setMaxZoom(mMaxZoom);
}

UvcChannel::~UvcChannel()
{
    alogv("Destruct");
    AutoMutex lock(mLock);
    if(mChannelState != State::IDLE)
    {
        aloge("fatal error! chn[%d] state[0x%x] wrong!", mChnId, mChannelState);
    }
    if (mpCommandThread != NULL) 
    {
        delete mpCommandThread;
        mpCommandThread = NULL;
    }
    if (mpCapThread != NULL)
    {
        delete mpCapThread;
        mpCapThread = NULL;
    }
    if (mpPrevThread != NULL)
    {
        delete mpPrevThread;
        mpPrevThread = NULL;
    }
    if (mpPicThread != NULL)
    {
        delete mpPicThread;
        mpPicThread = NULL;
    }
    if (mpPreviewWindow != NULL) 
    {
        delete mpPreviewWindow;
        mpPreviewWindow = NULL;
    }
    if (mpCallbackNotifier != NULL) 
    {
        delete mpCallbackNotifier;
        mpCallbackNotifier = NULL;
    }
    mFrameBuffers.clear();
    mFrameBufferIdList.clear();
    if (mpPreviewQueue != NULL) 
    {
        delete mpPreviewQueue;
        mpPreviewQueue = NULL;
    }
    if (mpPictureQueue != NULL) 
    {
        delete mpPictureQueue;
        mpPictureQueue = NULL;
    }
}

UvcChn UvcChannel::getChannelId()
{
    return mChnId;
}

status_t UvcChannel::startRecording(CameraRecordingProxyListener *pCb, int recorderId)
{
    return mpCallbackNotifier->startRecording(pCb, recorderId);
}
status_t UvcChannel::stopRecording(int recorderId)
{
    return mpCallbackNotifier->stopRecording(recorderId);
}
void UvcChannel::setDataListener(DataListener *pCb)
{
    mpCallbackNotifier->setDataListener(pCb);
}
void UvcChannel::setNotifyListener(NotifyListener *pCb)
{
    mpCallbackNotifier->setNotifyListener(pCb);
}

status_t UvcChannel::setPreviewDisplay(int hlay)
{
    return mpPreviewWindow->setPreviewWindow(hlay);
}

void UvcChannel::calculateCrop(RECT_S *rect, int zoom, int width, int height)
{
    rect->X = (width - width * 10 / (10 + zoom)) / 2;
    rect->Y = (height - height * 10 / (10 + zoom)) / 2;
    rect->Width = width - rect->X * 2;
    rect->Height = height - rect->Y * 2;
}

status_t UvcChannel::prepare()
{
    AutoMutex lock(mLock);

    if (mChannelState != State::IDLE)
    {
        aloge("prepare in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    alogv("UvcDev[%s], usbCameraId[%d], chn[%d]", mUvcDev.c_str(), mCameraId, mChnId);
    
    SIZE_S videoSizeIn;
    mParameters.getVideoSize(videoSizeIn);
    SIZE_S videoSizeOut;
    mParameters.getVideoSizeOut(videoSizeOut);
    int fps = mParameters.getPreviewFrameRate();
    PIXEL_FORMAT_E pixFmt = mParameters.getPreviewFormat();
    enum v4l2_colorspace csc = mParameters.getColorSpace();
    int nBufNum = mParameters.getVideoBufferNumber();
    int rot = mParameters.getPreviewRotation();
    TAKE_PICTURE_MODE_E picMode = mParameters.getPictureMode();
    bool bZoomSupport = mParameters.isZoomSupported();
    int zoom = mParameters.getZoom();
    int maxZoom = mParameters.getMaxZoom();
    alogd("uvcChn[%d]: size[%dx%d],fps[%d],pixFmt[0x%x],csc[%d], bufNum[%d],rot[%d],takePicMode[%d],zoomSupport[%d],zoomVal[%d,%d]", 
        mChnId, videoSizeOut.Width, videoSizeOut.Height, fps, pixFmt, csc, nBufNum, rot, picMode, bZoomSupport, zoom, maxZoom);

    //preview rotation setting
    mpPreviewWindow->setDisplayFrameRate(mParameters.getDisplayFrameRate());
    mpPreviewWindow->setPreviewRotation(rot);
    mpPreviewWindow->setDispBufferNum(2);
    //digital zoom setting
    mLastZoom = -1;
    if(mParameters.isZoomSupported())
    {
        mNewZoom = zoom;
        mMaxZoom = maxZoom;
    }

    mColorSpace = (int)csc;
    mFrameWidth = videoSizeIn.Width;
    mFrameHeight = videoSizeIn.Height;
    mFrameWidthOut = videoSizeOut.Width;
    mFrameHeightOut = videoSizeOut.Height;

    mChannelState = State::PREPARED;
    return OK;
}

status_t UvcChannel::release()
{
    AutoMutex lock(mLock);

    if (mChannelState != State::PREPARED) 
    {
        aloge("release in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    mChannelState = State::IDLE;
    return NO_ERROR;
}

status_t UvcChannel::startChannel()
{
    AutoMutex lock1(mLock);

    if (mChannelState != State::PREPARED)
    {
        aloge("startChannel in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    //create resources
    int nBufNum = mParameters.getVideoBufferNumber();
    if(!mFrameBuffers.empty())
    {
        aloge("fatal error! why frame buffers is not empty?");
        mFrameBuffers.clear();
    }
    if(!mFrameBufferIdList.empty())
    {
        aloge("fatal error! why bufferIdList is not empty?");
        mFrameBufferIdList.clear();
    }

    int totalNum = mpPreviewQueue->GetTotalElemNum();
    int idleNum = mpPreviewQueue->GetIdleElemNum();
    if(totalNum!=idleNum)
    {
        aloge("fatal error! preview queue has some elems being not idle! [%d], [%d]", idleNum, totalNum);
    }
    if(totalNum < nBufNum)
    {
        mpPreviewQueue->AddIdleElems(nBufNum-totalNum);
    }

    totalNum = mpPictureQueue->GetTotalElemNum();
    idleNum = mpPictureQueue->GetIdleElemNum();
    if(totalNum!=idleNum)
    {
        aloge("fatal error! picture queue has some elems being not idle! [%d], [%d]", idleNum, totalNum);
    }
    if(totalNum < nBufNum)
    {
        mpPictureQueue->AddIdleElems(nBufNum-totalNum);
    }
    
    alogv("startChannel");
    ERRORTYPE eRet;
    if(mChnId == UvcChn::UvcMainChn)
    {
        mVirtualUvcChn = 0;
        while(mVirtualUvcChn < UVC_MAX_CHN_NUM)
        {
            eRet = AW_MPI_UVC_CreateVirChn((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChn);
            if(SUCCESS == eRet)
            {
                break;
            }
            else if(ERR_UVC_EXIST == eRet)
            {
                mVirtualUvcChn++;
            }
            else
            {
                aloge("the %s usbcamera fail to create virchn[%d], 0x%x", mUvcDev.c_str(), mVirtualUvcChn, eRet);
                mVirtualUvcChn++;
            }
        }
        if(UVC_MAX_CHN_NUM == mVirtualUvcChn)
        {
            aloge("the %s usbcamera fail to create virchn.", mUvcDev.c_str());
            return UNKNOWN_ERROR;
        }
        eRet = AW_MPI_UVC_StartRecvPic((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChn);
        if(eRet != SUCCESS)
        {
            aloge("the %s usbcamera can not start uvcchn[%d], 0x%x", mUvcDev.c_str(), mVirtualUvcChn, eRet);
            return UNKNOWN_ERROR;
        }
    }
    else if(mChnId == UvcChn::VdecMainChn || mChnId == UvcChn::VdecSubChn)
    {
        if(NULL == mpVdecFrameManager)
        {
            aloge("fatal error! vdec frame manager is not set!");
            return UNKNOWN_ERROR;
        }
        mpVdecFrameManager->enableUvcChannel(mChnId);
    }
    else
    {
        aloge("fatal error! what chn[%d]?", mChnId);
        return UNKNOWN_ERROR;
    }
        
    if(mbPreviewEnable)
    {
        mpPreviewWindow->startPreview();
    }
    mpCapThread->startCapture();
    mFrameCounter = 0;
    mChannelState = State::STARTED;
    return NO_ERROR;
}

status_t UvcChannel::stopChannel()
{
    AutoMutex lock1(mLock);

    if (mChannelState != State::STARTED)
    {
        aloge("stopChannel in error state %d", mChannelState);
        return INVALID_OPERATION;
    }

    alogv("stopChannel");
    mpPreviewWindow->stopPreview();
    mpCapThread->pauseCapture();
    mpPrevThread->releaseAllFrames();
    mpPicThread->releaseAllFrames();

    ERRORTYPE ret;
    if(mChnId == UvcChn::UvcMainChn)
    {
        ret = AW_MPI_UVC_StopRecvPic((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! the %s usbcamera can not start uvcchn[%d], 0x%x", mUvcDev.c_str(), mVirtualUvcChn, ret);
        }
        ret = AW_MPI_UVC_DestroyVirChn((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChn);
        if(SUCCESS == ret)
        {
            mVirtualUvcChn = MM_INVALID_CHN;
        }
        else
        {
            aloge("fatal error! check code[0x%x]!", ret);
        }
    }
    else if(mChnId == UvcChn::VdecMainChn || mChnId == UvcChn::VdecSubChn)
    {
        if(NULL == mpVdecFrameManager)
        {
            aloge("fatal error! vdec frame manager is not set!");
        }
        mpVdecFrameManager->disableUvcChannel(mChnId);
    }
    else
    {
        aloge("fatal error! what chn[%d]?", mChnId);
    }
    //destroy resources
    mFrameBuffers.clear();
    if(!mFrameBufferIdList.empty())
    {
        aloge("fatal error! why bufferIdList is not empty?");
        mFrameBufferIdList.clear();
    }
    int totalNum = mpPreviewQueue->GetTotalElemNum();
    int idleNum = mpPreviewQueue->GetIdleElemNum();
    if(totalNum!=idleNum)
    {
        aloge("fatal error! preview queue has some elems being not idle! [%d],[%d]", idleNum, totalNum);
    }
    totalNum = mpPictureQueue->GetTotalElemNum();
    idleNum = mpPictureQueue->GetIdleElemNum();
    if(totalNum!=idleNum)
    {
        aloge("fatal error! picture queue has some elems being not idle! [%d],[%d]", idleNum, totalNum);
    }
    mChannelState = State::PREPARED;
    return NO_ERROR;
}

UvcChannel::State UvcChannel::getState()
{
    AutoMutex lock(mLock);
    return mChannelState;
}

bool UvcChannel::isPreviewEnabled()
{
    return mpPreviewWindow->isPreviewEnabled();
}

status_t UvcChannel::startRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    mbPreviewEnable = true;
    if (State::STARTED == mChannelState)
    {
        ret = mpPreviewWindow->startPreview();
    }
    return ret;
}

status_t UvcChannel::stopRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    mbPreviewEnable = false;
    ret = mpPreviewWindow->stopPreview();
    return ret;
}

status_t UvcChannel::releaseFrame(uint32_t index)
{
    VIDEO_FRAME_BUFFER_S *pFrmbuf = NULL;
    AutoMutex bufLock(mFrameBuffersLock);
    if (index >= (uint32_t)mFrameBuffers.size()) 
    {
        alogw("Be careful! maybe buffer index %d is not begin from 0!", index);
        //return BAD_VALUE;
    }
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
                aloge("fatal error! VI frame id[0x%x] is repeated!", index);
            }
        }
    }
    if(NULL == pFrmbuf)
    {
        aloge("fatal error! not find frame index[0x%x]", index);
        return UNKNOWN_ERROR;
    }
    
    int ret;
    AutoMutex lock(mRefCntLock);

    if (pFrmbuf->mRefCnt > 0 && --pFrmbuf->mRefCnt == 0) 
    {
        if(mChnId == UvcChn::UvcMainChn)
        {
            ret = AW_MPI_UVC_ReleaseFrame((UVC_DEV)mUvcDev.c_str(), mVirtualUvcChn, &pFrmbuf->mFrameBuf);
            if(ret != SUCCESS)
            {
                aloge("fatal error! uvc[%s]Chn[%d] release frame fail!", mUvcDev.c_str(), mVirtualUvcChn, ret);
            }
        }
        else if(mChnId == UvcChn::VdecMainChn || mChnId == UvcChn::VdecSubChn)
        {
            if(NULL == mpVdecFrameManager)
            {
                aloge("fatal error! vdec frame manager is not set!");
            }
            status_t eRet = mpVdecFrameManager->releaseFrame(mChnId, &pFrmbuf->mFrameBuf);
            if(eRet != NO_ERROR)
            {
                aloge("fatal error! uvc[%s] VdecChn[%d] release frame fail!", mUvcDev.c_str(), mChnId, eRet);
            }
        }
        else
        {
            aloge("fatal error! what chn[%d]?", mChnId);
        }
        int nCount = 0;
        for (std::list<unsigned int>::iterator it = mFrameBufferIdList.begin(); it!=mFrameBufferIdList.end();)
        {
            if(*it == pFrmbuf->mFrameBuf.mId)
            {
                if(0 == nCount)
                {
                    it = mFrameBufferIdList.erase(it);
                }
                else
                {
                    ++it;
                }
                nCount++;
            }
            else
            {
                ++it;
            }
        }
        if(nCount != 1)
        {
            aloge("fatal error! uvcDev[%s] Chn[%d] frame buffer id list is wrong! dstId[%d], find[%d]", mUvcDev.c_str(), mChnId, pFrmbuf->mFrameBuf.mId, nCount);
            alogd("current frame buffer id list elem number:%d", mFrameBufferIdList.size());
            for (std::list<unsigned int>::iterator it = mFrameBufferIdList.begin(); it!=mFrameBufferIdList.end(); ++it)
            {
                alogd("bufid[%d]", *it);
            }
        }
    }
    return NO_ERROR;
}

status_t UvcChannel::setParameters(CameraParameters &param)
{
    AutoMutex lock(mLock);
    if(State::IDLE == mChannelState)
    {
        mParameters = param;
        return NO_ERROR;
    }
    if (mChannelState != State::PREPARED && mChannelState != State::STARTED)
    {
        alogw("call in wrong channel state[0x%x]", mChannelState);
        return INVALID_OPERATION;
    }

    //detect digital zoom params
    int oldZoom, newZoom;
    oldZoom = mParameters.getZoom();
    newZoom = param.getZoom();
    if(oldZoom != newZoom)
    {
        if(newZoom >=0 && newZoom <= mMaxZoom)
        {
            alogd("paramZoom change[%d]->[%d]", oldZoom, newZoom);
            mNewZoom = newZoom;
        }
        else
        {
            aloge("fatal error! zoom value[%d] is invalid, keep last!", newZoom);
        }
    }

    //set parameters
    mParameters = param;
    mParameters.setZoom(mNewZoom);
    return NO_ERROR;
}

status_t UvcChannel::getParameters(CameraParameters &param) const
{
    param = mParameters;
    return NO_ERROR;
}

void UvcChannel::increaseBufRef(VIDEO_FRAME_BUFFER_S *pBuf)
{
    VIDEO_FRAME_BUFFER_S *pFrame = (VIDEO_FRAME_BUFFER_S*)pBuf;
    AutoMutex lock(mRefCntLock);
    ++pFrame->mRefCnt;
}

void UvcChannel::decreaseBufRef(unsigned int nBufId)
{
    releaseFrame(nBufId);
}

void UvcChannel::NotifyRenderStart()
{
    mpCallbackNotifier->NotifyRenderStart();
}
status_t UvcChannel::doTakePicture(unsigned int msgType)
{
    AutoMutex lock(mLock);

    TakePictureConfig(msgType);

    mTakePictureMode = mParameters.getPictureMode();
    mpCapThread->SendCommand_TakePicture();

    return NO_ERROR;
}

status_t UvcChannel::TakePictureConfig(unsigned int msgType)
{
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
    int quality = mParameters.getJpegThumbnailQuality();
    mpCallbackNotifier->setJpegThumbnailQuality(quality);

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

    return NO_ERROR;
}

status_t UvcChannel::doCancelContinuousPicture()
{
    AutoMutex lock(mLock);
    mpCapThread->SendCommand_CancelContinuousPicture();
    return NO_ERROR;
}

status_t UvcChannel::takePicture(unsigned int msgType, PictureRegionCallback *pPicReg)
{
    AutoMutex lock(mLock);
    if(mbTakePictureStart)
    {
        aloge("fatal error! During taking picture, we don't accept new takePicture command!");
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

status_t UvcChannel::cancelContinuousPicture()
{
    AutoMutex lock(mLock);
    if(!mbTakePictureStart)
    {
        aloge("fatal error! not start take picture!");
        return NO_ERROR;
    }
    mpCommandThread->SendCommand_CancelContinuousPicture();
    return NO_ERROR;
}

}; /* namespace EyeseeLinux */

