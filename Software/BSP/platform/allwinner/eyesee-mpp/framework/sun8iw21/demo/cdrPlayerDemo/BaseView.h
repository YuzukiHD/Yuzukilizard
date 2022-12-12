/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : BaseView.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-6
* Description:
********************************************************************************
*/
#ifndef _BASEVIEW_H_
#define _BASEVIEW_H_

#include <utils/Errors.h>

namespace EyeseeLinux {

class BaseView
{
public:
    BaseView(){}
    virtual ~BaseView(){}
public:
    class OnMessageListener
	{
	public:
        /**
         * Called when the media file is ready for playback.
         *
         * @param mp the MediaPlayer that is ready for playback
         */
		OnMessageListener(){}
		virtual ~OnMessageListener(){}
        //ret = INVALID_OPERATION, NO_ERROR, UNKNOWN_ERROR
        virtual status_t onMessage(int msg, void* pMsgData) = 0;
	};
    virtual void setOnMessageListener(OnMessageListener *pListener) = 0;
};

};

#endif  /* _BASEVIEW_H_ */

