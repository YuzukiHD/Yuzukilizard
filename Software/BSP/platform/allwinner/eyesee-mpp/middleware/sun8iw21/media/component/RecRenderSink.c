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

/*-------------------------------------------------------------------------
                        abbreviation description

    avsync_rc_swfv: rc: recRenderSink
                    swf: switch file
                    v: video duration is enough, prepare to switch file.
    avsync_rc_swfa: audio duration is enough, prepare to switch file
    avsync_ch_v: ch: cache.
                 v: video
    avsync_cal: cal: calculate. calculate number of video frames which are written more.
    mVideoPtsWriteMoreSt: St: start.
    avsync_bk1: bk: break.
-------------------------------------------------------------------------*/

//#define LOG_NDEBUG 0
#define LOG_TAG "RecSink"
#include <utils/plat_log.h>

//ref platform headers
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <math.h>

#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "mm_common.h"
#include "mm_comm_video.h"
#include "mm_comm_venc.h"
#include <mm_comm_mux.h>

//media internal common headers.
#include "media_common.h"
#include "mm_component.h"
#include "ComponentCommon.h"
#include <SystemBase.h>
#include "tmessage.h"
#include "tsemaphore.h"
#include <aenc_sw_lib.h>

#include "RecRenderSink.h"
#include <mp4_mux_lib.h>
#include <sa_config.h>
#include <dup2SeldomUsedFd.h>

//#include <ConfigOption.h>

#define NOTIFY_NEEDNEXTFD_IN_ADVANCE (10*1000)  //ms

extern ERRORTYPE RecSinkMuxerInit(RecSink *pSinkInfo, RecSinkPacket *pRSPacket);

static ERRORTYPE RecSinkIncreaseIdleRSPacketList(RecSink* pThiz)
{
    ERRORTYPE   eError = SUCCESS;
    RecSinkPacket   *pRSPacket;
    int i;
    for(i=0;i<RECSINK_MAX_PACKET_NUM;i++)
    {
        pRSPacket = (RecSinkPacket*)malloc(sizeof(RecSinkPacket));
        if(NULL == pRSPacket)
        {
            aloge("fatal error! malloc fail");
            eError = ERR_MUX_NOMEM;
            break;
        }
        list_add_tail(&pRSPacket->mList, &pThiz->mIdleRSPacketList);
    }
    return eError;
}

static ERRORTYPE RSSetRecSinkPacket(PARAM_IN RecSinkPacket *pDes, PARAM_IN RecSinkPacket *pSrc)
{
    pDes->mId           = pSrc->mId;
    pDes->mStreamType   = pSrc->mStreamType;
    pDes->mFlags        = pSrc->mFlags;
    pDes->mPts          = pSrc->mPts;
    pDes->mpData0       = pSrc->mpData0;
    pDes->mSize0        = pSrc->mSize0;
    pDes->mpData1       = pSrc->mpData1;
    pDes->mSize1        = pSrc->mSize1;
    pDes->mCurrQp       = pSrc->mCurrQp;
    pDes->mavQp         = pSrc->mavQp;
	pDes->mnGopIndex    = pSrc->mnGopIndex;
	pDes->mnFrameIndex  = pSrc->mnFrameIndex;
	pDes->mnTotalIndex  = pSrc->mnTotalIndex;
    pDes->mSourceType   = pSrc->mSourceType;
    pDes->mRefCnt       = 0;

    pDes->mStreamId = pSrc->mStreamId;
    return SUCCESS;
}

void RecSinkResetSomeMembers(RecSink *pThiz)
{
    pThiz->mMuxerId = -1;
    pThiz->nMuxerMode = MUXER_MODE_MP4;
    //pThiz->nOutputFd = -1;
    pThiz->nFallocateLen = 0;
    pThiz->nCallbackOutFlag = FALSE;
    //pThiz->pOutputFile = NULL;
    pThiz->pWriter = NULL;
    pThiz->pMuxerCtx = NULL;
    pThiz->mbMuxerInit = FALSE;
    int j;
    for(j=0;j<MAX_TRACK_COUNT;j++)
    {
        pThiz->mbTrackInit[j] = FALSE;
        pThiz->mPrevPts[j] = -1;
        pThiz->mBasePts[j] = -1;
        pThiz->mInputPrevPts[j] = -1;
        pThiz->mOrigBasePts[j] = -1;
    }
    pThiz->mRefVideoStreamIndex = 0;
    pThiz->mValidStreamCount = -1;
    for(j=0;j<MAX_VIDEO_TRACK_COUNT;j++)
    {
        pThiz->mDuration[j] = 0;
        pThiz->mPrevDuration[j] = 0;
        pThiz->mPrefetchFlag[j] = FALSE;
        pThiz->need_to_force_i_frm[j] = 0;
        pThiz->forced_i_frm[j] = 0;
        pThiz->key_frm_sent[j] = 0;
        pThiz->v_frm_drp_cnt[j] = 0;
    }
    //pThiz->mDuration = 0;
    pThiz->mDurationAudio = 0;
    pThiz->mDurationText = 0;
    pThiz->mLoopDuration = 0;
    pThiz->mLoopDurationAudio = 0;
    //pThiz->mPrevDuration = 0;
    pThiz->mPrevDurationAudio = 0;
    pThiz->mPrevDurationText = 0;
    pThiz->mCurFileEndTm = 0;
    pThiz->mFileSizeBytes = 0;
    pThiz->mVideoFrameCounter = 0;
    pThiz->mAudioFrmCnt = 0;
    //pThiz->nSwitchFd = -1;
    pThiz->nSwitchFdFallocateSize = 0;
    //pThiz->mSwitchFdImpactFlag = 0;
    pThiz->reset_fd_flag = FALSE;
    pThiz->need_set_next_fd = FALSE;
    pThiz->rec_file = FILE_NORMAL;
    pThiz->mRecordMode = RECORDER_MODE_NONE;
    pThiz->mFileDurationPolicy = RecordFileDurationPolicy_MinDuration;
    pThiz->mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pThiz->mFsSimpleCacheSize = 0;
    //pThiz->mStatus = COMP_StateLoaded;
    //pThiz->mImpactStartTm = -1;
    //pThiz->mImpactAgainTm = -1;
    //pThiz->mImpactPrevTm = -1;
    pThiz->mCurMaxFileDuration = -1;
    pThiz->mMaxFileDuration = -1;
    //pThiz->mImpactFileDuration = -1;
    pThiz->mNoInputPacketFlag = 0;
    //pThiz->mpMediaInf = NULL;
    //pThiz->mpVencExtraData = NULL;
    pThiz->mpCallbackWriter = NULL;
    pThiz->mbSdCardState = TRUE;
    pThiz->mpCallbacks = NULL;
    pThiz->mpAppData = NULL;
    pThiz->mPrefetchFlagAudio = FALSE;
    pThiz->mbShutDownNowFlag = FALSE;


    pThiz->bNeedSw = FALSE;
    pThiz->bNeedSwAudio = FALSE;
    pThiz->bTimeMeetAudio = FALSE;
    pThiz->mVideoFrmCntWriteMore = 0;
    pThiz->mVideoPtsWriteMoreSt = -1;
}

static RecSinkPacket* RecSinkGetRSPacket_l(RecSink *pRecSink)
{
    RecSinkPacket   *pRSPacket = NULL;
    if(!list_empty(&pRecSink->mValidRSPacketList))
    {
        pRSPacket = list_first_entry(&pRecSink->mValidRSPacketList, RecSinkPacket, mList);
        list_del(&pRSPacket->mList);
    }
    return pRSPacket;
}

static RecSinkPacket* RecSinkGetRSPacket(RecSink *pRecSink)
{
    RecSinkPacket   *pRSPacket = NULL;
    pthread_mutex_lock(&pRecSink->mRSPacketListMutex);
    pRSPacket = RecSinkGetRSPacket_l(pRecSink);
    pthread_mutex_unlock(&pRecSink->mRSPacketListMutex);
    return pRSPacket;
}

static void RecSinkMovePrefetchRSPackets(RecSink *pRecSink)
{
    pthread_mutex_lock(&pRecSink->mRSPacketListMutex);
    list_splice_init(&pRecSink->mPrefetchRSPacketList, &pRecSink->mValidRSPacketList);
    pthread_mutex_unlock(&pRecSink->mRSPacketListMutex);
}

static void RecSinkUpdateStreamBasePts(RecSink *pRecSink, RecSinkPacket *pSrcPkt)
{
    if (!pRecSink->mbTrackInit[pSrcPkt->mStreamId])
	{
		pRecSink->mbTrackInit[pSrcPkt->mStreamId] = TRUE;
        pRecSink->mBasePts[pSrcPkt->mStreamId] = pSrcPkt->mPts;
        alogw("streamIdx[%d-%d-%d] BasePts[%lld]us!", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pSrcPkt->mStreamId, pSrcPkt->mPts);
        if(-1 == pRecSink->mOrigBasePts[pSrcPkt->mStreamId])
        {
            pRecSink->mOrigBasePts[pSrcPkt->mStreamId] = pSrcPkt->mPts;
            alogd("streamIdx[%d-%d-%d] OrigBasePts[%lld]us!", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pSrcPkt->mStreamId, pSrcPkt->mPts);
        }
	}
    //if(pRecSink->mInputPrevPts[pDesPkt->stream_index] != -1)
    //{
    //    pDesPkt->duration = (pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000;
    //    if(pDesPkt->duration <= 0)
    //    {
    //        aloge("fatal error! stream[%d], curPts[%lld]us<=prevPts[%lld]us!", pDesPkt->stream_index, pSrcPkt->mPts, pRecSink->mInputPrevPts[pDesPkt->stream_index]);
    //    }
    //}
    //if(pDesPkt->duration < 0)
    //{
    //    if(pSrcPkt->mStreamType == CODEC_TYPE_VIDEO)
    //    {
    //        pDesPkt->duration = 1000*1000 / pRecSink->mpMediaInf->uVideoFrmRate;
    //    }
    //    else if (pSrcPkt->mStreamType == CODEC_TYPE_AUDIO)
    //    {
    //        pDesPkt->duration = MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate;
    //    }
    //    else if (pSrcPkt->mStreamType == CODEC_TYPE_TEXT)
    //    {
    //        pDesPkt->duration = 1000;
    //    }
    //    alogd("first packet of stream[%d], duration[%d]ms!", pSrcPkt->mStreamType, pDesPkt->duration);
    //}
}

/*******************************************************************************
Function name: RecSinkWriteRSPacket
Description:
    AVPacket,
    mbTrackInit[]
    mBasePts[]
    mPrevPts[]
    mDuration
    mDurationAudio
    mVideoFrameCounter
Parameters:

Return:

Time: 2014/2/18
*******************************************************************************/
static ERRORTYPE RecSinkWriteRSPacket(RecSink *pRecSink, RecSinkPacket *pSrcPkt)
{
    int ret;
    AVPacket packet;
    AVPacket *pDesPkt = &packet;
    memset(pDesPkt, 0, sizeof(AVPacket));
	pDesPkt->dts = -1;
    pDesPkt->data0 = pSrcPkt->mpData0;
    pDesPkt->size0 = pSrcPkt->mSize0;
    pDesPkt->data1 = pSrcPkt->mpData1;
    pDesPkt->size1 = pSrcPkt->mSize1;
    pDesPkt->stream_index = pSrcPkt->mStreamId;
    pDesPkt->flags = pSrcPkt->mFlags;
    pDesPkt->duration = -1;
    pDesPkt->pos = -1;
    pDesPkt->CurrQp = pSrcPkt->mCurrQp;
    pDesPkt->avQp = pSrcPkt->mavQp;
	pDesPkt->nGopIndex = pSrcPkt->mnGopIndex;
	pDesPkt->nFrameIndex = pSrcPkt->mnFrameIndex;
	pDesPkt->nTotalIndex = pSrcPkt->mnTotalIndex;

    //find VideoStream mediainfo
    int VideoInfoIndex = 0;
    for (VideoInfoIndex = 0; VideoInfoIndex < pRecSink->mpMediaInf->mVideoInfoValidNum; VideoInfoIndex++)
    {
        if (pSrcPkt->mStreamId == pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].mStreamId)
        {
            //alogd("VideoInfoIndex: %d, VideoStreamId: %d", VideoInfoIndex,
                    //pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].mStreamId);
            break;
        }
    }
    RecSinkUpdateStreamBasePts(pRecSink, pSrcPkt);
//    int64_t nBasePts;
//    if(pRecSink->mRecordMode & RECORDER_MODE_VIDEO)
//    {
//        if(pRecSink->mBasePts[CODEC_TYPE_VIDEO] >= 0)
//        {
//            nBasePts = pRecSink->mBasePts[CODEC_TYPE_VIDEO];
//        }
//        else
//        {
//            alogw("fatal error! videoBasePts[%lld]<0, cur stream_index[%d], check code!", pRecSink->mBasePts[CODEC_TYPE_VIDEO], pDesPkt->stream_index);
//            nBasePts = pRecSink->mBasePts[pDesPkt->stream_index];
//        }
//    }
//    else
//    {
//        nBasePts = pRecSink->mBasePts[pDesPkt->stream_index];
//    }
//    pDesPkt->pts = pSrcPkt->mPts - nBasePts;
//    if(pDesPkt->pts < 0)
//    {
//        alogw("Be careful! streamIdx[%d] pts[%lld]<0, [%lld][%lld][%lld], change to 0, check code!",
//            pDesPkt->stream_index, pDesPkt->pts, pSrcPkt->mPts, pRecSink->mBasePts[pDesPkt->stream_index], nBasePts);
//        pDesPkt->pts = 0;
//    }
    pDesPkt->pts = pSrcPkt->mPts - pRecSink->mBasePts[pDesPkt->stream_index];
    //if(pRecSink->mPrevPts[pDesPkt->stream_index] >= 0)
    //{
    //    pDesPkt->duration = (int)((pDesPkt->pts - pRecSink->mPrevPts[pDesPkt->stream_index])/1000);
    //}
    pRecSink->mPrevPts[pDesPkt->stream_index] = pDesPkt->pts;

    if(pRecSink->mInputPrevPts[pDesPkt->stream_index] != -1)
    {
        //if(pDesPkt->stream_index == CODEC_TYPE_VIDEO )
        if (pSrcPkt->mStreamType == CODEC_TYPE_VIDEO)
        {
            //if ((pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000 > 1000*1000/pRecSink->mpMediaInf->uVideoFrmRate *2)
            if ((pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000 >
                    1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate * 2)
            {
                aloge("rec_in_pts_v_invalid:%d-%lld-%lld-%lld",
                    pRecSink->mMuxerGrpId,pRecSink->mInputPrevPts[pDesPkt->stream_index],pSrcPkt->mPts,
                    (pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000);
            }
        }
        //else if(pDesPkt->stream_index  == CODEC_TYPE_AUDIO)
        else if (pSrcPkt->mStreamType == CODEC_TYPE_AUDIO)
        {
            //if ((pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000 > MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate *2)
            if ((pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000 >
                    MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate * 2)
            {
                aloge("rec_in_pts_a_invalid:%d-%lld-%lld-%lld",
                    pRecSink->mMuxerGrpId,pRecSink->mInputPrevPts[pDesPkt->stream_index],pSrcPkt->mPts,
                    (pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000);
            }
        }
    }

    pRecSink->mInputPrevPts[pDesPkt->stream_index] = pSrcPkt->mPts;

    if(pSrcPkt->mStreamType == CODEC_TYPE_VIDEO)   //if base on videoBasePts, then video duration is accurate.
    {
        //pRecSink->mDuration += pDesPkt->duration;
        //pRecSink->mLoopDuration += pDesPkt->duration;
        //pRecSink->mDuration = lround((double)pDesPkt->pts/1000 + 1000*1000/pRecSink->mpMediaInf->uVideoFrmRate);
        pRecSink->mDuration[pDesPkt->stream_index] = lround((double)pDesPkt->pts/1000
                                /*+ 1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate*/);
        if(pRecSink->mDuration[pDesPkt->stream_index] <= 0)
        {
            pDesPkt->duration = lround(1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate);
        }
        else
        {
            pDesPkt->duration = pRecSink->mDuration[pDesPkt->stream_index] - pRecSink->mPrevDuration[pDesPkt->stream_index];
        }
        pRecSink->mPrevDuration[pDesPkt->stream_index] = pRecSink->mDuration[pDesPkt->stream_index];
        if(pRecSink->mRefVideoStreamIndex == pSrcPkt->mStreamId)
        {
            //pRecSink->mLoopDuration = llround((double)(pSrcPkt->mPts - pRecSink->mOrigBasePts[pDesPkt->stream_index])/1000 +
                                                //1000*1000/pRecSink->mpMediaInf->uVideoFrmRate);
            pRecSink->mLoopDuration = llround((double)(pSrcPkt->mPts - pRecSink->mOrigBasePts[pDesPkt->stream_index])/1000
                            /*+ 1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate*/);
            pRecSink->mVideoFrameCounter++;
            pRecSink->video_frm_cnt++;
        }
        pRecSink->mFileSizeBytes += (int64_t)(pDesPkt->size0 + pDesPkt->size1 + 32);
    }
    else if (pSrcPkt->mStreamType == CODEC_TYPE_AUDIO)   //if base on videoBasePts, audio duration is not accurate.
    {
        //pRecSink->mDurationAudio += pDesPkt->duration;
        pRecSink->mDurationAudio = pDesPkt->pts/1000 /*+ MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate*/;
        if(pRecSink->mDurationAudio <= 0)
        {
            pDesPkt->duration = MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate;
        }
        else
        {
            pDesPkt->duration = pRecSink->mDurationAudio - pRecSink->mPrevDurationAudio;
        }
        pRecSink->mPrevDurationAudio = pRecSink->mDurationAudio;
        pRecSink->mFileSizeBytes += (int64_t)(pDesPkt->size0 + pDesPkt->size1);
        pRecSink->mLoopDurationAudio = (pSrcPkt->mPts - pRecSink->mOrigBasePts[pDesPkt->stream_index])/1000
                                                /*+ MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate*/;
        pRecSink->audio_frm_cnt++;
        pRecSink->mAudioFrmCnt++;
        if(0 == (pRecSink->mRecordMode & RECORDER_MODE_VIDEO))
        {
            pRecSink->mDuration[0] = pRecSink->mDurationAudio;
            pRecSink->mPrevDuration[0] = pRecSink->mPrevDurationAudio;
            pRecSink->mLoopDuration = pRecSink->mLoopDurationAudio;
        }
    }
	else if (pSrcPkt->mStreamType == CODEC_TYPE_TEXT)
	{
        //pRecSink->mDurationText += pDesPkt->duration;
        pRecSink->mDurationText = pDesPkt->pts/1000 /*+ 1000*/;
        pDesPkt->duration = pRecSink->mDurationText - pRecSink->mPrevDurationText;
        pRecSink->mPrevDurationText = pRecSink->mDurationText;
        pRecSink->mFileSizeBytes += (int64_t)(pDesPkt->size0 + pDesPkt->size1);
    }

    if(pRecSink->mbSdCardState || pRecSink->nCallbackOutFlag)
    {
		if (pRecSink->pWriter != NULL)
        {
            if(1 == pRecSink->rc_thm_pic_ready) // insert thm pic
            {
                AVPacket thm_packet;
                memset(&thm_packet,0,sizeof(AVPacket));

                thm_packet.flags |= AVPACKET_FLAG_THUMBNAIL;
                thm_packet.data0 = pRecSink->rc_thm_pic.p_thm_pic_addr;
                thm_packet.size0 = pRecSink->rc_thm_pic.thm_pic_size;

                ret = pRecSink->pWriter->MuxerWritePacket(pRecSink->pMuxerCtx, &thm_packet);
                if(0 != ret)
                {
                    aloge("rc_wrt_thm_pic_failed!!");
                }
                if(NULL != pRecSink->rc_thm_pic.p_thm_pic_addr) 
                {
                    free(pRecSink->rc_thm_pic.p_thm_pic_addr);
                    pRecSink->rc_thm_pic.p_thm_pic_addr = NULL;
                }
                pRecSink->rc_thm_pic.thm_pic_size = 0;
                pRecSink->rc_thm_pic_ready = 0;
            }
            ret = pRecSink->pWriter->MuxerWritePacket(pRecSink->pMuxerCtx, pDesPkt);
            if (ret != 0)
            {
                aloge("fatal error! muxerId[%d]muxerWritePacket FAILED", pRecSink->mMuxerId);
                pRecSink->mpCallbacks->EventHandler(
                        pRecSink,
                        pRecSink->mpAppData,
                        COMP_EventError,
                        ERR_MUX_NOT_PERM,
                        COMP_StateInvalid,
                        NULL);
                pRecSink->mStatus = COMP_StateInvalid;   //OMX_StateIdle;
            }
		}
        else
        {
			alogw("pWriter=NULL, muxer not initialize");
		}
    }
    else
    {
        alogw("Sdcard is not exist, skip MuxerWritePacket()!");
    }
    return SUCCESS;
}

static ERRORTYPE RecSinkReleaseRSPacket_l(RecSink *pRecSink, RecSinkPacket *pRSPacket)
{
    ERRORTYPE omxRet;
    omxRet = pRecSink->mpCallbacks->EmptyBufferDone(pRecSink, pRecSink->mpAppData, pRSPacket);
    if(omxRet != SUCCESS)
    {
        aloge("fatal error! emptyBufferDone fail[0x%x]", omxRet);
    }
    list_add_tail(&pRSPacket->mList, &pRecSink->mIdleRSPacketList);
    return omxRet;
}

static ERRORTYPE RecSinkToForceIFrame(RecSink *pRecSink, int nStreamId)
{
    ERRORTYPE omxRet;
    omxRet = pRecSink->mpCallbacks->ForceIFrame(pRecSink, pRecSink->mpAppData, nStreamId);
    if(omxRet != SUCCESS)
    {
        aloge("fatal error! ForceIFrame fail[0x%x]", omxRet);
    }
    return omxRet;
}

static ERRORTYPE RecSinkReleaseRSPacket(RecSink *pRecSink, RecSinkPacket *pRSPacket)
{
    ERRORTYPE omxRet;
    omxRet = pRecSink->mpCallbacks->EmptyBufferDone(pRecSink, pRecSink->mpAppData, pRSPacket);
    if(omxRet != SUCCESS)
    {
        aloge("fatal error! emptyBufferDone fail[0x%x]", omxRet);
    }
    pthread_mutex_lock(&pRecSink->mRSPacketListMutex);
    list_add_tail(&pRSPacket->mList, &pRecSink->mIdleRSPacketList);
    pthread_mutex_unlock(&pRecSink->mRSPacketListMutex);
    return omxRet;
}

static ERRORTYPE RecSinkDrainAllRSPackets(RecSink *pRecSink, BOOL bIgnore)
{
    int cnt = 0;
    int sendCnt = 0;
    RecSinkPacket   *pEntry, *pTmp;
    RecSinkPacket   *pRSPacket;
    pthread_mutex_lock(&pRecSink->mRSPacketListMutex);
    alogd("muxerId[%d]muxerMode[%d] begin to drain packets, bIgnore[%d]", pRecSink->mMuxerId, pRecSink->nMuxerMode, bIgnore);
    while(1)
    {
        if(bIgnore)
        {
            break;
        }
        if(!pRecSink->mbSdCardState)
        {
            alogd("sdcard is pull out");
            break;
        }
        pRSPacket = RecSinkGetRSPacket_l(pRecSink);
        if(pRSPacket)
        {
            cnt++;
            if(!pRecSink->mbShutDownNowFlag)
            {
                if(pRecSink->mbMuxerInit == FALSE)
                {
                    BOOL bGrant = TRUE;
                    if(pRecSink->mRecordMode & RECORDER_MODE_VIDEO)
                    {
                        if(!(pRSPacket->mFlags & AVPACKET_FLAG_KEYFRAME))
                        {
                            bGrant = FALSE;
                        }
                    }
                    if(bGrant)
                    {
                        pthread_mutex_lock(&pRecSink->mutex_reset_writer_lock);
                        if(pRecSink->nCallbackOutFlag==TRUE || (pRecSink->nOutputFd>=0 || pRecSink->mPath!=NULL) || pRecSink->reset_fd_flag==TRUE)
                        {
                            if(RecSinkMuxerInit(pRecSink, pRSPacket) != SUCCESS)
                            {
                                aloge("fatal error! muxerId[%d][%p]ValidMuxerInit Error!", pRecSink->mMuxerId, pRecSink);
                                pRecSink->mStatus = COMP_StateInvalid;   //OMX_StateIdle;
                                pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                                RecSinkReleaseRSPacket_l(pRecSink, pRSPacket);
                                break;
                            }
                        }
                        pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                    }
                }
                if(pRecSink->mbMuxerInit)
                {
                    RecSinkWriteRSPacket(pRecSink, pRSPacket);
                }
            }
            //release RSPacket
            RecSinkReleaseRSPacket_l(pRecSink, pRSPacket);
        }
        else
        {
            break;
        }
    }
    if(!list_empty(&pRecSink->mValidRSPacketList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pRecSink->mValidRSPacketList, mList)
        {
            pRecSink->mpCallbacks->EmptyBufferDone(pRecSink, pRecSink->mpAppData, pEntry);
            list_move_tail(&pEntry->mList, &pRecSink->mIdleRSPacketList);
            cnt++;
            sendCnt++;
        }
        alogd("left [%d]packets to send out immediately", sendCnt);
    }
    pthread_mutex_unlock(&pRecSink->mRSPacketListMutex);
    alogd("muxerId[%d]muxerMode[%d] drain [%d]packets, ShutDownNow[%d]", pRecSink->mMuxerId, pRecSink->nMuxerMode, cnt, pRecSink->mbShutDownNowFlag);
    return SUCCESS;
}

ERRORTYPE RecSinkStreamCallback(void *hComp, int event)
{
    RecSink *pRecSink = (RecSink*)hComp;
    alogd("WriteDisk fail: muxerId[%d], muxerFileType[0x%x], fd[%d]", pRecSink->mMuxerId, pRecSink->nMuxerMode, pRecSink->nOutputFd);
    pRecSink->mpCallbacks->EventHandler(
                                pRecSink,
                                pRecSink->mpAppData,
                                COMP_EventWriteDiskError,
                                pRecSink->mMuxerId,
                                0,
                                NULL);
    return SUCCESS;
}

static char *generateFilepathFromFd(const int fd)
{
    char fdPath[1024];
    char absoluteFilePath[1024];
    int ret;
    if(fd < 0)
    {
        aloge("fatal error! fd[%d] wrong", fd);
        return NULL;
    }

    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", fd);
    ret = readlink(fdPath, absoluteFilePath, 1024 - 1);
    if (ret == -1)
    {
        aloge("fatal error! readlink[%s] failure, errno(%s)", fdPath, strerror(errno));
        return NULL;
    }
    absoluteFilePath[ret] = '\0';
//    alogd("readlink[%s], filePath[%s]", fdPath, absoluteFilePath);
    return strdup(absoluteFilePath);
}


ERRORTYPE RecSinkMuxerInit(RecSink *pSinkInfo, RecSinkPacket *pRSPacket)
{
    alogd("RecSinkMuxerInit this Stream Id is : %d", pRSPacket->mStreamId);

    int ret = 0;
    char *file_path = NULL;

    alogd("sinkInfo[%p]muxerId[%d-%d],fd[%d], nCallbackOutFlag[%d], fileDurPolicy[%d], fileDur[%lld]ms",
        pSinkInfo, pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->nOutputFd, pSinkInfo->nCallbackOutFlag,
        pSinkInfo->mFileDurationPolicy, pSinkInfo->mMaxFileDuration);
    if(pSinkInfo->rec_file == FILE_NORMAL)
    {
        pSinkInfo->mCurMaxFileDuration = pSinkInfo->mMaxFileDuration;
        pSinkInfo->mCurFileEndTm += pSinkInfo->mCurMaxFileDuration;
        if(FALSE == pSinkInfo->bBufFromCacheFlag)
        {
            if(SOURCE_TYPE_CACHEMANAGER == pRSPacket->mSourceType)
            {
                if(pRSPacket->mStreamId != 0)
                {
                    alogw("Be careful! impact file init! [%d-%d-%d] packet is not main video stream!", pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pRSPacket->mStreamId);
                }
                //if RecSink indicates its packet is not from cache manager, but first RSPacket is from cache manager,
                //it means this RSPacket's pts may less than mCurFileEndTm.
                //In order to keep duration accurate, when reseting mCurFileEndTm, we must consider first cache RSPacket's pts.
                if(pRSPacket->mPts/1000 < pSinkInfo->mCurFileEndTm)
                {
                    pSinkInfo->mCurFileEndTm = pRSPacket->mPts/1000 + pSinkInfo->mCurMaxFileDuration;
                    alogd("muxerId[%d] maybe change to sos file, CurFileEndTm[%lld]ms", pSinkInfo->mMuxerId, pSinkInfo->mCurFileEndTm);
                }
            }
        }
    }
    else if(pSinkInfo->rec_file == FILE_NEED_SWITCH_TO_NORMAL)
    {
        if(pRSPacket->mStreamId != 0)
        {
            alogw("Be careful! impact file init, switch_to_normal! [%d-%d-%d] packet is not main video stream!", pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pRSPacket->mStreamId);
        }
        pSinkInfo->mCurMaxFileDuration = pSinkInfo->mMaxFileDuration;
        if(pRSPacket!=NULL && CODEC_TYPE_VIDEO==pRSPacket->mStreamType)
        {
            int tmpStreamIndex = pRSPacket->mStreamId;    //need to use proper method to get stream index of current packet.
            int64_t tmpDurationMs = (pRSPacket->mPts - pSinkInfo->mOrigBasePts[tmpStreamIndex])/1000;
            int64_t tmpThreshMs = 500;
            if(tmpDurationMs + tmpThreshMs >= pSinkInfo->mCurFileEndTm)
            {
                int64_t oldFileEndTm = pSinkInfo->mCurFileEndTm;
                while(1)
                {
                    pSinkInfo->mCurFileEndTm += pSinkInfo->mCurMaxFileDuration;
                    if(pSinkInfo->mCurFileEndTm > tmpDurationMs + tmpThreshMs)
                    {
                        break;
                    }
                }
                alogd("muxerId[%d]:switch file normal in muxerInit: tmpDuration[%lld]-LastFileEndTm[%lld]=[%lld], >=[%lld]ms, increase curFileTm",
                    pSinkInfo->mMuxerId, tmpDurationMs, oldFileEndTm, tmpDurationMs - oldFileEndTm, -tmpThreshMs);
            }
            else
            {
                alogd("muxerId[%d]:switch file normal in muxerInit: tmpDuration[%lld]-LastFileEndTm[%lld]=[%lld], <[%lld]ms, keep curFileTm",
                    pSinkInfo->mMuxerId, tmpDurationMs, pSinkInfo->mCurFileEndTm, tmpDurationMs - pSinkInfo->mCurFileEndTm, -tmpThreshMs);
            }
        }
        else
        {
            pSinkInfo->mCurFileEndTm += pSinkInfo->mCurMaxFileDuration;
        }
        alogd("muxerId[%d] switch file normal done in muxerInit()", pSinkInfo->mMuxerId);
        pSinkInfo->rec_file = FILE_NORMAL;
    }
    else if (pSinkInfo->rec_file == FILE_IMPACT_RECDRDING)
    {
        aloge("fatal error! not care impact!");
        //pSinkInfo->mCurMaxFileDuration = pSinkInfo->mImpactFileDuration;
        //pSinkInfo->mCurFileEndTm = pSinkInfo->mLoopDuration + pSinkInfo->mCurMaxFileDuration;
    }
    else if (pSinkInfo->rec_file == FILE_NEED_SWITCH_TO_IMPACT)
    {
        aloge("fatal error! not care impact!");
//        alogd("rec file state FILE_NEED_SWITCH_TO_IMPACT, so directly impact file.");
//        pSinkInfo->rec_file = FILE_IMPACT_RECDRDING;
//      pSinkInfo->mCurMaxFileDuration = pSinkInfo->mImpactFileDuration;
//        pSinkInfo->mCurFileEndTm += pSinkInfo->mCurMaxFileDuration;
    }
    else
    {
        aloge("fatal error! wrong rec file state[%d]", pSinkInfo->rec_file);
        pSinkInfo->mCurMaxFileDuration = pSinkInfo->mMaxFileDuration;
    }

    alogw("muxer_init:[%d-%d]-%d-%d-%lld-%lld", pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->reset_fd_flag,
        pSinkInfo->nOutputFd, pSinkInfo->mCurMaxFileDuration, pSinkInfo->mCurFileEndTm);
    if(FALSE == pSinkInfo->nCallbackOutFlag)
    {
        if(pSinkInfo->nOutputFd<0 && NULL==pSinkInfo->mPath)
        {
            alogw("Be careful! nOutputFd<0 when call RecRender MuxerInit, have last chance to get fd!");
            if(pSinkInfo->reset_fd_flag)
            {
                if(pSinkInfo->nSwitchFd >= 0)
                {
                    if(pSinkInfo->mSwitchFilePath)
                    {
                        aloge("fatal error! fd[%d] and path[%s] only use one, check code!", pSinkInfo->nSwitchFd, pSinkInfo->mSwitchFilePath);
                    }
                    pSinkInfo->nOutputFd = pSinkInfo->nSwitchFd;
                    pSinkInfo->nSwitchFd = -1;
                    pSinkInfo->nFallocateLen = pSinkInfo->nSwitchFdFallocateSize;
                    pSinkInfo->nSwitchFdFallocateSize = 0;
//                    if(pSinkInfo->mSwitchFdImpactFlag)
//                    {
//                        alogd("to write impact fd!");
//                        pSinkInfo->mSwitchFdImpactFlag = 0;
//                    }
                    pSinkInfo->reset_fd_flag = FALSE;
                }
                else if(pSinkInfo->mSwitchFilePath)
                {
                    if(pSinkInfo->mPath)
                    {
                        aloge("fatal error! mPath[%s] should be null", pSinkInfo->mPath);
                        free(pSinkInfo->mPath);
                        pSinkInfo->mPath = NULL;
                    }
                    pSinkInfo->mPath = strdup(pSinkInfo->mSwitchFilePath);
                    free(pSinkInfo->mSwitchFilePath);
                    pSinkInfo->mSwitchFilePath = NULL;
                    pSinkInfo->nFallocateLen = pSinkInfo->nSwitchFdFallocateSize;
                    pSinkInfo->nSwitchFdFallocateSize = 0;
//                    if(pSinkInfo->mSwitchFdImpactFlag)
//                    {
//                        alogd("to write impact fd!");
//                        pSinkInfo->mSwitchFdImpactFlag = 0;
//                    }
                    pSinkInfo->reset_fd_flag = FALSE;
                }
                else
                {
                    alogd("pSinkInfo->nSwitchFd[%d] < 0", pSinkInfo->nSwitchFd);
                }
            }
            else
            {
                aloge("fatal error! reset_fd flag != TRUE");
            }
        }
        if(pSinkInfo->nOutputFd>=0)
        {
            if(pSinkInfo->nOutputFd == 0)
            {
                alogd("Be careful! fd == 0");
            }
//            pSinkInfo->pOutputFile = fdopen(pSinkInfo->nOutputFd, "wb");
//            if(pSinkInfo->pOutputFile==NULL)
//            {
//                aloge("fatal error! get file fd failed, test");
//            }
        }

        file_path = generateFilepathFromFd(pSinkInfo->nOutputFd);
        if(NULL != file_path)
        {
            alogw("muxer_init2:%d-%s",pSinkInfo->nOutputFd,file_path);
        }


        else if(pSinkInfo->mPath!=NULL)
        {
        }
        else
        {
            alogd("RecSink[%p] is not set nOutputFd[%d]", pSinkInfo, pSinkInfo->nOutputFd);
//            if(pSinkInfo->pOutputFile!=NULL)
//            {
//                aloge("fatal error! RecSink[%p] pOutputFile[%p] is not NULL!", pSinkInfo, pSinkInfo->pOutputFile);
//                pSinkInfo->pOutputFile = NULL;
//            }
        }
    }
    int i;
    for(i=0;i<MAX_VIDEO_TRACK_COUNT;i++)
    {
        pSinkInfo->mDuration[i] = 0;
        pSinkInfo->mPrevDuration[i] = 0;
        pSinkInfo->mPrefetchFlag[i] = FALSE;
        pSinkInfo->need_to_force_i_frm[i] = 0;
        pSinkInfo->forced_i_frm[i] = 0;
        pSinkInfo->key_frm_sent[i] = 0;
        pSinkInfo->v_frm_drp_cnt[i] = 0;
    }
    //pSinkInfo->mDuration = 0;
    pSinkInfo->mDurationAudio = 0;
    pSinkInfo->mDurationText = 0;
    //pSinkInfo->mPrevDuration = 0;
    pSinkInfo->mPrevDurationAudio = 0;
    pSinkInfo->mPrevDurationText = 0;
    pSinkInfo->mFileSizeBytes = 0;
    for(i=0;i<MAX_TRACK_COUNT;i++)
    {
        pSinkInfo->mbTrackInit[i] = FALSE;
        pSinkInfo->mPrevPts[i] = -1;
        pSinkInfo->mBasePts[i] = -1;
    }
    pSinkInfo->mVideoFrameCounter = 0;
    pSinkInfo->mAudioFrmCnt = 0;
    if (pSinkInfo->pWriter == NULL)
    {
        pSinkInfo->pWriter = cedarx_record_writer_create(pSinkInfo->nMuxerMode);
        if (NULL == pSinkInfo->pWriter)
        {
            aloge("fatal error! cedarx_record_writer_create failed");
            return ERR_MUX_NOMEM;
        }
        pSinkInfo->pMuxerCtx = pSinkInfo->pWriter->MuxerOpen((int*)&ret);
        if (ret != SUCCESS)
        {
            aloge("fatal error! [%p]MuxerOpen failed", pSinkInfo);
            goto MUXER_OPEN_ERR;
        }
        else
        {
            alogv("MuxerOpen OK");
        }
        //TODO: now, write fd and callback mode are mutex. In the future, we will improve it if necessary.
        if (FALSE == pSinkInfo->nCallbackOutFlag)
        {
            if(pSinkInfo->nOutputFd>=0)
            {
                pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETFALLOCATELEN, (unsigned int)pSinkInfo->nFallocateLen, NULL);
                if (pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETCACHEFD2, (unsigned int)pSinkInfo->nOutputFd, NULL) != 0)
                {
                    aloge("fatal error! SETCACHEFD2 failed");
                    goto SETCACHEFD_ERR;
                }
            }
            else if(pSinkInfo->mPath!=NULL)
            {
                pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETFALLOCATELEN, (unsigned int)pSinkInfo->nFallocateLen, NULL);
                if (pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETCACHEFD, 0, (void*)pSinkInfo->mPath) != 0)
                {
                    aloge("fatal error! SETCACHEFD failed");
                    goto SETCACHEFD_ERR;
                }
            }
            //alogd("use fsWriteMode[%d], simpleCacheSize[%d]KB", pSinkInfo->mFsWriteMode, pSinkInfo->mFsSimpleCacheSize/1024);
            alogd("FileDurationPolicy[%d-%d][0x%x], use fsWriteMode[%d], simpleCacheSize[%ld]KB",
                    pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->mFileDurationPolicy, pSinkInfo->mFsWriteMode, pSinkInfo->mFsSimpleCacheSize/1024);
            if(FSWRITEMODE_CACHETHREAD == pSinkInfo->mFsWriteMode)
            {
                aloge("fatal error! not use cacheThread mode now!");
            }
            pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SET_FS_WRITE_MODE, pSinkInfo->mFsWriteMode, NULL);
            pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SET_FS_SIMPLE_CACHE_SIZE, pSinkInfo->mFsSimpleCacheSize, NULL);

            cdx_write_callback_t  callback;
            callback.hComp = pSinkInfo;
            callback.cb = RecSinkStreamCallback;
            pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SET_STREAM_CALLBACK, 0, (void*)&callback);
        }
        else
        {
            //pRecRenderData->writer->MuxerIoctrl(pRecRenderData->p_muxer_ctx, SETOUTURL, (unsigned int)pRecRenderData->url);
            if (pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, REGISTER_WRITE_CALLBACK, 0, (void*)pSinkInfo->mpCallbackWriter) != 0)
            {
                aloge("fatal error! REGISTER_WRITE_CALLBACK failed");
                goto SETCACHEFD_ERR;
            }
        }
    }
    else
    {
        aloge("fatal error! sinkInfo[%p]muxerId[%d] pWriter[%p] is not NULL!", pSinkInfo, pSinkInfo->mMuxerId, pSinkInfo->pWriter);
    }

    // mjpeg source form camera
//  if(pRecRenderData->is_compress_source == 1) {  /* gushiming compressed source */
//      pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SET_VIDEO_CODEC_ID, CODEC_ID_MJPEG);
//  }

    //pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETAVPARA, 0, (void*)(pSinkInfo->mpMediaInf));
    pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SET_ADD_REPAIR_INFO_FLAG, pSinkInfo->b_add_repair_info_flag, NULL);
    pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SET_FILE_REPAIR_INTERVAL, pSinkInfo->max_frms_tag_interval, NULL);
    int tmp_ret =  pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETAVPARA, 0, (void*)(pSinkInfo->mpMediaInf));
    if(0 != tmp_ret)
    {
        aloge("muxer_set_media_info_failed!!");
        goto SETCACHEFD_ERR;
    }

    if(pSinkInfo->mRecordMode & RECORDER_MODE_VIDEO)
    {
        if (!list_empty(pSinkInfo->mpVencHeaderDataList))
        {
            VencHeaderDataNode *pEntry;
            list_for_each_entry(pEntry, pSinkInfo->mpVencHeaderDataList, mList)
            {
                if (pEntry->mH264SpsPpsInfo.pBuffer && pEntry->mH264SpsPpsInfo.nLength > 0)
                {
                    alogd("pEntry->mH264SpsPpsInfo.nLength: %d", pEntry->mH264SpsPpsInfo.nLength);
                    pSinkInfo->pWriter->MuxerWriteExtraData(pSinkInfo->pMuxerCtx, 
                            (unsigned char *)pEntry->mH264SpsPpsInfo.pBuffer, pEntry->mH264SpsPpsInfo.nLength, pEntry->mStreamId);
                }
            }
        }
        else
        {
            alogw("VencExtraDataList is empty! May be use MJPEG encode or code error?");
        }
        /*
        if(pSinkInfo->mpVencExtraData && pSinkInfo->mpVencExtraData->nLength > 0)
        {
            pSinkInfo->pWriter->MuxerWriteExtraData(pSinkInfo->pMuxerCtx, (unsigned char *)pSinkInfo->mpVencExtraData->pBuffer, pSinkInfo->mpVencExtraData->nLength, 0);
        }
        */
    }
    int nAudioStreamNum = 0;
    if(pSinkInfo->mRecordMode & RECORDER_MODE_AUDIO)
    {
        nAudioStreamNum = 1;
        __extra_data_t  AudioExtraDataForMp4;
        MuxerGenerateAudioExtraData(&AudioExtraDataForMp4, pSinkInfo->mpMediaInf);
        pSinkInfo->pWriter->MuxerWriteExtraData(pSinkInfo->pMuxerCtx, AudioExtraDataForMp4.extra_data, AudioExtraDataForMp4.extra_data_len, pSinkInfo->mpMediaInf->mVideoInfoValidNum);
    }
    if(pSinkInfo->mRecordMode & RECORDER_MODE_TEXT)
    {   // do nothing
        pSinkInfo->pWriter->MuxerWriteExtraData(pSinkInfo->pMuxerCtx, NULL, 0, pSinkInfo->mpMediaInf->mVideoInfoValidNum+nAudioStreamNum);
    }


    ret = pSinkInfo->pWriter->MuxerWriteHeader(pSinkInfo->pMuxerCtx);
    if (ret != 0)
    {
        aloge("write header failed");
        goto SETCACHEFD_ERR;
    }
    else
    {
        alogv("write header ok");
    }
//    if(FALSE==pSinkInfo->reset_fd_flag)
//    {
//        aloge("fatal error! why reset fd flag = 0?");
//    }
//    pSinkInfo->reset_fd_flag = FALSE;
    pSinkInfo->mbMuxerInit = TRUE;
    return SUCCESS;

SETCACHEFD_ERR:
    pSinkInfo->pWriter->MuxerClose(pSinkInfo->pMuxerCtx);
MUXER_OPEN_ERR:
    cedarx_record_writer_destroy(pSinkInfo->pWriter);
    return ERR_MUX_ILLEGAL_PARAM;
}

/*******************************************************************************
Function name: RecSinkMuxerClose
Description:
    1. now, one muxer can only support one method: fwrite or CallbackOut.
        In future, we will change it if necessary.
Parameters:

Return:

Time: 2014/7/12
*******************************************************************************/
ERRORTYPE RecSinkMuxerClose(RecSink *pSinkInfo, int clrFile)
{
    ERRORTYPE ret = SUCCESS;
	if (pSinkInfo->pWriter != NULL)
    {
        /*
        alogw("avsync_muxer_close:%d-%d-%d-%d--%lld-%lld-%lld-%lld--%d-%d",
				pSinkInfo->mMuxerGrpId,pSinkInfo->mpMediaInf->nWidth,
				pSinkInfo->mDuration,pSinkInfo->mDurationAudio,
				pSinkInfo->mLoopDuration,pSinkInfo->mLoopDurationAudio,
				pSinkInfo->mCurMaxFileDuration,pSinkInfo->mCurFileEndTm,
				pSinkInfo->mVideoFrameCounter,pSinkInfo->mAudioFrmCnt);
        */
        if(FALSE == pSinkInfo->nCallbackOutFlag)
        {
            if(pSinkInfo->mbSdCardState)
            {
                ret = pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETTOTALTIME, pSinkInfo->mDuration[pSinkInfo->mRefVideoStreamIndex], NULL);
                if(ret != 0)
                {
                    aloge("writer->MuxerIoctrl setTOTALTIME FAILED, ret: %d", ret);
                }
                ret = pSinkInfo->pWriter->MuxerWriteTrailer(pSinkInfo->pMuxerCtx);
                if(ret!=0)
                {
                    aloge("muxerWriteTrailer ret:[%d]", ret);
                }
                ret = pSinkInfo->pWriter->MuxerClose(pSinkInfo->pMuxerCtx);
                if (ret != 0)
                {
                    aloge("muxerClose failed, ret = %d", ret);
                }
                //fflush(pSinkInfo->pOutputFile);
                if (clrFile == 1)
                {
                    aloge("fatal error! clrFile[%d]!=0", clrFile);
//                    #if (CDXCFG_FILE_SYSTEM==OPTION_FILE_SYSTEM_DIRECT_FATFS)
//                        fat_sync((int)pSinkInfo->pOutputFile);
//                        fat_lseek((int)pSinkInfo->pOutputFile, 0);
//                        fat_truncate((int)pSinkInfo->pOutputFile);
//                    #else
//                        fflush(pSinkInfo->pOutputFile);
//                        ftruncate(fileno(pSinkInfo->pOutputFile), 0);
//                        rewind(pSinkInfo->pOutputFile);
//                    #endif
                }
                else
                {
//                    alogd("before fflush");
//                    fflush(pSinkInfo->pOutputFile);
//                    alogd("before fsync");
//                    fsync(pSinkInfo->nOutputFd);
//                    alogd("after fsync");
                }
            }
            else
            {
                alogd("mbSdCardState[%d], sdcard is pull out", pSinkInfo->mbSdCardState);
                pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETSDCARDSTATE, pSinkInfo->mbSdCardState, NULL);
                pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETTOTALTIME, pSinkInfo->mDuration[pSinkInfo->mRefVideoStreamIndex], NULL);
                pSinkInfo->pWriter->MuxerClose(pSinkInfo->pMuxerCtx);
                alogd("mbSdCardState[%d], sdcard is pull out, muxer close done!", pSinkInfo->mbSdCardState);
            }
            if(pSinkInfo->nOutputFd < 0 && NULL==pSinkInfo->mPath)
            {
                aloge("fatal error! sinkInfo[%p]->nOutputFd[%d]<0", pSinkInfo, pSinkInfo->nOutputFd);
            }
            if(pSinkInfo->nOutputFd>=0)
            {
                close(pSinkInfo->nOutputFd);
                pSinkInfo->nOutputFd = -1;
            }
            if(pSinkInfo->mPath)
            {
                free(pSinkInfo->mPath);
                pSinkInfo->mPath = NULL;
            }
            if(FSWRITEMODE_CACHETHREAD == pSinkInfo->mFsWriteMode)
            {
                aloge("fatal error! not use cacheThread mode now!");
            }
            pSinkInfo->mpCallbacks->EventHandler(pSinkInfo, pSinkInfo->mpAppData, COMP_EventRecordDone, pSinkInfo->mMuxerId, 0, NULL);
        }
        else
        {
            pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SETTOTALTIME, pSinkInfo->mDuration[pSinkInfo->mRefVideoStreamIndex], NULL);
		    pSinkInfo->pWriter->MuxerWriteTrailer(pSinkInfo->pMuxerCtx);
		    pSinkInfo->pWriter->MuxerClose(pSinkInfo->pMuxerCtx);
            //pSinkInfo->nCallbackOutFlag = FALSE;
            pSinkInfo->mpCallbacks->EventHandler(pSinkInfo, pSinkInfo->mpAppData, COMP_EventRecordDone, pSinkInfo->mMuxerId, 0, NULL);
        }
		cedarx_record_writer_destroy(pSinkInfo->pWriter);
		pSinkInfo->pWriter = NULL;
		pSinkInfo->pMuxerCtx = NULL;
	}
    pSinkInfo->mbMuxerInit = FALSE;
    int videoStreamIndex = pSinkInfo->mRefVideoStreamIndex;
    int audioStreamIndex = pSinkInfo->mpMediaInf->mVideoInfoValidNum;
    // alogd("TOTAL duration: %d(ms)", pSinkInfo->mDuration);
    // alogd("TOTAL duration audio: %d(ms)", pSinkInfo->mDurationAudio);
    // alogd("TOTAL duration text: %d(ms)", pSinkInfo->mDurationText);
    alogw("LOOP duration:[%d-%d] %lld(ms), lastPts v[%lld]-a[%lld]=[%lld]ms, curInputPts v[%lld]-a[%lld]=[%lld]ms",
        pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->mLoopDuration,
        (pSinkInfo->mPrevPts[videoStreamIndex] + pSinkInfo->mBasePts[videoStreamIndex])/1000,
        (pSinkInfo->mPrevPts[audioStreamIndex] + pSinkInfo->mBasePts[audioStreamIndex])/1000,
        (pSinkInfo->mPrevPts[videoStreamIndex] + pSinkInfo->mBasePts[videoStreamIndex] - (pSinkInfo->mPrevPts[audioStreamIndex] + pSinkInfo->mBasePts[audioStreamIndex]))/1000,
        pSinkInfo->mDebugInputPts[videoStreamIndex]/1000,
        pSinkInfo->mDebugInputPts[audioStreamIndex]/1000,
        (pSinkInfo->mDebugInputPts[videoStreamIndex] - pSinkInfo->mDebugInputPts[audioStreamIndex])/1000);
    alogw("MuxerId: %d-%d, TOTAL file size: %lld(bytes)",pSinkInfo->mMuxerGrpId,pSinkInfo->mMuxerId, pSinkInfo->mFileSizeBytes);
    return SUCCESS;
}

BOOL RecSinkIfNeedSwitchFile(RecSink *pRecSink,int *need_switch_audio)
{
    BOOL bNeedSwitch = FALSE;
    if(TRUE == pRecSink->bNeedSw)
    {
        return pRecSink->bNeedSw;
    }
    else if(TRUE == pRecSink->bNeedSwAudio)
    {
        *need_switch_audio = 1;
        return FALSE;
    }

    if(pRecSink->mbMuxerInit /* && pRecRenderData->reset_fd_flag == TRUE */ && !pRecSink->nCallbackOutFlag) // no network
    {
        if(pRecSink->rec_file == FILE_NEED_SWITCH_TO_IMPACT || pRecSink->rec_file == FILE_NEED_SWITCH_TO_NORMAL)
        {
            bNeedSwitch = TRUE;
            pRecSink->bNeedSw = TRUE;
        }
        else if(pRecSink->mCurMaxFileDuration > 0)   //user set max duration, then segment file base on duration
        {
            //if(pRecSink->mLoopDuration >= pRecSink->mCurFileEndTm)
            //{
            //    bNeedSwitch = TRUE;
            //}
            //int nErrorInterval = 1000*1000/pRecSink->mpMediaInf->uVideoFrmRate/3;   //unit:ms
            //int nErrorInterval = 1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].uVideoFrmRate/3;
            if(pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_MinDuration ||
                pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_AccurateDuration)
            {
                if(pRecSink->mDuration[pRecSink->mRefVideoStreamIndex] /*+ nErrorInterval*/ >= pRecSink->mCurMaxFileDuration) // to use current actual duration,since current pkt will not be sent out
                {
                    bNeedSwitch = TRUE;
                    pRecSink->bNeedSw = TRUE;
					aloge("need_file_switch_v:%d-%d-%d-%lld-%d-%lld-%d-%d-%d-%d",
                            pRecSink->mMuxerGrpId,pRecSink->mDuration[pRecSink->mRefVideoStreamIndex],pRecSink->mDurationAudio,pRecSink->mCurMaxFileDuration,pRecSink->mFileDurationPolicy,
                            pRecSink->mLoopDurationAudio,pRecSink->video_frm_cnt,pRecSink->audio_frm_cnt,
                            pRecSink->mVideoFrameCounter,pRecSink->mAudioFrmCnt);
                }
            }
            else if(pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_AverageDuration)
            {
                if(pRecSink->mLoopDuration /*+ nErrorInterval*/ >= pRecSink->mCurFileEndTm)
                {
                    bNeedSwitch = TRUE;
                    pRecSink->bNeedSw = TRUE;
					aloge("need_file_switch_v3:%d-%d-%d-%lld-%d-%lld-%lld-%lld-%d-%d-%d-%d",
                                                                        pRecSink->mMuxerGrpId,pRecSink->mDuration[pRecSink->mRefVideoStreamIndex],pRecSink->mDurationAudio,
                                                                        pRecSink->mCurMaxFileDuration,pRecSink->mFileDurationPolicy,
                                                                        pRecSink->mLoopDuration,pRecSink->mCurFileEndTm,
                                                                        pRecSink->mLoopDurationAudio,pRecSink->video_frm_cnt,
                                                                        pRecSink->audio_frm_cnt,
                                                                        pRecSink->mVideoFrameCounter,
                                                                        pRecSink->mAudioFrmCnt);
                }
            }
            else
            {
                aloge("(f:%s, l:%d) fatal error! unknown FileDurationPolicy[0x%x], check code!", __FUNCTION__, __LINE__, pRecSink->mFileDurationPolicy);
            }

            if(FALSE == bNeedSwitch && (pRecSink->mRecordMode & RECORDER_MODE_AUDIO))    // to check audio status
            {
                //int nErrorInterval = MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate/3;   //unit:ms
                *need_switch_audio = 0;
                if(pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_MinDuration ||
                    pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_AccurateDuration)
                {
                    if(pRecSink->mDurationAudio /*+ nErrorInterval*/ >= pRecSink->mCurMaxFileDuration)
                    {
                        *need_switch_audio = 1;
                        pRecSink->bNeedSwAudio = TRUE;
						aloge("need_file_switch_a:%d-%d-%d-%lld-%d-%lld-%lld-%d-%d-%d-%d",pRecSink->mMuxerGrpId,pRecSink->mDurationAudio,pRecSink->mDuration[pRecSink->mRefVideoStreamIndex],
                                                                    pRecSink->mCurMaxFileDuration,pRecSink->mFileDurationPolicy,
                                                                    pRecSink->mLoopDurationAudio,pRecSink->mLoopDuration ,
                                                                    pRecSink->video_frm_cnt, pRecSink->audio_frm_cnt,
                                                                    pRecSink->mVideoFrameCounter,pRecSink->mAudioFrmCnt);
                    }
                }
                else if(pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_AverageDuration)
                {
                    if(pRecSink->mLoopDurationAudio /*+ nErrorInterval*/ >= pRecSink->mCurFileEndTm)
                    {
                        *need_switch_audio = 1;
                        pRecSink->bNeedSwAudio = TRUE;
						aloge("need_file_switch_a3:%d-%d-%d-%lld-%d-%lld-%lld-%lld-%d-%d-%d-%d",pRecSink->mMuxerGrpId,pRecSink->mDurationAudio,pRecSink->mDuration[pRecSink->mRefVideoStreamIndex],
                                                                            pRecSink->mCurMaxFileDuration,pRecSink->mFileDurationPolicy,
                                                                            pRecSink->mLoopDurationAudio,pRecSink->mCurFileEndTm,
                                                                            pRecSink->mLoopDuration,
                                                                            pRecSink->video_frm_cnt, pRecSink->audio_frm_cnt,
                                                                            pRecSink->mVideoFrameCounter,pRecSink->mAudioFrmCnt);
                    }
                }
                else
                {
                    aloge("(f:%s, l:%d) fatal error! unknown FileDurationPolicy[0x%x], check code!", __FUNCTION__, __LINE__, pRecSink->mFileDurationPolicy);
                }
            }

        }
        else if(pRecSink->mMaxFileSizeBytes > 0)   //user set max file size, then segment file base on fileSize.
        {
            if(pRecSink->mFileSizeBytes >= pRecSink->mMaxFileSizeBytes)
            {
                double fileSizeMB = (double)pRecSink->mFileSizeBytes/(1024*1024);
                alogv("fileSize[%lld]Bytes([%7.3lf]MB) >= max[%7.3lf]MB, rec_file[%d], need switch file", pRecSink->mFileSizeBytes, fileSizeMB, (double)pRecSink->mMaxFileSizeBytes/(1024*1024), pRecSink->rec_file);
                bNeedSwitch = TRUE;
                pRecSink->bNeedSw = TRUE;
            }
        }
        else    //not switch file
        {
            bNeedSwitch = FALSE;
            pRecSink->bNeedSw = FALSE;
        }
    }
    else
    {
        bNeedSwitch = FALSE;
    }
    return bNeedSwitch;
}

BOOL RecSinkIfNeedRequestNextFd(RecSink *pRecSink)
{
    if(pRecSink->need_set_next_fd == FALSE)
    {
        return FALSE;
    }
    BOOL bNeedRequest = FALSE;
    if(pRecSink->mCurMaxFileDuration > 0)   //user set max duration, then segment file base on duration
    {
        //if(pRecSink->mLoopDuration + NOTIFY_NEEDNEXTFD_IN_ADVANCE >= pRecSink->mCurFileEndTm)
        //{
        //    bNeedRequest = TRUE;
        //}
        if(pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_MinDuration ||
            pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_AccurateDuration)
        {
            if((pRecSink->mDuration[pRecSink->mRefVideoStreamIndex] + NOTIFY_NEEDNEXTFD_IN_ADVANCE) >= pRecSink->mCurMaxFileDuration)
            {
                bNeedRequest = TRUE;
            }
        }
        else if(pRecSink->mFileDurationPolicy == RecordFileDurationPolicy_AverageDuration)
        {
            if((pRecSink->mLoopDuration + NOTIFY_NEEDNEXTFD_IN_ADVANCE) >= pRecSink->mCurFileEndTm)
            {
                bNeedRequest = TRUE;
            }
            if((pRecSink->mLoopDurationAudio + NOTIFY_NEEDNEXTFD_IN_ADVANCE) >= pRecSink->mCurFileEndTm)
            {
                bNeedRequest = TRUE;
            }
        }
        else
        {
            aloge("(f:%s, l:%d) fatal error! unknown FileDurationPolicy[0x%x], check code!", __FUNCTION__, __LINE__, pRecSink->mFileDurationPolicy);
        }
    }
    else if(pRecSink->mMaxFileSizeBytes > 0)   //user set max file size, then segment file base on fileSize.
    {
        if(pRecSink->mFileSizeBytes + pRecSink->mMaxFileSizeBytes/10 >= pRecSink->mMaxFileSizeBytes)
        {
            double fileSizeMB = (double)pRecSink->mFileSizeBytes/(1024*1024);
            alogd("fileSize[%lld]Bytes([%7.3lf]MB) < max[%7.3lf]MB, rec_file[%d], need request next fd", pRecSink->mFileSizeBytes, fileSizeMB, (double)pRecSink->mMaxFileSizeBytes/(1024*1024), pRecSink->rec_file);
            bNeedRequest = TRUE;
        }
    }
    else
    {
        bNeedRequest = FALSE;
    }
    return bNeedRequest;
}

static BOOL RecSink_CheckStreamIdValid(RecSink *pRecSink, int nStreamId)
{
    if(pRecSink->mValidStreamCount < 0)
    {
        return TRUE;
    }
    for(int i=0; i<pRecSink->mValidStreamCount; i++)
    {
        if(pRecSink->mValidStreamIds[i] == nStreamId)
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL RecSinkGrantSwitchFileAudio(RecSink *pRecSink, RecSinkPacket *pRSPacket)
{
    BOOL bGrant = TRUE;

    if(pRecSink->mbSwitchFileNormalIncludeCache)
    {
        //because we will find key frame from cache, so not need wait key frame here.
        return TRUE;
    }
    if(RecordFileDurationPolicy_AccurateDuration == pRecSink->mFileDurationPolicy)
    {
        bGrant = TRUE;
    }

    else if(RecordFileDurationPolicy_MinDuration == pRecSink->mFileDurationPolicy || RecordFileDurationPolicy_AverageDuration == pRecSink->mFileDurationPolicy)
    {
        if(!(pRecSink->mRecordMode & RECORDER_MODE_VIDEO) || !(pRecSink->mRecordMode & RECORDER_MODE_AUDIO))
        {
            aloge("fatal error! recordMode[0x%x] wrong!", pRecSink->mRecordMode);
        }
        //(1)video0 duration >= audio duration
        if(!pRecSink->bTimeMeetAudio)
        {
            int64_t nVideoTotalTm = -1; //unit:ms
            int64_t nAudioTotalTm = -1; //unit:ms
            int audioStreamIndex = pRecSink->mpMediaInf->mVideoInfoValidNum;
            int VFrameDuration = 1000*1000 / pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].uVideoFrmRate;
            int AFrameDuration = MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate;
            nVideoTotalTm = (pRecSink->mPrevPts[pRecSink->mRefVideoStreamIndex]+pRecSink->mBasePts[pRecSink->mRefVideoStreamIndex])/1000+VFrameDuration;
            nAudioTotalTm = (pRecSink->mPrevPts[audioStreamIndex]+pRecSink->mBasePts[audioStreamIndex])/1000+AFrameDuration;
            if(nAudioTotalTm <= nVideoTotalTm)
            {
                alogd("RecSink[%d-%d] can switch file audio. VTime[%lld]-ATime[%lld]=[%lld]ms, rec_file[%d]",
                                             pRecSink->mMuxerGrpId,pRecSink->mMuxerId,
                                             nVideoTotalTm,
                                             nAudioTotalTm,
                                             nVideoTotalTm-nAudioTotalTm,
                                             pRecSink->rec_file);
                pRecSink->bTimeMeetAudio = TRUE;
            }
        }
        //(2)all videos meet key frame.
        BOOL bKeyFrameReady = FALSE;
        if(pRecSink->bTimeMeetAudio)
        {
            if((pRSPacket->mStreamType==CODEC_TYPE_VIDEO) && (pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
            {
                if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                {
                    aloge("fatal error! wrong streamId[%d]", pRSPacket->mStreamId);
                }
                if(!pRecSink->mPrefetchFlag[pRSPacket->mStreamId])
                {
                    //if current frame is video key frame, we can set prefetchFlag for this video.
                    pRecSink->mPrefetchFlag[pRSPacket->mStreamId] = TRUE;
                }
            }
            if(pRecSink->mpMediaInf->mVideoInfoValidNum > MAX_VIDEO_TRACK_COUNT)
            {
                aloge("fatal error! video stream num[%d] too many!", pRecSink->mpMediaInf->mVideoInfoValidNum);
            }
            int i;
            for(i=0; i<pRecSink->mpMediaInf->mVideoInfoValidNum; i++)
            {
                if(FALSE == RecSink_CheckStreamIdValid(pRecSink, i))
                {
                    alogd("streamId[%d] is not valid, ignore", i);
                    continue;
                }
                if(!pRecSink->mPrefetchFlag[i])
                {
                    break;
                }
            }
            if(i == pRecSink->mpMediaInf->mVideoInfoValidNum)
            {
                bKeyFrameReady = TRUE;
            }
        }
        if(pRecSink->bTimeMeetAudio && bKeyFrameReady)
        {
            int i;
            for(i=0; i<pRecSink->mpMediaInf->mVideoInfoValidNum; i++)
            {
                alogd("RecSink[%d-%d] can switch file. VDur[%d]:[%d]ms", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, i, pRecSink->mDuration[i]);
            }
            alogd("RecSink[%d-%d] can switch file. ADur:[%d]ms", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mDurationAudio);
            bGrant = TRUE;
        }
        else
        {
            bGrant = FALSE;
        }
    }
    return bGrant;
}

BOOL RecSinkGrantSwitchFile(RecSink *pRecSink, RecSinkPacket *pRSPacket)
{
    BOOL bGrant = TRUE;

    if(pRecSink->mbSwitchFileNormalIncludeCache)
    {
        //because we will find key frame from cache, so not need wait key frame here.
        return TRUE;
    }
    if(RecordFileDurationPolicy_AccurateDuration == pRecSink->mFileDurationPolicy)
    {
        bGrant = TRUE;
    }
    else if(RecordFileDurationPolicy_MinDuration == pRecSink->mFileDurationPolicy || RecordFileDurationPolicy_AverageDuration == pRecSink->mFileDurationPolicy)
    {
        if(pRecSink->mRecordMode & RECORDER_MODE_VIDEO)
        {
            if(pRecSink->mpMediaInf->mVideoInfoValidNum > MAX_VIDEO_TRACK_COUNT)
            {
                aloge("fatal error! video stream num[%d] too many!", pRecSink->mpMediaInf->mVideoInfoValidNum);
            }
            //when video meets key frame, we will set prefetch flag, so by prefetch flags, we can decide if all videos meet key frame.
            BOOL bKeyFrameReady = FALSE;
            int i;
            for(i=0; i<pRecSink->mpMediaInf->mVideoInfoValidNum; i++)
            {
                if(FALSE == RecSink_CheckStreamIdValid(pRecSink, i))
                {
                    alogd("streamId[%d] is not valid, ignore", i);
                    continue;
                }
                if(!pRecSink->mPrefetchFlag[i])
                {
                    break;
                }
            }
            if(i == pRecSink->mpMediaInf->mVideoInfoValidNum)
            {
                bKeyFrameReady = TRUE;
            }

            //use videoStream0 to compare with audioStream
            BOOL bAudioDone = FALSE;
            if(pRecSink->mPrefetchFlag[pRecSink->mRefVideoStreamIndex])
            {
                int64_t nVideoTotalTm = -1; //unit:ms
                int64_t nAudioTotalTm = -1; //unit:ms
                int VFrameDuration = 1000*1000 / pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].uVideoFrmRate;
                nVideoTotalTm = (pRecSink->mPrevPts[pRecSink->mRefVideoStreamIndex]+pRecSink->mBasePts[pRecSink->mRefVideoStreamIndex])/1000+VFrameDuration;
                if(pRecSink->mRecordMode & RECORDER_MODE_AUDIO)
                {
                    int AFrameDuration = MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate;
                    int audioStreamIndex = pRecSink->mpMediaInf->mVideoInfoValidNum;
                    nAudioTotalTm = (pRecSink->mPrevPts[audioStreamIndex]+pRecSink->mBasePts[audioStreamIndex])/1000+AFrameDuration;
                    if(nAudioTotalTm >= nVideoTotalTm)
                    {
                        alogw("RecSink[%d-%d-%d] can switch file. aSrcPts[%lld]ms, vSrcPts[%lld]ms, ATime[%lld]-VTime[%lld]=[%lld]ms, rec_file[%d]",
                                 pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mRefVideoStreamIndex,
                                 (pRecSink->mPrevPts[audioStreamIndex]+pRecSink->mBasePts[audioStreamIndex])/1000,
                                 (pRecSink->mPrevPts[pRecSink->mRefVideoStreamIndex]+pRecSink->mBasePts[pRecSink->mRefVideoStreamIndex])/1000,
                                 nAudioTotalTm,
                                 nVideoTotalTm,
                                 nAudioTotalTm - nVideoTotalTm,
                                 pRecSink->rec_file);
                        bAudioDone = TRUE;
                        if(!bKeyFrameReady)
                        {
                            alogw("Be careful! RecSink[%d-%d-%d] Not all video streams meet key frame! we still can't switch file, begin to prefetch audio!", 
                                pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mRefVideoStreamIndex);
                            //we need cache audio now. wait other videos to meet key frame.
                            pRecSink->mPrefetchFlagAudio = TRUE;
                        }
                    }
                    else
                    {
                        if(CODEC_TYPE_AUDIO == pRSPacket->mStreamType)
                        {
                            alogv("RecSink[%d-%d] wait switch file. audio packet. Time a[%lld]ms v[%lld]ms, rec_file[%d]",
                                pRecSink->mMuxerGrpId, pRecSink->mMuxerId,
                                nAudioTotalTm,
                                nVideoTotalTm,
                                pRecSink->rec_file);
                        }
                        bAudioDone = FALSE;
                    }
                }
                else
                {
                    bAudioDone = TRUE;
                }
            }

            if(bKeyFrameReady && bAudioDone)
            {
                int i;
                for(i=0; i<pRecSink->mpMediaInf->mVideoInfoValidNum; i++)
                {
                    alogd("RecSink[%d-%d] can switch file. VDur[%d]:[%d]ms", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, i, pRecSink->mDuration[i]);
                }
                alogd("RecSink[%d-%d] can switch file. ADur:[%d]ms", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mDurationAudio);
                bGrant = TRUE;
            }
            else
            {
                bGrant = FALSE;
            }
        }
        else
        {
            bGrant = TRUE;
        }
    }
    return bGrant;
}

static ERRORTYPE RecSinkSwitchFile_ReleaseRSPacketsUntilCachePacket(RecSink *pRecSink)
{
    if(!pRecSink->mbSwitchFileNormalIncludeCache)
    {
        aloge("fatal error! don't include cache, why call this function?");
        return SUCCESS;
    }
    int cnt = 0;
    int cacheCnt = 0;
    int nSentCacheCnt = 0;
    RecSinkPacket   *pEntry, *pTmp;
    RecSinkPacket   *pRSPacket;
    ERRORTYPE ret;
    pthread_mutex_lock(&pRecSink->mRSPacketListMutex);
    alogd("muxerId[%d-%d]muxerMode[%d] begin to release RSpackets", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->nMuxerMode);
    while(1)
    {
        pRSPacket = RecSinkGetRSPacket_l(pRecSink);
        if(pRSPacket)
        {
            cnt++;
            if(pRSPacket->mSourceType != SOURCE_TYPE_CACHEMANAGER)
            {
                //release RSPacket
                ret = RecSinkReleaseRSPacket_l(pRecSink, pRSPacket);
                if(ret != SUCCESS)
                {
                    aloge("fatal error! release rsPacket fail[0x%x]", ret);
                }
            }
            else
            {
                if(CODEC_TYPE_UNKNOWN==pRSPacket->mStreamType && -1==pRSPacket->mId)
                {
                    alogd("meet flag cache packet, release it. It means there is none cache packet. It's strange.");
                    ret = RecSinkReleaseRSPacket_l(pRecSink, pRSPacket);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! release rsPacket fail[0x%x]", ret);
                    }
                    pRSPacket = NULL;
                }
                cnt--;
                break;
            }
        }
        else
        {
            break;
        }
    }
    if(pRSPacket != NULL)
    {
        list_add(&pRSPacket->mList, &pRecSink->mValidRSPacketList);
    }
    alogd("muxerId[%d] switchFile until cache: release %d non-cache rsPackets", pRecSink->mMuxerId, cnt);

    ERRORTYPE omxRet;
    BOOL bMeetKeyFrame = FALSE;
    int nPacketCount = 0;
    if(!list_empty(&pRecSink->mValidRSPacketList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pRecSink->mValidRSPacketList, mList)
        {
            if(SOURCE_TYPE_CACHEMANAGER == pEntry->mSourceType)
            {
                if(!(pEntry->mStreamType==CODEC_TYPE_VIDEO && pEntry->mFlags&AVPACKET_FLAG_KEYFRAME && pRecSink->mRefVideoStreamIndex == pEntry->mStreamId))
                {
                    if(FALSE == bMeetKeyFrame)
                    {
                        omxRet = pRecSink->mpCallbacks->EmptyBufferDone(pRecSink, pRecSink->mpAppData, pEntry);
                        if(omxRet != SUCCESS)
                        {
                            aloge("fatal error! emptyBufferDone fail[0x%x]", omxRet);
                        }
                        list_move_tail(&pEntry->mList, &pRecSink->mIdleRSPacketList);
                        nSentCacheCnt++;
                    }
                }
                else
                {
                    bMeetKeyFrame = TRUE;
                }
                cacheCnt++;
            }
            else
            {
                alogw("It is possible to appear at last. packet[%d] source type[%d] is not SOURCE_TYPE_CACHEMANAGER", nPacketCount, pEntry->mSourceType);
            }
            nPacketCount++;
        }
    }
    alogd("muxerId[%d] switchFile until cache: total packet num: %d, left %d cache rsPackets, ignore %d cache rsPackets until meet key frame.",
        pRecSink->mMuxerId, nPacketCount, cacheCnt, nSentCacheCnt);
    pthread_mutex_unlock(&pRecSink->mRSPacketListMutex);
    pRecSink->mbSwitchFileNormalIncludeCache = FALSE;
    return SUCCESS;
}

void RecSinkSwitchFile(RecSink *pSinkInfo, int clrFile)
{
    if(0 == pSinkInfo->nOutputFd)
    {
        alogd("Be careful, fd == 0, sinkInfo[%p] muxerId[%d]", pSinkInfo, pSinkInfo->mMuxerId);
    }
//    alogd("TOTAL duration: %d(ms)", pSinkInfo->mDuration);
//    alogd("TOTAL duration audio: %d(ms)", pSinkInfo->mDurationAudio);
//    alogd("TOTAL file size: %lld(bytes)", pSinkInfo->mFileSizeBytes);

    //if switchFileAudio, let video duration >= audio duration.
    if(TRUE==pSinkInfo->bTimeMeetAudio && 0<pSinkInfo->mVideoFrmCntWriteMore)   // more video frame was sent to file
    {
        if(!(pSinkInfo->mRecordMode & RECORDER_MODE_VIDEO) || !(pSinkInfo->mRecordMode & RECORDER_MODE_AUDIO))
        {
            aloge("fatal error! recordMode[0x%x] wrong!", pSinkInfo->mRecordMode);
        }
        int cnt = 0;
        int nAudCnt = 0;
        RecSinkPacket   *pEntry, *pTmp;
        list_for_each_entry(pEntry, &pSinkInfo->mPrefetchRSPacketList, mList)
        {
            if(CODEC_TYPE_AUDIO == pEntry->mStreamType)
            {
                nAudCnt++;
            }
            cnt++;
        }
        alogw("avsync_a_list:[%d-%d-%d]-%d-%d-%d-%lld-%lld-%d", pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->mRefVideoStreamIndex, cnt, nAudCnt,
            pSinkInfo->mVideoFrmCntWriteMore, pSinkInfo->mVideoPtsWriteMoreSt, pSinkInfo->mVideoPtsWriteMoreEnd,
            pSinkInfo->mpMediaInf->mMediaVideoInfo[pSinkInfo->mRefVideoStreamIndex].nWidth);

        int VFrameDuration = 1000*1000 / pSinkInfo->mpMediaInf->mMediaVideoInfo[pSinkInfo->mRefVideoStreamIndex].uVideoFrmRate;
        int AFrameDuration = MAXDECODESAMPLE*1000 / pSinkInfo->mpMediaInf->sample_rate;
        int nAFrmMore = 0;
        BOOL bAllUsedFlag = TRUE;
        list_for_each_entry_safe(pEntry, pTmp, &pSinkInfo->mPrefetchRSPacketList, mList)
        {
            //check video time and audio time.
            if(CODEC_TYPE_AUDIO == pEntry->mStreamType)
            {
                int nAudioTotalTm = pEntry->mPts/1000+AFrameDuration;
                int nVideoTotalTm = (pSinkInfo->mPrevPts[pSinkInfo->mRefVideoStreamIndex]+pSinkInfo->mBasePts[pSinkInfo->mRefVideoStreamIndex])/1000+VFrameDuration;
                if(nAudioTotalTm > nVideoTotalTm)
                {
                    //this aFrame don't be written to current file. because in switchFileAudio case, audioTime <= videoTime.
                    alogd("avsync_rc_swfa: [%d-%d-%d]-%d-%d-%d", pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->mRefVideoStreamIndex, nVideoTotalTm, nAudioTotalTm, nAFrmMore);
                    bAllUsedFlag = FALSE;
                    break;
                }
                else
                {
                    list_del(&pEntry->mList);
                    RecSinkWriteRSPacket(pSinkInfo, pEntry);
                    RecSinkReleaseRSPacket(pSinkInfo, pEntry);
                    nAFrmMore++;
                }
            }
        }
        if(bAllUsedFlag)
        {
            alogd("avsync_rc_swfa write all afrms: [%d-%d]-%d-%d-%d", pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->mDuration[pSinkInfo->mRefVideoStreamIndex], pSinkInfo->mDurationAudio, nAFrmMore);
        }
    }

    int64_t tm1, tm2;
    tm1 = CDX_GetSysTimeUsMonotonic();
    RecSinkMuxerClose(pSinkInfo, clrFile);
    tm2 = CDX_GetSysTimeUsMonotonic();
    alogd("muxerId[%d-%d]recRender_MuxerClose[%lld]MB itl[%lld]ms",
            pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->mFileSizeBytes/(1024*1024), (tm2-tm1)/1000);
    int i;
    for (i=0;i<MAX_VIDEO_TRACK_COUNT;i++)
    {
        pSinkInfo->mDuration[i] = 0;
        pSinkInfo->mPrevDuration[i] = 0;
        pSinkInfo->mPrefetchFlag[i] = FALSE;
        pSinkInfo->need_to_force_i_frm[i] = 0;
        pSinkInfo->forced_i_frm[i] = 0;
        pSinkInfo->key_frm_sent[i] = 0;
        pSinkInfo->v_frm_drp_cnt[i] = 0;
    }
    //pSinkInfo->mDuration = 0;
    pSinkInfo->mDurationAudio = 0;
    pSinkInfo->mDurationText = 0;
    //pSinkInfo->mPrevDuration = 0;
    pSinkInfo->mPrevDurationAudio = 0;
    pSinkInfo->mPrevDurationText = 0;
    pSinkInfo->mFileSizeBytes = 0;

    pSinkInfo->bTimeMeetAudio = FALSE;
    pSinkInfo->mVideoFrmCntWriteMore = 0;
    pSinkInfo->mVideoPtsWriteMoreSt = -1;

    for(i=0;i<MAX_TRACK_COUNT;i++)
    {
        pSinkInfo->mbTrackInit[i] = FALSE;
        pSinkInfo->mPrevPts[i] = -1;
        pSinkInfo->mBasePts[i] = -1;
        pSinkInfo->mInputPrevPts[i] = -1;
//        if(pSinkInfo->mSwitchFdImpactFlag)
//        {
//        }
    }
//    if(pSinkInfo->mPrefetchFlag)
//    {
//        alogd("switchFile, reset prefetchFlag to false!");
//        pSinkInfo->mPrefetchFlag = FALSE;
//    }

    if(pSinkInfo->mPrefetchFlagAudio)
    {
        if(pSinkInfo->bNeedSw)
        {
            alogd("switchFile cache video, cache audio too. reset prefetchFlagAudio to false!");
        }
        if(pSinkInfo->bNeedSwAudio)
        {
            alogd("switchFile cache audio, reset prefetchFlagAudio to false!");
        }
        pSinkInfo->mPrefetchFlagAudio = FALSE;
    }
    alogd("muxer[%d-%d]switch file: needSwitchFlag[%d-%d]", pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, pSinkInfo->bNeedSw, pSinkInfo->bNeedSwAudio);
    pSinkInfo->bNeedSw = FALSE;
    pSinkInfo->bNeedSwAudio = FALSE;

    RecSinkMovePrefetchRSPackets(pSinkInfo);

    if(pSinkInfo->mbSwitchFileNormalIncludeCache)
    {
        //return all non-cache packets until meeting first cache packet.
        //    notice: unknown type and mId = -1, it is flag cache packet, don't fwrite it to files.
        RecSinkSwitchFile_ReleaseRSPacketsUntilCachePacket(pSinkInfo);
    }
    pSinkInfo->mVideoFrameCounter = 0;
    pSinkInfo->mAudioFrmCnt = 0;
    //change fd.
    if(pSinkInfo->nSwitchFd >= 0)
    {
        if(pSinkInfo->mSwitchFilePath)
        {
            aloge("fatal error! fd[%d] and path[%s] only use one, check code!", pSinkInfo->nSwitchFd, pSinkInfo->mSwitchFilePath);
        }
        pSinkInfo->nOutputFd = pSinkInfo->nSwitchFd;
        pSinkInfo->nSwitchFd = -1;
        pSinkInfo->nFallocateLen = pSinkInfo->nSwitchFdFallocateSize;
        pSinkInfo->nSwitchFdFallocateSize = 0;
//        if(pSinkInfo->mSwitchFdImpactFlag)
//        {
//            alogd("to write impact fd!");
//            pSinkInfo->mSwitchFdImpactFlag = 0;
//        }
        pSinkInfo->reset_fd_flag = FALSE;
    }
    else if(pSinkInfo->mSwitchFilePath!=NULL)
    {
        if(pSinkInfo->mPath)
        {
            aloge("fatal error! mPath[%s] should be null", pSinkInfo->mPath);
            free(pSinkInfo->mPath);
            pSinkInfo->mPath = NULL;
        }
        pSinkInfo->mPath = strdup(pSinkInfo->mSwitchFilePath);
        free(pSinkInfo->mSwitchFilePath);
        pSinkInfo->mSwitchFilePath = NULL;
        pSinkInfo->nFallocateLen = pSinkInfo->nSwitchFdFallocateSize;
        pSinkInfo->nSwitchFdFallocateSize = 0;
//        if(pSinkInfo->mSwitchFdImpactFlag)
//        {
//            alogd("to write impact fd!");
//            pSinkInfo->mSwitchFdImpactFlag = 0;
//        }
        pSinkInfo->reset_fd_flag = FALSE;
    }
    else
    {
        alogd("Be careful. pRecRenderData->nSwitchFd[%d],reset_fd_flag[%d] should be 0", pSinkInfo->nSwitchFd, pSinkInfo->reset_fd_flag);
    }

	if (pSinkInfo->rec_file == FILE_NEED_SWITCH_TO_IMPACT)
	{
        aloge("fatal error! check code!");
		pSinkInfo->rec_file = FILE_IMPACT_RECDRDING;
	}
	else if (pSinkInfo->rec_file == FILE_IMPACT_RECDRDING)
	{
        aloge("fatal error! check code!");
		pSinkInfo->rec_file = FILE_NORMAL;
	}
    else if(pSinkInfo->rec_file == FILE_NEED_SWITCH_TO_NORMAL)
    {
        int64_t oldTm = pSinkInfo->mCurFileEndTm;
        pSinkInfo->mCurFileEndTm = pSinkInfo->mLoopDuration;
        alogd("muxerId[%d-%d] switch file normal done. change CurFileEndTm[%lld]ms -> [%lld]ms",
            pSinkInfo->mMuxerGrpId, pSinkInfo->mMuxerId, oldTm, pSinkInfo->mCurFileEndTm);
        pSinkInfo->rec_file = FILE_NORMAL;
    }
    else
    {
    }
}

static void* RecSinkThread(void* pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    RecSink *pRecSink = (RecSink*) pThreadData;
    message_t cmd_msg;
    alogv("RecSink thread start run...\n");
    char threadName[20];
    snprintf(threadName, 20, "RecSink[%d]", pRecSink->mMuxerId);
    prctl(PR_SET_NAME, (unsigned long)threadName, 0, 0, 0);
    while (1) {
PROCESS_MESSAGE:
        if(get_message(&pRecSink->mMsgQueue, &cmd_msg) == 0)
        {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;

            alogv("get_message cmd:%d", cmd);

            // State transition command
            if (cmd == SetState)
            {
                // If the parameter states a transition to the same state
                // raise a same state transition error.
                if (pRecSink->mStatus == (COMP_STATETYPE) (cmddata))
                {
                    pRecSink->mpCallbacks->EventHandler(
                            pRecSink,
                            pRecSink->mpAppData,
                            COMP_EventError,
                            ERR_MUX_SAMESTATE,
                            0,
                            NULL);
                }
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE) (cmddata))
                    {
                        case COMP_StateInvalid:
                        {
                            pRecSink->mVideoPtsWriteMoreSt = -1;
                            pRecSink->mStatus = COMP_StateInvalid;
                            pRecSink->mpCallbacks->EventHandler(
                                    pRecSink,
                                    pRecSink->mpAppData,
                                    COMP_EventError,
                                    ERR_MUX_INCORRECT_STATE_TRANSITION,
                                    0,
                                    NULL);
                            pRecSink->mpCallbacks->EventHandler(
                                    pRecSink,
                                    pRecSink->mpAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pRecSink->mStatus,
                                    NULL);
                            break;
                        }

                        case COMP_StateLoaded:
                        {
                            alogv("set state LOADED");
                            if (pRecSink->mStatus == COMP_StateExecuting || pRecSink->mStatus == COMP_StatePause)
                            {
                                RecSinkMovePrefetchRSPackets(pRecSink);
                                RecSinkDrainAllRSPackets(pRecSink, FALSE);
                                if(pRecSink->mbMuxerInit)
                                {
                                    int64_t tm1, tm2;
                                    tm1 = CDX_GetSysTimeUsMonotonic();
                                    RecSinkMuxerClose(pRecSink, 0);
                                    tm2 = CDX_GetSysTimeUsMonotonic();
                                    alogd("muxerId[%d] recRender_MuxerClose[%lld]MB itl[%lld]ms", pRecSink->mMuxerId, pRecSink->mFileSizeBytes/(1024*1024), (tm2-tm1)/1000);
                                }
                                for(int i=0; i<MAX_VIDEO_TRACK_COUNT; i++)
                                {
                                    pRecSink->mPrefetchFlag[i] = FALSE;
                                }
                                pRecSink->mPrefetchFlagAudio = FALSE;
                            }
                            else
                            {
                                aloge("fatal error! muxerId[%d]muxerMode[%d] wrong status[%d]", pRecSink->mMuxerId, pRecSink->nMuxerMode, pRecSink->mStatus);
                            }
                            pRecSink->mStatus = COMP_StateLoaded;
                            pRecSink->mpCallbacks->EventHandler(
                                    pRecSink,
                                    pRecSink->mpAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pRecSink->mStatus,
                                    NULL);
                            if(cmd_msg.pReply)
                            {
                                cmd_msg.pReply->ReplyResult = (int)SUCCESS;
                                cdx_sem_up(&cmd_msg.pReply->ReplySem);
                            }
                            else
                            {
                                aloge("fatal error! must need pReply to reply!");
                            }
                            alogv("RecRender set state LOADED ok");
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            if (pRecSink->mStatus == COMP_StateLoaded || pRecSink->mStatus == COMP_StatePause)
                            {
                                pRecSink->mStatus = COMP_StateExecuting;
                                pRecSink->mpCallbacks->EventHandler(
                                        pRecSink,
                                        pRecSink->mpAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pRecSink->mStatus,
                                        NULL);
                            }
                            else
                            {
                                aloge("fatal error! Set wrong status[%d]->Executing", pRecSink->mStatus);
                                pRecSink->mpCallbacks->EventHandler(
                                        pRecSink,
                                        pRecSink->mpAppData,
                                        COMP_EventError,
                                        ERR_MUX_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            if(cmd_msg.pReply)
                            {
                                cmd_msg.pReply->ReplyResult = (int)SUCCESS;
                                cdx_sem_up(&cmd_msg.pReply->ReplySem);
                            }
                            else
                            {
                                aloge("fatal error! must need pReply to reply!");
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            // Transition can only happen from idle or executing state
                            if (pRecSink->mStatus == COMP_StateLoaded || pRecSink->mStatus == COMP_StateExecuting)
                            {
                                pRecSink->mStatus = COMP_StatePause;
                                pRecSink->mpCallbacks->EventHandler(
                                        pRecSink,
                                        pRecSink->mpAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pRecSink->mStatus,
                                        NULL);
                            }
                            else
                            {
                                aloge("fatal error! Set wrong status[%d]->Pause", pRecSink->mStatus);
                                pRecSink->mpCallbacks->EventHandler(
                                        pRecSink,
                                        pRecSink->mpAppData,
                                        COMP_EventError,
                                        ERR_MUX_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        default:
                        {
                            aloge("fatal error! wrong desStatus[%d], current status[%d]", (COMP_STATETYPE)cmddata, pRecSink->mStatus);
                            break;
                        }
                    }
                }
            }
            else if (cmd == Stop)
            {
                // Kill thread
                goto EXIT;
            }
            else if (SwitchFileNormal == cmd)
            {
                SwitchFileNormalInfo *pInfo = (SwitchFileNormalInfo*)cmd_msg.mpData;
                if(pInfo->mMuxerId != pRecSink->mMuxerId)
                {
                    aloge("fatal error! muxerId[%d]!=[%d]", pInfo->mMuxerId, pRecSink->mMuxerId);
                }
                //-----------------------------------------------------------------------------
                pthread_mutex_lock(&pRecSink->mutex_reset_writer_lock);
                int nFd = pInfo->mFd;
                int nFallocateLen = pInfo->mnFallocateLen;
                if(nFd < 0)
                {
                    aloge("Be careful! fd[%d] < 0, app maybe want to bypass frames in this muxerId[%d] next file", nFd, pRecSink->mMuxerId);
                    pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                    //return ERR_MUX_ILLEGAL_PARAM;
                }
                else
                {
                    if(pRecSink->nSwitchFd >= 0)
                    {
                        alogd("nSwitchFd[%d] already exist, directly close it! maybe impact happen during new fd is setting.", pRecSink->nSwitchFd);
                        close(pRecSink->nSwitchFd);
                        pRecSink->nSwitchFd = -1;
                        pRecSink->nSwitchFdFallocateSize = 0;
                    }
                //    if(pThiz->mSwitchFilePath)
                //    {
                //        alogd("switchFilePath[%s] already exist, maybe impact happen during new fd is setting.", pThiz->mSwitchFilePath);
                //        free(pThiz->mSwitchFilePath);
                //        pThiz->mSwitchFilePath = NULL;
                //        pThiz->nSwitchFdFallocateSize = 0;
                //    }
                    pRecSink->nSwitchFd = dup(nFd);
                    if(pRecSink->nSwitchFd < 0)
                    {
                        aloge("fatal error! dup fail:[%d]->[%d],(%s)", nFd, pRecSink->nSwitchFd, strerror(errno));
                        system("lsof");
                    }
                    //pThiz->nSwitchFd = dup2SeldomUsedFd(nFd);

                    pRecSink->nSwitchFdFallocateSize = nFallocateLen;
                    //pThiz->mSwitchFdImpactFlag = nIsImpact;
                    alogd("dup setfd[%d] to nSwitchFd[%d]", nFd, pRecSink->nSwitchFd);
                    if(TRUE == pRecSink->reset_fd_flag)
                    {
                        alogd("reset__fd_flag is already true, maybe impact happen during new fd is setting");
                    }
                    pRecSink->reset_fd_flag = TRUE;
                    pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                }
                //----------------------------------------------------------------------------
                pRecSink->mbSwitchFileNormalIncludeCache = pInfo->mbIncludeCache;
                if(pRecSink->rec_file != FILE_NORMAL)
                {
                    aloge("fatal error! Switch file normal, but rec_file=%d", pRecSink->rec_file);
                    goto PROCESS_MESSAGE;
                }
                ERRORTYPE switchRet = ERR_MUX_NOT_SUPPORT;
                if(FALSE == pRecSink->mbMuxerInit)
                {
                    alogd("switch file normal begin, not muxerInit, muxerId[%d], so close nOutputFd[%d]", pRecSink->mMuxerId, pRecSink->nOutputFd);
                    pRecSink->rec_file = FILE_NEED_SWITCH_TO_NORMAL;
                    if(pRecSink->nOutputFd>=0)
                    {
                        if(pRecSink->mPath)
                        {
                            aloge("fatal error! fd and path only use one! check code!");
                        }
                        close(pRecSink->nOutputFd);
                        pRecSink->nOutputFd = -1;
                        pRecSink->mpCallbacks->EventHandler(
                                pRecSink,
                                pRecSink->mpAppData,
                                COMP_EventRecordDone,
                                pRecSink->mMuxerId,
                                0,
                                NULL);
                    }
                    if(pRecSink->mPath)
                    {
                        free(pRecSink->mPath);
                        pRecSink->mPath = NULL;
                    }
                    pthread_mutex_lock(&pRecSink->mutex_reset_writer_lock);
                    if(pRecSink->reset_fd_flag)
                    {
                        if(pRecSink->nSwitchFd < 0 && NULL==pRecSink->mSwitchFilePath)
                        {
                            aloge("fatal error! reset__fd_flag is true but switchFd[%d]<0, check code!", pRecSink->nSwitchFd);
                            pRecSink->reset_fd_flag = FALSE;
                        }
                    }
                    pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                }
                else
                {
                    alogd("switch file normal begin, muxerId[%d]", pRecSink->mMuxerId);
                    pRecSink->rec_file = FILE_NEED_SWITCH_TO_NORMAL;
                }
                switchRet = SUCCESS;
                cmd_msg.pReply->ReplyResult = (int)switchRet;
                cdx_sem_up(&cmd_msg.pReply->ReplySem);
                pRecSink->mpCallbacks->EventHandler(
                        pRecSink,
                        pRecSink->mpAppData,
                        COMP_EventCmdComplete,
                        SwitchFileNormal,
                        pRecSink->mMuxerId,
                        (void*)switchRet);
                free(cmd_msg.mpData);
                cmd_msg.mpData = NULL;
            }
            else if(RecSink_SwitchFd == cmd)
            {
                ERRORTYPE eSwitchFdRet = SUCCESS;
                pthread_mutex_lock(&pRecSink->mutex_reset_writer_lock);
                /*if (nIsImpact == 1 && (pThiz->rec_file == FILE_IMPACT_RECDRDING || pThiz->rec_file == FILE_NEED_SWITCH_TO_IMPACT))
                {
                    alogd("impact file is recording, don't accept new impact Fd[%d].", nFd);
                    pthread_mutex_unlock(&pThiz->mutex_reset_writer_lock);
                    return SUCCESS;
                }*/
                if(FILE_NEED_SWITCH_TO_NORMAL == pRecSink->rec_file)
                {
                    alogw("Be careful! muxerId[%d] is in switch_to_normal, refuse switch fd", pRecSink->mMuxerId);
                    pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                    eSwitchFdRet = ERR_MUX_NOT_PERM;
                    goto _switchFdExit;
                }
                int nFd = cmddata;
                int nFallocateLen = cmd_msg.para1;
                if(nFd < 0)
                {
                    aloge("Be careful! fd[%d] < 0, app maybe want to bypass frames in this muxerId[%d] next file", nFd, pRecSink->mMuxerId);
                    pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                    //return ERR_MUX_ILLEGAL_PARAM;
                    goto _switchFdExit;
                }
                if(pRecSink->nSwitchFd >= 0)
                {
                    alogd("nSwitchFd[%d] already exist, directly close it! maybe impact happen during new fd is setting.", pRecSink->nSwitchFd);
                    close(pRecSink->nSwitchFd);
                    pRecSink->nSwitchFd = -1;
                    pRecSink->nSwitchFdFallocateSize = 0;
                }
            //    if(pThiz->mSwitchFilePath)
            //    {
            //        alogd("switchFilePath[%s] already exist, maybe impact happen during new fd is setting.", pThiz->mSwitchFilePath);
            //        free(pThiz->mSwitchFilePath);
            //        pThiz->mSwitchFilePath = NULL;
            //        pThiz->nSwitchFdFallocateSize = 0;
            //    }
                if(nFd >= 0)
                {
                    pRecSink->nSwitchFd = dup(nFd);
                    if(pRecSink->nSwitchFd < 0)
                    {
                        aloge("fatal error! dup fail:[%d]->[%d],(%s)", nFd, pRecSink->nSwitchFd, strerror(errno));
                        system("lsof");
                    }
                    //pThiz->nSwitchFd = dup2SeldomUsedFd(nFd);
                }

                pRecSink->nSwitchFdFallocateSize = nFallocateLen;
                //pThiz->mSwitchFdImpactFlag = nIsImpact;
                alogd("dup setfd[%d] to nSwitchFd[%d]", nFd, pRecSink->nSwitchFd);
                if(TRUE == pRecSink->reset_fd_flag)
                {
                    alogd("reset__fd_flag is already true, maybe impact happen during new fd is setting");
                }
                pRecSink->reset_fd_flag = TRUE;
                pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
            _switchFdExit:
                if(cmd_msg.pReply)
                {
                    cmd_msg.pReply->ReplyResult = (int)eSwitchFdRet;
                    cdx_sem_up(&cmd_msg.pReply->ReplySem);
                }
                else
                {
                    aloge("fatal error! must need pReply to reply!");
                }
            }
            else if (RecSink_InputPacketAvailable == cmd)
            {
                alogv("input packet available");
                //pRecSink->mNoInputPacketFlag = FALSE;
            }

            //precede to process message
            goto PROCESS_MESSAGE;
        }
        if (pRecSink->mStatus == COMP_StateExecuting)
        {
            //find mediainfo
            RecSinkPacket   *pRSPacket = RecSinkGetRSPacket(pRecSink);
            if(pRSPacket)
            {
                pthread_mutex_lock(&pRecSink->mutex_reset_writer_lock);

				/* if(pRecSink->need_to_force_i_frm == 1)
				{
					aloge("frc_i_frm_p_ck:%d-%d-%d",pRecSink->mMuxerGrpId,pRSPacket->mStreamType,pRSPacket->mFlags);
				} */
                int need_switch_audio = 0;      // audio duration meet the maxduration
                BOOL need_switch_video = FALSE;
                need_switch_video = RecSinkIfNeedSwitchFile(pRecSink,&need_switch_audio);

                if(TRUE == need_switch_video)   // time duration meet setting.case1: key frm,with audio;case2:key frm,without audio;case3:not key frm
                {
                    if(pRSPacket->mStreamType==CODEC_TYPE_VIDEO)
                    {
                        if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                        {
                            aloge("fatal error! wrong streamId[%d]", pRSPacket->mStreamId);
                        }
                        //(1)if video need force I frame?
                        if(!(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
                        {
                            if(FALSE == pRecSink->mPrefetchFlag[pRSPacket->mStreamId])
                            {
                                pRecSink->need_to_force_i_frm[pRSPacket->mStreamId] = 1;
                                if(0 == pRecSink->forced_i_frm[pRSPacket->mStreamId])
                                {
                                    if(pRecSink->mRefVideoStreamIndex == pRSPacket->mStreamId)
                                    {
                                        alogd("rec_sink_to_force_i_frm_v:[%d-%d]-%d-%d-%d-%lld-%lld-%lld--%lld-%lld-%lld-%lld-%d-%d-%d-%d-%d-%d",
                                            pRecSink->mMuxerGrpId,
                                            pRSPacket->mStreamId,
                                            pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].nWidth,
                                            pRecSink->mDuration[pRecSink->mRefVideoStreamIndex],pRecSink->mDurationAudio,pRecSink->mLoopDuration,pRecSink->mLoopDurationAudio,
                                            pRecSink->mCurFileEndTm,
                                            pRecSink->mBasePts[pRecSink->mRefVideoStreamIndex],pRSPacket->mPts,pRecSink->mBasePts[pRecSink->mpMediaInf->mVideoInfoValidNum],
                                            pRecSink->mOrigBasePts[pRSPacket->mStreamId],pRecSink->mVideoFrameCounter,pRecSink->mAudioFrmCnt,
                                            pRSPacket->mStreamType,pRSPacket->mFlags,
                                            pRecSink->video_frm_cnt,pRecSink->audio_frm_cnt);
                                    }
                                    else
                                    {
                                        alogd("rec_sink_to_force_i_frm_v:[%d-%d]-%d-%d--%lld-%lld-%lld-%d-%d",
                                            pRecSink->mMuxerGrpId,
                                            pRSPacket->mStreamId,
                                            pRecSink->mpMediaInf->mMediaVideoInfo[pRSPacket->mStreamId].nWidth,
                                            pRecSink->mDuration[pRSPacket->mStreamId],
                                            pRecSink->mBasePts[pRSPacket->mStreamId],pRSPacket->mPts,
                                            pRecSink->mOrigBasePts[pRSPacket->mStreamId],
                                            pRSPacket->mStreamType,pRSPacket->mFlags);
                                    }
                                    RecSinkToForceIFrame(pRecSink, pRSPacket->mStreamId);
                                    pRecSink->forced_i_frm[pRSPacket->mStreamId] = 1;
                                }
                            }
                        }
                        //(2)if meet KeyFrame, begin to prefetch.
                        else
                        {
                            if(!pRecSink->mPrefetchFlag[pRSPacket->mStreamId])
                            {
                                if(pRecSink->mRecordMode & RECORDER_MODE_AUDIO)
                                {
                                    int videoStreamIndex = pRSPacket->mStreamId;
                                    int audioStreamIndex = pRecSink->mpMediaInf->mVideoInfoValidNum;
                                    int64_t nVideoTotalTmUs = pRecSink->mPrevPts[videoStreamIndex]+pRecSink->mBasePts[videoStreamIndex];
                                    int64_t nAudioTotalTmUs = pRecSink->mPrevPts[audioStreamIndex]+pRecSink->mBasePts[audioStreamIndex];
                                    alogd("avsync_ch_v:[%d-%d-%d]-%d-%lld-%lld-%d-%d", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRSPacket->mStreamId,
                                            pRecSink->mpMediaInf->mMediaVideoInfo[videoStreamIndex].nWidth,
                                            nAudioTotalTmUs,
                                            nVideoTotalTmUs,
                                            pRSPacket->mStreamType, pRSPacket->mFlags);
                                }
                                pRecSink->mPrefetchFlag[pRSPacket->mStreamId] = TRUE;   // to cache video pkt in order to push more audio packet
                            }
                        }
                    }
                    BOOL bGrant = RecSinkGrantSwitchFile(pRecSink, pRSPacket);
                    if(bGrant)
                    {
                        if(pRecSink->nOutputFd >= 0 || pRecSink->mPath)
                        {
                            list_add_tail(&pRSPacket->mList, &pRecSink->mPrefetchRSPacketList);
                            alogw("avsync_rc_swfv:[%d-%d-%d]-%d-%d-%d-%lld-%lld--%lld-%lld-%lld-%lld-%d-%d-%d-%d-%d",
                                    pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mRefVideoStreamIndex,
                                    pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].nWidth,
                                    pRecSink->mDuration[pRecSink->mRefVideoStreamIndex],pRecSink->mDurationAudio,pRecSink->mLoopDuration,pRecSink->mCurFileEndTm,
                                    pRecSink->mBasePts[pRecSink->mRefVideoStreamIndex],pRSPacket->mPts,pRecSink->mBasePts[pRecSink->mpMediaInf->mVideoInfoValidNum],
                                    pRecSink->mOrigBasePts[pRecSink->mRefVideoStreamIndex],pRecSink->mVideoFrameCounter,pRecSink->mAudioFrmCnt,
                                    pRSPacket->mStreamType,pRecSink->video_frm_cnt,pRecSink->audio_frm_cnt);
                            pRSPacket = NULL;
                            RecSinkSwitchFile(pRecSink, 0);
                            pRSPacket = RecSinkGetRSPacket(pRecSink);
                            if(NULL == pRSPacket)
                            {
                                aloge("fatal error! check muxerId[%d]!", pRecSink->mMuxerId);
                            }
                            else
                            {
                                if(pRecSink->mRecordMode & RECORDER_MODE_VIDEO)
                                {
                                    if(!(pRSPacket->mStreamType==CODEC_TYPE_VIDEO && pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
                                    {
                                        aloge("fatal error! next file first frame is not key video frame!");
                                    }
                                    else
                                    {
                                        if(pRSPacket->mStreamId != 0)
                                        {
                                            alogd("Be careful! next file first frame stream=%d, !=0!", pRSPacket->mStreamId);
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            if(FALSE == pRecSink->nCallbackOutFlag)
                            {
                                aloge("fatal error! this muxerId[%d] has not fd, should be callback mode[%d], so don't switch file!",
                                    pRecSink->mMuxerId, pRecSink->nCallbackOutFlag);
                            }
                        }
                    }
                }
                else if(need_switch_audio)
                {
                    if(pRSPacket->mStreamType==CODEC_TYPE_AUDIO)
                    {
                        if(!pRecSink->mPrefetchFlagAudio)
                        {
                            if(!(pRecSink->mRecordMode & RECORDER_MODE_AUDIO))
                            {
                                aloge("fatal error! recorder no audio?");
                            }
                            int audioStreamIndex = pRecSink->mpMediaInf->mVideoInfoValidNum;
                            int64_t nVideoTotalTmUs = pRecSink->mPrevPts[pRecSink->mRefVideoStreamIndex]+pRecSink->mBasePts[pRecSink->mRefVideoStreamIndex];
                            int64_t nAudioTotalTmUs = pRecSink->mPrevPts[audioStreamIndex]+pRecSink->mBasePts[audioStreamIndex];
                            alogd("avsync_ch_a:[%d-%d-%d]-%d-%lld-%lld-%d-%d", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mRefVideoStreamIndex,
                                            pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].nWidth,
                                            nAudioTotalTmUs,
                                            nVideoTotalTmUs,
                                            pRSPacket->mStreamType, pRSPacket->mFlags);
                            pRecSink->mPrefetchFlagAudio = TRUE; // to cache audio pkt in order to push more video packet
                        }
                    }
                    BOOL bGrant = RecSinkGrantSwitchFileAudio(pRecSink, pRSPacket);
                    if(TRUE == bGrant)  // grant to switch file
                    {
                        if(pRecSink->nOutputFd >= 0 || pRecSink->mPath)
                        {
                            list_add_tail(&pRSPacket->mList, &pRecSink->mPrefetchRSPacketList);
                            alogw("avsync_rc_swfa:[%d-%d-%d]-%d-%d-%d-%lld-%lld-%lld--%lld-%lld-%lld-%lld-%d-%d-%d-%d-%d-%d",
                                pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mRefVideoStreamIndex,
                                pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].nWidth,
                                pRecSink->mDuration[pRecSink->mRefVideoStreamIndex],pRecSink->mDurationAudio,
								pRecSink->mLoopDuration,pRecSink->mLoopDurationAudio,pRecSink->mCurFileEndTm,
                                pRecSink->mBasePts[pRecSink->mRefVideoStreamIndex],pRSPacket->mPts,pRecSink->mBasePts[pRecSink->mpMediaInf->mVideoInfoValidNum],
                                pRecSink->mOrigBasePts[pRecSink->mRefVideoStreamIndex],pRecSink->mVideoFrameCounter,pRecSink->mAudioFrmCnt,
								pRSPacket->mStreamType,pRSPacket->mFlags,pRecSink->video_frm_cnt,pRecSink->audio_frm_cnt);

                            pRSPacket = NULL;
                            RecSinkSwitchFile(pRecSink, 0);
                            pRSPacket = RecSinkGetRSPacket(pRecSink);
                            if(NULL == pRSPacket)
                            {
                                aloge("fatal error! check muxerId[%d]!", pRecSink->mMuxerId);
                            }
                        }
                        else
                        {
                            if(FALSE == pRecSink->nCallbackOutFlag)
                            {
                                aloge("fatal error! this muxerId[%d] has not fd, should be callback mode[%d], so don't switch file!",
                                    pRecSink->mMuxerId, pRecSink->nCallbackOutFlag);
                            }
                        }
                    }
                    else        // not grant to switch file now
                    {
                        if(TRUE == pRecSink->bTimeMeetAudio) //time meet,but not all videos meet key frame
                        {
                            if(pRSPacket->mStreamType==CODEC_TYPE_VIDEO)
                            {
                                if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                                {
                                    aloge("fatal error! wrong streamId[%d]", pRSPacket->mStreamId);
                                }
                                //(1)if video need force I frame?
                                if(!(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
                                {
                                    if(FALSE == pRecSink->mPrefetchFlag[pRSPacket->mStreamId])
                                    {
                                        pRecSink->need_to_force_i_frm[pRSPacket->mStreamId] = 1;
                                        if(0 == pRecSink->forced_i_frm[pRSPacket->mStreamId])
                                        {
                                            alogd("rec_sink_to_force_i_frm_a:[%d-%d-%d]-%d-%d-%d-%lld-%lld-%lld--%lld-%lld-%lld-%lld-%d-%d-%d-%d-%d-%d",
                                                pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRSPacket->mStreamId,
                                                pRecSink->mpMediaInf->mMediaVideoInfo[pRSPacket->mStreamId].nWidth,
                                                pRecSink->mDuration[pRSPacket->mStreamId], pRecSink->mDurationAudio,
                								pRecSink->mLoopDuration, pRecSink->mLoopDurationAudio, pRecSink->mCurFileEndTm,
                                                pRecSink->mBasePts[pRSPacket->mStreamId], pRSPacket->mPts, pRecSink->mBasePts[pRecSink->mpMediaInf->mVideoInfoValidNum],
                                                pRecSink->mOrigBasePts[pRSPacket->mStreamId],
                								pRecSink->mVideoFrameCounter, pRecSink->mAudioFrmCnt,
                                                pRSPacket->mStreamType, pRSPacket->mFlags,
                								pRecSink->video_frm_cnt, pRecSink->audio_frm_cnt);
                                            RecSinkToForceIFrame(pRecSink, pRSPacket->mStreamId);
                                            pRecSink->forced_i_frm[pRSPacket->mStreamId] = 1;
                                        }
                                    }
                                }
                                //(2)if meet KeyFrame, begin to prefetch.
                                else
                                {
                                    if(!pRecSink->mPrefetchFlag[pRSPacket->mStreamId])
                                    {
                                        pRecSink->mPrefetchFlag[pRSPacket->mStreamId] = TRUE;   // can cache video pkt
                                    }
                                }

                                //if video0 need continue to write, then calculate video0's write packet number and pts.
                                if((pRecSink->mRefVideoStreamIndex == pRSPacket->mStreamId) && (FALSE == pRecSink->mPrefetchFlag[pRecSink->mRefVideoStreamIndex]))
                                {
                                    pRecSink->mVideoFrmCntWriteMore++;
                                    alogd("avsync_cal:[%d-%d-%d]-%d-%lld-%lld-%d",
    										pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mRefVideoStreamIndex,
    										pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].nWidth,
    										pRSPacket->mPts, pRecSink->mVideoPtsWriteMoreSt, pRecSink->mVideoFrmCntWriteMore);
                                    if(-1 == pRecSink->mVideoPtsWriteMoreSt)    // to record the pts of video frame write more
                                    {
                                        pRecSink->mVideoPtsWriteMoreSt = pRSPacket->mPts;
                                        pRecSink->mVideoPtsWriteMoreEnd = pRSPacket->mPts;
                                    }
                                    else
                                    {
                                        pRecSink->mVideoPtsWriteMoreEnd = pRSPacket->mPts;
                                    }
                                }
                            }
                        }
                    }
                }

                if(pRecSink->mbMuxerInit == FALSE
                    && (pRecSink->nCallbackOutFlag==TRUE || (pRecSink->nOutputFd>=0 || pRecSink->mPath!=NULL) || pRecSink->reset_fd_flag==TRUE))
                {
                    BOOL bGrant = TRUE;
                    if(pRecSink->mRecordMode & RECORDER_MODE_VIDEO)
                    {
                        if(pRSPacket->mStreamType==CODEC_TYPE_VIDEO && pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME)
                        {
                            bGrant = TRUE;
                        }
                        else
                        {
                            alogd("Be careful! stream[%d-%d-%d] first packet[%d,0x%x] is not key frame! But still grant!", 
                                pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRSPacket->mStreamId, pRSPacket->mStreamType, pRSPacket->mFlags);
                            bGrant = TRUE;
                        }
                    }
                    //first packet will be video or not.
                    //first video frame may be not key frame.
                    //we will make first packet to be video, although to do this will discard audio frames.
//                    alogd("muxerId[%d]: first packet grant[%d]: stream_type[%d], flags[0x%x], pts[%lld]ms, videoSize[%dx%d]",
//                        pRecSink->mMuxerId, bGrant, pRSPacket->mStreamType, pRSPacket->mFlags, pRSPacket->mPts, pRecSink->mpMediaInf->nWidth, pRecSink->mpMediaInf->nHeight);
                    if(bGrant)
                    {
                        if(RecSinkMuxerInit(pRecSink, pRSPacket) != SUCCESS)
                        {
                            aloge("fatal error! muxerId[%d][%p]ValidMuxerInit Error!", pRecSink->mMuxerId, pRecSink);
                            pRecSink->mStatus = COMP_StateInvalid;   //OMX_StateIdle;
                            pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                            RecSinkReleaseRSPacket(pRecSink, pRSPacket);
                            goto PROCESS_MESSAGE;
                        }
                        pRecSink->mVideoPtsWriteMoreSt = -1;
                        if(pRecSink->nCallbackOutFlag == FALSE)
                        {
                            //pRecSink->reset_fd_flag = FALSE;
                            if (pRecSink->rec_file == FILE_IMPACT_RECDRDING)
                            {
                                alogd("Need set next fd immediately after switch to impactFile");
                                pRecSink->mpCallbacks->EventHandler(pRecSink, pRecSink->mpAppData, COMP_EventNeedNextFd, pRecSink->mMuxerId, 0, NULL);
                                pRecSink->need_set_next_fd = FALSE;
                            }
                            else
                            {
                                pRecSink->need_set_next_fd = TRUE;
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&pRecSink->mutex_reset_writer_lock);
                BOOL bReleasePacket = TRUE;
                if(pRecSink->mbMuxerInit)
                {
                    if(CODEC_TYPE_VIDEO == pRSPacket->mStreamType)
                    {
                        if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                        {
                            aloge("fatal error! streamId[%d] wrong!", pRSPacket->mStreamId);
                        }
                        if(pRecSink->mPrefetchFlag[pRSPacket->mStreamId])
                        {
                            list_add_tail(&pRSPacket->mList, &pRecSink->mPrefetchRSPacketList);
                            bReleasePacket = FALSE;
                        }
                        else
                        {
                            if(!pRecSink->key_frm_sent[pRSPacket->mStreamId])
                            {
                                if(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME)
                                {
                                    pRecSink->key_frm_sent[pRSPacket->mStreamId] = 1;
                                    RecSinkWriteRSPacket(pRecSink, pRSPacket);
                                }
                                else
                                {
                                    pRecSink->v_frm_drp_cnt[pRSPacket->mStreamId]++;
                                    alogd("stream[%d-%d-%d] rc_snk_drp_v_frm:%d", pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRSPacket->mStreamId, pRecSink->v_frm_drp_cnt[pRSPacket->mStreamId]);
                                    //even we drop the front video frames, but first video frame's pts must be set as basePts of
                                    //current stream, to guarantee av sync.
                                    RecSinkUpdateStreamBasePts(pRecSink, pRSPacket);
                                }
                            }
                            else
                            {
                                RecSinkWriteRSPacket(pRecSink, pRSPacket);
                            }
                        }
                    }
                    else if(CODEC_TYPE_AUDIO == pRSPacket->mStreamType)
                    {
                        if(pRecSink->mPrefetchFlagAudio)
                        {
                            list_add_tail(&pRSPacket->mList, &pRecSink->mPrefetchRSPacketList);
                            bReleasePacket = FALSE;
                        }
                        else
                        {
                            RecSinkWriteRSPacket(pRecSink, pRSPacket);
                        }
                    }
                    else
                    {
                        RecSinkWriteRSPacket(pRecSink, pRSPacket);
                    }
                }
                //release RSPacket
                if(bReleasePacket)
                {
                    RecSinkReleaseRSPacket(pRecSink, pRSPacket);
                }
RELEASE:
//                if(pRecSink->mCurMaxFileDuration > 0
//                    && pRecSink->need_set_next_fd == TRUE
//                    && (pRecSink->mLoopDuration + NOTIFY_NEEDNEXTFD_IN_ADVANCE) >= pRecSink->mCurFileEndTm)
                if(RecSinkIfNeedRequestNextFd(pRecSink))
                {
                    alogd("muxerId[%d-%d-%d] Need set next fd. SinkInfo[0x%x], videosize[%dx%d], vLoopDur[%lld]ms, aLoopDur[%lld]ms, curFileEndTm[%lld]ms",
                        pRecSink->mMuxerGrpId, pRecSink->mMuxerId, pRecSink->mRefVideoStreamIndex, pRecSink,
                        pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].nWidth,
                        pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].nHeight,
                        pRecSink->mLoopDuration, pRecSink->mLoopDurationAudio,
                        pRecSink->mCurFileEndTm);
                    pRecSink->need_set_next_fd = FALSE;
                    pRecSink->mpCallbacks->EventHandler(pRecSink, pRecSink->mpAppData, COMP_EventNeedNextFd, pRecSink->mMuxerId, 0, NULL);
                }
            }
            else
            {
                pthread_mutex_lock(&pRecSink->mRSPacketListMutex);
                pRecSink->mNoInputPacketFlag = TRUE;
                pthread_mutex_unlock(&pRecSink->mRSPacketListMutex);
                TMessage_WaitQueueNotEmpty(&pRecSink->mMsgQueue, 0);
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pRecSink->mMsgQueue, 0);
        }
    }
EXIT:
    //must process switchFd message! Because fd must be release!
    alogv("RecSinkThread stopped");
    return (void*) SUCCESS;
}

static ERRORTYPE RecSinkConfig(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN CdxOutputSinkInfo *pCdxSink)
{
    RecSink *pRecRenderSink = (RecSink*)hComponent;
    if(pRecRenderSink->mStatus == COMP_StateExecuting)
    {
        alogw("status already executing, cannot config again!");
        return SUCCESS;
    }
    if(pRecRenderSink->mStatus != COMP_StateLoaded)
    {
        aloge("fatal error! call in wrong status[%d]", pRecRenderSink->mStatus);
        return SUCCESS;
    }
    pRecRenderSink->mMuxerId = pCdxSink->mMuxerId;
    pRecRenderSink->mMuxerGrpId = pCdxSink->mMuxerGrpId;
    pRecRenderSink->nMuxerMode = pCdxSink->nMuxerMode;
    pRecRenderSink->b_add_repair_info_flag = pCdxSink->add_repair_info;
    pRecRenderSink->max_frms_tag_interval = pCdxSink->mMaxFrmsTagInterval;

    aloge("rcsk_id:%d-%d-%p",pCdxSink->mMuxerId,pCdxSink->mMuxerGrpId,pRecRenderSink);
    if(pRecRenderSink->nOutputFd >= 0)
    {
        aloge("fatal error! nOutputFd[%d]>=0", pRecRenderSink->nOutputFd);
        close(pRecRenderSink->nOutputFd);
        pRecRenderSink->nOutputFd = -1;
    }
    if(pRecRenderSink->mPath)
    {
        aloge("fatal error! mPath[%s] is not null", pRecRenderSink->mPath);
        free(pRecRenderSink->mPath);
        pRecRenderSink->mPath = NULL;
    }
    pRecRenderSink->nCallbackOutFlag = pCdxSink->nCallbackOutFlag;
    pRecRenderSink->bBufFromCacheFlag = pCdxSink->bBufFromCacheFlag;
    //pRecRenderSink->pOutputFile = NULL;
    if(pCdxSink->nOutputFd >= 0)
    {
        pRecRenderSink->nOutputFd = dup(pCdxSink->nOutputFd);
        //pRecRenderSink->nOutputFd = dup2SeldomUsedFd(pCdxSink->nOutputFd);
        alogd("dup fd[%d]->[%d]", pCdxSink->nOutputFd, pRecRenderSink->nOutputFd);
        pRecRenderSink->nFallocateLen = pCdxSink->nFallocateLen;
        //pRecRenderSink->reset_fd_flag = TRUE;
    }
    else
    {
        alogd("fd or path both not set");
    }
    message_t   msg;
    InitMessage(&msg);
    msg.command = SetState;
    msg.para0 = COMP_StateExecuting;
    msg.pReply = ConstructMessageReply();
    putMessageWithData(&pRecRenderSink->mMsgQueue, &msg);

    int ret;
    while(1)
    {
        ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
        if(ret != 0)
        {
            aloge("fatal error! RecSink wait Set State fail:0x%x", ret);
        }
        else
        {
            break;
        }
    }
    ERRORTYPE result = (ERRORTYPE)msg.pReply->ReplyResult;
    alogd("receive set state reply: 0x%x!", result);
    DestructMessageReply(msg.pReply);
    msg.pReply = NULL;
    return result;
}

static ERRORTYPE RecSinkSetValidStreamIds(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int nStreamCount, 
        PARAM_IN int nStreamIds[])
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mValidStreamCount = nStreamCount;
    if(nStreamCount > 0)
    {
        if(nStreamCount > MAX_TRACK_COUNT)
        {
            aloge("fatal error! array size is excceed! check code![%d>%d]", nStreamCount, MAX_TRACK_COUNT);
        }
        pThiz->mRefVideoStreamIndex = nStreamIds[0];
        for(int i=0; i<nStreamCount; i++)
        {
            pThiz->mValidStreamIds[i] = nStreamIds[i];
        }
    }
    else
    {
        pThiz->mRefVideoStreamIndex = 0;
    }
    return SUCCESS;
}

static ERRORTYPE RecSinkSetCallbacks(
        PARAM_IN COMP_HANDLETYPE hComponent,
		PARAM_IN RecSinkCallbackType* pCallbacks,
		PARAM_IN void* pAppData)
{
    RecSink *pThiz = (RecSink*)hComponent;
	ERRORTYPE eError = SUCCESS;

	pThiz->mpCallbacks = pCallbacks;
	pThiz->mpAppData = pAppData;

	return eError;
}

static ERRORTYPE RecSinkEmptyThisBuffer(
            PARAM_IN COMP_HANDLETYPE hComponent,
            PARAM_IN RecSinkPacket* pRSPacket)
{
    RecSink *pThiz = (RecSink*)hComponent;
    ERRORTYPE eError = SUCCESS;
    RecSinkPacket   *pEntryPacket;
    pthread_mutex_lock(&pThiz->mRSPacketListMutex);
    if(list_empty(&pThiz->mIdleRSPacketList))
    {
        alogw("idleRSPacketList are all used, malloc more!");
        if(SUCCESS!=RecSinkIncreaseIdleRSPacketList(pThiz))
        {
            pthread_mutex_unlock(&pThiz->mRSPacketListMutex);
            return ERR_MUX_NOMEM;
        }
    }
    pEntryPacket = list_first_entry(&pThiz->mIdleRSPacketList, RecSinkPacket, mList);
    RSSetRecSinkPacket(pEntryPacket, pRSPacket);
    //debug input pts.
    int videoStreamIndex = pRSPacket->mStreamId;
    int audioStreamIndex = pThiz->mpMediaInf->mVideoInfoValidNum;
    if(CODEC_TYPE_VIDEO == pEntryPacket->mStreamType)
    {
        pThiz->mDebugInputPts[videoStreamIndex] = pEntryPacket->mPts;
    }
    else if(CODEC_TYPE_AUDIO == pEntryPacket->mStreamType)
    {
        pThiz->mDebugInputPts[audioStreamIndex] = pEntryPacket->mPts;
    }
    list_move_tail(&pEntryPacket->mList, &pThiz->mValidRSPacketList);
    if(TRUE == pThiz->mNoInputPacketFlag)
    {
        message_t   msg;
        msg.command = RecSink_InputPacketAvailable;
        put_message(&pThiz->mMsgQueue, &msg);
        pThiz->mNoInputPacketFlag = FALSE;
    }
    pthread_mutex_unlock(&pThiz->mRSPacketListMutex);
    return eError;
}

static ERRORTYPE RecSinkSetRecordMode(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN RECORDER_MODE nMode)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mRecordMode = nMode;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetMediaInf(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN _media_file_inf_t *pMediaInfoList)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mpMediaInf = pMediaInfoList;
    //pThiz->mpMediaInf = pMediaInf;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetVencExtraData(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN struct list_head *pVencHeaderDataList)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mpVencHeaderDataList = pVencHeaderDataList;
    //pThiz->mpVencExtraData = pExtraData;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetThmPic(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN char * p_buff,
        PARAM_IN int size)
{
    RecSink *pThiz = (RecSink*)hComponent;
    char *tmp_thm_buff = NULL;
    THM_PIC thm_pic;
    THM_PIC *pThmPic = &thm_pic;
    pThmPic->p_thm_pic_addr = p_buff;
    pThmPic->thm_pic_size = size;

    if(pThmPic->thm_pic_size > 0)
    {
        tmp_thm_buff = malloc(pThmPic->thm_pic_size);
        if(NULL != tmp_thm_buff)
        {
            memcpy(tmp_thm_buff,pThmPic->p_thm_pic_addr,pThmPic->thm_pic_size);
            if(NULL != pThiz->rc_thm_pic.p_thm_pic_addr)
            {
                alogw("warning,last_thm_pic_may_be_not_free,a:0x%p",pThiz->rc_thm_pic.p_thm_pic_addr);
                free(pThiz->rc_thm_pic.p_thm_pic_addr);
                pThiz->rc_thm_pic.p_thm_pic_addr = NULL;
            }
            pThiz->rc_thm_pic.p_thm_pic_addr = tmp_thm_buff;
            pThiz->rc_thm_pic.thm_pic_size = pThmPic->thm_pic_size;
            pThiz->rc_thm_pic_ready = 1;
        }
        else
        {
            aloge("rc_set_thm_pic_buff_malloc_failed,s:%d",pThmPic->thm_pic_size);
            
            pThiz->rc_thm_pic.p_thm_pic_addr = NULL;
            pThiz->rc_thm_pic.thm_pic_size = 0;
            pThiz->rc_thm_pic_ready = 0;
        }
    }
    else
    {
        aloge("rc_set_thm_pic_size is 0 !");
    }
    return SUCCESS;
}

static ERRORTYPE RecSinkSetMaxFileDuration(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int64_t nDuration)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mMaxFileDuration = nDuration;

    pthread_mutex_lock(&pThiz->mutex_reset_writer_lock);
    if(pThiz->rec_file == FILE_NORMAL && pThiz->mbMuxerInit)
    {
        if(pThiz->mMaxFileDuration > pThiz->mCurMaxFileDuration)
        {
            pThiz->mCurFileEndTm += pThiz->mMaxFileDuration - pThiz->mCurMaxFileDuration;
            alogd("RecSinkSetMaxFileDuration muxid[%d] type[%d] oldDur:%lldms newDur:%lldms newFileEndTm:%lldms",
                pThiz->mMuxerId, pThiz->rec_file, pThiz->mCurMaxFileDuration, pThiz->mMaxFileDuration, pThiz->mCurFileEndTm);
            pThiz->mCurMaxFileDuration = pThiz->mMaxFileDuration;
        }
        else if(pThiz->mMaxFileDuration < pThiz->mCurMaxFileDuration)
        {
            pThiz->mCurFileEndTm -= (pThiz->mCurMaxFileDuration - pThiz->mMaxFileDuration);
            alogw("Be careful! muxid[%d] type[%d] oldDur:%lldms newDur:%lldms newFileEndTm:%lldms",
                pThiz->mMuxerId, pThiz->rec_file, pThiz->mCurMaxFileDuration, pThiz->mMaxFileDuration, pThiz->mCurFileEndTm);
        }
    }
    pthread_mutex_unlock(&pThiz->mutex_reset_writer_lock);

    return SUCCESS;
}

/*static ERRORTYPE RecSinkSetImpactFileDuration(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int64_t nDuration)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mImpactFileDuration = nDuration;
    return SUCCESS;
}*/

static ERRORTYPE RecSinkSetFileDurationPolicy(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN RecordFileDurationPolicy nPolicy)
{
    RecordFileDurationPolicy mFileDurationPolicy;
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mFileDurationPolicy = nPolicy;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetShutDownNow(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN BOOL bShutDownNowFlag)
{
    RecordFileDurationPolicy mFileDurationPolicy;
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mbShutDownNowFlag = bShutDownNowFlag;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetMaxFileSize(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int64_t nSize)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mMaxFileSizeBytes = nSize;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetFsWriteMode(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN FSWRITEMODE nFsWriteMode)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mFsWriteMode = nFsWriteMode;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetFsSimpleCacheSize(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN int nSize)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mFsSimpleCacheSize = nSize;
    return SUCCESS;
}

static ERRORTYPE RecSinkSetCallbackWriter(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN CdxRecorderWriterCallbackInfo *pCallbackWriter)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mpCallbackWriter = pCallbackWriter;
    return SUCCESS;
}

static ERRORTYPE RecSinkSwitchFd(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nFd, PARAM_IN int nFallocateLen, PARAM_IN int nIsImpact)
{
    RecSink *pThiz = (RecSink*)hComponent;
    message_t   msg;
    InitMessage(&msg);
    msg.command = RecSink_SwitchFd;
    msg.para0 = nFd;
    msg.para1 = nFallocateLen;
    msg.pReply = ConstructMessageReply();
    putMessageWithData(&pThiz->mMsgQueue, &msg);

    int ret;
    while(1)
    {
        ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
        if(ret != 0)
        {
            aloge("fatal error! wait switch fd fail[0x%x]", ret);
        }
        else
        {
            break;
        }
    }
    ERRORTYPE result = (ERRORTYPE)msg.pReply->ReplyResult;
    alogd("receive switch fd reply: 0x%x!", result);
    DestructMessageReply(msg.pReply);
    msg.pReply = NULL;
    return result;
}

static ERRORTYPE RecSinkSendCmdSwitchFile(PARAM_IN COMP_HANDLETYPE hComponent, BOOL nCacheFlag)
{
    aloge("fatal error! call RecSinkSendCmdSwitchFile?");
    return ERR_MUX_NOT_SUPPORT;
    #if 0
    ERRORTYPE eError;
    RecSink *pThiz = (RecSink*)hComponent;
    if(pThiz->rec_file == FILE_NORMAL)
    {
        eError = SUCCESS;
    }
    else
    {
        alogd("already impact recording, recFileState[%d]", pThiz->rec_file);
        eError = ERR_MUX_SAMESTATE;
    }
    message_t   msg;
    msg.command = SwitchFile;
    msg.para0 = nCacheFlag;
    put_message(&pThiz->mMsgQueue, &msg);
    return eError;
    #endif
}

static ERRORTYPE RecSinkSendCmdSwitchFileNormal(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN SwitchFileNormalInfo *pInfo)
{
    ERRORTYPE eError;
    RecSink *pThiz = (RecSink*)hComponent;
    if(pThiz->rec_file == FILE_NORMAL)
    {
        eError = SUCCESS;
    }
    else
    {
        aloge("fatal error! can't switch file normal in impact recording, recFileState[%d], muxerId[%d]", pThiz->rec_file, pThiz->mMuxerId);
        eError = ERR_MUX_NOT_PERM;
        return eError;
    }
    message_t   msg;
    InitMessage(&msg);
    msg.command = SwitchFileNormal;
    msg.mpData = pInfo;
    msg.mDataSize = sizeof(SwitchFileNormalInfo);
    msg.pReply = ConstructMessageReply();
    putMessageWithData(&pThiz->mMsgQueue, &msg);

    int ret;
    while(1)
    {
        ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
        if(ret != 0)
        {
            aloge("fatal error! wait switch file normal fail[0x%x]", ret);
        }
        else
        {
            break;
        }
    }
    eError = (ERRORTYPE)msg.pReply->ReplyResult;
    alogd("receive switch file normal reply: 0x%x!", eError);
    DestructMessageReply(msg.pReply);
    msg.pReply = NULL;
    return eError;
}

static ERRORTYPE RecSinkSetSdcardState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN BOOL bSdcardState)
{
    RecSink *pThiz = (RecSink*)hComponent;
    pThiz->mbSdCardState = bSdcardState;
    return SUCCESS;
}

static ERRORTYPE RecSinkReset(PARAM_IN COMP_HANDLETYPE hComponent)
{
    RecSink *pThiz = (RecSink*)hComponent;
    if(pThiz->mStatus == COMP_StateLoaded)
    {
        alogv("status already loaded.");
        return SUCCESS;
    }
    if(pThiz->mStatus != COMP_StateExecuting && pThiz->mStatus != COMP_StatePause)
    {
        aloge("fatal error! call in wrong status[%d]", pThiz->mStatus);
    }
    message_t   msg;
    InitMessage(&msg);
    msg.command = SetState;
    msg.para0 = COMP_StateLoaded;
    msg.pReply = ConstructMessageReply();
    putMessageWithData(&pThiz->mMsgQueue, &msg);

    int ret;
    while(1)
    {
        ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
        if(ret != 0)
        {
            aloge("fatal error! wait set state fail:0x%x", ret);
        }
        else
        {
            break;
        }
    }
    alogd("receive set state reply: 0x%x!", msg.pReply->ReplyResult);
    DestructMessageReply(msg.pReply);
    msg.pReply = NULL;

    if(pThiz->nOutputFd >= 0)
    {
        if(pThiz->mPath)
        {
            aloge("fatal error! fd[%d] and path[%s] all exist! check code!", pThiz->nOutputFd, pThiz->mPath);
        }
        aloge("maybe not muxerInit? nOutputFd[%d]>=0", pThiz->nOutputFd);
        close(pThiz->nOutputFd);
        pThiz->nOutputFd = -1;
    }
    if(pThiz->mPath)
    {
        aloge("maybe not muxerInit? path[%s]>=0", pThiz->mPath);
        free(pThiz->mPath);
        pThiz->mPath = NULL;
    }
    pthread_mutex_lock(&pThiz->mutex_reset_writer_lock);
    if(pThiz->nSwitchFd >= 0)
    {
        if(pThiz->mSwitchFilePath)
        {
            aloge("fatal error! fd[%d] and path[%s] all exist! check code!", pThiz->nSwitchFd, pThiz->mSwitchFilePath);
        }
        close(pThiz->nSwitchFd);
        pThiz->nSwitchFd = -1;
        pThiz->reset_fd_flag = FALSE;
    }
    if(pThiz->mSwitchFilePath)
    {
        free(pThiz->mSwitchFilePath);
        pThiz->mSwitchFilePath = NULL;
        pThiz->reset_fd_flag = FALSE;
    }
    pthread_mutex_unlock(&pThiz->mutex_reset_writer_lock);
    RecSinkResetSomeMembers(pThiz);
    return SUCCESS;
}

ERRORTYPE RecSinkInit(PARAM_IN RecSink *pThiz)
{
    int err;
    ERRORTYPE   eError = SUCCESS;
    pThiz->nOutputFd = -1;
    pThiz->nSwitchFd = -1;
    pThiz->mPath = NULL;
    pThiz->mSwitchFilePath = NULL;
    pThiz->mStatus = COMP_StateLoaded;
    RecSinkResetSomeMembers(pThiz);
    if(0!=pthread_mutex_init(&pThiz->mutex_reset_writer_lock, NULL))
    {
        aloge("fatal error! pthread_mutex init fail");
        return ERR_MUX_NOMEM;
    }
    pThiz->ConfigByCdxSink = RecSinkConfig;
    pThiz->SetValidStreamIds = RecSinkSetValidStreamIds;
    pThiz->SetCallbacks = RecSinkSetCallbacks;
    pThiz->EmptyThisBuffer = RecSinkEmptyThisBuffer;
    pThiz->SetRecordMode = RecSinkSetRecordMode;
    pThiz->SetMediaInf = RecSinkSetMediaInf;
    pThiz->SetVencExtraData = RecSinkSetVencExtraData;
    pThiz->SetThmPic = RecSinkSetThmPic;

    pThiz->SetMaxFileDuration = RecSinkSetMaxFileDuration;
    //pThiz->SetImpactFileDuration = RecSinkSetImpactFileDuration;
    pThiz->SetFileDurationPolicy = RecSinkSetFileDurationPolicy;
    pThiz->SetMaxFileSize = RecSinkSetMaxFileSize;
    pThiz->SetFsWriteMode = RecSinkSetFsWriteMode;
    pThiz->SetFsSimpleCacheSize = RecSinkSetFsSimpleCacheSize;
    pThiz->SetCallbackWriter = RecSinkSetCallbackWriter;
    pThiz->SwitchFd = RecSinkSwitchFd;
    pThiz->SendCmdSwitchFile = RecSinkSendCmdSwitchFile;
    pThiz->SendCmdSwitchFileNormal = RecSinkSendCmdSwitchFileNormal;
    pThiz->SetSdcardState = RecSinkSetSdcardState;
    pThiz->Reset = RecSinkReset;
    pThiz->SetShutDownNow = RecSinkSetShutDownNow;
   
    memset(&pThiz->rc_thm_pic,0,sizeof(THM_PIC));
    pThiz->rc_thm_pic_ready = 0;

    pThiz->video_frm_cnt = 0;
    pThiz->audio_frm_cnt = 0;
    if(message_create(&pThiz->mMsgQueue)<0)
    {
        aloge("message create fail!");
        eError = ERR_MUX_NOMEM;
        goto _err0;
	}
//	if(cdx_sem_init(&pThiz->mSemStateComplete, 0)<0)
//	{
//        aloge("cdx sem init fail!");
//        eError = ERR_MUX_NOMEM;
//        goto _err1;
//	}
//    if(cdx_sem_init(&pThiz->mSemSwitchFdDone, 0)<0)
//	{
//        aloge("cdx sem init fail!");
//        eError = ERR_MUX_NOMEM;
//        goto _err1;
//	}
//    if(0 != cdx_sem_init(&pThiz->mSemSwitchFileNormalDone, 0))
//    {
//        aloge("fatal error! cdx sem init fail.");
//        eError = ERR_MUX_NOMEM;
//        goto _err1;
//    }
//    if(cdx_sem_init(&pThiz->mSemCmdComplete, 0)<0)
//	{
//        aloge("cdx sem init fail!");
//        eError = OMX_ErrorUndefined;
//        goto _err2;
//	}
    if(pthread_mutex_init(&pThiz->mRSPacketListMutex, NULL)!=0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_MUX_NOMEM;
        goto _err3;
    }
    //INIT_LIST_HEAD(&pThiz->mRSPacketBufList);
    INIT_LIST_HEAD(&pThiz->mPrefetchRSPacketList);
    INIT_LIST_HEAD(&pThiz->mValidRSPacketList);
    INIT_LIST_HEAD(&pThiz->mIdleRSPacketList);
    if(SUCCESS!=RecSinkIncreaseIdleRSPacketList(pThiz))
    {
        goto _err4;
    }

    err = pthread_create(&pThiz->mThreadId, NULL, RecSinkThread, (void*)pThiz);
	if (err || !pThiz->mThreadId)
    {
        aloge("pthread create fail!");
		eError = ERR_MUX_NOMEM;
		goto _err5;
	}
    return SUCCESS;
_err5:
    if(!list_empty(&pThiz->mIdleRSPacketList))
    {
        RecSinkPacket   *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pThiz->mIdleRSPacketList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    //INIT_LIST_HEAD(&pThiz->mRSPacketBufList);
_err4:
    pthread_mutex_destroy(&pThiz->mRSPacketListMutex);
_err3:
    //cdx_sem_deinit(&pThiz->mSemCmdComplete);
//_err2:
    //cdx_sem_deinit(&pThiz->mSemStateComplete);
    //cdx_sem_deinit(&pThiz->mSemSwitchFdDone);
    //cdx_sem_deinit(&pThiz->mSemSwitchFileNormalDone);
_err1:
    message_destroy(&pThiz->mMsgQueue);
_err0:
    pthread_mutex_destroy(&pThiz->mutex_reset_writer_lock);
    return eError;
}

ERRORTYPE RecSinkDestroy(RecSink *pThiz)
{
    int err;
    message_t   msg;
    msg.command = Stop;
    put_message(&pThiz->mMsgQueue, &msg);
	// Wait for thread to exit so we can get the status into "error"
	pthread_join(pThiz->mThreadId, (void*) &err);
    alogv("RecSink thread exit[%d]!", err);

    pthread_mutex_lock(&pThiz->mRSPacketListMutex);
    if(!list_empty(&pThiz->mPrefetchRSPacketList))
    {
        aloge("fatal error! prefetch RSPacket list is not empty! check code!");
    }
    if(!list_empty(&pThiz->mValidRSPacketList))
    {
        aloge("fatal error! valid RSPacket list is not empty! check code!");
    }
    if(!list_empty(&pThiz->mIdleRSPacketList))
    {
        RecSinkPacket   *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pThiz->mIdleRSPacketList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }

    pthread_mutex_unlock(&pThiz->mRSPacketListMutex);

    if(NULL != pThiz->rc_thm_pic.p_thm_pic_addr)
    {
        free(pThiz->rc_thm_pic.p_thm_pic_addr);
        pThiz->rc_thm_pic.p_thm_pic_addr = NULL;
    }
    //INIT_LIST_HEAD(&pThiz->mRSPacketBufList);
    INIT_LIST_HEAD(&pThiz->mPrefetchRSPacketList);
    INIT_LIST_HEAD(&pThiz->mValidRSPacketList);
    INIT_LIST_HEAD(&pThiz->mIdleRSPacketList);
    pthread_mutex_destroy(&pThiz->mutex_reset_writer_lock);
    message_destroy(&pThiz->mMsgQueue);
    //cdx_sem_deinit(&pThiz->mSemStateComplete);
    //cdx_sem_deinit(&pThiz->mSemSwitchFdDone);
    //cdx_sem_deinit(&pThiz->mSemSwitchFileNormalDone);
    //cdx_sem_deinit(&pThiz->mSemCmdComplete);
    pthread_mutex_destroy(&pThiz->mRSPacketListMutex);
    if(pThiz->mPath)
    {
        free(pThiz->mPath);
        pThiz->mPath = NULL;
    }
    if(pThiz->mSwitchFilePath)
    {
        free(pThiz->mSwitchFilePath);
        pThiz->mSwitchFilePath = NULL;
    }
    pThiz->mStatus = COMP_StateInvalid;
    return SUCCESS;
}
