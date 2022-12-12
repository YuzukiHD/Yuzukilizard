
//#define LOG_NDEBUG 0
#define LOG_TAG "RecordDemo"
#include <utils/plat_log.h>

//#include <binder/IPCThreadState.h>
//#include <binder/ProcessState.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <string>

#include <mm_common.h>
#include <vo/hwdisplay.h>
#include <mpi_sys.h>
#include <mpi_vi.h>
#include <mpi_vo.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

//#include <hwdisp_def.h>
//#include <fb.h>

using namespace std;
using namespace EyeseeLinux;

typedef struct _UserConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    int mViClock;               //MHz, 0 means use default.
    PIXEL_FORMAT_E mPicFormat;  //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameNum;
    int mFrameRate;
    int mBitsRate;
    int mDisplayFrameRate;      //0 means display every frame.
    int mDisplayWidth;
    int mDisplayHeight;
    int mPreviewRotation;
    int mTestDuration;          //unit:s, 0 mean infinite
}UserConfig;

int main(int argc, char *argv[])
{
    alogd("hello, cdrDemo");
    
    // 配置用户参数
    UserConfig mConfigPara;
    alogd("user not set config file. use default test parameter!");
    mConfigPara.mCaptureWidth     = 1920;
    mConfigPara.mCaptureHeight    = 1080;
    mConfigPara.mViClock          = 0;
    mConfigPara.mPicFormat        = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    mConfigPara.mFrameNum         = 5;
    mConfigPara.mFrameRate        = 25;
    mConfigPara.mBitsRate         = 10*1024*1024;
    mConfigPara.mDisplayFrameRate = 0;
    mConfigPara.mDisplayWidth     = 720; 
    mConfigPara.mDisplayHeight    = 1280;
    mConfigPara.mPreviewRotation  = 90;
    mConfigPara.mTestDuration     = 30;
    
    // 测试模式
    int PreviewTestFlag = 0;
    int RecordTestFlag  = 1;
    
    // init mpp system
    MPP_SYS_CONF_S mSysConf;
    memset(&mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&mSysConf);
    AW_MPI_SYS_Init();
    if (mConfigPara.mViClock != 0)
    {
        alogd("set vi clock to [%d]MHz", mConfigPara.mViClock);
        AW_MPI_VI_SetVIFreq(0, mConfigPara.mViClock * 1000000);
    }
    
    // 配置摄像机参数
    int cameraId = 0;
    EyeseeCamera::clearCamerasConfiguration();
    CameraInfo cameraInfo;
    cameraInfo.facing                 = CAMERA_FACING_BACK;
    cameraInfo.orientation            = 0;
    cameraInfo.canDisableShutterSound = true;
    cameraInfo.mCameraDeviceType      = CameraInfo::CAMERA_CSI;
    cameraInfo.mMPPGeometry.mCSIChn   = 1;  //摄像机硬件CSI接口编号，取决于物理连接
    ISPGeometry mISPGeometry;
    mISPGeometry.mISPDev = 0;
    mISPGeometry.mScalerOutChns.push_back(1);
    mISPGeometry.mScalerOutChns.push_back(3);
    cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
    mISPGeometry.mScalerOutChns.clear();
    mISPGeometry.mISPDev = 1;
    mISPGeometry.mScalerOutChns.push_back(0);
    mISPGeometry.mScalerOutChns.push_back(2);
    cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
    EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
        
    // 启动摄像头
    CameraInfo mCameraInfo;
    EyeseeCamera::getCameraInfo(cameraId, &mCameraInfo);
    EyeseeLinux::EyeseeCamera *mpCamera = NULL;
    mpCamera = EyeseeCamera::open(cameraId);
    //EyeseeCameraCallback mCameraCallbacks;
    //mpCamera->setInfoCallback(&mCameraCallbacks);
    mpCamera->prepareDevice();
    mpCamera->startDevice();
    
    // 配置VIPP1参数
    CameraParameters cameraParam;
    mpCamera->openChannel(1, true);  //从VIPP1预览码流    
    mpCamera->getParameters(1, cameraParam);  //获取VIPP1的配置参数
    SIZE_S captureSize = {(unsigned int)mConfigPara.mCaptureWidth, (unsigned int)mConfigPara.mCaptureHeight};
    cameraParam.setVideoSize(captureSize);                            //抓拍尺寸
    cameraParam.setPreviewFrameRate(mConfigPara.mFrameRate);          //抓拍帧率
    cameraParam.setDisplayFrameRate(mConfigPara.mDisplayFrameRate);   //显示帧率
    cameraParam.setPreviewFormat(mConfigPara.mPicFormat);             //图像格式
    cameraParam.setVideoBufferNumber(mConfigPara.mFrameNum);          //缓存帧数
    cameraParam.setPreviewRotation(mConfigPara.mPreviewRotation);     //旋转角度
    mpCamera->setParameters(1, cameraParam);
    mpCamera->prepareChannel(1);  //Notes:必须在调用setParameters之后调用
        
    // 配置输出资源
    VO_DEV mVoDev = 0;
    VO_LAYER mVoLayer = 0;
    int mUILayer = HLAY(2, 0);
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    AW_MPI_VO_Enable(mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(mUILayer);
    AW_MPI_VO_CloseVideoLayer(mUILayer);//close ui layer.
    AW_MPI_VO_EnableVideoLayer(0);
    AW_MPI_VO_GetVideoLayerAttr(0, &mLayerAttr);
    mLayerAttr.stDispRect.X      = 0;
    mLayerAttr.stDispRect.Y      = 0;
    mLayerAttr.stDispRect.Width  = mConfigPara.mDisplayWidth;
    mLayerAttr.stDispRect.Height = mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(0, &mLayerAttr);
    mpCamera->setChannelDisplay(1, mVoLayer);  //VIPP1-->Layer0
    mpCamera->startChannel(1);
    mpCamera->startRender(1);

    //start record
    char szFileName[256] = "Recorder.mp4";
    bool videoOnly = false;
    EyeseeRecorder *mpRecord = new EyeseeRecorder();
    mpRecord->setCameraProxy(mpCamera->getRecordingProxy(), 1);
    mpRecord->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    mpRecord->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, szFileName, 0, false);
    mpRecord->setVideoFrameRate(mConfigPara.mFrameRate);
    mpRecord->setVideoSize(mConfigPara.mCaptureWidth, mConfigPara.mCaptureHeight);
    mpRecord->setVideoEncoder(PT_H265);
    mpRecord->setVideoEncodingBitRate(mConfigPara.mBitsRate);
    if (!videoOnly)
    {
        alogd("set audio param");
        mpRecord->setAudioSource(EyeseeRecorder::AudioSource::MIC);
        mpRecord->setAudioSamplingRate(8000);
        mpRecord->setAudioChannels(1);
        mpRecord->setAudioEncodingBitRate(12200);
        mpRecord->setAudioEncoder(PT_AAC);
    }
    mpRecord->prepare();
    mpRecord->start();

    alogd("will record [%d]s!", mConfigPara.mTestDuration);
    usleep(mConfigPara.mTestDuration * 1000 * 1000);
    alogd("record done! stop()!");

    //stop record
    mpRecord->stop();
    delete mpRecord;
    mpRecord = NULL;
    
    // 关闭摄像机
    alogd("Stop EyeseeCamera Preview");
    mpCamera->stopRender(1);
    mpCamera->stopChannel(1);
    mpCamera->releaseChannel(1);
    mpCamera->closeChannel(1);
    mpCamera->stopDevice(); 
    mpCamera->releaseDevice(); 
    EyeseeCamera::close(mpCamera);
    mpCamera = NULL;

    // 关闭显示资源
    AW_MPI_VO_DisableVideoLayer(mVoLayer);
    AW_MPI_VO_RemoveOutsideVideoLayer(mUILayer);
    AW_MPI_VO_Disable(mVoDev);
    
    //exit mpp system
    AW_MPI_SYS_Exit(); 
    return 0;
    
    alogd("test done, quit!");
    return 0;
}

