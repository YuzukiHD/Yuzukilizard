
#ifndef _SAMPLE_EYESEEISE_H_
#define _SAMPLE_EYESEEISE_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>

typedef struct SampleEyeseeIseCmdLineParam
{
    std::string mConfigFilePath;
}SampleEyeseeIseCmdLineParam;

class SampleEyeseeIseContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleEyeseeIseContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};

class SampleEyeseeIseContext;
class EyeseeIseListener
    : public EyeseeLinux::EyeseeISE::PictureCallback
    , public EyeseeLinux::EyeseeISE::ErrorCallback
{
public:
    //bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeISE* pISE);
    void onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE);

    EyeseeIseListener(SampleEyeseeIseContext *pContext);
    virtual ~EyeseeIseListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};

// not use recorder
class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleEyeseeIseContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleEyeseeIseContext *const mpOwner;
};

class SampleEyeseeIseContext
{
public:
    SampleEyeseeIseContext();
    ~SampleEyeseeIseContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_LAYER mCamVoLayer[2];
    int mCamChnId[4];

    ISE_CHN_ATTR_S mIseChnAttr[4];
    VO_LAYER mIseVoLayer[4];
    ISE_CHN mIseChnId[4][4];

    CameraInfo mCameraInfo[2];
    EyeseeLinux::EyeseeCamera *mpCamera[2];
    EyeseeLinux::EyeseeISE *mpIse[4];
    EyeseeLinux::EyeseeRecorder *mpRecorder[4];
    EyeseeCameraListener mCameraListener;
    EyeseeIseListener mIseListener;
    EyeseeRecorderListener mRecorderListener;   // not use
    cdx_sem_t mSemRenderStart;


    int mMuxerId[2];
    std::deque<std::string> mSegmentFiles;
    int mFileNum;

    int mCamPicNum;
    int mIsePicNum;
};

#endif  /* _SAMPLE_EYESEEISE_H_ */

