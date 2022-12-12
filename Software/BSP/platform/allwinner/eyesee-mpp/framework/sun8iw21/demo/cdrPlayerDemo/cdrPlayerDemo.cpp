/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : cdrPlayerDemo.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-2
* Description:
    continue to play a list of mediafiles.
    can stop manually.
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "cdrPlayerDemo"
#include <utils/plat_log.h>

#include "TargaryenControl.h"
#include <iostream>

using namespace std;
using namespace EyeseeLinux;

int main(int argc, char *argv[])
{
    alogd("cdrPlayerDemo say hello!");
    // set up the thread-pool
    TargaryenControl *pControl = new TargaryenControl;
    pControl->init();
    pControl->threadRunning();
    pControl->reset();
    delete pControl;
    alogd("test done, quit!");
	return 0;
}
