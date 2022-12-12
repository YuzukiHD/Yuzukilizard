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
#define LOG_TAG "CedarXNativeWindowRenderer"
#include <CDX_Debug.h>

#include <binder/MemoryHeapBase.h>

#include <system/window.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaData.h>
#include <ui/GraphicBufferMapper.h>
#if (CEDARX_ANDROID_VERSION <= 9)
#include <gui/ISurfaceTexture.h>
#else
#include <gui/Surface.h>
#endif
#include <hardware/hal_public.h>
#include <ion/ion.h>
#include "CedarXNativeWindowRenderer.h"
#include <CedarXMetaData.h>
#include <CDX_Common.h>
#include <ConfigOption.h>
#include <config.h>

#if defined(__CHIP_VERSION_F81)
#define PHY_OFFSET 0x40000000
#elif defined(__CHIP_VERSION_F39)
#define PHY_OFFSET 0x20000000
#else
#define PHY_OFFSET 0x40000000
#endif

namespace EyeseeLinux {

CedarXNativeWindowRenderer::CedarXNativeWindowRenderer(
            const sp<ANativeWindow> &nativeWindow,
            const sp<MetaData> &meta,
            const int ionFd)
        : mNativeWindow(nativeWindow),
        mIonFd(dup(ionFd))
{
    int err;
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

    applyRotation(rotationDegrees);

    if(mCropWidth>=0 && mCropHeight>=0 && (nGpuBufWidth!=mCropWidth || nGpuBufHeight!=mCropHeight))
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

    mNumUndequeuedBuffers = 0;
    err = mNativeWindow->query(
            mNativeWindow.get(), NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
            (int *)&mNumUndequeuedBuffers);
    if (err != 0) 
    {
        ALOGE("(f:%s, l:%d) fatal error! NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS query failed: %s (%d)", __FUNCTION__, __LINE__, strerror(-err), -err);
        //return err;
    }
    if(mNumUndequeuedBuffers < 2)
    {
        ALOGW("(f:%s, l:%d) Be careful! min undequeued buffers number[%d] must >= 2!", __FUNCTION__, __LINE__, mNumUndequeuedBuffers);
        mNumUndequeuedBuffers = 2;
    }
    if(mNumUndequeuedBuffers > NUM_OF_PICTURES_KEEP_IN_LIST - 1)
    {
        ALOGE("(f:%s, l:%d) fatal error! GUI Undequeued Buffers[%d] > vdeclib preference[%d - 1]", __FUNCTION__, __LINE__, mNumUndequeuedBuffers, NUM_OF_PICTURES_KEEP_IN_LIST);
    }
    CHECK(meta->findInt32(kCedarXKeyFrameCount, &mFrameCount));
    if(mFrameCount <= mNumUndequeuedBuffers)
    {
        ALOGE("(f:%s, l:%d) fatal error! gpu buffer num[%d][%d]", __FUNCTION__, __LINE__, mFrameCount, mNumUndequeuedBuffers);
    }
    //mFrameCount += mNumUndequeuedBuffers;
    ALOGD("(f:%s, l:%d) set gpu frame count[%d], undequeuedNum[%d]", __FUNCTION__, __LINE__, mFrameCount, mNumUndequeuedBuffers);
    err = native_window_set_buffer_count(mNativeWindow.get(), mFrameCount);
    if (err != 0) 
    {
        ALOGE("(f:%s, l:%d) fatal error! native_window_set_buffer_count failed: %s (%d)", __FUNCTION__, __LINE__, strerror(-err), -err);
        //return err;
    }
}

int CedarXNativeWindowRenderer::control(int cmd, int para, void *pData)
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
        case VIDEORENDER_CMD_GET_ANATIVEWINDOWBUFFERS:
        {
            CdxANWBuffersInfo *pAnwBuffersInfo = (CdxANWBuffersInfo*)pData;
            int i;
            int err;
            // Dequeue buffers and send them to videoRender
            pAnwBuffersInfo->mnBufNum = 0;
            for (i = 0; i < mFrameCount; i++) 
            {
                ANativeWindowBuffer *buf;
                err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buf);
                if (err != 0) 
                {
                    ALOGE("(f:%s, l:%d) dequeueBuffer failed: %s (%d)", __FUNCTION__, __LINE__, strerror(-err), -err);
                    break;
                }
                CHECK_EQ(0, mNativeWindow->lockBuffer_DEPRECATED(mNativeWindow.get(), buf));

                GraphicBufferMapper &mapper = GraphicBufferMapper::get();
                Rect bounds(mWidth, mHeight);
                void *dst = NULL;
                uintptr_t dstPhyAddr = NULL;
                CHECK_EQ(0, mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst));

#if (CEDARX_ANDROID_VERSION >= 11)
            	ion_user_handle_t handle_ion = 0;
#else
            	struct ion_handle *handle_ion  = NULL;
#endif
#if(CDXCFG_GPU_TYPE == OPTION_GPU_TYPE_MALI)
    			private_handle_t* hnd = (private_handle_t *)(buf->handle);
#else
                IMG_native_handle_t* hnd = (IMG_native_handle_t*)(buf->handle);
#endif
                if(hnd != NULL)
                {
#if(CDXCFG_GPU_TYPE == OPTION_GPU_TYPE_MALI)
    				ion_import(mIonFd, hnd->share_fd, &handle_ion);
#else
    				ion_import(mIonFd, hnd->fd[0], &handle_ion);
#endif
                }
                else
                {
                    ALOGE("(f:%s, l:%d) fatal error! the hnd is wrong : hnd = %p", __FUNCTION__, __LINE__, hnd);
                    return UNKNOWN_ERROR;
                }
                dstPhyAddr = (uintptr_t)ion_getphyadr(mIonFd, handle_ion);
                if(dstPhyAddr >= PHY_OFFSET)
                {
                    dstPhyAddr -= PHY_OFFSET;
                }
            
                pAnwBuffersInfo->mANWBuffers[i].width     = buf->width;
                pAnwBuffersInfo->mANWBuffers[i].height    = buf->height;
                pAnwBuffersInfo->mANWBuffers[i].stride    = buf->stride;
                pAnwBuffersInfo->mANWBuffers[i].format    = buf->format;
                pAnwBuffersInfo->mANWBuffers[i].usage     = buf->usage;
                pAnwBuffersInfo->mANWBuffers[i].dst       = dst;
                pAnwBuffersInfo->mANWBuffers[i].dstPhy    = (void*)dstPhyAddr;
                pAnwBuffersInfo->mANWBuffers[i].pObjANativeWindowBuffer = (void*)buf;
                pAnwBuffersInfo->mANWBuffers[i].mIonUserHandle = (int)handle_ion;
                pAnwBuffersInfo->mANWBuffers[i].mbOccupyFlag = 1;
                pAnwBuffersInfo->mnBufNum++;
            }
            int cancelStart;
            int cancelEnd;
            if (err != 0) 
            {
                if(pAnwBuffersInfo->mnBufNum - mNumUndequeuedBuffers >= 0)
                {
                    cancelStart = pAnwBuffersInfo->mnBufNum - mNumUndequeuedBuffers;
                    cancelEnd = pAnwBuffersInfo->mnBufNum;
                }
                else
                {
                    cancelStart = 0;
                    cancelEnd = pAnwBuffersInfo->mnBufNum;
                }
            }
            else 
            {
                // Return the required minimum undequeued buffers to the native window.
                cancelStart = mFrameCount - mNumUndequeuedBuffers;
                cancelEnd = mFrameCount;
            }
            for(i=cancelStart; i<cancelEnd; i++)
            {
                ANativeWindowBuffer *buf = (ANativeWindowBuffer*)pAnwBuffersInfo->mANWBuffers[i].pObjANativeWindowBuffer;
                GraphicBufferMapper &mapper = GraphicBufferMapper::get();
                mapper.unlock(buf->handle);
                err = mNativeWindow->cancelBuffer(mNativeWindow.get(), buf, -1);
                ALOGE_IF(err != 0, "(f:%s, l:%d) fatal error! can not return buffer[%p][%p][%p] to native window",
                        __FUNCTION__, __LINE__, buf, pAnwBuffersInfo->mANWBuffers[i].dst, pAnwBuffersInfo->mANWBuffers[i].dstPhy);
                pAnwBuffersInfo->mANWBuffers[i].mbOccupyFlag = 0;
            }
            break;
        }
        case VIDEORENDER_CMD_UPDATE_DISPLAY_SIZE:
        {
            CdxVRFrameInfo *pFrameInfo = (CdxVRFrameInfo*)pData;
            if(pFrameInfo->mnDisplayWidth != mCropWidth || pFrameInfo->mnDisplayHeight != mCropHeight)
            {
                ALOGD("(f:%s, l:%d) need update crop gpuFrame[%dx%d], crop[%dx%d]->[%dx%d]", __FUNCTION__, __LINE__, 
                    mWidth, mHeight, mCropWidth, mCropHeight, pFrameInfo->mnDisplayWidth, pFrameInfo->mnDisplayHeight);
                mCropWidth = pFrameInfo->mnDisplayWidth;
                mCropHeight = pFrameInfo->mnDisplayHeight;
                //Rect crop;
                android_native_rect_t crop;
                crop.left = 0;
                crop.top  = 0;
                crop.right = mCropWidth;
                crop.bottom = mCropHeight;
                //mNativeWindow->perform(mNativeWindow.get(), NATIVE_WINDOW_SET_CROP, &crop);
                native_window_set_crop(mNativeWindow.get(), &crop);
            }
            break;
        }
        default:
        {
            ALOGW("undefined command[0x%x]!", cmd);
            ret = UNKNOWN_ERROR;
            break;
        }
    }
    return ret;
}
void CedarXNativeWindowRenderer::render(const void *data, size_t size) 
{
    ALOGE("(f:%s, l:%d) fatal error! why call here?", __FUNCTION__, __LINE__);
}

int CedarXNativeWindowRenderer::dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject)
{
    ANativeWindowBuffer *buf;
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
    return NO_ERROR;
}

int CedarXNativeWindowRenderer::enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject)
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
    //pObject->pObjANativeWindowBuffer = NULL;
    return NO_ERROR;
}

int CedarXNativeWindowRenderer::cancelFrame(ANativeWindowBufferCedarXWrapper *pObject)
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
    //pObject->pObjANativeWindowBuffer = NULL;
    return NO_ERROR;
}

CedarXNativeWindowRenderer::~CedarXNativeWindowRenderer()
{
    if(mIonFd >= 0)
    {
        ion_close(mIonFd);
        mIonFd = -1;
    }
}

void CedarXNativeWindowRenderer::applyRotation(int32_t rotationDegrees) {
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
}

}  // namespace android

