/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : Thread.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/01
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

// #define LOG_NDEBUG 0
#define LOG_TAG "Thread"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <sys/resource.h>
#include <utils/Thread.h>
#include <sys/prctl.h>

namespace EyeseeLinux {

Thread::Thread()
    : mThread(-1)
    , mLock("Thread::mLock")
    , mStatus(NO_ERROR)
    , mExitPending(false)
    , mRunning(false)
{
    mName = NULL;
}

Thread::~Thread()
{
    if(mName)
    {
        free(mName);
        mName = NULL;
    }
}

status_t Thread::readyToRun()
{
    return NO_ERROR;
}

status_t Thread::run(const char *name, int priority, size_t stack)
{
    pthread_attr_t attr;

    Mutex::Autolock _l(mLock);

    if (mRunning) {
        // thread already started
        return INVALID_OPERATION;
    }

    // reset status and exitPending to their default value, so we can
    // try again after an error happened (either below, or in readyToRun())
    mStatus = NO_ERROR;
    mExitPending = false;
    mThread = -1;

    mRunning = true;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (priority != 0 || name != NULL) {
        // todo
    }
    if(name != NULL) 
    {
        if(mName)
        {
            free(mName);
            mName = NULL;
        }
        mName = name ? strdup(name) : NULL;
    }
    if (stack > 0) {
        pthread_attr_setstacksize(&attr, stack);
    }

    errno = 0;
    int result = pthread_create(&mThread, &attr, _threadLoop, this);
    pthread_attr_destroy(&attr);
    if (result != 0) {
        aloge("pthread_create error(%s)!", strerror(errno));
        mStatus = UNKNOWN_ERROR;
        mRunning = false;
        mThread = -1;
        return UNKNOWN_ERROR;
    }
    
    // Do not refer to mStatus here: The thread is already running (may, in fact
    // already have exited with a valid mStatus result). The NO_ERROR indication
    // here merely indicates successfully starting the thread and does not
    // imply successful termination/execution.
    return NO_ERROR;
}

void *Thread::_threadLoop(void* user)
{
    Thread* const self = static_cast<Thread*>(user);
    if (self->mName)
    {
        // Mac OS doesn't have this, and we build libutil for the host too
        int hasAt = 0;
        int hasDot = 0;
        char *s = self->mName;
        while (*s) 
        {
            if (*s == '.') hasDot = 1;
            else if (*s == '@') hasAt = 1;
            s++;
        }
        int len = s - self->mName;
        if (len < 15 || hasAt || !hasDot) 
        {
            s = self->mName;
        } 
        else 
        {
            s = self->mName + len - 15;
        }
        prctl(PR_SET_NAME, (unsigned long) s, 0, 0, 0);
    }
    
    bool first = true;

    while(1) {
        bool result;
        if (first) {
            first = false;
            self->mStatus = self->readyToRun();
            result = (self->mStatus == NO_ERROR);

            if (result && !self->exitPending()) {
                // Binder threads (and maybe others) rely on threadLoop
                // running at least once after a successful ::readyToRun()
                // (unless, of course, the thread has already been asked to exit
                // at that point).
                // This is because threads are essentially used like this:
                //   (new ThreadSubclass())->run();
                // The caller therefore does not retain a strong reference to
                // the thread and the thread would simply disappear after the
                // successful ::readyToRun() call instead of entering the
                // threadLoop at least once.
                result = self->threadLoop();
            }
        } else {
            result = self->threadLoop();
        }

        // establish a scope for mLock
        {
            Mutex::Autolock _l(self->mLock);
            if (result == false || self->mExitPending) {
                self->mExitPending = true;
                self->mRunning = false;
                // clear thread ID so that requestExitAndWait() does not exit if
                // called by a new thread using the same thread ID as this one.
                self->mThread = -1;
                // note that interested observers blocked in requestExitAndWait are
                // awoken by broadcast, but blocked on mLock until break exits scope
                self->mThreadExitedCondition.broadcast();
                break;
            }
        }
    }

    return NULL;
}

void Thread::requestExit()
{
    Mutex::Autolock _l(mLock);
    mExitPending = true;
}

status_t Thread::requestExitAndWait()
{
    Mutex::Autolock _l(mLock);
    mExitPending = true;

    while (mRunning == true) {
        mThreadExitedCondition.wait(mLock);
    }
    // This next line is probably not needed any more, but is being left for
    // historical reference. Note that each interested party will clear flag.
    mExitPending = false;

    return mStatus;
}

status_t Thread::join()
{
    Mutex::Autolock _l(mLock);
    if (mThread == pthread_self()) {
        alogw(
        "Thread (this=%p): don't call join() from this "
        "Thread object's thread. It's a guaranteed deadlock!",
        this);

        return WOULD_BLOCK;
    }
    
    while (mRunning == true) {
        mThreadExitedCondition.wait(mLock);
    }

    return mStatus;
}

bool Thread::exitPending() const
{
    Mutex::Autolock _l(mLock);
    return mExitPending;
}

}; /* namespace EyeseeLinux */

