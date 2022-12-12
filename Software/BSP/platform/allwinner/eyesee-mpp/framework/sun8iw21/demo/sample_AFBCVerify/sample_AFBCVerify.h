
#ifndef _SAMPLE_AFBCVERIFY_H_
#define _SAMPLE_AFBCVERIFY_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <string>

#include <Errors.h>

typedef struct SampleAFBCVerifyCmdLineParam
{
    std::string mConfigFilePath;
}SampleAFBCVerifyCmdLineParam;

typedef struct SampleAFBCVerifyConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mCaptureFormat; //PIXEL_FORMAT_YUV_PLANAR_420
    int mPreviewWidth;
    int mPreviewHeight;
    PIXEL_FORMAT_E mPreviewFormat;
    int mFrameRate;
    int mCaptureBufNum;
    int mPreviewBufNum;
    bool bEncodeEnable;
    PAYLOAD_TYPE_E mEncodeType;
    int mEncodeDuration;  //unit:s, 0 mean infinite
    std::string mEncodeFolderPath;

    int mEncodeWidth;
    int mEncodeHeight;
    int mEncodeBitrate;
}SampleAFBCVerifyConfig;

class SampleAFBCVerifyContext;
class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleAFBCVerifyContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleAFBCVerifyContext *const mpOwner;
};

class SampleAFBCVerifyContext
{
public:
    SampleAFBCVerifyContext();
    ~SampleAFBCVerifyContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleAFBCVerifyCmdLineParam mCmdLinePara;
    SampleAFBCVerifyConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    int mCameraId;
    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeLinux::EyeseeRecorder *mpRecorder;
    EyeseeRecorderListener mRecorderListener;

    int mEncodeErrorNum;
};

#endif  /* _SAMPLE_AFBCVERIFY_H_ */

