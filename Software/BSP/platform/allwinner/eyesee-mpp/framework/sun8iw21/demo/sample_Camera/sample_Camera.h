
#ifndef _SAMPLE_CAMERA_H_
#define _SAMPLE_CAMERA_H_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseeCamera.h>

typedef struct SampleCameraCmdLineParam
{
    std::string mConfigFilePath;
}SampleCameraCmdLineParam;

typedef struct OSDConfigPara
{
    RGN_TYPE_E mOSDType;
    int mOSDPriority;
    int mOSDColor;  //ARGB
    bool mbOSDColorInvert;
    int mInvertLumThresh;
    int mInvertUnitWidth;
    int mInvertUnitHeight;
    RECT_S mOSDRect;
}OSDConfigPara;

typedef struct SampleCameraConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    int mViClock;   //MHz, 0 means use default.
    int mDigitalZoom;   //0~10
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameNum;
    int mFrameRate;
    int mDisplayFrameRate;  //0 means display every frame.
    int mDisplayWidth;
    int mDisplayHeight;
    int mPreviewRotation;
    RECT_S mUserRegion;
    int nSwitchRenderTimes;

    bool mbAntishakeEnable;
    int mAntishakeInputBuffers;
    int mAntishakeOutputBuffers;
    int mAntishakeOperationMode;

    int mJpegWidth;
    int mJpegHeight;
    int mJpegQuality;
    int mJpegThumbWidth;
    int mJpegThumbHeight;
    int mJpegThumbQuality;
    bool mbJpegThumbfileFlag;
    int mJpegNum;
    int mJpegInterval;  //unit:ms
    std::string mJpegFolderPath;

    int jpeg_region_overlay_x;
    int jpeg_region_overlay_y;
    int jpeg_region_overlay_w;
    int jpeg_region_overlay_h;
    int jpeg_region_overlay_priority;
    int jpeg_region_overlay_invert;

    int jpeg_region_cover_x;
    int jpeg_region_cover_y;
    int jpeg_region_cover_w;
    int jpeg_region_cover_h;
    int jpeg_region_cover_color;
    int jpeg_region_cover_priority;
    
    std::vector<OSDConfigPara> mOSDConfigParams;

    int mTestDuration;  //unit:s, 0 mean infinite
}SampleCameraConfig;

class SampleCameraContext;

class EyeseeCameraCallback
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    enum PicType
    {
        PicType_Main = 0,
        PicType_Thumb = 1,
    };
    PicType mPicType;
    EyeseeCameraCallback(SampleCameraContext *pContext);
    virtual ~EyeseeCameraCallback(){}
private:
    SampleCameraContext *const mpContext;
};

class SampleCameraPictureRegionCallback : public EyeseeLinux::PictureRegionCallback
{
public:
    SampleCameraPictureRegionCallback(){}
    ~SampleCameraPictureRegionCallback(){};
    virtual void addPictureRegion(std::list<PictureRegionType> &rPictureRegionList);
};

class SampleCameraContext
{
public:
    SampleCameraContext();
    ~SampleCameraContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleCameraCmdLineParam mCmdLinePara;
    SampleCameraConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeCameraCallback mCameraCallbacks;
    EyeseeCameraCallback mCameraThumbPictureCallback;
    SampleCameraPictureRegionCallback mSampleCameraPictureRegionCallback;
    cdx_sem_t mSemRenderStart;
    int mPicNum;
    int mScalerOutChns;

    //std::list<EyeseeLinux::OSDRectInfo> mOSDRects;
    //RGN_HANDLE mOverlayHandle;
    //RGN_HANDLE mCoverHandle;
    std::list<RGN_HANDLE> mRgnHandleList;
};

#endif  /* _SAMPLE_CAMERA_H_ */

