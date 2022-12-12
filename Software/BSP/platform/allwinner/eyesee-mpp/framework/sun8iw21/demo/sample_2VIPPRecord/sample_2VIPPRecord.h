
#ifndef _SAMPLE_2VIPPRECORD_H_
#define _SAMPLE_2VIPPRECORD_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>

typedef struct Sample2VIPPRecordCmdLineParam
{
    std::string mConfigFilePath;
}Sample2VIPPRecordCmdLineParam;

typedef struct Sample2VIPPRecordConfig
{
    int mVippMainChn;
    int mVippMainWidth;
    int mVippMainHeight;
    PIXEL_FORMAT_E mVippMainPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mVippMainFrameRate;
    int mVippMainDstFrameRate;
    int mVippMainBufferNum;
    PAYLOAD_TYPE_E mVippMainEncodeType;

    int mVippSubChn;
    int mVippSubWidth;
    int mVippSubHeight;
    PIXEL_FORMAT_E mVippSubPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mVippSubFrameRate;
    int mVippSubDstFrameRate;
    int mVippSubBufferNum;
    int mVippSubRotation;   //0, 90, 180, 270
    PAYLOAD_TYPE_E mVippSubEncodeType;
    bool mbVippSubEnableTakePic;
    
    PAYLOAD_TYPE_E mAudioEncodeType;

    int mMainEncodeWidth;
    int mMainEncodeHeight;
    int mMainEncodeBitrate;

    int mSubEncodeWidth;
    int mSubEncodeHeight;
    int mSubEncodeBitrate;

    int mSegmentCount;
    int mEncodeDuration;  //unit:s, 0 mean infinite
    std::string mEncodeFolderPath;
}Sample2VIPPRecordConfig;

class Sample2VIPPRecordContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(Sample2VIPPRecordContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    Sample2VIPPRecordContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(Sample2VIPPRecordContext *pOwner);
private:
    Sample2VIPPRecordContext *const mpOwner;
};

class Sample2VIPPRecordContext
{
public:
    Sample2VIPPRecordContext();
    ~Sample2VIPPRecordContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    std::string MakeSegmentFileName(bool bMainFile);
    EyeseeLinux::status_t StartRecord();
    EyeseeLinux::status_t StopRecord();    
    Sample2VIPPRecordCmdLineParam mCmdLinePara;
    Sample2VIPPRecordConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeCameraListener mCameraListener;

    EyeseeLinux::Mutex mRecordLock;
    bool mbRecording;
    EyeseeLinux::EyeseeRecorder *mpMainRecorder;
    EyeseeRecorderListener mMainRecorderListener;
    EyeseeLinux::EyeseeRecorder *mpSubRecorder;
    EyeseeRecorderListener mSubRecorderListener;

    EyeseeLinux::Mutex mFileListLock;
    std::deque<std::string> mMainSegmentFiles;
    std::deque<std::string> mSubSegmentFiles;
    cdx_sem_t mSemRenderStart;

    int mMainFileNum;
    int mSubFileNum;
    int mPicNum;
};

#endif  /* _SAMPLE_2VIPPRECORD_H_ */

