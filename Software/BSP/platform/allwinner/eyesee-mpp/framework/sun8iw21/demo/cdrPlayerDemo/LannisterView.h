/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : LannisterView.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-6
* Description:
********************************************************************************
*/
#ifndef _LANNISTERVIEW_H_
#define _LANNISTERVIEW_H_

#include <string>
#include <vector>

#include <utils/Thread.h>
#include "BaseView.h"
#include "EyeseeMessageQueue.h"

namespace EyeseeLinux {

class LannisterView : public BaseView
{
public:
    LannisterView();
    virtual ~LannisterView(){}
    virtual void setOnMessageListener(OnMessageListener *pListener);
    enum State
    {
        STATE_ERROR = -1,
        STATE_IDLE = 0,
        STATE_RUN = 1,
    };
    status_t start();
    status_t reset();
    enum MessageType
    {
        MSGTYPE_SHOW_HELP = 1,
        MSGTYPE_PLAY_FILE,
        MSGTYPE_PLAY_FILE_LOOP,
        MSGTYPE_PLAY_LIST_LOOP,
        MSGTYPE_PAUSE,
        MSGTYPE_RESUME,
        MSGTYPE_SEEK,
        MSGTYPE_STOP,
        MSGTYPE_EXIT,
        MSGTYPE_SETVOLUME,
        MSGTYPE_SETMUTE,
    };
    void showCommandList();
    void showStringList(std::vector<std::string>& stringList);
private:
    status_t stop_l();
    bool runOnce();
    class InnerThread : public Thread
    {
    public:
        InnerThread(LannisterView *pOwner);
        void start();
        void stop();
        virtual status_t readyToRun();
        virtual bool threadLoop();
    private:
        LannisterView* const mpOwner;
        pthread_t mThreadId;
    };
    class CommandList
    {
    public:
        static const std::string    Cmd_Help;
        static const std::string    Cmd_PlayFile;
        static const std::string    Cmd_PlayFileLoop;
        static const std::string    Cmd_PlayListLoop;
        static const std::string    Cmd_Pause;
        static const std::string    Cmd_Resume;
        static const std::string    Cmd_Seek;
        static const std::string    Cmd_Stop;
        static const std::string    Cmd_Exit;
        static const std::string    Cmd_SetVolume;
        static const std::string    Cmd_SetMute;
    private:
        CommandList(){};
    };
    Mutex mLock;
    State mState;
    OnMessageListener *mOnMessageListener;
    InnerThread *mpInnerThread;
    EyeseeMessageQueue mMsgQueue;
    bool mHintShowed;
    bool mbRejectUserIpnut;
};

};

#endif  /* _LANNISTERVIEW_H_ */

