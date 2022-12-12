//#define LOG_NDEBUG 0
#define LOG_TAG "sample_DualRecord"
#include <utils/plat_log.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>

#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

#include "sample_DualRecord_config.h"
#include "sample_DualRecord.h"

using namespace std;
using namespace EyeseeLinux;

SampleDualRecordContext *pSampleDualRecordContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleDualRecordContext!=NULL)
    {
        cdx_sem_up(&pSampleDualRecordContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0])
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
            }
            break;
        }
        default:
        {
            aloge("fatal error! unknown info[0x%x] from channel[%d]", info, chnId);
            bHandleInfoFlag = false;
            break;
        }
    }
    return bHandleInfoFlag;
}

void EyeseeCameraListener::onPictureTaken(int chnId, const void *data, int size, EyeseeCamera *pCamera)
{
    int ret = -1;
    aloge("fatal error! channel %d picture data size %d, not implement in this sample.", chnId, size);
}

EyeseeCameraListener::EyeseeCameraListener(SampleDualRecordContext *pContext)
    : mpContext(pContext)
{
}

void EyeseeRecorderListener::onError(EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);
    switch(what)
    {
        case EyeseeRecorder::MEDIA_ERROR_SERVER_DIED:
            break;
        default:
            break;
    }
}

void EyeseeRecorderListener::onInfo(EyeseeRecorder *pMr, int what, int extra)
{
    switch(what)
    {
        case EyeseeRecorder::MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD:
        {
            aloge("fatal error! We don't test this function in this sample! why receive onInfo message: need_set_next_fd, muxer_id[%d]?", extra);
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done,  muxer_id[%d]", extra);
            break;
        }
        default:
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
    }
}

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    aloge("fatal error! Do not test CallbackOut!");
}

EyeseeRecorderListener::EyeseeRecorderListener(SampleDualRecordContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleDualRecordContext::SampleDualRecordContext()
    :mCameraListener(this)
    ,mRecorderListener(this)
    ,mCameraListener1(this)
    ,mRecorderListener1(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(1, 0);
    mpCamera = NULL;
    mpRecorder = NULL;
    mpRecorder1 = NULL;
    bzero(&mConfigPara,sizeof(mConfigPara));
}

SampleDualRecordContext::~SampleDualRecordContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
    if(mpRecorder!=NULL)
    {
        aloge("fatal error! EyeseeRecorder is not destruct!");
    }
    if(mpRecorder1!=NULL)
    {
        aloge("fatal error! EyeseeRecorder1 is not destruct!");
    }
}

status_t SampleDualRecordContext::ParseCmdLine(int argc, char *argv[])
{
    alogd("this program path:[%s], arg number is [%d]", argv[0], argc);
    status_t ret = NO_ERROR;
    int i=1;
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                std::string errorString;
                errorString = "fatal error! use -h to learn how to set parameter!!!";
                cout<<errorString<<endl;
                ret = -1;
                break;
            }
            mCmdLinePara.mConfigFilePath = argv[i];
        }
        else if(!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_DualRecord.conf\n";
            cout<<helpString<<endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += ", type -h to get how to set parameter!";
            cout<<ignoreString<<endl;
        }
        i++;
    }
    return ret;
}
status_t SampleDualRecordContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mDigitalZoom = 0;
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mFrameRate = 25;
        mConfigPara.mDstFrameRate = mConfigPara.mFrameRate;
        mConfigPara.mTimeLapseFps = 0;
        mConfigPara.mEncodeType0 = PT_H265;
        mConfigPara.mEncodeType0 = PT_H265;
        mConfigPara.mAudioEncodeType = PT_MAX;
        mConfigPara.mEncodeDuration = 20;
        mConfigPara.mEncodeFolderPath = "/mnt/extsd/sample_DualRecord";
        mConfigPara.mEncodeWidth0 = 1920;
        mConfigPara.mEncodeHeight0 = 1080;
        mConfigPara.mEncodeBitrate0 = 14*1024*1024;
        mConfigPara.mFilePath0 = "file0.mp4";
        mConfigPara.mEncodeWidth1 = 1920;
        mConfigPara.mEncodeHeight1 = 1080;
        mConfigPara.mEncodeBitrate1 = 14*1024*1024;
        mConfigPara.mFilePath1 = "file1.mp4";
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_CAPTURE_HEIGHT, 0);
    mConfigPara.mDigitalZoom = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_DIGITAL_ZOOM, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_DUALRECORD_KEY_PIC_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_FRAME_RATE, 0);
    mConfigPara.mDstFrameRate = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_DST_FRAME_RATE, 0);
    if(mConfigPara.mDstFrameRate <= 0)
    {
        mConfigPara.mDstFrameRate = mConfigPara.mFrameRate;
    }
    mConfigPara.mTimeLapseFps = GetConfParaDouble(&stConfParser, SAMPLE_DUALRECORD_KEY_TIMELAPSE_FPS, 0);
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_TYPE0, NULL);
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mEncodeType0 = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mEncodeType0 = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mEncodeType0 = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mEncodeType0 = PT_H264;
    }
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_TYPE1, NULL);
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mEncodeType1 = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mEncodeType1 = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mEncodeType1 = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mEncodeType1 = PT_H264;
    }
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_DUALRECORD_KEY_AUDIO_ENCODE_TYPE, NULL);
    if(pStrEncodeType!=NULL)
    {
        if(!strcmp(pStrEncodeType, "aac"))
        {
            mConfigPara.mAudioEncodeType = PT_AAC;
        }
        else if(!strcmp(pStrEncodeType, "mp3"))
        {
            mConfigPara.mAudioEncodeType = PT_MP3;
        }
        else if(!strcmp(pStrEncodeType, ""))
        {
            alogd("user set no audio.");
            mConfigPara.mAudioEncodeType = PT_MAX;
        }
        else
        {
            aloge("fatal error! conf file audio encode type is [%s]?", pStrEncodeType);
            mConfigPara.mAudioEncodeType = PT_MAX;
        }
    }
    else
    {
        aloge("fatal error! not find key audio_encode_type?");
        mConfigPara.mAudioEncodeType = PT_MAX;
    }
    mConfigPara.mEncodeDuration = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_DURATION, 0);
    mConfigPara.mEncodeFolderPath = GetConfParaString(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_FOLDER, NULL);

    mConfigPara.mEncodeWidth0 = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_WIDTH0, 0);
    mConfigPara.mEncodeHeight0 = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_HEIGHT0, 0);
    mConfigPara.mEncodeBitrate0 = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_BITRATE0, 0)*1024*1024;
    mConfigPara.mFilePath0 = GetConfParaString(&stConfParser, SAMPLE_DUALRECORD_KEY_FILE_PATH0, NULL);
    mConfigPara.mEncodeWidth1 = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_WIDTH1, 0);
    mConfigPara.mEncodeHeight1 = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_HEIGHT1, 0);
    mConfigPara.mEncodeBitrate1 = GetConfParaInt(&stConfParser, SAMPLE_DUALRECORD_KEY_ENCODE_BITRATE1, 0)*1024*1024;
    mConfigPara.mFilePath1 = GetConfParaString(&stConfParser, SAMPLE_DUALRECORD_KEY_FILE_PATH1, NULL);
    alogd("path0[%s], path1[%s]", mConfigPara.mFilePath0.c_str(), mConfigPara.mFilePath1.c_str());
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleDualRecordContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("jpeg path is not set!");
        return UNKNOWN_ERROR;
    }
    const char* pFolderPath = strFolderPath.c_str();
    //check folder existence
    struct stat sb;
    if (stat(pFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pFolderPath, sb.st_mode);
            return UNKNOWN_ERROR;
        }
    }
    //create folder if necessary
    int ret = mkdir(pFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pFolderPath);
        return UNKNOWN_ERROR;
    }
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_DualRecord!"<<endl;
    SampleDualRecordContext stContext;
    pSampleDualRecordContext = &stContext;
    //parse command line param
    if(stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }
    //parse config file.
    if(stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }
    //check folder existence, create folder if necessary
    if(SUCCESS != stContext.CreateFolder(stContext.mConfigPara.mEncodeFolderPath))
    {
        return -1;
    }
    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //config camera.
    {
        int cameraId = 0;
        EyeseeCamera::clearCamerasConfiguration();
        CameraInfo cameraInfo;
        cameraInfo.facing                 = CAMERA_FACING_BACK;
        cameraInfo.orientation            = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType      = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn   = 1;
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
    }
    int cameraId;
    CameraInfo& cameraInfo = stContext.mCameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
    stContext.mpCamera->setInfoCallback(&stContext.mCameraListener);
    stContext.mpCamera->prepareDevice();
    stContext.mpCamera->startDevice();

    CameraParameters cameraParam;
    stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], true);
    stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    SIZE_S captureSize = {stContext.mConfigPara.mCaptureWidth, stContext.mConfigPara.mCaptureHeight};
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setDisplayFrameRate(stContext.mConfigPara.mDstFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
    cameraParam.setVideoBufferNumber(5);
    cameraParam.setPreviewRotation(0);
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer.
    VO_LAYER hlay = 0;
    while(hlay < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay))
        {
            break;
        }
        hlay++;
    }
    if(hlay >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
    }
    AW_MPI_VO_GetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X      = 0;
    stContext.mLayerAttr.stDispRect.Y      = 0;
    stContext.mLayerAttr.stDispRect.Width  = 640;
    stContext.mLayerAttr.stDispRect.Height = 480;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
    stContext.mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], stContext.mVoLayer);
    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    int ret = cdx_sem_down_timedwait(&stContext.mSemRenderStart, 5000);
    if(0 == ret)
    {
        alogd("app receive message that camera start render!");
    }
    else if(ETIMEDOUT == ret)
    {
        aloge("fatal error! wait render start timeout");
    }
    else
    {
        aloge("fatal error! other error[0x%x]", ret);
    }

    //record first file.
    stContext.mpRecorder = new EyeseeRecorder();
    stContext.mpRecorder->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder->setOnErrorListener(&stContext.mRecorderListener);
    stContext.mpRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    stContext.mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    std::string strFilePath = stContext.mConfigPara.mEncodeFolderPath + '/' + stContext.mConfigPara.mFilePath0;
    int nFallocateLen = 0;
    //nFallocateLen = 100*1024*1024;
    stContext.mpRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, (char*)strFilePath.c_str(), nFallocateLen, false);
    alogd("setVideoFrameRate=[%d]", stContext.mConfigPara.mDstFrameRate);
    stContext.mpRecorder->setVideoFrameRate(stContext.mConfigPara.mDstFrameRate);
    alogd("setEncodeVideoSize=[%dx%d]", stContext.mConfigPara.mEncodeWidth0, stContext.mConfigPara.mEncodeHeight0);
    stContext.mpRecorder->setVideoSize(stContext.mConfigPara.mEncodeWidth0, stContext.mConfigPara.mEncodeHeight0);
    stContext.mpRecorder->setVideoEncoder(stContext.mConfigPara.mEncodeType0);
    stContext.mpRecorder->setVideoEncodingBitRate(stContext.mConfigPara.mEncodeBitrate0);
    alogd("setSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    if(stContext.mConfigPara.mAudioEncodeType != PT_MAX)
    {
        stContext.mpRecorder->setAudioSamplingRate(8000);
        stContext.mpRecorder->setAudioChannels(1);
        stContext.mpRecorder->setAudioEncodingBitRate(12200);
        stContext.mpRecorder->setAudioEncoder(PT_AAC);
    }
    stContext.mpRecorder->setCaptureRate(stContext.mConfigPara.mTimeLapseFps);
    alogd("prepare()!");
    stContext.mpRecorder->prepare();
    alogd("start()!");
    stContext.mpRecorder->start();
    alogd("file0 will record [%d]s!", stContext.mConfigPara.mEncodeDuration);

    //record second file.
    if(!stContext.mConfigPara.mFilePath1.empty())
    {
        stContext.mpRecorder1 = new EyeseeRecorder();
        stContext.mpRecorder1->setOnInfoListener(&stContext.mRecorderListener1);
        stContext.mpRecorder1->setOnDataListener(&stContext.mRecorderListener1);
        stContext.mpRecorder1->setOnErrorListener(&stContext.mRecorderListener1);
        stContext.mpRecorder1->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        stContext.mpRecorder1->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
        stContext.mpRecorder1->setAudioSource(EyeseeRecorder::AudioSource::MIC);
        std::string strFilePath1 = stContext.mConfigPara.mEncodeFolderPath + '/' + stContext.mConfigPara.mFilePath1;
        stContext.mpRecorder1->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, (char*)strFilePath1.c_str(), nFallocateLen, false);
        alogd("setVideoFrameRate=[%d]", stContext.mConfigPara.mDstFrameRate);
        stContext.mpRecorder1->setVideoFrameRate(stContext.mConfigPara.mDstFrameRate);
        alogd("setEncodeVideoSize1=[%dx%d]", stContext.mConfigPara.mEncodeWidth1, stContext.mConfigPara.mEncodeHeight1);
        stContext.mpRecorder1->setVideoSize(stContext.mConfigPara.mEncodeWidth1, stContext.mConfigPara.mEncodeHeight1);
        stContext.mpRecorder1->setVideoEncoder(stContext.mConfigPara.mEncodeType1);
        stContext.mpRecorder1->setVideoEncodingBitRate(stContext.mConfigPara.mEncodeBitrate1);
        alogd("setSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        if(stContext.mConfigPara.mAudioEncodeType != PT_MAX)
        {
            stContext.mpRecorder1->setAudioSamplingRate(8000);
            stContext.mpRecorder1->setAudioChannels(1);
            stContext.mpRecorder1->setAudioEncodingBitRate(12200);
            stContext.mpRecorder1->setAudioEncoder(PT_AAC);
        }
        stContext.mpRecorder1->setCaptureRate(stContext.mConfigPara.mTimeLapseFps);
        alogd("prepare()!");
        stContext.mpRecorder1->prepare();
        alogd("start()!");
        stContext.mpRecorder1->start();
        alogd("file1 will record [%d]s!", stContext.mConfigPara.mEncodeDuration);
    }

    if(stContext.mConfigPara.mDigitalZoom > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, 5*1000);
        stContext.mpCamera->getParameters(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
        int oldZoom = cameraParam.getZoom();
        cameraParam.setZoom(stContext.mConfigPara.mDigitalZoom);
        stContext.mpCamera->setParameters(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
        alogd("change digital zoom[%d]->[%d]", oldZoom, stContext.mConfigPara.mDigitalZoom);
    }

    if(stContext.mConfigPara.mEncodeDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mEncodeDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

    alogd("record done! stop()!");
    //stop mHMR0
    stContext.mpRecorder->stop();
    delete stContext.mpRecorder;
    stContext.mpRecorder = NULL;
    if(stContext.mpRecorder1)
    {
        stContext.mpRecorder1->stop();
        delete stContext.mpRecorder1;
        stContext.mpRecorder1 = NULL;
    }

    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
    stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->stopDevice();
    stContext.mpCamera->releaseDevice();
    EyeseeCamera::close(stContext.mpCamera);
    stContext.mpCamera = NULL;
    //close vo
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;
    //exit mpp system
    AW_MPI_SYS_Exit();
    cout<<"bye, sample_EncodeResolutionChange!"<<endl;
    return result;
}

