//#define LOG_NDEBUG 0
#define LOG_TAG "sample_fisheye"
#include <utils/plat_log.h>
#include <sstream>
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
#include <BITMAP_S.h>
#include "sample_fisheye.h"

using namespace std;
using namespace EyeseeLinux;
#define MOUNT_PATH     "/mnt/extsd"
#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)
SampleEyeseeIseContext *gpSampleContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleContext != NULL)
    {
        cdx_sem_up(&gpSampleContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId==mpContext->mCamDispChnId[0] || chnId==mpContext->mCamDispChnId[1])
            {
                alogd("CamDispChnId(%d) notify app preview!", chnId);
            }
            else
            {
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCamDispChnId[0]);
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
}

void EyeseeCameraListener::addPictureRegion(std::list<PictureRegionType> &rPicRgnList)
{
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

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    aloge("fatal error! Do not test CallbackOut!");
}



EyeseeRecorderListener::EyeseeRecorderListener(SampleEyeseeIseContext *pOwner)
    : mpContext(pOwner)
{

}

EyeseeIseListener::EyeseeIseListener(SampleEyeseeIseContext *pContext)
    :mpContext(pContext)
{
}

void EyeseeIseListener::onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE)
{
    aloge("ise receive error, but not register error handle!!! chnId[%d], error[%d]", chnId, error);
}

bool EyeseeIseListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeISE *pISE)
{
    return true;
}

void EyeseeIseListener::onPictureTaken(int chnId, const void *data, int size, EyeseeISE* pISE)
{
}

void EyeseeIseListener::addPictureRegion(std::list<PictureRegionType> &rPicRgnList)
{
}

SampleEyeseeIseContext::SampleEyeseeIseContext()
    :mCameraListener(this)
    ,mIseListener(this)
    ,mRecorderListener(this)
    ,stop_cam_recorder_flag(false)
    ,stop_ise_recorder_flag(false)
    ,set_ise_recorder_fd_status(1)
    ,set_cam_recorder_fd_status(1)
{

    bzero(&mCmdLineParam, sizeof(mCmdLineParam));
    bzero(&mConfigParam, sizeof(mConfigParam));

    cdx_sem_init(&mSemExit, 0);

    mVoDev = 0;
    mUILayer = HLAY(2, 0);

    bzero(&mSysConf, sizeof(mSysConf));

    bzero(&mCamVoLayer, sizeof(mCamVoLayer));
    bzero(&mCamDispChnId, sizeof(mCamDispChnId));

    bzero(&mIseChnAttr, sizeof(mIseChnAttr));
    bzero(&mIseVoLayer, sizeof(mIseVoLayer));
    bzero(&mIseVoLayerEnable, sizeof(mIseVoLayerEnable));
    bzero(&mIseChnId, sizeof(mIseChnId));
    bzero(&mIseChnEnable, sizeof(mIseChnEnable));

    bzero(&mCameraInfo, sizeof(mCameraInfo));
    bzero(&mpCamera, sizeof(mpCamera));
    bzero(&mpIse, sizeof(mpIse));
    bzero(&mpRecorder, sizeof(mpRecorder));

    bzero(&mMuxerId, sizeof(mMuxerId));

    mCamPicNum = -1;
    mIsePicNum = -1;
}

SampleEyeseeIseContext::~SampleEyeseeIseContext()
{
    cdx_sem_deinit(&mSemExit);
    /*if(mpCamera[0]!=NULL)
    {
        aloge("fatal error! some EyeseeCameras is not destruct!");
    }
    if(mpIse!=NULL)
    {
        aloge("fatal error! some EyeseeIses is not destruct!");
    }
    if(mpRecorder[0]!=NULL || mpRecorder[1]!=NULL || mpRecorder[2]!=NULL || mpRecorder[3]!=NULL)
    {
        aloge("fatal error! some EyeseeRecorders is not destruct!");
    }*/
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
            else
            {
                mCmdLineParam.mConfigFilePath = argv[i];
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
    int ret = SUCCESS;
    const char *pConfPath;
    CONFPARSER_S stConfParser;
    CamDeviceParam *pCamDevParam;
    DispParam      *pDispParam;
    PhotoParam     *pPhotoParam;
    RecParam       *pRecParam;
    IseMoGroupOutputParam     *pIseMoParam;
    int i, j;
    char buf[64] = {0};
    char name[256];
    int GroupNum = 0;
    pConfPath = mCmdLineParam.mConfigFilePath.c_str();
    ret = createConfParser(pConfPath, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail!");
        ret = FAILURE;
        return ret;
    }

    mConfigParam.mCamEnable[0] = 1;
    if (mConfigParam.mCamEnable[0])
    {
        pCamDevParam = &mConfigParam.mCamDevParam[0];
        pCamDevParam->mCamCsiChn        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_CSI_CHN, 0);
        pCamDevParam->mCamId            = pCamDevParam->mCamCsiChn;
        pCamDevParam->mCamIspDev        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_ISP_DEV, 0);
        pCamDevParam->mCamVippNum       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_VIPP_NUM, 0);
        pCamDevParam->mCamCapWidth      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_CAP_WIDTH, 0);
        pCamDevParam->mCamCapHeight     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_CAP_HEIGHT, 0);
        pCamDevParam->mCamFrameRate     = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_FRAME_RATE, 0);
        pCamDevParam->mCamBufNum        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_BUF_NUM, 0);
        pCamDevParam->mVIChn_Preview = 0;
        for(i = 0;i < pCamDevParam->mCamVippNum;i++){
            pCamDevParam->mCamIspScale[i]   = i;
        }

        mConfigParam.mCamDispEnable = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM_DISP_ENABLE, 0);
        if (mConfigParam.mCamDispEnable)
        {

            pDispParam = &mConfigParam.mDispParam[0];
            pDispParam->mDispRectX      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_DISP_RECT_X, 0);
            pDispParam->mDispRectY      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_DISP_RECT_Y, 0);
            pDispParam->mDispRectWidth  = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_DISP_RECT_WIDTH, 0);
            pDispParam->mDispRectHeight = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_DISP_RECT_HEIGHT, 0);
        }

        mConfigParam.mCamRecEnable = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_ENABLE, 0);
        if (mConfigParam.mCamRecEnable)
        {
            pRecParam = &mConfigParam.mCamRecParam[0];
            snprintf(pRecParam->mRecParamFileName,256,"/mnt/extsd/loop_cam_rec0.mp4");
            strncpy(pRecParam->mRecParamVideoEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_V_TYPE, NULL), 64);
            pRecParam->mRecParamVideoSizeWidth       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_V_WIDTH, 0);
            pRecParam->mRecParamVideoSizeHeight      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_V_HEIGHT, 0);
            pRecParam->mRecParamVideoBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_V_BITRATE, 0);
            pRecParam->mRecParamVideoFrameRate       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_V_FRAMERATE, 0);
            strncpy(pRecParam->mRecParamAudioEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_A_TYPE, NULL), 64);
            pRecParam->mRecParamAudioSamplingRate      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_A_SR, 0);
            pRecParam->mRecParamAudioChannels        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_A_CHN, 0);
            pRecParam->mRecParamAudioBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_CAM_REC_A_BR, 0);
        }
    }

    mConfigParam.mIseMoEnable            = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_ENABLE, 0);
    if (mConfigParam.mIseMoEnable)
    {
        //ise param
        mConfigParam.mIseMoIutputParam.mIseMoGroupNum = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_GROUP_NUM, 0);
        mConfigParam.mIseMoIutputParam.mIseMoInWidth           = pCamDevParam->mCamCapWidth;
        mConfigParam.mIseMoIutputParam.mIseMoInHeight          = pCamDevParam->mCamCapHeight;
        mConfigParam.mIseMoIutputParam.mIseMoInLumaPitch       = pCamDevParam->mCamCapWidth;
        mConfigParam.mIseMoIutputParam.mIseMoInChromaPitch     = pCamDevParam->mCamCapWidth;
        mConfigParam.mIseMoIutputParam.mIseMoInYuvType         = 0;
        GroupNum = mConfigParam.mIseMoIutputParam.mIseMoGroupNum;

        for(i = 0;i < GroupNum;i++)
        {
            pIseMoParam = &mConfigParam.mIseMoOutputParam[i];
            snprintf(name, 256, "ise_mo_group%d_dewarp_mode", i);
            pIseMoParam->mIseMoDewarpMode      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_mount_type", i);
            pIseMoParam->mIseMoMountTpye      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_pan", i);
            pIseMoParam->mIseMoPan      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_tilt", i);
            pIseMoParam->mIseMoTilt      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_zoom", i);
            pIseMoParam->mIseMoZoom      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_w", i);
            pIseMoParam->mIseMoOutWidth      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_h", i);
            pIseMoParam->mIseMoOutHeight      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_filp", i);
            pIseMoParam->mIseMoOutFlip      = GetConfParaInt(&stConfParser, name, 0);
            snprintf(name, 256, "ise_mo_group%d_mir", i);
            pIseMoParam->mIseMoOutMirror      = GetConfParaInt(&stConfParser, name, 0);
            pIseMoParam->mIseMoOutYuvType        = 0;
            pIseMoParam->mIseMoP                 = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_P, 0);
            pIseMoParam->mIseMoCx                = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_CX, 0);
            pIseMoParam->mIseMoCy                = GetConfParaDouble(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_CY, 0);
        }

        //disp param
        mConfigParam.mIseMoDispEnable            = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_DISP_ENABLE, 0);
        if (mConfigParam.mIseMoDispEnable)
        {
            for (i=0; i < GroupNum; i++)
            {
                DispParam *pDispParam           = &mConfigParam.mIseMoDispParam[i];
                snprintf(name, 256, "ise_mo_disp_group%d_rect_x", i);
                pDispParam->mDispRectX          = GetConfParaInt(&stConfParser, name, 0);
                snprintf(name, 256, "ise_mo_disp_group%d_rect_y", i);
                pDispParam->mDispRectY          = GetConfParaInt(&stConfParser, name, 0);
                snprintf(name, 256, "ise_mo_disp_group%d_rect_w", i);
                pDispParam->mDispRectWidth      = GetConfParaInt(&stConfParser, name, 0);
                snprintf(name, 256, "ise_mo_disp_group%d_rect_h", i);
                pDispParam->mDispRectHeight     = GetConfParaInt(&stConfParser, name, 0);
                aloge("X %d,Y %d,W %d,H %d",pDispParam->mDispRectX,pDispParam->mDispRectY,
                        pDispParam->mDispRectWidth,pDispParam->mDispRectHeight);
            }
        }
        mConfigParam.mIseMoRecEnable = GetConfParaBoolean(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_ENABLE, 0);
        if (mConfigParam.mIseMoRecEnable)
        {
            for(i=0; i < GroupNum; i++){
               pRecParam = &mConfigParam.mIseMoRecParam[i];
               snprintf(pRecParam->mRecParamFileName,256,"/mnt/extsd/loop_ise_group0_%d.mp4",i);
               strncpy(pRecParam->mRecParamVideoEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_V_TYPE, NULL), 64);
               pRecParam->mRecParamVideoSizeWidth       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_V_WIDTH, 0);
               pRecParam->mRecParamVideoSizeHeight      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_V_HEIGHT, 0);
               pRecParam->mRecParamVideoBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_V_BITRATE, 0);
               pRecParam->mRecParamVideoFrameRate       = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_V_FRAMERATE, 0);
               strncpy(pRecParam->mRecParamAudioEncoder, GetConfParaString(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_A_TYPE, NULL), 64);
               pRecParam->mRecParamAudioSamplingRate      = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_A_SR, 0);
               pRecParam->mRecParamAudioChannels        = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_A_CHN, 0);
               pRecParam->mRecParamAudioBitRate         = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_ISE_MO_REC_A_BR, 0);
            }
        }
    }



    destroyConfParser(&stConfParser);

    return SUCCESS;
}

VO_LAYER requestChannelDisplayLayer(DispParam *pDiapParam)
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
        return -1;
    }

    VO_VIDEO_LAYER_ATTR_S layer_attr;
    AW_MPI_VO_GetVideoLayerAttr(hlay, &layer_attr);
    layer_attr.stDispRect.X = pDiapParam->mDispRectX;
    layer_attr.stDispRect.Y = pDiapParam->mDispRectY;
    layer_attr.stDispRect.Width = pDiapParam->mDispRectWidth;
    layer_attr.stDispRect.Height = pDiapParam->mDispRectHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &layer_attr);
    aloge("X %d,Y %d,W %d,H %d",layer_attr.stDispRect.X,layer_attr.stDispRect.Y,
       layer_attr.stDispRect.Width,layer_attr.stDispRect.Height);

    return hlay;
}

int configCamera(CamDeviceParam *pCamDevParam)
{
    int cameraId;
    CameraInfo cameraInfo;
    VI_CHN viChn;
    ISP_DEV ispDev;
    ISPGeometry mISPGeometry;

    cameraId = pCamDevParam->mCamId;
    cameraInfo.facing = CAMERA_FACING_BACK;
    cameraInfo.orientation = 0;
    cameraInfo.canDisableShutterSound = true;
    cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
    cameraInfo.mMPPGeometry.mCSIChn = pCamDevParam->mCamCsiChn;
    mISPGeometry.mISPDev = pCamDevParam->mCamIspDev;
    viChn = pCamDevParam->mCamIspScale[0];
    mISPGeometry.mScalerOutChns.push_back(viChn);
    viChn = pCamDevParam->mCamIspScale[1];
    mISPGeometry.mScalerOutChns.push_back(viChn);
    viChn = pCamDevParam->mCamIspScale[2];
    mISPGeometry.mScalerOutChns.push_back(viChn);
    viChn = pCamDevParam->mCamIspScale[3];
    mISPGeometry.mScalerOutChns.push_back(viChn);
    cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
    EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);

    return SUCCESS;
}

int startCam(int camId, CamDeviceParam *pCamDevParam, DispParam *pDispParam, SampleEyeseeIseContext *pContext)
{
    alogw("-----------------start Cam%d begin--------------------", camId);

    CameraInfo& cameraInfoRef = pContext->mCameraInfo[camId];
    EyeseeCamera::getCameraInfo(camId, &cameraInfoRef);
    pContext->mpCamera[camId] = EyeseeCamera::open(camId);
    pContext->mpCamera[camId]->setInfoCallback(&pContext->mCameraListener);
    pContext->mpCamera[camId]->prepareDevice();

    pContext->mpCamera[camId]->startDevice();
    int vipp_num = pCamDevParam->mCamVippNum;
    int vi_preview_chn = pContext->mConfigParam.mCamDevParam[camId].mVIChn_Preview;
    aloge("vipp num %d",vipp_num);
    for(int i = 0;i < vipp_num;i++)
    {
        aloge("=======scale enable %d=======",cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[i]);
        int vi_chn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[i];
        if(vi_chn == 0)
            pContext->mpCamera[camId]->openChannel(vi_chn, TRUE); //参考通道
        else
            pContext->mpCamera[camId]->openChannel(vi_chn, FALSE);
        aloge(">>>>> CAM_INFO(camId:%d, chnId:%d, mpCamera:%p)", camId, vi_chn, pContext->mpCamera[camId]);

        CameraParameters cameraParam;
        pContext->mpCamera[camId]->getParameters(vi_chn, cameraParam);
        SIZE_S captureSize;
#if 0
        if(i == 1){
            captureSize.Width = 2048;
            captureSize.Height = 2048;
            aloge("fix vipp w and h");
        } else {
            captureSize.Width = pCamDevParam->mCamCapWidth;
            captureSize.Height = pCamDevParam->mCamCapHeight;
        }
#endif
        captureSize.Width = pCamDevParam->mCamCapWidth;
        captureSize.Height = pCamDevParam->mCamCapHeight;
        cameraParam.setVideoSize(captureSize);
        cameraParam.setPreviewFrameRate(pCamDevParam->mCamFrameRate);
        cameraParam.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
        cameraParam.setVideoBufferNumber(pCamDevParam->mCamBufNum);
        cameraParam.setPreviewRotation(90);
        pContext->mpCamera[camId]->setParameters(vi_chn, cameraParam);
        pContext->mpCamera[camId]->prepareChannel(vi_chn);
        bool show_preview_flag = true;
        if(pContext->mConfigParam.mCamDispEnable && vi_chn == vi_preview_chn){
            int hlay = 0;
            while(hlay < VO_MAX_LAYER_NUM)
            {
               if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay))
               {
                   alogd("---------cam_id(%d)&chn_id(%d) use vo_layer(%d) to display---------", camId, vi_chn, hlay);
                   break;
               }
               hlay++;
            }
            if(hlay >= VO_MAX_LAYER_NUM)
            {
               aloge("fatal error! enable video layer fail!");
            }

            VO_VIDEO_LAYER_ATTR_S vo_layer_attr;
            AW_MPI_VO_GetVideoLayerAttr(hlay, &vo_layer_attr);
            vo_layer_attr.stDispRect.X = pDispParam->mDispRectX;
            vo_layer_attr.stDispRect.Y = pDispParam->mDispRectY;
            vo_layer_attr.stDispRect.Width = pDispParam->mDispRectWidth;
            vo_layer_attr.stDispRect.Height = pDispParam->mDispRectHeight;
            AW_MPI_VO_SetVideoLayerAttr(hlay, &vo_layer_attr);
            pContext->mCamVoLayer[camId] = hlay;
            pContext->mpCamera[camId]->setChannelDisplay(vi_chn, hlay);
            pContext->mpCamera[camId]->startChannel(vi_chn);
            pContext->mpCamera[camId]->startRender(vi_chn);
            aloge("X %d,Y %d,W %d,H %d",vo_layer_attr.stDispRect.X,vo_layer_attr.stDispRect.Y,
                    vo_layer_attr.stDispRect.Width,vo_layer_attr.stDispRect.Height);
        } else {
            pContext->mpCamera[camId]->startChannel(vi_chn);
        }
    }
    alogw("-----------------start Cam%d end--------------------", camId);

    return SUCCESS;
}

int stopCam(int camId, CamDeviceParam *pCamDevParam, DispParam *pDispParam, SampleEyeseeIseContext *pContext)
{
    alogw("-----------------stop Cam%d begin--------------------", camId);

    CameraInfo& cameraInfoRef = pContext->mCameraInfo[camId];
    int vipp_num = pCamDevParam->mCamVippNum;
    int vi_preview_chn = pContext->mConfigParam.mCamDevParam[camId].mVIChn_Preview;
    for(int i = 0;i < vipp_num;i++)
    {
        if (pContext->mConfigParam.mCamDispEnable && i == vi_preview_chn)
        {
            aloge("stop camera vo layer");
            pContext->mpCamera[camId]->stopRender(i);
            pContext->mpCamera[camId]->stopChannel(i);
            pContext->mpCamera[camId]->releaseChannel(i);
            pContext->mpCamera[camId]->closeChannel(i);
            AW_MPI_VO_DisableVideoLayer(pContext->mCamVoLayer[camId]);
            pContext->mCamVoLayer[camId] = -1;
        } else {
            pContext->mpCamera[camId]->stopChannel(i);
            pContext->mpCamera[camId]->releaseChannel(i);
            pContext->mpCamera[camId]->closeChannel(i);
        }
    }
    pContext->mpCamera[camId]->stopDevice();
    pContext->mpCamera[camId]->releaseDevice();
    EyeseeCamera::close(pContext->mpCamera[camId]);
    pContext->mpCamera[camId] = NULL;
    alogw("-----------------stop Cam%d end--------------------", camId);

    return SUCCESS;
}

void copyIseParamFromConfigFile(ISE_CHN_ATTR_S *pIseAttr, SampleEyeseeIseConfig *pIseConfig,int i)
{
    //intput
    pIseAttr->mode_attr.mFish.ise_cfg.in_h = pIseConfig->mIseMoIutputParam.mIseMoInHeight;
    pIseAttr->mode_attr.mFish.ise_cfg.in_w = pIseConfig->mIseMoIutputParam.mIseMoInWidth;
    pIseAttr->mode_attr.mFish.ise_cfg.in_yuv_type = pIseConfig->mIseMoIutputParam.mIseMoInYuvType;
    pIseAttr->mode_attr.mFish.ise_cfg.in_luma_pitch = pIseConfig->mIseMoIutputParam.mIseMoInWidth;
    pIseAttr->mode_attr.mFish.ise_cfg.in_chroma_pitch = pIseConfig->mIseMoIutputParam.mIseMoInWidth;

    //output
    pIseAttr->mode_attr.mFish.ise_cfg.dewarp_mode = (WARP_Type_MO)pIseConfig->mIseMoOutputParam[i].mIseMoDewarpMode;
    pIseAttr->mode_attr.mFish.ise_cfg.p  = pIseConfig->mIseMoOutputParam[i].mIseMoP;
    pIseAttr->mode_attr.mFish.ise_cfg.cx = pIseConfig->mIseMoOutputParam[i].mIseMoCx;
    pIseAttr->mode_attr.mFish.ise_cfg.cy = pIseConfig->mIseMoOutputParam[i].mIseMoCy;
    pIseAttr->mode_attr.mFish.ise_cfg.out_yuv_type = pIseConfig->mIseMoOutputParam[i].mIseMoOutYuvType;
    pIseAttr->mode_attr.mFish.ise_cfg.mount_mode = (MOUNT_Type_MO)pIseConfig->mIseMoOutputParam[i].mIseMoMountTpye;
    pIseAttr->mode_attr.mFish.ise_cfg.pan = pIseConfig->mIseMoOutputParam[i].mIseMoPan;
    pIseAttr->mode_attr.mFish.ise_cfg.tilt = pIseConfig->mIseMoOutputParam[i].mIseMoTilt;
    pIseAttr->mode_attr.mFish.ise_cfg.zoom = pIseConfig->mIseMoOutputParam[i].mIseMoZoom;
    pIseAttr->mode_attr.mFish.ise_cfg.out_en[0] = 1;
    pIseAttr->mode_attr.mFish.ise_cfg.out_w[0] = pIseConfig->mIseMoOutputParam[i].mIseMoOutWidth;
    pIseAttr->mode_attr.mFish.ise_cfg.out_h[0] = pIseConfig->mIseMoOutputParam[i].mIseMoOutHeight;
    pIseAttr->mode_attr.mFish.ise_cfg.out_flip[0] = pIseConfig->mIseMoOutputParam[i].mIseMoOutFlip;
    pIseAttr->mode_attr.mFish.ise_cfg.out_mirror[0] = pIseConfig->mIseMoOutputParam[i].mIseMoOutMirror;
    pIseAttr->mode_attr.mFish.ise_cfg.out_luma_pitch[0] = pIseConfig->mIseMoOutputParam[i].mIseMoOutWidth;
    pIseAttr->mode_attr.mFish.ise_cfg.out_chroma_pitch[0] = pIseConfig->mIseMoOutputParam[i].mIseMoOutWidth;
}

EyeseeISE* openIseDevice(ISE_MODE_E mode, EyeseeIseListener *pListener)
{
    EyeseeISE *pIse = EyeseeISE::open();
    pIse->setErrorCallback(pListener);
    pIse->setInfoCallback(pListener);

    pIse->prepareDevice(mode);

    return pIse;
}


int startIseMo(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;
    SampleEyeseeIseConfig *pConfigParam = &pContext->mConfigParam;
    EyeseeLinux::EyeseeISE *pIseContext[4];
    ISE_CFG_PARA_MO pMoCfg[4];
    int GroupNum = pConfigParam->mIseMoIutputParam.mIseMoGroupNum;
    aloge("GroupNum %d",GroupNum);
    int i = 0;
    int camChn[4];
    alogd("-----------------start Ise-Mo-Undistort begin--------------------");
    for(i = 0;i < GroupNum;i++)
    {
        pContext->mpIse[i] = openIseDevice(ISE_MODE_ONE_FISHEYE, &pContext->mIseListener);
        aloge("ise %p",pContext->mpIse[i]);
    }

    // init iseParam with camParam
    int camId;
    int use_vipp = 0;
    EyeseeLinux::EyeseeCamera *pCamera;

    pCamera = pContext->mpCamera[0];

    CameraParameters iseParam[4];

    CameraInfo& cameraInfoRef = pContext->mCameraInfo[0];
    for(i = 0;i < GroupNum;i++)
    {
        camChn[use_vipp] = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[use_vipp];
        pContext->mpCamera[0]->getParameters(camChn[use_vipp], iseParam[i]);
        aloge("mpIse %p", pContext->mpIse[i]);
        aloge("cam vipp %d",camChn[use_vipp]);
    }
    for(i = 0;i < GroupNum;i++)
    {
        copyIseParamFromConfigFile(&pContext->mIseChnAttr[i], pConfigParam, i);
        pMoCfg[i] = pContext->mIseChnAttr[i].mode_attr.mFish.ise_cfg;
        aloge("mpIse %p", pContext->mpIse[i]);
    }

    int iseChn;
    CameraChannelInfo camChnInfo[4];
    bool test = true;
    for (i=0; i<GroupNum; i++)
    {
        aloge("mpIse %p", pContext->mpIse[i]);
        aloge("",pContext->mIseChnAttr[i].mode_attr.mFish.ise_cfg.pan);
        int chn = pContext->mpIse[i]->openChannel(&pContext->mIseChnAttr[i]);
        if (chn != 0)
        {
            aloge("fatal error! why get real chn(%d) != expected i(%d)? check code!", chn, i);
        }
        SIZE_S ise_area;
        ise_area.Width = pMoCfg[i].out_w[0];    // complete same with CamInfo except width&height&rotation, for rec
        ise_area.Height = pMoCfg[i].out_h[0];
        iseParam[i].setVideoSize(ise_area);
        pContext->mpIse[i]->setParameters(0, iseParam[i]);
        bool show_preview_flag = true;
        aloge("disp enable %d",pContext->mConfigParam.mIseMoDispEnable);
        if(pContext->mConfigParam.mIseMoDispEnable){
            pContext->mIseVoLayer[i] = requestChannelDisplayLayer(&pConfigParam->mIseMoDispParam[i]);
            pContext->mIseVoLayerEnable[i] = TRUE;
            pContext->mpIse[i]->setChannelDisplay(0, pContext->mIseVoLayer[i]);
        }
        pContext->mpIse[i]->startChannel(0);
        camChnInfo[i].mnCameraChannel = camChn[use_vipp];
        camChnInfo[i].mpCameraRecordingProxy = pCamera->getRecordingProxy();
        vector<CameraChannelInfo> camChnVec;
        camChnVec.push_back(camChnInfo[i]);
        pContext->mpIse[i]->setCamera(camChnVec);
        pContext->mpIse[i]->startDevice();
    }
    alogd("-----------------start Ise-Mo-Undistort end--------------------");

    return ret;
}

int stopIseMo(SampleEyeseeIseContext *pContext)
{
    int ret = SUCCESS;
    SampleEyeseeIseConfig *pConfigParam = &pContext->mConfigParam;
    alogd("-----------------stop Ise-Mo begin--------------------");

    int GroupNum = pConfigParam->mIseMoIutputParam.mIseMoGroupNum;
    for (int i=0; i<GroupNum; i++)
    {
        pContext->mpIse[i]->stopDevice();
        pContext->mpIse[i]->closeChannel(0);
        pContext->mpIse[i]->releaseDevice();
        EyeseeISE::close(pContext->mpIse[i]);
        pContext->mpIse[i] = NULL;
    }

    int voLayer;
    for (voLayer = 0; voLayer < GroupNum; voLayer++)
    {
        if (pContext->mConfigParam.mIseMoDispEnable)
        {
            aloge("close vo layer");
            AW_MPI_VO_DisableVideoLayer(pContext->mIseVoLayer[voLayer]);
        }
    }

    alogd("-----------------stop Ise-Mo-Undistort end--------------------");

    return ret;
}

/**************************************REC****************************************/
EyeseeRecorder* prepareAVRecord(RecParam *pRecParam, EyeseeRecorderListener *pListener)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder = new EyeseeRecorder();

    pRecorder->setOnInfoListener(pListener);
    pRecorder->setOnDataListener(pListener);
    pRecorder->setOnErrorListener(pListener);
    pRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    pRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, pRecParam->mRecParamFileName, 0, false);
    pRecorder->setVideoFrameRate(pRecParam->mRecParamVideoFrameRate);
    PAYLOAD_TYPE_E type;
    if (!strcmp(pRecParam->mRecParamVideoEncoder, "h264"))
    {
        type = PT_H264;
    }
    else if (!strcmp(pRecParam->mRecParamVideoEncoder, "h265"))
    {
        type = PT_H265;
    }
    else
    {
        alogw("unsupported video encoder: [%s], use default: [h264]", pRecParam->mRecParamVideoEncoder);
        type = PT_H264;
    }
    pRecorder->setVideoEncoder(type);
    EyeseeRecorder::VEncBitRateControlAttr stRcAttr;
    stRcAttr.mVEncType = type;
    stRcAttr.mRcMode = EyeseeRecorder::VideoRCMode_CBR;
    switch (stRcAttr.mVEncType)
    {
        case PT_H264:
        {
            stRcAttr.mAttrH264Cbr.mBitRate = pRecParam->mRecParamVideoBitRate*1024*1024;
            stRcAttr.mAttrH264Cbr.mMaxQp = 51;
            stRcAttr.mAttrH264Cbr.mMinQp = 10;
            break;
        }
        case PT_H265:
        {
            stRcAttr.mAttrH265Cbr.mBitRate = pRecParam->mRecParamVideoBitRate*1024*1024;
            stRcAttr.mAttrH265Cbr.mMaxQp = 51;
            stRcAttr.mAttrH265Cbr.mMinQp = 10;
            break;
        }
        case PT_MJPEG:
        {
            stRcAttr.mAttrMjpegCbr.mBitRate = pRecParam->mRecParamVideoBitRate*1024*1024;
            break;
        }
        default:
        {
            aloge("unsupport video encode type, check code!");
            break;
        }
    }
    result = pRecorder->setVEncBitRateControlAttr(stRcAttr);
    if (result != NO_ERROR) {
        aloge("setVideoEncodingBitRate Failed(%d)", result);
    }
    pRecorder->setVideoSize(pRecParam->mRecParamVideoSizeWidth, pRecParam->mRecParamVideoSizeHeight);

    int max_duration_ms = 60 * 1000;
    pRecorder->setMaxDuration(max_duration_ms);
    pRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    pRecorder->setAudioSamplingRate(pRecParam->mRecParamAudioSamplingRate);
    pRecorder->setAudioChannels(pRecParam->mRecParamAudioChannels);
    pRecorder->setAudioEncodingBitRate(pRecParam->mRecParamAudioBitRate);
    if (!strcmp(pRecParam->mRecParamVideoEncoder, "aac"))
    {
        type = PT_AAC;
    }
    else
    {
        alogw("unsupported audio encoder: [%s], use default: [aac", pRecParam->mRecParamAudioEncoder);
        type = PT_AAC;
    }
    pRecorder->setAudioEncoder(type);

    return pRecorder;
}

int startCamRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;

    if (pContext->mConfigParam.mCamEnable[0] && pContext->mConfigParam.mCamRecEnable)
    {
        alogd("-------------start cam0 rec begin---------------");
        pRecorder = prepareAVRecord(&pContext->mConfigParam.mCamRecParam[0], &pContext->mRecorderListener);
        pContext->mpRecorder[0] = pRecorder;
        CameraParameters iseParam;
        CameraInfo& cameraInfoRef = pContext->mCameraInfo[0];
        int camRecChn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
        pRecorder->setCameraProxy(pContext->mpCamera[0]->getRecordingProxy(), camRecChn);
        pRecorder->prepare();
        pRecorder->start();
        alogd("-------------start cam0 rec end---------------");
    }

    /*if (pContext->mConfigParam.mCamEnable[1] && pContext->mConfigParam.mCamRecEnable[1])
    {
        alogd("-------------start cam1 rec begin---------------");
        pRecorder = prepareAVRecord(&pContext->mConfigParam.mCamRecParam[1], &pContext->mRecorderListener);
        pContext->mpRecorder[1] = pRecorder;
        CameraParameters iseParam;
        CameraInfo& cameraInfoRef = pContext->mCameraInfo[1];
        int camRecChn = cameraInfoRef.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
        pRecorder->setCameraProxy(pContext->mpCamera[1]->getRecordingProxy(), camRecChn);
        pRecorder->prepare();
        pRecorder->start();
        alogd("-------------start cam1 rec end---------------");
    }*/

    return result;
}

int stopCamRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;

    if (pContext->mConfigParam.mCamEnable[0] && pContext->mConfigParam.mCamRecEnable && pContext->mpRecorder[0])
    {
        alogd("-------------stop cam0 rec begin---------------");
        pContext->mpRecorder[0]->stop();
        pContext->mpRecorder[0] = NULL;
        alogd("-------------stop cam0 rec end---------------");
    }

    /*if (pContext->mConfigParam.mCamEnable[1] && pContext->mConfigParam.mCamRecEnable[1] && pContext->mpRecorder[1])
    {
        alogd("-------------stop cam1 rec begin---------------");
        pContext->mpRecorder[1]->stop();
        pContext->mpRecorder[1] = NULL;
        alogd("-------------stop cam1 rec end---------------");
    }*/

    return result;
}

int startIseRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;
    aloge("start recording");
    SampleEyeseeIseConfig *pConfigParam = &pContext->mConfigParam;
    int GroupNum = pConfigParam->mIseMoIutputParam.mIseMoGroupNum;
    aloge("GroupNum %d",GroupNum);
    for(int i = 0;i< GroupNum;i++)
    {
        if(pContext->mConfigParam.mIseMoRecEnable){
            pRecorder = prepareAVRecord(&pContext->mConfigParam.mIseMoRecParam[i], &pContext->mRecorderListener);
            pContext->mpRecorder[i+1] = pRecorder;
            pRecorder->setCameraProxy(pContext->mpIse[i]->getRecordingProxy(), pContext->mIseChnId[0][0]);
            pRecorder->prepare();
            pRecorder->start();
        }
    }
    return result;
}

int stopIseRec(SampleEyeseeIseContext *pContext)
{
    int result = SUCCESS;
    EyeseeRecorder *pRecorder;
    SampleEyeseeIseConfig *pConfigParam = &pContext->mConfigParam;
    int GroupNum = pConfigParam->mIseMoIutputParam.mIseMoGroupNum;
    aloge("GroupNum %d",GroupNum);
    for(int i = 0;i< GroupNum;i++)
    {
        if (pContext->mConfigParam.mIseMoRecEnable)
        {
            alogd("-------------stop ise-mo-rec1 rec begin---------------");
            pContext->mpRecorder[i+1]->stop();
            pContext->mpRecorder[i+1] = NULL;
            alogd("-------------stop ise-mo-rec1 rec end---------------");
        }

    }
    return result;
}

void EyeseeRecorderListener::onInfo(EyeseeRecorder *pMr, int what, int extra)
{
    int GroupNum = gpSampleContext->mConfigParam.mIseMoIutputParam.mIseMoGroupNum;
    EyeseeRecorder *pRecorder;
    char file_name[256];
    char cam_file_name[256];
    int i = 0;
    static int on_info_count = 1;
    switch(what)
    {
        case EyeseeRecorder::MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD:
        {
            aloge("receive onInfo message: need_set_next_fd, muxer_id[%d]", extra);
            aloge("cam status %d",gpSampleContext->set_cam_recorder_fd_status);
            if(gpSampleContext->set_ise_recorder_fd_status == 1){
                for(i = 0;i< GroupNum;i++)
                {
                    aloge("pMr %p ise_rec %p",pMr,gpSampleContext->mpRecorder[i+1]);
                    if(gpSampleContext->mpRecorder[i+1] == pMr){
                       snprintf(file_name, 256, "/mnt/extsd/loop_ise_group1_%d.mp4", i);
                       aloge("========set fd %s========",file_name);
                       int fd = open(file_name,O_RDWR);
                       ftruncate(fd,0);
                       close(fd);
                       if(gpSampleContext->mConfigParam.mIseMoRecEnable){
                         gpSampleContext->mpRecorder[i+1]->setOutputFileSync(file_name,0,extra);
//                         on_info_count++;
                       }
                       break;
                    }
                }
           }else if(gpSampleContext->set_ise_recorder_fd_status == 2){
               aloge("GroupNum %d",GroupNum);
                for(int i = 0;i< GroupNum;i++)
                {
                    aloge("pMr %p ise_rec %p",pMr,gpSampleContext->mpRecorder[i+1]);
                    if(gpSampleContext->mpRecorder[i+1] == pMr){
                         snprintf(file_name, 256, "/mnt/extsd/loop_ise_group0_%d.mp4", i);
                         aloge("========set fd %s========",file_name);
                         int fd = open(file_name,O_RDWR);
                         ftruncate(fd,0);
                         close(fd);
                         if(gpSampleContext->mConfigParam.mIseMoRecEnable){
                             gpSampleContext->mpRecorder[i+1]->setOutputFileSync(file_name,0,extra);
//                             on_info_count++;
                         }
                         break;
                    }
                 }
           }
           if(gpSampleContext->set_cam_recorder_fd_status == 1){
                if(gpSampleContext->mpRecorder[0] == pMr){
                    snprintf(cam_file_name, 256, "/mnt/extsd/loop_cam_rec%d.mp4", 1);
                    int fd = open(cam_file_name,O_RDWR);
                    ftruncate(fd,0);
                    aloge("cam file name %s",cam_file_name);
                    close(fd);
                    if(gpSampleContext->mConfigParam.mCamRecEnable){
                        gpSampleContext->mpRecorder[0]->setOutputFileSync(cam_file_name,0,extra);
                    }
                }
            }else if(gpSampleContext->set_cam_recorder_fd_status == 2){
                if(gpSampleContext->mpRecorder[0] == pMr){
                    snprintf(cam_file_name, 256, "/mnt/extsd/loop_cam_rec%d.mp4", 0);
                    int fd = open(cam_file_name,O_RDWR);
                    ftruncate(fd,0);
                    close(fd);
                    aloge("cam file name %s",cam_file_name);
                    if(gpSampleContext->mConfigParam.mCamRecEnable){
                        gpSampleContext->mpRecorder[0]->setOutputFileSync(cam_file_name,0,extra);
                    }
                }
            }
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            aloge("receive onInfo message: record_file_done, muxer_id[%d]", extra);
            for(int i = 0;i< GroupNum;i++)
            {
                if(gpSampleContext->mpRecorder[i+1] == pMr){
                    if(gpSampleContext->mConfigParam.mIseMoRecEnable){
                        on_info_count++;
                    }
                }
            }
            aloge("on_info_count %d,status %d",
                    on_info_count,gpSampleContext->set_ise_recorder_fd_status);
            if(on_info_count > 4){
                if(gpSampleContext->set_ise_recorder_fd_status == 1)
                    gpSampleContext->set_ise_recorder_fd_status = 2;
                else if(gpSampleContext->set_ise_recorder_fd_status == 2)
                    gpSampleContext->set_ise_recorder_fd_status = 1;
                on_info_count = 0;
            }
            if(gpSampleContext->mpRecorder[0] == pMr){
                aloge("cam status %d",gpSampleContext->set_cam_recorder_fd_status);
                if(gpSampleContext->set_cam_recorder_fd_status == 1)
                    gpSampleContext->set_cam_recorder_fd_status = 2;
                else if(gpSampleContext->set_cam_recorder_fd_status == 2)
                    gpSampleContext->set_cam_recorder_fd_status = 1;
                aloge("cam status %d",gpSampleContext->set_cam_recorder_fd_status);
            }
            break;
        }
        default:
        {
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
        }
    }
}

bool IsMounted(const char *mount_point, bool check_dev)
{
    const char *path = "/proc/mounts";
    FILE *fp = NULL;
    char line[255] = {0};
    std::string storage_device_;

    if (!(fp = fopen(path, "r"))) {
        aloge("fopen failed, path=%s",path);
        return 0;
    }

    while(fgets(line, sizeof(line), fp)) {
        if (line[0] == '/' && (strstr(line, mount_point) != NULL)) {
            char *saveptr = NULL;
            storage_device_ = strtok_r(line, " ", &saveptr);
            fclose(fp);
            fp = NULL;
            if (check_dev) {
                if (FILE_EXIST(storage_device_.c_str())) return true;
                else return false;
            } else {
                return true;
            }
        } else {
            memset(line,'\0',sizeof(line));
            continue;
        }

    }

    if (fp) {
        fclose(fp);
    }

    return false;
}

void CheckRecorderFile()
{
    stringstream ss;
    char file_name[256];
    int i = 0;
    for(i = 0;i < 2;i++){
        memset(&file_name,0,sizeof(file_name));
        snprintf(file_name, 256, "/mnt/extsd/loop_cam_rec%d.mp4", i);
        if (access((const char*)file_name, F_OK) == 0){
            aloge("==========file exist,remove file==========");
            ss << "rm -rf " << file_name;
            system(ss.str().c_str());
            aloge("cmd %s",ss.str().c_str());
            ss.clear();
        }
    }

    for(i = 0;i < 4;i++){
        memset(&file_name,0,sizeof(file_name));
        snprintf(file_name, 256, "/mnt/extsd/loop_ise_group0_%d.mp4", i);
        if (access((const char*)file_name, F_OK) == 0){
            aloge("==========file exist,remove file==========");
            ss << "rm -rf " << file_name;
            system(ss.str().c_str());
            aloge("cmd %s",ss.str().c_str());
            ss.clear();
        }
    }

    for(i = 0;i < 4;i++){
        memset(&file_name,0,sizeof(file_name));
        snprintf(file_name, 256, "/mnt/extsd/loop_ise_group1_%d.mp4", i);
        if (access((const char*)file_name, F_OK) == 0){
            aloge("==========file exist,remove file==========");
            ss << "rm -rf " << file_name;
            system(ss.str().c_str());
            aloge("cmd %s",ss.str().c_str());
            ss.clear();
        }
    }

}

int main(int argc, char *argv[])
{
    int result = 0;
    alogd("hello, sample_fisheye!");
    SampleEyeseeIseContext stContext;
    gpSampleContext = &stContext;
    //parse command line param
    if(stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        aloge("fatal error! command line param is wrong because of no config file! exit!");
        result = -1;
        return result;
    }
    //parse config file.
    if(stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail! exit!");
        result = -1;
        return result;
    }

    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    alogd("---------------------start EyeseeIse2 sample-------------------------");

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    int camId;
    CameraParameters cameraParam;
    SIZE_S captureSize;
    SIZE_S previewSize;
    VO_LAYER hlay;

    //config vo
    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer. not display, but layer still exist!

    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(stContext.mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_HDMI;
    spPubAttr.enIntfSync = VO_OUTPUT_1080P25;
    AW_MPI_VO_SetPubAttr(stContext.mVoDev, &spPubAttr);

    //config camera
    EyeseeCamera::clearCamerasConfiguration();
    CamDeviceParam *pCamDevParam = NULL;
    camId = 0;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        configCamera(pCamDevParam);
        camId = pCamDevParam->mCamId;
    }
    CheckRecorderFile();
    //start camera
    DispParam *pDispParam;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        pDispParam = &stContext.mConfigParam.mDispParam[camId];
        startCam(camId, pCamDevParam, pDispParam, &stContext);
    }

    //start ise
    if (stContext.mConfigParam.mIseMoEnable)
    {
        startIseMo(&stContext);
    }

    //start cam recorder
    if (stContext.mConfigParam.mCamRecEnable)
    {
        if(IsMounted(MOUNT_PATH,true)) {
            startCamRec(&stContext);
            stContext.stop_cam_recorder_flag = true;
        } else
            aloge("not have sd card, to preview!!!!");
    }

    //start ise recorder
    if (stContext.mConfigParam.mIseMoEnable && stContext.mConfigParam.mIseMoRecEnable)
    {
        if(IsMounted(MOUNT_PATH,true)) {
            startIseRec(&stContext);
            stContext.stop_ise_recorder_flag = true;
        }
        else
            aloge("not have sd card, to preview!!!!");
    }

    alogd("**********************wait user quit...*****************************");
    cdx_sem_down(&stContext.mSemExit);
    alogd("*************************user quit!!!*******************************");


    //stop ise recorder
    if (stContext.mConfigParam.mIseMoRecEnable && stContext.mConfigParam.mIseMoRecEnable)
    {
        if(stContext.stop_ise_recorder_flag)
            stopIseRec(&stContext);
    }

    //stop cam recorder
    if (stContext.mConfigParam.mCamRecEnable)
    {
        if(stContext.stop_cam_recorder_flag)
            stopCamRec(&stContext);
    }

    //stop ise
    if (stContext.mConfigParam.mIseMoEnable)
    {
        stopIseMo(&stContext);
    }

    //stop camera
    camId = 0;
    if (stContext.mConfigParam.mCamEnable[camId])
    {
        pCamDevParam = &stContext.mConfigParam.mCamDevParam[camId];
        pDispParam = &stContext.mConfigParam.mDispParam[camId];
        stopCam(camId, pCamDevParam, pDispParam, &stContext);
    }
    // destruct UI layer
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

    //exit mpp system
    AW_MPI_SYS_Exit(); 

    cout<<"bye, Sample_EyeseeIse2!"<<endl;
    return result;
}
