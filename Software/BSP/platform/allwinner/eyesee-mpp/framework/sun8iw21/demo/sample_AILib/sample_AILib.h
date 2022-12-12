#ifndef _SAMPLE_AILIb_H_
#define _SAMPLE_AILIb_H_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>


typedef struct SampleAILibCmdLineParam
{
    std::string mConfigFilePath;
}SampleAILibCmdLineParam;

typedef struct SampleAILibConfig
{
    int cam_id;
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mDisplayWidth;
    int mDisplayHeight;
    int mPreviewRotation;

    int mEveEnable;
    int mModEnable;
    int mVRPLEnable;

    int mRecEnable;
    
    int mTestDuration;  //unit:s, 0 mean infinite
}SampleAILibConfig;

class SampleAILibContext;

class EyeseeCameraCallback
    : public EyeseeLinux::EyeseeCamera::InfoCallback
    , public EyeseeLinux::EyeseeCamera::VLPRDataCallback
    , public EyeseeLinux::EyeseeCamera::FaceDetectDataCallback
    , public EyeseeLinux::EyeseeCamera::MODDataCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);

    void onVLPRData(int chnId, const AW_AI_CVE_VLPR_RULT_S *data, EyeseeLinux::EyeseeCamera* pCamera);
    void onFaceDetectData(int chnId, const AW_AI_EVE_EVENT_RESULT_S *data, EyeseeLinux::EyeseeCamera* pCamera);
    void onMODData(int chnId, const AW_AI_CVE_DTCA_RESULT_S *data, EyeseeLinux::EyeseeCamera *pCamera);

    EyeseeCameraCallback(SampleAILibContext *pContext);
    virtual ~EyeseeCameraCallback(){}
private:
    SampleAILibContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                             , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                             , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleAILibContext *mpContext);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleAILibContext *const mpContext;
};


class SampleAILibContext
{
public:
    SampleAILibContext();
    ~SampleAILibContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    EyeseeLinux::status_t initCVEFuncParam(AW_AI_CVE_DTCA_PARAM &paramDTCA, AW_AI_CVE_CLBR_PARAM &paramCLBR);

    SampleAILibCmdLineParam mCmdLinePara;
    SampleAILibConfig mConfigPara;

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
    EyeseeLinux::EyeseeRecorder *mpRecorder;
    EyeseeRecorderListener mRecorderListener;
    int mMuxerId;
};

#endif  /* _SAMPLE_AILIb_H_ */

