/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : ISEChannel.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/10
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "ISEChannel"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>

#include <ion_memmanager.h>
#include <memoryAdapter.h>
#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <MediaStructConvert.h>

#include <SystemBase.h>
#include <mpi_ise.h>
#include <mpi_videoformat_conversion.h>

#include "ISEChannel.h"

#include <ConfigOption.h>

using namespace std;

namespace EyeseeLinux {

ISEChannel::DoCaptureProcess::DoCaptureProcess(ISEChannel *pIseChn)
    : mpIseChn(pIseChn)
{
}

status_t ISEChannel::DoCaptureProcess::SendCommand_TakePicture()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_TakePicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t ISEChannel::DoCaptureProcess::SendCommand_CancelContinuousPicture()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_CancelContinuousPicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

ISEChannel::DoPreviewThread::DoPreviewThread(ISEChannel *pIseChn)
            : mpIseChn(pIseChn)
            , mbWaitPreviewFrame(false)
            , mbWaitReleaseAllFrames(false)
{
}
status_t ISEChannel::DoPreviewThread::startThread()
{
    status_t ret = run("ISEChnPreviewTh");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}
void ISEChannel::DoPreviewThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePreview_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t ISEChannel::DoPreviewThread::notifyNewFrameCome()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitPreviewFrame)
    {
        mbWaitPreviewFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypePreview_InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

status_t ISEChannel::DoPreviewThread::releaseAllFrames()
{
    AutoMutex lock(mWaitLock);
    mbWaitReleaseAllFrames = true;
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePreview_releaseAllFrames;
    mMsgQueue.queueMessage(&msg);
    while(mbWaitReleaseAllFrames)
    {
        mCondReleaseAllFramesFinished.wait(mWaitLock);
    }
    return NO_ERROR;
}

bool ISEChannel::DoPreviewThread::threadLoop() 
{
    if(!exitPending()) 
    {
        return mpIseChn->previewThread();
    } 
    else 
    {
        return false;
    }
}

ISEChannel::DoPictureThread::DoPictureThread(ISEChannel *pIseChn)
            : mpIseChn(pIseChn)
            , mbWaitPictureFrame(false)
            , mbWaitReleaseAllFrames(false)
{
}

status_t ISEChannel::DoPictureThread::startThread() 
{
    status_t ret = run("ISEChnPictureTh");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void ISEChannel::DoPictureThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePicture_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t ISEChannel::DoPictureThread::notifyNewFrameCome()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitPictureFrame)
    {
        mbWaitPictureFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypePicture_InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

status_t ISEChannel::DoPictureThread::notifyPictureEnd()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePicture_SendPictureEnd;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t ISEChannel::DoPictureThread::releaseAllFrames()
{
    AutoMutex lock(mWaitLock);
    mbWaitReleaseAllFrames = true;
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePicture_releaseAllFrames;
    mMsgQueue.queueMessage(&msg);
    while(mbWaitReleaseAllFrames)
    {
        mCondReleaseAllFramesFinished.wait(mWaitLock);
    }
    return NO_ERROR;
}

bool ISEChannel::DoPictureThread::threadLoop() 
{
    if(!exitPending()) 
    {
        return mpIseChn->pictureThread();
    } 
    else 
    {
        return false;
    }
}

ISEChannel::DoCommandThread::DoCommandThread(ISEChannel *pIseChn)
    : mpIseChn(pIseChn)
{
}

status_t ISEChannel::DoCommandThread::startThread()
{
    status_t ret = run("ISEChnCommand");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void ISEChannel::DoCommandThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCommand_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

bool ISEChannel::DoCommandThread::threadLoop()
{
    if(!exitPending()) 
    {
        return mpIseChn->commandThread();
    } 
    else 
    {
        return false;
    }
}

status_t ISEChannel::DoCommandThread::SendCommand_TakePicture(unsigned int msgType)
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCommand_TakePicture;
    msg.mPara0 = msgType;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t ISEChannel::DoCommandThread::SendCommand_CancelContinuousPicture()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCommand_CancelContinuousPicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t ISEChannel::GetChnOutFrameSize(AW_U32 iseMode, ISE_CHN chnId, ISE_CHN_ATTR_S *pAttr, int &mFrameWidth, int &mFrameHeight)
{
    if ((chnId == MM_INVALID_CHN) || (NULL == pAttr))
    {
        return BAD_VALUE;
    }

    if (ISEMODE_ONE_FISHEYE == iseMode)
    {
        mFrameWidth  = pAttr->mode_attr.mFish.ise_cfg.out_luma_pitch[chnId];
        mFrameHeight = pAttr->mode_attr.mFish.ise_cfg.out_h[chnId];
    }
    else if (ISEMODE_TWO_FISHEYE == iseMode)
    {
      #if MPPCFG_ISE_TWO_FISHEYE
        mFrameWidth  = pAttr->mode_attr.mDFish.ise_cfg.out_luma_pitch[chnId];
        mFrameHeight = pAttr->mode_attr.mDFish.ise_cfg.out_h[chnId];
      #endif
    }
    else if (ISEMODE_TWO_ISE == iseMode)
    {
      #if MPPCFG_ISE_TWO_ISE
        if (0 == chnId)
        {
            mFrameWidth = pAttr->mode_attr.mIse.ise_cfg.pano_w;
            mFrameHeight = pAttr->mode_attr.mIse.ise_cfg.pano_h;
        }
        else
        {
            mFrameWidth = pAttr->mode_attr.mIse.ise_proccfg.scalar_w[chnId - 1];
            mFrameHeight = pAttr->mode_attr.mIse.ise_proccfg.scalar_h[chnId - 1];
        }
      #endif
    }
    return NO_ERROR;
}

ISEChannel::ISEChannel(ISE_GRP nGroupId, unsigned int iseMode, ISE_CHN_ATTR_S *pChnAttr)
    : mpPrevThread(NULL)
    , mpPicThread(NULL)
    , mpCallbackNotifier(NULL)
    , mpPreviewWindow(NULL)
    , mpPreviewQueue(NULL)
    , mpPictureQueue(NULL)
    , mNewZoom(0)
    , mLastZoom(-1)
    , mMaxZoom(0xffffffff)
    , mpMemOps(NULL)
    , mChannelState(ISE_CHN_STATE_CONSTRUCTED)
    , mTakePictureMode(TAKE_PICTURE_MODE_NULL)
    , mbProcessTakePictureStart(false)
    , mContinuousPictureCnt(0)
    , mContinuousPictureMax(0)
    , mContinuousPictureStartTime(0)
    , mContinuousPictureLast(0)
    , mContinuousPictureInterval(0)
    , mGroupId(nGroupId)
    , mIseMode(iseMode)
    , mFrameWidth(0)
    , mFrameHeight(0)
    , mFrameCounter(0)
{
    memset(&mRectCrop, 0, sizeof(mRectCrop));
    memset(&mPicFrmBuffer, 0, sizeof(mPicFrmBuffer));
    memcpy(&mAttr, pChnAttr, sizeof(ISE_CHN_ATTR_S));
    //mColorSpace = V4L2_COLORSPACE_JPEG;

    bool bSuccessFlag = false;
    ERRORTYPE ret;
    //mISEChn.mModId = MOD_ID_ISE;
    //mISEChn.mDevId = 0;
    //mISEChn.mChnId = 0;
    mChannelId = 0;
    while(mChannelId < ISE_MAX_CHN_NUM)
    {
        ret = AW_MPI_ISE_CreatePort(mGroupId, mChannelId, &mAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = true;
            alogd("create ISE scaler channel[%d] success!", mChannelId);
            break;
        }
        else if(ERR_ISE_EXIST == ret)
        {
            alogd("ISE scaler channel[%d] is exist, find next!", mChannelId);
            mChannelId++;
        }
        else
        {
            alogd("create ISE scaler channel[%d] ret[0x%x], find next!", mChannelId, ret);
            mChannelId++;
        }
    }
    if(false == bSuccessFlag)
    {
        mChannelId = MM_INVALID_CHN;
        aloge("fatal error! create ISE scaler channel fail!");
    }
    if(ISEMODE_ONE_FISHEYE == iseMode && mGroupId == 0 && mChannelId == 0)
    {
        AW_MPI_ISE_SetISEFreq(mGroupId, 696);
    }

//    if(ISEMODE_TWO_FISHEYE == iseMode && mGroupId == 0 && mChannelId == 0)
//    {
//        AW_MPI_ISE_SetISEFreq(mGroupId,600);
//    }

//    if(ISEMODE_TWO_ISE == iseMode && mGroupId == 0 && mChannelId == 0)
//    {
//        AW_MPI_ISE_SetISEFreq(mGroupId,576);
//    }

    //AW_MPI_ISE_SetPortAttr(mGroupId, mChannelId, &mAttr);
    if (NO_ERROR != GetChnOutFrameSize(mIseMode, mChannelId, &mAttr, mFrameWidth, mFrameHeight))
    {
        aloge("get ise chn out frame size error!");
    }

    mpCallbackNotifier = new CallbackNotifier(mChannelId, this);
    mpPreviewWindow = new PreviewWindow(this);
    mbPreviewEnable = true;

    mpPreviewQueue = new OSAL_QUEUE;
    OSAL_QueueCreate(mpPreviewQueue, ISE_MAX_BUFFER_NUM);
    mpPictureQueue = new OSAL_QUEUE;
    OSAL_QueueCreate(mpPictureQueue, ISE_MAX_BUFFER_NUM);

    mpCapProcess = new DoCaptureProcess(this);

    mpPrevThread = new DoPreviewThread(this);
    //mpPrevThread->startThread();

    mpPicThread = new DoPictureThread(this);
    //mpPicThread->startThread();

    mpCommandThread = new DoCommandThread(this);
    //mpCommandThread->startThread();

    mTakePicMsgType = 0;
    mbTakePictureStart = false;
    mbProcessTakePictureStart = false;
    mParameters.setPictureMode(TAKE_PICTURE_MODE_FAST);
}

status_t ISEChannel::startChannel()
{
    if(mbPreviewEnable)
    {
        mpPreviewWindow->startPreview();
    }
    mpPrevThread->startThread();
    mpPicThread->startThread();
    mpCommandThread->startThread();
    mChannelState = ISE_CHN_STATE_STARTED;

    return NO_ERROR;
}

ISEChannel::~ISEChannel()
{
    ERRORTYPE ret;
    if(mChannelId != MM_INVALID_CHN)
    {
        ret = AW_MPI_ISE_DestroyPort(mGroupId, mChannelId);
        if (ret != SUCCESS)
        {
            aloge("fatal error! AW_MPI_ISE_DestroyChn error!");
        }
        mChannelId = MM_INVALID_CHN;
    }
    if (mpCommandThread != NULL)
    {
        mpCommandThread->stopThread();
        delete mpCommandThread;
    }
    if(mpCapProcess != NULL)
    {
        delete mpCapProcess;
    }
    if (mpPrevThread != NULL)
    {
        mpPrevThread->stopThread();
        delete mpPrevThread;
    }
    if (mpPicThread != NULL)
    {
        mpPicThread->stopThread();
        delete mpPicThread;
    }
    OSAL_QueueTerminate(mpPreviewQueue);
    delete mpPreviewQueue;
    mpPreviewQueue = NULL;

    OSAL_QueueTerminate(mpPictureQueue);
    delete mpPictureQueue;
    mpPictureQueue = NULL;

    if (mpPreviewWindow != NULL) {
        delete mpPreviewWindow;
    }
    if (mpCallbackNotifier != NULL) {
        delete mpCallbackNotifier;
    }
}

ISE_CHN ISEChannel::getChannelId()
{
    AutoMutex lock(mLock);
    return mChannelId;
}
status_t ISEChannel::setParameters(CameraParameters &param)
{
    mParameters = param;
    mColorSpace = mParameters.getColorSpace();
    int rotation = param.getPreviewRotation();
    if(rotation!=0 && rotation!=90 && rotation!=180 && rotation!=270 && rotation!=360)
    {
        aloge("fatal error! ise rotation[%d] is invalid!", rotation);
    }
    else
    {
        alogd("ise paramPreviewRotation set to %d degree", rotation);
        mpPreviewWindow->setPreviewRotation(rotation);
    }
    return NO_ERROR;
}

status_t ISEChannel::getParameters(CameraParameters &param)
{
    param = mParameters;
    return NO_ERROR;
}

status_t ISEChannel::setChannelDisplay(int hlay)
{
    return mpPreviewWindow->setPreviewWindow(hlay);
}

bool ISEChannel::isPreviewEnabled()
{
    return mpPreviewWindow->isPreviewEnabled();
}

status_t ISEChannel::startRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    mbPreviewEnable = true;
    if (ISE_CHN_STATE_STARTED == mChannelState)
    {
        mpPreviewWindow->setDispBufferNum(2);
        ret = mpPreviewWindow->startPreview();
    }
    return ret;
}

status_t ISEChannel::stopRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    mbPreviewEnable = false;
    ret = mpPreviewWindow->stopPreview();
    return ret;
}

status_t ISEChannel::stopChannel()
{
    AutoMutex lock1(mLock);

    if (mChannelState != ISE_CHN_STATE_STARTED)
    {
        aloge("stopChannel in error state %d", mChannelState);
        return INVALID_OPERATION;
    }

    alogv("stopChannel");
    if (mbPreviewEnable == true)
    {
        mpPreviewWindow->stopPreview();
    }
    mpPrevThread->releaseAllFrames();
    mpPicThread->releaseAllFrames();

    return NO_ERROR;
}

status_t ISEChannel::releaseAllISEFrames()
{
    mpPrevThread->releaseAllFrames();
    mpPicThread->releaseAllFrames();
    return NO_ERROR;
}

status_t ISEChannel::releaseRecordingFrame(uint32_t index)
{
    //VIDEO_FRAME_BUFFER_S *pFrmbuf = mpVideoFrmBuffer + index;
    VIDEO_FRAME_BUFFER_S *pFrmbuf = NULL;
    mFrameBuffersLock.lock();
    for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mFrameBuffers.begin(); it!=mFrameBuffers.end(); ++it)
    {
        if(it->mFrameBuf.mId == index)
        {
            if(NULL == pFrmbuf)
            {
                pFrmbuf = &*it;
            }
            else
            {
                aloge("fatal error! MPP-ISE frame id[0x%x] is repeated!", index);
            }
        }
    }
    mFrameBuffersLock.unlock();
    if(NULL == pFrmbuf)
    {
        aloge("fatal error! not find frame index[0x%x]", index);
        return UNKNOWN_ERROR;
    }

    int ret;
    AutoMutex lock(mRefCntLock);

    if (pFrmbuf->mRefCnt > 0 && --pFrmbuf->mRefCnt == 0) 
    {
        ret = AW_MPI_ISE_ReleaseData(mGroupId, mChannelId, &pFrmbuf->mFrameBuf);
        if (ret != SUCCESS) 
        {
            aloge("AW_MPI_ISE_ReleaseFrame error! GrpId:%d, ChnId:%d, mPicId:%d", mGroupId, mChannelId, pFrmbuf->mFrameBuf.mId);
        }
    }
    return NO_ERROR;
}

status_t ISEChannel::startRecording(CameraRecordingProxyListener *pCb, int recorderId)
{
    return mpCallbackNotifier->startRecording(pCb, recorderId);
}

status_t ISEChannel::stopRecording(int recorderId)
{
    return mpCallbackNotifier->stopRecording(recorderId);
}

void ISEChannel::setDataListener(DataListener *pCb)
{
    mpCallbackNotifier->setDataListener(pCb);
}

void ISEChannel::setNotifyListener(NotifyListener *pCb)
{
    mpCallbackNotifier->setNotifyListener(pCb);
}


status_t ISEChannel::doTakePicture(unsigned int msgType)
{
    AutoMutex lock(mLock);
    int jpeg_quality = mParameters.getJpegQuality();
    if (jpeg_quality <= 0) {
        jpeg_quality = 90;
    }
    mpCallbackNotifier->setJpegQuality(jpeg_quality);

    int jpeg_rotate = mParameters.getJpegRotation();
    if (jpeg_rotate <= 0) {
        jpeg_rotate = 0;
    }
    mpCallbackNotifier->setJpegRotate(jpeg_rotate);

    SIZE_S size;
    mParameters.getPictureSize(size);
    mpCallbackNotifier->setPictureSize(size.Width, size.Height);
    mParameters.getJpegThumbnailSize(size);
    mpCallbackNotifier->setJpegThumbnailSize(size.Width, size.Height);

    char *pGpsMethod = mParameters.getGpsProcessingMethod();
    if (pGpsMethod != NULL) {
        mpCallbackNotifier->setGPSLatitude(mParameters.getGpsLatitude());
        mpCallbackNotifier->setGPSLongitude(mParameters.getGpsLongitude());
        mpCallbackNotifier->setGPSAltitude(mParameters.getGpsAltitude());
        mpCallbackNotifier->setGPSTimestamp(mParameters.getGpsTimestamp());
        mpCallbackNotifier->setGPSMethod(pGpsMethod);
    }
    mTakePicMsgType = msgType;
    mpCallbackNotifier->enableMessage(msgType);

    mTakePictureMode = mParameters.getPictureMode();
    mpCapProcess->SendCommand_TakePicture();

    return NO_ERROR;
}

status_t ISEChannel::doCancelContinuousPicture()
{
    AutoMutex lock(mLock);
    mpCapProcess->SendCommand_CancelContinuousPicture();
    return NO_ERROR;
}

status_t ISEChannel::takePicture(unsigned int msgType, PictureRegionCallback *pPicReg)
{
    AutoMutex lock(mLock);
    if(mbTakePictureStart)
    {
        aloge("fatal error! ISE During taking picture, we don't accept new takePicture command!");
        return UNKNOWN_ERROR;
    }
    else
    {
        mbTakePictureStart = true;
        mpCallbackNotifier->setPictureRegionCallback(pPicReg);
        mpCommandThread->SendCommand_TakePicture(msgType);
        return NO_ERROR;
    }
}

status_t ISEChannel::notifyPictureRelease()
{
    return mpCallbackNotifier->notifyPictureRelease();
}

status_t ISEChannel::cancelContinuousPicture()
{
    AutoMutex lock(mLock);
    if(!mbTakePictureStart)
    {
        aloge("fatal error! ISE not start take picture!");
        return NO_ERROR;
    }
    mpCommandThread->SendCommand_CancelContinuousPicture();
    return NO_ERROR;
}

/*
void ISEChannel::postDataCompleted(const void *pData, int size)
{
    mpCallbackNotifier->postDataCompleted(pData, size);
}
*/

void ISEChannel::increaseBufRef(VIDEO_FRAME_BUFFER_S *pBuf)
{
    VIDEO_FRAME_BUFFER_S *pFrame = (VIDEO_FRAME_BUFFER_S*)pBuf;
    AutoMutex lock(mRefCntLock);
    ++pFrame->mRefCnt;
}

void ISEChannel::decreaseBufRef(unsigned int nBufId)
{
    releaseRecordingFrame(nBufId);
}

void ISEChannel::NotifyRenderStart()
{
    mpCallbackNotifier->NotifyRenderStart();
}

void ISEChannel::process()
{
    bool bRunningFlag = true;
    //bool bTakePictureStart = false;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
PROCESS_MESSAGE:
    getMsgRet = mpCapProcess->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoCaptureProcess::MsgTypeCapture_TakePicture == msg.mMsgType)
        {
            if(!mbProcessTakePictureStart)
            {
                mbProcessTakePictureStart = true;
            }
            else
            {
                alogd("Be careful! take picture is doing already!");
            }
            alogd("ISE take picture mode is [0x%x]", mTakePictureMode);
            //set picture number to callbackNotifier
            int nPicNum = 0;
            switch(mTakePictureMode)
            {
                case TAKE_PICTURE_MODE_FAST:
                    nPicNum = 1;
                    break;
                case TAKE_PICTURE_MODE_CONTINUOUS:
                    nPicNum = mParameters.getContinuousPictureNumber();
                    break;
                default:
                    nPicNum = 1;
                    break;
            }
            mpCallbackNotifier->setPictureNum(nPicNum);
        }
        else if(DoCaptureProcess::MsgTypeCapture_CancelContinuousPicture == msg.mMsgType)
        {
            if(mbProcessTakePictureStart)
            {
                if(TAKE_PICTURE_MODE_CONTINUOUS == mTakePictureMode)
                {
                    mpPicThread->notifyPictureEnd();
                    mContinuousPictureStartTime = 0;
                    mContinuousPictureLast = 0;
                    mContinuousPictureInterval = 0;
                    mContinuousPictureCnt = 0;
                    mContinuousPictureMax = 0;
                    mbProcessTakePictureStart = false;
                }
                else
                {
                    aloge("fatal error! take picture mode[0x%x] is not continuous!", mTakePictureMode);
                }
            }
            else
            {
                aloge("fatal error! not start take picture, mode[0x%x]!", mTakePictureMode);
            }
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }
    
    //VI_FRAME_BUF_INFO_S buffer;
    VIDEO_FRAME_INFO_S stVideoFrameInfo;
    ret = AW_MPI_ISE_GetData(mGroupId, mChannelId, &stVideoFrameInfo, 200);
    if (ret != SUCCESS) 
    {
        aloge("AW_MPI_ISE_GetData error!");
        return;
    }
    //VIDEO_FRAME_BUFFER_S *pFrmbuf = mpVideoFrmBuffer + buffer.u32PoolId;
    VIDEO_FRAME_BUFFER_S *pFrmbuf = NULL;
    mFrameBuffersLock.lock();
    for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mFrameBuffers.begin(); it!=mFrameBuffers.end(); ++it)
    {
        if(it->mFrameBuf.mId == stVideoFrameInfo.mId)
        {
            if(NULL == pFrmbuf)
            {
                pFrmbuf = &*it;
            }
            else
            {
                aloge("fatal error! MPP-ISE frame id[0x%x] is repeated!", stVideoFrameInfo.mId);
            }
        }
    }
    if(NULL == pFrmbuf)
    {
        alogd("ISEChannel[%d][%d] frame buffer array did not contain this bufferId[0x%x], add it.", mGroupId, mChannelId, stVideoFrameInfo.mId);
        VIDEO_FRAME_BUFFER_S newFrame;
        memset(&newFrame, 0, sizeof(VIDEO_FRAME_BUFFER_S));
        mFrameBuffers.push_back(newFrame);
        pFrmbuf = &mFrameBuffers.back();
    }
    pFrmbuf->mFrameBuf = stVideoFrameInfo;
    if(mRectCrop.Width != 0 && mRectCrop.Height != 0)
    {
        pFrmbuf->mFrameBuf.VFrame.mOffsetTop = mRectCrop.Y;
        pFrmbuf->mFrameBuf.VFrame.mOffsetBottom = mRectCrop.Y + mRectCrop.Height;
        pFrmbuf->mFrameBuf.VFrame.mOffsetLeft = mRectCrop.X;
        pFrmbuf->mFrameBuf.VFrame.mOffsetRight = mRectCrop.X + mRectCrop.Width;
    }
    pFrmbuf->mColorSpace = mColorSpace;
    //pFrmbuf->mIsThumbAvailable = 0;
    //pFrmbuf->mThumbUsedForPhoto = 0;
    pFrmbuf->mRefCnt = 1;
    mFrameBuffersLock.unlock();
    
    if(false == mbProcessTakePictureStart)
    {
        {
            AutoMutex lock(mRefCntLock);
            pFrmbuf->mRefCnt++;
        }
        OSAL_Queue(mpPreviewQueue, pFrmbuf);
        mpPrevThread->notifyNewFrameCome();
    }
    else
    {
        if (mTakePictureMode == TAKE_PICTURE_MODE_NORMAL)
        {
           aloge("fatal error! ISE can't support normal mode take picture !");
           mpPicThread->notifyPictureEnd();
           mbProcessTakePictureStart = false;
        } 
        else 
        {
            mRefCntLock.lock();
            pFrmbuf->mRefCnt++;
            mRefCntLock.unlock();
            OSAL_Queue(mpPreviewQueue, pFrmbuf);
            mpPrevThread->notifyNewFrameCome();

            if (mTakePictureMode == TAKE_PICTURE_MODE_FAST)
            {
                mRefCntLock.lock();
                pFrmbuf->mRefCnt++;
                mRefCntLock.unlock();
                mIsPicCopy = false;
                OSAL_Queue(mpPictureQueue, pFrmbuf);
                mpPicThread->notifyNewFrameCome();
                mpPicThread->notifyPictureEnd();
                mbProcessTakePictureStart = false;
            }
            else if(mTakePictureMode == TAKE_PICTURE_MODE_CONTINUOUS)
            {
                bool bPermit = false;
                if (0 == mContinuousPictureStartTime)    //let's begin!
                {
                    mContinuousPictureStartTime = CDX_GetSysTimeUsMonotonic()/1000;
                    mContinuousPictureLast = mContinuousPictureStartTime;
                    mContinuousPictureInterval = mParameters.getContinuousPictureIntervalMs();
                    mContinuousPictureCnt = 0;
                    mContinuousPictureMax = mParameters.getContinuousPictureNumber();
                    bPermit = true;
                    alogd("begin continous picture, will take [%d]pics, interval[%llu]ms!", mContinuousPictureMax, mContinuousPictureInterval);
                }
                else
                {
                    if(mContinuousPictureInterval <= 0)
                    {
                        bPermit = true;
                    }
                    else
                    {
                        uint64_t nCurTime = CDX_GetSysTimeUsMonotonic()/1000;
                        if(nCurTime > mContinuousPictureLast + mContinuousPictureInterval)
                        {
                            bPermit = true;
                        }
                    }
                }

                if(bPermit)
                {
                    mRefCntLock.lock();
                    pFrmbuf->mRefCnt++;
                    mRefCntLock.unlock();
                    mIsPicCopy = false;
                    OSAL_Queue(mpPictureQueue, pFrmbuf);
                    mpPicThread->notifyNewFrameCome();
                    mContinuousPictureCnt++;
                    if(mContinuousPictureCnt >= mContinuousPictureMax)
                    {
                        mpPicThread->notifyPictureEnd();
                        mContinuousPictureStartTime = 0;
                        mContinuousPictureLast = 0;
                        mContinuousPictureInterval = 0;
                        mContinuousPictureCnt = 0;
                        mContinuousPictureMax = 0;
                        mbProcessTakePictureStart = false;
                    }
                    else
                    {
                        mContinuousPictureLast = mContinuousPictureStartTime+mContinuousPictureCnt*mContinuousPictureInterval;
                    }
                }
            }
            else
            {
                aloge("fatal error! any other take picture mode[0x%x]?", mTakePictureMode);
                mbProcessTakePictureStart = false;
            }
        }
    }

    mFrameCounter++;

    releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
}

bool ISEChannel::previewThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    PROCESS_MESSAGE:
    getMsgRet = mpPrevThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoPreviewThread::MsgTypePreview_InputFrameAvailable == msg.mMsgType)
        {
        }
        else if(DoPreviewThread::MsgTypePreview_releaseAllFrames == msg.mMsgType)
        {
            AutoMutex lock(mpPrevThread->mWaitLock);
            for(;;)
            {
                pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPreviewQueue);
                if(pFrmbuf)
                {
                    releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
                }
                else
                {
                    break;
                }
            }
            if(mpPrevThread->mbWaitReleaseAllFrames)
            {
                mpPrevThread->mbWaitReleaseAllFrames = false;
            }
            else
            {
                aloge("fatal error! check code!");
            }
            mpPrevThread->mCondReleaseAllFramesFinished.signal();
        }
        else if(DoPreviewThread::MsgTypePreview_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }
    
    pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPreviewQueue);
    if (pFrmbuf == NULL) 
    {
        {
            AutoMutex lock(mpPrevThread->mWaitLock);
            if(OSAL_GetElemNum(mpPreviewQueue) > 0)
            {
                alogd("Low probability! ISE preview new frame come before check again.");
                goto PROCESS_MESSAGE;
            }
            else
            {
                mpPrevThread->mbWaitPreviewFrame = true;
            }
        }
        mpPrevThread->mMsgQueue.waitMessage();
        goto PROCESS_MESSAGE;
    }

    mpCallbackNotifier->onNextFrameAvailable(pFrmbuf);
    mpPreviewWindow->onNextFrameAvailable(pFrmbuf);

    releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
    //return true;
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

bool ISEChannel::pictureThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    bool bDrainPictureQueue = false;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    PROCESS_MESSAGE:
    getMsgRet = mpPicThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoPictureThread::MsgTypePicture_InputFrameAvailable == msg.mMsgType)
        {
        }
        else if(DoPictureThread::MsgTypePicture_SendPictureEnd == msg.mMsgType)
        {
            bDrainPictureQueue = true;
        }
        else if(DoPictureThread::MsgTypePicture_releaseAllFrames == msg.mMsgType)
        {
            AutoMutex lock(mpPicThread->mWaitLock);
            for(;;)
            {
                pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPictureQueue);
                if(pFrmbuf)
                {
                    releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
                }
                else
                {
                    break;
                }
            }
            if(mpPicThread->mbWaitReleaseAllFrames)
            {
                mpPicThread->mbWaitReleaseAllFrames = false;
            }
            else
            {
                aloge("fatal error! check code!");
            }
            mpPicThread->mCondReleaseAllFramesFinished.signal();
        }
        else if(DoPictureThread::MsgTypePicture_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }
    while(1)
    {
        pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPictureQueue);
        if (pFrmbuf == NULL) 
        {
            if(bDrainPictureQueue)
            {
                mpCallbackNotifier->disableMessage(mTakePicMsgType);
                bDrainPictureQueue = false;
                mpCallbackNotifier->takePictureEnd();
                mLock.lock();
                if(!mbTakePictureStart)
                {
                    aloge("fatal error! why ISE takePictureStart is false when we finish take picture?");
                }
                mbTakePictureStart = false;
                mLock.unlock();
            }
            {
                AutoMutex lock(mpPicThread->mWaitLock);
                if(OSAL_GetElemNum(mpPictureQueue) > 0)
                {
                    alogd("Low probability! ISE picture new frame come before check again.");
                    goto PROCESS_MESSAGE;
                }
                else
                {
                    mpPicThread->mbWaitPictureFrame = true;
                }
            }
            mpPicThread->mMsgQueue.waitMessage();
            goto PROCESS_MESSAGE;
        }

        mpCallbackNotifier->takePicture(pFrmbuf);

        releaseRecordingFrame(pFrmbuf->mFrameBuf.mId);
        if(!bDrainPictureQueue)
        {
            break;
        }
    }
    //return true;
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

bool ISEChannel::commandThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    PROCESS_MESSAGE:
    getMsgRet = mpCommandThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoCommandThread::MsgTypeCommand_TakePicture == msg.mMsgType)
        {
            doTakePicture(msg.mPara0);
        }
        else if(DoCommandThread::MsgTypeCommand_CancelContinuousPicture == msg.mMsgType)
        {
            doCancelContinuousPicture();
        }
        else if(DoCommandThread::MsgTypeCommand_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
    }
    else
    {
        mpCommandThread->mMsgQueue.waitMessage();
    }
    //return true;
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

}; /* namespace EyeseeLinux */

