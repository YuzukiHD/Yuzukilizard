/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CallbackDispatcher.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/02
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "CallbackDispatcher"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <CallbackDispatcher.h>


namespace EyeseeLinux {

bool CallbackDispatcher::CallbackDispatcherThread::threadLoop()
{
    return mpDispatcher->loop();
}

CallbackDispatcher::CallbackDispatcher()
    : mbDone(false)
{
    mpDispatcherThread = new CallbackDispatcherThread(this);
    mpDispatcherThread->run("CallbackDispatcherThread");
}

CallbackDispatcher::~CallbackDispatcher()
{
    {
        Mutex::Autolock autoLock(mLock);
        mbDone = true;
        mQueueChanged.signal();
    }
    // A join on self can happen if the last ref to CallbackDispatcher
    // is released within the CallbackDispatcherThread loop
    status_t status = mpDispatcherThread->join();
    if (status != WOULD_BLOCK) {
        // Other than join to self, the only other error return codes are
        // whatever readyToRun() returns, and we don't override that
        if(status != (status_t)NO_ERROR)
        {
            aloge("status[0x%x]!=NO_ERROR", status);
        }
    }
    delete mpDispatcherThread;
}

void CallbackDispatcher::post(const CallbackMessage &msg)
{
    Mutex::Autolock autoLock(mLock);
    mQueue.push_back(msg);
    mQueueChanged.signal();
}

void CallbackDispatcher::dispatch(const CallbackMessage &msg)
{
    handleMessage(msg);
}

bool CallbackDispatcher::loop()
{
    for(;;)
    {
        CallbackMessage msg;
        {
            Mutex::Autolock autoLock(mLock);
            while (!mbDone && mQueue.empty())
            {
                mQueueChanged.wait(mLock);
            }
            if (mbDone) 
            {
                break;
            }
            msg = mQueue.front();
            mQueue.pop_front();
        }
        dispatch(msg);
    }
    return false;
}

}; /* namespace EyeseeLinux */

