//#define LOG_NDEBUG 0
#define LOG_TAG "sample_OSD"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vo.h>
#include <BITMAP_S.h>

#include "sample_OSD_config.h"
#include "sample_OSD.h"

using namespace std;
using namespace EyeseeLinux;

static SampleOSDContext *pSampleOSDContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleOSDContext!=NULL)
    {
        cdx_sem_up(&pSampleOSDContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1])
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
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
    alogd("channel %d picture data size %d", chnId, size);
    if(chnId != mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0])
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    }
    char picName[64];
    sprintf(picName, "pic[%02d].jpg", 0 /*mpContext->mPicNum++*/);
    std::string PicFullPath = mpContext->mConfigPara.mEncodeFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
}

EyeseeCameraListener::EyeseeCameraListener(SampleOSDContext *pContext)
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

EyeseeRecorderListener::EyeseeRecorderListener(SampleOSDContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleOSDContext::SampleOSDContext()
    :mCameraListener(this)
    , mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = -1;
    mVoLayer = -1;
    mpCamera = NULL;
    mpRecorder = NULL;
    mVippOverlayHandle = MM_INVALID_HANDLE;
    mVippCoverHandle = MM_INVALID_HANDLE;
    mVippOrlHandle = MM_INVALID_HANDLE;
    mVencOverlayHandle = MM_INVALID_HANDLE;
    mVencCoverHandle = MM_INVALID_HANDLE;
    mVencOrlHandle = MM_INVALID_HANDLE;
    mVencId = -1;
    bzero(&mConfigPara,sizeof(mConfigPara));
}

SampleOSDContext::~SampleOSDContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
}

status_t SampleOSDContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_OSD.conf\n";
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

status_t SampleOSDContext::loadConfig()
{
    int ret;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mPreviewWidth = 1920;
        mConfigPara.mPreviewHeight = 1080;
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mDigitalZoom = 0;
        mConfigPara.mFrameRate = 30;
        mConfigPara.mDisplayFrameRate = 0;
        mConfigPara.mDisplayWidth = 480;
        mConfigPara.mDisplayHeight = 640;
        mConfigPara.mPreviewRotation = 0;

        mConfigPara.mVippOverlayX = 16;
        mConfigPara.mVippOverlayY = 16;
        mConfigPara.mVippOverlayW = 480;
        mConfigPara.mVippOverlayH = 480;

        mConfigPara.mVippCoverX = 64;
        mConfigPara.mVippCoverY = 64;
        mConfigPara.mVippCoverW = 128;
        mConfigPara.mVippCoverH = 128;
        mConfigPara.mVippCoverColor = 0x0000ff;

        mConfigPara.mVippOrlX = 128;
        mConfigPara.mVippOrlY = 128;
        mConfigPara.mVippOrlW = 256;
        mConfigPara.mVippOrlH = 256;
        mConfigPara.mVippOrlColor = 0xff0000;

        mConfigPara.mVencOverlayX = 32;
        mConfigPara.mVencOverlayY = 32;
        mConfigPara.mVencOverlayW = 128;
        mConfigPara.mVencOverlayH = 128;

        mConfigPara.mVencCoverX = 16;
        mConfigPara.mVencCoverY = 16;
        mConfigPara.mVencCoverW = 480;
        mConfigPara.mVencCoverH = 480;
        mConfigPara.mVencCoverColor = 0x00ff00;

        mConfigPara.mVencOrlX = 128;
        mConfigPara.mVencOrlY = 128;
        mConfigPara.mVencOrlW = 256;
        mConfigPara.mVencOrlH = 256;
        mConfigPara.mVencOrlColor = 0xff0000;

        mConfigPara.mEncodeType = PT_H265;
        mConfigPara.mAudioEncodeType = PT_AAC;
        mConfigPara.mEncodeWidth = 1920;
        mConfigPara.mEncodeHeight = 1080;
        mConfigPara.mEncodeBitrate = 6*1024*1024;
        mConfigPara.mEncodeFolderPath = "/mnt/extsd/sample_OSD_Files";
        mConfigPara.mFilePath = "file_osd.mp4";
        mConfigPara.mTestDuration = 20;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_CAPTURE_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_OSD_KEY_CAPTURE_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "afbc"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mPreviewWidth = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_PREVIEW_WIDTH, 0);
    mConfigPara.mPreviewHeight = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_PREVIEW_HEIGHT, 0);
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_OSD_KEY_PREVIEW_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "afbc"))
    {
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mDigitalZoom = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_DIGITAL_ZOOM, 0);

    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_FRAME_RATE, 0);
    mConfigPara.mDisplayFrameRate = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_DISPLAY_FRAME_RATE, 0);
    mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_DISPLAY_WIDTH, 0);
    mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_DISPLAY_HEIGHT, 0);
    mConfigPara.mPreviewRotation = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_PREVIEW_ROTATION, 0);

    mConfigPara.mVippOverlayX = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_OVERLAY_X, 0);
    mConfigPara.mVippOverlayY = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_OVERLAY_Y, 0);
    mConfigPara.mVippOverlayW = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_OVERLAY_W, 0);
    mConfigPara.mVippOverlayH = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_OVERLAY_H, 0);

    mConfigPara.mVippCoverX = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_COVER_X, 0);
    mConfigPara.mVippCoverY = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_COVER_Y, 0);
    mConfigPara.mVippCoverW = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_COVER_W, 0);
    mConfigPara.mVippCoverH = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_COVER_H, 0);
    mConfigPara.mVippCoverColor = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_COVER_COLOR, 0);

    mConfigPara.mVippOrlX = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_ORL_X, 0);
    mConfigPara.mVippOrlY = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_ORL_Y, 0);
    mConfigPara.mVippOrlW = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_ORL_W, 0);
    mConfigPara.mVippOrlH = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_ORL_H, 0);
    mConfigPara.mVippOrlColor = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VIPP_ORL_COLOR, 0);

    mConfigPara.mVencOverlayX = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_OVERLAY_X, 0);
    mConfigPara.mVencOverlayY = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_OVERLAY_Y, 0);
    mConfigPara.mVencOverlayW = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_OVERLAY_W, 0);
    mConfigPara.mVencOverlayH = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_OVERLAY_H, 0);

    mConfigPara.mVencCoverX = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_COVER_X, 0);
    mConfigPara.mVencCoverY = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_COVER_Y, 0);
    mConfigPara.mVencCoverW = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_COVER_W, 0);
    mConfigPara.mVencCoverH = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_COVER_H, 0);
    mConfigPara.mVencCoverColor = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_COVER_COLOR, 0);

    mConfigPara.mVencOrlX = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_ORL_X, 0);
    mConfigPara.mVencOrlY = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_ORL_Y, 0);
    mConfigPara.mVencOrlW = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_ORL_W, 0);
    mConfigPara.mVencOrlH = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_ORL_H, 0);
    mConfigPara.mVencOrlColor = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_VENC_ORL_COLOR, 0);

    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_OSD_KEY_ENCODE_TYPE, NULL);
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mEncodeType = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mEncodeType = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mEncodeType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mEncodeType = PT_H264;
    }
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_OSD_KEY_AUDIO_ENCODE_TYPE, NULL);
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
        mConfigPara.mAudioEncodeType = PT_MAX;
    }
    mConfigPara.mEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_ENCODE_WIDTH, 0);
    mConfigPara.mEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_ENCODE_HEIGHT, 0);
    mConfigPara.mEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_ENCODE_BITRATE, 0)*1024*1024;
    mConfigPara.mEncodeFolderPath = GetConfParaString(&stConfParser, SAMPLE_OSD_KEY_ENCODE_FOLDER, NULL);
    mConfigPara.mFilePath = GetConfParaString(&stConfParser, SAMPLE_OSD_KEY_FILE_PATH, NULL);

    mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_OSD_KEY_TEST_DURATION, 0);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleOSDContext::CreateFolder(const std::string& strFolderPath)
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
    cout<<"hello, sample_OSD!"<<endl;
    SampleOSDContext stContext;

    pSampleOSDContext = &stContext;
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
    
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        ISPGeometry tmpISPGeometry;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 0;
        tmpISPGeometry.mISPDev = 0;
        tmpISPGeometry.mScalerOutChns.clear();
        tmpISPGeometry.mScalerOutChns.push_back(0);
        tmpISPGeometry.mScalerOutChns.push_back(1);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(tmpISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    int cameraId;
    CameraInfo &cameraInfo = stContext.mCameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
    stContext.mpCamera->setInfoCallback(&stContext.mCameraListener);
    stContext.mpCamera->prepareDevice();
    stContext.mpCamera->startDevice();

    CameraParameters cameraParam;
    stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], true);
    stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    SIZE_S captureSize;
    captureSize.Width = stContext.mConfigPara.mCaptureWidth;
    captureSize.Height = stContext.mConfigPara.mCaptureHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mCaptureFormat);
    cameraParam.setVideoBufferNumber(5);
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    stContext.mpCamera->openChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], false);
    stContext.mpCamera->getParameters(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    SIZE_S previewSize;
    previewSize.Width = stContext.mConfigPara.mPreviewWidth;
    previewSize.Height = stContext.mConfigPara.mPreviewHeight;
    cameraParam.setVideoSize(previewSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mPreviewFormat);
    cameraParam.setVideoBufferNumber(5);
    stContext.mpCamera->setParameters(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    stContext.mpCamera->prepareChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);

    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer.
    VO_PUB_ATTR_S stPubAttr;
    AW_MPI_VO_GetPubAttr(stContext.mVoDev, &stPubAttr);
    stPubAttr.enIntfType = VO_INTF_LCD;
    stPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(stContext.mVoDev, &stPubAttr);
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
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
    stContext.mpCamera->setChannelDisplay(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], stContext.mVoLayer);

    stContext.mpCamera->startChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->startChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    int ret;
    ret = cdx_sem_down_timedwait(&stContext.mSemRenderStart, 5000);
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

    if(stContext.mConfigPara.mDigitalZoom > 0)
    {
        ret = cdx_sem_down_timedwait(&stContext.mSemExit, 5*1000);
        if(0 == ret)
        {
            alogd("user want to exit!");
            //goto _exit0;
        }
        stContext.mpCamera->getParameters(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        int oldZoom = cameraParam.getZoom();
        cameraParam.setZoom(stContext.mConfigPara.mDigitalZoom);
        stContext.mpCamera->setParameters(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        alogd("change digital zoom[%d]->[%d]", oldZoom, stContext.mConfigPara.mDigitalZoom);
    }

    //Vipp test overlay
    if(stContext.mConfigPara.mVippOverlayW!=0 && stContext.mConfigPara.mVippOverlayH!=0)
    {
        RGN_ATTR_S stRegion;
        memset(&stRegion, 0, sizeof(RGN_ATTR_S));
        stRegion.enType = OVERLAY_RGN;
        stRegion.unAttr.stOverlay.mPixelFmt = MM_PIXEL_FORMAT_RGB_8888;
        stRegion.unAttr.stOverlay.mSize = {(unsigned int)stContext.mConfigPara.mVippOverlayW, (unsigned int)stContext.mConfigPara.mVippOverlayH};
        stContext.mVippOverlayHandle = stContext.mpCamera->createRegion(&stRegion);
        BITMAP_S stBmp;
        memset(&stBmp, 0, sizeof(BITMAP_S));
        stBmp.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
        stBmp.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
        stBmp.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
        int nSize = BITMAP_S_GetdataSize(&stBmp);
        stBmp.mpData = malloc(nSize);
        if(NULL == stBmp.mpData)
        {
            aloge("fatal error! malloc fail!");
        }
        memset(stBmp.mpData, 0xCC, nSize);
        stContext.mpCamera->setRegionBitmap(stContext.mVippOverlayHandle, &stBmp);
        free(stBmp.mpData);
        RGN_CHN_ATTR_S stRgnChnAttr = {0};
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = stRegion.enType;
        stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = stContext.mConfigPara.mVippOverlayX;
        stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = stContext.mConfigPara.mVippOverlayY;
        stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
        stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TRUE;
        stContext.mpCamera->attachRegionToChannel(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
        stContext.mpCamera->attachRegionToChannel(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
    }
    //Vipp test cover
    if(stContext.mConfigPara.mVippCoverW!=0 && stContext.mConfigPara.mVippCoverH!=0)
    {
        RGN_ATTR_S stRegion;
        memset(&stRegion, 0, sizeof(RGN_ATTR_S));
        stRegion.enType = COVER_RGN;
        stContext.mVippCoverHandle = stContext.mpCamera->createRegion(&stRegion);
        RGN_CHN_ATTR_S stRgnChnAttr = {0};
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = stRegion.enType;
        stRgnChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = stContext.mConfigPara.mVippCoverX;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = stContext.mConfigPara.mVippCoverY;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width = stContext.mConfigPara.mVippCoverW;
        stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height = stContext.mConfigPara.mVippCoverH;
        stRgnChnAttr.unChnAttr.stCoverChn.mColor = stContext.mConfigPara.mVippCoverColor;
        stRgnChnAttr.unChnAttr.stCoverChn.mLayer = 0;
        stContext.mpCamera->attachRegionToChannel(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
        stContext.mpCamera->attachRegionToChannel(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
    }
    //Vipp test orl
    if (stContext.mConfigPara.mVippOrlW != 0 && stContext.mConfigPara.mVippOrlH != 0)
    {
        RGN_ATTR_S stRegion;
        memset(&stRegion, 0, sizeof(RGN_ATTR_S));
        stRegion.enType = ORL_RGN;
        stContext.mVippOrlHandle = stContext.mpCamera->createRegion(&stRegion);
        RGN_CHN_ATTR_S stRgnChnAttr = {0};
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = stRegion.enType;
        stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.X = stContext.mConfigPara.mVippOrlX;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y = stContext.mConfigPara.mVippOrlY;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width = stContext.mConfigPara.mVippOrlW;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height = stContext.mConfigPara.mVippOrlH;
        stRgnChnAttr.unChnAttr.stOrlChn.mColor = stContext.mConfigPara.mVippOrlColor;
        stRgnChnAttr.unChnAttr.stOrlChn.mThick = 6;
        stRgnChnAttr.unChnAttr.stOrlChn.mLayer = 0;
        stContext.mpCamera->attachRegionToChannel(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
        stContext.mpCamera->attachRegionToChannel(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
    }

    if (!stContext.mConfigPara.mFilePath.empty())
    {
        stContext.mpRecorder = new EyeseeRecorder();
        stContext.mpRecorder->setOnInfoListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnDataListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnErrorListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        stContext.mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
        stContext.mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);

        std::string strFilePath = stContext.mConfigPara.mEncodeFolderPath + '/' + stContext.mConfigPara.mFilePath;
        SinkParam stSinkParam;
        memset(&stSinkParam, 0, sizeof(SinkParam));
        stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
        stSinkParam.mOutputFd = -1;
        stSinkParam.mOutputPath = (char*)strFilePath.c_str();
        stSinkParam.mFallocateLen = 0;
        stSinkParam.mMaxDurationMs = 0;
        stSinkParam.bCallbackOutFlag = false;
        stSinkParam.bBufFromCacheFlag = false;
        stContext.mpRecorder->addOutputSink(&stSinkParam);
        stContext.mpRecorder->setCaptureRate(0);

        VencParameters stVencParam;

        VencParameters::VEncAttr stVEncAttr;
        memset(&stVEncAttr, 0, sizeof(VencParameters::VEncAttr));
        stVEncAttr.mType = stContext.mConfigPara.mEncodeType;
        if (PT_H264 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH264.mProfile = 1;
            stVEncAttr.mAttrH264.mLevel = H264_LEVEL_51;
        }
        else if (PT_H265 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH265.mProfile = 0;
            stVEncAttr.mAttrH265.mLevel = H265_LEVEL_62;
        }
        else if (PT_MJPEG == stVEncAttr.mType)
        {
        }
        else
        {
            aloge("fatal error! unsupport venctype[0x%x]", stVEncAttr.mType);
        }
        stVencParam.setVEncAttr(stVEncAttr);
        stVencParam.setVideoEncoder(stContext.mConfigPara.mEncodeType);
        stVencParam.setVideoFrameRate(stContext.mConfigPara.mFrameRate);
        SIZE_S stVideoSize;
        stVideoSize.Width = stContext.mConfigPara.mEncodeWidth;
        stVideoSize.Height = stContext.mConfigPara.mEncodeHeight;
        stVencParam.setVideoSize(stVideoSize);
        stVencParam.setVideoEncodingIFramesNumberInterVal(stContext.mConfigPara.mFrameRate);
        stVencParam.enableVideoEncodingPIntra(1);
        //stVencParam.setVideoEncodingRateControlMode(EyeseeLinux::VencParameters::VideoRCMode_CBR);
        VencParameters::VEncBitRateControlAttr stRcAttr;
        stRcAttr.mVEncType = stContext.mConfigPara.mEncodeType;
        stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
        switch (stRcAttr.mVEncType)
        {
            case PT_H264:
            {
                stRcAttr.mAttrH264Cbr.mBitRate = stContext.mConfigPara.mEncodeBitrate;
                stRcAttr.mAttrH264Cbr.mMaxQp = 51;
                stRcAttr.mAttrH264Cbr.mMinQp = 1;
                break;
            }
            case PT_H265:
            {
                stRcAttr.mAttrH265Cbr.mBitRate = stContext.mConfigPara.mEncodeBitrate;
                stRcAttr.mAttrH265Cbr.mMaxQp = 51;
                stRcAttr.mAttrH265Cbr.mMinQp = 1;
                break;
            }
            case PT_JPEG:
            {
                stRcAttr.mAttrMjpegCbr.mBitRate = stContext.mConfigPara.mEncodeBitrate;
                break;
            }
            default:
            {
                aloge("fatal error! unsupport vencType[0x%x]", stContext.mConfigPara.mEncodeType);
                break;
            }
        }
        stVencParam.setVEncBitRateControlAttr(stRcAttr);

        VENC_GOP_ATTR_S GopAttr;
        GopAttr = stVencParam.getGopAttr();
        GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        stVencParam.setGopAttr(GopAttr);

        VENC_SUPERFRAME_CFG_S stSuperFrameConfig = {
            .enSuperFrmMode = SUPERFRM_NONE,
            .SuperIFrmBitsThr = 0,
            .SuperPFrmBitsThr = 0,
            .SuperBFrmBitsThr = 0,
        };
        stVencParam.setVencSuperFrameConfig(stSuperFrameConfig);

        stContext.mVencId = 0;
        stContext.mpRecorder->setVencParameters(stContext.mVencId, &stVencParam);
        stContext.mpRecorder->bindVeVipp(stContext.mVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

        if(stContext.mConfigPara.mAudioEncodeType != PT_MAX)
        {
            stContext.mpRecorder->setAudioSamplingRate(8000);
            stContext.mpRecorder->setAudioChannels(1);
            stContext.mpRecorder->setAudioEncodingBitRate(12200);
            stContext.mpRecorder->setAudioEncoder(stContext.mConfigPara.mAudioEncodeType);
        }

        alogd("prepare()!");
        stContext.mpRecorder->prepare();
        alogd("start()!");
        stContext.mpRecorder->start();
        alogd("will record [%d]s!", stContext.mConfigPara.mTestDuration);

        //venc test overlay
        if (stContext.mConfigPara.mVencOverlayW != 0 && stContext.mConfigPara.mVencOverlayH != 0)
        {
            RGN_ATTR_S stRegion;
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = OVERLAY_RGN;
            stRegion.unAttr.stOverlay.mPixelFmt = MM_PIXEL_FORMAT_RGB_8888;
            stRegion.unAttr.stOverlay.mSize.Width = stContext.mConfigPara.mVencCoverW;
            stRegion.unAttr.stOverlay.mSize.Height = stContext.mConfigPara.mVencCoverH;
            stContext.mVencOverlayHandle = stContext.mpRecorder->createRegion(&stRegion);
            BITMAP_S stBmp;
            memset(&stBmp, 0, sizeof(BITMAP_S));
            stBmp.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
            stBmp.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
            stBmp.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
            int nSize = BITMAP_S_GetdataSize(&stBmp);
            stBmp.mpData = malloc(nSize);
            if(NULL == stBmp.mpData)
            {
                aloge("fatal error! malloc fail!");
            }
            memset(stBmp.mpData, 0xbb, nSize);
            stContext.mpRecorder->setRegionBitmap(stContext.mVencOverlayHandle, &stBmp);
            free(stBmp.mpData);
            RGN_CHN_ATTR_S stRgnChnAttr = {0};
            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = stContext.mConfigPara.mVencCoverX+160;
            stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = stContext.mConfigPara.mVencCoverY+160;
            stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
            stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = FALSE;
            stContext.mpRecorder->attachRegionToVenc(stContext.mVencId, stContext.mVencOverlayHandle, &stRgnChnAttr);
        }
        //venc test cover
        if(stContext.mConfigPara.mVencCoverW!=0 && stContext.mConfigPara.mVencCoverH!=0)
        {
            RGN_ATTR_S stRegion;
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = COVER_RGN;
            stContext.mVencCoverHandle = stContext.mpRecorder->createRegion(&stRegion);
            RGN_CHN_ATTR_S stRgnChnAttr = {0};
            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            stRgnChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = stContext.mConfigPara.mVencCoverX +96;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = stContext.mConfigPara.mVencCoverY +96;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width =  (unsigned int)stContext.mConfigPara.mVencCoverW;
            stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height = (unsigned int)stContext.mConfigPara.mVencCoverH;
            stRgnChnAttr.unChnAttr.stCoverChn.mColor = stContext.mConfigPara.mVencCoverColor;
            stRgnChnAttr.unChnAttr.stCoverChn.mLayer = 0;
            stContext.mpRecorder->attachRegionToVenc(stContext.mVencId, stContext.mVencCoverHandle, &stRgnChnAttr);
        }
        //venc test orl
        if (stContext.mConfigPara.mVencOrlW != 0 && stContext.mConfigPara.mVencOrlH != 0)
        {
            RGN_ATTR_S stRegion;
            memset(&stRegion, 0, sizeof(RGN_ATTR_S));
            stRegion.enType = ORL_RGN;
            stContext.mVencOrlHandle = stContext.mpRecorder->createRegion(&stRegion);
            RGN_CHN_ATTR_S stRgnChnAttr = {0};
            stRgnChnAttr.bShow = TRUE;
            stRgnChnAttr.enType = stRegion.enType;
            stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.X = stContext.mConfigPara.mVencOrlX + 96;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y = stContext.mConfigPara.mVencOrlY + 96;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width = (unsigned int)stContext.mConfigPara.mVencOrlW;
            stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height = (unsigned int)stContext.mConfigPara.mVencOrlH;
            stRgnChnAttr.unChnAttr.stOrlChn.mColor = stContext.mConfigPara.mVencCoverColor;
            stRgnChnAttr.unChnAttr.stOrlChn.mThick = 6;
            stRgnChnAttr.unChnAttr.stOrlChn.mLayer = 0;
            stContext.mpRecorder->attachRegionToVenc(stContext.mVencId, stContext.mVencOrlHandle, &stRgnChnAttr);
        }
    }

    sleep(5);
    //test change region
    {
        RGN_CHN_ATTR_S stRgnChnAttr;
        ERRORTYPE eRet = SUCCESS;
        //change the overlay
        if (stContext.mVippOverlayHandle != MM_INVALID_HANDLE)
        {
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpCamera->getRegionDisplayAttr(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X += 96;
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y += 0;
                stContext.mpCamera->setRegionDisplayAttr(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
            }
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet =  stContext.mpCamera->getRegionDisplayAttr(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X += 96;
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y += 0;
                stContext.mpCamera->setRegionDisplayAttr(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
            }
        }
        if (stContext.mVencOverlayHandle != MM_INVALID_HANDLE)
        {
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpRecorder->getRegionDisplayAttrOfVenc(stContext.mVencId, stContext.mVencOverlayHandle, &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X += 96;
                stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y += 96;
                stContext.mpRecorder->setRegionDisplayAttrOfVenc(stContext.mVencId, stContext.mVencOverlayHandle, &stRgnChnAttr);
            }
        }
        //change the cover
        if (stContext.mVippCoverHandle != MM_INVALID_HANDLE)
        {
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpCamera->getRegionDisplayAttr(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = 0;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = 0;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width += 48;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height += 48;
                stContext.mpCamera->setRegionDisplayAttr(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
            }
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpCamera->getRegionDisplayAttr(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.X = 0;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y = 0;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width += 48;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height += 48;
                stContext.mpCamera->setRegionDisplayAttr(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
            }
        }
        if (stContext.mVencCoverHandle != MM_INVALID_HANDLE)
        {
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpRecorder->getRegionDisplayAttrOfVenc(stContext.mVencId, stContext.mVencCoverHandle, &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.X += 208;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Y += 208;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Width += 16;
                stRgnChnAttr.unChnAttr.stCoverChn.stRect.Height += 16;
                stContext.mpRecorder->setRegionDisplayAttrOfVenc(stContext.mVencId, stContext.mVencCoverHandle, &stRgnChnAttr);
            }
        }
        //change the orl
        if (stContext.mVippOrlHandle != MM_INVALID_HANDLE)
        {
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpCamera->getRegionDisplayAttr(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.X += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height += 20;
                stContext.mpCamera->setRegionDisplayAttr(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], &stRgnChnAttr);
            }
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpCamera->getRegionDisplayAttr(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.X += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height += 20;
                stContext.mpCamera->setRegionDisplayAttr(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], &stRgnChnAttr);
            }
        }
        if (stContext.mVencOrlHandle != MM_INVALID_HANDLE)
        {
            memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
            eRet = stContext.mpRecorder->getRegionDisplayAttrOfVenc(stContext.mVencId, stContext.mVencOrlHandle, &stRgnChnAttr);
            if (SUCCESS == eRet)
            {
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.X += 30;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y += 30;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width += 60;
                stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height += 20;
                stContext.mpRecorder->setRegionDisplayAttrOfVenc(stContext.mVencId, stContext.mVencOrlHandle, &stRgnChnAttr);
            }

        }
    }

    if(stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

    //detach region from venc and destroy
    if(stContext.mVencOverlayHandle != MM_INVALID_HANDLE)
    {
        stContext.mpRecorder->detachRegionFromVenc(stContext.mVencId, stContext.mVencOverlayHandle);
        stContext.mpRecorder->destroyRegion(stContext.mVencOverlayHandle);
        stContext.mVencOverlayHandle = MM_INVALID_HANDLE;
    }
    if(stContext.mVencCoverHandle != MM_INVALID_HANDLE)
    {
        stContext.mpRecorder->detachRegionFromVenc(stContext.mVencId, stContext.mVencCoverHandle);
        stContext.mpRecorder->destroyRegion(stContext.mVencCoverHandle);
        stContext.mVencCoverHandle = MM_INVALID_HANDLE;
    }
    if (stContext.mVencOrlHandle != MM_INVALID_HANDLE)
    {
        stContext.mpRecorder->detachRegionFromVenc(stContext.mVencId, stContext.mVencOrlHandle);
        stContext.mpRecorder->destroyRegion(stContext.mVencOrlHandle);
        stContext.mVencOrlHandle = MM_INVALID_HANDLE;
    }

    if (stContext.mpRecorder != NULL)
    {
        alogd("record done stop()!");
        stContext.mpRecorder->stop();
        delete stContext.mpRecorder;
        stContext.mpRecorder = NULL;
    }

    //detach region from vipp and destroy
    if(stContext.mVippOverlayHandle != MM_INVALID_HANDLE)
    {
        stContext.mpCamera->detachRegionFromChannel(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        stContext.mpCamera->detachRegionFromChannel(stContext.mVippOverlayHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpCamera->destroyRegion(stContext.mVippOverlayHandle);
        stContext.mVippOverlayHandle = MM_INVALID_HANDLE;
    }
    if(stContext.mVippCoverHandle != MM_INVALID_HANDLE)
    {
        stContext.mpCamera->detachRegionFromChannel(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        stContext.mpCamera->detachRegionFromChannel(stContext.mVippCoverHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpCamera->destroyRegion(stContext.mVippCoverHandle);
        stContext.mVippCoverHandle = MM_INVALID_HANDLE;
    }
    if (stContext.mVippOrlHandle != MM_INVALID_HANDLE)
    {
        stContext.mpCamera->detachRegionFromChannel(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        stContext.mpCamera->detachRegionFromChannel(stContext.mVippOrlHandle, stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpCamera->destroyRegion(stContext.mVippOrlHandle);
        stContext.mVippCoverHandle = MM_INVALID_HANDLE;
    }

_exit0:
    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
    stContext.mpCamera->stopChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->releaseChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->closeChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->stopChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->releaseChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->closeChannel(stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
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
    cout<<"bye, sample_OSD!"<<endl;
    return result;
}
