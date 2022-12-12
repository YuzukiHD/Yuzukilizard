#define LOG_TAG "sample_RecordMultiStream"
#include <utils/plat_log.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include <media/mm_comm_vi.h>
#include <media/mpi_vi.h>
#include <vo/hwdisplay.h>
#include <log/log_wrapper.h>

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>

#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>
#include <SystemBase.h>

#include "sample_RecordMultiStream_config.h"
#include "sample_RecordMultiStream.h"

using namespace std;
using namespace EyeseeLinux;

SampleRecordMultiStreamContext *pSampleRecordMultiStream = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if (pSampleRecordMultiStream != NULL)
    {
        cdx_sem_up(&pSampleRecordMultiStream->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch (info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            int nVipp1 = pSampleRecordMultiStream->mConfigPara.mVipp1;
            if (chnId == mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1])
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

void EyeseeCameraListener::onPictureTaken(int chnId, const void * data, int size, EyeseeCamera *pCamera)
{
    aloge("fatal error! channel %d picture data size %d, not implement in this sample.", chnId, size);
}

EyeseeCameraListener::EyeseeCameraListener(SampleRecordMultiStreamContext *pContext)
    : mpContext(pContext)
{
}

void EyeseeRecorderListener::onError(EyeseeLinux :: EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);
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
            int nMuxerId = extra;
            alogd("receive onInfo message: need_set_next_fd, muxer_id[%d], segmentCount[%d]", nMuxerId, mpOwner->mConfigPara.mSegmentCount);
            if(mpOwner->mMuxerId != nMuxerId)
            {
                aloge("fatal error! why muxerId is not match[%d]!=[%d]?", mpOwner->mMuxerId, nMuxerId);
            }
            std::string strSegmentFilePath = mpOwner->mConfigPara.mEncodeFolderPath + '/' + mpOwner->MakeSegmentFileName();
            mpOwner->mpRecorder->setOutputFileSync((char*)strSegmentFilePath.c_str(), 0, nMuxerId);
            mpOwner->mSegmentFiles.push_back(strSegmentFilePath);
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            int nMuxerId = extra;
            alogd("receive onInfo message: record_file_done, muxer_id[%d], segmentCount[%d]", nMuxerId, mpOwner->mConfigPara.mSegmentCount);
            if(mpOwner->mConfigPara.mSegmentCount > 0)
            {
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

EyeseeRecorderListener::EyeseeRecorderListener(SampleRecordMultiStreamContext *pOwner)
    : mpOwner(pOwner)
{
}

SampleRecordMultiStreamContext::SampleRecordMultiStreamContext()
    :mCameraListener(this)
    ,mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera = NULL;
    mpRecorder = NULL;
    mFileNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mMuxerId = -1;
    mVencId0 = -1;
    mVencId1 = -1;
    mVipp0 = -1;
    mVipp1 = -1;
}

SampleRecordMultiStreamContext::~SampleRecordMultiStreamContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if (mpCamera != NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
    if (mpRecorder != NULL)
    {
        aloge("fatal error! EyeseeRecorder is not destruct!");
    }
}

status_t SampleRecordMultiStreamContext::ParseCmdLine(int argc, char *argv[])
{
    alogd("this program path:[%s], arg number is [%d]", argv[0], argc);
    status_t ret = NO_ERROR;
    int i = 1;
    while (i < argc)
    {
        if (!strcmp(argv[i], "-path"))
        {
            if (++i >= argc)
            {
                std::string errorString;
                errorString = "fatal error! use -h to learn how to set parameters!!!";
                cout << errorString << endl;
                ret = -1;
                break;
            }
            mCmdLinePara.mConfigFilePath = argv[i];
        }
        else if (!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_RecordMultiStream.conf";
            cout << helpString << endl;
            ret = -1;
            break;
        }
        i++;
    }
    return ret;
}

status_t SampleRecordMultiStreamContext::loadConfig(int argc, char *argv[])
{
    int ret;
    std::string &ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if (ConfigFilePath.empty())
    {
        alogd("user not set config file, use default test parameters!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mViClock = 0;
        mConfigPara.mVeClock = 0;
        mConfigPara.mCaptureFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mPreviewWidth = 640;
        mConfigPara.mPreviewHeight = 360;
        mConfigPara.mPreviewFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mDigitalZoom = 0;
        mConfigPara.mDispWidth = 480;
        mConfigPara.mDispHeight = 640;
        mConfigPara.mbPreviewEnable = true;
        mConfigPara.mFrameRate = 60;

        //first venc
        mConfigPara.mVipp0 = 0;
        mConfigPara.mEncodeType_0 = PT_H264;
        mConfigPara.mDstFrameRate_0 = 30;
        mConfigPara.mEncodeWidth_0 = 1920;
        mConfigPara.mEncodeHeight_0 = 1080;
        mConfigPara.mIDRFrameInterval_0 = mConfigPara.mDstFrameRate_0;
        mConfigPara.mEncodeBitrate_0 = 10*1024*1024;
        mConfigPara.mOnlineEnable_0 = 0;
        mConfigPara.mOnlineShareBufNum_0 = 0;

        //second venc
        mConfigPara.mVipp1 = 1;
        mConfigPara.mEncodeType_1 = PT_H264;
        mConfigPara.mDstFrameRate_1 = 30;
        mConfigPara.mEncodeWidth_1 = 640;
        mConfigPara.mEncodeHeight_1 = 360;
        mConfigPara.mIDRFrameInterval_1 = mConfigPara.mDstFrameRate_1;
        mConfigPara.mEncodeBitrate_1 = 1*1024*1024;

        mConfigPara.mAudioEncodeType = PT_AAC;
        mConfigPara.mAudioSampleRate = 8000;
        mConfigPara.mAudioChannelNum = 1;
        mConfigPara.mAudioEncodeBitrate = 20480;

        mConfigPara.mSegmentCount = 0;
        mConfigPara.mSegmentDuration = 0;

        mConfigPara.mEncodeDuration = 60;
        mConfigPara.mEncodeFolderPath = "/mnt/extsd/sample_RecordMultiStream_Files";

        mConfigPara.mFileName = "file.mp4";

        return SUCCESS;
    }

    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if (ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_CAPTURE_HEIGHT, 0);
    mConfigPara.mViClock = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_VI_CLOCK, 0);
    mConfigPara.mVeClock = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_VE_CLOCK, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_CAPTURE_FORMAT, NULL);
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
    mConfigPara.mPreviewWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_PREVIEW_WIDTH, 0);
    mConfigPara.mPreviewHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_PREVIEW_HEIGHT, 0);
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_PREVIEW_FORMAT, NULL);
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
    mConfigPara.mDigitalZoom = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_DIGITAL_ZOOM, 0);
    mConfigPara.mDispWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_DISP_WIDTH, 0);
    mConfigPara.mDispHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_DISP_HEIGHT, 0);
    mConfigPara.mbPreviewEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_PREVIEW_ENABLE, 0);
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_FRAME_RATE, 0);

    //first venc
    mConfigPara.mVipp0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_VIPP_0, 0);
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_TYPE_0, NULL);
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mEncodeType_0 = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mEncodeType_0 = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mEncodeType_0 = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mEncodeType_0 = PT_H264;
    }
    mConfigPara.mDstFrameRate_0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_DST_FRAMERATE_0, 1);
    if (mConfigPara.mDstFrameRate_0 <= 0)
    {
        mConfigPara.mDstFrameRate_0 = mConfigPara.mFrameRate;
    }
    mConfigPara.mEncodeWidth_0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_WIDTH_0, 0);
    mConfigPara.mEncodeHeight_0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_HEIGHT_0, 0);
    mConfigPara.mIDRFrameInterval_0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_IDRFRAME_INTERVAL_0, 0);
    mConfigPara.mEncodeBitrate_0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_BITRATE_0, 0)*1024*1024;
    mConfigPara.mOnlineEnable_0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ONLINE_ENABLE_0, 0);
    mConfigPara.mOnlineShareBufNum_0 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ONLINE_SHARE_BUF_NUM_0, 0);
    alogd("online enable:%d, shareBufNum:%d", mConfigPara.mOnlineEnable_0, mConfigPara.mOnlineShareBufNum_0);

    //second venc
    mConfigPara.mVipp1 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_VIPP_1, 0);
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_TYPE_1, NULL);
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mEncodeType_1 = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mEncodeType_1 = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mEncodeType_1 = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mEncodeType_1 = PT_H264;
    }
    mConfigPara.mDstFrameRate_1 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_DST_FRAMERATE_1, 1);
    if (mConfigPara.mDstFrameRate_1 <= 0)
    {
        mConfigPara.mDstFrameRate_1 = mConfigPara.mFrameRate;
    }
    mConfigPara.mEncodeWidth_1 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_WIDTH_1, 0);
    mConfigPara.mEncodeHeight_1 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_HEIGHT_1, 0);
    mConfigPara.mIDRFrameInterval_1 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_IDRFRAME_INTERVAL_1, 0);
    mConfigPara.mEncodeBitrate_1 = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_BITRATE_1, 0)*1024*1024;

    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_AUDIO_ENCODE_TYPE, NULL);
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
    mConfigPara.mAudioSampleRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_AUDIO_SAMPLE_RATE, 0);
    mConfigPara.mAudioChannelNum = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_AUDIO_CHANNEL_NUM, 0);
    mConfigPara.mAudioEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_AUDIO_ENCODE_BITRATE, 0);

    mConfigPara.mSegmentCount = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_SEGMENT_COUNT, 0);
    mConfigPara.mSegmentDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_SEGMENT_DURATION, 0);
    if(0 == mConfigPara.mSegmentCount)
    {
        mConfigPara.mSegmentDuration = 0;
    }
    mConfigPara.mEncodeDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_DURATION, 0);
    mConfigPara.mEncodeFolderPath = GetConfParaString(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_ENCODE_FOLDER, NULL);

    mConfigPara.mFileName = GetConfParaString(&stConfParser, SAMPLE_RECORDMULTISTREAM_KEY_FILE_NAME, NULL);
    mConfigPara.mMuxCacheEnable = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_MUX_CACHE_ENABLE, 0);
    mConfigPara.mMuxCacheStrmIdsFilterEnable = GetConfParaInt(&stConfParser, SAMPLE_RECORDMULTISTREAM_MUX_CACHE_STRM_IDS_FILTER_ENABLE, 0);
    alogd("fileName[%s]]", mConfigPara.mFileName.c_str());
    destroyConfParser(&stConfParser);

    return ret;
}

status_t SampleRecordMultiStreamContext::CreateFolder(const std::string &strFoldrpath)
{
    if (strFoldrpath.empty())
    {
        aloge("file path is not set!");
        return UNKNOWN_ERROR;
    }
    const char *pFolderPath = strFoldrpath.c_str();

    //check folder existence
    struct stat sb;
    if (0 == stat(pFolderPath, &sb))
    {
        if (S_ISDIR(sb.st_mode))
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
    if (!ret)
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

std::string SampleRecordMultiStreamContext::MakeSegmentFileName()
{
    std::string strFileName = mConfigPara.mFileName;
    std::string::size_type n = strFileName.rfind('.');
    if(n == std::string::npos)
    {
        strFileName += "_" + std::to_string(mFileNum);
    }
    else
    {
        strFileName.insert(n, ("_" + std::to_string(mFileNum)));
    }
    mFileNum++;
    return strFileName;
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout << "hello, sample_RecordMultiStream!" << endl;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);

    SampleRecordMultiStreamContext *pContext = new SampleRecordMultiStreamContext;
    pSampleRecordMultiStream = pContext;

    //parse command line param
    if (pContext->ParseCmdLine(argc, argv) != NO_ERROR)
    {
        result = -1;
        return result;
    }

    //parse config file
    if (pContext->loadConfig(argc, argv) != SUCCESS)
    {
        aloge("fatal error! np config file or parse conf file fail");
        result = -1;
        return result;
    }
    pContext->mVencId0 = 0;
    pContext->mVencId1 = 1;
    //check folder existence, cerate folder if necessary
    if (pContext->CreateFolder(pContext->mConfigPara.mEncodeFolderPath))
    {
        return -1;
    }

    //register process function for SIGINT, to exit program
    if (SIG_ERR == signal(SIGINT, handle_exit))
    {
        perror("can't catch SIGSEGV");
    }

    //init mpp system
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();
    if (pContext->mConfigPara.mViClock != 0)
    {
        alogd("set vi clock [%d]MHz", pContext->mConfigPara.mViClock);
        AW_MPI_VI_SetVIFreq(0, pContext->mConfigPara.mViClock*1000000);
    }
    AW_MPI_VENC_SetVEFreq(MM_INVALID_CHN, pContext->mConfigPara.mVeClock);

    bool bOneCapVippflag;
    if (pContext->mConfigPara.mVipp0 == pContext->mConfigPara.mVipp1)
    {
        bOneCapVippflag = true;
        alogd("one capture Vipp, Vipp[%d]", pContext->mConfigPara.mVipp0);
    }
    else
    {
        bOneCapVippflag = false;
        alogd("two capture Vipp, Vipp[%d], Vipp[%d]", pContext->mConfigPara.mVipp0, pContext->mConfigPara.mVipp1);
    }

    if (pContext->mConfigPara.mOnlineEnable_0)
    {
        pContext->mVipp0 = 0;
    }
    else
    {
        pContext->mVipp0 = HVIDEO(pContext->mConfigPara.mVipp0, 0);
    }
    pContext->mVipp1 = HVIDEO(pContext->mConfigPara.mVipp1, 0);

    //config camera
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
        tmpISPGeometry.mScalerOutChns.push_back(pContext->mVipp0);
        tmpISPGeometry.mScalerOutChns.push_back(pContext->mVipp1);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(tmpISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    int cameraId;
    CameraInfo &cameraInfo = pContext->mCameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    pContext->mpCamera = EyeseeCamera::open(cameraId);
    pContext->mpCamera->setInfoCallback(&pContext->mCameraListener);
    pContext->mpCamera->prepareDevice();
    pContext->mpCamera->startDevice();

    alogd("config vipp %d", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
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
    //only vipp0 support online, and it bind to venc ch0.
    if (pContext->mConfigPara.mOnlineEnable_0)
    {
        cameraParam.setOnlineEnable(true);
        cameraParam.setOnlineShareBufNum(pContext->mConfigPara.mOnlineShareBufNum_0);
    }
    pContext->mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    pContext->mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    alogd("config vipp %d", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
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

    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);
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

    pContext->mpRecorder = new EyeseeRecorder();
    pContext->mpRecorder->setOnInfoListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnDataListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnErrorListener(&pContext->mRecorderListener);
    alogd("setSourceChannel=[%d]", pContext->mVipp0);
    pContext->mpRecorder->setCameraProxy(pContext->mpCamera->getRecordingProxy(), pContext->mVipp0);
    if (!bOneCapVippflag)
    {
        pContext->mpRecorder->setCameraProxy(pContext->mpCamera->getRecordingProxy(), pContext->mVipp1);
    }
    pContext->mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    pContext->mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    
    
    if(pContext->mConfigPara.mMuxCacheEnable)
    {
        int CacheDuration = 5*1000;
        pContext->mpRecorder->setMuxCacheDuration(CacheDuration);

        if(pContext->mConfigPara.mMuxCacheStrmIdsFilterEnable)
        {
            std::vector<int> mCacheStrmIdsList; 
            mCacheStrmIdsList.push_back(pContext->mVencId0);
            mCacheStrmIdsList.push_back(pContext->mVencId1 +1);
            pContext->mpRecorder->setMuxCacheStrmIds(mCacheStrmIdsList);
        }
    }


    std::string strFilePath = pContext->mConfigPara.mEncodeFolderPath + '/' + pContext->mConfigPara.mFileName;
    int nFallocateLen = 0;
    if(pContext->mConfigPara.mSegmentCount > 0)
    {
        nFallocateLen = (pContext->mConfigPara.mEncodeBitrate_0 + pContext->mConfigPara.mEncodeBitrate_1 + pContext->mConfigPara.mAudioEncodeBitrate)*pContext->mConfigPara.mSegmentDuration/8*6/5;
        alogd("fallocateLen=%d", nFallocateLen);
    }
    SinkParam stSinkParam;
    memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
    stSinkParam.mOutputFd = -1;
    stSinkParam.mOutputPath = (char*)strFilePath.c_str();
    stSinkParam.mFallocateLen = nFallocateLen;
    stSinkParam.mMaxDurationMs = pContext->mConfigPara.mSegmentDuration * 1000;
    stSinkParam.bCallbackOutFlag = false;
    stSinkParam.bBufFromCacheFlag = false;
    pContext->mMuxerId = pContext->mpRecorder->addOutputSink(&stSinkParam);
    pContext->mSegmentFiles.push_back(strFilePath);
    pContext->mFileNum = 1;

    //first stream
    {
        VencParameters stVencParam;

        VencParameters::VEncAttr stVEncAttr;
        memset(&stVEncAttr, 0, sizeof(VencParameters::VEncAttr));
        stVEncAttr.mType = pContext->mConfigPara.mEncodeType_0;
        if (PT_H264 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH264.mProfile = 1;
            stVEncAttr.mAttrH264.mLevel = H264_LEVEL_51;
        }
        else if (PT_H265 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH265.mProfile = 0;
            stVEncAttr.mAttrH265.mLevel = H265_LEVEL_62;
        }
        else if (PT_MJPEG == stVEncAttr.mType)
        {
        }
        else
        {
            aloge("fatal error! unsupport venctype[0x%x]", stVEncAttr.mType);
        }
        stVencParam.setVEncAttr(stVEncAttr);

        if (pContext->mConfigPara.mOnlineEnable_0)
        {
            stVencParam.setOnlineEnable(true);
            stVencParam.setOnlineShareBufNum(pContext->mConfigPara.mOnlineShareBufNum_0);
        }
        stVencParam.setVideoEncoder(pContext->mConfigPara.mEncodeType_0);
        stVencParam.setVideoFrameRate(pContext->mConfigPara.mDstFrameRate_0);
        SIZE_S stVideoSize;
        stVideoSize.Width = pContext->mConfigPara.mEncodeWidth_0;
        stVideoSize.Height = pContext->mConfigPara.mEncodeHeight_0;
        stVencParam.setVideoSize(stVideoSize);
        stVencParam.setVideoEncodingIFramesNumberInterVal(pContext->mConfigPara.mIDRFrameInterval_0);
        stVencParam.enableVideoEncodingPIntra(true);
        VencParameters::VEncBitRateControlAttr stRcAttr;
        stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType_0;
        stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
        switch (pContext->mConfigPara.mEncodeType_0)
        {
            case PT_H264:
            {
                stRcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate_0;
                stRcAttr.mAttrH264Cbr.mMaxQp = 50;
                stRcAttr.mAttrH264Cbr.mMinQp = 1;
                break;
            }
            case PT_H265:
            {
                stRcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate_0;
                stRcAttr.mAttrH265Cbr.mMaxQp = 50;
                stRcAttr.mAttrH265Cbr.mMinQp = 1;
                break;
            }
            case PT_JPEG:
            {
                stRcAttr.mAttrMjpegCbr.mBitRate = pContext->mConfigPara.mEncodeType_0;
                break;
            }
            default:
            {
                aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType_0);
                break;
            }
        }
        stVencParam.setVEncBitRateControlAttr(stRcAttr);

        VENC_GOP_ATTR_S GopAttr;
        GopAttr = stVencParam.getGopAttr();
        GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        stVencParam.setGopAttr(GopAttr);

        stVencParam.enableHorizonFlip(false);
        stVencParam.enable3DNR(0);
        stVencParam.enableColor2Grey(false);
        stVencParam.enableNullSkip(false);

        pContext->mpRecorder->setVencParameters(pContext->mVencId0, &stVencParam);
        pContext->mpRecorder->bindVeVipp(pContext->mVencId0, pContext->mVipp0);
    }

    //second stream
    {
        VencParameters stVencParam;

        VencParameters::VEncAttr stVEncAttr;
        memset(&stVEncAttr, 0, sizeof(VencParameters::VEncAttr));
        stVEncAttr.mType = pContext->mConfigPara.mEncodeType_1;
        if (PT_H264 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH264.mProfile = 1;
            stVEncAttr.mAttrH264.mLevel = H264_LEVEL_51;
        }
        else if (PT_H265 == stVEncAttr.mType)
        {
            stVEncAttr.mAttrH265.mProfile = 0;
            stVEncAttr.mAttrH265.mLevel = H265_LEVEL_62;
        }
        else if (PT_MJPEG == stVEncAttr.mType)
        {
        }
        else
        {
            aloge("fatal error! unsupport venctype[0x%x]", stVEncAttr.mType);
        }
        stVencParam.setVEncAttr(stVEncAttr);
        stVencParam.setVideoEncoder(pContext->mConfigPara.mEncodeType_1);
        stVencParam.setVideoFrameRate(pContext->mConfigPara.mDstFrameRate_1);
        SIZE_S stVideoSize;
        stVideoSize.Width = pContext->mConfigPara.mEncodeWidth_1;
        stVideoSize.Height = pContext->mConfigPara.mEncodeHeight_1;
        stVencParam.setVideoSize(stVideoSize);
        stVencParam.setVideoEncodingIFramesNumberInterVal(pContext->mConfigPara.mIDRFrameInterval_1);
        stVencParam.enableVideoEncodingPIntra(true);
        VencParameters::VEncBitRateControlAttr stRcAttr;
        stRcAttr.mVEncType = pContext->mConfigPara.mEncodeType_1;
        stRcAttr.mRcMode = VencParameters::VideoRCMode_CBR;
        switch (pContext->mConfigPara.mEncodeType_1)
        {
            case PT_H264:
            {
                stRcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate_1;
                stRcAttr.mAttrH264Cbr.mMaxQp = 50;
                stRcAttr.mAttrH264Cbr.mMinQp = 1;
                break;
            }
            case PT_H265:
            {
                stRcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mEncodeBitrate_1;
                stRcAttr.mAttrH265Cbr.mMaxQp = 50;
                stRcAttr.mAttrH265Cbr.mMinQp = 1;
                break;
            }
            case PT_JPEG:
            {
                stRcAttr.mAttrMjpegCbr.mBitRate = pContext->mConfigPara.mEncodeType_1;
                break;
            }
            default:
            {
                aloge("fatal error! unsupport vencType[0x%x]", pContext->mConfigPara.mEncodeType_1);
                break;
            }
        }
        stVencParam.setVEncBitRateControlAttr(stRcAttr);

        VENC_GOP_ATTR_S GopAttr;
        GopAttr = stVencParam.getGopAttr();
        GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        stVencParam.setGopAttr(GopAttr);

        stVencParam.enableHorizonFlip(false);
        stVencParam.enable3DNR(0);
        stVencParam.enableColor2Grey(false);
        stVencParam.enableNullSkip(false);

        pContext->mpRecorder->setVencParameters(pContext->mVencId1, &stVencParam);
        pContext->mpRecorder->bindVeVipp(pContext->mVencId1, pContext->mVipp1);
    }

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

    if(pContext->mConfigPara.mEncodeDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mEncodeDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    alogd("record done! stop()!");
    pContext->mpRecorder->stop();

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
    log_quit();
    cout << "bye, sample_RecordMultiStream!" << endl;
    return result;
}
