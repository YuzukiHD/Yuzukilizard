/*
********************************************************************************
*                           eyesee framework module
*
*          (c) Copyright 2010-2019, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : VdecFrameManager.h
* Version: V1.0
* By     : eric_wang
* Date   : 2018-12-13
* Description:
********************************************************************************
*/
#ifndef _VDECFRAMEMANAGER_H_
#define _VDECFRAMEMANAGER_H_

#include <list>
#include <cmath>

#include <plat_log.h>
#include <mpi_uvc.h>
#include <uvcInput.h>
#include <mpi_vdec.h>
#include <mpi_sys.h>

#include <utils/Errors.h>
#include <utils/Thread.h>
#include <utils/EyeseeMessageQueue.h>
#include <UvcChnDefine.h>

namespace EyeseeLinux {

class VdecFrameManager
{
public:
    VdecFrameManager(VDEC_CHN nVdecChn, bool bVdecSubOutputEnable);
    ~VdecFrameManager();
    status_t start();
    /**
     * must make sure double frame all return, then release all frames to vdec.
     */
    status_t stop();
    private: status_t stop_l();
public:    
    //buffer manager
    struct DoubleFrame
    {            
        DoubleFrame();
        DoubleFrame(const DoubleFrame &lhs);
        VIDEO_FRAME_INFO_S mOutMainFrame;  //0: main stream; 1 : sub stream. note: sub stream just for mjepg vdec now!
        VIDEO_FRAME_INFO_S mOutSubFrame;
        bool mbMainValid;   //if main buffer contain valid frame.
        bool mbSubValid;    //if sub buffer contain valid frame.
        bool mbMainRequestFlag; //if main buffer is requested.
        bool mbMainReturnFlag;  //if main buffer is returned.
        bool mbSubRequestFlag;  //if sub buffer is requested.
        bool mbSubReturnFlag;   //if sub buffer is returned.
    };
    
    status_t enableUvcChannel(UvcChn chn);
    status_t disableUvcChannel(UvcChn chn);
    status_t getFrame(UvcChn chn, VIDEO_FRAME_INFO_S *pFrameInfo, int timeout);//unit:ms
    status_t releaseFrame(UvcChn chn, VIDEO_FRAME_INFO_S *pFrameInfo);
    
    status_t VdecNotifyNoFrameBuffer();
    private:status_t releaseAllFrames_l();
public:
    class DoRequestVdecFrameThread : public Thread
    {
    public:
        DoRequestVdecFrameThread(VdecFrameManager *p);
        ~DoRequestVdecFrameThread();
        status_t startRequestVdecFrame();
        status_t stopRequestVdecFrame();
        virtual bool threadLoop() override;

    private:
        bool RequestVdecFrameThread();

        enum class MsgType
        {
            SetState = 0,
            Exit,
        };
        enum class State
        {
            IDLE = 0,
            STARTED,
            PAUSE,
        };
        Mutex mThreadStateLock;
        Condition mThreadCondStateComplete;
        State mThreadState;
        EyeseeMessageQueue mMsgQueue;
        VdecFrameManager* const mpVdecFrameManager;
    };
    DoRequestVdecFrameThread *mpRequestThread;

    const VDEC_CHN mVdecChn;
    const bool mbVdecSubOutputEnable;   //vdeclib enable subframe output.
    enum class State
    {
        IDLE = 0,
        STARTED,
    };
    Mutex mStateLock;
    State mState;
    
    bool mbMainEnable;  //main buffer related uvcChannel is enable.
    bool mbSubEnable;   //sub buffer related uvcChannel is enable.
    Mutex mListLock;
    //static constexpr int mInitBufNums = 30;
    static const int mInitBufNums = 30;
    std::list<DoubleFrame> mIdleList;
    std::list<DoubleFrame> mReadyList;
    bool mWaitReadyFrameFlag;
    Condition mCondReadyFrame;
};

}

#endif  /* _VDECFRAMEMANAGER_H_ */

