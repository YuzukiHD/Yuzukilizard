
#ifndef _SAMPLE_CAMERA_TAKE_PICTURE_H_
#define _SAMPLE_CAMERA_TAKE_PICTURE_H_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseeCamera.h>

typedef struct SampleCameraTakePictureCmdLineParam
{
    std::string mConfigFilePath;
}SampleCameraTakePictureCmdLineParam;

typedef struct SampleCameraTakePictureConfig
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

    int mJpegWidth;
    int mJpegHeight;
    int mJpegThumbWidth;
    int mJpegThumbHeight;
    int mJpegNum;
    int mJpegInterval;  //unit:ms
    std::string mJpegFolderPath;

    int mRawWidth;
    int mRawHeight;
    int mRawNum;
    std::string mRawFolderPath;
    PIXEL_FORMAT_E mRawFormat;

    int mTestDuration;  //unit:s, 0 mean infinite
}SampleCameraTakePictureConfig;

class SampleCameraTakePictureContext;

class EyeseeCameraCallback
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraCallback(SampleCameraTakePictureContext *pContext);
    virtual ~EyeseeCameraCallback(){}
private:
    SampleCameraTakePictureContext *const mpContext;
};

class EyeseeCameraRawCallback
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraRawCallback(SampleCameraTakePictureContext *pContext);
    virtual ~EyeseeCameraRawCallback(){}
private:
    SampleCameraTakePictureContext *const mpContext;
};

class SampleCameraTakePictureContext
{
public:
    SampleCameraTakePictureContext();
    ~SampleCameraTakePictureContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleCameraTakePictureCmdLineParam mCmdLinePara;
    SampleCameraTakePictureConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeCameraCallback mCameraCallbacks;
	EyeseeCameraRawCallback mCameraRawCallbacks;
    cdx_sem_t mSemRenderStart;
    int mPicNum;
    int mScalerOutChns[4];
};

#endif  /* _SAMPLE_CAMERA_TAKE_PICTURE_H_ */

