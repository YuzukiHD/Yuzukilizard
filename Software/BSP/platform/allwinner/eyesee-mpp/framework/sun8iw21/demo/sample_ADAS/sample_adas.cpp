#define LOG_NDEBUG 0
#define LOG_TAG "sample_ADAS"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vo.h>

#include "sample_adas_config.h"
#include "sample_adas.h"


#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>


using namespace std;
using namespace EyeseeLinux;

static SampleAdasContext *pSampleAdasContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if (pSampleAdasContext != NULL)
    {
        cdx_sem_up(&pSampleAdasContext->mSemExit);
    }
}

EyeseeCameraCallback::EyeseeCameraCallback(SampleAdasContext *pContext)
    : mpContext(pContext)
{
}

bool EyeseeCameraCallback::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
    case CAMERA_INFO_RENDERING_START:
        if (chnId == mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0])
        {
            alogd("start rendering");
        }
        break;
    default:
        aloge("fatal error! unknown info[0x%x] from channel[%d]", info, chnId);
        bHandleInfoFlag = false;
        break;
    }
    return bHandleInfoFlag;
}

void EyeseeCameraCallback::onAdasOutData(int chnId, AW_AI_ADAS_DETECT_R_ *p_sResult,EyeseeLinux::EyeseeCamera* pCamera)
{
    #if 1
    int i = 0;
    static int num = 0;
    alogw("onAdasOutData---------------------------------------------------------------num=%d\n",num++);
    alogw("版本信息: ptrVersion = %s",p_sResult->nADASOutData.ptrVersion);
    alogw("左车道线左上角点X坐标: leftLaneLineX0 = %d \n",p_sResult->nADASOutData.leftLaneLineX0);
    alogw("左车道线左上角点Y坐标: leftLaneLineY0 = %d \n",p_sResult->nADASOutData.leftLaneLineY0);
    alogw("左车道线左下角点X坐标: leftLaneLineX1 = %d \n",p_sResult->nADASOutData.leftLaneLineX1);
    alogw("左车道线左下角点Y坐标: leftLaneLineY1 = %d \n",p_sResult->nADASOutData.leftLaneLineY1);
    
    alogw("右车道线右上角点X坐标: rightLaneLineX0 = %d \n",p_sResult->nADASOutData.rightLaneLineX0);
    alogw("右车道线右上角点Y坐标: rightLaneLineY0 = %d \n",p_sResult->nADASOutData.rightLaneLineY0);
    alogw("右车道线右下角点X坐标: rightLaneLineX1 = %d \n",p_sResult->nADASOutData.rightLaneLineX1);
    alogw("右车道线右下角点Y坐标: rightLaneLineY1 = %d \n",p_sResult->nADASOutData.rightLaneLineY1);

   // alogw("左车道线报警信息(0-黄色-未检测到 1-绿色-检测到 2-红色-压线报警 3-请重新标定摄像头)\n");
    alogw("左车道线报警信息: leftLaneLineWarn = %d \n",p_sResult->nADASOutData.leftLaneLineWarn);
    alogw("右车道线报警信息: rightLaneLineWarn = %d \n",p_sResult->nADASOutData.rightLaneLineWarn);

    alogw("下方一块的颜色(1-蓝色 2-绿色): dnColor = %d \n",p_sResult->nADASOutData.dnColor);
    alogw("车道线条状块分割点的个数: colorPointsNum = %d \n",p_sResult->nADASOutData.colorPointsNum);
    for(i= 0; i < p_sResult->nADASOutData.colorPointsNum; i++)
	{
		alogw("车道线条状块分割点的行坐标[%d]:%d \n",i, p_sResult->nADASOutData.rowIndex[i]);
        alogw("车道线条状块左边分割点的列坐标[%d]:%d \n",i,p_sResult->nADASOutData.ltColIndex[i]);
        alogw("车道线条状块中间分割点的列坐标[%d]:%d \n",i,p_sResult->nADASOutData.mdColIndex[i]);
        alogw("车道线条状块右边分割点的列坐标[%d]:%d \n",i,p_sResult->nADASOutData.rtColIndex[i]);
	}

    alogw("检测到车辆数目: carNum = %d \n",p_sResult->nADASOutData.carNum);
    for(i= 0; i < p_sResult->nADASOutData.carNum; i++)
	{
		alogw("车辆起点X坐标[%d]:%d \n",i,p_sResult->nADASOutData.carX[i]);
        alogw("车辆起点Y坐标[%d]:%d \n",i,p_sResult->nADASOutData.carY[i]);
        alogw("车辆宽度[%d]:%d \n",i,p_sResult->nADASOutData.carW[i]);
        alogw("车辆高度[%d]:%d \n",i,p_sResult->nADASOutData.carH[i]);
        alogw("与该车的碰撞时间[%d]: %f \n",i,p_sResult->nADASOutData.carTTC[i]);
        alogw("该车到本车的距离[%d]: %f \n",i,p_sResult->nADASOutData.carDist[i]);
        alogw("车辆报警信息(0-无信号 1-车道偏离警报 2-前车碰撞预警)[%d]: %d \n",i,p_sResult->nADASOutData.carWarn[i]);
	}
     alogw("检测车辆数目: plateNum = %d",p_sResult->nADASOutData.plateNum);

    for(i= 0; i < p_sResult->nADASOutData.plateNum; i++)
	{
		alogw("车辆左上角X坐标[%d]:%d \n",i,p_sResult->nADASOutData.plateX[i]);
        alogw("车辆左上角Y坐标[%d]:%d \n",i,p_sResult->nADASOutData.plateY[i]);
        alogw("车辆右下角X坐标[%d]:%d \n",i,p_sResult->nADASOutData.plateW[i]);
        alogw("车辆右下角Y坐标[%d]:%d \n",i,p_sResult->nADASOutData.plateH[i]);
	}
    alogd("subWidth = %d subHeight=%d",p_sResult->subWidth,p_sResult->subHeight);
    #endif
#if 1
    if(mpContext->mSampleAdasUi != NULL){
        mpContext->mSampleAdasUi->HandleAdasOutMsg(*p_sResult);
    }else{
        //alogw("mpContext->mSampleAdasUi != NULL");
    }
#endif
	
}
void EyeseeCameraCallback::onAdasOutData_v2(int chnId, AW_AI_ADAS_DETECT_R__v2 *p_sResult,EyeseeLinux::EyeseeCamera* pCamera)
{
	//输出车辆检测信息
	/*
	printf("There have %d cars,Driver get scores:%d!\n", p_sResult->nADASOutData_v2.cars.Num,p_sResult->nADASOutData_v2.score);
	
	printf("The distance %f!\n", p_sResult->nADASOutData_v2.cars.carP->dist);
	
	printf("ADAS lane detect: sx[%d], sy[%d], ex[%d], ey[%d],\n",
	p_sResult->nADASOutData_v2.lane.ltIdxs[0].x,
	p_sResult->nADASOutData_v2.lane.ltIdxs[0].y,
	p_sResult->nADASOutData_v2.lane.ltIdxs[1].x,
	p_sResult->nADASOutData_v2.lane.ltIdxs[2].y);
	//右侧车道线
	printf("ADAS lane detect: sx[%d], sy[%d], ex[%d], ey[%d],\n",
	p_sResult->nADASOutData_v2.lane.rtIdxs[0].x,
	p_sResult->nADASOutData_v2.lane.rtIdxs[0].y,
	p_sResult->nADASOutData_v2.lane.rtIdxs[1].x,
	p_sResult->nADASOutData_v2.lane.rtIdxs[1].y);
	//车道线报警信息
	printf("ADAS lane detect: leftLaneLineWarn[%d], rightLaneLineWarn[%d]\n",
	p_sResult->nADASOutData_v2.lane.ltWarn, //左车道线报警信息
	p_sResult->nADASOutData_v2.lane.rtWarn);//右车道线报警信息
    */
}


SampleAdasContext::SampleAdasContext()
    : mCameraCallbacks(this)
{
    cdx_sem_init(&mSemExit, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = -1;
    mVoLayer = -1;
    mpCamera = NULL;
    mSampleAdasUi = NULL;
    memset(&mConfigPara,0,sizeof(mConfigPara));
    mMuxerId = -1;
    mpRecCamera = NULL;
}

SampleAdasContext::~SampleAdasContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpCamera != NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
    if(mSampleAdasUi != NULL)
    {
        delete mSampleAdasUi;
        mSampleAdasUi = NULL;
    }
}

status_t SampleAdasContext::ParseCmdLine(int argc, const char *argv[])
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
            helpString += "\t run -path /home/sample_Camera.conf\n";
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

status_t SampleAdasContext::loadConfig()
{
    int ret;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;//1920;
        mConfigPara.mCaptureHeight = 1080;//1080;
        //mConfigPara.mDigitalZoom = 0;
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mFrameRate = 25;
        mConfigPara.mDisplayWidth = 640; 
        mConfigPara.mDisplayHeight = 480;
		mConfigPara.mPreviewRotation = 0;
        mConfigPara.mTestDuration = 1000; //s
        return SUCCESS;
    }

    memset(&mConfigPara, 0, sizeof(SampleAdasConfig));

    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }

    mConfigPara.cam_id = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_CAM_DEV, 0);
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_CAPTURE_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_CFG_KEY_PIC_FORMAT, NULL); 
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
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_FRAME_RATE, 0); 
    mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_DISPLAY_WIDTH, 0); 
    mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_DISPLAY_HEIGHT, 0);
    mConfigPara.mPreviewRotation = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_PREVIEW_ROTATION, 0);
    mConfigPara.mAdasEnable = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_ADAS, 0);
    mConfigPara.mAdasUiEnable = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_ADAS_UI, 0);
    mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_TEST_DURATION, 0);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}



int main(int argc, const char *argv[])
//int MiniGUIMain(int argc, const char* argv[])
{
    int result = 0;
    SampleAdasContext stContext;
    pSampleAdasContext= &stContext;

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
    memset(&stContext.mSysConf,0,sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

	alogd("after AW_MPI_SYS_Init!!!\n ");

    //config camera.
    int cameraId;
    int chn_id;
    {
        EyeseeCamera::clearCamerasConfiguration();
        CameraInfo cameraInfo;
        VI_CHN viChn;
        ISPGeometry mISPGeometry;

        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 0;
        //cameraInfo.mMPPGeometry.mISPDev = 1;
        mISPGeometry.mISPDev = 0;
        viChn = 0;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        mISPGeometry.mScalerOutChns.push_back(viChn);
        viChn = 1;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        mISPGeometry.mScalerOutChns.push_back(viChn);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }
	
    cameraId = stContext.mConfigPara.cam_id;
    EyeseeCamera::getCameraInfo(cameraId, &stContext.mCameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
    stContext.mpCamera->setInfoCallback(&stContext.mCameraCallbacks);
    stContext.mpCamera->prepareDevice();
    stContext.mpCamera->startDevice();

    chn_id = stContext.mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[cameraId*2];
    stContext.mpCamera->openChannel(chn_id, true);
    CameraParameters cameraParam;
    stContext.mpCamera->getParameters(chn_id, cameraParam);
    SIZE_S captureSize={(unsigned int)stContext.mConfigPara.mCaptureWidth, (unsigned int)stContext.mConfigPara.mCaptureHeight};
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setDisplayFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
    cameraParam.setVideoBufferNumber(7);
    cameraParam.setPreviewRotation(stContext.mConfigPara.mPreviewRotation);
    stContext.mpCamera->setParameters(chn_id, cameraParam);
    stContext.mpCamera->prepareChannel(chn_id);
	
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer.
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(stContext.mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(stContext.mVoDev, &spPubAttr);
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
    stContext.mpCamera->setChannelDisplay(chn_id, stContext.mVoLayer);
    stContext.mpCamera->startChannel(chn_id);
    if (stContext.mConfigPara.mAdasEnable)
    {
		#ifdef DEF_MPPCFG_ADAS_DETECT
			alogd("habo--> start to startAdasDetect\n");
	        AdasDetectParam mAdasDetectParam;
	        stContext.mpCamera->getAdasParams(chn_id,&mAdasDetectParam);
	        mAdasDetectParam.nrightSensity = 1;
	        mAdasDetectParam.nleftSensity = 1;
	        stContext.mpCamera->setAdasParams(chn_id,mAdasDetectParam);
	        stContext.mpCamera->setAdasDataCallback(&stContext.mCameraCallbacks);
	        stContext.mpCamera->startAdasDetect(chn_id);
		#endif
		
		#ifdef DEF_MPPCFG_ADAS_DETECT_V2
			alogd("habo--> start to mAdasDetectParam_v2 0\n");
			AdasDetectParam_v2 mAdasDetectParam_v2;
			stContext.mpCamera->getAdasParams_v2(chn_id,&mAdasDetectParam_v2);
			stContext.mpCamera->setAdasParams_v2(chn_id,mAdasDetectParam_v2);
			stContext.mpCamera->setAdasDataCallback(&stContext.mCameraCallbacks);
			alogd("-->startAdasDetect_v2 01\n");
			stContext.mpCamera->startAdasDetect_v2(chn_id);
			alogd("after startAdasDetect_v2 00\n");
		#endif		
    }

    if(stContext.mConfigPara.mAdasUiEnable)
    {
        stContext.mSampleAdasUi = new SampleAdasUi();
        stContext.mSampleAdasUi->MiniGuiInit(argc,argv);
        stContext.mSampleAdasUi->LoopUi(); 
    }

#ifdef ADAS_debug
    char str[256]  = {0};
    char val[16] = {0};
    int num = 0, vl = 0;
    RECT_S get_test_rect;
    memset(&get_test_rect, 0, sizeof(RECT_S));

    RECT_S test_rect;
    memset(&test_rect, 0, sizeof(RECT_S));
    alogd("===Test ADAS !===");
    int64_t time0 = 0;
    int64_t timeIntervalUs = 0;
    while (1)
    {
        memset(str, 0, sizeof(str));
        memset(val,0,sizeof(val));
        fgets(str, 256, stdin);
        num = atoi(str);
        if (99 == num) {
            alogd("break test.\n");
            alogd("enter ctrl+c to exit this program.\n");
            break;
        }
        switch (num) {
            case 1:
                alogd("\n\n--stopAdas--:\n\n");
                stContext.mpCamera->stopAdasDetect_v2(chn_id);
                break;
            case 2:
                alogd("\n\n--startAdas--:\n\n");
                stContext.mpCamera->startAdasDetect_v2(chn_id);
                break;
            default:
                alogd("intput error.\r\n");
                break;
        }
    }
#endif
    
    if(stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }
    //close camera
    alogd("HerbCamera stopPreview");
    if (stContext.mConfigPara.mAdasEnable)
    {
    	#ifdef DEF_MPPCFG_ADAS_DETECT
        stContext.mpCamera->stopAdasDetect(chn_id);
		#endif
		
		#ifdef DEF_MPPCFG_ADAS_DETECT_V2
        stContext.mpCamera->stopAdasDetect_v2(chn_id);
		#endif
    }
    stContext.mpCamera->stopChannel(chn_id);
    stContext.mpCamera->releaseChannel(chn_id);
    stContext.mpCamera->closeChannel(chn_id);
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
    return 0;
}

