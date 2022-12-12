/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : StarkModel.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-10
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "StarkModel"
#include <utils/plat_log.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <fstream>

#include <hwdisplay.h>
#include <mpi_vo.h>
#include "StarkModel.h"
#include "stringOperate.h"
#include "BaseMessageHandler.h"
//#include <fb.h>
#include <EyeseePlayer.h>

using namespace std;

namespace EyeseeLinux {

static status_t parseFileList(char *pFileListPath, vector<string>& mediaFileList)
{
    mediaFileList.clear();
    if(NULL == pFileListPath || 0==strlen(pFileListPath))
    {
        aloge("fatal error! pFileListPath[%s] wrong!", pFileListPath);
        return UNKNOWN_ERROR;
    }
    string tmp;
    ifstream is(pFileListPath, ios_base::in);
    if(!is.is_open())
    {
        aloge("fatal error! open[%s] fail!", pFileListPath);
        goto _err0;
    }
    while(getline(is,tmp))
    {
        removeCharFromString(tmp, '\"');
        removeCharFromString(tmp, '\r');
        removeCharFromString(tmp, '\n');
        mediaFileList.push_back(tmp);
        //alogd("read file[%s], is.rdstate=%d", pFileListPath, is.rdstate());
    }
    //alogd("read file[%s], is.rdstate=%d", pFileListPath, is.rdstate());
    alogd("parse fileList[%s], get [%d]files", pFileListPath, mediaFileList.size());
    for(size_t i=0; i<mediaFileList.size(); i++)
    {
        alogd("file[%d]:[%s]", i, mediaFileList[i].c_str());
    }
    //is.close();
    return NO_ERROR;

_err0:
    return UNKNOWN_ERROR;
}

StarkModel::InnerThread::InnerThread(StarkModel *pOwner)
    :mpOwner(pOwner)
{
    mThreadId = 0;
}
void StarkModel::InnerThread::start()
{
    run("StarkModelThread");
}
void StarkModel::InnerThread::stop()
{
    requestExitAndWait();
}
status_t StarkModel::InnerThread::readyToRun()
{
    mThreadId = pthread_self();
    return Thread::readyToRun();
}
bool StarkModel::InnerThread::threadLoop()
{
    if(!exitPending())
    {
        return mpOwner->StarkModelThread();
    }
    else
    {
        return false;
    }
}

StarkModel::PlayerListener::PlayerListener(StarkModel *pOwner)
    : mpOwner(pOwner)
{
}

void StarkModel::PlayerListener::onPrepared(EyeseePlayer *pMp)
{
    alogd("receive onPrepared message!");
    Mutex::Autolock autoLock(mpOwner->mPlayerListenerLock);
    mpOwner->mbPrepareDone = true;
    mpOwner->mOnPrepared.signal();
}
void StarkModel::PlayerListener::onCompletion(EyeseePlayer *pMp)
{
    alogd("receive onCompletion message!");
    Mutex::Autolock autoLock(mpOwner->mPlayerListenerLock);
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_PLAYBACK_COMPLETED;
    mpOwner->mMsgQueue.queueMessage(&msgIn);
}
bool StarkModel::PlayerListener::onError(EyeseePlayer *pMp, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);    //MEDIA_ERROR_UNKNOWN
    Mutex::Autolock autoLock(mpOwner->mPlayerListenerLock);
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_ERROR_MEDIA_ERROR;
    msgIn.mPara0 = what;
    msgIn.mPara1 = extra;
    mpOwner->mMsgQueue.queueMessage(&msgIn);
    return true;
}
void StarkModel::PlayerListener::onVideoSizeChanged(EyeseePlayer *pMp, int width, int height)
{
    alogd("receive onVideoSizeChanged message!size[%dx%d]", width, height);
    Mutex::Autolock autoLock(mpOwner->mPlayerListenerLock);
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_MEDIA_SET_VIDEO_SIZE;
    msgIn.mPara0 = width;
    msgIn.mPara1 = height;
    mpOwner->mMsgQueue.queueMessage(&msgIn);
}
bool StarkModel::PlayerListener::onInfo(EyeseePlayer *pMp, int what, int extra)
{
    alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
    Mutex::Autolock autoLock(mpOwner->mPlayerListenerLock);
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_MEDIA_INFO;
    msgIn.mPara0 = what;
    msgIn.mPara1 = extra;
    mpOwner->mMsgQueue.queueMessage(&msgIn);
    return true;
}
void StarkModel::PlayerListener::onSeekComplete(EyeseePlayer *pMp)
{
    alogd("receive onSeekComplete message!");
    Mutex::Autolock autoLock(mpOwner->mPlayerListenerLock);
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_MEDIA_SEEK_COMPLETE;
    mpOwner->mMsgQueue.queueMessage(&msgIn);
}
StarkModel::StarkModel()
    : mPlayerListener(this)
{
    mOnMessageListener = NULL;
    mCMP = NULL;
    mHLay = -1;
    mPrerequisiteFlag = false;
    mPlayMode = PlayMode_OneFile;
    mCurMediaFileIndex = -1;
    
    mbPrepareDone = false;
    mCurrentState = STATE_IDLE;
    mpInnerThread = NULL;
}
StarkModel::~StarkModel()
{
    reset();
}

int StarkModel::getCurrentState()
{
    Mutex::Autolock autoLock(mLock);
    return mCurrentState;
}
status_t StarkModel::init()
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_INITIALIZED)
    {
        return NO_ERROR;
    }
    if(mCurrentState != STATE_IDLE)
    {
        aloge("init called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    
    mpInnerThread = new InnerThread(this);
    mpInnerThread->start();
    mCurrentState = STATE_INITIALIZED;
    return NO_ERROR;
}
status_t StarkModel::reset()
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_IDLE)
    {
        return NO_ERROR;
    }
    if(mpInnerThread!=NULL)
    {
        EyeseeMessage msgIn;
        msgIn.mWhoseMsg = MessageOfStarkModel;
        msgIn.mMsgType = MSGTYPE_QUIT_THREAD;
        mMsgQueue.queueMessage(&msgIn);
        mpInnerThread->stop();
        delete mpInnerThread;
        mpInnerThread = NULL;
    }
    mMsgQueue.flushMessage();
    mOnMessageListener = NULL;
    if(mPrerequisiteFlag)
    {
        mCMP->reset();
        delete mCMP;
        mCMP = NULL;

        //hwd_layer_close(mHLay);
        //hwd_layer_release(mHLay);
        AW_MPI_VO_DisableVideoLayer(mHLay);
        mHLay = -1;

        mPrerequisiteFlag = false;
    }
    mbPrepareDone = false;
    mPlayMode = PlayMode_OneFile;
    mMediaFileList.clear();
    mCurMediaFileIndex = -1;
    mCurrentState = STATE_IDLE;
    return NO_ERROR;
}

void StarkModel::setOnMessageListener(OnMessageListener *pListener)
{
    Mutex::Autolock autoLock(mLock);
    mOnMessageListener = pListener;
}

status_t StarkModel::playFile(char *pFilePath)
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_STARTED)
    {
        alogd("already state[0x%x], return immediately", mCurrentState);
        return NO_ERROR;
    }
    if(mCurrentState != STATE_INITIALIZED)
    {
        aloge("playFile called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    if(NULL==pFilePath || 0==strlen(pFilePath))
    {
        aloge("fatal error! pFilePath[%s] wrong!", pFilePath);
        return BAD_VALUE;
    }
    mPlayMode = PlayMode_OneFile;
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_PLAY_FILE;
    msgIn.setData(pFilePath, strlen(pFilePath)+1);
    mMsgQueue.queueMessage(&msgIn);
    mCurrentState = STATE_STARTED;
    return NO_ERROR;
}
status_t StarkModel::playFileLoop(char *pFilePath)
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_STARTED || mCurrentState == STATE_PAUSED)
    {
        alogd("already state[0x%x], reject operation, return immediately", mCurrentState);
        return NO_ERROR;
    }
    if(mCurrentState != STATE_INITIALIZED)
    {
        aloge("playFile called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    if(NULL==pFilePath || 0==strlen(pFilePath))
    {
        aloge("fatal error! pFilePath[%s] wrong!", pFilePath);
        return BAD_VALUE;
    }
    mPlayMode = PlayMode_OneFileLoop;
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_PLAY_FILE;
    msgIn.setData(pFilePath, strlen(pFilePath)+1);
    mMsgQueue.queueMessage(&msgIn);
    mCurrentState = STATE_STARTED;
    return NO_ERROR;
}

status_t StarkModel::playListLoop(char *pFileListPath)
{
    status_t ret;
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_STARTED || mCurrentState == STATE_PAUSED)
    {
        alogd("already state[0x%x], reject operation, return immediately", mCurrentState);
        return NO_ERROR;
    }
    if(mCurrentState != STATE_INITIALIZED)
    {
        aloge("playFile called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    if(NULL==pFileListPath || 0==strlen(pFileListPath))
    {
        aloge("fatal error! pFileListPath[%s] wrong!", pFileListPath);
        return BAD_VALUE;
    }
    mPlayMode = PlayMode_ListLoop;
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_PLAY_LIST_LOOP;
    msgIn.setData(pFileListPath, strlen(pFileListPath)+1);
    mMsgQueue.queueMessage(&msgIn);
    mCurrentState = STATE_STARTED;
    return NO_ERROR;
}

status_t StarkModel::pauseFile()
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_PAUSED)
    {
        alogd("already state[0x%x], return immediately", mCurrentState);
        return NO_ERROR;
    }
    if(mCurrentState != STATE_STARTED)
    {
        aloge("pauseFile called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_PAUSE_FILE;
    mMsgQueue.queueMessage(&msgIn);
    mExecuteDoneCondition.wait(mLock);
    mCurrentState = STATE_PAUSED;
    return NO_ERROR;
}

status_t StarkModel::resumeFile()
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_STARTED)
    {
        alogd("already state[0x%x], return immediately", mCurrentState);
        return NO_ERROR;
    }
    if(mCurrentState != STATE_PAUSED)
    {
        aloge("resumeFile called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    alogd("pause->play");
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_RESUME_FILE;
    mMsgQueue.queueMessage(&msgIn);
    mExecuteDoneCondition.wait(mLock);
    mCurrentState = STATE_STARTED;
    return NO_ERROR;
}
status_t StarkModel::seekFile(int msec)
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState != STATE_STARTED && mCurrentState != STATE_PAUSED)
    {
        aloge("seekFile called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_SEEK_FILE;
    msgIn.mPara0 = msec;
    mMsgQueue.queueMessage(&msgIn);
    mExecuteDoneCondition.wait(mLock);
    return NO_ERROR;
}
status_t StarkModel::stopFile()
{
    Mutex::Autolock autoLock(mLock);
    if(mCurrentState == STATE_INITIALIZED)
    {
        alogd("already state[0x%x], return immediately", mCurrentState);
        return NO_ERROR;
    }
    if(mCurrentState != STATE_STARTED && mCurrentState != STATE_PAUSED)
    {
        aloge("stopFile called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msgIn;
    msgIn.mWhoseMsg = MessageOfStarkModel;
    msgIn.mMsgType = MSGTYPE_STOP_FILE;
    mMsgQueue.queueMessage(&msgIn);
    mExecuteDoneCondition.wait(mLock);
    mCurrentState = STATE_INITIALIZED;
    return NO_ERROR;
}

status_t StarkModel::setVolume(float leftVolume, float rightVolume)
{
    Mutex::Autolock autoLock(mLock);
    return mCMP->setVolume(leftVolume, rightVolume);
}

status_t StarkModel::setMute(BOOL mute)
{
    Mutex::Autolock autoLock(mLock);
    return mCMP->setMuteMode(mute);
}

status_t StarkModel::StarkModelThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t ret;
    while(1)
    {
    PROCESS_MESSAGE:
        ret = mMsgQueue.dequeueMessage(&msg);
        if(ret != NO_ERROR)
        {
            if(NOT_ENOUGH_DATA == ret)
            {
                mMsgQueue.waitMessage();
                continue;
            }
            else
            {
                aloge("fatal error! message queue ret[0x%x], quit thread!", ret);
                bRunningFlag = false;
                goto _exit0;
            }
        }
        if(msg.mWhoseMsg != MessageOfStarkModel)
        {
            aloge("fatal error! msg[0x%x] come from [0x%x]!", msg.mMsgType, msg.mWhoseMsg);
        }
        switch(msg.mMsgType)
        {
            case MSGTYPE_PLAY_FILE:
            {
                Mutex::Autolock autoLock(mLock);
                if (access((char*)msg.getData(), F_OK) != 0) 
                {
                    aloge("fatal error! file[%s] is not exist!", (char*)msg.getData());
                    mOnMessageListener->onMessage(MSGTYPE_ERROR_INVALID_FILEPATH, NULL);
                    break;
                }
                if(mPlayMode == PlayMode_OneFileLoop)
                {
                    mMediaFileList.clear();
                    mMediaFileList.push_back(string((char*)msg.getData()));
                    mCurMediaFileIndex = 0;
                }
                executePlayFile((char*)msg.getData());
                break;
            }
            case MSGTYPE_PLAY_LIST_LOOP:
            {
                Mutex::Autolock autoLock(mLock);
                //parse filelist file, get filelist.
                ret = parseFileList((char*)msg.getData(), mMediaFileList);
                if(ret!=NO_ERROR)
                {
                    aloge("fatal error! parse file[%s] fail! ret[0x%x]", (char*)msg.getData(), ret);
                    mOnMessageListener->onMessage(MSGTYPE_ERROR_PARSE_FILE_LIST_FAIL, NULL);
                    break;
                }
                if(0 == mMediaFileList.size())
                {
                    alogd("listFile[%s] is empty!", (char*)msg.getData());
                    mOnMessageListener->onMessage(MSGTYPE_ERROR_FILELIST_EMPTY, NULL);
                    break;
                }
                mCurMediaFileIndex = 0;
                executePlayFile(mMediaFileList[mCurMediaFileIndex++].c_str());
                if(mCurMediaFileIndex >= (int)mMediaFileList.size())
                {
                    mCurMediaFileIndex = 0;
                }
                break;
            }
            case MSGTYPE_PAUSE_FILE:
            {
                Mutex::Autolock autoLock(mLock);
                if(STATE_PAUSED == mCurrentState)
                {
                    alogd("already pause!");
                }
                else if(STATE_STARTED == mCurrentState)
                {
                    mCMP->pause();
                }
                else
                {
                    aloge("fatal error! wrong state[%d] when process pause!", mCurrentState);
                }
                mExecuteDoneCondition.signal();
                break;
            }
            case MSGTYPE_RESUME_FILE:
            {
                Mutex::Autolock autoLock(mLock);
                if(mCurrentState == STATE_PAUSED)
                {
                    mCMP->start();
                }
                else
                {
                    aloge("fatal error! wrong state[%d] when resume file!", mCurrentState);
                }
                mExecuteDoneCondition.signal();
                break;
            }
            case MSGTYPE_SEEK_FILE:
            {
                Mutex::Autolock autoLock(mLock);
                alogd("seek [%d]ms!", msg.mPara0);
                mCMP->seekTo(msg.mPara0);
                break;
            }
            case MSGTYPE_STOP_FILE:
            {
                Mutex::Autolock autoLock(mLock);
                if(STATE_INITIALIZED== mCurrentState)
                {
                    alogd("already stop!");
                }
                else if(STATE_STARTED == mCurrentState || STATE_PAUSED == mCurrentState)
                {
                    if(mPrerequisiteFlag)
                    {
                        mCMP->reset();
                    }
                }
                else
                {
                    aloge("fatal error! wrong state[%d] when process pause!", mCurrentState);
                }
                mExecuteDoneCondition.signal();
                break;
            }
            case MSGTYPE_QUIT_THREAD:
            {
                bRunningFlag = false;
                goto _exit0;
            }
            case MSGTYPE_PLAYBACK_COMPLETED:
            {
                Mutex::Autolock autoLock(mLock);
                if(STATE_PAUSED == mCurrentState)
                {
                    alogw("Be careful! meet pause when play completed");
                }
                if(PlayMode_OneFile == mPlayMode)
                {
                    alogd("play one file done!");
                    mCMP->reset();
                    mOnMessageListener->onMessage(MSGTYPE_PLAYBACK_COMPLETED, NULL);
                }
                else if(PlayMode_OneFileLoop == mPlayMode || PlayMode_ListLoop == mPlayMode)
                {
                    alogd("play next file[%s] in mediaList!", mMediaFileList[mCurMediaFileIndex].c_str());
                    mCMP->reset();
                    executePlayFile(mMediaFileList[mCurMediaFileIndex++].c_str());
                    if(mCurMediaFileIndex >= (int)mMediaFileList.size())
                    {
                        mCurMediaFileIndex = 0;
                    }
                }
                break;
            }
            case MSGTYPE_ERROR_MEDIA_ERROR:
            {
                Mutex::Autolock autoLock(mLock);
                struct MediaErrorInfo mediaErrorInfo;
                mediaErrorInfo.mMediaErrorType = msg.mPara0;
                mediaErrorInfo.mExtra = msg.mPara1;
                mOnMessageListener->onMessage(MSGTYPE_ERROR_MEDIA_ERROR, &mediaErrorInfo);
                break;
            }
            case MSGTYPE_MEDIA_SET_VIDEO_SIZE:
            {
                Mutex::Autolock autoLock(mLock);
                alogd("ignore video size changed!");
                break;
            }
            case MSGTYPE_MEDIA_INFO:
            {
                Mutex::Autolock autoLock(mLock);
                alogd("ignore media info!");
                break;
            }
            case MSGTYPE_MEDIA_SEEK_COMPLETE:
            {
                Mutex::Autolock autoLock(mLock);
                alogd("signal seek complete!");
                mExecuteDoneCondition.signal();
                break;
            }
            default:
            {
                aloge("unknown msg[0x%x]!", msg.mMsgType);
                break;
            }
        }
    }
_exit0:
    alogd("exit StarkModelThread()");
    return bRunningFlag;
}

status_t StarkModel::executePlayFile(const char *pFilePath)
{
    if(mCurrentState != STATE_STARTED)
    {
        aloge("fatal error! why state[0x%x] is not started?", mCurrentState);
    }
    if(false == mPrerequisiteFlag)
    {
        mCMP = new EyeseePlayer;

        alogd("------------hdmi setting------------------");
        VO_PUB_ATTR_S voattr;
        voattr.enIntfType = VO_INTF_LCD;
        voattr.enIntfSync = VO_OUTPUT_1080P60;
        AW_MPI_VO_SetPubAttr(0, &voattr);
        alogd("------------hdmi setting end------------------");

        alogd("screenInfo[%dx%d]", 800, 600);
        struct view_info sur;
        sur.x = 0;
        sur.y = 0;
        sur.w = 720;
        sur.h = 720;
        //mHLay = hwd_layer_request_v40(&sur, 1);
        mHLay = 0;
        while(mHLay < VO_MAX_LAYER_NUM)
        {
            if(SUCCESS == AW_MPI_VO_EnableVideoLayer(mHLay))
            {
                break;
            }
            mHLay++;
        }
        if(mHLay >= VO_MAX_LAYER_NUM)
        {
            aloge("fatal error! enable video layer fail!");
        }
        VO_VIDEO_LAYER_ATTR_S stLayerAttr;
        AW_MPI_VO_GetVideoLayerAttr(mHLay, &stLayerAttr);
        stLayerAttr.stDispRect.X = sur.x;
        stLayerAttr.stDispRect.Y = sur.y;
        stLayerAttr.stDispRect.Width = sur.w;
        stLayerAttr.stDispRect.Height = sur.h;
        AW_MPI_VO_SetVideoLayerAttr(mHLay, &stLayerAttr);
        alogd("requestSurface hlay=%d", mHLay);
        mPrerequisiteFlag = true;
    }
    mCMP->setOnPreparedListener(&mPlayerListener);
    mCMP->setOnCompletionListener(&mPlayerListener);
    mCMP->setOnErrorListener(&mPlayerListener);
    mCMP->setOnVideoSizeChangedListener(&mPlayerListener);
    mCMP->setOnInfoListener(&mPlayerListener);
    mCMP->setOnSeekCompleteListener(&mPlayerListener);
    mCMP->setDataSource(pFilePath);
    //mCMP->setAudioStreamType(AUDIO_STREAM_MUSIC/*AudioManager.STREAM_MUSIC*/);
    mCMP->setDisplay(mHLay);
    //mCMP->setAOCardType(PCM_CARD_TYPE_SNDHDMI);
    //mCMP->setRotation(ROTATE_90);
    mCMP->setOutputPixelFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    //mCMP->setLooping(PlayMode_OneFileLoop==mPlayMode?true:false);
    //mCMP->setScreenOnWhilePlaying(true);
    {
        Mutex::Autolock autoLock(mPlayerListenerLock);
        mCMP->prepare();
//        mbPrepareDone = false;
//        while(false == mbPrepareDone)
//        {
//            mOnPrepared.wait(mPlayerListenerLock);
//        }
    }
    mCMP->start();
    return NO_ERROR;
}

};
