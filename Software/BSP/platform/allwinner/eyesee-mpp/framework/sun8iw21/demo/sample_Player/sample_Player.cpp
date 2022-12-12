//#define LOG_NDEBUG 0
#define LOG_TAG "sample_Player"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vo.h>
#include <mpi_vdec.h>

#include "sample_Player_config.h"
#include "sample_Player.h"

using namespace std;
using namespace EyeseeLinux;

static SamplePlayerContext *pSamplePlayerContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSamplePlayerContext!=NULL)
    {
        cdx_sem_up(&pSamplePlayerContext->mSemExit);
    }
}

void EyeseePlayerListener::onCompletion(EyeseePlayer *pMp)
{
    alogd("receive onCompletion message!");
    cdx_sem_up(&mpOwner->mSemExit);
}

bool EyeseePlayerListener::onError(EyeseePlayer *pMp, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);    //what: EyeseeLinux::EyeseePlayer::MEDIA_ERROR_UNKNOWN, extra: 
    switch(what)
    {
        case EyeseeLinux::EyeseePlayer::MEDIA_ERROR_IO:
            break;
        default:
            break;
    }
    return true;
}

void EyeseePlayerListener::onVideoSizeChanged(EyeseePlayer *pMp, int width, int height)
{
    alogd("receive onVideoSizeChanged message!size[%dx%d]", width, height);
}

bool EyeseePlayerListener::onInfo(EyeseePlayer *pMp, int what, int extra)
{
    alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);    //what: EyeseeLinux::EyeseePlayer::MEDIA_INFO_UNKNOWN
    switch(what)
    {
        case EyeseeLinux::EyeseePlayer::MEDIA_INFO_VIDEO_RENDERING_START:
            break;
        default:
            break;
    }
    return true;
}

void EyeseePlayerListener::onSeekComplete(EyeseePlayer *pMp)
{
    alogd("receive onSeekComplete message!");
}

EyeseePlayerListener::EyeseePlayerListener(SamplePlayerContext *pOwner)
    : mpOwner(pOwner)
{
}

SamplePlayerContext::SamplePlayerContext()
    :mPlayerListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = -1;
    mVoLayer = -1;
    mpPlayer = NULL;
    mLoopNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
}

SamplePlayerContext::~SamplePlayerContext()
{
    cdx_sem_deinit(&mSemExit);
    if(mpPlayer!=NULL)
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
        mConfigPara.mFilePath = "/mnt/extsd/test.mp4";
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mVdecOutputMaxWidth = 0;
        mConfigPara.mVdecOutputMaxHeight = 0;
        mConfigPara.mDisplayWidth = 480;
        mConfigPara.mDisplayHeight = 640; 
        mConfigPara.mbForbidAudio = false;
        mConfigPara.mRotation = ROTATE_NONE;
        mConfigPara.mbSetVideolayer = true;
        mConfigPara.mIntfType = VO_INTF_LCD;
        mConfigPara.mIntfSync = VO_OUTPUT_1080P60;
        mConfigPara.mVeFreq = 0;
        mConfigPara.mbLoop = false;
        mConfigPara.mSelectTrackIndex = 0;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mFilePath = GetConfParaString(&stConfParser, SAMPLE_PLAYER_KEY_FILE_PATH, NULL);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_PLAYER_KEY_PIC_FORMAT, NULL); 
    if(!strcasecmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcasecmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcasecmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcasecmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mVdecOutputMaxWidth = GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_VDEC_OUTPUT_MAX_WIDTH, 0); 
    mConfigPara.mVdecOutputMaxHeight = GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_VDEC_OUTPUT_MAX_HEIGHT, 0); 
    mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_DISPLAY_WIDTH, 0); 
    mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_DISPLAY_HEIGHT, 0); 
    mConfigPara.mbForbidAudio = (bool)GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_FORBID_AUDIO, 0); 
    mConfigPara.mRotation = (ROTATE_E)GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_ROTATION, 0); 
    mConfigPara.mbSetVideolayer = (bool)GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_SET_VIDEOLAYER, 0);
    char *pStrIntfType = (char*)GetConfParaString(&stConfParser, SAMPLE_PLAYER_KEY_INTERFACE_TYPE, NULL); 
    if(!strcasecmp(pStrIntfType, "lcd"))
    {
        mConfigPara.mIntfType = VO_INTF_LCD;
    }
    else if(!strcasecmp(pStrIntfType, "hdmi"))
    {
        mConfigPara.mIntfType = VO_INTF_HDMI;
    }
    else if(!strcasecmp(pStrIntfType, "cvbs"))
    {
        mConfigPara.mIntfType = VO_INTF_CVBS;
    }
    else
    {
        aloge("fatal error! conf file interface type is [%s]?", pStrIntfType);
        mConfigPara.mIntfType = VO_INTF_LCD;
    }
    char *pStrIntfSync = (char*)GetConfParaString(&stConfParser, SAMPLE_PLAYER_KEY_INTERFACE_SYNC, NULL); 
    if(!strcasecmp(pStrIntfSync, "480I") || !strcasecmp(pStrIntfSync, "NTSC"))
    {
        mConfigPara.mIntfSync = VO_OUTPUT_NTSC;
    }
    else if(!strcasecmp(pStrIntfSync, "1080P60"))
    {
        mConfigPara.mIntfSync = VO_OUTPUT_1080P60;
    }
    else if(!strcasecmp(pStrIntfSync, "PAL"))
    {
        mConfigPara.mIntfSync = VO_OUTPUT_PAL;
    }
    else
    {
        aloge("fatal error! conf file interface type is [%s]?", pStrIntfSync);
        mConfigPara.mIntfSync = VO_OUTPUT_NTSC;
    }
    mConfigPara.mVeFreq = GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_VE_FREQ, 0); 
    mConfigPara.mbLoop = (bool)GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_LOOP, 0); 
    mConfigPara.mSelectTrackIndex = GetConfParaInt(&stConfParser, SAMPLE_PLAYER_KEY_SELECT_TRACK_INDEX, 0);
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_Player!"<<endl;
    SamplePlayerContext *pContext = new SamplePlayerContext;
    pSamplePlayerContext = pContext;
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
    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");
    //init mpp system
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();
    AW_MPI_VDEC_SetVEFreq(MM_INVALID_CHN, pContext->mConfigPara.mVeFreq);

    pContext->mpPlayer = new EyeseePlayer;
    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    VO_PUB_ATTR_S stPubAttr;
    AW_MPI_VO_GetPubAttr(0, &stPubAttr);
    stPubAttr.enIntfType = pContext->mConfigPara.mIntfType;
    stPubAttr.enIntfSync = pContext->mConfigPara.mIntfSync;
    AW_MPI_VO_SetPubAttr(0, &stPubAttr);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);//close ui layer.
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
    pContext->mLayerAttr.stDispRect.Width = pContext->mConfigPara.mDisplayWidth;
    pContext->mLayerAttr.stDispRect.Height = pContext->mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &pContext->mLayerAttr);
    pContext->mVoLayer = hlay;
    alogd("requestSurface hlay=%d", pContext->mVoLayer);
    pContext->mpPlayer->setOnCompletionListener(&pContext->mPlayerListener);
    pContext->mpPlayer->setOnErrorListener(&pContext->mPlayerListener);
    pContext->mpPlayer->setOnVideoSizeChangedListener(&pContext->mPlayerListener);
    pContext->mpPlayer->setOnInfoListener(&pContext->mPlayerListener);
    pContext->mpPlayer->setOnSeekCompleteListener(&pContext->mPlayerListener);
    if(pContext->mConfigPara.mbForbidAudio)
    {
        pContext->mpPlayer->grantPlayAudio(false);
    }
    pContext->mpPlayer->setDataSource(pContext->mConfigPara.mFilePath);
    std::vector<EyeseePlayer::TrackInfo> trackInfo;
    pContext->mpPlayer->getTrackInfo(trackInfo);
    for (std::vector<EyeseePlayer::TrackInfo>::iterator it = trackInfo.begin(); it != trackInfo.end(); ++it)
    {
        switch (it->mTrackType)
        {
            case EyeseePlayer::TrackInfo::MEDIA_TRACK_TYPE_VIDEO:
            {
                alogd("%s, CodeType: %d, Width: %d, Height: %d",
                    it->mLanguage.c_str(), it->mVideoTrackInfo.mCodecType,
                    it->mVideoTrackInfo.mWidth, it->mVideoTrackInfo.mHeight);
                break;
            }
            case EyeseePlayer::TrackInfo::MEDIA_TRACK_TYPE_AUDIO:
            {
                alogd("%s, CodeType: %d, SampleRate: %d, ChannelNum: %d",
                    it->mLanguage.c_str(), it->mAudioTrackInfo.mCodecType,
                    it->mAudioTrackInfo.mSampleRate, it->mAudioTrackInfo.mChannelNum);
                break;
            }
            case EyeseePlayer::TrackInfo::MEDIA_TRACK_TYPE_TIMEDTEXT:
            {
                alogd("%s", it->mLanguage.c_str());
                break;
            }
            default:
                alogw("unsuport Track Type: %d", it->mTrackType);
                break;
        }
    }
    pContext->mpPlayer->selectTrack(pContext->mConfigPara.mSelectTrackIndex);
    //mCMP->setAudioStreamType(AUDIO_STREAM_MUSIC/*AudioManager.STREAM_MUSIC*/);
    if(pContext->mConfigPara.mbSetVideolayer)
    {
        pContext->mpPlayer->setDisplay(pContext->mVoLayer);
    }
    pContext->mpPlayer->setOutputPixelFormat(pContext->mConfigPara.mPicFormat);
    pContext->mpPlayer->enableScaleMode(true, pContext->mConfigPara.mVdecOutputMaxWidth, pContext->mConfigPara.mVdecOutputMaxHeight);
    pContext->mpPlayer->setRotation(pContext->mConfigPara.mRotation);
    pContext->mpPlayer->setLooping(pContext->mConfigPara.mbLoop);
    pContext->mpPlayer->prepare();
    pContext->mpPlayer->start();

    float leftVolume = 0, rightVolume = 0;
    pContext->mpPlayer->getVolume(&leftVolume, &rightVolume);
    alogd("[0]get volume:left[%f], right[%f]", leftVolume, rightVolume);
//    int i;
//    for(i=0; i<5; i++)
//    {
//        leftVolume = rightVolume = 1.0;
//        pContext->mpPlayer->setVolume(leftVolume, rightVolume);
//        alogd("[1]set volume:left[%f], right[%f]", leftVolume, rightVolume);
//        pContext->mpPlayer->getVolume(&leftVolume, &rightVolume);
//        alogd("[1]get volume:left[%f], right[%f]", leftVolume, rightVolume);
//        sleep(10);
//        leftVolume = rightVolume = 0.1;
//        pContext->mpPlayer->setVolume(leftVolume, rightVolume);
//        alogd("[2]set volume:left[%f], right[%f]", leftVolume, rightVolume);
//        pContext->mpPlayer->getVolume(&leftVolume, &rightVolume);
//        alogd("[2]get volume:left[%f], right[%f]", leftVolume, rightVolume);
//        sleep(10);
//    }

    //sleep(10);
    cdx_sem_down(&pContext->mSemExit);
    //close player
    pContext->mpPlayer->stop();
    pContext->mpPlayer->reset();
    delete pContext->mpPlayer;
    pContext->mpPlayer = NULL;
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
    cout<<"bye, sample_Player!"<<endl;
    return result;
}

