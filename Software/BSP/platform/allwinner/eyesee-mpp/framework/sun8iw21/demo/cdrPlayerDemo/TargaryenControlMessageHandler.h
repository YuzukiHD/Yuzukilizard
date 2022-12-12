/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : TargaryenControlMessageHandler.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-7
* Description:
********************************************************************************
*/
#ifndef _TARGARYENCONTROLMESSAGEHANDLER_H_
#define _TARGARYENCONTROLMESSAGEHANDLER_H_

#include "BaseMessageHandler.h"
#include "TargaryenControl.h"

namespace EyeseeLinux {

class TargaryenControlMessageHandler : public BaseMessageHandler
{
public:
    virtual bool matchMessageHandler(EyeseeMessage *pMsg);
    virtual status_t handleMessage(EyeseeMessage *pMsg);
    TargaryenControlMessageHandler(TargaryenControl* pTCtrl);
    virtual ~TargaryenControlMessageHandler(){}
private:
    TargaryenControl* const mpTCtrl;
};

};

#endif  /* _TARGARYENCONTROLMESSAGEHANDLER_H_ */

