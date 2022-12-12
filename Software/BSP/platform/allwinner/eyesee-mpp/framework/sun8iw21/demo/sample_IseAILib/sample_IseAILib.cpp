//#define LOG_NDEBUG 0
#define LOG_TAG "sample_IseAILib"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>
#include <sys/time.h>
#include <dlfcn.h>

#include <hwdisplay.h>
//#include <record_writer.h>
#include <confparser.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>
#include <EyeseeISE.h>
#include <SystemBase.h>

#include "sample_IseAILib_config.h"
#include "sample_IseAILib.h"

using namespace std;
using namespace EyeseeLinux;

SampleEyeseeIseAILibContext *pSampleContext = NULL;

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

EyeseeCameraListener::EyeseeCameraListener(SampleEyeseeIseAILibContext *pContext)
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



EyeseeRecorderListener::EyeseeRecorderListener(SampleEyeseeIseAILibContext *pOwner)
    : mpOwner(pOwner)
{
}

EyeseeIseListener::EyeseeIseListener(SampleEyeseeIseAILibContext *pContext)
    :mpContext(pContext)
{
}

void EyeseeIseListener::onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE)
{
    aloge("receive erro. chnId[%d], error[%d]", chnId, error);
}

void EyeseeIseListener::onVLPRData(int chnId, const AW_AI_CVE_VLPR_RULT_S *data, EyeseeLinux::EyeseeISE* pCamera)
{
    if (chnId == mpContext->mIseChnId[0])
    {
        int plate_num = data->s32NumOutput;
        if (plate_num > 0)
        {
            alogd("total recorgnize: [%d]", plate_num);
            for(int i=0; i < plate_num; i++)
            {
                alogd("[%d]:serial=%s", data->astOutput[i].as8PlateNum);
                alogd("\trect.left[%d],rect.top[%d],rect.right[%d],rect.bottom[%d]", data->astOutput[i].stPlateRect.s16Left, \
                    data->astOutput[i].stPlateRect.s16Top, data->astOutput[i].stPlateRect.s16Right, \
                    data->astOutput[i].stPlateRect.s16Bottom);
                //alogd("[%d]:serial=%s", data->astOutput[i].astPlateCharRect);
                alogd("\tconfidence=%d", data->astOutput[i].flConfidence);
            }
        }
    }
}

void EyeseeIseListener::onFaceDetectData(int chnId, const AW_AI_EVE_EVENT_RESULT_S *data, EyeseeLinux::EyeseeISE* pCamera)
{
    if (chnId == mpContext->mIseChnId[0])
    {
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
    }
}

void EyeseeIseListener::onMODData(int chnId, const AW_AI_CVE_DTCA_RESULT_S *data, EyeseeLinux::EyeseeISE *pCamera)
{
    if (chnId == mpContext->mIseChnId[0])
    {
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
    }
}


SampleEyeseeIseAILibContext::SampleEyeseeIseAILibContext()
    :mCameraListener(this)
    ,mIseListener(this)
    ,mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    mUILayer = HLAY(2, 0);
    mpCamera[0] = NULL;
    mpCamera[1] = NULL;
    mpIse = NULL;
    mIseChnId[0] = -1;
    mIseChnId[1] = -1;
    mIseChnId[2] = -1;
    mIseChnId[3] = -1;
    mpRecorder[0] = NULL;
    mpRecorder[1] = NULL;
    mpRecorder[2] = NULL;
    mpRecorder[3] = NULL;
    mCamChnId[0] = -1;
    mCamChnId[1] = -1;
    mIseChnCnt = -1;
    mMuxerId = -1;
    memset(&mConfigPara,0,sizeof(mConfigPara));
}

SampleEyeseeIseAILibContext::~SampleEyeseeIseAILibContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpCamera[0]!=NULL || mpCamera[1]!=NULL)
    {
        if (mpCamera[0])
            aloge("fatal error! EyeseeCamera[0] is not destruct!");
        if (mpCamera[1])
            aloge("fatal error! EyeseeCamera[1] is not destruct!");
    }
    if(mpIse != NULL)
    {
        aloge("fatal error! EyeseeIse is not destruct!");
    }
    if(mpRecorder[0]!=NULL || mpRecorder[1]!=NULL || mpRecorder[2]!=NULL || mpRecorder[3]!=NULL)
    {
        aloge("fatal error! EyeseeRecorder is not destruct!");
    }
}

status_t SampleEyeseeIseAILibContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_IseAILib.conf\n";
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

status_t SampleEyeseeIseAILibContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;

    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mCaptureFrameRate = 25;
        mConfigPara.mEveEnable = 0;
        mConfigPara.mModEnable = 0;
        mConfigPara.mVRPLEnable = 0;
        mConfigPara.mTestDuration = 1000; //s
        return SUCCESS;
    }

    memset(&mConfigPara, 0, sizeof(SampleEyeseeIseConfig));

    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }

    mConfigPara.ise_mode = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_ISE_MODE, 0);
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_CAPTURE_HEIGHT, 0);
    mConfigPara.mCaptureFrameRate = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_FRAME_RATE, 0);

    mConfigPara.mEveEnable = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_ENABLE_EVE, 0); 
    mConfigPara.mModEnable = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_ENABLE_MOD, 0); 
    mConfigPara.mVRPLEnable = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_ENABLE_VRPL, 0);
    mConfigPara.mISERecEnable = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_REC_ENABLE, 0);
    mConfigPara.ise_chn_out_w = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_ISE_OUT_W, 0);
    mConfigPara.ise_chn_out_h = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_ISE_OUT_H, 0);

    mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_EYESEEISE_KEY_TEST_DURATION, 0);

    alogd("ise_mode=%d, cap_w=%d, cap_h=%d, frameRate=%d,eve=%d, mod=%d, vlpr=%d, ise_rec=%d, test_time=%d", \
                mConfigPara.ise_mode, mConfigPara.mCaptureWidth, mConfigPara.mCaptureHeight, \
                mConfigPara.mCaptureFrameRate, mConfigPara.mEveEnable, mConfigPara.mModEnable, \
                mConfigPara.mVRPLEnable, mConfigPara.mISERecEnable, mConfigPara.mTestDuration);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

EyeseeISE* openIseDevice(ISE_MODE_E mode, EyeseeIseListener *pListener)
{
    alogd("############## open IseDev ###############");

    alogd("[ise] step1: open");
    EyeseeISE *pIse = EyeseeISE::open();
    pIse->setErrorCallback(pListener);

    alogd("[ise] step2: prepareDevice");
    pIse->prepareDevice(mode);

    return pIse;
}

ISE_CHN openIseChannel(EyeseeISE *pIse, ISE_CHN_ATTR_S *pIseAttr, CameraParameters &refCamParam, RECT_S *pDispRect)
{
    alogd("############# open IseChn ##############");

    alogd("[ise] step4: openChannel");
    ISE_CHN ise_chn = pIse->openChannel(pIseAttr);
    if (ise_chn < 0) 
    {
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
        pSampleContext->mIseVoLayer[ise_chn] = ise_hlay;

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

/**
  main chn cannot do scareler
**/
int initIseChnAttr(ISE_CHN_ATTR_S *pIseChnAttr, ISE_MODE_E mode, ISE_SIZE in_size, ISE_SIZE out_size)
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

    if (!((in_size.h==out_size.h) && (in_size.w==out_size.w)))
    {
        aloge("size error! main chn cannot do scareler!");
        return -1;
    }

    switch (mode)
    {
    case ISE_MODE_ONE_FISHEYE:
        alogd("############### [IseMode: OneFishEye] ################");
        pOneFishCfgPara = &pIseChnAttr->mode_attr.mFish.ise_cfg;
        pOneFishCfgPara->dewarp_mode = WARP_NORMAL;
        if (pOneFishCfgPara->dewarp_mode == WARP_NORMAL)
        {
            pOneFishCfgPara->mount_mode = MOUNT_WALL;
            pOneFishCfgPara->pan = 90;
            pOneFishCfgPara->tilt = 90;
            pOneFishCfgPara->zoom = 1;
        }
        pOneFishCfgPara->in_h = in_size.h;//720;
        pOneFishCfgPara->in_w = in_size.w;//720;
        pOneFishCfgPara->in_luma_pitch = in_size.w;//720;
        pOneFishCfgPara->in_chroma_pitch = in_size.w;//720;
        pOneFishCfgPara->in_yuv_type = 0;
        pOneFishCfgPara->out_en[0] = 1;
        pOneFishCfgPara->out_h[0] = out_size.h;//360;
        pOneFishCfgPara->out_w[0] = out_size.w;//360;
        pOneFishCfgPara->out_luma_pitch[0] = out_size.w;//384;
        pOneFishCfgPara->out_chroma_pitch[0] = out_size.w;//384;
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
        pTwoFish->ise_cfg.in_h = in_size.h;//320;
        pTwoFish->ise_cfg.in_w = in_size.w;//320;
        pTwoFish->ise_cfg.out_h[0] = out_size.h;//320;
        pTwoFish->ise_cfg.out_w[0] = out_size.w*2;//640;
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
        pTwoFish->ise_cfg.in_luma_pitch = in_size.w;//320;
        pTwoFish->ise_cfg.in_chroma_pitch = in_size.w;//320;
        pTwoFish->ise_cfg.out_luma_pitch[0] = out_size.w*2;
        pTwoFish->ise_cfg.out_chroma_pitch[0] = out_size.w*2;
        break;

    case ISE_MODE_TWO_ISE:
        alogd("############### [IseMode: TwoIse] ################");
        pTwoIse = &pIseChnAttr->mode_attr.mIse;
        // init ISE_CFG_PARA_STI
        pTwoIse->ise_cfg.ncam       = 2;
        pTwoIse->ise_cfg.in_h       = in_size.h;
        pTwoIse->ise_cfg.in_w       = in_size.w;
        pTwoIse->ise_cfg.pano_h     = out_size.h;//1080;
        pTwoIse->ise_cfg.pano_w     = out_size.w*2;//3840;
        pTwoIse->ise_cfg.yuv_type   = 0;
        pTwoIse->ise_cfg.p0         = 44.0f;
        pTwoIse->ise_cfg.p1         = 136.0f;
        pTwoIse->ise_cfg.ov         = 320;

        // 根据所使用原图缩放比例对camera matrix进行缩放
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 3; j++)
            {
                cam_matr[i][j] = cam_matr[i][j] / (1080.0/pTwoIse->ise_cfg.in_h);
                cam_matr_prime[i][j] = cam_matr_prime[i][j] / (1080.0/pTwoIse->ise_cfg.in_h);
            }
        }
        // init ISE_ADV_PARA_STI
        memcpy(&pTwoIse->ise_cfg.calib_matr, cam_matr, sizeof(cam_matr));
        memcpy(&pTwoIse->ise_cfg.calib_matr_cv, cam_matr_prime, sizeof(cam_matr_prime));
        memcpy(&pTwoIse->ise_cfg.distort, dist, sizeof(dist));
        pTwoIse->ise_cfg.stre_coeff         = 1.0;
        pTwoIse->ise_cfg.offset_r2l         = 0;
        pTwoIse->ise_cfg.pano_fov           = 186.0f;
        pTwoIse->ise_cfg.t_angle            = 0.0f;
        pTwoIse->ise_cfg.hfov               = 78.0f;
        pTwoIse->ise_cfg.wfov               = 130.0f;
        pTwoIse->ise_cfg.wfov_rev           = 130.0f;
        pTwoIse->ise_cfg.in_luma_pitch      = pTwoIse->ise_cfg.in_w;
        pTwoIse->ise_cfg.in_chroma_pitch    = pTwoIse->ise_cfg.in_w;
        pTwoIse->ise_cfg.pano_luma_pitch    = pTwoIse->ise_cfg.in_w*2;
        pTwoIse->ise_cfg.pano_chroma_pitch  = pTwoIse->ise_cfg.in_w*2;
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

extern "C" ISE_MODE_E map_Ise_Mode_By_Val(int mode_val)
{
    switch (mode_val)
    {
    case 0:
        return ISE_MODE_ONE_FISHEYE;

    case 1:
        return ISE_MODE_TWO_FISHEYE;

    case 2:
        return ISE_MODE_TWO_ISE;

    default:
        return ISE_MODE_ONE_FISHEYE;
    }
}

status_t SampleEyeseeIseAILibContext::initCVEFuncParam(AW_AI_CVE_DTCA_PARAM &paramDTCA, \
    AW_AI_CVE_CLBR_PARAM &paramCLBR, ISE_SIZE pic_size)
{
/////init CLBR param
    paramCLBR.dim_w = pic_size.w; //640
    paramCLBR.dim_h = pic_size.h; //360;
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
    pFunction->param.perimeter.region.astPoints[1] = {(AW_S16)(mConfigPara.mCaptureWidth-1), 0};
    pFunction->param.perimeter.region.astPoints[2] = {(AW_S16)(mConfigPara.mCaptureWidth-1), (AW_S16)(mConfigPara.mCaptureHeight-1)};
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
    paramDTCA.cParamInout.u32FrameTime = 1*1000 / mConfigPara.mCaptureFrameRate;
    
    return SUCCESS;
}

status_t GetIseChnOutFrameSize(AW_U32 iseMode, ISE_CHN chnId, ISE_CHN_ATTR_S *pAttr, ISE_SIZE &out_size)
{
    if ((chnId < 0) || (NULL == pAttr))
    {
        return BAD_VALUE;
    }

    if (ISEMODE_ONE_FISHEYE == iseMode)
    {
        out_size.w  = pAttr->mode_attr.mFish.ise_cfg.out_luma_pitch[chnId];
        out_size.h = pAttr->mode_attr.mFish.ise_cfg.out_h[chnId];
    }
    else if (ISEMODE_TWO_FISHEYE == iseMode)
    {
        out_size.w  = pAttr->mode_attr.mDFish.ise_cfg.out_luma_pitch[chnId];
        out_size.h = pAttr->mode_attr.mDFish.ise_cfg.out_h[chnId];
    }
    else if (ISEMODE_TWO_ISE == iseMode)
    {
        if (0 == chnId)
        {
            out_size.w = pAttr->mode_attr.mIse.ise_cfg.pano_w;
            out_size.h = pAttr->mode_attr.mIse.ise_cfg.pano_h;
        }
        else
        {
            out_size.w = pAttr->mode_attr.mIse.ise_proccfg.scalar_w[chnId - 1];
            out_size.h = pAttr->mode_attr.mIse.ise_proccfg.scalar_h[chnId - 1];
        }
    }
    return NO_ERROR;
}


/*
  only test ISE main channel
  1: 16:9 or 4:3 input video frame
  2: main channle cannot support scareler, other channel support [1/8, 1] scareler
     when scareler output w must be 8 align, h must be 4 align
  3: the camera parameters 16:9 or 4:3 is different
*/
int main(int argc, char *argv[])
{
    int result = 0;
    int ret;
    cout<<"hello, sample_EyeseeIse!"<<endl;
    SampleEyeseeIseAILibContext stContext;
    ISE_MODE_E ise_mode;

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
    {
        perror("can't catch SIGSEGV");
    }

    alogd("---------------------start ise sample-------------------------");

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    ise_mode = map_Ise_Mode_By_Val(stContext.mConfigPara.ise_mode);

    int cam0_id, cam1_id;
    int cam_chn_cnt;
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
        mISPGeometry.mISPDev = 0;
        viChn = 0;
        mISPGeometry.mScalerOutChns.push_back(viChn);
        viChn = 1;
        mISPGeometry.mScalerOutChns.push_back(viChn);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);

        cameraId = 1;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 1;
        mISPGeometry.mISPDev = 1;
        viChn = 2;
        mISPGeometry.mScalerOutChns.push_back(viChn);
        viChn = 3;
        mISPGeometry.mScalerOutChns.push_back(viChn);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    int cam0_cap_width = 1920;//720;
    int cam0_cap_height = 1080;//720;
    int cam1_cap_width = 1920;//720;
    int cam1_cap_height = 1080;//720;
    int cap_frame_rate = 25;

    cam0_cap_width = cam1_cap_width = stContext.mConfigPara.mCaptureWidth;
    cam0_cap_height = cam1_cap_height = stContext.mConfigPara.mCaptureHeight;
    cap_frame_rate = stContext.mConfigPara.mCaptureFrameRate;

   //open cam0
    {
        alogd("###################################");
        alogd("########### start Cam0! ###########");
        alogd("###################################");

        alogd("---------step1: startDevice---------");
        cam0_id = 0;
        CameraInfo& cameraInfoRef0 = stContext.mCameraInfo[0];
        EyeseeCamera::getCameraInfo(cam0_id, &cameraInfoRef0);
        stContext.mpCamera[0] = EyeseeCamera::open(cam0_id);
        stContext.mpCamera[0]->setInfoCallback(&stContext.mCameraListener);
        stContext.mpCamera[0]->prepareDevice();
        stContext.mpCamera[0]->startDevice();

        alogd("---------step2: open channel---------");
        int cam0_chnX_id = cameraInfoRef0.mMPPGeometry.mISPGeometrys[cam0_id].mScalerOutChns[cam0_id*2];
        stContext.mCamChnId[0] = cam0_chnX_id;
        stContext.mpCamera[0]->openChannel(cam0_chnX_id, TRUE);
        alogd(">>>>> CAM0_INFO(%d, %p)", stContext.mCamChnId[0], stContext.mpCamera[0]);

        alogd("---------step3: prepare channel---------");
        stContext.mpCamera[0]->getParameters(cam0_chnX_id, cameraParam);
        captureSize.Width = cam0_cap_width;
        captureSize.Height = cam0_cap_height;
        cameraParam.setVideoSize(captureSize);
        cameraParam.setPreviewFrameRate(cap_frame_rate);
        cameraParam.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
        cameraParam.setVideoBufferNumber(7);
        cameraParam.setPreviewRotation(0);
        ret = stContext.mpCamera[0]->setParameters(cam0_chnX_id, cameraParam);
        stContext.mpCamera[0]->prepareChannel(cam0_chnX_id);

        alogd("---------step4: start channel---------");
        stContext.mpCamera[0]->startChannel(cam0_chnX_id);
    }

   //open cam1
    {
        if (ise_mode != ISE_MODE_ONE_FISHEYE)
        {
            alogd("###################################");
            alogd("########### start Cam1! ###########");
            alogd("###################################");
    
            alogd("---------step1: startDevice---------");
            cam1_id = 1;
            CameraInfo& cameraInfoRef1 = stContext.mCameraInfo[1];
            EyeseeCamera::getCameraInfo(cam1_id, &cameraInfoRef1);
            stContext.mpCamera[1] = EyeseeCamera::open(cam1_id);
            stContext.mpCamera[1]->setInfoCallback(&stContext.mCameraListener);
            stContext.mpCamera[1]->prepareDevice();
            stContext.mpCamera[1]->startDevice();
    
            alogd("---------step2: open channel---------");
            int cam1_chnX_id = cameraInfoRef1.mMPPGeometry.mISPGeometrys[cam1_id].mScalerOutChns[cam1_id*2];
            stContext.mCamChnId[1] = cam1_chnX_id;
            stContext.mpCamera[1]->openChannel(cam1_chnX_id, TRUE);
            alogd(">>>>> CAM1_INFO(%d, %p)", stContext.mCamChnId[1], stContext.mpCamera[1]);
    
            alogd("---------step3: prepare channel---------");
            stContext.mpCamera[1]->getParameters(cam1_chnX_id, cameraParam);
            captureSize.Width = cam1_cap_width;
            captureSize.Height = cam1_cap_height;
            cameraParam.setVideoSize(captureSize);
            cameraParam.setPreviewFrameRate(cap_frame_rate);
            cameraParam.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
            cameraParam.setVideoBufferNumber(7);
            cameraParam.setPreviewRotation(0);
            ret = stContext.mpCamera[1]->setParameters(cam1_chnX_id, cameraParam);
            stContext.mpCamera[1]->prepareChannel(cam1_chnX_id);
    
            alogd("---------step4: start channel---------");
            stContext.mpCamera[1]->startChannel(cam1_chnX_id);
        }
    }

    int ise_input_width = cam0_cap_width;
    int ise_input_height = cam0_cap_height;
    ISE_SIZE in_size = {ise_input_width, ise_input_height};
    //int ise_output_width = stContext.mConfigPara.ise_chn_out_w;
    //int ise_output_height = stContext.mConfigPara.ise_chn_out_h;    
    //ISE_SIZE out_size = {ise_output_width, ise_output_height};
  //only test main chnn, not use ise_out_size param    
    ISE_SIZE out_size = {in_size.w, in_size.h};
    alogd("###ise input:%dx%d, ise_chn_out:%dx%d", in_size.w, in_size.h, out_size.w, out_size.h);

  //open ise
    {
        alogd("###################################");
        alogd("########### start ISE ###########");
        alogd("###################################");

        alogd("---------step1: open ISE Device---------");
        stContext.mpIse = openIseDevice(ise_mode, &stContext.mIseListener);

        alogd("---------step2: open ISE channel ---------");
        memset(&stContext.mIseChnAttr, 0, sizeof(ISE_CHN_ATTR_S));
        initIseChnAttr(&stContext.mIseChnAttr, ise_mode, in_size, out_size);
        // init ise_camParam
        stContext.mpCamera[0]->getParameters(stContext.mCamChnId[0], cameraParam);

        GetIseChnOutFrameSize(ise_mode, 0, &stContext.mIseChnAttr, out_size);

        SIZE_S ise_chn_rect;
        ise_chn_rect.Width = out_size.w;    // complete same with CamInfo except width&height, for rec
        ise_chn_rect.Height = out_size.h;
        cameraParam.setVideoSize(ise_chn_rect);
        cameraParam.setPreviewRotation(0);
        // init disp area
        RECT_S disp_rect;
        disp_rect.X = 0;
        disp_rect.Y = 0;
        disp_rect.Width = 640;
        disp_rect.Height = 360;
        stContext.mIseChnId[0] = openIseChannel(stContext.mpIse, &stContext.mIseChnAttr, cameraParam, &disp_rect);
        if (0 != stContext.mIseChnId[0])
        {
            aloge("only test ise chn 0,why open other chn???");
        }

        if (stContext.mConfigPara.mVRPLEnable)
        {
            alogd("###start ise vlpr");
            stContext.mpIse->setVLPRDataCallback(&stContext.mIseListener);
            stContext.mpIse->startVLPRDetect(stContext.mIseChnId[0]);
        }
        if (stContext.mConfigPara.mEveEnable)
        {
            alogd("###start ise eve");
            stContext.mpIse->setFaceDetectDataCallback(&stContext.mIseListener);
            stContext.mpIse->startFaceDetect(stContext.mIseChnId[0]);
        }
        if (stContext.mConfigPara.mModEnable)
        {
            alogd("###start ise mod");
            int chnId = stContext.mIseChnId[0];
            AW_AI_CVE_DTCA_PARAM paramDTCA = {0};
            AW_AI_CVE_CLBR_PARAM paramCLBR = {0};
            ISE_SIZE pic_size = {out_size.w, out_size.h};
            stContext.initCVEFuncParam(paramDTCA, paramCLBR, pic_size);
            stContext.mpIse->setMODDataCallback(&stContext.mIseListener);
            stContext.mpIse->setMODParams(chnId, &paramDTCA, &paramCLBR);
            stContext.mpIse->startMODDetect(chnId);
        }

        alogd("---------step3: start ISE Device---------");
        CameraChannelInfo cam_chn_info;
        cam_chn_info.mnCameraChannel = stContext.mCamChnId[0];
        cam_chn_info.mpCameraRecordingProxy = stContext.mpCamera[0]->getRecordingProxy();
        vector<CameraChannelInfo> cam_chn_vec;
        cam_chn_vec.push_back(cam_chn_info);
        if (ise_mode != ISE_MODE_ONE_FISHEYE)
        {
            cam_chn_info.mnCameraChannel = stContext.mCamChnId[1];
            //cam_chn_info.mpCamera = stContext.mpCamera[1];
            cam_chn_vec.push_back(cam_chn_info);
        }
        startIseDevice(stContext.mpIse, cam_chn_vec);
		//sleep(5);
    }

  //ISE rec
    {
        if (stContext.mConfigPara.mISERecEnable)
        {
            alogd("start record ise");
            stContext.mpRecorder[0] = new EyeseeRecorder();
            stContext.mpRecorder[0]->setOnInfoListener(&stContext.mRecorderListener);
            stContext.mpRecorder[0]->setOnDataListener(&stContext.mRecorderListener);
            stContext.mpRecorder[0]->setOnErrorListener(&stContext.mRecorderListener);
//            stContext.mpRecorder[0]->setISE(stContext.mpIse, stContext.mIseChnId[0]);
            stContext.mpRecorder[0]->setCameraProxy(stContext.mpIse->getRecordingProxy(), stContext.mIseChnId[0]);
            stContext.mpRecorder[0]->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
            char fname0[] = "/mnt/extsd/sample_iseailib/rec_0.mp4";
            stContext.mMuxerId[0] = stContext.mpRecorder[0]->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, fname0, 0, false);
            stContext.mpRecorder[0]->setVideoFrameRate(cap_frame_rate);
            alogd("setEncodeVideoSize=[%dx%d]", ISE_CHN_OUT_FRAME_W, ISE_CHN_OUT_FRAME_H);
            stContext.mpRecorder[0]->setVideoSize(ISE_CHN_OUT_FRAME_W, ISE_CHN_OUT_FRAME_H);
            stContext.mpRecorder[0]->setVideoEncoder(PT_H264);
            stContext.mpRecorder[0]->setVideoEncodingBitRate(8*1024*1024);
            stContext.mpRecorder[0]->setAudioSource(EyeseeRecorder::AudioSource::MIC);
            stContext.mpRecorder[0]->setAudioSamplingRate(8000);
            stContext.mpRecorder[0]->setAudioChannels(1);
            stContext.mpRecorder[0]->setAudioEncodingBitRate(12200);
            stContext.mpRecorder[0]->setAudioEncoder(PT_AAC);
            stContext.mpRecorder[0]->prepare();
            alogd("start()!");
            stContext.mpRecorder[0]->start();
        }
    }

    alogd("wait over...");
    if(stContext.mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

    alogd("####################################################");
    alogd("############### STOP REC & ISE & CAM ###############");
    alogd("####################################################");

    {
        if (stContext.mConfigPara.mISERecEnable)
        {
            if (stContext.mpRecorder[0] != NULL)
            {
                alogd("-----------------destroy REC--------------------");
                stContext.mpRecorder[0]->stop();
                delete stContext.mpRecorder[0];
                stContext.mpRecorder[0] = NULL;
            }
        }
    }

    {
        alogd("-----------------destroy ISE--------------------");
        if (stContext.mConfigPara.mModEnable)
        {
            alogd("stop ise mod");
            stContext.mpIse->stopMODDetect(stContext.mIseChnId[0]);
        }
        if (stContext.mConfigPara.mEveEnable)
        {
            alogd("stop ise eve");
            stContext.mpIse->stopFaceDetect(stContext.mIseChnId[0]);
        }
        if (stContext.mConfigPara.mVRPLEnable)
        {
            stContext.mpIse->stopVLPRDetect(stContext.mIseChnId[0]);
        }

        stContext.mpIse->stopDevice();
        stContext.mpIse->closeChannel(stContext.mIseChnId[0]);
        stContext.mpIse->releaseDevice();
        EyeseeISE::close(stContext.mpIse);
        stContext.mpIse = NULL;
        AW_MPI_VO_DisableVideoLayer(stContext.mIseVoLayer[0]);
    }

    {
        alogd("-----------------destroy cam0--------------------");
        stContext.mpCamera[0]->stopChannel(stContext.mCamChnId[0]);
        stContext.mpCamera[0]->releaseChannel(stContext.mCamChnId[0]);
        stContext.mpCamera[0]->closeChannel(stContext.mCamChnId[0]);
        stContext.mpCamera[0]->stopDevice();
        stContext.mpCamera[0]->releaseDevice();
        EyeseeCamera::close(stContext.mpCamera[0]);
        stContext.mpCamera[0] = NULL;
    }

    {
        if (ise_mode != ISE_MODE_ONE_FISHEYE)
        {
            alogd("-----------------destroy cam1--------------------");
            stContext.mpCamera[1]->stopChannel(stContext.mCamChnId[1]);
            stContext.mpCamera[1]->releaseChannel(stContext.mCamChnId[1]);
            stContext.mpCamera[1]->closeChannel(stContext.mCamChnId[1]);
            stContext.mpCamera[1]->stopDevice();
            stContext.mpCamera[1]->releaseDevice(); 
            EyeseeCamera::close(stContext.mpCamera[1]);
            stContext.mpCamera[1] = NULL;
        }
    }

    // destruct UI layer
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

    //exit mpp system
    AW_MPI_SYS_Exit(); 

    cout<<"bye, Sample_EyeseeIseAILib!"<<endl;

    return result;
}



#if 0
static ERRORTYPE configSubChnAttr(SampleIseConfig *pConfig)
{
    if (ISE_MODE_MO_FISH == pConfig->mIseMode)
    {
        FISH *pFishAttr = &pConfig->subChnAttr.mode_attr.mFish;
        {//main
            pFishAttr->ise_cfg.dewarp_mode = WARP_NORMAL;
            pFishAttr->ise_cfg.out_en[0] = 1;       
            pFishAttr->ise_cfg.in_h = 320;
            pFishAttr->ise_cfg.in_w = 320;
            pFishAttr->ise_cfg.out_h[0] = 160;
            pFishAttr->ise_cfg.out_w[0] = 160;
            pFishAttr->ise_cfg.p  = 101.862167f;
            pFishAttr->ise_cfg.cx = 160;
            pFishAttr->ise_cfg.cy = 160;
            pFishAttr->ise_cfg.in_yuv_type = 0; // YUV420
            pFishAttr->ise_cfg.out_flip[0]   = 0;
            pFishAttr->ise_cfg.out_mirror[0] = 0;
            pFishAttr->ise_cfg.out_yuv_type = 0; // YUV420
            pFishAttr->ise_cfg.in_luma_pitch = 320;
            pFishAttr->ise_cfg.in_chroma_pitch = 320;
            pFishAttr->ise_cfg.out_luma_pitch[0] = 160;
            pFishAttr->ise_cfg.out_chroma_pitch[0] = 160;
            if (WARP_NORMAL == pFishAttr->ise_cfg.dewarp_mode)
            {
                pFishAttr->ise_cfg.mount_mode = MOUNT_WALL;
                pFishAttr->ise_cfg.pan = 135;
                pFishAttr->ise_cfg.tilt = 135;
                pFishAttr->ise_cfg.zoom = 1;
            }
        }
        {//chn1
            pFishAttr->ise_cfg.out_en[1] = 1;
            pFishAttr->ise_cfg.out_h[1] = 80;
            pFishAttr->ise_cfg.out_w[1] = 80;
            pFishAttr->ise_cfg.out_flip[1] = 0;
            pFishAttr->ise_cfg.out_mirror[1] = 0;
            pFishAttr->ise_cfg.out_luma_pitch[1] = 80;
            pFishAttr->ise_cfg.out_chroma_pitch[1] = 80;
        }
        {//chn2
            pFishAttr->ise_cfg.out_en[2] = 1;
            pFishAttr->ise_cfg.out_h[2] = 80;
            pFishAttr->ise_cfg.out_w[2] = 80;
            pFishAttr->ise_cfg.out_flip[2] = 0;
            pFishAttr->ise_cfg.out_mirror[2] = 0;
            pFishAttr->ise_cfg.out_luma_pitch[2] = 80;
            pFishAttr->ise_cfg.out_chroma_pitch[2] = 80;
        }
        {//chn3
            pFishAttr->ise_cfg.out_en[3] = 1;
            pFishAttr->ise_cfg.out_h[3] = 80;
            pFishAttr->ise_cfg.out_w[3] = 80;
            pFishAttr->ise_cfg.out_flip[3] = 0;
            pFishAttr->ise_cfg.out_mirror[3] = 0;
            pFishAttr->ise_cfg.out_luma_pitch[3] = 80;
            pFishAttr->ise_cfg.out_chroma_pitch[3] = 80;
        }
    }
    else if (ISE_MODE_D_FISH == pConfig->mIseMode)
    {
        DFISH *pDFishAttr = &pConfig->subChnAttr.mode_attr.mDFish;
        {//main
            //pDFishAttr = &pConfig->subChnAttr[0].mode_attr.mFish;
            pDFishAttr->ise_cfg.out_en[0] = 1;
            pDFishAttr->ise_cfg.in_h = 320;    
            pDFishAttr->ise_cfg.in_w = 320;
            pDFishAttr->ise_cfg.out_h[0] = 320;
            pDFishAttr->ise_cfg.out_w[0] = 640;
            pDFishAttr->ise_cfg.p0  = 101.862167f;
            pDFishAttr->ise_cfg.cx0 = 160;   
            pDFishAttr->ise_cfg.cy0 = 160;
            pDFishAttr->ise_cfg.p1  = 101.862167f;
            pDFishAttr->ise_cfg.cx1 = 160;
            pDFishAttr->ise_cfg.cy1 = 160;
            pDFishAttr->ise_cfg.in_yuv_type = 0; // YUV420
            pDFishAttr->ise_cfg.out_flip[0]   = 0;
            pDFishAttr->ise_cfg.out_mirror[0] = 0;  
            pDFishAttr->ise_cfg.out_yuv_type = 0; // YUV420
            pDFishAttr->ise_cfg.in_luma_pitch = 320;          
            pDFishAttr->ise_cfg.in_chroma_pitch = 320;         
            pDFishAttr->ise_cfg.out_luma_pitch[0] = 640;          
            pDFishAttr->ise_cfg.out_chroma_pitch[0] = 640;
        }
        {//chn1
            //pDFishAttr = &pConfig->subChnAttr[1].mode_attr.mFish;
            pDFishAttr->ise_cfg.out_en[1] = 1;
            pDFishAttr->ise_cfg.out_h[1] = 160;
            pDFishAttr->ise_cfg.out_w[1] = 320;        
            pDFishAttr->ise_cfg.out_flip[1] = 0;
            pDFishAttr->ise_cfg.out_mirror[1] = 0;
            pDFishAttr->ise_cfg.out_luma_pitch[1] = 320;
            pDFishAttr->ise_cfg.out_chroma_pitch[1] = 320;
        }
        {//chn2            
            pDFishAttr->ise_cfg.out_en[2] = 1;
            pDFishAttr->ise_cfg.out_h[2] = 80;
            pDFishAttr->ise_cfg.out_w[2] = 160;
            pDFishAttr->ise_cfg.out_flip[2] = 0;
            pDFishAttr->ise_cfg.out_mirror[2] = 0;
            pDFishAttr->ise_cfg.out_luma_pitch[2] = 160;
            pDFishAttr->ise_cfg.out_chroma_pitch[2] = 160;
        }
        {//chn3
            pDFishAttr->ise_cfg.out_en[3] = 1;
            pDFishAttr->ise_cfg.out_h[3] = 40;
            pDFishAttr->ise_cfg.out_w[3] = 80;
            pDFishAttr->ise_cfg.out_flip[3] = 0;
            pDFishAttr->ise_cfg.out_mirror[3] = 0;
            pDFishAttr->ise_cfg.out_luma_pitch[3] = 80;
            pDFishAttr->ise_cfg.out_chroma_pitch[3] = 80;
        }
    } 
    else if (ISE_MODE_D_FISH_ISE == pConfig->mIseMode)
    {
        // mindvision 2.2mm镜头参数
        double cam_matr[3][3] = { {6.4083926311003916e+002, 0.0, 6.4046604117646211e+002},
                                  {0.0, 6.4083926311003916e+002, 4.8024084852398539e+002},
                                  {0.0, 0.0, 1.0} };
        double dist[8] = { -1.1238957055436295e-001, -2.8526234113446611e-002,
                           -1.5744176875224516e-003, 4.3263933494422564e-004, 
                           -1.8503663972417678e-002, 1.9027042284894091e-001, 
                           -9.3597920343908084e-002, -3.2434503034753627e-002  };
        double cam_matr_prime[3][3] = { {3.7479101562500000e+002, 0.0, 6.40e+002},
                                        {0.0, 3.6949392700195312e+002, 4.80e+002},
                                        {0.0, 0., 1.} };
        int i, j;
        for(i = 0; i < 2; i++) 
        {        
            for(j = 0; j < 3; j++) 
            {            
                //cam_matr[i][j] = cam_matr[i][j] / TEST_ORG_SCALE;
                //cam_matr_prime[i][j] = cam_matr_prime[i][j] / TEST_ORG_SCALE;
                cam_matr[i][j] = cam_matr[i][j] / 4;
                cam_matr_prime[i][j] = cam_matr_prime[i][j] / 4;
            }
        }

        ISE *pISEAttr = &pConfig->subChnAttr.mode_attr.mIse;
        {//main            
            pISEAttr->ise_cfg.ncam = 2;
            pISEAttr->ise_cfg.in_h = 240;
            pISEAttr->ise_cfg.in_w = 320;
            pISEAttr->ise_cfg.pano_h = 240;
            pISEAttr->ise_cfg.pano_w = 640;
            pISEAttr->ise_cfg.p0  = 51.5f;
            pISEAttr->ise_cfg.p1  = 128.5f;         
            pISEAttr->ise_cfg.yuv_type = 0; //YUV420
            
            memcpy(pISEAttr->ise_adv.calib_matr, cam_matr, sizeof(cam_matr));
            memcpy(pISEAttr->ise_adv.calib_matr_cv, cam_matr_prime, sizeof(cam_matr_prime));
            memcpy(pISEAttr->ise_adv.distort, dist, sizeof(dist));
            pISEAttr->ise_adv.stre_coeff = 1.0;
            pISEAttr->ise_adv.offset_r2l = 0;
            pISEAttr->ise_adv.pano_fov = 186.0f;
            pISEAttr->ise_adv.t_angle = 0.0f;
            pISEAttr->ise_adv.hfov = 78.0f;
            pISEAttr->ise_adv.wfov = 124.0f;
            pISEAttr->ise_adv.wfov_rev = 124.0f;
            pISEAttr->ise_adv.in_luma_pitch = 320;
            pISEAttr->ise_adv.in_chroma_pitch = 320;
            pISEAttr->ise_adv.pano_luma_pitch = 640;
            pISEAttr->ise_adv.pano_chroma_pitch = 640;
            pISEAttr->ise_proccfg.pano_flip = 0;
            pISEAttr->ise_proccfg.pano_mirr = 0;
        }
        {//chn1
            pISEAttr->ise_proccfg.scalar_en[0] = 1;
            pISEAttr->ise_proccfg.scalar_h[0] = 144;
            pISEAttr->ise_proccfg.scalar_w[0] = 480;
            pISEAttr->ise_proccfg.scalar_flip[0] = 0;
            pISEAttr->ise_proccfg.scalar_mirr[0] = 0;
            pISEAttr->ise_proccfg.scalar_luma_pitch[0] = 480;
            pISEAttr->ise_proccfg.scalar_chroma_pitch[0] = 480;
        }
        {//chn2
            pISEAttr->ise_proccfg.scalar_en[1] = 1;
            pISEAttr->ise_proccfg.scalar_h[1] = 120;
            pISEAttr->ise_proccfg.scalar_w[1] = 320;
            pISEAttr->ise_proccfg.scalar_flip[1] = 0;
            pISEAttr->ise_proccfg.scalar_mirr[1] = 0;
            pISEAttr->ise_proccfg.scalar_luma_pitch[1] = 320;
            pISEAttr->ise_proccfg.scalar_chroma_pitch[1] = 320;
        }
        {//chn3
            pISEAttr->ise_proccfg.scalar_en[2] = 1;
            pISEAttr->ise_proccfg.scalar_h[2] = 64;
            pISEAttr->ise_proccfg.scalar_w[2] = 160;
            pISEAttr->ise_proccfg.scalar_flip[2] = 0;
            pISEAttr->ise_proccfg.scalar_mirr[2] = 0;
            pISEAttr->ise_proccfg.scalar_luma_pitch[2] = 160;
            pISEAttr->ise_proccfg.scalar_chroma_pitch[2] = 160;
        }
    }

    return SUCCESS;
}
#endif

