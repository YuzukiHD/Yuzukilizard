//#define LOG_NDEBUG 0
#define LOG_TAG "sample_Camera_takepicture"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vo.h>

#include "sample_Camera_takepicture_config.h"
#include "sample_Camera_takepicture.h"

// Raw data is get by ScalerOutChns 0
// JPEG data  is get by ScalerOutChns 1
// Preview data is  get by ScalerOutChns 3
#define  SAMPLE_CAMERA_SCALER_OUT_CHN_RAW        (0)
#define  SAMPLE_CAMERA_SCALER_OUT_CHN_JPEG       (1)
#define  SAMPLE_CAMERA_SCALER_OUT_CHN_PREVIEW    (3)

using namespace std;
using namespace EyeseeLinux;

static SampleCameraTakePictureContext *pSampleCameraTakePictureContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleCameraTakePictureContext!=NULL)
    {
        cdx_sem_up(&pSampleCameraTakePictureContext->mSemExit);
    }
}

bool EyeseeCameraCallback::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mScalerOutChns[3])
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mScalerOutChns[3]);
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

void EyeseeCameraCallback::onPictureTaken(int chnId, const void *data, int size, EyeseeCamera *pCamera)
{
    int ret = -1;
    alogd("channel %d picture data size %d", chnId, size);
    if(chnId != mpContext->mScalerOutChns[1])
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mScalerOutChns[1]);
    }
    char picName[64];
    sprintf(picName, "pic[%02d].jpg", mpContext->mPicNum++);
    std::string PicFullPath = mpContext->mConfigPara.mJpegFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
}

EyeseeCameraCallback::EyeseeCameraCallback(SampleCameraTakePictureContext *pContext)
    : mpContext(pContext)
{
}

bool EyeseeCameraRawCallback::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mScalerOutChns[3])
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mScalerOutChns[3]);
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

void EyeseeCameraRawCallback::onPictureTaken(int chnId, const void *data, int size, EyeseeCamera *pCamera)
{
    int ret = -1;
    alogd("channel %d raw data size %d", chnId, size);
    if(chnId != mpContext->mScalerOutChns[0])
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mScalerOutChns[1]);
    }
    char picName[64];
    sprintf(picName, "raw[%02d].bin", mpContext->mPicNum++);
    std::string PicFullPath = mpContext->mConfigPara.mRawFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
}

EyeseeCameraRawCallback::EyeseeCameraRawCallback(SampleCameraTakePictureContext *pContext)
    : mpContext(pContext)
{
}

SampleCameraTakePictureContext::SampleCameraTakePictureContext()
    :mCameraCallbacks(this)
    , mCameraRawCallbacks(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = -1;
    mVoLayer = -1;
    mpCamera = NULL;
    mPicNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
    memset(&mScalerOutChns,0,sizeof(mScalerOutChns));
}

SampleCameraTakePictureContext::~SampleCameraTakePictureContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
}

status_t SampleCameraTakePictureContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_Camera_takepicture.conf\n";
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

status_t SampleCameraTakePictureContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mPreviewWidth = 640;
        mConfigPara.mPreviewHeight = 360;
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mDigitalZoom = 0;
        mConfigPara.mFrameRate = 30;
        mConfigPara.mDisplayFrameRate = 0;
        mConfigPara.mDisplayWidth = 640;
        mConfigPara.mDisplayHeight = 480;
        mConfigPara.mPreviewRotation = 0;

        mConfigPara.mJpegWidth = 1920;
        mConfigPara.mJpegHeight = 1080;
        mConfigPara.mJpegThumbWidth = 320;
        mConfigPara.mJpegThumbHeight = 180;
        mConfigPara.mJpegNum = 0;
        mConfigPara.mJpegInterval = 500;
        mConfigPara.mJpegFolderPath = "/mnt/extsd/sample_Camera_takepicture";

        mConfigPara.mRawWidth       = 1920;
        mConfigPara.mRawHeight      = 1080;
        mConfigPara.mRawNum         = 0;
        mConfigPara.mRawFolderPath  = "/mnt/extsd/sample_Camera_takepicture";
        mConfigPara.mRawFormat      = MM_PIXEL_FORMAT_RAW_SBGGR8;

        mConfigPara.mTestDuration = 0;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_CAPTURE_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_CAPTURE_FORMAT, NULL);
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
    mConfigPara.mPreviewWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_PREVIEW_WIDTH, 0);
    mConfigPara.mPreviewHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_PREVIEW_HEIGHT, 0);
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_PREVIEW_FORMAT, NULL);
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
    mConfigPara.mDigitalZoom = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_DIGITAL_ZOOM, 0);
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_FRAME_RATE, 0);
    mConfigPara.mDisplayFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_DISPLAY_FRAME_RATE, 0);
    mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_DISPLAY_WIDTH, 0);
    mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_DISPLAY_HEIGHT, 0);
    mConfigPara.mPreviewRotation = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_PREVIEW_ROTATION, 0);

    mConfigPara.mJpegWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_JPEG_WIDTH, 0);
    mConfigPara.mJpegHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_JPEG_HEIGHT, 0);
    mConfigPara.mJpegThumbWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_JPEG_THUMB_WIDTH, 0);
    mConfigPara.mJpegThumbHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_JPEG_THUMB_HEIGHT, 0);
    mConfigPara.mJpegNum = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_JPEG_NUM, 0);
    mConfigPara.mJpegInterval = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_JPEG_INTERVAL, 0);
    mConfigPara.mJpegFolderPath = GetConfParaString(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_JPEG_FOLDER, NULL);

    mConfigPara.mRawWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_RAW_WIDTH, 0);
    mConfigPara.mRawHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_RAW_HEIGHT, 0);
    mConfigPara.mRawNum = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_RAW_NUM, 0);
    mConfigPara.mRawFolderPath = GetConfParaString(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_RAW_FOLDER, NULL);
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_RAW_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "sbggr8"))
    {
		mConfigPara.mRawFormat	= MM_PIXEL_FORMAT_RAW_SBGGR8;
    }
    else
    {
        aloge("fatal error! conf file raw_format is [%s]?", pStrPixelFormat);
        mConfigPara.mRawFormat	= MM_PIXEL_FORMAT_RAW_SBGGR8;
    }

    mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_TAKE_PICTURE_KEY_TEST_DURATION, 0);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleCameraTakePictureContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("jpeg path is not set!");
        return UNKNOWN_ERROR;
    }
    const char* pJpegFolderPath = strFolderPath.c_str();
    //check folder existence
    struct stat sb;
    if (stat(pJpegFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pJpegFolderPath, sb.st_mode);
            return UNKNOWN_ERROR;
        }
    }
    //create folder if necessary
    int ret = mkdir(pJpegFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pJpegFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pJpegFolderPath);
        return UNKNOWN_ERROR;
    }
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_Camera_TAKE_PICTURE!"<<endl;
    SampleCameraTakePictureContext stContext;
    pSampleCameraTakePictureContext = &stContext;
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
    if(SUCCESS != stContext.CreateFolder(stContext.mConfigPara.mJpegFolderPath))
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
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        VI_CHN chn;
        ISPGeometry mISPGeometry;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 1;
        mISPGeometry.mISPDev = 0;
        chn = 1;
        mISPGeometry.mScalerOutChns.push_back(chn);
        chn = 3;
        mISPGeometry.mScalerOutChns.push_back(chn);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        mISPGeometry.mScalerOutChns.clear();
        mISPGeometry.mISPDev = 1;
        chn = 0;
        mISPGeometry.mScalerOutChns.push_back(chn);
        chn = 2;
        mISPGeometry.mScalerOutChns.push_back(chn);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    //config default ScalerOutChn.
    stContext.mScalerOutChns[0] = SAMPLE_CAMERA_SCALER_OUT_CHN_RAW; // Raw
    stContext.mScalerOutChns[1] = SAMPLE_CAMERA_SCALER_OUT_CHN_JPEG; // JPEG
    stContext.mScalerOutChns[2] = -1;
    stContext.mScalerOutChns[3] = SAMPLE_CAMERA_SCALER_OUT_CHN_PREVIEW; // Preview

    int cameraId;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &stContext.mCameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
    stContext.mpCamera->setInfoCallback(&stContext.mCameraCallbacks);
    stContext.mpCamera->prepareDevice();
    stContext.mpCamera->startDevice();

    CameraParameters cameraParam;

	alogd(" =================== preview begin");
    stContext.mpCamera->openChannel(stContext.mScalerOutChns[3], true);
    stContext.mpCamera->getParameters(stContext.mScalerOutChns[3], cameraParam);
    SIZE_S previewSize={(unsigned int)stContext.mConfigPara.mPreviewWidth, (unsigned int)stContext.mConfigPara.mPreviewHeight};
    cameraParam.setVideoSize(previewSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setDisplayFrameRate(stContext.mConfigPara.mDisplayFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mPreviewFormat);
    cameraParam.setVideoBufferNumber(6);
    cameraParam.setPreviewRotation(stContext.mConfigPara.mPreviewRotation);
    stContext.mpCamera->setParameters(stContext.mScalerOutChns[3], cameraParam);
    stContext.mpCamera->prepareChannel(stContext.mScalerOutChns[3]);

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
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
    stContext.mpCamera->setChannelDisplay(stContext.mScalerOutChns[3], stContext.mVoLayer);
    stContext.mpCamera->startChannel(stContext.mScalerOutChns[3]);

	if(stContext.mConfigPara.mJpegNum > 0)
	{
		alogd(" =================== Jpeg begin %d",stContext.mConfigPara.mJpegNum);
		stContext.mpCamera->openChannel(stContext.mScalerOutChns[1], false);
		stContext.mpCamera->getParameters(stContext.mScalerOutChns[1], cameraParam);
		SIZE_S captureSize={(unsigned int)stContext.mConfigPara.mCaptureWidth, (unsigned int)stContext.mConfigPara.mCaptureHeight};
		cameraParam.setVideoSize(captureSize);
		//cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
		//cameraParam.setDisplayFrameRate(stContext.mConfigPara.mDisplayFrameRate);
		cameraParam.setPreviewFormat(stContext.mConfigPara.mCaptureFormat);
		cameraParam.setVideoBufferNumber(6);
		stContext.mpCamera->setParameters(stContext.mScalerOutChns[1], cameraParam);
		stContext.mpCamera->prepareChannel(stContext.mScalerOutChns[1]);
		stContext.mpCamera->startChannel(stContext.mScalerOutChns[1]);
		{
			stContext.mpCamera->getParameters(stContext.mScalerOutChns[1], cameraParam);
			cameraParam.setPictureMode(TAKE_PICTURE_MODE_CONTINUOUS);
			cameraParam.setContinuousPictureNumber(stContext.mConfigPara.mJpegNum);
			cameraParam.setContinuousPictureIntervalMs(stContext.mConfigPara.mJpegInterval);
			cameraParam.setPictureSize(SIZE_S{(unsigned int)stContext.mConfigPara.mJpegWidth, (unsigned int)stContext.mConfigPara.mJpegHeight});
			cameraParam.setJpegThumbnailSize(SIZE_S{(unsigned int)stContext.mConfigPara.mJpegThumbWidth, (unsigned int)stContext.mConfigPara.mJpegThumbHeight});
			stContext.mpCamera->setParameters(stContext.mScalerOutChns[1], cameraParam);
			stContext.mpCamera->takePicture(stContext.mScalerOutChns[1], NULL, NULL, &stContext.mCameraCallbacks);
		}
		alogd(" =================== Jpeg end");
	}


	if(stContext.mConfigPara.mRawNum > 0)
	{
		alogd(" =================== Raw begin  %d",stContext.mConfigPara.mRawNum);
		stContext.mpCamera->openChannel(stContext.mScalerOutChns[0], false);
		stContext.mpCamera->getParameters(stContext.mScalerOutChns[0], cameraParam);
		SIZE_S rawSize={(unsigned int)stContext.mConfigPara.mRawWidth, (unsigned int)stContext.mConfigPara.mRawHeight};
		cameraParam.setVideoSize(rawSize);
		cameraParam.setPreviewFormat(stContext.mConfigPara.mRawFormat);
		cameraParam.setVideoBufferNumber(6);
		stContext.mpCamera->setParameters(stContext.mScalerOutChns[0], cameraParam);
		stContext.mpCamera->prepareChannel(stContext.mScalerOutChns[0]);
		stContext.mpCamera->startChannel(stContext.mScalerOutChns[0]);
		{
			stContext.mpCamera->getParameters(stContext.mScalerOutChns[0], cameraParam);
			cameraParam.setPictureMode(TAKE_PICTURE_MODE_FAST);
			cameraParam.setPictureSize(SIZE_S{(unsigned int)stContext.mConfigPara.mRawWidth, (unsigned int)stContext.mConfigPara.mRawHeight});
			stContext.mpCamera->setParameters(stContext.mScalerOutChns[0], cameraParam);
			stContext.mpCamera->takePicture(stContext.mScalerOutChns[0], NULL, &stContext.mCameraRawCallbacks, NULL);
		}
		alogd(" =================== Raw end");
	}

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
            goto _exit0;
        }
        stContext.mpCamera->getParameters(stContext.mScalerOutChns[3], cameraParam);
        int oldZoom = cameraParam.getZoom();
        cameraParam.setZoom(stContext.mConfigPara.mDigitalZoom);
        stContext.mpCamera->setParameters(stContext.mScalerOutChns[3], cameraParam);
        alogd("change digital zoom[%d]->[%d]", oldZoom, stContext.mConfigPara.mDigitalZoom);
    }

    if(stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

_exit0:
    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
	stContext.mpCamera->stopChannel(stContext.mScalerOutChns[1]);
	stContext.mpCamera->releaseChannel(stContext.mScalerOutChns[1]);
	stContext.mpCamera->closeChannel(stContext.mScalerOutChns[1]);

	stContext.mpCamera->stopChannel(stContext.mScalerOutChns[3]);
	stContext.mpCamera->releaseChannel(stContext.mScalerOutChns[3]);
	stContext.mpCamera->closeChannel(stContext.mScalerOutChns[3]);

	stContext.mpCamera->stopChannel(stContext.mScalerOutChns[0]);
	stContext.mpCamera->releaseChannel(stContext.mScalerOutChns[0]);
	stContext.mpCamera->closeChannel(stContext.mScalerOutChns[0]);

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
    cout<<"bye, sample_Camera!"<<endl;
    return result;
}

