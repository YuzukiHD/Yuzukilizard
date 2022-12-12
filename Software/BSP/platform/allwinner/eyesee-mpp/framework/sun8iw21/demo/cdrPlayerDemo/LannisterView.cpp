/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : LannisterView.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-8
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "LannisterView"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>

#include <limits> 
#include <iostream>
#include <string>

#include "LannisterView.h"
#include "BaseMessageHandler.h"
#include "stringOperate.h"

using namespace std;

namespace EyeseeLinux {

const string LannisterView::CommandList::Cmd_Help           ("help");
const string LannisterView::CommandList::Cmd_PlayFile       ("playfile");
const string LannisterView::CommandList::Cmd_PlayFileLoop   ("playfileloop");
const string LannisterView::CommandList::Cmd_PlayListLoop   ("playlistloop");
const string LannisterView::CommandList::Cmd_Pause          ("pause");
const string LannisterView::CommandList::Cmd_Resume         ("resume");
const string LannisterView::CommandList::Cmd_Seek           ("seek");
const string LannisterView::CommandList::Cmd_Stop           ("stop");
const string LannisterView::CommandList::Cmd_Exit           ("exit");
const string LannisterView::CommandList::Cmd_SetVolume      ("setvolume");
const string LannisterView::CommandList::Cmd_SetMute        ("setmute");

LannisterView::InnerThread::InnerThread(LannisterView *pOwner)
    : mpOwner(pOwner)
{
    mThreadId = -1;
}
void LannisterView::InnerThread::start()
{
    run("LannisterViewThread");
}
void LannisterView::InnerThread::stop()
{
    requestExitAndWait();
}
status_t LannisterView::InnerThread::readyToRun()
{
    mThreadId = pthread_self();

    return Thread::readyToRun();
}
bool LannisterView::InnerThread::threadLoop()
{
    if(!exitPending())
    {
        return mpOwner->runOnce();
    }
    else
    {
        return false;
    }
}
        
LannisterView::LannisterView()
{
    mState = STATE_IDLE;
    mOnMessageListener = NULL;
    mHintShowed = false;
    mbRejectUserIpnut = false;
    mpInnerThread = NULL;
}
void LannisterView::setOnMessageListener(OnMessageListener *pListener)
{
    mOnMessageListener = pListener;
}

status_t LannisterView::start()
{
    Mutex::Autolock autoLock(mLock);
    if(mState == STATE_RUN)
    {
        return NO_ERROR;
    }
    if(mState != STATE_IDLE)
    {
        aloge("start called in an invalid state: %d", mState);
        return INVALID_OPERATION;
    }
    mpInnerThread = new InnerThread(this);
    mpInnerThread->start();
    mState = STATE_RUN;
    return NO_ERROR;
}

status_t LannisterView::reset()
{
    Mutex::Autolock autoLock(mLock);
    if(mState == STATE_IDLE)
    {
        return NO_ERROR;
    }
    if (STATE_RUN == mState)
    {
        status_t ret;
        alogd("state[0x%x]->Idle", mState);
        ret = stop_l();
        if(ret != NO_ERROR)
        {
            aloge("fatal error! state[0x%x]->Idle fail!", mState);
        }
    }
    return NO_ERROR;
}
void LannisterView::showCommandList()
{
    string strLine;
    strLine = "command list:";
    cout<<strLine<<endl;
    
    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_Help;
    cout<<strLine<<endl;
    
    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_PlayFile;
    strLine += "\t";
    strLine += "e.g., ";
    strLine += CommandList::Cmd_PlayFile;
    strLine += " /mnt/extsd/xxx.mp4";
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_PlayFileLoop;
    strLine += "\t";
    strLine += "e.g., ";
    strLine += CommandList::Cmd_PlayFileLoop;
    strLine += " /mnt/extsd/xxx.mp4";
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_PlayListLoop;
    strLine += "\t";
    strLine += "e.g., ";
    strLine += CommandList::Cmd_PlayListLoop;
    strLine += " /mnt/extsd/fileList.txt";
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_Pause;
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_Resume;
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_Seek;
    strLine += "\t";
    strLine += "e.g., ";
    strLine += CommandList::Cmd_Seek;
    strLine += " 5000";
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_Stop;
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_Exit;
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_SetVolume;
    strLine += "\t";
    strLine += "e.g., ";
    strLine += CommandList::Cmd_SetVolume;
    strLine += " 0.3";
    cout<<strLine<<endl;

    strLine.clear();
    strLine += "\t";
    strLine += CommandList::Cmd_SetMute;
    strLine += "\t";
    strLine += "\t";
    strLine += "e.g., ";
    strLine += CommandList::Cmd_SetMute;
    strLine += " 1/0";
    cout<<strLine<<endl;
}

void LannisterView::showStringList(vector<string>& stringList)
{
    for(size_t i=0; i<stringList.size(); i++)
    {
        cout<<stringList[i].c_str()<<endl;
    }
}

status_t LannisterView::stop_l()
{
    if(STATE_IDLE == mState)
    {
        return NO_ERROR;
    }
    if(mState != STATE_RUN)
    {
        aloge("called in invalid state[0x%x]", mState);
        return INVALID_OPERATION;
    }
    if (mpInnerThread == NULL) 
    {
        aloge("fatal error! LannisterViewThread is null?");
    }
    else
    {
        //send stop msg
        EyeseeMessage msgIn;
        msgIn.mWhoseMsg = MessageOfLannisterView;
        msgIn.mMsgType = MSGTYPE_STOP;
        mMsgQueue.queueMessage(&msgIn);
        mpInnerThread->stop();
        delete mpInnerThread;
        mpInnerThread = NULL;
    }
    mMsgQueue.flushMessage();
    mHintShowed = false;
    mbRejectUserIpnut = false;
    mState = STATE_IDLE;
    return NO_ERROR;
}

/*******************************************************************************
Function name: android.LannisterView.runOnce
Description: 
    
Parameters: 
    
Return: 
    true: want thread loop again.
    false: want finish this thread.
Time: 2015/7/8
*******************************************************************************/
bool LannisterView::runOnce()
{
    bool bRunningFlag = true;
    string::size_type pos;
    string usrInputString;
    EyeseeMessage msg;
    if(false == mHintShowed)
    {
        cout<<"welcome to cdrPlayerDemo, input \"help\" to display command list."<<endl;
        mHintShowed = true;
    }
    PROCESS_MESSAGE:
    if(NO_ERROR == mMsgQueue.dequeueMessage(&msg))
    {
        if(msg.mWhoseMsg != MessageOfLannisterView)
        {
            aloge("fatal error! msg[0x%x] come from [0x%x]!", msg.mMsgType, msg.mWhoseMsg);
        }
        switch(msg.mMsgType)
        {
            case MSGTYPE_STOP:
            {
                bRunningFlag = false;
                goto _exit0;
            }
            default:
            {
                aloge("unknown msg[0x%x]!", msg.mMsgType);
                break;
            }
        }
        //precede to process message
        goto PROCESS_MESSAGE;
    }
    if(mbRejectUserIpnut)
    {
        mMsgQueue.waitMessage();
        goto PROCESS_MESSAGE;
    }
    //read user input
    while(!getline(cin, usrInputString))
    {
        cout<<"[1]cin.rdstate="<<cin.rdstate()<<endl;
        cin.clear();
        cin.ignore(numeric_limits<std::streamsize>::max(), '\n');
        cout<<"[2]cin.rdstate="<<cin.rdstate()<<endl;
        cout<<"input wrong, please input again!"<<endl;
    }
    
    removeCharFromString(usrInputString, '\"');
    removeCharFromString(usrInputString, '\r');
    removeCharFromString(usrInputString, '\n');
    pos = usrInputString.find(' ');
    //parse user input.
    if(!usrInputString.compare(0, pos, CommandList::Cmd_Help))
    {
        mOnMessageListener->onMessage(MSGTYPE_SHOW_HELP, NULL);
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_PlayFile))
    {
        string::size_type filePathPos = usrInputString.rfind(' ');
        if(filePathPos!=string::npos)
        {
            if(filePathPos+1 < usrInputString.size())
            {
                mOnMessageListener->onMessage(MSGTYPE_PLAY_FILE, (void*)(usrInputString.c_str()+filePathPos+1));
            }
            else
            {
                alogd("user input filePath[%s] is invalid, ignore!", usrInputString.c_str());
            }
        }
        else
        {
            alogd("user input filePath[%s] is invalid, ignore!", usrInputString.c_str());
        }
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_PlayFileLoop))
    {
        string::size_type filePathPos = usrInputString.rfind(' ');
        if(filePathPos!=string::npos)
        {
            if(filePathPos+1 < usrInputString.size())
            {
                mOnMessageListener->onMessage(MSGTYPE_PLAY_FILE_LOOP, (void*)(usrInputString.c_str()+filePathPos+1));
            }
            else
            {
                alogd("user input filePath is invalid, ignore!");
            }
        }
        else
        {
            alogd("user not input filePath, ignore!");
        }
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_PlayListLoop))
    {
        string::size_type fileListPos = usrInputString.rfind(' ');
        if(fileListPos!=string::npos)
        {
            if(fileListPos+1 < usrInputString.size())
            {
                mOnMessageListener->onMessage(MSGTYPE_PLAY_LIST_LOOP, (void*)(usrInputString.c_str()+fileListPos+1));
            }
            else
            {
                alogd("user input fileListPath is invalid, ignore!");
            }
        }
        else
        {
            alogd("user not input fileListPath, ignore!");
        }
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_Pause))
    {
        mOnMessageListener->onMessage(MSGTYPE_PAUSE, NULL);
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_Resume))
    {
        mOnMessageListener->onMessage(MSGTYPE_RESUME, NULL);
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_Seek))
    {
        int seekMs = -1;
        string::size_type tmPos = usrInputString.rfind(' ');
        if(tmPos!=string::npos)
        {
            if(tmPos+1 < usrInputString.size())
            {
                seekMs = atoi(usrInputString.c_str()+tmPos+1);
                if(seekMs < 0)
                {
                    aloge("fatal error! seekMs[%d] wrong", seekMs);
                }
            }
        }
        if(seekMs >=0)
        {
            mOnMessageListener->onMessage(MSGTYPE_SEEK, (void*)seekMs);
        }
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_Stop))
    {
        mOnMessageListener->onMessage(MSGTYPE_STOP, NULL);
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_Exit))
    {
        mOnMessageListener->onMessage(MSGTYPE_EXIT, NULL);
        mbRejectUserIpnut = true;
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_SetVolume))
    {
        float volumeVal;
        bool bValidFlag = false;
        string::size_type volumePos = usrInputString.rfind(' ');
        if(volumePos!=string::npos)
        {
            if(volumePos+1 < usrInputString.size())
            {
                volumeVal = atof(usrInputString.c_str()+volumePos+1);
                bValidFlag = true;
            }
        }
        if(bValidFlag)
        {
            if(volumeVal >= 0.0 && volumeVal<=1.0)
            {
                mOnMessageListener->onMessage(MSGTYPE_SETVOLUME, reinterpret_cast<void*&>(volumeVal));
            }
        }
    }
    else if(!usrInputString.compare(0, pos, CommandList::Cmd_SetMute))
    {
        int volumeVal;
        bool bValidFlag = false;
        string::size_type volumePos = usrInputString.rfind(' ');
        if(volumePos!=string::npos)
        {
            if(volumePos+1 < usrInputString.size())
            {
                volumeVal = atoi(usrInputString.c_str()+volumePos+1);
                bValidFlag = true;
            }
        }
        if(bValidFlag)
        {
            if(volumeVal >= 1)
            {
                mOnMessageListener->onMessage(MSGTYPE_SETMUTE, reinterpret_cast<void*>(1));
            }
            else
            {
                mOnMessageListener->onMessage(MSGTYPE_SETMUTE, reinterpret_cast<void*>(0));
            }
        }
    }
    else
    {
        //cout<<"usrInputString=["<<usrInputString<<"]\n"<<"unknown command, call shell to process it:)"<<endl;
        system(usrInputString.c_str());
    }
    return bRunningFlag;
_exit0:
    alogd("exit LannisterViewThread()");
    return bRunningFlag;
}

};

