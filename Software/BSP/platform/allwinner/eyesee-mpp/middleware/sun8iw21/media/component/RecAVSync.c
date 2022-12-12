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
//#define LOG_NDEBUG 0
#define LOG_TAG "RecAVSync"
#include <utils/plat_log.h>

#include <stdlib.h>

#include <cedarx_avs_counter.h>
#include <RecAVSync.h>
#include <SystemBase.h>

static ERRORTYPE RecAVSyncSetVideoPts(
        PARAM_IN struct RecAVSync *pSync,
        PARAM_IN int64_t nPts)
{
    if(-1 == pSync->mGetFirstVideoTime)
    {
        pSync->mGetFirstVideoTime = CDX_GetSysTimeUsMonotonic()/1000;
        alogd("get first video frame at [%lld]ms", pSync->mGetFirstVideoTime);
    }
    if(-1 == pSync->mVideoBasePts)
    {
        pSync->mVideoBasePts = nPts;
        alogd("get first video frame pts [%lld]ms", pSync->mVideoBasePts);
        return SUCCESS;
    }
    if(-1 == pSync->mGetFirstAudioTime)
    {
        return SUCCESS;
    }
    int64_t nPrevAdjustPts;
    if(-1 == pSync->mLastAdjustPts)
    {
        nPrevAdjustPts = pSync->mVideoBasePts;
    }
    else
    {
        nPrevAdjustPts = pSync->mLastAdjustPts;
    }
    if(nPts - nPrevAdjustPts >= AVS_ADJUST_PERIOD_MS)
    {
        //begin av sync
        int64_t nVideoDuration = nPts - pSync->mVideoBasePts;
        int64_t nAudioSize = pSync->mAudioDataSize;
        int64_t nAudioDuration = nAudioSize*1000/((pSync->mBitsPerSample/8)*pSync->mChannelNum*pSync->mSampleRate);
        if(-1 == pSync->mFirstDurationDiff)
        {
            pSync->mFirstDurationDiff = nVideoDuration-nAudioDuration;
            alogd("first durationDiff[%lld]", pSync->mFirstDurationDiff);
        }
        //int64_t nCacheDiff = (pSync->mGetFirstVideoTime - pSync->mGetFirstAudioTime);   //-:audio cache more; +:video cache more;
        int64_t nCacheDiff = -pSync->mFirstDurationDiff;   //-:front audio cache more; +: front video cache more;
        int64_t mediaTimediff = nVideoDuration - nAudioDuration + nCacheDiff;
        if(mediaTimediff > 200 || mediaTimediff < -200)
        {
            int adjust_ratio;
            adjust_ratio = mediaTimediff / (AVS_ADJUST_PERIOD_MS/100);
            adjust_ratio = adjust_ratio >  2 ? 2 : adjust_ratio;
            adjust_ratio = adjust_ratio < -2 ? -2 : adjust_ratio;

            pSync->mpAvsCounter->adjust(pSync->mpAvsCounter, adjust_ratio);
            alogd("----adjust ratio:%d, cachediff:%lld, video:%lld, audio:%lld, durationDiff:%lld, diff:%lld, diff-percent:%lld ", 
                adjust_ratio, nCacheDiff, nVideoDuration, nAudioDuration, nVideoDuration-nAudioDuration, mediaTimediff, mediaTimediff / (AVS_ADJUST_PERIOD_MS/100));
        }
        else
        {
            pSync->mpAvsCounter->adjust(pSync->mpAvsCounter, 0);
            alogv("----adjust ratio: 0, cachediff:%lld, video: %lld(ms), auido: %lld(ms), durationDiff:%lld, diff: %lld(ms) ",
                nCacheDiff, nVideoDuration, nAudioDuration, nVideoDuration-nAudioDuration, mediaTimediff);
        }
    
        pSync->mLastAdjustPts = nPts;
    }
    return SUCCESS;
}
static ERRORTYPE RecAVSyncSetAudioInfo(
        PARAM_IN struct RecAVSync *pSync,
        PARAM_IN int nSampleRate,
        PARAM_IN int nChannelNum,
        PARAM_IN int nBitsPerSample)
{
    pSync->mSampleRate = nSampleRate;
    pSync->mChannelNum = nChannelNum;
    pSync->mBitsPerSample = nBitsPerSample;
    return SUCCESS;
    
}
static ERRORTYPE RecAVSyncAddAudioDataLen(
        PARAM_IN struct RecAVSync *pSync,
        PARAM_IN int nLen,
        PARAM_IN int64_t nPts)
{
    if(-1 == pSync->mGetFirstAudioTime)
    {
        pSync->mGetFirstAudioTime = CDX_GetSysTimeUsMonotonic()/1000;
        alogd("get first audio frame at [%lld]ms", pSync->mGetFirstAudioTime);
    }
    if(-1 == pSync->mAudioBasePts)
    {
        pSync->mAudioBasePts = nPts;
        alogd("get first audio frame pts [%lld]ms", pSync->mAudioBasePts);
    }
    pSync->mAudioDataSize += nLen;
    return SUCCESS;
}

static ERRORTYPE RecAVSyncGetTimeDiff(
        PARAM_IN struct RecAVSync *pSync,
        PARAM_OUT int64_t* pDiff)
{
    pSync->mpAvsCounter->get_time_diff(pSync->mpAvsCounter, pDiff);
    return SUCCESS;
}

ERRORTYPE RecAVSyncInit(RecAVSync *pThiz)
{
    pThiz->mGetFirstVideoTime = -1;
    pThiz->mGetFirstAudioTime = -1;
    pThiz->mFirstDurationDiff = -1;
    pThiz->mVideoBasePts = -1;
    pThiz->mAudioBasePts = -1;
    pThiz->mAudioDataSize = 0;
    pThiz->mSampleRate = 0;
    pThiz->mChannelNum = 0;
    pThiz->mBitsPerSample = 0;
    pThiz->mLastAdjustPts = -1;
    // request avs counter
	pThiz->mpAvsCounter = cedarx_avs_counter_request();
	if (pThiz->mpAvsCounter == NULL) 
    {
        aloge("cedarx_avs_counter request failed!");
        return FAILURE;
	}
    pThiz->SetVideoPts = RecAVSyncSetVideoPts;
    pThiz->SetAudioInfo = RecAVSyncSetAudioInfo;
    pThiz->AddAudioDataLen = RecAVSyncAddAudioDataLen;
    pThiz->GetTimeDiff = RecAVSyncGetTimeDiff;

    pThiz->mpAvsCounter->start(pThiz->mpAvsCounter);
    return SUCCESS;
}
ERRORTYPE RecAVSyncDestroy(RecAVSync *pThiz)
{
    if(pThiz->mpAvsCounter)
    {
        cedarx_avs_counter_release(pThiz->mpAvsCounter);
        pThiz->mpAvsCounter = NULL;
    }
    return SUCCESS;
}
RecAVSync* RecAVSyncConstruct()
{
    RecAVSync *pThiz = (RecAVSync*)malloc(sizeof(RecAVSync));
    if(NULL == pThiz)
    {
        aloge("fatal error! malloc fail!");
        return NULL;
    }
    if(SUCCESS!=RecAVSyncInit(pThiz))
    {
        free(pThiz);
        pThiz = NULL;
    }
    return pThiz;
}
void RecAVSyncDestruct(RecAVSync *pThiz)
{
    RecAVSyncDestroy(pThiz);
    free(pThiz);
}

