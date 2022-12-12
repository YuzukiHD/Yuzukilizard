/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : StarkModelMessageHandler.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-11
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "StarkModelMessageHandler"
#include <utils/plat_log.h>

#include <vector>

#include "StarkModelMessageHandler.h"

using namespace std;

namespace EyeseeLinux {

bool StarkModelMessageHandler::matchMessageHandler(EyeseeMessage *pMsg)
{
    if(MessageOfStarkModel == pMsg->mWhoseMsg)
    {
        return true;
    }
    else
    {
        return false;
    }
}

status_t StarkModelMessageHandler::handleMessage(EyeseeMessage *pMsg)
{
    status_t ret = NO_ERROR;
    if(pMsg->mWhoseMsg != MessageOfStarkModel)
    {
        aloge("fatal error! wrong msg[0x%x]of[0x%x] come!", pMsg->mMsgType, pMsg->mWhoseMsg);
        return BAD_TYPE;
    }
    switch(pMsg->mMsgType)
    {
        case StarkModel::MSGTYPE_PLAYBACK_COMPLETED:
        {
            alogd("receive play complete, stop now!");
            mpTCtrl->mpPlayer->stopFile();
            vector<string> stringArray;
            stringArray.push_back(string("play end."));
            mpTCtrl->mpCommandReceiver->showStringList(stringArray);
            break;
        }
        case StarkModel::MSGTYPE_ERROR_INVALID_FILEPATH:
        {
            mpTCtrl->mpPlayer->stopFile();
            vector<string> stringArray;
            stringArray.push_back(string("file is not exist!"));
            stringArray.push_back(string("please check your input file path."));
            mpTCtrl->mpCommandReceiver->showStringList(stringArray);
            break;
        }
        case StarkModel::MSGTYPE_ERROR_PARSE_FILE_LIST_FAIL:
        {
            mpTCtrl->mpPlayer->stopFile();
            vector<string> stringArray;
            stringArray.push_back(string("sorry, parse media list file fail"));
            stringArray.push_back(string("please check your mediaListFile."));
            mpTCtrl->mpCommandReceiver->showStringList(stringArray);
            break;
        }
        case StarkModel::MSGTYPE_ERROR_FILELIST_EMPTY:
        {
            mpTCtrl->mpPlayer->stopFile();
            vector<string> stringArray;
            stringArray.push_back(string("retrieve none item from media list file."));
            stringArray.push_back(string("we can't play list loop."));
            stringArray.push_back(string("please check your mediaListFile."));
            mpTCtrl->mpCommandReceiver->showStringList(stringArray);
            break;
        }
        case StarkModel::MSGTYPE_ERROR_PREPARE_FAIL:
        {
            mpTCtrl->mpPlayer->stopFile();
            vector<string> stringArray;
            stringArray.push_back(string("prepare prerequisite conditions fail")); 
            stringArray.push_back(string("we can't play,sorry! need check code"));
            mpTCtrl->mpCommandReceiver->showStringList(stringArray);
            break;
        }
        case StarkModel::MSGTYPE_ERROR_MEDIA_ERROR:
        {
            alogd("receive media error type[0x%x] extra[0x%x], stop now!", pMsg->mPara0, pMsg->mPara1);
            mpTCtrl->mpPlayer->stopFile();
            vector<string> stringArray;
            stringArray.push_back(string("play meet a media error:"));
            char bufStr[128] = {0};
            snprintf(bufStr, sizeof(bufStr), "mediaErrorType[0x%x], extra[0x%x]", pMsg->mPara0, pMsg->mPara1);
            stringArray.push_back(string(bufStr));
            mpTCtrl->mpCommandReceiver->showStringList(stringArray);
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

StarkModelMessageHandler::StarkModelMessageHandler(TargaryenControl* pTCtrl)
    : mpTCtrl(pTCtrl)
{
}

};

