//#define LOG_NDEBUG 0
#define LOG_TAG "sample_VideoResizer"
#include <utils/plat_log.h>
#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <confparser.h>
#include <mpi_sys.h>
#include "sample_VideoResizer_config.h"
#include "sample_VideoResizer.h"

using namespace std;
using namespace EyeseeLinux;

static SamplePlayerContext *pSamplePlayerContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    exit(-1);
}

SamplePlayerContext::SamplePlayerContext()
{
    cdx_sem_init(&mSemExit, 0);
    mpVideoResizer = NULL;
    bzero(&mConfigPara,sizeof(mConfigPara));
}

SamplePlayerContext::~SamplePlayerContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpVideoResizer!=NULL)
    {
        aloge("fatal error! EyeseePlayer is not destruct!");
    }
}

status_t SamplePlayerContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_Player.conf\n";
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

status_t SamplePlayerContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mSrcFilePath      = "/mnt/extsd/sample_Player_Files/test.mp4";
        mConfigPara.mDstFilePath      = "/mnt/extsd/sample_VideoResizer/output.mp4";
        mConfigPara.mDstBitStreamPath = "/mnt/extsd/sample_VideoResizer/output.bin";
        mConfigPara.mDstVideoWidth   = 640;
        mConfigPara.mDstVideoHeight  = 480;
        mConfigPara.mDstFrameRate    = 25;
        mConfigPara.mDstBitRate      = 4 * 1024 * 1024;
        mConfigPara.mDstVideoEncFmt  = PT_H264;
        mConfigPara.mMediaFileFmt    = MEDIA_FILE_FORMAT_MP4;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }

    mConfigPara.mSrcFilePath      = GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_KEY_FILE_PATH, NULL);
    mConfigPara.mDstFilePath      = GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_KEY_DST_FILE_PATH, NULL);
    mConfigPara.mDstBitStreamPath = GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_KEY_DST_BITSTREAM_PATH, NULL);
    mConfigPara.mDstVideoWidth    = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_KEY_VIDEO_WIDTH, 0);
    mConfigPara.mDstVideoHeight   = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_KEY_VIDEO_HEIGHT, 0);
    mConfigPara.mDstFrameRate     = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_KEY_FRAME_RATE, 0);
    mConfigPara.mDstBitRate       = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_KEY_BIT_RATE, 0);
    char *pStrIntfVideoEncFmt     = (char*)GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_KEY_VIDEO_ENC_FMT, NULL);
    if (!strcasecmp(pStrIntfVideoEncFmt, "h264"))
    {
        mConfigPara.mDstVideoEncFmt = PT_H264;
    }
    else if (!strcasecmp(pStrIntfVideoEncFmt, "h265"))
    {
        mConfigPara.mDstVideoEncFmt = PT_H265;
    }
    else
    {
        aloge("fatal error! conf file video_enc_fmt is [%s]?", pStrIntfVideoEncFmt);
        mConfigPara.mDstVideoEncFmt = PT_H264;
    }

    char *pStrIntfMediaFileFmt = (char*)GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_KEY_MEDIA_FILE_FMT, NULL);
    if (!strcasecmp(pStrIntfMediaFileFmt, "mp4"))
    {
        mConfigPara.mMediaFileFmt  = MEDIA_FILE_FORMAT_MP4;
    }
    else if (!strcasecmp(pStrIntfMediaFileFmt, "ts"))
    {
        mConfigPara.mMediaFileFmt  = MEDIA_FILE_FORMAT_TS;
    }
    else
    {
        aloge("fatal error! conf file media_file_fmt is [%s]?", pStrIntfMediaFileFmt);
        mConfigPara.mMediaFileFmt  = MEDIA_FILE_FORMAT_MP4;
    }

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static void SaveBitStream(std::shared_ptr<VREncBuffer> pVREncBuffer, FILE *fp)
{
    VREncBuffer* pTmp = NULL;
    pTmp = pVREncBuffer.get();
    if (pTmp != NULL)
    {
        int ret = fwrite(pTmp->getPointer(), 1, pTmp->getSize(), fp);
        alogd("fwrite ret %d",ret);
    }
}


class MyMessageListener: public EyeseeVideoResizer::OnMessageListener
{
public:
    bool onMessage(EyeseeVideoResizer *pMp, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
    {
        if (MOD_ID_MUX == pChn->mModId)
        {
            switch(event)
            {
            case MPP_EVENT_RECORD_DONE:
                {
                    int muxerId = *(int*)pEventData;
                    alogd("sample:file done, mux_id=%d", muxerId);
                    break;
                }
            case MPP_EVENT_NEED_NEXT_FD:
                {
                    int muxerId = *(int*)pEventData;
                    alogd("mux need next fd, mux_id=%d", muxerId);
                    break;
                }
            case MPP_EVENT_BSFRAME_AVAILABLE:
                {
                    break;
                }
            default:
                break;
            }
        }

        if (MOD_ID_DEMUX == pChn->mModId)
        {
            switch (event)
            {
            case MPP_EVENT_NOTIFY_EOF:
                alogd("sample:demux to end of file");
                if (pSamplePlayerContext!=NULL)
                {
                    cdx_sem_up(&pSamplePlayerContext->mSemExit);
                }
                break;

            default:
                break;
            }
        }

        return SUCCESS;
    }
};


int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_VideoResizer!"<<endl;
    SamplePlayerContext stContext;
    pSamplePlayerContext = &stContext;

    // parse command line param
    if (stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }

    // parse config file.
    if(stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }

    // register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        perror("can't catch SIGSEGV");
    }

    // init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    stContext.mpVideoResizer = new EyeseeLinux::EyeseeVideoResizer();

    MyMessageListener* pListener = new MyMessageListener();
    stContext.mpVideoResizer->setOnMessageListener(pListener);

    int srcFd = open(stContext.mConfigPara.mSrcFilePath.c_str(), O_RDONLY);
    assert(srcFd > 0);
    MEDIA_FILE_FORMAT_E mMediaFileFmt = MEDIA_FILE_FORMAT_MP4; //MEDIA_FILE_FORMAT_RAW
    stContext.mpVideoResizer->setDataSource(srcFd);
    stContext.mpVideoResizer->setVideoSize(stContext.mConfigPara.mDstVideoWidth, stContext.mConfigPara.mDstVideoHeight);
    stContext.mpVideoResizer->setVideoEncFmt(stContext.mConfigPara.mDstVideoEncFmt);
    stContext.mpVideoResizer->addOutputFormatAndOutputSink(mMediaFileFmt, stContext.mConfigPara.mDstFilePath.c_str(), 0, false);
    stContext.mpVideoResizer->setFrameRate(stContext.mConfigPara.mDstFrameRate);
    stContext.mpVideoResizer->setBitRate(stContext.mConfigPara.mDstBitRate);
    stContext.mpVideoResizer->prepare();
    stContext.mpVideoResizer->start();

    alogd("started");

    if (MEDIA_FILE_FORMAT_RAW == mMediaFileFmt)
    {
        FILE *fp = fopen(stContext.mConfigPara.mDstBitStreamPath.c_str(), "wb");
        int contIndex = 0;
        std::shared_ptr<VREncBuffer> mVREncBuffer;
        while (contIndex < 3000)
        {
            SaveBitStream(stContext.mpVideoResizer->getPacket(), fp);
            usleep(100 * 1000);
            contIndex++;
        }
        fclose(fp);
    }
    cdx_sem_down(&stContext.mSemExit);

    stContext.mpVideoResizer->stop();
    close(srcFd);

    delete stContext.mpVideoResizer;
    stContext.mpVideoResizer = NULL;

    //exit mpp system
    AW_MPI_SYS_Exit();

    cout<<"bye, sample_VideoResizer!"<<endl;
    return result;
}

