/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : CedarXNativeWindowRenderer.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-9-14
* Description:
    CedarXNativeWindowRender is designed for VideoRenderMode: VideoRender_GUI,
which malloc vdec frame buffer from native window directly.
********************************************************************************
*/
#ifndef _CEDARXNATIVEWINDOWRENDERER_H_
#define _CEDARXNATIVEWINDOWRENDERER_H_

#include <utils/RefBase.h>
#if (CEDARX_ANDROID_VERSION <= 9)
#include <gui/ISurfaceTexture.h>
#else
#include <gui/Surface.h>
#endif

#include <CDX_PlayerAPI.h>
#include "CedarXRender.h"

namespace EyeseeLinux {

struct CedarXNativeWindowRenderer : public CedarXRenderer {
    CedarXNativeWindowRenderer(
            const sp<ANativeWindow> &nativeWindow,
            const sp<MetaData> &meta,
            const int ionFd);
    void render(const void *data, size_t size);
    int control(int cmd, int para, void *pData);
    int dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    int enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject);
    int cancelFrame(ANativeWindowBufferCedarXWrapper *pObject);
protected:
    virtual ~CedarXNativeWindowRenderer();

private:
    int mIonFd;
    int mVideoScalingMode;  //NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW
    sp<ANativeWindow> mNativeWindow;
    int32_t mWidth, mHeight;    //buf width and height
    //int32_t mCropLeft, mCropTop, mCropRight, mCropBottom;
    int32_t mCropWidth, mCropHeight;    //display width and height
    int32_t mPixelFormat;    //CDX_PIXEL_FORMAT_YV12

    int32_t mNumUndequeuedBuffers;  //ANativeWindow must remain some buffers undequeued.
    int32_t mFrameCount;    //ANativeWindow frame count.
    
    void applyRotation(int32_t rotationDegrees);

    CedarXNativeWindowRenderer(const CedarXNativeWindowRenderer &);
    CedarXNativeWindowRenderer &operator=(
            const CedarXNativeWindowRenderer &);
};

}  // namespace android

#endif  /* _CEDARXNATIVEWINDOWRENDERER_H_ */

