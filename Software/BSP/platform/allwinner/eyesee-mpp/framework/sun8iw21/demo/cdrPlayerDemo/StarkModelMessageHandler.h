/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : StarkModelMessageHandler.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-7
* Description:
********************************************************************************
*/
#ifndef _STARKMODELMESSAGEHANDLER_H_
#define _STARKMODELMESSAGEHANDLER_H_

#include "BaseMessageHandler.h"
#include "TargaryenControl.h"

namespace EyeseeLinux {

class StarkModelMessageHandler : public BaseMessageHandler
{
public:
    virtual bool matchMessageHandler(EyeseeMessage *pMsg);
    virtual status_t handleMessage(EyeseeMessage *pMsg);
    StarkModelMessageHandler(TargaryenControl* pTCtrl);
    virtual ~StarkModelMessageHandler(){}
private:
    TargaryenControl* const mpTCtrl;
};

};

#endif  /* _STARKMODELMESSAGEHANDLER_H_ */


