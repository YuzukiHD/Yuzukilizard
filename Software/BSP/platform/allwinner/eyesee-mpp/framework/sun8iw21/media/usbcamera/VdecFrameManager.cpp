/*
********************************************************************************
*                           eyesee framework module
*
*          (c) Copyright 2010-2018, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : VdecFrameManager.cpp
* Version: V1.0
* By     : 
* Date   : 2018-12-13
* Description:
********************************************************************************
*/
#include <plat_log.h>

#include <mpi_uvc.h>
#include <uvcInput.h>
#include <mpi_vdec.h>
#include <mpi_sys.h>

#include <cmath>
#include <cstring>
#include "VdecFrameManager.h"

namespace EyeseeLinux {

VdecFrameManager::DoubleFrame::DoubleFrame()
{
    memset(&mOutMainFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    memset(&mOutSubFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    mbMainValid = false;
    mbSubValid = false;
    mbMainRequestFlag = false;
    mbMainReturnFlag = false;
    mbSubRequestFlag = false;
    mbSubReturnFlag = false;
}            
VdecFrameManager::DoubleFrame::DoubleFrame(const DoubleFrame &lhs)
{
    mOutMainFrame = lhs.mOutMainFrame;
    mOutSubFrame = lhs.mOutSubFrame;
    mbMainValid = lhs.mbMainValid;
    mbSubValid = lhs.mbSubValid;
    mbMainRequestFlag = lhs.mbMainRequestFlag;
    mbMainReturnFlag = lhs.mbMainReturnFlag;
    mbSubRequestFlag = lhs.mbSubRequestFlag;
    mbSubReturnFlag = lhs.mbSubReturnFlag;
}

VdecFrameManager::DoRequestVdecFrameThread::DoRequestVdecFrameThread(VdecFrameManager *p)
    :mpVdecFrameManager(p)
{
    mThreadState = State::IDLE;
    status_t ret = run("VFMReqVDecFrame");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
}
VdecFrameManager::DoRequestVdecFrameThread::~DoRequestVdecFrameThread()
{
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}
status_t VdecFrameManager::DoRequestVdecFrameThread::startRequestVdecFrame()
{
    AutoMutex lock(mThreadStateLock);
    if(State::STARTED == mThreadState)
    {
        alogd("already in started");
        return NO_ERROR;
    }
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::SetState;
    msg.mPara0 = (int)State::STARTED;
    mMsgQueue.queueMessage(&msg);
    while(State::STARTED != mThreadState)
    {
        mThreadCondStateComplete.wait(mThreadStateLock);
    }
    return NO_ERROR;
}
status_t VdecFrameManager::DoRequestVdecFrameThread::stopRequestVdecFrame()
{
    AutoMutex lock(mThreadStateLock);
    if(State::IDLE == mThreadState)
    {
        alogd("already in idle");
        return NO_ERROR;
    }
    EyeseeMessage msg;
    msg.mMsgType = (int)MsgType::SetState;
    msg.mPara0 = (int)State::IDLE;
    mMsgQueue.queueMessage(&msg);
    while(State::IDLE != mThreadState)
    {
        mThreadCondStateComplete.wait(mThreadStateLock);
    }
    return NO_ERROR;
}
bool VdecFrameManager::DoRequestVdecFrameThread::threadLoop()
{
    if(!exitPending())
    {
        return RequestVdecFrameThread();
    } 
    else
    {
        return false;
    }
}

bool VdecFrameManager::DoRequestVdecFrameThread::RequestVdecFrameThread()
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
            if (MsgType::SetState == (MsgType)msg.mMsgType) 
            {
                AutoMutex stateLock(mThreadStateLock);
                if (mThreadState == (State)msg.mPara0)
                {
                    alogw("Be careful! state is same[0x%x], check code!", mThreadState);
                    mThreadCondStateComplete.signal();
                }
                else 
                {
                    switch ((State)msg.mPara0) 
                    {
                        case State::IDLE: 
                        {
                            if (State::STARTED == mThreadState || State::PAUSE == mThreadState)
                            {
                                mThreadState = State::IDLE;
                                mThreadCondStateComplete.signal();
                            }
                            else
                            {
                                aloge("fatal error! wrong current state[0x%x]", mThreadState);
                            }
                            break;
                        }
                        case State::STARTED:
                        {
                            if (State::IDLE == mThreadState || State::PAUSE == mThreadState)
                            {
                                mThreadState = State::STARTED;
                                mThreadCondStateComplete.signal();
                            }
                            else
                            {
                                aloge("fatal error! wrong current state[0x%x]", mThreadState);
                            }
                            break;
                        }
                        default:
                        {
                            alogw("Be careful! don't support other state[0x%x] transition", msg.mPara0);
                            break;
                        }
                    }
                }
            } 
            else if(MsgType::Exit == (MsgType)msg.mMsgType)
            {
                // Kill thread
                bRunningFlag = false;
                goto _exit0;
            }
            goto PROCESS_MESSAGE;
        }

        if(State::STARTED == mThreadState)
        {
            VIDEO_FRAME_INFO_S stMainFrame;
            VIDEO_FRAME_INFO_S stSubFrame;
            ret = AW_MPI_VDEC_GetDoubleImage(mpVdecFrameManager->mVdecChn, &stMainFrame, &stSubFrame, 1000);
            if(SUCCESS == ret)
            {
                AutoMutex lock(mpVdecFrameManager->mListLock);
                if(mpVdecFrameManager->mIdleList.empty())
                {
                    alogd("Be careful! idle list is empty? malloc more");
                    mpVdecFrameManager->mIdleList.emplace_back();
                }
                auto firstIt = mpVdecFrameManager->mIdleList.begin();
                firstIt->mOutMainFrame = stMainFrame;
                firstIt->mbMainValid = true;
                if(mpVdecFrameManager->mbVdecSubOutputEnable)
                {
                    firstIt->mOutSubFrame = stSubFrame;
                    firstIt->mbSubValid = true;
                }
                else
                {
                    firstIt->mbSubValid = false;
                }
                firstIt->mbMainRequestFlag = false;
                firstIt->mbMainReturnFlag = false;
                firstIt->mbSubRequestFlag = false;
                firstIt->mbSubReturnFlag = false;
                mpVdecFrameManager->mReadyList.splice(mpVdecFrameManager->mReadyList.end(), mpVdecFrameManager->mIdleList, firstIt);
                if(mpVdecFrameManager->mWaitReadyFrameFlag)
                {
                    mpVdecFrameManager->mWaitReadyFrameFlag = false;
                    mpVdecFrameManager->mCondReadyFrame.signal();
                }
            }
            else if(ERR_VDEC_NOBUF == ret)
            {
                alogd("Be careful! get no frame?");
            }
            else
            {
                alogw("Be careful! get frame from vdec fail[0x%x]", ret);
            }
        }
        else
        {
            mMsgQueue.waitMessage();
        }
    }
_exit0:
    return bRunningFlag;
}


VdecFrameManager::VdecFrameManager(VDEC_CHN nVdecChn, bool bVdecSubOutputEnable)
    :mVdecChn(nVdecChn)
    ,mbVdecSubOutputEnable(bVdecSubOutputEnable)
{
    mState = State::IDLE;
    mbMainEnable = false;
    mbSubEnable = false;
    mIdleList.resize(mInitBufNums);
    mWaitReadyFrameFlag = false;

    mpRequestThread = new DoRequestVdecFrameThread(this);
    if(nullptr == mpRequestThread)
    {
        aloge("fatal error! malloc fail!");
    }
}
    
VdecFrameManager::~VdecFrameManager()
{
    AutoMutex stateLock(mStateLock);
    if(mState != State::IDLE)
    {
        aloge("fatal error! why state[%d] != IDLE?", mState);
    }
    if(mpRequestThread != nullptr)
    {
        delete mpRequestThread;
        mpRequestThread = nullptr;
    }

    AutoMutex listLock(mListLock);
    if(mReadyList.size() != 0)
    {
        aloge("fatal error! why readylist has [%d] elems?", mReadyList.size());
    }
    
}

status_t VdecFrameManager::start()
{
    status_t ret = NO_ERROR;
    AutoMutex stateLock(mStateLock);
    if(State::STARTED == mState)
    {
        alogd("already started!");
        return NO_ERROR;
    }
    if(State::IDLE == mState)
    {
        ret = mpRequestThread->startRequestVdecFrame();
        if(NO_ERROR == ret)
        {
            mState = State::STARTED;
        }
        else
        {
            aloge("fatal error! why reqeust thread can't start[0x%x]?", ret);
        }
        
    }
    return ret;
}
status_t VdecFrameManager::stop()
{
    AutoMutex stateLock(mStateLock);
    return stop_l();
}
status_t VdecFrameManager::stop_l()
{
    status_t ret = NO_ERROR;
    if(State::IDLE == mState)
    {
        alogd("already idle!");
        return NO_ERROR;
    }
    if(State::STARTED == mState)
    {
        {
            AutoMutex listLock(mListLock);
            if(false != mbMainEnable || false != mbSubEnable)
            {
                aloge("fatal error! vdec chn is not close, can't stop!");
                return PERMISSION_DENIED;
            }
        }
        ret = mpRequestThread->stopRequestVdecFrame();
        if(NO_ERROR == ret)
        {
            status_t relRet = releaseAllFrames_l();
            if(relRet != NO_ERROR)
            {
                aloge("fatal error! check code!");
            }
            mState = State::IDLE;
        }
        else
        {
            aloge("fatal error! why reqeust thread can't stop[0x%x]?", ret);
        }
    }
    return ret;
}

status_t VdecFrameManager::enableUvcChannel(UvcChn chn)
{
    status_t ret = NO_ERROR;
    AutoMutex listLock(mListLock);
    if(chn == UvcChn::VdecMainChn)
    {
        mbMainEnable = true;
    }
    else if(chn == UvcChn::VdecSubChn)
    {
        if(mbVdecSubOutputEnable)
        {
            mbSubEnable = true;
        }
        else
        {
            aloge("fatal error! vdeclib sub output is disable! can't enable uvcChannel[%d]", chn);
            mbSubEnable = false;
            ret = INVALID_OPERATION;
        }
    }
    else
    {
        aloge("fatal error! wrong chn[%d] for vdecFrameManager!", chn);
        ret = BAD_VALUE;
    }
    return ret;
}

status_t VdecFrameManager::disableUvcChannel(UvcChn chn)
{
    status_t ret = NO_ERROR;
    AutoMutex listLock(mListLock);
    if(chn == UvcChn::VdecMainChn)
    {
        mbMainEnable = false;
    }
    else if(chn == UvcChn::VdecSubChn)
    {
        mbSubEnable = false;
    }
    else
    {
        aloge("fatal error! wrong chn[%d] for vdecFrameManager!", chn);
        ret = BAD_VALUE;
    }
    return ret;
}

/*
 * @timeout -1: wait forever, 0:return immediately, >0:unit:ms.
 * @return BAD_VALUE, PERMISSION_DENIED, NO_ERROR
 */
status_t VdecFrameManager::getFrame(UvcChn chn, VIDEO_FRAME_INFO_S *pFrameInfo, int timeout) //unit:ms
{
    status_t eRet = NO_ERROR;
    if(chn != UvcChn::VdecMainChn && chn != UvcChn::VdecSubChn)
    {
        aloge("fatal error! wrong chn[%d], must vdec chn!", chn);
        return BAD_VALUE;
    }
    if(NULL == pFrameInfo)
    {
        aloge("fatal error! null pointer!");
        return BAD_VALUE;
    }
    {
        AutoMutex stateLock(mStateLock);
        if(mState != State::STARTED)
        {
            alogw("Be careful! vdec frame manager is not in state::started!");
            return PERMISSION_DENIED;
        }
    }
    AutoMutex listLock(mListLock);
    if(chn == UvcChn::VdecMainChn)
    {
        if(mbMainEnable != true)
        {
            aloge("fatal error! uvcChannel[%d] is disable! why request sub frame?", chn);
            return BAD_VALUE;
        }
    }
    if(chn == UvcChn::VdecSubChn)
    {
        if(mbVdecSubOutputEnable != true)
        {
            aloge("fatal error! vdeclib sub output is disable! why request sub frame?");
            return BAD_VALUE;
        }
        if(mbSubEnable != true)
        {
            aloge("fatal error! uvcChannel[%d] is disable! why request sub frame?", chn);
            return BAD_VALUE;
        }
    }
TryToGetFrame:
    bool bFind = false;
    if(!mReadyList.empty())
    {
        for(DoubleFrame& i : mReadyList)
        {
            if(chn == UvcChn::VdecMainChn)
            {
                if(i.mbMainValid)
                {
                    if(true == i.mbMainRequestFlag)
                    {
                        continue;
                    }
                    else
                    {
                        if(i.mbMainReturnFlag != false)
                        {
                            aloge("fatal error! mainFrame req/ret flag[%d][%d]", i.mbMainRequestFlag, i.mbMainReturnFlag);
                        }
                        //set this node main frame to pFrameInfo
                        i.mbMainRequestFlag = true;
                        *pFrameInfo = i.mOutMainFrame;
                        bFind = true;
                        break;
                    }
                }
                else
                {
                    aloge("fatal error! mainFrame is not valid?");
                }
            }
            else if(chn == UvcChn::VdecSubChn)
            {
                if(i.mbSubValid)
                {
                    if(true == i.mbSubRequestFlag)
                    {
                        continue;
                    }
                    else
                    {
                        if(i.mbSubReturnFlag != false)
                        {
                            aloge("fatal error! subFrame req/ret flag[%d][%d]", i.mbSubRequestFlag, i.mbSubReturnFlag);
                        }
                        //set this node sub frame to pFrameInfo
                        i.mbSubRequestFlag = true;
                        *pFrameInfo = i.mOutSubFrame;
                        bFind = true;
                        break;
                    }
                }
                else
                {
                    aloge("fatal error! subFrame is not valid?");
                }
            }
            else
            {
                aloge("fatal error! check code!");
            }
        }
    }
    if(bFind!=true)
    {
        if(timeout < 0)
        {
            mWaitReadyFrameFlag = true;
            mCondReadyFrame.wait(mListLock);
            goto TryToGetFrame;
        }
        else if(0 == timeout)
        {
            eRet = NOT_ENOUGH_DATA;
        }
        else if(timeout > 0)
        {
            mWaitReadyFrameFlag = true;
            int64_t reltime = timeout*1000000;
            status_t ret = mCondReadyFrame.waitRelative(mListLock, reltime);
            if (TIMED_OUT == ret)
            {
                alogv("wait ready frame timeout[%d]ms, ret[%d]", timeout, ret);
                eRet = NOT_ENOUGH_DATA;
                mWaitReadyFrameFlag = false;
            } 
            else if (NO_ERROR == ret) 
            {
                if(mWaitReadyFrameFlag != false)
                {
                    aloge("fatal error! why ready frame flag[%d] != false?", mWaitReadyFrameFlag);
                    mWaitReadyFrameFlag = false;
                }
                goto TryToGetFrame;
            } 
            else 
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                eRet = NOT_ENOUGH_DATA;
                mWaitReadyFrameFlag = false;
            }
        }
    }
    else
    {
        eRet = NO_ERROR;
    }
    return eRet;
}

status_t VdecFrameManager::releaseFrame(UvcChn chn, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    status_t eRet = NO_ERROR;
    if(chn != UvcChn::VdecMainChn && chn != UvcChn::VdecSubChn)
    {
        aloge("fatal error! wrong chn[%d], must vdec chn!", chn);
        return BAD_VALUE;
    }
    if(NULL == pFrameInfo)
    {
        aloge("fatal error! null pointer!");
        return BAD_VALUE;
    }
    AutoMutex listLock(mListLock);
    if(chn == UvcChn::VdecMainChn)
    {
        if(mbMainEnable != true)
        {
            aloge("fatal error! uvcChannel[%d] is disable! why request sub frame?", chn);
            return BAD_VALUE;
        }
    }
    if(chn == UvcChn::VdecSubChn)
    {
        if(mbVdecSubOutputEnable != true)
        {
            aloge("fatal error! vdeclib sub output is disable! why request sub frame?");
            return BAD_VALUE;
        }
        if(mbSubEnable != true)
        {
            aloge("fatal error! uvcChannel[%d] is disable! why request sub frame?", chn);
            return BAD_VALUE;
        }
    }
    
    if(mReadyList.empty())
    {
        aloge("fatal error! ready list is empty, why release frame?");
        return BAD_VALUE;
    }
    int nFindNum = 0;
    for(std::list<DoubleFrame>::iterator it = mReadyList.begin(); it != mReadyList.end();)
    {
        bool bItIncrease = false;
        if(chn == UvcChn::VdecMainChn)
        {
            if(it->mbMainValid != true)
            {
                aloge("fatal error! mainFrame is not valid?");
            }
            if(pFrameInfo->mId == it->mOutMainFrame.mId)
            {
                if(0 == nFindNum)
                {
                    if(true == it->mbMainRequestFlag && false == it->mbMainReturnFlag)
                    {
                        it->mbMainReturnFlag = true;
                        //decide if we can release frame.
                        bool bCanReturn = false;
                        if(true == it->mbSubValid)
                        {
                            if(true == mbSubEnable)
                            {
                                if(true==it->mbSubRequestFlag && true==it->mbSubReturnFlag)
                                {
                                    bCanReturn = true;
                                }
                            }
                            else
                            {
                                if((true==it->mbSubRequestFlag && true==it->mbSubReturnFlag)
                                    || (false==it->mbSubRequestFlag && false==it->mbSubReturnFlag))
                                {
                                    bCanReturn = true;
                                }
                                else
                                {
                                    aloge("fatal error! sub frame channel is disable, why sub req[%d]/ret[%d] is not match?", it->mbSubRequestFlag, it->mbSubReturnFlag);
                                }
                            }
                        }
                        else
                        {
                            bCanReturn = true;
                        }
                        if(bCanReturn)
                        {
                            int ret = AW_MPI_VDEC_ReleaseDoubleImage(mVdecChn, &it->mOutMainFrame, &it->mOutSubFrame);
                            if(SUCCESS == ret)
                            {
                                auto tmp = it;
                                ++it;
                                mIdleList.splice(mIdleList.end(), mReadyList, tmp);
                                bItIncrease = true;
                            }
                            else
                            {
                                aloge("fatal error! release frame to vdec fail[0x%x], check code!", ret);
                            }
                        }
                    }
                    else
                    {
                        aloge("fatal error! why value is not respected, frameId[%d], mainReq[%d]/Ret[%d]", it->mbMainRequestFlag, it->mbMainReturnFlag);
                    }
                }
                else
                {
                    aloge("fatal error! already exist frameId[%d], num[%d]?", pFrameInfo->mId, nFindNum);
                }
                nFindNum++;
            }
        }
        else if(chn == UvcChn::VdecSubChn)
        {
            if(it->mbSubValid != true)
            {
                aloge("fatal error! subFrame is not valid?");
            }
            if(pFrameInfo->mId == it->mOutSubFrame.mId)
            {
                if(0 == nFindNum)
                {
                    if(true == it->mbSubRequestFlag && false == it->mbSubReturnFlag)
                    {
                        it->mbSubReturnFlag = true;
                        //decide if we can release frame.
                        bool bCanReturn = false;
                        if(true == it->mbMainValid)
                        {
                            if(true == mbMainEnable)
                            {
                                if(true==it->mbMainRequestFlag && true==it->mbMainReturnFlag)
                                {
                                    bCanReturn = true;
                                }
                            }
                            else
                            {
                                if((true==it->mbMainRequestFlag && true==it->mbMainReturnFlag)
                                    || (false==it->mbMainRequestFlag && false==it->mbMainReturnFlag))
                                {
                                    bCanReturn = true;
                                }
                                else
                                {
                                    aloge("fatal error! main frame channel is disable, why main req[%d]/ret[%d] is not match?", it->mbMainRequestFlag, it->mbMainReturnFlag);
                                }
                            }
                        }
                        else
                        {
                            bCanReturn = true;
                        }
                        if(bCanReturn)
                        {
                            int ret = AW_MPI_VDEC_ReleaseDoubleImage(mVdecChn, &it->mOutMainFrame, &it->mOutSubFrame);
                            if(SUCCESS == ret)
                            {
                                auto tmp = it;
                                ++it;
                                mIdleList.splice(mIdleList.end(), mReadyList, tmp);
                                bItIncrease = true;
                            }
                            else
                            {
                                aloge("fatal error! release frame to vdec fail[0x%x], check code!", ret);
                            }
                        }
                    }
                    else
                    {
                        aloge("fatal error! why value is not respected, frameId[%d], mainReq[%d]/Ret[%d]", it->mbSubRequestFlag, it->mbSubReturnFlag);
                    }
                }
                else
                {
                    aloge("fatal error! already exist frameId[%d], num[%d]?", pFrameInfo->mId, nFindNum);
                }
                nFindNum++;
            }
        }
        else
        {
            aloge("fatal error! wrong chn[%d], must vdec chn!", chn);
        }
        if(!bItIncrease)
        {
            ++it;
        }
    }
    if(0 == nFindNum)
    {
        aloge("fatal error! uvcChn[%d] not find frameId[%d]", chn, pFrameInfo->mId);
    }
    return eRet;
}
/**
 * when mpi_vdec send callback message of no frame buffer, eyeseeUsbCamera will call this fuction to release frame to vdec
 * to let vdec thread decode contiguously.
 */
status_t VdecFrameManager::VdecNotifyNoFrameBuffer()
{
    status_t eError = NO_ERROR;
    AutoMutex stateLock(mStateLock);
    if(mState != State::STARTED)
    {
        alogd("ignore vdec no frame buffer message.");
        return NO_ERROR;
    }
    AutoMutex listLock(mListLock);
    if(mReadyList.empty())
    {
        alogw("Be careful! ready list is empty, can't release frame!");
        return NO_ERROR;
    }
    bool bRelease = false;
    for(std::list<DoubleFrame>::iterator it = mReadyList.begin(); it != mReadyList.end();)
    {
        if(false == it->mbMainValid && false == it->mbSubValid)
        {
            aloge("fatal error! check code!");
        }
        int bMainPermit = false;
        int bSubPermit = false;
        if(it->mbMainValid)
        {
            if(true == mbMainEnable)
            {
                if(true==it->mbMainRequestFlag)
                {
                    if(true==it->mbMainReturnFlag)
                    {
                        bMainPermit = true;
                    }
                }
                else
                {
                    if(false!=it->mbMainReturnFlag)
                    {
                        aloge("fatal error! check code!");
                    }
                    bMainPermit = true;
                }
            }
            else
            {
                if((true==it->mbMainRequestFlag && true==it->mbMainReturnFlag)
                    || (false==it->mbMainRequestFlag && false==it->mbMainReturnFlag))
                {
                    bMainPermit = true;
                }
                else
                {
                    aloge("fatal error! check code![%d][%d]", it->mbMainRequestFlag, it->mbMainReturnFlag);
                    bMainPermit = true;
                }
            }
        }
        else
        {
            bMainPermit = true;
        }

        if(it->mbSubValid)
        {
            if(true == mbSubEnable)
            {
                if(true==it->mbSubRequestFlag)
                {
                    if(true==it->mbSubReturnFlag)
                    {
                        bSubPermit = true;
                    }
                }
                else
                {
                    if(false!=it->mbSubReturnFlag)
                    {
                        aloge("fatal error! check code!");
                    }
                    bSubPermit = true;
                }
            }
            else
            {
                if((true==it->mbSubRequestFlag && true==it->mbSubReturnFlag)
                    || (false==it->mbSubRequestFlag && false==it->mbSubReturnFlag))
                {
                    bSubPermit = true;
                }
                else
                {
                    aloge("fatal error! check code![%d][%d]", it->mbSubRequestFlag, it->mbSubReturnFlag);
                    bSubPermit = true;
                }
            }
        }
        else
        {
            bSubPermit = true;
        }

        if(bMainPermit && bSubPermit)
        {
            int ret = AW_MPI_VDEC_ReleaseDoubleImage(mVdecChn, &it->mOutMainFrame, &it->mOutSubFrame);
            if(SUCCESS == ret)
            {
                auto tmp = it;
                ++it;
                mIdleList.splice(mIdleList.end(), mReadyList, tmp);
                bRelease = true;
                break;
            }
            else
            {
                aloge("fatal error! release frame to vdec fail[0x%x], check code!", ret);
            }
        }
        ++it;
    }
    if(bRelease)
    {
        eError = NO_ERROR;
    }
    else
    {
        alogd("Be careful! [%d]ready frames can't return.", mReadyList.size());
        eError = NOT_ENOUGH_DATA;
    }
    return eError;
}

status_t VdecFrameManager::releaseAllFrames_l()
{
    status_t ret = NO_ERROR;
    AutoMutex listLock(mListLock);
    for(std::list<DoubleFrame>::iterator it = mReadyList.begin(); it != mReadyList.end();)
    {
        bool bIncrease = false;
        if(false == it->mbMainValid && false == it->mbSubValid)
        {
            aloge("fatal error! check code!");
        }
        int bMainPermit = false;
        int bSubPermit = false;
        if(it->mbMainValid)
        {
            if((true==it->mbMainRequestFlag && true==it->mbMainReturnFlag)
                || (false==it->mbMainRequestFlag && false==it->mbMainReturnFlag))
            {
                bMainPermit = true;
            }
            else
            {
                aloge("fatal error! check code![%d][%d]", it->mbMainRequestFlag, it->mbMainReturnFlag);
            }
        }
        else
        {
            bMainPermit = true;
        }

        if(it->mbSubValid)
        {
            if((true==it->mbSubRequestFlag && true==it->mbSubReturnFlag)
                || (false==it->mbSubRequestFlag && false==it->mbSubReturnFlag))
            {
                bSubPermit = true;
            }
            else
            {
                aloge("fatal error! check code![%d][%d]", it->mbSubRequestFlag, it->mbSubReturnFlag);
            }
        }
        else
        {
            bSubPermit = true;
        }

        if(bMainPermit && bSubPermit)
        {
            int ret = AW_MPI_VDEC_ReleaseDoubleImage(mVdecChn, &it->mOutMainFrame, &it->mOutSubFrame);
            if(SUCCESS == ret)
            {
                auto tmp = it;
                ++it;
                mIdleList.splice(mIdleList.end(), mReadyList, tmp);
                bIncrease = true;
            }
            else
            {
                aloge("fatal error! release frame to vdec fail[0x%x], check code!", ret);
            }
        }
        else
        {
            aloge("fatal error! check code!");
        }
        if(false == bIncrease)
        {
            ++it;
        }
    }
    if(mReadyList.empty())
    {
        ret = NO_ERROR;
    }
    else
    {
        aloge("fatal error! ready list still has nodes!");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

}

