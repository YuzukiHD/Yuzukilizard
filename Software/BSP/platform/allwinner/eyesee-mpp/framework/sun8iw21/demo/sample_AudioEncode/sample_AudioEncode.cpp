//#define LOG_NDEBUG 0
#define LOG_TAG "sample_AudioEncode"
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

#include <confparser.h>

#include <EyeseeRecorder.h>

#include "sample_AudioEncode_config.h"
#include "sample_AudioEncode.h"

using namespace std;
using namespace EyeseeLinux;

SampleAudioEncodeContext *gpSampleAudioEncodeContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleAudioEncodeContext!=NULL)
    {
        cdx_sem_up(&gpSampleAudioEncodeContext->mSemExit);
    }
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
            std::string strSegmentFilePath = mpOwner->mConfigPara.mFileDir + '/' + mpOwner->MakeSegmentFileName();
            mpOwner->mpRecorder->setOutputFileSync((char*)strSegmentFilePath.c_str(), 0, nMuxerId);
            mpOwner->mSegmentFiles.push_back(strSegmentFilePath);
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done,  muxer_id[%d]", extra);
            int nMuxerId = extra;
            int ret;
            while((int)mpOwner->mSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
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

EyeseeRecorderListener::EyeseeRecorderListener(SampleAudioEncodeContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleAudioEncodeContext::SampleAudioEncodeContext()
    :mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    mpRecorder = NULL;
    mMuxerId = -1;
    mFileFormat = MEDIA_FILE_FORMAT_AAC;
    mFileNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
}

SampleAudioEncodeContext::~SampleAudioEncodeContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpRecorder!=NULL)
    {
        aloge("fatal error! EyeseeRecorder is not destruct!");
    }
}

status_t SampleAudioEncodeContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_AudioEncode.conf\n";
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
status_t SampleAudioEncodeContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mPcmSampleRate = 44100;
        mConfigPara.mPcmChannelCnt = 1;
        mConfigPara.mAudioEncodeType = PT_AAC;
        mConfigPara.mEncodeBitrate = 1*1000*1000;
        mConfigPara.mFileDir = "/mnt/extsd/sample_AudioEncode_Files";
        mConfigPara.mSegmentCount = 3;
        mConfigPara.mSegmentDuration = 10;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mPcmSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AUDIOENCODE_KEY_PCM_SAMPLE_RATE, 0); 
    mConfigPara.mPcmChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AUDIOENCODE_KEY_PCM_CHANNEL_CNT, 0); 
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_AUDIOENCODE_KEY_AUDIO_ENCODE_TYPE, NULL); 
    if(!strcmp(pStrEncodeType, "aac"))
    {
        mConfigPara.mAudioEncodeType = PT_AAC;
    }
    else if(!strcmp(pStrEncodeType, "mp3"))
    {
        mConfigPara.mAudioEncodeType = PT_MP3;
    }
    else
    {
        aloge("fatal error! conf audio encode type is [%s]?", pStrEncodeType);
        mConfigPara.mAudioEncodeType = PT_AAC;
    }
    mConfigPara.mEncodeBitrate  = GetConfParaInt(&stConfParser, SAMPLE_AUDIOENCODE_KEY_ENCODE_BITRATE, 0)*1024*1024; 
    mConfigPara.mFileDir = (char*)GetConfParaString(&stConfParser, SAMPLE_AUDIOENCODE_KEY_FILE_DIR, NULL); 
    mConfigPara.mSegmentCount = GetConfParaInt(&stConfParser, SAMPLE_AUDIOENCODE_KEY_SEGMENT_COUNT, 0);
    mConfigPara.mSegmentDuration = GetConfParaInt(&stConfParser, SAMPLE_AUDIOENCODE_KEY_SEGMENT_DURATION, 0);
    alogd("fileDir[%s]", mConfigPara.mFileDir.c_str());
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleAudioEncodeContext::CreateFolder(const std::string& strFolderPath)
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
std::string SampleAudioEncodeContext::MakeSegmentFileName()
{
    char fileName[64];
    std::string suffix;
    switch(mFileFormat)
    {
        case MEDIA_FILE_FORMAT_AAC:
            suffix = "aac";
            break;
        case MEDIA_FILE_FORMAT_MP3:
            suffix = "mp3";
            break;
        default:
            aloge("fatal error! unknown file format:0x%x", mFileFormat);
            suffix = "unknown";
            break;
    }
    sprintf(fileName, "AudioFile[%04d].%s", mFileNum++, suffix.c_str());
    return std::string(fileName);
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_AudioEncode!"<<endl;
    SampleAudioEncodeContext *pContext = new SampleAudioEncodeContext;
    gpSampleAudioEncodeContext = pContext;
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
    switch(pContext->mConfigPara.mAudioEncodeType)
    {
        case PT_AAC:
            pContext->mFileFormat = MEDIA_FILE_FORMAT_AAC;
            break;
        case PT_MP3:
            pContext->mFileFormat = MEDIA_FILE_FORMAT_MP3;
            break;
        default:
            aloge("fatal error! unknown audio encode type:%d", pContext->mConfigPara.mAudioEncodeType);
            pContext->mFileFormat = MEDIA_FILE_FORMAT_AAC;
            break;
    }
    //check folder existence, create folder if necessary
    if(SUCCESS != pContext->CreateFolder(pContext->mConfigPara.mFileDir))
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

    //record audio file.
    pContext->mpRecorder = new EyeseeRecorder();
    pContext->mpRecorder->setOnInfoListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnDataListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setOnErrorListener(&pContext->mRecorderListener);
    pContext->mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    std::string strSegmentFilePath = pContext->mConfigPara.mFileDir + '/' + pContext->MakeSegmentFileName();
    //pContext->mMuxerId = pContext->mpRecorder->addOutputFormatAndOutputSink(pContext->mFileFormat, (char*)strSegmentFilePath.c_str(), 0, false);
    SinkParam stSinkParam;
    memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = pContext->mFileFormat;
    stSinkParam.mOutputFd = -1;
    stSinkParam.mOutputPath = (char*)strSegmentFilePath.c_str();
    stSinkParam.mFallocateLen = 0;
    stSinkParam.mMaxDurationMs = 0;
    stSinkParam.bCallbackOutFlag = false;
    stSinkParam.bBufFromCacheFlag = false;
    pContext->mMuxerId = pContext->mpRecorder->addOutputSink(&stSinkParam);
    pContext->mSegmentFiles.push_back(strSegmentFilePath);

    pContext->mpRecorder->setAudioSamplingRate(pContext->mConfigPara.mPcmSampleRate);
    pContext->mpRecorder->setAudioChannels(pContext->mConfigPara.mPcmChannelCnt);
    pContext->mpRecorder->setAudioEncodingBitRate(pContext->mConfigPara.mEncodeBitrate);
    pContext->mpRecorder->setAudioEncoder(pContext->mConfigPara.mAudioEncodeType);
    pContext->mpRecorder->enableAttachAACHeader(false);

    pContext->mpRecorder->setMaxDuration(pContext->mConfigPara.mSegmentDuration*1000);
    alogd("prepare()!");
    pContext->mpRecorder->prepare();
    
    alogd("start()!");
    pContext->mpRecorder->start();

    cdx_sem_down(&pContext->mSemExit);
    alogd("record done! stop()!");
    //stop mHMR0
    pContext->mpRecorder->stop();
    delete pContext->mpRecorder;
    pContext->mpRecorder = NULL;

    //exit mpp system
    AW_MPI_SYS_Exit(); 
    delete pContext;
    gpSampleAudioEncodeContext = NULL;
    cout<<"bye, sample_AudioEnocde!"<<endl;
    return result;
}

