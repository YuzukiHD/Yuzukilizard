/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CEDARXNATIVE_RENDERER_H_
#define CEDARXNATIVE_RENDERER_H_

//#include <utils/RefBase.h>
//#include <hwdisplay.h>
#include "CedarXRender.h"

namespace EyeseeLinux {

struct MetaData;

class CedarXNativeRenderer : public CedarXRenderer
{
public:
    CedarXNativeRenderer(const int hlay, MetaData *meta);

    ~CedarXNativeRenderer();

    virtual void render(const void *data, size_t size);
    virtual int control(int cmd, int para, void *pData);
    virtual int dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    virtual int enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    virtual int cancelFrame(ANativeWindowBufferCedarXWrapper *pObject);

private:
    int mVideoLayerId;
    int32_t mWidth, mHeight;    //bufWidth and bufHeight
    int32_t mCropLeft, mCropTop, mCropRight, mCropBottom;
    int32_t mCropWidth, mCropHeight;    //displayWidth and displayHeight
    int32_t mPixelFormat;    //e_hwc_format
    CdxPixelFormat mCdxPixelFormat;
    int32_t mColorSpace;    //VENC_BT601
    int32_t mLayerShowed;

    CedarXNativeRenderer(const CedarXNativeRenderer &);
    CedarXNativeRenderer &operator=(const CedarXNativeRenderer &);
};

}  // namespace android

#endif  // SOFTWARE_RENDERER_H_

