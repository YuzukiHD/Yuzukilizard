
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_ThumbRetriever"

#include <utils/plat_log.h>
#include <sys/stat.h>
#include <cstdio>
#include <assert.h>
#include <csignal>
#include <iostream>
#include <confparser.h>

#include <mpi_sys.h>

#include "sample_ThumbRetriever.h"
#include "sample_ThumbRetriever_config.h"

using namespace std;
using namespace EyeseeLinux;

static void ShowMediaInfo(DEMUX_MEDIA_INFO_S* pMediaInfo)
{
    alogd("======== MediaInfo ===========");
    alogd("FileSize = %d Duration = %d ms", pMediaInfo->mFileSize, pMediaInfo->mDuration);
    alogd("TypeMap H264(%d) H265(%d) MJPEG(%d) JPEG(%d) AAC(%d) MP3(%d)", PT_H264, PT_H265, PT_MJPEG, PT_JPEG, PT_AAC, PT_MP3);
    // Video Or Jpeg Info
    for (int i = 0; i < pMediaInfo->mVideoNum; i++)
    {//VideoType: PT_JPEG, PT_H264, PT_H265, PT_MJPEG
        alogd("Video: CodecType = %d wWidth = %d wHeight = %d nFrameRate = %d AvgBitsRate = %d MaxBitsRate = %d"
            ,pMediaInfo->mVideoStreamInfo[i].mCodecType
            ,pMediaInfo->mVideoStreamInfo[i].mWidth
            ,pMediaInfo->mVideoStreamInfo[i].mHeight
            ,pMediaInfo->mVideoStreamInfo[i].mFrameRate
            ,pMediaInfo->mVideoStreamInfo[i].mAvgBitsRate
            ,pMediaInfo->mVideoStreamInfo[i].mMaxBitsRate
            );
    }

    // Audio Info
    for (int i = 0; i < pMediaInfo->mAudioNum; i++)
    {
        alogd("Audio: CodecType = %d nChaninelNum = %d nBitsPerSample = %d nSampleRate = %d AvgBitsRate = %d MaxBitsRate = %d"
            ,pMediaInfo->mAudioStreamInfo[i].mCodecType
            ,pMediaInfo->mAudioStreamInfo[i].mChannelNum
            ,pMediaInfo->mAudioStreamInfo[i].mBitsPerSample
            ,pMediaInfo->mAudioStreamInfo[i].mSampleRate
            ,pMediaInfo->mAudioStreamInfo[i].mAvgBitsRate
            ,pMediaInfo->mAudioStreamInfo[i].mMaxBitsRate
            );
    }

    return;
}

static SampleThumbContext *pSampleThumbContext = NULL;

SampleThumbContext::SampleThumbContext()
{
    alogd("SampleThumbContext had create!");
    bzero(&mConfigPara,sizeof(mConfigPara));
    mThumbRetriever = NULL;
}
SampleThumbContext::~SampleThumbContext()
{
    if(mThumbRetriever)
    {
        delete mThumbRetriever;
    }
    alogd("SampleThumbContext had destory!");
}

status_t SampleThumbContext::ParseCmdLine(int argc, char *argv[])
{
    alogd("this program path:[%s], arg number is [%d]", argv[0], argc);
    status_t ret = NO_ERROR;
    int i = 1;
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                std::string errorString;
                errorString = "fatal error! use -h to learn how to set parameter!!!";
                cout << errorString << endl;
                ret = -1;
                break;
            }
            mCmdLinePara.mConfigFilePath = argv[i];
            }
        else if(!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_ThumbRetriever.conf\n";
            cout << helpString << endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += ", type -h to get how to set parameter!";
            cout << ignoreString << endl;
        }
        i++;
    }
    return ret;
}

status_t SampleThumbContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        mConfigPara.mFilePath = "/home/sample_ThumbRetriever_Files/mp4.mp4";
        mConfigPara.mJpegWidth = 480;
        mConfigPara.mJpegHeight = 320;
        mConfigPara.mJpegPath = "/home/sample_ThumbRetriever_Files";
        return NO_ERROR;
    }

    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }

    mConfigPara.mFilePath = GetConfParaString(&stConfParser, SAMPLE_THUMBRETRIEVER_KEY_FILE_PATH, NULL);
    mConfigPara.mJpegWidth = GetConfParaInt(&stConfParser, SAMPLE_THUMBRETRIEVER_KEY_JPEG_WIDTH, 0);
    mConfigPara.mJpegHeight = GetConfParaInt(&stConfParser, SAMPLE_THUMBRETRIEVER_KEY_JPEG_HEIGHT, 0);
    mConfigPara.mSeekStartPosition = GetConfParaInt(&stConfParser, SAMPLE_THUMBRETRIEVER_KEY_SEEK_START_POSITION, 0);
    mConfigPara.mJpegPath = GetConfParaString(&stConfParser, SAMPLE_THUMBRETRIEVER_KEY_JPEG_PATH, NULL);

    destroyConfParser(&stConfParser);

    return ret;
}

status_t SampleThumbContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("jpeg path is not set!");
        return UNKNOWN_ERROR;
    }
    const char* pJpegFolderPath = strFolderPath.c_str();
    //check folder existence
    struct stat sb;
    if (stat(pJpegFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pJpegFolderPath, sb.st_mode);
            return UNKNOWN_ERROR;
        }
    }
    //create folder if necessary
    int ret = mkdir(pJpegFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pJpegFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pJpegFolderPath);
        return UNKNOWN_ERROR;
    }
}

static void SavePicture(std::shared_ptr<CMediaMemory> pJpegThumb)
{
    if (!pSampleThumbContext || !pJpegThumb)
    {
        aloge("fatal error! invalid context or jpeg content");
        return;
    }
    std::string PicturePath = pSampleThumbContext->mConfigPara.mJpegPath + "/thumb.jpeg";
    FILE *fp = fopen(PicturePath.c_str(), "wb");
    fwrite(pJpegThumb->getPointer(), 1, pJpegThumb->getSize(), fp);
    fclose(fp);
}


int main(int argc, char *argv[])
{
    int result = 0;
    cout << "Hello, sample_ThumbRetriever" << endl;

    SampleThumbContext stContext;
    pSampleThumbContext = &stContext;

    if(stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        result = -1;
        return result;
    }

    if(stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }

    if(stContext.CreateFolder(stContext.mConfigPara.mJpegPath) != SUCCESS)
    {
        return -1;
    }

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    stContext.mThumbRetriever = new EyeseeThumbRetriever();

    stContext.mThumbRetriever->setDataSource(stContext.mConfigPara.mFilePath.c_str());
    DEMUX_MEDIA_INFO_S stMediaInfo = {0};
    stContext.mThumbRetriever->getMediaInfo(&stMediaInfo);
    ShowMediaInfo(&stMediaInfo);
    SavePicture(stContext.mThumbRetriever->getJpegAtTime(stContext.mConfigPara.mSeekStartPosition, stContext.mConfigPara.mJpegWidth, stContext.mConfigPara.mJpegHeight));
    stContext.mThumbRetriever->reset();
    delete stContext.mThumbRetriever;
    stContext.mThumbRetriever = NULL;
    AW_MPI_SYS_Exit();
    
    return result;
}
