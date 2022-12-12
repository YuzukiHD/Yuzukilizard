/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VREncBuffer.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/01/24
  Last Modified :
  Description   : memory block struct
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "VREncBuffer"
#include <utils/plat_log.h>

#include <cstdlib>
#include <cstring>
#include <utility>
#include <VREncBuffer.h>

namespace EyeseeLinux
{

VREncBuffer::VREncBuffer(int size)
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
}

//copy constructor
VREncBuffer::VREncBuffer(const VREncBuffer& ref)
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
}

//copy assignment
VREncBuffer& VREncBuffer::operator= (const VREncBuffer& ref)
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
    return *this;
}

//move constructor
VREncBuffer::VREncBuffer(VREncBuffer&& rRef)
{
    pMem = rRef.pMem;
    mSize = rRef.mSize;
    rRef.pMem = NULL;
    rRef.mSize = 0;
}

//move assignment
VREncBuffer& VREncBuffer::operator=(VREncBuffer&& rRef)
{
    std::swap(pMem, rRef.pMem);
    std::swap(mSize, rRef.mSize);
    return *this;
}


VREncBuffer::~VREncBuffer()
{
    alogv("~VREncBuffer()");
    if(pMem)
    {
        free(pMem);
    }
    //alogv("destructor this=%p: mem=%p, size=%d", this, pMem, mSize);
}

void* VREncBuffer::getPointer()
{
    return pMem;
}

int VREncBuffer::getSize()
{
    return mSize;
}

void* VREncBuffer::reallocBuffer(int size)
{
    if (pMem)
    {
        free(pMem);
        pMem = NULL;
    }

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
    
    return pMem;
}

void VREncBuffer::freeBuffer()
{
    if (pMem)
    {
        free(pMem);
    }
}

}; /* namespace EyeseeLinux */

