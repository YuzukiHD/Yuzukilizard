#ifndef __IPCLINUX_EIS_CHANNEL_H__
#define __IPCLINUX_EIS_CHANNEL_H__


#include <vector>

#include <plat_type.h>
#include <plat_errno.h>
#include <plat_defines.h>
#include <plat_math.h>
#include <Errors.h>
#include <memoryAdapter.h>
#include <sc_interface.h>

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <utils/EyeseeQueue.h>
#include <OSAL_Mutex.h>
#include <OSAL_Queue.h>

#include <camera.h>
#include <type_camera.h>
#include <CameraParameters.h>
#include <CameraListener.h>
#include "../camera/CallbackNotifier.h"
#include "../camera/PreviewWindow.h"

#include <mm_comm_eis.h>

namespace EyeseeLinux{

#define EIS_MAX_BUFFER_NUM  (40)

enum EISChannelState {
	/* Object has been constructed. */
	EIS_CHN_STATE_CONSTRUCTED = 0,
	/* Object was prepared. */
	EIS_CHN_STATE_PREPARED,
	/* EIS channel has been started. */
	EIS_CHN_STATE_STARTED,
};

class EISChannel : public CameraBufferReference
{
public:
    EISChannel(ISE_GRP nGroupId, EIS_ATTR_S *pChnAttr);
    ~EISChannel(); 
	
    EIS_CHN getChannelId();
    status_t setParameters(CameraParameters &param);
    status_t getParameters(CameraParameters &param);
	
    status_t setChannelDisplay(int hlay);
    bool isPreviewEnabled();

	status_t startRender();
    status_t stopRender();
	
    status_t startChannel();
    status_t stopChannel();

    status_t releaseAllEISFrames();
    status_t releaseRecordingFrame(uint32_t index);
	
    status_t startRecording(CameraRecordingProxyListener *pCb, int recorderId);
    status_t stopRecording(int recorderId);
	
    void setDataListener(DataListener *pCb);
    void setNotifyListener(NotifyListener *pCb);

    status_t takePicture(unsigned int msgType, PictureRegionCallback *pPicReg); //msgType:CAMERA_MSG_COMPRESSED_IMAGE
    status_t notifyPictureRelease();
    status_t cancelContinuousPicture();

    void increaseBufRef(VIDEO_FRAME_BUFFER_S *pBuf);
    void decreaseBufRef(unsigned int nBufId);
    void NotifyRenderStart();

    void process();

    bool previewThread();		// will be called in thread
    bool pictureThread();
    bool commandThread();

protected:
    class DoCaptureProcess
    {
    public:
        DoCaptureProcess(EISChannel *pEisChn);
        status_t SendCommand_TakePicture();
        status_t SendCommand_CancelContinuousPicture();
    private:
        friend class EISChannel;
        enum CaptureMsgType
        {
            MsgTypeCapture_TakePicture = 0x100,
            MsgTypeCapture_CancelContinuousPicture,
        };
        EyeseeMessageQueue mMsgQueue;
        EISChannel* const mpEisChn;
    };
	
    class DoPreviewThread : public Thread
    {
    public:
        DoPreviewThread(EISChannel *pEisChn);
        status_t startThread();
        void stopThread();
        status_t notifyNewFrameCome();
        status_t releaseAllFrames();
        virtual bool threadLoop();
    private:
        friend class EISChannel;
        enum PreviewMsgType
        {
            MsgTypePreview_InputFrameAvailable = 0x200,
            MsgTypePreview_releaseAllFrames,
            MsgTypePreview_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        Mutex mWaitLock;
        Condition mCondReleaseAllFramesFinished;
        bool mbWaitReleaseAllFrames;
        bool mbWaitPreviewFrame;
        EISChannel* const mpEisChn;
    };

    class DoPictureThread : public Thread
    {
    public:
        DoPictureThread(EISChannel *pEisChn);
        status_t startThread();
        void stopThread();
        status_t notifyNewFrameCome();
        status_t notifyPictureEnd();
        status_t releaseAllFrames();
        virtual bool threadLoop();
    private:
        friend class EISChannel;
        enum PictureMsgType
        {
            MsgTypePicture_InputFrameAvailable = 0x300,
            MsgTypePicture_SendPictureEnd,
            MsgTypePicture_releaseAllFrames,
            MsgTypePicture_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        Mutex mWaitLock;
        Condition mCondReleaseAllFramesFinished;
        bool mbWaitReleaseAllFrames;
        bool mbWaitPictureFrame;
        EISChannel* const mpEisChn;
    }; 
	
    class DoCommandThread : public Thread
    {
    public:
        DoCommandThread(EISChannel *pEisChn);
        status_t startThread();
        void stopThread();
        status_t SendCommand_TakePicture(unsigned int msgType); //msgType:CAMERA_MSG_COMPRESSED_IMAGE
        status_t SendCommand_CancelContinuousPicture();
        bool threadLoop();
    private:
        friend class EISChannel;
        enum CommandMsgType
        {
            MsgTypeCommand_TakePicture = 0x400,
            MsgTypeCommand_CancelContinuousPicture,
            MsgTypeCommand_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        EISChannel* const mpEisChn;
    };


private:
    status_t doTakePicture(unsigned int msgType);   //msgType:CAMERA_MSG_COMPRESSED_IMAGE
    status_t doCancelContinuousPicture();

    status_t GetChnOutFrameSize(EIS_CHN chnId, EIS_ATTR_S *pAttr, int &mFrameWidth, int &mFrameHeight); 

    DoCaptureProcess *mpCapProcess;
    DoCommandThread *mpCommandThread;


    DoPreviewThread *mpPrevThread; 
    Mutex mPrevThreadLock;
    Condition mPrevThreadCond; 
	
    DoPictureThread *mpPicThread;
    Mutex mPicThreadLock;
    Condition mPicThreadCond; 

    CallbackNotifier *mpCallbackNotifier; 
	
    PreviewWindow *mpPreviewWindow;
    bool mbPreviewEnable; 
	
    OSAL_QUEUE *mpPreviewQueue;
    OSAL_QUEUE *mpPictureQueue;

	
    CameraParameters mParameters; 
	
    int mNewZoom, mLastZoom, mMaxZoom;
    RECT_S mRectCrop;

	
    std::list<VIDEO_FRAME_BUFFER_S> mFrameBuffers;
    Mutex mFrameBuffersLock;
    VIDEO_FRAME_BUFFER_S mPicFrmBuffer; 

    struct ScMemOpsS *mpMemOps; 
    void* mpVirAddr;
    unsigned int mPhyAddr; 
	
    EISChannelState mChannelState;
    Mutex mLock;

    Mutex mRefCntLock;
	
    bool mIsPicCopy; 
	
    unsigned int mTakePicMsgType;   //CAMERA_MSG_COMPRESSED_IMAGE
    TAKE_PICTURE_MODE_E mTakePictureMode;
    bool mbTakePictureStart;  //indicate start to take picture;
    bool mbProcessTakePictureStart;  //internal used by process
    int mContinuousPictureCnt;
    int mContinuousPictureMax;
    uint64_t mContinuousPictureStartTime;
    uint64_t mContinuousPictureLast;
    uint64_t mContinuousPictureInterval;    //ms

    int mColorSpace; 
	
    int mFrameWidth, mFrameHeight;  //capture picture size.
    unsigned int mFrameCounter;
	
    EIS_GRP mGroupId; 
    EIS_CHN mChannelId;
    EIS_ATTR_S mAttr;
};



};


#endif
