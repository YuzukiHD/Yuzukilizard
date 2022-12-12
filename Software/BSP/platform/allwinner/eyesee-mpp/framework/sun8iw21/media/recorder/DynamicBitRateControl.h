/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/
#ifndef __IPCLINUX_DYNAMICBITRATECONTROL_H__
#define __IPCLINUX_DYNAMICBITRATECONTROL_H__

#include <tmessage.h>
#include <EyeseeRecorder.h>

namespace EyeseeLinux {

#define DBRC_MAX_STEP_LEVEL (2)
//typedef struct DynamicBitRateControl
//{
//    pthread_t mDynamicBitRateControlThreadId;
//    message_queue_t mDBRCMsgQueue;
//    void *mPriv; //EyeseeRecorder*

//    int mStepLevel; //0:not adjust, 1:adjust level one, 2:adjust level two.
//}DynamicBitRateControl;

class DynamicBitRateControl
{
public:
    DynamicBitRateControl(EyeseeRecorder *pRecCtx);
    ~DynamicBitRateControl();

    status_t GetBufferState(EyeseeRecorder::BufferState &bufferState);
private:
    static void* DynamicBitRateControlThread(void *data);
//    int DynamicBitRateControlInit(void *pRecCtx);
//    void DynamicBitRateControlDestruct();
//    int DynamicBitRateControlDestroy();
    int UpdateVEncBitRate(EyeseeRecorder *pRecCtx, int newBitRate, int *pOldBitRate);

    pthread_t mDynamicBitRateControlThreadId;
    message_queue_t mDBRCMsgQueue;
    EyeseeRecorder *mPriv;

    Mutex mBufferStateLock;
    bool mbBufferStateValid;
    EyeseeRecorder::BufferState mBufferState;

    int mStepLevel; //0:not adjust, 1:adjust level one, 2:adjust level two.
};



}; /* EyeseeLinux */

#endif /* __IPCLINUX_DYNAMICBITRATECONTROL_H__ */

