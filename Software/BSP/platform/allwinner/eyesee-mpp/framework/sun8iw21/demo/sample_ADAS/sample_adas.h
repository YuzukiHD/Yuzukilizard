#ifndef _SAMPLE_ADAS_H_
#define _SAMPLE_ADAS_H_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseeCamera.h>
#include "sample_adas_ui.h"

typedef struct SampleAdasCmdLineParam
{
    std::string mConfigFilePath;
}SampleAdasCmdLineParam;

typedef struct SampleAdasConfig
{
    int cam_id;
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mDisplayWidth;
    int mDisplayHeight;
    int mPreviewRotation;
    int mAdasEnable;
    int mAdasUiEnable;
    int mTestDuration;  //unit:s, 0 mean infinite
}SampleAdasConfig;

class SampleAdasContext;

class EyeseeCameraCallback
    : public EyeseeLinux::EyeseeCamera::InfoCallback
    , public EyeseeLinux::EyeseeCamera::AdasDataCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onAdasOutData(int chnId, AW_AI_ADAS_DETECT_R_ *p_sResult,EyeseeLinux::EyeseeCamera* pCamera);
	void onAdasOutData_v2(int chnId, AW_AI_ADAS_DETECT_R__v2 *p_sResult,EyeseeLinux::EyeseeCamera* pCamera);
    EyeseeCameraCallback(SampleAdasContext *pContext);
    virtual ~EyeseeCameraCallback(){}
private:
    SampleAdasContext *const mpContext;
};


class SampleAdasUi;
class SampleAdasContext
{
public:
    SampleAdasContext();
    ~SampleAdasContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, const char *argv[]);
    EyeseeLinux::status_t loadConfig();
    SampleAdasCmdLineParam mCmdLinePara;
    SampleAdasConfig mConfigPara;
    cdx_sem_t mSemExit;
    int mUILayer;
    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeCameraCallback mCameraCallbacks;
    EyeseeLinux::EyeseeCamera *mpRecCamera;
    SampleAdasUi *mSampleAdasUi;
    int mMuxerId;
};

#endif  /* _SAMPLE_ADAS_H_ */

