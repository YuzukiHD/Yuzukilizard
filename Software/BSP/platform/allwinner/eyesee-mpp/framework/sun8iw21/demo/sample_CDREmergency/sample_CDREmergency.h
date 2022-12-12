
#ifndef _SAMPLE_RECORDER_H_
#define _SAMPLE_RECORDER_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>

typedef struct SampleRecorderCmdLineParam
{
    std::string mConfigFilePath;
}SampleRecorderCmdLineParam;

enum SampleRecorderMethod
{
    ByTime = 0,  //by time duration
    BySize,      //by file size.
};
typedef struct SampleRecorderConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mDisplayWidth;
    int mDisplayHeight;
    PAYLOAD_TYPE_E mEncodeType;
    PAYLOAD_TYPE_E mAudioEncodeType;
    int mEncodeWidth;
    int mEncodeHeight;
    int mEncodeBitrate;
    int mEncodeFrameRate;

    std::string mFolderPath;
    unsigned int mCount;  //keeping  files when app running.
    SampleRecorderMethod mMethod;
    int mDuration;  //unit:s, 0 mean infinite
    int mSize;   //unit: Byte. 0 mean infinite.
    int mMuxCacheDuration;
    int mFirstEmergencyTime;
    int mSecondEmergencyTime;
}SampleRecorderConfig;

class SampleRecorderContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleRecorderContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleRecorderContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleRecorderContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleRecorderContext *const mpOwner;
};

class SampleRecorderContext
{
public:
    SampleRecorderContext();
    ~SampleRecorderContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    std::string MakeFileName();
    EyeseeLinux::status_t setVideoFrameRateAndIFramesNumberInterval(SampleRecorderContext *stContext, int fps);
    SampleRecorderCmdLineParam mCmdLinePara;
    SampleRecorderConfig mConfigPara;

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
    cdx_sem_t mSemRenderStart;
    int mMuxerId;
    int mMuxerIdEmergency_01;
    int mMuxerIdEmergency_02;

    std::deque<std::string> mFiles;
    int mFileNum;
    int mPicNum;
};

#endif  /* _SAMPLE_RECORDER_H_ */

