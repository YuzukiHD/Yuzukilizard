//#define LOG_NDEBUG 0
#define LOG_TAG "sample_RecorderSegment"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>

#include <hwdisplay.h>
#include <record_writer.h>
#include <confparser.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

#include "sample_RecorderSegment_config.h"
#include "sample_RecorderSegment.h"

using namespace std;
using namespace EyeseeLinux;

SampleRecorderSegmentContext *pSampleRecorderSegmentContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleRecorderSegmentContext!=NULL)
    {
        cdx_sem_up(&pSampleRecorderSegmentContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0])
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
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
    alogd("channel %d picture data size %d", chnId, size);
    if(chnId != mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0])
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    }
    char picName[64];
    sprintf(picName, "pic[%02d].jpg", mpContext->mPicNum++);
    std::string PicFullPath = mpContext->mConfigPara.mSegmentFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
}

EyeseeCameraListener::EyeseeCameraListener(SampleRecorderSegmentContext *pContext)
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
            if(mpOwner->mMuxerId != nMuxerId)
            {
                aloge("fatal error! why muxerId is not match[%d]!=[%d]?", mpOwner->mMuxerId, nMuxerId);
            }
            std::string strSegmentFilePath = mpOwner->mConfigPara.mSegmentFolderPath + '/' + mpOwner->MakeSegmentFileName();
            mpOwner->mpRecorder->setOutputFileSync((char*)strSegmentFilePath.c_str(), 0, nMuxerId);
            mpOwner->mSegmentFiles.push_back(strSegmentFilePath);
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done,  muxer_id[%d]", extra);
            int nMuxerId = extra;
            int ret;
            while(mpOwner->mSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
            {
                if ((ret = remove(mpOwner->mSegmentFiles[0].c_str())) < 0)
                {
                    aloge("fatal error! delete file[%s] failed:%s", mpOwner->mSegmentFiles[0].c_str(), strerror(errno));
                }
                else
                {
                    alogd("delete file[%s] success, ret[0x%x]", mpOwner->mSegmentFiles[0].c_str(), ret);
                }
                mpOwner->mSegmentFiles.pop_front();
            }
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

EyeseeRecorderListener::EyeseeRecorderListener(SampleRecorderSegmentContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleRecorderSegmentContext::SampleRecorderSegmentContext()
    :mCameraListener(this)
    ,mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera = NULL;
    mpRecorder = NULL;
    mFileNum = 0;
    mPicNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mMuxerId = -1;
    mVencId = -1;
}

SampleRecorderSegmentContext::~SampleRecorderSegmentContext()
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

status_t SampleRecorderSegmentContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_RecorderSegment.conf\n";
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
status_t SampleRecorderSegmentContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mFrameRate = 60;
        mConfigPara.mDispWidth = 480;
        mConfigPara.mDispHeight = 640;
        mConfigPara.mEncodeType = PT_H264;
        mConfigPara.mAudioEncodeType = PT_MAX;
        mConfigPara.mEncodeWidth = 1920;
        mConfigPara.mEncodeHeight = 1080;
        mConfigPara.mEncodeBitrate = 14*1024*1024;
        mConfigPara.mFileDurationPolicy = RecordFileDurationPolicy_AverageDuration;
        mConfigPara.mSegmentFolderPath = "/home/sample_RecorderSegment_Files";
        mConfigPara.mSegmentCount = 3;
        mConfigPara.mSegmentMethod = SegmentByTime;
        mConfigPara.mSegmentDuration = 60;
        mConfigPara.mSegmentSize = 100*1024*1024;
        mConfigPara.mJpegWidth = 1920;
        mConfigPara.mJpegHeight = 1080;
        mConfigPara.mJpegThumbWidth = 320;
        mConfigPara.mJpegThumbHeight = 180;
        mConfigPara.mJpegNum = 0;
        mConfigPara.mJpegInterval = 500;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_CAPTURE_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_PIC_FORMAT, NULL);
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
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_FRAME_RATE, 0);
    mConfigPara.mDispWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_DISP_WIDTH, 0);
    mConfigPara.mDispHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_DISP_HEIGHT, 0);
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_ENCODE_TYPE, NULL); 
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
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_AUDIO_ENCODE_TYPE, NULL);
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
    mConfigPara.mEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_ENCODE_WIDTH, 0);
    mConfigPara.mEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_ENCODE_HEIGHT, 0);
    mConfigPara.mEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_ENCODE_BITRATE, 0)*1024*1024;
    mConfigPara.mFileDurationPolicy = static_cast<RecordFileDurationPolicy>(GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_FILE_DURATION_POLICY, 0));

    mConfigPara.mSegmentFolderPath = GetConfParaString(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_SEGMENT_FOLDER, NULL);
    mConfigPara.mSegmentCount = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_SEGMENT_COUNT, 0);
    char *pStrSegmentMethod = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_SEGMENT_METHOD, NULL);
    if(!strcmp(pStrSegmentMethod, "time"))
    {
        mConfigPara.mSegmentMethod = SegmentByTime;
    }
    else if(!strcmp(pStrSegmentMethod, "size"))
    {
        mConfigPara.mSegmentMethod = SegmentBySize;
    }
    else
    {
        aloge("fatal error! conf file segment method is [%s]?", pStrSegmentMethod);
        mConfigPara.mSegmentMethod = SegmentByTime;
    }
    mConfigPara.mSegmentDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_SEGMENT_DURATION, 0);
    mConfigPara.mSegmentSize = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_SEGMENT_SIZE, 0)*1024*1024;

    mConfigPara.mJpegWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_JPEG_WIDTH, 0);
    mConfigPara.mJpegHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_JPEG_HEIGHT, 0);
    mConfigPara.mJpegThumbWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_JPEG_THUMB_WIDTH, 0);
    mConfigPara.mJpegThumbHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_JPEG_THUMB_HEIGHT, 0);
    mConfigPara.mJpegNum = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_JPEG_NUM, 0);
    mConfigPara.mJpegInterval = GetConfParaInt(&stConfParser, SAMPLE_RECORDERSEGMENT_KEY_JPEG_INTERVAL, 0);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleRecorderSegmentContext::CreateFolder(const std::string& strFolderPath)
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

std::string SampleRecorderSegmentContext::MakeSegmentFileName()
{
    char fileName[64];
    sprintf(fileName, "File%s[%04d].mp4", mConfigPara.mSegmentMethod==SegmentByTime?"ByTime":"BySize", mFileNum++);
    return std::string(fileName);
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_RecorderSegment!"<<endl;
    SampleRecorderSegmentContext *pContext = new SampleRecorderSegmentContext;
    pSampleRecorderSegmentContext = pContext;
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
    if(SUCCESS != pContext->CreateFolder(pContext->mConfigPara.mSegmentFolderPath))
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

    //config camera.
    int cameraId = 0;
    {
        EyeseeCamera::clearCamerasConfiguration();

        CameraInfo cameraInfo;
        cameraInfo.facing                 = CAMERA_FACING_BACK;
        cameraInfo.orientation            = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType      = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn   = 0;  //摄像机硬件CSI接口编号，取决于物理连接

        ISPGeometry mISPGeometry;
        mISPGeometry.mISPDev = 0;
        mISPGeometry.mScalerOutChns.push_back(HVIDEO(0, 0));
        mISPGeometry.mScalerOutChns.push_back(HVIDEO(1, 0));
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);

        mISPGeometry.mScalerOutChns.clear();
        mISPGeometry.mISPDev = 1;
        mISPGeometry.mScalerOutChns.push_back(HVIDEO(0, 1));
        mISPGeometry.mScalerOutChns.push_back(HVIDEO(1, 1));
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);

        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    CameraInfo& cameraInfo = pContext->mCameraInfo;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    pContext->mpCamera = EyeseeCamera::open(cameraId);
    pContext->mpCamera->setInfoCallback(&pContext->mCameraListener);
    pContext->mpCamera->prepareDevice();
    pContext->mpCamera->startDevice();

    pContext->mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], true);
    CameraParameters cameraParam;
    pContext->mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    SIZE_S captureSize;
    captureSize.Width = pContext->mConfigPara.mCaptureWidth;
    captureSize.Height = pContext->mConfigPara.mCaptureHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(pContext->mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(pContext->mConfigPara.mPicFormat);
    cameraParam.setVideoBufferNumber(5);
    pContext->mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    pContext->mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

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
    alogd("prepare setPreviewDisplay(), hlay=%d", pContext->mVoLayer);
    pContext->mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], pContext->mVoLayer);
    pContext->mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
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
    status_t eRet;
    //record first file.
    pContext->mpRecorder = new EyeseeRecorder();
    pContext->mpRecorder->setOnInfoListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnDataListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnErrorListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setCameraProxy(pContext->mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    pContext->mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    pContext->mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    SinkParam stSinkParam;
    memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
    stSinkParam.mOutputFd = -1;
    std::string strSegmentFilePath = pContext->mConfigPara.mSegmentFolderPath + '/' + pContext->MakeSegmentFileName();
    stSinkParam.mOutputPath = (char*)strSegmentFilePath.c_str();
    stSinkParam.mFallocateLen = 0;
    stSinkParam.bCallbackOutFlag = false;
    stSinkParam.bBufFromCacheFlag = false;
    pContext->mpRecorder->addOutputSink(&stSinkParam);
    pContext->mSegmentFiles.push_back(strSegmentFilePath);

    VencParameters stVencParam;
    alogd("setVideoFrameRate=[%d]", pContext->mConfigPara.mFrameRate);
    stVencParam.setVideoFrameRate(pContext->mConfigPara.mFrameRate);
    alogd("setEncodeVideoSize=[%dx%d]", pContext->mConfigPara.mEncodeWidth, pContext->mConfigPara.mEncodeHeight);
    SIZE_S stVideoSize;
    stVideoSize.Width = pContext->mConfigPara.mEncodeWidth;
    stVideoSize.Height = pContext->mConfigPara.mEncodeHeight;
    stVencParam.setVideoSize(stVideoSize);
    VencParameters::VEncAttr stVEncAttr;
    memset(&stVEncAttr, 0, sizeof(VencParameters::VEncAttr));
    stVEncAttr.mType = pContext->mConfigPara.mEncodeType;
    if(PT_H264 == stVEncAttr.mType)
    {
        stVEncAttr.mAttrH264.mProfile = 1;
        stVEncAttr.mAttrH264.mLevel = H264_LEVEL_51;
    }
    else if(PT_H265 == stVEncAttr.mType)
    {
        stVEncAttr.mAttrH265.mProfile = 0;
        stVEncAttr.mAttrH265.mLevel = H265_LEVEL_62;
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
    //stVencParam.setVideoEncodingRateControlMode(VencParameters::VideoRCMode_CBR);
    VencParameters::VEncBitRateControlAttr stRcAttr;
    stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType;
    stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
    switch(pContext->mConfigPara.mEncodeType)
    {
        case PT_H264:
        {
            stRcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate;
            stRcAttr.mAttrH264Cbr.mMaxQp = 51;
            stRcAttr.mAttrH264Cbr.mMinQp = 1;
            break;
        }
        case PT_H265:
        {
            stRcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate;
            stRcAttr.mAttrH265Cbr.mMaxQp = 51;
            stRcAttr.mAttrH265Cbr.mMinQp = 1;
            break;
        }
        case PT_MJPEG:
        {
            stRcAttr.mAttrMjpegCbr.mBitRate = pContext->mConfigPara.mEncodeBitrate;
            break;
        }
        default:
        {
            aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType);
            break;
        }
    }
    stVencParam.setVEncBitRateControlAttr(stRcAttr);
    pContext->mVencId = 0;
    pContext->mpRecorder->setVencParameters(pContext->mVencId, &stVencParam);
    pContext->mpRecorder->bindVeVipp(pContext->mVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    alogd("setSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    if(pContext->mConfigPara.mAudioEncodeType != PT_MAX)
    {
        pContext->mpRecorder->setAudioSamplingRate(8000);
        pContext->mpRecorder->setAudioChannels(1);
        pContext->mpRecorder->setAudioEncodingBitRate(12200);
        pContext->mpRecorder->setAudioEncoder(PT_AAC);
    }
    if(SegmentByTime == pContext->mConfigPara.mSegmentMethod)
    {
        pContext->mpRecorder->setMaxDuration(pContext->mConfigPara.mSegmentDuration*1000);
    }
    else
    {
        pContext->mpRecorder->setMaxFileSize(pContext->mConfigPara.mSegmentSize);
    }
    alogd("prepare()!");
    pContext->mpRecorder->prepare();
    if(SegmentByTime == pContext->mConfigPara.mSegmentMethod)
    {
        eRet = pContext->mpRecorder->setSwitchFileDurationPolicy(pContext->mMuxerId, pContext->mConfigPara.mFileDurationPolicy);
        if(eRet != NO_ERROR)
        {
            aloge("fatal error! set file duration policy fail");
        }
    }
    
    alogd("start()!");
    pContext->mpRecorder->start();

    if(pContext->mConfigPara.mJpegNum > 0)
    {
        sleep(5);
        pContext->mpCamera->getParameters(pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
        cameraParam.setPictureMode(TAKE_PICTURE_MODE_CONTINUOUS);
        cameraParam.setContinuousPictureNumber(pContext->mConfigPara.mJpegNum);
        cameraParam.setContinuousPictureIntervalMs(pContext->mConfigPara.mJpegInterval);
        cameraParam.setPictureSize(SIZE_S{(unsigned int)pContext->mConfigPara.mJpegWidth, (unsigned int)pContext->mConfigPara.mJpegHeight});
        cameraParam.setJpegThumbnailSize(SIZE_S{(unsigned int)pContext->mConfigPara.mJpegThumbWidth, (unsigned int)pContext->mConfigPara.mJpegThumbHeight});
        pContext->mpCamera->setParameters(pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
        pContext->mpCamera->takePicture(pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], NULL, NULL, &pContext->mCameraListener);
    }
    cdx_sem_down(&pContext->mSemExit);

    alogd("record done! stop()!");
    //stop mHMR0
    pContext->mpRecorder->stop();

    delete pContext->mpRecorder;
    pContext->mpRecorder = NULL;

    //close camera
    alogd("HerbCamera::release()");
    alogd("HerbCamera stopPreview()");
    pContext->mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    pContext->mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    pContext->mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
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
    cout<<"bye, sample_RecorderSegment!"<<endl;
    return result;
}
