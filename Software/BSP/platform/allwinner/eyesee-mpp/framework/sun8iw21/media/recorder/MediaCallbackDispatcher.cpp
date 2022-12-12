//#define LOG_NDEBUG 0
#define LOG_TAG "MediaCallbackDispatcher"
#include <utils/plat_log.h>

#include <string.h>
#include <errno.h>
#include "MediaCallbackDispatcher.h"

namespace EyeseeLinux {

void* MediaCallbackDispatcher::DispatcherThread(void* user)
{
    MediaCallbackDispatcher *pDispatcher = (MediaCallbackDispatcher*)user;
    pDispatcher->loop();
    return (void*)NO_ERROR;
}

MediaCallbackDispatcher::MediaCallbackDispatcher()
    : mDone(false) 
{
    errno = 0;
    int result = pthread_create(&mThreadId, NULL, DispatcherThread, this);
    if (result != 0) 
    {
        aloge("fatal error! pthread_create error(%s)!", strerror(errno));
    }
}

MediaCallbackDispatcher::~MediaCallbackDispatcher() 
{
    {
        Mutex::Autolock autoLock(mLock);

        mDone = true;
        mQueueChanged.signal();
    }
    void *status;
    pthread_join(mThreadId, &status);
}

void MediaCallbackDispatcher::post(const MediaCallbackMessage &msg) 
{
    Mutex::Autolock autoLock(mLock);

    mQueue.push_back(msg);
    if(mQueue.size() > (unsigned int)mMsgNumThreshold)
    {
        aloge("fatal error! message number[%d] exceed threshold[%d]!", mQueue.size(), mMsgNumThreshold);
    }
    mQueueChanged.signal();
}

void MediaCallbackDispatcher::dispatch(const MediaCallbackMessage &msg) 
{
    handleMessage(msg);
}

bool MediaCallbackDispatcher::loop() 
{
    for (;;) 
    {
        MediaCallbackMessage msg;

        {
            Mutex::Autolock autoLock(mLock);
            while (!mDone && mQueue.empty()) 
            {
                mQueueChanged.wait(mLock);
            }

            if (mDone) 
            {
                break;
            }

            msg = *mQueue.begin();
            mQueue.erase(mQueue.begin());
        }

        dispatch(msg);
    }

    return false;
}

};

