#ifndef _SAMPLE_RECORDSWITCHFILENORMAL_H_
#define _SAMPLE_RECORDSWITCHFILENORMAL_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>

typedef struct SampleRecordSwitchFileNormalCmdLineParam
{
    std::string mConfigFilePath;
}SampleRecordSwitchFileNormalCmdLineParam;

enum SampleSegmentMethod
{
    SegmentByTime = 0,  //by time duration
    SegmentBySize,      //by file size.
};
typedef struct SampleRecordSwitchFileNormalConfig
{
    int mMainVippWidth;
    int mMainVippHeight;
    int mSubVippWidth;
    int mSubVippHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    PAYLOAD_TYPE_E mEncodeType;
    PAYLOAD_TYPE_E mAudioEncodeType;
    int mMainEncodeWidth;
    int mMainEncodeHeight;
    int mMainEncodeBitrate;
    int mSubEncodeWidth;
    int mSubEncodeHeight;
    int mSubEncodeBitrate;

    std::string mSegmentFolderPath;
    unsigned int mSegmentCount;  //keeping segment files when app running.
    SampleSegmentMethod mSegmentMethod;
    int mSegmentDuration;  //unit:s, 0 mean infinite
    int mSegmentSize;   //unit: Byte. 0 mean infinite.
}SampleRecordSwitchFileNormalConfig;

class SampleRecordSwitchFileNormalContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleRecordSwitchFileNormalContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleRecordSwitchFileNormalContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleRecordSwitchFileNormalContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleRecordSwitchFileNormalContext *const mpOwner;
};

class SampleRecordSwitchFileNormalContext
{
public:
    SampleRecordSwitchFileNormalContext();
    ~SampleRecordSwitchFileNormalContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    std::string MakeSegmentFileName(bool bMainFile);
    SampleRecordSwitchFileNormalCmdLineParam mCmdLinePara;
    SampleRecordSwitchFileNormalConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeLinux::EyeseeRecorder *mpMainRecorder;
    EyeseeLinux::EyeseeRecorder *mpSubRecorder;
    EyeseeRecorderListener mMainRecorderListener;
    EyeseeRecorderListener mSubRecorderListener;
    cdx_sem_t mSemRenderStart;
    int mMainMuxerId;
    int mSubMuxerId;
    EyeseeLinux::Mutex mFilesLock;
    std::deque<std::string> mMainSegmentFiles;
    std::deque<std::string> mSubSegmentFiles;
    int mMainFileNum;
    int mSubFileNum;
    int mMainVencId;
    int mSubVencId;
};

#endif  /* _SAMPLE_RECORDSWITCHFILENORMAL_H_ */

