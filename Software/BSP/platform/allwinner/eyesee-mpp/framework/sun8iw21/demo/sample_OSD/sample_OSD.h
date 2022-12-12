#ifndef _SAMPLE_OSD_H2_
#define _SAMPLE_OSD_H2_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

typedef struct SampleOSDCmdLineParam
{
    std::string mConfigFilePath;
}SampleOSDCmdLineParam;

typedef struct SampleOSDConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mCaptureFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mPreviewWidth;
    int mPreviewHeight;
    PIXEL_FORMAT_E mPreviewFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mDigitalZoom;   //0~10
    
    int mFrameRate;
    int mDisplayFrameRate;  //0 means display every frame.
    int mDisplayWidth;
    int mDisplayHeight;
    int mPreviewRotation;   //0, 90, 180, 270

    int mVippOverlayX;
    int mVippOverlayY;
    int mVippOverlayW;
    int mVippOverlayH;

    int mVippCoverX;
    int mVippCoverY;
    int mVippCoverW;
    int mVippCoverH;
    int mVippCoverColor;

    int mVippOrlX;
    int mVippOrlY;
    int mVippOrlW;
    int mVippOrlH;
    int mVippOrlColor;

    int mVencOverlayX;
    int mVencOverlayY;
    int mVencOverlayW;
    int mVencOverlayH;

    int mVencCoverX;
    int mVencCoverY;
    int mVencCoverW;
    int mVencCoverH;
    int mVencCoverColor;

    int mVencOrlX;
    int mVencOrlY;
    int mVencOrlW;
    int mVencOrlH;
    int mVencOrlColor;

    PAYLOAD_TYPE_E mEncodeType;
    PAYLOAD_TYPE_E mAudioEncodeType;
    int mEncodeWidth;
    int mEncodeHeight;
    int mEncodeBitrate;
    std::string mEncodeFolderPath;
    std::string mFilePath;

    int mTestDuration;  //unit:s, 0 mean infinite
}SampleOSDConfig;

class SampleOSDContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleOSDContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleOSDContext *const mpContext;
};
class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleOSDContext *pOwner);
private:
    SampleOSDContext *const mpOwner;
};

class SampleOSDContext
{
public:
    SampleOSDContext();
    ~SampleOSDContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleOSDCmdLineParam mCmdLinePara;
    SampleOSDConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeLinux::EyeseeRecorder *mpRecorder;
    EyeseeCameraListener mCameraListener;
    EyeseeRecorderListener mRecorderListener;

    RGN_HANDLE mVippOverlayHandle;
    RGN_HANDLE mVippCoverHandle;
    RGN_HANDLE mVippOrlHandle;
    RGN_HANDLE mVencOverlayHandle;
    RGN_HANDLE mVencCoverHandle;
    RGN_HANDLE mVencOrlHandle;
    
    cdx_sem_t mSemRenderStart;

    int mVencId;
};

#endif  /* _SAMPLE_OSD_H2_ */

