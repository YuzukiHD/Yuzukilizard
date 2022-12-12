/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : TargaryenControlMessageHandler.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-10
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "TargaryenControlMessageHandler"
#include <utils/plat_log.h>

#include "TargaryenControlMessageHandler.h"

namespace EyeseeLinux {

bool TargaryenControlMessageHandler::matchMessageHandler(EyeseeMessage *pMsg)
{
    if(MessageOfTargaryenControl == pMsg->mWhoseMsg)
    {
        return true;
    }
    else
    {
        return false;
    }
}

status_t TargaryenControlMessageHandler::handleMessage(EyeseeMessage *pMsg)
{
    status_t ret = NO_ERROR;
    if(pMsg->mWhoseMsg != MessageOfTargaryenControl)
    {
        aloge("(f:%s, l:%d) fatal error! wrong msg[0x%x]of[0x%x] come!", __FUNCTION__,__LINE__, pMsg->mMsgType, pMsg->mWhoseMsg);
        return BAD_TYPE;
    }
    switch(pMsg->mMsgType)
    {
        case TargaryenControl::MSGTYPE_QUIT_THREAD:
        {
            mpTCtrl->mThreadQuitFlag = true;
            break;
        }
        default:
        {
            aloge("(f:%s, l:%d) fatal error! unknown msgType[%d]", __FUNCTION__,__LINE__, pMsg->mMsgType);
            ret = BAD_TYPE;
            break;
        }
    }
    return ret;
}

TargaryenControlMessageHandler::TargaryenControlMessageHandler(TargaryenControl* pTCtrl)
    : mpTCtrl(pTCtrl)
{
}

};

