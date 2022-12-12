#ifndef _SAMPLE_DUALRECORD_H_
#define _SAMPLE_DUALRECORD_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <string>

#include <Errors.h>

typedef struct SampleDualRecordCmdLineParam
{
    std::string mConfigFilePath;
}SampleDualRecordCmdLineParam;

typedef struct SampleDualRecordConfig
{
    unsigned int mCaptureWidth;
    unsigned int mCaptureHeight;
    int mDigitalZoom;   //0~10
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mDstFrameRate;
    double mTimeLapseFps;   //0:disable.
    PAYLOAD_TYPE_E mEncodeType0;
    PAYLOAD_TYPE_E mEncodeType1;
    PAYLOAD_TYPE_E mAudioEncodeType;
    int mEncodeDuration;  //unit:s, 0 mean infinite
    std::string mEncodeFolderPath;

    int mEncodeWidth0;
    int mEncodeHeight0;
    int mEncodeBitrate0;
    std::string mFilePath0;

    int mEncodeWidth1;
    int mEncodeHeight1;
    int mEncodeBitrate1;
    std::string mFilePath1;
}SampleDualRecordConfig;

class SampleDualRecordContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleDualRecordContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleDualRecordContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleDualRecordContext *pOwner);
private:
    SampleDualRecordContext *const mpOwner;
};

class SampleDualRecordContext
{
public:
    SampleDualRecordContext();
    ~SampleDualRecordContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleDualRecordCmdLineParam mCmdLinePara;
    SampleDualRecordConfig mConfigPara;

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
    EyeseeLinux::EyeseeRecorder *mpRecorder1;
    EyeseeCameraListener mCameraListener1;
    EyeseeRecorderListener mRecorderListener1;

    cdx_sem_t mSemRenderStart;
};

#endif  /* _SAMPLE_DUALRECORD_H_ */

