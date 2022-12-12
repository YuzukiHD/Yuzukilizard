/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VIChannel.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/01
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_VI_CHANNEL_H__
#define __IPCLINUX_VI_CHANNEL_H__

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
#include <OSAL_Mutex.h>
#include <OSAL_Queue.h>

#include <camera.h>
#include <type_camera.h>
#include <CameraParameters.h>
#include <CameraListener.h>
//#include <VIPPOSD.h>
#include <ModData.h>
#include <AWMD_Interface.h>

#include "CallbackNotifier.h"
#include "PreviewWindow.h"
#include <AdasData.h>


namespace EyeseeLinux {

class CallbackNotifier;

class VIChannel : public CameraBufferReference
{
public:
    const bool mbForceRef;
    VIChannel(ISP_DEV nIspDevId, int chnId, bool bForceRef);
    ~VIChannel();

    bool captureThread();
    bool previewThread();
    bool pictureThread();
    bool commandThread();
    enum VIChannelState
    {
        /* Object has been constructed. */
        VI_CHN_STATE_CONSTRUCTED = 0,
        /* Object was prepare to start. */
        VI_CHN_STATE_PREPARED,
        /* VI channel has been started. */
        VI_CHN_STATE_STARTED,
    };
    status_t prepare();
    status_t release();
    status_t startChannel();
    status_t stopChannel();
    VIChannelState getState();
    status_t getMODParams(MOTION_DETECT_ATTR_S *pParamMD);
    status_t setMODParams(MOTION_DETECT_ATTR_S pParamMD);
    status_t startMODDetect();
    status_t stopMODDetect();
    status_t getAdasParams(AdasDetectParam *pParamADAS);
    status_t setAdasParams(AdasDetectParam pParamADAS);
    status_t getAdasInParams(AdasInParam *pParamADAS);
    status_t setAdasInParams(AdasInParam pParamADAS);
    status_t startAdasDetect();
    status_t stopAdasDetect();

    status_t getAdasParams_v2(AdasDetectParam_v2 *pParamADAS_v2);
    status_t setAdasParams_v2(AdasDetectParam_v2 pParamADAS_v2);
    status_t getAdasInParams_v2(AdasInParam_v2 *pParamADAS_v2);
    status_t setAdasInParams_v2(AdasInParam_v2 pParamADAS_v2);
    status_t startAdasDetect_v2();
    status_t stopAdasDetect_v2();

    inline int getChannelId()
    {
        return mChnId;
    }
    bool isPreviewEnabled();
    status_t startRender();
    status_t stopRender();
    status_t pauseRender();
    status_t resumeRender();
    status_t storeDisplayFrame(uint64_t framePts);
    status_t releaseFrame(uint32_t index);

    
    void setPicCapMode(uint32_t mode);   // added to improve the pic taking operation in dark environment
    void setFrmDrpThrForPicCapMode(uint32_t frm_cnt);
    
    inline status_t startRecording(CameraRecordingProxyListener *pCb, int recorderId)
    {
        return mpCallbackNotifier->startRecording(pCb, recorderId, mChnId);
    }
    inline status_t stopRecording(int recorderId)
    {
        return mpCallbackNotifier->stopRecording(recorderId);
    }
    inline void setDataListener(DataListener *pCb)
    {
        mpCallbackNotifier->setDataListener(pCb);
    }
    inline void setNotifyListener(NotifyListener *pCb)
    {
        mpCallbackNotifier->setNotifyListener(pCb);
    }
    status_t setParameters(CameraParameters &param);
    status_t getParameters(CameraParameters &param) const;
//    static bool compareOSDRectInfo(const OSDRectInfo& first, const OSDRectInfo& second);
//    status_t setOSDRects(std::list<OSDRectInfo> &rects);
//    status_t getOSDRects(std::list<OSDRectInfo> **ppRects);
//    status_t OSDOnOff(bool bOnOff);
    status_t takePicture(unsigned int msgType, PictureRegionCallback *pPicReg); //msgType:CAMERA_MSG_COMPRESSED_IMAGE
    status_t notifyPictureRelease();
    status_t cancelContinuousPicture();
    status_t KeepPictureEncoder(bool bKeep);
    status_t releasePictureEncoder();
    inline status_t setPreviewDisplay(int hlay)
    {
        return mpPreviewWindow->setPreviewWindow(hlay);
    }
    status_t changePreviewDisplay(int hlay);
    static void calculateCrop(RECT_S *rect, int zoom, int width, int height);
    static int planeNumOfV4l2PixFmt(int nV4l2PixFmt);   //V4L2_PIX_FMT_NV21

    void increaseBufRef(VIDEO_FRAME_BUFFER_S *pBuf);
    void decreaseBufRef(unsigned int nBufId);
    void NotifyRenderStart();
    status_t PreviewG2dTimeoutCb(void);
private:
    status_t doTakePicture(unsigned int msgType);   //msgType:CAMERA_MSG_COMPRESSED_IMAGE
    status_t doCancelContinuousPicture();
    status_t DebugLoopSaveFrame(VIDEO_FRAME_INFO_S *pFrame);
protected:
    class DoCaptureThread : public Thread
    {
    public:
        DoCaptureThread(VIChannel *pViChn);
        status_t startThread();
        void stopThread();
        status_t startCapture();
        status_t pauseCapture();
        status_t SendCommand_TakePicture();
        status_t SendCommand_CancelContinuousPicture();
        virtual bool threadLoop();
    private:
        friend class VIChannel;
        Mutex mStateLock;
        Condition mPauseCompleteCond;
        Condition mStartCompleteCond;
        enum CaptureMsgType
        {
            MsgTypeCapture_SetState = 0x100,
            MsgTypeCapture_TakePicture,
            MsgTypeCapture_CancelContinuousPicture,
            MsgTypeCapture_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        typedef enum
        {
            /* Do not capture frame. */
            CAPTURE_STATE_NULL,
            /* Do not capture frame. */
            CAPTURE_STATE_PAUSED,
            /* Start capture frame. */
            CAPTURE_STATE_STARTED,
            /* exit thread*/
            //CAPTURE_STATE_EXIT,
        } CAPTURE_THREAD_STATE_E;
        CAPTURE_THREAD_STATE_E mCapThreadState;
        VIChannel* const mpViChn;
    };

    class DoPreviewThread : public Thread
    {
    public:
        DoPreviewThread(VIChannel *pViChn);
        status_t startThread();
        void stopThread();
        status_t notifyNewFrameCome();
        status_t releaseAllFrames();
        virtual bool threadLoop();
    private:
        friend class VIChannel;
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
        VIChannel* const mpViChn;
    };

    class DoPictureThread : public Thread
    {
    public:
        DoPictureThread(VIChannel *pViChn);
        status_t startThread();
        void stopThread();
        status_t notifyNewFrameCome();
        status_t notifyPictureEnd();
        status_t releaseAllFrames();
        virtual bool threadLoop();
    private:
        friend class VIChannel;
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
        VIChannel* const mpViChn;
    };

    class DoMODThread : public Thread
    {
    public:
        DoMODThread(VIChannel *pViChn);
        ~DoMODThread();
        status_t startThread();
        void stopThread();
        status_t getMODParams(MOTION_DETECT_ATTR_S *pParamMD);
        status_t setMODParams(MOTION_DETECT_ATTR_S pParamMD);
        status_t startMODDetect();
        status_t stopMODDetect();
        bool IsMODDetectEnable();
        status_t sendFrame(VIDEO_FRAME_BUFFER_S *pFrmBuf);
        virtual bool threadLoop();
    private:
        friend class VIChannel;
        bool MODThread();
        status_t releaseAllFrames();
        //status_t config_VDA_CHN_ATTR_S();
        //static ERRORTYPE VDACallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
        enum MODMsgType
        {
            MsgTypeMOD_DetectOn = 0x400,
            MsgTypeMOD_DetectOff,
            MsgTypeMOD_InputFrameAvailable,
            MsgTypeMOD_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        EyeseeQueue *mpMODFrameQueue;   //VIDEO_FRAME_BUFFER_S
        Mutex mWaitLock;
        bool mbWaitFrame;
        Mutex mMODDetectLock;
        Condition mCondMODDetect;
        bool mbMODDetectEnable;
        AW_HANDLE mhMOD;
        int m_Sensitivity;
        int m_width;
        int m_height;
        AWMD *m_awmd;
        AWMDVar *m_MDvar;
        unsigned int mFrameCounter;
        VIChannel* const mpViChn;
    };

    class DoCommandThread : public Thread
    {
    public:
        DoCommandThread(VIChannel *pViChn);
        status_t startThread();
        void stopThread();
        bool threadLoop();
        status_t SendCommand_TakePicture(unsigned int msgType); //msgType:CAMERA_MSG_COMPRESSED_IMAGE
        status_t SendCommand_CancelContinuousPicture();
    private:
        friend class VIChannel;
        enum CommandMsgType
        {
            MsgTypeCommand_TakePicture = 0x10,
            MsgTypeCommand_CancelContinuousPicture,
            MsgTypeCommand_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        VIChannel* const mpViChn;
    };
    class DoAdasThread : public Thread
    {
        public:
            DoAdasThread(VIChannel *pViChn);
            ~DoAdasThread();
            status_t startThread();
            void stopThread();
            status_t getAdasParams(AdasDetectParam *pParamADAS);
            status_t setAdasParams(AdasDetectParam pParamADAS);
            status_t getAdasInParams(AdasInParam *pParamADAS);
            status_t setAdasInParams(AdasInParam pParamADAS);
            status_t startAdasDetect();
            status_t stopAdasDetect();
            
            status_t getAdasParams_v2(AdasDetectParam_v2 *pParamADAS_v2);
            status_t setAdasParams_v2(AdasDetectParam_v2 pParamADAS_v2);
            status_t getAdasInParams_v2(AdasInParam_v2 *pParamADAS_v2);
            status_t setAdasInParams_v2(AdasInParam_v2 pParamADAS_v2);
            status_t startAdasDetect_v2();
            status_t stopAdasDetect_v2();

            bool IsAdasDetectEnable();
            status_t sendFrame(VIDEO_FRAME_BUFFER_S *pFrmBuf);
            status_t scalAdasFrame(VIDEO_FRAME_INFO_S *pFrmInfo);
            virtual bool threadLoop();

        private:
            friend class VIChannel;
            bool AdasThread();
            status_t releaseAllFrames();
            enum AdasMsgType
            {
                MsgTypeADAS_DetectOn = 0x700,
                MsgTypeADAS_DetectOff,
                MsgTypeADAS_InputFrameAvailable,
                MsgTypeADAS_Exit,
            };
            EyeseeMessageQueue mMsgQueue;
            EyeseeQueue *mpADASFrameQueue;   //VIDEO_FRAME_BUFFER_S
            Mutex mWaitLock;
            bool mbWaitFrame;
            Mutex mAdasDetectLock;
            Condition mCondAdasDetect;
            bool mbAdasDetectEnable;
            AW_HANDLE mhADAS;
            unsigned int mFrameCounter;
            VIChannel* const mpViChn;
            AdasDetectParam mAdasDetectParam;
            AdasDetectParam_v2 mAdasDetectParam_v2;
        public:
            AdasInParam mAdasInParam;
            AdasInParam_v2 mAdasInParam_v2;
            pthread_mutex_t mDataTransferLock;
            bool mAdasDataCallBackOut;
            bool mGetFrameDataOk;
            unsigned char *mPtrImgBuffer;
            int m_width;
            int m_height;
            int mAdasG2DHandle;
    };
public:
      DoAdasThread * GetmpAdasThread(){return mpAdasThread;}
      CallbackNotifier *GetMpCallbackNotifier(){return mpCallbackNotifier;}
private:
    ISP_DEV mIspDevId;
    int mChnId;

    DoCaptureThread *mpCapThread;
    //Mutex mCapThreadLock;
    //Condition mCapThreadCond;
//    bool mCanBeStoped;
//    Mutex mCanBeStopedLock;
//    Condition mCanBeStopedCond;

    DoPreviewThread *mpPrevThread;
    //Mutex mPrevThreadLock;
    //Condition mPrevThreadCond;

    DoPictureThread *mpPicThread;
    //Mutex mPicThreadLock;
    //Condition mPicThreadCond;

    DoMODThread *mpMODThread;

    DoAdasThread *mpAdasThread;
    DoCommandThread *mpCommandThread;

    OSAL_QUEUE *mpPreviewQueue;
    OSAL_QUEUE *mpPictureQueue;

    Mutex mRefCntLock;
    Mutex mLock;

    int mNewZoom, mLastZoom, mMaxZoom;
    RECT_S mRectCrop;   //after soft zoom, show_area in picBuf. extend show_area to whole screen to show zoom effect.
                        //and encode this show_area too.

    //VIDEO_FRAME_BUFFER_S *mpVideoFrmBuffer;
    std::list<VIDEO_FRAME_BUFFER_S> mFrameBuffers;
    std::list<unsigned int> mFrameBufferIdList;
    Mutex mFrameBuffersLock;

    CallbackNotifier *mpCallbackNotifier;
    PreviewWindow *mpPreviewWindow;
    bool mbPreviewEnable;
    bool mbKeepPictureEncoder;
    Mutex mTakePictureLock;

    VIChannelState mChannelState;

    bool mIsPicCopy;    //copy picBuf to out buf.
    unsigned int mTakePicMsgType;   //CAMERA_MSG_COMPRESSED_IMAGE
    TAKE_PICTURE_MODE_E mTakePictureMode;
    bool mbTakePictureStart;  //indicate start to take picture;
    
    //for TAKE_PICTURE_MODE_CONTINUOUS
    int mContinuousPictureCnt;
    int mContinuousPictureMax;
    uint64_t mContinuousPictureStartTime;   //unit:ms, start continuousPicture time.
    uint64_t mContinuousPictureLast;    //unit:ms, next picture time.
    uint64_t mContinuousPictureInterval;   //unit:ms

    CameraParameters mParameters;
    int mColorSpace;    //V4L2_COLORSPACE_JPEG
    int mFrameWidth, mFrameHeight;  //capture picture size(not frame buf size).
    int mFrameWidthOut, mFrameHeightOut;    //mpi_vi output picture size(not frame buf size). In common, frameWidthOut == frameWidth, but in anti-shake mode, frameWidthOut < frameWidth.
    //std::list<OSDRectInfo> mOSDRects;

    struct ScMemOpsS *mpMemOps;

    int mDbgFrameNum;
    std::list<std::string> mDbgFrameFilePathList;

    unsigned int mFrameCounter;

    unsigned int mCamDevTimeoutCnt;

    unsigned int PicCapModeEn;          // added to imporve the pic taking operation in dark environment
    unsigned int FrmDrpCntForPicCapMode; 
    unsigned int FrmDrpThrForPicCapMode;
};

}; /* namespace EyeseeLinux */

#endif /* __IPCLINUX_VI_CHANNEL_H__ */
