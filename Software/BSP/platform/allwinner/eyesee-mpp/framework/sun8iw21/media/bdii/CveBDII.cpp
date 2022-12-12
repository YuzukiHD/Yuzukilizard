/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CveBDII.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/02
  Last Modified :
  Description   : ise device.
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "CveBDII"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>

#include <SystemBase.h>
#include <ConfigOption.h>

#include <VIDEO_FRAME_INFO_S.h>
#include "mpi_sys.h"
#include "ion_memmanager.h"

#include "CveBDII.h"

using namespace std;

namespace EyeseeLinux {

//#define TIME_TEST
//#define DEBUG_DATA_SAVE_OUT

status_t CveBDII::CameraProxyInfo::SetBDIICameraChnInfo(BDIICameraChnInfo *pCameraChnInfo)
{
    if (mpCameraProxy != NULL)
    {
        delete mpCameraProxy;
        mpCameraProxy = NULL;
    }
    if(pCameraChnInfo->mpCamera)
    {
        mpCameraProxy = pCameraChnInfo->mpCamera->getRecordingProxy();
        mnCameraChannel = pCameraChnInfo->mnCameraChannel;
    }
    else
    {
        aloge("fatal error! camera == NULL");
    }
    return NO_ERROR;
}

status_t CveBDII::CameraProxyInfo::releaseUsedFrame_l(VIDEO_FRAME_BUFFER_S *pFrame)
{
    if (pFrame)
    {
        bool findFlag = false;

     //first release v4l2 buf
        mpCameraProxy->releaseRecordingFrame(mnCameraChannel, pFrame->mFrameBuf.mId);
     //next release uselist node to idlelist
        for (std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mUsedFrameBufList.begin(); it!=mUsedFrameBufList.end(); ++it)
        {
            if (it->mFrameBuf.mId == pFrame->mFrameBuf.mId)
            {
                findFlag = true;
                mIdleFrameBufList.splice(mIdleFrameBufList.end(), mUsedFrameBufList, it);
                break;
            }
        }

        if (!findFlag)
        {
            aloge("fatal error! why frame[%d] not in cameraproxy used list??", pFrame->mFrameBuf.mId);
        }
    }
    else
    {
        aloge("fatal error! frame is null!");    
    }

    return NO_ERROR;
}

status_t CveBDII::CameraProxyInfo::releaseUsedFrame(VIDEO_FRAME_BUFFER_S *pFrame)
{
    AutoMutex autoLock(mFrameBufListLock);
    return releaseUsedFrame_l(pFrame);
}

status_t CveBDII::CameraProxyInfo::releaseAllFrames()
{
    AutoMutex lock(mFrameBufListLock);
    int frameCnt = 0;

    if (!mReadyFrameBufList.empty())
    {
        alogw("Warning! cameraProxy[%p] still has [%d]frames unhandled, release directly", mpCameraProxy, mReadyFrameBufList.size());
        for (std::list<VIDEO_FRAME_BUFFER_S>::iterator it=mReadyFrameBufList.begin(); it!=mReadyFrameBufList.end(); ++it)
        {
            mpCameraProxy->releaseRecordingFrame(mnCameraChannel, it->mFrameBuf.mId);
            frameCnt++;
        }
    }

    if (!mUsedFrameBufList.empty())
    {
        aloge("fatal error! still [%d] frame in use", mUsedFrameBufList.size());
    }
    return NO_ERROR;
}

CveBDII::CameraProxyInfo::CameraProxyInfo()
{
    mFrameCounter = 0;
    mpCameraProxy = NULL;
    mnCameraChannel = 0;;
    mIdleFrameBufList.resize(5); //guarantee enough frame number.
}

CveBDII::CameraProxyInfo::CameraProxyInfo(const CameraProxyInfo& ref)
{
    mpCameraProxy       = ref.mpCameraProxy;
    mnCameraChannel     = ref.mnCameraChannel;
    mIdleFrameBufList   = ref.mIdleFrameBufList;
    mReadyFrameBufList  = ref.mReadyFrameBufList;
}

CveBDII::CameraProxyInfo::~CameraProxyInfo()
{
    releaseAllFrames();
    if (mpCameraProxy != NULL)
    {
        delete mpCameraProxy;
        mpCameraProxy = NULL;
    }
}

CveBDII::CameraProxyListener::CameraProxyListener(CveBDII *pCveBDII, int nCameraIndex)
{
    mpCveBDII = pCveBDII;
    mCameraIndex = nCameraIndex;
}

void CveBDII::CameraProxyListener::dataCallbackTimestamp(const void *pdata)
{
    mpCveBDII->dataCallbackTimestamp(mCameraIndex, (const VIDEO_FRAME_BUFFER_S*)pdata);
}

CveBDII::DoProcessFrameThread::DoProcessFrameThread(CveBDII *pCveBDII)
    : mpCveBDII(pCveBDII)
    , hBDII(NULL)
    , mpCostImg(NULL)
    , mpDeepImg(NULL)
    , mCostImagSize(0)
    , mDeepImagSize(0)
    , bSaveYuvOut(false)
{
    mbWaitInputFrameBuf = false;
    mProcessFrameThreadState = PROCESS_FRAME_STATE_LOADED;
    mbThreadOK = true;
    mpProcessFrameDataQueue = new EyeseeQueue(3);
    //mIdleBDIINodeList.clear();
    mIdleBDIINodeList.resize(5); //for BDII_FRAME_NODE_S
    mImageSize = {0, 0};
    if(NO_ERROR != run("BDIIProcessFrameThread"))
    {
        aloge("fatal error! create thread fail");
        mbThreadOK = false;
    }
}

CveBDII::DoProcessFrameThread::~DoProcessFrameThread()
{
    ProcessFrameThreadStop();
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeProcessFrame_Exit;
    mProcessFrameMsgQueue.queueMessage(&msg);
    requestExitAndWait();
    mProcessFrameMsgQueue.flushMessage();
    delete mpProcessFrameDataQueue;
}

status_t CveBDII::DoProcessFrameThread::ProcessFrameThreadStart()
{
    AutoMutex autoLock(mLock);
    AutoMutex autoLock2(mStateLock);

    if(mProcessFrameThreadState == PROCESS_FRAME_STATE_STARTED)
    {
        alogd("already in started");
        return NO_ERROR;
    }

    if(mProcessFrameThreadState != PROCESS_FRAME_STATE_LOADED && mProcessFrameThreadState != PROCESS_FRAME_STATE_PAUSED)
    {
        aloge("fatal error! can't call in state[0x%x]", mProcessFrameThreadState);
        return INVALID_OPERATION;
    }

#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    {
        if ((NULL == mpCostImg) || (NULL == mpDeepImg))
        {
            //mpCveBDII->mParameters.getVideoSize(imagSize);
            if (NULL != mpCveBDII->mCameraProxies[0].mpCameraProxy)
            {
                CameraParameters cameraParameters;
                mpCveBDII->mCameraProxies[0].mpCameraProxy->getParameters(mpCveBDII->mCameraProxies[0].mnCameraChannel, cameraParameters);
                cameraParameters.getVideoSizeOut(mImageSize);
            }

            unsigned int size;
            unsigned int nPhyAddr;

            if (NULL == mpCostImg)
            {
                mCostImagSize = size = mImageSize.Width * mImageSize.Height * sizeof(AW_S32);
                AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &mpCostImg, size);
            }
            if (NULL == mpDeepImg)
            {
                mDeepImagSize = size = mImageSize.Width * mImageSize.Height * sizeof(AW_S16);
                AW_MPI_SYS_MmzAlloc_Cached(&nPhyAddr, &mpDeepImg, size);
            }
        }

        if (NULL == hBDII)
        {
            alogd("create BDII handle");
            hBDII = AW_AI_CVE_BDII_Create();
            if (NULL == hBDII)
            {
                aloge("create BDII fail!!");
            }
            else
            {
                AW_STATUS_E status = AW_AI_CVE_BDII_Init(hBDII, &mInitParam);
                if (AW_STATUS_OK != status)
                {
                    aloge("BDII init fail, destroy it!");
                    AW_AI_CVE_BDII_Release(hBDII);
                    hBDII = NULL;
                }
            }
        }
    }

    if ((NULL==hBDII) || (NULL==mpCostImg) || (NULL==mpDeepImg))
    {
        aloge("start thread fail! hBDII or img buffer NULL");
        return UNKNOWN_ERROR;
    }
#endif

    EyeseeMessage msg;
    msg.mMsgType = MsgTypeProcessFrame_SetState;
    msg.mPara0 = PROCESS_FRAME_STATE_STARTED;
    mProcessFrameMsgQueue.queueMessage(&msg);
    while(PROCESS_FRAME_STATE_STARTED != mProcessFrameThreadState)
    {
    	mStartCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}

#if 0
status_t CveBDII::DoProcessFrameThread::ProcessFrameThreadPause()
{
    AutoMutex autoLock(mLock);
    AutoMutex autoLock2(mStateLock);
    if(mProcessFrameThreadState == PROCESS_FRAME_STATE_PAUSED)
    {
        alogd("already in paused");
        return NO_ERROR;
    }
    if(mProcessFrameThreadState != PROCESS_FRAME_STATE_STARTED)
    {
        aloge("fatal error! can't call in state[0x%x]", mProcessFrameThreadState);
        return INVALID_OPERATION;
    }
    EyeseeMessage msg;
    msg.mMsgType = MsgTypeProcessFrame_SetState;
    msg.mPara0 = PROCESS_FRAME_STATE_PAUSED;
    mProcessFrameMsgQueue.queueMessage(&msg);
    while(PROCESS_FRAME_STATE_PAUSED != mProcessFrameThreadState)
    {
        mPauseCompleteCond.wait(mStateLock);
    }
    return NO_ERROR;
}
#endif

status_t CveBDII::DoProcessFrameThread::ProcessFrameThreadStop()
{
    AutoMutex autoLock(mLock);
    {
        AutoMutex autoLock2(mStateLock);
        if(mProcessFrameThreadState != PROCESS_FRAME_STATE_LOADED)
        {
            EyeseeMessage msg;
            msg.mMsgType = MsgTypeProcessFrame_SetState;
            msg.mPara0 = PROCESS_FRAME_STATE_LOADED;
            mProcessFrameMsgQueue.queueMessage(&msg);
            while(PROCESS_FRAME_STATE_LOADED != mProcessFrameThreadState)
            {
                mLoadedCompleteCond.wait(mStateLock);
            }
        }
     #if (MPPCFG_BDII == OPTION_BDII_ENABLE)
        {
            if (NULL != mpCostImg)
            {
                unsigned int uPhyAddr = 0;
                alogd("release costimg buf");
                AW_MPI_SYS_MmzFree(uPhyAddr, mpCostImg);
                mpCostImg = NULL;
            }
            if (NULL != mpDeepImg)
            {
                unsigned int uPhyAddr = 0;
                alogd("release deepimg buf");
                AW_MPI_SYS_MmzFree(uPhyAddr, mpDeepImg);
                mpDeepImg = NULL;
            }

            if (NULL != hBDII)
            {
                AW_STATUS_E status;
                alogd("release BDII handle");
                status = AW_AI_CVE_BDII_Release(hBDII);
                if (AW_STATUS_ERROR == status)
                {
                    aloge("BDII release fail!");
                }
                hBDII = NULL;
            }
        }
     #endif
    }
    mbWaitInputFrameBuf = false;
    return NO_ERROR;
}

#if 0
status_t CveBDII::DoProcessFrameThread::SendMessage_InputFrameReady()
{
    //mbWaitInputFrameBuf = false;
    EyeseeMessage msg;
    msg.mMsgType = DoProcessFrameThread::MsgTypeProcessFrame_InputFrameAvailable;
    mProcessFrameMsgQueue.queueMessage(&msg);
    return NO_ERROR;
}
#endif

status_t CveBDII::DoProcessFrameThread::SetBDIIProcessConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param)
{
    AutoMutex autoLock(mLock);
    mInitParam = Param;
    return NO_ERROR;
}

status_t CveBDII::DoProcessFrameThread::GetBDIIProcessConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param)
{
    AutoMutex autoLock(mLock);
    Param = mInitParam;
    return NO_ERROR;
}

bool CveBDII::DoProcessFrameThread::threadLoop()
{
    if(!exitPending())
    {
        return processFrameThread();
    }
    else
    {
        return false;
    }
}

status_t CveBDII::DoProcessFrameThread::debugYuvDataOut(bool isOut, std::vector<BDIIDebugFileOutString>&vec)
{
#ifdef DEBUG_DATA_SAVE_OUT
    AutoMutex autoLock(mLock);
    if (bSaveYuvOut == isOut)
    {
        alogw("has already set [%d]!", isOut);
        return NO_ERROR;
    }
    bSaveYuvOut = isOut;
    if (!isOut)
    {
        for (map<string, FILE *>::iterator it = mMapFdOut.begin(); it != mMapFdOut.end(); ++it)
        {
            if (it->second)
            {
                fclose(it->second);
                it->second = NULL;
            }
        }
        mMapFdOut.clear();
    }
    else
    {
        if (vec.size() > 0)
        {
            for (map<string, FILE *>::iterator it = mMapFdOut.begin(); it != mMapFdOut.end(); ++it)
            {
                if (it->second)
                {
                    fclose(it->second);
                    it->second = NULL;
                }
            }
            mMapFdOut.clear();

            FILE *fp;
            string str;
            for (vector<BDIIDebugFileOutString>::iterator it = vec.begin(); it != vec.end(); ++it)
            {
                if (!it->fileOutStr.empty())
                {
                    fp = fopen(it->fileOutStr.c_str(), "wb");
                    if (BDIIDebugFileOutString::FILE_STR_LEFT_YUV == it->fileOutStyle)
                    {
                        str = string("in_left");
                    }
                    else if (BDIIDebugFileOutString::FILE_STR_RIGHT_YUV == it->fileOutStyle)
                    {
                        str = string("in_right");
                    }
                    else if (BDIIDebugFileOutString::FILE_STR_RESULT_DEEP_YUV == it->fileOutStyle)
                    {
                        str = string("result_deep");
                    }
                    else if (BDIIDebugFileOutString::FILE_STR_RESULT_COST_YUV == it->fileOutStyle)
                    {
                        str = string("result_cost");
                    }
                    mMapFdOut.insert(make_pair(str, fp));
                }
            }
        }
        else
        {
            bSaveYuvOut = false;
            aloge("input file path empty!");
        }
    }
    return NO_ERROR;
#else
    return UNKNOWN_ERROR;
#endif
}

status_t CveBDII::DoProcessFrameThread::sendFrame(VIDEO_FRAME_BUFFER_S *pFrame0, VIDEO_FRAME_BUFFER_S *pFrame1)
{
    status_t ret;
    AutoMutex lock(mLock);
    if ((NULL ==pFrame0) || (NULL ==pFrame1))
    {
        return BAD_VALUE;
    }

    AutoMutex nodeLock(mBDIINodeLock);
    if (mIdleBDIINodeList.empty())
    {
        alogd("Be careful! node idlelist is empty, increase!");
        BDII_FRAME_NODE_S elem;
        memset(&elem, 0, sizeof(BDII_FRAME_NODE_S));
        mIdleBDIINodeList.insert(mIdleBDIINodeList.end(), 5, elem);
    }
    BDII_FRAME_NODE_S *pFrmBuf = &mIdleBDIINodeList.front();
    pFrmBuf->pFrame0 = pFrame0;
    pFrmBuf->pFrame1 = pFrame1;

//when putelemdata success, process frame thread will stop receive elem data until handled the elem data over release it
//so when in handling, call sendframe() will not success
    ret = mpProcessFrameDataQueue->PutElemDataValid((void*)pFrmBuf);
    if (NO_ERROR != ret)
    {
        return ret;
    }
    mUsedBDIINodeList.splice(mUsedBDIINodeList.end(), mIdleBDIINodeList, mIdleBDIINodeList.begin());
    //alogd("only for debug! BDII frame pair pts[%lld]-[%lld]=[%lld]us!", pFrame0->mFrameBuf.VFrame.mpts, pFrame1->mFrameBuf.VFrame.mpts, 
    //    (int64_t)(pFrame0->mFrameBuf.VFrame.mpts - pFrame1->mFrameBuf.VFrame.mpts));
    while(mpProcessFrameDataQueue->GetValidElemNum() > 1)
    {
        //alogd("only for debug! BDII speed is slower than camera speed!");
        BDII_FRAME_NODE_S *pNodebuf = (BDII_FRAME_NODE_S*)mpProcessFrameDataQueue->GetValidElemData();
        //release previous elem data
        mpCveBDII->mCameraProxies[0].releaseUsedFrame_l(pNodebuf->pFrame0);
        mpCveBDII->mCameraProxies[1].releaseUsedFrame_l(pNodebuf->pFrame1);
        //free used bdii node
        bool bFind = false;
        for (std::list<BDII_FRAME_NODE_S>::iterator it=mUsedBDIINodeList.begin(); it!=mUsedBDIINodeList.end(); ++it)
        {
            if (&(*it) == pNodebuf)
            {
                mIdleBDIINodeList.splice(mIdleBDIINodeList.end(), mUsedBDIINodeList, it);
                bFind = true;
                break;
            }
        }
        if(!bFind)
        {
            aloge("fatal error! not find BDIINodebuf[%p] in usedNodeList!", pNodebuf);
        }
        mpProcessFrameDataQueue->ReleaseElemData(pNodebuf);
    }

    AutoMutex lock2(mWaitLock);
    if(mbWaitInputFrameBuf)
    {
        mbWaitInputFrameBuf = false;
        EyeseeMessage msg;
        msg.mMsgType = MsgTypeProcessFrame_InputFrameAvailable;
        mProcessFrameMsgQueue.queueMessage(&msg);
    }
    return NO_ERROR;
}


bool CveBDII::DoProcessFrameThread::processFrameThread()
{
    bool bRunningFlag = true;
    EyeseeMessage msg;
    status_t getMsgRet;
    ERRORTYPE ret;
    while(1)
    {
    PROCESS_MESSAGE:
        getMsgRet = mProcessFrameMsgQueue.dequeueMessage(&msg);
        if(getMsgRet == NO_ERROR)
        {
            if(MsgTypeProcessFrame_SetState == msg.mMsgType)
            {
                AutoMutex autoLock(mStateLock);
                if(msg.mPara0 == mProcessFrameThreadState)
                {
                    aloge("fatal error! same state[0x%x]", mProcessFrameThreadState);
                }
                else if(PROCESS_FRAME_STATE_LOADED == msg.mPara0)
                {
                    if(PROCESS_FRAME_STATE_PAUSED == mProcessFrameThreadState || PROCESS_FRAME_STATE_STARTED == mProcessFrameThreadState)
                    {
                        //release all input frame pair
                        while(1)
                        {
                            BDII_FRAME_NODE_S *pNodebuf = (BDII_FRAME_NODE_S*)mpProcessFrameDataQueue->GetValidElemData();
                            if(pNodebuf != NULL)
                            {
                                //release frame to cameraproxy
                                mpCveBDII->mCameraProxies[0].releaseUsedFrame(pNodebuf->pFrame0);
                                mpCveBDII->mCameraProxies[1].releaseUsedFrame(pNodebuf->pFrame1);
                                {
                                    bool bFind = false;
                                    AutoMutex lock(mBDIINodeLock); //free used bdii node
                                    for (std::list<BDII_FRAME_NODE_S>::iterator it=mUsedBDIINodeList.begin(); it!=mUsedBDIINodeList.end(); it++)
                                    {
                                        if (&(*it) == pNodebuf)
                                        {
                                            mIdleBDIINodeList.splice(mIdleBDIINodeList.end(), mUsedBDIINodeList, it);
                                            bFind = true;
                                            break;
                                        }
                                    }
                                    if(!bFind)
                                    {
                                        aloge("fatal error! not find nodeBuf[%p] in usedBDIINodeList!", pNodebuf);
                                    }
                                }
                                alogd("release BDII frame pair[%p] in usedBDIINodeList!", pNodebuf);
                                mpProcessFrameDataQueue->ReleaseElemData(pNodebuf); //enable to receiver elem data
                            }
                            else
                            {
                                break;
                            }
                        }
                        AutoMutex lock(mBDIINodeLock);
                        if(mUsedBDIINodeList.size()!= 0)
                        {
                            aloge("fatal error! why still has [%d]used BDII nodes", mUsedBDIINodeList.size());
                        }
                    }
                    else
                    {
                        aloge("fatal error! unknown state[0x%x]", mProcessFrameThreadState);
                    }
                    alogd("process frame thread state[0x%x]->loaded", mProcessFrameThreadState);
                    mProcessFrameThreadState = PROCESS_FRAME_STATE_LOADED;
                    mLoadedCompleteCond.signal();
                    
                }
                else if(PROCESS_FRAME_STATE_PAUSED == msg.mPara0)
                {
                    alogd("process frame thread state[0x%x]->paused", mProcessFrameThreadState);
                    mProcessFrameThreadState = PROCESS_FRAME_STATE_PAUSED;
                    mPauseCompleteCond.signal();
                }
                else if(PROCESS_FRAME_STATE_STARTED == msg.mPara0)
                {
                    alogd("process frame thread state[0x%x]->started", mProcessFrameThreadState);
                    mProcessFrameThreadState = PROCESS_FRAME_STATE_STARTED;
                    mStartCompleteCond.signal();
                }
                else
                {
                    aloge("fatal error! check code!");
                }
            }
            else if(MsgTypeProcessFrame_InputFrameAvailable == msg.mMsgType)
            {
                //alogv("input frame come");
            }
            else if(MsgTypeProcessFrame_Exit == msg.mMsgType)
            {
                {
                    AutoMutex lock(mBDIINodeLock);
                    if (!mUsedBDIINodeList.empty()) //return node to camproxy
                    {
                        aloge("fatal error! usedBDII list still in use! release it");

                        for (std::list<BDII_FRAME_NODE_S>::iterator it=mUsedBDIINodeList.begin(); it!=mUsedBDIINodeList.end(); it++)
                        {
                            mpCveBDII->mCameraProxies[0].releaseUsedFrame(it->pFrame0);
                            mpCveBDII->mCameraProxies[1].releaseUsedFrame(it->pFrame1);
                        }
                    }
                }

                bRunningFlag = false;
                goto _exit0;
            }
            else
            {
                aloge("unknown msg[0x%x]!", msg.mMsgType);
            }
            goto PROCESS_MESSAGE;
        }

        if(PROCESS_FRAME_STATE_STARTED == mProcessFrameThreadState)
        {
     #if (MPPCFG_BDII == OPTION_BDII_ENABLE)
            BDII_FRAME_NODE_S *pNodebuf = NULL;
            pNodebuf = (BDII_FRAME_NODE_S*)mpProcessFrameDataQueue->GetValidElemData();
            if (pNodebuf == NULL)
            {
                {
                    AutoMutex lock(mWaitLock);
                    if(mpProcessFrameDataQueue->GetValidElemNum() > 0)
                    {
                        alogd("Low probability! face new frame come before check again.");
                        goto PROCESS_MESSAGE;
                    }
                    else
                    {
                        mbWaitInputFrameBuf = true;
                    }
                }
                mProcessFrameMsgQueue.waitMessage();
                goto PROCESS_MESSAGE;
            }

         //disable to receiver elem data
            VIDEO_FRAME_BUFFER_S *pbuf[2];
            pbuf[0] = pNodebuf->pFrame0;
            pbuf[1] = pNodebuf->pFrame1;

            if ((NULL ==pbuf[0]) || (NULL ==pbuf[1]))
            {
                aloge("fatal error! video frame null!");
            }

            AW_STATUS_E status;
            AW_IMAGE_S leftImag, rightImag;
            CVE_BDII_RES_OUT_S resOutput;

            resOutput.result.as16DeepImg = (AW_S16 *)mpDeepImg;
            resOutput.result.as32CostImg = (AW_S32 *)mpCostImg;

            leftImag.mWidth = pbuf[0]->mFrameBuf.VFrame.mWidth;
            leftImag.mHeight = pbuf[0]->mFrameBuf.VFrame.mHeight;
            leftImag.mpVirAddr[0] = pbuf[0]->mFrameBuf.VFrame.mpVirAddr[0];
            leftImag.mStride[0] = pbuf[0]->mFrameBuf.VFrame.mWidth;

            rightImag.mWidth = pbuf[1]->mFrameBuf.VFrame.mWidth;
            rightImag.mHeight = pbuf[1]->mFrameBuf.VFrame.mHeight;
            rightImag.mpVirAddr[0] = pbuf[1]->mFrameBuf.VFrame.mpVirAddr[0];
            rightImag.mStride[0] = pbuf[1]->mFrameBuf.VFrame.mWidth;

            //alogd("fram0[%d], frame1[%d]", pbuf0->mId, pbuf1->mId);
        #ifdef DEBUG_DATA_SAVE_OUT
            {
                AutoMutex autoLock(mLock);
                if (bSaveYuvOut)
                {
                    int i = 0;
                    map<string, FILE *>::iterator mIt;
                    mIt = mMapFdOut.find("in_left");
                    if (mIt != mMapFdOut.end())
                    {
                        if (mIt->second)
                        {
                            fwrite(leftImag.mpVirAddr[0], leftImag.mWidth*leftImag.mHeight, 1, mIt->second);
                        }
                    }

                    mIt = mMapFdOut.find("in_right");
                    if (mIt != mMapFdOut.end())
                    {
                        if (mIt->second)
                        {
                            fwrite(rightImag.mpVirAddr[0], rightImag.mWidth*rightImag.mHeight, 1, mIt->second);
                        }
                    }
                }
            }
        #endif

        #ifdef TIME_TEST
            int64_t nStartTm = CDX_GetSysTimeUsMonotonic();
        #endif
            status = AW_AI_CVE_BDII_Process(hBDII, &leftImag, &rightImag, &resOutput.result);
            resOutput.costImgSize = mCostImagSize;
            resOutput.deepImgSize = mDeepImagSize;
            resOutput.mImgSize = mImageSize;
            resOutput.mPts = pbuf[0]->mFrameBuf.VFrame.mpts;
        #ifdef TIME_TEST
            int64_t nEndTm = CDX_GetSysTimeUsMonotonic();
            alogv("CVE_BDII_Process:time used:%d us", nEndTm - nStartTm);
        #endif
            if (AW_STATUS_OK == status)
            {
                if(mpCveBDII->isMessageEnabled(CAMERA_MSG_BDII_DATA))
                {
                    if (mpCveBDII->mpDataCallback)
                    {
                        unsigned int nCameraMsgType = 0;
                        nCameraMsgType |= CAMERA_MSG_BDII_DATA;

                        std::shared_ptr<CMediaMemory> spMem = std::make_shared<CMediaMemory>(sizeof(CVE_BDII_RES_OUT_S));
                        memcpy(spMem->getPointer(), &resOutput, sizeof(CVE_BDII_RES_OUT_S));

                        mpCveBDII->mpDataCallback->postData(nCameraMsgType, 0, spMem, sizeof(CVE_BDII_RES_OUT_S));
                    }
                }
                if(mpCveBDII->mpPreviewThread)
                {
                    mpCveBDII->mpPreviewThread->sendDeepImage(&resOutput);
                }
              #ifdef DEBUG_DATA_SAVE_OUT
                {
                    AutoMutex autoLock(mLock);
                    if (bSaveYuvOut)
                    {
                        int i = 0;
                        map<string, FILE *>::iterator mIt;
                        mIt = mMapFdOut.find("result_deep");
                        if (mIt != mMapFdOut.end())
                        {
                            if (mIt->second)
                            {
                                fwrite(resOutput.result.as16DeepImg, resOutput.deepImgSize, 1, mIt->second);
                            }
                        }

                        mIt = mMapFdOut.find("result_cost");
                        if (mIt != mMapFdOut.end())
                        {
                            if (mIt->second)
                            {
                                fwrite(resOutput.result.as32CostImg, resOutput.costImgSize, 1, mIt->second);
                            }
                        }
                    }
                }
              #endif
            }
            else if (AW_STATUS_ERROR == status)
            {
                aloge("process error!");
            }
            else if (AW_STATUS_SKIP == status)
            {
                alogw("process skip!");
            }

            for (int i=0; i<2; i++) //release frame to cameraproxy
            {
                mpCveBDII->mCameraProxies[i].releaseUsedFrame(pbuf[i]);
            }

            {
                AutoMutex lock(mBDIINodeLock); //free used bdii node
                for (std::list<BDII_FRAME_NODE_S>::iterator it=mUsedBDIINodeList.begin(); it!=mUsedBDIINodeList.end(); it++)
                {
                    if (&(*it) == pNodebuf)
                    {
                        mIdleBDIINodeList.splice(mIdleBDIINodeList.end(), mUsedBDIINodeList, it);
                        break;
                    }
                }
            }

            mpProcessFrameDataQueue->ReleaseElemData(pNodebuf); //enable to receiver elem data
     #endif
        }
        else
        {
            mProcessFrameMsgQueue.waitMessage();
        }
    }

    return true;
_exit0:
  #ifdef DEBUG_DATA_SAVE_OUT
    {
        AutoMutex autoLock(mLock);
        if (bSaveYuvOut)
        {
            for (map<string, FILE *>::iterator it = mMapFdOut.begin(); it != mMapFdOut.end(); ++it)
            {
                if (it->second)
                {
                    fclose(it->second);
                    it->second = NULL;
                }
            }
            mMapFdOut.clear();
            bSaveYuvOut = false;
        }
    }
  #endif
    return bRunningFlag;
}

int CveBDII::DoPreviewThread::mFrameIdCounter = 0;
CveBDII::DoPreviewThread::DoPreviewThread(CveBDII *pBDII)
    : mpBDII(pBDII)
{
    mHlay = MM_INVALID_LAYER;
    mPreviewRotation = 0;
    mpPreviewWindow = new PreviewWindow(this);
    if(NULL == mpPreviewWindow)
    {
        aloge("fatal error! malloc preview window fail!");
    }
    mbWaitPreviewFrame = false;
    mImageSize = {0, 0};
}
CveBDII::DoPreviewThread::~DoPreviewThread()
{
    if (mpPreviewWindow != NULL)
    {
        delete mpPreviewWindow;
    }
}
status_t CveBDII::DoPreviewThread::startThread()
{
    //prepare video frame buffer
    mImageSize = {0, 0};
    CameraParameters cameraParameters;
    mpBDII->mCameraProxies[0].mpCameraProxy->getParameters(mpBDII->mCameraProxies[0].mnCameraChannel, cameraParameters);
    cameraParameters.getVideoSizeOut(mImageSize);
    mIdleFrameList.resize(4);
    for(VIDEO_FRAME_BUFFER_S& i : mIdleFrameList)
    {
        memset(&i, 0, sizeof(VIDEO_FRAME_BUFFER_S));
        i.mFrameBuf.VFrame.mWidth = mImageSize.Width;
        i.mFrameBuf.VFrame.mHeight = mImageSize.Height;
        i.mFrameBuf.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        int nYSize = mImageSize.Width * mImageSize.Height;
        if(SUCCESS != AW_MPI_SYS_MmzAlloc_Cached(&i.mFrameBuf.VFrame.mPhyAddr[0], &i.mFrameBuf.VFrame.mpVirAddr[0], nYSize))
        {
            aloge("fatal error! palloc fail!");
        }
        int nCSize = mImageSize.Width * mImageSize.Height/2;
        if(SUCCESS != AW_MPI_SYS_MmzAlloc_Cached(&i.mFrameBuf.VFrame.mPhyAddr[1], &i.mFrameBuf.VFrame.mpVirAddr[1], nCSize))
        {
            aloge("fatal error! palloc fail!");
        }
        memset(i.mFrameBuf.VFrame.mpVirAddr[1], 0x80, nCSize);
        AW_MPI_SYS_MmzFlushCache(i.mFrameBuf.VFrame.mPhyAddr[1], i.mFrameBuf.VFrame.mpVirAddr[1], nCSize);
        i.mFrameBuf.VFrame.mOffsetTop = 0;
        i.mFrameBuf.VFrame.mOffsetBottom = mImageSize.Height;
        i.mFrameBuf.VFrame.mOffsetLeft = 0;
        i.mFrameBuf.VFrame.mOffsetRight = mImageSize.Width;
        i.mFrameBuf.mId = mFrameIdCounter++;
    }
    mpPreviewWindow->setPreviewWindow(mHlay);
    mpPreviewWindow->setPreviewRotation(mPreviewRotation);
    mpPreviewWindow->setDispBufferNum(0);
    mpPreviewWindow->startPreview();
    status_t ret = run("BDIIPreview");
    if(ret != NO_ERROR)
    {
        aloge("fatal error! run thread fail!");
    }
    return ret;
}

void CveBDII::DoPreviewThread::stopThread()
{
    EyeseeMessage msg;
    msg.mMsgType = MsgTypePreview_Exit;
    mMsgQueue.queueMessage(&msg);
    join();
    mMsgQueue.flushMessage();

    mpPreviewWindow->stopPreview();
    //free video frame buffer
    AutoMutex lock(mFrameLock);
    if(!mDisplayFrameList.empty())
    {
        aloge("fatal error! display frame list is not empty, check code!");
        mIdleFrameList.splice(mIdleFrameList.end(), mDisplayFrameList);
    }
    if(!mReadyFrameList.empty())
    {
        aloge("fatal error! ready frame list is not empty, check code!");
        mIdleFrameList.splice(mIdleFrameList.end(), mReadyFrameList);
    }
    if(!mUsingFrame.empty())
    {
        aloge("fatal error! using frame list is not empty, check code!");
        mIdleFrameList.splice(mIdleFrameList.end(), mUsingFrame);
    }
    for(VIDEO_FRAME_BUFFER_S& i : mIdleFrameList)
    {
        AW_MPI_SYS_MmzFree(i.mFrameBuf.VFrame.mPhyAddr[0], i.mFrameBuf.VFrame.mpVirAddr[0]);
        AW_MPI_SYS_MmzFree(i.mFrameBuf.VFrame.mPhyAddr[1], i.mFrameBuf.VFrame.mpVirAddr[1]);
    }
}

status_t CveBDII::DoPreviewThread::convertBDIIDeepImage2NV21(CVE_BDII_RES_OUT_S *pBDIIResult, VIDEO_FRAME_BUFFER_S *pFrmBuf)
{
    if(pBDIIResult->mImgSize.Width != pFrmBuf->mFrameBuf.VFrame.mWidth
        || pBDIIResult->mImgSize.Height != pFrmBuf->mFrameBuf.VFrame.mHeight)
    {
        aloge("fatal error! BDII image size[%dx%d] is not match frameBufSize[%dx%d], can't convert!", 
            pBDIIResult->mImgSize.Width, pBDIIResult->mImgSize.Height, 
            pFrmBuf->mFrameBuf.VFrame.mWidth, pFrmBuf->mFrameBuf.VFrame.mHeight);
        return UNKNOWN_ERROR;
    }
    VideoFrameBufferSizeInfo stFrameSizeInfo;
    getVideoFrameBufferSizeInfo(&pFrmBuf->mFrameBuf, &stFrameSizeInfo);
    int nPixelNum = pBDIIResult->mImgSize.Width*pBDIIResult->mImgSize.Height;
    char *img = (char*)pFrmBuf->mFrameBuf.VFrame.mpVirAddr[0];
    int16_t img_d;
    for (int i = 0; i < nPixelNum; ++i) 
    {
        img_d = pBDIIResult->result.as16DeepImg[i];
        if (img_d < 0) 
        {
            img[i] = 0;
        } 
        else 
        {
            img_d = img_d >> 2;
            if (img_d > 255) 
            {
                img[i] = 255;
            } 
            else 
            {
                img[i] = img_d;
            }
        }
    }
    AW_MPI_SYS_MmzFlushCache(pFrmBuf->mFrameBuf.VFrame.mPhyAddr[0], pFrmBuf->mFrameBuf.VFrame.mpVirAddr[0], stFrameSizeInfo.mYSize);
    return NO_ERROR;
}

status_t CveBDII::DoPreviewThread::sendDeepImage(CVE_BDII_RES_OUT_S *pBDIIResult)
{
    if(mHlay < 0)
    {
        return NO_INIT;
    }
    //convert to nv21 frame buffer
    {
        AutoMutex lock(mFrameLock);
        if(mIdleFrameList.empty())
        {
            alogw("Be careful! No Idle frame left, discard this image!");
            return UNKNOWN_ERROR;
        }
        if(!mUsingFrame.empty())
        {
            aloge("fatal error! using frame list is not empty!");
        }
        mUsingFrame.splice(mUsingFrame.end(), mIdleFrameList, mIdleFrameList.begin());
    }
    VIDEO_FRAME_BUFFER_S *pFrmBuf = &mUsingFrame.front();
    convertBDIIDeepImage2NV21(pBDIIResult, pFrmBuf);
    pFrmBuf->mFrameBuf.VFrame.mpts = pBDIIResult->mPts;
    {
        AutoMutex lock(mFrameLock);
        mReadyFrameList.splice(mReadyFrameList.end(), mUsingFrame, mUsingFrame.begin());
        if(mReadyFrameList.back().mRefCnt != 0)
        {
            aloge("fatal error! refCnt[%d] != 0", mReadyFrameList.back().mRefCnt);
            mReadyFrameList.back().mRefCnt = 0;
        }
        if(!mUsingFrame.empty())
        {
            aloge("fatal error! using frame list is not empty!");
        }
    }
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

status_t CveBDII::DoPreviewThread::setDisplay(int hlay)
{
    mHlay = hlay;
    return NO_ERROR;
}

status_t CveBDII::DoPreviewThread::setPreviewRotation(int rotation)
{
    mPreviewRotation = rotation;
    return NO_ERROR;
}

bool CveBDII::DoPreviewThread::threadLoop()
{
    if(!exitPending())
    {
        return previewThread();
    }
    else
    {
        return false;
    }
}

bool CveBDII::DoPreviewThread::previewThread()
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
            if(DoPreviewThread::MsgTypePreview_InputFrameAvailable == msg.mMsgType)
            {
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

        {
            AutoMutex lock(mFrameLock);
            if(!mReadyFrameList.empty())
            {
                mDisplayFrameList.splice(mDisplayFrameList.end(), mReadyFrameList, mReadyFrameList.begin());
                pFrmbuf = &mDisplayFrameList.back();
                if(pFrmbuf->mRefCnt != 0)
                {
                    aloge("fatal error! refCnt[%d]!=0", pFrmbuf->mRefCnt);
                }
                pFrmbuf->mRefCnt = 1;
            }
            else
            {
                pFrmbuf = NULL;
                AutoMutex lock(mWaitLock);
                mbWaitPreviewFrame = true;
            }
        }
        if(NULL == pFrmbuf)
        {
            int nWaitTime = 0;    //unit:ms, 10*1000
            int msgNum = mMsgQueue.waitMessage(nWaitTime);
            if(msgNum <= 0)
            {
                aloge("fatal error! preview thread wait message timeout[%d]ms! msgNum[%d], bWait[%d]", nWaitTime, msgNum, mbWaitPreviewFrame);
            }
            goto PROCESS_MESSAGE;
        }
        mpPreviewWindow->onNextFrameAvailable(pFrmbuf);

        decreaseBufRef(pFrmbuf->mFrameBuf.mId);
        //return true;
        goto PROCESS_MESSAGE;
    }
_exit0:
    return bRunningFlag;
}

void CveBDII::DoPreviewThread::increaseBufRef(VIDEO_FRAME_BUFFER_S *pbuf)
{
    AutoMutex lock(mFrameLock);
    ++pbuf->mRefCnt;
}

void CveBDII::DoPreviewThread::decreaseBufRef(unsigned int nBufId)
{
    AutoMutex lock(mFrameLock);
    bool bFind = false;
    std::list<VIDEO_FRAME_BUFFER_S>::iterator itFrm;
    for(std::list<VIDEO_FRAME_BUFFER_S>::iterator it = mDisplayFrameList.begin(); it != mDisplayFrameList.end(); ++it)
    {
        if(it->mFrameBuf.mId == nBufId)
        {
            if(false == bFind)
            {
                bFind = true;
                itFrm = it;
            }
            else
            {
                aloge("fatal error! more than one bufId[%d]?", nBufId);
            }
        }
    }
    if(false == bFind)
    {
        aloge("fatal error! more than one bufId[%d]?", nBufId);
        return;
    }
    if (itFrm->mRefCnt > 0 && --itFrm->mRefCnt == 0) 
    {
        mIdleFrameList.splice(mIdleFrameList.end(), mDisplayFrameList, itFrm);
    }
}

void CveBDII::DoPreviewThread::NotifyRenderStart()
{
    if(mpBDII->mpNotifyCallback)
    {
        unsigned int nCameraMsgType = CAMERA_MSG_INFO;  //CAMERA_MSG_COMPRESSED_IMAGE;
        CameraMsgInfoType eMsgInfoType = CAMERA_INFO_RENDERING_START;
        mpBDII->mpNotifyCallback->notify(nCameraMsgType, 0, eMsgInfoType, 0);
    }
}

CveBDII::CveBDII(unsigned int mID)
    : m_ID(mID)
    , mpDataCallback(NULL)
    , mpNotifyCallback(NULL)
    , mpProcessFrameThread(NULL)
{
    alogd("BDII Construct");
    //mInitCamIndex[0] = -1;
    //mInitCamIndex[1] = -1;
    //mCameraProxies.clear();
    //mCameraProxies.resize(2); //left & right
    mMessageEnabler = 0;
    mBDIIState = BDII_STATE_CONSTRUCTED;
#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    mpProcessFrameThread = new DoProcessFrameThread(this);
#endif
    mpPreviewThread = new DoPreviewThread(this);
}

CveBDII::~CveBDII()
{
    alogd("Destruct");
    stop();
#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    if (mpProcessFrameThread)
    {
        delete mpProcessFrameThread;
    }
#endif
    if (mpPreviewThread)
    {
        delete mpPreviewThread;
    }
}

status_t CveBDII::setCamera(std::vector<BDIICameraChnInfo>& cameraChannels)
{
    if(cameraChannels.size() <= 0)
    {
        aloge("fatal error! none camera is set!");
        return BAD_VALUE;
    }
    if(cameraChannels.size() != 2)
    {
        aloge("fatal error! need two cameras!");
    }

    if (mBDIIState != BDII_STATE_CONSTRUCTED)
    {
        aloge("setCamera in error state %d, must before prepared!", mBDIIState);
        return INVALID_OPERATION;
    }
    //mCameraProxies.clear();
    mCameraProxies.resize(cameraChannels.size());
    int i = 0;
    for(std::vector<BDIICameraChnInfo>::iterator it = cameraChannels.begin(); it!= cameraChannels.end(); ++it)
    {
        if (BDII_CAMERA_LEFT == it->mCamStyle)
        {
            i = 0;
        }
        else
        {
            i = 1;
        }
        mCameraProxies[i].SetBDIICameraChnInfo(&(*it));
    }

 //get camera_params
    //if (NULL != mCameraProxies[0].mpCameraProxy)
    //{
    //    mCameraProxies[0].mpCameraProxy->getParameters(mCameraProxies[0].mnCameraChannel, mParameters);
    //}

    return NO_ERROR;
}

status_t CveBDII::prepare()
{
    AutoMutex lock(mLock);
    status_t result;

    if(mBDIIState != BDII_STATE_CONSTRUCTED)
    {
        aloge("prepare in error state %d", mBDIIState);
        return INVALID_OPERATION;
    }

    //cdx_sem_init(&mWaitReturnFrameSem, 0);
    mBDIIState = BDII_STATE_PREPARED;

    return NO_ERROR;
_err0:
    return UNKNOWN_ERROR;
}

status_t CveBDII::start()
{
    AutoMutex lock(mLock);
    if (mBDIIState == BDII_STATE_STARTED)
    {
        alogd("already in state started");
        return NO_ERROR;
    }
    if (mBDIIState != BDII_STATE_PREPARED)
    {
        aloge("start in error state %d", mBDIIState);
        return INVALID_OPERATION;
    }
    //judge if camera is set successful.
    {
        bool bCameraOK = true;
        if (mCameraProxies.size() > 0)
        {
            for (std::vector<CameraProxyInfo>::iterator it=mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
            {
                if (NULL == it->mpCameraProxy)
                {
                    aloge("Camera is not set yet?");
                    bCameraOK = false;
                }
            }
        }
        else
        {
            aloge("no camera set?");
            bCameraOK = false;
        }
        if (!bCameraOK)
        {
            return INVALID_OPERATION;
        }
    }

    if (mCameraProxies.size() != 2)
    {
        aloge("fatal error! need two cameras!");
        return INVALID_OPERATION;
    }

    mBDIIState = BDII_STATE_STARTED;

    if(mpPreviewThread)
    {
        mpPreviewThread->startThread();
    }
#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    mpProcessFrameThread->ProcessFrameThreadStart();
#endif

    for (unsigned int i=0; i<mCameraProxies.size(); i++)
    {
        mCameraProxies[i].mpCameraProxy->startRecording(
            mCameraProxies[i].mnCameraChannel,
            new CameraProxyListener(this, i),
            m_ID);
    }

    return NO_ERROR;
}

status_t CveBDII::stop_l()
{
    if (mBDIIState != BDII_STATE_STARTED)
    {
        aloge("stop in error state %d", mBDIIState);
        return INVALID_OPERATION;
    }

  //stop transe camera frame
    for (unsigned int i=0; i<mCameraProxies.size(); i++)
    {
        mCameraProxies[i].mpCameraProxy->stopRecording(
            mCameraProxies[i].mnCameraChannel,
            m_ID);
    }

    //mInitCamIndex[0] = -1;
    //mInitCamIndex[1] = -1;

  //stop process thread
#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    mpProcessFrameThread->ProcessFrameThreadStop();
#endif
    if(mpPreviewThread)
    {
        mpPreviewThread->stopThread();
    }
    return NO_ERROR;
}

status_t CveBDII::stop()
{
    status_t ret;
    AutoMutex lock(mLock);

    alogd("stop");

    if(BDII_STATE_CONSTRUCTED == mBDIIState)
    {
        return NO_ERROR;
    }
    if(BDII_STATE_STARTED == mBDIIState)
    {
        ret = stop_l();
        mBDIIState = BDII_STATE_CONSTRUCTED;
    }
    if(BDII_STATE_PREPARED == mBDIIState)
    {
        alogw("Be careful! stop in prepared status");
        mBDIIState = BDII_STATE_CONSTRUCTED;
    }
    //cdx_sem_deinit(&mWaitReturnFrameSem);
    return NO_ERROR;
}

status_t CveBDII::setDisplay(int hlay)
{
    if(mpPreviewThread)
    {
        mpPreviewThread->setDisplay(hlay);
    }
    return NO_ERROR;
}

status_t CveBDII::setPreviewRotation(int rotation)
{
    if(mpPreviewThread)
    {
        mpPreviewThread->setPreviewRotation(rotation);
    }
    return NO_ERROR;
}

/*int CveBDII::ifStartReceiveFrame(int nCameraIndex)
{
    AutoMutex lock(mLock);
    if (!((nCameraIndex==mInitCamIndex[0]) || (nCameraIndex==mInitCamIndex[1])))
    {
        if (mInitCamIndex[0] < 0)
        {
            mInitCamIndex[0] = nCameraIndex;
        }
        else if (mInitCamIndex[1] < 0)
        {
            mInitCamIndex[1] = nCameraIndex;
        }
    }

    if (!((mInitCamIndex[0] <0) || (mInitCamIndex[1] < 0)))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}*/

void CveBDII::dataCallbackTimestamp(int nCameraIndex, const VIDEO_FRAME_BUFFER_S *pCameraFrameInfo)
{
    if(NULL == pCameraFrameInfo)
    {
        aloge("fatal error! camera frame is NULL?");
        return;
    }

    //int startReceive = ifStartReceiveFrame(nCameraIndex);
    int startReceive = 1;

    CameraProxyInfo& CameraProxy = mCameraProxies[nCameraIndex];
    if (startReceive)
    {
        CameraProxy.mFrameCounter++;
    }

    //int frameRate = mParameters.getPreviewFrameRate();

#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    if (startReceive)
    {
        //alogd("cam[%d], receiver frame[%d]", nCameraIndex, pCameraFrameInfo->mFrameBuf.mId);
        {
            AutoMutex lock(CameraProxy.mFrameBufListLock);
            if(CameraProxy.mIdleFrameBufList.empty())
            {
                CameraProxy.mIdleFrameBufList.emplace_back();
            }
            if (!CameraProxy.mIdleFrameBufList.empty())
            {
                memcpy(&CameraProxy.mIdleFrameBufList.front(), pCameraFrameInfo, sizeof(VIDEO_FRAME_BUFFER_S));
                CameraProxy.mReadyFrameBufList.splice(CameraProxy.mReadyFrameBufList.end(), CameraProxy.mIdleFrameBufList, CameraProxy.mIdleFrameBufList.begin());
            }
            else
            {
                aloge("fatal error! emplace_back fail?");
            }
        }

        for (std::vector<CameraProxyInfo>::iterator it = mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
        {
            it->mFrameBufListLock.lock();
        }
        int nFramePairNum = -1; //least frame number in cameraProxy.
        for (std::vector<CameraProxyInfo>::iterator it = mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
        {
            if(-1 == nFramePairNum)
            {
                nFramePairNum = it->mReadyFrameBufList.size();
            }
            else
            {
                if(nFramePairNum > (int)it->mReadyFrameBufList.size())
                {
                    nFramePairNum = it->mReadyFrameBufList.size();
                }
            }
            if(0 == nFramePairNum)
            {
                break;
            }
        }
        if(nFramePairNum > 1)
        {
            aloge("fatal error! impossible! max frame pair number is 1! check code!");
        }
        if(nFramePairNum > 0)
        {
            VIDEO_FRAME_BUFFER_S *pFram0 = &mCameraProxies[0].mReadyFrameBufList.front();
            VIDEO_FRAME_BUFFER_S *pFram1 = &mCameraProxies[1].mReadyFrameBufList.front();

            status_t ret = mpProcessFrameThread->sendFrame(pFram0, pFram1);
            if (NO_ERROR == ret)
            {
                mCameraProxies[0].mUsedFrameBufList.splice(mCameraProxies[0].mUsedFrameBufList.end(), mCameraProxies[0].mReadyFrameBufList, mCameraProxies[0].mReadyFrameBufList.begin());
                mCameraProxies[1].mUsedFrameBufList.splice(mCameraProxies[1].mUsedFrameBufList.end(), mCameraProxies[1].mReadyFrameBufList, mCameraProxies[1].mReadyFrameBufList.begin());
            }
            else
            {
                aloge("fatal error! sendFrame can't be fail, check code!");
                mCameraProxies[0].mpCameraProxy->releaseRecordingFrame(mCameraProxies[0].mnCameraChannel, pFram0->mFrameBuf.mId);
                mCameraProxies[1].mpCameraProxy->releaseRecordingFrame(mCameraProxies[1].mnCameraChannel, pFram1->mFrameBuf.mId);
                mCameraProxies[0].mIdleFrameBufList.splice(mCameraProxies[0].mIdleFrameBufList.end(), mCameraProxies[0].mReadyFrameBufList, mCameraProxies[0].mReadyFrameBufList.begin());
                mCameraProxies[1].mIdleFrameBufList.splice(mCameraProxies[1].mIdleFrameBufList.end(), mCameraProxies[1].mReadyFrameBufList, mCameraProxies[1].mReadyFrameBufList.begin());
            }
        }

        for (std::vector<CameraProxyInfo>::iterator it = mCameraProxies.begin(); it!=mCameraProxies.end(); ++it)
        {
            it->mFrameBufListLock.unlock();
        }
    }
    else
#endif
    {
        CameraProxy.mpCameraProxy->releaseRecordingFrame(CameraProxy.mnCameraChannel, pCameraFrameInfo->mFrameBuf.mId);
    }
}

void CveBDII::setDataListener(DataListener *pCb)
{
    mpDataCallback = pCb;
}

void CveBDII::setNotifyListener(NotifyListener *pCb)
{
    mpNotifyCallback = pCb;
}

void CveBDII::enableMessage(unsigned int msg_type)
{
    alogv("msg_type = 0x%x", msg_type);
    AutoMutex lock(&mLock);
    mMessageEnabler |= msg_type;
    alogv("**** Currently enabled messages:");
}

void CveBDII::disableMessage(unsigned int msg_type)
{
    alogv("msg_type = 0x%x", msg_type);

    AutoMutex lock(&mLock);
    mMessageEnabler &= ~msg_type;
    alogv("**** Currently enabled messages:");
}

bool CveBDII::isMessageEnabled(unsigned int msg_type)  //CAMERA_MSG_COMPRESSED_IMAGE
{
    return mMessageEnabler & msg_type;
}

status_t CveBDII::setBDIIConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param)
{
#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    if (mpProcessFrameThread)
    {
        return mpProcessFrameThread->SetBDIIProcessConfigParams(Param);
    }
    return UNKNOWN_ERROR;
#else
    return NO_ERROR;
#endif
}

status_t CveBDII::getBDIIConfigParams(AW_AI_CVE_BDII_INIT_PARA_S &Param)
{
#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    if (mpProcessFrameThread)
    {
        return mpProcessFrameThread->GetBDIIProcessConfigParams(Param);
    }
    return UNKNOWN_ERROR;
#else
    return NO_ERROR;
#endif
}

status_t CveBDII::setDebugYuvDataOut(bool saveOut, std::vector<BDIIDebugFileOutString>&vec)
{
#if (MPPCFG_BDII == OPTION_BDII_ENABLE)
    if (mpProcessFrameThread)
    {
        return mpProcessFrameThread->debugYuvDataOut(saveOut, vec);
    }
    return UNKNOWN_ERROR;
#else
    return NO_ERROR;
#endif
}


}; /* namespace EyeseeLinux */
