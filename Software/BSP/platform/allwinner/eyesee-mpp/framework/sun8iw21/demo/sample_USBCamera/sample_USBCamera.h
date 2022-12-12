#ifndef _SAMPLE_USBCAMERA_H_
#define _SAMPLE_USBCAMERA_H_

#include <string>
#include <deque>

#include <mm_comm_sys.h>
#include <mm_comm_vo.h>

#include <EyeseeUSBCamera.h>
#include <EyeseeRecorder.h>

struct SampleUSBCamCmdLineParam
{
    std::string mConfigFilePath;
};

struct SampleUSBCamConfig
{
    std::string mUSBCamDevName;
    int mCaptureWidth;
    int mCaptureHeight;
    UVC_CAPTURE_FORMAT mPicFormat;
    int mFrameRate;
    int mFrameNum;

    int mVdecFlag;
    int mVdecBufSize;
    int mVdecPriority;
    int mVdecPicWidth;
    int mVdecPicHeight;
    PIXEL_FORMAT_E mVdecOutputPixelFormat;
    int mVdecSubPicEnable;
    int mVdecSubPicWidth;
    int mVdecSubPicHeight;
    PIXEL_FORMAT_E mVdecSubOutputPixelFormat;
    int mVdecExtraFrameNum;

    int mDispChnNum;
    int mDispWidth;
    int mDispHeight;
    int mPreviewRotation;

    UvcChn mPhotoChn;
    int mPhotoNum;
    int mPhotoInterval; //unit:ms
    bool mPhotoPostviewEnable;   //if enable postview encode.
    int mPhotoWidth;
    int mPhotoHeight;
    int mPhotoThumbWidth;
    int mPhotoThumbHeight;

    UvcChn mEncodeChn;
    PAYLOAD_TYPE_E mEncodeType;
    int mEncodeWidth;
    int mEncodeHeight;
    int mEncodeBitrate;
    std::string mDirPath;
    unsigned int mSegmentCounts;
    unsigned int mSegmentDuration;

    int mTestDuration;
};

class SampleUSBCamContext;

class EyeseeUSBCameraCallback
    : public EyeseeLinux::EyeseeUSBCamera::PictureCallback
    , public EyeseeLinux::EyeseeUSBCamera::InfoCallback
    , public EyeseeLinux::EyeseeUSBCamera::ErrorCallback
    , public EyeseeLinux::PictureRegionCallback
{
public:
    virtual void onError(UvcChn chnId, CameraMsgErrorType error, EyeseeLinux::EyeseeUSBCamera *pCamera) override;
    virtual bool onInfo(UvcChn chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeUSBCamera *pCamera) override;
    virtual void onPictureTaken(UvcChn chnId, const void *data, int size, EyeseeLinux::EyeseeUSBCamera* pCamera) override;
    virtual void addPictureRegion(std::list<PictureRegionType> &rPictureRegionList) override;

    enum class PicType
    {
        PicType_Main = 0,
        PicType_Thumb = 1,
    };
    EyeseeUSBCameraCallback(SampleUSBCamContext *pContext);
    virtual ~EyeseeUSBCameraCallback();
private:
    SampleUSBCamContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleUSBCamContext *pOwner);    
    virtual void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra) override;
    virtual void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra) override;
    virtual void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra) override;
private:
     SampleUSBCamContext  * const mpOwner;
};

class SampleUSBCamContext
{
public:
    SampleUSBCamContext();
    ~SampleUSBCamContext();
    SampleUSBCamContext(const SampleUSBCamContext &other) = delete; //do not allow to copy
    SampleUSBCamContext& operator=(const SampleUSBCamContext &other) = delete;

    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);   
    EyeseeLinux::status_t loadConfig();    
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);   
    std::string MakeSegmentFileName();

    MPP_SYS_CONF_S mSysConf;
    SampleUSBCamCmdLineParam mCmdLineParam;
    SampleUSBCamConfig mConfigParam;

    EyeseeLinux::EyeseeUSBCamera::USBCameraCaptureParam mUSBCamCapParam;
    EyeseeLinux::EyeseeUSBCamera::USBCameraVdecParam mUsbCamVdecParam;
    //vo 
    int mUILayer;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    UvcChn mDisplayUvcChn;

    //muxid
    int mMuxerId;
    
    EyeseeLinux::EyeseeUSBCamera *mpUSBCamera;
    EyeseeLinux::EyeseeRecorder *mpRecorder;
    EyeseeUSBCameraCallback mUSBCameraListener;
    EyeseeRecorderListener mRecorderListener;

    std::deque<std::string> mSegmentFiles;
    unsigned int mFileNum;
    unsigned int mPicNum;
    std::vector<int> mPicNumArray;
    //unsigned int mFileNumMax;
    //unsigned int mSegmentSize;
    
    cdx_sem_t mSemExit;
    
};


#endif

