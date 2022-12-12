/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : ISEDevice.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/08
  Last Modified :
  Description   : ISE(Image Stitching Engine) module.
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_ISE_DEVICE_H__
#define __IPCLINUX_ISE_DEVICE_H__

#include <vector>

#include <plat_type.h>
#include <plat_errno.h>
#include <plat_defines.h>
#include <plat_math.h>
#include <Errors.h>

#include <EyeseeISECommon.h>
#include <camera.h>
#include <CameraParameters.h>
#include <CameraListener.h>

#include "ISEChannel.h"


namespace EyeseeLinux {

typedef struct ISEChannelInfo
{
    ISE_CHN mChnId;
    ISEChannel *mpChannel;
} ISEChannelInfo;

class ISEDevice
{
public:
    ISEDevice(unsigned int nISEId);
    ~ISEDevice();

    status_t prepare(ISE_MODE_E mode,PIXEL_FORMAT_E pixelformat);
    status_t release();
    status_t start();
    status_t stop();
    status_t reset();
    /**
     * open a mpp-ise channel. Must call after prepare().
     *
     * @return chnId>=0 if success. chnId=-1 indicate fail.
     * @param pChnAttr ise scaler channel attributes
     */
    ISE_CHN openChannel(ISE_CHN_ATTR_S *pChnAttr);
    status_t closeChannel(ISE_CHN chnId);
    status_t startChannel(ISE_CHN chnId);
    status_t setParameters(int chnId, CameraParameters &param);
    status_t getParameters(int chnId, CameraParameters &param);
    status_t setChannelDisplay(int chnId, int hlay);
    bool previewEnabled(int chnId);
    status_t startRender(int chnId);
    status_t stopRender(int chnId);

    status_t setMoPortAttr(ISE_CHN_ATTR_S *pAttr);

    void releaseRecordingFrame(int chnId, uint32_t index);
    status_t startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId);
    status_t stopRecording(int chnId, int recorderId);
    void setDataListener(int chnId, DataListener *pCb);
    void setNotifyListener(int chnId, NotifyListener *pCb);

    status_t takePicture(int chnId, unsigned int msgType, PictureRegionCallback *pPicReg);
    status_t notifyPictureRelease(int chnId);
    status_t cancelContinuousPicture(int chnId);
    //void postDataCompleted(int chnId, const void *pData, int size);

    status_t setCamera(std::vector<CameraChannelInfo>& cameraChannels);
    void dataCallbackTimestamp(int nCameraIndex, const VIDEO_FRAME_BUFFER_S *pCameraFrameInfo);
private:
    status_t release_l();
    status_t stop_l();
protected:
    class DoSendPicThread : public Thread
    {
    public:
        DoSendPicThread(ISEDevice *pIseDev);
        ~DoSendPicThread();
        status_t SendPicThreadStart();
        status_t SendPicThreadPause();
        status_t SendPicThreadReset();
        status_t SendMessage_InputFrameReady();
        
    private:
        friend class ISEDevice;
        Mutex mLock;
        Mutex mStateLock;
        Condition mLoadedCompleteCond;
        Condition mPauseCompleteCond;
        Condition mStartCompleteCond;
        // Values for SendPicThread message type
        enum SendPicMsgType
        {
            MsgTypeSendPic_SetState = 0x100,
            MsgTypeSendPic_InputFrameAvailable,
            MsgTypeSendPic_Exit,
        };
        EyeseeMessageQueue mSendPicMsgQueue;
        bool mbWaitInputFrameBuf;
        enum ISE_SENDPIC_STATE_E
        {
            ISE_SENDPIC_STATE_LOADED,
            /* Do not send frame to ISE. */
            ISE_SENDPIC_STATE_PAUSED,
            /* Start send frame. */
            ISE_SENDPIC_STATE_STARTED,
        };
        ISE_SENDPIC_STATE_E mSendPicThreadState;
        virtual bool threadLoop();
        /**
         * send mCameraProxies's frame to MPP-ISE.
         *
         * @return true if want thread continue. false if want to quit thread.
         */
        bool sendPicThread();
        bool mbThreadOK;
        ISEDevice* const mpIseDev;
    };
    class DoReturnPicThread : public Thread
    {
    public:
        DoReturnPicThread(ISEDevice *pIseDev);
        ~DoReturnPicThread();
        status_t Start();
        status_t Reset();
        status_t notifyReturnPictureCome();
        status_t SendCommand_ReturnAllFrames();
        
    private:
        Mutex mLock;
        Mutex mStateLock;
        Condition mStateCond;
        // Values for ReturnPicThread message type
        enum ReturnPicMsgType
        {
            MsgTypeReturnPic_SetState = 0x200,
            MsgTypeReturnPic_InputFrameAvailable,
            MsgTypeReturnPic_ReturnAllFrames,
            MsgTypeReturnPic_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        Mutex mInputFrameLock;
        bool mbWaitInputFrameBuf;
        enum ISE_RETURNPIC_STATE_E
        {
            ISE_RETURNPIC_STATE_LOADED,
            /* Start return frame. */
            ISE_RETURNPIC_STATE_STARTED,
        };
        ISE_RETURNPIC_STATE_E mState;
        
        virtual bool threadLoop();
        status_t returnAllFrames();
        /**
         * return mCameraProxies's frame to camera.
         *
         * @return true if want thread continue. false if want to quit thread.
         */
        bool returnPicThread();
        bool mbThreadOK;
        
        ISEDevice* const mpIseDev;
    };
    class DoCaptureThread : public Thread
    {
    public:
        DoCaptureThread(ISEDevice *pIseDev);
        ~DoCaptureThread();
        /**
         * paused -> started
         */
        status_t CaptureThreadStart();
        /**
         * started -> paused
         */
        status_t CaptureThreadPause();
        /**
         * all -> loaded
         */
        status_t CaptureThreadReset();
    private:
        Mutex mLock;
        Mutex mStateLock;
        Condition mLoadedCompleteCond;
        Condition mPauseCompleteCond;
        Condition mStartCompleteCond;
        enum CaptureMsgType
        {
            MsgTypeCapture_SetState = 0x300,
            MsgTypeCapture_Exit,
        };
        EyeseeMessageQueue mCaptureMsgQueue;
        enum ISE_CAPTURE_STATE_E
        {
            /* thread unexist. */
            ISE_CAPTURE_STATE_LOADED,
            /* Do not capture frame. */
            ISE_CAPTURE_STATE_PAUSED,
            /* Start capture frame. */
            ISE_CAPTURE_STATE_STARTED,
        };
        ISE_CAPTURE_STATE_E mCapThreadState;
        virtual bool threadLoop();
        bool captureThread();
        bool mbThreadOK;
        ISEDevice* const mpIseDev;
    };

private:
    class CameraProxyListener: public CameraRecordingProxyListener  //I set to camera, camera call.
    {
    public:
        CameraProxyListener(ISEDevice *pIse, int nCameraIndex);
        virtual void dataCallbackTimestamp(const void *pdata);

    private:
        ISEDevice * mpIse;
        int mCameraIndex;
    };
    //camera input
    class CameraProxyInfo
    {
    public:
        CameraRecordingProxy *mpCameraProxy;    // for return to camera
        int mnCameraChannel;
        std::list<VIDEO_FRAME_BUFFER_S> mIdleFrameBufList;
        std::list<VIDEO_FRAME_BUFFER_S> mReadyFrameBufList;
        std::list<VIDEO_FRAME_BUFFER_S> mUsedFrameBufList;
        std::list<VIDEO_FRAME_BUFFER_S> mWaitReleaseFrameBufList;
        std::list<VIDEO_FRAME_BUFFER_S> mReleasingFrameBufList; //only one element in it.
        Mutex mFrameBufListLock;
        status_t SetCameraChannelInfo(CameraChannelInfo *pCameraChnInfo);
        status_t addReleaseFrame(VIDEO_FRAME_INFO_S *pVIFrame);
        CameraProxyInfo();
        CameraProxyInfo(const CameraProxyInfo& ref);
        ~CameraProxyInfo();
    };
    std::vector<CameraProxyInfo> mCameraProxies;
    enum v4l2_colorspace mColorSpace;
    void notify(MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
    static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
    ISEChannel *searchISEChannel(int chnId);

    //int mDeviceId;
    const unsigned int mISEId;
    
    enum ISEDeviceState 
    {
        /* Object has been constructed. */
        ISE_STATE_CONSTRUCTED = 0,
        /* Object was prepare to start. */
        ISE_STATE_PREPARED,
        /* VI channel has been started. */
        ISE_STATE_STARTED,
    };
    ISEDeviceState mDevState;

    DoSendPicThread *mpSendPicThread;
    DoReturnPicThread *mpReturnPicThread;
    DoCaptureThread *mpCapThread;
    std::vector<ISEChannelInfo> mISEChannelVector;
    Mutex mISEChannelVectorLock;
    
    Mutex mLock;

    //MPP components
    //MPP_CHN_S mISEGroup;
    ISE_GRP mGroupId;
    ISE_GROUP_ATTR_S mIseAttr;
}; /* class ISEDevice */

}; /* namespace EyeseeLinux */

#endif /* __IPCLINUX_ISE_DEVICE_H__ */

