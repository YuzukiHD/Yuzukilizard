/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CveBDII.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/08/08
  Last Modified :
  Description   : ISE(Image Stitching Engine) module.
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_CVE_BDII_H__
#define __IPCLINUX_CVE_BDII_H__

#include <vector>

#include <plat_type.h>
#include <plat_errno.h>
#include <plat_defines.h>
#include <plat_math.h>
#include <Errors.h>

#include <tsemaphore.h>

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <utils/EyeseeQueue.h>
#include <utils/EyeseeMessageQueue.h>

#include <OSAL_Mutex.h>
#include <OSAL_Queue.h>

#include <camera.h>
#include <CameraParameters.h>
#include <CameraListener.h>
#include "../camera/CameraBufferReference.h"
#include "../camera/PreviewWindow.h"

#include "EyeseeCamera.h"
#include "EyeseeBDIICommon.h"


namespace EyeseeLinux {

typedef struct BDII_FRAME_NODE_S
{
    VIDEO_FRAME_BUFFER_S *pFrame0;
    VIDEO_FRAME_BUFFER_S *pFrame1;
}BDII_FRAME_NODE_S;

class CveBDII
{
public:
    CveBDII(unsigned int mId);
    ~CveBDII();

    status_t prepare();
    status_t start();
    status_t stop();

    status_t setDisplay(int hlay);
    status_t setPreviewRotation(int rotation);

    void setDataListener(DataListener *pCb);
    void setNotifyListener(NotifyListener *pCb);
    void enableMessage(unsigned int msg_type);  //CAMERA_MSG_COMPRESSED_IMAGE
    void disableMessage(unsigned int msg_type); //CAMERA_MSG_COMPRESSED_IMAGE
    bool isMessageEnabled(unsigned int msg_type);  //CAMERA_MSG_COMPRESSED_IMAGE

    status_t setCamera(std::vector<BDIICameraChnInfo>& cameraChannels);

    void dataCallbackTimestamp(int nCameraIndex, const VIDEO_FRAME_BUFFER_S *pCameraFrameInfo);

    status_t setBDIIConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param);
    status_t getBDIIConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param);

    status_t setDebugYuvDataOut(bool saveOut, std::vector<BDIIDebugFileOutString>&vec);

protected:
    class DoProcessFrameThread : public Thread
    {
    public:
        DoProcessFrameThread(CveBDII *pCveBDII);
        ~DoProcessFrameThread();
        status_t ProcessFrameThreadStart();
        //status_t ProcessFrameThreadPause();
        status_t ProcessFrameThreadStop();

        status_t sendFrame(VIDEO_FRAME_BUFFER_S *pFrame0, VIDEO_FRAME_BUFFER_S *pFrame1);
        //status_t SendMessage_InputFrameReady();

        status_t SetBDIIProcessConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param);
        status_t GetBDIIProcessConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param);

        status_t debugYuvDataOut(bool isOut, std::vector<BDIIDebugFileOutString>&vec);

    private:
        friend class CveBDII;
        Mutex mLock;
        Mutex mWaitLock;
        Mutex mStateLock;
        Mutex mBDIINodeLock;

        std::list<BDII_FRAME_NODE_S> mIdleBDIINodeList; //BDII_FRAME_NODE_S frame node
        std::list<BDII_FRAME_NODE_S> mUsedBDIINodeList; //BDII_FRAME_NODE_S frame node

        Condition mLoadedCompleteCond;
        Condition mPauseCompleteCond;
        Condition mStartCompleteCond;
        // Values for ProcessFrameThread message type
        enum ProcessFrameMsgType
        {
            MsgTypeProcessFrame_SetState = 0x100,
            MsgTypeProcessFrame_InputFrameAvailable,
            MsgTypeProcessFrame_Exit,
        };

        EyeseeMessageQueue mProcessFrameMsgQueue;
        EyeseeQueue *mpProcessFrameDataQueue; //BDII_FRAME_NODE_S -> thread to handle

        bool mbWaitInputFrameBuf;
        enum PROCESS_FRAME_STATE_E
        {
            PROCESS_FRAME_STATE_LOADED,
            /* Do not send frame to ISE. */
            PROCESS_FRAME_STATE_PAUSED,
            /* Start send frame. */
            PROCESS_FRAME_STATE_STARTED,
        };
        PROCESS_FRAME_STATE_E mProcessFrameThreadState;
        virtual bool threadLoop();
        /**
         * send mCameraProxies's frame to MPP-ISE.
         *
         * @return true if want thread continue. false if want to quit thread.
         */
        bool processFrameThread();

        CveBDII* const mpCveBDII;
        bool mbThreadOK;
        AW_AI_CVE_BDII_INIT_PARA_S mInitParam;
        AW_HANDLE hBDII;
        void *mpCostImg;
        void *mpDeepImg;
        unsigned int mCostImagSize;
        unsigned int mDeepImagSize;
        SIZE_S mImageSize;

        bool bSaveYuvOut;
        std::map<std::string, FILE *> mMapFdOut;
    };

    class DoPreviewThread : public Thread, public CameraBufferReference
    {
    public:
        DoPreviewThread(CveBDII *pBDII);
        ~DoPreviewThread();
        status_t startThread();
        void stopThread();
        status_t sendDeepImage(CVE_BDII_RES_OUT_S *pBDIIResult);
        status_t setDisplay(int hlay);
        status_t setPreviewRotation(int rotation);
        virtual bool threadLoop();
        virtual void increaseBufRef(VIDEO_FRAME_BUFFER_S *pbuf);
        virtual void decreaseBufRef(unsigned int nBufId);
        virtual void NotifyRenderStart();
    private:
        bool previewThread();
        static status_t convertBDIIDeepImage2NV21(CVE_BDII_RES_OUT_S *pBDIIResult, VIDEO_FRAME_BUFFER_S *pFrmBuf);
        //friend class CveBDII;
        int mHlay;
        int mPreviewRotation;   //90: need clock-wise rotate 90 degree to display normal picture.
        enum PreviewMsgType
        {
            MsgTypePreview_InputFrameAvailable = 0x200,
            MsgTypePreview_Exit,
        };
        Mutex mFrameLock;
        std::list<VIDEO_FRAME_BUFFER_S> mIdleFrameList;
        std::list<VIDEO_FRAME_BUFFER_S> mUsingFrame;  //only one element
        std::list<VIDEO_FRAME_BUFFER_S> mReadyFrameList;
        std::list<VIDEO_FRAME_BUFFER_S> mDisplayFrameList;
        SIZE_S mImageSize;
        static int mFrameIdCounter;
        PreviewWindow *mpPreviewWindow;
        EyeseeMessageQueue mMsgQueue;
        Mutex mWaitLock;
        //Condition mCondReleaseAllFramesFinished;
        //bool mbWaitReleaseAllFrames;
        bool mbWaitPreviewFrame;
        CveBDII* const mpBDII;
    };

private:
    status_t stop_l();
    //int ifStartReceiveFrame(int nCameraIndex);

private:
    class CameraProxyListener: public CameraRecordingProxyListener  //I set to camera, camera call.
    {
    public:
        CameraProxyListener(CveBDII *pIse, int nCameraIndex);
        virtual void dataCallbackTimestamp(const void *pdata);

    private:
        CveBDII * mpCveBDII;
        int mCameraIndex;
    };
    //camera input
    class CameraProxyInfo
    {
    public:
        CameraRecordingProxy *mpCameraProxy;    // for return to camera
        int mnCameraChannel;

        unsigned int mFrameCounter; //reserve

        std::list<VIDEO_FRAME_BUFFER_S> mIdleFrameBufList;
        std::list<VIDEO_FRAME_BUFFER_S> mReadyFrameBufList;
        std::list<VIDEO_FRAME_BUFFER_S> mUsedFrameBufList;

        Mutex mFrameBufListLock;
        status_t SetBDIICameraChnInfo(BDIICameraChnInfo *pCameraChnInfo);
        status_t releaseUsedFrame(VIDEO_FRAME_BUFFER_S *pFrame);
        status_t releaseUsedFrame_l(VIDEO_FRAME_BUFFER_S *pFrame);
        status_t releaseAllFrames();
        CameraProxyInfo();
        CameraProxyInfo(const CameraProxyInfo& ref);
        ~CameraProxyInfo();
    };
    std::vector<CameraProxyInfo> mCameraProxies;

    //CameraParameters mParameters;
    enum BDIIWorkState
    {
        BDII_STATE_CONSTRUCTED = 0,
        BDII_STATE_PREPARED,
        BDII_STATE_STARTED,
    };
    BDIIWorkState mBDIIState;

    Mutex mLock;

    //BOOL mWaitReturnSomeFrameFlag;
    //cdx_sem_t mWaitReturnFrameSem;

    const unsigned int m_ID;

    unsigned int mMessageEnabler;   //CAMERA_MSG_BDII_DATA
    DataListener *mpDataCallback;
    NotifyListener *mpNotifyCallback;
    DoProcessFrameThread *mpProcessFrameThread;
    DoPreviewThread *mpPreviewThread;

    //int mInitCamIndex[2]; //the camera that has receiver data

}; /* class CveBDII */

}; /* namespace EyeseeLinux */

#endif /* __IPCLINUX_CVE_BDII_H__ */

