//#define LOG_NDEBUG 0
#define LOG_TAG "sample_RecordSwitchFileNormal"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>

#include <hwdisplay.h>
#include <record_writer.h>
#include <confparser.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

#include "sample_RecordSwitchFileNormal_config.h"
#include "sample_RecordSwitchFileNormal.h"

using namespace std;
using namespace EyeseeLinux;

SampleRecordSwitchFileNormalContext *pSampleRecordSwitchFileNormalContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if (pSampleRecordSwitchFileNormalContext!=NULL)
    {
        cdx_sem_up(&pSampleRecordSwitchFileNormalContext->mSemExit);
    }
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
    aloge("fatal error! Do not test CallbackOut!");
}

void EyeseeRecorderListener::onError(EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d], recorderId[%d]", what, extra, pMr->mRecorderId);
    switch (what)
    {
        case EyeseeRecorder::MEDIA_ERROR_SERVER_DIED:
            break;
        default:
            break;
    }
}

void EyeseeRecorderListener::onInfo(EyeseeRecorder *pMr, int what, int extra)
{
    switch (what)
    {
        case EyeseeRecorder::MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD:
        {
            alogd("receive onInfo message: need_set_next_fd, muxer_id[%d]", extra);
            int nMuxerId = extra;
            if (mpOwner->mpMainRecorder != pMr)
            {
                aloge("fatal error! why is not main recorder[%p]!=[%p]?", mpOwner->mpMainRecorder, pMr);
            }
            std::string strSegmentFilePath = mpOwner->mConfigPara.mSegmentFolderPath + '/' + mpOwner->MakeSegmentFileName(true);
            mpOwner->mpMainRecorder->setOutputFileSync((char*)strSegmentFilePath.c_str(), 0, nMuxerId);
            alogd("add file[%s] success", strSegmentFilePath.c_str());
            mpOwner->mMainSegmentFiles.push_back(strSegmentFilePath);
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done, muxer_id[%d]", extra);
            int nMuxerId = extra;
            if (mpOwner->mpMainRecorder == pMr)
            {
                //let subRecorder switch file normal!
                std::string strSegmentFilePath = mpOwner->mConfigPara.mSegmentFolderPath + '/' + mpOwner->MakeSegmentFileName(false);
                int ret = mpOwner->mpSubRecorder->switchFileNormal((char*)strSegmentFilePath.c_str(), 0, mpOwner->mSubMuxerId);
                if (NO_ERROR == ret)
                {
                    AutoMutex autoLock(mpOwner->mFilesLock);
                    alogd("add file[%s] success", strSegmentFilePath.c_str());
                    mpOwner->mSubSegmentFiles.push_back(strSegmentFilePath);
                }
                else
                {
                    aloge("fatal error! why switch sub file normal fail?[0x%x]", ret);
                }
                while (mpOwner->mMainSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
                {
                    if ((ret = remove(mpOwner->mMainSegmentFiles[0].c_str())) < 0)
                    {
                        aloge("fatal error! delete file[%s] failed:%s", mpOwner->mMainSegmentFiles[0].c_str(), strerror(errno));
                    }
                    else
                    {
                        alogd("delete file[%s] success, ret[0x%x]", mpOwner->mMainSegmentFiles[0].c_str(), ret);
                    }
                    mpOwner->mMainSegmentFiles.pop_front();
                }
            }
            else if (mpOwner->mpSubRecorder == pMr)
            {
                int ret;
                AutoMutex autoLock(mpOwner->mFilesLock);
                while (mpOwner->mSubSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
                {
                    if ((ret = remove(mpOwner->mSubSegmentFiles[0].c_str())) < 0)
                    {
                        aloge("fatal error! delete file[%s] failed:%s", mpOwner->mSubSegmentFiles[0].c_str(), strerror(errno));
                    }
                    else
                    {
                        alogd("delete file[%s] success, ret[0x%x]", mpOwner->mSubSegmentFiles[0].c_str(), ret);
                    }
                    mpOwner->mSubSegmentFiles.pop_front();
                }
            }
            else
            {
                aloge("fatal error! pMr[%p] is wrong!", pMr);
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

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    aloge("fatal error! Do not test CallbackOut!");
}

EyeseeRecorderListener::EyeseeRecorderListener(SampleRecordSwitchFileNormalContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleRecordSwitchFileNormalContext::SampleRecordSwitchFileNormalContext()
    :mMainRecorderListener(this)
    ,mSubRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera = NULL;
    mpMainRecorder = NULL;
    mpSubRecorder = NULL;
    mMainFileNum = 0;
    mSubFileNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mMainMuxerId = -1;
    mMainVencId = -1;
    mSubMuxerId = -1;
    mSubVencId = -1;
}

SampleRecordSwitchFileNormalContext::~SampleRecordSwitchFileNormalContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
    if(mpMainRecorder!=NULL)
    {
        aloge("fatal error! main EyeseeRecorder is not destruct!");
    }
    if(mpSubRecorder!=NULL)
    {
        aloge("fatal error! sub EyeseeRecorder is not destruct!");
    }
}

status_t SampleRecordSwitchFileNormalContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_RecordSwitchFileNormal.conf\n";
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

status_t SampleRecordSwitchFileNormalContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mMainVippWidth = 1920;
        mConfigPara.mMainVippHeight = 1080;
        mConfigPara.mSubVippWidth = 640;
        mConfigPara.mSubVippHeight = 360;
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mFrameRate = 30;
        mConfigPara.mEncodeType = PT_H264;
        mConfigPara.mAudioEncodeType = PT_MAX;
        mConfigPara.mMainEncodeWidth = 1920;
        mConfigPara.mMainEncodeHeight = 1080;
        mConfigPara.mMainEncodeBitrate = 6*1024*1024;
        mConfigPara.mSubEncodeWidth = 640;
        mConfigPara.mSubEncodeHeight = 360;
        mConfigPara.mSubEncodeBitrate = 2*1024*1024;
        mConfigPara.mSegmentFolderPath = "/home/sample_RecordSwitchFileNormal_Files";
        mConfigPara.mSegmentCount = 3;
        mConfigPara.mSegmentMethod = SegmentByTime;
        mConfigPara.mSegmentDuration = 60;
        mConfigPara.mSegmentSize = 100*1024*1024;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mMainVippWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_VIPP_WIDTH, 0);
    mConfigPara.mMainVippHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_VIPP_HEIGHT, 0);
    mConfigPara.mSubVippWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_VIPP_WIDTH, 0);
    mConfigPara.mSubVippHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_VIPP_HEIGHT, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_PIC_FORMAT, NULL);
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
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_FRAME_RATE, 0);
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_ENCODE_TYPE, NULL);
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
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_AUDIO_ENCODE_TYPE, NULL);
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
    mConfigPara.mMainEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_WIDTH, 0);
    mConfigPara.mMainEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_HEIGHT, 0);
    mConfigPara.mMainEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_BITRATE, 0)*1024*1024;
    mConfigPara.mSubEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_ENCODE_WIDTH, 0);
    mConfigPara.mSubEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_ENCODE_HEIGHT, 0);
    mConfigPara.mSubEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_ENCODE_BITRATE, 0)*1024*1024;

    mConfigPara.mSegmentFolderPath = GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_FOLDER, NULL);
    mConfigPara.mSegmentCount = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_COUNT, 0);
    char *pStrSegmentMethod = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_METHOD, NULL);
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
    mConfigPara.mSegmentDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_DURATION, 0);
    mConfigPara.mSegmentSize = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_SIZE, 0)*1024*1024;

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleRecordSwitchFileNormalContext::CreateFolder(const std::string& strFolderPath)
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

std::string SampleRecordSwitchFileNormalContext::MakeSegmentFileName(bool bMainFile)
{
    char fileName[64];
    if (bMainFile)
    {
        sprintf(fileName, "File%s[%04d]_main.mp4", mConfigPara.mSegmentMethod==SegmentByTime?"ByTime":"BySize", mMainFileNum++);
    }
    else
    {
        sprintf(fileName, "File%s[%04d]_sub.mp4", mConfigPara.mSegmentMethod==SegmentByTime?"ByTime":"BySize", mSubFileNum++);
    }
    return std::string(fileName);
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_RecordSwitchFileNormal!"<<endl;
    SampleRecordSwitchFileNormalContext stContext;
    pSampleRecordSwitchFileNormalContext = &stContext;  //global var

    // parse command line param
    if (stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }

    // parse config file.
    if (stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }

    // check folder existence, create folder if necessary
    if (SUCCESS != stContext.CreateFolder(stContext.mConfigPara.mSegmentFolderPath))
    {
        aloge("fatal error! Create output path %s fail", stContext.mConfigPara.mSegmentFolderPath);
        return -1;
    }

    // register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    // init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //config camera.
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        ISPGeometry mISPGeometry;
        VI_CHN chn;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 1;
        mISPGeometry.mISPDev = 0;
        mISPGeometry.mScalerOutChns.push_back(0);
        mISPGeometry.mScalerOutChns.push_back(1);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    int cameraId;
    CameraInfo& cameraInfo = stContext.mCameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
    stContext.mpCamera->prepareDevice();
    stContext.mpCamera->startDevice();

    stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], true);
    CameraParameters cameraParam;
    stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    SIZE_S captureSize;
    captureSize.Width = stContext.mConfigPara.mMainVippWidth;
    captureSize.Height = stContext.mConfigPara.mMainVippHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
    cameraParam.setVideoBufferNumber(7);
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], false);
    stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    captureSize.Width = stContext.mConfigPara.mSubVippWidth;
    captureSize.Height = stContext.mConfigPara.mSubVippHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
    cameraParam.setVideoBufferNumber(7);
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);

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
    stContext.mLayerAttr.stDispRect.X      = 0;
    stContext.mLayerAttr.stDispRect.Y      = 0;
    stContext.mLayerAttr.stDispRect.Width  = 480;
    stContext.mLayerAttr.stDispRect.Height = 640;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
    stContext.mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], stContext.mVoLayer);

    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    int ret = cdx_sem_down_timedwait(&stContext.mSemRenderStart, 5000);
    if (0 == ret)
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

    // prepare to record main file.
    stContext.mpMainRecorder = new EyeseeRecorder();
    stContext.mpMainRecorder->setOnInfoListener(&stContext.mMainRecorderListener);
    stContext.mpMainRecorder->setOnDataListener(&stContext.mMainRecorderListener);
    stContext.mpMainRecorder->setOnErrorListener(&stContext.mMainRecorderListener);
    stContext.mpMainRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    stContext.mpMainRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);

    SinkParam stSinkParam;
    memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
    stSinkParam.mOutputFd = -1;
    std::string strSegmentFilePath = stContext.mConfigPara.mSegmentFolderPath + '/' + stContext.MakeSegmentFileName(true);
    stSinkParam.mOutputPath = (char*)strSegmentFilePath.c_str();
    stSinkParam.mFallocateLen = 0;
    stSinkParam.mMaxDurationMs = 0;
    stSinkParam.bCallbackOutFlag  = false;
    stSinkParam.bBufFromCacheFlag = false;
    stContext.mpMainRecorder->addOutputSink(&stSinkParam);
    stContext.mMainSegmentFiles.push_back(strSegmentFilePath);
    VencParameters stVencParam;
    alogd("setVideoFrameRate=[%d]", stContext.mConfigPara.mFrameRate);
    stVencParam.setVideoFrameRate(stContext.mConfigPara.mFrameRate);
    alogd("setEncodeVideoSize=[%dx%d]", stContext.mConfigPara.mMainEncodeWidth, stContext.mConfigPara.mMainEncodeHeight);
    SIZE_S stVideoSize;
    stVideoSize.Width = stContext.mConfigPara.mMainEncodeWidth;
    stVideoSize.Height = stContext.mConfigPara.mMainEncodeHeight;
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
            stRcAttr.mAttrH264Cbr.mBitRate = stContext.mConfigPara.mMainEncodeBitrate;
            stRcAttr.mAttrH264Cbr.mMaxQp = 51;
            stRcAttr.mAttrH264Cbr.mMinQp = 1;
            break;
        }
        case PT_H265:
        {
            stRcAttr.mAttrH265Cbr.mBitRate = stContext.mConfigPara.mMainEncodeBitrate;
            stRcAttr.mAttrH265Cbr.mMaxQp = 51;
            stRcAttr.mAttrH265Cbr.mMinQp = 1;
            break;
        }
        case PT_MJPEG:
        {
            stRcAttr.mAttrMjpegCbr.mBitRate = stContext.mConfigPara.mMainEncodeBitrate;
            break;
        }
        default:
        {
            aloge("fatal error! unsupport vencType[0x%x]", stContext.mConfigPara.mEncodeType);
            break;
        }
    }
    stVencParam.setVEncBitRateControlAttr(stRcAttr);
    alogd("CameraSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpMainRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mMainVencId = 0;
    stContext.mpMainRecorder->setVencParameters(stContext.mMainVencId, &stVencParam);
    stContext.mpMainRecorder->bindVeVipp(stContext.mMainVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    if (stContext.mConfigPara.mAudioEncodeType != PT_MAX)
    {
        stContext.mpMainRecorder->setAudioSamplingRate(8000);
        stContext.mpMainRecorder->setAudioChannels(1);
        stContext.mpMainRecorder->setAudioEncodingBitRate(12200);
        stContext.mpMainRecorder->setAudioEncoder(PT_AAC);
    }
    if (SegmentByTime == stContext.mConfigPara.mSegmentMethod)
    {
        stContext.mpMainRecorder->setMaxDuration(stContext.mConfigPara.mSegmentDuration*1000);
    }
    else
    {
        stContext.mpMainRecorder->setMaxFileSize(stContext.mConfigPara.mSegmentSize);
    }
    alogd("prepare()!");
    stContext.mpMainRecorder->prepare();

    //prepare to record sub file.
    stContext.mpSubRecorder = new EyeseeRecorder();
    stContext.mpSubRecorder->setOnInfoListener(&stContext.mSubRecorderListener);
    stContext.mpSubRecorder->setOnDataListener(&stContext.mSubRecorderListener);
    stContext.mpSubRecorder->setOnErrorListener(&stContext.mSubRecorderListener);
    stContext.mpSubRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    stContext.mpSubRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
    stSinkParam.mOutputFd = -1;
    strSegmentFilePath = stContext.mConfigPara.mSegmentFolderPath + '/' + stContext.MakeSegmentFileName(false);
    stSinkParam.mOutputPath = (char*)strSegmentFilePath.c_str();
    stSinkParam.mFallocateLen = 0;
    stSinkParam.mMaxDurationMs = 0;
    stSinkParam.bCallbackOutFlag  = false;
    stSinkParam.bBufFromCacheFlag = false;
    stContext.mpSubRecorder->addOutputSink(&stSinkParam);
    stContext.mSubSegmentFiles.push_back(strSegmentFilePath);
    alogd("setVideoFrameRate=[%d]", stContext.mConfigPara.mFrameRate);
    stVencParam.setVideoFrameRate(stContext.mConfigPara.mFrameRate);
    alogd("setEncodeVideoSize=[%dx%d]", stContext.mConfigPara.mMainEncodeWidth, stContext.mConfigPara.mMainEncodeHeight);
    stVideoSize.Width = stContext.mConfigPara.mSubEncodeWidth;
    stVideoSize.Height = stContext.mConfigPara.mSubEncodeHeight;
    stVencParam.setVideoSize(stVideoSize);
    stVencParam.setVideoEncoder(stContext.mConfigPara.mEncodeType);
    //stVencParam.setVideoEncodingRateControlMode(VencParameters::VideoRCMode_CBR);
    stRcAttr.mVEncType = stContext.mConfigPara.mEncodeType;
    stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
    switch(stContext.mConfigPara.mEncodeType)
    {
        case PT_H264:
        {
            stRcAttr.mAttrH264Cbr.mBitRate = stContext.mConfigPara.mSubEncodeBitrate;
            stRcAttr.mAttrH264Cbr.mMaxQp = 51;
            stRcAttr.mAttrH264Cbr.mMinQp = 1;
            break;
        }
        case PT_H265:
        {
            stRcAttr.mAttrH265Cbr.mBitRate = stContext.mConfigPara.mSubEncodeBitrate;
            stRcAttr.mAttrH265Cbr.mMaxQp = 51;
            stRcAttr.mAttrH265Cbr.mMinQp = 1;
            break;
        }
        case PT_MJPEG:
        {
            stRcAttr.mAttrMjpegCbr.mBitRate = stContext.mConfigPara.mSubEncodeBitrate;
            break;
        }
        default:
        {
            aloge("fatal error! unsupport vencType[0x%x]", stContext.mConfigPara.mEncodeType);
            break;
        }
    }
    stVencParam.setVEncBitRateControlAttr(stRcAttr);
    alogd("CameraSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpSubRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mSubVencId = 1;
    stContext.mpSubRecorder->setVencParameters(stContext.mSubVencId, &stVencParam);
    stContext.mpSubRecorder->bindVeVipp(stContext.mSubVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    if (stContext.mConfigPara.mAudioEncodeType != PT_MAX)
    {
        stContext.mpSubRecorder->setAudioSamplingRate(8000);
        stContext.mpSubRecorder->setAudioChannels(1);
        stContext.mpSubRecorder->setAudioEncodingBitRate(12200);
        stContext.mpSubRecorder->setAudioEncoder(PT_AAC);
    }
    alogd("prepare()!");
    stContext.mpSubRecorder->prepare();

    alogd("start()!");
    stContext.mpMainRecorder->start();
    stContext.mpSubRecorder->start();

    cdx_sem_down(&stContext.mSemExit);
    alogd("record done! stop()!");

    //stop recorder
    stContext.mpMainRecorder->stop();
    stContext.mpSubRecorder->stop();

    delete stContext.mpMainRecorder;
    stContext.mpMainRecorder = NULL;
    delete stContext.mpSubRecorder;
    stContext.mpSubRecorder = NULL;

    // close camera
    alogd("EyeseeCamera::release()");
    alogd("EyeseeCamera stopPreview()");
    stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->stopDevice();
    stContext.mpCamera->releaseDevice();
    EyeseeCamera::close(stContext.mpCamera);
    stContext.mpCamera = NULL;

    // close vo
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;

    // exit mpp system
    AW_MPI_SYS_Exit();

    cout<<"bye, sample_RecordSwitchFileNormal!"<<endl;
    return result;
}
