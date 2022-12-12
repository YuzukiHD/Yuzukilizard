/******************************************************************************
  Copyright (C), 2001-2019, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : UvcChannel.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2018/12/13
  Last Modified :
  Description   : Uvc channel, get frame from mpi_uvc, or mpi_vdec's VdecFrameManager.
  Function List :
  History       :
******************************************************************************/
#ifndef _UVCCHANNEL_H_
#define _UVCCHANNEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include <string>
#include <list>

#include <mm_comm_vi.h>
#include <mm_common.h>
#include <sc_interface.h>

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <utils/EyeseeQueue.h>

#include <camera.h>
#include <type_camera.h>
#include <CameraParameters.h>
#include <CameraListener.h>

#include "CallbackNotifier.h"
#include "PreviewWindow.h"
#include <UvcChnDefine.h>

namespace EyeseeLinux {

class CallbackNotifier;
class VdecFrameManager;

class UvcChannel : public CameraBufferReference
{
public:
    UvcChannel(std::string uvcName, int nCameraId, UvcChn chnId, VdecFrameManager *pManager);
    ~UvcChannel();
    
    bool commandThread();
    enum class State
    {
        /* Object has been constructed. */
        IDLE = 0,
        /* Object was prepare to start. */
        PREPARED,
        /* VI channel has been started. */
        STARTED,
    };
    status_t prepare();
    status_t release();
    status_t startChannel();
    status_t stopChannel();
    State getState();

    UvcChn getChannelId();
    bool isPreviewEnabled();
    status_t startRender();
    status_t stopRender();
    status_t releaseFrame(uint32_t index);
    
    status_t startRecording(CameraRecordingProxyListener *pCb, int recorderId);
    status_t stopRecording(int recorderId);
    void setDataListener(DataListener *pCb);
    void setNotifyListener(NotifyListener *pCb);
    status_t setParameters(CameraParameters &param);
    status_t getParameters(CameraParameters &param) const;
    status_t takePicture(unsigned int msgType, PictureRegionCallback *pPicReg); //msgType:CAMERA_MSG_COMPRESSED_IMAGE
    status_t cancelContinuousPicture();
    status_t setPreviewDisplay(int hlay);
    static void calculateCrop(RECT_S *rect, int zoom, int width, int height);

    virtual void increaseBufRef(VIDEO_FRAME_BUFFER_S *pBuf) override;
    virtual void decreaseBufRef(unsigned int nBufId) override;
    virtual void NotifyRenderStart() override;

private:
    status_t doTakePicture(unsigned int msgType);   //msgType:CAMERA_MSG_COMPRESSED_IMAGE
    status_t TakePictureConfig(unsigned int msgType);   
    status_t doCancelContinuousPicture();

protected:
    class DoCaptureThread : public Thread
    {
    public:
        DoCaptureThread(UvcChannel *pViChn);
        ~DoCaptureThread();
        status_t startCapture();
        status_t pauseCapture();
        status_t SendCommand_TakePicture();
        status_t SendCommand_CancelContinuousPicture();
        virtual bool threadLoop();
    private:
        bool captureThread();
        Mutex mStateLock;
        Condition mPauseCompleteCond;
        Condition mStartCompleteCond;
        enum class MsgType
        {
            SetState = 0x100,
            TakePicture,
            CancelContinuousPicture,
            Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        enum class State
        {
            /* Do not capture frame. */
            IDLE,
            /* Do not capture frame. */
            PAUSED,
            /* Start capture frame. */
            STARTED,
            /* exit thread*/
            //CAPTURE_STATE_EXIT,
        } ;
        State mCapThreadState;
        UvcChannel* const mpUvcChn;
    };

    class DoPreviewThread : public Thread
    {
    public:
        DoPreviewThread(UvcChannel *pUvcChn);
        ~DoPreviewThread();
        status_t startThread();
        void stopThread();
        status_t notifyNewFrameCome();
        status_t releaseAllFrames();
        virtual bool threadLoop();
    private:
        bool previewThread();
        enum class MsgType
        {
            InputFrameAvailable = 0x200,
            releaseAllFrames,
            Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        Mutex mWaitLock;
        Condition mCondReleaseAllFramesFinished;
        bool mbWaitReleaseAllFrames;
        bool mbWaitPreviewFrame;
        UvcChannel* const mpUvcChn;
    };

    class DoPictureThread : public Thread
    {
    public:
        DoPictureThread(UvcChannel *pViChn);
        ~DoPictureThread();
        status_t startThread();
        void stopThread();
        status_t notifyNewFrameCome();
        status_t notifyPictureEnd();
        status_t releaseAllFrames();
        virtual bool threadLoop();
    private:
        bool pictureThread();
        enum class MsgType
        {
            InputFrameAvailable = 0x300,
            SendPictureEnd,
            releaseAllFrames,
            Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        Mutex mWaitLock;
        Condition mCondReleaseAllFramesFinished;
        bool mbWaitReleaseAllFrames;
        bool mbWaitPictureFrame;
        UvcChannel* const mpUvcChn;
    };

    class DoCommandThread : public Thread
    {
    public:
        DoCommandThread(UvcChannel *pViChn);
        ~DoCommandThread();
        status_t startThread();
        void stopThread();
        bool threadLoop();
        status_t SendCommand_TakePicture(unsigned int msgType); //msgType:CAMERA_MSG_COMPRESSED_IMAGE
        status_t SendCommand_CancelContinuousPicture();
    private:
        bool commandThread();
        enum class MsgType
        {
            TakePicture = 0x10,
            CancelContinuousPicture,
            Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        UvcChannel* const mpUvcChn;
    };

private:
    const std::string mUvcDev;
    const int mCameraId;
    const UvcChn mChnId;   //UvcMainChn
    UVC_CHN mVirtualUvcChn; //from mpi_uvc, when get frame from uvc capture.
    VdecFrameManager * const mpVdecFrameManager;   //from vdec, when get frame for vdec output.
    

    DoCaptureThread *mpCapThread;

    DoPreviewThread *mpPrevThread;

    DoPictureThread *mpPicThread;

    DoCommandThread *mpCommandThread;

    EyeseeQueue *mpPreviewQueue;
    EyeseeQueue *mpPictureQueue;

    Mutex mRefCntLock;
    Mutex mLock;

    int mNewZoom, mLastZoom, mMaxZoom;
    RECT_S mRectCrop;   //after soft zoom, show_area in picBuf. extend show_area to whole screen to show zoom effect.
                        //and encode this show_area too.

    std::list<VIDEO_FRAME_BUFFER_S> mFrameBuffers;
    std::list<unsigned int> mFrameBufferIdList;
    Mutex mFrameBuffersLock;

    CallbackNotifier *mpCallbackNotifier;
    PreviewWindow *mpPreviewWindow;
    bool mbPreviewEnable;

    State mChannelState;

    unsigned int mTakePicMsgType;   //CAMERA_MSG_COMPRESSED_IMAGE
    TAKE_PICTURE_MODE_E mTakePictureMode;
    bool mbTakePictureStart;  //indicate start to take picture;
    
    //for TAKE_PICTURE_MODE_CONTINUOUS
    int mContinuousPictureCounter;
    int mContinuousPictureMax;
    uint64_t mContinuousPictureStartTime;   //unit:ms, start continuousPicture time.
    uint64_t mContinuousPictureLast;    //unit:ms, next picture time.
    uint64_t mContinuousPictureInterval;   //unit:ms

    CameraParameters mParameters;
    int mColorSpace;    //V4L2_COLORSPACE_JPEG
    int mFrameWidth, mFrameHeight;  //capture picture size(not frame buf size). get from device!
    int mFrameWidthOut, mFrameHeightOut;    //mpi_vi output picture size(not frame buf size). In common, frameWidthOut == frameWidth, but in anti-shake mode, frameWidthOut < frameWidth. It is buffer size, not content size.

    unsigned int mFrameCounter;

    unsigned int mCamDevTimeoutCnt;
};

}; /* namespace EyeseeLinux */

#endif  /* _UVCCHANNEL_H_ */

