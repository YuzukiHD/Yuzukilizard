#ifndef __SAMPLE_RECORDMULTISTREAM_H__
#define __SAMPLE_RECORDMULTISTREAM_H__

#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_venc.h>

#include <string.h>

#include <Errors.h>

typedef struct SampleRecordMultiStreamCmdLineParam
{
    std::string mConfigFilePath;
}SampleRecordMultiStreamCmdLineParam;

typedef struct SampleRecordMultiStreamConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    int mViClock;
    int mVeClock;
    PIXEL_FORMAT_E mCaptureFormat;
    int mPreviewWidth;
    int mPreviewHeight;
    PIXEL_FORMAT_E mPreviewFormat;
    int mDigitalZoom;
    int mDispWidth;
    int mDispHeight;
    bool mbPreviewEnable;
    int mFrameRate;

    //first venc
    int mVipp0; //venc0 use which vipp's frames to encode.
    PAYLOAD_TYPE_E mEncodeType_0;
    int mDstFrameRate_0;
    int mEncodeWidth_0;
    int mEncodeHeight_0;
    int mIDRFrameInterval_0;
    int mEncodeBitrate_0;
    /* Only vipp0 & venc0 support online */
    int mOnlineEnable_0;
    int mOnlineShareBufNum_0;

    //second venc
    int mVipp1;
    PAYLOAD_TYPE_E mEncodeType_1;
    int mDstFrameRate_1;
    int mEncodeWidth_1;
    int mEncodeHeight_1;
    int mIDRFrameInterval_1;
    int mEncodeBitrate_1;

    PAYLOAD_TYPE_E mAudioEncodeType;
    int mAudioSampleRate;
    int mAudioChannelNum;
    int mAudioEncodeBitrate;    //bps

    int mSegmentCount;  //0: disable segment, >=1: keeping file number.
    int mSegmentDuration;   //unit:s

    int mEncodeDuration;  //unit:s, 0 mean infinite
    std::string mEncodeFolderPath;
    std::string mFileName;

    int mSkipStream;

    int mMuxCacheEnable;
    int mMuxCacheStrmIdsFilterEnable;

}SampleRecordMultiStreamConfig;

class SampleRecordMultiStreamContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera *pCamera);

    EyeseeCameraListener(SampleRecordMultiStreamContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleRecordMultiStreamContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                        , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                        , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleRecordMultiStreamContext *pOwner);
private:
    SampleRecordMultiStreamContext *const mpOwner;
};

class SampleRecordMultiStreamContext
{
public:
    SampleRecordMultiStreamContext();
    ~SampleRecordMultiStreamContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig(int argc, char *argv[]);
    static EyeseeLinux::status_t CreateFolder(const std::string &strFoldrpath);
    std::string MakeSegmentFileName();
    SampleRecordMultiStreamCmdLineParam mCmdLinePara;
    SampleRecordMultiStreamConfig mConfigPara;

    int mVipp0;
    int mVipp1;

    int mVencId0;   //vencId, app decide.
    int mVencId1;   //vencId, app decide.

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
};

#endif  /* __SAMPLE_RECORDMULTISTREAM_H__ */
