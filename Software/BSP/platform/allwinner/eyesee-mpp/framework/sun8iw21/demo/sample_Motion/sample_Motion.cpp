//#define LOG_NDEBUG 0
#define LOG_TAG "sample_Motion"

#include<stdio.h>
#include<iostream>

#include <utils/plat_log.h>
#include <confparser.h>
#include <mpi_sys.h>

#include "sample_Motion.h"
#include "sample_Motion_config.h"

using namespace EyeseeLinux;


static SampleMotionContext *pSampleMotionContext = NULL;


SampleMotionContext::SampleMotionContext()
    : mpMotion(NULL)
{
    bzero(&mConfigParam,sizeof(mConfigParam));
}

SampleMotionContext::~SampleMotionContext()
{
    if(mpMotion)
    {
        delete mpMotion;
        mpMotion = NULL;
    }
    
}

status_t SampleMotionContext::ParseCmdLine(int argc, char *argv[])
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
                std::cout << errorString << std::endl;
                ret = -1;
                break;
            }
            mCmdLineParam.mConfigFilePath = argv[i];
            }
        else if(!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_Motion.conf\n";
            std::cout << helpString << std::endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += ", type -h to get how to set parameter!";
            std::cout << ignoreString << std::endl;
        }
        i++;
    }
    return ret;
    
}

status_t SampleMotionContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLineParam.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        return UNKNOWN_ERROR;
    }

    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }

    mConfigParam.mFilePath = GetConfParaString(&stConfParser, SAMPLE_MOTION_KEY_FILE_PATH, NULL);
    mConfigParam.mJpegWidth = GetConfParaInt(&stConfParser, SAMPLE_MOTION_KEY_JPEG_WIDTH, 0);
    mConfigParam.mJpegHeight = GetConfParaInt(&stConfParser, SAMPLE_MOTION_KEY_JPEG_HEIGHT, 0);
    mConfigParam.mStartTimeUs = GetConfParaInt(&stConfParser, SAMPLE_MOTION_KEY_START_TIME, 0);
    mConfigParam.mFrameNums = GetConfParaInt(&stConfParser, SAMPLE_MOTION_KEY_FRAME_NUMS, 5);
    mConfigParam.mFrameStep = GetConfParaInt(&stConfParser, SAMPLE_MOTION_KEY_FRAME_STEP, 10);
    mConfigParam.mObjectsNums = GetConfParaInt(&stConfParser, SAMPLE_MOTION_KEY_OBJECT_NUMS, 5);
    mConfigParam.mJpegPath = GetConfParaString(&stConfParser, SAMPLE_MOTION_KEY_JPEG_PATH, NULL);

    destroyConfParser(&stConfParser);

    return ret;
    
}

status_t SampleMotionContext::CreateFolder(const std::string& strFolderPath)
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
    if(0 == pJpegThumb->getSize())
    {
        alogd("no picture!!");
        return ;
    }
    std::string PicturePath = pSampleMotionContext->mConfigParam.mJpegPath + "/thumb.jpeg";
    std::cout << "the motion picture is " << PicturePath << std::endl;
    
    FILE *fp = fopen(PicturePath.c_str(), "wb");
    if(!fp)
    {
        aloge("open file error!");
    }
    fwrite(pJpegThumb->getPointer(), 1, pJpegThumb->getSize(), fp);
    fclose(fp);
    return ;
}

int
main(int argc, char **argv)
{
    int result = 0;
    std::cout << "Hello, sample_Motion" << std::endl;

    SampleMotionContext stContext;
    pSampleMotionContext = &stContext;

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

    if(stContext.CreateFolder(stContext.mConfigParam.mJpegPath) != SUCCESS)
    {
        result = -1;
        return result;
    }

    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    stContext.mpMotion = new EyeseeLinux::EyeseeMotion();
    stContext.mpMotion->setVideoFileSource(stContext.mConfigParam.mFilePath.c_str());
    EyeseeMotion::MotionPictureParam jpegParam;
    jpegParam.mPicWidth = stContext.mConfigParam.mJpegWidth;
    jpegParam.mPicHeight = stContext.mConfigParam.mJpegHeight;
    jpegParam.mStartTimeUs = stContext.mConfigParam.mStartTimeUs;
    jpegParam.mSourceFrameNums = stContext.mConfigParam.mFrameNums;
    jpegParam.mSourceFrameStep = stContext.mConfigParam.mFrameStep;
    jpegParam.mObjectNums = stContext.mConfigParam.mObjectsNums;
    
    SavePicture(stContext.mpMotion->getMotionJpeg(jpegParam));    
    stContext.mpMotion->reset();

    return 0;
}




