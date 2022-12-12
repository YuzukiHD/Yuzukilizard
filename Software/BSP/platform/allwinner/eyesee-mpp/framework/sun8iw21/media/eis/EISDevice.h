#ifndef __IPCLINUX_EIS_DEVICE_H__
#define __IPCLINUX_EIS_DEVICE_H__

#include <vector>

#include <plat_type.h>
#include <plat_errno.h>
#include <plat_defines.h>
#include <plat_math.h>
#include <Errors.h>

#include <EyeseeEISCommon.h>
#include <camera.h>
#include <CameraParameters.h>
#include <CameraListener.h>

#include "EISChannel.h"


namespace EyeseeLinux {

typedef struct EISChannelInfo_s
{
	EIS_CHN mChnId;
	EISChannel *mpChannel;
} EISChannelInfo;

class EISDevice{
public:
    EISDevice(unsigned int nEISId);
    ~EISDevice(); 
	
    status_t prepare(EIS_ATTR_S *pEisChnAttr);
    status_t release();
    status_t start();
    status_t stop();
    status_t reset(); 
	
    EIS_CHN openChannel(EIS_ATTR_S *pChnAttr);
    status_t closeChannel(EIS_CHN chnId);
    status_t startChannel(EIS_CHN chnId); 
	
    status_t setParameters(int chnId, CameraParameters &param);
    status_t getParameters(int chnId, CameraParameters &param); 
	
    status_t setChannelDisplay(int chnId, int hlay); 
    bool previewEnabled(int chnId);
	
    status_t startRender(int chnId);
    status_t stopRender(int chnId); 
	
    status_t setEISChnAttr(EIS_ATTR_S *pAttr);			// key point    samuel.zhou

    void releaseRecordingFrame(int chnId, uint32_t index);
    status_t startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId);
    status_t stopRecording(int chnId, int recorderId); 

    void setDataListener(int chnId, DataListener *pCb);
    void setNotifyListener(int chnId, NotifyListener *pCb);

    status_t setCamera(std::vector<EIS_CameraChannelInfo>& cameraChannels);
	
    status_t takePicture(int chnId, unsigned int msgType, PictureRegionCallback *pPicReg);
    status_t notifyPictureRelease(int chnId);
    status_t cancelContinuousPicture(int chnId); 
	
    void dataCallbackTimestamp(int nCameraIndex, const VIDEO_FRAME_BUFFER_S *pCameraFrameInfo); 

	
private: 
	status_t release_l();
	status_t stop_l();

protected: 
    class DoSendPicThread : public Thread
    {
    public:
        DoSendPicThread(EISDevice *pEisDev);
        ~DoSendPicThread();
		
        status_t SendPicThreadStart();
        status_t SendPicThreadPause();
        status_t SendPicThreadReset();
        status_t SendMessage_InputFrameReady();
        
    private:
        friend class EISDevice;
		
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
		
        enum EIS_SENDPIC_STATE_E
        {
            EIS_SENDPIC_STATE_LOADED,
            /* Do not send frame to EIS. */
            EIS_SENDPIC_STATE_PAUSED,
            /* Start send frame. */
            EIS_SENDPIC_STATE_STARTED,
        };
        EIS_SENDPIC_STATE_E mSendPicThreadState;
		
        virtual bool threadLoop();
        /**
         * send mCameraProxies's frame to MPP-EIS.
         *
         * @return true if want thread continue. false if want to quit thread.
         */
        bool sendPicThread();
        bool mbThreadOK;
		
        EISDevice* const mpEisDev;
    };

	
    class DoReturnPicThread : public Thread
    {
    public:
        DoReturnPicThread(EISDevice *pEisDev);
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
		
        enum EIS_RETURNPIC_STATE_E
        {
            EIS_RETURNPIC_STATE_LOADED,
            /* Start return frame. */
            EIS_RETURNPIC_STATE_STARTED,
        };
        EIS_RETURNPIC_STATE_E mState;
        
        virtual bool threadLoop();
        status_t returnAllFrames();
        /**
         * return mCameraProxies's frame to camera.
         *
         * @return true if want thread continue. false if want to quit thread.
         */
        bool returnPicThread();
        bool mbThreadOK;
        
        EISDevice* const mpEisDev;
    };
	
    class DoCaptureThread : public Thread
    {
    public:
        DoCaptureThread(EISDevice *pEisDev);
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
		
        enum EIS_CAPTURE_STATE_E
        {
            /* thread unexist. */
            EIS_CAPTURE_STATE_LOADED,
            /* Do not capture frame. */
            EIS_CAPTURE_STATE_PAUSED,
            /* Start capture frame. */
            EIS_CAPTURE_STATE_STARTED,
        };
        EIS_CAPTURE_STATE_E mCapThreadState;
		
        virtual bool threadLoop();
        bool captureThread();
		
        bool mbThreadOK;
        EISDevice* const mpEisDev;
    };

private: 
    class CameraProxyListener: public CameraRecordingProxyListener  //I set to camera, camera call.
    {		// listen to camara
    public:
        CameraProxyListener(EISDevice *pEis, int nCameraIndex);
		
        virtual void dataCallbackTimestamp(const void *pdata);

    private:
        EISDevice * mpEis;
        int mCameraIndex;
    }; 
    class CameraProxyInfo
    {
    public:
        CameraRecordingProxy *mpCameraProxy;    // store set camara proxy
        int mnCameraChannel;
		
        std::list<VIDEO_FRAME_BUFFER_S> mIdleFrameBufList;		// can be fetched to store frm info  samuel.zhou
        std::list<VIDEO_FRAME_BUFFER_S> mReadyFrameBufList;		// can be used frame
        std::list<VIDEO_FRAME_BUFFER_S> mUsedFrameBufList;		// used frm
        std::list<VIDEO_FRAME_BUFFER_S> mWaitReleaseFrameBufList;// waiting to be released frm
        std::list<VIDEO_FRAME_BUFFER_S> mReleasingFrameBufList; //only one element in it.
        
        Mutex mFrameBufListLock;		// need to lock,when want to do proxy operation		samuel.zhou
		
        status_t SetCameraChannelInfo(EIS_CameraChannelInfo *pCameraChnInfo);
        status_t addReleaseFrame(VIDEO_FRAME_INFO_S *pVIFrame);
		
        CameraProxyInfo();
        CameraProxyInfo(const CameraProxyInfo& ref);
        ~CameraProxyInfo();
    };

    std::vector<CameraProxyInfo> mCameraProxies;

    enum v4l2_colorspace mColorSpace;

    void notify(MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
    static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
	
    EISChannel *searchEISChannel(int chnId);
    const unsigned int mEISId;			// key   ???? samuel.zhou

    enum EISDeviceState 
    {
        /* Object has been constructed. */
        EIS_STATE_CONSTRUCTED = 0,
        /* Object was prepare to start. */
        EIS_STATE_PREPARED,
        /* VI channel has been started. */
        EIS_STATE_STARTED,
    };
    EISDeviceState mDevState;

	
    DoSendPicThread *mpSendPicThread;
	// fetch frm node from wait release frm list,move node to releasing frm list ,
	// call proxy release function to release current frm,and move node to idle frm list.	samuel.zhou
    DoReturnPicThread *mpReturnPicThread;
	
    DoCaptureThread *mpCapThread;

//	EISChannelInfo mEISChannel; 
    std::vector<EISChannelInfo> mEISChannelVector;
	
    Mutex mEISChannelVectorLock; 
    Mutex mLock; 
	
    EIS_GRP mGroupId;
	EIS_ATTR_S mEisChnAttr;
};



};


#endif
