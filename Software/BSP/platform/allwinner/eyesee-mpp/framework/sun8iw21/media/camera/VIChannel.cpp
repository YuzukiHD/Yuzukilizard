/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VIChannel.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/02
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "VIChannel"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <memoryAdapter.h>
#include <SystemBase.h>
#include <VIDEO_FRAME_INFO_S.h>
#include <mpi_vi_private.h>
#include <mpi_vi.h>
#include <mpi_isp.h>
#include <mpi_videoformat_conversion.h>

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <MediaStructConvert.h>
#include "VIChannel.h"
#include <ConfigOption.h>

#include <mpi_sys.h>
#include <sys/ioctl.h>
#include <linux/g2d_driver.h>
#include <PIXEL_FORMAT_E_g2d_format_convert.h>

using namespace std;

#define DEBUG_STORE_VI_FRAME (0)
#define DEBUG_SAVE_FRAME_TO_TMP (0)
#define DEBUG_SAVE_FRAME_NUM  (60)

namespace EyeseeLinux {

VIChannel::DoCaptureThread::DoCaptureThread(VIChannel *pViChn)
    : mpViChn(pViChn)
{
    mCapThreadState = CAPTURE_STATE_NULL;
}

status_t VIChannel::DoCaptureThread::startThread()
{
    mCapThreadState = CAPTURE_STATE_PAUSED;
    status_t ret = run("VIChnCapture");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void VIChannel::DoCaptureThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t VIChannel::DoCaptureThread::startCapture()
{
    AutoMutex lock(mStateLock);
    if(mCapThreadState == CAPTURE_STATE_STARTED)
    {
        alogd("already in started");
        return NO_ERROR;
    }
    if(mCapThreadState != CAPTURE_STATE_PAUSED)
    {
        aloge("fatal error! can't call in state[0x%x]", mCapThreadState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_SetState;
    msg.mPara0 = CAPTURE_STATE_STARTED;
    mMsgQueue.queueMessage(&msg);
    while(CAPTURE_STATE_STARTED != mCapThreadState)
    {
        mStartCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

status_t VIChannel::DoCaptureThread::pauseCapture()
{
    AutoMutex lock(mStateLock);
    if(mCapThreadState == CAPTURE_STATE_PAUSED)
    {
        alogd("already in paused");
        return NO_ERROR;
    }
    if(mCapThreadState != CAPTURE_STATE_STARTED)
    {
        aloge("fatal error! can't call in state[0x%x]", mCapThreadState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_SetState;
    msg.mPara0 = CAPTURE_STATE_PAUSED;
    mMsgQueue.queueMessage(&msg);
    while(CAPTURE_STATE_PAUSED != mCapThreadState)
    {
        mPauseCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

bool VIChannel::DoCaptureThread::threadLoop()
{
    if(!exitPending())
    {
        return mpViChn->captureThread();
    } 
    else
    {
        return false;
    }
}

status_t VIChannel::DoCaptureThread::SendCommand_TakePicture()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_TakePicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t VIChannel::DoCaptureThread::SendCommand_CancelContinuousPicture()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCapture_CancelContinuousPicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

VIChannel::DoPreviewThread::DoPreviewThread(VIChannel *pViChn)
    : mpViChn(pViChn)
{
    mbWaitPreviewFrame = false;
    mbWaitReleaseAllFrames = false;
}
status_t VIChannel::DoPreviewThread::startThread()
{
    status_t ret = run("VIChnPreview");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void VIChannel::DoPreviewThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePreview_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t VIChannel::DoPreviewThread::notifyNewFrameCome()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitPreviewFrame)
    {
        mbWaitPreviewFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypePreview_InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

status_t VIChannel::DoPreviewThread::releaseAllFrames()
{
    AutoMutex lock(mWaitLock);
    mbWaitReleaseAllFrames = true;
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePreview_releaseAllFrames;
    mMsgQueue.queueMessage(&msg);
    while(mbWaitReleaseAllFrames)
    {
        mCondReleaseAllFramesFinished.wait(mWaitLock);
    }
    return NO_ERROR;
}

bool VIChannel::DoPreviewThread::threadLoop()
{
    if(!exitPending())
    {
        return mpViChn->previewThread();
    }
    else
    {
        return false;
    }
}

VIChannel::DoPictureThread::DoPictureThread(VIChannel *pViChn)
    : mpViChn(pViChn)
{
    mbWaitPictureFrame = false;
    mbWaitReleaseAllFrames = false;
}

status_t VIChannel::DoPictureThread::startThread() 
{
    status_t ret = run("VIChnPicture");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void VIChannel::DoPictureThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePicture_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

status_t VIChannel::DoPictureThread::notifyNewFrameCome()
{
    AutoMutex lock(mWaitLock);
    if(mbWaitPictureFrame)
    {
        mbWaitPictureFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypePicture_InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}

status_t VIChannel::DoPictureThread::notifyPictureEnd()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePicture_SendPictureEnd;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t VIChannel::DoPictureThread::releaseAllFrames()
{
    AutoMutex lock(mWaitLock);
    mbWaitReleaseAllFrames = true;
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePicture_releaseAllFrames;
    mMsgQueue.queueMessage(&msg);
    while(mbWaitReleaseAllFrames)
    {
        mCondReleaseAllFramesFinished.wait(mWaitLock);
    }
    return NO_ERROR;
}

bool VIChannel::DoPictureThread::threadLoop()
{
    if(!exitPending())
    {
        return mpViChn->pictureThread();
    } 
    else
    {
        return false;
    }
}

VIChannel::DoMODThread::DoMODThread(VIChannel *pViChn)
    : mpViChn(pViChn)
{
    mpMODFrameQueue = NULL;
    mbWaitFrame = false;
    mbMODDetectEnable = false;
    mhMOD = NULL;
    mFrameCounter = 0;
    m_Sensitivity = 4;
    m_awmd = NULL;
    m_MDvar = NULL;
    m_width = 0;
    m_height = 0;
}
VIChannel::DoMODThread::~DoMODThread()
{
    if(mpMODFrameQueue)
    {
        alogw("fatal error! why MOD frame queue is not null?");
        delete mpMODFrameQueue;
    }
}
status_t VIChannel::DoMODThread::startThread()
{
    status_t ret = run("VIChnMOD");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}
void VIChannel::DoMODThread::stopThread()
{
    stopMODDetect();
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeMOD_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}
status_t VIChannel::DoMODThread::getMODParams(MOTION_DETECT_ATTR_S *pParamMD)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mMODDetectLock);
    pParamMD->nSensitivity = m_Sensitivity;

    return ret;
}

status_t VIChannel::DoMODThread::setMODParams(MOTION_DETECT_ATTR_S pParamMD)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mMODDetectLock);
    if(!mbMODDetectEnable)
    {
        m_Sensitivity = pParamMD.nSensitivity;
    }
    else
    {
        aloge("fatal error! refuse to set mod params during detect on!");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

status_t VIChannel::DoMODThread::startMODDetect()
{
    status_t eRet = NO_ERROR;
    #if(MPPCFG_MOTION_DETECT_SOFT == OPTION_MOTION_DETECT_SOFT_ENABLE)
    AutoMutex autoLock(mMODDetectLock);
    if(mbMODDetectEnable)
    {
        alogw("MOD detect already enable");
        return NO_ERROR;
    }

    //prepare eve face lib.
    //initilize
    m_width = mpViChn->mFrameWidth;
    m_height = mpViChn->mFrameHeight;
    m_awmd = allocAWMD(m_width, m_height, 16, 16, 5);
    m_MDvar = &m_awmd->variable;
    eRet = m_awmd->vpInit(m_MDvar, 6);
    if( eRet != NO_ERROR)
    {
        aloge("fatal error, vpInit failed!");
        eRet = UNKNOWN_ERROR;
        goto _err0;
    }

    unsigned char *p_mask;
    p_mask = (unsigned char*)malloc(m_width*m_height);
    if(NULL == p_mask)
    {
        aloge("fatal error, malloc failed!");
        eRet = NO_MEMORY;
        goto _err0;
    }
    memset(p_mask, 255, m_width*m_height);

    eRet = m_awmd->vpSetROIMask(m_MDvar, p_mask, m_width, m_height);
    if( eRet != NO_ERROR)
    {
        free(p_mask);
        p_mask = NULL;
        aloge("fatal error, vpSetROIMask failed!");
        eRet = UNKNOWN_ERROR;
        goto _err0;
    }
    m_awmd->vpSetSensitivityLevel(m_MDvar, m_Sensitivity);
    if( eRet != NO_ERROR)
    {
        free(p_mask);
        p_mask = NULL;
        aloge("fatal error, vpSetSensitivityLevel failed!");
        eRet = UNKNOWN_ERROR;
        goto _err0;
    }

    m_awmd->vpSetShelterPara(m_MDvar, 0, 0);
    if( eRet != NO_ERROR)
    {
        free(p_mask);
        p_mask = NULL;
        aloge("fatal error, vpSetShelterPara failed!");
        eRet = UNKNOWN_ERROR;
        goto _err0;
    }
    free(p_mask);
    p_mask = NULL;
    
    //notify faceDetect thread to detectOn.
    {
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeMOD_DetectOn;
    mMsgQueue.queueMessage(&msg);
    while(!mbMODDetectEnable)
    {
        mCondMODDetect.wait(mMODDetectLock);
    }
    }
    return NO_ERROR;
_err0:
    #else
    alogd("motion detect is disable\n");
    #endif
    return eRet;
}

status_t VIChannel::DoMODThread::stopMODDetect()
{
    status_t eRet = NO_ERROR;
    #if(MPPCFG_MOTION_DETECT_SOFT == OPTION_MOTION_DETECT_SOFT_ENABLE)
    AutoMutex autoLock(mMODDetectLock);
    if(!mbMODDetectEnable)
    {
        alogw("MOD already disable");
        return NO_ERROR;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeMOD_DetectOff;
    mMsgQueue.queueMessage(&msg);
    while(mbMODDetectEnable)
    {
        mCondMODDetect.wait(mMODDetectLock);
    }

    // algo terminate
    if( m_awmd )
    {
        freeAWMD(m_awmd);
        m_awmd = NULL;
    }
    #else 
    alogd("motion detect is disable\n");
    #endif
    return eRet;
}

bool VIChannel::DoMODThread::IsMODDetectEnable()
{
    AutoMutex autoLock(mMODDetectLock);
    return mbMODDetectEnable;
}
status_t VIChannel::DoMODThread::sendFrame(VIDEO_FRAME_BUFFER_S *pFrmBuf)
{
    status_t ret;
    AutoMutex lock(mMODDetectLock);
    if(false == mbMODDetectEnable)
    {
        alogw("don't send frame when MOD disable!");
        return INVALID_OPERATION;
    }

    ret = mpMODFrameQueue->PutElemDataValid((void*)pFrmBuf);
    if (NO_ERROR != ret)
    {
        return ret;
    }

    AutoMutex lock2(mWaitLock);
    if(mbWaitFrame)
    {
        mbWaitFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeMOD_InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}
bool VIChannel::DoMODThread::threadLoop()
{
    if(!exitPending())
    {
        return MODThread();
    } 
    else
    {
        return false;
    }
}

bool VIChannel::DoMODThread::MODThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    while(1)
    {
    PROCESS_MESSAGE:
        getMsgRet = mMsgQueue.dequeueMessage(&msg);
        if(getMsgRet == NO_ERROR)
        {
            if(MsgTypeMOD_DetectOn == msg.mMsgType)
            {
                AutoMutex autoLock(mMODDetectLock);
                if(!mbMODDetectEnable)
                {
                    if(mpMODFrameQueue)
                    {
                        aloge("fatal error! why pQueue is not null?");
                        delete mpMODFrameQueue;
                    }
                    mpMODFrameQueue = new EyeseeQueue();
                    mFrameCounter = 0;
                    mbMODDetectEnable = true;
                }
                else
                {
                    alogw("already enable MOD");
                }
                mCondMODDetect.signal();
            }
            else if(MsgTypeMOD_DetectOff == msg.mMsgType)
            {
                AutoMutex autoLock(mMODDetectLock);
                if(mbMODDetectEnable)
                {
                    releaseAllFrames();
                    delete mpMODFrameQueue;
                    mpMODFrameQueue = NULL;
                    mbMODDetectEnable = false;
                }
                else
                {
                    alogw("already disable MOD");
                    if(mpMODFrameQueue)
                    {
                        aloge("fatal error! why MOD frame queue is not null?");
                        delete mpMODFrameQueue;
                        mpMODFrameQueue = NULL;
                    }
                }
                mCondMODDetect.signal();
            }
            else if(MsgTypeMOD_InputFrameAvailable == msg.mMsgType)
            {
                //alogv("MOD frame input");
            }
            else if(MsgTypeMOD_Exit == msg.mMsgType)
            {
                AutoMutex autoLock(mMODDetectLock);
                if(mbMODDetectEnable)
                {
                    aloge("fatal error! must stop MOD before exit!");
                    mbMODDetectEnable = false;
                }
                if(mpMODFrameQueue)
                {
                    releaseAllFrames();
                    delete mpMODFrameQueue;
                    mpMODFrameQueue = NULL;
                }
                bRunningFlag = false;
                goto _exit0;
            }
            else
            {
                aloge("fatal error! unknown msg[0x%x]!", msg.mMsgType);
            }
            goto PROCESS_MESSAGE;
        }

        if(mbMODDetectEnable)
        {
            pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpMODFrameQueue->GetValidElemData();
            if (pFrmbuf == NULL) 
            {
                {
                    AutoMutex lock(mWaitLock);
                    if(mpMODFrameQueue->GetValidElemNum() > 0)
                    {
                        alogd("Low probability! MOD new frame come before check again.");
                        goto PROCESS_MESSAGE;
                    }
                    else
                    {
                        mbWaitFrame = true;
                    }
                }
                mMsgQueue.waitMessage();
                goto PROCESS_MESSAGE;
            }
    #if(MPPCFG_MOTION_DETECT_SOFT == OPTION_MOTION_DETECT_SOFT_ENABLE)
#ifdef TIME_TEST
            int64_t nStartTm = CDX_GetSysTimeUsMonotonic();//start timer
#endif
            // video frame time
            mFrameCounter++;
            // process image
            MotionDetectResult result;
            result.nResult = m_awmd->vpRun((unsigned char *)(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[0]), m_MDvar);
#ifdef TIME_TEST
            int64_t nEndTm = CDX_GetSysTimeUsMonotonic();
            alogv("CVE_DTCA_Process:time used:%d us", nEndTm - nStartTm);
#endif
            std::shared_ptr<CMediaMemory> spMem = std::make_shared<CMediaMemory>(sizeof(MotionDetectResult));
            memcpy(spMem->getPointer(), &result, sizeof(MotionDetectResult));
            mpViChn->mpCallbackNotifier->postMODData(spMem);

            mpViChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
            mpMODFrameQueue->ReleaseElemData(pFrmbuf); //release to idle list
         #else
            aloge("motion detect is disable\n");
            return false;
         #endif
        }
        else
        {
            mMsgQueue.waitMessage();
        }
    }
_exit0:
    return bRunningFlag;
}

status_t VIChannel::DoMODThread::releaseAllFrames()
{
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    while(1)
    {
        pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpMODFrameQueue->GetValidElemData();
        if(pFrmbuf)
        {
            mpViChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
            mpMODFrameQueue->ReleaseElemData(pFrmbuf); //release to idle list
        }
        else
        {
            break;
        }
    }
    return NO_ERROR;
}

VIChannel::DoCommandThread::DoCommandThread(VIChannel *pViChn)
    : mpViChn(pViChn)
{
}

status_t VIChannel::DoCommandThread::startThread()
{
    status_t ret = run("VIChnCommand");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void VIChannel::DoCommandThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCommand_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}

bool VIChannel::DoCommandThread::threadLoop()
{
    if(!exitPending()) 
    {
        return mpViChn->commandThread();
    } 
    else 
    {
        return false;
    }
}

status_t VIChannel::DoCommandThread::SendCommand_TakePicture(unsigned int msgType)
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCommand_TakePicture;
    msg.mPara0 = msgType;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

status_t VIChannel::DoCommandThread::SendCommand_CancelContinuousPicture()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeCommand_CancelContinuousPicture;
    mMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}

/*************************adas*******************************/
VIChannel::DoAdasThread::DoAdasThread(VIChannel *pViChn)
    : mpViChn(pViChn)
{
    mpADASFrameQueue = NULL;
    mbWaitFrame = false;
    mbAdasDetectEnable = false;
    mhADAS = NULL;
    mFrameCounter = 0;
    m_width = 0;
    m_height = 0;
    mPtrImgBuffer = NULL;
    mAdasDataCallBackOut = true;
    mGetFrameDataOk = false;
    mAdasG2DHandle = -1;
    pthread_mutex_init(&mDataTransferLock, NULL); 
}
VIChannel::DoAdasThread::~DoAdasThread()
{
    if(mpADASFrameQueue)
    {
        alogw("fatal error! why adas frame queue is not null?");
        delete mpADASFrameQueue;
    }
    if(mPtrImgBuffer != NULL)
    {
        free(mPtrImgBuffer);
        mPtrImgBuffer = NULL;
    }
    pthread_mutex_destroy(&mDataTransferLock);
}
status_t VIChannel::DoAdasThread::startThread()
{
    status_t ret = run("VIChnADAS");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}
void VIChannel::DoAdasThread::stopThread()
{
    stopAdasDetect();
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeADAS_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();
}
status_t VIChannel::DoAdasThread::getAdasParams(AdasDetectParam *pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mAdasDetectLock);
    pParamADAS = &mAdasDetectParam;
    return ret;
}
status_t VIChannel::DoAdasThread::setAdasParams(AdasDetectParam pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mAdasDetectLock);
    if(!mbAdasDetectEnable)
    {
        //ADAS init default data
        mAdasDetectParam.nframeWidth = pParamADAS.nframeWidth;//视频宽度
        mAdasDetectParam.nframeHeight = pParamADAS.nframeHeight;//视频高度
        mAdasDetectParam.nroiX = pParamADAS.nroiX;//感兴趣区域X坐标
        mAdasDetectParam.nroiY = pParamADAS.nroiY;//感兴趣区域Y坐标
        mAdasDetectParam.nroiW = pParamADAS.nroiW;//感兴趣区域宽度
        mAdasDetectParam.nroiH = pParamADAS.nroiH;//感兴趣区域高度
        mAdasDetectParam.nfps = pParamADAS.nfps;//视频的帧率fps
        //camera paramter
        mAdasDetectParam.nfocalLength = pParamADAS.nfocalLength;//焦距um
        mAdasDetectParam.npixelSize = pParamADAS.npixelSize;//sensor 单个像素大小um
        mAdasDetectParam.nhorizonViewAngle = pParamADAS.nhorizonViewAngle;//水平视场角
        mAdasDetectParam.nverticalViewAngle = pParamADAS.nverticalViewAngle;//垂直视场角
        mAdasDetectParam.nvanishX = pParamADAS.nvanishX;//天际消失点X坐标
        mAdasDetectParam.nvanishY = pParamADAS.nvanishY;//天际消失点Y坐标
        mAdasDetectParam.nhoodLineDist = pParamADAS.nhoodLineDist;//车头距离
        mAdasDetectParam.nvehicleWidth = pParamADAS.nvehicleWidth;//车身宽度
        mAdasDetectParam.nwheelDist = pParamADAS.nwheelDist;//车轮距离
        mAdasDetectParam.ncameraToCenterDist = pParamADAS.ncameraToCenterDist;//摄像头中心距离
        mAdasDetectParam.ncameraHeight = pParamADAS.ncameraHeight;//相机安装高度
        mAdasDetectParam.nleftSensity = pParamADAS.nleftSensity;//左侧车道线检测灵敏度
        mAdasDetectParam.nrightSensity = pParamADAS.nrightSensity;//右侧车道线检测灵敏度
        mAdasDetectParam.nfcwSensity = pParamADAS.nfcwSensity;//前车碰撞预警灵敏度

    }
    else
    {
        aloge("fatal error! refuse to set adas params during detect on!");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

status_t VIChannel::DoAdasThread::getAdasInParams(AdasInParam *pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mAdasDetectLock);
    pParamADAS = &mAdasInParam;
    return ret;
}
status_t VIChannel::DoAdasThread::setAdasInParams(AdasInParam pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mAdasDetectLock);
    if(!mbAdasDetectEnable)
    {
        //GPS数据
        mAdasInParam.ngpsSpeed = pParamADAS.ngpsSpeed;//KM/H  千米每小时
        mAdasInParam.ngpsSpeedEnable = pParamADAS.ngpsSpeedEnable;//1-gps速度有效  0-gps速度无效
        //Gsensor数据
        mAdasInParam.nGsensorStop = pParamADAS.nGsensorStop;//加速度传感器停止标志
        mAdasInParam.nGsensorStopEnable = pParamADAS.nGsensorStopEnable;//加速度传感器停止标志使能标记
        mAdasInParam.nsensity = pParamADAS.nsensity;//检测灵敏度
        mAdasInParam.nCPUCostLevel = pParamADAS.nCPUCostLevel;//cpu消耗等级
        mAdasInParam.nluminanceValue = pParamADAS.nluminanceValue;//关照强度LV
        //车辆转向灯信号
        mAdasInParam.nlightSignal = pParamADAS.nlightSignal;//0-无信号 1-左转向  2-右转向
        mAdasInParam.nlightSignalEnable = pParamADAS.nlightSignalEnable;//信号灯使能信号
        mAdasInParam.ncameraHeight = pParamADAS.ncameraHeight;//相机安装的高度
        mAdasInParam.ndataType = pParamADAS.ndataType;//0-YUV 1-NV12 2-NV21 3-RGB...
        mAdasInParam.ncameraType = pParamADAS.ncameraType;//摄像头类型(0-前置摄像头 1-后置摄像头 2-左侧摄像头 3-右侧摄像头...)
        mAdasInParam.nimageWidth = pParamADAS.nimageWidth;
        mAdasInParam.nimageHeight = pParamADAS.nimageHeight;
        mAdasInParam.ncameraNum = pParamADAS.ncameraNum;//摄像头个数
        //GPS经纬度信息
        mAdasInParam.nLng = pParamADAS.nLng;//GPS经度
        mAdasInParam.nLngValue = pParamADAS.nLngValue;
        mAdasInParam.nLat = pParamADAS.nLat;//GPS维度
        mAdasInParam.nLatValue = pParamADAS.nLatValue;
        mAdasInParam.naltitude = pParamADAS.naltitude;//GPS海拔
        mAdasInParam.ndeviceType = pParamADAS.ndeviceType;//设备类型(0-行车记录仪 1-后视镜...)
        strcpy(mAdasInParam.nsensorID,pParamADAS.nsensorID);
        mAdasInParam.nnetworkStatus = pParamADAS.nnetworkStatus;//0-端口 1-连接

    }
    else
    {
        aloge("fatal error! refuse to set adas in params during detect on!");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}


/*************************adas_v2*******************************/
status_t VIChannel::DoAdasThread::getAdasParams_v2(AdasDetectParam_v2 *pParamADAS_v2)
{

    status_t ret = NO_ERROR;
    AutoMutex autoLock(mAdasDetectLock);
    pParamADAS_v2 = &mAdasDetectParam_v2;
    return ret;
}
status_t VIChannel::DoAdasThread::setAdasParams_v2(AdasDetectParam_v2 pParamADAS_v2)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mAdasDetectLock);
    if(!mbAdasDetectEnable)
    {
        mAdasDetectParam_v2.isGps = pParamADAS_v2.isGps;//有无gps
        mAdasDetectParam_v2.isGsensor = pParamADAS_v2.isGsensor;//有无Gsensor，出v3之外其他方案请置为0
        mAdasDetectParam_v2.isObd = pParamADAS_v2.isObd;//有无读取OBD
        mAdasDetectParam_v2.isCalibrate = pParamADAS_v2.isCalibrate;//是否进行安装标定
        mAdasDetectParam_v2.fps = pParamADAS_v2.fps;//帧率
        mAdasDetectParam_v2.width_big = pParamADAS_v2.width_big;//主码流图 1920*1080
        mAdasDetectParam_v2.height_big = pParamADAS_v2.height_big;
        mAdasDetectParam_v2.width_small = pParamADAS_v2.width_small;//子码流图 960*540
        mAdasDetectParam_v2.height_small = pParamADAS_v2.height_small;
        mAdasDetectParam_v2.angH = pParamADAS_v2.angH;//镜头的水平视角
        mAdasDetectParam_v2.angV = pParamADAS_v2.angV;//镜头的垂直视角
        mAdasDetectParam_v2.widthOri = pParamADAS_v2.widthOri;//原始的图像宽度
        mAdasDetectParam_v2.heightOri = pParamADAS_v2.heightOri;//原始的图像高度
        mAdasDetectParam_v2.vanishLine = pParamADAS_v2.vanishLine;//???
        mAdasDetectParam_v2.vanishLineIsOk = pParamADAS_v2.vanishLineIsOk;//????
    }
    else
    {
        aloge("fatal error! refuse to set adas in params during detect on!");
        ret = UNKNOWN_ERROR;
    }
    return ret;

}
status_t VIChannel::DoAdasThread::getAdasInParams_v2(AdasInParam_v2 *pParamADAS_v2)
{
    status_t ret = NO_ERROR;
    return ret;

}
status_t VIChannel::DoAdasThread::setAdasInParams_v2(AdasInParam_v2 pParamADAS_v2)
{
    status_t ret = NO_ERROR;
    return ret;
}
status_t  VIChannel::DoAdasThread::scalAdasFrame(VIDEO_FRAME_INFO_S *pFrmInfo)
{
    status_t ret = NO_ERROR;
    ERRORTYPE mallocRet = SUCCESS;
    unsigned int nPhyAddr = 0;
    void* pVirtAddr = NULL;
    int srcWidth = 0;
    int srcHeight = 0;
    int dstWidth = 0;
    int dstHeight = 0;
    g2d_fmt_enh Format;

    srcWidth = pFrmInfo->VFrame.mWidth;
    srcHeight = pFrmInfo->VFrame.mHeight;
    dstWidth = mAdasDetectParam_v2.width_small;
    dstHeight = mAdasDetectParam_v2.height_small;

    if((dstWidth%16) != 0)
    {
        alogd("The Width should be a multiple of 16!");
    }
    convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pFrmInfo->VFrame.mPixelFormat, &Format);
    mallocRet = AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &pVirtAddr,dstWidth *dstHeight*3/2);

    if(mallocRet != SUCCESS)
    {
        alogd("ion memallco fail!!");
        ret = -1;
        return ret;
    }

    struct mixer_para mixer_blit_para;
    memset(&mixer_blit_para, 0, sizeof(struct mixer_para));

    mixer_blit_para.flag_h = G2D_BLT_NONE_H;
    mixer_blit_para.op_flag = OP_BITBLT;
    mixer_blit_para.src_image_h.use_phy_addr = 1;
    mixer_blit_para.src_image_h.format = Format;
    mixer_blit_para.src_image_h.width = srcWidth;
    mixer_blit_para.src_image_h.height = srcHeight;
    mixer_blit_para.src_image_h.mode = G2D_GLOBAL_ALPHA;
    mixer_blit_para.src_image_h.alpha = 0x0;
    mixer_blit_para.src_image_h.color = 0xFFFFFFFF;
    mixer_blit_para.src_image_h.clip_rect.x = 0;
    mixer_blit_para.src_image_h.clip_rect.y = 0;
    mixer_blit_para.src_image_h.clip_rect.w = srcWidth;
    mixer_blit_para.src_image_h.clip_rect.h = srcHeight;
    mixer_blit_para.src_image_h.align[0] = 0;
    mixer_blit_para.src_image_h.align[1] = 0;
    mixer_blit_para.src_image_h.align[2] = 0;
    mixer_blit_para.src_image_h.laddr[0] = pFrmInfo->VFrame.mPhyAddr[0];
    mixer_blit_para.src_image_h.laddr[1] = pFrmInfo->VFrame.mPhyAddr[1];
    mixer_blit_para.src_image_h.laddr[2] = pFrmInfo->VFrame.mPhyAddr[2];

    mixer_blit_para.dst_image_h.use_phy_addr = 1;
    mixer_blit_para.dst_image_h.format = Format;
    mixer_blit_para.dst_image_h.width = dstWidth;
    mixer_blit_para.dst_image_h.height = dstHeight;
    mixer_blit_para.dst_image_h.mode = G2D_GLOBAL_ALPHA;
    mixer_blit_para.dst_image_h.alpha = 0x0;
    mixer_blit_para.dst_image_h.color = 0xFFFFFFFF;
    mixer_blit_para.dst_image_h.clip_rect.x = 0;
    mixer_blit_para.dst_image_h.clip_rect.y = 0;
    mixer_blit_para.dst_image_h.clip_rect.w = dstWidth;
    mixer_blit_para.dst_image_h.clip_rect.h = dstHeight;
    mixer_blit_para.dst_image_h.align[0] = 0;
    mixer_blit_para.dst_image_h.align[1] = 0;
    mixer_blit_para.dst_image_h.align[2] = 0;
    mixer_blit_para.dst_image_h.laddr[0] = nPhyAddr;
    mixer_blit_para.dst_image_h.laddr[1] = nPhyAddr+dstWidth*dstHeight;
    mixer_blit_para.dst_image_h.laddr[2] = nPhyAddr+dstWidth*dstHeight*5/4;

    unsigned long arg[2];
    arg[0] = (unsigned long)&mixer_blit_para;
    arg[1] = 1;
    ret = ioctl(mAdasG2DHandle, G2D_CMD_MIXER_TASK, arg);
    if(ret < 0)
    {
        alogd("G2D ioctl error scaling failed!!");
        goto error;
    }
    memcpy(mPtrImgBuffer, pVirtAddr, dstWidth*dstHeight*3/2);
#if 0
    g2dScal_i++;
    FILE *fd2;
    char filename2[128];
    sprintf(filename2, "/mnt/extsd/g2d%dx%d_%d.yuv",dstWidth,dstHeight,g2dScal_i);
    fd2 = fopen(filename2, "wb+");
    fwrite(mPtrImgBuffer,dstWidth*dstHeight*3/2,1, fd2);
    fclose(fd2);
#endif
error:
    AW_MPI_SYS_MmzFree(nPhyAddr, pVirtAddr);
    return ret;
}


void AdasInCallBack(ADASInData *ptrADASInData,void *dv)
{
    VIChannel* const mVich = (VIChannel* const)dv;
    if(!mVich->GetmpAdasThread()->mGetFrameDataOk)
    {
        alogw("Frame data has no ready !!!");
        //usleep(20000);
    }
    else
    {
        pthread_mutex_lock(&(mVich->GetmpAdasThread()->mDataTransferLock));
        //init adas in callback data
        ADASInData nADASInData;
        nADASInData.gpsSpeed = mVich->GetmpAdasThread()->mAdasInParam.ngpsSpeed;//KM/H  千米每小时
        nADASInData.gpsSpeedEnable = mVich->GetmpAdasThread()->mAdasInParam.ngpsSpeedEnable;//1-gps速度有效  0-gps速度无效
        //Gsensor数据
        nADASInData.GsensorStop = mVich->GetmpAdasThread()->mAdasInParam.nGsensorStop;//加速度传感器停止标志
        nADASInData.GsensorStopEnable = mVich->GetmpAdasThread()->mAdasInParam.nGsensorStopEnable;//加速度传感器停止标志使能标记
        nADASInData.sensity = mVich->GetmpAdasThread()->mAdasInParam.nsensity;//检测灵敏度
        nADASInData.CPUCostLevel = mVich->GetmpAdasThread()->mAdasInParam.nCPUCostLevel;//cpu消耗等级
        nADASInData.luminanceValue = mVich->GetmpAdasThread()->mAdasInParam.nluminanceValue;//关照强度LV
        //车辆转向灯信号
        nADASInData.lightSignal = mVich->GetmpAdasThread()->mAdasInParam.nlightSignal;//0-无信号 1-左转向  2-右转向
        nADASInData.lightSignalEnable = mVich->GetmpAdasThread()->mAdasInParam.nlightSignalEnable;//信号灯使能信号
        nADASInData.cameraHeight = mVich->GetmpAdasThread()->mAdasInParam.ncameraHeight;//相机安装的高度
        nADASInData.dataType = mVich->GetmpAdasThread()->mAdasInParam.ndataType;//0-YUV 1-NV12 2-NV21 3-RGB...
        nADASInData.cameraType = mVich->GetmpAdasThread()->mAdasInParam.ncameraType;//摄像头类型(0-前置摄像头 1-后置摄像头 2-左侧摄像头 3-右侧摄像头...)
        nADASInData.imageWidth = mVich->GetmpAdasThread()->mAdasInParam.nimageWidth;
        nADASInData.imageHeight = mVich->GetmpAdasThread()->mAdasInParam.nimageHeight;
        nADASInData.cameraNum = mVich->GetmpAdasThread()->mAdasInParam.ncameraNum;//摄像头个数
        //GPS经纬度信息
        nADASInData.Lng = mVich->GetmpAdasThread()->mAdasInParam.nLng;//GPS经度
        nADASInData.LngValue = mVich->GetmpAdasThread()->mAdasInParam.nLngValue;
        nADASInData.Lat = mVich->GetmpAdasThread()->mAdasInParam.nLat;//GPS维度
        nADASInData.LatValue = mVich->GetmpAdasThread()->mAdasInParam.nLatValue;
        nADASInData.altitude = mVich->GetmpAdasThread()->mAdasInParam.naltitude;//GPS海拔
        nADASInData.deviceType = mVich->GetmpAdasThread()->mAdasInParam.ndeviceType;//设备类型(0-行车记录仪 1-后视镜...)
        strcpy(nADASInData.sensorID,mVich->GetmpAdasThread()->mAdasInParam.nsensorID);
        nADASInData.networkStatus = mVich->GetmpAdasThread()->mAdasInParam.nnetworkStatus;//0-端口 1-连接
        nADASInData.ptrFrameDataArray=NULL;
        nADASInData.ptrFrameData=mVich->GetmpAdasThread()->mPtrImgBuffer;//vvvvv:获取captureThread线程得到的图像数据
#if 0
        printf("\n-capture picture data w:%d h:%d-\n",nADASInData.imageWidth,nADASInData.imageHeight);
        unsigned int data_size;
        FILE *fileNode = fopen("/mnt/extsd/adas.yuv", "wb+");
        data_size = (nADASInData.imageWidth * nADASInData.imageHeight)*3/2;
        //yuvBuffer = (unsigned char *)malloc(data_size);
        fwrite(nADASInData.ptrFrameData,1,data_size,fileNode);
        fclose(fileNode);
#endif
        memcpy(ptrADASInData,&nADASInData,sizeof(ADASInData));
        pthread_mutex_unlock(&(mVich->GetmpAdasThread()->mDataTransferLock));
    }
}

void AdasOutCallBack(ADASOutData *ptrADASOutData, void *dv)
{
    VIChannel* const mVich = (VIChannel* const)dv;
    if(!mVich->GetmpAdasThread()->mGetFrameDataOk)
    {
        alogw("Frame data has no ready !!!");
        //usleep(20000);
    }
    else
    {
        if(NULL!=ptrADASOutData)
        {
            AW_AI_ADAS_DETECT_R_ mAwAiAdasDetectR;
            memcpy(&(mAwAiAdasDetectR.nADASOutData), ptrADASOutData, sizeof(ADASOutData));
            mAwAiAdasDetectR.subWidth = mVich->GetmpAdasThread()->m_width;
            mAwAiAdasDetectR.subHeight = mVich->GetmpAdasThread()->m_height;
            std::shared_ptr<CMediaMemory> spMem = std::make_shared<CMediaMemory>(sizeof(AW_AI_ADAS_DETECT_R_));
            memcpy(spMem->getPointer(), &mAwAiAdasDetectR, sizeof(AW_AI_ADAS_DETECT_R_));
            mVich->GetMpCallbackNotifier()->postAdasData(spMem);
            mVich->GetmpAdasThread()->mAdasDataCallBackOut = true;
        }

    }
}

void AdasInCallBack_v2(FRAMEIN &imgIn,void *dv)
{
    VIChannel* const mVich = (VIChannel* const)dv;
    pthread_mutex_lock(&(mVich->GetmpAdasThread()->mDataTransferLock));
    imgIn.mode = mVich->GetmpAdasThread()->mAdasInParam_v2.mode;//模式选择,mode ==1时暂停检测，除v3之外的其他方案请置为 0
    imgIn.imgSmall.imgSize.width = mVich->GetmpAdasThread()->mAdasInParam_v2.small_w;
    imgIn.imgSmall.imgSize.height = mVich->GetmpAdasThread()->mAdasInParam_v2.small_h;

    imgIn.gps.enable = mVich->GetmpAdasThread()->mAdasInParam_v2.gps_enable;//有无GPS信号
    imgIn.gps.speed = mVich->GetmpAdasThread()->mAdasInParam_v2.speed;//车速
    imgIn.sensity.fcwSensity = mVich->GetmpAdasThread()->mAdasInParam_v2.fcwSensity;//前车防撞灵敏度：0：高，1：中，2：低    默认值为1
    imgIn.sensity.ldwSensity = mVich->GetmpAdasThread()->mAdasInParam_v2.ldwSensity;//车道偏离灵敏度：0：高，1：中，2：低    默认值为1
    imgIn.imgSmall.ptr=mVich->GetmpAdasThread()->mPtrImgBuffer;//vvvvv:获取captureThread线程得到的图像数据
    //alogd("In--> W:%d H:%d",imgIn.imgSmall.imgSize.width,imgIn.imgSmall.imgSize.height);
    #if 0
        printf("\n-capture picture data w:%d h:%d-\n",960,540);
        unsigned int data_size;
        FILE *fileNode = fopen("/mnt/extsd/adas.yuv", "wb+");
        data_size = (960 * 540);
        //yuvBuffer = (unsigned char *)malloc(data_size);
        fwrite(imgIn.imgSmall.ptr,1,data_size,fileNode);
        fclose(fileNode);
    #endif
    pthread_mutex_unlock(&(mVich->GetmpAdasThread()->mDataTransferLock));
}

void AdasOutCallBack_v2(ADASOUT &adasOut,void *dv)
{
        VIChannel* const mVich = (VIChannel* const)dv;
        if(!mVich->GetmpAdasThread()->mGetFrameDataOk)
        {
            alogw("Frame data has no ready !!!");
            //usleep(20000);
        }
        else
        {
            //if(NULL!=&adasOut)
            //{
                AW_AI_ADAS_DETECT_R__v2 mAwAiAdasDetectR_v2;
                memcpy(&(mAwAiAdasDetectR_v2.nADASOutData_v2), &adasOut, sizeof(ADASOUT));
                mAwAiAdasDetectR_v2.subWidth = mVich->GetmpAdasThread()->m_width;
                mAwAiAdasDetectR_v2.subHeight = mVich->GetmpAdasThread()->m_height;
                //alogd("Out--> w:%d,h:%d",mVich->GetmpAdasThread()->m_width,mVich->GetmpAdasThread()->m_height);
                std::shared_ptr<CMediaMemory> spMem = std::make_shared<CMediaMemory>(sizeof(AW_AI_ADAS_DETECT_R__v2));
                memcpy(spMem->getPointer(), &mAwAiAdasDetectR_v2, sizeof(AW_AI_ADAS_DETECT_R__v2));
                mVich->GetMpCallbackNotifier()->postAdasData(spMem);
                mVich->GetmpAdasThread()->mAdasDataCallBackOut = true;
            //}
        }
}

status_t VIChannel::DoAdasThread::startAdasDetect()
{
    status_t eRet = NO_ERROR;
    #if(MPPCFG_ADAS_DETECT == OPTION_ADAS_DETECT_ENABLE)
    AutoMutex autoLock(mAdasDetectLock);
    if(mbAdasDetectEnable)
    {
        alogw("adas detect already enable");
        return NO_ERROR;
    }
    //prepare eve face lib.
    //initilize
    m_width = mpViChn->mFrameWidth;
    m_height = mpViChn->mFrameHeight;
    //init adas data
    ADASPara nADASPara;
    //ADAS init default data
    nADASPara.frameWidth = mAdasDetectParam.nframeWidth;//视频宽度
    nADASPara.frameHeight = mAdasDetectParam.nframeHeight;//视频高度
    nADASPara.roiX = mAdasDetectParam.nroiX;//感兴趣区域X坐标
    nADASPara.roiY = mAdasDetectParam.nroiY;//感兴趣区域Y坐标
    nADASPara.roiW = mAdasDetectParam.nroiW;//感兴趣区域宽度
    nADASPara.roiH = mAdasDetectParam.nroiH;//感兴趣区域高度
    nADASPara.fps = mpViChn->mParameters.mFrameRate;//视频的帧率fps
    //camera paramter 
    nADASPara.focalLength = mAdasDetectParam.nfocalLength;//焦距um
    nADASPara.pixelSize = mAdasDetectParam.npixelSize;//sensor 单个像素大小um
    nADASPara.horizonViewAngle = mAdasDetectParam.nhorizonViewAngle;//水平视场角
    nADASPara.verticalViewAngle = mAdasDetectParam.nverticalViewAngle;//垂直视场角
    nADASPara.vanishX = mAdasDetectParam.nvanishX;//天际消失点X坐标
    nADASPara.vanishY = mAdasDetectParam.nvanishY;//天际消失点Y坐标
    nADASPara.hoodLineDist = mAdasDetectParam.nhoodLineDist;//车头距离
    nADASPara.vehicleWidth = mAdasDetectParam.nvehicleWidth;//车身宽度
    nADASPara.wheelDist = mAdasDetectParam.nwheelDist;//车轮距离
    nADASPara.cameraToCenterDist = mAdasDetectParam.ncameraToCenterDist;//摄像头中心距离
    nADASPara.cameraHeight = mAdasDetectParam.ncameraHeight;//相机安装高度
    nADASPara.leftSensity = mAdasDetectParam.nleftSensity;//左侧车道线检测灵敏度
    nADASPara.rightSensity = mAdasDetectParam.nrightSensity;//右侧车道线检测灵敏度
    nADASPara.fcwSensity = mAdasDetectParam.nfcwSensity;//前车碰撞预警灵敏度
    nADASPara.awAdasIn = AdasInCallBack;
    nADASPara.awAdasOut = AdasOutCallBack;
    //init ADAS
    AW_AI_ADAS_Init(&nADASPara,(void*)mpViChn);
    mPtrImgBuffer = (unsigned char *)malloc(m_width * m_height*3/2);
    if(mPtrImgBuffer == NULL)
    {
        aloge("malloc mPtrImgBuffer fail !!!");
        return UNKNOWN_ERROR;
    }
    //notify faceDetect thread to detectOn.
    {
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeADAS_DetectOn;
        mMsgQueue.queueMessage(&msg);
        while(!mbAdasDetectEnable)
        {
            mCondAdasDetect.wait(mAdasDetectLock);
        }
    }
    return NO_ERROR;
_err0:
    #else
    alogd("adas detect is disable\n");
    #endif
    return eRet;
}

status_t VIChannel::DoAdasThread::startAdasDetect_v2()
{   
#if 1
    status_t eRet = NO_ERROR;
    #if(MPPCFG_ADAS_DETECT_V2 == OPTION_ADAS_DETECT_ENABLE)
    AutoMutex autoLock(mAdasDetectLock);
    if(mbAdasDetectEnable)
    {
        alogw("adas_v2 detect already enable");
        return NO_ERROR;
    }
    m_width = mAdasDetectParam_v2.width_small;
    m_height = mAdasDetectParam_v2.height_small;
    //init adas data
    ADASIN nADASPara_v2;
    //ADAS init default data

    nADASPara_v2.config.isGps = mAdasDetectParam_v2.isGps;//有无gps
    nADASPara_v2.config.isGsensor = mAdasDetectParam_v2.isGsensor;//有无Gsensor，出v3之外其他方案请置为0
    nADASPara_v2.config.isObd = mAdasDetectParam_v2.isObd;//有无读取OBD
    nADASPara_v2.config.isCalibrate = mAdasDetectParam_v2.isCalibrate;//是否进行安装标定

    nADASPara_v2.fps = mAdasDetectParam_v2.fps;//帧率
    nADASPara_v2.imgSizeBig.width = mAdasDetectParam_v2.width_big;//主码流图 1920*1080
    nADASPara_v2.imgSizeBig.height = mAdasDetectParam_v2.height_big;

    nADASPara_v2.imgSizeSmall.width = mAdasDetectParam_v2.width_small;//子码流图 960*540
    nADASPara_v2.imgSizeSmall.height = mAdasDetectParam_v2.height_small;
    
    nADASPara_v2.viewAng.angH = mAdasDetectParam_v2.angH;//镜头的水平视角
    nADASPara_v2.viewAng.angV = mAdasDetectParam_v2.angV;//镜头的垂直视角
    nADASPara_v2.viewAng.widthOri = mAdasDetectParam_v2.widthOri;//原始的图像宽度
    nADASPara_v2.viewAng.heightOri = mAdasDetectParam_v2.heightOri;//原始的图像高度
    nADASPara_v2.vanishLine = mAdasDetectParam_v2.vanishLine;//???
    nADASPara_v2.vanishLineIsOk = mAdasDetectParam_v2.vanishLineIsOk;//????

    nADASPara_v2.table.imgSize.width = nADASPara_v2.imgSizeSmall.width;
    nADASPara_v2.table.imgSize.height = nADASPara_v2.imgSizeSmall.height;
    nADASPara_v2.table.ptr = NULL;

    printf("angH:%d,angV3:%d\n\n",nADASPara_v2.viewAng.angH,nADASPara_v2.viewAng.angV);
    mPtrImgBuffer = (unsigned char *)malloc(m_width * m_height*3/2);
    if(mPtrImgBuffer == NULL)
    {
        aloge("malloc mPtrImgBuffer fail !!!");
        return UNKNOWN_ERROR;
    }
    callbackIn=AdasInCallBack_v2;
    callbackOut=AdasOutCallBack_v2;

    //init ADAS
    InitialAdas(nADASPara_v2,(void **)mpViChn);

    // open g2dDriver
    if(mAdasG2DHandle < 0)
    {
        mAdasG2DHandle = open("/dev/g2d", O_RDWR, 0);
        if (mAdasG2DHandle < 0)
        {
            alogd("fatal error! open /dev/g2d failed");
        }
        alogd("open /dev/g2d OK");
    }
    else
    {
        alogd("fatal error! why g2dDriver[%d] is open?", mAdasG2DHandle);
    }


    //notify faceDetect thread to detectOn.
    {
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeADAS_DetectOn;
        mMsgQueue.queueMessage(&msg);
        while(!mbAdasDetectEnable)
        {
            mCondAdasDetect.wait(mAdasDetectLock);
        }
    }
    return NO_ERROR;
_err0:
    alogd("adas_v2 detect is disable\n");
    #endif
    return eRet;
#endif
}

status_t VIChannel::DoAdasThread::stopAdasDetect()
{
    status_t eRet = NO_ERROR;
    #if(MPPCFG_ADAS_DETECT == OPTION_ADAS_DETECT_ENABLE)
    AutoMutex autoLock(mAdasDetectLock);
    if(!mbAdasDetectEnable)
    {
        alogw("adas already disable");
        return NO_ERROR;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeADAS_DetectOff;
    mMsgQueue.queueMessage(&msg);
    while(mbAdasDetectEnable)
    {
        mCondAdasDetect.wait(mAdasDetectLock);
    }
    //do uninit adas
    AW_AI_ADAS_UnInit();
    if(mPtrImgBuffer)
    {
        free(mPtrImgBuffer);
        mPtrImgBuffer = NULL;
    }
    #else 
    alogd("adas detect is disable\n");
    #endif
    
    return eRet;
}
status_t VIChannel::DoAdasThread::stopAdasDetect_v2()
{
    status_t eRet = NO_ERROR;
    #if(MPPCFG_ADAS_DETECT_V2 == OPTION_ADAS_DETECT_ENABLE)
    AutoMutex autoLock(mAdasDetectLock);
    if(!mbAdasDetectEnable)
    {
        alogw("adas already disable");
        return NO_ERROR;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeADAS_DetectOff;
    mMsgQueue.queueMessage(&msg);
    while(mbAdasDetectEnable)
    {
        mCondAdasDetect.wait(mAdasDetectLock);
    }
    //do uninit adas
    ReleaseAdas();
    if(mPtrImgBuffer)
    {
        free(mPtrImgBuffer);
        mPtrImgBuffer = NULL;
    }

    if(mAdasG2DHandle >= 0)
    {
        close(mAdasG2DHandle);
        mAdasG2DHandle = -1;
    }

    alogd("adas detect is disable\n");
    #endif
    
    return eRet;
}

bool VIChannel::DoAdasThread::IsAdasDetectEnable()
{
    AutoMutex autoLock(mAdasDetectLock);
    return mbAdasDetectEnable;
}
status_t VIChannel::DoAdasThread::sendFrame(VIDEO_FRAME_BUFFER_S *pFrmBuf)
{
    status_t ret;
    AutoMutex lock(mAdasDetectLock);
    if(false == mbAdasDetectEnable)
    {
        alogw("don't send frame when ADAS disable!");
        return INVALID_OPERATION;
    }

    ret = mpADASFrameQueue->PutElemDataValid((void*)pFrmBuf);
    if (NO_ERROR != ret)
    {
        return ret;
    }

    AutoMutex lock2(mWaitLock);
    if(mbWaitFrame)
    {
        mbWaitFrame = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeADAS_InputFrameAvailable;
        mMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}
bool VIChannel::DoAdasThread::threadLoop()
{
    if(!exitPending())
    {
        return AdasThread();
    } 
    else
    {
        return false;
    }
}
bool VIChannel::DoAdasThread::AdasThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    while(1)
    {
    PROCESS_MESSAGE:
        getMsgRet = mMsgQueue.dequeueMessage(&msg);
        if(getMsgRet == NO_ERROR)
        {
            if(MsgTypeADAS_DetectOn == msg.mMsgType)
            {
                AutoMutex autoLock(mAdasDetectLock);
                if(!mbAdasDetectEnable)
                {
                    if(mpADASFrameQueue)
                    {
                        aloge("fatal error! why pQueue is not null?");
                        delete mpADASFrameQueue;
                    }
                    mpADASFrameQueue = new EyeseeQueue();
                    mFrameCounter = 0;
                    mbAdasDetectEnable = true;
                }
                else
                {
                    alogw("already enable MOD");
                }
                mCondAdasDetect.signal();
            }
            else if(MsgTypeADAS_DetectOff == msg.mMsgType)
            {
                AutoMutex autoLock(mAdasDetectLock);
                if(mbAdasDetectEnable)
                {
                    releaseAllFrames();
                    delete mpADASFrameQueue;
                    mpADASFrameQueue = NULL;
                    mbAdasDetectEnable = false;
                }
                else
                {
                    alogw("already disable Adas");
                    if(mpADASFrameQueue)
                    {
                        aloge("fatal error! why Adas frame queue is not null?");
                        delete mpADASFrameQueue;
                        mpADASFrameQueue = NULL;
                    }
                }
                mCondAdasDetect.signal();
            }
            else if(MsgTypeADAS_InputFrameAvailable == msg.mMsgType)
            {
                alogv("Adas frame input");
            }
            else if(MsgTypeADAS_Exit == msg.mMsgType)
            {
                AutoMutex autoLock(mAdasDetectLock);
                if(mbAdasDetectEnable)
                {
                    aloge("fatal error! must stop ADAS before exit!");
                    mbAdasDetectEnable = false;
                }
                if(mpADASFrameQueue)
                {
                    releaseAllFrames();
                    delete mpADASFrameQueue;
                    mpADASFrameQueue = NULL;
                }
                bRunningFlag = false;
                goto _exit0;
            }
            else
            {
                aloge("fatal error! unknown msg[0x%x]!", msg.mMsgType);
            }
            goto PROCESS_MESSAGE;
        }

        if(mbAdasDetectEnable)
        {
            pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpADASFrameQueue->GetValidElemData();
            if (pFrmbuf == NULL) 
            {
                {
                    AutoMutex lock(mWaitLock);
                    if(mpADASFrameQueue->GetValidElemNum() > 0)
                    {
                        alogw("Low probability! Adas new frame come before check again.");
                        goto PROCESS_MESSAGE;
                    }
                    else
                    {
                        mbWaitFrame = true;
                    }
                }
                mMsgQueue.waitMessage();
                goto PROCESS_MESSAGE;
            }
            //回调数据到应用
        #if(MPPCFG_ADAS_DETECT == OPTION_ADAS_DETECT_ENABLE)
            // video frame time
            mFrameCounter++;
            if(mAdasDataCallBackOut == true)
            {
                if(NULL != mPtrImgBuffer)
                {
                    pthread_mutex_lock(&mDataTransferLock);
                    //copy data
                    memcpy(mPtrImgBuffer, (unsigned char *)(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[0]), m_width*m_height);
                    memcpy(mPtrImgBuffer+m_width*m_height, (unsigned char *)(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[1]), m_width*m_height/2);
                    pthread_mutex_unlock(&mDataTransferLock);
                    mAdasDataCallBackOut = false;
                    mGetFrameDataOk = true;
                }
                else
                {
                    aloge("mPtrImgBuffer is NUll ");
                }
            }
            mpViChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
            mpADASFrameQueue->ReleaseElemData(pFrmbuf); //release to idle list
        #elif(MPPCFG_ADAS_DETECT_V2 == OPTION_ADAS_DETECT_ENABLE)
            // video frame time
            mFrameCounter++;
            if(mAdasDataCallBackOut == true)
            {
                if(NULL != mPtrImgBuffer)
                {
                    pthread_mutex_lock(&mDataTransferLock);
                    //scal the frame which needed by adas
                    scalAdasFrame(&pFrmbuf->mFrameBuf);
                    pthread_mutex_unlock(&mDataTransferLock);
                    mAdasDataCallBackOut = false;
                    mGetFrameDataOk = true;
                }
                else
                {
                    aloge("mPtrImgBuffer is NUll ");
                }
            }
            mpViChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
            mpADASFrameQueue->ReleaseElemData(pFrmbuf); //release to idle list
        #else
            aloge("adas detect is disable\n");
            return false;
        #endif
        }
        else
        {
            mMsgQueue.waitMessage();
        }
        
    }
_exit0:
    return bRunningFlag;
}
status_t VIChannel::DoAdasThread::releaseAllFrames()
{
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    while(1)
    {
        pFrmbuf = (VIDEO_FRAME_BUFFER_S*)mpADASFrameQueue->GetValidElemData();
        if(pFrmbuf)
        {
            mpViChn->releaseFrame(pFrmbuf->mFrameBuf.mId);
            mpADASFrameQueue->ReleaseElemData(pFrmbuf); //release to idle list
        }
        else
        {
            break;
        }
    }
    return NO_ERROR;
}
VIChannel::VIChannel(ISP_DEV nIspDevId, int chnId, bool bForceRef)
    : mbForceRef(bForceRef)
    , mChnId(chnId)
    , mpPreviewQueue(NULL)
    , mpPictureQueue(NULL)
    //, mpVideoFrmBuffer(NULL)
    , mChannelState(VI_CHN_STATE_CONSTRUCTED)
    , mIsPicCopy(false)
    , mContinuousPictureCnt(0)
    , mContinuousPictureMax(0)
    , mContinuousPictureStartTime(0)
    , mContinuousPictureLast(0)
    , mContinuousPictureInterval(0)
    , mColorSpace(0)
    , mFrameWidth(0)
    , mFrameHeight(0)
    , mFrameWidthOut(0)
    , mFrameHeightOut(0)
    , mCamDevTimeoutCnt(0)
    , PicCapModeEn(0)
    , FrmDrpCntForPicCapMode(0)
    , FrmDrpThrForPicCapMode(3)
{
    alogv("Construct");
    mIspDevId = nIspDevId;
    memset(&mRectCrop, 0, sizeof(mRectCrop));

    mpMemOps = MemAdapterGetOpsS();
    if (CdcMemOpen(mpMemOps) < 0) {
        aloge("CdcMemOpen failed!");
    }
    alogv("CdcMemOpen ok");

    mLastZoom = -1;
    mNewZoom = 0;
    mMaxZoom = 10;
    mbPreviewEnable = true;
    mbKeepPictureEncoder = false;

    mFrameCounter = 0;
    mDbgFrameNum = 0;
    
    mpCallbackNotifier = new CallbackNotifier(mChnId, this);
    mpPreviewWindow = new PreviewWindow(this);

    mpCapThread = new DoCaptureThread(this);
    mpCapThread->startThread();

    mpPrevThread = new DoPreviewThread(this);
    mpPrevThread->startThread();

    mpPicThread = new DoPictureThread(this);
    mpPicThread->startThread();

    mpCommandThread = new DoCommandThread(this);
    mpCommandThread->startThread();
#if(MPPCFG_MOTION_DETECT_SOFT == OPTION_MOTION_DETECT_SOFT_ENABLE)
    mpMODThread = new DoMODThread(this);
    mpMODThread->startThread();
#else
    mpMODThread = NULL;
#endif
    
#if(MPPCFG_ADAS_DETECT == OPTION_ADAS_DETECT_ENABLE)
    mpAdasThread = new DoAdasThread(this);
    mpAdasThread->startThread();
#elif(MPPCFG_ADAS_DETECT_V2 == OPTION_ADAS_DETECT_ENABLE)
    mpAdasThread = new DoAdasThread(this);
    mpAdasThread->startThread();
#else
    mpAdasThread = NULL;
#endif
    mTakePicMsgType = 0;
    mbTakePictureStart = false;
    std::vector<CameraParameters::SensorParamSet> SensorParamSets;
    SensorParamSets.emplace_back(3840, 2160, 15);
    SensorParamSets.emplace_back(3840, 2160, 25);
    SensorParamSets.emplace_back(3840, 2160, 30);
    SensorParamSets.emplace_back(3840, 2160, 50);
    SensorParamSets.emplace_back(3840, 2160, 60);
    SensorParamSets.emplace_back(2160, 2160, 30);
    SensorParamSets.emplace_back(1920, 1080, 30);
    SensorParamSets.emplace_back(1920, 1080, 60);
    SensorParamSets.emplace_back(1920, 1080, 120);
    SensorParamSets.emplace_back(1280, 720, 180);
    SensorParamSets.emplace_back(1280, 540, 240);
    mParameters.setSupportedSensorParamSets(SensorParamSets);
    mParameters.setPreviewFrameRate(SensorParamSets[0].mFps);
    SIZE_S size={(unsigned int)SensorParamSets[0].mWidth, (unsigned int)SensorParamSets[0].mHeight};
    mParameters.setVideoSize(size);
    mParameters.setPreviewFormat(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    mParameters.setColorSpace(V4L2_COLORSPACE_JPEG);
    mParameters.setVideoBufferNumber(5);
    //preview rotation setting
    mParameters.setPreviewRotation(0);
    mParameters.setPictureMode(TAKE_PICTURE_MODE_FAST);
    //digital zoom setting
    mParameters.setZoomSupported(true);
    mParameters.setZoom(mNewZoom);
    mParameters.setMaxZoom(mMaxZoom);
    //The default is offline
    mParameters.setOnlineEnable(false);
    mParameters.setOnlineShareBufNum(0);
}

VIChannel::~VIChannel()
{
    alogv("Destruct");
    if (mpCommandThread != NULL) 
    {
        mpCommandThread->stopThread();
        delete mpCommandThread;
    }
    if (mpCapThread != NULL)
    {
        mpCapThread->stopThread();
        delete mpCapThread;
    }
    if (mpPrevThread != NULL)
    {
        mpPrevThread->stopThread();
        delete mpPrevThread;
    }
    if (mpPicThread != NULL)
    {
        mpPicThread->stopThread();
        delete mpPicThread;
    }
    if (mpMODThread != NULL)
    {
        mpMODThread->stopThread();
        delete mpMODThread;
    }
    if (mpAdasThread != NULL)
    {
        mpAdasThread->stopThread();
        delete mpAdasThread;
    }
    if (mpPreviewWindow != NULL) {
        delete mpPreviewWindow;
    }
    if (mpCallbackNotifier != NULL) {
        delete mpCallbackNotifier;
    }

//    if (mpVideoFrmBuffer != NULL) {
//        free(mpVideoFrmBuffer);
//    }
    mFrameBuffers.clear();
    mFrameBufferIdList.clear();
    if (mpPreviewQueue != NULL) {
        OSAL_QueueTerminate(mpPreviewQueue);
        delete mpPreviewQueue;
    }
    if (mpPictureQueue != NULL) {
        OSAL_QueueTerminate(mpPictureQueue);
        delete mpPictureQueue;
    }

    CdcMemClose(mpMemOps);
}

void VIChannel::calculateCrop(RECT_S *rect, int zoom, int width, int height)
{
    rect->X = (width - width * 10 / (10 + zoom)) / 2;
    rect->Y = (height - height * 10 / (10 + zoom)) / 2;
    rect->Width = width - rect->X * 2;
    rect->Height = height - rect->Y * 2;
}

int VIChannel::planeNumOfV4l2PixFmt(int nV4l2PixFmt)
{
    int nPlaneNum;
    switch(nV4l2PixFmt)
    {
        case V4L2_PIX_FMT_NV12M:
        case V4L2_PIX_FMT_NV21M:
            nPlaneNum = 2;
            break;
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_YUV420M:
        case V4L2_PIX_FMT_YVU420M:
            nPlaneNum = 3;
            break;
        default:
            aloge("Unknown V4l2Pixelformat[0x%x].", nV4l2PixFmt);
            nPlaneNum = 1;
            break;
    }
    return nPlaneNum;
}

status_t VIChannel::prepare()
{
    AutoMutex lock(mLock);

    if (mChannelState != VI_CHN_STATE_CONSTRUCTED) 
    {
        aloge("prepare in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    alogv("Vipp dev[%d] chn[0]", mChnId);
    ERRORTYPE eRet = AW_MPI_VI_CreateVipp(mChnId);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
        return UNKNOWN_ERROR;
    }

    VI_ATTR_S attr;
    memset(&attr, 0, sizeof(VI_ATTR_S));
    attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    attr.memtype = V4L2_MEMORY_MMAP;
    attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(mParameters.getPreviewFormat());
    attr.format.field = V4L2_FIELD_NONE;
    attr.format.colorspace = mParameters.getColorSpace();
    SIZE_S size;
    mParameters.getVideoSize(size);
    attr.format.width = size.Width;
    attr.format.height = size.Height;
    attr.nbufs = mParameters.getVideoBufferNumber();
    attr.nplanes = 0;
    attr.fps = mParameters.getPreviewFrameRate();
    attr.capturemode = mParameters.getCaptureMode();
    attr.use_current_win = mbForceRef?0:1;
    attr.drop_frame_num = mParameters.getLostFrameNumber();
    //set online
    attr.mOnlineEnable = mParameters.getOnlineEnable();
    attr.mOnlineShareBufNum = (enum dma_buffer_num)mParameters.getOnlineShareBufNum();
    //check sensor param valid.
    if(mbForceRef)
    {
        bool bMatch = false;
        CameraParameters::SensorParamSet userParam = {(int)attr.format.width, (int)attr.format.height, (int)attr.fps};
        std::vector<CameraParameters::SensorParamSet> supportedSensorParamSets;
        mParameters.getSupportedSensorParamSets(supportedSensorParamSets);
        for(CameraParameters::SensorParamSet &i : supportedSensorParamSets)
        {
            if(userParam == i)
            {
                bMatch = true;
                break;
            }
        }
        if(!bMatch)
        {
            alogw("Be careful! not find match sensor param set! isp param will be selected near user param!");
        }
        alogv("set user sensor param: vipp[%d], size[%dx%d], fps[%d]", mChnId, userParam.mWidth, userParam.mHeight, userParam.mFps);
    }
    alogd("set vipp[%d]attr:[%dx%d],fps[%d], pixFmt[0x%x], nbufs[%d]", mChnId, attr.format.width, attr.format.height, attr.fps, attr.format.pixelformat, attr.nbufs);

    eRet = AW_MPI_VI_SetVippAttr(mChnId, &attr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
    eRet = AW_MPI_VI_GetVippAttr(mChnId, &attr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI GetVippAttr failed");
    }
    //use VI_ATTR_S to init CameraParameters.
    alogd("get vipp[%d]attr:[%dx%d],fps[%d], pixFmt[0x%x], nbufs[%d]", mChnId, attr.format.width, attr.format.height, attr.fps, attr.format.pixelformat, attr.nbufs);
    size.Width = attr.format.width;
    size.Height = attr.format.height;
    mParameters.setVideoSize(size);
    mParameters.setPreviewFrameRate(attr.fps);
    mParameters.setPreviewFormat(map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(attr.format.pixelformat));
    mParameters.setColorSpace((enum v4l2_colorspace)attr.format.colorspace);
    mParameters.setVideoBufferNumber(attr.nbufs);

    //mirror
    int nMirror = mParameters.GetMirror();
    alogv("set vipp mirror[%d]", nMirror);
    AW_MPI_VI_SetVippMirror(mChnId, nMirror);
    //Flip
    int nFlip = mParameters.GetFlip();
    alogv("set flip[%d]", nFlip);
    AW_MPI_VI_SetVippFlip(mChnId, nFlip);
    
    //preview rotation setting
    mpPreviewWindow->setDisplayFrameRate(mParameters.getDisplayFrameRate());
    mpPreviewWindow->setPreviewRotation(mParameters.getPreviewRotation());

    RECT_S mB4G2dRegion = mParameters.getB4G2dUserRegion();
    if(mB4G2dRegion.Width !=0 && mB4G2dRegion.Height !=0)
    {
        mpPreviewWindow->setB4G2dPreviewRegion(mParameters.getB4G2dUserRegion());
        alogd("Enable B4G2dRegion it will saving memory!");
    }
    RECT_S mUserRegion = mParameters.getUserRegion();
    if(mUserRegion.Width !=0 && mUserRegion.Height !=0)
    {
        mpPreviewWindow->setPreviewRegion(mParameters.getUserRegion());
        alogd("Enable UserRegion!");
    }
    mpPreviewWindow->setDispBufferNum(2);
    //digital zoom setting
    mLastZoom = -1;
    if(mParameters.isZoomSupported())
    {
        mNewZoom = mParameters.getZoom();
        mMaxZoom = mParameters.getMaxZoom();
    }

    mColorSpace = attr.format.colorspace;
    mFrameWidth = attr.format.width;
    mFrameHeight = attr.format.height;

    SIZE_S sizeOut;
    mParameters.getVideoSizeOut(sizeOut);
    mFrameWidthOut = sizeOut.Width;
    mFrameHeightOut = sizeOut.Height;

    mChannelState = VI_CHN_STATE_PREPARED;
    return OK;
}

status_t VIChannel::release()
{
    AutoMutex lock(mLock);

    if (mChannelState != VI_CHN_STATE_PREPARED) 
    {
        aloge("release in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    ERRORTYPE ret = AW_MPI_VI_DestroyVipp(mChnId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! DestroyVipp fail!");
    }
    mChannelState = VI_CHN_STATE_CONSTRUCTED;
    return NO_ERROR;
}

status_t VIChannel::startChannel()
{
    AutoMutex lock1(mLock);

    if (mChannelState != VI_CHN_STATE_PREPARED)
    {
        aloge("startChannel in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    
    //create resources
    int nBufNum = mParameters.getVideoBufferNumber();
//    if (mpVideoFrmBuffer == NULL)
//    {
//        mpVideoFrmBuffer = (VIDEO_FRAME_BUFFER_S*)malloc(sizeof(VIDEO_FRAME_BUFFER_S) * nBufNum);
//        if (mpVideoFrmBuffer == NULL) 
//        {
//            aloge("fatal error! alloc mpVideoFrmBuffer error!");
//            return NO_MEMORY;
//        }
//    }
    if(!mFrameBuffers.empty())
    {
        aloge("fatal error! why frame buffers is not empty?");
        mFrameBuffers.clear();
    }
    if(!mFrameBufferIdList.empty())
    {
        aloge("fatal error! why bufferIdList is not empty?");
        mFrameBufferIdList.clear();
    }

    if (mpPreviewQueue == NULL) 
    {
        mpPreviewQueue = new OSAL_QUEUE;
    } 
    else 
    {
        OSAL_QueueTerminate(mpPreviewQueue);
    }
    OSAL_QueueCreate(mpPreviewQueue, nBufNum);

    if (mpPictureQueue == NULL) 
    {
        mpPictureQueue = new OSAL_QUEUE;
    } 
    else 
    {
        OSAL_QueueTerminate(mpPictureQueue);
    }
    OSAL_QueueCreate(mpPictureQueue, nBufNum);
    
    alogv("startChannel");
    ERRORTYPE ret = AW_MPI_VI_EnableVipp(mChnId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    ret = AW_MPI_VI_CreateVirChn(mChnId, 0, NULL);
    if(ret != SUCCESS)
    {
        aloge("fatal error! createVirChn fail!");
    }
    ret = AW_MPI_VI_EnableVirChn(mChnId, 0);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enableVirChn fail!");
    }
    if(mbPreviewEnable)
    {
        mpPreviewWindow->startPreview();
    }
    mpCapThread->startCapture();
    mDbgFrameNum = 0;
    mDbgFrameFilePathList.clear();
    mFrameCounter = 0;
    mChannelState = VI_CHN_STATE_STARTED;
    return NO_ERROR;
}

status_t VIChannel::stopChannel()
{
    AutoMutex lock1(mLock);

    if (mChannelState != VI_CHN_STATE_STARTED)
    {
        aloge("stopChannel in error state %d", mChannelState);
        return INVALID_OPERATION;
    }

    alogv("stopChannel");
    if(mpMODThread)
    {
        mpMODThread->stopMODDetect();
    }
    mpPreviewWindow->stopPreview();
    mpCapThread->pauseCapture();
    mpPrevThread->releaseAllFrames();
    mpPicThread->releaseAllFrames();

    ERRORTYPE ret;
    ret = AW_MPI_VI_DisableVirChn(mChnId, 0);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disableVirChn fail!");
    }
    ret = AW_MPI_VI_DestroyVirChn(mChnId, 0);
    if(ret != SUCCESS)
    {
        aloge("fatal error! destroyVirChn fail!");
    }
    ret = AW_MPI_VI_DisableVipp(mChnId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! disableVipp fail!");
    }
    //destroy resources
//    if (mpVideoFrmBuffer != NULL) 
//    {
//        free(mpVideoFrmBuffer);
//        mpVideoFrmBuffer = NULL;
//    }
    mFrameBuffers.clear();
    if(!mFrameBufferIdList.empty())
    {
        aloge("fatal error! why bufferIdList is not empty?");
        mFrameBufferIdList.clear();
    }
    if (mpPreviewQueue != NULL) 
    {
        OSAL_QueueTerminate(mpPreviewQueue);
        delete mpPreviewQueue;
        mpPreviewQueue = NULL;
    }
    if (mpPictureQueue != NULL) 
    {
        OSAL_QueueTerminate(mpPictureQueue);
        delete mpPictureQueue;
        mpPictureQueue = NULL;
    }
    
    mChannelState = VI_CHN_STATE_PREPARED;
    return NO_ERROR;
}

VIChannel::VIChannelState VIChannel::getState()
{
    AutoMutex lock(mLock);
    return mChannelState;
}
status_t VIChannel::getMODParams(MOTION_DETECT_ATTR_S *pParamMD)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpMODThread)
    {
        ret = mpMODThread->getMODParams(pParamMD);
    }
    else
    {
        aloge("fatal error! mod thread is null");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

status_t VIChannel::setMODParams(MOTION_DETECT_ATTR_S pParamMD)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpMODThread)
    {
        ret = mpMODThread->setMODParams(pParamMD);
    }
    else
    {
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

status_t VIChannel::startMODDetect()
{
    AutoMutex autoLock(mLock);
    if (mChannelState != VI_CHN_STATE_STARTED) 
    {
        aloge("start Motion Object Detection in error state %d", mChannelState);
        return INVALID_OPERATION;
    }

    if(mpMODThread)
    {
        return mpMODThread->startMODDetect();
    }
    else
    {
        return INVALID_OPERATION;
    }
}

status_t VIChannel::stopMODDetect()
{
    AutoMutex autoLock(mLock);
    if (mChannelState != VI_CHN_STATE_STARTED)
    {
        aloge("stop Motion Object Detection in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    if(mpMODThread)
    {
        return mpMODThread->stopMODDetect();
    }
    else
    {
        return INVALID_OPERATION;
    }
}

status_t VIChannel::getAdasParams(AdasDetectParam *pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->getAdasParams(pParamADAS);
    }
    else
    {
        aloge("fatal error! mod thread is null");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::setAdasParams(AdasDetectParam pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->setAdasParams(pParamADAS);
    }
    else
    {
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::getAdasInParams(AdasInParam *pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->getAdasInParams(pParamADAS);
    }
    else
    {
        aloge("fatal error! mod thread is null");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::setAdasInParams(AdasInParam pParamADAS)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->setAdasInParams(pParamADAS);
    }
    else
    {
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::startAdasDetect()
{
    AutoMutex autoLock(mLock);
    if (mChannelState != VI_CHN_STATE_STARTED) 
    {
        aloge("start Motion Object Detection in error state %d", mChannelState);
        return INVALID_OPERATION;
    }

    if(mpAdasThread)
    {
        return mpAdasThread->startAdasDetect();
    }
    else
    {
        return INVALID_OPERATION;
    }
}
status_t VIChannel::stopAdasDetect()
{
    AutoMutex autoLock(mLock);
    if (mChannelState != VI_CHN_STATE_STARTED)
    {
        aloge("stop Motion Object Detection in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    if(mpAdasThread)
    {
        return mpAdasThread->stopAdasDetect();
    }
    else
    {
        return INVALID_OPERATION;
    }
}
//===========
status_t VIChannel::getAdasParams_v2(AdasDetectParam_v2 *pParamADAS_v2)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->getAdasParams_v2(pParamADAS_v2);
    }
    else
    {
        aloge("fatal error! mod thread is null");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::setAdasParams_v2(AdasDetectParam_v2 pParamADAS_v2)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->setAdasParams_v2(pParamADAS_v2);
    }
    else
    {
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::getAdasInParams_v2(AdasInParam_v2 *pParamADAS_v2)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->getAdasInParams_v2(pParamADAS_v2);
    }
    else
    {
        aloge("fatal error! mod thread is null");
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::setAdasInParams_v2(AdasInParam_v2 pParamADAS_v2)
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mpAdasThread)
    {
        ret = mpAdasThread->setAdasInParams_v2(pParamADAS_v2);
    }
    else
    {
        ret = UNKNOWN_ERROR;
    }
    return ret;
}
status_t VIChannel::startAdasDetect_v2()
{
    AutoMutex autoLock(mLock);
    if (mChannelState != VI_CHN_STATE_STARTED) 
    {
        aloge("start Motion Object Detection in error state %d", mChannelState);
        return INVALID_OPERATION;
    }

    if(mpAdasThread)
    {
        return mpAdasThread->startAdasDetect_v2();
    }
    else
    {
        return INVALID_OPERATION;
    }
}
status_t VIChannel::stopAdasDetect_v2()
{
    AutoMutex autoLock(mLock);
    if (mChannelState != VI_CHN_STATE_STARTED)
    {
        aloge("stop Motion Object Detection in error state %d", mChannelState);
        return INVALID_OPERATION;
    }
    if(mpAdasThread)
    {
        return mpAdasThread->stopAdasDetect_v2();
    }
    else
    {
        return INVALID_OPERATION;
    }
}

bool VIChannel::isPreviewEnabled()
{
    return mpPreviewWindow->isPreviewEnabled();
}

status_t VIChannel::startRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    mbPreviewEnable = true;
    if (VI_CHN_STATE_STARTED == mChannelState)
    {
        ret = mpPreviewWindow->startPreview();
    }
    return ret;
}

status_t VIChannel::stopRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    mbPreviewEnable = false;
    ret = mpPreviewWindow->stopPreview();
    return ret;
}

status_t VIChannel::pauseRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mbPreviewEnable)
    {
        ret = mpPreviewWindow->pausePreview();
    }
    return ret;
}

status_t VIChannel::resumeRender()
{
    status_t ret = NO_ERROR;
    AutoMutex autoLock(mLock);
    if(mbPreviewEnable)
    {
        ret = mpPreviewWindow->resumePreview();
    }
    return ret;
}

status_t VIChannel::changePreviewDisplay(int hlay)
{
    return mpPreviewWindow->changePreviewWindow(hlay);
}

void VIChannel::setPicCapMode(uint32_t mode)
{
    int ret; 
    ret = AW_MPI_ISP_SetScene(mIspDevId,mode);
    if(mode == 0 && SUCCESS == ret)
    {
        PicCapModeEn = 1; 
    }else{
        PicCapModeEn = 0;
    }
}

void VIChannel::setFrmDrpThrForPicCapMode(uint32_t frm_cnt)
{
    FrmDrpThrForPicCapMode = frm_cnt;
} 

status_t VIChannel::storeDisplayFrame(uint64_t framePts)
{
    return mpPreviewWindow->storeDisplayFrame(framePts);
}

status_t VIChannel::releaseFrame(uint32_t index)
{
    if (index >= (uint32_t)mParameters.getVideoBufferNumber()) 
    {
        aloge("Fatal error! invalid buffer index %d!", index);
        return BAD_VALUE;
    }
//    if(NULL == mpVideoFrmBuffer)
//    {
//        aloge("fatal error! mpVideoFrmBuffer == NULL!");
//    }
//    VIDEO_FRAME_BUFFER_S *pFrmbuf = mpVideoFrmBuffer + index;

    VIDEO_FRAME_BUFFER_S *pFrmbuf = NULL;
    AutoMutex bufLock(mFrameBuffersLock);
    for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mFrameBuffers.begin(); it!=mFrameBuffers.end(); ++it)
    {
        if(it->mFrameBuf.mId == index)
        {
            if(NULL == pFrmbuf)
            {
                pFrmbuf = &*it;
            }
            else
            {
                aloge("fatal error! VI frame id[0x%x] is repeated!", index);
            }
        }
    }
    if(NULL == pFrmbuf)
    {
        aloge("fatal error! not find frame index[0x%x]", index);
        return UNKNOWN_ERROR;
    }

    
    int ret;

    AutoMutex lock(mRefCntLock);

    if (pFrmbuf->mRefCnt > 0 && --pFrmbuf->mRefCnt == 0) 
    {
        if(MM_PIXEL_FORMAT_YUV_AW_AFBC == pFrmbuf->mFrameBuf.VFrame.mPixelFormat)
        {
            //alogd("vipp[%d] afbc clear %dByte", mChnId, pFrmbuf->mFrameBuf.VFrame.mStride[0]);
            //memset(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[0], 0x0, 200*1024);  //200*1024, pFrmbuf->mFrameBuf.VFrame.mStride[0]
        }
        else if(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pFrmbuf->mFrameBuf.VFrame.mPixelFormat)
        {
            //alogd("vipp[%d] afbc clear first 200KB", mChnId);
            //memset(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[0]+200*1024, 0, 200*1024);
        }
        ret = AW_MPI_VI_ReleaseFrame(mChnId, 0, &pFrmbuf->mFrameBuf);
        if (ret != SUCCESS) 
        {
            aloge("fatal error! AW_MPI_VI ReleaseFrame error!");
        }
        int nCount = 0;
        for (std::list<unsigned int>::iterator it = mFrameBufferIdList.begin(); it!=mFrameBufferIdList.end();)
        {
            if(*it == pFrmbuf->mFrameBuf.mId)
            {
                if(0 == nCount)
                {
                    it = mFrameBufferIdList.erase(it);
                }
                else
                {
                    ++it;
                }
                nCount++;
            }
            else
            {
                ++it;
            }
        }
        if(nCount != 1)
        {
            aloge("fatal error! vipp[%d] frame buffer id list is wrong! dstId[%d], find[%d]", mChnId, pFrmbuf->mFrameBuf.mId, nCount);
            alogd("current frame buffer id list elem number:%d", mFrameBufferIdList.size());
            for (std::list<unsigned int>::iterator it = mFrameBufferIdList.begin(); it!=mFrameBufferIdList.end(); ++it)
            {
                alogd("bufid[%d]", *it);
            }
        }
    }
    return NO_ERROR;
}
/* This function verifies cameraParameters. If error is found, return false.
 * @return true: normal, false: fail.
 */
static bool VerifyCameraParameters(CameraParameters &param)
{
    bool bValidFlag = true;
    //(1) verify B4G2dRegion and displayRegion, the latter must be included in the first.
    RECT_S stDisplayRect = param.getUserRegion();
    RECT_S stB4G2dRect = param.getB4G2dUserRegion();
    if((0 == stDisplayRect.Width && 0 == stDisplayRect.Height) || (0 == stB4G2dRect.Width && 0 == stB4G2dRect.Height))
    {
        bValidFlag = true;
    }
    else
    {
        if(stDisplayRect.X >= stB4G2dRect.X 
            && stDisplayRect.Y >= stB4G2dRect.Y
            && stDisplayRect.X + stDisplayRect.Width <= stB4G2dRect.X + stB4G2dRect.Width
            && stDisplayRect.Y + stDisplayRect.Height <= stB4G2dRect.Y + stB4G2dRect.Height)
        {
            bValidFlag = true;
        }
        else
        {
            alogw("fatal error! cameraParameter userRegion param is wrong![%d,%d,%dx%d]exceed[%d,%d,%dx%d]",
                stDisplayRect.X, stDisplayRect.Y, stDisplayRect.Width, stDisplayRect.Height,
                stB4G2dRect.X, stB4G2dRect.Y, stB4G2dRect.Width, stB4G2dRect.Height);
            bValidFlag = false;
        }
    }
    if(false == bValidFlag)
    {
        return false;
    }
    //(2) continue...
    
    return bValidFlag;
}

status_t VIChannel::setParameters(CameraParameters &param)
{
    AutoMutex lock(mLock);
    if(VI_CHN_STATE_CONSTRUCTED == mChannelState)
    {
        mParameters = param;
        return NO_ERROR;
    }
    if (mChannelState != VI_CHN_STATE_PREPARED && mChannelState != VI_CHN_STATE_STARTED)
    {
        alogw("call in wrong channel state[0x%x]", mChannelState);
        //mParameters = param;
        return INVALID_OPERATION;
    }
    //verify new params, if some params are wrong, return without change.
    if(false == VerifyCameraParameters(param))
    {
        aloge("fatal error! cameraParams is verified fail. ignore this param!");
        return BAD_VALUE;
    }
    //detect param change, and update.
    bool bAttrUpdate = false;
    VI_ATTR_S attr;
    ERRORTYPE ret;
    ret = AW_MPI_VI_GetVippAttr(mChnId, &attr);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! getVippAttr fail");
    }
    attr.use_current_win = mbForceRef?0:1;
    int oldCaptureMode, newCaptureMode;
    oldCaptureMode = mParameters.getCaptureMode();
    newCaptureMode = param.getCaptureMode();
    if(oldCaptureMode != newCaptureMode)
    {
        alogd("paramCaptureMode change[0x%x]->[0x%x]", oldCaptureMode, newCaptureMode);
        bAttrUpdate = true;
        attr.capturemode = newCaptureMode;
    }
    SIZE_S oldParamSize, newParamSize;
    mParameters.getVideoSize(oldParamSize);
    param.getVideoSize(newParamSize);
    if(oldParamSize.Width!=newParamSize.Width || oldParamSize.Height!=newParamSize.Height)
    {
        alogd("paramVideoSize change[%dx%d]->[%dx%d]", oldParamSize.Width, oldParamSize.Height, newParamSize.Width, newParamSize.Height);
        bAttrUpdate = true;
        attr.format.width = newParamSize.Width;
        attr.format.height = newParamSize.Height;
        mFrameWidth = attr.format.width;
        mFrameHeight = attr.format.height;
    }
    PIXEL_FORMAT_E oldFormat, newFormat;
    oldFormat = mParameters.getPreviewFormat();
    newFormat = param.getPreviewFormat();
    if(oldFormat!=newFormat)
    {
        alogd("paramPixelFormat change[0x%x]->[0x%x]", oldFormat, newFormat);
        bAttrUpdate = true;
        attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(newFormat);
    }
    enum v4l2_colorspace oldColorspace, newColorspace;
    oldColorspace = mParameters.getColorSpace();
    newColorspace = param.getColorSpace();
    if(oldColorspace != newColorspace)
    {
        alogd("paramColorSpace change[0x%x]->[0x%x]", oldColorspace, newColorspace);
        bAttrUpdate = true;
        attr.format.colorspace = newColorspace;
        mColorSpace = attr.format.colorspace;
    }
    int oldBufNum, newBufNum;
    oldBufNum = mParameters.getVideoBufferNumber();
    newBufNum = param.getVideoBufferNumber();
    if(oldBufNum != newBufNum)
    {
        alogd("paramBufNum change[%d]->[%d]", oldBufNum, newBufNum);
        bAttrUpdate = true;
        attr.nbufs = newBufNum;
    }
    int oldFrameRate, newFrameRate;
    oldFrameRate = mParameters.getPreviewFrameRate();
    newFrameRate = param.getPreviewFrameRate();
    if(oldFrameRate != newFrameRate)
    {
        alogd("paramFrameRate change[%d]->[%d]", oldFrameRate, newFrameRate);
        bAttrUpdate = true;
        attr.fps = newFrameRate;
    }
    bool oldOnlineEnable, newOnlineEnable;
    oldOnlineEnable = mParameters.getOnlineEnable();
    newOnlineEnable = param.getOnlineEnable();
    if(oldOnlineEnable != newOnlineEnable)
    {
        alogd("paramOnlineEnable change[%d]->[%d]", oldOnlineEnable, newOnlineEnable);
        bAttrUpdate = true;
        attr.mOnlineEnable = newOnlineEnable;
    }
    int oldOnlineShareBufNum, newOnlineShareBufNum;
    oldOnlineShareBufNum = mParameters.getOnlineShareBufNum();
    newOnlineShareBufNum = param.getOnlineShareBufNum();
    if(oldOnlineShareBufNum != newOnlineShareBufNum)
    {
        alogd("paramOnlineShareBufNum change[%d]->[%d]", oldOnlineShareBufNum, newOnlineShareBufNum);
        bAttrUpdate = true;
        attr.mOnlineShareBufNum = (enum dma_buffer_num)newOnlineShareBufNum;
    }
    if(bAttrUpdate)
    {
        ret = AW_MPI_VI_SetVippAttr(mChnId, &attr);
        if(ret!=SUCCESS)
        {
            aloge("fatal error! setVippAttr fail");
        }
    }
    //mirror
    int oldMirror = mParameters.GetMirror();
    int newMirror = param.GetMirror();
    if(oldMirror != newMirror)
    {
        alogd("change mirror[%d]->[%d]", oldMirror, newMirror);
        AW_MPI_VI_SetVippMirror(mChnId, newMirror);
    }
    //Flip
    int oldFlip = mParameters.GetFlip();
    int newFlip = param.GetFlip();
    if(oldFlip != newFlip)
    {
        alogd("change flip[%d]->[%d]", oldFlip, newFlip);
        AW_MPI_VI_SetVippFlip(mChnId, newFlip);
    }
    //shut time
    VI_SHUTTIME_CFG_S oldShutTime, newShutTime;
    mParameters.getShutTime(oldShutTime);
    param.getShutTime(newShutTime);
    if(oldShutTime.eShutterMode != newShutTime.eShutterMode || oldShutTime.iTime != newShutTime.iTime)
    {
        alogd("change ShutTime config, shutmode[%d]->[%d], shuttime[%d]->[%d]", oldShutTime.eShutterMode, newShutTime.eShutterMode,
                                                                        oldShutTime.iTime, newShutTime.iTime);
        AW_MPI_VI_SetVippShutterTime(mChnId, &newShutTime);
    }

    //detect preview rotation, displayFrameRate, UserRegion
    int oldPreviewRotation, newPreviewRotation, curPreviewRotation;
    oldPreviewRotation = mParameters.getPreviewRotation();
    newPreviewRotation = param.getPreviewRotation();
    curPreviewRotation = oldPreviewRotation;
    if(oldPreviewRotation != newPreviewRotation)
    {
        if(newPreviewRotation!=0 && newPreviewRotation!=90 && newPreviewRotation!=180 && newPreviewRotation!=270 && newPreviewRotation!=360)
        {
            aloge("fatal error! new rotation[%d] is invalid!", newPreviewRotation);
        }
        else
        {
            alogd("paramPreviewRotation change[%d]->[%d]", oldPreviewRotation, newPreviewRotation);
            curPreviewRotation = newPreviewRotation;
            mpPreviewWindow->setPreviewRotation(newPreviewRotation);
        }
    }
    int oldDisplayFrameRate, newDisplayFrameRate;
    oldDisplayFrameRate = mParameters.getDisplayFrameRate();
    newDisplayFrameRate = param.getDisplayFrameRate();
    if(oldDisplayFrameRate != newDisplayFrameRate)
    {
        alogd("paramDisplayFrameRate change[%d]->[%d]", oldDisplayFrameRate, newDisplayFrameRate);
        mpPreviewWindow->setDisplayFrameRate(newDisplayFrameRate);
    }
    RECT_S oldUserRegion,newUserRegion;
    oldUserRegion = mParameters.getUserRegion();
    newUserRegion = param.getUserRegion();
    if(oldUserRegion.X != newUserRegion.X || oldUserRegion.Y != newUserRegion.Y ||
        oldUserRegion.Width != newUserRegion.Width || oldUserRegion.Height != newUserRegion.Height)
    {
        alogd("UserRegion change[X:%d,Y:%d,W:%d,H:%d]->[X:%d,Y:%d,W:%d,H:%d]",oldUserRegion.X,oldUserRegion.Y, 
        oldUserRegion.Width,oldUserRegion.Height,newUserRegion.X,newUserRegion.Y,newUserRegion.Width,
        newUserRegion.Height);
        mpPreviewWindow->setPreviewRegion(param.mUserRegion);
    }
    RECT_S oldB4G2dUserRegion,newB4G2dUserRegion;
    oldB4G2dUserRegion = mParameters.getB4G2dUserRegion();
    newB4G2dUserRegion = param.getB4G2dUserRegion();
    if(oldB4G2dUserRegion.X != newB4G2dUserRegion.X || oldB4G2dUserRegion.Y != newB4G2dUserRegion.Y ||
        oldB4G2dUserRegion.Width != newB4G2dUserRegion.Width || oldB4G2dUserRegion.Height != newB4G2dUserRegion.Height)
    {
        alogd("B4G2dUserRegion change[X:%d,Y:%d,W:%d,H:%d]->[X:%d,Y:%d,W:%d,H:%d]",oldB4G2dUserRegion.X,oldB4G2dUserRegion.Y, 
        oldB4G2dUserRegion.Width,oldB4G2dUserRegion.Height,newB4G2dUserRegion.X,newB4G2dUserRegion.Y,newB4G2dUserRegion.Width,
        newB4G2dUserRegion.Height);
        mpPreviewWindow->setB4G2dPreviewRegion(param.mB4G2dUserRegion);
    }
    //mpPreviewWindow->getPreviewRegion(pParamRegion);

    //detect digital zoom params
    int oldZoom, newZoom;
    oldZoom = mParameters.getZoom();
    newZoom = param.getZoom();
    if(oldZoom != newZoom)
    {
        if(newZoom >=0 && newZoom <= mMaxZoom)
        {
            alogd("paramZoom change[%d]->[%d]", oldZoom, newZoom);
            mNewZoom = newZoom;
        }
        else
        {
            aloge("fatal error! zoom value[%d] is invalid, keep last!", newZoom);
        }
    }

    //set parameters
    mParameters = param;
    mParameters.setPreviewRotation(curPreviewRotation);
    mParameters.setZoom(mNewZoom);
    return NO_ERROR;
}

status_t VIChannel::getParameters(CameraParameters &param) const
{
    param = mParameters;
    return NO_ERROR;
}
/*
bool VIChannel::compareOSDRectInfo(const OSDRectInfo& first, const OSDRectInfo& second)
{
    if(first.mRect.Y < second.mRect.Y)
    {
        return true;
    }
    if(first.mRect.Y > second.mRect.Y)
    {
        return false;
    }
    if(first.mRect.X < second.mRect.X)
    {
        return true;
    }
    if(first.mRect.X > second.mRect.X)
    {
        return false;
    }
    return false;
}
*/
/**
 * VIPP: video input post process.
 * VIPP process osd rects in position order of lines. From top to bottom, left to right.
 * So must sort osd rects according above orders.
 */
 /*
status_t VIChannel::setOSDRects(std::list<OSDRectInfo> &rects)
{
    mOSDRects = rects;
    mOSDRects.sort(compareOSDRectInfo);
    return NO_ERROR;
}

status_t VIChannel::getOSDRects(std::list<OSDRectInfo> **ppRects)
{
    if(ppRects)
    {
        *ppRects = &mOSDRects;
    }
    return NO_ERROR;
}

status_t VIChannel::OSDOnOff(bool bOnOff)
{
    if(bOnOff)
    {
      //  aloge("OSDOnOff unknown osd type[0x%x]", elem.mType);
        VI_OsdMaskRegion stOverlayRegion;
        memset(&stOverlayRegion, 0, sizeof(VI_OsdMaskRegion));
        VI_OsdMaskRegion stCoverRegion;
        memset(&stCoverRegion, 0, sizeof(VI_OsdMaskRegion));
        int bufSize = 0;
        for(OSDRectInfo& elem : mOSDRects)
        {
            if(OSDType_Overlay == elem.mType)
            {
                stOverlayRegion.chromakey = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(elem.mFormat);
                stOverlayRegion.global_alpha = 16;
                stOverlayRegion.bitmap[stOverlayRegion.clipcount] = elem.GetBuffer();
                stOverlayRegion.region[stOverlayRegion.clipcount].left   = elem.mRect.X;
                stOverlayRegion.region[stOverlayRegion.clipcount].top    = elem.mRect.Y;
                stOverlayRegion.region[stOverlayRegion.clipcount].width  = elem.mRect.Width;
                stOverlayRegion.region[stOverlayRegion.clipcount].height = elem.mRect.Height;
                stOverlayRegion.clipcount++;
                bufSize += elem.GetBufferSize();
            }
            else if(OSDType_Cover == elem.mType)
            {
                stCoverRegion.chromakey = elem.mColor;
                stCoverRegion.global_alpha = 16;
                stCoverRegion.bitmap[stCoverRegion.clipcount] = NULL;
                stCoverRegion.region[stCoverRegion.clipcount].left   = elem.mRect.X;
                stCoverRegion.region[stCoverRegion.clipcount].top    = elem.mRect.Y;
                stCoverRegion.region[stCoverRegion.clipcount].width  = elem.mRect.Width;
                stCoverRegion.region[stCoverRegion.clipcount].height = elem.mRect.Height;
                stCoverRegion.clipcount++;
            }
            else
            {
                aloge("fatal error! unknown osd type[0x%x]", elem.mType);
            }
        }
        if(stOverlayRegion.clipcount > 0)
        {
            AW_MPI_VI_SetOsdMaskRegion(mChnId, &stOverlayRegion);
            AW_MPI_VI_UpdateOsdMaskRegion(mChnId, 1);
        }
        else
        {
            VI_OsdMaskRegion stOverlayRegion;
            memset(&stOverlayRegion, 0, sizeof(VI_OsdMaskRegion));
            char bitmap[100];
            bitmap[0] = 'c';
            stOverlayRegion.clipcount = 0;
            stOverlayRegion.bitmap[0] = &bitmap[0];
            AW_MPI_VI_SetOsdMaskRegion(mChnId, &stOverlayRegion);
            AW_MPI_VI_UpdateOsdMaskRegion(mChnId, 1);
        }
        if(stCoverRegion.clipcount > 0)
        {
            AW_MPI_VI_SetOsdMaskRegion(mChnId, &stCoverRegion);
            AW_MPI_VI_UpdateOsdMaskRegion(mChnId, 1);
        }
        else
        {
            VI_OsdMaskRegion stCoverRegion;
            memset(&stCoverRegion, 0, sizeof(VI_OsdMaskRegion));
            stCoverRegion.clipcount = 0;
            stCoverRegion.bitmap[0] = NULL;
            AW_MPI_VI_SetOsdMaskRegion(mChnId, &stCoverRegion);
            AW_MPI_VI_UpdateOsdMaskRegion(mChnId, 1);
        }
    }
    else
    {
        VI_OsdMaskRegion stOverlayRegion;
        memset(&stOverlayRegion, 0, sizeof(VI_OsdMaskRegion));
        VI_OsdMaskRegion stCoverRegion;
        memset(&stCoverRegion, 0, sizeof(VI_OsdMaskRegion));

        char bitmap[100];
        bitmap[0] = 'c';
        stOverlayRegion.clipcount = 0;
        stOverlayRegion.bitmap[0] = &bitmap[0];
        stOverlayRegion.chromakey = V4L2_PIX_FMT_RGB32;
        AW_MPI_VI_SetOsdMaskRegion(mChnId, &stOverlayRegion);
        AW_MPI_VI_UpdateOsdMaskRegion(mChnId, 1);

        stCoverRegion.clipcount = 0;
        stCoverRegion.bitmap[0] = NULL;
        AW_MPI_VI_SetOsdMaskRegion(mChnId, &stCoverRegion);
        AW_MPI_VI_UpdateOsdMaskRegion(mChnId, 1);

    }
    return NO_ERROR;
}
*/
bool VIChannel::captureThread()
{
    bool bRunningFlag = true;
    bool bTakePictureStart = false;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    VIDEO_FRAME_INFO_S buffer;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    if (mParameters.getOnlineEnable())
    {
        alogw("vipp %d is online, do not support capture, quit!", mChnId);
        bRunningFlag = false;
        goto _exit0;
    }
PROCESS_MESSAGE:
    getMsgRet = mpCapThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoCaptureThread::MsgTypeCapture_SetState == msg.mMsgType)
        {
            AutoMutex autoLock(mpCapThread->mStateLock);
            if(msg.mPara0 == mpCapThread->mCapThreadState)
            {
                aloge("fatal error! same state[0x%x]", mpCapThread->mCapThreadState);
            }
            else if(DoCaptureThread::CAPTURE_STATE_PAUSED == msg.mPara0)
            {
                alogv("VIchn captureThread state[0x%x]->paused", mpCapThread->mCapThreadState);
                mpCapThread->mCapThreadState = DoCaptureThread::CAPTURE_STATE_PAUSED;
                mpCapThread->mPauseCompleteCond.signal();
            }
            else if(DoCaptureThread::CAPTURE_STATE_STARTED == msg.mPara0)
            {
                alogv("VIchn captureThread state[0x%x]->started", mpCapThread->mCapThreadState);
                mpCapThread->mCapThreadState = DoCaptureThread::CAPTURE_STATE_STARTED;
                mpCapThread->mStartCompleteCond.signal();
            }
            else
            {
                aloge("fatal error! check code!");
            }
        }
        else if(DoCaptureThread::MsgTypeCapture_TakePicture == msg.mMsgType)
        {
            if(!bTakePictureStart)
            {
                bTakePictureStart = true;
            }
            else
            {
                alogd("Be careful! take picture is doing already!");
            }
            alogd("take picture mode is [0x%x]", mTakePictureMode);
            //set picture number to callbackNotifier
            int nPicNum = 0;
            switch(mTakePictureMode)
            {
                case TAKE_PICTURE_MODE_FAST:
                    nPicNum = 1;
                    break;
                case TAKE_PICTURE_MODE_CONTINUOUS:
                    nPicNum = mParameters.getContinuousPictureNumber();
                    break;
                default:
                    nPicNum = 1;
                    break;
            }
            mpCallbackNotifier->setPictureNum(nPicNum);
        }
        else if(DoCaptureThread::MsgTypeCapture_CancelContinuousPicture == msg.mMsgType)
        {
            if(!bTakePictureStart)
            {
                if(TAKE_PICTURE_MODE_CONTINUOUS == mTakePictureMode)
                {
                    mpPicThread->notifyPictureEnd();
                    mContinuousPictureStartTime = 0;
                    mContinuousPictureLast = 0;
                    mContinuousPictureInterval = 0;
                    mContinuousPictureCnt = 0;
                    mContinuousPictureMax = 0;
                    bTakePictureStart = false; 
                }
                else
                {
                    aloge("fatal error! take picture mode[0x%x] is not continuous!", mTakePictureMode);
                }
            }
            else
            {
                aloge("fatal error! not start take picture, mode[0x%x]!", mTakePictureMode);
            }
        }
        else if(DoCaptureThread::MsgTypeCapture_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }
    
    if(mpCapThread->mCapThreadState == DoCaptureThread::CAPTURE_STATE_PAUSED)
    {
        mpCapThread->mMsgQueue.waitMessage();
        goto PROCESS_MESSAGE;
    }

    ret = AW_MPI_VI_GetFrame(mChnId, 0, &buffer, 500);
    if (ret != SUCCESS)
    {
        mFrameBuffersLock.lock();
        alogw("vipp[%d] channel contain [%d] frame buffers", mChnId, mFrameBufferIdList.size());
        mFrameBuffersLock.unlock();
        int preview_num = OSAL_GetElemNum(mpPreviewQueue);
        int picture_num = OSAL_GetElemNum(mpPictureQueue);
        alogw("vipp[%d]: preview_num: %d, picture_num: %d, timeout: 500ms", mChnId, preview_num, picture_num);
        mCamDevTimeoutCnt++;
        if (mCamDevTimeoutCnt%10 == 0)
        {
            aloge("timeout for %d times when VI_GetFrame!", mCamDevTimeoutCnt);
            mpCallbackNotifier->NotifyCameraDeviceTimeout();
        }
        //usleep(10*1000);
        //return true;
        goto PROCESS_MESSAGE;
    }

    if(PicCapModeEn && 3<=FrmDrpThrForPicCapMode && FrmDrpCntForPicCapMode <= FrmDrpThrForPicCapMode )
    {
        ret = AW_MPI_VI_ReleaseFrame(mChnId, 0, &buffer);
        if (ret != SUCCESS) 
        {
            aloge("fatal error! AW_MPI_VI ReleaseFrame error!");
        }
        FrmDrpCntForPicCapMode++;
        goto PROCESS_MESSAGE;
    }

    FrmDrpCntForPicCapMode = 0;
    mCamDevTimeoutCnt = 0;
    //alogd("print vipp[%d]pts[%lld]ms", mChnId, buffer.VFrame.mpts/1000);
#if (DEBUG_STORE_VI_FRAME!=0)
    if(mDbgFrameNum%30 == 0)
    {
        AW_MPI_VI_Debug_StoreFrame(mChnId, 0, "/home/sample_EncodeResolutionChange_Files");
    }
    mDbgFrameNum++;
#endif
#if (DEBUG_SAVE_FRAME_TO_TMP!=0)
    DebugLoopSaveFrame(&buffer);
#endif
    mFrameCounter++;

    //pFrmbuf = mpVideoFrmBuffer + buffer.mId;
    pFrmbuf = NULL;
    mFrameBuffersLock.lock();
    for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mFrameBuffers.begin(); it!=mFrameBuffers.end(); ++it)
    {
        if(it->mFrameBuf.mId == buffer.mId)
        {
            if(NULL == pFrmbuf)
            {
                pFrmbuf = &*it;
            }
            else
            {
                aloge("fatal error! VI frame id[0x%x] is repeated!", buffer.mId);
            }
        }
    }
    if(NULL == pFrmbuf)
    {
        alogv("ISP[%d]VIPP[%d] frame buffer array did not contain this bufferId[0x%x], add it.", mIspDevId, mChnId, buffer.mId);
        mFrameBuffers.emplace_back();
        memset(&mFrameBuffers.back(), 0, sizeof(VIDEO_FRAME_BUFFER_S));
        pFrmbuf = &mFrameBuffers.back();
    }
    pFrmbuf->mFrameBuf = buffer;
    if (mLastZoom != mNewZoom)
    {
        calculateCrop(&mRectCrop, mNewZoom, mFrameWidthOut, mFrameHeightOut);
        mLastZoom = mNewZoom;
        alogd("zoom[%d], CROP: [%d, %d, %d, %d]", mNewZoom, mRectCrop.X, mRectCrop.Y, mRectCrop.Width, mRectCrop.Height);
    }
    pFrmbuf->mFrameBuf.VFrame.mOffsetTop = mRectCrop.Y;
    pFrmbuf->mFrameBuf.VFrame.mOffsetBottom = mRectCrop.Y + mRectCrop.Height;
    pFrmbuf->mFrameBuf.VFrame.mOffsetLeft = mRectCrop.X;
    pFrmbuf->mFrameBuf.VFrame.mOffsetRight = mRectCrop.X + mRectCrop.Width;
    pFrmbuf->mColorSpace = mColorSpace;
    //pFrmbuf->mIsThumbAvailable = 0;
    //pFrmbuf->mThumbUsedForPhoto = 0;
    pFrmbuf->mRefCnt = 1;
    mFrameBufferIdList.push_back(pFrmbuf->mFrameBuf.mId);
    mFrameBuffersLock.unlock();

    if(false == bTakePictureStart)
    {
        {
            AutoMutex lock(mRefCntLock);
            pFrmbuf->mRefCnt++;
        }
        OSAL_Queue(mpPreviewQueue, pFrmbuf);
        mpPrevThread->notifyNewFrameCome();
    }
    else
    {
        if (mTakePictureMode == TAKE_PICTURE_MODE_NORMAL) 
        {
           aloge("fatal error! don't support normal mode take picture temporary!");
           mpPicThread->notifyPictureEnd();
           bTakePictureStart = false;
        } 
        else 
        {
            mRefCntLock.lock();
            pFrmbuf->mRefCnt++;
            mRefCntLock.unlock();
            OSAL_Queue(mpPreviewQueue, pFrmbuf);
            mpPrevThread->notifyNewFrameCome();

            if (mTakePictureMode == TAKE_PICTURE_MODE_FAST)
            {
                mRefCntLock.lock();
                pFrmbuf->mRefCnt++;
                mRefCntLock.unlock();
                mIsPicCopy = false;
                //mTakePictureMode = TAKE_PICTURE_MODE_NULL;
                OSAL_Queue(mpPictureQueue, pFrmbuf);
                mpPicThread->notifyNewFrameCome();
                mpPicThread->notifyPictureEnd();
                bTakePictureStart = false;
            }
            else if(mTakePictureMode == TAKE_PICTURE_MODE_CONTINUOUS)
            {
                bool bPermit = false;
                if (0 == mContinuousPictureStartTime)    //let's begin!
                {
                    mContinuousPictureStartTime = CDX_GetSysTimeUsMonotonic()/1000;
                    mContinuousPictureLast = mContinuousPictureStartTime;
                    mContinuousPictureInterval = mParameters.getContinuousPictureIntervalMs();
                    mContinuousPictureCnt = 0;
                    mContinuousPictureMax = mParameters.getContinuousPictureNumber();
                    bPermit = true;
                    alogd("begin continous picture, will take [%d]pics, interval[%llu]ms, curTm[%lld]ms", mContinuousPictureMax, mContinuousPictureInterval, mContinuousPictureLast);
                }
                else
                {
                    if(mContinuousPictureInterval <= 0)
                    {
                        bPermit = true;
                    }
                    else
                    {
                        uint64_t nCurTime = CDX_GetSysTimeUsMonotonic()/1000;
                        if(nCurTime >= mContinuousPictureLast + mContinuousPictureInterval)
                        {
                            //alogd("capture picture, curTm[%lld]ms, [%lld][%lld]", nCurTime, mContinuousPictureLast, mContinuousPictureInterval);
                            bPermit = true;
                        }
                    }
                }

                if(bPermit)
                {
                    mRefCntLock.lock();
                    pFrmbuf->mRefCnt++;
                    mRefCntLock.unlock();
                    mIsPicCopy = false;
                    OSAL_Queue(mpPictureQueue, pFrmbuf);
                    mpPicThread->notifyNewFrameCome();
                    mContinuousPictureCnt++;
                    if(mContinuousPictureCnt >= mContinuousPictureMax)
                    {
                        mpPicThread->notifyPictureEnd();
                        mContinuousPictureStartTime = 0;
                        mContinuousPictureLast = 0;
                        mContinuousPictureInterval = 0;
                        mContinuousPictureCnt = 0;
                        mContinuousPictureMax = 0;
                        bTakePictureStart = false;
                    }
                    else
                    {
                        mContinuousPictureLast = mContinuousPictureStartTime+mContinuousPictureCnt*mContinuousPictureInterval;
                    }
                }
            }
            else
            {
                aloge("fatal error! any other take picture mode[0x%x]?", mTakePictureMode);
                bTakePictureStart = false;
            }
        }
    }

    if(mpMODThread && mpMODThread->IsMODDetectEnable())
    {
        mRefCntLock.lock();
        pFrmbuf->mRefCnt++;
        mRefCntLock.unlock();
        if(NO_ERROR != mpMODThread->sendFrame(pFrmbuf))
        {
            AutoMutex lock(mRefCntLock);
            pFrmbuf->mRefCnt--;
        }
    }
    if(mpAdasThread && mpAdasThread->IsAdasDetectEnable())
    {
        mRefCntLock.lock();
        pFrmbuf->mRefCnt++;
        mRefCntLock.unlock();
        if(NO_ERROR != mpAdasThread->sendFrame(pFrmbuf))
        {
            AutoMutex lock(mRefCntLock);
            pFrmbuf->mRefCnt--;
        }
    }
    

    releaseFrame(pFrmbuf->mFrameBuf.mId);
    //return true;
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

bool VIChannel::previewThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    if (mParameters.getOnlineEnable())
    {
        alogw("vipp %d is online, do not support preview, quit!", mChnId);
        bRunningFlag = false;
        goto _exit0;
    }
    PROCESS_MESSAGE:
    getMsgRet = mpPrevThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoPreviewThread::MsgTypePreview_InputFrameAvailable == msg.mMsgType)
        {
        }
        else if(DoPreviewThread::MsgTypePreview_releaseAllFrames == msg.mMsgType)
        {
            AutoMutex lock(mpPrevThread->mWaitLock);
            for(;;)
            {
                pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPreviewQueue);
                if(pFrmbuf)
                {
                    releaseFrame(pFrmbuf->mFrameBuf.mId);
                }
                else
                {
                    break;
                }
            }
            if(mpPrevThread->mbWaitReleaseAllFrames)
            {
                mpPrevThread->mbWaitReleaseAllFrames = false;
            }
            else
            {
                aloge("fatal error! check code!");
            }
            mpPrevThread->mCondReleaseAllFramesFinished.signal();
        }
        else if(DoPreviewThread::MsgTypePreview_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }
    
    pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPreviewQueue);
    if (pFrmbuf == NULL) 
    {
        {
            AutoMutex lock(mpPrevThread->mWaitLock);
            if(OSAL_GetElemNum(mpPreviewQueue) > 0)
            {
                alogd("Low probability! preview new frame come before check again.");
                goto PROCESS_MESSAGE;
            }
            else
            {
                mpPrevThread->mbWaitPreviewFrame = true;
            }
        }
        int nWaitTime = 0;    //unit:ms, 10*1000
        int msgNum = mpPrevThread->mMsgQueue.waitMessage(nWaitTime);
        if(msgNum <= 0)
        {
            aloge("fatal error! preview thread wait message timeout[%d]ms! msgNum[%d], bWait[%d]", nWaitTime, msgNum, mpPrevThread->mbWaitPreviewFrame);
        }
        goto PROCESS_MESSAGE;
    }

    mpCallbackNotifier->onNextFrameAvailable(pFrmbuf, mChnId);
    mpPreviewWindow->onNextFrameAvailable(pFrmbuf);

    releaseFrame(pFrmbuf->mFrameBuf.mId);
    //return true;
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

bool VIChannel::pictureThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    bool bDrainPictureQueue = false;
    VIDEO_FRAME_BUFFER_S *pFrmbuf;
    if (mParameters.getOnlineEnable())
    {
        alogw("vipp %d is online, do not support picture, quit!", mChnId);
        bRunningFlag = false;
        goto _exit0;
    }
    PROCESS_MESSAGE:
    getMsgRet = mpPicThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoPictureThread::MsgTypePicture_InputFrameAvailable == msg.mMsgType)
        {
        }
        else if(DoPictureThread::MsgTypePicture_SendPictureEnd == msg.mMsgType)
        {
            bDrainPictureQueue = true;
        }
        else if(DoPictureThread::MsgTypePicture_releaseAllFrames == msg.mMsgType)
        {
            AutoMutex lock(mpPicThread->mWaitLock);
            for(;;)
            {
                pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPictureQueue);
                if(pFrmbuf)
                {
                    releaseFrame(pFrmbuf->mFrameBuf.mId);
                }
                else
                {
                    break;
                }
            }
            if(mpPicThread->mbWaitReleaseAllFrames)
            {
                mpPicThread->mbWaitReleaseAllFrames = false;
            }
            else
            {
                aloge("fatal error! check code!");
            }
            mpPicThread->mCondReleaseAllFramesFinished.signal();
        }
        else if(DoPictureThread::MsgTypePicture_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
        goto PROCESS_MESSAGE;
    }

    while(1)
    {
        pFrmbuf = (VIDEO_FRAME_BUFFER_S*)OSAL_Dequeue(mpPictureQueue);
        if (pFrmbuf == NULL) 
        {
            if(bDrainPictureQueue)
            {
                mpCallbackNotifier->disableMessage(mTakePicMsgType);
                bDrainPictureQueue = false;
                if(false == mbKeepPictureEncoder)
                {
                    AutoMutex lock(mTakePictureLock);
                    mpCallbackNotifier->takePictureEnd();
                }
                mLock.lock();
                if(!mbTakePictureStart)
                {
                    aloge("fatal error! why takePictureStart is false when we finish take picture?");
                }
                mbTakePictureStart = false;
                mLock.unlock();
            }
            {
                AutoMutex lock(mpPicThread->mWaitLock);
                if(OSAL_GetElemNum(mpPictureQueue) > 0)
                {
                    alogd("Low probability! picture new frame come before check again.");
                    goto PROCESS_MESSAGE;
                }
                else
                {
                    mpPicThread->mbWaitPictureFrame = true;
                }
            }
            mpPicThread->mMsgQueue.waitMessage();
            goto PROCESS_MESSAGE;
        }
#if 0
        static int index = 0;
        char str[50];
        sprintf(str, "./savepicture/4/%d.n21", index++);
        FILE *fp = fopen(str, "wb");
        if(fp)
        {
            VideoFrameBufferSizeInfo FrameSizeInfo;
            getVideoFrameBufferSizeInfo(&pFrmbuf->mFrameBuf, &FrameSizeInfo);
            int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
            for(int i=0; i<3; i++)
            {
                if(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[i] != NULL)
                {
                    fwrite(pFrmbuf->mFrameBuf.VFrame.mpVirAddr[i], 1, yuvSize[i], fp);
                }
            }
            alogd("save a frame!");
            fclose(fp);
        }
#endif
        bool ret = false;
        mTakePictureLock.lock();
        ret = mpCallbackNotifier->takePicture(pFrmbuf, false, true);    // to take picture for post view
        mTakePictureLock.unlock();
        if((mTakePicMsgType&CAMERA_MSG_POSTVIEW_FRAME) && (false == ret))
        {
            aloge("post_view_take_pic_fail");
            mpCallbackNotifier->NotifyCameraTakePicFail();
        }

        struct isp_exif_attribute isp_exif_s; 
        memset(&isp_exif_s,0,sizeof(isp_exif_s));
        int iso_speed_idx = 0;
        int iso_speed_value[] = {100,200,400,800,1600,3200,6400};
        AW_MPI_ISP_AE_GetISOSensitive(mIspDevId,&iso_speed_idx);

        if(7 < iso_speed_idx)
        {
            iso_speed_idx = 7;
        }
        if(0 > iso_speed_idx)
        {
            iso_speed_idx = 0;
        }
        if(0 < iso_speed_idx)
        {
            iso_speed_idx -= 1;
        }

        isp_exif_s.iso_speed = iso_speed_value[iso_speed_idx];

        int exposure_bias_idx = 0;
        int exposure_bias_value[] = {-3,-2,-1,0,1,2,3};
        AW_MPI_ISP_AE_GetExposureBias(mIspDevId,&exposure_bias_idx);
        if(0 >= exposure_bias_idx )
        {
            exposure_bias_idx = 0;
        }
        else if(7 < exposure_bias_idx)
        {
            exposure_bias_idx = 6;
        }
        else
        {
            exposure_bias_idx -= 1;
        } 
        
        isp_exif_s.exposure_bias = exposure_bias_value[exposure_bias_idx];
        // to take picture for normal use,if failed in taking picture for post view,to take pic for normal use
        // may succeed,so not jump thie step.
        mTakePictureLock.lock();
        ret = mpCallbackNotifier->takePicture(pFrmbuf, false, false,&isp_exif_s);
        mTakePictureLock.unlock();
        if(false == ret)
        {
            aloge("normal_take_pic_fail");
            mpCallbackNotifier->NotifyCameraTakePicFail();
        }

        if (mIsPicCopy) 
        {
            aloge("fatal error! we do not copy!");
            //CdcMemPfree(mpMemOps, pFrmbuf->mFrameBuf.pVirAddr[0]);
        } 
        else
        {
            releaseFrame(pFrmbuf->mFrameBuf.mId);
        }
        if(!bDrainPictureQueue)
        {
            break;
        }
    }
    //return true;
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

bool VIChannel::commandThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    if (mParameters.getOnlineEnable())
    {
        alogw("vipp %d is online, do not support command, quit!", mChnId);
        bRunningFlag = false;
        goto _exit0;
    }
PROCESS_MESSAGE:
    getMsgRet = mpCommandThread->mMsgQueue.dequeueMessage(&msg);
    if(getMsgRet == NO_ERROR)
    {
        if(DoCommandThread::MsgTypeCommand_TakePicture == msg.mMsgType)
        {
            doTakePicture(msg.mPara0);
        }
        else if(DoCommandThread::MsgTypeCommand_CancelContinuousPicture == msg.mMsgType)
        {
            doCancelContinuousPicture();
        }
        else if(DoCommandThread::MsgTypeCommand_Exit == msg.mMsgType)
        {
            bRunningFlag = false;
            goto _exit0;
        }
        else
        {
            aloge("unknown msg[0x%x]!", msg.mMsgType);
        }
    }
    else
    {
        mpCommandThread->mMsgQueue.waitMessage();
    }
    //return true;
    goto PROCESS_MESSAGE;
_exit0:
    return bRunningFlag;
}

void VIChannel::increaseBufRef(VIDEO_FRAME_BUFFER_S *pBuf)
{
    VIDEO_FRAME_BUFFER_S *pFrame = (VIDEO_FRAME_BUFFER_S*)pBuf;
    AutoMutex lock(mRefCntLock);
    ++pFrame->mRefCnt;
}

void VIChannel::decreaseBufRef(unsigned int nBufId)
{
    releaseFrame(nBufId);
}

void VIChannel::NotifyRenderStart()
{
    mpCallbackNotifier->NotifyRenderStart();
}
status_t VIChannel::doTakePicture(unsigned int msgType)
{
    AutoMutex lock(mLock);
    int jpeg_quality = mParameters.getJpegQuality();
    if (jpeg_quality <= 0) {
        jpeg_quality = 90;
    }
    mpCallbackNotifier->setJpegQuality(jpeg_quality);

    int jpeg_rotate = mParameters.getJpegRotation();
    if (jpeg_rotate <= 0) {
        jpeg_rotate = 0;
    }
    mpCallbackNotifier->setJpegRotate(jpeg_rotate);

    SIZE_S size;
    mParameters.getPictureSize(size);
    mpCallbackNotifier->setPictureSize(size.Width, size.Height);
    mParameters.getJpegThumbnailSize(size);
    mpCallbackNotifier->setJpegThumbnailSize(size.Width, size.Height);
    int quality = mParameters.getJpegThumbnailQuality();
    mpCallbackNotifier->setJpegThumbnailQuality(quality);

    char *pGpsMethod = mParameters.getGpsProcessingMethod();
    if (pGpsMethod != NULL) {
        mpCallbackNotifier->setGPSLatitude(mParameters.getGpsLatitude());
        mpCallbackNotifier->setGPSLongitude(mParameters.getGpsLongitude());
        mpCallbackNotifier->setGPSAltitude(mParameters.getGpsAltitude());
        mpCallbackNotifier->setGPSTimestamp(mParameters.getGpsTimestamp());
        mpCallbackNotifier->setGPSMethod(pGpsMethod);
    }
    mTakePicMsgType = msgType;
    mpCallbackNotifier->enableMessage(msgType);

//    mCapThreadLock.lock();
//    mTakePictureMode = TAKE_PICTURE_MODE_FAST;
//    mCapThreadLock.unlock();
    mTakePictureMode = mParameters.getPictureMode();
    mpCapThread->SendCommand_TakePicture();

    return NO_ERROR;
}

status_t VIChannel::doCancelContinuousPicture()
{
    AutoMutex lock(mLock);
    mpCapThread->SendCommand_CancelContinuousPicture();
    return NO_ERROR;
}

status_t VIChannel::takePicture(unsigned int msgType, PictureRegionCallback *pPicReg)
{
    AutoMutex lock(mLock);
    if(mbTakePictureStart)
    {
        aloge("fatal error! During taking picture, we don't accept new takePicture command!");
        return UNKNOWN_ERROR;
    }
    else
    {
        mbTakePictureStart = true;
        mpCallbackNotifier->setPictureRegionCallback(pPicReg);
        mpCommandThread->SendCommand_TakePicture(msgType);
        return NO_ERROR;
    }
}

status_t VIChannel::notifyPictureRelease()
{
    return mpCallbackNotifier->notifyPictureRelease();
}

status_t VIChannel::cancelContinuousPicture()
{
    AutoMutex lock(mLock);
    if(!mbTakePictureStart)
    {
        aloge("fatal error! not start take picture!");
        return NO_ERROR;
    }
    mpCommandThread->SendCommand_CancelContinuousPicture();
    return NO_ERROR;
}

status_t VIChannel::KeepPictureEncoder(bool bKeep)
{
    AutoMutex lock(mLock);
    mbKeepPictureEncoder = bKeep;
    return NO_ERROR;
}

status_t VIChannel::releasePictureEncoder()
{
    AutoMutex lock(mTakePictureLock);
    alogw("vipp[%d] force release picture encoder! user must guarantee not taking picture now.", mChnId);
    mpCallbackNotifier->takePictureEnd();
    return NO_ERROR;
}

status_t VIChannel::DebugLoopSaveFrame(VIDEO_FRAME_INFO_S *pFrame)
{
    char DbgStoreFilePath[256];
    if(MM_PIXEL_FORMAT_YUV_AW_AFBC == pFrame->VFrame.mPixelFormat)
    {
        //alogd("store vipp[%d] afbc buffer id[%d]", mChnId, pFrame->mId);
        snprintf(DbgStoreFilePath, 256, "/tmp/pic[%d][id%d][%lldus].afbc", mDbgFrameNum++, pFrame->mId, pFrame->VFrame.mpts);
    }
    else if(MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X == pFrame->VFrame.mPixelFormat)
    {
        //alogd("store vipp[%d] LBC_2_0X buffer id[%d]", mChnId, pFrame->mId);
        snprintf(DbgStoreFilePath, 256, "/tmp/pic[%d][id%d][%lldus].lbc20x", mDbgFrameNum++, pFrame->mId, pFrame->VFrame.mpts);
    }
    else if(MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X == pFrame->VFrame.mPixelFormat)
    {
        //alogd("store vipp[%d] LBC_2_5X buffer id[%d]", mChnId, pFrame->mId);
        snprintf(DbgStoreFilePath, 256, "/tmp/pic[%d][id%d][%lldus].lbc25x", mDbgFrameNum++, pFrame->mId, pFrame->VFrame.mpts);
    }
    else if(MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X == pFrame->VFrame.mPixelFormat)
    {
        //alogd("store vipp[%d] LBC_1_0X buffer id[%d]", mChnId, pFrame->mId);
        snprintf(DbgStoreFilePath, 256, "/tmp/pic[%d][id%d][%lldus].lbc10x", mDbgFrameNum++, pFrame->mId, pFrame->VFrame.mpts);
    }
    else if(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pFrame->VFrame.mPixelFormat)
    {
        //alogd("store vipp[%d] nv21 buffer id[%d]", mChnId, pFrame->mId);
        snprintf(DbgStoreFilePath, 256, "/tmp/pic[%d][id%d][%lldus].nv21", mDbgFrameNum++, pFrame->mId, pFrame->VFrame.mpts);
    }
    else if(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == pFrame->VFrame.mPixelFormat)
    {
        //alogd("store vipp[%d] nv12 buffer id[%d]", mChnId, pFrame->mId);
        snprintf(DbgStoreFilePath, 256, "/tmp/pic[%d][id%d][%lldus].nv12", mDbgFrameNum++, pFrame->mId, pFrame->VFrame.mpts);
    }
    else
    {
        aloge("fatal error! unsupport pixel format[0x%x]", pFrame->VFrame.mPixelFormat);
    }

    //alogd("prepare store frame in file[%s]", DbgStoreFilePath);
    FILE *dbgFp = fopen(DbgStoreFilePath, "wb");
    if(dbgFp != NULL)
    {
        VideoFrameBufferSizeInfo FrameSizeInfo;
        getVideoFrameBufferSizeInfo(pFrame, &FrameSizeInfo);
        int yuvSize[3] = {FrameSizeInfo.mYSize, FrameSizeInfo.mUSize, FrameSizeInfo.mVSize};
        for(int i=0; i<3; i++)
        {
            if(pFrame->VFrame.mpVirAddr[i] != NULL)
            {
                fwrite(pFrame->VFrame.mpVirAddr[i], 1, yuvSize[i], dbgFp);
                //alogd("virAddr[%d]=[%p], length=[%d]", i, pFrame->VFrame.mpVirAddr[i], yuvSize[i]);
            }
        }
        fclose(dbgFp);
        //alogd("store frame in file[%s]", DbgStoreFilePath);
        mDbgFrameFilePathList.emplace_back(DbgStoreFilePath);
    }
    else
    {
        aloge("fatal error! open file[%s] fail!", DbgStoreFilePath);
    }
    while(mDbgFrameFilePathList.size() > DEBUG_SAVE_FRAME_NUM)
    {
        remove(mDbgFrameFilePathList.front().c_str());
        mDbgFrameFilePathList.pop_front();
    }
    return NO_ERROR;
}

status_t VIChannel::PreviewG2dTimeoutCb(void)
{
    mpCallbackNotifier->NotifyG2DTimeout();
    return NO_ERROR;
}

}; /* namespace EyeseeLinux */

