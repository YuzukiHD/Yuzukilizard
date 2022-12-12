/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : LannisterViewMessageHandler.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-7
* Description:
********************************************************************************
*/
#ifndef _LANNISTERVIEWMESSAGEHANDLER_H_
#define _LANNISTERVIEWMESSAGEHANDLER_H_

#include "BaseMessageHandler.h"
#include "TargaryenControl.h"

namespace EyeseeLinux {

class LannisterViewMessageHandler : public BaseMessageHandler
{
public:
    virtual bool matchMessageHandler(EyeseeMessage *pMsg);
    virtual status_t handleMessage(EyeseeMessage *pMsg);
    LannisterViewMessageHandler(TargaryenControl* pTCtrl);
    virtual ~LannisterViewMessageHandler(){}
private:
    TargaryenControl* const mpTCtrl;
};

};

#endif  /* _LANNISTERVIEWMESSAGEHANDLER_H_ */


