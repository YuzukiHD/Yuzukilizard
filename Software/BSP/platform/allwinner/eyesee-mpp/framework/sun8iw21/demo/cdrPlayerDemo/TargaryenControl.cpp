/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : TargaryenControl.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-6
* Description:
********************************************************************************
*/
//#define LOG_NDEBUG 0
#define LOG_TAG "TargaryenControl"
#include <utils/plat_log.h>

#include <stdio.h>
#include <string.h>

#include <hwdisplay.h>
#include <mm_comm_sys.h>
#include <mpi_sys.h>
#include <mpi_vo.h>
#include "TargaryenControl.h"
#include "LannisterViewMessageHandler.h"
#include "StarkModelMessageHandler.h"
#include "TargaryenControlMessageHandler.h"

namespace EyeseeLinux {

TargaryenControl::LannisterViewListener::LannisterViewListener(TargaryenControl *pTCtrl)
    : mpTCtrl(pTCtrl)
{
}
status_t TargaryenControl::LannisterViewListener::onMessage(int msg, void* pMsgData)
{
    status_t ret = NO_ERROR;
    switch(msg)
    {
        case LannisterView::MSGTYPE_SHOW_HELP:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_PLAY_FILE:
        {
            char *pFilePath = (char*)pMsgData;
            int strLength;
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            if(pFilePath)
            {
                strLength = strlen(pFilePath);
                if(strLength > 0)
                {
                    msgIn.setData(pFilePath, strLength + 1);
                }
            }
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_PLAY_FILE_LOOP:
        {
            char *pFilePath = (char*)pMsgData;
            int strLength;
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            if(pFilePath)
            {
                strLength = strlen(pFilePath);
                if(strLength > 0)
                {
                    msgIn.setData(pFilePath, strLength + 1);
                }
            }
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_PLAY_LIST_LOOP:
        {
            char *pFileListPath = (char*)pMsgData;
            int strLength;
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            if(pFileListPath)
            {
                strLength = strlen(pFileListPath);
                if(strLength > 0)
                {
                    msgIn.setData(pFileListPath, strLength + 1);
                }
            }
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_PAUSE:
        case LannisterView::MSGTYPE_RESUME:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_SEEK:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            msgIn.mPara0 = (int)pMsgData;
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_STOP:
        case LannisterView::MSGTYPE_EXIT:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_SETVOLUME:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            msgIn.mPara0 = reinterpret_cast<int>(pMsgData);
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case LannisterView::MSGTYPE_SETMUTE:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfLannisterView;
            msgIn.mMsgType = msg;
            msgIn.mPara0 = reinterpret_cast<int>(pMsgData);
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        default:
        {
            aloge("fatal error! unknown msgType[%d]", msg);
            ret = UNKNOWN_ERROR;
            break;
        }
    }
    return ret;
}

TargaryenControl::StarkModelListener::StarkModelListener(TargaryenControl *pTCtrl)
    : mpTCtrl(pTCtrl)
{
}
status_t TargaryenControl::StarkModelListener::onMessage(int msg, void* pMsgData)
{
    status_t ret = NO_ERROR;
    switch(msg)
    {
        case StarkModel::MSGTYPE_PLAYBACK_COMPLETED:
        case StarkModel::MSGTYPE_ERROR_INVALID_FILEPATH:
        case StarkModel::MSGTYPE_ERROR_PARSE_FILE_LIST_FAIL:
        case StarkModel::MSGTYPE_ERROR_FILELIST_EMPTY:
        case StarkModel::MSGTYPE_ERROR_PREPARE_FAIL:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfStarkModel;
            msgIn.mMsgType = msg;
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        case StarkModel::MSGTYPE_ERROR_MEDIA_ERROR:
        {
            EyeseeMessage msgIn;
            msgIn.mWhoseMsg = MessageOfStarkModel;
            msgIn.mMsgType = msg;
            struct StarkModel::MediaErrorInfo *pMediaErrorInfo = (struct StarkModel::MediaErrorInfo*)pMsgData;
            msgIn.mPara0 = pMediaErrorInfo->mMediaErrorType;
            msgIn.mPara1 = pMediaErrorInfo->mExtra;
            mpTCtrl->mMsgQueue.queueMessage(&msgIn);
            break;
        }
        default:
        {
            aloge("fatal error! unknown msgType[%d]", msg);
            ret = UNKNOWN_ERROR;
            break;
        }
    }
    return ret;
}

TargaryenControl::TargaryenControl()
{
    mState = TargaryenControl::STATE_IDLE;
    mWaitThreadQuitFlag = false;
    mThreadQuitFlag = false;
	mpCommandReceiver = NULL;
    mpPlayer = NULL;
    //config hwdisplay, init mpp.
    {
        MPP_SYS_CONF_S nSysConf;
        memset(&nSysConf, 0, sizeof(MPP_SYS_CONF_S));
        nSysConf.nAlignWidth = 32;
        AW_MPI_SYS_SetConf(&nSysConf);
        AW_MPI_SYS_Init();
        //hw_display_init();
        AW_MPI_VO_Enable(0);

        mUILayer = HLAY(2,0);
        alogd("add outside mUILayer = %d", mUILayer);
        AW_MPI_VO_AddOutsideVideoLayer(mUILayer);
        AW_MPI_VO_CloseVideoLayer(mUILayer);

        //hwd_layer_close(4);
    }
}

TargaryenControl::~TargaryenControl()
{
    reset();

    alogd("remove outside mUILayer = %d", mUILayer);
    ERRORTYPE ret = AW_MPI_VO_RemoveOutsideVideoLayer(mUILayer);
    if(ret != SUCCESS)
    {
        aloge("fatal error! why remove outside layer[%d] fail?", mUILayer);
    }
    
    AW_MPI_VO_Disable(0);
    AW_MPI_SYS_Exit(); 
    //hw_display_deinit();
}

status_t TargaryenControl::init()
{
    size_t i;
    Mutex::Autolock autoLock(mLock);
    if(mState == TargaryenControl::STATE_INITIALIZED)
    {
        alogv("already initialized");
        return NO_ERROR;
    }
    if(mState != TargaryenControl::STATE_IDLE)
    {
        aloge("fatal error! state[%d]!=IDLE", mState);
        return UNKNOWN_ERROR;
    }
    if(mpCommandReceiver!=NULL || mpPlayer!=NULL)
    {
        aloge("fatal error! component[%p][%p]", mpCommandReceiver, mpPlayer);
        return ALREADY_EXISTS;
    }
    
    mpCommandReceiver = new LannisterView;
    mpLannisterViewListener = new LannisterViewListener(this);
    mpCommandReceiver->setOnMessageListener(mpLannisterViewListener);
    mpCommandReceiver->start();
    
    mpPlayer = new StarkModel;
    mpStarkModelListener = new StarkModelListener(this);
    mpPlayer->setOnMessageListener(mpStarkModelListener);
    mpPlayer->init();
    
    BaseMessageHandler *p;
    p = new LannisterViewMessageHandler(this);
    mMessageHandlerArray.push_back(p);
    p = new StarkModelMessageHandler(this);
    mMessageHandlerArray.push_back(p);
    p = new TargaryenControlMessageHandler(this);
    mMessageHandlerArray.push_back(p);
    mState = TargaryenControl::STATE_INITIALIZED;
    return NO_ERROR;
}

status_t TargaryenControl::reset()
{
    Mutex::Autolock autoLock(mLock);
    if(TargaryenControl::STATE_RUNNING == mState)
    {
        alogd("app force exit?");
        mWaitThreadQuitFlag = true;
        EyeseeMessage msgIn;
        msgIn.mWhoseMsg = MessageOfTargaryenControl;
        msgIn.mMsgType = TargaryenControl::MSGTYPE_QUIT_THREAD;
        mMsgQueue.queueMessage(&msgIn);
        while(mWaitThreadQuitFlag)
        {
            mSignalThreadQuit.wait(mLock);
        }
    }
    if(mpCommandReceiver)
    {
        mpCommandReceiver->reset();
        delete mpCommandReceiver;
        mpCommandReceiver = NULL;
    }
    if(mpPlayer)
    {
        mpPlayer->reset();
        delete mpPlayer;
        mpPlayer = NULL;
    }
    size_t i;
    for(i=0; i<mMessageHandlerArray.size(); i++)
    {
        delete mMessageHandlerArray[i];
    }
    mMessageHandlerArray.clear();
    mWaitThreadQuitFlag = false;
    mThreadQuitFlag = false;
    mMsgQueue.flushMessage();
    mState = TargaryenControl::STATE_IDLE;
    return NO_ERROR;
}

status_t TargaryenControl::threadRunning()
{
    {
        Mutex::Autolock autoLock(mLock);
        if(TargaryenControl::STATE_INITIALIZED != mState)
        {
            aloge("fatal error! wrong state[%d]!", mState);
            return UNKNOWN_ERROR;
        }
        mState = TargaryenControl::STATE_RUNNING;
    }
    EyeseeMessage msgOut;
    status_t    ret;
    status_t    threadRet = NO_ERROR;
    size_t  i;
    int     num;
    while(1)
    {
    PROCESS_MESSAGE:
        ret = mMsgQueue.dequeueMessage(&msgOut);
        if(NOT_ENOUGH_DATA == ret)
        {
            mMsgQueue.waitMessage();
            continue;
        }
        if(NO_ERROR != ret)
        {
            aloge("fatal error! ret[0x%x], check message queue!", ret);
            threadRet = UNKNOWN_ERROR;
            break;
        }
        num = 0;
        for(i=0; i<mMessageHandlerArray.size(); i++)
        {
            if(true == mMessageHandlerArray[i]->matchMessageHandler(&msgOut))
            {
                if(num > 0)
                {
                    num++;
                    aloge("fatal error! messageHandler[%d] handle msg[0x%x]of[0x%x], repeated num[%d]!", i, msgOut.mMsgType, msgOut.mWhoseMsg, num);
                    continue;
                }
                ret = mMessageHandlerArray[i]->handleMessage(&msgOut);
                if(ret != NO_ERROR)
                {
                    aloge("fatal error! messageHandler[%d] handle msg[0x%x]of[0x%x] fail[0x%x]!", i, msgOut.mMsgType, msgOut.mWhoseMsg, ret);
                }
                num++;
            }
        }
        {
            Mutex::Autolock autoLock(mLock);
            if(mThreadQuitFlag)
            {
                mThreadQuitFlag = false;
                goto _exit0;
            }
        }
    }
_exit0:
    {
        Mutex::Autolock autoLock(mLock);
        mState = TargaryenControl::STATE_INITIALIZED;
        if(mWaitThreadQuitFlag)
        {
            mWaitThreadQuitFlag = false;
            mSignalThreadQuit.signal();
        }
    }
    alogd("TargaryenControl thread quit[0x%x]", threadRet);
    return threadRet;
}

};
