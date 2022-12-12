/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VideoEnc_Component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/09
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "VideoEnc_Component"
#include <utils/plat_log.h>

//ref platform headers
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include <assert.h>
#include <sched.h>
#include <pthread.h>
#include <sys/syscall.h>

#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include <plat_systrace.h>

#include "cdx_list.h"

//media api headers to app
#include "SystemBase.h"
#include "mm_common.h"
#include "mm_comm_video.h"
#include "mm_comm_venc.h"
#include "mpi_venc.h"

//media internal common headers.
#include "media_common.h"
#include <media_common_vcodec.h>
#include "mm_component.h"
#include "ComponentCommon.h"
#include "tmessage.h"
#include "tsemaphore.h"
#include <vencoder.h>
#include <memoryAdapter.h>
#include <veAdapter.h>
#include "VideoEnc_Component.h"
#include <VencCompStream.h>
#include <EncodedStream.h>
#include <VideoFrameInfoNode.h>
#include <VIDEO_FRAME_INFO_S.h>
#include <mm_comm_venc_op.h>
#include "cedarx_demux.h"
#include <sunxi_camera_v2.h>
#include <media_debug.h>

//------------------------------------------------------------------------------------

//#define VIDEO_ENC_TIME_DEBUG
//#define STORE_TIMEOUT_VIDEO_FRAME
//#define ENABLE_ENCODE_STATISTICS

//#define VENC_IGNORE_ALL_FRAME

//#define CHECK_VIDEO_ENCODE_OUT_BITRATE

//#define STORE_VENC_IN_FRAME
//#define STORE_VENC_OUT_STREAM
//#define STORE_VENC_CH_ID      (1)


#ifdef STORE_VENC_IN_FRAME
#define STORE_VENC_FRAME_FILE "/mnt/extsd/venc_frame.yuv"
#define STORE_VENC_FRAME_TOTAL_CNT  (100)
static FILE* g_debug_fp_frame = NULL;
static unsigned int g_debug_frame_cnt = 0;
#endif
#ifdef STORE_VENC_OUT_STREAM
#define STORE_VENC_STREAM_FILE "/mnt/extsd/venc_stream.raw"
static FILE* g_debug_fp_stream = NULL;
static unsigned int g_debug_stream_cnt = 0;
#endif


static void VideoEncSendBackInputFrame(VIDEOENCDATATYPE *pVideoEncData, VIDEO_FRAME_INFO_S *pFrameInfo);
static void* ComponentThread(void* pThreadData);

static int VencCompEventHandler (
        VideoEncoder* pEncoder,
        void* pAppData,
        VencEventType eEvent,
        unsigned int nData1,
        unsigned int nData2,
        void* pEventData)
{
	VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *)pAppData;

    switch(eEvent)
    {
        case VencEvent_UpdateMbModeInfo:
        {
            alogv("VencEvent_UpdateMbModeInfo got !!!!!!!");
            /**
             * The encoding has just been completed, but the encoding of the next frame has not yet started.
             * Before encoding, set the encoding method of the frame to be encoded.
             * The user can modify the encoding parameters before setting.
             */
            pVideoEncData->pCallbacks->EventHandler(
                                    pVideoEncData->hSelf, pVideoEncData->pAppData,
                                    COMP_EventQpMapUpdateMBModeInfo,
                                    0,
                                    0,
                                    &pVideoEncData->mMBModeCtrl);

            if ((0 == pVideoEncData->mMBModeCtrl.mode_ctrl_en) || (NULL == pVideoEncData->mMBModeCtrl.p_map_info))
            {
                aloge("fatal error! Venc ModeCtrl: mode_ctrl_en=%d, p_map_info=%p",
                    pVideoEncData->mMBModeCtrl.mode_ctrl_en, pVideoEncData->mMBModeCtrl.p_map_info);
            }

            if (pVideoEncData->mMBModeCtrl.mode_ctrl_en == 1)
            {
                /**
                 * Notice: Only for P-frame, I-frame is NOT support.
                 * The encoding library only sets the QPMAP parameter for the P-frame,
                 * if the current frame is an I-frame, the QPMAP setting is ignored.
                 */
                VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamMBModeCtrl, &pVideoEncData->mMBModeCtrl);
            }

            break;
        }
        case VencEvent_UpdateMbStatInfo:
        {
            alogv("VencEvent_UpdateMbStatInfo got !!!!!!!");
            /**
             * The encoding has just been completed, but the encoding of the next frame has not yet started.
             * and the QpMap information of the frame that has just been encoded is obtained.
             * GetMbSumInfo(pContext->pVideoEnc, &pVideoEncData->mMbSumInfo);
             */
            VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamMBSumInfoOutput, &pVideoEncData->mMbSumInfo);

            VencMBSumInfo *pMbSumInfo = &pVideoEncData->mMbSumInfo;
            if ((0 == pMbSumInfo->avg_qp) || (NULL == pMbSumInfo->p_mb_mad_qp_sse) || (NULL == pMbSumInfo->p_mb_bin_img) || (NULL == pMbSumInfo->p_mb_mv))
            {
                aloge("fatal error! Venc MBSumInfo: avg_mad=%d, avg_sse=%d, avg_qp=%d, avg_psnr=%lf, "
                    "p_mb_mad_qp_sse=%p, p_mb_bin_img=%p, p_mb_mv=%p",
                    pMbSumInfo->avg_mad, pMbSumInfo->avg_sse, pMbSumInfo->avg_qp, pMbSumInfo->avg_psnr,
                    pMbSumInfo->p_mb_mad_qp_sse, pMbSumInfo->p_mb_bin_img, pMbSumInfo->p_mb_mv);
            }

            pVideoEncData->pCallbacks->EventHandler(
                                    pVideoEncData->hSelf, pVideoEncData->pAppData,
                                    COMP_EventQpMapUpdateMBStatInfo,
                                    0,
                                    0,
                                    &pVideoEncData->mMbSumInfo);
            break;
        }
        case VencEvent_UpdateIspToVeParam:
        {
            VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;
            if (pIsp2VeParam)
            {
                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf, pVideoEncData->pAppData,
                                        COMP_EventLinkageIsp2VeParam,
                                        0,
                                        0,
                                        (void*)pIsp2VeParam);
            }
            break;
        }
        case VencEvent_UpdateVeToIspParam:
        {
            VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)pEventData;
            if (pVe2IspParam)
            {
                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf, pVideoEncData->pAppData,
                                        COMP_EventLinkageVe2IspParam,
                                        0,
                                        0,
                                        (void*)pVe2IspParam);
            }
            break;
        }
        case VencEvent_UpdateIspMotionParam:
        {
            // TODO
            break;
        }
        default:
            alogv("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
	return 0;
}

static int VencCompInputBufferDone (
        VideoEncoder* pEncoder,
        void* pAppData,
        VencCbInputBufferDoneInfo* pBufferDoneInfo)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *)pAppData;
    (void)pEncoder;

    if ((NULL == pVideoEncData) || (NULL == pBufferDoneInfo))
    {
        aloge("fatal error! invalid input params, %p, %p", pVideoEncData, pBufferDoneInfo);
        return -1;
    }

    alogv("VencChn[%d] CompInputBufferDone got !!!!!!! mFlagOutputStream=%d", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mFlagOutputStream);
    pVideoEncData->mDbgEncodeCnt++;

    if (0 == pVideoEncData->mOnlineEnableFlag)
    {
        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
        if (!list_empty(&pVideoEncData->mBufQ.mUsingFrameList))
        {
            VideoFrameInfoNode *pFrameNode = list_first_entry(&pVideoEncData->mBufQ.mUsingFrameList, VideoFrameInfoNode, mList);
            VencInputBuffer *pInputBuffer = pBufferDoneInfo->pInputBuffer;
            if (NULL == pInputBuffer)
            {
                aloge("fatal error! VencChn[%d] invalid input params, %p", pVideoEncData->mMppChnInfo.mChnId, pInputBuffer);
                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                return -1;
            }
            if (pInputBuffer->nID != pFrameNode->VFrame.mId)
            {
                aloge("fatal error! VencChn[%d] input buf ID is NOT match, nID: %d, mId: %d", pVideoEncData->mMppChnInfo.mChnId, pInputBuffer->nID, pFrameNode->VFrame.mId);
            }

            // save the venc result
            pFrameNode->mResult = pBufferDoneInfo->nResult;
            alogv("VencChn[%d] inputBuffer ID[%d], venc lib result[%d]", pVideoEncData->mMppChnInfo.mChnId, pInputBuffer->nID, pBufferDoneInfo->nResult);

            list_move_tail(&pFrameNode->mList, &pVideoEncData->mBufQ.mUsedFrameList);
            alogv("input orig frame ID[%d] list: Using -> Used", pFrameNode->VFrame.mId);

            if(pVideoEncData->mbEnableMotionSearch)
            {
                if (!(pInputBuffer->nFlag & VENC_BUFFERFLAG_KEYFRAME))
                {
                    VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamMotionSearchResult, (void*)&pVideoEncData->mMotionResult);
                }
            }
            if (FALSE == pVideoEncData->mbForbidDiscardingFrame)
            {
                if (pVideoEncData->mWaitInputFrameUsingEmptyFlag)
                {
                    pthread_cond_signal(&pVideoEncData->mInputFrameUsingEmptyCondition);
                    alogd("VencChn[%d] send cond signal inputFrameUsingEmptyCondition", pVideoEncData->mMppChnInfo.mChnId);
                }
            }
        }
        else
        {
            aloge("fatal error! VencChn[%d] Almost impossible, why input using frame list is empty but got venc empty buf done cb, "
                "please check code, nID: %d", pVideoEncData->mMppChnInfo.mChnId, pBufferDoneInfo->pInputBuffer->nID);
        }
        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
    }
    else
    {
        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
        if (list_empty(&pVideoEncData->mBufQ.mIdleFrameList))
        {
            if (VENC_RESULT_DROP_FRAME != pBufferDoneInfo->nResult)
            {
                alogv("VencChn[%d] idle frame is empty, malloc one! result[%d]", pVideoEncData->mMppChnInfo.mChnId, pBufferDoneInfo->nResult);
            }
            if (pVideoEncData->mBufQ.buf_unused!=0)
            {
                aloge("fatal error! VencChn[%d] buf_unused must be zero!", pVideoEncData->mMppChnInfo.mChnId);
            }
            VideoFrameInfoNode *pNode = (VideoFrameInfoNode*)malloc(sizeof(VideoFrameInfoNode));
            if (NULL == pNode)
            {
                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                aloge("fatal error! VencChn[%d] malloc fail!", pVideoEncData->mMppChnInfo.mChnId);
                return ERR_VENC_NOMEM;
            }
            list_add_tail(&pNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
            pVideoEncData->mBufQ.buf_unused++;
        }
        VideoFrameInfoNode *pFrameNode = list_first_entry(&pVideoEncData->mBufQ.mIdleFrameList, VideoFrameInfoNode, mList);

        pFrameNode->mResult = pBufferDoneInfo->nResult;
        alogv("VFrame ID[%d], venc lib result[%d]", pFrameNode->VFrame.mId, pBufferDoneInfo->nResult);
        pVideoEncData->mBufQ.buf_unused--;
        list_move_tail(&pFrameNode->mList, &pVideoEncData->mBufQ.mUsedFrameList);
        alogv("input orig frame ID[%d] list: Using -> Used", pFrameNode->VFrame.mId);

        VencInputBuffer *pInputBuffer = pBufferDoneInfo->pInputBuffer;
        if (NULL == pInputBuffer)
        {
            aloge("fatal error! VencChn[%d] invalid input params, %p", pVideoEncData->mMppChnInfo.mChnId, pInputBuffer);
            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
            return -1;
        }
        if (pVideoEncData->mbEnableMotionSearch)
        {
            if (!(pInputBuffer->nFlag & VENC_BUFFERFLAG_KEYFRAME))
            {
                VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamMotionSearchResult, (void*)&pVideoEncData->mMotionResult);
            }
        }
        if (FALSE == pVideoEncData->mbForbidDiscardingFrame)
        {
            if (pVideoEncData->mWaitInputFrameUsingEmptyFlag)
            {
                pthread_cond_signal(&pVideoEncData->mInputFrameUsingEmptyCondition);
                alogd("send cond signal inputFrameUsingEmptyCondition");
            }
        }
        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
    }

    if (TRUE == pVideoEncData->mbForbidDiscardingFrame)
    {
        cdx_sem_up_unique(&pVideoEncData->mInputFrameBufDoneSem);
        alogv("send sem inputFrameBufDoneSem");
    }
    else
    {
        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        if (0 == pVideoEncData->mFlagOutputStream)
        {
            pVideoEncData->mFlagOutputStream = 1;
            message_t msg;
            msg.command = VEncComp_OutputStreamAvailable;
            put_message(&pVideoEncData->cmd_queue, &msg);
            alogv("VencChn[%d] send message VEncComp OutputStreamAvailable", pVideoEncData->mMppChnInfo.mChnId);
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    }

    return 0;
}

static int SoftFrameRateCtrlDestroy(SoftFrameRateCtrl *pThiz)
{
    pThiz->enable = FALSE;
    pThiz->mBasePts = -1;
    pThiz->mCurrentWantedPts = -1;
    pThiz->mFrameCounter = 0;
    return 0;
}

static VIDEOENCBUFFERMANAGER *VideoEncBufferInit(void)
{
    VIDEOENCBUFFERMANAGER *manager;

    manager = (VIDEOENCBUFFERMANAGER*)malloc(sizeof(VIDEOENCBUFFERMANAGER));
    if (NULL == manager)
    {
        aloge("Failed to alloc VIDEOENCBUFFERMANAGER(%s)", strerror(errno));
        return NULL;
    }
    memset(manager, 0, sizeof(VIDEOENCBUFFERMANAGER));

    manager->buffer = (unsigned char*)malloc(COMPRESSED_SRC_ENC_BUF_LEN * sizeof(unsigned char));
    if (NULL == manager->buffer)
    {
        aloge("Failed to alloc buffer(%s)", strerror(errno));
        free(manager);
        return NULL;
    }
    pthread_mutex_init(&manager->lock ,NULL);

    return manager;
}

static void VideoEncBufferDeInit(VIDEOENCBUFFERMANAGER *manager)
{
    if (NULL == manager)
    {
        return;
    }
    pthread_mutex_destroy(&manager->lock);
    free(manager->buffer);
    free(manager);
}

static ERRORTYPE VideoEncBufferPushFrame(VIDEOENCBUFFERMANAGER *manager, FRAMEDATATYPE *frame)
{
    ERRORTYPE eError = SUCCESS;
    unsigned int length;
    unsigned int flag = FRAME_BEGIN_FLAG;
    unsigned char  *ptr, *ptr_ori;

    if (manager == NULL)
    {
        aloge("Buffer manager is NULL!");
        return ERR_VENC_NULL_PTR;
    }

    length = frame->info.size + sizeof(FRAMEINFOTYPE) + sizeof(unsigned int);
    pthread_mutex_lock(&manager->lock);
    if (manager->writePos > manager->readPos)
    {
        if (manager->writePos + length <= COMPRESSED_SRC_ENC_BUF_LEN)
        {
            ptr_ori = ptr = manager->buffer + manager->writePos;
            memcpy(ptr, &flag, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            memcpy(ptr, &frame->info, sizeof(FRAMEINFOTYPE));
            ptr += sizeof(FRAMEINFOTYPE);
            memcpy(ptr, (void*)frame->addrY, frame->info.size);
            ptr += frame->info.size;
            manager->writePos += (ptr - ptr_ori);
            manager->count++;
            manager->mUnprefetchFrameNum++;
            if (manager->writePos + sizeof(unsigned int) <= COMPRESSED_SRC_ENC_BUF_LEN)
            {
                memset(ptr, 0, sizeof(unsigned int));
            }
        }
        else
        {
            if (length <= manager->readPos)
            {
                ptr_ori = ptr = manager->buffer;
                memcpy(ptr, &flag, sizeof(unsigned int));
                ptr += sizeof(unsigned int);
                memcpy(ptr, &frame->info, sizeof(FRAMEINFOTYPE));
                ptr += sizeof(FRAMEINFOTYPE);
                memcpy(ptr, (void*)frame->addrY, frame->info.size);
                ptr += frame->info.size;
                manager->writePos = (ptr - ptr_ori);
                manager->count++;
                manager->mUnprefetchFrameNum++;
            }
            else
            {
                alogw("Buffer full, %d frames, writePos=%d, readPos=%d, frame_size=%d!",
                    manager->count, manager->writePos, manager->readPos, frame->info.size);
                eError = ERR_VENC_BUF_FULL;
            }
        }
    }
    else if (manager->writePos < manager->readPos)
    {
        if (manager->readPos - manager->writePos >= length)
        {
            ptr_ori = ptr = manager->buffer + manager->writePos;
            memcpy(ptr, &flag, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            memcpy(ptr, &frame->info, sizeof(FRAMEINFOTYPE));
            ptr += sizeof(FRAMEINFOTYPE);
            memcpy(ptr, (void*)frame->addrY, frame->info.size);
            ptr += frame->info.size;
            manager->writePos += (ptr - ptr_ori);
            manager->count++;
            manager->mUnprefetchFrameNum++;
        }
        else
        {
            alogw("Buffer full, %d frames, writePos=%d, readPos=%d, frame_size=%d!",
                manager->count, manager->writePos, manager->readPos, frame->info.size);
            eError = ERR_VENC_BUF_FULL;
        }
    }
    else
    {
        if (manager->count == 0)
        {
            if(manager->mUnprefetchFrameNum != 0)
            {
                aloge("fatal error! unprefetchNum[%d]!=0", manager->mUnprefetchFrameNum);
            }
            manager->readPos = manager->prefetchPos = 0;
            ptr_ori = ptr = manager->buffer;
            memcpy(ptr, &flag, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            memcpy(ptr, &frame->info, sizeof(FRAMEINFOTYPE));
            ptr += sizeof(FRAMEINFOTYPE);
            memcpy(ptr, (void*)frame->addrY, frame->info.size);
            ptr += frame->info.size;
            manager->writePos = (ptr - ptr_ori);
            manager->count++;
            manager->mUnprefetchFrameNum++;
        }
        else
        {
            alogw("Buffer full, %d frames, writePos=%d, readPos=%d, frame_size=%d!",
                manager->count, manager->writePos, manager->readPos, frame->info.size);
            eError = ERR_VENC_BUF_FULL;
        }
    }
    pthread_mutex_unlock(&manager->lock);
    return eError;
}

static ERRORTYPE VideoEncBufferGetFrame(VIDEOENCBUFFERMANAGER *manager, FRAMEDATATYPE *frame)
{
    ERRORTYPE eError = SUCCESS;

    if (manager == NULL)
    {
        aloge("Buffer manager is NULL!");
        return ERR_VENC_ILLEGAL_PARAM;
    }

    pthread_mutex_lock(&manager->lock);
    if (manager->mUnprefetchFrameNum > 0)
    {
        unsigned int flag;
        unsigned char  *ptr;

        if (manager->prefetchPos + sizeof(unsigned int) < COMPRESSED_SRC_ENC_BUF_LEN)
        {
            memcpy(&flag, manager->buffer+manager->prefetchPos, sizeof(unsigned int));
            if (flag == FRAME_BEGIN_FLAG)
            {
                ptr = manager->buffer + manager->prefetchPos;
            }
            else
            {
                ptr = manager->buffer;
            }
        }
        else
        {
            ptr = manager->buffer;
        }
        ptr += sizeof(unsigned int);
        memcpy(&frame->info, ptr, sizeof(FRAMEINFOTYPE));
        ptr += sizeof(FRAMEINFOTYPE);
        frame->addrY = (char*)ptr;
        manager->mUnprefetchFrameNum--;
        ptr += frame->info.size;
        manager->prefetchPos = ptr - manager->buffer;
    }
    else
    {
        //alogw("Buffer empty!");
        eError = ERR_VENC_BUF_EMPTY;
    }
    pthread_mutex_unlock(&manager->lock);
    return eError;
}

static ERRORTYPE VideoEncBufferReleaseFrame(VIDEOENCBUFFERMANAGER *manager, FRAMEDATATYPE *releaseFrame)
{
    FRAMEDATATYPE frame;
    ERRORTYPE eError = SUCCESS;

    if (manager == NULL)
    {
        aloge("Buffer manager is NULL!");
        return ERR_VENC_ILLEGAL_PARAM;
    }

    pthread_mutex_lock(&manager->lock);
    if (manager->count > 0)
    {
        unsigned int flag;
        if (manager->readPos + sizeof(unsigned int) < COMPRESSED_SRC_ENC_BUF_LEN)
        {
            memcpy(&flag, manager->buffer+manager->readPos, sizeof(unsigned int));
            if (flag != FRAME_BEGIN_FLAG)
            {
                manager->readPos = 0;
                memcpy(&flag, manager->buffer+manager->readPos, sizeof(unsigned int));
            }
        }
        else
        {
            manager->readPos = 0;
            memcpy(&flag, manager->buffer+manager->readPos, sizeof(unsigned int));
        }
        if(flag != FRAME_BEGIN_FLAG)
        {
            aloge("fatal error! data header flag[0x%x] wrong", flag);
        }
        manager->readPos += sizeof(unsigned int);
        memcpy(&frame.info, manager->buffer + manager->readPos, sizeof(FRAMEINFOTYPE));
        frame.addrY = (char*)manager->buffer + manager->readPos + sizeof(FRAMEINFOTYPE);
        if(releaseFrame->addrY!=frame.addrY
            || releaseFrame->info.timeStamp!=frame.info.timeStamp
            || releaseFrame->info.bufferId!=frame.info.bufferId
            || releaseFrame->info.size!=frame.info.size)
        {
            aloge("fatal error! frame not match!addrY[%p]timeStamp[%lld]bufferId[%d]size[%d]!=[%p][%lld][%d][%d]", 
                releaseFrame->addrY, releaseFrame->info.timeStamp, releaseFrame->info.bufferId, releaseFrame->info.size,
                frame.addrY, frame.info.timeStamp, frame.info.bufferId, frame.info.size);
        }
        manager->readPos += sizeof(FRAMEINFOTYPE) + frame.info.size;
        manager->count--;
    }
    else
    {
        //alogw("Buffer empty!");
        aloge("fatal error! count==0, check code!");
        eError = ERR_VENC_BUF_EMPTY;
    }
    pthread_mutex_unlock(&manager->lock);
    return eError;
}

static ERRORTYPE configVencSuperFrameConfigByVENC_SUPERFRAME_CFG_S(VencSuperFrameConfig *pDst, VENC_SUPERFRAME_CFG_S* pSrc)
{
    memset(pDst, 0, sizeof(VencSuperFrameConfig));
    switch(pSrc->enSuperFrmMode)
    {
        case SUPERFRM_NONE:
            pDst->eSuperFrameMode = VENC_SUPERFRAME_NONE;
            break;
        case SUPERFRM_DISCARD:
            pDst->eSuperFrameMode = VENC_SUPERFRAME_DISCARD;
            break;
        case SUPERFRM_REENCODE:
            pDst->eSuperFrameMode = VENC_SUPERFRAME_REENCODE;
            break;
        default:
            aloge("fatal error! wrong superFrmMode[0x%x]", pSrc->enSuperFrmMode);
            pDst->eSuperFrameMode = VENC_SUPERFRAME_NONE;
            break;
    }
    pDst->nMaxIFrameBits = pSrc->SuperIFrmBitsThr;
    pDst->nMaxPFrameBits = pSrc->SuperPFrmBitsThr;
    return SUCCESS;
}

/*******************************************************************************
Function name: estimateBitRate
Description:
    only estimate fixqp mode bitRate
Parameters:

Return:
    Kbit/s
Time: 2017/6/15
*******************************************************************************/
static int estimateBitRate(VENC_CHN_ATTR_S *pVEncChnAttr)
{
    int bitRateKb = 24*1024*1024 >> 10;
    if(VENC_RC_MODE_H264FIXQP == pVEncChnAttr->RcAttr.mRcMode)
    {
        int nArea = pVEncChnAttr->VeAttr.AttrH264e.PicWidth * pVEncChnAttr->VeAttr.AttrH264e.PicHeight;
        if(nArea >= 3840*2160)
        {
            bitRateKb = 20*1024;
        }
        else if(nArea >= 1920*1080)
        {
            bitRateKb = 12*1024;
        }
        else if(nArea >= 1280*720)
        {
            bitRateKb = 8*1024;
        }
        else
        {
            bitRateKb = 4*1024;
        }
    }
    else if(VENC_RC_MODE_H265FIXQP == pVEncChnAttr->RcAttr.mRcMode)
    {
        int nArea = pVEncChnAttr->VeAttr.AttrH265e.mPicWidth * pVEncChnAttr->VeAttr.AttrH265e.mPicHeight;
        if(nArea >= 3840*2160)
        {
            bitRateKb = 10*1024;
        }
        else if(nArea >= 1920*1080)
        {
            bitRateKb = 6*1024;
        }
        else if(nArea >= 1280*720)
        {
            bitRateKb = 4*1024;
        }
        else
        {
            bitRateKb = 2*1024;
        }
    }
    else if(VENC_RC_MODE_MJPEGFIXQP == pVEncChnAttr->RcAttr.mRcMode)
    {
        int nArea = pVEncChnAttr->VeAttr.AttrMjpeg.mPicWidth * pVEncChnAttr->VeAttr.AttrMjpeg.mPicHeight;
        if(nArea >= 3840*2160)
        {
            bitRateKb = 40*1024;
        }
        else if(nArea >= 1920*1080)
        {
            bitRateKb = 24*1024;
        }
        else if(nArea >= 1280*720)
        {
            bitRateKb = 16*1024;
        }
        else
        {
            bitRateKb = 8*1024;
        }
    }
    else
    {
        aloge("fatal error! unsupport rc mode[0x%x], use default[%d]Kbit/s", pVEncChnAttr->RcAttr.mRcMode, bitRateKb);
    }
    return bitRateKb;
}

static int setVbvBufferConfig(VideoEncoder *pCedarV, VIDEOENCDATATYPE *pVideoEncData)
{
    #define VEValidSizeScope (0xfff0000/8)
    int bitRateKb;
    int nCacheSize;
    int vbvSize;
    int nMinSize;
    int nThreshSize;
    int nUsrStreamBufSize = 0;
    int nUsrThreshSize = 0;
    if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        nMinSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight*3/2;
        nThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight;
        nUsrStreamBufSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize;
        nUsrThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize;
    }
    else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        nMinSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight*3/2;
        nThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight;
        nUsrStreamBufSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mBufSize;
        nUsrThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mThreshSize;
    }
    else if(PT_MJPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        nMinSize = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight*3/2;
        nThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight;
        nUsrStreamBufSize = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mBufSize;
        nUsrThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mThreshSize;
    }
    else
    {
        alogw("unsupported venc type:0x%x, calculate minSize!", pVideoEncData->mEncChnAttr.VeAttr.Type);
        nMinSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight*3/2;
        nThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth*pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight;
        nUsrStreamBufSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize;
        nUsrThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize;
    }
    if(nThreshSize > 7*1024*1024)
    {
        alogw("Be careful! threshSize[%d]bytes too large, reduce to 7MB", nThreshSize);
        nThreshSize = 7*1024*1024;

    }
    double nBufferSeconds = 4;    //unit:s.
    //int ret;

    if (pVideoEncData->mVbvBufTime > 0)
    {
        nBufferSeconds = pVideoEncData->mVbvBufTime;
        alogd("set vbv buffer to %lfs", nBufferSeconds);
    }

    unsigned int nBitRate;
    nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
    if(nBitRate > 0)
    {
        bitRateKb = nBitRate>>10;
    }
    else
    {
        alogd("bitRate=0, maybe rc mode[0x%x] is fixqp", pVideoEncData->mEncChnAttr.RcAttr.mRcMode);
        bitRateKb = estimateBitRate(&pVideoEncData->mEncChnAttr);
    }
    nCacheSize = (bitRateKb*nBufferSeconds)*(1024/8);
    vbvSize = nCacheSize + nThreshSize;

    if (vbvSize < nMinSize)
    {
        vbvSize = nMinSize;
    }
    if(nUsrStreamBufSize > 0)
    {
        alogd("user set stream buffer size[%d]bytes", nUsrStreamBufSize);
        vbvSize = nUsrStreamBufSize;
    }
    if(vbvSize%1024 != 0)
    {
        vbvSize = ALIGN(vbvSize, 1024);
    }

    if(nUsrThreshSize > 0)
    {
        alogd("user set threshSize[%d]bytes", nUsrThreshSize);
        nThreshSize = nUsrThreshSize;
    }
    #if (AWCHIP == AW_V5)
    if(vbvSize + nThreshSize > VEValidSizeScope)
    {
        alogw("Be careful! vbvSize[%d] + threshSize[%d] > [%d]", vbvSize, nThreshSize, VEValidSizeScope);
        vbvSize = VEValidSizeScope - nThreshSize;
        if(vbvSize%1024 != 0)
        {
            vbvSize = (vbvSize/1024)*1024;
        }
        alogw("Be careful! decrease vbvSize to [%d]", vbvSize);
    }
    #elif (AWCHIP == AW_V316)
    #else
    #endif
    if(vbvSize <= nThreshSize)
    {
        aloge("fatal error! vbvSize[%d] <= nThreshSize[%d]", vbvSize, nThreshSize);
    }

//    if (vbvSize > 24*1024*1024) 
//    {
//        alogw("Be careful! vbvSize[%d] too large, exceed 24M byte", vbvSize);
//        vbvSize = 24*1024*1024;
//    }
    alogd("bit rate is %dKb, set encode vbv size %d, frame length threshold %d", bitRateKb, vbvSize, nThreshSize);
    VencSetParameter(pCedarV, VENC_IndexParamSetVbvSize, &vbvSize);
    VencSetParameter(pCedarV, VENC_IndexParamSetFrameLenThreshold, &nThreshSize);
    return 0;
}

/**
 * VENC_ATTR_H264_S.mProfile : 0: baseline; 1:MP; 2:HP; 3: SVC-T [0,3];
 * VENC_H264PROFILETYPE:
 *
 */
VENC_H264PROFILETYPE map_VENC_ATTR_H264_S_Profile_to_VENC_H264PROFILETYPE(unsigned int nProfile)
{
    VENC_H264PROFILETYPE vencProfileType = VENC_H264ProfileMain;
    switch(nProfile)
    {
        case 0:
            vencProfileType = VENC_H264ProfileBaseline;
            break;
        case 1:
            vencProfileType = VENC_H264ProfileMain;
            break;
        case 2:
            vencProfileType = VENC_H264ProfileHigh;
            break;
        case 3:
            aloge("fatal error! not support SVC-T!");
            break;
        default:
            aloge("fatal error! unknown h264 profile");
            break;
    }
    return vencProfileType;
}

VENC_H264LEVELTYPE map_H264_LEVEL_E_to_VENC_H264LEVELTYPE(H264_LEVEL_E nLevel)
{
    VENC_H264LEVELTYPE nVencLevel;
    switch(nLevel)
    {
        case H264_LEVEL_1:
            nVencLevel = VENC_H264Level1;
            break;
        case H264_LEVEL_11:
            nVencLevel = VENC_H264Level11;
            break;
        case H264_LEVEL_12:
            nVencLevel = VENC_H264Level12;
            break;
        case H264_LEVEL_13:
            nVencLevel = VENC_H264Level13;
            break;
        case H264_LEVEL_2:
            nVencLevel = VENC_H264Level2;
            break;
        case H264_LEVEL_21:
            nVencLevel = VENC_H264Level21;
            break;
        case H264_LEVEL_22:
            nVencLevel = VENC_H264Level22;
            break;
        case H264_LEVEL_3:
            nVencLevel = VENC_H264Level3;
            break;
        case H264_LEVEL_31:
            nVencLevel = VENC_H264Level31;
            break;
        case H264_LEVEL_32:
            nVencLevel = VENC_H264Level32;
            break;
        case H264_LEVEL_4:
            nVencLevel = VENC_H264Level4;
            break;
        case H264_LEVEL_41:
            nVencLevel = VENC_H264Level41;
            break;
        case H264_LEVEL_42:
            nVencLevel = VENC_H264Level42;
            break;
        case H264_LEVEL_5:
            nVencLevel = VENC_H264Level5;
            break;
        case H264_LEVEL_51:
            nVencLevel = VENC_H264Level51;
            break;
        default:
            nVencLevel = VENC_H264LevelDefault;
            break;
    }
    return nVencLevel;
}

VENC_H265PROFILETYPE map_VENC_ATTR_H265_S_Profile_to_VENC_H265PROFILETYPE(unsigned int nProfile)
{
    VENC_H265PROFILETYPE vencProfileType = VENC_H265ProfileMain;
    switch(nProfile)
    {
        case 0:
            vencProfileType = VENC_H265ProfileMain;
            break;
        case 1:
            vencProfileType = VENC_H265ProfileMain10;
            break;
        case 2:
            vencProfileType = VENC_H265ProfileMainStill;
            break;
        default:
            aloge("fatal error! unknown h265 profile");
            break;
    }
    return vencProfileType;
}

VENC_H265LEVELTYPE map_H265_LEVEL_E_to_VENC_H265LEVELTYPE(H265_LEVEL_E nLevel)
{
    VENC_H265LEVELTYPE nVencLevel;
    switch(nLevel)
    {
        case H265_LEVEL_1:
            nVencLevel = VENC_H265Level1;
            break;
        case H265_LEVEL_2:
            nVencLevel = VENC_H265Level2;
            break;
        case H265_LEVEL_21:
            nVencLevel = VENC_H265Level21;
            break;
        case H265_LEVEL_3:
            nVencLevel = VENC_H265Level3;
            break;
        case H265_LEVEL_31:
            nVencLevel = VENC_H265Level31;
            break;
        case H265_LEVEL_41:
            nVencLevel = VENC_H265Level41;
            break;
        case H265_LEVEL_5:
            nVencLevel = VENC_H265Level5;
            break;
        case H265_LEVEL_51:
            nVencLevel = VENC_H265Level51;
            break;
        case H265_LEVEL_52:
            nVencLevel = VENC_H265Level52;
            break;
        case H265_LEVEL_6:
            nVencLevel = VENC_H265Level6;
            break;
        case H265_LEVEL_61:
            nVencLevel = VENC_H265Level61;
            break;
        case H265_LEVEL_62:
            nVencLevel = VENC_H265Level62;
            break;
        default:
            nVencLevel = VENC_H265LevelDefault;
            break;
    }
    return nVencLevel;
}

VENC_PIXEL_FMT map_PIXEL_FORMAT_E_to_VENC_PIXEL_FMT(PIXEL_FORMAT_E pixelFormat)
{
    VENC_PIXEL_FMT vencPixelFormat = VENC_PIXEL_YVU420SP;
    switch(pixelFormat)
    {
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_AW_NV21M:
            vencPixelFormat = VENC_PIXEL_YVU420SP;
            break;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
            vencPixelFormat = VENC_PIXEL_YUV420SP;
            break;
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
            vencPixelFormat = VENC_PIXEL_YVU420P;
            break;
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
            vencPixelFormat = VENC_PIXEL_YUV420P;
            break;
        case MM_PIXEL_FORMAT_YUYV_PACKAGE_422:
            vencPixelFormat = VENC_PIXEL_YUYV422;
            break;
        case MM_PIXEL_FORMAT_YUV_AW_AFBC: //all winner private yuv format
            vencPixelFormat = VENC_PIXEL_AFBC_AW;
            break;
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X: //all winner private yuv format
            vencPixelFormat = VENC_PIXEL_LBC_AW;
            break;
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422:
            vencPixelFormat = VENC_PIXEL_YVU422SP;
            break;
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422:
            vencPixelFormat = VENC_PIXEL_YUV422SP;
            break;
        default:
            aloge("fatal error! unsupported temporary! pixelForamt[0x%x]", pixelFormat);
            break;
    }
    return vencPixelFormat;
}

static inline int map_ROTATE_E_to_VENC_Rotate_Angle(ROTATE_E rotate)
{
    int angle = 0;
    switch(rotate)
    {
        case ROTATE_NONE:
            angle = 0;
            break;

        case ROTATE_90:
            angle = 90;
            break;

        case ROTATE_180:
            angle = 180;
            break;

        case ROTATE_270:
            angle = 270;
            break;

        case ROTATE_BUTT:
        default:
            aloge("fatal error! unsupported roate angle!");
            break;
    }
    return angle;
}

static VENC_COLOR_SPACE map_v4l2_colorspace_to_VENC_COLOR_SPACE(enum v4l2_colorspace eV4l2Csc)
{
    VENC_COLOR_SPACE eVencCsc;
    switch((int)eV4l2Csc)
    {
        case V4L2_COLORSPACE_JPEG:
            eVencCsc = VENC_YCC;
            break;
        case V4L2_COLORSPACE_REC709:
        case V4L2_COLORSPACE_REC709_PART_RANGE:
            eVencCsc = VENC_BT709;
            break;
        default:
            aloge("fatal error! unsupported v4l2 color space[0x%x]!", eV4l2Csc);
            eVencCsc = VENC_YCC;
            break;
    }
    return eVencCsc;
}

static int checkColorSpaceFullRange(int nColorSpace)    //enum v4l2_colorspace or enum user_v4l2_colorspace
{
    int nFullRangeFlag = 0;
    switch(nColorSpace)
    {
        case V4L2_COLORSPACE_JPEG:
        case V4L2_COLORSPACE_REC709:
        {
            nFullRangeFlag = 1;
            break;
        }
        case V4L2_COLORSPACE_REC709_PART_RANGE:
        {
            nFullRangeFlag = 0;
            break;
        }
        default:
        {
            aloge("fatal error! unsupported v4l2 color space[0x%x]!", nColorSpace);
            nFullRangeFlag = 1;
            break;
        }
    }
    return nFullRangeFlag;
}

static int CedarvEncInit(VIDEOENCDATATYPE *pVideoEncData)
{
    int ret = -1;
    VideoEncoder *pCedarV = NULL;

    loadMppVencParams(pVideoEncData, NULL);
    alogd("VencChn[%d] Create VeType=%d", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type);

    if(PT_JPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        pCedarV = VencCreate(VENC_CODEC_JPEG);
    }
    else if(PT_MJPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        pCedarV = VencCreate(VENC_CODEC_JPEG);
    }
    else if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        pCedarV = VencCreate(VENC_CODEC_H264);
    }
    else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        pCedarV = VencCreate(VENC_CODEC_H265);
    }
    else
    {
        aloge("fatal error! unknown venc type:0x%x", pVideoEncData->mEncChnAttr.VeAttr.Type);
    }

    //pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
    pVideoEncData->mbCedarvVideoEncInitFlag = FALSE;
    //pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);

    pVideoEncData->pCedarV = pCedarV;

    return ret;
}

static int getMostSuitabeleGopSize(int GopSize, int MaxKeyInterval)
{
     int i = 0;
     int nTemp = 0;
     int nGopSize = GopSize;
     int nMaxKeyInterval = MaxKeyInterval;

     if (0 == nGopSize)
     {
        nGopSize = nMaxKeyInterval;
     }

     if (nGopSize > 64)
     {
        if (nMaxKeyInterval >= 1 && nMaxKeyInterval <= 64)
        {
            nGopSize = nMaxKeyInterval;
        }
        else
        {
            i = 1;
            while (1)
            {
                nTemp = nMaxKeyInterval/i;
                if (nTemp <= 64)
                {
                    break;
                }
                i++;
            }
            nGopSize = nMaxKeyInterval/i;
        }
        alogd("user set invalid gopSize[%d], use most suitable gopsize[%d].", GopSize, nGopSize);
     }

    return nGopSize;
}

static ERRORTYPE change_VENC_CHN_ATTR_BitRate(VENC_CHN_ATTR_S *pCurAttr, VENC_CHN_ATTR_S *pChnAttr)
{
    ERRORTYPE eRet = SUCCESS;

    if (pChnAttr->RcAttr.mRcMode == pCurAttr->RcAttr.mRcMode)
    {
        switch (pChnAttr->RcAttr.mRcMode)
        {
            case VENC_RC_MODE_H264CBR:
                pCurAttr->RcAttr.mAttrH264Cbr.mBitRate = pChnAttr->RcAttr.mAttrH264Cbr.mBitRate;
                break;
            case VENC_RC_MODE_H264VBR:
                pCurAttr->RcAttr.mAttrH264Vbr.mMaxBitRate = pChnAttr->RcAttr.mAttrH264Vbr.mMaxBitRate;
                break;
            case VENC_RC_MODE_H264FIXQP:
                break;
            case VENC_RC_MODE_H264ABR:
                pCurAttr->RcAttr.mAttrH264Abr.mMaxBitRate = pChnAttr->RcAttr.mAttrH264Abr.mMaxBitRate;
                break;
            case VENC_RC_MODE_H265CBR:
                pCurAttr->RcAttr.mAttrH265Cbr.mBitRate = pChnAttr->RcAttr.mAttrH265Cbr.mBitRate;
                break;
            case VENC_RC_MODE_H265VBR:
                pCurAttr->RcAttr.mAttrH265Vbr.mMaxBitRate = pChnAttr->RcAttr.mAttrH265Vbr.mMaxBitRate;
                break;
            case VENC_RC_MODE_H265FIXQP:
                break;
            case VENC_RC_MODE_H265ABR:
                pCurAttr->RcAttr.mAttrH265Abr.mMaxBitRate = pChnAttr->RcAttr.mAttrH265Abr.mMaxBitRate;
                break;
            case VENC_RC_MODE_MJPEGCBR:
                pCurAttr->RcAttr.mAttrMjpegeCbr.mBitRate = pChnAttr->RcAttr.mAttrMjpegeCbr.mBitRate;;
                break;
            case VENC_RC_MODE_MJPEGFIXQP:
                break;
            default:
                eRet = FAILURE;
                alogw("unsupported temporary: chn attr RcAttr RcMode[0x%x]", pChnAttr->RcAttr.mRcMode);
                break;
        }
    }
    else
    {
        eRet = FAILURE;
    }

    return eRet;
}

static void VideoEncMBInfoInit(VencMBInfo *pMBInfo, VENC_CHN_ATTR_S *pChnAttr)
{
    unsigned int mb_num = 0;

    if ((NULL == pMBInfo) || (NULL == pChnAttr))
    {
        aloge("fatal error! invalid input params! %p,%p", pMBInfo, pChnAttr);
        return;
    }

#if (AWCHIP == AW_V853)
    // calc mb num
    if (PT_H264 == pChnAttr->VeAttr.Type)
    {
        unsigned int pic_mb_width = ALIGN(pChnAttr->VeAttr.AttrH264e.PicWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pChnAttr->VeAttr.AttrH264e.PicHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;
        pMBInfo->num_mb = total_mb_num;
    }
    else if (PT_H265 == pChnAttr->VeAttr.Type)
    {
        unsigned int pic_mb_width = ALIGN(pChnAttr->VeAttr.AttrH265e.mPicWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pChnAttr->VeAttr.AttrH265e.mPicHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;
        pMBInfo->num_mb = total_mb_num;
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pChnAttr->VeAttr.Type);
        return;
    }
#else
    // calc mb num
    if (PT_H264 == pChnAttr->VeAttr.Type)
    {
        unsigned int pic_mb_width = ALIGN(pChnAttr->VeAttr.AttrH264e.PicWidth, 16) >> 4;
        unsigned int pic_mb_height = ALIGN(pChnAttr->VeAttr.AttrH264e.PicHeight, 16) >> 4;
        unsigned int total_mb_num = pic_mb_width * pic_mb_height;
        pMBInfo->num_mb = total_mb_num;
    }
    else if (PT_H265 == pChnAttr->VeAttr.Type)
    {
        unsigned int pic_4ctu_width = ALIGN(pChnAttr->VeAttr.AttrH265e.mPicWidth, 128) >> 5;
        unsigned int pic_ctu_height = ALIGN(pChnAttr->VeAttr.AttrH265e.mPicHeight, 32) >> 5;
        unsigned int total_ctu_num = pic_4ctu_width * pic_ctu_height;
        pMBInfo->num_mb = total_ctu_num;
    }
    else
    {
        aloge("fatal error! Unexpected venc type:0x%x, QPMAP only for H264/H265!", pChnAttr->VeAttr.Type);
        return;
    }
#endif

    mb_num = pMBInfo->num_mb;

    // malloc mb info p_para
    if (NULL == pMBInfo->p_para)
    {
        pMBInfo->p_para = (VencMBInfoPara *)malloc(sizeof(VencMBInfoPara) * mb_num);
        if (NULL == pMBInfo->p_para)
        {
            aloge("fatal error! malloc pMBInfo->p_para fail! size=%d", sizeof(VencMBInfoPara) * mb_num);
            return;
        }
    }
    else
    {
        alogw("init mb info repeated? pMBInfo->p_para is not NULL!");
    }
    memset(pMBInfo->p_para, 0, sizeof(VencMBInfoPara) * mb_num);
    alogd("mb_num=%d, pMBInfo->p_para=%p", mb_num, pMBInfo->p_para);
}

static void VideoEncMBInfoDeinit(VencMBInfo *pMBInfo)
{
    if (NULL == pMBInfo)
    {
        aloge("fatal error! invalid input params!");
        return;
    }

    if (pMBInfo->p_para)
    {
        free(pMBInfo->p_para);
        pMBInfo->p_para = NULL;
    }
    pMBInfo->num_mb = 0;
}

static VencCbType gVencCompCbType = {
	.EventHandler = VencCompEventHandler,
	.InputBufferDone = VencCompInputBufferDone,
};

static int CedarvVideoEncInit(VIDEOENCDATATYPE *pVideoEncData)
{
    int ret = SUCCESS;
    if(pVideoEncData->pCedarV)
    {
        loadMppVencParams(pVideoEncData, NULL);

        VencBaseConfig baseConfig;
        unsigned int nDstWidth = 0;
        unsigned int nDstHeight = 0;
        unsigned int nSrcStride = 0;
        unsigned int nDstFrameRate = 0;

        if(PT_JPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            int jpegEncMode = 0;    //0:jpeg; 1:mjpeg
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegEncMode, &jpegEncMode);
            int rotate = map_ROTATE_E_to_VENC_Rotate_Angle(pVideoEncData->mEncChnAttr.VeAttr.Rotate);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamRotation, &rotate);
            if(pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize > 0)
            {
                VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSetVbvSize, &pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize);
            }
            if(pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize > 0)
            {
                VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSetFrameLenThreshold, &pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize);
            }
            nDstWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicWidth;
            nDstHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicHeight;
            nSrcStride = ALIGN(pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, 16);

            VencJpegVideoSignal sVideoSignal;
            memset(&sVideoSignal, 0, sizeof(VencJpegVideoSignal));
            sVideoSignal.src_colour_primaries = map_v4l2_colorspace_to_VENC_COLOR_SPACE(pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);
            sVideoSignal.dst_colour_primaries = sVideoSignal.src_colour_primaries;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegVideoSignal, &sVideoSignal);
        }
        else if (PT_MJPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            int jpegEncMode = 1;    //0:jpeg; 1:mjpeg
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegEncMode, &jpegEncMode);
            switch(pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
            {
                case VENC_RC_MODE_MJPEGCBR:
                {
                    int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
                    alogd("mjpeg set init nBitRate:%d", nBitRate);
                    VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamBitrate, (void*)&nBitRate);
                    VencBitRateRange bitRateRange = {nBitRate, nBitRate};
                    VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSetBitRateRange, (void*)&bitRateRange);
                    alogd("mjpeg set init Qfactor:%d", pVideoEncData->mJpegParam.Qfactor);
                    VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegQuality, (void*)&pVideoEncData->mJpegParam.Qfactor);
                    break;
                }
                case VENC_RC_MODE_MJPEGFIXQP:
                {
                    alogw("not support temporary!");
                    int jpegQuality = pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor;  //70
                    VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegQuality, &jpegQuality);
                    break;
                }
                default:
                {
                    aloge("fatal error! wrong mjpeg rc mode[0x%x], check code!", pVideoEncData->mEncChnAttr.RcAttr.mRcMode);
                    break;
                }
            }
            //int nDstFrameRate;
            if(pVideoEncData->mFrameRateInfo.DstFrmRate > 0)
            {
                nDstFrameRate = pVideoEncData->mFrameRateInfo.DstFrmRate;
            }
            else
            {
                nDstFrameRate = 25;
            }
            alogd("mjpeg set init Framerate:%d", nDstFrameRate);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamFramerate, (void*)&nDstFrameRate);
            setVbvBufferConfig(pVideoEncData->pCedarV, pVideoEncData);

            nDstWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth;
            nDstHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight;
            nSrcStride = ALIGN(pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, 16);

            int rotate = map_ROTATE_E_to_VENC_Rotate_Angle(pVideoEncData->mEncChnAttr.VeAttr.Rotate);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamRotation, &rotate);

            VencJpegVideoSignal sVideoSignal;
            memset(&sVideoSignal, 0, sizeof(VencJpegVideoSignal));
            sVideoSignal.src_colour_primaries = map_v4l2_colorspace_to_VENC_COLOR_SPACE(pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);
            sVideoSignal.dst_colour_primaries = sVideoSignal.src_colour_primaries;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegVideoSignal, &sVideoSignal);
        }
        else if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            VencH264Param h264Param;
            memset(&h264Param, 0, sizeof(VencH264Param));
            h264Param.nCodingMode = VENC_FRAME_CODING;
            h264Param.bEntropyCodingCABAC = 1;
            h264Param.nBitrate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
            //int nDstFrameRate;
            if(pVideoEncData->mFrameRateInfo.DstFrmRate > 0)
            {
                nDstFrameRate = pVideoEncData->mFrameRateInfo.DstFrmRate;
            }
            else
            {
                nDstFrameRate = 25;
            }
            // keep Qp and IFrameInterval for dynamic adjust vencoder param
            if(0 == pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval)
            {
                pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval = nDstFrameRate;
            }
            h264Param.nMaxKeyInterval = pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval;
            h264Param.sProfileLevel.nProfile = map_VENC_ATTR_H264_S_Profile_to_VENC_H264PROFILETYPE(pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.Profile);
            h264Param.sProfileLevel.nLevel = map_H264_LEVEL_E_to_VENC_H264LEVELTYPE(pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mLevel);//VENC_H264Level51;
            //h264Param.bLongRefEnable = (unsigned char)pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbLongTermRef;
            switch(pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
            {
                case VENC_RC_MODE_H264CBR:
                {
                    h264Param.sRcParam.eRcMode = AW_CBR;
                    int minQp = pVideoEncData->mRcParam.ParamH264Cbr.mMinQp;
                    int maxQp = pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h264CBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mRcParam.ParamH264Cbr.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h264CBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp = 51;
                    }
                    h264Param.sQPRange.nMinqp = pVideoEncData->mRcParam.ParamH264Cbr.mMinQp;
                    h264Param.sQPRange.nMaxqp = pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp;
                    h264Param.sQPRange.nMaxPqp = pVideoEncData->mRcParam.ParamH264Cbr.mMaxPqp;
                    h264Param.sQPRange.nMinPqp = pVideoEncData->mRcParam.ParamH264Cbr.mMinPqp;
                    h264Param.sQPRange.nQpInit = pVideoEncData->mRcParam.ParamH264Cbr.mQpInit;
                    h264Param.sQPRange.bEnMbQpLimit = pVideoEncData->mRcParam.ParamH264Cbr.mbEnMbQpLimit;
                    break;
                }
                case VENC_RC_MODE_H264VBR:
                {
                    h264Param.sRcParam.eRcMode = AW_VBR;
                    int minQp = pVideoEncData->mRcParam.ParamH264Vbr.mMinQp;
                    int maxQp = pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h264VBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mRcParam.ParamH264Vbr.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h264VBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp = 51;
                    }
                    h264Param.sQPRange.nMinqp = pVideoEncData->mRcParam.ParamH264Vbr.mMinQp;
                    h264Param.sQPRange.nMaxqp = pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp;
                    h264Param.sQPRange.nMaxPqp = pVideoEncData->mRcParam.ParamH264Vbr.mMaxPqp;
                    h264Param.sQPRange.nMinPqp = pVideoEncData->mRcParam.ParamH264Vbr.mMinPqp;
                    h264Param.sQPRange.nQpInit = pVideoEncData->mRcParam.ParamH264Vbr.mQpInit;
                    h264Param.sQPRange.bEnMbQpLimit = pVideoEncData->mRcParam.ParamH264Vbr.mbEnMbQpLimit;
                    h264Param.sRcParam.sVbrParam.uMaxBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate;
                    h264Param.sRcParam.sVbrParam.nMovingTh = pVideoEncData->mRcParam.ParamH264Vbr.mMovingTh;
                    h264Param.sRcParam.sVbrParam.nQuality = pVideoEncData->mRcParam.ParamH264Vbr.mQuality;
                    h264Param.sRcParam.sVbrParam.nIFrmBitsCoef = pVideoEncData->mRcParam.ParamH264Vbr.mIFrmBitsCoef;
                    h264Param.sRcParam.sVbrParam.nPFrmBitsCoef = pVideoEncData->mRcParam.ParamH264Vbr.mPFrmBitsCoef;
                    break;
                }
                case VENC_RC_MODE_H264FIXQP:
                {
                    h264Param.sRcParam.eRcMode = AW_FIXQP;
                    h264Param.sRcParam.sFixQp.bEnable = 1;
                    int IQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mIQp;
                    int PQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mPQp;
                    if (!(IQp>=1 && IQp<=51))
                    {
                        alogw("h264FixQp IQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", IQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mIQp = 30;
                    }
                    if (!(PQp>=1 && PQp<=51))
                    {
                        alogw("h264FixQp PQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", PQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mPQp = 30;
                    }
                    h264Param.sRcParam.sFixQp.nIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mIQp;
                    h264Param.sRcParam.sFixQp.nPQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mPQp;
                    break;
                }
                case VENC_RC_MODE_H264ABR:
                {
                    h264Param.sRcParam.eRcMode = AW_AVBR;
                    int minQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMinQp;
                    int maxQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h264ABR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h264ABR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMaxQp = 51;
                    }
                    h264Param.sQPRange.nMinqp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMinQp;
                    h264Param.sQPRange.nMaxqp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMaxQp;
                    int minIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMinIQp;
                    if (!(minIQp>=20 && minIQp<=40))
                    {
                        alogw("uMinIQp should be in [20,40]! usr_SetVal:%d, change to defaultVal:30!", minIQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 30;
                    }
                    h264Param.sRcParam.uMinIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMinIQp;
                    h264Param.sRcParam.sVbrParam.uMaxBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate;
                    //h264Param.sRcParam.sVbrParam.uRatioChangeQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp;
                    h264Param.sRcParam.sVbrParam.nQuality = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Abr.mQuality;
                    break;
                }
                case VENC_RC_MODE_H264QPMAP:
                {
                    alogd("H264 set RcMode = AW_QPMAP");
                    h264Param.sRcParam.eRcMode = AW_QPMAP;
                    int minQp = pVideoEncData->mRcParam.ParamH264QpMap.mMinQp;
                    int maxQp = pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h264QPMAP minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mRcParam.ParamH264QpMap.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h264QPMAP maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp = 51;
                    }
                    h264Param.sQPRange.nMinqp = pVideoEncData->mRcParam.ParamH264QpMap.mMinQp;
                    h264Param.sQPRange.nMaxqp = pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp;
                    h264Param.sQPRange.nMaxPqp = pVideoEncData->mRcParam.ParamH264QpMap.mMaxPqp;
                    h264Param.sQPRange.nMinPqp = pVideoEncData->mRcParam.ParamH264QpMap.mMinPqp;
                    h264Param.sQPRange.nQpInit = pVideoEncData->mRcParam.ParamH264QpMap.mQpInit;
                    h264Param.sQPRange.bEnMbQpLimit = pVideoEncData->mRcParam.ParamH264QpMap.mbEnMbQpLimit;
                    break;
                }
                default:
                {
                    aloge("fatal error! wrong h264 rc mode[0x%x], check code!", pVideoEncData->mEncChnAttr.RcAttr.mRcMode);
                    break;
                }
            }

            h264Param.nFramerate = nDstFrameRate;
			h264Param.nSrcFramerate = pVideoEncData->mFrameRateInfo.SrcFrmRate;
            //config the GopParam
            if(VENC_GOPMODE_NORMALP == pVideoEncData->mEncChnAttr.GopAttr.enGopMode)
            {
                //h264Param.bLongRefEnable = 0;
                h264Param.sGopParam.bUseGopCtrlEn = 1;
                h264Param.sGopParam.eGopMode = AW_NORMALP;
            }
            else
            {
                aloge("Sorry, this version do not support [%d] GopMode snd set VENC_GOPMODE_NORMALP GopMode!", pVideoEncData->mEncChnAttr.GopAttr.enGopMode);
                h264Param.sGopParam.bUseGopCtrlEn = 1;
                h264Param.sGopParam.eGopMode = AW_NORMALP;
            }
            h264Param.sGopParam.sRefParam.bAdvancedRefEn = pVideoEncData->mRefParam.bAdvancedRefEn;
            h264Param.sGopParam.sRefParam.nBase = pVideoEncData->mRefParam.nBase;
            h264Param.sGopParam.sRefParam.nEnhance = pVideoEncData->mRefParam.nEnhance;
            h264Param.sGopParam.sRefParam.bRefBaseEn = pVideoEncData->mRefParam.bRefBaseEn;
            if (pVideoEncData->mRefParam.bAdvancedRefEn)
            {
                alogd("h264 advancedRef:%d,%d,%d,%d", pVideoEncData->mRefParam.bAdvancedRefEn, pVideoEncData->mRefParam.nBase,
                    pVideoEncData->mRefParam.nEnhance, pVideoEncData->mRefParam.bRefBaseEn);
            }
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264Param, &h264Param);

            int nIFilterEnable = 0;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamIfilter, &nIFilterEnable);

            VencH264VideoSignal sVideoSignal;
            memset(&sVideoSignal, 0, sizeof(VencH264VideoSignal));
            sVideoSignal.video_format = DEFAULT;
            sVideoSignal.full_range_flag = checkColorSpaceFullRange((int)pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);;
            sVideoSignal.src_colour_primaries = map_v4l2_colorspace_to_VENC_COLOR_SPACE(pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);
            sVideoSignal.dst_colour_primaries = sVideoSignal.src_colour_primaries;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264VideoSignal, &sVideoSignal);

            //int nPskip = 0;
            //VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSetPSkip, &nPskip);

            int nFastEnc = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.FastEncFlag ? 1 : 0;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamFastEnc, &nFastEnc);

            int sliceHeight = 0;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSliceHeight, &sliceHeight);
            unsigned char bPFrameIntraEn = (unsigned char)pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamPFrameIntraEn, (void*)&bPFrameIntraEn);

            setVbvBufferConfig(pVideoEncData->pCedarV, pVideoEncData);
            nDstWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth;
            nDstHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight;
            nSrcStride = ALIGN(pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, 16);

            int rotate = map_ROTATE_E_to_VENC_Rotate_Angle(pVideoEncData->mEncChnAttr.VeAttr.Rotate);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamRotation, &rotate);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamIQpOffset, &pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.IQpOffset);

            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamIsNightCaseFlag, (void*)&pVideoEncData->DayOrNight);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamHighPassFilter, &pVideoEncData->mVencHighPassFilter);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamProductCase, &pVideoEncData->mRcParam.product_mode);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSensorType, &pVideoEncData->mRcParam.sensor_type);
        }
        else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            VencH265Param h265Param;
            memset(&h265Param, 0, sizeof(VencH265Param));
            h265Param.sProfileLevel.nProfile = map_VENC_ATTR_H265_S_Profile_to_VENC_H265PROFILETYPE(pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mProfile);
            h265Param.sProfileLevel.nLevel = map_H265_LEVEL_E_to_VENC_H265LEVELTYPE(pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mLevel);//VENC_H265Level62;
            //h265Param.bLongTermRef = (int)pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbLongTermRef;

            switch(pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
            {
                case VENC_RC_MODE_H265CBR:
                {
                    h265Param.sRcParam.eRcMode = AW_CBR;
                    int minQp = pVideoEncData->mRcParam.ParamH265Cbr.mMinQp;
                    int maxQp = pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h265CBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mRcParam.ParamH265Cbr.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h265CBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp = 51;
                    }
                    h265Param.sQPRange.nMinqp = pVideoEncData->mRcParam.ParamH265Cbr.mMinQp;
                    h265Param.sQPRange.nMaxqp = pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp;
                    h265Param.sQPRange.nMaxPqp = pVideoEncData->mRcParam.ParamH265Cbr.mMaxPqp;
                    h265Param.sQPRange.nMinPqp = pVideoEncData->mRcParam.ParamH265Cbr.mMinPqp;
                    h265Param.sQPRange.nQpInit = pVideoEncData->mRcParam.ParamH265Cbr.mQpInit;
                    h265Param.sQPRange.bEnMbQpLimit = pVideoEncData->mRcParam.ParamH265Cbr.mbEnMbQpLimit;
                    break;
                }
                case VENC_RC_MODE_H265VBR:
                {
                    h265Param.sRcParam.eRcMode = AW_VBR;
                    int minQp = pVideoEncData->mRcParam.ParamH265Vbr.mMinQp;
                    int maxQp = pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h265VBR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mRcParam.ParamH265Vbr.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h265VBR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp = 51;
                    }
                    h265Param.sQPRange.nMinqp = pVideoEncData->mRcParam.ParamH265Vbr.mMinQp;
                    h265Param.sQPRange.nMaxqp = pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp;
                    h265Param.sQPRange.nMaxPqp = pVideoEncData->mRcParam.ParamH265Vbr.mMaxPqp;
                    h265Param.sQPRange.nMinPqp = pVideoEncData->mRcParam.ParamH265Vbr.mMinPqp;
                    h265Param.sQPRange.nQpInit = pVideoEncData->mRcParam.ParamH265Vbr.mQpInit;
                    h265Param.sQPRange.bEnMbQpLimit = pVideoEncData->mRcParam.ParamH265Vbr.mbEnMbQpLimit;
                    h265Param.sRcParam.sVbrParam.uMaxBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate;
                    h265Param.sRcParam.sVbrParam.nMovingTh = pVideoEncData->mRcParam.ParamH265Vbr.mMovingTh;
                    h265Param.sRcParam.sVbrParam.nQuality = pVideoEncData->mRcParam.ParamH265Vbr.mQuality;
                    h265Param.sRcParam.sVbrParam.nIFrmBitsCoef = pVideoEncData->mRcParam.ParamH265Vbr.mIFrmBitsCoef;
                    h265Param.sRcParam.sVbrParam.nPFrmBitsCoef = pVideoEncData->mRcParam.ParamH265Vbr.mPFrmBitsCoef;
                    break;
                }
                case VENC_RC_MODE_H265FIXQP:
                {
                    h265Param.sRcParam.eRcMode = AW_FIXQP;
                    h265Param.sRcParam.sFixQp.bEnable = 1;
                    int IQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mIQp;
                    int PQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mPQp;
                    if (!(IQp>=1 && IQp<=51))
                    {
                        alogw("h265FixQp IQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", IQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mIQp = 30;
                    }
                    if (!(PQp>=1 && PQp<=51))
                    {
                        alogw("h265FixQp PQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 30!", PQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mPQp = 30;
                    }
                    h265Param.sRcParam.sFixQp.nIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mIQp;
                    h265Param.sRcParam.sFixQp.nPQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mPQp;
                    break;
                }
                case VENC_RC_MODE_H265ABR:
                {
                    h265Param.sRcParam.eRcMode = AW_AVBR;
                    int minQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMinQp;
                    int maxQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h265ABR minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h265ABR maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMaxQp = 51;
                    }
                    h265Param.sQPRange.nMinqp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMinQp;
                    h265Param.sQPRange.nMaxqp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMaxQp;
                    int minIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMinIQp;
                    if (!(minIQp>=20 && minIQp<=40))
                    {
                        alogw("uMinIQp should be in [20,40]! usr_SetVal:%d, change to defaultVal:30!", minIQp);
                        pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMinIQp = 30;
                    }
                    h265Param.sRcParam.uMinIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMinIQp;
                    h265Param.sRcParam.sVbrParam.uMaxBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate;
                    //h265Param.sRcParam.sVbrParam.uRatioChangeQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp;
                    h265Param.sRcParam.sVbrParam.nQuality = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Abr.mQuality;
                    break;
                }
                case VENC_RC_MODE_H265QPMAP:
                {
                    alogd("H265 set RcMode = AW_QPMAP");
                    h265Param.sRcParam.eRcMode = AW_QPMAP;
                    int minQp = pVideoEncData->mRcParam.ParamH265QpMap.mMinQp;
                    int maxQp = pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp;
                    if (!(minQp>=1 && minQp<=51))
                    {
                        alogw("h265QPMAP minQp should in range:[1,51]! but usr_SetVal: %d! change it to default: 1", minQp);
                        pVideoEncData->mRcParam.ParamH265QpMap.mMinQp = 1;
                    }
                    if (!(maxQp>=minQp && maxQp>=1 && maxQp<=51))
                    {
                        alogw("h265QPMAP maxQp should in range:[minQp,51]! but usr_SetVal: %d! change it to default: 51", maxQp);
                        pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp = 51;
                    }
                    h265Param.sQPRange.nMinqp = pVideoEncData->mRcParam.ParamH265QpMap.mMinQp;
                    h265Param.sQPRange.nMaxqp = pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp;
                    h265Param.sQPRange.nMaxPqp = pVideoEncData->mRcParam.ParamH265QpMap.mMaxPqp;
                    h265Param.sQPRange.nMinPqp = pVideoEncData->mRcParam.ParamH265QpMap.mMinPqp;
                    h265Param.sQPRange.nQpInit = pVideoEncData->mRcParam.ParamH265QpMap.mQpInit;
                    h265Param.sQPRange.bEnMbQpLimit = pVideoEncData->mRcParam.ParamH265QpMap.mbEnMbQpLimit;
                    break;
                }
                default:
                {
                    aloge("fatal error! wrong h265 rc mode[0x%x], check code!", pVideoEncData->mEncChnAttr.RcAttr.mRcMode);
                    break;
                }
            }
            //int nDstFrameRate;
            if(pVideoEncData->mFrameRateInfo.DstFrmRate > 0)
            {
                nDstFrameRate = pVideoEncData->mFrameRateInfo.DstFrmRate;
            }
            else
            {
                nDstFrameRate = 25;
            }
            h265Param.nFramerate = nDstFrameRate;
            h265Param.nSrcFramerate = pVideoEncData->mFrameRateInfo.SrcFrmRate;
			h265Param.nBitrate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
            if(0 == pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval)
            {
                pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval = nDstFrameRate;
            }

            //set gop size
            h265Param.idr_period = pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval;
            h265Param.nIntraPeriod = pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval;
            //h265Param.nGopSize = getMostSuitabeleGopSize(pVideoEncData->mEncChnAttr.GopAttr.mGopSize, pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval);
            h265Param.nGopSize = pVideoEncData->mEncChnAttr.GopAttr.mGopSize;
            /*
            if(h265Param.nGopSize <= 0)
            {
                alogd("user set invalid gopSize[%d], use idr interval[%d] as default.", h265Param.nGopSize, h265Param.idr_period);
                h265Param.nGopSize = h265Param.idr_period;
            }
            */

            h265Param.nQPInit = 26;

            if(VENC_GOPMODE_NORMALP == pVideoEncData->mEncChnAttr.GopAttr.enGopMode)
            {
                //h265Param.bLongTermRef = 0;
                h265Param.sGopParam.bUseGopCtrlEn = 1;
                h265Param.sGopParam.eGopMode = AW_NORMALP;
            }
            else
            {
                aloge("Sorry, this version do not support [%d] GopMode snd set VENC_GOPMODE_NORMALP GopMode!", pVideoEncData->mEncChnAttr.GopAttr.enGopMode);
                //h265Param.bLongTermRef = 0;
                h265Param.sGopParam.bUseGopCtrlEn = 1;
                h265Param.sGopParam.eGopMode = AW_NORMALP;
            }
            h265Param.sGopParam.sRefParam.bAdvancedRefEn = pVideoEncData->mRefParam.bAdvancedRefEn;
            h265Param.sGopParam.sRefParam.nBase = pVideoEncData->mRefParam.nBase;
            h265Param.sGopParam.sRefParam.nEnhance = pVideoEncData->mRefParam.nEnhance;
            h265Param.sGopParam.sRefParam.bRefBaseEn = pVideoEncData->mRefParam.bRefBaseEn;
            if (pVideoEncData->mRefParam.bAdvancedRefEn)
            {
                alogd("h265 advancedRef:%d,%d,%d,%d", pVideoEncData->mRefParam.bAdvancedRefEn, pVideoEncData->mRefParam.nBase,
                    pVideoEncData->mRefParam.nEnhance, pVideoEncData->mRefParam.bRefBaseEn);
            }
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH265Param, &h265Param);

            //VencSetParameter(pCedarV, VENC_IndexParamMaxKeyInterval, (void*)&pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval);
            //unsigned int nVirIFrameInterval = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mVirtualIFrameInterval;
            //VencSetParameter(pCedarV, VENC_IndexParamVirtualIFrame, (void*)&nVirIFrameInterval);
            int nFastEnc = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mFastEncFlag ? 1 : 0;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamFastEnc, &nFastEnc);
            unsigned char bPFrameIntraEn = (unsigned char)pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamPFrameIntraEn, (void*)&bPFrameIntraEn);

            setVbvBufferConfig(pVideoEncData->pCedarV, pVideoEncData);
            nDstWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth;
            nDstHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight;
            nSrcStride = ALIGN(pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, 16);

            int rotate = map_ROTATE_E_to_VENC_Rotate_Angle(pVideoEncData->mEncChnAttr.VeAttr.Rotate);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamRotation, &rotate);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamIQpOffset, &pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.IQpOffset);

            VencH264VideoSignal sVideoSignal;
            memset(&sVideoSignal, 0, sizeof(VencH264VideoSignal));
            sVideoSignal.video_format = DEFAULT;
            sVideoSignal.full_range_flag = checkColorSpaceFullRange((int)pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);
            sVideoSignal.src_colour_primaries = map_v4l2_colorspace_to_VENC_COLOR_SPACE(pVideoEncData->mEncChnAttr.VeAttr.mColorSpace);
            sVideoSignal.dst_colour_primaries = sVideoSignal.src_colour_primaries;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamVUIVideoSignal, &sVideoSignal); 
            
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamIsNightCaseFlag, (void*)&pVideoEncData->DayOrNight);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamHighPassFilter, &pVideoEncData->mVencHighPassFilter);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamProductCase, &pVideoEncData->mRcParam.product_mode);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSensorType, &pVideoEncData->mRcParam.sensor_type);
        }
        else
        {
            aloge("fatal error! unknown venc type:0x%x", pVideoEncData->mEncChnAttr.VeAttr.Type);
        }

        if(pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth != nSrcStride)
        {
            aloge("fatal error! srcWidth[%d]!=srcStride[%d]", pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, nSrcStride);
        }
#ifdef ENABLE_ENCODE_STATISTICS
        unsigned char bEnableStatistics = 1;
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamEncodeTimeEn, (void*)&bEnableStatistics);
#endif

        if ((VENC_RC_MODE_H264QPMAP == pVideoEncData->mEncChnAttr.RcAttr.mRcMode) ||
            (VENC_RC_MODE_H265QPMAP == pVideoEncData->mEncChnAttr.RcAttr.mRcMode))
        {
            VideoEncMBInfoInit(&pVideoEncData->mMBInfo, &pVideoEncData->mEncChnAttr);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamMBInfoOutput, &pVideoEncData->mMBInfo);
            // Notice: for first frame, not set MBModeCtrl.
            // VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamMBModeCtrl, &pVideoEncData->mMBModeCtrl);
        }
        
        if (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            if (pVideoEncData->mH264VuiParam.VuiTimeInfo.timing_info_present_flag)
            {
                /** timing_info_present_flag is the default param for H264. */
                VencH264VideoTiming timeInfo;
                memset(&timeInfo, 0, sizeof(VencH264VideoTiming));
                timeInfo.fixed_frame_rate_flag = pVideoEncData->mH264VuiParam.VuiTimeInfo.fixed_frame_rate_flag;
                timeInfo.num_units_in_tick = pVideoEncData->mH264VuiParam.VuiTimeInfo.num_units_in_tick;
                timeInfo.time_scale = pVideoEncData->mH264VuiParam.VuiTimeInfo.time_scale;
                alogd("H264 fixed_frame_rate_flag: %d, num_units_in_tick: %d, time_scale: %d",
                    timeInfo.fixed_frame_rate_flag, timeInfo.num_units_in_tick, timeInfo.time_scale);
                VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264VideoTiming, &timeInfo);
            }
        }
        else if (PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            if (pVideoEncData->mH265VuiParam.VuiTimeInfo.timing_info_present_flag)
            {
                VencH265TimingS timeInfo;
                memset(&timeInfo, 0, sizeof(VencH265TimingS));
                timeInfo.timing_info_present_flag = pVideoEncData->mH265VuiParam.VuiTimeInfo.timing_info_present_flag;
                timeInfo.num_units_in_tick = pVideoEncData->mH265VuiParam.VuiTimeInfo.num_units_in_tick;
                timeInfo.time_scale = pVideoEncData->mH265VuiParam.VuiTimeInfo.time_scale;
                timeInfo.num_ticks_poc_diff_one = pVideoEncData->mH265VuiParam.VuiTimeInfo.num_ticks_poc_diff_one_minus1;
                alogd("H265 timing_info_present_flag: %d, num_units_in_tick: %d, time_scale: %d, num_ticks_poc_diff_one: %d",
                    timeInfo.timing_info_present_flag, timeInfo.num_units_in_tick, timeInfo.time_scale, timeInfo.num_ticks_poc_diff_one);
                VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH265Timing, &timeInfo);
            }
        }

        //init 2dnr before Venc Init.
        if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParam2DFilter, (void*)&pVideoEncData->m2DfilterParam);
        }

        //init 3dnr before Venc Init.
        if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParam3DFilterNew, (void*)&pVideoEncData->m3DfilterParam);
        }

        // encpp sharp
        alogd("veChn[%d], EncppEnable=%d", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.EncppAttr.mbEncppEnable);
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamEnableEncppSharp, (void*)&pVideoEncData->mEncChnAttr.EncppAttr.mbEncppEnable);

        if (PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            unsigned int DropFrameNum = pVideoEncData->mEncChnAttr.VeAttr.mDropFrameNum;
            alogd("DropFrameNum: %d", DropFrameNum);
            if (DropFrameNum > 0)
            {
                if (FALSE == pVideoEncData->mbForbidDiscardingFrame)
                    VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamDropFrame, (void*)&DropFrameNum);
                else
                    aloge("fatal error, setting DropFrameNum is not supported in ForbidDiscardingFrame mode!");
            }
        }

        alogd("GDC enable: %d", pVideoEncData->mEncChnAttr.GdcAttr.bGDC_en);
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamGdcConfig, &pVideoEncData->mEncChnAttr.GdcAttr);

        memset(&baseConfig, 0 ,sizeof(VencBaseConfig));
        baseConfig.nInputWidth = pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth;
        baseConfig.nInputHeight = pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight;
        baseConfig.nStride = nSrcStride;
        baseConfig.nDstWidth = nDstWidth;
        baseConfig.nDstHeight = nDstHeight;
        baseConfig.eInputFormat = map_PIXEL_FORMAT_E_to_VENC_PIXEL_FMT(pVideoEncData->mEncChnAttr.VeAttr.PixelFormat);
        switch(pVideoEncData->mEncChnAttr.VeAttr.PixelFormat)
        {
            case MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X:
            {
                baseConfig.bLbcLossyComEnFlag2x = 1;
                baseConfig.bLbcLossyComEnFlag2_5x = 0;
                break;
            }
            case MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X:
            {
                baseConfig.bLbcLossyComEnFlag2x = 0;
                baseConfig.bLbcLossyComEnFlag2_5x = 1;
                break;
            }
            case MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X:
            {
                baseConfig.bLbcLossyComEnFlag2x = 0;
                baseConfig.bLbcLossyComEnFlag1_5x = 1;
                break;
            }
            case MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X:
            {
                baseConfig.bLbcLossyComEnFlag2x = 0;
                baseConfig.bLbcLossyComEnFlag2_5x = 0;
                break;
            }
            default:
                break;
        }
        baseConfig.bIsVbvNoCache = 1;   //video encode config vbv buffer no cache!
        //baseConfig.memops = pVideoEncData->mMemOps;

        if (pVideoEncData->mEncChnAttr.VeAttr.mOnlineEnable)
        {
            baseConfig.bOnlineMode = 1;
            baseConfig.bOnlineChannel = 1;
            unsigned int OnlineShareBufNum = pVideoEncData->mEncChnAttr.VeAttr.mOnlineShareBufNum;
            if ((2 < OnlineShareBufNum) || (0 >= OnlineShareBufNum))
            {
                baseConfig.nOnlineShareBufNum = 1;
                alogw("user set OnlineShareBufNum=%d is invalid value, reset it to 1.", OnlineShareBufNum);
            }
            else
            {
                baseConfig.nOnlineShareBufNum = OnlineShareBufNum;
            }
            pVideoEncData->mOnlineEnableFlag = 1;
        }
        else
        {
            baseConfig.bOnlineMode = 1;
            baseConfig.bOnlineChannel = 0;
            pVideoEncData->mOnlineEnableFlag = 0;
        }
        alogd("Online Mode=%d, Channel=%d, ShareBufNum=%d, OnlineEnableFlag=%d",
            baseConfig.bOnlineMode, baseConfig.bOnlineChannel, baseConfig.nOnlineShareBufNum, pVideoEncData->mOnlineEnableFlag);

        switch (pVideoEncData->mEncChnAttr.VeAttr.mVeRefFrameLbcMode)
        {
            case VENC_REF_FRAME_LBC_MODE_2_0X:
            {
                baseConfig.rec_lbc_mode = LBC_MODE_2_0X;
                break;
            }
            case VENC_REF_FRAME_LBC_MODE_2_5X:
            {
                baseConfig.rec_lbc_mode = LBC_MODE_2_5X;
                break;
            }
            case VENC_REF_FRAME_LBC_MODE_NO_LOSSY:
            {
                baseConfig.rec_lbc_mode = LBC_MODE_NO_LOSSY;
                break;
            }
            case VENC_REF_FRAME_LBC_MODE_DEFAULT:
            case VENC_REF_FRAME_LBC_MODE_1_5X:
            default:
            {
                baseConfig.rec_lbc_mode = LBC_MODE_1_5X;
                break;
            }
        }
        alogd("rec_lbc_mode=%d", baseConfig.rec_lbc_mode);

        ret = VencInit(pVideoEncData->pCedarV, &baseConfig);
        if(VENC_RESULT_OK != ret)
        {
            aloge("fatal error vencInit fail:%x",ret);
            if(VENC_RESULT_EFUSE_ERROR == ret)
            {
                return ERR_VENC_EFUSE_ERROR;
            }
            else if(VENC_RESULT_NO_MEMORY == ret)
            {
                return ERR_VENC_NOMEM;
            }
            else if(VENC_RESULT_NULL_PTR == ret)
            {
                return ERR_VENC_NULL_PTR;
            }
        }
        else
        {
            alogd("Venc Init success");

            int venclib_ret = VencSetCallbacks(pVideoEncData->pCedarV, &gVencCompCbType, (void *)pVideoEncData);
            if (VENC_RESULT_OK != venclib_ret)
            {
                aloge("fatal error Venc SetCallbacks fail:%x", venclib_ret);
            }
            else
            {
                alogd("Venc SetCallbacks success");
            }

            venclib_ret = VencStart(pVideoEncData->pCedarV);
            if (VENC_RESULT_OK != venclib_ret)
            {
                aloge("fatal error! Venc Start fail, ret=%d", venclib_ret);
            }
            else
            {
                alogd("Venc Start success");
            }
        }

        memcpy(&pVideoEncData->mBaseConfig, &baseConfig, sizeof(VencBaseConfig));
        pVideoEncData->mbCedarvVideoEncInitFlag = TRUE;

        pVideoEncData->mMemOps = baseConfig.memops;
        pVideoEncData->mVeOpsS = baseConfig.veOpsS;
        pVideoEncData->mpVeOpsSelf = baseConfig.pVeOpsSelf;

        if(pVideoEncData->mVEFreq)
        {
            alogd("venc init VE freq to [%d]MHz", pVideoEncData->mVEFreq);
            //CdcVeSetSpeed(baseConfig.veOpsS, baseConfig.pVeOpsSelf, pVideoEncData->mVEFreq);
            VencSetFreq(pVideoEncData->pCedarV, pVideoEncData->mVEFreq);
        }


        //unsigned char nPrintParamsFlag = 0;
        //VencGetParameter(pCedarV, VENC_IndexParamAllParams, (void*)&nPrintParamsFlag);
		if((PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type) || (PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type))
		{
			if(VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSaveBSFile, (void*)&pVideoEncData->mSaveBSFile) != 0)
			{
				aloge("fatal error! can not save bs file!");
			}
		}

        //open the proc to catch parame of VE.
        if(VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamProcSet, (void*)&pVideoEncData->mProcSet) != 0)
        {
            aloge("fatal error! can not open proc info!");
        }
        if(VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamChannelNum, (void*)&pVideoEncData->mMppChnInfo.mChnId) != 0)
        {
            aloge("fatal error! set channel num[%d] fail", pVideoEncData->mMppChnInfo.mChnId);
        }

        //init color2grey
        if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            unsigned char OpenColor2Grey = pVideoEncData->mColor2GreyParam.bColor2Grey?1:0;
            alogd("set Color2Grey [%d], in Venchn[%d]", OpenColor2Grey, pVideoEncData->mMppChnInfo.mChnId);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamChmoraGray,(void*)&OpenColor2Grey);
        }

        // config if drop overflow frame directly.
        unsigned int bDropOverflowFrameFlag = pVideoEncData->mbDropOverflowFrameFlag;
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamDropOverflowFrame, (void*)&bDropOverflowFrameFlag);
        alogd("vencChn[%d] had init!", pVideoEncData->mMppChnInfo.mChnId);

        // set VencTargetBitsClipParam.
        VencTargetBitsClipParam *pBitsClipParam = &pVideoEncData->mRcParam.mBitsClipParam;
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamTargetBitsClipParam, (void*)pBitsClipParam);

        // set VencAeDiffParam.
        VencAeDiffParam *pAeDiffParam = &pVideoEncData->mRcParam.mAeDiffParam;
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamAeDiffParam, (void*)pAeDiffParam);
    }
    else
    {
        aloge("fatal error! the video encoder do not creat!!");
    }
    return ret;
}

static ERRORTYPE config_VENC_STREAM_S_by_EncodedStream(VENC_STREAM_S *pDst, EncodedStream *pSrc, VIDEOENCDATATYPE *pVideoEncData)
{
    if(pDst->mPackCount<1 || NULL==pDst->mpPack)
    {
        aloge("fatal error! wrong param[%d],[%p]!", pDst->mPackCount, pDst->mpPack);
        return ERR_VENC_ILLEGAL_PARAM;
    }
    pDst->mpPack[0].mpAddr0 = pSrc->pBuffer;
    pDst->mpPack[0].mpAddr1 = pSrc->pBufferExtra;
    pDst->mpPack[0].mpAddr2 = pSrc->pBufferExtra2;
    pDst->mpPack[0].mLen0 = pSrc->nBufferLen;
    pDst->mpPack[0].mLen1 = pSrc->nBufferExtraLen;
    pDst->mpPack[0].mLen2 = pSrc->nBufferExtraLen2;
    pDst->mpPack[0].mPTS = pSrc->nTimeStamp;
    pDst->mpPack[0].mbFrameEnd = TRUE;
    if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        pDst->mpPack[0].mDataType.enH264EType = (pSrc->nFlags & CEDARV_FLAG_KEYFRAME) ? H264E_NALU_ISLICE : H264E_NALU_PSLICE;
    }
    else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        pDst->mpPack[0].mDataType.enH265EType = (pSrc->nFlags & CEDARV_FLAG_KEYFRAME) ? H265E_NALU_ISLICE : H265E_NALU_PSLICE;
    }
    else if(PT_MJPEG == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_JPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        pDst->mpPack[0].mDataType.enJPEGEType = JPEGE_PACK_PIC;
    }
    else
    {
        alogw("fatal error! unsupported temporary!");
    }
    pDst->mpPack[0].mOffset = 0;
    pDst->mpPack[0].mDataNum = 0;
    pDst->mSeq = pSrc->nID;


    if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        //temporary not use h264 info.
        memset(&pDst->mH264Info, 0, sizeof(VENC_STREAM_INFO_H264_S));
    }
    else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        //temporary not use h265 info.
        memset(&pDst->mH265Info, 0, sizeof(VENC_STREAM_INFO_H265_S));
    }
    else if(PT_MJPEG == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_JPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        //temporary not use jpeg info.
        memset(&pDst->mJpegInfo, 0, sizeof(VENC_STREAM_INFO_JPEG_S));
    }
    else
    {
        alogw("fatal error! unsupported temporary!");
    }
    return SUCCESS;
}

static ERRORTYPE config_VencOutputBuffer_By_EncodedStream(VencOutputBuffer* pDst, EncodedStream* pSrc)
{
    if (pDst == NULL || pSrc == NULL)
    {
        aloge("fatal error! Empty pointer in config_VencOutputBuffer_By_EncodedStream");
        return ERR_VENC_ILLEGAL_PARAM;
    }

    pDst->nID = pSrc->nID;
    pDst->nPts = pSrc->nTimeStamp;
    pDst->nFlag = 0;
    if (pSrc->nFlags & CEDARV_FLAG_KEYFRAME)
    {
        pDst->nFlag |= VENC_BUFFERFLAG_KEYFRAME;
    }
    if (pSrc->nFlags & CEDARV_FLAG_EOS)
    {
        pDst->nFlag |= VENC_BUFFERFLAG_EOS;
    }
    if (pSrc->nFlags & CEDARV_FLAG_THUMB)
    {
        pDst->nFlag |= VENC_BUFFERFLAG_THUMB;
    }
    pDst->nSize0 = pSrc->nBufferLen;
    pDst->nSize1 = pSrc->nBufferExtraLen;
    pDst->nSize2 = pSrc->nBufferExtraLen2;
    pDst->pData0 = pSrc->pBuffer;
    pDst->pData1 = pSrc->pBufferExtra;
    pDst->pData2 = pSrc->pBufferExtra2;

    memcpy(&pDst->frame_info, &pSrc->video_frame_info, sizeof(FrameInfo));
    return SUCCESS;
}

static ERRORTYPE config_EncodedStream_By_VencOutputBuffer(EncodedStream* pDst, VencOutputBuffer* pSrc)
{
    if (pDst == NULL || pSrc == NULL)
    {
        aloge("fatal error! Empty pointer in config_EncodedStream_By_VencOutputBuffer");
        return ERR_VENC_ILLEGAL_PARAM;
    }

    pDst->media_type = CDX_PacketVideo;
    pDst->nID = pSrc->nID;
    pDst->nFilledLen = pSrc->nSize0 + pSrc->nSize1;
    pDst->nTobeFillLen = pDst->nFilledLen;
    pDst->nTimeStamp = pSrc->nPts;
    pDst->nFlags = 0;
    if (pSrc->nFlag & VENC_BUFFERFLAG_KEYFRAME)
    {
        pDst->nFlags |= CEDARV_FLAG_KEYFRAME;
    }
    if (pSrc->nFlag & VENC_BUFFERFLAG_EOS)
    {
        pDst->nFlags |= CEDARV_FLAG_EOS;
    }
    if (pSrc->nFlag & VENC_BUFFERFLAG_THUMB)
    {
        pDst->nFlags |= CEDARV_FLAG_THUMB;
    }

    pDst->pBuffer = pSrc->pData0;
    pDst->nBufferLen = pSrc->nSize0;
    pDst->pBufferExtra = pSrc->pData1;
    pDst->nBufferExtraLen = pSrc->nSize1;
    pDst->pBufferExtra2 = pSrc->pData2;
    pDst->nBufferExtraLen2 = pSrc->nSize2;

    pDst->video_stream_type = VIDEO_TYPE_MAJOR;
    memcpy(&pDst->video_frame_info, &pSrc->frame_info, sizeof(FrameInfo));

    pDst->infoVersion = -1;
    pDst->pChangedStreamsInfo = NULL;
    pDst->duration = -1;

    return SUCCESS;
}

static ERRORTYPE freeVideoFrameInfoNode(VideoFrameInfoNode *pNode)
{
    free(pNode);
    return SUCCESS;
}

static ERRORTYPE VideoEncConfigVencInputBufferRoiParam(VIDEOENCDATATYPE *pVideoEncData, VencInputBuffer *pEncBuf)
{
    int i=0;
    int cfg_num = ARRAY_SIZE(pEncBuf->roi_param);

    pEncBuf->bUseInputBufferRoi = 0;
    if (!list_empty(&pVideoEncData->mRoiCfgList))
    {
        //alogd("set frame roi");
        VEncRoiCfgNode *pEntry, *pTemp;
        list_for_each_entry_safe(pEntry, pTemp, &pVideoEncData->mRoiCfgList, mList)
        {
            if (i >= cfg_num)
            {
                break;
            }
            pEncBuf->roi_param[i].bEnable = pEntry->mROI.bEnable;
            pEncBuf->roi_param[i].index = pEntry->mROI.Index;
            pEncBuf->roi_param[i].nQPoffset = pEntry->mROI.Qp;
            pEncBuf->roi_param[i].roi_abs_flag = pEntry->mROI.bAbsQp;
            pEncBuf->roi_param[i].sRect.nLeft = pEntry->mROI.Rect.X;
            pEncBuf->roi_param[i].sRect.nTop = pEntry->mROI.Rect.Y;
            pEncBuf->roi_param[i].sRect.nWidth = pEntry->mROI.Rect.Width;
            pEncBuf->roi_param[i].sRect.nHeight = pEntry->mROI.Rect.Height;
            if(pEntry->mROI.bEnable)
            {
                pEncBuf->bUseInputBufferRoi = 1;
            }
            i++;
        }
    }

    return SUCCESS;
}

//QPMap
/*
static ERRORTYPE checkMbMode(PARAM_IN VIDEOENCDATATYPE *pVideoEncData)
{
    if (((PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type) || (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)) \
        && pVideoEncData->mMBmode.mMBmodeCtrl.mode_ctrl_en && pVideoEncData->mMBmode.mb_num)
    {
        //alogd("checkmb mode,num=%d", pVideoEncData->mMBmode.mb_num);
        if (pVideoEncData->pCedarV)
        {
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamMBModeCtrl, &pVideoEncData->mMBmode.mMBmodeCtrl);
        }
    }

    return SUCCESS;
}
*/
/**
 * only for dynamic attribute updating. Take effect immediately.
 * must call it during vencoder working.
 */
static ERRORTYPE VideoEncUpdateQp(
        PARAM_IN VIDEOENCDATATYPE *pVideoEncData,
        PARAM_IN VENC_CHN_ATTR_S *pChnAttr)
{
    VENC_CHN_ATTR_S *pCurAttr = &pVideoEncData->mEncChnAttr;
    VENC_RC_PARAM_S *pCurRcParam = &pVideoEncData->mRcParam;
    ERRORTYPE eError = SUCCESS;
    int ret;
    if(pCurAttr->RcAttr.mRcMode == pChnAttr->RcAttr.mRcMode)
    {
        switch(pChnAttr->RcAttr.mRcMode)
        {
            case VENC_RC_MODE_H264CBR:
            {
                break;
            }
            case VENC_RC_MODE_H264VBR:
            {
                break;
            }
            case VENC_RC_MODE_H264FIXQP:
            {
                if(pCurAttr->RcAttr.mAttrH264FixQp.mIQp != pChnAttr->RcAttr.mAttrH264FixQp.mIQp
                    || pCurAttr->RcAttr.mAttrH264FixQp.mPQp != pChnAttr->RcAttr.mAttrH264FixQp.mPQp)
                {
                    VencFixQP stH264FixQp;
                    stH264FixQp.bEnable = 1;
                    stH264FixQp.nIQp = pChnAttr->RcAttr.mAttrH264FixQp.mIQp;
                    stH264FixQp.nPQp = pChnAttr->RcAttr.mAttrH264FixQp.mPQp;
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264FixQP, (void*)&stH264FixQp);
                    if (0 == ret)
                    {
                        pCurAttr->RcAttr.mAttrH264FixQp.mIQp = pChnAttr->RcAttr.mAttrH264FixQp.mIQp;
                        pCurAttr->RcAttr.mAttrH264FixQp.mPQp = pChnAttr->RcAttr.mAttrH264FixQp.mPQp;
                    }
                    else
                    {
                        eError = FAILURE;
                    }
                }
                break;
            }
            case VENC_RC_MODE_H264ABR:
            {
                if(pCurAttr->RcAttr.mAttrH264Abr.mMaxQp != pChnAttr->RcAttr.mAttrH264Abr.mMaxQp
                    || pCurAttr->RcAttr.mAttrH264Abr.mMinQp != pChnAttr->RcAttr.mAttrH264Abr.mMinQp)
                {
                    VencQPRange QpRange;
                    memset(&QpRange, 0, sizeof(QpRange));
                    QpRange.nMaxqp = pChnAttr->RcAttr.mAttrH264Abr.mMaxQp;
                    QpRange.nMinqp = pChnAttr->RcAttr.mAttrH264Abr.mMinQp;
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                    if (0 == ret)
                    {
                        pCurAttr->RcAttr.mAttrH264Abr.mMaxQp = pChnAttr->RcAttr.mAttrH264Abr.mMaxQp;
                        pCurAttr->RcAttr.mAttrH264Abr.mMinQp = pChnAttr->RcAttr.mAttrH264Abr.mMinQp;
                    }
                    else
                    {
                        eError = FAILURE;
                    }
                }
                break;
            }
            case VENC_RC_MODE_H265CBR:
            {
                break;
            }
            case VENC_RC_MODE_H265VBR:
            {
                break;
            }
            case VENC_RC_MODE_H265FIXQP:
            {
                if(pCurAttr->RcAttr.mAttrH265FixQp.mIQp != pChnAttr->RcAttr.mAttrH265FixQp.mIQp
                    || pCurAttr->RcAttr.mAttrH265FixQp.mPQp != pChnAttr->RcAttr.mAttrH265FixQp.mPQp)
                {
                    VencFixQP stH265FixQp;
                    stH265FixQp.bEnable = 1;
                    stH265FixQp.nIQp = pChnAttr->RcAttr.mAttrH265FixQp.mIQp;
                    stH265FixQp.nPQp = pChnAttr->RcAttr.mAttrH265FixQp.mPQp;
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264FixQP, (void*)&stH265FixQp);
                    if (0 == ret)
                    {
                        pCurAttr->RcAttr.mAttrH265FixQp.mIQp = pChnAttr->RcAttr.mAttrH265FixQp.mIQp;
                        pCurAttr->RcAttr.mAttrH265FixQp.mPQp = pChnAttr->RcAttr.mAttrH265FixQp.mPQp;
                    }
                    else
                    {
                        eError = FAILURE;
                    }
                }
                break;
            }
            case VENC_RC_MODE_H265ABR:
            {
                if(pCurAttr->RcAttr.mAttrH265Abr.mMaxQp != pChnAttr->RcAttr.mAttrH265Abr.mMaxQp
                    || pCurAttr->RcAttr.mAttrH265Abr.mMinQp != pChnAttr->RcAttr.mAttrH265Abr.mMinQp)
                {
                    VencQPRange QpRange;
                    memset(&QpRange, 0, sizeof(QpRange));
                    QpRange.nMaxqp = pChnAttr->RcAttr.mAttrH265Abr.mMaxQp;
                    QpRange.nMinqp = pChnAttr->RcAttr.mAttrH265Abr.mMinQp;
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                    if (0 == ret)
                    {
                        pCurAttr->RcAttr.mAttrH265Abr.mMaxQp = pChnAttr->RcAttr.mAttrH265Abr.mMaxQp;
                        pCurAttr->RcAttr.mAttrH265Abr.mMinQp = pChnAttr->RcAttr.mAttrH265Abr.mMinQp;
                    }
                    else
                    {
                        eError = FAILURE;
                    }
                }
                break;
            }
            case VENC_RC_MODE_MJPEGFIXQP:
            {
                if(pCurAttr->RcAttr.mAttrMjpegeFixQp.mQfactor != pChnAttr->RcAttr.mAttrMjpegeFixQp.mQfactor)
                {
                    int jpegQuality = pChnAttr->RcAttr.mAttrMjpegeFixQp.mQfactor;
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegQuality, &jpegQuality);
                    if (0 == ret)
                    {
                        pCurAttr->RcAttr.mAttrMjpegeFixQp.mQfactor = pChnAttr->RcAttr.mAttrMjpegeFixQp.mQfactor;
                    }
                    else
                    {
                        eError = FAILURE;
                    }
                }
                break;
            }
            default:
            {
                //alogd("fatal error! other rc mode[0x%x] don't need set qp!", pChnAttr->RcAttr.mRcMode);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! DstRcMode[0x%x]!=Cur[0x%x], wrong setting!", pChnAttr->RcAttr.mRcMode, pCurAttr->RcAttr.mRcMode);
        eError = ERR_VENC_NOT_SUPPORT;
    }

    return eError;
}

ERRORTYPE VideoEncGetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (pPortDef->nPortIndex == pVideoEncData->sInPortDef.nPortIndex)
    {
        memcpy(pPortDef, &pVideoEncData->sInPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else if (pPortDef->nPortIndex == pVideoEncData->sOutPortDef.nPortIndex)
    {
        memcpy(pPortDef, &pVideoEncData->sOutPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else                                                    // error
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE VideoEncSetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    if (pPortDef->nPortIndex == pVideoEncData->sInPortDef.nPortIndex)
    {       // inport definition
        memcpy(&pVideoEncData->sInPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else if (pPortDef->nPortIndex == pVideoEncData->sOutPortDef.nPortIndex)
    {      // outport definition
        memcpy(&pVideoEncData->sOutPortDef, pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else                                                    // error
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE VideoEncGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_VENCODER_PORTS; i++)
    {
        if(pVideoEncData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pVideoEncData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE VideoEncSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_VENCODER_PORTS; i++)
    {
        if(pVideoEncData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pVideoEncData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE VideoEncGetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT MPP_CHN_S *pChn)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(pChn, &pVideoEncData->mMppChnInfo);
    return SUCCESS;
}

ERRORTYPE VideoEncSetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pVideoEncData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE VideoEncGetChannelFd(PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT int *pChnFd)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pChnFd = pVideoEncData->mOutputFrameNotifyPipeFds[0];
    return SUCCESS;
}

ERRORTYPE VideoEncGetTunnelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = ERR_VENC_UNEXIST;

    if(pVideoEncData->sInPortTunnelInfo.nPortIndex == pTunnelInfo->nPortIndex)
    {
        memcpy(pTunnelInfo, &pVideoEncData->sInPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else if(pVideoEncData->sOutPortTunnelInfo.nPortIndex == pTunnelInfo->nPortIndex)
    {
        memcpy(pTunnelInfo, &pVideoEncData->sOutPortTunnelInfo, sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_VENC_UNEXIST;
    }
    return eError;
}

ERRORTYPE VideoEncGetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_CHN_ATTR_S *pChnAttr)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    //copy_VENC_CHN_ATTR_S(pChnAttr, &pVideoEncData->mEncChnAttr);
    *pChnAttr = pVideoEncData->mEncChnAttr;
    return SUCCESS;
}

ERRORTYPE VideoEncSetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_CHN_ATTR_S *pChnAttr)
{
    int ret;
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (pChnAttr->VeAttr.mOnlineEnable)
    {
        alogd("vencChn[%d] enable online!", pVideoEncData->mMppChnInfo.mChnId);
    }

#if (AWCHIP == AW_V853)
    if (TRUE == pChnAttr->EncppAttr.mbEncppEnable)
    {
        unsigned int SrcPicWidthAlign = ALIGN(pChnAttr->VeAttr.SrcPicWidth, 16);
        unsigned int SrcPicHeightAlign = ALIGN(pChnAttr->VeAttr.SrcPicHeight, 16);
        alogv("VencChn[%d] update SrcPic Width:%d(%d), Height:%d(%d)", pVideoEncData->mMppChnInfo.mChnId, pChnAttr->VeAttr.SrcPicWidth, SrcPicWidthAlign, pChnAttr->VeAttr.SrcPicHeight, SrcPicHeightAlign);
        pChnAttr->VeAttr.SrcPicWidth = SrcPicWidthAlign;
        pChnAttr->VeAttr.SrcPicHeight = SrcPicHeightAlign;
    }
#endif

    if(pVideoEncData->pCedarV && pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        //when vencLib is exist, only can change dynamic attribute.
        //changable param type: BitRate, Qp, IFrameInterval
        if(pChnAttr->VeAttr.Type == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            VENC_CHN_ATTR_S *pCurAttr = &pVideoEncData->mEncChnAttr;
            if(PT_H264 == pChnAttr->VeAttr.Type || PT_H265 == pChnAttr->VeAttr.Type)
            {
                // IFrameInterval control
                if (pCurAttr->VeAttr.MaxKeyInterval != pChnAttr->VeAttr.MaxKeyInterval)
                {
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamMaxKeyInterval, (void*)&pChnAttr->VeAttr.MaxKeyInterval);
                    if (0 == ret)
                    {
                        pCurAttr->VeAttr.MaxKeyInterval = pChnAttr->VeAttr.MaxKeyInterval;
                    }
                    else
                    {
                        eError = FAILURE;
                        alogw("VENCType[%d] update max key interval fail!", pChnAttr->VeAttr.Type);
                    }
                }
                // BitRate control
                unsigned int nBitRate1 = GetBitRateFromVENC_CHN_ATTR_S(pChnAttr);
                unsigned int nBitRate2 = GetBitRateFromVENC_CHN_ATTR_S(pCurAttr);
                if(nBitRate1 != nBitRate2)
                {
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamBitrate, (void*)&nBitRate1);
                    if (0 == ret)
                    {
                        change_VENC_CHN_ATTR_BitRate(pCurAttr, pChnAttr);
                    }
                    else
                    {
                        eError = FAILURE;
                        alogw("VENCType[%d] update bit rate fail!", pChnAttr->VeAttr.Type);
                    }
                }

                // Qp control
                eError = VideoEncUpdateQp(pVideoEncData, pChnAttr);
                if (SUCCESS != eError)
                {
                    eError = FAILURE;
                    alogw("VENCType[%d] update Qp fail!", pChnAttr->VeAttr.Type);
                }

                // GDC control
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamGdcConfig, &pChnAttr->GdcAttr);
                if (0 != ret)
                {
                    eError = FAILURE;
                    alogw("VENCType[%d] update GDC fail!", pChnAttr->VeAttr.Type);
                }

                if (pCurAttr->EncppAttr.mbEncppEnable != pChnAttr->EncppAttr.mbEncppEnable)
                {
                    alogd("veChn[%d], update EncppEnable old=%d new=%d", pVideoEncData->mMppChnInfo.mChnId, pCurAttr->EncppAttr.mbEncppEnable, pChnAttr->EncppAttr.mbEncppEnable);
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamEnableEncppSharp, (void*)&pChnAttr->EncppAttr.mbEncppEnable);
                    if (0 != ret)
                    {
                        eError = FAILURE;
                        alogw("VencChn[%d] VENCType[%d] update EnableEncppSharp fail!", pVideoEncData->mMppChnInfo.mChnId, pChnAttr->VeAttr.Type);
                    }
                    pCurAttr->EncppAttr.mbEncppEnable = pChnAttr->EncppAttr.mbEncppEnable;
                }
            }
            else if(PT_MJPEG == pChnAttr->VeAttr.Type)
            {
                // BitRate control
                unsigned int nBitRate1 = GetBitRateFromVENC_CHN_ATTR_S(pChnAttr);
                unsigned int nBitRate2 = GetBitRateFromVENC_CHN_ATTR_S(pCurAttr);
                if(nBitRate1 != nBitRate2)
                {
                    ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamBitrate, (void*)&nBitRate1);
                    if (0 == ret)
                    {
                        change_VENC_CHN_ATTR_BitRate(pCurAttr, pChnAttr);
                    }
                    else
                    {
                        eError = FAILURE;
                        alogw("VENCType[%d] update bit rate fail!", pChnAttr->VeAttr.Type);
                    }
                }

                // Qp control
                eError = VideoEncUpdateQp(pVideoEncData, pChnAttr);
                if (SUCCESS != eError)
                {
                    eError = FAILURE;
                    alogw("VENCType[%d] update Qp fail!", pChnAttr->VeAttr.Type);
                }

                // Dynamic change Video Encoder Picture Size, in StateExecuting just for JPEG and MJPEG.
                int  iPicEncDstWidth      = 0;
                int  iPicEncDstHeight     = 0;
                if ((pCurAttr->VeAttr.AttrMjpeg.mPicWidth  != pChnAttr->VeAttr.AttrMjpeg.mPicWidth)
                    || (pCurAttr->VeAttr.AttrMjpeg.mPicHeight != pChnAttr->VeAttr.AttrMjpeg.mPicHeight))
                {
                    alogd(" PT_MJPEG change encode Picture size from w*h [%d*%d] to w*h [%d*%d]."
                          ,pCurAttr->VeAttr.AttrMjpeg.mPicWidth
                          ,pCurAttr->VeAttr.AttrMjpeg.mPicHeight
                          ,pChnAttr->VeAttr.AttrMjpeg.mPicWidth
                          ,pChnAttr->VeAttr.AttrMjpeg.mPicHeight);

                    iPicEncDstWidth  = pChnAttr->VeAttr.AttrMjpeg.mPicWidth;
                    iPicEncDstHeight = pChnAttr->VeAttr.AttrMjpeg.mPicHeight;
                    if ((iPicEncDstWidth > 0) && (iPicEncDstHeight > 0))
                    {
                        if(pVideoEncData->mbCedarvVideoEncInitFlag)
                        {
                            VencReset(pVideoEncData->pCedarV);
                            alogd("Venc Reset");

                            pVideoEncData->mBaseConfig.nDstWidth  = iPicEncDstWidth;
                            pVideoEncData->mBaseConfig.nDstHeight = iPicEncDstHeight;

                            ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamUpdateBaseInfo, (void*)&pVideoEncData->mBaseConfig);
                            if (0 == ret)
                            {
                                pCurAttr->VeAttr.AttrMjpeg.mPicWidth = iPicEncDstWidth;
                                pCurAttr->VeAttr.AttrMjpeg.mPicHeight = iPicEncDstHeight;
                            }
                            else
                            {
                                eError = FAILURE;
                                alogw("VENC mjpeg update base info fail!");
                            }
                        }
                    }
                    else
                    {
                        aloge("fatal error! Invalid picture size w*h [%d * %d]", iPicEncDstWidth, iPicEncDstHeight);
                        //pChnAttr->VeAttr.AttrMjpeg.mPicWidth = pCurAttr->VeAttr.AttrMjpeg.mPicWidth;
                        //pChnAttr->VeAttr.AttrMjpeg.mPicHeight = pCurAttr->VeAttr.AttrMjpeg.mPicHeight;
                        eError = ERR_VENC_NOT_SUPPORT;
                    }
                }

                // GDC control
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamGdcConfig, &pChnAttr->GdcAttr);
                if (0 != ret)
                {
                    eError = FAILURE;
                    alogw("VENCType[%d] update GDC fail!", pChnAttr->VeAttr.Type);
                }
            }
            else if(PT_JPEG == pChnAttr->VeAttr.Type)
            {
                // Dynamic change Video Encoder Picture Size, in StateExecuting just for JPEG and MJPEG.
                int  iPicEncDstWidth      = 0;
                int  iPicEncDstHeight     = 0;
                if ((pCurAttr->VeAttr.AttrJpeg.PicWidth  != pChnAttr->VeAttr.AttrJpeg.PicWidth)
                    || (pCurAttr->VeAttr.AttrJpeg.PicHeight != pChnAttr->VeAttr.AttrJpeg.PicHeight))
                {
                    alogd(" PT_JPEG change encode Picture size from w*h [%d*%d] to w*h [%d*%d] ."
                          ,pCurAttr->VeAttr.AttrJpeg.PicWidth
                          ,pCurAttr->VeAttr.AttrJpeg.PicHeight
                          ,pChnAttr->VeAttr.AttrJpeg.PicWidth
                          ,pChnAttr->VeAttr.AttrJpeg.PicHeight);

                    iPicEncDstWidth  = pChnAttr->VeAttr.AttrJpeg.PicWidth;
                    iPicEncDstHeight = pChnAttr->VeAttr.AttrJpeg.PicHeight;
                    if ((iPicEncDstWidth > 0) && (iPicEncDstHeight > 0))
                    {
                        if(pVideoEncData->mbCedarvVideoEncInitFlag)
                        {
                            VencReset(pVideoEncData->pCedarV);
                            alogd("Venc Reset");

                            pVideoEncData->mBaseConfig.nDstWidth  = iPicEncDstWidth;
                            pVideoEncData->mBaseConfig.nDstHeight = iPicEncDstHeight;

                            ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamUpdateBaseInfo, (void*)&pVideoEncData->mBaseConfig);
                            if (0 == ret)
                            {
                                pCurAttr->VeAttr.AttrJpeg.PicWidth = iPicEncDstWidth;
                                pCurAttr->VeAttr.AttrJpeg.PicHeight = iPicEncDstHeight;
                            }
                            else
                            {
                                eError = FAILURE;
                                alogw("VENC jpeg update base info fail!");
                            }
                        }
                    }
                    else
                    {
                        aloge("fatal error! Invalid picture size w*h [%d * %d]",iPicEncDstWidth ,iPicEncDstHeight);
                        //pChnAttr->VeAttr.AttrJpeg.PicWidth = pCurAttr->VeAttr.AttrJpeg.PicWidth;
                        //pChnAttr->VeAttr.AttrJpeg.PicHeight = pCurAttr->VeAttr.AttrJpeg.PicHeight;
                        eError = ERR_VENC_NOT_SUPPORT;
                    }
                }

                // GDC control
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamGdcConfig, &pChnAttr->GdcAttr);
                if (0 != ret)
                {
                    eError = FAILURE;
                    alogw("VENCType[%d] update GDC fail!", pChnAttr->VeAttr.Type);
                }
            }
            else
            {
                aloge("fatal error! unsupport encode type: %d", pChnAttr->VeAttr.Type);
            }
        }
        else
        {
            aloge("fatal error! venc type [0x%x]!=[0x%x]!", pChnAttr->VeAttr.Type, pVideoEncData->mEncChnAttr.VeAttr.Type);
            eError = ERR_VENC_NOT_SUPPORT;
        }
    }
    else
    {
        pVideoEncData->mEncChnAttr = *pChnAttr;
        eError = SUCCESS;
    }

    if(MM_PIXEL_FORMAT_BUTT == pVideoEncData->mEncChnAttr.VeAttr.PixelFormat)
    {
        alogd("VEnc component will be in compress mode!");
        pVideoEncData->is_compress_source = 1;
    }
    else
    {
        pVideoEncData->is_compress_source = 0;
    }

    return eError;
}

static ERRORTYPE VideoEncGetRcParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_RC_PARAM_S *pRcParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pRcParam = pVideoEncData->mRcParam;
    return SUCCESS;
}
static ERRORTYPE VideoEncSetRcParam(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN VENC_RC_PARAM_S *pRcParam)
{
    int ret;
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV && pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        //when vencLib is init, only can change dynamic attribute.
        //changable param type: Qp
        if(VENC_RC_MODE_H264CBR == pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
        {
            VENC_PARAM_H264_CBR_S *pCurH264Cbr = &pVideoEncData->mRcParam.ParamH264Cbr;
            VENC_PARAM_H264_CBR_S *pNewH264Cbr = &pRcParam->ParamH264Cbr;
            // Qp control
            if(pCurH264Cbr->mMaxQp != pNewH264Cbr->mMaxQp
                || pCurH264Cbr->mMinQp != pNewH264Cbr->mMinQp
                || pCurH264Cbr->mMaxPqp != pNewH264Cbr->mMaxPqp
                || pCurH264Cbr->mMinPqp != pNewH264Cbr->mMinPqp
                || pCurH264Cbr->mQpInit != pNewH264Cbr->mQpInit
                || pCurH264Cbr->mbEnMbQpLimit != pNewH264Cbr->mbEnMbQpLimit)
            {
                VencQPRange QpRange;
                memset(&QpRange, 0, sizeof(QpRange));
                QpRange.nMaxqp = pNewH264Cbr->mMaxQp;
                QpRange.nMinqp = pNewH264Cbr->mMinQp;
                QpRange.nMaxPqp = pNewH264Cbr->mMaxPqp;
                QpRange.nMinPqp = pNewH264Cbr->mMinPqp;
                QpRange.nQpInit = pNewH264Cbr->mQpInit;
                QpRange.bEnMbQpLimit = pNewH264Cbr->mbEnMbQpLimit;
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                if (0 == ret)
                {
                    *pCurH264Cbr = *pNewH264Cbr;
                }
                else
                {
                    alogw("VENC update Qp fail!");
                    eError = FAILURE;
                }
            }
        }
        else if(VENC_RC_MODE_H264VBR == pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
        {
            VENC_PARAM_H264_VBR_S *pCurH264Vbr = &pVideoEncData->mRcParam.ParamH264Vbr;
            VENC_PARAM_H264_VBR_S *pNewH264Vbr = &pRcParam->ParamH264Vbr;
            // Qp control
            if(pCurH264Vbr->mMaxQp != pNewH264Vbr->mMaxQp
              || pCurH264Vbr->mMinQp != pNewH264Vbr->mMinQp
              || pCurH264Vbr->mMaxPqp != pNewH264Vbr->mMaxPqp
              || pCurH264Vbr->mMinPqp != pNewH264Vbr->mMinPqp
              || pCurH264Vbr->mQpInit != pNewH264Vbr->mQpInit
              || pCurH264Vbr->mbEnMbQpLimit != pNewH264Vbr->mbEnMbQpLimit)
            {
                VencQPRange QpRange;
                memset(&QpRange, 0, sizeof(QpRange));
                QpRange.nMaxqp = pNewH264Vbr->mMaxQp;
                QpRange.nMinqp = pNewH264Vbr->mMinQp;
                QpRange.nMaxPqp = pNewH264Vbr->mMaxPqp;
                QpRange.nMinPqp = pNewH264Vbr->mMinPqp;
                QpRange.nQpInit = pNewH264Vbr->mQpInit;
                QpRange.bEnMbQpLimit = pNewH264Vbr->mbEnMbQpLimit;
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                if (0 == ret)
                {
                    *pCurH264Vbr = *pNewH264Vbr;
                }
                else
                {
                    alogw("VENC h264 vbr update Qp fail!");
                    eError = FAILURE;
                }
            }
        }
        else if(VENC_RC_MODE_H264QPMAP == pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
        {
            VENC_PARAM_H264_QPMAP_S *pCurH264QpMap = &pVideoEncData->mRcParam.ParamH264QpMap;
            VENC_PARAM_H264_QPMAP_S *pNewH264QpMap = &pRcParam->ParamH264QpMap;
            // Qp control
            if(pCurH264QpMap->mMaxQp != pNewH264QpMap->mMaxQp
              || pCurH264QpMap->mMinQp != pNewH264QpMap->mMinQp
              || pCurH264QpMap->mMaxPqp != pNewH264QpMap->mMaxPqp
              || pCurH264QpMap->mMinPqp != pNewH264QpMap->mMinPqp
              || pCurH264QpMap->mQpInit != pNewH264QpMap->mQpInit
              || pCurH264QpMap->mbEnMbQpLimit != pNewH264QpMap->mbEnMbQpLimit)
            {
                VencQPRange QpRange;
                memset(&QpRange, 0, sizeof(QpRange));
                QpRange.nMaxqp = pNewH264QpMap->mMaxQp;
                QpRange.nMinqp = pNewH264QpMap->mMinQp;
                QpRange.nMaxPqp = pNewH264QpMap->mMaxPqp;
                QpRange.nMinPqp = pNewH264QpMap->mMinPqp;
                QpRange.nQpInit = pNewH264QpMap->mQpInit;
                QpRange.bEnMbQpLimit = pNewH264QpMap->mbEnMbQpLimit;
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                if (0 == ret)
                {
                    *pCurH264QpMap = *pNewH264QpMap;
                }
                else
                {
                    alogw("VENC h264 QpMap update Qp fail!");
                    eError = FAILURE;
                }
            }
        }
        else if(VENC_RC_MODE_H265CBR == pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
        {
            VENC_PARAM_H265_CBR_S *pCurH265Cbr = &pVideoEncData->mRcParam.ParamH265Cbr;
            VENC_PARAM_H265_CBR_S *pNewH265Cbr = &pRcParam->ParamH265Cbr;
            // Qp control
            if(pCurH265Cbr->mMaxQp != pNewH265Cbr->mMaxQp
              || pCurH265Cbr->mMinQp != pNewH265Cbr->mMinQp
              || pCurH265Cbr->mMaxPqp != pNewH265Cbr->mMaxPqp
              || pCurH265Cbr->mMinPqp != pNewH265Cbr->mMinPqp
              || pCurH265Cbr->mQpInit != pNewH265Cbr->mQpInit
              || pCurH265Cbr->mbEnMbQpLimit != pNewH265Cbr->mbEnMbQpLimit)
            {
                VencQPRange QpRange;
                memset(&QpRange, 0, sizeof(QpRange));
                QpRange.nMaxqp = pNewH265Cbr->mMaxQp;
                QpRange.nMinqp = pNewH265Cbr->mMinQp;
                QpRange.nMaxPqp = pNewH265Cbr->mMaxPqp;
                QpRange.nMinPqp = pNewH265Cbr->mMinPqp;
                QpRange.nQpInit = pNewH265Cbr->mQpInit;
                QpRange.bEnMbQpLimit = pNewH265Cbr->mbEnMbQpLimit;
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                if (0 == ret)
                {
                    *pCurH265Cbr = *pNewH265Cbr;
                }
                else
                {
                    alogw("VENC h265 update Qp fail!");
                    eError = FAILURE;
                }
            }
        }
        else if(VENC_RC_MODE_H265VBR == pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
        {
            VENC_PARAM_H265_VBR_S *pCurH265Vbr = &pVideoEncData->mRcParam.ParamH265Vbr;
            VENC_PARAM_H265_VBR_S *pNewH265Vbr = &pRcParam->ParamH265Vbr;
            // Qp control
            if(pCurH265Vbr->mMaxQp != pNewH265Vbr->mMaxQp
              || pCurH265Vbr->mMaxQp != pNewH265Vbr->mMaxQp
              || pCurH265Vbr->mMaxPqp != pNewH265Vbr->mMaxPqp
              || pCurH265Vbr->mMinPqp != pNewH265Vbr->mMinPqp
              || pCurH265Vbr->mQpInit != pNewH265Vbr->mQpInit
              || pCurH265Vbr->mbEnMbQpLimit != pNewH265Vbr->mbEnMbQpLimit)
            {
                VencQPRange QpRange;
                memset(&QpRange, 0, sizeof(QpRange));
                QpRange.nMaxqp = pNewH265Vbr->mMaxQp;
                QpRange.nMinqp = pNewH265Vbr->mMinQp;
                QpRange.nMaxPqp = pNewH265Vbr->mMaxPqp;
                QpRange.nMinPqp = pNewH265Vbr->mMinPqp;
                QpRange.nQpInit = pNewH265Vbr->mQpInit;
                QpRange.bEnMbQpLimit = pNewH265Vbr->mbEnMbQpLimit;
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                if (0 == ret)
                {
                    *pCurH265Vbr = *pNewH265Vbr;
                }
                else
                {
                    alogw("VENC h265 vbr update Qp fail!");
                    eError = FAILURE;
                }
            }
        }
        else if(VENC_RC_MODE_H265QPMAP == pVideoEncData->mEncChnAttr.RcAttr.mRcMode)
        {
            VENC_PARAM_H265_QPMAP_S *pCurH265QpMap = &pVideoEncData->mRcParam.ParamH265QpMap;
            VENC_PARAM_H265_QPMAP_S *pNewH265QpMap = &pRcParam->ParamH265QpMap;
            // Qp control
            if(pCurH265QpMap->mMaxQp != pNewH265QpMap->mMaxQp
              || pCurH265QpMap->mMaxQp != pNewH265QpMap->mMaxQp
              || pCurH265QpMap->mMaxPqp != pNewH265QpMap->mMaxPqp
              || pCurH265QpMap->mMinPqp != pNewH265QpMap->mMinPqp
              || pCurH265QpMap->mQpInit != pNewH265QpMap->mQpInit
              || pCurH265QpMap->mbEnMbQpLimit != pNewH265QpMap->mbEnMbQpLimit)
            {
                VencQPRange QpRange;
                memset(&QpRange, 0, sizeof(QpRange));
                QpRange.nMaxqp = pNewH265QpMap->mMaxQp;
                QpRange.nMinqp = pNewH265QpMap->mMinQp;
                QpRange.nMaxPqp = pNewH265QpMap->mMaxPqp;
                QpRange.nMinPqp = pNewH265QpMap->mMinPqp;
                QpRange.nQpInit = pNewH265QpMap->mQpInit;
                QpRange.bEnMbQpLimit = pNewH265QpMap->mbEnMbQpLimit;
                ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264QPRange, (void*)&QpRange);
                if (0 == ret)
                {
                    *pCurH265QpMap = *pNewH265QpMap;
                }
                else
                {
                    alogw("VENC h265 QpMap update Qp fail!");
                    eError = FAILURE;
                }
            }
        }
        else
        {
            aloge("fatal error! unsupport venc rcMode[0x%x]!", pVideoEncData->mEncChnAttr.RcAttr.mRcMode);
            eError = ERR_VENC_NOT_SUPPORT;
        }
    }
    else
    {
        pVideoEncData->mRcParam = *pRcParam;
        eError = SUCCESS;
    }
    return eError;
}

ERRORTYPE VideoEncGetVencChnState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_CHN_STAT_S *pChnStat)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pthread_mutex_lock(&pVideoEncData->mStateLock);
    if(COMP_StateIdle == pVideoEncData->state || COMP_StateExecuting == pVideoEncData->state)
    {
        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
        //pChnStat->mLeftPics = ENC_FIFO_LEVEL - pVideoEncData->mBufQ.buf_unused;//?
        //pChnStat->mLeftPics = 0;
        int LeftPicsCounts = 0;
        struct list_head *pEntry;
        list_for_each(pEntry, &pVideoEncData->mBufQ.mReadyFrameList)
        {
            ++LeftPicsCounts;
        }
        pChnStat->mLeftPics = LeftPicsCounts;
        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

        aloge("need implement mLeftStreamBytes!");
        VbvInfo vbvInfo;
        memset(&vbvInfo, 0, sizeof(VbvInfo));
        VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamVbvInfo, (void*)&vbvInfo);
        pChnStat->mLeftStreamBytes = vbvInfo.coded_size;
        pChnStat->mLeftStreamFrames = VencGetValidOutputBufNum(pVideoEncData->pCedarV);
        if(pChnStat->mLeftStreamFrames > 0)
        {
            //because venclib only support one packBuf per frame even multi-slice.
            pChnStat->mCurPacks = 1;
        }
        pthread_mutex_lock(&pVideoEncData->mRecvPicControlLock);
        if(TRUE == pVideoEncData->mLimitedFramesFlag)
        {
            aloge("unsupported temporary!");
            pChnStat->mLeftRecvPics = pVideoEncData->mRecvPicParam.mRecvPicNum - pVideoEncData->mCurRecvPicNum; //
            pChnStat->mLeftEncPics = pChnStat->mLeftRecvPics + pChnStat->mLeftPics; //
        }
        else
        {
            pChnStat->mLeftRecvPics = 0;
            pChnStat->mLeftEncPics = 0;
        }
        pthread_mutex_unlock(&pVideoEncData->mRecvPicControlLock);

        eError = SUCCESS;
    }
    else
    {
        eError = ERR_VENC_NOT_PERM;
    }
    pthread_mutex_unlock(&pVideoEncData->mStateLock);
    return eError;
}

ERRORTYPE VideoEncGetStream(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT VENC_STREAM_S *pStream,
    PARAM_IN int nMilliSec)
{
    ERRORTYPE eError;
    int ret;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(COMP_StateIdle != pVideoEncData->state && COMP_StateExecuting != pVideoEncData->state)
    {
        alogw("call getStream in wrong state[0x%x]", pVideoEncData->state);
        return ERR_VENC_NOT_PERM;
    }
    if(pVideoEncData->mOutputPortTunnelFlag)
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_VENC_NOT_PERM;
    }
    if(pStream->mPackCount<1 || NULL==pStream->mpPack)
    {
        aloge("fatal error! wrong param[%d],[%p]!", pStream->mPackCount, pStream->mpPack);
        return ERR_VENC_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
_TryToGetOutFrame:
    if(!list_empty(&pVideoEncData->mReadyOutFrameList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pVideoEncData->mReadyOutFrameList, ENCODER_NODE_T, mList);
        config_VENC_STREAM_S_by_EncodedStream(pStream, &pEntry->stEncodedStream, pVideoEncData);
        list_move_tail(&pEntry->mList, &pVideoEncData->mUsedOutFrameList);
        alogv("output coded frame ID[%d] list: Ready -> Used", pEntry->stEncodedStream.nID);
        char tmpRdCh;
        read(pVideoEncData->mOutputFrameNotifyPipeFds[0], &tmpRdCh, 1);
        eError = SUCCESS;
    }
    else
    {
        if(0 == nMilliSec)
        {
            eError = ERR_VENC_BUF_EMPTY;
        }
        else if(nMilliSec < 0)
        {
            pVideoEncData->mWaitOutFrameFlag = TRUE;
            while(list_empty(&pVideoEncData->mReadyOutFrameList))
            {
                pthread_cond_wait(&pVideoEncData->mOutFrameCondition, &pVideoEncData->mOutFrameListMutex);
            }
            pVideoEncData->mWaitOutFrameFlag = FALSE;
            goto _TryToGetOutFrame;
        }
        else
        {
            pVideoEncData->mWaitOutFrameFlag = TRUE;
            alogv("set waitOutFrameFlag = TRUE");
            ret = pthread_cond_wait_timeout(&pVideoEncData->mOutFrameCondition, &pVideoEncData->mOutFrameListMutex, nMilliSec);
            if(ETIMEDOUT == ret)
            {
                alogv("VencChn[%d] wait output frame timeout[%d]ms, ret[%d]", pVideoEncData->mMppChnInfo.mChnId, nMilliSec, ret);
                eError = ERR_VENC_BUF_EMPTY;
                pVideoEncData->mWaitOutFrameFlag = FALSE;
            }
            else if(0 == ret)
            {
                pVideoEncData->mWaitOutFrameFlag = FALSE;
                goto _TryToGetOutFrame;
            }
            else
            {
                aloge("fatal error! pthread cond wait timeout ret[%d]", ret);
                eError = ERR_VENC_BUF_EMPTY;
                pVideoEncData->mWaitOutFrameFlag = FALSE;
            }
        }
    }
    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    return eError;
}

ERRORTYPE VideoEncReleaseStream(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN VENC_STREAM_S *pStream)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(COMP_StateIdle != pVideoEncData->state && COMP_StateExecuting != pVideoEncData->state)
    {
        alogw("call getStream in wrong state[0x%x]", pVideoEncData->state);
        return ERR_VENC_NOT_PERM;
    }
    if(pVideoEncData->mOutputPortTunnelFlag)
    {
        aloge("fatal error! can't call getStream() in tunnel mode!");
        return ERR_VENC_NOT_PERM;
    }
    if(pStream->mPackCount<1 || NULL==pStream->mpPack)
    {
        aloge("fatal error! wrong param[%d],[%p]!", pStream->mPackCount, pStream->mpPack);
        return ERR_VENC_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
    if(!list_empty(&pVideoEncData->mUsedOutFrameList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pVideoEncData->mUsedOutFrameList, ENCODER_NODE_T, mList);
        if (pVideoEncData->is_compress_source == 0)
        {
            if(  (pStream->mpPack[0].mpAddr0 == pEntry->stEncodedStream.pBuffer)
              && (pStream->mpPack[0].mpAddr1 == pEntry->stEncodedStream.pBufferExtra)
              )
            {
                VencOutputBuffer stOutputBuffer;
                config_VencOutputBuffer_By_EncodedStream(&stOutputBuffer, &pEntry->stEncodedStream);
                VencQueueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
                alogv("output coded frame ID[%d] list: Used -> Idle", pEntry->stEncodedStream.nID);
                eError = SUCCESS;
            }
            else
            {
                aloge("fatal error! venc stream[%p][%p] is not match UsedOutFrameList first entry[%p][%p]",
                    pStream->mpPack[0].mpAddr0, pStream->mpPack[0].mpAddr1,
                    pEntry->stEncodedStream.pBuffer, pEntry->stEncodedStream.pBufferExtra);
                eError = ERR_VENC_ILLEGAL_PARAM;
            }
        }
        else
        {
            if(  (pStream->mpPack[0].mpAddr0 == pEntry->stEncodedStream.pBuffer)
              && (pStream->mpPack[0].mLen0 == pEntry->stEncodedStream.nBufferLen)
              )
            {
                FRAMEDATATYPE frame;
                frame.addrY = (char*)pEntry->stEncodedStream.pBuffer;
                frame.info.timeStamp = pEntry->stEncodedStream.nTimeStamp;
                frame.info.bufferId = pEntry->stEncodedStream.nID;
                frame.info.size = pEntry->stEncodedStream.nBufferLen;

                eError = VideoEncBufferReleaseFrame(pVideoEncData->buffer_manager, &frame);
                if(eError != SUCCESS)
                {
                   aloge("fatal error! videoEncBufferReleaseFrame fail[%d]", eError);
                }
                list_add_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
            }
            else
            {
                aloge("fatal error! venc stream[%p] is not match UsedOutFrameList first entry[%p]",
                    pStream->mpPack[0].mpAddr0, pEntry->stEncodedStream.pBuffer);
                eError = ERR_VENC_ILLEGAL_PARAM;
            }
        }
        if(pVideoEncData->mWaitOutFrameReturnFlag)
        {
            pVideoEncData->mWaitOutFrameReturnFlag = FALSE;
            message_t msg;
            msg.command = VEncComp_OutputFrameReturn;
            put_message(&pVideoEncData->cmd_queue, &msg);
            alogv("VencChn[%d] send message VEncComp OutputFrameReturn", pVideoEncData->mMppChnInfo.mChnId);
        }
        if (pVideoEncData->mWaitOutFrameFullFlag)
        {
            int cnt = 0,cnt1 = 0;
            struct list_head *pList;
            list_for_each(pList, &pVideoEncData->mIdleOutFrameList)
            {
                cnt++;
            }            
            //added for fixing bug that one new encoded frm is added to readylist when out frm buffer 
            //is avaliable in case that decode process is suspended when vbv full is full.the new added
            //frm registered in readylist will not be fetched out since the operation of app,such as the 
            // reset operation including the component destruction.
            list_for_each(pList, &pVideoEncData->mReadyOutFrameList) 
            {
                cnt1++;
            }
            if ((cnt+cnt1)>=pVideoEncData->mFrameNodeNum)
            {
                pthread_cond_signal(&pVideoEncData->mOutFrameFullCondition);
            }
        }
    }
    else
    {
        alogw("Be careful! venc stream[%p][%p] is not find, maybe reset channel before call this function?", pStream->mpPack[0].mpAddr0, pStream->mpPack[0].mpAddr1);
        eError = ERR_VENC_ILLEGAL_PARAM;
    }
    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    return eError;
}

ERRORTYPE VideoEncGetStreamDuration(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT double *pTimeDuration)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pTimeDuration = pVideoEncData->mVbvBufTime;
    return SUCCESS;
}

ERRORTYPE VideoEncSetStreamDuration(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN double nTimeDuration)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pVideoEncData->mVbvBufTime = nTimeDuration;
    return SUCCESS;
}

ERRORTYPE VideoEncGetRoiCfg(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT VENC_ROI_CFG_S *pROI)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    VEncRoiCfgNode *pEntry;

    pthread_mutex_lock(&pVideoEncData->mRoiLock);
    list_for_each_entry(pEntry, &pVideoEncData->mRoiCfgList, mList)
    {
        if (pROI->Index == pEntry->mROI.Index)
        {
            memcpy(pROI, &pEntry->mROI, sizeof(VENC_ROI_CFG_S));
            return SUCCESS;
        }
    }
    pthread_mutex_unlock(&pVideoEncData->mRoiLock);

    return ERR_VENC_NOT_CONFIG;
}

ERRORTYPE VideoEncSetRoiCfg(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_ROI_CFG_S *pRoiCfg)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    BOOL findFlag = FALSE;

    if (NULL == pRoiCfg)
    {
        aloge("fatal error, pRoiCfg is NULL!");
        return ERR_VENC_NULL_PTR;
    }

    //int SrcPicWidth = pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth;
    //int SrcPicHeight = pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight;
    SIZE_S dstPicSize;
    getPicSizeFromVENC_ATTR_S(&pVideoEncData->mEncChnAttr.VeAttr, &dstPicSize);
    if ((pRoiCfg->Rect.X%16!=0) || (pRoiCfg->Rect.Y%16!=0) || (pRoiCfg->Rect.Width%16!=0) || (pRoiCfg->Rect.Height%16!=0)
        || (pRoiCfg->Rect.X+pRoiCfg->Rect.Width > dstPicSize.Width) || (pRoiCfg->Rect.Y+pRoiCfg->Rect.Height > dstPicSize.Height))
    {
        aloge("fatal error! ROI area setting is wrong with (%d,%d,%d,%d)! They must be integral multiple of 16 and is in SrcPic's internal area!",
            pRoiCfg->Rect.X, pRoiCfg->Rect.Y, pRoiCfg->Rect.Width, pRoiCfg->Rect.Height);
        return FAILURE;
    }

    pthread_mutex_lock(&pVideoEncData->mRoiLock);
    if (!list_empty(&pVideoEncData->mRoiCfgList))
    {
        VEncRoiCfgNode *pEntry, *pTemp;
        list_for_each_entry_safe(pEntry, pTemp, &pVideoEncData->mRoiCfgList, mList)
        {
            if (pEntry->mROI.Index == pRoiCfg->Index)
            {
                findFlag = TRUE;
                memcpy(&pEntry->mROI, pRoiCfg, sizeof(VENC_ROI_CFG_S));
                break;
            }
        }
    }

    if (FALSE == findFlag)
    {
        VEncRoiCfgNode *pRoiCfgNode = (VEncRoiCfgNode*)malloc(sizeof(VEncRoiCfgNode));
        if (pRoiCfgNode != NULL)
        {
            memcpy(&pRoiCfgNode->mROI, pRoiCfg, sizeof(VENC_ROI_CFG_S));
            list_add_tail(&pRoiCfgNode->mList, &pVideoEncData->mRoiCfgList);
        }
        else
        {
            aloge("fatal error, malloc RoiCfgNode failed! size=%d", sizeof(VEncRoiCfgNode));
            pthread_mutex_unlock(&pVideoEncData->mRoiLock);
            return ERR_VENC_NOMEM;
        }
    }
    pthread_mutex_unlock(&pVideoEncData->mRoiLock);

    if (pVideoEncData->mOnlineEnableFlag)
    {
        VencROIConfig sRoiConfig;
        memset(&sRoiConfig, 0, sizeof(VencROIConfig));
        sRoiConfig.bEnable = pRoiCfg->bEnable;
        sRoiConfig.index = pRoiCfg->Index;
        sRoiConfig.nQPoffset = pRoiCfg->Qp;
        sRoiConfig.roi_abs_flag = pRoiCfg->bAbsQp;
        sRoiConfig.sRect.nLeft = pRoiCfg->Rect.X;
        sRoiConfig.sRect.nTop = pRoiCfg->Rect.Y;
        sRoiConfig.sRect.nWidth = pRoiCfg->Rect.Width;
        sRoiConfig.sRect.nHeight = pRoiCfg->Rect.Height;
        alogv("ROIConfig en:%d idx[%d]Qp[%d]AbsQp[%d] X[%d]Y[%d]W[%d]H[%d]",
            pRoiCfg->bEnable, pRoiCfg->Index, pRoiCfg->Qp, pRoiCfg->bAbsQp,
            pRoiCfg->Rect.X, pRoiCfg->Rect.Y, pRoiCfg->Rect.Width, pRoiCfg->Rect.Height);
        int ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamROIConfig, &sRoiConfig);
        if (0 != ret)
        {
            aloge("fatal error, ROIConfig failed! ret=%d", ret);
            return FAILURE;
        }
    }

    return SUCCESS;
}

ERRORTYPE VideoEncGetRoiBgFrameRate(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT VENC_ROIBG_FRAME_RATE_S *pRoiBgFrameRate)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(NULL == pRoiBgFrameRate)
    {
        aloge("fatal error! null ptr!");
        return ERR_VENC_NULL_PTR;
    }
    pthread_mutex_lock(&pVideoEncData->mRoiLock);
    *pRoiBgFrameRate = pVideoEncData->mRoiBgFrameRate;
    pthread_mutex_unlock(&pVideoEncData->mRoiLock);
    return eError;
}

ERRORTYPE VideoEncSetRoiBgFrameRate(PARAM_IN COMP_HANDLETYPE hComponent,
                    PARAM_IN VENC_ROIBG_FRAME_RATE_S *pRoiBgFrameRate)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(NULL == pRoiBgFrameRate)
    {
        aloge("fatal error! null ptr!");
        return ERR_VENC_NULL_PTR;
    }
    pthread_mutex_lock(&pVideoEncData->mRoiLock);
    pVideoEncData->mRoiBgFrameRate = *pRoiBgFrameRate;

    if(pVideoEncData->pCedarV)
    {
        if(PT_H264 != pVideoEncData->mEncChnAttr.VeAttr.Type &&
            PT_H265 != pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            alogw("may be venc[%d] do not support RoiGbFrameRate!", pVideoEncData->mEncChnAttr.VeAttr.Type);
        }
        VencAlterFrameRateInfo stInfo;
        memset(&stInfo, 0, sizeof(VencAlterFrameRateInfo));
        stInfo.bEnable = 1;
        stInfo.bUseUserSetRoiInfo = 0;
        stInfo.sRoiBgFrameRate.nSrcFrameRate = pVideoEncData->mRoiBgFrameRate.mSrcFrmRate;
        stInfo.sRoiBgFrameRate.nDstFrameRate = pVideoEncData->mRoiBgFrameRate.mDstFrmRate;
        int ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamAlterFrame,(void*)&stInfo);
        if(0 == ret)
        {
            eError = SUCCESS;
        }
        else
        {
            aloge("fatal error! VENC IndexParamAlterFrame fail!");
            eError = ERR_VENC_BUSY;
        }
    }
    else
    {
        aloge("fatal error! why cedarc is not created?");
        eError = ERR_VENC_NULL_PTR;
    }
    pthread_mutex_unlock(&pVideoEncData->mRoiLock);
    return eError;
}

/*static __inline VENC_OVERLAY_ARGB_TYPE map_BitMapColor_Venc_ARGB_TYPE(VENC_OVERLAY_BITMAP_COLOR_E bitmapColor)
{
    VENC_OVERLAY_ARGB_TYPE argb_type;
    switch (bitmapColor)
    {
    case BITMAP_COLOR_ARGB8888:
        argb_type = VENC_OVERLAY_ARGB8888;
        break;

    case BITMAP_COLOR_ARGB4444:
        argb_type = VENC_OVERLAY_ARGB4444;
        break;

    case BITMAP_COLOR_ARGB1555:
        argb_type = VENC_OVERLAY_ARGB1555;
        break;

    default:
        argb_type = VENC_OVERLAY_ARGB8888;
        break;
    }
    return argb_type;
}

//from low to high
static ERRORTYPE orderRegionByPriority(VENC_OVERLAY_INFO *pOverlayInfo)
{
    int i, j;
    int num;
    VENC_OVERLAY_REGION_INFO_S tmpRegion;

    num = pOverlayInfo->regionNum;
    if (num > 1)
    {
        for (i=0; i < num-1; i++)
        {
            for (j=0; j< num-i-1; j++)
            {
                if (pOverlayInfo->region[j].nPriority > pOverlayInfo->region[j+1].nPriority)
                {
                    memcpy(&tmpRegion, &pOverlayInfo->region[j], sizeof(VENC_OVERLAY_REGION_INFO_S));
                    memcpy(&pOverlayInfo->region[j], &pOverlayInfo->region[j+1], sizeof(VENC_OVERLAY_REGION_INFO_S));
                    memcpy(&pOverlayInfo->region[j+1], &tmpRegion, sizeof(VENC_OVERLAY_REGION_INFO_S));
                }
            }
        }
    }
    return SUCCESS;
}*/

static ERRORTYPE VideoEncSetOsd(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN VencOverlayInfoS *pOverlayInfo)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (NULL == pOverlayInfo)
    {
        aloge("fatal error! null pointer in function para");
        return FAILURE;
    }
    pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
    if (pVideoEncData->pCedarV != NULL && pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        //pthread_mutex_lock(&pVideoEncData->mVencOverlayLock);
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSetOverlay, (void*)pOverlayInfo);
        //pthread_mutex_unlock(&pVideoEncData->mVencOverlayLock);
    }
    else
    {
        aloge("fatal error! VencChn[%d] has no vencLib[%p-%d]", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->pCedarV, pVideoEncData->mbCedarvVideoEncInitFlag);
    }
    pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
    return SUCCESS;
}

static ERRORTYPE VideoEncSetVEFreq(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nFreq)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE*) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pVideoEncData->mVEFreq = nFreq;
    if(nFreq > 0)
    {
        //if(pVideoEncData->mVeOpsS && pVideoEncData->mpVeOpsSelf)
        {
            alogd("venc set VE freq to [%d]MHz", nFreq);
            //CdcVeSetSpeed(pVideoEncData->mVeOpsS, pVideoEncData->mpVeOpsSelf, nFreq);
            VencSetFreq(pVideoEncData->pCedarV, nFreq);
        }
    }
    return SUCCESS;
}

ERRORTYPE VideoEncGetH264Vui(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_PARAM_H264_VUI_S *pVuiParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV)
    {
        VencH264VideoTiming timeInfo;
        memset(&timeInfo, 0, sizeof(VencH264VideoTiming));
        VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264VideoTiming, &timeInfo);
        pVideoEncData->mH264VuiParam.VuiTimeInfo.fixed_frame_rate_flag = timeInfo.fixed_frame_rate_flag;
        pVideoEncData->mH264VuiParam.VuiTimeInfo.num_units_in_tick = timeInfo.num_units_in_tick;
        pVideoEncData->mH264VuiParam.VuiTimeInfo.time_scale = timeInfo.time_scale;
    }
    else
    {
        aloge("fatal error! vencLib is not create?");
    }
    memcpy(pVuiParam, &pVideoEncData->mH264VuiParam, sizeof(VENC_PARAM_H264_VUI_S));
    return SUCCESS;
}

ERRORTYPE VideoEncSetH264Vui(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_PARAM_H264_VUI_S *pVuiParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        aloge("fatal error! The setting is invalid after CedarvVideoEnc Init.");
        return FAILURE;
    }
    memcpy(&pVideoEncData->mH264VuiParam, pVuiParam, sizeof(VENC_PARAM_H264_VUI_S));
    return SUCCESS;
}

ERRORTYPE VideoEncGetH265Vui(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_PARAM_H265_VUI_S *pVuiParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV)
    {
        VencH265TimingS timeInfo;
        memset(&timeInfo, 0, sizeof(VencH265TimingS));
        VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamH265Timing, &timeInfo);
        pVideoEncData->mH265VuiParam.VuiTimeInfo.timing_info_present_flag = timeInfo.timing_info_present_flag;
        pVideoEncData->mH265VuiParam.VuiTimeInfo.num_units_in_tick = timeInfo.num_units_in_tick;
        pVideoEncData->mH265VuiParam.VuiTimeInfo.time_scale = timeInfo.time_scale;
        pVideoEncData->mH265VuiParam.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = timeInfo.num_ticks_poc_diff_one;
    }
    else
    {
        aloge("fatal error! vencLib is not create?");
    }
    memcpy(pVuiParam, &pVideoEncData->mH265VuiParam, sizeof(VENC_PARAM_H265_VUI_S));
    return SUCCESS;
}

ERRORTYPE VideoEncSetH265Vui(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_PARAM_H265_VUI_S *pVuiParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        aloge("fatal error! The setting is invalid after CedarvVideoEnc Init.");
        return FAILURE;
    }
    memcpy(&pVideoEncData->mH265VuiParam, pVuiParam, sizeof(VENC_PARAM_H265_VUI_S));
    return SUCCESS;
}

ERRORTYPE VideoEncGetJpegParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_PARAM_JPEG_S *pJpegParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    memcpy(pJpegParam, &pVideoEncData->mJpegParam, sizeof(VENC_PARAM_JPEG_S));
    return SUCCESS;
}

ERRORTYPE VideoEncSetJpegParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_PARAM_JPEG_S *pJpegParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        if(pVideoEncData->mJpegParam.Qfactor != pJpegParam->Qfactor)
        {
            alogd("jpeg Qfactor is change[%d]->[%d]", pVideoEncData->mJpegParam.Qfactor, pJpegParam->Qfactor);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegQuality, &pJpegParam->Qfactor);
        }
    }
    else
    {
        aloge("fatal error! vencLib is not create?");
    }
    memcpy(&pVideoEncData->mJpegParam, pJpegParam, sizeof(VENC_PARAM_JPEG_S));
    return SUCCESS;
}

ERRORTYPE VideoEncGetJpegExifInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_EXIFINFO_S *pJpegExifInfo)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    memcpy(pJpegExifInfo, &pVideoEncData->mJpegExifInfo, sizeof(VENC_EXIFINFO_S));
    return SUCCESS;
}

ERRORTYPE VideoEncSetJpegExifInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_EXIFINFO_S *pJpegExifInfo)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        if(0!=memcmp(&pVideoEncData->mJpegExifInfo, pJpegExifInfo, sizeof(VENC_EXIFINFO_S)))
        {
            alogd("jpeg exif info is not match.");
            EXIFInfo stExifInfo;
            memset(&stExifInfo, 0, sizeof(EXIFInfo));
            memcpy(stExifInfo.CameraMake, pJpegExifInfo->CameraMake, INFO_LENGTH);
            memcpy(stExifInfo.CameraModel, pJpegExifInfo->CameraModel, INFO_LENGTH);
            memcpy(stExifInfo.DateTime, pJpegExifInfo->DateTime, DATA_TIME_LENGTH);
            stExifInfo.ThumbWidth = pJpegExifInfo->ThumbWidth;
            stExifInfo.ThumbHeight = pJpegExifInfo->ThumbHeight;
            stExifInfo.Orientation = pJpegExifInfo->Orientation;
            stExifInfo.ExposureTime.num = NUMERATOR32(pJpegExifInfo->fr32ExposureTime);
            stExifInfo.ExposureTime.den = DENOMINATOR32(pJpegExifInfo->fr32ExposureTime);
            stExifInfo.ExposureBiasValue.num = pJpegExifInfo->ExposureBiasValueNum;
            stExifInfo.ExposureBiasValue.den = 1;
            stExifInfo.FNumber.num = NUMERATOR32(pJpegExifInfo->fr32FNumber);
            stExifInfo.FNumber.den = DENOMINATOR32(pJpegExifInfo->fr32FNumber);
            stExifInfo.ISOSpeed = pJpegExifInfo->ISOSpeed;
            stExifInfo.MeteringMode = pJpegExifInfo->MeteringMode;
            stExifInfo.FocalLength.num = NUMERATOR32(pJpegExifInfo->fr32FocalLength);
            stExifInfo.FocalLength.den = DENOMINATOR32(pJpegExifInfo->fr32FocalLength);
            stExifInfo.WhiteBalance = pJpegExifInfo->WhiteBalance;
            stExifInfo.enableGpsInfo = pJpegExifInfo->enableGpsInfo;
            stExifInfo.gps_latitude = pJpegExifInfo->gps_latitude;
            stExifInfo.gps_longitude = pJpegExifInfo->gps_longitude;
            stExifInfo.gps_altitude = pJpegExifInfo->gps_altitude;
            stExifInfo.gps_timestamp = pJpegExifInfo->gps_timestamp;
            memcpy(stExifInfo.gpsProcessingMethod, pJpegExifInfo->gpsProcessingMethod, GPS_PROCESS_METHOD_LENGTH);
            memcpy(stExifInfo.CameraSerialNum, pJpegExifInfo->CameraSerialNum, 128);
            stExifInfo.FocalLengthIn35mmFilm = pJpegExifInfo->FocalLengthIn35mmFilm;
            memcpy(stExifInfo.ImageName, pJpegExifInfo->ImageName, 128);
            memcpy(stExifInfo.ImageDescription, pJpegExifInfo->ImageDescription, 128);
            stExifInfo.thumb_quality = pJpegExifInfo->thumb_quality;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegExifInfo, (void*)&stExifInfo);
        }
    }
    else
    {
        aloge("fatal error! vencLib is not create?");
    }
    memcpy(&pVideoEncData->mJpegExifInfo, pJpegExifInfo, sizeof(VENC_EXIFINFO_S));
    return SUCCESS;
}

ERRORTYPE VideoEncGetJpegThumbBuffer(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_JPEG_THUMB_BUFFER_S *pJpegThumbBuffer)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        EXIFInfo exif;
        memset(&exif, 0, sizeof(EXIFInfo));
        VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamJpegExifInfo, &exif);
        pJpegThumbBuffer->ThumbAddrVir = exif.ThumbAddrVir;
        pJpegThumbBuffer->ThumbLen = exif.ThumbLen;
    }
    else
    {
        aloge("fatal error! vencLib is not create?");
    }
    return SUCCESS;
}
        
ERRORTYPE VideoEncGetHighPassFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VencHighPassFilter *pHightPassFilter)
{ 
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(NULL != pHightPassFilter)
    {
        memcpy(pHightPassFilter,&pVideoEncData->mVencHighPassFilter,sizeof(VencHighPassFilter));
    }

    return SUCCESS;
}
        

ERRORTYPE VideoEncSetHighPassFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencHighPassFilter *pHightPassFilter)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(NULL != pHightPassFilter)
    {
        memcpy(&pVideoEncData->mVencHighPassFilter,pHightPassFilter,sizeof(VencHighPassFilter));
    } 

    if(pVideoEncData->pCedarV != NULL)
    {
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamHighPassFilter, &pVideoEncData->mVencHighPassFilter);
    }
    
    return SUCCESS;
} 

ERRORTYPE VideoEncGetDayOrNight(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT int *DayOrNight)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

     *DayOrNight = pVideoEncData->DayOrNight; 
    
    return SUCCESS;
}


ERRORTYPE VideoEncSetDayOrNight(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int *DayOrNight)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pVideoEncData->DayOrNight = *DayOrNight; 
    
    if(pVideoEncData->pCedarV != NULL)
    {
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamIsNightCaseFlag, (void*)&pVideoEncData->DayOrNight);
    }
    return SUCCESS;
} 

ERRORTYPE VideoEncGetFrameRate(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_FRAME_RATE_S *pFrameRate)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pthread_mutex_lock(&pVideoEncData->mFrameRateLock);
    memcpy(pFrameRate, &pVideoEncData->mFrameRateInfo, sizeof(VENC_FRAME_RATE_S));
    pthread_mutex_unlock(&pVideoEncData->mFrameRateLock);
    return SUCCESS;
}

ERRORTYPE VideoEncSetFrameRate(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_FRAME_RATE_S *pFrameRate)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pVideoEncData->mFrameRateLock);
	if(pVideoEncData->state == COMP_StateExecuting)// executing state to adjst frm rate,to bkp old frm rate info
	{
		alogw("vencChn[%d] set_src_frm_rate:%d-%d-%d", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mFrameRateInfo.DstFrmRate,pVideoEncData->mFrameRateInfo.SrcFrmRate,pFrameRate->DstFrmRate);
		memcpy(&pVideoEncData->mPreFrameRateInfo, &pVideoEncData->mFrameRateInfo, sizeof(VENC_FRAME_RATE_S));
	}
    memcpy(&pVideoEncData->mFrameRateInfo, pFrameRate, sizeof(VENC_FRAME_RATE_S));
    if (pVideoEncData->mFrameRateInfo.DstFrmRate < pVideoEncData->mFrameRateInfo.SrcFrmRate)
    {
        alogd("Low framerate mode, dst:%d, src:%d, enable soft frame rate control!", pVideoEncData->mFrameRateInfo.DstFrmRate, pVideoEncData->mFrameRateInfo.SrcFrmRate);
        pVideoEncData->mSoftFrameRateCtrl.enable = TRUE;
        pVideoEncData->mSoftFrameRateCtrl.mBasePts = -1;
    }
    else
    {
        SoftFrameRateCtrlDestroy(&pVideoEncData->mSoftFrameRateCtrl);
        if(pVideoEncData->mFrameRateInfo.DstFrmRate > pVideoEncData->mFrameRateInfo.SrcFrmRate)
        {
            alogd("Be careful! dstFrameRate[%d] > srcFrameRate[%d]", pVideoEncData->mFrameRateInfo.DstFrmRate, pVideoEncData->mFrameRateInfo.SrcFrmRate);
            //pVideoEncData->mFrameRateInfo.DstFrmRate = pVideoEncData->mFrameRateInfo.SrcFrmRate;
        }
    }
    if(pVideoEncData->timeLapseEnable)
    {
        pVideoEncData->mCapTimeLapse.videoFrameIntervalUs = (double)1000000/pVideoEncData->mFrameRateInfo.DstFrmRate;
        alogd("Be careful! DstFrameRate is set now. Dst frame interval[%lf]us, timeLapseMode[%d]", pVideoEncData->mCapTimeLapse.videoFrameIntervalUs, pVideoEncData->mCapTimeLapse.recType);
    }

	/* Cancel the restriction, in order to adapt to the dynamic frame rate setting of VE Lib. */
	// if(pVideoEncData->state == COMP_StateIdle) // allow to adjust params of enc library only at idle state,added for dynamic frame rate condition. 
	{
		if (pVideoEncData->pCedarV != NULL)
		{
			//if ((PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type) || (PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type))
			{
				int nDstFrameRate = pFrameRate->DstFrmRate;
                alogd("vencChn[%d] set new framerate:%d", pVideoEncData->mMppChnInfo.mChnId, nDstFrameRate);
				VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamFramerate, &nDstFrameRate);
			}
		}
	}
    //init roiBgFrameRate now. 
    pVideoEncData->mRoiBgFrameRate.mSrcFrmRate = pVideoEncData->mFrameRateInfo.DstFrmRate;
    pVideoEncData->mRoiBgFrameRate.mDstFrmRate = pVideoEncData->mFrameRateInfo.DstFrmRate;
    pthread_mutex_unlock(&pVideoEncData->mFrameRateLock);
    return SUCCESS;
}

ERRORTYPE VideoEncGetTimeLapse(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT int64_t* pTimeLapse)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pTimeLapse = pVideoEncData->mCapTimeLapse.capFrameIntervalUs;
    return SUCCESS;
}

ERRORTYPE VideoEncSetTimeLapse(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int64_t* pTimeLapse)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    BOOL bPermitChange = FALSE;
    pthread_mutex_lock(&pVideoEncData->mStateLock);
    if(COMP_StateIdle == pVideoEncData->state)
    {
        bPermitChange = TRUE;
    }
    else if(COMP_StateExecuting == pVideoEncData->state || COMP_StatePause == pVideoEncData->state)
    {
        if(pVideoEncData->timeLapseEnable && COMP_RECORD_TYPE_TIMELAPSE==pVideoEncData->mCapTimeLapse.recType)
        {
            if(pTimeLapse!=NULL && *pTimeLapse > 0)
            {
                bPermitChange = TRUE;
            }
        }
    }
    if(FALSE == bPermitChange)
    {
        alogw("vencComp state[0x%x], timeLapseEnable[%d],type[%d], can't set timelapse!", pVideoEncData->state, pVideoEncData->timeLapseEnable, pVideoEncData->mCapTimeLapse.recType);
        eError = ERR_VENC_NOT_PERM;
        goto _err0;
    }
    pthread_mutex_lock(&pVideoEncData->mCapTimeLapseLock);
    if(pTimeLapse!=NULL && *pTimeLapse >= 0)
    {
        pVideoEncData->timeLapseEnable = TRUE;
        pVideoEncData->mCapTimeLapse.capFrameIntervalUs = *pTimeLapse;
        //if capFrameIntervalUs is 0, it means every input frame will be encoded, but reset its pts to play as another frame rate.
        if(pVideoEncData->mCapTimeLapse.capFrameIntervalUs > 0)
        {
            pVideoEncData->mCapTimeLapse.recType = COMP_RECORD_TYPE_TIMELAPSE;
        }
        else
        {
            pVideoEncData->mCapTimeLapse.recType = COMP_RECORD_TYPE_SLOW;
        }
        if(pVideoEncData->mFrameRateInfo.DstFrmRate > 0)
        {
            pVideoEncData->mCapTimeLapse.videoFrameIntervalUs = (double)1000000/pVideoEncData->mFrameRateInfo.DstFrmRate;
        }
        else
        {
            pVideoEncData->mCapTimeLapse.videoFrameIntervalUs = 40000;
            alogd("Be careful! timeLapseMode is [0x%x]! DstFrameRate is not set now, when set it, calculate dst frame interval!", pVideoEncData->mCapTimeLapse.recType);
        }
        //pVideoEncData->mCapTimeLapse.lastCapTimeUs = 0;
        //pVideoEncData->mCapTimeLapse.lastTimeStamp = 0;
        alogd("SetTimeLapse: captureIntervalUs=%lf, dstFrameIntervalUs=%lf, recType=%d",
            pVideoEncData->mCapTimeLapse.capFrameIntervalUs, pVideoEncData->mCapTimeLapse.videoFrameIntervalUs, pVideoEncData->mCapTimeLapse.recType);
    }
    else
    {
        pVideoEncData->timeLapseEnable = FALSE;
        pVideoEncData->mCapTimeLapse.capFrameIntervalUs = -1;
    }
    pthread_mutex_unlock(&pVideoEncData->mCapTimeLapseLock);
_err0:
    pthread_mutex_unlock(&pVideoEncData->mStateLock);
    return eError;
}

ERRORTYPE VideoEncGetCropCfg(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_CROP_CFG_S *pCropCfg)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    memcpy(pCropCfg, &pVideoEncData->mCropCfg, sizeof(VENC_CROP_CFG_S));
    return SUCCESS;
}

ERRORTYPE VideoEncSetCropCfg(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_CROP_CFG_S *pCropCfg)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    int SrcPicWidth = pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth;
    int SrcPicHeight = pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight;
    if ((pCropCfg->Rect.X%16!=0) || (pCropCfg->Rect.Y%8!=0) || (pCropCfg->Rect.Width%16!=0) || (pCropCfg->Rect.Height%8!=0)
        || (pCropCfg->Rect.X+pCropCfg->Rect.Width > SrcPicWidth) || (pCropCfg->Rect.Y+pCropCfg->Rect.Height > SrcPicHeight))
    {
        aloge("CropCfg is wrong with (%d,%d,%d,%d)! Must be multiple of 16, srcPicSize[%dx%d]!", pCropCfg->Rect.X, pCropCfg->Rect.Y,
            pCropCfg->Rect.Width, pCropCfg->Rect.Height, SrcPicWidth, SrcPicHeight);
        return FAILURE;
    }
    else
    {
        memcpy(&pVideoEncData->mCropCfg, pCropCfg, sizeof(VENC_CROP_CFG_S));
        return SUCCESS;
    }
}

ERRORTYPE VideoEncGetStreamBufInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_STREAM_BUF_INFO_S *pStreamBufInfo)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        alogw("need implement streamBufInfo");
        eError = ERR_VENC_NOT_SUPPORT;
    }
    else
    {
        aloge("fatal error! video encoder lib is not build currently!");
        eError = ERR_VENC_NOT_PERM;
    }
    return eError;
}

ERRORTYPE VideoEncGetSuperFrameCfg(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT VENC_SUPERFRAME_CFG_S* pSuperFrmParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pSuperFrmParam = pVideoEncData->mSuperFrmParam;
    return SUCCESS;
}

ERRORTYPE VideoEncSetSuperFrameCfg(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN VENC_SUPERFRAME_CFG_S* pSuperFrmParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV != NULL)
    {
        VencSuperFrameConfig stSuperFrameConfig;
        configVencSuperFrameConfigByVENC_SUPERFRAME_CFG_S(&stSuperFrameConfig, pSuperFrmParam);
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSuperFrameConfig, (void*)&stSuperFrameConfig);
    }
    else
    {
        aloge("fatal error! why venclib is NULL?");
        eError = ERR_VENC_SYS_NOTREADY;
    }
    pVideoEncData->mSuperFrmParam = *pSuperFrmParam;
    return eError;
}

ERRORTYPE VideoEncGetIntraRefreshParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VENC_PARAM_INTRA_REFRESH_S *pIntraRefresh)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pIntraRefresh = pVideoEncData->mEncIntraRefreshParam;
    return SUCCESS;
}

ERRORTYPE VideoEncSetIntraRefreshParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_PARAM_INTRA_REFRESH_S *pIntraRefresh)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV != NULL)
    {
        if (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            VencCyclicIntraRefresh stIntraRefresh;
            stIntraRefresh.bEnable = pIntraRefresh->bRefreshEnable;
            stIntraRefresh.nBlockNumber = pIntraRefresh->RefreshLineNum;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264CyclicIntraRefresh, (void*)&stIntraRefresh);
            pVideoEncData->mEncIntraRefreshParam = *pIntraRefresh;

            if(pIntraRefresh->bRefreshEnable)
            { //PIntraRefresh base on PFrameIntraEn, so if enable refresh, must enable PFrameIntraEn.
                unsigned char bPFrameIntraEn = 1;
                VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamPFrameIntraEn, (void*)&bPFrameIntraEn);
                switch(pVideoEncData->mEncChnAttr.VeAttr.Type)
                {
                    case PT_H264:
                        pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
                        break;
                    case PT_H265:
                        pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
                        break;
                    default:
                        aloge("fatal error! unsupport encode type:%d", pVideoEncData->mEncChnAttr.VeAttr.Type);
                        break;
                }
            }
        }
        else
        {
            aloge("fatal error! encodeType[0x%x] don't support intraRefresh", pVideoEncData->mEncChnAttr.VeAttr.Type);
        }
    }
    else
    {
        aloge("fatal error! why venclib is NULL?");
    }
    return eError;
}

ERRORTYPE VideoEncGetSmartPParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VencSmartFun *pSmartPParam)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pSmartPParam = pVideoEncData->mEncSmartPParam;
    return SUCCESS;
}

ERRORTYPE VideoEncSetSmartPParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencSmartFun *pSmartPParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV != NULL)
    {
        if ((PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type) || (PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type))
        {
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSmartFuntion, (void*)pSmartPParam);
            pVideoEncData->mEncSmartPParam = *pSmartPParam;
        }
        else
        {
            aloge("fatal error! encodeType[0x%x] don't support smartP", pVideoEncData->mEncChnAttr.VeAttr.Type);
        }
    }
    else
    {
        aloge("fatal error! why venclib is NULL?");
    }
    return eError;
}

ERRORTYPE VideoEncGetBrightness(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VencBrightnessS *pBrightness)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *pBrightness = pVideoEncData->mEncBrightness;
    return SUCCESS;
}

ERRORTYPE VideoEncSetBrightness(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencBrightnessS *pBrightness)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV != NULL)
    {
        if (PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamBright, (void*)pBrightness);
            pVideoEncData->mEncBrightness = *pBrightness;
        }
        else
        {
            aloge("fatal error! encodeType[0x%x] don't support to set brightness", pVideoEncData->mEncChnAttr.VeAttr.Type);
        }
    }
    else
    {
        aloge("fatal error! why venclib is NULL?");
    }
    return eError;
}

ERRORTYPE VideoEncExtraData(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VencHeaderData *pVencHeaderData)
{
    ERRORTYPE eError = SUCCESS;
    VENC_RESULT_TYPE vencRet = VENC_RESULT_ERROR;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(NULL == pVideoEncData->pCedarV)
    {
        if(pVideoEncData->is_compress_source == 0)
        {
            CedarvEncInit(pVideoEncData);
        }
    }
    if(pVideoEncData->pCedarV != NULL)
    {
        pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
        if(!pVideoEncData->mbCedarvVideoEncInitFlag)
        {
            eError = CedarvVideoEncInit(pVideoEncData);
            if(eError != SUCCESS)
            {
                aloge("fatal error! vdeclib init fail[0x%x]", eError);
                pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
                goto _exit0;
            }
            pVideoEncData->mbCedarvVideoEncInitFlag = TRUE;
        }
        pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);

        if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            vencRet = VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264SPSPPS, (void*)pVencHeaderData);    //VencHeaderData    spsppsInfo;
        }
        else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            vencRet = VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamH265Header, (void*)pVencHeaderData);
        }
        else
        {
            alogd("not support other venc type[0x%x]", pVideoEncData->mEncChnAttr.VeAttr.Type);
        }
        if(vencRet == VENC_RESULT_OK)
        {
            if(pVencHeaderData->pBuffer!=NULL && pVencHeaderData->nLength > 0)
            {
                if(NULL == pVideoEncData->mpVencHeaderData)
                {
                    pVideoEncData->mpVencHeaderData = (VencHeaderData*)malloc(sizeof(VencHeaderData));
                    if(pVideoEncData->mpVencHeaderData)
                    {
                        memset(pVideoEncData->mpVencHeaderData, 0, sizeof(VencHeaderData));
                    }
                    else
                    {
                        aloge("fatal error! malloc fail!");
                    }
                }
                if(pVideoEncData->mpVencHeaderData)
                {
                    if(pVideoEncData->mpVencHeaderData->pBuffer)
                    {
                        free(pVideoEncData->mpVencHeaderData->pBuffer);
                        pVideoEncData->mpVencHeaderData->pBuffer = NULL;
                    }
                    pVideoEncData->mpVencHeaderData->nLength = 0;

                    pVideoEncData->mpVencHeaderData->pBuffer = malloc(pVencHeaderData->nLength);
                    if(pVideoEncData->mpVencHeaderData->pBuffer)
                    {
                        memcpy(pVideoEncData->mpVencHeaderData->pBuffer, pVencHeaderData->pBuffer, pVencHeaderData->nLength);
                        pVideoEncData->mpVencHeaderData->nLength = pVencHeaderData->nLength;
                    }
                    else
                    {
                        aloge("fatal error! malloc fail!");
                    }
                }
            }
        }
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_VENC_NULL_PTR;
        //because in compressed mode, analysing key frame is under protection of mOutFrameListMutex.
        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        if(pVideoEncData->mpVencHeaderData)
        {
            if(pVideoEncData->mpVencHeaderData->pBuffer != NULL && pVideoEncData->mpVencHeaderData->nLength > 0)
            {
                pVencHeaderData->pBuffer = pVideoEncData->mpVencHeaderData->pBuffer;
                pVencHeaderData->nLength = pVideoEncData->mpVencHeaderData->nLength;
                eError = SUCCESS;
            }
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    }
_exit0:
    return eError;
}

ERRORTYPE VideoEncResetChannel(PARAM_IN COMP_HANDLETYPE hComponent, BOOL bForceReleaseOutFrameInNonTunnelMode)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->state != COMP_StateIdle)
    {
        aloge("fatal error! must reset channel in stateIdle!");
        return ERR_VENC_NOT_PERM;
    }

    // return input frames
    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
    // Both used and using list should be empty, otherwise a warning will be given.
    if (!list_empty(&pVideoEncData->mBufQ.mUsedFrameList))
    {
        int num = 0;
        VideoFrameInfoNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mBufQ.mUsedFrameList, mList)
        {
            num++;
        }
        aloge("fatal error! vencChn[%d] usedFrameList is NOT empty! left frames num = [%d] ", pVideoEncData->mMppChnInfo.mChnId, num);
    }
    if (!list_empty(&pVideoEncData->mBufQ.mUsingFrameList))
    {
        int num = 0;
        VideoFrameInfoNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mBufQ.mUsingFrameList, mList)
        {
            num++;
        }
        aloge("fatal error! vencChn[%d] usingFrameList is NOT empty! left frames num = [%d] ", pVideoEncData->mMppChnInfo.mChnId, num);
    }
    if (!list_empty(&pVideoEncData->mBufQ.mReadyFrameList))
    {
        int num = 0;
        VideoFrameInfoNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mBufQ.mReadyFrameList, mList)
        {
            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
            VideoEncSendBackInputFrame(pVideoEncData, &pEntry->VFrame);
            pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
            list_move_tail(&pEntry->mList, &pVideoEncData->mBufQ.mIdleFrameList);
            pVideoEncData->mBufQ.buf_unused++;
            alogv("release input orig frame ID[%d], list: Ready -> Idle, buf unused[%d]", pEntry->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
            num++;
        }
        alogd("vencChn[%d] return input orig [%d]frames list: Ready -> Idle", pVideoEncData->mMppChnInfo.mChnId, num);
    }
    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

    pthread_mutex_lock(&pVideoEncData->mRecvPicControlLock);
    pVideoEncData->mLimitedFramesFlag = FALSE;
    pVideoEncData->mRecvPicParam.mRecvPicNum = 0;
    pVideoEncData->mCurRecvPicNum = 0;
    pthread_mutex_unlock(&pVideoEncData->mRecvPicControlLock);

    // return output frames to venclib.
    if(FALSE == pVideoEncData->mOutputPortTunnelFlag)
    {
        //return output streams to venclib directly. Don't worry about user take streams,
        //if user return stream after it, return ERR_VENC_ILLEGAL_PARAM.
        //user must guarantee that he return all streams before call this function.
        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        if(!list_empty(&pVideoEncData->mUsedOutFrameList))
        {
            if(bForceReleaseOutFrameInNonTunnelMode)
            {
                alogd("Be careful! non-tunnel mode, force release usedOutFrameList");
                int cnt = 0;
                ENCODER_NODE_T *pEntry, *pTmp;
                list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mUsedOutFrameList, mList)
                {
                    VencOutputBuffer stOutputBuffer;
                    config_VencOutputBuffer_By_EncodedStream(&stOutputBuffer, &pEntry->stEncodedStream);
                    VencQueueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                    list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
                    alogv("output coded frame ID[%d] list: Used -> Idle", pEntry->stEncodedStream.nID);
                    cnt++;
                }
                alogw("Be careful! non-tunnel mode, force release [%d] usedOutFrames", cnt);
            }
            else
            {
                alogd("Be careful! non-tunnel mode, usedOutFrameList is not empty, need wait in future.");
            }
        }
        if(!list_empty(&pVideoEncData->mReadyOutFrameList))
        {
            if(bForceReleaseOutFrameInNonTunnelMode)
            {
                alogd("Be careful! non-tunnel mode, force release readyOutFrameList");
                ENCODER_NODE_T *pEntry, *pTmp;
                list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mReadyOutFrameList, mList)
                {
                    VencOutputBuffer stOutputBuffer;
                    config_VencOutputBuffer_By_EncodedStream(&stOutputBuffer, &pEntry->stEncodedStream);
                    VencQueueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                    list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
                    alogv("output coded frame ID[%d] list: Ready -> Idle", pEntry->stEncodedStream.nID);
                    char tmpRdCh;
                    read(pVideoEncData->mOutputFrameNotifyPipeFds[0], &tmpRdCh, 1);
                }
            }
            else
            {
                alogd("Be careful! non-tunnel mode, readyOutFrameList is not empty, need wait in future.");
            }
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    }
    else
    {
        //verify all output frames back to venclib.
        //when component turn to stateIdle, it will guarantee all output frames back.
        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        if(!list_empty(&pVideoEncData->mUsedOutFrameList))
        {
            alogd("Be careful! venc usedOutFrameList is not empty in tunnel mode!");
        }
        if(!list_empty(&pVideoEncData->mReadyOutFrameList))
        {
            aloge("fatal error! venc mReadyOutFrameList is not empty in tunnel mode!");
        }
        int cnt = 0;
        struct list_head *pList;
        list_for_each(pList, &pVideoEncData->mIdleOutFrameList)
        {
            cnt++;
        }
        if(cnt != pVideoEncData->mFrameNodeNum)
        {
            alogw("Be careful! venc output frames count not match [%d]!=[%d]", cnt, pVideoEncData->mFrameNodeNum);
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    }
    alogd("success");
    return SUCCESS;
}

ERRORTYPE VideoEncSetRecvPicParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VENC_RECV_PIC_PARAM_S *pRecvPicParam)
{
    //ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pthread_mutex_lock(&pVideoEncData->mRecvPicControlLock);
    if(pRecvPicParam && pRecvPicParam->mRecvPicNum > 0)
    {
        pVideoEncData->mLimitedFramesFlag = TRUE;
        pVideoEncData->mRecvPicParam = *pRecvPicParam;
    }
    else
    {
        pVideoEncData->mLimitedFramesFlag = FALSE;
        pVideoEncData->mRecvPicParam.mRecvPicNum = 0;
    }
    pVideoEncData->mCurRecvPicNum = 0;
    pthread_mutex_unlock(&pVideoEncData->mRecvPicControlLock);

    return SUCCESS;
}

ERRORTYPE VideoEncGetRefParam(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN VENC_PARAM_REF_S *pstRefParam
    )
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pstRefParam->Base = pVideoEncData->mRefParam.nBase;
    pstRefParam->bEnablePred = pVideoEncData->mRefParam.bRefBaseEn;
    pstRefParam->Enhance = pVideoEncData->mRefParam.nEnhance;

    return SUCCESS;
}

ERRORTYPE VideoEncSetRefParam(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN VENC_PARAM_REF_S *pstRefParam
    )
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(!pstRefParam)
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
        return eError;
    }
#if 0
    pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
    if(!pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        pVideoEncData->mRefParam.bAdvancedRefEn = 1;
        pVideoEncData->mRefParam.nBase = pstRefParam->Base;
        pVideoEncData->mRefParam.nEnhance = pstRefParam->Enhance;
        pVideoEncData->mRefParam.bRefBaseEn = pstRefParam->bEnablePred;
        eError = SUCCESS;
    }
    else
    {
        aloge("CAUTION: the video encoder had init, the set refparam is not work!!!");
        eError = ERR_VENC_INCORRECT_STATE_OPERATION;
    }
    pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
 #endif
    if(pstRefParam->Base > 0)
    {
        pVideoEncData->mRefParam.bAdvancedRefEn = 1;
    }
    else
    {
        pVideoEncData->mRefParam.bAdvancedRefEn = 0;
    }
    pVideoEncData->mRefParam.nBase = pstRefParam->Base;
    pVideoEncData->mRefParam.nEnhance = pstRefParam->Enhance;
    pVideoEncData->mRefParam.bRefBaseEn = pstRefParam->bEnablePred;
    eError = SUCCESS;
    return eError;
}


ERRORTYPE VideoEncGetColor2Grey(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN VENC_COLOR2GREY_S *pRecvPicParam
    )
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pRecvPicParam->bColor2Grey = pVideoEncData->mColor2GreyParam.bColor2Grey;
    return SUCCESS;
}


ERRORTYPE VideoEncSetColor2Grey(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN VENC_COLOR2GREY_S *pRecvPicParam
    )
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(!pRecvPicParam)
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
        return eError;
    }

    if(pVideoEncData->pCedarV)
    {
        if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            unsigned char OpenColor2Grey = pRecvPicParam->bColor2Grey?1:0;
            alogd("set Color2Grey [%d], in Venchn[%d]", OpenColor2Grey, pVideoEncData->mMppChnInfo.mChnId);
            if(0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamChmoraGray,(void*)&OpenColor2Grey))
            {
                pVideoEncData->mColor2GreyParam.bColor2Grey = pRecvPicParam->bColor2Grey;
                eError = SUCCESS;
            }
            else
            {
                eError = ERR_VENC_BUSY;
            }
        }
        else
        {
            alogd("the Color2Grey only support H264 and H265!");
            eError = ERR_VENC_NOT_SUPPORT; // no support to jpeg mjpeg.
        }
    }
    else
    {
        eError = ERR_VENC_NULL_PTR;
        pVideoEncData->mColor2GreyParam = *pRecvPicParam;
    }

    return eError;
}

static ERRORTYPE VideoEncGet2DFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT s2DfilterParam *p2DfilterParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (NULL == p2DfilterParam)
    {
        aloge("fatal error, invalid input param!");
        eError = ERR_VENC_ILLEGAL_PARAM;
    }
    else
    {
        memcpy(p2DfilterParam, &pVideoEncData->m2DfilterParam, sizeof(s2DfilterParam));
    }

    return eError;
}

static ERRORTYPE VideoEncSet2DFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN s2DfilterParam *p2DfilterParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    int ret;

    if((pVideoEncData->pCedarV != NULL) && (p2DfilterParam != NULL))
    {
        ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParam2DFilter, (void*)p2DfilterParam);
        if(ret != 0)
        {
            aloge("fatal error! set 2DFilter failed!");
            eError = FAILURE;
        }
        memcpy(&pVideoEncData->m2DfilterParam, p2DfilterParam, sizeof(s2DfilterParam));
    }

    return eError;
}

static ERRORTYPE VideoEncGet3DFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT s3DfilterParam *p3DfilterParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (NULL == p3DfilterParam)
    {
        aloge("fatal error, invalid input param!");
        eError = ERR_VENC_ILLEGAL_PARAM;
    }
    else
    {
        memcpy(p3DfilterParam, &pVideoEncData->m3DfilterParam, sizeof(s3DfilterParam));
    }

    return eError;
}

static ERRORTYPE VideoEncSet3DFilter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN s3DfilterParam *p3DfilterParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    int ret;

    if((pVideoEncData->pCedarV != NULL) && (p3DfilterParam != NULL))
    {
        ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParam3DFilterNew, (void*)p3DfilterParam);
        if(ret != 0)
        {
            aloge("fatal error! set 3DFilterNew failed!");
            eError = FAILURE;
        }
        memcpy(&pVideoEncData->m3DfilterParam, p3DfilterParam, sizeof(s3DfilterParam));
    }

    return eError;
}


ERRORTYPE VideoEncGetHorizonFlip(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT BOOL *bpHorizonFlipFlag)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    *bpHorizonFlipFlag = pVideoEncData->mHorizonFlipFlag;
    return SUCCESS;
}

ERRORTYPE VideoEncSetHorizonFlip(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT BOOL bHorizonFlipFlag)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        unsigned int OpenHorizonFlip = bHorizonFlipFlag?1:0;
        if(0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamHorizonFlip, (void*)&OpenHorizonFlip))
        {
            //alogd("set HorizonFlip [%d], in Venchn[%d]", OpenHorizonFlip, pVideoEncData->mMppChnInfo.mChnId);
            pVideoEncData->mHorizonFlipFlag = bHorizonFlipFlag;
            eError = SUCCESS;
        }
        else
        {
             eError = ERR_VENC_BUSY;
        }
    }
    else
    {
        eError = ERR_VENC_NULL_PTR;
        pVideoEncData->mHorizonFlipFlag = bHorizonFlipFlag;
    }

    return eError;
}

ERRORTYPE VideoEncSetAdaptiveIntraInP(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bAdaptiveIntraInPFlag)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pVideoEncData->pCedarV)
    {
        if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            unsigned char OpenAdaptiveIntraInPFlag = bAdaptiveIntraInPFlag?1:0;
            if(0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamAdaptiveIntraInP, (void*)&OpenAdaptiveIntraInPFlag))
            {
                //alogd("set AdaptiveIntraInP [%d], in Venchn[%d]", OpenAdaptiveIntraInPFlag, pVideoEncData->mMppChnInfo.mChnId);
                pVideoEncData->mAdaptiveIntraInPFlag = bAdaptiveIntraInPFlag;
                eError = SUCCESS;
            }
            else
            {
                eError = ERR_VENC_BUSY;
            }
        }
        else
        {
           aloge("fatal error, the AdaptiveIntraInP just for H265!");
           eError = ERR_VENC_NOT_SUPPORT;
        }
    }
    else
    {
        eError = ERR_VENC_NULL_PTR;
        pVideoEncData->mAdaptiveIntraInPFlag = bAdaptiveIntraInPFlag;
    }

    return eError;
}

ERRORTYPE VideoEncSetH264SVCSkip(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN VencH264SVCSkip *pSVCSkip)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pVideoEncData->pCedarV)
    {
        if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            if(0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264SVCSkip, (void*)pSVCSkip))
            {
                alogd("Venchn[%d] enable svcSkip:%d,%d", pVideoEncData->mMppChnInfo.mChnId, pSVCSkip->nTemporalSVC, pSVCSkip->nSkipFrame);
                eError = SUCCESS;
            }
            else
            {
                eError = ERR_VENC_BUSY;
            }
        }
        else
        {
           aloge("fatal error, SVC skip is just for H264!");
           eError = ERR_VENC_NOT_SUPPORT;
        }
    }
    else
    {
        aloge("fatal error! encLib is not created?");
        eError = ERR_VENC_NULL_PTR;
    }

    return eError;
}

ERRORTYPE VideoEncEnableNullSkip(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bEnable)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pVideoEncData->pCedarV)
    {
        unsigned int bEnableFlag = (unsigned int)bEnable;
        if(0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSetNullFrame, (void*)&bEnableFlag))
        {
            eError = SUCCESS;
        }
        else
        {
            aloge("fatal error, vencType[0x%x] don't support nullSkip!", pVideoEncData->mEncChnAttr.VeAttr.Type);
            eError = ERR_VENC_BUSY;
        }
    }
    else
    {
        aloge("fatal error, venclib is not created!");
        eError = ERR_VENC_NULL_PTR;
    }

    return eError;
}

ERRORTYPE VideoEncEnablePSkip(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bEnable)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pVideoEncData->pCedarV)
    {
        unsigned int bEnableFlag = (unsigned int)bEnable;
        if(0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSetPSkip, (void*)&bEnableFlag))
        {
            eError = SUCCESS;
        }
        else
        {
            aloge("fatal error, vencType[0x%x] don't support pSkip!", pVideoEncData->mEncChnAttr.VeAttr.Type);
            eError = ERR_VENC_BUSY;
        }
    }
    else
    {
        aloge("fatal error, venclib is not created!");
        eError = ERR_VENC_NULL_PTR;
    }

    return eError;
}

ERRORTYPE  VideoEncForbidDiscardingFrame(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bForbid)
{
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    // can NOT change the flag while ven component thread is executing
    if (pVideoEncData->state == COMP_StateExecuting)
    {
        aloge("fatal error, ven component thread is executing, can not change this flag! ");
        return FAILURE;
    }

    if (pVideoEncData->mOnlineEnableFlag && pVideoEncData->mbForbidDiscardingFrame)
    {
        aloge("fatal error, venc online, ForbidDiscardingFrame is not supported, please use offline channel.");
        return FAILURE;
    }

    alogd("forbidDiscardingFrame flag will change from %d to %d ", pVideoEncData->mbForbidDiscardingFrame, bForbid);
    pVideoEncData->mbForbidDiscardingFrame = bForbid;

    return SUCCESS;
}

ERRORTYPE VideoEncGetCacheState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT CacheState* pCacheState)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pVideoEncData->pCedarV != NULL) {
        VbvInfo vbvInfo;
        eError = VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamVbvInfo, (void*)&vbvInfo);
        pCacheState->mValidSize = vbvInfo.coded_size / 1024;
        pCacheState->mTotalSize = (vbvInfo.vbv_size-vbvInfo.maxFrameLen) / 1024;
        pCacheState->mValidSizePercent = 100 * pCacheState->mValidSize / pCacheState->mTotalSize;

		// struct list_head *pList;
		// int cnt1 = 0;
		// list_for_each(pList, &pVideoEncData->mReadyOutFrameList)
		// {
			// cnt1++;
		// }
		// int cnt2 = 0;
		// list_for_each(pList, &pVideoEncData->mUsedOutFrameList)
		// {
			// cnt2++;
		// }
		// aloge("vbv_buff_st:%d-%d-%d-%d-%d",vbvInfo.coded_size,vbvInfo.vbv_size,vbvInfo.coded_frame_num,cnt1,cnt2);
    }

    return eError;
}

ERRORTYPE VideoEncRequestIDR(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bInstant)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        if(0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamForceKeyFrame, (void*)0))
        {
            alogd("VeChn[%d] sdk force I frame", pVideoEncData->mMppChnInfo.mChnId);
            pVideoEncData->bForceKeyFrameFlag = TRUE;
            eError = SUCCESS;
        }
        else
        {
            eError = ERR_VENC_BUSY;
        }
    }
    else
    {
        eError = ERR_VENC_NULL_PTR;
    }
    return eError;
}

static ERRORTYPE VideoEncSetMotionSearchParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencMotionSearchParam *pMotionParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    int ret;
    pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
    if(pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamMotionSearchParam, (void*)pMotionParam);
        if(ret != 0)
        {
            aloge("fatal error! check code!");
            eError = FAILURE;
        }
        pVideoEncData->mbEnableMotionSearch = pMotionParam->en_motion_search;
        if (TRUE == pVideoEncData->mbEnableMotionSearch)
        {
            if (pVideoEncData->mMotionResult.region)
            {
                alogw("fatal error! why region is not NULL!");
                free(pVideoEncData->mMotionResult.region);
                pVideoEncData->mMotionResult.region = NULL;
            }
            unsigned int size = pMotionParam->hor_region_num * pMotionParam->ver_region_num * sizeof(VencMotionSearchRegion);
            pVideoEncData->mMotionResult.region = (VencMotionSearchRegion*)malloc(size);
            if (NULL == pVideoEncData->mMotionResult.region)
            {
                aloge("fatal error! malloc Region failed! size=%d", size);
                eError = ERR_VENC_NOMEM;
            }
            else
            {
                memset(pVideoEncData->mMotionResult.region, 0, size);
            }
        }
        else
        {
            if (pVideoEncData->mMotionResult.region)
            {
                free(pVideoEncData->mMotionResult.region);
                pVideoEncData->mMotionResult.region = NULL;
            }
        }
    }
    pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);

    return eError;
}

static ERRORTYPE VideoEncGetMotionSearchParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VencMotionSearchParam *pMotionParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    int ret;
    pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
    if(pVideoEncData->mbCedarvVideoEncInitFlag)
    {
        ret = VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamMotionSearchParam, (void*)pMotionParam);
        if(ret != 0)
        {
            aloge("fatal error! check code!");
            eError = FAILURE;
        }
    }
    pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);

    return eError;
}

static ERRORTYPE VideoEncGetMotionSearchResult(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_OUT VencMotionSearchResult *pMotionResult)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
    if(pVideoEncData->mbEnableMotionSearch)
    {
        pMotionResult->total_region_num = pVideoEncData->mMotionResult.total_region_num;
        pMotionResult->motion_region_num = pVideoEncData->mMotionResult.motion_region_num;
        if (pMotionResult->region && pVideoEncData->mMotionResult.region && pMotionResult->total_region_num)
        {
            memcpy(pMotionResult->region, pVideoEncData->mMotionResult.region, pMotionResult->total_region_num * sizeof(VencMotionSearchRegion));
        }
    }
    else
    {
        alogw("Be careful! motion search is not enable!");
        eError = ERR_VENC_BUSY;
    }
    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
    return eError;
}

ERRORTYPE VideoEncSaveBSFile(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencSaveBSFile* pSaveParam)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if((pVideoEncData->pCedarV != NULL) && (pSaveParam != NULL))
    {
        if (memcmp(&pVideoEncData->mSaveBSFile, pSaveParam, sizeof(VencSaveBSFile)) == 0)
        {
            aloge("user set the same SaveBSFile => filename:%s, enable:%u, start_time:%u, end_time:%u", pSaveParam->filename,
                pSaveParam->save_bsfile_flag, pSaveParam->save_start_time, pSaveParam->save_end_time);
            eError = ERR_VENC_ILLEGAL_PARAM;
        }
        else
        {
            memcpy(&pVideoEncData->mSaveBSFile, pSaveParam, sizeof(VencSaveBSFile));
            if (pVideoEncData->mbCedarvVideoEncInitFlag)
            {
                if(VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSaveBSFile, (void*)pSaveParam) != 0)
                {
                    aloge("update VE SaveBsFile fail!");
                    eError = ERR_VENC_BUSY;
                }
            }
        }
    }
    else
    {
        eError = ERR_VENC_NULL_PTR;
    }
    return eError;
}

ERRORTYPE VideoEncUpdateProcSet(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VeProcSet* pVeProcSet)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if((pVideoEncData->pCedarV != NULL) && (pVeProcSet != NULL))
    {
        if (memcmp(&pVideoEncData->mProcSet, pVeProcSet, sizeof(VeProcSet)) == 0)
        {
            aloge("user set the same VeProcSet => enable:%u, freq:%u, BTTime:%u, FRTime:%u", pVeProcSet->bProcEnable,
                pVeProcSet->nProcFreq, pVeProcSet->nStatisBitRateTime, pVeProcSet->nStatisFrRateTime);
            eError = ERR_VENC_ILLEGAL_PARAM;
        }
        else
        {
            memcpy(&pVideoEncData->mProcSet, pVeProcSet, sizeof(VeProcSet));
            if (pVideoEncData->mbCedarvVideoEncInitFlag)
            {
                //update proc set to catch parame of VE.
                if(VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamProcSet, (void*)pVeProcSet) != 0)
                {
                    aloge("update VE proc set fail!");
                    eError = ERR_VENC_BUSY;
                }
            }
        }
    }
    else
    {
        eError = ERR_VENC_NULL_PTR;
    }

    return eError;
}

static ERRORTYPE VideoEncDestroyEncoder(PARAM_IN COMP_HANDLETYPE hComponent)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    
    pthread_mutex_lock(&pVideoEncData->mStateLock);
    if(COMP_StateIdle == pVideoEncData->state)
    {
        //output frames must all return! else return FAIL!
        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        //wait all outFrame return.
        int cnt;
        struct list_head *pList;
        int cnt1 = 0;
        list_for_each(pList, &pVideoEncData->mReadyOutFrameList){cnt1++;} 
        int cnt2 = 0;
        list_for_each(pList, &pVideoEncData->mUsedOutFrameList){cnt2++;}
        cnt = 0;
        list_for_each(pList, &pVideoEncData->mIdleOutFrameList){cnt++;}
        if(cnt + cnt1 + cnt2 != pVideoEncData->mFrameNodeNum)
        {
            aloge("fatal error! why outFrame number is not match? check code! [%d-%d-%d]!=[%d]", cnt, cnt1, cnt2, pVideoEncData->mFrameNodeNum);
        }
        if(cnt+cnt1<pVideoEncData->mFrameNodeNum)
        {
            alogw("Be careful! not all outFrames are back,[%d-%d-%d,%d]! so can't destroy encLib", cnt, cnt1, cnt2, pVideoEncData->mFrameNodeNum);
            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
            pthread_mutex_unlock(&pVideoEncData->mStateLock);
            return ERR_VENC_NOT_PERM;
        }

        cnt = 0;
        while((!list_empty(&pVideoEncData->mReadyOutFrameList)))
        {
            ENCODER_NODE_T *pEntry = list_first_entry(&pVideoEncData->mReadyOutFrameList, ENCODER_NODE_T, mList);
            if (pVideoEncData->is_compress_source == 0)
            {
                VencOutputBuffer stOutputBuffer;
                config_VencOutputBuffer_By_EncodedStream(&stOutputBuffer, &pEntry->stEncodedStream);
                VencQueueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
                alogv("output coded frame ID[%d] list: Ready -> Idle", pEntry->stEncodedStream.nID);
                cnt++;
            }
        }
        if(cnt > 0)
        {
            alogd("release [%d] ready frames before destroy encLib!", cnt);
        }

        cnt = 0;
        list_for_each(pList, &pVideoEncData->mIdleOutFrameList){cnt++;}
        if(cnt<pVideoEncData->mFrameNodeNum)
        {
            aloge("fatal error! wait Venc idleOutFrameList full:%d-%d",cnt,pVideoEncData->mFrameNodeNum);
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);

        //calculate input frames number.
        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
        cnt = 0;
        list_for_each(pList, &pVideoEncData->mBufQ.mReadyFrameList){cnt++;}
        alogd("there is [%d] input frames when destroy encLib", cnt);
        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

        //destroy encoder
        pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
        if (pVideoEncData->pCedarV != NULL)
        {
            VencDestroy(pVideoEncData->pCedarV);
            pVideoEncData->mbCedarvVideoEncInitFlag = FALSE;
            pVideoEncData->pCedarV = NULL;

            VideoEncMBInfoDeinit(&pVideoEncData->mMBInfo);
        }
        pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
        eError = SUCCESS;
    }
    else
    {
        alogw("Be careful! venc state[%d] is not allowed to destroy vencLib!", pVideoEncData->state);
        eError = ERR_VENC_INCORRECT_STATE_OPERATION;
    }
    pthread_mutex_unlock(&pVideoEncData->mStateLock);
    return eError;
}

static ERRORTYPE VideoEncSetIsp2VeParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencIspMotionParam* pParam)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        if (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            if (0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamIspMotionParam,(void*)pParam))
            {
                alogv("Venchn[%d] set Isp2VeParam:%d-%d", pVideoEncData->mMppChnInfo.mChnId, pParam->dis_default_para, pParam->large_mv_th);
                eError = SUCCESS;
            }
            else
            {
                aloge("fatal error! VenChn[%d] set isp2VeParam[%p] fail!", pVideoEncData->mMppChnInfo.mChnId, pParam);
                eError = ERR_VENC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! VenChn[%d] VeType %d is not support SetIsp2VeParam!", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type);
            eError = ERR_VENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        aloge("fatal error! VenChn[%d] not init!", pVideoEncData->mMppChnInfo.mChnId);
        eError = ERR_VENC_NULL_PTR;
    }
    return eError;
}

static ERRORTYPE VideoEncGetVe2IspParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT VencVe2IspParam* pParam)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV)
    {
        if (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            if (0 == VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamVe2IspParam, (void*)pParam))
            {
                alogv("VenChn[%d] get Ve2IspParam:%d-%d", pVideoEncData->mMppChnInfo.mChnId,
                    pParam->mMovingLevelInfo.is_overflow, pParam->mMovingLevelInfo.moving_level_table[0]);
                eError = SUCCESS;
            }
            else
            {
                aloge("fatal error! VenChn[%d] get Ve2IspParam[%p] fail!", pVideoEncData->mMppChnInfo.mChnId, pParam);
                eError = ERR_VENC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! VenChn[%d] VeType %d is not support GetVe2IspParam!", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type);
            eError = ERR_VENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        aloge("fatal error! VenChn[%d] not init!", pVideoEncData->mMppChnInfo.mChnId);
        eError = ERR_VENC_NULL_PTR;
    }
    return eError;
}

static ERRORTYPE VideoEncSetWbYuv(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN sWbYuvParam* pWbYuvParam)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pVideoEncData->pCedarV)
    {
        if (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            if (0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamEnableWbYuv,(void*)pWbYuvParam))
            {
                eError = SUCCESS;
            }
            else
            {
                aloge("fatal error! VenChn[%d] set pWbYuvParam[%p] fail!", pVideoEncData->mMppChnInfo.mChnId, pWbYuvParam);
                eError = ERR_VENC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! VenChn[%d] VeType %d is not support EnableWbYuv!", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type);
            eError = ERR_VENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        aloge("fatal error! VenChn[%d] not init!", pVideoEncData->mMppChnInfo.mChnId);
        eError = ERR_VENC_NULL_PTR;
    }
    return eError;
}

static ERRORTYPE VideoEncGetWbYuv(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencThumbInfo* pThumbInfo)
{
    ERRORTYPE eError;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->pCedarV)
    {
        if (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
        {
            if (0 == VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamGetThumbYUV,(void*)pThumbInfo))
            {
                eError = SUCCESS;
            }
            else
            {
                alogv("fatal error! VenChn[%d] get wb-yuv [%p] fail!", pVideoEncData->mMppChnInfo.mChnId, pThumbInfo);
                eError = ERR_VENC_BUSY;
            }
        }
        else
        {
            aloge("fatal error! VenChn[%d] VeType %d is not support GetWbYuv!", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type);
            eError = ERR_VENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        aloge("fatal error! VenChn[%d] not init!", pVideoEncData->mMppChnInfo.mChnId);
        eError = ERR_VENC_NULL_PTR;
    }
    return eError;
}

/*****************************************************************************/

ERRORTYPE VideoEncSendCommand(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd,
        PARAM_IN unsigned int nParam1,
        PARAM_IN void* pCmdData)
{
    VIDEOENCDATATYPE *pVideoEncData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    alogv("VideoEncSendCommand: %d", Cmd);

    pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(!pVideoEncData)
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
    	goto COMP_CONF_CMD_FAIL;
    }

    if (pVideoEncData->state == COMP_StateInvalid)
    {
        eError = ERR_VENC_SYS_NOTREADY;
        goto COMP_CONF_CMD_FAIL;
    }
    switch (Cmd)
    {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        default:
            alogw("impossible comp_command[0x%x]", Cmd);
            eCmd = -1;
            break;
    }

    if (eCmd == SetState)
    {
        if (nParam1 == COMP_StateExecuting)
            alogd("set VencChn[%d] Comp StateExecuting", pVideoEncData->mMppChnInfo.mChnId);
        if (nParam1 == COMP_StatePause)
            alogd("set VencChn[%d] Comp StatePause", pVideoEncData->mMppChnInfo.mChnId);
        if (nParam1 == COMP_StateIdle)
            alogd("set VencChn[%d] Comp StateIdle", pVideoEncData->mMppChnInfo.mChnId);
        if (nParam1 == COMP_StateLoaded)
            alogd("set VencChn[%d] Comp StateLoaded", pVideoEncData->mMppChnInfo.mChnId);
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pVideoEncData->cmd_queue, &msg);
    alogv("VencChn[%d] send message eCmd=%d", pVideoEncData->mMppChnInfo.mChnId, eCmd);

COMP_CONF_CMD_FAIL: 
    return eError;
}

/*****************************************************************************/
ERRORTYPE VideoEncGetState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_STATETYPE* pState)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    *pState = pVideoEncData->state;

    return eError;
}

/*****************************************************************************/
ERRORTYPE VideoEncSetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_CALLBACKTYPE* pCallbacks, PARAM_IN void* pAppData)
{
    VIDEOENCDATATYPE *pVideoEncData;
    ERRORTYPE eError = SUCCESS;

    pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
	if(!pVideoEncData || !pCallbacks || !pAppData)
    {
        eError = ERR_VENC_ILLEGAL_PARAM;
    	goto COMP_CONF_CMD_FAIL;
    }

    pVideoEncData->pCallbacks = pCallbacks;
    pVideoEncData->pAppData = pAppData;

    COMP_CONF_CMD_FAIL: return eError;
}

ERRORTYPE VideoEncGetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{

    ERRORTYPE eError = SUCCESS;

    switch(nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = VideoEncGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = VideoEncGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = VideoEncGetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelFd:
        {
            eError = VideoEncGetChannelFd(hComponent, (int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTunnelInfo:
        {
            eError = VideoEncGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencChnAttr:
        {
            eError = VideoEncGetChnAttr(hComponent, (VENC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencChnPriority:
        {
            alogw("unsupported temporary get venc chn priority!");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencChnState:
        {
            eError = VideoEncGetVencChnState(hComponent, (VENC_CHN_STAT_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencGetStream:
        {
            VEncStream *pStream = (VEncStream*)pComponentConfigStructure;
            eError = VideoEncGetStream(hComponent, pStream->pStream, pStream->nMilliSec);
            break;
        }
        case COMP_IndexVendorVencStreamDuration:
        {
            eError = VideoEncGetStreamDuration(hComponent, (double*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRoiCfg:
        {
            eError = VideoEncGetRoiCfg(hComponent, (VENC_ROI_CFG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRoiBgFrameRate:
        {
            eError = VideoEncGetRoiBgFrameRate(hComponent, (VENC_ROIBG_FRAME_RATE_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencH264SliceSplit:
        {
            alogw("need implement H264SliceSplit");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264InterPred:
        {
            alogw("need implement H264InterPred");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264IntraPred:
        {
            alogw("need implement H264IntraPred");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Trans:
        {
            alogw("need implement H264Trans");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Entropy:
        {
            alogw("need implement H264Entropy");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Poc:
        {
            alogw("need implement H264Poc");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Dblk:
        {
            alogw("need implement H264Dblk");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Vui:
        {
            eError = VideoEncGetH264Vui(hComponent, (VENC_PARAM_H264_VUI_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencH265Vui:
        {
            eError = VideoEncGetH265Vui(hComponent, (VENC_PARAM_H265_VUI_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencJpegParam:
        {
            eError = VideoEncGetJpegParam(hComponent, (VENC_PARAM_JPEG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencJpegExifInfo:
        {
            eError = VideoEncGetJpegExifInfo(hComponent, (VENC_EXIFINFO_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencJpegThumbBuffer:
        {
            eError = VideoEncGetJpegThumbBuffer(hComponent, (VENC_JPEG_THUMB_BUFFER_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencMjpegParam:
        {
            alogw("need implement MjpegParam");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencFrameRate:
        {
            eError = VideoEncGetFrameRate(hComponent, (VENC_FRAME_RATE_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTimeLapse:
        {
            eError = VideoEncGetTimeLapse(hComponent, (int64_t*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRcParam:
        {
            eError = VideoEncGetRcParam(hComponent, (VENC_RC_PARAM_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRefParam:
        {
            eError = VideoEncGetRefParam(hComponent, (VENC_PARAM_REF_S *) pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencColor2Grey:
        {
            eError = VideoEncGetColor2Grey(hComponent, (VENC_COLOR2GREY_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencCrop:
        {
            eError = VideoEncGetCropCfg(hComponent, (VENC_CROP_CFG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencStreamBufInfo:
        {
            eError = VideoEncGetStreamBufInfo(hComponent, (VENC_STREAM_BUF_INFO_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRcPriority:
        {
            alogw("unsupported temporary: VencRcPriority");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265SliceSplit:
        {
            alogw("need implement H265SliceSplit");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265PredUnit:
        {
            alogw("need implement H265PredUnit");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Trans:
        {
            alogw("need implement H265Trans");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Entropy:
        {
            alogw("need implement H265Entropy");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Dblk:
        {
            alogw("need implement H265Dblk");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Sao:
        {
            alogw("need implement H265Sao");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencFrameLostStrategy:
        {
            alogw("need implement FrameLostStrategy");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencSuperFrameCfg:
        {
            eError = VideoEncGetSuperFrameCfg(hComponent, (VENC_SUPERFRAME_CFG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencIntraRefresh:
        {
            eError = VideoEncGetIntraRefreshParam(hComponent, (VENC_PARAM_INTRA_REFRESH_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencSmartP:
        {
            //eError = VideoEncGetSmartPParam(hComponent, (VencSmartFun*)pComponentConfigStructure);
            alogw("unsupported: VencSmartP");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencBrightness:
        {
            eError = VideoEncGetBrightness(hComponent, (VencBrightnessS*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorExtraData:
        {
            eError = VideoEncExtraData(hComponent, (VencHeaderData*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVenc2DFilter:
        {
            eError = VideoEncGet2DFilter(hComponent, (s2DfilterParam *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVenc3DFilter:
        {
            eError = VideoEncGet3DFilter(hComponent, (s3DfilterParam *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencCacheState:
        {
            eError = VideoEncGetCacheState(hComponent, (CacheState*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencHorizonFlip:
        {
            eError = VideoEncGetHorizonFlip(hComponent, (BOOL *)pComponentConfigStructure);
            break;
        }       
        case COMP_IndexVendorVencHighPassFilter:
        {
            eError = VideoEncGetHighPassFilter(hComponent, (VencHighPassFilter*)pComponentConfigStructure);
            break;
        }        
        case COMP_IndexVendorVencDayOrNight:
        {
            eError = VideoEncGetDayOrNight(hComponent, (int *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencMotionSearchParam:
        {
            eError = VideoEncGetMotionSearchParam(hComponent, (VencMotionSearchParam*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencMotionSearchResult:
        {
            eError = VideoEncGetMotionSearchResult(hComponent, (VencMotionSearchResult*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencVe2IspParam:
        {
            eError = VideoEncGetVe2IspParam(hComponent, (VencVe2IspParam*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencGetWbYuv:
        {
            eError = VideoEncGetWbYuv(hComponent, (VencThumbInfo*)pComponentConfigStructure);
            break;
        }
        default:
        {
            aloge("fatal error! unknown getConfig Index[0x%x]", nIndex);
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
    }
    return eError;
}

ERRORTYPE VideoEncSetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    ERRORTYPE eError = SUCCESS;
    //int ret;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = VideoEncSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = VideoEncSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = VideoEncSetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencChnAttr:
        {
            eError = VideoEncSetChnAttr(hComponent, (VENC_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencChnPriority:
        {
            alogw("unsupported temporary set venc chn priority!");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencResetChannel:
        {
            eError = VideoEncResetChannel(hComponent, TRUE);
            break;
        }
        case COMP_IndexVendorVencRecvPicParam:
        {
            eError = VideoEncSetRecvPicParam(hComponent, (VENC_RECV_PIC_PARAM_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencReleaseStream:
        {
            eError = VideoEncReleaseStream(hComponent, (VENC_STREAM_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencUserData:
        {
            alogw("need implement Insert UserData?");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencStreamDuration: /**< reference: double */
        {
            eError = VideoEncSetStreamDuration(hComponent, *(double*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRequestIDR:
        {
            eError = VideoEncRequestIDR(hComponent, *(BOOL*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRoiCfg:
        {
            eError = VideoEncSetRoiCfg(hComponent, (VENC_ROI_CFG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRoiBgFrameRate:
        {
            eError = VideoEncSetRoiBgFrameRate(hComponent, (VENC_ROIBG_FRAME_RATE_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencH264SliceSplit:
        {
            alogw("need implement H264SliceSplit");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264InterPred:
        {
            alogw("need implement H264InterPred");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264IntraPred:
        {
            alogw("need implement H264IntraPred");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Trans:
        {
            alogw("need implement H264Trans");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Entropy:
        {
            alogw("need implement H264Entropy");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Poc:
        {
            alogw("need implement H264Poc");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Dblk:
        {
            alogw("need implement H264Dblk");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH264Vui:
        {
            eError = VideoEncSetH264Vui(hComponent, (VENC_PARAM_H264_VUI_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencH265Vui:
        {
            eError = VideoEncSetH265Vui(hComponent, (VENC_PARAM_H265_VUI_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencJpegParam:
        {
            eError = VideoEncSetJpegParam(hComponent, (VENC_PARAM_JPEG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencJpegExifInfo:
        {
            eError = VideoEncSetJpegExifInfo(hComponent, (VENC_EXIFINFO_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencMjpegParam:
        {
            alogw("need implement MjpegParam");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencFrameRate:
        {
            eError = VideoEncSetFrameRate(hComponent, (VENC_FRAME_RATE_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTimeLapse:
        {
            eError = VideoEncSetTimeLapse(hComponent, (int64_t*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRcParam:
        {
            eError = VideoEncSetRcParam(hComponent, (VENC_RC_PARAM_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencRefParam:
        {
            eError = VideoEncSetRefParam(hComponent, (VENC_PARAM_REF_S *) pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencColor2Grey:
        {
            eError = VideoEncSetColor2Grey(hComponent, (VENC_COLOR2GREY_S *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencCrop:
        {
            eError = VideoEncSetCropCfg(hComponent, (VENC_CROP_CFG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencEnableIDR:
        {
            alogw("need implement EnableIDR");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencRcPriority:
        {
            alogw("need implement RcPriority");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265SliceSplit:
        {
            alogw("need implement H265SliceSplit");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265PredUnit:
        {
            alogw("need implement H265PredUnit");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Trans:
        {
            alogw("need implement H265Trans");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Entropy:
        {
            alogw("need implement H265Entropy");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Dblk:
        {
            alogw("need implement H265Dblk");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencH265Sao:
        {
            alogw("need implement H265Sao");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencFrameLostStrategy:
        {
            alogw("need implement FrameLostStrategy");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencSuperFrameCfg:
        {
            eError = VideoEncSetSuperFrameCfg(hComponent, (VENC_SUPERFRAME_CFG_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencIntraRefresh:
        {
            eError = VideoEncSetIntraRefreshParam(hComponent, (VENC_PARAM_INTRA_REFRESH_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencSmartP:
        {
            //eError = VideoEncSetSmartPParam(hComponent, (VencSmartFun*)pComponentConfigStructure);
            alogw("unsupported: VencSmartP");
            eError = ERR_VENC_NOT_SUPPORT;
            break;
        }
        case COMP_IndexVendorVencBrightness:
        {
            eError = VideoEncSetBrightness(hComponent, (VencBrightnessS*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencOsd:
        {
            eError = VideoEncSetOsd(hComponent, (VencOverlayInfoS*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVEFreq:
        {
            eError = VideoEncSetVEFreq(hComponent, *(int*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVenc2DFilter:
        {
            eError = VideoEncSet2DFilter(hComponent, (s2DfilterParam *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVenc3DFilter:
        {
            eError = VideoEncSet3DFilter(hComponent, (s3DfilterParam *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencHorizonFlip:
        {
            eError = VideoEncSetHorizonFlip(hComponent, *(BOOL *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencAdaptiveIntraInP:
        {
            eError = VideoEncSetAdaptiveIntraInP(hComponent, *(BOOL *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencH264SVCSkip:
        {
            eError = VideoEncSetH264SVCSkip(hComponent, (VencH264SVCSkip*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencEnableNullSkip:
        {
            eError = VideoEncEnableNullSkip(hComponent, *(BOOL *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencEnablePSkip:
        {
            eError = VideoEncEnablePSkip(hComponent, *(BOOL *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencForbidDiscardingFrame:
        {
            eError = VideoEncForbidDiscardingFrame(hComponent, *(BOOL *)pComponentConfigStructure);
            break;
        }    
        case COMP_IndexVendorSaveBSFile:
        {
            eError = VideoEncSaveBSFile(hComponent, (VencSaveBSFile*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencProcSet:
        {
            eError = VideoEncUpdateProcSet(hComponent, (VeProcSet*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencHighPassFilter:
        {
            eError = VideoEncSetHighPassFilter(hComponent, (VencHighPassFilter*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencDayOrNight:
        {
            eError = VideoEncSetDayOrNight(hComponent, (int *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencDestroyEncoder:
        {
            eError = VideoEncDestroyEncoder(hComponent);
            break;
        }
        case COMP_IndexVendorVencMotionSearchParam:
        {
            eError = VideoEncSetMotionSearchParam(hComponent, (VencMotionSearchParam*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencIsp2VeParam:
        {
            eError = VideoEncSetIsp2VeParam(hComponent, (VencIspMotionParam*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorVencSetWbYuv:
        {
            eError = VideoEncSetWbYuv(hComponent, (sWbYuvParam*)pComponentConfigStructure);
            break;
        }
        default:
        {
            aloge("unknown Index[0x%x]", nIndex);
            eError = ERR_VENC_ILLEGAL_PARAM;
            break;
        }
    }

    return eError;
}

ERRORTYPE VideoEncComponentTunnelRequest(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  unsigned int nPort,
        PARAM_IN  COMP_HANDLETYPE hTunneledComp,
        PARAM_IN  unsigned int nTunneledPort,
        PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pVideoEncData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pVideoEncData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pVideoEncData->state);
        eError = ERR_VENC_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }

    COMP_PARAM_PORTDEFINITIONTYPE   *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE    *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE   *pPortBufSupplier;
    BOOL bFindFlag;
    int i;
    bFindFlag = FALSE;
    if(pVideoEncData->sInPortDef.nPortIndex == nPort)
    {
        pPortDef = &pVideoEncData->sInPortDef;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pVideoEncData->sOutPortDef.nPortIndex == nPort)
        {
            pPortDef = &pVideoEncData->sOutPortDef;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_VENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    if(pVideoEncData->sInPortTunnelInfo.nPortIndex == nPort)
    {
        pPortTunnelInfo = &pVideoEncData->sInPortTunnelInfo;
        bFindFlag = TRUE;
    }
    if(FALSE == bFindFlag)
    {
        if(pVideoEncData->sOutPortTunnelInfo.nPortIndex == nPort)
        {
            pPortTunnelInfo = &pVideoEncData->sOutPortTunnelInfo;
            bFindFlag = TRUE;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_VENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_VENCODER_PORTS; i++)
    {
        if(pVideoEncData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pVideoEncData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_VENC_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        if(pPortDef->eDir == COMP_DirOutput)
        {
            pVideoEncData->mOutputPortTunnelFlag = FALSE;
            alogd("set outputPortTunnelFlag = FALSE");
        }
        else
        {
            pVideoEncData->mInputPortTunnelFlag = FALSE;
            alogd("set inputPortTunnelFlag = FALSE");
        }
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        if (pVideoEncData->mOutputPortTunnelFlag) {
            aloge("VEnc_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        pVideoEncData->mOutputPortTunnelFlag = TRUE;
        alogd("set outputPortTunnelFlag = TRUE");
    }
    else
    {
        if (pVideoEncData->mInputPortTunnelFlag) {
            aloge("VEnc_Comp inport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput)
        {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_VENC_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;

        //The component B informs component A about the final result of negotiation.
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
        {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pVideoEncData->mInputPortTunnelFlag = TRUE;
        alogd("set inputPortTunnelFlag = TRUE");
    }

COMP_CMD_FAIL:
    return eError;
}

/**
 * @return  SUCCESS: accept frame, and store frame in internal list, comp will return frame in internal thread.
 *          ERR_VENC_SYS_NOTREADY: deny frame.
 *          ERR_VENC_BUF_FULL: deny frame.
 *          ERR_VENC_EXIST: accept frame, use frame done, and don't store frame in internal list. return frame now.
 *          ERR_VENC_NOMEM: no memory. deny frame.
 *          ERR_VENC_NOT_PERM: deny frame.
 */
ERRORTYPE VideoEncEmptyThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    ERRORTYPE eError    = SUCCESS;
    VIDEOENCDATATYPE *pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (pVideoEncData->state != COMP_StateExecuting)
    {
        alogd("send frame when venc state[0x%x] is not executing", pVideoEncData->state);
        //eError = COMP_ErrorInvalidState;
        //goto ERROR;
    }

    if (pVideoEncData->mOnlineEnableFlag)
    {
        if (FALSE == pVideoEncData->mInputPortTunnelFlag)
        {
            aloge("fatal error! venc online, The input of venc component must be bound to virvi component.");
        }
        else
        {
            aloge("fatal error! venc online, csi&ve driver transfer data directly, please check virvi component.");
        }
        return ERR_VENC_NOT_PERM;
    }

    pthread_mutex_lock(&pVideoEncData->mRecvPicControlLock);
    if(pVideoEncData->mLimitedFramesFlag && pVideoEncData->mCurRecvPicNum >= pVideoEncData->mRecvPicParam.mRecvPicNum)
    {
        alogd("the venc cahnnel[%d] had received %d frame, it is enough!", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mCurRecvPicNum);
        eError = ERR_VENC_BUF_FULL;
        goto ERROR;
    }

    VIDEO_FRAME_INFO_S *pEncFrame;
    if(pVideoEncData->mInputPortTunnelFlag)
    {
        pEncFrame = (VIDEO_FRAME_INFO_S*)pBuffer->pOutputPortPrivate;
    }
    else
    {
        pEncFrame = (VIDEO_FRAME_INFO_S*)pBuffer->pAppPrivate;
    }

    if(pVideoEncData->is_compress_source == 0)
    {
        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
        if(list_empty(&pVideoEncData->mBufQ.mIdleFrameList))
        {
            alogd("VencChn[%d] idle frame is empty, malloc one!", pVideoEncData->mMppChnInfo.mChnId);
            if(pVideoEncData->mBufQ.buf_unused!=0)
            {
                aloge("fatal error! VencChn[%d] buf_unused must be zero!", pVideoEncData->mMppChnInfo.mChnId);
            }
            VideoFrameInfoNode *pNode = (VideoFrameInfoNode*)malloc(sizeof(VideoFrameInfoNode));
            if(NULL == pNode)
            {
                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                aloge("fatal error! VencChn[%d] malloc fail!", pVideoEncData->mMppChnInfo.mChnId);
                eError = ERR_VENC_NOMEM;
                goto ERROR;
            }
            list_add_tail(&pNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
            pVideoEncData->mBufQ.buf_unused++;
        }
        VideoFrameInfoNode *pFirstNode = list_first_entry(&pVideoEncData->mBufQ.mIdleFrameList, VideoFrameInfoNode, mList);
        VIDEO_FRAME_INFO_S *pDstEncFrame = &pFirstNode->VFrame;

        if (pVideoEncData->csi_first_frame)
        {
            pVideoEncData->csi_base_time = pEncFrame->VFrame.mpts;
            pVideoEncData->mPrevInputPts = pEncFrame->VFrame.mpts;
            pVideoEncData->mCapTimeLapse.lastCapTimeUs = 0;
            pVideoEncData->mCapTimeLapse.lastTimeStamp = 0;
            pVideoEncData->csi_first_frame = FALSE;
            int64_t tm1 = CDX_GetSysTimeUsMonotonic();
//            if(1920 == pEncFrame->VFrame.mWidth && 1080 == pEncFrame->VFrame.mHeight)//(pVideoEncData == g_pVideoEncData[0])
//            {
//                alogw("avsync_first video frame pts[%lld]us,tm1[%lld]us, vSize[%dx%d]",
//                    pEncFrame->VFrame.mpts, tm1, pEncFrame->VFrame.mWidth, pEncFrame->VFrame.mHeight);
//            }
            alogd("VencChn[%d]: first video frame pts[%lld]us,tm1[%lld]us, vSize[%dx%d], frmId[%d,%d]",
                    pVideoEncData->mMppChnInfo.mChnId, pEncFrame->VFrame.mpts, tm1, pEncFrame->VFrame.mWidth,
                    pEncFrame->VFrame.mHeight, pEncFrame->mId,pEncFrame->VFrame.mFramecnt);
        }

        if (pEncFrame->VFrame.mpts - pVideoEncData->mPrevInputPts >= 300*1000)
        {
            alogd("Be careful! VencChn[%d] vencInputPts[%lld]-[%lld]=[%lld]us, vBufSize[%dx%d]", pVideoEncData->mMppChnInfo.mChnId,
                pEncFrame->VFrame.mpts, pVideoEncData->mPrevInputPts, pEncFrame->VFrame.mpts - pVideoEncData->mPrevInputPts,
                pEncFrame->VFrame.mWidth, pEncFrame->VFrame.mHeight);
        }

        pVideoEncData->mPrevInputPts = pEncFrame->VFrame.mpts;
        memcpy(pDstEncFrame, pEncFrame, sizeof(VIDEO_FRAME_INFO_S));
        alogv("wr buf_id: %d, nTimeStamp: [%lld]us", pDstEncFrame->mId, pDstEncFrame->VFrame.mpts);
        pVideoEncData->mBufQ.buf_unused--;
        list_move_tail(&pFirstNode->mList, &pVideoEncData->mBufQ.mReadyFrameList);
        alogv("input orig frame ID[%d] list: Idle -> Ready", pFirstNode->VFrame.mId);

        if(0 == pVideoEncData->mFlagInputFrame)
        {
            pVideoEncData->mFlagInputFrame = 1;
            message_t msg;
            msg.command = VEncComp_InputFrameAvailable;
            put_message(&pVideoEncData->cmd_queue, &msg);
            alogv("VencChn[%d] send message VEncComp InputFrameAvailable", pVideoEncData->mMppChnInfo.mChnId);
        }
        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
    }
    else
    {
        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        FRAMEDATATYPE frame;

        //VIDEO_FRAME_INFO_S *pEncFrame = (VIDEO_FRAME_INFO_S*)pBuffer->pAppPrivate;

        if(pVideoEncData->csi_first_frame)
        {
            pVideoEncData->csi_base_time = pEncFrame->VFrame.mpts;
            pVideoEncData->mPrevInputPts = pEncFrame->VFrame.mpts;
            pVideoEncData->mCapTimeLapse.lastCapTimeUs = 0;
            pVideoEncData->mCapTimeLapse.lastTimeStamp = 0;
            pVideoEncData->csi_first_frame = FALSE;
        }
        frame.info.timeStamp = pEncFrame->VFrame.mpts;
        frame.info.bufferId = pEncFrame->mId;
        frame.info.size = pEncFrame->VFrame.mStride[0];
        frame.addrY = (char*)pEncFrame->VFrame.mpVirAddr[0];
        eError = VideoEncBufferPushFrame(pVideoEncData->buffer_manager, &frame);
        if (eError != SUCCESS)
        {
            alogw("Be careful! current buffer queue is full!");
            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
            goto ERROR;
        }
        else
        {
            eError = ERR_VENC_EXIST;
        }
        //MM_COMPONENTTYPE *InputTunnelComp = (MM_COMPONENTTYPE *)(pVideoEncData->sInPortTunnelInfo.hTunnel);
        //InputTunnelComp->FillThisBuffer(InputTunnelComp, pBuffer);
        if(0 == pVideoEncData->mFlagInputFrame)
        {
            pVideoEncData->mFlagInputFrame = 1;
            message_t msg;
            msg.command = VEncComp_InputFrameAvailable;
            put_message(&pVideoEncData->cmd_queue, &msg);
            alogv("VencChn[%d] send message VEncComp InputFrameAvailable", pVideoEncData->mMppChnInfo.mChnId);
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    }

    if(pVideoEncData->mLimitedFramesFlag)
    {
        pVideoEncData->mCurRecvPicNum++;
    }

ERROR:
    pthread_mutex_unlock(&pVideoEncData->mRecvPicControlLock);
    return eError;
}

ERRORTYPE VideoEncFillThisBuffer(
        PARAM_IN  COMP_HANDLETYPE hComponent,
        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    VIDEOENCDATATYPE *pVideoEncData;
    ERRORTYPE eError = SUCCESS;
    int ret;
    pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pBuffer->nOutputPortIndex == pVideoEncData->sOutPortDef.nPortIndex)
    {// output tunnel mode

        EncodedStream *pOutFrame = (EncodedStream*)pBuffer->pOutputPortPrivate;

        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        BOOL bFind = FALSE;
        ENCODER_NODE_T *pEntry;
        list_for_each_entry(pEntry, &pVideoEncData->mUsedOutFrameList, mList)
        {
            if (pEntry->stEncodedStream.nID == pOutFrame->nID)
            {
                bFind = TRUE;
                break;
            }
        }

        if (!bFind)
        {
            int num = 0;
            list_for_each_entry(pEntry, &pVideoEncData->mUsedOutFrameList, mList)
            {
                num++;
                aloge("[%d-%d-%d]", pEntry->stEncodedStream.nID, pOutFrame->nID, pBuffer->nInputPortIndex);
            }
            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
            aloge("fatal error! try to release one output buffer that never be used! ID = %d, inPortIndex:%d, usedOutCnt:%d", pOutFrame->nID, pBuffer->nInputPortIndex, num);
            return ERR_VENC_ILLEGAL_PARAM;
        }

        // release buffer
        if (pVideoEncData->is_compress_source == 0)
        {
            VencOutputBuffer stOutputBuffer;
            config_VencOutputBuffer_By_EncodedStream(&stOutputBuffer, pOutFrame);
            ret = VencQueueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
            if (ret != 0)
            {
                aloge("fatal error! Venc QueueOutputBuf fail[%d]", ret);
            }

            list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
            alogv("output coded frame ID[%d] list: Used -> Idle", pEntry->stEncodedStream.nID);

            if(pVideoEncData->mWaitOutFrameReturnFlag)
            {
                pVideoEncData->mWaitOutFrameReturnFlag = FALSE;
                message_t msg;
                msg.command = VEncComp_OutputFrameReturn;
                put_message(&pVideoEncData->cmd_queue, &msg);
                alogv("VencChn[%d] send message VEncComp OutputFrameReturn", pVideoEncData->mMppChnInfo.mChnId);
            }
            if (pVideoEncData->mWaitOutFrameFullFlag)
            {
                int cnt = 0;
                struct list_head *pList;
                list_for_each(pList, &pVideoEncData->mIdleOutFrameList)
                {
                    cnt++;
                }
                if (cnt>=pVideoEncData->mFrameNodeNum)
                {
                    pthread_cond_signal(&pVideoEncData->mOutFrameFullCondition);
                }
            }
            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
        }
        else
        {  /* gushiming compressed source */
            ERRORTYPE   releaseOmxRet;
            FRAMEDATATYPE frame;
            frame.addrY = (char*)pOutFrame->pBuffer;
            frame.info.timeStamp = pOutFrame->nTimeStamp;
            frame.info.bufferId = pOutFrame->nID;
            frame.info.size = pOutFrame->nBufferLen;
            releaseOmxRet = VideoEncBufferReleaseFrame(pVideoEncData->buffer_manager, &frame);
            if(releaseOmxRet != SUCCESS)
            {
                aloge("fatal error! videoEncBufferReleaseFrame fail[%d]", releaseOmxRet);
            }

            list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
            alogv("output coded frame ID[%d] list: Used -> Idle", pEntry->stEncodedStream.nID);

            if (pVideoEncData->mWaitOutFrameFullFlag)
            {
                int cnt = 0;
                struct list_head *pList;
                list_for_each(pList, &pVideoEncData->mIdleOutFrameList)
                {
                    cnt++;
                }
                if (cnt>=pVideoEncData->mFrameNodeNum)
                {
                    pthread_cond_signal(&pVideoEncData->mOutFrameFullCondition);
                }
            }
            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
        }
    }
    else
    {
        aloge("fatal error! outPortIndex[%d]!=[%d]", pBuffer->nOutputPortIndex, pVideoEncData->sOutPortDef.nPortIndex);
    }
//ERROR:
    return eError;
}

/*****************************************************************************/
ERRORTYPE VideoEncComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    VIDEOENCDATATYPE *pVideoEncData;
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    message_t msg;
    //int i = 0;
    alogv("VideoEnc Component DeInit");
    pVideoEncData = (VIDEOENCDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if(pVideoEncData->mDiscardFrameCnt > 0)
    {
        unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
        alogw("Be careful! VencChn[%d] type[%d] vBufSize[%dx%d]: discard [%d]frames! bitRate[%d]Mbit",
            pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type,
            pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight,
            pVideoEncData->mDiscardFrameCnt, nBitRate/(1000*1000));
    }
  #ifdef VIDEO_ENC_TIME_DEBUG
    alogd("VencChn[%d] type[%d] vBufSize[%dx%d]: encodeSuccess Duration[%lld]ms, FrameCount[%d], averagePerFrame[%lld]ms",
        pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type,
        pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight,
        pVideoEncData->mTotalEncodeSuccessDuration/1000, pVideoEncData->mEncodeSuccessCount, 
        pVideoEncData->mEncodeSuccessCount?pVideoEncData->mTotalEncodeSuccessDuration/pVideoEncData->mEncodeSuccessCount/1000:1);
  #endif
    SIZE_S dstPicSize;
    if (SUCCESS == getPicSizeFromVENC_ATTR_S(&pVideoEncData->mEncChnAttr.VeAttr, &dstPicSize))
    {
        alogd("vencChn[%d],type[%d],vSrcSize[%dx%d],vDstSize[%dx%d]: max frameSize of this encoding process is [%d]Byte([%f]MB)",
            pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type,
            pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight,
            dstPicSize.Width, dstPicSize.Height, 
            pVideoEncData->mStatMaxFrameSize, (float)pVideoEncData->mStatMaxFrameSize/(1024*1024));
    }
    else
    {
        aloge("fatal error! get dst size fail! check code!");
    }
    int cnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pVideoEncData->mBufQ.mIdleFrameList)
    {
        cnt++;
    }
    if(pVideoEncData->mBufQ.buf_unused != cnt)
    {
        aloge("fatal error! inputFrames[%d]<[%d] must return all before!", ENC_FIFO_LEVEL-pVideoEncData->mBufQ.buf_unused, cnt);
    }
    if(!list_empty(&pVideoEncData->mBufQ.mUsedFrameList))
    {
        aloge("fatal error! why usedFrameList is not empty?");
    }
    if(!list_empty(&pVideoEncData->mBufQ.mUsingFrameList))
    {
        aloge("fatal error! why usingFrameList is not empty?");
    }
    if(!list_empty(&pVideoEncData->mBufQ.mReadyFrameList))
    {
        aloge("fatal error! why readyInputFrame is not empty?");
    }
    if(!list_empty(&pVideoEncData->mBufQ.mIdleFrameList))
    {
        VideoFrameInfoNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mBufQ.mIdleFrameList, mList)
        {
            list_del(&pEntry->mList);
            freeVideoFrameInfoNode(pEntry);
        }
    }
    pthread_mutex_destroy(&pVideoEncData->mutex_fifo_ops_lock);
    pthread_cond_destroy(&pVideoEncData->mInputFrameUsingEmptyCondition);


    // STAR ADD 2011-5-27
    if (pVideoEncData->pCedarV != NULL)
    {
        VencDestroy(pVideoEncData->pCedarV);
        pVideoEncData->pCedarV = NULL;
        VideoEncMBInfoDeinit(&pVideoEncData->mMBInfo);
    }
    if(pVideoEncData->mpVencHeaderData)
    {
        if(pVideoEncData->mpVencHeaderData->pBuffer)
        {
            free(pVideoEncData->mpVencHeaderData->pBuffer);
            pVideoEncData->mpVencHeaderData->pBuffer = NULL;
        }
        pVideoEncData->mpVencHeaderData->nLength = 0;
        free(pVideoEncData->mpVencHeaderData);
        pVideoEncData->mpVencHeaderData = NULL;
    }
    msg.command = eCmd;
    put_message(&pVideoEncData->cmd_queue, &msg);
    alogv("VencChn[%d] send message Stop", pVideoEncData->mMppChnInfo.mChnId);
    //cdx_sem_up(&pVideoEncData->cdx_sem_wait_message);

    alogv("wait VideoEnc component exit!...");
    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pVideoEncData->thread_id, (void*) &eError);


    //post_message_to_vbuf(pVideoEncData, OMX_VBufCommand_Exit, 0);

    // Wait for thread to exit so we can get the status into "error"
    //pthread_join(pVideoEncData->thread_buf_id, (void*) &eError);

    //cdx_sem_deinit(&pVideoEncData->cdx_sem_wait_message);
    //cdx_sem_deinit(&cdx_sem_fifo_ops);

    if (pVideoEncData->mMotionResult.region)
    {
        free(pVideoEncData->mMotionResult.region);
        pVideoEncData->mMotionResult.region = NULL;
    }

    cdx_sem_deinit(&pVideoEncData->mInputFrameBufDoneSem);

    message_destroy(&pVideoEncData->cmd_queue);

    if (pVideoEncData->buffer_manager != NULL) {
        VideoEncBufferDeInit(pVideoEncData->buffer_manager);
        pVideoEncData->buffer_manager = NULL;
    }

    pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
    if(!list_empty(&pVideoEncData->mUsedOutFrameList))
    {
        aloge("fatal error! outUsedFrame must be 0!");
    }
    if(!list_empty(&pVideoEncData->mReadyOutFrameList))
    {
        aloge("fatal error! outReadyFrame must be 0!");
    }
    int nodeNum = 0;
    if(!list_empty(&pVideoEncData->mIdleOutFrameList))
    {
        ENCODER_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            nodeNum++;
        }
    }
    if(nodeNum!=pVideoEncData->mFrameNodeNum)
    {
        aloge("fatal error! frame node number is not match[%d][%d]", nodeNum, pVideoEncData->mFrameNodeNum);
    }
    if(pVideoEncData->mFrameNodeNum != BITSTREAM_FRAME_SIZE)
    {
        alogw("Low probability! VEncComp idle out frame total num: %d -> %d!", BITSTREAM_FRAME_SIZE, pVideoEncData->mFrameNodeNum);
    }
    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
    pthread_cond_destroy(&pVideoEncData->mOutFrameFullCondition);
    pthread_cond_destroy(&pVideoEncData->mOutFrameCondition);
    pthread_mutex_destroy(&pVideoEncData->mOutFrameListMutex);
    pthread_mutex_destroy(&pVideoEncData->mStateLock);
    close(pVideoEncData->mOutputFrameNotifyPipeFds[0]);
    close(pVideoEncData->mOutputFrameNotifyPipeFds[1]);

    pthread_mutex_destroy(&pVideoEncData->mCedarvVideoEncInitFlagLock);
    //pthread_mutex_destroy(&pVideoEncData->mVencOverlayLock);

    pthread_mutex_destroy(&pVideoEncData->mRecvPicControlLock);

    //free mRoiCfgList element
    pthread_mutex_lock(&pVideoEncData->mRoiLock);
    if(!list_empty(&pVideoEncData->mRoiCfgList))
    {
        VEncRoiCfgNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mRoiCfgList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pVideoEncData->mRoiLock);

    pthread_mutex_destroy(&pVideoEncData->mRoiLock);
    pthread_mutex_destroy(&pVideoEncData->mFrameRateLock);
    pthread_mutex_destroy(&pVideoEncData->mCapTimeLapseLock);
    if(pVideoEncData)
    {
        free(pVideoEncData);
    }

#ifdef STORE_VENC_IN_FRAME
    if (STORE_VENC_CH_ID == pVideoEncData->mMppChnInfo.mChnId)
    {
        if (g_debug_fp_frame != NULL)
        {
            fclose(g_debug_fp_frame);
            g_debug_fp_frame = NULL;
        }
        alogd("save total frame cnt %d", g_debug_frame_cnt);
        g_debug_frame_cnt = 0;
    }
#endif

#ifdef STORE_VENC_OUT_STREAM
    if (STORE_VENC_CH_ID == pVideoEncData->mMppChnInfo.mChnId)
    {
        if (g_debug_fp_stream != NULL)
        {
            fclose(g_debug_fp_stream);
            g_debug_fp_stream = NULL;
        }
        alogd("save total stream cnt %d", g_debug_stream_cnt);
        g_debug_stream_cnt = 0;
    }
#endif

    alogd("VideoEnc component exited!");

    return eError;
}

/*****************************************************************************/
ERRORTYPE VideoEncComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    VIDEOENCDATATYPE *pVideoEncData;
    ERRORTYPE eError = SUCCESS;
    unsigned int err;
    int i = 0;

    pComp = (MM_COMPONENTTYPE *) hComponent;

    // Create private data
    pVideoEncData = (VIDEOENCDATATYPE *) malloc(sizeof(VIDEOENCDATATYPE));
    memset(pVideoEncData, 0x0, sizeof(VIDEOENCDATATYPE));

#ifdef VENC_IGNORE_ALL_FRAME
    pVideoEncData->mbDebug_IgnoreAllFrame = 1;
#endif

    err = pipe(pVideoEncData->mOutputFrameNotifyPipeFds);
    if (err)
    {
        aloge("fatal error! pipe fail[0x%x]", err);
        eError = ERR_VENC_NOMEM;
        goto EXIT;
    }
    pVideoEncData->mBufQ.buf_unused = ENC_FIFO_LEVEL;
    INIT_LIST_HEAD(&pVideoEncData->mBufQ.mIdleFrameList);
    INIT_LIST_HEAD(&pVideoEncData->mBufQ.mReadyFrameList);
    INIT_LIST_HEAD(&pVideoEncData->mBufQ.mUsingFrameList);
    INIT_LIST_HEAD(&pVideoEncData->mBufQ.mUsedFrameList);
    for (i = 0; i < ENC_FIFO_LEVEL; i++)
    {
        VideoFrameInfoNode *pNode = (VideoFrameInfoNode*)malloc(sizeof(VideoFrameInfoNode));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
            eError = ERR_VENC_NOMEM;
            goto EXIT;
        }
        memset(pNode, 0, sizeof(VideoFrameInfoNode));
        list_add_tail(&pNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
    }
    pComp->pComponentPrivate = (void*) pVideoEncData;
    pVideoEncData->state = COMP_StateLoaded;
    pthread_mutex_init(&pVideoEncData->mStateLock,NULL);
    pVideoEncData->hSelf = hComponent;

    pthread_mutex_init(&pVideoEncData->mFrameRateLock, NULL);
    pVideoEncData->timeLapseEnable = FALSE;
    pthread_mutex_init(&pVideoEncData->mCapTimeLapseLock, NULL);
    pVideoEncData->csi_first_frame = TRUE;
    pthread_mutex_init(&pVideoEncData->mutex_fifo_ops_lock ,NULL);

    cdx_sem_init(&pVideoEncData->mInputFrameBufDoneSem, 0);

    pVideoEncData->mFlagInputFrame = 0;
    INIT_LIST_HEAD(&pVideoEncData->mIdleOutFrameList);
    INIT_LIST_HEAD(&pVideoEncData->mReadyOutFrameList);
    INIT_LIST_HEAD(&pVideoEncData->mUsedOutFrameList);
    INIT_LIST_HEAD(&pVideoEncData->mRoiCfgList);
    pthread_mutex_init(&pVideoEncData->mRoiLock, NULL);

    //init color2grey is close
    pVideoEncData->mColor2GreyParam.bColor2Grey = FALSE;

    //init 2dfilter(2dnr) is open
    pVideoEncData->m2DfilterParam.enable_2d_filter = 1;
    pVideoEncData->m2DfilterParam.filter_strength_y = 127;
    pVideoEncData->m2DfilterParam.filter_strength_uv = 127;
    pVideoEncData->m2DfilterParam.filter_th_y = 11;
    pVideoEncData->m2DfilterParam.filter_th_uv = 7;

    //init 3dfilter(3dnr) is open
    pVideoEncData->m3DfilterParam.enable_3d_filter = 1;
    pVideoEncData->m3DfilterParam.adjust_pix_level_enable = 0;
    pVideoEncData->m3DfilterParam.smooth_filter_enable = 1;
    pVideoEncData->m3DfilterParam.max_pix_diff_th = 6;
    pVideoEncData->m3DfilterParam.max_mv_th = 8;
    pVideoEncData->m3DfilterParam.max_mad_th = 14;
    pVideoEncData->m3DfilterParam.min_coef = 13;
    pVideoEncData->m3DfilterParam.max_coef = 16;

    //init HorizonFlip and AdaptiveIntraInPFlag  is closed
    pVideoEncData->mHorizonFlipFlag = FALSE;
    pVideoEncData->mAdaptiveIntraInPFlag = FALSE;

    //init procset
    pVideoEncData->mProcSet.bProcEnable = 1;
    pVideoEncData->mProcSet.nProcFreq = 120;
    pVideoEncData->mProcSet.nStatisBitRateTime = 1000;
    pVideoEncData->mProcSet.nStatisFrRateTime = 1000;

    pVideoEncData->mJpegParam.Qfactor = 95;

    //init SaveBsFile, disable it
    memset(&pVideoEncData->mSaveBSFile, 0, sizeof(VencSaveBSFile));

    //init RecvPic
    pthread_mutex_init(&pVideoEncData->mRecvPicControlLock, NULL);
    pVideoEncData->mLimitedFramesFlag = FALSE;
    pVideoEncData->mRecvPicParam.mRecvPicNum = 0;
    pVideoEncData->mCurRecvPicNum = 0;

    pthread_mutex_init(&pVideoEncData->mCedarvVideoEncInitFlagLock, NULL);
    pVideoEncData->mbCedarvVideoEncInitFlag = FALSE;
    //pthread_mutex_init(&pVideoEncData->mVencOverlayLock, NULL);

    for(i=0;i<BITSTREAM_FRAME_SIZE;i++)
    {
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        memset(pNode, 0, sizeof(ENCODER_NODE_T));
        list_add_tail(&pNode->mList, &pVideoEncData->mIdleOutFrameList);
        pVideoEncData->mFrameNodeNum++;
    }
    err = pthread_mutex_init(&pVideoEncData->mOutFrameListMutex, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
        eError = ERR_VENC_NOMEM;
        goto EXIT;
    }
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    err = pthread_cond_init(&pVideoEncData->mOutFrameFullCondition, &condAttr);
    if(err!=0)
    {
        aloge("fatal error! pthread cond init fail!");
        eError = ERR_VENC_NOMEM;
        goto EXIT;
    }
    pthread_cond_init(&pVideoEncData->mOutFrameCondition, &condAttr);

    pthread_cond_init(&pVideoEncData->mInputFrameUsingEmptyCondition, &condAttr);

    // Fill in function pointers
    pComp->SetCallbacks     = VideoEncSetCallbacks;
    pComp->SendCommand      = VideoEncSendCommand;
    pComp->GetConfig        = VideoEncGetConfig;
    pComp->SetConfig        = VideoEncSetConfig;
    pComp->GetState         = VideoEncGetState;
    pComp->ComponentTunnelRequest = VideoEncComponentTunnelRequest;
    pComp->ComponentDeInit  = VideoEncComponentDeInit;
    pComp->EmptyThisBuffer  = VideoEncEmptyThisBuffer;
    pComp->FillThisBuffer   = VideoEncFillThisBuffer;

    // Initialize component data structures to default values
    pVideoEncData->sPortParam.nPorts = 0x2;
    pVideoEncData->sPortParam.nStartPortNumber = 0x0;

    // Initialize the video parameters for input port
    pVideoEncData->sInPortDef.nPortIndex = 0x0;
    pVideoEncData->sInPortDef.bEnabled = TRUE;
    pVideoEncData->sInPortDef.eDomain = COMP_PortDomainVideo;
    pVideoEncData->sInPortDef.format.video.cMIMEType = "H264";
    pVideoEncData->sInPortDef.format.video.nFrameWidth = 176;
    pVideoEncData->sInPortDef.format.video.nFrameHeight = 144;
    pVideoEncData->sInPortDef.eDir = COMP_DirInput;
    pVideoEncData->sInPortDef.format.video.eCompressionFormat = PT_H264;
    pVideoEncData->sInPortDef.format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    // Initialize the video parameters for output port
    pVideoEncData->sOutPortDef.nPortIndex = 0x1;
    pVideoEncData->sOutPortDef.bEnabled = TRUE;
    pVideoEncData->sOutPortDef.eDomain = COMP_PortDomainVideo;
    pVideoEncData->sOutPortDef.format.video.cMIMEType = "YVU420";
    pVideoEncData->sOutPortDef.format.video.nFrameWidth = 176;
    pVideoEncData->sOutPortDef.format.video.nFrameHeight = 144;
    pVideoEncData->sOutPortDef.eDir = COMP_DirOutput;
    pVideoEncData->sOutPortDef.format.video.eCompressionFormat = PT_H264;
    pVideoEncData->sOutPortDef.format.video.eColorFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    pVideoEncData->sPortBufSupplier[0].nPortIndex = 0x0;
    pVideoEncData->sPortBufSupplier[0].eBufferSupplier = COMP_BufferSupplyOutput;
    pVideoEncData->sPortBufSupplier[1].nPortIndex = 0x1;
    pVideoEncData->sPortBufSupplier[1].eBufferSupplier = COMP_BufferSupplyOutput;

    pVideoEncData->sInPortTunnelInfo.nPortIndex = 0x0;
    pVideoEncData->sInPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;
    pVideoEncData->sOutPortTunnelInfo.nPortIndex = 0x1;
    pVideoEncData->sOutPortTunnelInfo.eTunnelType = TUNNEL_TYPE_COMMON;

    if (message_create(&pVideoEncData->cmd_queue)<0)
    {
        aloge("fatal error! message error!");
        eError = ERR_VENC_NOMEM;
        goto EXIT;
    }

    // Create the component thread
    pthread_attr_t attr;
    int rs;
    /* init thread attr */
    rs = pthread_attr_init (&attr);
    assert (rs == 0);
    //pthread_attr_setstacksize(&attr, 0x40000);
    #if 0
    struct sched_param schedParam;
    /* get current sched policy */
    int curPolicy;
    int newPolicy = SCHED_FIFO;
    rs = pthread_attr_getschedpolicy(&attr, &curPolicy);
    assert (rs == 0);
    if(curPolicy != newPolicy)
    {
        alogd("we will change venc thread sched policy [%d]->[%d]", curPolicy, newPolicy);
    }
    /* get priority scope of SCHED_FIFO policy */
    int maxPriority = sched_get_priority_max(newPolicy);
    assert(maxPriority != -1);
    int minPriority = sched_get_priority_min(newPolicy);
    assert(minPriority != -1);
    alogd("priority[%d] scope[%d, %d]", newPolicy, minPriority, maxPriority);
    /* set sched policy SCHED_FIFO */
    rs = pthread_attr_setschedpolicy(&attr, newPolicy);
    assert (rs == 0);
    /* set priority */
    rs = pthread_attr_getschedparam(&attr, &schedParam);
    assert (rs == 0);
    int curSchedPriority = schedParam.__sched_priority;
    int newSchedPriority = (maxPriority + minPriority)/2;
    if (curSchedPriority != newSchedPriority)
    {
        schedParam.__sched_priority = newSchedPriority;
        rs = pthread_attr_setschedparam(&attr, &schedParam);
        assert (rs == 0);
    }
    alogd("in schedPolicy[%d], change venc thread priority [%d]->[%d]", newPolicy, curSchedPriority, newSchedPriority);
    #endif
    err = pthread_create(&pVideoEncData->thread_id, &attr, ComponentThread, pVideoEncData);
    /* destroy pthread_attr_t */
    rs = pthread_attr_destroy (&attr);
    assert (rs == 0);
    if (err /*|| !pVideoEncData->thread_id*/)
    {
        eError = ERR_VENC_NOMEM;
        goto EXIT;
    }
    alogd("create VideoEnc threadId:0x%lx", pVideoEncData->thread_id);
    EXIT: 
    if(eError != SUCCESS)
    {
        aloge("fatal error! VideoEnc init fail[0x%x]", eError);
    }
    return eError;
}

/**
 * analyse if compressed frame is key frame, and spspps info.
 * @return true if it is key frame.
 */
static BOOL VideoEncAnalyseCompressedFrame(VIDEOENCDATATYPE* pVideoEncData, FRAMEDATATYPE *pCompressedFrame)
{
    BOOL bKeyFrame;
    if(PT_JPEG == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_MJPEG == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        bKeyFrame = TRUE;
    }
    else if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        aloge("fatal error! need analyse h264 data!");
        bKeyFrame = TRUE;
        if(NULL == pVideoEncData->mpVencHeaderData)
        {
            //analyse spspps info of h264, nal_type 0x67, 0x68.
        }
    }
    else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        aloge("fatal error! need analyse h265 data!");
        bKeyFrame = TRUE;
        if(NULL == pVideoEncData->mpVencHeaderData)
        {
            //analyse spspps info of h265
        }
    }
    else
    {
        aloge("fatal error! unknown encodeType[0x%x]", pVideoEncData->mEncChnAttr.VeAttr.Type);
        bKeyFrame = FALSE;
    }
    return bKeyFrame;
}

/**
 * statistics period is Key frame interval.
 * If actual bitRate is 1.5 or more times than dstBitRate, print warning!
 */
static void statVideoBitRate(VIDEOENCDATATYPE *pVideoEncData, VencOutputBuffer *pVencOutputBuffer)
{
    if(PT_H264!=pVideoEncData->mEncChnAttr.VeAttr.Type && PT_H265!=pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        return;
    }
    int nKeyFrameInterval = pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval;
    int nFrameRate = pVideoEncData->mFrameRateInfo.DstFrmRate;
    unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
    if(pVencOutputBuffer->nFlag & VENC_BUFFERFLAG_KEYFRAME)
    {
        //deduce result, and compare to dstBitRate
        if ((pVideoEncData->mFrameNumInKeyFrameInvertal) && (pVideoEncData->mFrameNumInKeyFrameInvertal == nKeyFrameInterval))
        {
            unsigned int nActualBitRate = (unsigned int)((int64_t)pVideoEncData->mBytesKeyFrameInterval*nFrameRate*8/pVideoEncData->mFrameNumInKeyFrameInvertal);
#ifdef CHECK_VIDEO_ENCODE_OUT_BITRATE
            if (nBitRate)
            {
                float bitrateTimes = (float)nActualBitRate / nBitRate;
                alogd("VencChn[%d] actual bitRate[%d Kbps] / dstBitRate[%d Kbps] = %.2f, [%d-%d-%d][VencCh%d-%s]",
                    pVideoEncData->mMppChnInfo.mChnId, nActualBitRate/1024, nBitRate/1024, bitrateTimes,
                    pVideoEncData->mBytesKeyFrameInterval, pVideoEncData->mFrameNumInKeyFrameInvertal, nFrameRate,
                    pVideoEncData->mMppChnInfo.mChnId, PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type ? "H264" : "H265");
            }
#else
            if ((nBitRate) && (nActualBitRate/nBitRate >= 2))
            {
                alogw("VencChn[%d] Be careful! actual bitRate[%d] / dstBitRate[%d] = %d, too large! [%d-%d-%d]", pVideoEncData->mMppChnInfo.mChnId, nActualBitRate, nBitRate, 
                    nActualBitRate/nBitRate, pVideoEncData->mBytesKeyFrameInterval, pVideoEncData->mFrameNumInKeyFrameInvertal, nFrameRate);
            }
#endif
        }
        else
        {
            if(pVideoEncData->mFrameNumInKeyFrameInvertal != 0)
            {
                alogw("keyFrameInterval is not match [%d-%d]!=[%d], maybe forceIFrame, give up statistics!", pVideoEncData->mBytesKeyFrameInterval, pVideoEncData->mFrameNumInKeyFrameInvertal, nKeyFrameInterval);
            }
        }
        pVideoEncData->mFrameNumInKeyFrameInvertal = 0;
        pVideoEncData->mBytesKeyFrameInterval = 0;
    }
    pVideoEncData->mFrameNumInKeyFrameInvertal++;
    pVideoEncData->mBytesKeyFrameInterval += (pVencOutputBuffer->nSize0 + pVencOutputBuffer->nSize1);
}

static int SendOrigFrameToVencLib(VIDEOENCDATATYPE *pVideoEncData, VideoFrameInfoNode *pFrameNode)
{
    VencInputBuffer encBuf;
    VideoEncoder *pCedarV = NULL;

    if ((NULL == pVideoEncData) || (NULL == pFrameNode))
    {
        aloge("fatal error! invalid input param!");
        return -1;
    }

    pCedarV = pVideoEncData->pCedarV;
    if (NULL == pCedarV)
    {
        aloge("fatal error! invalid input param!");
        return -1;
    }

    memset(&encBuf, 0, sizeof(VencInputBuffer));

    encBuf.nID = pFrameNode->VFrame.mId;
    encBuf.nPts = (long long)pFrameNode->VFrame.VFrame.mpts/* - pVideoEncData->csi_base_time*/;
    encBuf.nFlag = 0;
    encBuf.pAddrPhyY = (unsigned char*)pFrameNode->VFrame.VFrame.mPhyAddr[0];
    encBuf.pAddrPhyC = (unsigned char*)pFrameNode->VFrame.VFrame.mPhyAddr[1];
    encBuf.pAddrVirY = (unsigned char*)pFrameNode->VFrame.VFrame.mpVirAddr[0];
    encBuf.pAddrVirC = (unsigned char*)pFrameNode->VFrame.VFrame.mpVirAddr[1];

#ifdef STORE_VENC_IN_FRAME
    if ((STORE_VENC_CH_ID == pVideoEncData->mMppChnInfo.mChnId) &&
        (STORE_VENC_FRAME_TOTAL_CNT > g_debug_frame_cnt))
    {
        if (g_debug_fp_frame == NULL)
        {
            g_debug_fp_frame = fopen(STORE_VENC_FRAME_FILE, "wb");
            assert(g_debug_fp_frame != NULL);
            alogd("####### [DEBUG] open %s, VencChn%d #######", STORE_VENC_FRAME_FILE, STORE_VENC_CH_ID);
        }
        if (pFrameNode->VFrame.VFrame.mStride[0] > 0)
            fwrite(encBuf.pAddrVirY, 1, pFrameNode->VFrame.VFrame.mStride[0], g_debug_fp_frame);
        if (pFrameNode->VFrame.VFrame.mStride[1] > 0)
            fwrite(encBuf.pAddrVirC, 1, pFrameNode->VFrame.VFrame.mStride[1], g_debug_fp_frame);
        g_debug_frame_cnt++;
    }
#endif

    if (pVideoEncData->timeLapseEnable)
    {
        pthread_mutex_lock(&pVideoEncData->mCapTimeLapseLock);
        if (pVideoEncData->mCapTimeLapse.recType == COMP_RECORD_TYPE_SLOW)
        {
            encBuf.nPts = pVideoEncData->mCapTimeLapse.lastTimeStamp;
            pVideoEncData->mCapTimeLapse.lastTimeStamp += pVideoEncData->mCapTimeLapse.videoFrameIntervalUs;
        }
        else if (pVideoEncData->mCapTimeLapse.recType == COMP_RECORD_TYPE_TIMELAPSE)
        {
            if (pVideoEncData->mCapTimeLapse.lastCapTimeUs > 0
                && pFrameNode->VFrame.VFrame.mpts < pVideoEncData->mCapTimeLapse.lastCapTimeUs + pVideoEncData->mCapTimeLapse.capFrameIntervalUs)
            {
                pthread_mutex_unlock(&pVideoEncData->mCapTimeLapseLock);
                // The original input frame has been returned and no further processing is required.
                return 1;
            }
            if(pVideoEncData->mCapTimeLapse.lastCapTimeUs <= 0)
            {
                pVideoEncData->mCapTimeLapse.lastCapTimeUs = pFrameNode->VFrame.VFrame.mpts;
            }
            else
            {
                int64_t n = (pFrameNode->VFrame.VFrame.mpts - pVideoEncData->mCapTimeLapse.lastCapTimeUs - pVideoEncData->mCapTimeLapse.capFrameIntervalUs)/pVideoEncData->mCapTimeLapse.capFrameIntervalUs;
                pVideoEncData->mCapTimeLapse.lastCapTimeUs += (n+1)*pVideoEncData->mCapTimeLapse.capFrameIntervalUs;
            }
            encBuf.nPts = pVideoEncData->mCapTimeLapse.lastTimeStamp;
            pVideoEncData->mCapTimeLapse.lastTimeStamp += pVideoEncData->mCapTimeLapse.videoFrameIntervalUs;
        }
        pthread_mutex_unlock(&pVideoEncData->mCapTimeLapseLock);
    }
    else
    {
        pthread_mutex_lock(&pVideoEncData->mFrameRateLock);
        if (pVideoEncData->mSoftFrameRateCtrl.enable)
        {
            if(-1 == pVideoEncData->mSoftFrameRateCtrl.mBasePts)
            {
                pVideoEncData->mSoftFrameRateCtrl.mBasePts = encBuf.nPts;
                pVideoEncData->mSoftFrameRateCtrl.mCurrentWantedPts = pVideoEncData->mSoftFrameRateCtrl.mBasePts;
                pVideoEncData->mSoftFrameRateCtrl.mFrameCounter = 0;
            }
            if(encBuf.nPts >= pVideoEncData->mSoftFrameRateCtrl.mCurrentWantedPts)
            {
                int videoFramerate = pVideoEncData->mFrameRateInfo.DstFrmRate*1000;
                pVideoEncData->mSoftFrameRateCtrl.mFrameCounter++;
                pVideoEncData->mSoftFrameRateCtrl.mCurrentWantedPts = pVideoEncData->mSoftFrameRateCtrl.mBasePts + (int64_t)pVideoEncData->mSoftFrameRateCtrl.mFrameCounter*(1000*1000) * 1000 / videoFramerate;
            }
            else
            {
                pthread_mutex_unlock(&pVideoEncData->mFrameRateLock);
                // The original frame has been returned and no further processing is required.
                return 1;
            }
        }
        pthread_mutex_unlock(&pVideoEncData->mFrameRateLock);
    }

    if (pVideoEncData->mCropCfg.bEnable)
    {
        encBuf.bEnableCorp = 1;
        encBuf.sCropInfo.nLeft = pVideoEncData->mCropCfg.Rect.X;
        encBuf.sCropInfo.nTop = pVideoEncData->mCropCfg.Rect.Y;
        encBuf.sCropInfo.nWidth = pVideoEncData->mCropCfg.Rect.Width;
        encBuf.sCropInfo.nHeight= pVideoEncData->mCropCfg.Rect.Height;
        alogv("crop enable, Left:%d Top:%d Width:%d Height:%d",
            encBuf.sCropInfo.nLeft, encBuf.sCropInfo.nTop, encBuf.sCropInfo.nWidth, encBuf.sCropInfo.nHeight);
    }
    else
    {
        int showWidth = pFrameNode->VFrame.VFrame.mOffsetRight - pFrameNode->VFrame.VFrame.mOffsetLeft;
        int showHeight = pFrameNode->VFrame.VFrame.mOffsetBottom - pFrameNode->VFrame.VFrame.mOffsetTop;
        if(showWidth != pFrameNode->VFrame.VFrame.mWidth || showHeight != pFrameNode->VFrame.VFrame.mHeight)
        {
            if ((MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X == pFrameNode->VFrame.VFrame.mPixelFormat) ||
                (MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X == pFrameNode->VFrame.VFrame.mPixelFormat) ||
                (MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X == pFrameNode->VFrame.VFrame.mPixelFormat) ||
                (MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X == pFrameNode->VFrame.VFrame.mPixelFormat))
            {
                if(ALIGN(showHeight, 16) != pFrameNode->VFrame.VFrame.mHeight)
                {
                    aloge("fatal error! VencChn[%d] lbc format[%d] do not support crop![%dx%d, %dx%d]", 
                        pVideoEncData->mMppChnInfo.mChnId, pFrameNode->VFrame.VFrame.mPixelFormat,
                        showWidth, showHeight, pFrameNode->VFrame.VFrame.mWidth, pFrameNode->VFrame.VFrame.mHeight);
                }
            }
            else
            {
                encBuf.bEnableCorp = 1;
                encBuf.sCropInfo.nLeft = pFrameNode->VFrame.VFrame.mOffsetLeft;
                encBuf.sCropInfo.nTop = pFrameNode->VFrame.VFrame.mOffsetTop;
                encBuf.sCropInfo.nWidth = showWidth;
                encBuf.sCropInfo.nHeight= showHeight;
                alogv("need enable crop. VencChn[%d] show size[%dx%d]!= frameSize[%dx%d], %d-%d-%d-%d--%d-%d!",
                        pVideoEncData->mMppChnInfo.mChnId,
                        showWidth, showHeight,
                        pFrameNode->VFrame.VFrame.mWidth,
                        pFrameNode->VFrame.VFrame.mHeight,
                        pFrameNode->VFrame.VFrame.mOffsetLeft,
                        pFrameNode->VFrame.VFrame.mOffsetRight,
                        pFrameNode->VFrame.VFrame.mOffsetTop,
                        pFrameNode->VFrame.VFrame.mOffsetBottom,
                        pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth,
                        pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight);
            }
        }
        else
        {
            if(pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth != pFrameNode->VFrame.VFrame.mWidth
                || pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight != pFrameNode->VFrame.VFrame.mHeight)
            {
                alogw("fatal error! enc src_size[%dx%d]!= frameSize[%dx%d], %d-%d-%d-%d!",
                    pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth,
                    pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight,
                    pFrameNode->VFrame.VFrame.mWidth,
                    pFrameNode->VFrame.VFrame.mHeight,
                    pFrameNode->VFrame.VFrame.mOffsetLeft,
                    pFrameNode->VFrame.VFrame.mOffsetRight,
                    pFrameNode->VFrame.VFrame.mOffsetTop,
                    pFrameNode->VFrame.VFrame.mOffsetBottom);
            }
            encBuf.bEnableCorp = 0;
        }
    }

    encBuf.ispPicVar = 0;
    encBuf.envLV = pFrameNode->VFrame.VFrame.mEnvLV;
    pthread_mutex_lock(&pVideoEncData->mRoiLock);
    VideoEncConfigVencInputBufferRoiParam(pVideoEncData, &encBuf);
    pthread_mutex_unlock(&pVideoEncData->mRoiLock);

    encBuf.bNeedFlushCache = 1;
    int ret = VencQueueInputBuf(pCedarV, &encBuf);
    if (ret != 0)
    {
        aloge("fatal error! Venc QueueInputBuf fail, ret=%d", ret);
        return -2;
    }

    return 0;
}

static void VideoEncSendBackInputFrame(VIDEOENCDATATYPE *pVideoEncData, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    COMP_BUFFERHEADERTYPE BufferHeader;
    memset(&BufferHeader, 0, sizeof(COMP_BUFFERHEADERTYPE));
    if (FALSE == pVideoEncData->mInputPortTunnelFlag)
    {
        BufferHeader.pAppPrivate = pFrameInfo;
        pVideoEncData->pCallbacks->EmptyBufferDone(
                pVideoEncData->hSelf,
                pVideoEncData->pAppData,
                &BufferHeader);
    }
    else
    {
        BufferHeader.pOutputPortPrivate = pFrameInfo;
        BufferHeader.nOutputPortIndex = pVideoEncData->sInPortTunnelInfo.nTunnelPortIndex;
        BufferHeader.nInputPortIndex = pVideoEncData->sInPortTunnelInfo.nPortIndex;
        COMP_FillThisBuffer(pVideoEncData->sInPortTunnelInfo.hTunnel, &BufferHeader);
    }
    alogv("release input FrameId[%d]", pFrameInfo->mId);
}

static int ReturnUseOrigFrameBeforeSetStateIdle(VIDEOENCDATATYPE *pVideoEncData)
{
    if (NULL == pVideoEncData)
    {
        aloge("fatal error! invalid input param");
        return -1;
    }

    // Wait for usingFrameList is empty.
    if (TRUE == pVideoEncData->mbForbidDiscardingFrame)
    {
        while (1)
        {
            pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
            if (list_empty(&pVideoEncData->mBufQ.mUsingFrameList))
            {
                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                break;
            }
            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

            // Wait for vencLib to finish processing.
            alogd("wait for sem inputFrameBufDone 200ms ...");
            int semRet = cdx_sem_down_timedwait(&pVideoEncData->mInputFrameBufDoneSem, 200); // unit: ms
            if (ETIMEDOUT == semRet)
            {
                alogw("wait inputFrameBufDoneSem 200ms timeout.");
                continue;
            }
            else if (0 != semRet)
            {
                aloge("fatal error! cdx sem_down_timedwait fail, ret=%d", semRet);
            }
            else
            {
                alogd("got sem inputFrameBufDone");
            }
        }
    }
    else
    {
        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
        pVideoEncData->mWaitInputFrameUsingEmptyFlag = TRUE;
        while (1)
        {
            if (list_empty(&pVideoEncData->mBufQ.mUsingFrameList))
            {
                alogd("VencChn[%d] input orig Using FrameList is empty", pVideoEncData->mMppChnInfo.mChnId);
                break;
            }
            int cnt = 0;
            struct list_head *pList;
            list_for_each(pList, &pVideoEncData->mBufQ.mUsingFrameList)
            {
                cnt++;
            }

            alogw("wait for VencChn[%d] input orig usingFrameList empty, left frames cnt = [%d]", pVideoEncData->mMppChnInfo.mChnId, cnt);
            pthread_cond_wait(&pVideoEncData->mInputFrameUsingEmptyCondition, &pVideoEncData->mutex_fifo_ops_lock);
            alogw("wait for VencChn[%d] input orig usingFrameList empty done", pVideoEncData->mMppChnInfo.mChnId);
        }
        pVideoEncData->mWaitInputFrameUsingEmptyFlag = FALSE;
        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
    }

    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
    // for return Used orig list from VencComp InputBufferDone callback.
    if (!list_empty(&pVideoEncData->mBufQ.mUsedFrameList))
    {
        int num = 0;
        VideoFrameInfoNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mBufQ.mUsedFrameList, mList)
        {
            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
            VideoEncSendBackInputFrame(pVideoEncData, &pEntry->VFrame);
            pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
            list_move_tail(&pEntry->mList, &pVideoEncData->mBufQ.mIdleFrameList);
            pVideoEncData->mBufQ.buf_unused++;
            alogv("VencChn[%d] release input orig frame ID[%d], list: Used -> Idle, buf unused[%d]", pVideoEncData->mMppChnInfo.mChnId, pEntry->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
            num++;
        }
        alogd("VencChn[%d] return input orig [%d]frames list: Used -> Idle", pVideoEncData->mMppChnInfo.mChnId, num);
    }
    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

    return 0;
}

static int UpdateReadyOutFrameList_l(VIDEOENCDATATYPE *pVideoEncData, VencOutputBuffer *pOutputBuffer)
{
    if ((NULL == pVideoEncData) || (NULL == pOutputBuffer))
    {
        aloge("fatal error! invalid input params, %p, %p", pVideoEncData, pOutputBuffer);
        return -1;
    }

    if (list_empty(&pVideoEncData->mIdleOutFrameList))
    {
        alogv("VencChn[%d] idle out frame list is empty, curNum[%d], malloc more!", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mFrameNodeNum);
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if (pNode)
        {
            memset(pNode, 0, sizeof(ENCODER_NODE_T));
            list_add_tail(&pNode->mList, &pVideoEncData->mIdleOutFrameList);
            pVideoEncData->mFrameNodeNum++;
        }
        else
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            return ERR_VENC_NOMEM;
        }
    }

    ENCODER_NODE_T *pEntry = list_first_entry(&pVideoEncData->mIdleOutFrameList, ENCODER_NODE_T, mList);
    statVideoBitRate(pVideoEncData, pOutputBuffer);
    config_EncodedStream_By_VencOutputBuffer(&pEntry->stEncodedStream, pOutputBuffer);
    if(pVideoEncData->bForceKeyFrameFlag)
    {
        if(pOutputBuffer->nFlag & VENC_BUFFERFLAG_KEYFRAME)
        {
            alogd("VeChn[%d] receive sdk force I frame", pVideoEncData->mMppChnInfo.mChnId);
            pVideoEncData->bForceKeyFrameFlag = FALSE;
        }
    }

    SIZE_S dstPicSize;
    if (SUCCESS == getPicSizeFromVENC_ATTR_S(&pVideoEncData->mEncChnAttr.VeAttr, &dstPicSize))
    {
        unsigned int outSize = pOutputBuffer->nSize0 + pOutputBuffer->nSize1;
        if (outSize > dstPicSize.Width*dstPicSize.Height*3/4)
        {
            alogw("VencChn[%d] Impossible! large frameSize[%d]Byte([%f]MB), [%d][%d], dstSize[%dx%d]!", pVideoEncData->mMppChnInfo.mChnId, outSize, (float)outSize/(1024*1024), pOutputBuffer->nSize0, pOutputBuffer->nSize1, dstPicSize.Width, dstPicSize.Height);
        }
        if (outSize > pVideoEncData->mStatMaxFrameSize)
        {
            pVideoEncData->mStatMaxFrameSize = outSize;
            alogd("VencChn[%d] current max frameSize[%d]Byte([%f]MB), dstSize[%dx%d]!", pVideoEncData->mMppChnInfo.mChnId, outSize, (float)outSize/(1024*1024), dstPicSize.Width, dstPicSize.Height);
        }
        pVideoEncData->mStatStreamSize += outSize;
    }

    list_move_tail(&pEntry->mList, &pVideoEncData->mReadyOutFrameList);
    alogv("output coded frame ID[%d] list: Idle -> Ready", pEntry->stEncodedStream.nID);
    if (FALSE == pVideoEncData->mOutputPortTunnelFlag)
    {
        char tmpWrCh = 'V';
        write(pVideoEncData->mOutputFrameNotifyPipeFds[1], &tmpWrCh, 1);
    }

#ifdef STORE_VENC_OUT_STREAM
    if (STORE_VENC_CH_ID == pVideoEncData->mMppChnInfo.mChnId)
    {
        if (g_debug_fp_stream == NULL)
        {
            g_debug_fp_stream = fopen(STORE_VENC_STREAM_FILE, "wb");
            assert(g_debug_fp_stream != NULL);
            alogd("####### [DEBUG] open %s, VencChn%d #######", STORE_VENC_STREAM_FILE, STORE_VENC_CH_ID);
            VencHeaderData stHeaderData;
            memset(&stHeaderData, 0, sizeof(VencHeaderData));
            if(PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type)
            {
                VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264SPSPPS, (void*)&stHeaderData);
            }
            else if(PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
            {
                VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamH265Header, (void*)&stHeaderData);
            }
            if (stHeaderData.nLength)
            {
                fwrite(stHeaderData.pBuffer, 1, stHeaderData.nLength, g_debug_fp_stream);
            }
        }
        if (pOutputBuffer->nSize0 > 0)
            fwrite(pOutputBuffer->pData0, 1, pOutputBuffer->nSize0, g_debug_fp_stream);
        if (pOutputBuffer->nSize1 > 0)
            fwrite(pOutputBuffer->pData1, 1, pOutputBuffer->nSize1, g_debug_fp_stream);
        if (pOutputBuffer->nSize2 > 0)
            fwrite(pOutputBuffer->pData2, 1, pOutputBuffer->nSize2, g_debug_fp_stream);
        g_debug_stream_cnt++;
    }
#endif

    return 0;
}

static int ProcessReadyOutFrameList(VIDEOENCDATATYPE *pVideoEncData)
{
    int ret = 0;
    if (FALSE == pVideoEncData->mOutputPortTunnelFlag)
    {
        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
        if (!list_empty(&pVideoEncData->mReadyOutFrameList))
        {
            if (pVideoEncData->mWaitOutFrameFlag)
            {
                alogv("vencChn[%d] send cond signal OutFrameCondition", pVideoEncData->mMppChnInfo.mChnId);
                pthread_cond_signal(&pVideoEncData->mOutFrameCondition);
            }
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
        ret = 0;
    }
    else
    {
        //send all readyOutFrames to next tunnel-component.
        if (!list_empty(&pVideoEncData->mReadyOutFrameList))
        {
            int num = 0;
            ENCODER_NODE_T *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mReadyOutFrameList, mList)
            {
                pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                list_move_tail(&pEntry->mList, &pVideoEncData->mUsedOutFrameList);
                alogv("output coded frame ID[%d] list: Ready -> Used", pEntry->stEncodedStream.nID);
                pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);  // unlock before calling Empty ThisBuffer
                num++;

                COMP_BUFFERHEADERTYPE    obh;
                obh.nOutputPortIndex = pVideoEncData->sOutPortTunnelInfo.nPortIndex;
                obh.nInputPortIndex = pVideoEncData->sOutPortTunnelInfo.nTunnelPortIndex;
                obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
                alogv("pOutTunnelComp->Empty ThisBuffer for non-compress source");
                ERRORTYPE omxRet = COMP_EmptyThisBuffer(pVideoEncData->sOutPortTunnelInfo.hTunnel, &obh);
                if (SUCCESS == omxRet)
                {
                    //Notes: Must move buf node to UsedList before calling Empty ThisBuffer
                }
                else
                {
                    aloge("fatal error! VEnc output frame fail[0x%x], return frame to venc!", omxRet);
                    VencOutputBuffer stOutputBuffer;
                    config_VencOutputBuffer_By_EncodedStream(&stOutputBuffer, &pEntry->stEncodedStream);
                    int vencLibRet = VencQueueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                    if (vencLibRet != 0)
                    {
                        aloge("fatal error! Venc QueueOutputBuf fail[%d], returning stream may cause vencLib bitStream management error!", vencLibRet);
                    }
                    pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                    list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
                    alogv("output coded frame ID[%d] list: Used -> Idle", pEntry->stEncodedStream.nID);
                    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                }
            }
        }
        ret = 0;
    }
    return ret;
}

/*
static int UpdateOutputFrameList(VIDEOENCDATATYPE *pVideoEncData, VencOutputBuffer *pOutputBuffer)
{
    if ((NULL == pVideoEncData) || (NULL == pOutputBuffer))
    {
        aloge("fatal error! invalid input params, %p, %p", pVideoEncData, pOutputBuffer);
        return -1;
    }

    pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
    if (list_empty(&pVideoEncData->mIdleOutFrameList))
    {
        alogv("Low probability! VEncComp idle out frame list is empty, curNum[%d], malloc more!", pVideoEncData->mFrameNodeNum);
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if (pNode)
        {
            memset(pNode, 0, sizeof(ENCODER_NODE_T));
            list_add_tail(&pNode->mList, &pVideoEncData->mIdleOutFrameList);
            pVideoEncData->mFrameNodeNum++;
        }
        else
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            return ERR_VENC_NOMEM;
        }
    }

    ENCODER_NODE_T *pEntry = list_first_entry(&pVideoEncData->mIdleOutFrameList, ENCODER_NODE_T, mList);
    statVideoBitRate(pVideoEncData, pOutputBuffer);
    config_EncodedStream_By_VencOutputBuffer(&pEntry->stEncodedStream, pOutputBuffer);

    SIZE_S dstPicSize;
    if (SUCCESS == getPicSizeFromVENC_ATTR_S(&pVideoEncData->mEncChnAttr.VeAttr, &dstPicSize))
    {
        unsigned int outSize = pOutputBuffer->nSize0 + pOutputBuffer->nSize1;
        if (outSize > dstPicSize.Width*dstPicSize.Height*3/4)
        {
            alogw("Impossible! large frameSize[%d]Byte([%f]MB), [%d][%d], dstSize[%dx%d]!", outSize, (float)outSize/(1024*1024), pOutputBuffer->nSize0, pOutputBuffer->nSize1, dstPicSize.Width, dstPicSize.Height);
        }
        if (outSize > pVideoEncData->mStatMaxFrameSize)
        {
            pVideoEncData->mStatMaxFrameSize = outSize;
            alogd("current max frameSize[%d]Byte([%f]MB), dstSize[%dx%d]!", outSize, (float)outSize/(1024*1024), dstPicSize.Width, dstPicSize.Height);
        }
        pVideoEncData->mStatStreamSize += outSize;
    }

    if (TRUE == pVideoEncData->mOutputPortTunnelFlag)
    {
        list_move_tail(&pEntry->mList, &pVideoEncData->mUsedOutFrameList);
        alogv("output coded frame ID[%d] list: Idle -> Used", pEntry->stEncodedStream.nID);
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);  // unlock before calling Empty ThisBuffer

        MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)(pVideoEncData->sOutPortTunnelInfo.hTunnel);
        COMP_BUFFERHEADERTYPE    obh;
        obh.nOutputPortIndex = pVideoEncData->sOutPortTunnelInfo.nPortIndex;
        obh.nInputPortIndex = pVideoEncData->sOutPortTunnelInfo.nTunnelPortIndex;
        obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
        alogv("pOutTunnelComp->Empty ThisBuffer for non-compress source");
        ERRORTYPE omxRet = pOutTunnelComp->EmptyThisBuffer(pOutTunnelComp, &obh);
        if (SUCCESS == omxRet)
        {
            //Notes: Must move buf node to UsedList before calling Empty ThisBuffer
        }
        else
        {
            alogd("Be careful! VEnc output frame fail[0x%x], return frame to venc!", omxRet);
            int vencLibRet = VencQueueOutputBuf(pVideoEncData->pCedarV, pOutputBuffer);
            if (vencLibRet != 0)
            {
                aloge("fatal error! Venc QueueOutputBuf fail, ret=%d", vencLibRet);
            }
            pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
            list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
            alogv("output coded frame ID[%d] list: Used -> Idle", pEntry->stEncodedStream.nID);
            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
        }
    }
    else
    {
        list_move_tail(&pEntry->mList, &pVideoEncData->mReadyOutFrameList);
        alogv("output coded frame ID[%d] list: Idle -> Ready", pEntry->stEncodedStream.nID);
        if (pVideoEncData->mWaitOutFrameFlag)
        {
            pthread_cond_signal(&pVideoEncData->mOutFrameCondition);
        }
        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
        char tmpWrCh = 'V';
        write(pVideoEncData->mOutputFrameNotifyPipeFds[1], &tmpWrCh, 1);
    }

    return 0;
}
*/

static void SendVencErrorTimeoutCallback(VIDEOENCDATATYPE *pVideoEncData, VideoFrameInfoNode *pFrameNode)
{
    if ((NULL == pVideoEncData) || (NULL == pFrameNode))
    {
        aloge("fatal error! invalid input params! %p,%p", pVideoEncData, pFrameNode);
        return;
    }

    pVideoEncData->mEncodeTimeoutCnt++;

    SIZE_S PicSize = {0, 0};
    getPicSizeFromVENC_ATTR_S(&pVideoEncData->mEncChnAttr.VeAttr, &PicSize);
    unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
    aloge("encode Error! vencChn[%d],vencType[%d],pixelFormat[0x%x],PicSrcDstSize[%dx%d->%dx%d],frameRate[%d->%d],bitRate[%d],timeLapse:[%d,%lf,%lf],frameId[%d],pts[%lld]us, cntr[%d/%d]",
        pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type,
        pFrameNode->VFrame.VFrame.mPixelFormat,
        pFrameNode->VFrame.VFrame.mWidth, pFrameNode->VFrame.VFrame.mHeight,
        PicSize.Width, PicSize.Height,
        pVideoEncData->mFrameRateInfo.SrcFrmRate, pVideoEncData->mFrameRateInfo.DstFrmRate,
        nBitRate,
        pVideoEncData->timeLapseEnable,
        pVideoEncData->mCapTimeLapse.capFrameIntervalUs, pVideoEncData->mCapTimeLapse.videoFrameIntervalUs,
        pFrameNode->VFrame.mId, pFrameNode->VFrame.VFrame.mpts,
        pVideoEncData->mEncodeTimeoutCnt, pVideoEncData->mDbgEncodeCnt);

    //send callback message to notify encode error
    pVideoEncData->pCallbacks->EventHandler(
                        pVideoEncData->hSelf,
                        pVideoEncData->pAppData,
                        COMP_EventError,
                        ERR_VENC_TIMEOUT,
                        0,
                        (void*)&pFrameNode->VFrame.VFrame.mpts);
}

static void SendVencVbvFullCallback(VIDEOENCDATATYPE *pVideoEncData)
{
    if (NULL == pVideoEncData)
    {
        aloge("fatal error! invalid input param");
        return;
    }

    if (0 == pVideoEncData->bitStreamBufFullCnt)
    {
        unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
        alogw("vencChn[%d] encode [%d]frames, fail BsFull, validFrames[%d], bitRate[%d]Mbit",
            pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mDbgEncodeCnt, VencGetValidOutputBufNum(pVideoEncData->pCedarV), nBitRate/(1000*1000));
    }

    if (0 == pVideoEncData->bitStreamBufFullCnt % 30)
    {
        //alogw("vbv full contiguous count:%d", pVideoEncData->bitStreamBufFullCnt);
        pVideoEncData->pCallbacks->EventHandler(
            pVideoEncData->hSelf,
            pVideoEncData->pAppData,
            COMP_EventRecVbvFull,
            0,
            0,
            NULL);
    }
    pVideoEncData->bitStreamBufFullCnt++;
}

/**
 *  Component Thread
 *    The Component Thread function is exeuted in a separate pThread and
 *    is used to implement the actual component functions.
 */
/*****************************************************************************/
static void* ComponentThread(void* pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    VIDEOENCDATATYPE* pVideoEncData = (VIDEOENCDATATYPE*) pThreadData;
    message_t cmd_msg;

    alogv("VideoEncoder ComponentThread start run...");
    sprintf(pVideoEncData->mThreadName, "VEncComp_%d", pVideoEncData->mMppChnInfo.mChnId);
    prctl(PR_SET_NAME, (unsigned long)pVideoEncData->mThreadName, 0, 0, 0);

    while (1)
    {
PROCESS_MESSAGE:
        if (get_message(&pVideoEncData->cmd_queue, &cmd_msg) == 0)
        {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;

            alogv("VideoEnc Component Thread get_message cmd:%d", cmd);

            // State transition command
            if (cmd == SetState)
            {
                // If the parameter states a transition to the same state
                //   raise a same state transition error.
                if (pVideoEncData->state == (COMP_STATETYPE) (cmddata))
                    pVideoEncData->pCallbacks->EventHandler(pVideoEncData->hSelf,
                            pVideoEncData->pAppData,
                            COMP_EventError,
                            ERR_VENC_SAMESTATE,
                            0,
                            NULL);
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE) (cmddata))
                    {
                        case COMP_StateInvalid:
                            pVideoEncData->state = COMP_StateInvalid;
                            pVideoEncData->pCallbacks->EventHandler(pVideoEncData->hSelf,
                                    pVideoEncData->pAppData,
                                    COMP_EventError,
                                    ERR_VENC_INVALIDSTATE,
                                    0,
                                    NULL);
                            pVideoEncData->pCallbacks->EventHandler(pVideoEncData->hSelf,
                                    pVideoEncData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pVideoEncData->state,
                                    NULL);
                            break;
                        case COMP_StateLoaded:
                        {
                            if (pVideoEncData->state != COMP_StateIdle)
                            {
                                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf,
                                        pVideoEncData->pAppData,
                                        COMP_EventError,
                                        ERR_VENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            alogv("VencChn[%d] StateLoaded begin", pVideoEncData->mMppChnInfo.mChnId);
                            //MM_COMPONENTTYPE *InputTunnelComp = (MM_COMPONENTTYPE *)(pVideoEncData->sInPortTunnelInfo.hTunnel);
                            VideoEncResetChannel(pVideoEncData->hSelf, FALSE);

                            pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                            pVideoEncData->mWaitOutFrameFullFlag = TRUE;
                            //wait all outFrame return.
                            int cnt;
                            struct list_head *pList;
                            while(1)
                            {
                                int cnt1 = 0;
                                list_for_each(pList, &pVideoEncData->mReadyOutFrameList)
                                {
                                    cnt1++;
                                }
                                int cnt2 = 0;
                                list_for_each(pList, &pVideoEncData->mUsedOutFrameList)
                                {
                                    cnt2++;
                                }
                                cnt = 0;
                                list_for_each(pList, &pVideoEncData->mIdleOutFrameList)
                                {
                                    cnt++;
                                }
                                if(cnt+cnt1<pVideoEncData->mFrameNodeNum)
                                {
                                    alogw("wait VencChn[%d] idleOutFrameList full:%d-%d-%d-%d", pVideoEncData->mMppChnInfo.mChnId, cnt,cnt1,cnt2,pVideoEncData->mFrameNodeNum);
                                    pthread_cond_wait(&pVideoEncData->mOutFrameFullCondition, &pVideoEncData->mOutFrameListMutex);
                                    alogw("wait VencChn[%d] idleOutFrameList full_done", pVideoEncData->mMppChnInfo.mChnId);
                                }
                                else
                                {
                                    break;
                                }
                            }

                            // to force release the frm in readyoutlist after the one in usedoutlist,the frm releasing order is
                            // important for the vbv buffer in venc library.
                            //added for fixing bug that one new encoded frm is added to readylist when out frm buffer
                            //is avaliable in case that decode process is suspended when vbv full is full.the new added
                            //frm registered in readylist will not be fetched out since the operation of app,such as the
                            // reset operation including the component destruction.
                            while((!list_empty(&pVideoEncData->mReadyOutFrameList)))
                            {
                                ENCODER_NODE_T *pEntry = list_first_entry(&pVideoEncData->mReadyOutFrameList, ENCODER_NODE_T, mList);
                                if (pVideoEncData->is_compress_source == 0)
                                {
                                    VencOutputBuffer stOutputBuffer;
                                    config_VencOutputBuffer_By_EncodedStream(&stOutputBuffer, &pEntry->stEncodedStream);
                                    VencQueueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                                    list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
                                    alogv("output coded frame ID[%d] list: Ready -> Idle", pEntry->stEncodedStream.nID);
                                    if(FALSE == pVideoEncData->mOutputPortTunnelFlag)
                                    {
                                        char tmpRdCh;
                                        read(pVideoEncData->mOutputFrameNotifyPipeFds[0], &tmpRdCh, 1);
                                    }
                                }
                            }

                            cnt = 0;
                            list_for_each(pList, &pVideoEncData->mIdleOutFrameList)
                            {
                                cnt++;
                            }
                            if(cnt<pVideoEncData->mFrameNodeNum)
                            {
                                aloge("fatal error! wait Venc idleOutFrameList full:%d-%d",cnt,pVideoEncData->mFrameNodeNum);
                            }

                            pVideoEncData->mWaitOutFrameFullFlag = FALSE;
                            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                            alogv("wait Venc idleOutFrameList full done");
                            pVideoEncData->state = COMP_StateLoaded;
                            pVideoEncData->pCallbacks->EventHandler(
                                    pVideoEncData->hSelf, pVideoEncData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pVideoEncData->state,
                                    NULL);
                            alogd("VencChn[%d] StateLoaded ok", pVideoEncData->mMppChnInfo.mChnId);
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if(pVideoEncData->state == COMP_StateLoaded)
                            {
                                //pVideoEncData->mMemOps = MemAdapterGetOpsS();
                                alogv("VencChn[%d]: loaded->idle ...", pVideoEncData->mMppChnInfo.mChnId);
                                //create vdeclib!
                                if(pVideoEncData->is_compress_source == 0)
                                {
                                    CedarvEncInit(pVideoEncData);
                                }
                                else
                                {
                                    alogd("VencChn[%d] component is in compress mode, create buffer manager.", pVideoEncData->mMppChnInfo.mChnId);
                                    if (pVideoEncData->buffer_manager == NULL)
                                    {
                                        pVideoEncData->buffer_manager = VideoEncBufferInit();
                                    }
                                }
                                pVideoEncData->state = COMP_StateIdle;
                                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf,
                                        pVideoEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pVideoEncData->state,
                                        NULL);
                            }
                            else if(pVideoEncData->state == COMP_StatePause || pVideoEncData->state == COMP_StateExecuting)
                            {
                                alogd("VencChn[%d]: pause/executing[0x%x]->idle ...", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->state);
                                #ifdef ENABLE_ENCODE_STATISTICS
                                    VencEncodeTimeS stEncodeStatistics;
                                    memset(&stEncodeStatistics, 0, sizeof(VencEncodeTimeS));
                                    if(pVideoEncData->pCedarV)
                                    {
                                        VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamGetEncodeTime, (void*)&stEncodeStatistics);
                                        alogd("VencChn[%d] statistics: avrIdleTime[%d]us, avrEncTime[%d]us, maxIdleTime[%d]us, maxEncTime[%d]us; totalFrame:[%d]",
                                            pVideoEncData->mMppChnInfo.mChnId, stEncodeStatistics.avr_empty_time, stEncodeStatistics.avr_enc_time,
                                            stEncodeStatistics.max_empty_time, stEncodeStatistics.max_enc_time, stEncodeStatistics.frame_num);
                                    }
                                #endif
                                if (0 == pVideoEncData->mOnlineEnableFlag)
                                {
                                    ReturnUseOrigFrameBeforeSetStateIdle(pVideoEncData);
                                }
                                else
                                {
                                    int venclib_ret = VencPause(pVideoEncData->pCedarV);
                                    if (VENC_RESULT_OK != venclib_ret)
                                    {
                                        aloge("fatal error! Venc Pause fail, ret=%d", venclib_ret);
                                    }
                                    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                                    if (!list_empty(&pVideoEncData->mBufQ.mUsedFrameList))
                                    {
                                        VideoFrameInfoNode *pEntry, *pTmp;
                                        list_for_each_entry_safe(pEntry, pTmp, &pVideoEncData->mBufQ.mUsedFrameList, mList)
                                        {
                                            list_move_tail(&pEntry->mList, &pVideoEncData->mBufQ.mIdleFrameList);
                                            pVideoEncData->mBufQ.buf_unused++;
                                        }
                                    }
                                    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                                }
                                pVideoEncData->state = COMP_StateIdle;
                                VideoEncResetChannel(pVideoEncData->hSelf, FALSE);
                                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf,
                                        pVideoEncData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pVideoEncData->state,
                                        NULL);
                            }
                            else
                            {
                                aloge("fatal error! current state[0x%x] can't turn to idle!", pVideoEncData->state);
                                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf,
                                        pVideoEncData->pAppData,
                                        COMP_EventError,
                                        ERR_VENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StateExecuting:
                            // Transition can only happen from pause or idle state
                            if (pVideoEncData->state == COMP_StateIdle || pVideoEncData->state == COMP_StatePause)
                            {
                                int eError = SUCCESS;
                                if( COMP_StateIdle == pVideoEncData->state)
                                {
                                    if(NULL == pVideoEncData->pCedarV)
                                    {
                                        if(pVideoEncData->is_compress_source == 0)
                                        {
                                            CedarvEncInit(pVideoEncData);
                                        }
                                    } 
                                    pthread_mutex_lock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
                                    if(pVideoEncData->pCedarV && !pVideoEncData->mbCedarvVideoEncInitFlag)
                                    {
                                        eError = CedarvVideoEncInit(pVideoEncData);
                                        if(SUCCESS == eError)
                                        {
                                            pVideoEncData->mbCedarvVideoEncInitFlag = TRUE;
                                        }
                                        else
                                        {
                                            aloge("fatal error! vdeclib init fail[0x%x]", eError);
                                        }
                                    }
                                    pthread_mutex_unlock(&pVideoEncData->mCedarvVideoEncInitFlagLock);
                                }
                                else if (COMP_StatePause == pVideoEncData->state)
                                {
                                    int venclib_ret = VencStart(pVideoEncData->pCedarV);
                                    if (VENC_RESULT_OK != venclib_ret)
                                    {
                                        aloge("fatal error! Venc Start fail, ret=%d", venclib_ret);
                                        eError = FAILURE;
                                    }
                                }

                                if(SUCCESS == eError)
                                {
                                    alogd("VencChn[%d]: idle/pause[0x%x]->executing ...", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->state);
                                    pVideoEncData->state = COMP_StateExecuting;
                                    pVideoEncData->pCallbacks->EventHandler(
                                            pVideoEncData->hSelf,
                                            pVideoEncData->pAppData,
                                            COMP_EventCmdComplete,
                                            COMP_CommandStateSet,
                                            pVideoEncData->state,
                                            NULL);
                                }
                                else
                                {
                                    pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf,
                                        pVideoEncData->pAppData,
                                        COMP_EventError,
                                        ERR_VENC_INCORRECT_STATE_TRANSITION,
                                        eError,
                                        NULL);
                                }
                            }
                            else
                            {
                                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf,
                                        pVideoEncData->pAppData,
                                        COMP_EventError,
                                        ERR_VENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        case COMP_StatePause:
                            // Transition can only happen from idle or executing state
                            if (pVideoEncData->state == COMP_StateIdle || pVideoEncData->state == COMP_StateExecuting)
                            {
                                int venclib_ret = VencPause(pVideoEncData->pCedarV);
                                if (VENC_RESULT_OK == venclib_ret)
                                {
                                    alogd("VencChn[%d] Pause success", pVideoEncData->mMppChnInfo.mChnId);
                                    pVideoEncData->state = COMP_StatePause;
                                    pVideoEncData->pCallbacks->EventHandler(
                                            pVideoEncData->hSelf,
                                            pVideoEncData->pAppData,
                                            COMP_EventCmdComplete,
                                            COMP_CommandStateSet,
                                            pVideoEncData->state,
                                            NULL);
                                }
                                else
                                {
                                    aloge("fatal error! Venc Pause fail, ret=%d", venclib_ret);
                                    pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf,
                                        pVideoEncData->pAppData,
                                        COMP_EventError,
                                        ERR_VENC_INCORRECT_STATE_TRANSITION,
                                        venclib_ret,
                                        NULL);
                                }
                            }
                            else
                            {
                                pVideoEncData->pCallbacks->EventHandler(
                                        pVideoEncData->hSelf, pVideoEncData->pAppData,
                                        COMP_EventError,
                                        ERR_VENC_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
            else if (cmd == Flush)
            {
            }
            else if (cmd == Stop)
            {
                // Kill thread
                goto EXIT;
            }
            else if(cmd == VEncComp_InputFrameAvailable)
            {
            }
            else if(cmd == VEncComp_OutputFrameReturn)
            {
            }
            else if(cmd == VEncComp_OutputStreamAvailable)
            {
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (pVideoEncData->state == COMP_StateExecuting)
        {
            /* For Venc online mode and online channel.
             * MPP Venc component do not need to participate in the management of the frame buffer.
             * The dirver layer implements the logical management of the frame buffer.
             * MPP Venc component only need to start from ve_driver obtains the encoded stream data.
             */
            if (pVideoEncData->mOnlineEnableFlag)
            {
                // Request all output streams from the encoding library at once.
                int reqCodedFrameCnt = 0;
                pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                while (1)
                {
                    VencOutputBuffer stOutputBuffer;
                    int vencLibRet = VencDequeueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                    alogv("Venc DequeueOutputBuf ret=%d", vencLibRet);
                    if (0 != vencLibRet)
                    {
                        break;
                    }
                    reqCodedFrameCnt++;
                    int ret = UpdateReadyOutFrameList_l(pVideoEncData, &stOutputBuffer);
                    if (0 != ret)
                    {
                        aloge("fatal error! update ReadyOutFrameList fail? check code!");
                        break;
                    }
                }
                pVideoEncData->mFlagOutputStream = 0;
                pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);

                //process ReadyOutFrameList. if tunnel mode, send frame to next component; if non-tunnel mode, signal if necessary.
                ProcessReadyOutFrameList(pVideoEncData);

                if (0 == reqCodedFrameCnt)
                {
                    alogv("No encoded stream requested, wait for message");
                    TMessage_WaitQueueNotEmpty(&pVideoEncData->cmd_queue, 0);
                }
                else
                {
                    alogv("req CodedFrameCnt = %d", reqCodedFrameCnt);
                }

                // for online, processing venc result only, no need to return input orig frame
                pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                if (!list_empty(&pVideoEncData->mBufQ.mUsedFrameList))
                {
                    VideoFrameInfoNode *pUsedFrameNode = NULL;
                    VideoFrameInfoNode *pTmp = NULL;
                    list_for_each_entry_safe(pUsedFrameNode, pTmp, &pVideoEncData->mBufQ.mUsedFrameList, mList)
                    {
                        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                        int result = pUsedFrameNode->mResult;
                        if (VENC_RESULT_ERROR == result)
                        {
                            SendVencErrorTimeoutCallback(pVideoEncData, pUsedFrameNode);
                        }
                        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                        list_move_tail(&pUsedFrameNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
                        pVideoEncData->mBufQ.buf_unused++;
                        alogv("release input orig frame ID[%d], list: Used -> Idle, buf unused[%d]", pUsedFrameNode->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
                        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                        // let's process encode result.
                        if (VENC_RESULT_OK != result)
                        {
                            if (VENC_RESULT_BITSTREAM_IS_FULL == result)
                            {
                                pVideoEncData->mDiscardFrameCnt++;
                                SendVencVbvFullCallback(pVideoEncData);
                                if (FALSE == pVideoEncData->mOutputPortTunnelFlag)
                                {
                                    pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                                    if (pVideoEncData->mWaitOutFrameFlag)
                                    {
                                        pthread_cond_signal(&pVideoEncData->mOutFrameCondition);
                                    }
                                    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                                }
                            }
                            else if (VENC_RESULT_DROP_FRAME == result)
                            {
                                alogv("chn[%d] encode result=%d(drop_frm)", pVideoEncData->mMppChnInfo.mChnId, result);
                            }
                            else if (VENC_RESULT_CONTINUE == result)
                            {
                                alogv("chn[%d] encode result=%d(continue)", pVideoEncData->mMppChnInfo.mChnId, result);
                            }
                            else
                            {
                                alogw("chn[%d] encode result=%d", pVideoEncData->mMppChnInfo.mChnId, result);
                            }
                        }
                        else
                        {
                            if (pVideoEncData->bitStreamBufFullCnt != 0)
                            {
                                unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
                                alogw("vencChn[%d] fail BsFull[%d] contiguous count:%d, send vbvFull msg [%d]times, validFrames[%d], bitRate[%d]Mbit", 
                                    pVideoEncData->mMppChnInfo.mChnId, result, pVideoEncData->bitStreamBufFullCnt, pVideoEncData->bitStreamBufFullCnt/30+1,
                                    VencGetValidOutputBufNum(pVideoEncData->pCedarV), nBitRate/(1000*1000));
                                pVideoEncData->bitStreamBufFullCnt = 0;
                            }
                        }
                        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                    }
                }
                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

                goto PROCESS_MESSAGE;
            }

            // For Venc offline mode or online mode but offline channel.
            if (pVideoEncData->is_compress_source == 0)
            {
                // for debug
                if (pVideoEncData->mbDebug_IgnoreAllFrame)
                {
                    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                    if (list_empty(&pVideoEncData->mBufQ.mReadyFrameList))
                    {
                        alogv("input orig frame list: Ready is empty");
                        pVideoEncData->mFlagInputFrame = 0;
                        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                        TMessage_WaitQueueNotEmpty(&pVideoEncData->cmd_queue, 0);
                    }
                    else
                    {
                        VideoFrameInfoNode *pFrameNodeDebug = list_first_entry(&pVideoEncData->mBufQ.mReadyFrameList, VideoFrameInfoNode, mList);
                        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                        VideoEncSendBackInputFrame(pVideoEncData, &pFrameNodeDebug->VFrame);
                        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                        list_move_tail(&pFrameNodeDebug->mList, &pVideoEncData->mBufQ.mIdleFrameList);
                        pVideoEncData->mBufQ.buf_unused++;
                        alogv("[Debug IgnoreAllFrame] input orig frame ID[%d] list: Ready -> Idle, buf unused[%d]", pFrameNodeDebug->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
                        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                    }
                    goto PROCESS_MESSAGE;
                }

                if (FALSE == pVideoEncData->mbForbidDiscardingFrame) //allow to drop frame, e.g., h264,h265,mjpeg
                {
                    // Send all ready orig frames to venc lib at once.
                    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                    if (!list_empty(&pVideoEncData->mBufQ.mReadyFrameList))
                    {
                        int num = 0;
                        VideoFrameInfoNode *pReadyFrameNode = NULL;
                        VideoFrameInfoNode *pTmp = NULL;
                        list_for_each_entry_safe(pReadyFrameNode, pTmp, &pVideoEncData->mBufQ.mReadyFrameList, mList)
                        {
                            int ret = SendOrigFrameToVencLib(pVideoEncData, pReadyFrameNode);
                            if (1 == ret)
                            {
                                // The original frame has been returned, continue to send the next frame, if any.
                                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                                VideoEncSendBackInputFrame(pVideoEncData, &pReadyFrameNode->VFrame);
                                pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                                list_move_tail(&pReadyFrameNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
                                pVideoEncData->mBufQ.buf_unused++;
                                alogv("VencChn[%d] release input orig frame ID[%d], list: Ready -> Idle, buf unused[%d]", pVideoEncData->mMppChnInfo.mChnId, pReadyFrameNode->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
                                continue;
                            }
                            else if (0 != ret)
                            {
                                aloge("fatal error! Send OrigFrameToVencLib fail, ret=%d", ret);
                                break;
                            }
                            else
                            {
                                list_move_tail(&pReadyFrameNode->mList, &pVideoEncData->mBufQ.mUsingFrameList);
                                alogv("input orig frame ID[%d] list: Ready -> Using", pReadyFrameNode->VFrame.mId);
                            }
                            num++;
                        }
                        pVideoEncData->mFlagInputFrame = 0;
                        alogv("vencChn[%d] send [%d]frames to the encoding library at once ", pVideoEncData->mMppChnInfo.mChnId, num);
                    }
                    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

                    // Request all output streams from the encoding library at once.
                    int reqCodedFrameCnt = 0;
                    pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                    while (1)
                    {
                        VencOutputBuffer stOutputBuffer;
                        int vencLibRet = VencDequeueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                        alogv("vencChn[%d] DequeueOutputBuf ret=%d", pVideoEncData->mMppChnInfo.mChnId, vencLibRet);
                        if (0 != vencLibRet)
                        {
                            break;
                        }
//                        int streamSize = stOutputBuffer.nSize0+stOutputBuffer.nSize1+stOutputBuffer.nSize2;
//                        if(streamSize >= 100*1024)
//                        {
//                            alogd("vencChn[%d] encodeCnt[%d]: streamSize[%d]KB>=100KB", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mDbgEncodeCnt, streamSize/1024);
//                        }
                        reqCodedFrameCnt++;
                        int ret = UpdateReadyOutFrameList_l(pVideoEncData, &stOutputBuffer);
                        if (0 != ret)
                        {
                            aloge("fatal error! update ReadyOutFrameList fail? check code!");
                            break;
                        }
                    }
                    pVideoEncData->mFlagOutputStream = 0;
                    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);

                    //process ReadyOutFrameList. if tunnel mode, send frame to next component; if non-tunnel mode, signal if necessary.
                    ProcessReadyOutFrameList(pVideoEncData);

                    if (0 == reqCodedFrameCnt)
                    {
                        alogv("vencChn[%d] No encoded stream requested, wait for message", pVideoEncData->mMppChnInfo.mChnId);
                        TMessage_WaitQueueNotEmpty(&pVideoEncData->cmd_queue, 0);
                    }
                    else
                    {
                        alogv("vencChn[%d] req CodedFrameCnt = %d", pVideoEncData->mMppChnInfo.mChnId, reqCodedFrameCnt);
                    }

                    // return input orig used frames
                    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                    if (!list_empty(&pVideoEncData->mBufQ.mUsedFrameList))
                    {
                        alogv("return input orig frames");
                        VideoFrameInfoNode *pUsedFrameNode = NULL;
                        VideoFrameInfoNode *pTmp = NULL;
                        list_for_each_entry_safe(pUsedFrameNode, pTmp, &pVideoEncData->mBufQ.mUsedFrameList, mList)
                        {
                            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                            int result = pUsedFrameNode->mResult;
                            if (VENC_RESULT_ERROR == result)
                            {
                                SendVencErrorTimeoutCallback(pVideoEncData, pUsedFrameNode);
                            }
                            // return input orig frame
                            VideoEncSendBackInputFrame(pVideoEncData, &pUsedFrameNode->VFrame);
                            pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                            list_move_tail(&pUsedFrameNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
                            pVideoEncData->mBufQ.buf_unused++;
                            alogv("vencChn[%d] release input orig frame ID[%d], list: Used -> Idle, buf unused[%d]", pVideoEncData->mMppChnInfo.mChnId, pUsedFrameNode->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
                            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                            // let's process encode result.
                            if (VENC_RESULT_OK != result)
                            {
                                if (VENC_RESULT_BITSTREAM_IS_FULL == result)
                                {
                                    pVideoEncData->mDiscardFrameCnt++;
                                    SendVencVbvFullCallback(pVideoEncData);
                                    if (FALSE == pVideoEncData->mOutputPortTunnelFlag)
                                    {
                                        pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                                        if (pVideoEncData->mWaitOutFrameFlag)
                                        {
                                            pthread_cond_signal(&pVideoEncData->mOutFrameCondition);
                                        }
                                        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                                    }
                                }
                                else if (VENC_RESULT_DROP_FRAME == result)
                                {
                                    alogw("chn[%d] encode result=%d(drop_frm)", pVideoEncData->mMppChnInfo.mChnId, result);
                                }
                                else
                                {
                                    alogw("chn[%d] encode result=%d", pVideoEncData->mMppChnInfo.mChnId, result);
                                }
                            }
                            else
                            {
                                if (pVideoEncData->bitStreamBufFullCnt != 0)
                                {
                                    unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&pVideoEncData->mEncChnAttr);
                                    alogw("vencChn[%d] encode [%d]frames, fail BsFull[%d] contiguous count:%d, send vbvFull msg [%d]times, validFrames[%d], bitRate[%d]Mbit", 
                                        pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mDbgEncodeCnt, result, pVideoEncData->bitStreamBufFullCnt, pVideoEncData->bitStreamBufFullCnt/30+1,
                                        VencGetValidOutputBufNum(pVideoEncData->pCedarV), nBitRate/(1000*1000));
                                    pVideoEncData->bitStreamBufFullCnt = 0;
                                }
                            }
                            pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                        }
                    }
                    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                }
                else //forbid to drop frame, e.g., jpeg
                {
                    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                    // Forbid Discarding Frame flow, the execution will not continue when there is no input frame.
                    if ((list_empty(&pVideoEncData->mBufQ.mReadyFrameList)) &&
                        (list_empty(&pVideoEncData->mBufQ.mUsingFrameList)) &&
                        (list_empty(&pVideoEncData->mBufQ.mUsedFrameList)))
                    {
                        pVideoEncData->mFlagInputFrame = 0;
                        pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                        TMessage_WaitQueueNotEmpty(&pVideoEncData->cmd_queue, 0);
                        alogv("input orig frame list: Ready & Using & Used are all empty");
                        goto PROCESS_MESSAGE;
                    }
                    VideoFrameInfoNode *pReadyFrameNode = list_first_entry(&pVideoEncData->mBufQ.mReadyFrameList, VideoFrameInfoNode, mList);

                    if (0 == pVideoEncData->mFlagDoNotSendOrigFrameToVencLib)
                    {
                        alogv("send only one orig ready frame to venc lib at once.");
                        int ret = SendOrigFrameToVencLib(pVideoEncData, pReadyFrameNode);
                        if (1 == ret)
                        {
                            // The original input frame has been returned and no further processing is required.
                            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                            VideoEncSendBackInputFrame(pVideoEncData, &pReadyFrameNode->VFrame);
                            pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                            list_move_tail(&pReadyFrameNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
                            pVideoEncData->mBufQ.buf_unused++;
                            alogv("release input orig frame ID[%d], list: Ready -> Idle, buf unused[%d]", pReadyFrameNode->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
                            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                            goto PROCESS_MESSAGE;
                        }
                        if (0 != ret)
                        {
                            aloge("fatal error! Send OrigFrameToVencLib fail, ret=%d", ret);
                        }
                        else
                        {
                            pVideoEncData->mFlagInputFrame = 0;
                            list_move_tail(&pReadyFrameNode->mList, &pVideoEncData->mBufQ.mUsingFrameList);
                            alogv("input orig frame ID[%d] list: Ready -> Using", pReadyFrameNode->VFrame.mId);
                        }
                    }
                    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

                    // wait for Venc CompInputBufferDone callback.
                    int semRet = cdx_sem_down_timedwait(&pVideoEncData->mInputFrameBufDoneSem, 200); // unit: ms
                    if (ETIMEDOUT == semRet)
                    {
                        alogw("wait inputFrameBufDoneSem 200ms timeout, goto PROCESS_MESSAGE, but DO NOT send orig frame to venc lib.");
                        pVideoEncData->mFlagDoNotSendOrigFrameToVencLib = 1;
                    }
                    else if (0 == semRet)
                    {
                        // process encode result.
                        pVideoEncData->mFlagDoNotSendOrigFrameToVencLib = 0;
                        pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                        if (!list_empty(&pVideoEncData->mBufQ.mUsedFrameList))
                        {
                            VideoFrameInfoNode *pUsedFrameNode = list_first_entry(&pVideoEncData->mBufQ.mUsedFrameList, VideoFrameInfoNode, mList);
                            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);

                            int result = pUsedFrameNode->mResult;
                            alogv("venc frame ID[%d] result=%d", pUsedFrameNode->VFrame.mId, result);

                            if (VENC_RESULT_OK == result)
                            {
                                // Get coded frame from venc lib.
                                VencOutputBuffer stOutputBuffer;
                                int ret = VencDequeueOutputBuf(pVideoEncData->pCedarV, &stOutputBuffer);
                                if (0 != ret)
                                {
                                    aloge("fatal error! Venc DequeueOutputBuf fail when result=OK, ret=%d", ret);
                                }
                                pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                                ret = UpdateReadyOutFrameList_l(pVideoEncData, &stOutputBuffer);
                                if (0 != ret)
                                {
                                    aloge("fatal error! update ReadyOutFrameList fail? check code!");
                                }
                                pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                                //process ReadyOutFrameList. if tunnel mode, send frame to next component; if non-tunnel mode, signal if necessary.
                                //when forbid DiscardingFrame, only non-tunnel is available.
                                ProcessReadyOutFrameList(pVideoEncData);
                                
                                // return input orig frame
                                VideoEncSendBackInputFrame(pVideoEncData, &pUsedFrameNode->VFrame);
                                pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                                list_move_tail(&pUsedFrameNode->mList, &pVideoEncData->mBufQ.mIdleFrameList);
                                pVideoEncData->mBufQ.buf_unused++;
                                alogv("release input orig frame ID[%d], list: Used -> Idle, buf unused[%d]", pUsedFrameNode->VFrame.mId, pVideoEncData->mBufQ.buf_unused);
                                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                            }
                            else if (VENC_RESULT_BITSTREAM_IS_FULL == result)
                            {
                                pVideoEncData->mDiscardFrameCnt++;
                                SendVencVbvFullCallback(pVideoEncData);
                                // wait for 200ms, if no message, will do coded again.
                                pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                                pVideoEncData->mWaitOutFrameReturnFlag = TRUE;
                                pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                                alogw("result=VBV FULL, wait for cmd queue message 200ms.");
                                if (TMessage_WaitQueueNotEmpty(&pVideoEncData->cmd_queue, 200) <= 0) // unit: ms
                                {
                                    alogw("result=%d, will do venc again.", result);
                                    pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                                    list_move(&pUsedFrameNode->mList, &pVideoEncData->mBufQ.mReadyFrameList);
                                    alogv("re-encode input orig frame ID[%d], list: Used -> Ready", pUsedFrameNode->VFrame.mId);
                                    pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                                }
                            }
                            else
                            {
                                if (VENC_RESULT_ERROR == result)
                                {
                                    SendVencErrorTimeoutCallback(pVideoEncData, pUsedFrameNode);
                                }
                                // will do coded again.
                                pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                                pVideoEncData->mWaitOutFrameReturnFlag = TRUE;
                                pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                                alogw("result=%d, will do venc again.", result);
                                pthread_mutex_lock(&pVideoEncData->mutex_fifo_ops_lock);
                                list_move(&pUsedFrameNode->mList, &pVideoEncData->mBufQ.mReadyFrameList);
                                alogv("re-encode input orig frame ID[%d], list: Used -> Ready", pUsedFrameNode->VFrame.mId);
                                pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                            }
                            // clear FlagStream, It's just a process step, not useful for the process of Forbid Discarding Frame.
                            pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                            pVideoEncData->mFlagOutputStream = 0;
                            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                        }
                        else
                        {
                            aloge("fatal error! Low probability, The input orig frame Used list is empty !");
                            pthread_mutex_unlock(&pVideoEncData->mutex_fifo_ops_lock);
                        }
                    }
                    else
                    {
                        aloge("fatal error! cdx sem_down_timedwait fail, ret=%d", semRet);
                        pVideoEncData->mFlagDoNotSendOrigFrameToVencLib = 0;
                    }
                }
            }
            else
            {  /* gushiming compressed source */
                ERRORTYPE   eError;
                ERRORTYPE   releaseOmxRet;
                FRAMEDATATYPE   frame;
                pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                if(list_empty(&pVideoEncData->mIdleOutFrameList))
                {
                    aloge("fatal error! why idleOutFrameList is empty when in compressed source? wait 200ms");
                    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                    usleep(200*1000);
                    continue;
                }
                ENCODER_NODE_T *pEntry = list_first_entry(&pVideoEncData->mIdleOutFrameList, ENCODER_NODE_T, mList);
                memset(&pEntry->stEncodedStream, 0 , sizeof(EncodedStream));
                eError = VideoEncBufferGetFrame(pVideoEncData->buffer_manager, &frame);
                if (eError == SUCCESS)
                {
                    pEntry->stEncodedStream.nFilledLen = (unsigned int)frame.info.size;
                    pEntry->stEncodedStream.pBuffer = (unsigned char*)frame.addrY;
                    pEntry->stEncodedStream.nBufferLen = (unsigned int)frame.info.size;
                    pEntry->stEncodedStream.pBufferExtra = NULL;
                    pEntry->stEncodedStream.nBufferExtraLen = 0;
                    if(VideoEncAnalyseCompressedFrame(pVideoEncData, &frame))
                    {
                        pEntry->stEncodedStream.nFlags |= CEDARV_FLAG_KEYFRAME;
                    }
                    pEntry->stEncodedStream.nTimeStamp = frame.info.timeStamp/* - pVideoEncData->csi_base_time*/;
                    pEntry->stEncodedStream.nID = frame.info.bufferId;
                    alogv("rd buf_id: %d, nTimeStamp: %lld", pEntry->stEncodedStream.nID, pEntry->stEncodedStream.nTimeStamp);

                    if (pVideoEncData->mOutputPortTunnelFlag)
                    {
                        list_move_tail(&pEntry->mList, &pVideoEncData->mUsedOutFrameList);
                        alogv("output coded frame ID[%d] list: Idle -> Used", pEntry->stEncodedStream.nID);
                        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);  //Notes: unlock before calling Empty ThisBuffer
                        if(!pVideoEncData->mbSendSpspps)
                        {
                            if(pVideoEncData->mpVencHeaderData)
                            {
                                if(pVideoEncData->mpVencHeaderData->pBuffer != NULL && pVideoEncData->mpVencHeaderData->nLength > 0)
                                {
                                    ERRORTYPE ret = COMP_SetConfig(pVideoEncData->sOutPortTunnelInfo.hTunnel, COMP_IndexVendorExtraData, pVideoEncData->mpVencHeaderData);
                                    if(SUCCESS == ret)
                                    {
                                        pVideoEncData->mbSendSpspps = TRUE;
                                    }
                                    else
                                    {
                                        aloge("fatal error! why set spspps fail[0x%x]?", ret);
                                    }
                                }
                            }
                        }
                        MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)(pVideoEncData->sOutPortTunnelInfo.hTunnel);
                        COMP_BUFFERHEADERTYPE obh;
                        obh.nOutputPortIndex = pVideoEncData->sOutPortTunnelInfo.nPortIndex;
                        obh.nInputPortIndex = pVideoEncData->sOutPortTunnelInfo.nTunnelPortIndex;
                        obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
                        alogd("pOutTunnelComp->Empty ThisBuffer for compress source");
                        ERRORTYPE omxRet = pOutTunnelComp->EmptyThisBuffer(pOutTunnelComp, &obh);
                        if (SUCCESS == omxRet)
                        {
                            // Must move to UsedList before calling Empty ThisBuffer
                        }
                        else
                        {
                            aloge("fatal error! VEnc output frame fail, return frame to buffer_manager!");
                            releaseOmxRet = VideoEncBufferReleaseFrame(pVideoEncData->buffer_manager, &frame);
                            if (releaseOmxRet != SUCCESS)
                            {
                                aloge("fatal error! videoEncBufferReleaseFrame fail[%d]", releaseOmxRet);
                            }
                            pthread_mutex_lock(&pVideoEncData->mOutFrameListMutex);
                            list_move_tail(&pEntry->mList, &pVideoEncData->mIdleOutFrameList);
                            alogv("output coded frame ID[%d] list: Used -> Idle", pEntry->stEncodedStream.nID);
                            pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                        }
                    }
                    else
                    {
                        list_add_tail(&pEntry->mList, &pVideoEncData->mReadyOutFrameList);
                        if (pVideoEncData->mWaitOutFrameFlag)
                        {
                            pthread_cond_signal(&pVideoEncData->mOutFrameCondition);
                        }
                        pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                    }
                    continue;
                }
                else
                {
                    pVideoEncData->mFlagInputFrame = 0;
                    pthread_mutex_unlock(&pVideoEncData->mOutFrameListMutex);
                    TMessage_WaitQueueNotEmpty(&pVideoEncData->cmd_queue, 0);
                    continue;
                }
            }
        }
        else
        {
            alogv("Encoder Component Thread not OMX_StateExecuting\n");
            TMessage_WaitQueueNotEmpty(&pVideoEncData->cmd_queue, 0);
        }
    }
EXIT:
    alogv("VideoEncoder Component Thread stopped");
    return (void*) SUCCESS;
}

