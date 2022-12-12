/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : EyeseeMessageQueue.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2016-09-12
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeMessageQueue"
#include <utils/plat_log.h>

#include <malloc.h>
#include <string.h>
#include <Errors.h>

#include <algorithm>
#include "EyeseeMessageQueue.h"

namespace EyeseeLinux {

status_t EyeseeMessage::setData(char* pData, int nDataSize)
{
    if(mpData)
    {
        aloge("fatal error! need free mpData[%p]!", mpData);
        free(mpData);
        mpData = NULL;
        mDataSize = 0;
    }
    if(NULL==pData || nDataSize <= 0)
    {
        return NO_ERROR;
    }
    mpData = malloc(nDataSize);
    if(NULL == mpData)
    {
        aloge("fatal error! malloc fail!");
    }
    memcpy(mpData, pData, nDataSize);
    mDataSize = nDataSize;
    return NO_ERROR;
}

void* EyeseeMessage::getData(int* pDataSize)
{
    if(pDataSize)
    {
        *pDataSize = mDataSize;
    }
    return mpData;
}

status_t EyeseeMessage::reset()
{
    mWhoseMsg = 0;
	mMsgType = 0;
	mPara0 = 0;
	mPara1 = 0;
    if(mpData!=NULL)
    {
        free(mpData);
        mpData = NULL;
    }
    mDataSize = 0;
    return NO_ERROR;
}

EyeseeMessage& EyeseeMessage::operator= (const EyeseeMessage& rhs)
{
    mWhoseMsg = rhs.mWhoseMsg;
	mMsgType = rhs.mMsgType;
	mPara0 = rhs.mPara0;
	mPara1 = rhs.mPara1;
    if(rhs.mpData!= NULL && rhs.mDataSize > 0)
    {
        mpData = malloc(rhs.mDataSize);
        if(mpData)
        {
            memcpy(mpData, rhs.mpData, rhs.mDataSize);
            mDataSize = rhs.mDataSize;
        }
        else
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            mpData = NULL;
            mDataSize = 0;
        }
    }
    else
    {
        if(rhs.mpData!=NULL || rhs.mDataSize!=0)
        {
            aloge("fatal error! rhs EyeseeMessage wrong[%p][%d]!", rhs.mpData, rhs.mDataSize);
        }
        mpData = NULL;
        mDataSize = 0;
    }

    return *this; 
}

EyeseeMessage& EyeseeMessage::operator=(EyeseeMessage&& rRef)
{
    //swap value
    std::swap(mWhoseMsg, rRef.mWhoseMsg);
    std::swap(mMsgType, rRef.mMsgType);
    std::swap(mPara0, rRef.mPara0);
    std::swap(mPara1, rRef.mPara1);
    std::swap(mpData, rRef.mpData);
    std::swap(mDataSize, rRef.mDataSize);
    return *this;
}

EyeseeMessage::EyeseeMessage()
{
    mWhoseMsg = 0;
	mMsgType = 0;
	mPara0 = 0;
	mPara1 = 0;
    mpData = NULL;
    mDataSize = 0;
}

EyeseeMessage::~EyeseeMessage()
{
    reset();
}

EyeseeMessageQueue::EyeseeMessageQueue()
{
    mWaitMessageFlag = false;
    mCount = 0;
}

status_t EyeseeMessageQueue::queueMessage(EyeseeMessage *pMsgIn)
{
    Mutex::Autolock autoLock(mLock);
    //find idle message
    if(mIdleMessageList.empty())
    {
        mIdleMessageList.resize(1);
        mCount++;
        if(mCount > 8)
        {
            aloge("fatal error! message count[%d] too many", mCount);
        }
    }
    *mIdleMessageList.begin() = *pMsgIn;
    mValidMessageList.splice(mValidMessageList.end(), mIdleMessageList, mIdleMessageList.begin());
    if(mWaitMessageFlag)
    {
        mCondMessageQueueChanged.signal();
    }
    return NO_ERROR;
}

status_t EyeseeMessageQueue::queueMessage(EyeseeMessage&& rRefMsgIn)
{
    Mutex::Autolock autoLock(mLock);
    //find idle message
    if(mIdleMessageList.empty())
    {
        mIdleMessageList.resize(1);
        mCount++;
        if(mCount > 8)
        {
            aloge("fatal error! message count[%d] too many", mCount);
        }
    }
    *mIdleMessageList.begin() = std::move(rRefMsgIn);
    mValidMessageList.splice(mValidMessageList.end(), mIdleMessageList, mIdleMessageList.begin());
    if(mWaitMessageFlag)
    {
        mCondMessageQueueChanged.signal();
    }
    return NO_ERROR;
}

/*******************************************************************************
Function name: android.EyeseeMessageQueue.dequeueMessage
Description: 
    
Parameters: 
    
Return: 
    return value is NOT_ENOUGH_DATA, UNKNOWN_ERROR, NO_ERROR
Time: 2015/7/13
*******************************************************************************/
status_t EyeseeMessageQueue::dequeueMessage(EyeseeMessage *pMsgOut)
{
    Mutex::Autolock autoLock(mLock);
    if(mValidMessageList.empty())
    {
        return NOT_ENOUGH_DATA;
    }
    //pMsgOut->reset();
    //*pMsgOut = *mValidMessageList.begin();
    //mValidMessageList.begin()->reset();
    *pMsgOut = std::move(*mValidMessageList.begin());
    mIdleMessageList.splice(mIdleMessageList.end(), mValidMessageList, mValidMessageList.begin());
    return NO_ERROR;
}

status_t EyeseeMessageQueue::flushMessage()
{
    Mutex::Autolock autoLock(mLock);
    /*
    for(std::list<EyeseeMessage>::iterator it = mValidMessageList.begin(); it != mValidMessageList.end(); ++it)
    {
        it->reset();
    }
    */
    /*
    for(auto& i : mValidMessageList)
    {
        i.reset();
    }
    */
    std::for_each( std::begin(mValidMessageList), std::end(mValidMessageList), [](EyeseeMessage& i) {
        i.reset();
    } );
    
    mIdleMessageList.splice(mIdleMessageList.end(), mValidMessageList);
    return NO_ERROR;
}
/*******************************************************************************
Function name: android.EyeseeMessageQueue.waitMessage
Description: 
    
Parameters: 
    timeout: unit:ms
Return: 
    return value is the message number.
Time: 2015/7/13
*******************************************************************************/
int EyeseeMessageQueue::waitMessage(unsigned int timeout)
{
    Mutex::Autolock autoLock(mLock);
    if(!mValidMessageList.empty())
    {
        return mValidMessageList.size();
    }
    mWaitMessageFlag = true;
    if(timeout<=0)
    {
        while(mValidMessageList.empty())
        {
            mCondMessageQueueChanged.waitRelative(mLock, (int64_t)500*1000000);
        }
    }
    else
    {
        if(mValidMessageList.empty())
        {
            status_t ret = mCondMessageQueueChanged.waitRelative(mLock, (int64_t)timeout*1000000);
            if(TIMED_OUT == ret)
            {
                //alogd("pthread cond timeout np timeout[%d]", ret);
            }
            else if(NO_ERROR == ret)
            {
            }
            else
            {
                aloge("fatal error! waitRelative fail[%d][%s]", ret, strerror(errno));
            }
        }
    }
    mWaitMessageFlag = false;
    return mValidMessageList.size();
}

int EyeseeMessageQueue::getMessageCount()
{
    Mutex::Autolock autoLock(mLock);
    return (int)mValidMessageList.size();
}

};

