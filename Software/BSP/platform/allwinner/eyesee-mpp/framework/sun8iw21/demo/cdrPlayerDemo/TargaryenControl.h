/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : TargaryenControl.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-6
* Description:
********************************************************************************
*/
#ifndef _TARGARYENCONTROL_H_
#define _TARGARYENCONTROL_H_

#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include "BaseMessageHandler.h"
#include "LannisterView.h"
#include "StarkModel.h"

namespace EyeseeLinux {

class TargaryenControl
{
public:
    TargaryenControl();
    ~TargaryenControl();
    enum State
    {
        STATE_ERROR = -1,
        STATE_IDLE = 0,
        STATE_INITIALIZED = 1,
        STATE_RUNNING = 2,
    };
    enum MessageType
    {
        MSGTYPE_QUIT_THREAD = 1,
    };
    status_t init();
    status_t reset();
    status_t threadRunning();
private:
    State       mState; //TargaryenControl::State::IDLE
    Mutex       mLock;
    bool        mWaitThreadQuitFlag;
    Condition   mSignalThreadQuit;
    LannisterView *mpCommandReceiver;
    StarkModel  *mpPlayer;  //for test player
    //ArrynModel  *mpRecorder; //for test recorder.
    bool        mThreadQuitFlag;
    EyeseeMessageQueue mMsgQueue;
    int mUILayer;
    
    class LannisterViewListener : public BaseView::OnMessageListener
    {
    public:
        LannisterViewListener(TargaryenControl *pTCtrl);
        virtual status_t onMessage(int msg, void* pMsgData);
    private:
		TargaryenControl * const mpTCtrl;
    };
    LannisterViewListener    *mpLannisterViewListener;

    class StarkModelListener : public BaseModel::OnMessageListener
    {
    public:
        StarkModelListener(TargaryenControl *pTCtrl);
        virtual status_t onMessage(int msg, void* pMsgData);
    private:
		TargaryenControl * const mpTCtrl;
    };
    StarkModelListener   *mpStarkModelListener;

    friend class LannisterViewMessageHandler;
    friend class StarkModelMessageHandler;
    friend class TargaryenControlMessageHandler;
    std::vector<BaseMessageHandler*> mMessageHandlerArray;
};

};

#endif  /* _TARGARYENCONTROL_H_ */

