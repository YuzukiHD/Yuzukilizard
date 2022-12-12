/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeQueue.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/12/12
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeQueue"
#include <utils/plat_log.h>

#include "EyeseeQueue.h"

namespace EyeseeLinux {

EyeseeQueue::EyeseeQueue(int elemNum)
{
    if(elemNum > 0)
    {
        mIdleList.resize(elemNum);
    }
}

EyeseeQueue::~EyeseeQueue()
{
    Mutex::Autolock autoLock(mLock);
    if(mValidList.size() > 0)
    {
        aloge("fatal error! there is [%d]elemDatas not dequeue.", mValidList.size());
    }
    if(mUsedList.size() > 0)
    {
        aloge("fatal error! there is [%d]elemDatas not return.", mUsedList.size());
    }
}

status_t EyeseeQueue::PutElemDataValid(void *pData)
{
    Mutex::Autolock autoLock(mLock);
    if (mIdleList.empty())
    {
        return UNKNOWN_ERROR;
    }
    mIdleList.front().mpData = pData;
    mValidList.splice(mValidList.end(), mIdleList, mIdleList.begin());
    return NO_ERROR;
}

void* EyeseeQueue::GetValidElemData()
{
    void *pData;
    Mutex::Autolock autoLock(mLock);
    if(mValidList.empty())
    {
        return NULL;
    }
    pData = mValidList.front().mpData;
    mUsedList.splice(mUsedList.end(), mValidList, mValidList.begin());
    return pData;
}

void* EyeseeQueue::ReleaseElemData(void *pData)
{
    Mutex::Autolock autoLock(mLock);

    for (std::list<Elem>::iterator it=mUsedList.begin(); it != mUsedList.end(); it++)
    {
        if (it->mpData == pData)
        {
            mIdleList.splice(mIdleList.end(), mUsedList, it);
            return pData;
        }
    }

    aloge("release elem not in usedlist!!");
    return NULL;
}

int EyeseeQueue::GetIdleElemNum()
{
    Mutex::Autolock autoLock(mLock);
    return mIdleList.size();
}

int EyeseeQueue::GetValidElemNum()
{
    Mutex::Autolock autoLock(mLock);
    return mValidList.size();
}

int EyeseeQueue::GetTotalElemNum()
{
    Mutex::Autolock autoLock(mLock);
    return mIdleList.size() + mValidList.size() + mUsedList.size();
}

status_t EyeseeQueue::AddIdleElems(int nNum)
{
    Mutex::Autolock autoLock(mLock);
    Elem tmp={NULL};
    mIdleList.insert(mIdleList.end(), nNum, tmp);
    return NO_ERROR;
}

}

