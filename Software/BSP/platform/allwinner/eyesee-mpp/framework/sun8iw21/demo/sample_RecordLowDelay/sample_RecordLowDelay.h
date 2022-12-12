
#ifndef _SAMPLE_RECORDLOWDELAY_H_
#define _SAMPLE_RECORDLOWDELAY_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <string>

#include <Errors.h>

typedef struct SampleRecordLowDelayCmdLineParam
{
    std::string mConfigFilePath;
}SampleRecordLowDelayCmdLineParam;

typedef struct SampleRecordLowDelayConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mCaptureFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mPreviewWidth;
    int mPreviewHeight;
    PIXEL_FORMAT_E mPreviewFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    bool mbPreviewEnable;
    bool mbEncodeEnable;
    int mFrameRate;
    int mDisplayFrameRate;  //0 means display every frame.
    int mCaptureBufNum;
    int mPreviewBufNum;
    PAYLOAD_TYPE_E mEncodeType;

    int mEncodeWidth;
    int mEncodeHeight;
    int mEncodeBitrate;
    
    int mEncodeDuration;  //unit:s, 0 mean infinite
    bool mbCallbackFlag;
    std::string mEncodeFolderPath;
    std::string mCallbackFilePath;
    std::string mFilePath2;
}SampleRecordLowDelayConfig;

class SampleRecordLowDelayContext;
class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleRecordLowDelayContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleRecordLowDelayContext *const mpOwner;
};

class SampleRecordLowDelayContext
{
public:
    SampleRecordLowDelayContext();
    ~SampleRecordLowDelayContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleRecordLowDelayCmdLineParam mCmdLinePara;
    SampleRecordLowDelayConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeLinux::EyeseeRecorder *mpRecorder;
    EyeseeRecorderListener mRecorderListener;

    FILE *mRawFile;
    int64_t mLastCallbackPts;
    int mVencId;
};

#endif  /* _SAMPLE_RECORDLOWDELAY_H_ */

