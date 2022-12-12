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

//#define LOG_NDEBUG 0
#define LOG_TAG "CedarXSoftwareRenderer"
#include <CDX_Debug.h>

#include <binder/MemoryHeapBase.h>

#if (CEDARX_ANDROID_VERSION < 7)
#include <binder/MemoryHeapPmem.h>
#include <surfaceflinger/Surface.h>
#include <ui/android_native_buffer.h>
#else
#include <system/window.h>
#endif

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaData.h>
#include <ui/GraphicBufferMapper.h>
#if (CEDARX_ANDROID_VERSION <= 9)
#include <gui/ISurfaceTexture.h>
#else
#include <gui/Surface.h>
#endif
//#include <hardware/hal_public.h>
#include <ion/ion.h>
#include "CedarXSoftwareRenderer.h"
#include <CDX_Common.h>

extern "C" {
extern unsigned int cedarv_address_phy2vir(void *addr);
}
namespace EyeseeLinux {

CedarXSoftwareRenderer::CedarXSoftwareRenderer(
        const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta)
    : //mYUVMode(None),
    mNativeWindow(nativeWindow)
{
    CHECK(meta->findInt32(kKeyColorFormat, &mPixelFormat));

    //CHECK(meta->findInt32(kKeyScreenID, &screenID));
    //CHECK(meta->findInt32(kKeyColorFormat, &halFormat));
    CHECK(meta->findInt32(kKeyWidth, &mWidth));
    CHECK(meta->findInt32(kKeyHeight, &mHeight));
    CHECK(meta->findInt32(kKeyDisplayWidth, &mCropWidth));
    CHECK(meta->findInt32(kKeyDisplayHeight, &mCropHeight));

    int32_t nVdecInitRotation;  //clock wise.
    int32_t rotationDegrees;    //command gpu transform frame buffer clock wise degree.
    if (!meta->findInt32(kKeyRotation, &nVdecInitRotation)) {
        ALOGD("(f:%s, l:%d) find fail nVdecInitRotation[%d]", __FUNCTION__, __LINE__, nVdecInitRotation);
        rotationDegrees = 0;
    }
    else
    {
        switch(nVdecInitRotation)
        {
            case 0:
            {
                rotationDegrees = 0;
                break;
            }
            case 1:
            {
                rotationDegrees = 270;
                break;
            }
            case 2:
            {
                rotationDegrees = 180;
                break;
            }
            case 3:
            {
                rotationDegrees = 90;
                break;
            }
            default:
            {
                ALOGE("(f:%s, l:%d) fatal error! nInitRotation=%d", __FUNCTION__, __LINE__, nVdecInitRotation);
                rotationDegrees = 0;
                break;
            }
        }
    }
	
    size_t nGpuBufWidth, nGpuBufHeight;
    nGpuBufWidth = mWidth;  //ALIGN32(mWidth);
    nGpuBufHeight = mHeight;    //ALIGN32(mHeight);

    CHECK(mNativeWindow != NULL);
    int usage = 0;
    meta->findInt32(kKeyIsDRM, &usage);
    if(usage) {
    	ALOGD("(f:%s, l:%d) Be careful! protected video", __FUNCTION__, __LINE__);
    	usage = GRALLOC_USAGE_PROTECTED;
    }
    usage |= GRALLOC_USAGE_SW_READ_NEVER /*| GRALLOC_USAGE_SW_WRITE_OFTEN*/
            | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP;

    CHECK_EQ(0,
            native_window_set_usage(
            mNativeWindow.get(),
            usage));

    CHECK_EQ(0,
            native_window_set_scaling_mode(
            mNativeWindow.get(),
            NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));
    mVideoScalingMode = NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW;

    CHECK_EQ(0, native_window_set_buffers_geometry(
                mNativeWindow.get(),
                nGpuBufWidth,
                nGpuBufHeight,
                mPixelFormat));
    uint32_t transform;
    switch (rotationDegrees) {
        case 0: transform = 0; break;
        case 90: transform = HAL_TRANSFORM_ROT_90; break;
        case 180: transform = HAL_TRANSFORM_ROT_180; break;
        case 270: transform = HAL_TRANSFORM_ROT_270; break;
        default: transform = 0; break;
    }

    if (transform) {
        CHECK_EQ(0, native_window_set_buffers_transform(
                    mNativeWindow.get(), transform));
    }
    if(nGpuBufWidth!=mCropWidth || nGpuBufHeight!=mCropHeight)
    {
        ALOGD("(f:%s, l:%d) Be careful! need crop gpuFrame![%dx%d], crop[%dx%d]", __FUNCTION__, __LINE__, nGpuBufWidth, nGpuBufHeight, mCropWidth, mCropHeight);
        //Rect crop;
        android_native_rect_t crop;
        crop.left = 0;
        crop.top  = 0;
        crop.right = mCropWidth;
        crop.bottom = mCropHeight;
        //mNativeWindow->perform(mNativeWindow.get(), NATIVE_WINDOW_SET_CROP, &crop);
        native_window_set_crop(mNativeWindow.get(), &crop);
    }
}

CedarXSoftwareRenderer::~CedarXSoftwareRenderer() 
{
}

//static int ALIGN(int x, int y) {
    // y must be a power of 2.
//    return (x + y - 1) & ~(y - 1);
//}

int CedarXSoftwareRenderer::control(int cmd, int para, void *pData)
{
    int ret = NO_ERROR;
    switch(cmd)
    {
        case VIDEORENDER_CMD_SET_CROP:
        {
            android_native_rect_t *pCrop = (android_native_rect_t*)pData;
            ret = native_window_set_crop(mNativeWindow.get(), pCrop);
            break;
        }
        case VIDEORENDER_CMD_SET_SCALING_MODE:
        {
            if (mNativeWindow != NULL) 
            {
                if(mVideoScalingMode != para)
                {
                    ret = native_window_set_scaling_mode(mNativeWindow.get(), para);
                    if (NO_ERROR == ret)
                    {
                        mVideoScalingMode = para;
                    }
                    else
                    {
                        ALOGE("(f:%s, l:%d) fatal error! Failed to set scaling mode: %d", __FUNCTION__, __LINE__, ret);
                    }
                }
            }
            break;
        }
        default:
            ALOGW("undefined command[0x%x]!", cmd);
            ret = UNKNOWN_ERROR;
            break;
    }
    return ret;
}
void CedarXSoftwareRenderer::render(const void *pObject, size_t size, void *platformPrivate)
{
    ALOGE("(f:%s, l:%d) fatal error! why call here?", __FUNCTION__, __LINE__);
}

int CedarXSoftwareRenderer::dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    ANativeWindowBuffer *buf;
	//libhwclayerpara_t*  pOverlayParam = NULL;
    //Virtuallibhwclayerpara *pVirtuallibhwclayerpara = (Virtuallibhwclayerpara*)pObject;
    ANativeWindowBufferCedarXWrapper *pANativeWindowBuffer = (ANativeWindowBufferCedarXWrapper*)pObject;
    int err;

#if (CEDARX_ANDROID_VERSION >=8)
    if ((err = mNativeWindow->dequeueBuffer_DEPRECATED(mNativeWindow.get(), &buf)) != 0) 
    {
        ALOGW("(f:%s, l:%d) Surface::dequeueBuffer returned error %d", __FUNCTION__, __LINE__, err);
        return -1;
    }
    CHECK_EQ(0, mNativeWindow->lockBuffer_DEPRECATED(mNativeWindow.get(), buf));
#else
    if ((err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf)) != 0)
    {
        LOGW("Surface::dequeueBuffer returned error %d", err);
        return -1;
    }
    CHECK_EQ(0, mNativeWindow->lockBuffer(mNativeWindow.get(), buf));
#endif

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    //Rect bounds(ALIGN32(mWidth), ALIGN32(mHeight));
    Rect bounds(mWidth, mHeight);

    void *dst;
    void *dstPhyAddr = NULL;
    int nIonUserHandle = -1;
    CHECK_EQ(0, mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst));
    //dstPhyAddr = getPhysicalAddress(buf, &nIonUserHandle);
//    LOGD("buf->stride:%d, buf->width:%d, buf->height:%d buf->format:%d, buf->usage:%d,WXH:%dx%d dst:%p", 
//        buf->stride, buf->width, buf->height, buf->format, buf->usage, mWidth, mHeight, dst);
    pANativeWindowBuffer->width     = buf->width;
    pANativeWindowBuffer->height    = buf->height;
    pANativeWindowBuffer->stride    = buf->stride;
    pANativeWindowBuffer->format    = buf->format;
    pANativeWindowBuffer->usage     = buf->usage;
    pANativeWindowBuffer->dst       = dst;
    pANativeWindowBuffer->dstPhy    = dstPhyAddr;
    pANativeWindowBuffer->pObjANativeWindowBuffer = (void*)buf;
    pANativeWindowBuffer->mIonUserHandle = nIonUserHandle;
    return 0;
}

int CedarXSoftwareRenderer::enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    int err;
    ANativeWindowBuffer *buf = (ANativeWindowBuffer*)pObject->pObjANativeWindowBuffer;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    CHECK_EQ(0, mapper.unlock(buf->handle));

#if (CEDARX_ANDROID_VERSION >=8)
    if ((err = mNativeWindow->queueBuffer_DEPRECATED(mNativeWindow.get(), buf)) != 0) {
        LOGW("Surface::queueBuffer returned error %d", err);
    }
    buf = NULL;
#else 
    if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf)) != 0) {
        LOGW("Surface::queueBuffer returned error %d", err);
    }
    buf = NULL;
#endif
    pObject->pObjANativeWindowBuffer = NULL;
    return 0;
}

int CedarXSoftwareRenderer::cancelFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    int err;
    ANativeWindowBuffer *buf = (ANativeWindowBuffer*)pObject->pObjANativeWindowBuffer;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    CHECK_EQ(0, mapper.unlock(buf->handle));

#if (CEDARX_ANDROID_VERSION >=8)
    if ((err = mNativeWindow->cancelBuffer_DEPRECATED(mNativeWindow.get(), buf)) != 0) {
        ALOGW("Surface::cancelBuffer returned error %d", err);
    }
    buf = NULL;
#else 
    if ((err = mNativeWindow->cancelBuffer(mNativeWindow.get(), buf)) != 0) {
        ALOGW("Surface::cancelBuffer returned error %d", err);
    }
    buf = NULL;
#endif
    pObject->pObjANativeWindowBuffer = NULL;
    return NO_ERROR;
}

#if 0
void* CedarXSoftwareRenderer::getPhysicalAddress(ANativeWindowBuffer *pANWB, int *pIonUserHandle)
{
    return NULL;
    uintptr_t phyAddr;
    int nGPUType = 1;   //1:mali, 0:img
    uintptr_t PHY_OFFSET = 0x40000000;
#if (CEDARX_ANDROID_VERSION >= 11)
    ion_user_handle_t handle_ion = 0;
#else
    struct ion_handle *handle_ion  = NULL;
#endif

#if (defined(__CHIP_VERSION_F81))
    #error "wrong chip!"
#elif (defined(__CHIP_VERSION_F73))
    IMG_native_handle_t* hnd = (IMG_native_handle_t*)(pANWB->handle);
    nGPUType = 0;;
    if(hnd != NULL)
    {
        ion_import(mIonFd, hnd->fd[0], &handle_ion);
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! ANWB->handle[%p]", __FUNCTION__, __LINE__, hnd);
        return NULL;
    }
#else 
    private_handle_t* hnd = (private_handle_t *)(pANWB->handle);
    nGPUType = 1;
    if(hnd != NULL)
    {
        ion_import(mIonFd, hnd->share_fd, &handle_ion);
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! ANWB->handle[%p]", __FUNCTION__, __LINE__, hnd);
        return NULL;
    }
#endif
    phyAddr = (uintptr_t)ion_getphyadr(mIonFd, handle_ion);
    if(phyAddr >= PHY_OFFSET)
    {
        phyAddr -= PHY_OFFSET;
    }
    if(pIonUserHandle)
    {
        *pIonUserHandle = (int)handle_ion;
    }
    return (void*)phyAddr;
}
#endif

CedarXLocalRenderer::CedarXLocalRenderer(const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta)
    : mTarget(new CedarXSoftwareRenderer(nativeWindow, meta)) 
{
}
int CedarXLocalRenderer::control(int cmd, int para, void *pData) 
{
    return mTarget->control(cmd, para, pData);
}
void CedarXLocalRenderer::render(const void *data, size_t size) 
{
    mTarget->render(data, size, NULL);
}
int CedarXLocalRenderer::dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    return mTarget->dequeueFrame(pObject);
}
int CedarXLocalRenderer::enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    return mTarget->enqueueFrame(pObject);
}

int CedarXLocalRenderer::cancelFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    return mTarget->cancelFrame(pObject);
}

CedarXLocalRenderer::~CedarXLocalRenderer() 
{
    delete mTarget;
    mTarget = NULL;
}

}  // namespace android

