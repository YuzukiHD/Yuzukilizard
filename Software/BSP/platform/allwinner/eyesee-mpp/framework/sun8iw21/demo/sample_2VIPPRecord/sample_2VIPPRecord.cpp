//#define LOG_NDEBUG 0
#define LOG_TAG "sample_2VIPPRecord"
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

#include <mm_comm_vi.h>
#include <mpi_vi.h>
#include <mpi_venc.h>
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>

#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

#include "sample_2VIPPRecord_config.h"
#include "sample_2VIPPRecord.h"

using namespace std;
using namespace EyeseeLinux;

Sample2VIPPRecordContext *pSample2VIPPRecordContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSample2VIPPRecordContext!=NULL)
    {
        cdx_sem_up(&pSample2VIPPRecordContext->mSemExit);
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
    if(chnId != mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1])
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    }
    char picName[64];
    sprintf(picName, "pic[%04d]_thumb.jpg", mpContext->mPicNum++);
    std::string PicFullPath = mpContext->mConfigPara.mEncodeFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
    alogd("channel %d picture data size %d, save to [%s]", chnId, size, PicFullPath.c_str());
}

EyeseeCameraListener::EyeseeCameraListener(Sample2VIPPRecordContext *pContext)
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
            int nMuxerId = extra;
            if(mpOwner->mpMainRecorder == pMr)
            {
                std::string strSegmentFilePath = mpOwner->mConfigPara.mEncodeFolderPath + '/' + mpOwner->MakeSegmentFileName(true);
                int64_t nFallocateSize = 0;
                if(mpOwner->mConfigPara.mEncodeDuration > 0)
                {
                    nFallocateSize = (int64_t)mpOwner->mConfigPara.mMainEncodeBitrate*mpOwner->mConfigPara.mEncodeDuration/8 + 100*1024*1024;
                }
                mpOwner->mpMainRecorder->setOutputFileSync((char*)strSegmentFilePath.c_str(), nFallocateSize, nMuxerId);
                AutoMutex autoLock(mpOwner->mFileListLock);
                mpOwner->mMainSegmentFiles.push_back(strSegmentFilePath);
                alogd("main recorder receive onInfo message: need_set_next_fd, muxer_id[%d], set file[%s], fallocateSize[%lld]MB", nMuxerId, strSegmentFilePath.c_str(), nFallocateSize/(1024*1024));
            }
            else if(mpOwner->mpSubRecorder == pMr)
            {
                std::string strSegmentFilePath = mpOwner->mConfigPara.mEncodeFolderPath + '/' + mpOwner->MakeSegmentFileName(false);
                int64_t nFallocateSize = 0;
                if(mpOwner->mConfigPara.mEncodeDuration > 0)
                {
                    nFallocateSize = (int64_t)mpOwner->mConfigPara.mSubEncodeBitrate*mpOwner->mConfigPara.mEncodeDuration/8 + 100*1024*1024;
                }
                mpOwner->mpSubRecorder->setOutputFileSync((char*)strSegmentFilePath.c_str(), nFallocateSize, nMuxerId);
                AutoMutex autoLock(mpOwner->mFileListLock);
                mpOwner->mSubSegmentFiles.push_back(strSegmentFilePath);
                alogd("sub recorder receive onInfo message: need_set_next_fd, muxer_id[%d], set file[%s], fallocateSize[%lld]MB", nMuxerId, strSegmentFilePath.c_str(), nFallocateSize/(1024*1024));
            }
            else
            {
                aloge("fatal error! check pMr[%p], mainRecorder[%p], subRecorder[%p]", pMr, mpOwner->mpMainRecorder, mpOwner->mpSubRecorder);
            }
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done, muxer_id[%d]", extra);
            int nMuxerId = extra;
            int ret;
            AutoMutex autoLock(mpOwner->mFileListLock);
            while((int)mpOwner->mMainSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
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
            while((int)mpOwner->mSubSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
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
            break;
        }
        default:
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
    }
}

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    alogv("ignore callback data!");
    VEncBuffer *pEncBuf;
    while(1)
    {
        pEncBuf = pMr->getOneBsFrame();
        if(pEncBuf)
        {
            pMr->freeOneBsFrame(pEncBuf);
        }
        else
        {
            break;
        }
    }
}

EyeseeRecorderListener::EyeseeRecorderListener(Sample2VIPPRecordContext *pOwner)
    : mpOwner(pOwner)
{
}
Sample2VIPPRecordContext::Sample2VIPPRecordContext()
    :mCameraListener(this)
    ,mMainRecorderListener(this)
    ,mSubRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera = NULL;
    mpMainRecorder = NULL;
    mpSubRecorder = NULL;

    mbRecording = false;
    mMainFileNum = 0;
    mSubFileNum = 0;
    mPicNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
}

Sample2VIPPRecordContext::~Sample2VIPPRecordContext()
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

status_t Sample2VIPPRecordContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_2VIPPRecord.conf\n";
            helpString += "\t r:run to record; s:stop\n";
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

status_t Sample2VIPPRecordContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mVippMainChn = 0;
        mConfigPara.mVippMainWidth = 1920;
        mConfigPara.mVippMainHeight = 1080;
        mConfigPara.mVippMainPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mVippMainFrameRate = 30;
        mConfigPara.mVippMainDstFrameRate = mConfigPara.mVippMainFrameRate;
        mConfigPara.mVippMainBufferNum = 5;
        mConfigPara.mVippMainEncodeType = PT_H265;

        mConfigPara.mVippSubChn = 1;
        mConfigPara.mVippSubWidth = 640;
        mConfigPara.mVippSubHeight = 360;
        mConfigPara.mVippSubPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mVippSubFrameRate = 30;
        mConfigPara.mVippSubDstFrameRate = mConfigPara.mVippSubFrameRate;
        mConfigPara.mVippSubBufferNum = 5;
        mConfigPara.mVippSubRotation = 0;
        mConfigPara.mVippSubEncodeType = PT_H264;
        mConfigPara.mbVippSubEnableTakePic = false;

        mConfigPara.mAudioEncodeType = PT_AAC;

        mConfigPara.mMainEncodeWidth = 1920;
        mConfigPara.mMainEncodeHeight = 1080;
        mConfigPara.mMainEncodeBitrate = 6*1024*1024;

        mConfigPara.mSubEncodeWidth = 640;
        mConfigPara.mSubEncodeHeight = 360;
        mConfigPara.mSubEncodeBitrate = 1*1024*1024;
        
        mConfigPara.mSegmentCount = 3;
        mConfigPara.mEncodeDuration = 120;
        mConfigPara.mEncodeFolderPath = "/home/sample_2VIPPRecord_Files";
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mVippMainChn = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_CHN, 0); 
    mConfigPara.mVippMainWidth = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_WIDTH, 0); 
    mConfigPara.mVippMainHeight = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_HEIGHT, 0); 
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_PIC_FORMAT, NULL); 
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mVippMainPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mVippMainPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mVippMainPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mVippMainPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "afbc"))
    {
        mConfigPara.mVippMainPicFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mVippMainPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mVippMainFrameRate = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_FRAME_RATE, 0);
    mConfigPara.mVippMainDstFrameRate = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_DST_FRAME_RATE, 0);
    if(mConfigPara.mVippMainDstFrameRate <= 0)
    {
        mConfigPara.mVippMainDstFrameRate = mConfigPara.mVippMainFrameRate;
    }
    mConfigPara.mVippMainBufferNum = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_BUFFER_NUM, 0);
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_MAIN_ENCODE_TYPE, NULL); 
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mVippMainEncodeType = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mVippMainEncodeType = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mVippMainEncodeType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mVippMainEncodeType = PT_H264;
    }

    mConfigPara.mVippSubChn = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_CHN, 0);
    mConfigPara.mVippSubWidth = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_WIDTH, 0);
    mConfigPara.mVippSubHeight = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_HEIGHT, 0);
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_PIC_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mVippSubPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mVippSubPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mVippSubPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mVippSubPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "afbc"))
    {
        mConfigPara.mVippSubPicFormat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mVippSubPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mVippSubFrameRate = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_FRAME_RATE, 0);
    mConfigPara.mVippSubDstFrameRate = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_DST_FRAME_RATE, 0);
    if(mConfigPara.mVippSubDstFrameRate <= 0)
    {
        mConfigPara.mVippSubDstFrameRate = mConfigPara.mVippSubFrameRate;
    }
    mConfigPara.mVippSubBufferNum = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_BUFFER_NUM, 0);
    mConfigPara.mVippSubRotation = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_ROTATION, 0);
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_ENCODE_TYPE, NULL); 
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mVippSubEncodeType = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mVippSubEncodeType = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mVippSubEncodeType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mVippSubEncodeType = PT_H264;
    }
    mConfigPara.mbVippSubEnableTakePic = (bool)GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_VIPP_SUB_ENABLE_TAKE_PIC, 0);
    
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_2VIPPRECORD_KEY_AUDIO_ENCODE_TYPE, NULL);
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

    mConfigPara.mMainEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_MAIN_ENCODE_WIDTH, 0);
    mConfigPara.mMainEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_MAIN_ENCODE_HEIGHT, 0);
    mConfigPara.mMainEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_MAIN_ENCODE_BITRATE, 0)*1024*1024;

    mConfigPara.mSubEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_SUB_ENCODE_WIDTH, 0);
    mConfigPara.mSubEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_SUB_ENCODE_HEIGHT, 0);
    mConfigPara.mSubEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_SUB_ENCODE_BITRATE, 0)*1024*1024;

    mConfigPara.mSegmentCount = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_SEGMENT_COUNT, 0);
    mConfigPara.mEncodeDuration = GetConfParaInt(&stConfParser, SAMPLE_2VIPPRECORD_KEY_ENCODE_DURATION, 0);
    mConfigPara.mEncodeFolderPath = GetConfParaString(&stConfParser, SAMPLE_2VIPPRECORD_KEY_ENCODE_FOLDER, NULL);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t Sample2VIPPRecordContext::CreateFolder(const std::string& strFolderPath)
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

std::string Sample2VIPPRecordContext::MakeSegmentFileName(bool bMainFile)
{
    char fileName[64];
    if(bMainFile)
    {
        sprintf(fileName, "File[%04d]_main.mp4", mMainFileNum++);
    }
    else
    {
        sprintf(fileName, "File[%04d]_sub.mp4", mSubFileNum++);
    }
    return std::string(fileName);
}

status_t Sample2VIPPRecordContext::StartRecord()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mRecordLock);
    if(mbRecording)
    {
        alogd("already record");
        return NO_ERROR;
    }
    //record main file.
    mpMainRecorder = new EyeseeRecorder();
    mpMainRecorder->setOnInfoListener(&mMainRecorderListener);
    mpMainRecorder->setOnDataListener(&mMainRecorderListener);
    mpMainRecorder->setOnErrorListener(&mMainRecorderListener);
    mpMainRecorder->setCameraProxy(mpCamera->getRecordingProxy(), mConfigPara.mVippMainChn);
    mpMainRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    mpMainRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    std::string strFilePath = mConfigPara.mEncodeFolderPath + '/' + MakeSegmentFileName(true);
    int64_t nFallocateLen = 0;
    if(mConfigPara.mEncodeDuration > 0)
    {
        nFallocateLen = (int64_t)mConfigPara.mMainEncodeBitrate*mConfigPara.mEncodeDuration/8 + 100*1024*1024;
    }
    mpMainRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, (char*)strFilePath.c_str(), (int)nFallocateLen, false);
    mpMainRecorder->setVideoFrameRate(mConfigPara.mVippMainFrameRate);
    mpMainRecorder->setVideoSize(mConfigPara.mMainEncodeWidth, mConfigPara.mMainEncodeHeight);
    mpMainRecorder->setVideoEncoder(mConfigPara.mVippMainEncodeType);
    mpMainRecorder->setVideoEncodingBitRate(mConfigPara.mMainEncodeBitrate);
    if(mConfigPara.mAudioEncodeType != PT_MAX)
    {
        mpMainRecorder->setAudioSamplingRate(44100);
        mpMainRecorder->setAudioChannels(1);
        mpMainRecorder->setAudioEncodingBitRate(12200);
        mpMainRecorder->setAudioEncoder(PT_AAC);
    }
    mpMainRecorder->setMaxDuration(mConfigPara.mEncodeDuration*1000);
    mpMainRecorder->prepare();
    mpMainRecorder->start();

    //record sub file.
    mpSubRecorder = new EyeseeRecorder();
    mpSubRecorder->setOnInfoListener(&mSubRecorderListener);
    mpSubRecorder->setOnDataListener(&mSubRecorderListener);
    mpSubRecorder->setOnErrorListener(&mSubRecorderListener);
    mpSubRecorder->setCameraProxy(mpCamera->getRecordingProxy(), mConfigPara.mVippSubChn);
    mpSubRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    mpSubRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    std::string strSubFilePath = mConfigPara.mEncodeFolderPath + '/' + MakeSegmentFileName(false);
    nFallocateLen = 0;
    if(mConfigPara.mEncodeDuration > 0)
    {
        nFallocateLen = (int64_t)mConfigPara.mSubEncodeBitrate*mConfigPara.mEncodeDuration/8 + 100*1024*1024;
    }
    mpSubRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_RAW, -1, 0, false);
    mpSubRecorder->addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_MP4, (char*)strSubFilePath.c_str(), (int)nFallocateLen, false);
    mpSubRecorder->setVideoFrameRate(mConfigPara.mVippSubFrameRate);
    mpSubRecorder->setVideoSize(mConfigPara.mSubEncodeWidth, mConfigPara.mSubEncodeHeight);
    mpSubRecorder->setVideoEncoder(mConfigPara.mVippSubEncodeType);
    mpSubRecorder->setVideoEncodingBitRate(mConfigPara.mSubEncodeBitrate);
    if(mConfigPara.mAudioEncodeType != PT_MAX)
    {
        mpSubRecorder->setAudioSamplingRate(44100);
        mpSubRecorder->setAudioChannels(1);
        mpSubRecorder->setAudioEncodingBitRate(12200);
        mpSubRecorder->setAudioEncoder(PT_AAC);
    }
    mpSubRecorder->setMaxDuration(mConfigPara.mEncodeDuration*1000);
    mpSubRecorder->prepare();
    mpSubRecorder->start();

    //sub file thumb picture
    if(mConfigPara.mbVippSubEnableTakePic)
    {
        CameraParameters cameraParam;
        mpCamera->getParameters(mConfigPara.mVippSubChn, cameraParam);
        cameraParam.setPictureMode(TAKE_PICTURE_MODE_FAST);
        cameraParam.setPictureSize(SIZE_S{(unsigned int)mConfigPara.mVippSubWidth, (unsigned int)mConfigPara.mVippSubHeight});
        cameraParam.setJpegQuality(90);
        cameraParam.setJpegThumbnailSize(SIZE_S{0, 0});
        cameraParam.setJpegThumbnailQuality(90);
        mpCamera->setParameters(mConfigPara.mVippSubChn, cameraParam);
        mpCamera->takePicture(mConfigPara.mVippSubChn, NULL, NULL, NULL, &mCameraListener, NULL);
    }
    mbRecording = true;
    return NO_ERROR;
}

status_t Sample2VIPPRecordContext::StopRecord()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mRecordLock);
    if(false == mbRecording)
    {
        alogd("already stop record");
        return NO_ERROR;
    }
    mpMainRecorder->stop();
    delete mpMainRecorder;
    mpMainRecorder = NULL;

    mpSubRecorder->stop();
    delete mpSubRecorder;
    mpSubRecorder = NULL;

    mbRecording = false;
    return ret;
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_2VIPPRecord!"<<endl;
    Sample2VIPPRecordContext stContext;
    pSample2VIPPRecordContext = &stContext;
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
    strcpy(stContext.mSysConf.mkfcTmpDir, "/tmp");
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();
    AW_MPI_VI_SetVIFreq(0,480000000);
    AW_MPI_VENC_SetVEFreq(MM_INVALID_CHN, 632);

    //config camera.
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        VI_CHN chn;
        ISPGeometry mISPGeometry;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 0;
        mISPGeometry.mISPDev = 0;
        mISPGeometry.mScalerOutChns.push_back(stContext.mConfigPara.mVippMainChn);
        mISPGeometry.mScalerOutChns.push_back(stContext.mConfigPara.mVippSubChn);
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        //cameraInfo.mMPPGeometry.mISPDev = 0;
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(stContext.mConfigPara.mVippMainChn);
        //cameraInfo.mMPPGeometry.mScalerOutChns.push_back(stContext.mConfigPara.mVippSubChn);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }
    int cameraId;
    CameraInfo& cameraInfo = stContext.mCameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
    stContext.mpCamera->setInfoCallback(&stContext.mCameraListener);
    stContext.mpCamera->prepareDevice(); 
    stContext.mpCamera->startDevice();

    stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], true);
    CameraParameters cameraParam;
    stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    SIZE_S captureSize;
    captureSize.Width = stContext.mConfigPara.mVippMainWidth;
    captureSize.Height = stContext.mConfigPara.mVippMainHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mVippMainFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mVippMainPicFormat);
    cameraParam.setVideoBufferNumber(stContext.mConfigPara.mVippMainBufferNum);
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], false);
    stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    SIZE_S previewSize = {(unsigned int)stContext.mConfigPara.mVippSubWidth, (unsigned int)stContext.mConfigPara.mVippSubHeight};
    cameraParam.setVideoSize(previewSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mVippSubFrameRate);
    cameraParam.setDisplayFrameRate(30);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mVippSubPicFormat);
    cameraParam.setVideoBufferNumber(stContext.mConfigPara.mVippSubBufferNum);
    cameraParam.setPreviewRotation(stContext.mConfigPara.mVippSubRotation);
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    
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
    stContext.mLayerAttr.stDispRect.Width = 240;
    stContext.mLayerAttr.stDispRect.Height = 320;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
    stContext.mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], stContext.mVoLayer);

    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    int ret = cdx_sem_down_timedwait(&stContext.mSemRenderStart, 5000);
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

    //try to get user input
    int nUsrCmd;
    while(1)
    {
        nUsrCmd = getchar();
        if(nUsrCmd == 'r')
        {
            //start to record
            if(true == stContext.mbRecording)
            {
                alogd("already recording, continue");
                continue;
            }
            stContext.StartRecord();
        }
        else if(nUsrCmd == 's')
        {
            //stop record
            stContext.StopRecord();
        }
        else if(nUsrCmd == 'e')
        {
            //exit
            stContext.StopRecord();
            break;
        }
        else
        {
            alogd("ignore input[0x%x]", nUsrCmd);
        }
        if(cdx_sem_get_val(&stContext.mSemExit) > 0)
        {
            alogd("quit getchar() to exit!");
            break;
        }
    }
    //stop record
    stContext.StopRecord();
    //close camera
    alogd("HerbCamera::release()");
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
    //close vo
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;
    //exit mpp system
    AW_MPI_SYS_Exit(); 
    cout<<"bye, sample_2VIPPRecord!"<<endl;
    return result;
}

