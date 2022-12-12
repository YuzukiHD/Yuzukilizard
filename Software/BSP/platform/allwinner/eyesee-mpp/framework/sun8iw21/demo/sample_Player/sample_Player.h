
#ifndef _SAMPLE_PLAYER_H_
#define _SAMPLE_PLAYER_H_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseePlayer.h>

typedef struct SamplePlayerCmdLineParam
{
    std::string mConfigFilePath;
}SamplePlayerCmdLineParam;

typedef struct SamplePlayerConfig
{
    std::string mFilePath;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mVdecOutputMaxWidth;    //0 means no restraint
    int mVdecOutputMaxHeight;
    int mDisplayWidth;
    int mDisplayHeight;
    bool mbForbidAudio;
    ROTATE_E mRotation;  //clock wise: 0:0, 1:90, 2:180, 3:270
    bool mbSetVideolayer;   //true:set valid video layer, false: not set layer
    VO_INTF_TYPE_E mIntfType;   //VO_INTF_LCD
    VO_INTF_SYNC_E mIntfSync;
    int mVeFreq;    //unit:MHz, 0: use default value
    bool mbLoop;    //true: loop forever; false: not loop
    int mSelectTrackIndex;
}SamplePlayerConfig;

class SamplePlayerContext;

class EyeseePlayerListener : public EyeseeLinux::EyeseePlayer::OnCompletionListener
                            , public EyeseeLinux::EyeseePlayer::OnErrorListener
                            , public EyeseeLinux::EyeseePlayer::OnVideoSizeChangedListener
                            , public EyeseeLinux::EyeseePlayer::OnInfoListener
                            , public EyeseeLinux::EyeseePlayer::OnSeekCompleteListener
{
public:
    EyeseePlayerListener(SamplePlayerContext *pOwner);
    void onCompletion(EyeseeLinux::EyeseePlayer *pMp);
    bool onError(EyeseeLinux::EyeseePlayer *pMp, int what, int extra);
    void onVideoSizeChanged(EyeseeLinux::EyeseePlayer *pMp, int width, int height);
    bool onInfo(EyeseeLinux::EyeseePlayer *pMp, int what, int extra);
    void onSeekComplete(EyeseeLinux::EyeseePlayer *pMp);
private:
    SamplePlayerContext *const mpOwner;
};

class SamplePlayerContext
{
public:
    SamplePlayerContext();
    ~SamplePlayerContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    SamplePlayerCmdLineParam mCmdLinePara;
    SamplePlayerConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    EyeseeLinux::EyeseePlayer *mpPlayer;
    EyeseePlayerListener mPlayerListener;
    int mLoopNum;
};

#endif  /* _SAMPLE_PLAYER_H_ */

