
#ifndef _SAMPLE_RECORDERSEGMENT_H_
#define _SAMPLE_RECORDERSEGMENT_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>

typedef struct SampleRecorderSegmentCmdLineParam
{
    std::string mConfigFilePath;
}SampleRecorderSegmentCmdLineParam;

enum SampleRecorderSegmentMethod
{
    SegmentByTime = 0,  //by time duration
    SegmentBySize,      //by file size.
};
typedef struct SampleRecorderSegmentConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mDispWidth;
    int mDispHeight;
    PAYLOAD_TYPE_E mEncodeType;
    PAYLOAD_TYPE_E mAudioEncodeType;
    int mEncodeWidth;
    int mEncodeHeight;
    int mEncodeBitrate;
    RecordFileDurationPolicy mFileDurationPolicy;

    std::string mSegmentFolderPath;
    unsigned int mSegmentCount;  //keeping segment files when app running.
    SampleRecorderSegmentMethod mSegmentMethod;
    int mSegmentDuration;  //unit:s, 0 mean infinite
    int mSegmentSize;   //unit: Byte. 0 mean infinite.

    int mJpegWidth;
    int mJpegHeight;
    int mJpegThumbWidth;
    int mJpegThumbHeight;
    int mJpegNum;
    int mJpegInterval;  //unit:ms
}SampleRecorderSegmentConfig;

class SampleRecorderSegmentContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleRecorderSegmentContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleRecorderSegmentContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleRecorderSegmentContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleRecorderSegmentContext *const mpOwner;
};

class SampleRecorderSegmentContext
{
public:
    SampleRecorderSegmentContext();
    ~SampleRecorderSegmentContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    std::string MakeSegmentFileName();
    SampleRecorderSegmentCmdLineParam mCmdLinePara;
    SampleRecorderSegmentConfig mConfigPara;

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
    std::deque<std::string> mSegmentFiles;
    int mFileNum;
    int mPicNum;
    int mVencId;
};

#endif  /* _SAMPLE_RECORDERSEGMENT_H_ */

