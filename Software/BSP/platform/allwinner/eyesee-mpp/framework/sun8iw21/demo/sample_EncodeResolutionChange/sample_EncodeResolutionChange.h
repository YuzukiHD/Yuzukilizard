
#ifndef _SAMPLE_ENCODERESOLUTIONCHANGE_H_
#define _SAMPLE_ENCODERESOLUTIONCHANGE_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_venc.h>

#include <string>

#include <Errors.h>

typedef struct SampleEncodeResolutionChangeCmdLineParam
{
    std::string mConfigFilePath;
}SampleEncodeResolutionChangeCmdLineParam;

typedef struct SampleEncodeResolutionChangeConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    int mViClock;   //MHz, 0 means use default.
    int mVeClock;   //MHz, 0 means use default.
    PIXEL_FORMAT_E mCaptureFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mPreviewWidth;
    int mPreviewHeight;
    PIXEL_FORMAT_E mPreviewFormat;
    int mDigitalZoom;   //0~10
    int mDispWidth;
    int mDispHeight;
    bool mbPreviewEnable;
    int mFrameRate;
    int mDstFrameRate;

    //venc attr
    PAYLOAD_TYPE_E mEncodeType;
    int mH264_profile;         // 0:BL 1:MP 2:HP
    H264_LEVEL_E mH264Level;         // 5.1, 4.1, 3.1
    int mH265_profile;        //0:MP
    H265_LEVEL_E mH265Level;         // 6.2, 5.2
    int mEncodeWidth0;
    int mEncodeHeight0;
    int mEncodeWidth1;
    int mEncodeHeight1;
    int mIDRFrameInterval;
    bool mbVideoEncodingPIntra;

    //venc rc attr
    //EyeseeLinux::EyeseeRecorder::VideoEncodeRateControlMode mVideoRCMode;
    EyeseeLinux::VencParameters::VideoEncodeRateControlMode mVideoRCMode;
    int mQp1;
    int mQp2;
    int mEncodeBitrate0;
    int mEncodeBitrate1;

    //venc gop attr
    VENC_GOP_MODE_E mGopMode;
    unsigned int mSPInterval;
    int mVirtualIFrameInterval;

    bool mbAdvanceRef;
    int nBase;
    int nEnhance;
    bool mbHorizonfilp;
    bool mbAdaptiveintrainp;
    int mb3dnr;
    bool mbcolor2grey;
    bool mbNullSkipEnable;
    unsigned char bRefBaseEn;
    bool mbEnableSmartP;
    int mIntraRefreshBlockNum;
    double mTimeLapseFps;   //0:disable.
    VENC_SUPERFRM_MODE_E mSuperFrmMode;
    unsigned int mSuperIFrmBitsThr;
    unsigned int mSuperPFrmBitsThr;

    PAYLOAD_TYPE_E mAudioEncodeType;
    int mAudioSampleRate;
    int mAudioChannelNum;
    int mAudioEncodeBitrate;    //bps

    int mEncodeDuration;  //unit:s, 0 mean infinite
    std::string mEncodeFolderPath;
    std::string mFilePath0;
    std::string mFilePath1;
}SampleEncodeResolutionChangeConfig;

class SampleEncodeResolutionChangeContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleEncodeResolutionChangeContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleEncodeResolutionChangeContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleEncodeResolutionChangeContext *pOwner);
private:
    SampleEncodeResolutionChangeContext *const mpOwner;
};

class SampleEncodeResolutionChangeContext
{
public:
    SampleEncodeResolutionChangeContext();
    ~SampleEncodeResolutionChangeContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleEncodeResolutionChangeCmdLineParam mCmdLinePara;
    SampleEncodeResolutionChangeConfig mConfigPara;

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

    int mVencId;
};

#endif  /* _SAMPLE_ENCODERESOLUTIONCHANGE_H_ */
