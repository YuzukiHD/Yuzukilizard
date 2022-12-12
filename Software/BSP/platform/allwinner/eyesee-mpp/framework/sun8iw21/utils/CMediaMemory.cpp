/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CMediaMemory.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/01/24
  Last Modified :
  Description   : memory block struct
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "CMediaMemory"
#include <utils/plat_log.h>

#include <cstdlib>
#include <cstring>
#include <utility>

#include <CMediaMemory.h>

namespace EyeseeLinux
{
CMediaMemory::CMediaMemory()
{
    pMem = NULL;
    mSize = 0;
    //alogv("default construct this=%p", this);
}

CMediaMemory::CMediaMemory(int size)
{
    if(size > 0)
    {
        pMem = malloc(size);
        if(pMem == NULL)
        {
            mSize = 0;
            aloge("malloc size[%d] fail", size);
        }
        else
        {
            mSize = size;
        }
    }
    else
    {
        pMem = NULL;
        mSize = 0;
    }
    //alogv("construct this=%p, mem=%p, size=%d", this, pMem, mSize);
}

//copy constructor
CMediaMemory::CMediaMemory(const CMediaMemory& ref)
{
    if(ref.mSize > 0)
    {
        mSize = ref.mSize;
        pMem = malloc(mSize);
        memcpy(pMem, ref.pMem, mSize);
    }
    else
    {
        pMem = NULL;
        mSize = 0;
    }
    alogw("Be careful! please avoid copy construct this=%p, mem=%p, size=%d", this, pMem, mSize);
}

//copy assignment
CMediaMemory& CMediaMemory::operator= (const CMediaMemory& ref)
{
    if(ref.mSize > 0)
    {
        mSize = ref.mSize;
        pMem = malloc(mSize);
        memcpy(pMem, ref.pMem, mSize);
    }
    else
    {
        pMem = NULL;
        mSize = 0;
    }
    alogw("Be careful! please avoid copy assignment this=%p, mem=%p, size=%d", this, pMem, mSize);
    return *this;
}

//move constructor
CMediaMemory::CMediaMemory(CMediaMemory&& rRef)
{
    pMem = rRef.pMem;
    mSize = rRef.mSize;
    rRef.pMem = NULL;
    rRef.mSize = 0;
    //alogv("move construct this=%p, mem=%p, size=%d", this, pMem, mSize);
}

//move assignment
CMediaMemory& CMediaMemory::operator=(CMediaMemory&& rRef)
{
    std::swap(pMem, rRef.pMem);
    std::swap(mSize, rRef.mSize);
    //alogv("move assignment this=%p: after swap: mem=%p, size=%d, rRef mem=%p, size=%d", this, pMem, mSize, rRef.pMem, rRef.mSize);
    return *this;
}

CMediaMemory::~CMediaMemory()
{
    if(pMem)
    {
        free(pMem);
    }
    //alogv("destructor this=%p: mem=%p, size=%d", this, pMem, mSize);
}

void* CMediaMemory::getPointer() const
{
    return pMem;
}

int CMediaMemory::getSize() const
{
    return mSize;
}

}; /* namespace EyeseeLinux */

