/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : LannisterViewMessageHandler.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-8
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "LannisterViewMessageHandler"
#include <utils/plat_log.h>

#include "LannisterViewMessageHandler.h"

namespace EyeseeLinux {

bool LannisterViewMessageHandler::matchMessageHandler(EyeseeMessage *pMsg)
{
    if(MessageOfLannisterView == pMsg->mWhoseMsg)
    {
        return true;
    }
    else
    {
        return false;
    }
}

status_t LannisterViewMessageHandler::handleMessage(EyeseeMessage *pMsg)
{
    status_t ret = NO_ERROR;
    if(pMsg->mWhoseMsg != MessageOfLannisterView)
    {
        aloge("fatal error! wrong msg[0x%x]of[0x%x] come!", pMsg->mMsgType, pMsg->mWhoseMsg);
        return BAD_TYPE;
    }
    switch(pMsg->mMsgType)
    {
        case LannisterView::MSGTYPE_SHOW_HELP:
        {
            mpTCtrl->mpCommandReceiver->showCommandList();
            break;
        }
        case LannisterView::MSGTYPE_PLAY_FILE:
        {
            char *pFilePath = (char*)pMsg->getData();
            mpTCtrl->mpPlayer->playFile(pFilePath);
            break;
        }
        case LannisterView::MSGTYPE_PLAY_FILE_LOOP:
        {
            char *pFilePath = (char*)pMsg->getData();
            mpTCtrl->mpPlayer->playFileLoop(pFilePath);
            break;
        }
        case LannisterView::MSGTYPE_PLAY_LIST_LOOP:
        {
            char *pFileListPath = (char*)pMsg->getData();
            mpTCtrl->mpPlayer->playListLoop(pFileListPath);
            break;
        }
        case LannisterView::MSGTYPE_PAUSE:
        {
            mpTCtrl->mpPlayer->pauseFile();
            break;
        }
        case LannisterView::MSGTYPE_RESUME:
        {
            mpTCtrl->mpPlayer->resumeFile();
            break;
        }
        case LannisterView::MSGTYPE_SEEK:
        {
            mpTCtrl->mpPlayer->seekFile(pMsg->mPara0);
            break;
        }
        case LannisterView::MSGTYPE_STOP:
        {
            mpTCtrl->mpPlayer->stopFile();
            break;
        }
        case LannisterView::MSGTYPE_EXIT:
        {
            alogd("receive exit command, process!");
            mpTCtrl->mpCommandReceiver->reset();
            mpTCtrl->mpPlayer->reset();
            mpTCtrl->mThreadQuitFlag = true;
            break;
        }
        case LannisterView::MSGTYPE_SETVOLUME:
        {
            float volumeVal = reinterpret_cast<float&>(pMsg->mPara0);
            alogd("receive setVolume command, volume[%f]!", volumeVal);
            mpTCtrl->mpPlayer->setVolume(volumeVal, volumeVal);
            break;
        }
        case LannisterView::MSGTYPE_SETMUTE:
        {
            int volumeVal = pMsg->mPara0;
            alogd("receive setMute command, volume[%d]!", volumeVal);
            mpTCtrl->mpPlayer->setMute((BOOL)volumeVal);
            break;
        }
        default:
        {
            aloge("fatal error! unknown msgType[%d]", pMsg->mMsgType);
            ret = BAD_TYPE;
            break;
        }
    }
    return ret;
}

LannisterViewMessageHandler::LannisterViewMessageHandler(TargaryenControl* pTCtrl)
    : mpTCtrl(pTCtrl)
{
}

};

