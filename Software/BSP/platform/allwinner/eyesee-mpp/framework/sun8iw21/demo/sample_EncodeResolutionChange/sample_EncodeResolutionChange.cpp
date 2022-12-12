//#define LOG_NDEBUG 0
#define LOG_TAG "sample_EncodeResolutionChange"
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
#include <VencParameters.h>

#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>
#include <SystemBase.h>

#include "sample_EncodeResolutionChange_config.h"
#include "sample_EncodeResolutionChange.h"

using namespace std;
using namespace EyeseeLinux;

SampleEncodeResolutionChangeContext *pSampleEncodeResolutionChangeContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleEncodeResolutionChangeContext!=NULL)
    {
        cdx_sem_up(&pSampleEncodeResolutionChangeContext->mSemExit);
    }
}

static MEDIA_FILE_FORMAT_E deduceFileFormat(std::string strFileName)
{
    MEDIA_FILE_FORMAT_E eFileFormat = MEDIA_FILE_FORMAT_DEFAULT;
    std::string::size_type n = strFileName.rfind('.');
    if((n != std::string::npos) && (n+1 < strFileName.size()))
    {
        std::string strSuffix = strFileName.substr(n+1);
        if(strSuffix == "mp4")
        {
            eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
        else if(strSuffix == "ts")
        {
            eFileFormat = MEDIA_FILE_FORMAT_TS;
        }
    }
    return eFileFormat;
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
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
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

EyeseeCameraListener::EyeseeCameraListener(SampleEncodeResolutionChangeContext *pContext)
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

EyeseeRecorderListener::EyeseeRecorderListener(SampleEncodeResolutionChangeContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleEncodeResolutionChangeContext::SampleEncodeResolutionChangeContext()
    :mCameraListener(this)
    ,mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera = NULL;
    mpRecorder = NULL;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mVencId = -1;
}

SampleEncodeResolutionChangeContext::~SampleEncodeResolutionChangeContext()
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
}

status_t SampleEncodeResolutionChangeContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_EncodeResolutionChange.conf\n";
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
status_t SampleEncodeResolutionChangeContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mViClock = 0;
        mConfigPara.mVeClock = 0;
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mPreviewWidth = 640;
        mConfigPara.mPreviewHeight = 360;
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mDigitalZoom = 0;
        mConfigPara.mDispWidth = 360;
        mConfigPara.mDispHeight = 640;
        mConfigPara.mbPreviewEnable = true;
        mConfigPara.mFrameRate = 60;
        mConfigPara.mDstFrameRate = 30;

        mConfigPara.mEncodeType = PT_H264;
        mConfigPara.mH264_profile = 1;   // 0:BL 1:MP 2:HP
        mConfigPara.mH264Level = H264_LEVEL_51;
        mConfigPara.mH265_profile = 0;  //0:MP
        mConfigPara.mH265Level = H265_LEVEL_62;
        mConfigPara.mEncodeWidth0 = 1920;
        mConfigPara.mEncodeHeight0 = 1080;
        mConfigPara.mEncodeWidth1 = 1280;
        mConfigPara.mEncodeHeight1 = 720;
        mConfigPara.mIDRFrameInterval = mConfigPara.mDstFrameRate;
        mConfigPara.mbVideoEncodingPIntra = true;

        //mConfigPara.mVideoRCMode = EyeseeLinux::EyeseeRecorder::VideoRCMode_CBR;
        mConfigPara.mVideoRCMode = EyeseeLinux::VencParameters::VideoRCMode_CBR;
        mConfigPara.mQp1 = 1;
        mConfigPara.mQp2 = 51;
        mConfigPara.mEncodeBitrate0 = 10*1024*1024;
        mConfigPara.mEncodeBitrate1 = 8*1024*1024;

        mConfigPara.mGopMode = VENC_GOPMODE_NORMALP;
        mConfigPara.mSPInterval = 0;
        mConfigPara.mVirtualIFrameInterval = 0;


        mConfigPara.mbEnableSmartP = false;
        mConfigPara.mIntraRefreshBlockNum = 0;
        mConfigPara.mbAdvanceRef = false;
        mConfigPara.mbHorizonfilp = false;
        mConfigPara.mbAdaptiveintrainp = false;
        mConfigPara.mb3dnr = 0;
        mConfigPara.mbcolor2grey = false;
        mConfigPara.mbNullSkipEnable = false;
        mConfigPara.mTimeLapseFps = 0;
        mConfigPara.mSuperFrmMode = SUPERFRM_NONE;
        mConfigPara.mSuperIFrmBitsThr = 0;
        mConfigPara.mSuperPFrmBitsThr = 0;

        mConfigPara.mAudioEncodeType = PT_AAC;
        mConfigPara.mAudioSampleRate = 8000;
        mConfigPara.mAudioChannelNum = 1;
        mConfigPara.mAudioEncodeBitrate = 20480;

        mConfigPara.mEncodeDuration = 20;
        mConfigPara.mEncodeFolderPath = "/mnt/extsd/sample_EncodeResolutionChange_Files";

        mConfigPara.mFilePath0 = "file0.mp4";
        mConfigPara.mFilePath1 = ""; //file1.mp4";
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_CAPTURE_HEIGHT, 0);
    mConfigPara.mViClock = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_VI_CLOCK, 0);
    mConfigPara.mVeClock = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_VE_CLOCK, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_CAPTURE_FORMAT, NULL);
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
    else if(!strcmp(pStrPixelFormat, "lbc20x"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc25x"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc10x"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else if(!strcmp(pStrPixelFormat, "nv61"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    }
    else if(!strcmp(pStrPixelFormat, "nv16"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mPreviewWidth = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_PREVIEW_WIDTH, 0);
    mConfigPara.mPreviewHeight = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_PREVIEW_HEIGHT, 0);
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_PREVIEW_FORMAT, NULL);
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
    else if(!strcmp(pStrPixelFormat, "lbc20x"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc25x"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc10x"))
    {
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else if(!strcmp(pStrPixelFormat, "nv61"))
    {
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    }
    else if(!strcmp(pStrPixelFormat, "nv16"))
    {
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mDigitalZoom = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_DIGITAL_ZOOM, 0);
    mConfigPara.mDispWidth = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_DISP_WIDTH, 0);
    mConfigPara.mDispHeight = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_DISP_HEIGHT, 0);
    mConfigPara.mbPreviewEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_PREVIEW_ENABLE, 0);
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_FRAME_RATE, 0);

   // char *pStrLongTermRef = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_LONG_TERM_REF, NULL);
   // if(!strcmp(pStrLongTermRef, "yes"))
   // {
   //     mConfigPara.mbLongTermRef = true;
   // }
   // else
   // {
   //     mConfigPara.mbLongTermRef = false;
   // }
    mConfigPara.mDstFrameRate = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_DST_FRAME_RATE, 0);
    if(mConfigPara.mDstFrameRate <= 0)
    {
        mConfigPara.mDstFrameRate = mConfigPara.mFrameRate;
    }

    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_TYPE, NULL);
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
    mConfigPara.mH264_profile = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_H264_PROFILE, 1);
    if(mConfigPara.mH264_profile < 0 || mConfigPara.mH264_profile  > 2)
    {
        aloge("h264_profile had over range [0, 1, 2], had set to be 1");
        mConfigPara.mH264_profile = 1;
    }
    char *pStrH264Level = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_H264_LEVEL, NULL);
    if(!strcmp(pStrH264Level, "5.1"))
    {
        mConfigPara.mH264Level = H264_LEVEL_51;
    }
    else if(!strcmp(pStrH264Level, "4.1"))
    {
        mConfigPara.mH264Level = H264_LEVEL_41;
    }
    else if(!strcmp(pStrH264Level, "3.1"))
    {
        mConfigPara.mH264Level = H264_LEVEL_31;
    }
    else
    {
        aloge("fatal error! unsupport h264 level: [%s]?", pStrH264Level);
        mConfigPara.mH264Level = H264_LEVEL_51;
    }
    mConfigPara.mH265_profile = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_H265_PROFILE, 0);
    if(mConfigPara.mH265_profile < 0 || mConfigPara.mH265_profile  > 1)
    {
        aloge("h265_profile had over range [0, 1, 2], had set to be 0(MP)");
        mConfigPara.mH265_profile = 0;
    }
    char *pStrH265Level = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_H265_LEVEL, NULL);
    if(!strcmp(pStrH265Level, "6.2"))
    {
        mConfigPara.mH265Level = H265_LEVEL_62;
    }
    else if(!strcmp(pStrH265Level, "5.2"))
    {
        mConfigPara.mH265Level = H265_LEVEL_52;
    }
    else
    {
        aloge("fatal error! unsupport h265 level: [%s]?", pStrH265Level);
        mConfigPara.mH265Level = H265_LEVEL_62;
    }
    mConfigPara.mEncodeWidth0 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_WIDTH0, 0);
    mConfigPara.mEncodeHeight0 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_HEIGHT0, 0);
    mConfigPara.mEncodeWidth1 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_WIDTH1, 0);
    mConfigPara.mEncodeHeight1 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_HEIGHT1, 0);
    mConfigPara.mIDRFrameInterval = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_IDRFRAME_INTERVAL, 0);
    mConfigPara.mbVideoEncodingPIntra = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_VIDEOENCODINGPINTRA, 0);

    char *pStrVideoRCMode = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_RC_MODE, NULL);
    if(pStrVideoRCMode!=NULL)
    {
        if(!strcmp(pStrVideoRCMode, "cbr"))
        {
            //mConfigPara.mVideoRCMode = EyeseeLinux::EyeseeRecorder::VideoRCMode_CBR;
            mConfigPara.mVideoRCMode = EyeseeLinux::VencParameters::VideoRCMode_CBR;
        }
        else if(!strcmp(pStrVideoRCMode, "fixqp"))
        {
            //mConfigPara.mVideoRCMode = EyeseeLinux::EyeseeRecorder::VideoRCMode_FIXQP;
            mConfigPara.mVideoRCMode = EyeseeLinux::VencParameters::VideoRCMode_FIXQP;
        }
        else if(!strcmp(pStrVideoRCMode, "abr"))
        {
            //mConfigPara.mVideoRCMode = EyeseeLinux::EyeseeRecorder::VideoRCMode_ABR;
            mConfigPara.mVideoRCMode = EyeseeLinux::VencParameters::VideoRCMode_ABR;
        }
        else
        {
            aloge("fatal error! conf file video rc mode is [%s]?", pStrVideoRCMode);
            //mConfigPara.mVideoRCMode = EyeseeLinux::EyeseeRecorder::VideoRCMode_CBR;
            mConfigPara.mVideoRCMode = EyeseeLinux::VencParameters::VideoRCMode_CBR;
        }
    }
    else
    {
        aloge("fatal error! not find key video rc mode?");
        //mConfigPara.mVideoRCMode = EyeseeLinux::EyeseeRecorder::VideoRCMode_CBR;
        mConfigPara.mVideoRCMode = EyeseeLinux::VencParameters::VideoRCMode_CBR;
    }
    mConfigPara.mQp1 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_QP1, 0);
    mConfigPara.mQp2 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_QP2, 0);
    mConfigPara.mEncodeBitrate0 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_BITRATE0, 0)*1024*1024;
    mConfigPara.mEncodeBitrate1 = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_BITRATE1, 0)*1024*1024;

    char *GopModestr = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_GOP_MODE, NULL);
    if(!strcmp(GopModestr, "NORMALP"))
    {
        mConfigPara.mGopMode = VENC_GOPMODE_NORMALP;
    }
    else if(!strcmp(GopModestr, "DUALP"))
    {
        mConfigPara.mGopMode = VENC_GOPMODE_DUALP;
    }
    else if(!strcmp(GopModestr, "SMARTP"))
    {
        mConfigPara.mGopMode  = VENC_GOPMODE_SMARTP;
    }
    else
    {
        aloge("do not support %s GopMode, and set NORMALP mode!" ,GopModestr);
        mConfigPara.mGopMode = VENC_GOPMODE_NORMALP;
    }
    mConfigPara.mSPInterval = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_SPINTERVAL, 0);
    mConfigPara.mVirtualIFrameInterval = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_VIRTUALIFRAME_INTERVAL, 0);
    //mConfigPara.mbLongTermRefEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_LONGTERMREF_ENABLE, 0);

    mConfigPara.mbAdvanceRef = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ADVANCEDREF_ENABLE, 0);
    if(mConfigPara.mbAdvanceRef )
    {
        mConfigPara.nBase = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_NBASE, 0);
        mConfigPara.nEnhance = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_NENHANCE, 0);
        mConfigPara.bRefBaseEn = (unsigned char)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_BREFBASEEN, 0);
    }
    else
    {
        mConfigPara.nBase = 0;
        mConfigPara.nEnhance = 0;
        mConfigPara.bRefBaseEn = 0;
    }

    mConfigPara.mbHorizonfilp = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_HORIZONFLIP, 0);
    mConfigPara.mbAdaptiveintrainp = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ADAPTIVEINTRAINP, 0);
    mConfigPara.mb3dnr = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_3DNR, 0);
    mConfigPara.mbcolor2grey = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_COLOR2GREY, 0);
    mConfigPara.mbNullSkipEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_NULLSKIP, 0);

    mConfigPara.mbEnableSmartP = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_SMARTP, 0);
    mConfigPara.mIntraRefreshBlockNum = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_INTRAREFRESHBLOCKNUM, 0);
    mConfigPara.mTimeLapseFps = GetConfParaDouble(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_TIMELAPSE_FPS, 0);

    char *pStrSuperFrmMode = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_SUPER_FRM_MODE, NULL);
    if(pStrSuperFrmMode!=NULL)
    {
        if(!strcmp(pStrSuperFrmMode, "none"))
        {
            mConfigPara.mSuperFrmMode = SUPERFRM_NONE;
        }
        else if(!strcmp(pStrSuperFrmMode, "discard"))
        {
            mConfigPara.mSuperFrmMode = SUPERFRM_DISCARD;
        }
        else if(!strcmp(pStrSuperFrmMode, "reencode"))
        {
            mConfigPara.mSuperFrmMode = SUPERFRM_REENCODE;
        }
        else
        {
            aloge("fatal error! super frm mode is [%s]?", pStrSuperFrmMode);
            mConfigPara.mSuperFrmMode = SUPERFRM_NONE;
        }
    }
    else
    {
        aloge("fatal error! not find key super frm mode?");
        mConfigPara.mSuperFrmMode = SUPERFRM_NONE;
    }
    mConfigPara.mSuperIFrmBitsThr = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_SUPER_IFRM_BITSTHR, 0);
    mConfigPara.mSuperPFrmBitsThr = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_SUPER_PFRM_BITSTHR, 0);

    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_AUDIO_ENCODE_TYPE, NULL);
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
    mConfigPara.mAudioSampleRate = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_AUDIO_SAMPLE_RATE, 0);
    mConfigPara.mAudioChannelNum = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_AUDIO_CHANNEL_NUM, 0);
    mConfigPara.mAudioEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_AUDIO_ENCODE_BITRATE, 0);

    mConfigPara.mEncodeDuration = GetConfParaInt(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_DURATION, 0);
    mConfigPara.mEncodeFolderPath = GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_ENCODE_FOLDER, NULL);

    mConfigPara.mFilePath0 = GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_FILE_PATH0, NULL);
    mConfigPara.mFilePath1 = GetConfParaString(&stConfParser, SAMPLE_ENCODERESOLUTIONCHANGE_KEY_FILE_PATH1, NULL);
    alogd("path0[%s], path1[%s]", mConfigPara.mFilePath0.c_str(), mConfigPara.mFilePath1.c_str());
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleEncodeResolutionChangeContext::CreateFolder(const std::string& strFolderPath)
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
    cout<<"hello, sample_EncodeResolutionChange!"<<endl;
    SampleEncodeResolutionChangeContext* pContext = new SampleEncodeResolutionChangeContext;
    pSampleEncodeResolutionChangeContext = pContext;
    //parse command line param
    if(pContext->ParseCmdLine(argc, argv) != NO_ERROR)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }
    //parse config file.
    if(pContext->loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }
    //check folder existence, create folder if necessary
    if(SUCCESS != pContext->CreateFolder(pContext->mConfigPara.mEncodeFolderPath))
    {
        return -1;
    }
    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");
    //init mpp system
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();
    if(pContext->mConfigPara.mViClock != 0)
    {
        alogd("set vi clock to [%d]MHz", pContext->mConfigPara.mViClock);
        AW_MPI_VI_SetVIFreq(0, pContext->mConfigPara.mViClock*1000000);
    }
    AW_MPI_VENC_SetVEFreq(MM_INVALID_CHN, pContext->mConfigPara.mVeClock); //534
    //config camera.
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

        //test
        //tmpISPGeometry.mScalerOutChns.push_back(2);

        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(tmpISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }
    int cameraId;
    CameraInfo& cameraInfo = pContext->mCameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    pContext->mpCamera = EyeseeCamera::open(cameraId);
    pContext->mpCamera->setInfoCallback(&pContext->mCameraListener);
    pContext->mpCamera->prepareDevice();
    pContext->mpCamera->startDevice();

    CameraParameters cameraParam;
    pContext->mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], true);
    pContext->mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    SIZE_S captureSize;
    captureSize.Width = pContext->mConfigPara.mCaptureWidth;
    captureSize.Height = pContext->mConfigPara.mCaptureHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(pContext->mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(pContext->mConfigPara.mCaptureFormat);
    cameraParam.setVideoBufferNumber(5);
    pContext->mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    pContext->mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    pContext->mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], false);
    pContext->mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    SIZE_S previewSize;
    previewSize.Width = pContext->mConfigPara.mPreviewWidth;
    previewSize.Height = pContext->mConfigPara.mPreviewHeight;
    cameraParam.setVideoSize(previewSize);
    cameraParam.setPreviewFrameRate(pContext->mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(pContext->mConfigPara.mPreviewFormat);
    cameraParam.setVideoBufferNumber(5);
    pContext->mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    pContext->mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);

    //test
    //pContext->mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[2], cameraParam);
    //pContext->mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[2]);

    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);//close ui layer.
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);
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
    AW_MPI_VO_GetVideoLayerAttr(hlay, &pContext->mLayerAttr);
    pContext->mLayerAttr.stDispRect.X = 0;
    pContext->mLayerAttr.stDispRect.Y = 0;
    pContext->mLayerAttr.stDispRect.Width = pContext->mConfigPara.mDispWidth;
    pContext->mLayerAttr.stDispRect.Height = pContext->mConfigPara.mDispHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &pContext->mLayerAttr);
    pContext->mVoLayer = hlay;
    //camera preview test
    if(pContext->mConfigPara.mbPreviewEnable)
    {
        alogd("prepare setPreviewDisplay(), hlay=%d", pContext->mVoLayer);
        pContext->mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], pContext->mVoLayer);
    }
    pContext->mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    pContext->mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    //test
    //pContext->mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[2]);

    int ret = cdx_sem_down_timedwait(&pContext->mSemRenderStart, 5000);
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
    pContext->mpRecorder = new EyeseeRecorder();
    pContext->mpRecorder->setOnInfoListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnDataListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnErrorListener(&pContext->mRecorderListener);
    alogd("setSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    pContext->mpRecorder->setCameraProxy(pContext->mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    //test
    pContext->mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    pContext->mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    std::string strFilePath = pContext->mConfigPara.mEncodeFolderPath + '/' + pContext->mConfigPara.mFilePath0;
    int nFallocateLen = 0;
    SinkParam stSinkParam;
    memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = deduceFileFormat(pContext->mConfigPara.mFilePath0);
    stSinkParam.mOutputFd = -1;
    stSinkParam.mOutputPath = (char*)strFilePath.c_str();
    stSinkParam.mFallocateLen = nFallocateLen;
    stSinkParam.mMaxDurationMs = 0;
    stSinkParam.bCallbackOutFlag = false;
    stSinkParam.bBufFromCacheFlag = false;
    pContext->mpRecorder->addOutputSink(&stSinkParam);
    alogd("setVideoFrameRate=[%d]", pContext->mConfigPara.mDstFrameRate);
    //VencParameters
    VencParameters stVencParam;
    stVencParam.setVideoFrameRate(pContext->mConfigPara.mDstFrameRate);
    VencParameters::VEncAttr stVEncAttr;
    memset(&stVEncAttr, 0, sizeof(VencParameters::VEncAttr));
    stVEncAttr.mType = pContext->mConfigPara.mEncodeType;
    if(PT_H264 == stVEncAttr.mType)
    {
        stVEncAttr.mAttrH264.mProfile = pContext->mConfigPara.mH264_profile;
        stVEncAttr.mAttrH264.mLevel = pContext->mConfigPara.mH264Level;
    }
    else if(PT_H265 == stVEncAttr.mType)
    {
        stVEncAttr.mAttrH265.mProfile = pContext->mConfigPara.mH265_profile;
        stVEncAttr.mAttrH265.mLevel = pContext->mConfigPara.mH265Level;
    }
    else if(PT_MJPEG == stVEncAttr.mType)
    {
    }
    else
    {
        aloge("fatal error! unsupport vencType[0x%x]", stVEncAttr.mType);
    }
    stVencParam.setVEncAttr(stVEncAttr);
    stVencParam.setVideoEncoder(pContext->mConfigPara.mEncodeType);
    SIZE_S VideoSize;
    VideoSize.Width = pContext->mConfigPara.mEncodeWidth0;
    VideoSize.Height = pContext->mConfigPara.mEncodeHeight0;
    stVencParam.setVideoSize(VideoSize);
    alogd("setIDRFrameInterval=[%d]", pContext->mConfigPara.mIDRFrameInterval);
    stVencParam.setVideoEncodingIFramesNumberInterVal(pContext->mConfigPara.mIDRFrameInterval);
    stVencParam.enableVideoEncodingPIntra(pContext->mConfigPara.mbVideoEncodingPIntra);
    //stVencParam.setVideoEncodingRateControlMode(pContext->mConfigPara.mVideoRCMode);
    if (EyeseeLinux::VencParameters::VideoRCMode_CBR == pContext->mConfigPara.mVideoRCMode)
    {
        VencParameters::VEncBitRateControlAttr stRcAttr;
        memset(&stRcAttr, 0, sizeof(stRcAttr));
        stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType;
        stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
        switch(pContext->mConfigPara.mEncodeType)
        {
            case PT_H264:
            {
                stRcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate0;
                stRcAttr.mAttrH264Cbr.mMaxQp = pContext->mConfigPara.mQp2;
                stRcAttr.mAttrH264Cbr.mMinQp = pContext->mConfigPara.mQp1;
                stRcAttr.mAttrH264Cbr.mMaxPqp = 50;
                stRcAttr.mAttrH264Cbr.mMinPqp = 10;
                stRcAttr.mAttrH264Cbr.mQpInit = 30;
                stRcAttr.mAttrH264Cbr.mbEnMbQpLimit = 0;
                break;
            }
            case PT_H265:
            {
                stRcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate0;
                stRcAttr.mAttrH265Cbr.mMaxQp = pContext->mConfigPara.mQp2;
                stRcAttr.mAttrH265Cbr.mMinQp = pContext->mConfigPara.mQp1;
                stRcAttr.mAttrH265Cbr.mMaxPqp = 50;
                stRcAttr.mAttrH265Cbr.mMinPqp = 10;
                stRcAttr.mAttrH265Cbr.mQpInit = 30;
                stRcAttr.mAttrH265Cbr.mbEnMbQpLimit = 0;
                break;
            }
            case PT_MJPEG:
            {
                stRcAttr.mAttrMjpegCbr.mBitRate = pContext->mConfigPara.mEncodeBitrate0;
                break;
            }
            default:
            {
                aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType);
                break;
            }
        }
        stVencParam.setVEncBitRateControlAttr(stRcAttr);
    }
    else if (EyeseeLinux::VencParameters::VideoRCMode_FIXQP == pContext->mConfigPara.mVideoRCMode)
    {
        int IQp = pContext->mConfigPara.mQp1;
        int PQp = pContext->mConfigPara.mQp2;
        VencParameters::VEncBitRateControlAttr stRcAttr;
        stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType;
        stRcAttr.mRcMode = VencParameters::VideoRCMode_FIXQP;
        switch(pContext->mConfigPara.mEncodeType)
        {
            case PT_H264:
            {
                stRcAttr.mAttrH264FixQp.mIQp = IQp;
                stRcAttr.mAttrH264FixQp.mPQp = PQp;
                break;
            }
            case PT_H265:
            {
                stRcAttr.mAttrH265FixQp.mIQp = IQp;
                stRcAttr.mAttrH265FixQp.mPQp = PQp;
                break;
            }
            case PT_MJPEG:
            {
                stRcAttr.mAttrMjpegFixQp.mQfactor = IQp;
                break;
            }
            default:
            {
                aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType);
                break;
            }
        }
        stVencParam.setVEncBitRateControlAttr(stRcAttr);
    }
    else if (EyeseeLinux::VencParameters::VideoRCMode_ABR == pContext->mConfigPara.mVideoRCMode)
    {
        int minIQp = pContext->mConfigPara.mQp1;
        int maxIQp = pContext->mConfigPara.mQp2;
        VencParameters::VEncBitRateControlAttr stRcAttr;
        stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType;
        stRcAttr.mRcMode = VencParameters::VideoRCMode_ABR;
        switch(pContext->mConfigPara.mEncodeType)
        {
            case PT_H264:
            {
                stRcAttr.mAttrH264Abr.mMaxBitRate = pContext->mConfigPara.mEncodeBitrate0 + 2*1024*1024;
                stRcAttr.mAttrH264Abr.mRatioChangeQp = 85;
                stRcAttr.mAttrH264Abr.mQuality = 8;
                stRcAttr.mAttrH264Abr.mMinIQp = minIQp;
                stRcAttr.mAttrH264Abr.mMaxIQp = maxIQp;
                stRcAttr.mAttrH264Abr.mMaxQp = 51;
                stRcAttr.mAttrH264Abr.mMinQp = 1;
                break;
            }
            case PT_H265:
            {
                stRcAttr.mAttrH265Abr.mMaxBitRate = pContext->mConfigPara.mEncodeBitrate0 + 2*1024*1024;
                stRcAttr.mAttrH265Abr.mRatioChangeQp = 85;
                stRcAttr.mAttrH265Abr.mQuality = 8;
                stRcAttr.mAttrH265Abr.mMinIQp = minIQp;
                stRcAttr.mAttrH265Abr.mMaxIQp = maxIQp;
                stRcAttr.mAttrH265Abr.mMaxQp = 51;
                stRcAttr.mAttrH265Abr.mMinQp = 1;
                break;
            }
            default:
            {
                aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType);
                break;
            }
        }
        stVencParam.setVEncBitRateControlAttr(stRcAttr);
    }

    VENC_GOP_ATTR_S GopAttr;
    GopAttr.enGopMode = pContext->mConfigPara.mGopMode;
    if(VENC_GOPMODE_DUALP == pContext->mConfigPara.mGopMode)
    {
        GopAttr.stDualP.mSPInterval = pContext->mConfigPara.mSPInterval;
    }
    else if(VENC_GOPMODE_SMARTP == pContext->mConfigPara.mGopMode)
    {
        GopAttr.mGopSize = pContext->mConfigPara.mVirtualIFrameInterval;
    }
    stVencParam.setGopAttr(GopAttr);
    if(pContext->mConfigPara.mbAdvanceRef)
    {
        VENC_PARAM_REF_S RefParam;
        RefParam.Base = pContext->mConfigPara.nBase;
        RefParam.Enhance = pContext->mConfigPara.nEnhance;
        RefParam.bEnablePred = pContext->mConfigPara.bRefBaseEn;
        stVencParam.setRefParam(RefParam);
    }

    if(pContext->mConfigPara.mbEnableSmartP)
    {
        VencSmartFun smartParam;
        memset(&smartParam, 0, sizeof(VencSmartFun));
        smartParam.smart_fun_en = 1;
        smartParam.img_bin_en = 1;
        smartParam.img_bin_th = 0;
        smartParam.shift_bits = 2;
        alogd("setSmartP shift_bits=[%d]", smartParam.shift_bits);
        stVencParam.setVideoEncodingSmartP(smartParam);
    }

    alogd("setIntraRefreshBlockNum=[%d]", pContext->mConfigPara.mIntraRefreshBlockNum);
    VENC_PARAM_INTRA_REFRESH_S stIntraRefresh;
    if(pContext->mConfigPara.mIntraRefreshBlockNum > 0)
    {
        stIntraRefresh = {TRUE, FALSE, (unsigned int)pContext->mConfigPara.mIntraRefreshBlockNum, 0};
    }
    else
    {
        stIntraRefresh = {FALSE, FALSE, 0, 0};
    }
    stVencParam.setVideoEncodingIntraRefresh(stIntraRefresh);
    pContext->mpRecorder->setCaptureRate(pContext->mConfigPara.mTimeLapseFps);
    stVencParam.enableHorizonFlip(pContext->mConfigPara.mbHorizonfilp);
    stVencParam.enableAdaptiveIntraInp(pContext->mConfigPara.mbAdaptiveintrainp);
    //stVencParam.set3DFilter(pContext->mConfigPara.mb3dnr);
    stVencParam.enableColor2Grey(pContext->mConfigPara.mbcolor2grey);
    stVencParam.enableNullSkip(pContext->mConfigPara.mbNullSkipEnable);
    VENC_SUPERFRAME_CFG_S stSuperFrameConfig = {
        .enSuperFrmMode = pContext->mConfigPara.mSuperFrmMode,
        .SuperIFrmBitsThr = pContext->mConfigPara.mSuperIFrmBitsThr,
        .SuperPFrmBitsThr = pContext->mConfigPara.mSuperPFrmBitsThr,
        .SuperBFrmBitsThr = 0,
    };
    stVencParam.setVencSuperFrameConfig(stSuperFrameConfig);
    pContext->mVencId = 0;
    pContext->mpRecorder->setVencParameters(pContext->mVencId, &stVencParam);
    //bind Venc and Vipp
    pContext->mpRecorder->bindVeVipp(pContext->mVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    if(pContext->mConfigPara.mAudioEncodeType != PT_MAX)
    {
        pContext->mpRecorder->setAudioSamplingRate(pContext->mConfigPara.mAudioSampleRate);
        pContext->mpRecorder->setAudioChannels(pContext->mConfigPara.mAudioChannelNum);
        pContext->mpRecorder->setAudioEncodingBitRate(pContext->mConfigPara.mAudioEncodeBitrate);
        pContext->mpRecorder->setAudioEncoder(pContext->mConfigPara.mAudioEncodeType);
    }
    alogd("prepare()!");
    pContext->mpRecorder->prepare();

    alogd("start()!");
    pContext->mpRecorder->start();
    alogd("will record [%d]s!", pContext->mConfigPara.mEncodeDuration);
    if(pContext->mConfigPara.mDigitalZoom > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, 5*1000);
        pContext->mpCamera->getParameters(pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
        int oldZoom = cameraParam.getZoom();
        cameraParam.setZoom(pContext->mConfigPara.mDigitalZoom);
        pContext->mpCamera->setParameters(pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
        alogd("change digital zoom0[%d]->[%d]", oldZoom, pContext->mConfigPara.mDigitalZoom);

        pContext->mpCamera->getParameters(pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        oldZoom = cameraParam.getZoom();
        cameraParam.setZoom(pContext->mConfigPara.mDigitalZoom);
        pContext->mpCamera->setParameters(pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        alogd("change digital zoom1[%d]->[%d]", oldZoom, pContext->mConfigPara.mDigitalZoom);
    }

#ifdef Pause_debug
    char str[256]  = {0};
    char val[16] = {0};
    int num = 0, vl = 0;
    RECT_S get_test_rect;
    memset(&get_test_rect, 0, sizeof(RECT_S));

    RECT_S test_rect;
    memset(&test_rect, 0, sizeof(RECT_S));
    alogd("===Test Pause and Resume!!===");
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
        switch (num) {;
            case 1:
                alogd("\n\n--Pause--:\n\n");
                pContext->mpRecorder->pause(TRUE);
                time0 = CDX_GetSysTimeUsMonotonic();
                break;
            case 2:
                alogd("\n\n--Resume--:\n\n");
                pContext->mpRecorder->pause(FALSE);
                timeIntervalUs = (CDX_GetSysTimeUsMonotonic() - time0);
                alogd("-->Pause duration time %lld[us]",timeIntervalUs);
                break;
            default:
                alogd("intput error.\r\n");
                break;
        }
    }
#endif

    if(pContext->mConfigPara.mEncodeDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mEncodeDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    alogd("record done! stop()!");
    //stop mHMR0
    pContext->mpRecorder->stop();
    //delete pContext->mpRecorder;
    //pContext->mpRecorder = NULL;

    //record second file.
    if(!pContext->mConfigPara.mFilePath1.empty())
    {
        pContext->mpRecorder->reset();
        pContext->mpRecorder->setOnInfoListener(&pContext->mRecorderListener);
        pContext->mpRecorder->setOnDataListener(&pContext->mRecorderListener);
        pContext->mpRecorder->setOnErrorListener(&pContext->mRecorderListener);
        alogd("setSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        pContext->mpRecorder->setCameraProxy(pContext->mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        pContext->mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
        pContext->mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
        std::string strFilePath = pContext->mConfigPara.mEncodeFolderPath + '/' + pContext->mConfigPara.mFilePath1;
        SinkParam stSinkParam;
        memset(&stSinkParam, 0, sizeof(SinkParam));
        stSinkParam.mOutputFormat = deduceFileFormat(pContext->mConfigPara.mFilePath1);
        stSinkParam.mOutputFd = -1;
        stSinkParam.mOutputPath = (char*)strFilePath.c_str();
        stSinkParam.mFallocateLen = 0;
        stSinkParam.mMaxDurationMs = 0;
        stSinkParam.bCallbackOutFlag = false;
        stSinkParam.bBufFromCacheFlag = false;
        pContext->mpRecorder->addOutputSink(&stSinkParam);
        VencParameters::VEncAttr stVEncAttr;
        memset(&stVEncAttr, 0, sizeof(VencParameters::VEncAttr));
        stVEncAttr.mType = pContext->mConfigPara.mEncodeType;
        if(PT_H264 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH264.mProfile = pContext->mConfigPara.mH264_profile;
            stVEncAttr.mAttrH264.mLevel = pContext->mConfigPara.mH264Level;
        }
        else if(PT_H265 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH265.mProfile = pContext->mConfigPara.mH265_profile;
            stVEncAttr.mAttrH265.mLevel = pContext->mConfigPara.mH265Level;
        }
        else if(PT_MJPEG == stVEncAttr.mType)
        {
        }
        else
        {
            aloge("fatal error! unsupport vencType[0x%x]", stVEncAttr.mType);
        }
        alogd("setEncodeVideoSize=[%dx%d]", pContext->mConfigPara.mEncodeWidth1, pContext->mConfigPara.mEncodeHeight1);
        alogd("setIDRFrameInterval=[%d]", pContext->mConfigPara.mIDRFrameInterval);
        alogd("setVideoRCMode=[%d]", pContext->mConfigPara.mVideoRCMode);
        if (EyeseeLinux::VencParameters::VideoRCMode_CBR == pContext->mConfigPara.mVideoRCMode)
        {
            VencParameters::VEncBitRateControlAttr stRcAttr;
            stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType;
            stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
            switch(pContext->mConfigPara.mEncodeType)
            {
                case PT_H264:
                {
                    stRcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate1;
                    stRcAttr.mAttrH264Cbr.mMaxQp = pContext->mConfigPara.mQp2;
                    stRcAttr.mAttrH264Cbr.mMinQp = pContext->mConfigPara.mQp1;
                    break;
                }
                case PT_H265:
                {
                    stRcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate1;
                    stRcAttr.mAttrH265Cbr.mMaxQp = pContext->mConfigPara.mQp2;
                    stRcAttr.mAttrH265Cbr.mMinQp = pContext->mConfigPara.mQp1;
                    break;
                }
                case PT_MJPEG:
                {
                    stRcAttr.mAttrMjpegCbr.mBitRate = pContext->mConfigPara.mEncodeBitrate1;
                    break;
                }
                default:
                {
                    aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType);
                    break;
                }
            }
            stVencParam.setVEncBitRateControlAttr(stRcAttr);
        }
        else if (EyeseeLinux::VencParameters::VideoRCMode_FIXQP == pContext->mConfigPara.mVideoRCMode)
        {
            int IQp = pContext->mConfigPara.mQp1;
            int PQp = pContext->mConfigPara.mQp2;
            VencParameters::VEncBitRateControlAttr stRcAttr;
            stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType;
            stRcAttr.mRcMode = VencParameters::VideoRCMode_FIXQP;
            switch(pContext->mConfigPara.mEncodeType)
            {
                case PT_H264:
                {
                    stRcAttr.mAttrH264FixQp.mIQp = IQp;
                    stRcAttr.mAttrH264FixQp.mPQp = PQp;
                    break;
                }
                case PT_H265:
                {
                    stRcAttr.mAttrH265FixQp.mIQp = IQp;
                    stRcAttr.mAttrH265FixQp.mPQp = PQp;
                    break;
                }
                case PT_MJPEG:
                {
                    stRcAttr.mAttrMjpegFixQp.mQfactor = IQp;
                    break;
                }
                default:
                {
                    aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType);
                    break;
                }
            }
            stVencParam.setVEncBitRateControlAttr(stRcAttr);
        }
        else if (EyeseeLinux::VencParameters::VideoRCMode_ABR == pContext->mConfigPara.mVideoRCMode)
        {
            int minIQp = pContext->mConfigPara.mQp1;
            int maxIQp = pContext->mConfigPara.mQp2;
            VencParameters::VEncBitRateControlAttr stRcAttr;
            stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType;
            stRcAttr.mRcMode = VencParameters::VideoRCMode_ABR;
            switch(pContext->mConfigPara.mEncodeType)
            {
                case PT_H264:
                {
                    stRcAttr.mAttrH264Abr.mMaxBitRate = pContext->mConfigPara.mEncodeBitrate1 + 2*1024*1024;
                    stRcAttr.mAttrH264Abr.mRatioChangeQp = 85;
                    stRcAttr.mAttrH264Abr.mQuality = 8;
                    stRcAttr.mAttrH264Abr.mMinIQp = minIQp;
                    stRcAttr.mAttrH264Abr.mMaxIQp = maxIQp;
                    stRcAttr.mAttrH264Abr.mMaxQp = 51;
                    stRcAttr.mAttrH264Abr.mMinQp = 1;
                    break;
                }
                case PT_H265:
                {
                    stRcAttr.mAttrH265Abr.mMaxBitRate = pContext->mConfigPara.mEncodeBitrate1 + 2*1024*1024;
                    stRcAttr.mAttrH265Abr.mRatioChangeQp = 85;
                    stRcAttr.mAttrH265Abr.mQuality = 8;
                    stRcAttr.mAttrH265Abr.mMinIQp = minIQp;
                    stRcAttr.mAttrH265Abr.mMaxIQp = maxIQp;
                    stRcAttr.mAttrH265Abr.mMaxQp = 51;
                    stRcAttr.mAttrH265Abr.mMinQp = 1;
                    break;
                }
                default:
                {
                    aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType);
                    break;
                }
            }
            stVencParam.setVEncBitRateControlAttr(stRcAttr);
        }

        VENC_GOP_ATTR_S GopAttr;
        GopAttr.enGopMode = pContext->mConfigPara.mGopMode;
        if(VENC_GOPMODE_DUALP == pContext->mConfigPara.mGopMode)
        {
            GopAttr.stDualP.mSPInterval = pContext->mConfigPara.mSPInterval;
        }
        else if(VENC_GOPMODE_SMARTP == pContext->mConfigPara.mGopMode)
        {
            GopAttr.mGopSize = pContext->mConfigPara.mVirtualIFrameInterval;
        }
        VENC_PARAM_INTRA_REFRESH_S stIntraRefresh;
        if(pContext->mConfigPara.mIntraRefreshBlockNum > 0)
        {
            stIntraRefresh = {TRUE, FALSE, (unsigned int)pContext->mConfigPara.mIntraRefreshBlockNum, 0};
        }
        else
        {
            stIntraRefresh = {FALSE, FALSE, 0, 0};
        }
        pContext->mpRecorder->setCaptureRate(pContext->mConfigPara.mTimeLapseFps);
        VENC_SUPERFRAME_CFG_S stSuperFrameConfig = {
            .enSuperFrmMode = pContext->mConfigPara.mSuperFrmMode,
            .SuperIFrmBitsThr = pContext->mConfigPara.mSuperIFrmBitsThr,
            .SuperPFrmBitsThr = pContext->mConfigPara.mSuperPFrmBitsThr,
            .SuperBFrmBitsThr = 0,
        };
        stVencParam.setVencSuperFrameConfig(stSuperFrameConfig);
        pContext->mVencId = 0;
        pContext->mpRecorder->setVencParameters(pContext->mVencId, &stVencParam);
        pContext->mpRecorder->bindVeVipp(pContext->mVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        if(pContext->mConfigPara.mAudioEncodeType != PT_MAX)
        {
            pContext->mpRecorder->setAudioSamplingRate(pContext->mConfigPara.mAudioSampleRate);
            pContext->mpRecorder->setAudioChannels(pContext->mConfigPara.mAudioChannelNum);
            pContext->mpRecorder->setAudioEncodingBitRate(pContext->mConfigPara.mAudioEncodeBitrate);
            pContext->mpRecorder->setAudioEncoder(pContext->mConfigPara.mAudioEncodeType);
        }
        alogd("prepare()!");
        pContext->mpRecorder->prepare();
        alogd("start()!");
        pContext->mpRecorder->start();
        alogd("will record [%d]s!", pContext->mConfigPara.mEncodeDuration);
        if(pContext->mConfigPara.mEncodeDuration > 0)
        {
            cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mEncodeDuration*1000);
        }
        else
        {
            cdx_sem_down(&pContext->mSemExit);
        }

        alogd("record done! stop()!");
        //stop mHMR
        pContext->mpRecorder->stop();
    }
    else
    {
        alogd("user don't set second file, exit now.");
    }
    delete pContext->mpRecorder;
    pContext->mpRecorder = NULL;

    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
    pContext->mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    pContext->mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    pContext->mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    pContext->mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    pContext->mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    pContext->mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    pContext->mpCamera->stopDevice();
    pContext->mpCamera->releaseDevice();
    EyeseeCamera::close(pContext->mpCamera);
    pContext->mpCamera = NULL;
    //close vo
    AW_MPI_VO_DisableVideoLayer(pContext->mVoLayer);
    pContext->mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = -1;
    //exit mpp system
    AW_MPI_SYS_Exit();

    delete pContext;
    pContext = NULL;
    cout<<"bye, sample_EncodeResolutionChange!"<<endl;
    return result;
}
