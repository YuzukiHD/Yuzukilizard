//#define LOG_NDEBUG 0
#define LOG_TAG "sample_EyeseeIse"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>
#include <sys/time.h>
#include <dlfcn.h>

#include <hwdisplay.h>
#include <record_writer.h>
#include <confparser.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>
#include <EyeseeISE.h>
#include <SystemBase.h>

#include "sample_EyeseeIse_config.h"
#include "sample_EyeseeIse.h"

using namespace std;
using namespace EyeseeLinux;

SampleEyeseeIseContext *pSampleContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleContext != NULL)
    {
        cdx_sem_up(&pSampleContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId==mpContext->mCamChnId[0] || chnId==mpContext->mCamChnId[1])
            {
                alogd("chnId(%d) notify app preview!", chnId);
//                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCamChnId[0]);
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
    //aloge("fatal error! channel %d picture data size %d, not implement in this sample.", chnId, size);
    int ret = -1;
    alogd("cam(%p:%d) takes picture with data size %d", pCamera, chnId, size);
    char picPath[128];
    sprintf(picPath, "/mnt/extsd/sample_eyeseeise_fish/CamPic[%p]_[%02d].jpg", pCamera, mpContext->mCamPicNum++);
    FILE *fp = fopen(picPath, "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);

}

EyeseeCameraListener::EyeseeCameraListener(SampleEyeseeIseContext *pContext)
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
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done, muxer_id[%d]", extra);
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



EyeseeRecorderListener::EyeseeRecorderListener(SampleEyeseeIseContext *pOwner)
    : mpOwner(pOwner)
{
}

EyeseeIseListener::EyeseeIseListener(SampleEyeseeIseContext *pContext)
    :mpContext(pContext)
{
}

void EyeseeIseListener::onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE)
{
    aloge("receive erro. chnId[%d], error[%d]", chnId, error);
}

void EyeseeIseListener::onPictureTaken(int chnId, const void *data, int size, EyeseeISE* pISE)
{
    //aloge("fatal error! ise channel %d picture data size %d, not implement in this sample.", chnId, size);
    int ret = -1;
    alogd("ise(%p:%d) takes picture with data size %d", pISE, chnId, size);
    char picPath[128];
    sprintf(picPath, "/mnt/extsd/sample_eyeseeise/IsePic[%p]_[%02d].jpg", pISE, mpContext->mIsePicNum++);
    FILE *fp = fopen(picPath, "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
}


SampleEyeseeIseContext::SampleEyeseeIseContext()
    :mCameraListener(this)
    ,mIseListener(this)
    ,mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera[0] = NULL;
    mpCamera[1] = NULL;
    mpIse[0] = NULL;
    mpIse[1] = NULL;
    mpIse[2] = NULL;
    mpIse[3] = NULL;
    mpRecorder[0] = NULL;
    mpRecorder[1] = NULL;
    mpRecorder[2] = NULL;
    mpRecorder[3] = NULL;
    mFileNum = 0;
    mCamPicNum = 0;
    mIsePicNum = 0;
    mCamChnId = 0;
    mMuxerId = 0;
}

SampleEyeseeIseContext::~SampleEyeseeIseContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera[0]!=NULL || mpCamera[1]!=NULL)
    {
        aloge("fatal error! some EyeseeCameras is not destruct!");
    }
    if(mpIse[0]!=NULL || mpIse[1]!=NULL || mpIse[2]!=NULL || mpIse[3]!=NULL)
    {
        aloge("fatal error! some EyeseeIses is not destruct!");
    }
    if(mpRecorder[0]!=NULL || mpRecorder[1]!=NULL || mpRecorder[2]!=NULL || mpRecorder[3]!=NULL)
    {
        aloge("fatal error! some EyeseeRecorders is not destruct!");
    }
}

status_t SampleEyeseeIseContext::ParseCmdLine(int argc, char *argv[])
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
        }
        else if(!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_EyeseeIse.conf\n";
            cout<<helpString<<endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += "], type -h to get how to set parameter!";
            cout<<ignoreString<<endl;
        }
        i++;
    }
    return ret;
}
status_t SampleEyeseeIseContext::loadConfig()
{
    int ret;
    char *ptr;
    return SUCCESS;
}


int ise_input_width;
int ise_input_height;
int ise_output_width;
int ise_output_height;

EyeseeISE* openIseDevice(ISE_MODE_E mode, EyeseeIseListener *pListener)
{
    alogd("##########################################");
    alogd("############## open IseDev ###############");
    alogd("##########################################");

    alogd("[ise] step1: open");
    EyeseeISE *pIse = EyeseeISE::open();
    pIse->setErrorCallback(pListener);

    alogd("[ise] step2: prepareDevice");
    pIse->prepareDevice(mode);

    return pIse;
}

ISE_CHN openIseChannel(EyeseeISE *pIse, ISE_CHN_ATTR_S *pIseAttr, VO_LAYER *pVoLayer, CameraParameters &refCamParam, RECT_S *pDispRect)
{
    alogd("########################################");
    alogd("############# open IseChn ##############");
    alogd("########################################");

    alogd("[ise] step4: openChannel");
    ISE_CHN ise_chn = pIse->openChannel(pIseAttr);
    if (ise_chn < 0) {
        aloge("open ise channel fail!");
        return -1;
    }

    alogd("[ise] step5: setParameters");
    pIse->setParameters(ise_chn, refCamParam);

    if (pDispRect->Height!=0 && pDispRect->Width!=0)
    {
        alogd("[ise] step6: request hlay");
        int ise_hlay = 0;
        while(ise_hlay < VO_MAX_LAYER_NUM)
        {
            if(SUCCESS == AW_MPI_VO_EnableVideoLayer(ise_hlay))
            {
                alogd("ise get VoLayerId: %d", ise_hlay);
                break;
            }
            ise_hlay++;
        }
        if(ise_hlay >= VO_MAX_LAYER_NUM)
        {
            aloge("fatal error! enable video layer fail!");
            return -1;
        }
        *pVoLayer = ise_hlay;

        alogd("[ise] step7: update disp rect");
        VO_VIDEO_LAYER_ATTR_S layer_attr;
        AW_MPI_VO_GetVideoLayerAttr(ise_hlay, &layer_attr);
        layer_attr.stDispRect.X = pDispRect->X;
        layer_attr.stDispRect.Y = pDispRect->Y;
        layer_attr.stDispRect.Width = pDispRect->Width;
        layer_attr.stDispRect.Height = pDispRect->Height;
        AW_MPI_VO_SetVideoLayerAttr(ise_hlay, &layer_attr);
        AW_MPI_VO_OpenVideoLayer(ise_hlay);

        alogd("[ise] step8: set ise_chn(%d) to vo_layer(%d)", ise_chn, ise_hlay);
        pIse->setChannelDisplay(ise_chn, ise_hlay);
    }

    pIse->startChannel(ise_chn);

    return ise_chn;
}

int startIseDevice(EyeseeISE *pIse, vector<CameraChannelInfo> &refCamChnInfo)
{
    alogd("[ise] step9: setCamera");
    pIse->setCamera(refCamChnInfo);

    alogd("[ise] step10: startDevice");
    pIse->startDevice();

    return 0;
}

int initIseChnAttr(ISE_CHN_ATTR_S *pIseChnAttr, ISE_MODE_E mode)
{
    ISE_CFG_PARA_MO *pOneFishCfgPara;
    DFISH *pTwoFish;
    ISE *pTwoIse;
    double cam_matr[3][3] = {{1.0115217430885405e+003, 0., 9.60e+002},
                             {0., 1.0115217430885405e+003, 5.40e+002},
                             {0., 0., 1.}};
    double dist[8] = {6.1446514462273027e-001, -2.5223829431213818e-001,
                      -7.6746144767994573e-004, -2.0856862740027446e-004,
                      -6.2572787678747341e-002, 9.5921588321979923e-001,
                      -9.8592064991125133e-002, -1.7993258997361919e-001};
    double cam_matr_prime[3][3] = {{5.5839984130859375e+002, 0., 9.60e+002},
                                   {0., 5.5591241455078125e+002, 5.40e+002},
                                   {0., 0., 1.}};

    switch (mode)
    {
    case ISE_MODE_ONE_FISHEYE:
        alogd("############### [IseMode: OneFishEye] ################");
        pOneFishCfgPara = &pIseChnAttr->mode_attr.mFish.ise_cfg;
        pOneFishCfgPara->dewarp_mode = WARP_NORMAL;
        if (pOneFishCfgPara->dewarp_mode == WARP_NORMAL)
        {
            pOneFishCfgPara->mount_mode = MOUNT_TOP;
            pOneFishCfgPara->pan = 90;
            pOneFishCfgPara->tilt = 45;
            pOneFishCfgPara->zoom = 2;
        }
        pOneFishCfgPara->in_h = ise_input_height;//720;
        pOneFishCfgPara->in_w = ise_input_width;//720;
        pOneFishCfgPara->in_luma_pitch = pOneFishCfgPara->in_w;
        pOneFishCfgPara->in_chroma_pitch = pOneFishCfgPara->in_w;
        pOneFishCfgPara->in_yuv_type = 0;
        pOneFishCfgPara->out_en[0] = 1;
        pOneFishCfgPara->out_h[0] = ise_output_height;
        pOneFishCfgPara->out_w[0] = ise_output_width;
        pOneFishCfgPara->out_luma_pitch[0] = pOneFishCfgPara->out_w[0];
        pOneFishCfgPara->out_chroma_pitch[0] = pOneFishCfgPara->out_w[0];
        pOneFishCfgPara->out_flip[0] = 0;
        pOneFishCfgPara->out_mirror[0] = 0;
        pOneFishCfgPara->out_yuv_type = 0;
        pOneFishCfgPara->p = pOneFishCfgPara->in_w/3.1415;
        pOneFishCfgPara->cx = pOneFishCfgPara->in_w/2;
        pOneFishCfgPara->cy = pOneFishCfgPara->in_h/2;
        pOneFishCfgPara->out_yuv_type = 0;
        break;

    case ISE_MODE_TWO_FISHEYE:
        alogd("############### [IseMode: TwoFishEye] ################");
        pTwoFish = &pIseChnAttr->mode_attr.mDFish;
        pTwoFish->ise_cfg.out_en[0] = 1;
        pTwoFish->ise_cfg.in_h = ise_input_height;//720;//320;
        pTwoFish->ise_cfg.in_w = ise_input_width;//720;//320;
        pTwoFish->ise_cfg.out_h[0] = pTwoFish->ise_cfg.in_h;//320;
        pTwoFish->ise_cfg.out_w[0] = pTwoFish->ise_cfg.in_w*2;//640;
        pTwoFish->ise_cfg.p0 = (float)pTwoFish->ise_cfg.in_h/3.1415;
        pTwoFish->ise_cfg.cx0 = pTwoFish->ise_cfg.in_w/2;
        pTwoFish->ise_cfg.cy0 = pTwoFish->ise_cfg.in_h/2;
        pTwoFish->ise_cfg.p1 = (float)pTwoFish->ise_cfg.in_h/3.1415;
        pTwoFish->ise_cfg.cx1 = pTwoFish->ise_cfg.in_w/2;
        pTwoFish->ise_cfg.cy1 = pTwoFish->ise_cfg.in_h/2;
        pTwoFish->ise_cfg.in_yuv_type = 0;
        pTwoFish->ise_cfg.out_flip[0] = 0;
        pTwoFish->ise_cfg.out_mirror[0] = 0;
        pTwoFish->ise_cfg.out_yuv_type = 0;
        pTwoFish->ise_cfg.in_luma_pitch = pTwoFish->ise_cfg.in_w;//720;
        pTwoFish->ise_cfg.in_chroma_pitch = pTwoFish->ise_cfg.in_w;//720;
        pTwoFish->ise_cfg.out_luma_pitch[0] = pTwoFish->ise_cfg.out_w[0];//640;
        pTwoFish->ise_cfg.out_chroma_pitch[0] = pTwoFish->ise_cfg.out_w[0];//640;
        break;

    case ISE_MODE_TWO_ISE:
        alogd("############### [IseMode: TwoIse] ################");
        pTwoIse = &pIseChnAttr->mode_attr.mIse;
        // init ISE_CFG_PARA_STI
        pTwoIse->ise_cfg.ncam       = 2;
        pTwoIse->ise_cfg.in_h       = ise_input_height;
        pTwoIse->ise_cfg.in_w       = ise_input_width;
        pTwoIse->ise_cfg.pano_h     = pTwoIse->ise_cfg.in_h;
        pTwoIse->ise_cfg.pano_w     = pTwoIse->ise_cfg.in_w*2;
        pTwoIse->ise_cfg.yuv_type   = 0;
        pTwoIse->ise_cfg.p0         = 51.5f;
        pTwoIse->ise_cfg.p1         = 128.5f;
        pTwoIse->ise_cfg.ov         = 320;

        // 根据所使用原图缩放比例对camera matrix进行缩放
//        for (int i = 0; i < 2; i++)
//        {
//            for (int j = 0; j < 3; j++)
//            {
//                cam_matr[i][j] = cam_matr[i][j] / (1080.0/pTwoIse->ise_cfg.in_h);
//                cam_matr_prime[i][j] = cam_matr_prime[i][j] / (1080.0/pTwoIse->ise_cfg.in_h);
//            }
//        }
        // init ISE_ADV_PARA_STI
//        memcpy(&pTwoIse->ise_adv.calib_matr, cam_matr, 3*3*sizeof(double));
//        memcpy(&pTwoIse->ise_adv.calib_matr_cv, cam_matr_prime, 3*3*sizeof(double));
//        memcpy(&pTwoIse->ise_adv.distort, dist, 3*3*sizeof(double));
//        pTwoIse->ise_adv.stre_coeff         = 1.0;
//        pTwoIse->ise_adv.offset_r2l         = 0;
//        pTwoIse->ise_adv.pano_fov           = 186.0f;
//        pTwoIse->ise_adv.t_angle            = 0.0f;
//        pTwoIse->ise_adv.hfov               = 78.0f;
//        pTwoIse->ise_adv.wfov               = 124.0f;
//        pTwoIse->ise_adv.wfov_rev           = 124.0f;
//        pTwoIse->ise_adv.in_luma_pitch      = pTwoIse->ise_cfg.in_w;
//        pTwoIse->ise_adv.in_chroma_pitch    = pTwoIse->ise_cfg.in_w;
//        pTwoIse->ise_adv.pano_luma_pitch    = pTwoIse->ise_cfg.in_w*2;
//        pTwoIse->ise_adv.pano_chroma_pitch  = pTwoIse->ise_cfg.in_w*2;
        // init ISE_PROCCFG_PARA_STI
        pTwoIse->ise_proccfg.pano_flip      = 0;
        pTwoIse->ise_proccfg.pano_mirr      = 0;
        break;

    default:
        aloge("############### [IseMode: unknown] ################");
        break;
    }

    return 0;
}

#define CAM0
#define CAM0_PREVIEW
//#define CAM0_REC

#define CAM1
#define CAM1_PREVIEW
//#define CAM1_REC

//#define ISE_ONE_FISHEYE
//#define PTZ0
//#define PTZ0_PHOTO_FAST
//#define PTZ0_PHOTO_CONTINUOUS
//#define PTZ0_REC
//#define PTZ1
//#define PTZ2
//#define PTZ3
//#define PTZ3_REC

//#define ISE_TWO_FISHEYE
//#define DFISH_REC

//#define ISE_TWO_ISE
//#define ISE_REC

int main(int argc, char *argv[])
{
    int result = 0;
    int ret;
    cout<<"hello, sample_EyeseeIse!"<<endl;
    SampleEyeseeIseContext stContext;
    pSampleContext = &stContext;
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

    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    alogd("---------------------start ise sample-------------------------");

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();


    int cam0_id, cam1_id;
    CameraParameters cameraParam;
    SIZE_S captureSize;
    VO_LAYER hlay;

    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer. not display, but layer still exist!

    //config camera
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
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
        //viChn = 0;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        //viChn = 2;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        mISPGeometry.mISPDev = 1;
        mISPGeometry.mScalerOutChns.push_back(0);
        mISPGeometry.mScalerOutChns.push_back(2);
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
        //viChn = 1;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        //viChn = 3;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(viChn);
        mISPGeometry.mISPDev = 0;
        mISPGeometry.mScalerOutChns.push_back(1);
        mISPGeometry.mScalerOutChns.push_back(3);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

#ifdef ISE_ONE_FISHEYE
    #warning "#################[compile use mode: ISE_ONE_FISHEYE]#################"
    int cam0_cap_width = 1920;
    int cam0_cap_height = 1080;
    int cam1_cap_width = 720;
    int cam1_cap_height = 720;
#elif defined ISE_TWO_FISHEYE
    #warning "#################[compile use mode: ISE_TWO_FISHEYE]#################"
    int cam0_cap_width = 720;
    int cam0_cap_height = 720;
    int cam1_cap_width = 720;
    int cam1_cap_height = 720;
#elif defined ISE_TWO_ISE
    #warning "#################[compile use mode: ISE_TWO_ISE]#################"
    int cam0_cap_width = 1920;
    int cam0_cap_height = 1080;
    int cam1_cap_width = 1920;
    int cam1_cap_height = 1080;
#elif (defined(CAM0) || defined(CAM1))
    #warning "#################[compile use mode: CAM0]#################"
    int cam0_cap_width = 1920;
    int cam0_cap_height = 1080;
    int cam1_cap_width = 1920;
    int cam1_cap_height = 1080;
#else
    #error "Unknown ISE type!"
#endif

#ifdef CAM0
    alogd("###################################");
    alogd("########### start Cam0! ###########");
    alogd("###################################");

    alogd("---------step1: init cameraInfo end---------");

    cam0_id = 0;
    CameraInfo& cameraInfoRef0 = stContext.mCameraInfo[0];
    EyeseeCamera::getCameraInfo(cam0_id, &cameraInfoRef0);
    stContext.mpCamera[0] = EyeseeCamera::open(cam0_id);
    stContext.mpCamera[0]->setInfoCallback(&stContext.mCameraListener);
    stContext.mpCamera[0]->prepareDevice();
    stContext.mpCamera[0]->startDevice();
    alogd("---------step2: startDevice end---------");

    int cam0_chnX_id = cameraInfoRef0.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[cam0_id*2+1];
    stContext.mCamChnId[0] = cam0_chnX_id;
    stContext.mpCamera[0]->openChannel(cam0_chnX_id, TRUE);
    alogd(">>>>> CAM0_INFO(%d, %p)", stContext.mCamChnId[0], stContext.mpCamera[0]);
    alogd("---------step3: init channel end---------");

    stContext.mpCamera[0]->getParameters(cam0_chnX_id, cameraParam);
    captureSize.Width = cam0_cap_width;
    captureSize.Height = cam0_cap_height;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(30);
    cameraParam.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    cameraParam.setVideoBufferNumber(5);
    cameraParam.setPreviewRotation(90);
    ret = stContext.mpCamera[0]->setParameters(cam0_chnX_id, cameraParam);
    stContext.mpCamera[0]->prepareChannel(cam0_chnX_id);
    alogd("---------step4: update camera param again end---------");

#ifdef CAM0_PREVIEW
    hlay = 0;
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
    alogd("---------step5: open vo VideoLayer(%d) end---------", hlay);

    AW_MPI_VO_GetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = 720;
    stContext.mLayerAttr.stDispRect.Height = 640;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    AW_MPI_VO_OpenVideoLayer(hlay);
    stContext.mCamVoLayer[0] = hlay;
    alogd("---------step6: update vo VideoLayer attr end---------");

    alogd("---------step7: prepare preview...---------");
    stContext.mpCamera[0]->setChannelDisplay(cam0_chnX_id, stContext.mCamVoLayer[0]);
#endif
    stContext.mpCamera[0]->startChannel(cam0_chnX_id);
    alogd("---------step8: start preview!---------");

    sleep(5);

#endif


#ifdef CAM1
    alogd("###################################");
    alogd("########### start Cam1! ###########");
    alogd("###################################");

    alogd("---------step1: init cameraInfo end---------");

    cam1_id = 1;
    CameraInfo& cameraInfoRef1 = stContext.mCameraInfo[1];
    EyeseeCamera::getCameraInfo(cam1_id, &cameraInfoRef1);
    stContext.mpCamera[1] = EyeseeCamera::open(cam1_id);
    stContext.mpCamera[1]->setInfoCallback(&stContext.mCameraListener);
    stContext.mpCamera[1]->prepareDevice();
    stContext.mpCamera[1]->startDevice();
    alogd("---------step2: startDevice end---------");

    int cam1_chnX_id = cameraInfoRef1.mMPPGeometry.mISPGeometrys[1].mScalerOutChns[cam1_id*2+1];
    stContext.mCamChnId[1] = cam1_chnX_id;
    stContext.mpCamera[1]->openChannel(cam1_chnX_id, TRUE);
    alogd(">>>>> CAM1_INFO(%d, %p)", stContext.mCamChnId[1], stContext.mpCamera[1]);
    alogd("---------step3: init channel end---------");

    stContext.mpCamera[1]->getParameters(cam1_chnX_id, cameraParam);
    captureSize.Width = cam1_cap_width;
    captureSize.Height = cam1_cap_height;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(30);
    cameraParam.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    cameraParam.setVideoBufferNumber(5);
    cameraParam.setPreviewRotation(90);
    ret = stContext.mpCamera[1]->setParameters(cam1_chnX_id, cameraParam);
    stContext.mpCamera[1]->prepareChannel(cam1_chnX_id);
    alogd("---------step4: update camera param again end---------");

#ifdef CAM1_PREVIEW
    hlay = 0;
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
    alogd("---------step5: open vo VideoLayer end---------");

    AW_MPI_VO_GetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 640;
    stContext.mLayerAttr.stDispRect.Width = 720;
    stContext.mLayerAttr.stDispRect.Height = 640;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    AW_MPI_VO_OpenVideoLayer(hlay);
    stContext.mCamVoLayer[1] = hlay;
    alogd("---------step6: update vo VideoLayer(%d) attr end---------", hlay);

    alogd("---------step7: prepare preview...---------");
    stContext.mpCamera[1]->setChannelDisplay(cam1_chnX_id, stContext.mCamVoLayer[1]);
#endif
    stContext.mpCamera[1]->startChannel(cam1_chnX_id);
    alogd("---------step8: start preview!---------");

    sleep(5);

#endif


#ifdef ISE_ONE_FISHEYE

    ise_input_width = cam0_cap_width;
    ise_input_height = cam0_cap_height;
    ise_output_width = 384;
    ise_output_height = 360;

#ifdef PTZ0
    /******************** [step I] ********************/
    stContext.mpIse[0] = openIseDevice(ISE_MODE_ONE_FISHEYE, &stContext.mIseListener);

    /******************** [step II] *******************/
    // init ise_attr
    memset(&stContext.mIseChnAttr[0], 0, sizeof(ISE_CHN_ATTR_S));
    initIseChnAttr(&stContext.mIseChnAttr[0], ISE_MODE_ONE_FISHEYE);
    // init ise_camParam
    stContext.mpCamera[0]->getParameters(stContext.mCamChnId[0], cameraParam);
    SIZE_S ise_chn_rect;
    ise_chn_rect.Width = ise_output_width;    // complete same with CamInfo except width&height, for rec
    ise_chn_rect.Height = ise_output_height;
    cameraParam.setVideoSize(ise_chn_rect);
    cameraParam.setPreviewRotation(90);
    // init disp area
    RECT_S disp_rect;
    disp_rect.X = 0;
    disp_rect.Y = 200;
    disp_rect.Height = 350;
    disp_rect.Width = 350;
    stContext.mIseChnId[0][0] = openIseChannel(stContext.mpIse[0], &stContext.mIseChnAttr[0], &stContext.mIseVoLayer[0], cameraParam, &disp_rect);

    /******************** [step III] *******************/
    CameraChannelInfo cam_chn_info;
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
    cam_chn_info.mpCameraRecordingProxy = stContext.mpCamera[0]->getRecordingProxy();
    alogd("set CameraChannelInfo(%d,%p) to PTZ0(%d,%p)",
        cam_chn_info.mnCameraChannel, cam_chn_info.mpCamera, stContext.mIseChnId[0][0], stContext.mpIse[0]);
    vector<CameraChannelInfo> cam_chn_vec;
    cam_chn_vec.push_back(cam_chn_info);
    startIseDevice(stContext.mpIse[0], cam_chn_vec);

    sleep(1);

#ifdef PTZ0_PHOTO_CONTINUOUS
    stContext.mpIse[0]->getParameters(stContext.mIseChnId[0][0], cameraParam);
    SIZE_S size;
    cameraParam.getVideoSize(size);
    cameraParam.setPictureMode(TAKE_PICTURE_MODE_CONTINUOUS);
    cameraParam.setContinuousPictureNumber(20);
    cameraParam.setContinuousPictureIntervalMs(500);
    cameraParam.setPictureSize(SIZE_S{720,480});
    cameraParam.setJpegThumbnailSize(SIZE_S{320,240});
    stContext.mpIse[0]->setParameters(stContext.mIseChnId[0][0], cameraParam);
    stContext.mpIse[0]->takePicture(stContext.mIseChnId[0][0], NULL, NULL, &stContext.mIseListener);
    sleep(5);
    stContext.mpIse[0]->cancelContinuousPicture(stContext.mIseChnId[0][0]);
    sleep(5);
#endif

#ifdef PTZ0_PHOTO_FAST
    stContext.mpIse[0]->getParameters(stContext.mIseChnId[0][0], cameraParam);
    cameraParam.setPictureMode(TAKE_PICTURE_MODE_FAST);
    cameraParam.setPictureSize(SIZE_S{720,480});
    stContext.mpIse[0]->setParameters(stContext.mIseChnId[0][0], cameraParam);
    sleep(1);
    stContext.mpIse[0]->takePicture(stContext.mIseChnId[0][0], NULL, NULL, &stContext.mIseListener);
    sleep(1);
    stContext.mpIse[0]->takePicture(stContext.mIseChnId[0][0], NULL, NULL, &stContext.mIseListener);
    sleep(1);
    stContext.mpIse[0]->takePicture(stContext.mIseChnId[0][0], NULL, NULL, &stContext.mIseListener);
    sleep(1);
    stContext.mpIse[0]->takePicture(stContext.mIseChnId[0][0], NULL, NULL, &stContext.mIseListener);
    sleep(1);
    stContext.mpIse[0]->takePicture(stContext.mIseChnId[0][0], NULL, NULL, &stContext.mIseListener);
    sleep(5);
#endif
    sleep(1);
#endif  /* PTZ0 */

#ifdef PTZ1
    /******************** [step I] ********************/
    stContext.mpIse[1] = openIseDevice(ISE_MODE_ONE_FISHEYE, &stContext.mIseListener);

    /******************** [step II] *******************/
    // init ise_attr
    memset(&stContext.mIseChnAttr[1], 0, sizeof(ISE_CHN_ATTR_S));
    initIseChnAttr(&stContext.mIseChnAttr[1], ISE_MODE_ONE_FISHEYE);
    // init ise_camParam
    stContext.mpCamera[0]->getParameters(stContext.mCamChnId[0], cameraParam);
    ise_chn_rect.Width = ise_output_width;
    ise_chn_rect.Height = ise_output_height;
    cameraParam.setVideoSize(ise_chn_rect);
    cameraParam.setPreviewRotation(90);
    // init disp area
    disp_rect.X = 0;
    disp_rect.Y = 700;
    disp_rect.Height = 350;
    disp_rect.Width = 350;
    stContext.mIseChnId[1][0] = openIseChannel(stContext.mpIse[1], &stContext.mIseChnAttr[1], &stContext.mIseVoLayer[1], cameraParam, &disp_rect);

    /******************** [step III] *******************/
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
    cam_chn_info.mpCameraRecordingProxy = stContext.mpCamera[0]->getRecordingProxy();
    alogd("set CameraChannelInfo(%d,%p) to PTZ1(%d,%p)",
        cam_chn_info.mnCameraChannel, cam_chn_info.mpCamera, stContext.mIseChnId[1][0], stContext.mpIse[1]);
    cam_chn_vec.clear();
    cam_chn_vec.push_back(cam_chn_info);
    startIseDevice(stContext.mpIse[1], cam_chn_vec);
    sleep(1);
#endif

#ifdef PTZ2
    /******************** [step I] ********************/
    stContext.mpIse[2] = openIseDevice(ISE_MODE_ONE_FISHEYE, &stContext.mIseListener);

    /******************** [step II] *******************/
    // init ise_attr
    memset(&stContext.mIseChnAttr[2], 0, sizeof(ISE_CHN_ATTR_S));
    initIseChnAttr(&stContext.mIseChnAttr[2], ISE_MODE_ONE_FISHEYE);
    // init ise_camParam
    stContext.mpCamera[0]->getParameters(stContext.mCamChnId[0], cameraParam);
    ise_chn_rect.Width = ise_output_width;
    ise_chn_rect.Height = ise_output_height;
    cameraParam.setVideoSize(ise_chn_rect);
    cameraParam.setPreviewRotation(90);
    // init disp area
    disp_rect.X = 370;
    disp_rect.Y = 200;
    disp_rect.Height = 350;
    disp_rect.Width = 349;
    stContext.mIseChnId[2][0] = openIseChannel(stContext.mpIse[2], &stContext.mIseChnAttr[2], &stContext.mIseVoLayer[2], cameraParam, &disp_rect);

    /******************** [step III] *******************/
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
    cam_chn_info.mpCameraRecordingProxy = stContext.mpCamera[0]->getRecordingProxy();
    alogd("set CameraChannelInfo(%d,%p) to PTZ2(%d,%p)",
        cam_chn_info.mnCameraChannel, cam_chn_info.mpCamera, stContext.mIseChnId[2][0], stContext.mpIse[2]);
    cam_chn_vec.clear();
    cam_chn_vec.push_back(cam_chn_info);
    startIseDevice(stContext.mpIse[2], cam_chn_vec);
    sleep(1);
#endif

#ifdef PTZ3
    /******************** [step I] ********************/
    stContext.mpIse[3] = openIseDevice(ISE_MODE_ONE_FISHEYE, &stContext.mIseListener);

    /******************** [step II] *******************/
    /****************** [chn0, not disp] **********************/
    // init ise_attr
    memset(&stContext.mIseChnAttr[3], 0, sizeof(ISE_CHN_ATTR_S));
    initIseChnAttr(&stContext.mIseChnAttr[3], ISE_MODE_ONE_FISHEYE);
    // init ise_camParam
    stContext.mpCamera[0]->getParameters(stContext.mCamChnId[0], cameraParam);
    ise_chn_rect.Width = ise_output_width;
    ise_chn_rect.Height = ise_output_height;
    cameraParam.setVideoSize(ise_chn_rect);
    cameraParam.setPreviewRotation(90);
    // reset disp area, not disp
    memset(&disp_rect, 0, sizeof(RECT_S));
    stContext.mIseChnId[3][0] = openIseChannel(stContext.mpIse[3], &stContext.mIseChnAttr[3], &stContext.mIseVoLayer[3] , cameraParam, &disp_rect);
    /****************** [chn1, disp] **********************/
    ISE_CFG_PARA_MO *pFishCfgPara = &stContext.mIseChnAttr[3].mode_attr.mFish.ise_cfg;
    pFishCfgPara->out_en[1] = 1;
    pFishCfgPara->out_w[1] = ise_output_width;
    pFishCfgPara->out_h[1] = ise_output_height;
    pFishCfgPara->out_flip[1] = 0;
    pFishCfgPara->out_mirror[1] = 0;
    pFishCfgPara->out_luma_pitch[1] = ise_output_width;
    pFishCfgPara->out_chroma_pitch[1] = ise_output_width;
    // set disp area
    disp_rect.X = 370;
    disp_rect.Y = 700;
    disp_rect.Height = 350;
    disp_rect.Width = 350;
    stContext.mIseChnId[3][1] = openIseChannel(stContext.mpIse[3], &stContext.mIseChnAttr[3], &stContext.mIseVoLayer[3], cameraParam, &disp_rect);
    /****************** [chn2, not disp] **********************/
    pFishCfgPara->out_en[2] = 1;
    pFishCfgPara->out_w[2] = ise_output_width;
    pFishCfgPara->out_h[2] = ise_output_height;
    pFishCfgPara->out_flip[2] = 0;
    pFishCfgPara->out_mirror[2] = 0;
    pFishCfgPara->out_luma_pitch[2] = ise_output_width;
    pFishCfgPara->out_chroma_pitch[2] = ise_output_width;
    // reset disp area
    memset(&disp_rect, 0, sizeof(RECT_S));
    stContext.mIseChnId[3][2] = openIseChannel(stContext.mpIse[3], &stContext.mIseChnAttr[3], &stContext.mIseVoLayer[3], cameraParam, &disp_rect);
    /****************** [chn3, not disp] **********************/
    pFishCfgPara->out_en[3] = 1;
    pFishCfgPara->out_w[3] = ise_output_width;
    pFishCfgPara->out_h[3] = ise_output_height;
    pFishCfgPara->out_flip[3] = 0;
    pFishCfgPara->out_mirror[3] = 0;
    pFishCfgPara->out_luma_pitch[3] = ise_output_width;
    pFishCfgPara->out_chroma_pitch[3] = ise_output_width;
    // reset disp area
    memset(&disp_rect, 0, sizeof(RECT_S));
    stContext.mIseChnId[3][3] = openIseChannel(stContext.mpIse[3], &stContext.mIseChnAttr[3], &stContext.mIseVoLayer[3], cameraParam, &disp_rect);

    /******************** [step III] *******************/
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
    cam_chn_info.mpCameraRecordingProxy = stContext.mpCamera[0]->getRecordingProxy();
    cam_chn_vec.clear();
    cam_chn_vec.push_back(cam_chn_info);
    startIseDevice(stContext.mpIse[3], cam_chn_vec);
    sleep(1);
#endif

#endif /* ISE_ONE_FISHEYE */


#ifdef ISE_TWO_FISHEYE
    alogd("---------------------PREPARE ISE_TWO_FISHEYE----------------------");

    ise_input_width = cam0_cap_width;
    ise_input_height = cam0_cap_height;
    ise_output_width = ise_input_width * 2;
    ise_output_height = cam0_cap_height;

    /******************** [step I] ********************/
    stContext.mpIse[0] = openIseDevice(ISE_MODE_TWO_FISHEYE, &stContext.mIseListener);

    /******************** [step II] *******************/
    // init ise_attr
    memset(&stContext.mIseChnAttr[0], 0, sizeof(ISE_CHN_ATTR_S));
    initIseChnAttr(&stContext.mIseChnAttr[0], ISE_MODE_TWO_FISHEYE);

    // init ise_camParam
    stContext.mpCamera[0]->getParameters(stContext.mCamChnId[0], cameraParam);
    SIZE_S ise_chn_rect;
    ise_chn_rect.Width = ise_output_width;    // complete same with CamInfo except width&height, for rec
    ise_chn_rect.Height = ise_output_height;
    cameraParam.setVideoSize(ise_chn_rect);

    // init disp area
    RECT_S disp_rect;
    disp_rect.X = 0;
    disp_rect.Y = 0;
    disp_rect.Height = ise_output_width;
    disp_rect.Width = ise_output_height;

    stContext.mIseChnId[0][0] = openIseChannel(stContext.mpIse[0], &stContext.mIseChnAttr[0], &stContext.mIseVoLayer[0], cameraParam, &disp_rect);

    /******************** [step III] *******************/
    vector<CameraChannelInfo> cam_chn_vec;
    CameraChannelInfo cam_chn_info;
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
    cam_chn_info.mpCamera = stContext.mpCamera[0];
    cam_chn_vec.push_back(cam_chn_info);
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[1];
    cam_chn_info.mpCameraRecordingProxy = stContext.mpCamera[1]->getRecordingProxy();
    cam_chn_vec.push_back(cam_chn_info);
    alogd("CamChnInfoVector >>>> cam0:(%p,%d) cam1:(%p,%d)", cam_chn_vec[0].mpCamera, cam_chn_vec[0].mnCameraChannel, cam_chn_vec[1].mpCamera, cam_chn_vec[1].mnCameraChannel);
    startIseDevice(stContext.mpIse[0], cam_chn_vec);
    alogd("---------------------BEGIN ISE_TWO_FISHEYE----------------------");
    sleep(1);
#endif /* ISE_TWO_FISHEYE */


#ifdef ISE_TWO_ISE
    alogd("---------------------PREPARE ISE_TWO_ISE----------------------");

    ise_input_width = cam0_cap_width;
    ise_input_height = cam0_cap_height;
    ise_output_width = ise_input_width * 2;
    ise_output_height = cam0_cap_height;

    /******************** [step I] ********************/
    stContext.mpIse[0] = openIseDevice(ISE_MODE_TWO_ISE, &stContext.mIseListener);

    /******************** [step II] *******************/
    // init ise_attr
    memset(&stContext.mIseChnAttr[0], 0, sizeof(ISE_CHN_ATTR_S));
    initIseChnAttr(&stContext.mIseChnAttr[0], ISE_MODE_TWO_ISE);

    // init ise_camParam
    stContext.mpCamera[0]->getParameters(stContext.mCamChnId[0], cameraParam);
    SIZE_S ise_chn_rect;
    ise_chn_rect.Width = ise_output_width;    // complete same with CamInfo except width&height, for rec
    ise_chn_rect.Height = ise_output_height;
    cameraParam.setVideoSize(ise_chn_rect);
    cameraParam.setPreviewRotation(90);

    // init disp area
    RECT_S disp_rect;
    disp_rect.X = 0;
    disp_rect.Y = 0;
    disp_rect.Height = 1280;
    disp_rect.Width = 720;

    stContext.mIseChnId[0][0] = openIseChannel(stContext.mpIse[0], &stContext.mIseChnAttr[0], &stContext.mIseVoLayer[0], cameraParam, &disp_rect);

    /******************** [step III] *******************/
    vector<CameraChannelInfo> cam_chn_vec;
    CameraChannelInfo cam_chn_info;
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
    cam_chn_info.mpCamera = stContext.mpCamera[0];
    cam_chn_vec.push_back(cam_chn_info);
    cam_chn_info.mnCameraChannel = stContext.mCamChnId[1];
    cam_chn_info.mpCameraRecordingProxy = stContext.mpCamera[1]->getRecordingProxy();
    cam_chn_vec.push_back(cam_chn_info);
    alogd("CamChnInfoVector >>>> cam0:(%p,%d) cam1:(%p,%d)", cam_chn_vec[0].mpCamera, cam_chn_vec[0].mnCameraChannel, cam_chn_vec[1].mpCamera, cam_chn_vec[1].mnCameraChannel);
    startIseDevice(stContext.mpIse[0], cam_chn_vec);
    alogd("---------------------BEGIN ISE_TWO_ISE----------------------");
    sleep(1);
#endif /* ISE_TWO_ISE */


/***************************************for test REC...***************************************************/
#if defined(CAM0) || defined(CAM1)

#if defined(CAM0_REC)
    alogd("---------------------PREPARE CAM0_RECORDING...----------------------");
    stContext.mpRecorder[0] = new EyeseeRecorder();
    stContext.mpRecorder[0]->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnErrorListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setCameraProxy(stContext.mpCamera[0]->getRecordingProxy(), stContext.mCamChnId[0]);
    stContext.mpRecorder[0]->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    char fname0[] = "/mnt/extsd/sample_eyeseeise/cam0_chn2.mp4";
    stContext.mMuxerId[0] = stContext.mpRecorder[0]->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, fname0, 0, false);
    stContext.mpRecorder[0]->setVideoFrameRate(30);
    alogd("setEncodeVideoSize=[%dx%d]", 384, 360);
    stContext.mpRecorder[0]->setVideoSize(1024, 768);
    stContext.mpRecorder[0]->setVideoEncoder(PT_H265);
    stContext.mpRecorder[0]->setVideoEncodingBitRate(8*1024*1024);
    stContext.mpRecorder[0]->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    stContext.mpRecorder[0]->setAudioSamplingRate(8000);
    stContext.mpRecorder[0]->setAudioChannels(1);
    stContext.mpRecorder[0]->setAudioEncodingBitRate(12200);
    stContext.mpRecorder[0]->setAudioEncoder(PT_AAC);

    alogd("prepare()!");
    stContext.mpRecorder[0]->prepare();
    alogd("start()!");
    stContext.mpRecorder[0]->start();
    alogd("---------------------CAM0_RECORDING...----------------------");
    sleep(5);
#endif
#if defined(CAM1_REC)
    alogd("---------------------PREPARE CAM1_RECORDING...----------------------");
    stContext.mpRecorder[1] = new EyeseeRecorder();
    stContext.mpRecorder[1]->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder[1]->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder[1]->setOnErrorListener(&stContext.mRecorderListener);
    stContext.mpRecorder[1]->setCameraProxy(stContext.mpCamera[1]->getRecordingProxy(), stContext.mCamChnId[1]);
    stContext.mpRecorder[1]->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    char fname1[] = "/mnt/extsd/sample_eyeseeise/cam1_chn3.mp4";
    stContext.mMuxerId[1] = stContext.mpRecorder[1]->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, fname1, 0, false);
    stContext.mpRecorder[1]->setVideoFrameRate(30);
    alogd("setEncodeVideoSize=[%dx%d]", 720, 720);
    stContext.mpRecorder[1]->setVideoSize(1024, 768);
    stContext.mpRecorder[1]->setVideoEncoder(PT_H265);
    stContext.mpRecorder[1]->setVideoEncodingBitRate(16*1024*1024);
    stContext.mpRecorder[1]->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    stContext.mpRecorder[1]->setAudioSamplingRate(8000);
    stContext.mpRecorder[1]->setAudioChannels(1);
    stContext.mpRecorder[1]->setAudioEncodingBitRate(12200);
    stContext.mpRecorder[1]->setAudioEncoder(PT_AAC);

    alogd("prepare()!");
    stContext.mpRecorder[1]->prepare();
    alogd("start()!");
    stContext.mpRecorder[1]->start();
    alogd("---------------------CAM1_RECORDING...----------------------");
    sleep(5);
#endif

#if defined(CAM0_REC)
    alogd("---------------------CAM0_STOP_RECORDING...----------------------");
    stContext.mpRecorder[0]->stop();
    delete stContext.mpRecorder[0];
    stContext.mpRecorder[0] = NULL;
#endif
#if defined(CAM1_REC)
    alogd("---------------------CAM1_STOP_RECORDING...----------------------");
    stContext.mpRecorder[1]->stop();
    delete stContext.mpRecorder[1];
    stContext.mpRecorder[1] = NULL;
#endif
    alogd("---------------------CAM_STOP_RECORDING END!----------------------");

#endif


#ifdef ISE_ONE_FISHEYE

#ifdef PTZ0_REC
    stContext.mpRecorder[0] = new EyeseeRecorder();
    stContext.mpRecorder[0]->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnErrorListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setCameraProxy(stContext.mpIse[0]->getRecordingProxy(), stContext.mIseChnId[0][0]);
    stContext.mpRecorder[0]->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    char fname0[] = "/mnt/extsd/sample_eyeseeise/ptz0.mp4";
    stContext.mMuxerId[0] = stContext.mpRecorder[0]->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, fname0, 0, false);
    stContext.mpRecorder[0]->setVideoFrameRate(30);
    alogd("setEncodeVideoSize=[%dx%d]", 384, 360);
    stContext.mpRecorder[0]->setVideoSize(384, 360);
    stContext.mpRecorder[0]->setVideoEncoder(PT_H264);
    stContext.mpRecorder[0]->setVideoEncodingBitRate(8*1024*1024);
    stContext.mpRecorder[0]->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    stContext.mpRecorder[0]->setAudioSamplingRate(8000);
    stContext.mpRecorder[0]->setAudioChannels(1);
    stContext.mpRecorder[0]->setAudioEncodingBitRate(12200);
    stContext.mpRecorder[0]->setAudioEncoder(PT_AAC);

    alogd("prepare()!");
    stContext.mpRecorder[0]->prepare();
    alogd("start()!");
    stContext.mpRecorder[0]->start();
    sleep(10);
#endif

#ifdef PTZ3_REC
    stContext.mpRecorder[1] = new EyeseeRecorder();
    stContext.mpRecorder[1]->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder[1]->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder[1]->setOnErrorListener(&stContext.mRecorderListener);
    stContext.mpRecorder[1]->setISE(stContext.mpIse[3], stContext.mIseChnId[3][2]);
    stContext.mpRecorder[1]->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    char fname1[] = "/mnt/extsd/sample_eyeseeise/ptz3.mp4";
    stContext.mMuxerId[1] = stContext.mpRecorder[0]->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, fname1, 0, false);
    stContext.mpRecorder[1]->setVideoFrameRate(30);
    alogd("setEncodeVideoSize=[%dx%d]", 384, 360);
    stContext.mpRecorder[1]->setVideoSize(384, 360);
    stContext.mpRecorder[1]->setVideoEncoder(PT_H264);
    stContext.mpRecorder[1]->setVideoEncodingBitRate(8*1024*1024);
    stContext.mpRecorder[1]->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    stContext.mpRecorder[1]->setAudioSamplingRate(8000);
    stContext.mpRecorder[1]->setAudioChannels(1);
    stContext.mpRecorder[1]->setAudioEncodingBitRate(12200);
    stContext.mpRecorder[1]->setAudioEncoder(PT_AAC);

    alogd("prepare()!");
    stContext.mpRecorder[0]->prepare();
    alogd("start()!");
    stContext.mpRecorder[0]->start();
    sleep(10);
#endif


#ifdef PTZ0_REC
    stContext.mpRecorder[0]->stop();
    delete stContext.mpRecorder[0];
    stContext.mpRecorder[0] = NULL;
#endif

#ifdef PTZ1_REC
    stContext.mpRecorder[1]->stop();
    delete stContext.mpRecorder[0];
    stContext.mpRecorder[1] = NULL;
#endif

#endif  /* ISE_ONE_FISHEYE */


#if defined(ISE_TWO_FISHEYE) && defined(DFISH_REC)
    stContext.mpRecorder[0] = new EyeseeRecorder();
    stContext.mpRecorder[0]->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnErrorListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setISE(stContext.mpIse[0], stContext.mIseChnId[0][0]);
    stContext.mpRecorder[0]->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    char fname0[] = "/mnt/extsd/sample_eyeseeise_dfish/dfish.mp4";
    stContext.mMuxerId[0] = stContext.mpRecorder[0]->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, fname0, 0, false);
    stContext.mpRecorder[0]->setVideoFrameRate(30);
    alogd("setEncodeVideoSize=[%dx%d]", 1080, 720);
    stContext.mpRecorder[0]->setVideoSize(1080, 720);
    stContext.mpRecorder[0]->setVideoEncoder(PT_H264);
    stContext.mpRecorder[0]->setVideoEncodingBitRate(4*1024*1024);
    stContext.mpRecorder[0]->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    stContext.mpRecorder[0]->setAudioSamplingRate(8000);
    stContext.mpRecorder[0]->setAudioChannels(1);
    stContext.mpRecorder[0]->setAudioEncodingBitRate(12200);
    stContext.mpRecorder[0]->setAudioEncoder(PT_AAC);

    alogd("prepare()!");
    stContext.mpRecorder[0]->prepare();
    alogd("start()!");
    stContext.mpRecorder[0]->start();

    sleep(20);

    stContext.mpRecorder[0]->stop();
    delete stContext.mpRecorder[0];
    stContext.mpRecorder[0] = NULL;
#endif  /* ISE_TWO_FISHEYE */

#if defined(ISE_TWO_ISE) && defined(ISE_REC)
    alogd("---------------------PREPARE ISE_TWO_ISE RECORDER----------------------");
    stContext.mpRecorder[0] = new EyeseeRecorder();
    stContext.mpRecorder[0]->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setOnErrorListener(&stContext.mRecorderListener);
    stContext.mpRecorder[0]->setISE(stContext.mpIse[0], stContext.mIseChnId[0][0]);
    stContext.mpRecorder[0]->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    char fname0[] = "/mnt/extsd/sample_eyeseeise_ise/ise.mp4";
    stContext.mMuxerId[0] = stContext.mpRecorder[0]->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, fname0, 0, false);
    stContext.mpRecorder[0]->setVideoFrameRate(30);
    alogd("setEncodeVideoSize=[%dx%d]", 1080, 720);
    stContext.mpRecorder[0]->setVideoSize(1080, 720);
    stContext.mpRecorder[0]->setVideoEncoder(PT_H264);
    stContext.mpRecorder[0]->setVideoEncodingBitRate(4*1024*1024);
    stContext.mpRecorder[0]->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    stContext.mpRecorder[0]->setAudioSamplingRate(8000);
    stContext.mpRecorder[0]->setAudioChannels(1);
    stContext.mpRecorder[0]->setAudioEncodingBitRate(12200);
    stContext.mpRecorder[0]->setAudioEncoder(PT_AAC);

    alogd("prepare()!");
    stContext.mpRecorder[0]->prepare();
    alogd("start()!");
    stContext.mpRecorder[0]->start();

    sleep(20);

    stContext.mpRecorder[0]->stop();
    delete stContext.mpRecorder[0];
    stContext.mpRecorder[0] = NULL;
    alogd("---------------------BEGIN ISE_TWO_ISE RECORDER----------------------");
#endif  /* ISE_TWO_ISE */

/******************************************************************************************/
    alogd("wait trigger exit...");
    cdx_sem_down(&stContext.mSemExit);
    alogd("trigger exit! record done!!!");
    alogd("---------------------------------------------------------------------");

    alogd("####################################################");
    alogd("############### STOP REC & ISE & CAM ###############");
    alogd("####################################################");

/******************************************************************************************/
#ifdef ISE_ONE_FISHEYE

#ifdef PTZ0
    alogd("-----------------destroy PTZ0--------------------");
    stContext.mpIse[0]->stopDevice();
    stContext.mpIse[0]->closeChannel(stContext.mIseChnId[0][0]);
    stContext.mpIse[0]->releaseDevice();
    EyeseeISE::close(stContext.mpIse[0]);
    stContext.mpIse[0] = NULL;
    ret = AW_MPI_VO_DisableVideoLayer(stContext.mIseVoLayer[0]);
    alogd("release pw. ret:%d, VoLayer:%d", ret, stContext.mIseVoLayer[0]);
    alogd("-----------------after Destroy PTZ0--------------------");
    sleep(1);
#endif

#ifdef PTZ1
    alogd("-----------------destroy PTZ1--------------------");
    stContext.mpIse[1]->stopDevice();
    stContext.mpIse[1]->closeChannel(stContext.mIseChnId[1][0]);
    stContext.mpIse[1]->releaseDevice();
    EyeseeISE::close(stContext.mpIse[1]);
    stContext.mpIse[1] = NULL;
    ret = AW_MPI_VO_DisableVideoLayer(stContext.mIseVoLayer[1]);
    alogd("release pw. ret:%d, VoLayer:%d", ret, stContext.mIseVoLayer[1]);
    alogd("-----------------after Destroy PTZ1--------------------");
    sleep(1);
#endif

#ifdef PTZ2
    alogd("-----------------destroy PTZ2--------------------");
    stContext.mpIse[2]->stopDevice();
    stContext.mpIse[2]->closeChannel(stContext.mIseChnId[2][0]);
    stContext.mpIse[2]->releaseDevice();
    EyeseeISE::close(stContext.mpIse[2]);
    stContext.mpIse[2] = NULL;
    ret = AW_MPI_VO_DisableVideoLayer(stContext.mIseVoLayer[2]);
    alogd("release pw. ret:%d, VoLayer:%d", ret, stContext.mIseVoLayer[2]);
    alogd("-----------------after Destroy PTZ2--------------------");
    sleep(1);
#endif

#ifdef PTZ3
    alogd("-----------------destroy PTZ3--------------------");
    stContext.mpIse[3]->stopDevice();
    stContext.mpIse[3]->closeChannel(stContext.mIseChnId[3][0]);
    stContext.mpIse[3]->closeChannel(stContext.mIseChnId[3][1]);
    stContext.mpIse[3]->closeChannel(stContext.mIseChnId[3][2]);
    stContext.mpIse[3]->closeChannel(stContext.mIseChnId[3][3]);
    stContext.mpIse[3]->releaseDevice();
    EyeseeISE::close(stContext.mpIse[3]);
    stContext.mpIse[3] = NULL;
    AW_MPI_VO_DisableVideoLayer(stContext.mIseVoLayer[3]);
    alogd("-----------------after Destroy PTZ3--------------------");
    sleep(1);
#endif

#endif  /* ISE_ONE_FISHEYE */


#ifdef ISE_TWO_FISHEYE
    alogd("-----------------destroy dfish--------------------");
    stContext.mpIse[0]->stopDevice();
    stContext.mpIse[0]->closeChannel(stContext.mIseChnId[0][0]);
    stContext.mpIse[0]->releaseDevice();
    EyeseeISE::close(stContext.mpIse[0]);
    stContext.mpIse[0] = NULL;
    AW_MPI_VO_DisableVideoLayer(stContext.mIseVoLayer[0]);
    sleep(1);
#endif  /* ISE_TWO_FISHEYE */


#ifdef ISE_TWO_ISE
    alogd("-----------------destroy ise--------------------");
    stContext.mpIse[0]->stopDevice();
    stContext.mpIse[0]->closeChannel(stContext.mIseChnId[0][0]);
    stContext.mpIse[0]->releaseDevice();
    EyeseeISE::close(stContext.mpIse[0]);
    stContext.mpIse[0] = NULL;
    AW_MPI_VO_DisableVideoLayer(stContext.mIseVoLayer[0]);
    sleep(1);
#endif  /* ISE_TWO_ISE */


#ifdef CAM0
    alogd("-----------------destroy cam0--------------------");
    stContext.mpCamera[0]->stopChannel(stContext.mCamChnId[0]);
    stContext.mpCamera[0]->releaseChannel(stContext.mCamChnId[0]);
    stContext.mpCamera[0]->closeChannel(stContext.mCamChnId[0]);
    stContext.mpCamera[0]->stopDevice();
    stContext.mpCamera[0]->releaseDevice();
    EyeseeCamera::close(stContext.mpCamera[0]);
    stContext.mpCamera[0] = NULL;
    AW_MPI_VO_DisableVideoLayer(stContext.mCamVoLayer[0]);
    stContext.mCamVoLayer[0] = -1;
    sleep(1);
#endif

#ifdef CAM1
    alogd("-----------------destroy cam1--------------------");
    stContext.mpCamera[1]->stopChannel(stContext.mCamChnId[1]);
    stContext.mpCamera[1]->releaseChannel(stContext.mCamChnId[1]);
    stContext.mpCamera[1]->closeChannel(stContext.mCamChnId[1]);
    stContext.mpCamera[1]->stopDevice();
    stContext.mpCamera[1]->releaseDevice(); 
    EyeseeCamera::close(stContext.mpCamera[1]);
    stContext.mpCamera[1] = NULL;
    AW_MPI_VO_DisableVideoLayer(stContext.mCamVoLayer[1]);
    stContext.mCamVoLayer[1] = -1;
    sleep(1);
#endif

    // destruct UI layer
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

    //exit mpp system
    AW_MPI_SYS_Exit(); 
    cout<<"bye, Sample_EyeseeIse!"<<endl;
    return result;
}
