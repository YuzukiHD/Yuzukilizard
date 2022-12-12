/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeRecorder.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/07
  Last Modified :
  Description   : recorder use mpp modules to implement video recording.
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "VideoEnc_Component"
#include <utils/plat_log.h>

#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include <vector>
#include "CameraFrameManager.h"

namespace EyeseeLinux {

CameraFrameManager::CameraFrameManager(CameraRecordingProxy *pCameraProxy          , VI_DEV nChannel)
{
    auto ret = mCameraProxyMap.insert(std::make_pair(nChannel, pCameraProxy));
    if (ret.second != true)
    {
        aloge("fatal error! mCameraProxyMap insert(%p, %d) fail!", pCameraProxy, nChannel);
    }
    mIdleEncBufList.resize(5);
    mDone = false;
    mbWaitFrameFlag = false;
    int result = pthread_create(&mThreadId, NULL, CameraFrameThread, this);
    if (result != 0)
    {
        aloge("fatal error! pthread_create error(%s)!", strerror(errno));
    }
}

CameraFrameManager::~CameraFrameManager()
{
    {
        Mutex::Autolock autoLock(mLock);

        mDone = true;
        mCondWaitFrame.signal();
    }
    void *status;
    pthread_join(mThreadId, &status);

    mIdleEncBufList.clear();
    if(!mReleasingEncBufList.empty())
    {
        aloge("fatal error! It still has [%d]Frames releasing to camera", mReleasingEncBufList.size());
        for(std::list<VideoFrameInfo>::iterator it=mReleasingEncBufList.begin(); it!=mReleasingEncBufList.end();)
        {
            alogd("ReleasingFrameId[0x%x], we are releasing to camera", it->mFrameInfo.mId);
            it = mReleasingEncBufList.erase(it);
        }
    }
    if(!mWaitReleaseEncBufList.empty())
    {
        aloge("fatal error! It still has [%d]Frames not release to camera", mWaitReleaseEncBufList.size());
        for(std::list<VideoFrameInfo>::iterator it=mWaitReleaseEncBufList.begin(); it!=mWaitReleaseEncBufList.end();)
        {
            alogd("waitReleaseFrameId[0x%x], we should release to camera", it->mFrameInfo.mId);
            it = mWaitReleaseEncBufList.erase(it);
        }
    }
}

status_t CameraFrameManager::addReleaseFrame(VI_DEV Vipp, VIDEO_FRAME_INFO_S *pReleaseFrame)
{
    Mutex::Autolock autoLock(mLock);
    if(mIdleEncBufList.empty())
    {
        alogd("Be careful! IdleEncBuf is empty, increase!");
        //mIdleEncBufList.resize(5);
        VideoFrameInfo elem;
        mIdleEncBufList.insert (mIdleEncBufList.end(), 5, elem);
    }
    mIdleEncBufList.front().mVipp = Vipp;
    memcpy(&mIdleEncBufList.front().mFrameInfo, pReleaseFrame, sizeof(VIDEO_FRAME_INFO_S));
    mWaitReleaseEncBufList.splice(mWaitReleaseEncBufList.end(), mIdleEncBufList, mIdleEncBufList.begin());
    if(mbWaitFrameFlag)
    {
        mCondWaitFrame.signal();
    }
    return NO_ERROR;
}

status_t CameraFrameManager::addCameraProxy(CameraRecordingProxy * pCameraProxy, VI_DEV Vipp)
{
    std::map<VI_DEV, CameraRecordingProxy*>::iterator it;
    it = mCameraProxyMap.find(Vipp);
    if (it != mCameraProxyMap.end())
    {
        alogd("CameraRecordingProxy: %p, Vipp: %d has already exist", it->second, it->first);
        return NO_ERROR;
    }
    auto ret = mCameraProxyMap.insert(std::make_pair(Vipp, pCameraProxy));
    if (ret.second != true)
    {
        aloge("fatal error! mCameraProxyMap insert(%p, %d) fail!", pCameraProxy, Vipp);
        return BAD_VALUE;
    }
    return NO_ERROR;
}

void* CameraFrameManager::CameraFrameThread(void* user)
{
    CameraFrameManager *pThiz = (CameraFrameManager*)user;
    prctl(PR_SET_NAME, (unsigned long)"CameraFrameThread", 0, 0, 0);
    while(1)
    {
        pThiz->mLock.lock();
        if(!pThiz->mWaitReleaseEncBufList.empty())
        {
            if(!pThiz->mReleasingEncBufList.empty())
            {
                aloge("fatal error! check code!");
            }
            pThiz->mReleasingEncBufList.splice(pThiz->mReleasingEncBufList.end(), pThiz->mWaitReleaseEncBufList, pThiz->mWaitReleaseEncBufList.begin());
            pThiz->mLock.unlock();
            //release to camera
            VI_DEV Vipp = pThiz->mReleasingEncBufList.front().mVipp;
            //find CameraProxy
            std::map<VI_DEV, CameraRecordingProxy*>::iterator it = pThiz->mCameraProxyMap.find(Vipp);
            if (pThiz->mCameraProxyMap.end() == it)
            {
                aloge("fatal error! mCameraProxyMap don't have Vipp[%d]", Vipp);
            }
            CameraRecordingProxy *npCameraProxy = it->second;

            npCameraProxy->releaseRecordingFrame(Vipp, pThiz->mReleasingEncBufList.front().mFrameInfo.mId);
            pThiz->mLock.lock();
            pThiz->mIdleEncBufList.splice(pThiz->mIdleEncBufList.end(), pThiz->mReleasingEncBufList, pThiz->mReleasingEncBufList.begin());
            pThiz->mLock.unlock();
        }
        else
        {
            while(!pThiz->mDone && pThiz->mWaitReleaseEncBufList.empty())
            {
                pThiz->mbWaitFrameFlag = true;
                pThiz->mCondWaitFrame.wait(pThiz->mLock);
                pThiz->mbWaitFrameFlag = false;
            }
            if(pThiz->mDone)
            {
                if(pThiz->mWaitReleaseEncBufList.empty())
                {
                    //alogd("detect mDone is true, exit CameraFrameThread! vipp[%d]", pThiz->mCameraChannel);
                    pThiz->mLock.unlock();
                    break;
                }
                else
                {
                    //alogd("detect mDone is true, but need release [%d]frames to camera before exit! vipp[%d]", pThiz->mWaitReleaseEncBufList.size(), pThiz->mCameraChannel);
                    pThiz->mLock.unlock();
                }
            }
            else
            {
                pThiz->mLock.unlock();
            }
        }
    }
    return (void*)NO_ERROR;
}

};
