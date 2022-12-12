#define LOG_NDEBUG 0
#define LOG_TAG "sample_AILib"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vo.h>

#include "sample_AILib_config.h"
#include "sample_AILib.h"

using namespace std;
using namespace EyeseeLinux;

static SampleAILibContext *pSampleAILibContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if (pSampleAILibContext != NULL)
    {
        cdx_sem_up(&pSampleAILibContext->mSemExit);
    }
}

EyeseeCameraCallback::EyeseeCameraCallback(SampleAILibContext *pContext)
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

void EyeseeCameraCallback::onVLPRData(int chnId, const AW_AI_CVE_VLPR_RULT_S *data, EyeseeLinux::EyeseeCamera* pCamera)
{
//    if (chnId == mpContext->mCameraInfo.mMPPGeometry.mScalerOutChns[0])
//    {
        int plate_num = data->s32NumOutput;
        alogd("total recorgnize: [%d]", plate_num);
        if (plate_num > 0)
        {
            for(int i=0; i < plate_num; i++)
            {
                alogd("PlateNum = %s", data->astOutput[i].as8PlateNum);
                alogd("rect.left = %d,rect.top = %d ,rect.right = %d,rect.bottom = %d",
                    data->astOutput[i].stPlateRect.s16Left, data->astOutput[i].stPlateRect.s16Top,
                    data->astOutput[i].stPlateRect.s16Right, data->astOutput[i].stPlateRect.s16Bottom);
                alogd("confidence=%d", data->astOutput[i].flConfidence);
            }
        }
//    }
}

void EyeseeCameraCallback::onFaceDetectData(int chnId, const AW_AI_EVE_EVENT_RESULT_S *data, EyeseeLinux::EyeseeCamera* pCamera)
{
//    if (chnId == mpContext->mCameraInfo.mMPPGeometry.mScalerOutChns[0])
//    {
#if 0 //
        alogd("event_num=%d", data->sEvent.s32EventNum);
        for (int i = 0; i < data->sEvent.s32EventNum; i++)
        {
            switch (data->sEvent.astEvents[i].u32Type)
            {
            case AW_AI_EVE_EVENT_FACEDETECT:
                {
                    int human_cnt = 0;
                    alogd("get target num:[%d]", data->sTarget.s32TargetNum);
                    for (int cnt = 0; cnt < data->sTarget.s32TargetNum; cnt++)
                    {
                        switch (data->sTarget.astTargets[cnt].u32Type)
                        {
                        case AW_TGT_TYPE_HUMAN:
                            {
                                RECT_S rect;
                                rect.X = data->sTarget.astTargets[cnt].stRect.s16Left;
                                rect.Y = data->sTarget.astTargets[cnt].stRect.s16Top;
                                rect.Width = data->sTarget.astTargets[cnt].stRect.s16Right -
                                    data->sTarget.astTargets[cnt].stRect.s16Left;
                                rect.Height = data->sTarget.astTargets[cnt].stRect.s16Bottom -
                                    data->sTarget.astTargets[cnt].stRect.s16Top;
                                alogd("get human:[%d]", ++human_cnt);
                                alogd("\tx=[%d],y=[%d],w=[%d],h=[%d]", rect.X, rect.Y, rect.Width, rect.Height);
                            }
                            break;

                        case AW_TGT_TYPE_VEHICLE:
                            alogd("why get vechelic??");
                            break;

                        case AW_TGT_TYPE_UNKNOWN:
                        default:
                            break;
                        }
                    }
                }  
                break;

            case AW_AI_EVE_EVENT_HEADDETECT:
                alogd("not handle this event, AW_AI_EVE_EVENT_HEADDETECT");
                break;
 
            default:
                aloge("unknown event");
                break;
            }
        }
#endif //

        {//
            int human_cnt = 0;
            if (data->sTarget.s32TargetNum >0)
            {
                alogd("get target num:[%d]", data->sTarget.s32TargetNum);
            }
            for (int cnt = 0; cnt < data->sTarget.s32TargetNum; cnt++)
            {
                switch (data->sTarget.astTargets[cnt].u32Type)
                {
                case AW_TGT_TYPE_HUMAN:
                    {
                        RECT_S rect;
                        rect.X = data->sTarget.astTargets[cnt].stRect.s16Left;
                        rect.Y = data->sTarget.astTargets[cnt].stRect.s16Top;
                        rect.Width = data->sTarget.astTargets[cnt].stRect.s16Right -
                            data->sTarget.astTargets[cnt].stRect.s16Left;
                        rect.Height = data->sTarget.astTargets[cnt].stRect.s16Bottom -
                            data->sTarget.astTargets[cnt].stRect.s16Top;
                        alogd("get human:[%d]", ++human_cnt);
                        alogd("x = %d,y = %d,w = %d,h = %d", rect.X, rect.Y, rect.Width, rect.Height);
                    }
                    break;
                case AW_TGT_TYPE_VEHICLE:
                    alogd("why get vechelic??");
                    break;
                case AW_TGT_TYPE_UNKNOWN:
                default:
                    break;
                }
            }
        }
//    }
}

void EyeseeCameraCallback::onMODData(int chnId, const AW_AI_CVE_DTCA_RESULT_S *data, EyeseeLinux::EyeseeCamera *pCamera)
{
//    if (chnId == mpContext->mCameraInfo.mMPPGeometry.mScalerOutChns[0])
//    {
        alogd("mod event_num=%d", data->stEventSet.s32EventNum);

        for (int i = 0; i < data->stEventSet.s32EventNum; i++)
        {
            switch (data->stEventSet.astEvents[i].u32Type)
            {
            case AW_AI_CVEEVT_TYPE_SIGNAL_BAD:
                {
                    alogd("video signal unusual!!!");
                    AW_AI_CVE_DTCA_EVTDAT_S *vexcept = (AW_AI_CVE_DTCA_EVTDAT_S*)(data->stEventSet.astEvents[i].u8Data);
                    if (AW_AI_CVEVE_TYPE_CameraOccluded == vexcept->vexcept.type)
                    {
                        alogd("CameraOccluded, confidence=[%d]", vexcept->vexcept.conf);
                    }
                }
                break;

            case AW_AI_CVEEVT_TYPE_PERIMETER:
                {
                    alogd("get cve event: PERIMETER");
                }
                break;
 
            default:
                aloge("has not init the function event[%d], why receiver event msg???", data->stEventSet.astEvents[i].u32Type);
                break;
            }
        }

        {//
            alogd("get target_num=%d", data->stTargetSet.s32TargetNum);
            for (int i = 0; i < data->stTargetSet.s32TargetNum; i++)
            {        
                int x = data->stTargetSet.astTargets[i].stPoint.s16X;
                int y = data->stTargetSet.astTargets[i].stPoint.s16Y;
                int top = data->stTargetSet.astTargets[i].stRect.s16Top;
                int left = data->stTargetSet.astTargets[i].stRect.s16Left;
                int right = data->stTargetSet.astTargets[i].stRect.s16Right;
                int bottom = data->stTargetSet.astTargets[i].stRect.s16Bottom;
                alogd("target[i]:  point.x=%d, point.y=%d", x, y);
                alogd("rect=%d, %d, %d, %d", top, left, right, bottom);
            }
        }
//    }
}

EyeseeRecorderListener::EyeseeRecorderListener(SampleAILibContext *pContext)
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
            alogd("receive onInfo message: need_set_next_fd, muxer_id[%d]", extra);
            int nMuxerId = extra;
            if (mpContext->mMuxerId != nMuxerId)
            {
                aloge("fatal error! why muxerId is not match[%d]!=[%d]?", mpContext->mMuxerId, nMuxerId);
            }
            static int cnt = 0;
            cnt++;
            char path[64] = {0};
            sprintf(path, "/mnt/extsd/sample_ailib/rec_%d.mp4", cnt);
            mpContext->mpRecorder->setOutputFileSync((char*)path, 0, nMuxerId);
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

SampleAILibContext::SampleAILibContext()
    : mCameraCallbacks(this)
    , mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = -1;
    mVoLayer = -1;
    mpCamera = NULL;
    memset(&mConfigPara,0,sizeof(mConfigPara));
    mMuxerId = -1;
    mpRecorder = NULL;
    mpRecCamera = NULL;
}

SampleAILibContext::~SampleAILibContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpCamera != NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
}

status_t SampleAILibContext::ParseCmdLine(int argc, char *argv[])
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

status_t SampleAILibContext::loadConfig()
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
        mConfigPara.mDisplayWidth = 640; 
        mConfigPara.mDisplayHeight = 480;
		mConfigPara.mPreviewRotation = 0;
        mConfigPara.mTestDuration = 1000; //s
        return SUCCESS;
    }

    memset(&mConfigPara, 0, sizeof(SampleAILibConfig));

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

    mConfigPara.mEveEnable = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_EVE, 0);
    mConfigPara.mModEnable = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_MOD, 0);
    mConfigPara.mVRPLEnable = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_VRPL, 0);

    mConfigPara.mRecEnable = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_ENABLE_RECORD, 0);

    mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_CFG_KEY_TEST_DURATION, 0);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleAILibContext::initCVEFuncParam(AW_AI_CVE_DTCA_PARAM &paramDTCA, AW_AI_CVE_CLBR_PARAM &paramCLBR)
{
/////init CLBR param
    paramCLBR.dim_w = mConfigPara.mCaptureWidth; //640
    paramCLBR.dim_h = mConfigPara.mCaptureHeight; //360;
    paramCLBR.coef.cx = 0.000000;
    paramCLBR.coef.cy = 0.000993;
    paramCLBR.coef.cf = 0.050475;
    paramCLBR.fdlines.num_used = 4;//4;
    paramCLBR.fdlines.lines[0].type = 1;
    paramCLBR.fdlines.lines[0].ref_length = 200;
    paramCLBR.fdlines.lines[0].ref_line.stPStart.s16X = 160;
    paramCLBR.fdlines.lines[0].ref_line.stPStart.s16Y = 30;
    paramCLBR.fdlines.lines[0].ref_line.stPEnd.s16X = 160;
    paramCLBR.fdlines.lines[0].ref_line.stPEnd.s16Y = 50;

    paramCLBR.fdlines.lines[1].type = 1;
    paramCLBR.fdlines.lines[1].ref_length = 100;
    paramCLBR.fdlines.lines[1].ref_line.stPStart.s16X = 160;
    paramCLBR.fdlines.lines[1].ref_line.stPStart.s16Y = 130;
    paramCLBR.fdlines.lines[1].ref_line.stPEnd.s16X = 160;
    paramCLBR.fdlines.lines[1].ref_line.stPEnd.s16Y = 150;

    paramCLBR.fdlines.lines[2].type = 1;
    paramCLBR.fdlines.lines[2].ref_length = 200;
    paramCLBR.fdlines.lines[2].ref_line.stPStart.s16X = 40;
    paramCLBR.fdlines.lines[2].ref_line.stPStart.s16Y = 30;
    paramCLBR.fdlines.lines[2].ref_line.stPEnd.s16X = 40;
    paramCLBR.fdlines.lines[2].ref_line.stPEnd.s16Y = 50;

    paramCLBR.fdlines.lines[3].type = 1;
    paramCLBR.fdlines.lines[3].ref_length = 67;
    paramCLBR.fdlines.lines[3].ref_line.stPStart.s16X = 40;
    paramCLBR.fdlines.lines[3].ref_line.stPStart.s16Y = 230;
    paramCLBR.fdlines.lines[3].ref_line.stPEnd.s16X = 40;
    paramCLBR.fdlines.lines[3].ref_line.stPEnd.s16Y = 250;

/////init DTCA param
    paramDTCA.cParamAlgo.cParamApp.dim_w = mConfigPara.mCaptureWidth; //640;
    paramDTCA.cParamAlgo.cParamApp.dim_h = mConfigPara.mCaptureHeight; //360;
  //set use function.we just use func: cover & move detect
    paramDTCA.cParamAlgo.cParamApp.functions.num_used = 1;
    AW_AI_CVE_VANA_FUNC *pFunction = &paramDTCA.cParamAlgo.cParamApp.functions.funcs[0];
    strcpy(pFunction->name, "perimeter");
    pFunction->enable = 1;
    pFunction->level = 3;
    pFunction->param.perimeter.region.s32Num = 4;
    pFunction->param.perimeter.region.astPoints[0] = {0, 0};
    pFunction->param.perimeter.region.astPoints[1] = {(AW_S16)((mConfigPara.mCaptureWidth-1)/2), 0};
    pFunction->param.perimeter.region.astPoints[2] = {(AW_S16)((mConfigPara.mCaptureWidth-1)/2), (AW_S16)(mConfigPara.mCaptureHeight-1)};
    pFunction->param.perimeter.region.astPoints[3] = {0, (AW_S16)(mConfigPara.mCaptureHeight-1)};
    pFunction->param.perimeter.type_constrain = 1;
    pFunction->param.perimeter.type_mode = 0;
    pFunction->param.perimeter.size_constrain = 0;
    pFunction->param.perimeter.size_min = 1000;
    pFunction->param.perimeter.size_max = 100000;
#if 0    
    strcpy(paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].name, "tripwire");
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].enable = 1;
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].level = 1;
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].param.tripwire.type_mode
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].param.tripwire.type_constrain
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].param.tripwire.size_constrain
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].param.tripwire.twline
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].param.tripwire.size_max
    paramDTCA.cParamAlgo.cParamApp.functions.funcs[1].param.tripwire.size_min
#endif
  //for signal check
    paramDTCA.cParamAlgo.cParamApp.performance.do_istab = 0;
    paramDTCA.cParamAlgo.cParamApp.performance.scene_mode = 0;
    paramDTCA.cParamAlgo.cParamApp.performance.sensitivity = 0;

  //for advenced param set.just to do
    paramDTCA.cParamAlgo.s32UseAdvParam = 0;
    paramDTCA.cParamAlgo.cParamAdv.bgm_period = 15;//15;
    paramDTCA.cParamAlgo.cParamAdv.bgm_period_noise = 0;

  //for IO param
    paramDTCA.cParamInout.s32Width = mConfigPara.mCaptureWidth; //640;
    paramDTCA.cParamInout.s32Height = mConfigPara.mCaptureHeight; //360;
    paramDTCA.cParamInout.u32FrameTime = 1*1000 / mConfigPara.mFrameRate;
    
    return SUCCESS;
}

int main(int argc, char *argv[])
{
    int result = 0;
    SampleAILibContext stContext;
    pSampleAILibContext = &stContext;
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
        viChn = 2;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        mISPGeometry.mScalerOutChns.push_back(viChn);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);

        mISPGeometry.mScalerOutChns.clear();
        cameraId = 1;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 1;
        //cameraInfo.mMPPGeometry.mISPDev = 0;
        mISPGeometry.mISPDev = 0;
        viChn = 1;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        mISPGeometry.mScalerOutChns.push_back(viChn);
        viChn = 3;
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
    alogd("##chn=%d", chn_id);
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
    stContext.mpCamera->setChannelDisplay(chn_id, stContext.mVoLayer);
    stContext.mpCamera->startChannel(chn_id);
    //stContext.mpCamera->startRender(stContext.mCameraInfo.mMPPGeometry.mScalerOutChns[0]);
    alogd("VRPLEnable = %d,EveEnable = %d,ModEnable = %d" ,
            stContext.mConfigPara.mVRPLEnable,
            stContext.mConfigPara.mEveEnable,
            stContext.mConfigPara.mModEnable);

    if (stContext.mConfigPara.mVRPLEnable)
    {
        stContext.mpCamera->setVLPRDataCallback(&stContext.mCameraCallbacks);
        stContext.mpCamera->startVLPRDetect(chn_id);
    }
    if (stContext.mConfigPara.mEveEnable)
    {
        stContext.mpCamera->setFaceDetectDataCallback(&stContext.mCameraCallbacks);
        stContext.mpCamera->startFaceDetect(chn_id);
    }
    if (stContext.mConfigPara.mModEnable)
    {
        AW_AI_CVE_DTCA_PARAM paramDTCA = {0};
        AW_AI_CVE_CLBR_PARAM paramCLBR = {0};
        stContext.mpCamera->getMODParams(chn_id, &paramDTCA, &paramCLBR);
        stContext.initCVEFuncParam(paramDTCA, paramCLBR);
        stContext.mpCamera->setMODParams(chn_id, &paramDTCA, &paramCLBR);

        stContext.mpCamera->setMODDataCallback(&stContext.mCameraCallbacks);
        stContext.mpCamera->startMODDetect(chn_id);
    }

    int chn_id2 = -1;
    if (stContext.mConfigPara.mRecEnable)
    {
//        cameraId = 1;
        /*cameraId = stContext.mConfigPara.cam_id;*/
        EyeseeCamera::getCameraInfo(cameraId, &stContext.mCameraInfo);
        stContext.mpRecCamera = EyeseeCamera::open(cameraId);
        stContext.mpRecCamera->setInfoCallback(&stContext.mCameraCallbacks);
        stContext.mpRecCamera->prepareDevice();
        stContext.mpRecCamera->startDevice();

//        chn_id2 = stContext.mCameraInfo.mMPPGeometry.mScalerOutChns[cameraId*2];
        if(cameraId == 0)
            chn_id2 = 2;
        if(cameraId == 1)
            chn_id2 = 3;
        alogd("##chn2=%d", chn_id2);
        stContext.mpRecCamera->openChannel(chn_id2, true);
        CameraParameters cameraParam;
        stContext.mpRecCamera->getParameters(chn_id2, cameraParam);
        SIZE_S captureSize={(unsigned int)stContext.mConfigPara.mCaptureWidth, (unsigned int)stContext.mConfigPara.mCaptureHeight};
        cameraParam.setVideoSize(captureSize);
        cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
        //cameraParam.setDisplayFrameRate(stContext.mConfigPara.mFrameRate);
        cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
        cameraParam.setVideoBufferNumber(7);
        //cameraParam.setPreviewRotation(stContext.mConfigPara.mPreviewRotation);
        stContext.mpRecCamera->setParameters(chn_id2, cameraParam);
        stContext.mpRecCamera->prepareChannel(chn_id2);
        stContext.mpRecCamera->startChannel(chn_id2);

        alogd("start record record");
        stContext.mpRecorder = new EyeseeRecorder();
        stContext.mpRecorder->setOnInfoListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnDataListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnErrorListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setCameraProxy(stContext.mpRecCamera->getRecordingProxy(), chn_id2);
        stContext.mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
        stContext.mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
        stContext.mpRecorder->setMaxDuration(1*60*1000);
        char rec_name0[] = "/mnt/extsd/rec_0.mp4";
//        char rec_name0[] = "/tmp/rec_0.mp4";
        stContext.mMuxerId = stContext.mpRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, rec_name0, 0, false);
        stContext.mpRecorder->setVideoFrameRate(stContext.mConfigPara.mFrameRate);
        stContext.mpRecorder->setVideoSize(stContext.mConfigPara.mCaptureWidth, stContext.mConfigPara.mCaptureHeight);
        stContext.mpRecorder->setVideoEncoder(PT_H264);
        stContext.mpRecorder->setVideoEncodingBitRate(8*1024*1024);
        stContext.mpRecorder->setAudioSamplingRate(8000);
        stContext.mpRecorder->setAudioChannels(1);
        stContext.mpRecorder->setAudioEncodingBitRate(12200);
        stContext.mpRecorder->setAudioEncoder(PT_AAC);
        stContext.mpRecorder->prepare();
        stContext.mpRecorder->start();
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
    if (stContext.mConfigPara.mRecEnable)
    {
        alogd("stop camera chn[%d] & record", chn_id2);
        if (stContext.mpRecorder != NULL)
        {
            stContext.mpRecorder->stop();
            delete stContext.mpRecorder;
            stContext.mpRecorder = NULL;
        }
        {
            stContext.mpRecCamera->stopChannel(chn_id2);
            stContext.mpRecCamera->releaseChannel(chn_id2);
            stContext.mpRecCamera->closeChannel(chn_id2);
            stContext.mpRecCamera->stopDevice();
            stContext.mpRecCamera->releaseDevice();
            EyeseeCamera::close(stContext.mpRecCamera);
        }
    }

    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
    if (stContext.mConfigPara.mModEnable)
    {
        stContext.mpCamera->stopMODDetect(chn_id);
    }
    if (stContext.mConfigPara.mEveEnable)
    {
        stContext.mpCamera->stopFaceDetect(chn_id);
    }
    if (stContext.mConfigPara.mVRPLEnable)
    {
        stContext.mpCamera->stopVLPRDetect(chn_id);
    }
    //stContext.mpCamera->stopRender(chn_id);
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
    cout<<"bye, sample_AILib!"<<endl;
    return result;
}
