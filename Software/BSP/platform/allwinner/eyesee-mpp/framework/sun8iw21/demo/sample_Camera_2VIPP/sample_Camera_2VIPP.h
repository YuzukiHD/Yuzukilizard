
#ifndef _SAMPLE_CAMERA_2VIPP_H_
#define _SAMPLE_CAMERA_2VIPP_H_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseeCamera.h>

typedef struct SampleCamera2VIPPCmdLineParam
{
    std::string mConfigFilePath;
}SampleCamera2VIPPCmdLineParam;

typedef struct SampleCamera2VIPPConfig
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

    int mTestDuration;  //unit:s, 0 mean infinite
}SampleCamera2VIPPConfig;

class SampleCamera2VIPPContext;

class EyeseeCameraCallback
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraCallback(SampleCamera2VIPPContext *pContext);
    virtual ~EyeseeCameraCallback(){}
private:
    SampleCamera2VIPPContext *const mpContext;
};
class SampleCamera2VIPPContext
{
public:
    SampleCamera2VIPPContext();
    ~SampleCamera2VIPPContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleCamera2VIPPCmdLineParam mCmdLinePara;
    SampleCamera2VIPPConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeCameraCallback mCameraCallbacks;
    cdx_sem_t mSemRenderStart;
    int mPicNum;
    int mScalerOutChns[4];
};

#endif  /* _SAMPLE_CAMERA_2VIPP_H_ */

