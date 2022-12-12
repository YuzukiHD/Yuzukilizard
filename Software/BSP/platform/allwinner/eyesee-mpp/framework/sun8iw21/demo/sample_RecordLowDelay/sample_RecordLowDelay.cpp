//#define LOG_NDEBUG 0
#define LOG_TAG "sample_RecorderCallbackOut"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>

#include <hwdisplay.h>
#include <record_writer.h>
#include <confparser.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>
#include <mpi_venc.h>

#include "sample_RecordLowDelay_config.h"
#include "sample_RecordLowDelay.h"

using namespace std;
using namespace EyeseeLinux;

SampleRecordLowDelayContext *pSampleRecordLowDelayContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleRecordLowDelayContext!=NULL)
    {
        cdx_sem_up(&pSampleRecordLowDelayContext->mSemExit);
    }
}

void EyeseeRecorderListener::onError(EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);
    switch(what)
    {
        case EyeseeRecorder::MEDIA_ERROR_VENC_TIMEOUT:
        {
            uint64_t framePts = *(uint64_t*)extra;
            alogd("app know error afbc frame pts[%lld]us", framePts);
            break;
        }
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
            alogd("receive onInfo message! media_info_type[%d] muxer_id[%d]", what, extra);
            break;
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
            alogd("receive onInfo message! media_info_type[%d] muxer_id[%d]", what, extra);
            break;
        default:
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
    }
}

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    VEncBuffer* frame = NULL;

    while (1) 
    {
        frame = pMr->getOneBsFrame();
        if (frame == NULL)
            break;

        if (frame->data == NULL) 
        {
            static int cnt = 0;
            pMr->freeOneBsFrame(frame);
            aloge("fatal error! null data: %d", ++cnt);
            break;
        }

        if (frame->stream_type == RawPacketTypeVideo) 
        { // video
            if(mpOwner->mConfigPara.mEncodeType == PT_H264)
            {
                /*if ((*(frame->data + 4) & 0x1F) == 5) 
                { // if it is IDR frame
                    VencHeaderData head_info;
                    pMr->getEncDataHeader(&head_info);
                    // write sps/pps at the begining of the file and the front of I frame
                    fwrite(head_info.pBuffer, 1, head_info.nLength, mpOwner->mRawFile);
                }*/
            }
            else if(mpOwner->mConfigPara.mEncodeType == PT_H265)
            {
            }
            else if(mpOwner->mConfigPara.mEncodeType == PT_MJPEG)
            {
            }
            else
            {
                aloge("fatal error! unknown video encode type[%d]", mpOwner->mConfigPara.mEncodeType);
            }
            if(-1 == mpOwner->mLastCallbackPts)
            {
                mpOwner->mLastCallbackPts = frame->pts;
            }
            else
            {
                int64_t itl = frame->pts - mpOwner->mLastCallbackPts;
                if(itl > (1000000/mpOwner->mConfigPara.mFrameRate)*2)
                {
                    alogd("frame lost: itl[%lld]us > frameItl[%lld]us", itl, 1000000/mpOwner->mConfigPara.mFrameRate);
                }
                mpOwner->mLastCallbackPts = frame->pts;
            }
            //alogd("pts[%lld]us?", frame->pts);
            if(mpOwner->mRawFile)
            {
                fwrite(frame->data, 1, frame->data_size, mpOwner->mRawFile);
            }
        }
        else if(frame->stream_type == RawPacketTypeVideoExtra)
        {
            alogd("stream type is video extra(spspps), size[%d]", frame->data_size);
            if(mpOwner->mRawFile)
            {
                fwrite(frame->data, 1, frame->data_size, mpOwner->mRawFile);
            }
        }
        else 
        {
            alogd("stream type: %d", frame->stream_type);
        }
        pMr->freeOneBsFrame(frame);
    }
}
EyeseeRecorderListener::EyeseeRecorderListener(SampleRecordLowDelayContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleRecordLowDelayContext::SampleRecordLowDelayContext()
    :mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = MM_INVALID_DEV;
    mVoLayer = MM_INVALID_LAYER;
    mpCamera = NULL;
    mpRecorder = NULL;
    mRawFile = NULL;
    mLastCallbackPts = -1;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mVencId = -1;
}

SampleRecordLowDelayContext::~SampleRecordLowDelayContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
    if(mpRecorder!=NULL)
    {
        aloge("fatal error! EyeseeRecorder is not destruct!");
    }
}

status_t SampleRecordLowDelayContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_RecordLowDelay.conf\n";
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
status_t SampleRecordLowDelayContext::loadConfig()
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
        mConfigPara.mPreviewHeight = 480;
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mbPreviewEnable = true;
        mConfigPara.mbEncodeEnable = true;
        mConfigPara.mFrameRate = 30;
        mConfigPara.mDisplayFrameRate = 0;
        mConfigPara.mCaptureBufNum = 5;
        mConfigPara.mPreviewBufNum = 5;
        mConfigPara.mEncodeType = PT_H265;
        mConfigPara.mEncodeWidth = 1920;
        mConfigPara.mEncodeHeight = 1080;
        mConfigPara.mEncodeBitrate = 6*1024*1024;
        mConfigPara.mEncodeDuration = 20;
        mConfigPara.mbCallbackFlag = true;
        mConfigPara.mEncodeFolderPath = "/home/sample_RecordLowDelay_Files";
        mConfigPara.mCallbackFilePath = "";
        mConfigPara.mFilePath2 = "";
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_CAPTURE_WIDTH, 0); 
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_CAPTURE_HEIGHT, 0); 
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_CAPTURE_FORMAT, NULL); 
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

    mConfigPara.mPreviewWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_PREVIEW_WIDTH, 0); 
    mConfigPara.mPreviewHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_PREVIEW_HEIGHT, 0); 
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_PREVIEW_FORMAT, NULL); 
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
    
    mConfigPara.mbPreviewEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_PREVIEW_ENABLE, 0);
    mConfigPara.mbEncodeEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_ENCODE_ENABLE, 0);
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_FRAME_RATE, 0); 
    mConfigPara.mDisplayFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_DISPLAY_FRAME_RATE, 0);
    mConfigPara.mCaptureBufNum = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_CAPTURE_BUF_NUM, 0); 
    mConfigPara.mPreviewBufNum = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_PREVIEW_BUF_NUM, 0); 
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_ENCODE_TYPE, NULL); 
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
    mConfigPara.mEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_ENCODE_WIDTH, 0);
    mConfigPara.mEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_ENCODE_HEIGHT, 0);
    mConfigPara.mEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_ENCODE_BITRATE, 0)*1024*1024;
    
    mConfigPara.mEncodeDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_ENCODE_DURATION, 0);
    mConfigPara.mbCallbackFlag = (bool)GetConfParaInt(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_CALLBACK_FLAG, 0);
    mConfigPara.mEncodeFolderPath = GetConfParaString(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_ENCODE_FOLDER, NULL);
    mConfigPara.mCallbackFilePath = GetConfParaString(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_CALLBACK_FILE_PATH, NULL);
    mConfigPara.mFilePath2 = GetConfParaString(&stConfParser, SAMPLE_RECORDLOWDELAY_KEY_FILE_PATH2, NULL);
    alogd("callbackPath[%s], path2[%s]", mConfigPara.mCallbackFilePath.c_str(), mConfigPara.mFilePath2.c_str());
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleRecordLowDelayContext::CreateFolder(const std::string& strFolderPath)
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
    cout<<"hello, sample_RecordLowDelay!"<<endl;
    SampleRecordLowDelayContext stContext;
    pSampleRecordLowDelayContext = &stContext;
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
    AW_MPI_VENC_SetVEFreq(MM_INVALID_CHN, 534);

    //config camera.
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        ISPGeometry mISPGeometry;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 1;
        //cameraInfo.mMPPGeometry.mISPDev = 0;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(1);
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(3);
        mISPGeometry.mScalerOutChns.push_back(0);
        mISPGeometry.mScalerOutChns.push_back(1);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }
    int cameraId;
    CameraInfo cameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
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
    cameraParam.setVideoBufferNumber(stContext.mConfigPara.mCaptureBufNum);
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    stContext.mVoDev = 0;
    AW_MPI_VO_Enable(stContext.mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer.
    if(stContext.mConfigPara.mbPreviewEnable)
    {
        stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], false);
        stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        SIZE_S previewSize;
        previewSize.Width = stContext.mConfigPara.mPreviewWidth;
        previewSize.Height = stContext.mConfigPara.mPreviewHeight;
        cameraParam.setVideoSize(previewSize);
        cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
        cameraParam.setDisplayFrameRate(stContext.mConfigPara.mDisplayFrameRate);
        cameraParam.setPreviewFormat(stContext.mConfigPara.mPreviewFormat);
        cameraParam.setVideoBufferNumber(stContext.mConfigPara.mPreviewBufNum);
        stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
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
        stContext.mLayerAttr.stDispRect.Width = stContext.mConfigPara.mPreviewWidth;
        stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.mPreviewHeight;
        AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
        stContext.mVoLayer = hlay;
        //camera preview test
        alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
        stContext.mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], stContext.mVoLayer);
    }

    //record first file.
    if(stContext.mConfigPara.mbEncodeEnable)
    {
        stContext.mpRecorder = new EyeseeRecorder();
        stContext.mpRecorder->setOnInfoListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnDataListener(&stContext.mRecorderListener);
        stContext.mpRecorder->setOnErrorListener(&stContext.mRecorderListener);
        alogd("setSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        stContext.mpRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        stContext.mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
        if(!stContext.mConfigPara.mFilePath2.empty())
        {
            std::string strFilePath2 = stContext.mConfigPara.mEncodeFolderPath + '/' + stContext.mConfigPara.mFilePath2;
            SinkParam stSinkParam;
            memset(&stSinkParam, 0, sizeof(SinkParam));
            stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
            stSinkParam.mOutputFd = -1;
            stSinkParam.mOutputPath = (char*)strFilePath2.c_str();
            stSinkParam.mFallocateLen = 0;
            stSinkParam.mMaxDurationMs = 0;
            stSinkParam.bBufFromCacheFlag = false;
            stSinkParam.bCallbackOutFlag = false;
            stContext.mpRecorder->addOutputSink(&stSinkParam);
        }
        else
        {
            alogd("don't write mp4 file");
        }
        if(!stContext.mConfigPara.mCallbackFilePath.empty())
        {
            std::string strRawFilePath = stContext.mConfigPara.mEncodeFolderPath + '/' + stContext.mConfigPara.mCallbackFilePath;
            stContext.mRawFile = fopen(strRawFilePath.c_str(), "wb");
        }
        else
        {
            alogd("not need raw file");
            stContext.mRawFile = NULL;
        }
        SinkParam stSinkParam;
        memset(&stSinkParam, 0, sizeof(SinkParam));
        stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_RAW;
        stSinkParam.mOutputFd = -1;
        stSinkParam.mOutputPath = NULL;
        stSinkParam.mFallocateLen = 0;
        stSinkParam.mMaxDurationMs = 0;
        stSinkParam.bCallbackOutFlag = true;
        stSinkParam.bBufFromCacheFlag = false;
        stContext.mpRecorder->addOutputSink(&stSinkParam);

        VencParameters stVencParam;
        alogd("setVideoFrameRate=[%d]", stContext.mConfigPara.mFrameRate);
        stVencParam.setVideoFrameRate(stContext.mConfigPara.mFrameRate);
        alogd("setEncodeVideoSize=[%dx%d]", stContext.mConfigPara.mEncodeWidth, stContext.mConfigPara.mEncodeHeight);
        SIZE_S stVideoSize;
        stVideoSize.Width = stContext.mConfigPara.mEncodeWidth;
        stVideoSize.Height = stContext.mConfigPara.mEncodeHeight;
        stVencParam.setVideoSize(stVideoSize);
        stVencParam.setVideoEncoder(stContext.mConfigPara.mEncodeType);
        //stVencParam.setVideoEncodingRateControlMode(VencParameters::VideoRCMode_CBR);
        VencParameters::VEncBitRateControlAttr stRcAttr;
        stRcAttr.mVEncType = stContext.mConfigPara.mEncodeType;
        stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
        switch(stContext.mConfigPara.mEncodeType)
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
            case PT_MJPEG:
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
        stContext.mVencId = 0;
        stContext.mpRecorder->setVencParameters(stContext.mVencId, &stVencParam);
        stContext.mpRecorder->bindVeVipp(stContext.mVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
        alogd("prepare()!");
        stContext.mpRecorder->prepare();
        alogd("start()!");
        stContext.mpRecorder->start();
    }

    alogd("camera begin to start channel!");
    if(stContext.mConfigPara.mbPreviewEnable)
    {
        stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    }
    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    alogd("will keep [%d]s!", stContext.mConfigPara.mEncodeDuration);
    
    if(stContext.mConfigPara.mEncodeDuration > 0)
    {
        cdx_sem_down_timedwait(&stContext.mSemExit, stContext.mConfigPara.mEncodeDuration*1000);
    }
    else
    {
        cdx_sem_down(&stContext.mSemExit);
    }

    if(stContext.mConfigPara.mbEncodeEnable)
    {
        alogd("record done! stop()!");
        //stop mHMR0
        stContext.mpRecorder->stop();

        delete stContext.mpRecorder;
        stContext.mpRecorder = NULL;
    }

    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
    stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    if(stContext.mConfigPara.mbPreviewEnable)
    {
        stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    }
    stContext.mpCamera->stopDevice(); 
    stContext.mpCamera->releaseDevice(); 
    EyeseeCamera::close(stContext.mpCamera);
    stContext.mpCamera = NULL;
    //close vo
    if(stContext.mVoLayer >= 0)
    {
        AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
        stContext.mVoLayer = MM_INVALID_LAYER;
    }
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = MM_INVALID_DEV;
    //exit mpp system
    AW_MPI_SYS_Exit(); 
    if(stContext.mRawFile)
    {
        fclose(stContext.mRawFile);
        stContext.mRawFile = NULL;
    }
    cout<<"bye, sample_RecordLowDelay!"<<endl;
    return result;
}
