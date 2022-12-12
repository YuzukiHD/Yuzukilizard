#ifndef _SAMPLE_EYESEEISE_AILIB_H_
#define _SAMPLE_EYESEEISE_AILIB_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>

#include <EyeseeCamera.h>
#include <EyeseeISE.h>


#define ISE_CHN_OUT_FRAME_W  384
#define ISE_CHN_OUT_FRAME_H  360

typedef struct ISE_SIZE
{
    int w;
    int h;
}ISE_SIZE;


typedef struct SampleEyeseeIseCmdLineParam
{
    std::string mConfigFilePath;
}SampleEyeseeIseCmdLineParam;

typedef struct SampleEyeseeIseConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    int mCaptureFrameRate;

    int ise_mode;

    int ise_chn_out_w;
    int ise_chn_out_h;

    int mEveEnable;
    int mModEnable;
    int mVRPLEnable;

    int mISERecEnable;
    
    int mTestDuration;  //unit:s, 0 mean infinite
}SampleEyeseeIseConfig;


class SampleEyeseeIseAILibContext;
class EyeseeCameraListener : public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);

    EyeseeCameraListener(SampleEyeseeIseAILibContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleEyeseeIseAILibContext *const mpContext;
};

class SampleEyeseeIseAILibContext;
class EyeseeIseListener : public EyeseeLinux::EyeseeISE::ErrorCallback
    , public EyeseeLinux::EyeseeISE::VLPRDataCallback
    , public EyeseeLinux::EyeseeISE::FaceDetectDataCallback
    , public EyeseeLinux::EyeseeISE::MODDataCallback
{
public:
    //bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeISE *pCamera);
    void onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE);

    void onVLPRData(int chnId, const AW_AI_CVE_VLPR_RULT_S *data, EyeseeLinux::EyeseeISE* pCamera);
    void onFaceDetectData(int chnId, const AW_AI_EVE_EVENT_RESULT_S *data, EyeseeLinux::EyeseeISE* pCamera);
    void onMODData(int chnId, const AW_AI_CVE_DTCA_RESULT_S *data, EyeseeLinux::EyeseeISE *pCamera);

    EyeseeIseListener(SampleEyeseeIseAILibContext *pContext);
    virtual ~EyeseeIseListener(){}
private:
    SampleEyeseeIseAILibContext *const mpContext;
};

// not use recorder
class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleEyeseeIseAILibContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleEyeseeIseAILibContext *const mpOwner;
};


#define MAX_CAM_NUM  (2)
#define MAX_ISE_CHN_NUM  (4)


class SampleEyeseeIseAILibContext
{
public:
    SampleEyeseeIseAILibContext();
    ~SampleEyeseeIseAILibContext();

    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();

    EyeseeLinux::status_t initCVEFuncParam(AW_AI_CVE_DTCA_PARAM &paramDTCA, AW_AI_CVE_CLBR_PARAM &paramCLBR, ISE_SIZE pic_size);


    SampleEyeseeIseConfig mConfigPara;

    SampleEyeseeIseCmdLineParam mCmdLinePara;

    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

    int mUILayer;
    VO_DEV mVoDev;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    ISE_GRP mIseGrp;
    ISE_CHN_ATTR_S mIseChnAttr;
    VO_LAYER mIseVoLayer[MAX_ISE_CHN_NUM];
    ISE_CHN mIseChnId[MAX_ISE_CHN_NUM];
    int mIseChnCnt;
    EyeseeLinux::EyeseeISE *mpIse;

    CameraInfo mCameraInfo[MAX_CAM_NUM];
    int mCamChnId[MAX_CAM_NUM];
    EyeseeLinux::EyeseeCamera *mpCamera[MAX_CAM_NUM];

    EyeseeLinux::EyeseeRecorder *mpRecorder[MAX_ISE_CHN_NUM];

    EyeseeCameraListener mCameraListener;
    EyeseeIseListener mIseListener;
    EyeseeRecorderListener mRecorderListener;   // not use

    int mMuxerId[MAX_ISE_CHN_NUM];
};

#endif  /* _SAMPLE_EYESEEISE_AILIB_H_ */

