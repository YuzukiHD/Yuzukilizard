#ifndef _SAMPLE_AI_BDII_H_
#define _SAMPLE_AI_BDII_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>

#include <EyeseeBDII.h>

typedef struct SampleAIBDII_CmdLineParam
{
    std::string mConfigFilePath;
}SampleAIBDII_CmdLineParam;

typedef struct SampleAIBDIIConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;

    int mEnableDisp;
    int mDisplayWidth;
    int mDisplayHeight;
    int mPreviewRotation;

    int mBDIIEnable;
    
    int mTestDuration;  //unit:s, 0 mean infinite
}SampleAIBDIIConfig;

class SampleAIBDIIContext;

class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    EyeseeCameraListener(SampleAIBDIIContext *pContext);
    virtual ~EyeseeCameraListener(){}

    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
private:
    SampleAIBDIIContext *const mpContext;
};


class EyeseeBDIIListener
    : public EyeseeLinux::EyeseeBDII::BDIIDataCallback
    , public EyeseeLinux::EyeseeBDII::ErrorCallback
{
public:
    EyeseeBDIIListener(SampleAIBDIIContext *pContext);
    virtual ~EyeseeBDIIListener(){}

    void onBDIIData(int chnId, const EyeseeLinux::CVE_BDII_RES_OUT_S *data, EyeseeLinux::EyeseeBDII* pBDII);
    void onError(int chnId, int error, EyeseeLinux::EyeseeBDII *pBDII);
private:
    SampleAIBDIIContext *const mpContext;
};


class SampleAIBDIIContext
{
public:
    SampleAIBDIIContext();
    ~SampleAIBDIIContext();

    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();

    SampleAIBDII_CmdLineParam mCmdLinePara;
    SampleAIBDIIConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;

    VO_DEV mVoDev;
    //VO_CHN mVoChn[2];
    VO_LAYER mVoLayer[3];
    VO_VIDEO_LAYER_ATTR_S mLayerAttr[3];

    CameraInfo mCameraInfo[2];
    int mCamChnId[2];
    EyeseeLinux::EyeseeCamera *mpCamera[2];

    EyeseeLinux::EyeseeBDII *mpBDII;

    EyeseeCameraListener *mpCameraCallback;
    EyeseeBDIIListener *mpBDIIListener;
};

#endif  /* _SAMPLE_AI_BDII_H_ */

