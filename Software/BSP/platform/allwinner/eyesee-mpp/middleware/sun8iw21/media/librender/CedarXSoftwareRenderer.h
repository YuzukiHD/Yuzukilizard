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

#ifndef SOFTWARE_RENDERER_H_
#define SOFTWARE_RENDERER_H_

#include <utils/RefBase.h>
#if (CEDARX_ANDROID_VERSION < 7)
#include <ui/android_native_buffer.h>
#endif
//#include <hardware/hwcomposer.h>
//#include "virtual_hwcomposer.h"
#if (CEDARX_ANDROID_VERSION <= 9)
#include <gui/ISurfaceTexture.h>
#else
#include <gui/Surface.h>
#endif

#include <CDX_PlayerAPI.h>
#include "CedarXRender.h"

#define ADAPT_A10_GPU_RENDER (0)
#define ADAPT_A31_GPU_RENDER (0)

namespace EyeseeLinux {

struct MetaData;

class CedarXSoftwareRenderer {
public:
    CedarXSoftwareRenderer(
            const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta);

    ~CedarXSoftwareRenderer();
    int control(int cmd, int para, void *pData);
    void render(
            const void *data, size_t size, void *platformPrivate);
    int dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    int enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    int cancelFrame(ANativeWindowBufferCedarXWrapper *pObject);
private:
//    enum YUVMode {
//        None,
//    };

    //OMX_COLOR_FORMATTYPE mColorFormat;
    //YUVMode mYUVMode;
    int mVideoScalingMode;  //NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW
    sp<ANativeWindow> mNativeWindow;
    int32_t mWidth, mHeight;    //buf width and height
    //int32_t mCropLeft, mCropTop, mCropRight, mCropBottom;
    int32_t mCropWidth, mCropHeight;    //display width and height
    int32_t mPixelFormat;    //CDX_PIXEL_FORMAT_YV12

    //void* getPhysicalAddress(ANativeWindowBuffer *pANWB, int *pIonUserHandle);
    CedarXSoftwareRenderer(const CedarXSoftwareRenderer &);
    CedarXSoftwareRenderer &operator=(const CedarXSoftwareRenderer &);
};

struct CedarXLocalRenderer : public CedarXRenderer 
{
    CedarXLocalRenderer(const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta);
    int control(int cmd, int para, void *pData);
    void render(const void *data, size_t size);
    int dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    int enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    int cancelFrame(ANativeWindowBufferCedarXWrapper *pObject);
protected:
    virtual ~CedarXLocalRenderer();

private:
    CedarXSoftwareRenderer *mTarget;
    CedarXLocalRenderer(const CedarXLocalRenderer &);
    CedarXLocalRenderer &operator=(const CedarXLocalRenderer &);;
};

}  // namespace android

#endif  // SOFTWARE_RENDERER_H_

