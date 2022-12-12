/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VIPPOSD.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/12/01
  Last Modified :
  Description   : some structure of VIPP OSD to share
  Function List :
  History       :
******************************************************************************/
#define LOG_NDEBUG 0
#define LOG_TAG "VIPPOSD"
#include <utils/plat_log.h>

#include <string.h>

#include <VIPPOSD.h>

namespace EyeseeLinux {
/*
status_t OSDRectInfo::AllocBuffer(int nBufSize)
{
    status_t ret = NO_ERROR;
    if(mpBuf!=NULL)
    {
        aloge("fatal error! why osd rect already has buffer?");
        free(mpBuf);
        mpBuf = NULL;
        mBufSize = 0;
    }
    if(mBufSize!=0)
    {
        aloge("fatal error! why osd rect buf size[%d] is not zero?", mBufSize);
        mBufSize = 0;
    }
    if(nBufSize > 0)
    {
        mpBuf = malloc(nBufSize);
        if(mpBuf!=NULL)
        {
            mBufSize = nBufSize;
        }
        else
        {
            aloge("fatal error! malloc fail!");
            ret = NO_MEMORY;
        }
    }
    else
    {
        aloge("fatal error! bufSize[%d] is wrong, no need malloc!", nBufSize);
    }
    return ret;
}

status_t OSDRectInfo::FreeBuffer()
{
    status_t ret = NO_ERROR;
    if(mpBuf!=NULL)
    {
        free(mpBuf);
        mpBuf = NULL;
        mBufSize = 0;
    }
    if(mBufSize!=0)
    {
        aloge("fatal error! why osd rect buf size[%d] is not zero?", mBufSize);
        mBufSize = 0;
    }
    return ret;
}

void* OSDRectInfo::GetBuffer()
{
    return mpBuf;
}

int OSDRectInfo::GetBufferSize()
{
    return mBufSize;
}

OSDRectInfo::OSDRectInfo(const OSDRectInfo& rhs)
{
    mType = rhs.mType;
    mFormat = rhs.mFormat;
    mColor = rhs.mColor;
    mRect = rhs.mRect;
    mpBuf = NULL;
    mBufSize = 0;
    if(rhs.mpBuf)
    {
        if(rhs.mBufSize > 0)
        {
            mpBuf = malloc(rhs.mBufSize);
            if(mpBuf!=NULL)
            {
                mBufSize = rhs.mBufSize;
                memcpy(mpBuf, rhs.mpBuf, mBufSize);
            }
            else
            {
                aloge("fatal error! malloc fail!");
            }
        }
        else
        {
            aloge("fatal error! buf[%p], size[%d]", rhs.mpBuf, rhs.mBufSize);
        }
    }
}

OSDRectInfo& OSDRectInfo::operator= (const OSDRectInfo& rhs)
{
    mType = rhs.mType;
    mFormat = rhs.mFormat;
    mColor = rhs.mColor;
    mRect = rhs.mRect;
    if(mpBuf!=NULL)
    {
        if(mBufSize > 0)
        {
            // alogv("free osd rect buf[%p]size[%d]", mpBuf, mBufSize);
        }
        else
        {
            aloge("fatal error! buf[%p]size[%d]", mpBuf, mBufSize);
        }
        free(mpBuf);
    }
    else
    {
        if(mBufSize != 0)
        {
            aloge("fatal error! buf[%p]size[%d]", mpBuf, mBufSize);
        }
    }
    mpBuf = NULL;
    mBufSize = 0;
    if(rhs.mpBuf)
    {
        if(rhs.mBufSize > 0)
        {
            mpBuf = malloc(rhs.mBufSize);
            if(mpBuf!=NULL)
            {
                mBufSize = rhs.mBufSize;
                memcpy(mpBuf, rhs.mpBuf, mBufSize);
            }
            else
            {
                aloge("fatal error! malloc fail!");
            }
        }
        else
        {
            aloge("fatal error! buf[%p], size[%d]", rhs.mpBuf, rhs.mBufSize);
        }
    }
    return *this;
}

OSDRectInfo::OSDRectInfo()
{
    mType = OSDType_Overlay;
    mFormat = MM_PIXEL_FORMAT_RGB_8888;
    mColor = 0;
    mRect = {0, 0, 0, 0};
    mpBuf = NULL;
    mBufSize = 0;
}

OSDRectInfo::~OSDRectInfo()
{
    if(mpBuf!=NULL)
    {
        if(mType != OSDType_Overlay)
        {
            aloge("fatal error! osd type[0x%x] is wrong!", mType);
        }
        if(0 == mBufSize)
        {
            aloge("fatal error! bufSize == 0!");
        }
        alogv("free osd rect buf[%p]size[%d]", mpBuf, mBufSize);
        free(mpBuf);
    }
}
*/
}

