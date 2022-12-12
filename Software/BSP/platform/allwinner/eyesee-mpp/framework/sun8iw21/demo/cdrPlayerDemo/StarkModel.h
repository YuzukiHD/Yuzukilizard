/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : StarkModel.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-10
* Description:
********************************************************************************
*/
#ifndef _STARKMODEL_H_
#define _STARKMODEL_H_

#include <thread_db.h>
#include <vector>
#include <string>

#include "BaseModel.h"
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <utils/Thread.h>
#include "EyeseeMessageQueue.h"
#include <EyeseePlayer.h>

namespace EyeseeLinux {

class StarkModel : public BaseModel
{
    // ref to frameworks/base/core/java/android/widget/VideoView.java
public:
    StarkModel();
    virtual ~StarkModel();

    enum State
    {
        STATE_ERROR = -1,
        STATE_IDLE = 0,
        STATE_INITIALIZED = 1,
        STATE_STARTED = 4,
        STATE_PAUSED = 5,
    };
    int getCurrentState();    //StarkModel::State::STATE_IDLE
    status_t init();
    status_t reset();
    struct MediaErrorInfo   //for MSGTYPE_ERROR_MEDIA_ERROR
    {
        int mMediaErrorType;    //enum media_error_type
        int mExtra;
    };
    enum MessageType
    {
        MSGTYPE_PLAY_FILE = 1,
        MSGTYPE_PLAY_LIST_LOOP,
        MSGTYPE_PAUSE_FILE,
        MSGTYPE_RESUME_FILE,
        MSGTYPE_SEEK_FILE,
        MSGTYPE_STOP_FILE,
        MSGTYPE_QUIT_THREAD,
        MSGTYPE_PLAYBACK_COMPLETED,
        MSGTYPE_MEDIA_SET_VIDEO_SIZE,
        MSGTYPE_MEDIA_INFO, //MEDIA_INFO_UNKNOWN
        MSGTYPE_MEDIA_SEEK_COMPLETE,
        MSGTYPE_ERROR_MEDIA_ERROR,  //MEDIA_ERROR_UNKNOWN, ...
        MSGTYPE_ERROR_UNKNOWN,
        MSGTYPE_ERROR_INVALID_FILEPATH,
        MSGTYPE_ERROR_PREPARE_FAIL,
        MSGTYPE_ERROR_PARSE_FILE_LIST_FAIL,
        MSGTYPE_ERROR_FILELIST_EMPTY,
        MSGTYPE_ERROR_PLAY_FAIL,
    };
    virtual void setOnMessageListener(OnMessageListener *pListener);

    status_t playFile(char *pFilePath = NULL);
    status_t playFileLoop(char *pFilePath);
    status_t playListLoop(char *pFileListPath);
    status_t pauseFile();
    status_t resumeFile();
    status_t seekFile(int msec);    //unit: ms
    status_t stopFile();
    status_t setVolume(float leftVolume, float rightVolume);
    status_t setMute(BOOL mute);
private:
    status_t StarkModelThread();
    status_t executePlayFile(const char *pFilePath);
    OnMessageListener *mOnMessageListener;
    Mutex   mLock;
    bool    mbPrepareDone;
    Mutex       mPlayerListenerLock;
    Condition   mOnPrepared;
    int     mCurrentState;  //StarkModel::State::STATE_IDLE
    bool    mPrerequisiteFlag;
    EyeseeMessageQueue mMsgQueue;
    Condition   mExecuteDoneCondition;

    enum PlayMode
    {
        PlayMode_OneFile = 0,
        PlayMode_OneFileLoop,
        PlayMode_ListLoop,
    };
    PlayMode    mPlayMode;
    int mCurMediaFileIndex;
    std::vector<std::string> mMediaFileList;
    class InnerThread : public Thread
    {
    public:
        InnerThread(StarkModel *pOwner);
        void start();
        void stop();
        virtual status_t readyToRun() ;
        virtual bool threadLoop();
    private:
        StarkModel* const mpOwner;
        thread_t mThreadId;
    };
    InnerThread *mpInnerThread;
    class PlayerListener : public EyeseePlayer::OnPreparedListener
							, public EyeseePlayer::OnCompletionListener
							, public EyeseePlayer::OnErrorListener
							, public EyeseePlayer::OnVideoSizeChangedListener
							, public EyeseePlayer::OnInfoListener
							, public EyeseePlayer::OnSeekCompleteListener
	{
	public:
		PlayerListener(StarkModel *pOwner);
		void onPrepared(EyeseePlayer *pMp);
		void onCompletion(EyeseePlayer *pMp);
		bool onError(EyeseePlayer *pMp, int what, int extra);
		void onVideoSizeChanged(EyeseePlayer *pMp, int width, int height);
		bool onInfo(EyeseePlayer *pMp, int what, int extra);
		void onSeekComplete(EyeseePlayer *pMp);
	private:
		StarkModel *const mpOwner;
	};
    PlayerListener mPlayerListener;
    EyeseePlayer *mCMP;
    int mHLay;
};

};

#endif  /* _STARKMODEL_H_ */

