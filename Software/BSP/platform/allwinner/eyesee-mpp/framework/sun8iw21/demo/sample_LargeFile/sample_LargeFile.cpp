//#define LOG_NDEBUG 0
#define LOG_TAG "sample_Camera"
#include <utils/plat_log.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstdio>
#include <csignal>
#include <iostream>

#include <confparser.h>

#include "sample_LargeFile_config.h"
#include "sample_LargeFile.h"

using namespace std;
using namespace EyeseeLinux;

static SampleLargeFileContext *pSampleLargeFileContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleLargeFileContext!=NULL)
    {
        cdx_sem_up(&pSampleLargeFileContext->mSemExit);
    }
}

SampleLargeFileContext::SampleLargeFileContext()
{
    cdx_sem_init(&mSemExit, 0);
    mpChunkBuf = NULL;
    mCurWriteSize = 0;
    mFd = -1;
    bzero(&mConfigPara,sizeof(mConfigPara));
}

SampleLargeFileContext::~SampleLargeFileContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpChunkBuf)
    {
        delete mpChunkBuf;
        mpChunkBuf = NULL;
    }
    if(mFd >= 0)
    {
        close(mFd);
        mFd = -1;
    }
}

status_t SampleLargeFileContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_LargeFile.conf\n";
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

status_t SampleLargeFileContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mChunkSize = 1*1024*1024;
        mConfigPara.mTotalSize = (int64_t)2*1024*1024*1024;
        mConfigPara.mFileDir = "/home/sample_LargeFile_Files";
        mConfigPara.mFileName = "LargeFile.dat";
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mChunkSize = (int64_t)GetConfParaInt(&stConfParser, SAMPLE_LARGEFILE_KEY_ChunkSize, 0)*1024*1024;
    mConfigPara.mTotalSize = (int64_t)GetConfParaInt(&stConfParser, SAMPLE_LARGEFILE_KEY_TotalSize, 0)*1024*1024;
    mConfigPara.mFileDir = GetConfParaString(&stConfParser, SAMPLE_LARGEFILE_KEY_FileFolder, NULL);
    mConfigPara.mFileName = GetConfParaString(&stConfParser, SAMPLE_LARGEFILE_KEY_FileName, NULL);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleLargeFileContext::CreateFolder(const std::string& strFolderPath)
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

void SampleLargeFileContext::CreateChunk()
{
    if(mpChunkBuf)
    {
        alogw("Be careful! why chunkBuf[%p] buf[%p]size[%d] exist? delete it!", mpChunkBuf, mpChunkBuf->getPointer(), mpChunkBuf->getSize());
        delete mpChunkBuf;
    }
    mpChunkBuf = new CMediaMemory(mConfigPara.mChunkSize);
    if(NULL == mpChunkBuf->getPointer())
    {
        aloge("fatal error! malloc fail!");
        delete mpChunkBuf;
        mpChunkBuf = NULL;
        return;
    }
    memset(mpChunkBuf->getPointer(), 0xAD, mConfigPara.mChunkSize);
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_LargeFile!"<<endl;
    SampleLargeFileContext stContext;
    pSampleLargeFileContext = &stContext;
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
    if(SUCCESS != stContext.CreateFolder(stContext.mConfigPara.mFileDir))
    {
        return -1;
    }
    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    stContext.CreateChunk();

    //create file.
    std::string strFilePath = stContext.mConfigPara.mFileDir + '/' + stContext.mConfigPara.mFileName;
    stContext.mFd = open((char*)strFilePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (stContext.mFd < 0) 
    {
		aloge("Failed to open %s", (char*)strFilePath.c_str());
		return -1;
	}

    //write file
    stContext.mCurWriteSize = 0;
    while(stContext.mCurWriteSize < stContext.mConfigPara.mTotalSize)
    {
        ssize_t nWrittenBytes = write(stContext.mFd , stContext.mpChunkBuf->getPointer(), (size_t)stContext.mConfigPara.mChunkSize);
        if(nWrittenBytes != (ssize_t)stContext.mConfigPara.mChunkSize)
        {
            aloge("fatal error! writtenByes[%d] != chunkSize[%lld], errno[%d][%s]", nWrittenBytes, stContext.mConfigPara.mChunkSize, errno, strerror(errno));
        }
        if(nWrittenBytes < 0)
        {
            nWrittenBytes = 0;
        }
        stContext.mCurWriteSize += nWrittenBytes;
        if(cdx_sem_get_val(&stContext.mSemExit) > 0 )
        {
            alogd("take user exit order, exit!");
            break;
        }
    }
    alogd("write file size: %lld bytes", stContext.mCurWriteSize);
    if(stContext.mFd >= 0)
    {
        close(stContext.mFd);
        alogd("close fd[%d]", stContext.mFd);
        stContext.mFd = -1;
    }
    cout<<"bye, sample_LargeFile!"<<endl;
    return result;
}

