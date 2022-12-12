/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : BaseMessageHandler.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-7
* Description:
********************************************************************************
*/
#ifndef _BASEMESSAGEHANDLER_H_
#define _BASEMESSAGEHANDLER_H_

#include "EyeseeMessageQueue.h"

namespace EyeseeLinux {

enum WhoseMessage
{
    MessageOfTargaryenControl = 0,
    MessageOfLannisterView = 1,
    MessageOfStarkModel = 2,
    MessageOf,
};

class BaseMessageHandler
{
public:
    virtual bool matchMessageHandler(EyeseeMessage *pMsg) = 0;
    virtual status_t handleMessage(EyeseeMessage *pMsg) = 0;
    BaseMessageHandler(){}
    virtual ~BaseMessageHandler(){}
};

};

#endif  /* _BASEMESSAGEHANDLER_H_ */


