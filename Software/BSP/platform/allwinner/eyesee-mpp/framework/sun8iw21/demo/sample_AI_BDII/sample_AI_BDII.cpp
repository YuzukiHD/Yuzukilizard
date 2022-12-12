#define LOG_NDEBUG 0
#define LOG_TAG "sample_AI_BDII"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <stdio.h>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vo.h>

#include <EyeseeCamera.h>

#include "sample_AI_BDII_config.h"
#include "sample_AI_BDII.h"

#include <stdio.h>


using namespace std;
using namespace EyeseeLinux;

static SampleAIBDIIContext *pSampleAIBDIIContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if (pSampleAIBDIIContext != NULL)
    {
        cdx_sem_up(&pSampleAIBDIIContext->mSemExit);
    }
}

EyeseeCameraListener::EyeseeCameraListener(SampleAIBDIIContext *pContext)
    : mpContext(pContext)
{
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
    case CAMERA_INFO_RENDERING_START:
	    if(chnId==mpContext->mCamChnId[0] || chnId==mpContext->mCamChnId[1])
        {
            alogd("cam chn[%d] start rendering", chnId);
        }
        break;

    default:
        aloge("fatal error! unknown info[0x%x] from channel[%d]", info, chnId);
        bHandleInfoFlag = false;
        break;
    }
    return bHandleInfoFlag;
}

EyeseeBDIIListener::EyeseeBDIIListener(SampleAIBDIIContext *pContext)
    :mpContext(pContext)
{

}

void EyeseeBDIIListener::onBDIIData(int chnId, const EyeseeLinux::CVE_BDII_RES_OUT_S *data, EyeseeLinux::EyeseeBDII* pBDII)
{
    alogd("on data avalible!");
    static int cnt = 0;
    if (cnt++ == 300)
    {
        unsigned int size = data->deepImgSize;
        int buf_size = size / sizeof(AW_S16);
        char *pImagbuf = (char *)malloc(buf_size);
        FILE *fp = fopen("/mnt/extsd/sample_ai_bdii/deepimg_y.yuv", "wb");
        if (fp != NULL)
        {
            fseek(fp, 0, SEEK_SET);
            int value;
            for (int j=0; j<buf_size; j++)
            {
                value = data->result.as16DeepImg[j];
                if (value < 0)
                {
                    value = 0;
                }
                else
                {
                    value = value >> 2;
                    if (value > 255)
                    {
                        value = 255;
                    }
                }
                pImagbuf[j] = value;
            }
            fwrite(pImagbuf, 1, buf_size, fp);
            fclose(fp);
            free(pImagbuf);
        }else{
            if(pImagbuf != NULL)
                free(pImagbuf);
        }
    }
}

void EyeseeBDIIListener::onError(int chnId, int error, EyeseeLinux::EyeseeBDII *pBDII)
{
    aloge("Be careful, on error!!");
}


SampleAIBDIIContext::SampleAIBDIIContext()
    : mpBDII(NULL)
    , mpCameraCallback(NULL)
    , mpBDIIListener(NULL)
{
    cdx_sem_init(&mSemExit, 0);
    mUILayer = HLAY(2, 0);
    //mVoDev[0] = -1;
    mVoLayer[0] = -1;
    //mVoDev[1] = -1;
    mVoLayer[1] = -1;

    mpCamera[0] = NULL;
    mpCamera[1] = NULL;

    mCamChnId[0] = -1;
    mCamChnId[1] = -1;
    memset(&mConfigPara,0,sizeof(mConfigPara));
    mpCameraCallback = new EyeseeCameraListener(this);
    mpBDIIListener = new EyeseeBDIIListener(this);
}

SampleAIBDIIContext::~SampleAIBDIIContext()
{
    cdx_sem_deinit(&mSemExit);
    if (mpCamera[0]!=NULL)
    {
        aloge("fatal error! EyeseeCamera[0] is not destruct!");
    }
    if (mpCamera[1]!=NULL)
    {
        aloge("fatal error! EyeseeCamera[1] is not destruct!");
    }

}

status_t SampleAIBDIIContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_AI_BDII.conf\n";
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

status_t SampleAIBDIIContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 640;//1920;
        mConfigPara.mCaptureHeight = 360;//1080;
        //mConfigPara.mDigitalZoom = 0;
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mFrameRate = 25;
        mConfigPara.mDisplayWidth = 320;
        mConfigPara.mDisplayHeight = 240;
        mConfigPara.mPreviewRotation = 0;
        mConfigPara.mEnableDisp = 1;
        mConfigPara.mTestDuration = 1000; //s
        return SUCCESS;
    }

    memset(&mConfigPara, 0, sizeof(SampleAIBDIIConfig));

    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }

    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_CAPTURE_HEIGHT, 0);
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_FRAME_RATE, 0); 
    mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_DISPLAY_WIDTH, 0); 
    mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_DISPLAY_HEIGHT, 0);
    mConfigPara.mPreviewRotation = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_PREVIEW_ROTATION, 0);
    mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    mConfigPara.mBDIIEnable = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_BDII, 0);
    mConfigPara.mEnableDisp = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_DISPLAY, 0);

    mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_TEST_DURATION, 0);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}


int main(int argc, char *argv[])
{
    int result = 0;
    SampleAIBDIIContext stContext;

    pSampleAIBDIIContext = &stContext;
 //parse command line param
    if(stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        aloge("fatal error! command line param is wrong, exit!");
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

 //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        perror("can't catch SIGSEGV");
    }

 //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

 //close ui layer
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer. not display, but layer still exist!

 //config camera.
    int cameraId;
    int chn_id;
    {
        EyeseeCamera::clearCamerasConfiguration();
        CameraInfo cameraInfo0;
        ISPGeometry mISPGeometry;

        cameraId = 0;
        cameraInfo0.facing = CAMERA_FACING_BACK;
        cameraInfo0.orientation = 0;
        cameraInfo0.canDisableShutterSound = true;
        cameraInfo0.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo0.mMPPGeometry.mCSIChn = 1;
        //cameraInfo0.mMPPGeometry.mISPDev = 0;
        //cameraInfo0.mMPPGeometry.mScalerOutChns.push_back(1);
        //cameraInfo0.mMPPGeometry.mScalerOutChns.push_back(3);
        mISPGeometry.mISPDev = 0;
        mISPGeometry.mScalerOutChns.push_back(1);
        mISPGeometry.mScalerOutChns.push_back(3);
        cameraInfo0.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo0);

        mISPGeometry.mScalerOutChns.clear();
        cameraId = 1;
        CameraInfo cameraInfo1;
        cameraInfo1.facing = CAMERA_FACING_BACK;
        cameraInfo1.orientation = 0;
        cameraInfo1.canDisableShutterSound = true;
        cameraInfo1.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo1.mMPPGeometry.mCSIChn = 1;
        //cameraInfo1.mMPPGeometry.mISPDev = 0;
        //cameraInfo1.mMPPGeometry.mScalerOutChns.push_back(1);
        //cameraInfo1.mMPPGeometry.mScalerOutChns.push_back(3);
        mISPGeometry.mISPDev = 0;
        mISPGeometry.mScalerOutChns.push_back(1);
        mISPGeometry.mScalerOutChns.push_back(3);
        cameraInfo1.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);

        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo1);
    }

  //open cam0
    {
        cameraId = 0;
        EyeseeCamera::getCameraInfo(cameraId, &stContext.mCameraInfo[0]);
        stContext.mpCamera[0] = EyeseeCamera::open(cameraId);
        stContext.mpCamera[0]->setInfoCallback(stContext.mpCameraCallback);
        stContext.mpCamera[0]->prepareDevice();
        stContext.mpCamera[0]->startDevice();

        chn_id = stContext.mCameraInfo[0].mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
        alogd("##cam0_chn=%d", chn_id);
        stContext.mCamChnId[0] = chn_id;
        stContext.mpCamera[0]->openChannel(chn_id, true);
        CameraParameters cameraParam;
        stContext.mpCamera[0]->getParameters(chn_id, cameraParam);

        SIZE_S captureSize={(unsigned int)stContext.mConfigPara.mCaptureWidth, (unsigned int)stContext.mConfigPara.mCaptureHeight};

        cameraParam.setVideoSize(captureSize);
        cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
        cameraParam.setDisplayFrameRate(stContext.mConfigPara.mFrameRate);
        cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
        cameraParam.setVideoBufferNumber(7);
        cameraParam.setPreviewRotation(stContext.mConfigPara.mPreviewRotation);

        stContext.mpCamera[0]->setParameters(chn_id, cameraParam);
        stContext.mpCamera[0]->prepareChannel(chn_id);

        if (stContext.mConfigPara.mEnableDisp)
        {
            //stContext.mVoDev[0] = 0;
            //AW_MPI_VO_Enable(stContext.mVoDev[0]);
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
            AW_MPI_VO_GetVideoLayerAttr(hlay, &stContext.mLayerAttr[0]);
            stContext.mLayerAttr[0].stDispRect.X = 0;
            stContext.mLayerAttr[0].stDispRect.Y = 0;
            stContext.mLayerAttr[0].stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
            stContext.mLayerAttr[0].stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
            AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr[0]);
            stContext.mVoLayer[0] = hlay;
            alogd("cam[%d] setPreviewDisplay(), hlay=%d", cameraId, stContext.mVoLayer[0]);

            stContext.mpCamera[0]->setChannelDisplay(chn_id, stContext.mVoLayer[0]);
        }
        stContext.mpCamera[0]->startChannel(chn_id);
    }

   //open cam1
    {
        cameraId = 1;
        EyeseeCamera::getCameraInfo(cameraId, &stContext.mCameraInfo[1]);
        stContext.mpCamera[1] = EyeseeCamera::open(cameraId);
        stContext.mpCamera[1]->setInfoCallback(stContext.mpCameraCallback);
        stContext.mpCamera[1]->prepareDevice();
        stContext.mpCamera[1]->startDevice();
        
        chn_id = stContext.mCameraInfo[1].mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
        alogd("cam1_chn=%d", chn_id);
        stContext.mCamChnId[1] = chn_id;
        stContext.mpCamera[1]->openChannel(chn_id, true);
        CameraParameters cameraParam;
        stContext.mpCamera[1]->getParameters(chn_id, cameraParam);
  
        SIZE_S captureSize={(unsigned int)stContext.mConfigPara.mCaptureWidth, (unsigned int)stContext.mConfigPara.mCaptureHeight};
  
        cameraParam.setVideoSize(captureSize);
        cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
        cameraParam.setDisplayFrameRate(stContext.mConfigPara.mFrameRate);
        cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
        cameraParam.setVideoBufferNumber(7);
        cameraParam.setPreviewRotation(stContext.mConfigPara.mPreviewRotation);

        stContext.mpCamera[1]->setParameters(chn_id, cameraParam);
        stContext.mpCamera[1]->prepareChannel(chn_id);

        if (stContext.mConfigPara.mEnableDisp)
        {
            //stContext.mVoDev[1] = 0;
            //AW_MPI_VO_Enable(stContext.mVoDev[1]);
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
            AW_MPI_VO_GetVideoLayerAttr(hlay, &stContext.mLayerAttr[1]);
            //stContext.mLayerAttr[1].stDispRect.X = stContext.mLayerAttr[0].stDispRect.X+100;
            //stContext.mLayerAttr[1].stDispRect.Y = 0;
            stContext.mLayerAttr[1].stDispRect.X = stContext.mConfigPara.mDisplayWidth;
            stContext.mLayerAttr[1].stDispRect.Y = 0;
            stContext.mLayerAttr[1].stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
            stContext.mLayerAttr[1].stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
            AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr[1]);
            stContext.mVoLayer[1] = hlay;
            alogd("##cam[%d] setPreviewDisplay(), hlay=%d", cameraId, stContext.mVoLayer[1]);

            stContext.mpCamera[1]->setChannelDisplay(chn_id, stContext.mVoLayer[1]);
        }
        stContext.mpCamera[1]->startChannel(chn_id);
    }


    if (stContext.mConfigPara.mBDIIEnable)
    {
        stContext.mpBDII = EyeseeBDII::open();
        if (stContext.mpBDII)
        {
            vector<BDIICameraChnInfo> cam_chn_vec;
            BDIICameraChnInfo cam_chn_info;

            cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
            cam_chn_info.mpCamera = stContext.mpCamera[0];
            cam_chn_info.mCamStyle = BDII_CAMERA_LEFT;
            cam_chn_vec.push_back(cam_chn_info);

            cam_chn_info.mnCameraChannel = stContext.mCamChnId[1];
            cam_chn_info.mpCamera = stContext.mpCamera[1];
            cam_chn_info.mCamStyle = BDII_CAMERA_RIGHT;
            cam_chn_vec.push_back(cam_chn_info);

            stContext.mpBDII->setCamera(cam_chn_vec);
            //stContext.mpBDII->setBDIIDataCallback(stContext.mpBDIIListener);
            stContext.mpBDII->setPreviewRotation(stContext.mConfigPara.mPreviewRotation);

            if (stContext.mConfigPara.mEnableDisp)
            {
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
                AW_MPI_VO_GetVideoLayerAttr(hlay, &stContext.mLayerAttr[2]);
                stContext.mLayerAttr[2].stDispRect.X = 0;
                stContext.mLayerAttr[2].stDispRect.Y = stContext.mConfigPara.mDisplayHeight;
                stContext.mLayerAttr[2].stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
                stContext.mLayerAttr[2].stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
                AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr[2]);
                stContext.mVoLayer[2] = hlay;
                alogd("##bdii setPreviewDisplay(), hlay=%d", stContext.mVoLayer[2]);
                stContext.mpBDII->setDisplay(stContext.mVoLayer[2]);
            }

            AW_AI_CVE_BDII_INIT_PARA_S m_param = {0};
            m_param.u8ftzero = 31;
            m_param.u8maxDisparity = 64;
            m_param.u8SADWindowSize = 7;
            m_param.u8textureThreshold = 10;
            m_param.u8uniquenessRatio = 15;
            m_param.u8disp12MaxDiff = 1;
            stContext.mpBDII->setBDIIConfigParams(m_param);

            /*std::vector<BDIIDebugFileOutString> strVec;
            BDIIDebugFileOutString fileoutStr;
            fileoutStr.fileOutStr = "/home/bdii_left.yuv";
            fileoutStr.fileOutStyle = BDIIDebugFileOutString::FILE_STR_LEFT_YUV;
            strVec.push_back(fileoutStr);

            fileoutStr.fileOutStr = "/home/bdii_right.yuv";
            fileoutStr.fileOutStyle = BDIIDebugFileOutString::FILE_STR_RIGHT_YUV;
            strVec.push_back(fileoutStr);

            fileoutStr.fileOutStr = "/home/bdii_res_deep.yuv";
            fileoutStr.fileOutStyle = BDIIDebugFileOutString::FILE_STR_RESULT_DEEP_YUV;
            strVec.push_back(fileoutStr);

            fileoutStr.fileOutStr = "/home/bdii_res_cost.yuv";
            fileoutStr.fileOutStyle = BDIIDebugFileOutString::FILE_STR_RESULT_COST_YUV;
            strVec.push_back(fileoutStr);

            stContext.mpBDII->debugYuvDataOut(true, strVec);*/

            stContext.mpBDII->prepare();
            stContext.mpBDII->start();
        }
    }

    alogd("wait over");
    if(stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

_exit0:
    alogd("over! to release resource");

  //close eyeseeBDII
    {
        if (stContext.mConfigPara.mBDIIEnable)
        {
            if (stContext.mpBDII)
            {
                //std::vector<BDIIDebugFileOutString> strVec;
                //stContext.mpBDII->debugYuvDataOut(false, strVec);
                stContext.mpBDII->stop();
                EyeseeBDII::close(stContext.mpBDII);
                stContext.mpBDII = NULL;
            }
        }
        if (stContext.mConfigPara.mEnableDisp)
        {
            AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer[2]);
            stContext.mVoLayer[2] = MM_INVALID_LAYER;
        }
    }

  //close cam0
    {
        cameraId = 0;
        chn_id = stContext.mCamChnId[0];
        stContext.mpCamera[0]->stopChannel(chn_id);
        stContext.mpCamera[0]->releaseChannel(chn_id);
        stContext.mpCamera[0]->closeChannel(chn_id);
        stContext.mpCamera[0]->stopDevice(); 
        stContext.mpCamera[0]->releaseDevice();
        EyeseeCamera::close(stContext.mpCamera[0]);
        stContext.mpCamera[0] = NULL;

      //close vo
        if (stContext.mConfigPara.mEnableDisp)
        {
            AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer[0]);
            stContext.mVoLayer[0] = MM_INVALID_LAYER;
        }
    }

  //close cam1
    {
        cameraId = 1;
        chn_id = stContext.mCamChnId[1];
        stContext.mpCamera[1]->stopChannel(chn_id);
        stContext.mpCamera[1]->releaseChannel(chn_id);
        stContext.mpCamera[1]->closeChannel(chn_id);
        stContext.mpCamera[1]->stopDevice(); 
        stContext.mpCamera[1]->releaseDevice();
        EyeseeCamera::close(stContext.mpCamera[1]);
        stContext.mpCamera[1] = NULL;

      //close vo
        if (stContext.mConfigPara.mEnableDisp)
        {
            AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer[1]);
            stContext.mVoLayer[1] = MM_INVALID_LAYER;
        }
    }
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    if (stContext.mpCameraCallback)
    {
        delete stContext.mpCameraCallback;
    }
    if (stContext.mpBDIIListener)
    {
        delete stContext.mpBDIIListener;
    }

    //exit mpp system
    AW_MPI_SYS_Exit(); 
    cout<<"bye, sample_AI_BDII!"<<endl;
    return result;
}
