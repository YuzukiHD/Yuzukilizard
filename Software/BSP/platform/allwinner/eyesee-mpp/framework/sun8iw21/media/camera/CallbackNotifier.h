/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CallbackNotifier.h
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/03
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/
#ifndef __IPCLINUX_CALLBACK_NOTIFIER_H__
#define __IPCLINUX_CALLBACK_NOTIFIER_H__

#include <vector>
#include <memory>

#include <plat_type.h>
#include <plat_errno.h>
#include <plat_defines.h>
#include <plat_math.h>

#include <Errors.h>

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>

#include <camera.h>
#include <type_camera.h>
#include <CameraListener.h>
#include <EyeseeMessageQueue.h>
#include "CameraJpegEncoder.h"
#include "CameraBufferReference.h"
#include <PictureRegionCallback.h>

namespace EyeseeLinux {

typedef struct RecordingProxyListener
{
    int mRecorderId;
    int mInputChnId; // the recording input chnId, it depend on previous component. as default is 0.  
    CameraRecordingProxyListener *mpListener;
} RecordingProxyListener;

struct PictureBuffer
{
    bool mbSharedMemPrepared;   //true:mPicBuf is prepared, mpData0... are not used; false:mPicBuf is null, mpData0... are used to store venclibBuffer address.
    unsigned char *mpData0;
    unsigned char *mpData1;
    unsigned char *mpData2;
    unsigned int mLen0;
    unsigned int mLen1;
    unsigned int mLen2;
    unsigned int mThumbOffset;
    unsigned int mThumbLen;
    std::shared_ptr<CMediaMemory> mPicBuf;
    size_t mDataSize;
    unsigned int mMsgType;  //CAMERA_MSG_COMPRESSED_IMAGE
    bool mIsContinuous;
    //bool mIsEnd;

    PictureBuffer()
    {
        mbSharedMemPrepared = false;
        mpData0 = NULL;
        mpData1 = NULL;
        mpData2 = NULL;
        mLen0 = 0;
        mLen1 = 0;
        mLen2 = 0;
        mThumbOffset = 0;
        mThumbLen = 0;
        mPicBuf.reset();
        mDataSize = 0;
        mMsgType = 0;
        mIsContinuous =false;
    }
};

class CallbackNotifier {
public:
    CallbackNotifier(int chnId, CameraBufferReference *pCbr);
    ~CallbackNotifier();

    status_t NotifyRenderStart();
    status_t NotifyCameraDeviceTimeout();

    status_t NotifyCameraTakePicFail();

    int onNextFrameAvailable(void* frame, int chnId = 0);
    status_t postMODData(std::shared_ptr<CMediaMemory>& spData); //AW_AI_CVE_DTCA_RESULT_S
    status_t postAdasData(std::shared_ptr<CMediaMemory>& spData);//AW_AI_ADAS_DETECT_R
    status_t postFaceDetectData(std::shared_ptr<CMediaMemory>& spData);    //AW_AI_EVE_EVENT_RESULT_S
    status_t postVLPRData(std::shared_ptr<CMediaMemory>& spData); //AW_AI_CVE_VLPR_RULT_S

    void setJpegQuality(int jpeg_quality)
    {
        mJpegQuality = jpeg_quality;
    }
    void setPictureNum(int num);
    inline void setPictureSize(int w, int h)
    {
        mPictureWidth = w;
        mPictureHeight = h;
    }

    // Sets JPEG rotate used to compress frame during picture taking.
    inline void setJpegRotate(int jpeg_rotate)
    {
        mJpegRotate = jpeg_rotate;
    }

    inline void setGPSLatitude(double gpsLatitude)
    {
        mGpsLatitude = gpsLatitude;
    }

    inline void setGPSLongitude(double gpsLongitude)
    {
        mGpsLongitude = gpsLongitude;
    }

    inline void setGPSAltitude(long gpsAltitude)
    {
        mGpsAltitude = gpsAltitude;
    }

    inline void setGPSTimestamp(long gpsTimestamp)
    {
        mGpsTimestamp = gpsTimestamp;
    }

    void setGPSMethod(const char * gpsMethod);

    inline void setJpegThumbnailSize(int w, int h)
    {
        mPictureThumbWidth = w;
        mPictureThumbHeight= h;
    }

    inline void setJpegThumbnailQuality(int quality)
    {
        mPictureThumbQuality = quality;
    }

//    void setSaveFolderPath(const char *str);
//    void setSnapPath(const char *str);
//    void startContinuousPicture()
//    {
//        mSavePictureCnt = 0;
//    }

    status_t startRecording(CameraRecordingProxyListener *pCb, int recorderId, int chnId = 0);
    //status_t startRecording(CameraRecordingProxyListener *pCb, int recorderId, int chnId);
    status_t stopRecording(int recorderId);
    void setDataListener(DataListener *pCb);
    void setNotifyListener(NotifyListener *pCb);

    void enableMessage(unsigned int msg_type);  //CAMERA_MSG_COMPRESSED_IMAGE
    void disableMessage(unsigned int msg_type); //CAMERA_MSG_COMPRESSED_IMAGE
    inline int isMessageEnabled(unsigned int msg_type)  //CAMERA_MSG_COMPRESSED_IMAGE
    {
        return mMessageEnabler & msg_type;
    }

    void getCurrentDateTime(char *buf, int bufsize);
    void setExifMake(char *make, int size);
    void setExifModel(char *model, int size);
    void setExifCameraSerialNum(char *model, int size);
    /**
     * use encoder to make picture.
     *
     * @return true if success.
     * @param isContinuous indicate if encode several pictures contiguously.
     * @param isThumbnailPic this param is used to indicate if thumbnail picture is encoded as a independent jpeg(true),
     *      or is encoded in main picture(false).
     */
    bool takePicture(const void* frame,
                        bool isContinuous=false,
                        bool isThumbnailPic=false,
                        const struct isp_exif_attribute* ispExif=NULL);
    /**
     * postprocess take picture. e.g, destroy video encoder.
     */
    void takePictureEnd();
    bool savePictureThread();
    //void postDataCompleted(const void *pData, int size);
    void setPictureRegionCallback(PictureRegionCallback *pCb);
    void createPictureRegion();
    void destoryPictureRegion();
    status_t notifyPictureRelease();
    status_t NotifyG2DTimeout();

protected:
    class DoSavePictureThread : public Thread
    {
    public:
        DoSavePictureThread(CallbackNotifier *pCallbackNotifier);
        status_t startThread();
        void stopThread();
        status_t notifyNewPictureCome();
        status_t notifyPictureRelease();
        virtual bool threadLoop();
    private:
        friend class CallbackNotifier;
        enum SavePicMsgType
        {
            MsgTypeSavePic_InputPictureAvailable = 0x100,
            MsgTypeSavePic_ReleasePicture,
            MsgTypeSavePic_Exit,
        };
        EyeseeMessageQueue mMsgQueue;
        Mutex mWaitLock;
        bool mbWaitPicture;
        bool mbWaitReleasePicture;
        CallbackNotifier* const mpCallbackNotifier;
    };

private:
    //MPP_CHN_S mMppChnInfo;
    const int mChnId;
    CameraBufferReference *mpCamBufRef;

    std::vector<RecordingProxyListener> mRecListener;
    Mutex mRecListenerLock;
    DataListener *mpDataCallback;   //preview callback, transport preview frame to app.
    NotifyListener *mpNotifyCallback;

    int mPictureNum;
    int mPictureWidth;
    int mPictureHeight;
    int mPictureThumbWidth;
    int mPictureThumbHeight;
    int mPictureThumbQuality;
    int mJpegQuality;
    int mJpegRotate;
    double mGpsLatitude;
    double mGpsLongitude;
    double mGpsAltitude;
    long mGpsTimestamp;
    char *mpGpsProcessingMethod;
    CameraJpegEncoder *mpJpegenc;
    CameraJpegEncConfig mJpegEncConfig;

    unsigned int mMessageEnabler;   //CAMERA_MSG_COMPRESSED_IMAGE

    std::list<PictureBuffer> mPicBufList;
    std::list<PictureBuffer> mWaitReleasePicBufList;
    bool mbWaitPicBufListEmpty;
    Condition mCondWaitPicBufListEmpty;
    Mutex mPicBufLock;
    //Condition mSavePictureCond;
    Mutex mSavePictureLock;
    char *mpFolderPath; //picture folder path
    char *mpSnapPath;   //picture path
    int mSavePictureCnt;    //for msg CAMERA_MSG_CONTINUOUSSNAP

    //PictureRegionCallback
    PictureRegionCallback *mpPictureRegionCallback;
    std::list<PictureRegionType> mPictureRegionList;
    std::list<RGN_HANDLE> mPictureRegionHandleList;
    Mutex mPictureRegionLock;
    

    DoSavePictureThread *mpSavePicThread;

    Mutex mLock;
}; /* class CallbackNotifier */

}; /* namespace EyeseeLinux */

#endif /* __IPCLINUX_CALLBACK_NOTIFIER_H__ */

