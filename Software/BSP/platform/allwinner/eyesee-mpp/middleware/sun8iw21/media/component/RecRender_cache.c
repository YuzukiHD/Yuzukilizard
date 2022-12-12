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
#define LOG_TAG "RecRender_cache"
#include <utils/plat_log.h>

//ref platform headers
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
#include "mpi_venc.h"

//media internal common headers.
#include "media_common.h"
#include "mm_component.h"
#include "ComponentCommon.h"
#include "SystemBase.h"
#include "tmessage.h"
#include "tsemaphore.h"

#include <record_writer.h>
//#include <type_camera.h>
#include "RecRenderSink.h"
#include "RecRender_cache.h"
#include <cdx_list.h>


#define AVPACKET_CACHE_SIZE				(4*1024*1024)
#define AVPACKET_CACHE_ENLARGE_SIZE		(2*1024*1024)
#define AVPACKET_BEGIN_FLAG				0x55AA55AA
#define PACKET_NUM_IN_RSPACKETBUF       (1000)

typedef struct RPCMDuration
{
    int64_t mTotalDuration;
    int64_t mUsingPacketDuration;
    int64_t mReadyDuration;
}RPCMDuration;
/*
 * use video0 duration as total duraton.
 */
static ERRORTYPE RPCMGetDuration_l(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT RPCMDuration *pRPCMDuration)
{
    //int64_t nDuration = 0;  //unit:ms
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
	if (manager == NULL)
	{
		aloge("Buffer manager is NULL!");
		return ERR_MUX_NOMEM;
	}
    int64_t     nUsingBeginPts  = -1;   //unit:us
    int64_t     nUsingLastPts   = -1;   //unit:us
    int64_t     nReadyBeginPts  = -1;   //unit:us
    int64_t     nReadyLastPts   = -1;   //unit:us
    BOOL    nFindUsingBeginPtsFlag  = FALSE;
    BOOL    nFindReadyBeginPtsFlag  = FALSE;
    int64_t     nUsingDuration = 0; //unit:ms
    int64_t     nReadyDuration = 0; //unit:ms
    int64_t     nTotalDuration = 0; //unit:ms
    RecSinkPacket   *pPacketEntry;
    if(!list_empty(&manager->mUsingPacketList))
    {
        list_for_each_entry(pPacketEntry, &manager->mUsingPacketList, mList)
        {
            if ((CODEC_TYPE_VIDEO == pPacketEntry->mStreamType) && (manager->mRefStreamId == pPacketEntry->mStreamId))
            {
                nUsingBeginPts = pPacketEntry->mPts;
                break;
            }
        }
        list_for_each_entry_reverse(pPacketEntry, &manager->mUsingPacketList, mList)
        {
            if ((CODEC_TYPE_VIDEO == pPacketEntry->mStreamType) && (manager->mRefStreamId == pPacketEntry->mStreamId))
            {
                nUsingLastPts = pPacketEntry->mPts;
                break;
            }
        }
        if (nUsingBeginPts!=-1 && nUsingLastPts!=-1)
        {
            nFindUsingBeginPtsFlag = TRUE;
            if(nUsingBeginPts < 0)
            {
                //alogd("Be careful! usingPts[%lld],[%lld] may be < 0", nUsingBeginPts, nUsingLastPts);
            }
        }
    }
    if(!list_empty(&manager->mReadyPacketList))
    {
        list_for_each_entry(pPacketEntry, &manager->mReadyPacketList, mList)
        {
            if ((CODEC_TYPE_VIDEO == pPacketEntry->mStreamType) && (manager->mRefStreamId == pPacketEntry->mStreamId))
            {
                nReadyBeginPts = pPacketEntry->mPts;
                break;
            }
        }
        list_for_each_entry_reverse(pPacketEntry, &manager->mReadyPacketList, mList)
        {
            if ((CODEC_TYPE_VIDEO == pPacketEntry->mStreamType) && (manager->mRefStreamId == pPacketEntry->mStreamId))
            {
                nReadyLastPts = pPacketEntry->mPts;
                break;
            }

        }
        if (nReadyBeginPts!=-1 && nReadyLastPts!=-1)
        {
            nFindReadyBeginPtsFlag = TRUE;
            if(nReadyBeginPts < 0)
            {
                //alogd("Be careful! readyPts[%lld],[%lld] may be < 0", nReadyBeginPts, nReadyLastPts);
            }
        }
    }
//    alogd("flag[%d][%d] U[%lld][%lld] R[%lld][%lld]",
//        nFindUsingBeginPtsFlag, nFindReadyBeginPtsFlag, 
//        nUsingBeginPts, nUsingLastPts, nReadyBeginPts, nReadyLastPts);
    if(nFindUsingBeginPtsFlag)
    {
        nUsingDuration = (nUsingLastPts - nUsingBeginPts)/1000;
    }
    if(nFindReadyBeginPtsFlag)
    {
        nReadyDuration = (nReadyLastPts - nReadyBeginPts)/1000;
    }
    
    if(nFindUsingBeginPtsFlag && nFindReadyBeginPtsFlag)
    {
        nTotalDuration = (nReadyLastPts - nUsingBeginPts)/1000;
    }
    else
    {
        nTotalDuration = nUsingDuration + nReadyDuration;
    }
    pRPCMDuration->mTotalDuration = nTotalDuration;
    pRPCMDuration->mUsingPacketDuration = nUsingDuration;
    pRPCMDuration->mReadyDuration = nReadyDuration;
    alogv("U[%lld] R[%lld] T[%lld] C[%lld]", nUsingDuration, nReadyDuration, nTotalDuration, manager->mCacheTime);

	return SUCCESS;
}

static ERRORTYPE RPCMGetDuration(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT RPCMDuration *pRPCMDuration)
{
    ERRORTYPE eError;
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    pthread_mutex_lock(&manager->mPacketListLock);
    eError = RPCMGetDuration_l(hComponent, pRPCMDuration);
    pthread_mutex_unlock(&manager->mPacketListLock);
    return eError;
}

static ERRORTYPE RPCMSetRecSinkPacket(RsPacketCacheManager *pThiz, PARAM_IN RecSinkPacket *pDes, PARAM_IN RecSinkPacket *pSrc)
{
    pDes->mId           = pThiz->mPacketIdCounter++;
    pDes->mStreamType   = pSrc->mStreamType;
    pDes->mFlags        = pSrc->mFlags;
    pDes->mPts          = pSrc->mPts;
    pDes->mpData0       = NULL;
    pDes->mSize0        = 0;
    pDes->mpData1       = NULL;
    pDes->mSize1        = 0;
    pDes->mCurrQp       = pSrc->mCurrQp;
    pDes->mavQp         = pSrc->mavQp;
	pDes->mnGopIndex    = pSrc->mnGopIndex;
	pDes->mnFrameIndex  = pSrc->mnFrameIndex;
	pDes->mnTotalIndex  = pSrc->mnTotalIndex;
    pDes->mSourceType   = SOURCE_TYPE_CACHEMANAGER;
    pDes->mRefCnt       = 0;
    pDes->mStreamId     = pSrc->mStreamId;
    return SUCCESS;
}

static ERRORTYPE RPCMIncreaseIdlePacketList(RsPacketCacheManager *pThiz)
{
    ERRORTYPE eError = SUCCESS;
    RecSinkPacket   *pRSPacket;
    int i;
    for(i=0;i<PACKET_NUM_IN_RSPACKETBUF;i++)
    {
        pRSPacket = (RecSinkPacket*)malloc(sizeof(RecSinkPacket));
        if(NULL == pRSPacket)
        {
            aloge("fatal error! malloc fail");
            eError = ERR_MUX_NOMEM;
            break;
        }
        
        list_add_tail(&pRSPacket->mList, &pThiz->mIdlePacketList);
    }
    return SUCCESS;
}

static ERRORTYPE RPCMIncreaseDataBufList(RsPacketCacheManager *pThiz, int nSize)
{
    DynamicBuffer *pDB = malloc(sizeof(DynamicBuffer));
    //int ret = posix_memalign((void **)&pDB->mpBuffer, 4096, nSize);
    //if(ret!=0)
    //{
    //    aloge("fatal error! malloc size[%d] fail(%s)!", nSize, strerror(errno));
    //    goto _err0;
    //}
    pDB->mpBuffer = malloc(nSize);
    if(NULL == pDB->mpBuffer)
    {
        aloge("fatal error! malloc size[%d] fail(%s)!", nSize, strerror(errno));
        goto _err0;
    }
    pDB->mSize = nSize;
    list_add_tail(&pDB->mList, &pThiz->mDataBufList);
    pThiz->mBufSize += nSize;
    return SUCCESS;
_err0:
    free(pDB);
    return ERR_MUX_NOMEM;
}



static ERRORTYPE RPCMSetRefStream(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nRefStreamId)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    manager->mRefStreamId = nRefStreamId;
    return SUCCESS;
}

/*******************************************************************************
Function name: RPCMPushPacket
Description: 
    get one RecSinkPacket from idleList to readyList.
Parameters: 
    
Return: 
    
Time: 2015/2/26
*******************************************************************************/
static ERRORTYPE RPCMPushPacket(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN RecSinkPacket *pRSPacket)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    ERRORTYPE eError = SUCCESS;
    int nLeftBufSize;
    int nPacketDataSize;
    RecSinkPacket   *pPacketEntry;
    if (manager == NULL)
    {
        return ERR_MUX_NULL_PTR;
    }
    
    pthread_mutex_lock(&manager->mPacketListLock);
    //prepare idle RSPacket and dataBuffer.
    if(list_empty(&manager->mIdlePacketList))
    {
        alogw("idlePacketList are all used, malloc more!");
        if(SUCCESS!=RPCMIncreaseIdlePacketList(manager))
        {
            pthread_mutex_unlock(&manager->mPacketListLock);
            return ERR_MUX_NOMEM;
        }
    }
    nPacketDataSize = pRSPacket->mSize0 + pRSPacket->mSize1;
    nLeftBufSize = manager->mBufSize - manager->mUsedBufSize;
    if(nLeftBufSize < nPacketDataSize)
    {
        int nEnlargeSize = ((nPacketDataSize - nLeftBufSize + AVPACKET_CACHE_ENLARGE_SIZE - 1)/AVPACKET_CACHE_ENLARGE_SIZE)*AVPACKET_CACHE_ENLARGE_SIZE;
        if(SUCCESS!=RPCMIncreaseDataBufList(manager, nEnlargeSize))
        {
            pthread_mutex_unlock(&manager->mPacketListLock);
            return ERR_MUX_NOMEM;
        }
        nLeftBufSize = manager->mBufSize - manager->mUsedBufSize;
        alogd("increase dataBuf size to [%d]KB! packetSize[%d]KB, leftSize[%d]KB, stream[%d-%d], pts[%lld]us",
            manager->mBufSize/1024, nPacketDataSize/1024, nLeftBufSize/1024, manager->mRefStreamId, pRSPacket->mStreamId, pRSPacket->mPts);
    }
    // get one RecSinkPacket from mIdlePacketList
    pPacketEntry = list_first_entry(&manager->mIdlePacketList, RecSinkPacket, mList);
    RPCMSetRecSinkPacket(manager, pPacketEntry, pRSPacket);
    //if exception, discard packet.
    if(pRSPacket->mSize0 < 0 || pRSPacket->mSize1 < 0 || (pRSPacket->mSize0==0&&pRSPacket->mSize1>0))
    {
        aloge("fatal error! packetSize[%d][%d]<0, check code!", pRSPacket->mSize0, pRSPacket->mSize1);
        pthread_mutex_unlock(&manager->mPacketListLock);
        return ERR_MUX_ILLEGAL_PARAM;
    }
    //if empty packet, push to readyPacketList, we accept empty packet.
    if(pRSPacket->mSize0 + pRSPacket->mSize1 <= 0)
    {
        //alogd("empty packet, need not copy.");
        pPacketEntry->mpData0 = manager->mpWritePos;
        manager->mPacketCount++;
        list_move_tail(&pPacketEntry->mList, &manager->mReadyPacketList);
        goto _configPts;
    }
    
    int     nDataBufLeftSize;
    char*     pWritePos = manager->mpWritePos;
    int     nCopyLen0 = 0;
    int     nCopyLen1 = 0;
    int     nArrayDBPacketSize[2] = {0};
    char*     pArrayDBPacketPtr[2] = {NULL};
    int     nDataBufNum = 0;
    BOOL    nLocateFlag = FALSE;
    BOOL    nDoneFlag = FALSE;
    DynamicBuffer   *pLocateDataBufEntry;
    DynamicBuffer   *pDataBufEntry;
    DynamicBuffer   *pTmpDataBufEntry;
    //locate dataBuf by mpWritePos, copy packet data.
    list_for_each_entry(pLocateDataBufEntry, &manager->mDataBufList, mList)
    {
        if(pWritePos >= pLocateDataBufEntry->mpBuffer && pWritePos < pLocateDataBufEntry->mpBuffer+pLocateDataBufEntry->mSize)
        {
            nLocateFlag = TRUE;
            break;
        }
    }
    if(!nLocateFlag)
    {
        aloge("fatal error! why locate dataBuf fail?");
        pthread_mutex_unlock(&manager->mPacketListLock);
        return ERR_MUX_UNEXIST;
    }
    pDataBufEntry = pLocateDataBufEntry;
    list_for_each_entry_from(pDataBufEntry, &manager->mDataBufList, mList)
    {
        if(pWritePos < pDataBufEntry->mpBuffer || pWritePos >= pDataBufEntry->mpBuffer+pDataBufEntry->mSize)
        {
            aloge("fatal error! pWt wrong, check code!");
        }
        if(nDataBufNum>=2)
        {
            aloge("fatal error! need 3 or more dataBuf to copy packetSize[%d]KB, don't support, discard packet, modify AVPACKET_CACHE_ENLARGE_SIZE!", 
                nPacketDataSize/1024);
            pthread_mutex_unlock(&manager->mPacketListLock);
            return ERR_MUX_NOT_SUPPORT;
        }
        pArrayDBPacketPtr[nDataBufNum] = pWritePos;
        nDataBufLeftSize = (char*)pDataBufEntry->mpBuffer + pDataBufEntry->mSize - pWritePos;
        if(nCopyLen0 < pRSPacket->mSize0)
        {
            if(nDataBufLeftSize >= pRSPacket->mSize0-nCopyLen0)
            {
                memcpy(pWritePos, pRSPacket->mpData0+nCopyLen0, pRSPacket->mSize0-nCopyLen0);
                pWritePos += pRSPacket->mSize0-nCopyLen0;
                nArrayDBPacketSize[nDataBufNum] += pRSPacket->mSize0-nCopyLen0;
                nCopyLen0 = pRSPacket->mSize0;
                if(pWritePos == pDataBufEntry->mpBuffer+pDataBufEntry->mSize)
                {
                    if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                    {
                        pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                    }
                    else
                    {
                        pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                    }
                    pWritePos = pTmpDataBufEntry->mpBuffer;
                    //decide if copy done
                    if(0 == pRSPacket->mSize1)
                    {
                        nDoneFlag = TRUE;
                        break;
                    }
                    else
                    {
                        nDataBufNum++;
                        continue;
                    }
                }
                if(0 == pRSPacket->mSize1)
                {
                    nDoneFlag = TRUE;
                    break;
                }
            }
            else
            {
                memcpy(pWritePos, pRSPacket->mpData0+nCopyLen0, nDataBufLeftSize);
                nArrayDBPacketSize[nDataBufNum] += nDataBufLeftSize;
                nCopyLen0 += nDataBufLeftSize;
                if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                {
                    pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                }
                else
                {
                    pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                }
                pWritePos = pTmpDataBufEntry->mpBuffer;
                nDataBufNum++;
                continue;
            }
        }
        nDataBufLeftSize = (char*)pDataBufEntry->mpBuffer+pDataBufEntry->mSize-pWritePos;
        if(nCopyLen1 < pRSPacket->mSize1)
        {
            if(nDataBufLeftSize >= pRSPacket->mSize1-nCopyLen1)
            {
                memcpy(pWritePos, pRSPacket->mpData1+nCopyLen1, pRSPacket->mSize1-nCopyLen1);
                pWritePos += pRSPacket->mSize1-nCopyLen1;
                nArrayDBPacketSize[nDataBufNum] += pRSPacket->mSize1-nCopyLen1;
                nCopyLen1 = pRSPacket->mSize1;
                if(pWritePos == pDataBufEntry->mpBuffer+pDataBufEntry->mSize)
                {
                    if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                    {
                        pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                    }
                    else
                    {
                        pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                    }
                    pWritePos = pTmpDataBufEntry->mpBuffer;
                    nDataBufNum++;
                }
                nDoneFlag = TRUE;
                break;
            }
            else
            {
                memcpy(pWritePos, pRSPacket->mpData1+nCopyLen1, nDataBufLeftSize);
                nArrayDBPacketSize[nDataBufNum] += nDataBufLeftSize;
                nCopyLen1 += nDataBufLeftSize;
                if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                {
                    pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                }
                else
                {
                    pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                }
                pWritePos = pTmpDataBufEntry->mpBuffer;
                nDataBufNum++;
                continue;
            }
        }
        else
        {
            aloge("fatal error! impossible come here, check code!");
        }
    }
    if(TRUE == nDoneFlag)
    {
        manager->mpWritePos = pWritePos;
        manager->mUsedBufSize+=nPacketDataSize;
        manager->mPacketCount++;
        pPacketEntry->mpData0 = pArrayDBPacketPtr[0];
        pPacketEntry->mSize0 = nArrayDBPacketSize[0];
        pPacketEntry->mpData1 = pArrayDBPacketPtr[1];
        pPacketEntry->mSize1 = nArrayDBPacketSize[1];
        list_move_tail(&pPacketEntry->mList, &manager->mReadyPacketList);
        goto _configPts;
    }
    if(nDataBufNum>=2)
    {
        aloge("fatal error! need 3 or more dataBuf to copy packetSize[%d]KB, don't support, discard packet, modify AVPACKET_CACHE_ENLARGE_SIZE!", 
            nPacketDataSize/1024);
        pthread_mutex_unlock(&manager->mPacketListLock);
        return ERR_MUX_NOT_SUPPORT;
    }
    BOOL nLoopEndFlag = FALSE;
    list_for_each_entry(pDataBufEntry, &manager->mDataBufList, mList)
    {
        if(pWritePos < pDataBufEntry->mpBuffer || pWritePos >= pDataBufEntry->mpBuffer+pDataBufEntry->mSize)
        {
            aloge("fatal error! pWt wrong, check code!");
        }
        if(nLoopEndFlag)
        {
            aloge("fatal error! not enough dataBuf to contain packetSize[%d]KB? check code!", nPacketDataSize/1024);
            break;
        }
        if(pDataBufEntry == pLocateDataBufEntry)
        {
            if(manager->mpReadPos < pDataBufEntry->mpBuffer || manager->mpReadPos >= pDataBufEntry->mpBuffer+pDataBufEntry->mSize)
            {
                aloge("fatal error! mpReadPos[%p] is wrong?[%p][%p], check code!",
                    manager->mpReadPos, pDataBufEntry->mpBuffer, pDataBufEntry->mpBuffer+pDataBufEntry->mSize);
            }
            nLoopEndFlag = TRUE;
        }
        if(nDataBufNum>=2)
        {
            aloge("fatal error! need 3 or more dataBuf to copy packetSize[%d]KB, don't support, discard packet, modify AVPACKET_CACHE_ENLARGE_SIZE!", 
                nPacketDataSize/1024);
            pthread_mutex_unlock(&manager->mPacketListLock);
            return ERR_MUX_NOT_SUPPORT;
        }
        pArrayDBPacketPtr[nDataBufNum] = pWritePos;
        nDataBufLeftSize = (char*)pDataBufEntry->mpBuffer + pDataBufEntry->mSize - pWritePos;
        if(nCopyLen0 < pRSPacket->mSize0)
        {
            if(nDataBufLeftSize >= pRSPacket->mSize0-nCopyLen0)
            {
                memcpy(pWritePos, pRSPacket->mpData0+nCopyLen0, pRSPacket->mSize0-nCopyLen0);
                pWritePos += pRSPacket->mSize0-nCopyLen0;
                nArrayDBPacketSize[nDataBufNum] += pRSPacket->mSize0-nCopyLen0;
                nCopyLen0 = pRSPacket->mSize0;
                if(pWritePos == pDataBufEntry->mpBuffer+pDataBufEntry->mSize)
                {
                    if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                    {
                        pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                    }
                    else
                    {
                        pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                    }
                    pWritePos = pTmpDataBufEntry->mpBuffer;
                    //decide if copy done
                    if(0 == pRSPacket->mSize1)
                    {
                        nDoneFlag = TRUE;
                        break;
                    }
                    else
                    {
                        nDataBufNum++;
                        continue;
                    }
                }
                if(0 == pRSPacket->mSize1)
                {
                    nDoneFlag = TRUE;
                    break;
                }
            }
            else
            {
                memcpy(pWritePos, pRSPacket->mpData0+nCopyLen0, nDataBufLeftSize);
                nArrayDBPacketSize[nDataBufNum] += nDataBufLeftSize;
                nCopyLen0 += nDataBufLeftSize;
                if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                {
                    pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                }
                else
                {
                    pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                }
                pWritePos = pTmpDataBufEntry->mpBuffer;
                nDataBufNum++;
                continue;
            }
        }
        nDataBufLeftSize = (char*)pDataBufEntry->mpBuffer+pDataBufEntry->mSize-pWritePos;
        if(nCopyLen1 < pRSPacket->mSize1)
        {
            if(nDataBufLeftSize >= pRSPacket->mSize1-nCopyLen1)
            {
                memcpy(pWritePos, pRSPacket->mpData1+nCopyLen1, pRSPacket->mSize1-nCopyLen1);
                pWritePos += pRSPacket->mSize1-nCopyLen1;
                nArrayDBPacketSize[nDataBufNum] += pRSPacket->mSize1-nCopyLen1;
                nCopyLen1 = pRSPacket->mSize1;
                if(pWritePos == pDataBufEntry->mpBuffer+pDataBufEntry->mSize)
                {
                    if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                    {
                        pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                    }
                    else
                    {
                        pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                    }
                    pWritePos = pTmpDataBufEntry->mpBuffer;
                    nDataBufNum++;
                }
                nDoneFlag = TRUE;
                break;
            }
            else
            {
                memcpy(pWritePos, pRSPacket->mpData1+nCopyLen1, nDataBufLeftSize);
                nArrayDBPacketSize[nDataBufNum] += nDataBufLeftSize;
                nCopyLen1 += nDataBufLeftSize;
                if(list_is_last(&pDataBufEntry->mList, &manager->mDataBufList))
                {
                    pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                }
                else
                {
                    pTmpDataBufEntry = list_entry(pDataBufEntry->mList.next, DynamicBuffer, mList);
                }
                pWritePos = pTmpDataBufEntry->mpBuffer;
                nDataBufNum++;
                continue;
            }
        }
        else
        {
            aloge("fatal error! impossible come here, check code!");
        }
    }
    if(TRUE == nDoneFlag)
    {
        manager->mpWritePos = pWritePos;
        manager->mUsedBufSize+=nPacketDataSize;
        manager->mPacketCount++;
        pPacketEntry->mpData0 = pArrayDBPacketPtr[0];
        pPacketEntry->mSize0 = nArrayDBPacketSize[0];
        pPacketEntry->mpData1 = pArrayDBPacketPtr[1];
        pPacketEntry->mSize1 = nArrayDBPacketSize[1];
        list_move_tail(&pPacketEntry->mList, &manager->mReadyPacketList);
        goto _configPts;
    }
    else
    {
        aloge("fatal error! copy [%d]KB fail!", nPacketDataSize/1024);
        pthread_mutex_unlock(&manager->mPacketListLock);
        return ERR_MUX_ILLEGAL_PARAM;
    }

_configPts:
    //config firstPts, decide if is full.
    if(manager->mFirstPts < 0)
    {
        manager->mFirstPts = pPacketEntry->mPts;
    }
    if(FALSE == manager->mIsFull)
    {
        if ((pPacketEntry->mPts - manager->mFirstPts)/1000 >= manager->mCacheTime)
        {
            alogd("cache manager full![%lld]-[%lld]>=[%lld]ms", pPacketEntry->mPts/1000, manager->mFirstPts/1000, manager->mCacheTime);
            manager->mIsFull = TRUE;
        }
    }
    //verify size at last.
    if(nPacketDataSize!=(pPacketEntry->mSize0 + pPacketEntry->mSize1) || nPacketDataSize!=(pRSPacket->mSize0+pRSPacket->mSize1))
    {
        aloge("fatal error! verify packetSize fail.[%d][%d][%d][%d][%d], check code!", 
            nPacketDataSize, pPacketEntry->mSize0, pPacketEntry->mSize1, pRSPacket->mSize0, pRSPacket->mSize1);
    }
    pthread_mutex_unlock(&manager->mPacketListLock);
    return eError;
}

ERRORTYPE RPCMGetPacket(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT RecSinkPacket **ppRSPacket)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
	ERRORTYPE eError = SUCCESS;

	if (manager == NULL)
	{
		aloge("Buffer manager is NULL!");
		return ERR_MUX_NULL_PTR;
	}

	pthread_mutex_lock(&manager->mPacketListLock);
    if(!list_empty(&manager->mReadyPacketList))
	{
        RecSinkPacket   *pEntry = list_first_entry(&manager->mReadyPacketList, RecSinkPacket, mList);
        pEntry->mRefCnt = 1;
        list_move_tail(&pEntry->mList, &manager->mUsingPacketList);
        *ppRSPacket = pEntry;
	}
	else
	{
		alogv("Buffer empty!");
		eError = ERR_MUX_UNEXIST;
	}
	pthread_mutex_unlock(&manager->mPacketListLock);
	return eError;
}

ERRORTYPE RPCMRefPacket(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nId)
{
    ERRORTYPE eError = SUCCESS;
    int    nFindFlag = 0;
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    pthread_mutex_lock(&manager->mPacketListLock);
    if(!list_empty(&manager->mUsingPacketList))
	{
        RecSinkPacket   *pEntry;
        list_for_each_entry(pEntry, &manager->mUsingPacketList, mList)
        {
            if(pEntry->mId == nId)
            {
                if(0 == nFindFlag)
                {
                    nFindFlag++;
                    pEntry->mRefCnt++;
                }
                else
                {
                    nFindFlag++;
                    aloge("fatal error! repeat packetId[%d-%d],[%d], check code!", pEntry->mStreamId, nId, nFindFlag);
                }
                break;
            }
        }
        if(0==nFindFlag)
        {
            aloge("fatal error! nId[%d] is not find!", nId);
            eError = ERR_MUX_UNEXIST;
        }
	}
	else
	{
		aloge("fatal error! nId[%d] is wrong!", nId);
		eError = ERR_MUX_UNEXIST;
	}
	pthread_mutex_unlock(&manager->mPacketListLock);
    return eError;
}


/*******************************************************************************
Function name: RPCMReleasePacket_l
Description: 
    release using packet list.
Parameters: 
    
Return: 
    
Time: 2018/4/2
*******************************************************************************/
static ERRORTYPE RPCMReleasePacket_l(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nId)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    int nFindFlag = 0;
    BOOL nReleaseFlag = FALSE;
	ERRORTYPE eError = SUCCESS;
    RecSinkPacket   *pDesEntry = NULL;

	if (!list_empty(&manager->mUsingPacketList))
	{
        RecSinkPacket   *pEntry;
        RecSinkPacket   *pReleaseEntry = NULL;
        list_for_each_entry(pEntry, &manager->mUsingPacketList, mList)
        {
            if(pEntry->mId == nId)
            {
                if(0 == nFindFlag)
                {
                    nFindFlag++;
                    pDesEntry = pEntry;
                    if(pEntry->mRefCnt>0)
                    {
                        pEntry->mRefCnt--;
                        if(0 == pEntry->mRefCnt)
                        {
                            pReleaseEntry = pEntry;
                        }
                    }
                    else
                    {
                        aloge("fatal error! packetId[%d-%d] refCnt[%d]<=0, check code!", pEntry->mStreamId, nId, pEntry->mRefCnt);
                    }
                }
                else
                {
                    nFindFlag++;
                    aloge("fatal error! repeat packetId[%d-%d],[%d], check code!", pEntry->mStreamId, nId, nFindFlag);
                }
                break;
            }
        }
        if(0==nFindFlag)
        {
            aloge("fatal error! nId[%d] is not find!", nId);
            eError = ERR_MUX_UNEXIST;
        }
        if(pReleaseEntry)
        {
            RecSinkPacket   *pFirstEntry = list_first_entry(&manager->mUsingPacketList, RecSinkPacket, mList);
            if(pReleaseEntry!=pFirstEntry)
            {
                aloge("fatal error! must release first entry in using packet list! check code!");
            }
            //release packet
            char*     pData;
            int     nSize;
            if(pReleaseEntry->mSize1>0)
            {
                pData = pReleaseEntry->mpData1;
                nSize = pReleaseEntry->mSize1;
            }
            else
            {
                pData = pReleaseEntry->mpData0;
                nSize = pReleaseEntry->mSize0;
            }
            int nFindDBFlag = 0;
            DynamicBuffer *pDBEntry;
            DynamicBuffer *pTmpDataBufEntry;
            list_for_each_entry(pDBEntry, &manager->mDataBufList, mList)
            {
                if(pData>=pDBEntry->mpBuffer && pData<pDBEntry->mpBuffer+pDBEntry->mSize)
                {
                    if(0 == nFindDBFlag)
                    {
                        nFindDBFlag++;
                        if(pData+nSize < pDBEntry->mpBuffer+pDBEntry->mSize)
                        {
                            manager->mpReadPos = pData+nSize;
                        }
                        else if(pData+nSize == pDBEntry->mpBuffer+pDBEntry->mSize)
                        {
                            if(list_is_last(&pDBEntry->mList, &manager->mDataBufList))
                            {
                                pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                            }
                            else
                            {
                                pTmpDataBufEntry = list_entry(pDBEntry->mList.next, DynamicBuffer, mList);
                            }
                            manager->mpReadPos = pTmpDataBufEntry->mpBuffer;
                        }
                        else
                        {
                            aloge("fatal error! what happend? mpData0[%p][%d], mpData1[%p][%d], DB[%p][%d]check code!",
                                pReleaseEntry->mpData0, pReleaseEntry->mSize0, pReleaseEntry->mpData1, pReleaseEntry->mSize1, pDBEntry->mpBuffer, pDBEntry->mSize);
                        }
                        manager->mUsedBufSize -= (pReleaseEntry->mSize0+pReleaseEntry->mSize1);
                        manager->mPacketCount--;
                        list_move_tail(&pReleaseEntry->mList, &manager->mIdlePacketList);
                        nReleaseFlag = TRUE;
                    }
                    else
                    {
                        nFindDBFlag++;
                        aloge("fatal error! find more [%d]DataBuf, check code!", nFindFlag);
                    }
                }
            }
            if(0 == nFindDBFlag)
            {
                aloge("fatal error! no dataBuf is not find! packet[%p][%d],[%p][%d]", 
                    pReleaseEntry->mpData0, pReleaseEntry->mSize0, pReleaseEntry->mpData1, pReleaseEntry->mSize1);
                eError = ERR_MUX_UNEXIST;
            }
        }
	}
	else
	{
		aloge("fatal error! nId[%d] is wrong!", nId);
		eError = ERR_MUX_UNEXIST;
	}
    
    if(manager->mWaitReleaseUsingPacketFlag)
    {
        if(nReleaseFlag)
        {
            aloge("fatal error! should not release this cachePacketId[%d], check code!", nId);
        }
        if(nFindFlag)
        {
            if(1 == pDesEntry->mRefCnt)
            {
                if(manager->mWaitReleaseUsingPacketId == nId)
                {
                    alogv("signal releaseUsingPacketId[%d], maybe fwrite slow!", manager->mWaitReleaseUsingPacketId);
                    manager->mWaitReleaseUsingPacketFlag = FALSE;
                    manager->mWaitReleaseUsingPacketId = -1;
                    pthread_cond_signal(&manager->mCondReleaseUsingPacket);
                }
                else
                {
                    aloge("fatal error! waitReleasePacketId[%d]!=releaseId[%d-%d], check code!", 
                        manager->mWaitReleaseUsingPacketId, pDesEntry->mStreamId, nId);
                }
            }
            else if(pDesEntry->mRefCnt <= 0)
            {
                aloge("fatal error! cachePacketId[%d-%d] refCnt[%d]<=0, check code!", pDesEntry->mStreamId, nId, pDesEntry->mRefCnt);
            }
            else
            {
                alogd("Be careful! cachePacketId[%d-%d] refCnt[%d], more recSink?", pDesEntry->mStreamId, nId, pDesEntry->mRefCnt);
            }
        }
    }
	return eError;
}

/*******************************************************************************
Function name: RPCMReleaseReadyPacket_l
Description: 
    release ready packet list.
Parameters: 
    
Return: 
    
Time: 2018/4/2
*******************************************************************************/
static ERRORTYPE RPCMReleaseReadyPacket_l(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nId)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    int nFindFlag     = 0;
    BOOL nReleaseFlag = FALSE;
    ERRORTYPE eError  = SUCCESS;
    RecSinkPacket  *pDesEntry = NULL;

	if (!list_empty(&manager->mReadyPacketList))
	{
        RecSinkPacket   *pEntry;
        RecSinkPacket   *pReleaseEntry = NULL;
        list_for_each_entry(pEntry, &manager->mReadyPacketList, mList)
        {
            if(pEntry->mId == nId)
            {
                if(0 == nFindFlag)
                {
                    nFindFlag++;
                    pDesEntry = pEntry;
                    if(pEntry->mRefCnt!=0)
                    {
                        aloge("fatal error! ready packet[%d-%d] ref[%d]!=0, check code!", pEntry->mStreamId, pEntry->mId, pEntry->mRefCnt);
                    }
                    pReleaseEntry = pEntry;
                }
                else
                {
                    nFindFlag++;
                    aloge("fatal error! repeat packetId[%d-%d],[%d], check code!", pEntry->mStreamId, nId, nFindFlag);
                }
                break;
            }
        }
        if(0==nFindFlag)
        {
            aloge("fatal error! nId[%d] is not find!", nId);
            eError = ERR_MUX_UNEXIST;
        }
        if(pReleaseEntry)
        {
            RecSinkPacket   *pFirstEntry = list_first_entry(&manager->mReadyPacketList, RecSinkPacket, mList);
            if(pReleaseEntry!=pFirstEntry)
            {
                aloge("fatal error! must release first entry in using packet list! check code!");
            }
            //release packet
            char*   pData;
            int     nSize;
            if(pReleaseEntry->mSize1>0)
            {
                pData = pReleaseEntry->mpData1;
                nSize = pReleaseEntry->mSize1;
            }
            else
            {
                pData = pReleaseEntry->mpData0;
                nSize = pReleaseEntry->mSize0;
            }
            int nFindDBFlag = 0;
            DynamicBuffer *pDBEntry;
            DynamicBuffer *pTmpDataBufEntry;
            list_for_each_entry(pDBEntry, &manager->mDataBufList, mList)
            {
                if(pData>=pDBEntry->mpBuffer && pData<pDBEntry->mpBuffer+pDBEntry->mSize)
                {
                    if(0 == nFindDBFlag)
                    {
                        nFindDBFlag++;
                        if(pData+nSize < pDBEntry->mpBuffer+pDBEntry->mSize)
                        {
                            manager->mpReadPos = pData+nSize;
                        }
                        else if(pData+nSize == pDBEntry->mpBuffer+pDBEntry->mSize)
                        {
                            if(list_is_last(&pDBEntry->mList, &manager->mDataBufList))
                            {
                                pTmpDataBufEntry = list_first_entry(&manager->mDataBufList, DynamicBuffer, mList);
                            }
                            else
                            {
                                pTmpDataBufEntry = list_entry(pDBEntry->mList.next, DynamicBuffer, mList);
                            }
                            manager->mpReadPos = pTmpDataBufEntry->mpBuffer;
                        }
                        else
                        {
                            aloge("fatal error! what happend? mpData0[%p][%d], mpData1[%p][%d], DB[%p][%d]check code!",
                                pReleaseEntry->mpData0, pReleaseEntry->mSize0, pReleaseEntry->mpData1, pReleaseEntry->mSize1, pDBEntry->mpBuffer, pDBEntry->mSize);
                        }
                        manager->mUsedBufSize -= (pReleaseEntry->mSize0+pReleaseEntry->mSize1);
                        manager->mPacketCount--;
                        list_move_tail(&pReleaseEntry->mList, &manager->mIdlePacketList);
                        nReleaseFlag = TRUE;
                    }
                    else
                    {
                        nFindDBFlag++;
                        aloge("fatal error! find more [%d]DataBuf, check code!", nFindFlag);
                    }
                }
            }
            if(0 == nFindDBFlag)
            {
                aloge("fatal error! no dataBuf is not find! packet[%p][%d],[%p][%d]",
                    pReleaseEntry->mpData0, pReleaseEntry->mSize0, pReleaseEntry->mpData1, pReleaseEntry->mSize1);
                eError = ERR_MUX_UNEXIST;
            }
        }
	}
	else
	{
		aloge("fatal error! nId[%d] is wrong!", nId);
		eError = ERR_MUX_UNEXIST;
	}
	return eError;
}

static ERRORTYPE RPCMReleasePacket(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN int nId)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    ERRORTYPE eError = SUCCESS;

    pthread_mutex_lock(&manager->mPacketListLock);
    eError = RPCMReleasePacket_l(hComponent, nId);
    pthread_mutex_unlock(&manager->mPacketListLock);
    return eError;
}

static BOOL RPCMIsReady(PARAM_IN COMP_HANDLETYPE hComponent)
{
	BOOL ret;
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
	if (manager == NULL)
	{
		aloge("Buffer manager is NULL!");
		return FALSE;
	}
	//pthread_mutex_lock(&manager->mPacketListLock);
	ret = manager->mIsFull;
	//pthread_mutex_unlock(&manager->mPacketListLock);
	return ret;
}

/*******************************************************************************
Function name: RPCMReleaseEarlistPacket
Description: 
    
Parameters: 
    bForce: if force to release, wait until packet is released.
Return: 
    SUCCESS: release one frame.
    OMX_ErrorTimeout: releaes frame fail because of timeout.
    OMX_ErrorNotReady: release frame fail because refCnt > 1, and not force release.
    OMX_ErrorUndefined: release frame fail because of other reasons.
Time: 2015/5/27
*******************************************************************************/
static ERRORTYPE RPCMReleaseEarlistPacket(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN BOOL bForce)
{
	RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
	ERRORTYPE eError = SUCCESS;
    int ret;
    int waitTimeout = 500;  //unit:ms
	pthread_mutex_lock(&manager->mPacketListLock);
    if(FALSE == manager->mIsFull)
    {
        aloge("fatal error! cache not full!");
        pthread_mutex_unlock(&manager->mPacketListLock);
        return ERR_MUX_NOT_PERM;
    }
	if (!list_empty(&manager->mUsingPacketList))
	{
        RecSinkPacket   *pReleaseEntry = list_first_entry(&manager->mUsingPacketList, RecSinkPacket, mList);
        if(1 == pReleaseEntry->mRefCnt)
        {
            eError = RPCMReleasePacket_l(hComponent, pReleaseEntry->mId);
        }
        else if(pReleaseEntry->mRefCnt > 1 && bForce)
        {
            if(manager->mWaitReleaseUsingPacketFlag!=FALSE)
            {
                aloge("fatal error! why manager->mWaitReleaseUsingPacketFlag[%d]!=false?", manager->mWaitReleaseUsingPacketFlag);
            }
            manager->mWaitReleaseUsingPacketFlag = TRUE;
            manager->mWaitReleaseUsingPacketId = pReleaseEntry->mId;
            alogv("need wait cachePacketId[%d-%d]RefCnt[%d] release, maybe fwrite slow", pReleaseEntry->mStreamId, manager->mWaitReleaseUsingPacketId, pReleaseEntry->mRefCnt);
            while(manager->mWaitReleaseUsingPacketFlag)
            {
                ret = pthread_cond_wait_timeout(&manager->mCondReleaseUsingPacket, &manager->mPacketListLock, waitTimeout);
                if(0 == ret)
                {
                }
                else
                {
                    alogw("Impossible! wait cachePacketId[%d-%d-%d],[%d], timeout[%d]ms, ret[%d]", 
                        pReleaseEntry->mStreamId, pReleaseEntry->mId, pReleaseEntry->mRefCnt, 
                        manager->mWaitReleaseUsingPacketId, waitTimeout, ret);
                    if(manager->mWaitReleaseUsingPacketFlag)
                    {
                        manager->mWaitReleaseUsingPacketFlag = FALSE;
                    }
                    else
                    {
                        aloge("fatal error! how this happened? timeout and flag is set to false!");
                    }
                    break;
                }
            }
            if(0 == ret)
            {
                eError = RPCMReleasePacket_l(hComponent, pReleaseEntry->mId);
            }
            else
            {
                eError = ERR_MUX_TIMEOUT;
            }
        }
        else
        {
            if(pReleaseEntry->mRefCnt < 1)
            {
                aloge("fatal error! earlist using packet ref cnt[%d] is wrong!", pReleaseEntry->mRefCnt);
                eError = ERR_MUX_NOT_PERM;
            }
            else
            {
                alogv("not force release earlist packet, ref cnt[%ld], bForce[%d]", pReleaseEntry->mRefCnt, bForce);
                eError = ERR_MUX_BUSY;
            }
        }
	}
    else if(!list_empty(&manager->mReadyPacketList))
    {
        RecSinkPacket   *pReleaseEntry = list_first_entry(&manager->mReadyPacketList, RecSinkPacket, mList);
        if(pReleaseEntry->mRefCnt != 0)
        {
            aloge("fatal error! why ready packet list ref[%d]!=0", pReleaseEntry->mRefCnt);
        }
        eError = RPCMReleaseReadyPacket_l(hComponent, pReleaseEntry->mId);
    }
	else
	{
		aloge("fatal error! mCacheTime[%lld]ms, check code!", manager->mCacheTime);
		eError = ERR_MUX_ILLEGAL_PARAM;
	}
	pthread_mutex_unlock(&manager->mPacketListLock);
	return eError;
}

static int RPCMGetReadyPacketCount(PARAM_IN COMP_HANDLETYPE hComponent)
{
    int nCount;
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
	if (manager == NULL)
	{
		aloge("Buffer manager is NULL!");
		return -1;
	}
	pthread_mutex_lock(&manager->mPacketListLock);
    nCount = 0;
    struct list_head    *pEntry;
    list_for_each(pEntry, &manager->mReadyPacketList)
    {
        nCount++;
    }
	pthread_mutex_unlock(&manager->mPacketListLock);
	return nCount;
}
ERRORTYPE RPCMQueryCacheState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT CacheState *pState)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    pthread_mutex_lock(&manager->mPacketListLock);
    if(FALSE == manager->mIsFull)
    {
        aloge("fatal error! cache not full!");
        pthread_mutex_unlock(&manager->mPacketListLock);
        return ERR_MUX_SYS_NOTREADY;
    }
    unsigned int nUsedCacheCnt = 0;
    unsigned int nUsedCacheSize = 0;
    unsigned int nReadyCacheCnt = 0;
    unsigned int nReadyCacheSize = 0;
    RecSinkPacket *pEntry;
    list_for_each_entry(pEntry, &manager->mUsingPacketList, mList)
    {
        nUsedCacheSize += pEntry->mSize0+pEntry->mSize1;
        nUsedCacheCnt++;
    }
    list_for_each_entry(pEntry, &manager->mReadyPacketList, mList)
    {
        nReadyCacheSize += pEntry->mSize0+pEntry->mSize1;
        nReadyCacheCnt++;
    }
    pState->mValidSize = (nUsedCacheSize+nReadyCacheSize)/1024;
    pState->mTotalSize = manager->mBufSize/1024;
    pState->mValidSizePercent = pState->mValidSize*100/pState->mTotalSize;
    alogv("manager[%p][%d]MB usingPacketCnt[%d], usingSize[%d], readyPacketCnt[%d], readySize[%d], sizePercent[%d]!", 
        manager, manager->mBufSize/(1024*1024), nUsedCacheCnt, nUsedCacheSize, nReadyCacheCnt, nReadyCacheSize, pState->mValidSizePercent);
    pthread_mutex_unlock(&manager->mPacketListLock);
    return SUCCESS;
}

static ERRORTYPE RPCMControlCacheLevel(PARAM_IN COMP_HANDLETYPE hComponent)
{
    ERRORTYPE   eError = SUCCESS;
    BOOL    nForceFlag = FALSE;
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    RPCMDuration sRPCMDuration;
    while(1)
    {
        if(SUCCESS != RPCMGetDuration(hComponent, &sRPCMDuration))
        {
            return ERR_MUX_NOT_SUPPORT;
        }
        if(sRPCMDuration.mTotalDuration <= manager->mCacheTime)
        {
            break;
        }

        //query cache state, if cache state >= 85%, force release earlist packet.
        nForceFlag = TRUE;
        CacheState cacheState;
        if(SUCCESS == RPCMQueryCacheState(manager, &cacheState))
        {
            if(cacheState.mValidSizePercent < 85)
            {
                nForceFlag = FALSE;
            }
        }
        eError = RPCMReleaseEarlistPacket(manager, nForceFlag);
        if(SUCCESS == eError)
        {
            continue;
        }
        else
        {
            break;
        }
    }
    if(SUCCESS != eError)
    {
        return eError;
    }
    //check if first video frame is key frame. if not, need release more packets.
    // if there are two or more video streams, we can't guarantee that all videos will begin with key frame.
    // we can only let the first video0 frame in the cache manager is key frame.
    pthread_mutex_lock(&manager->mPacketListLock);
    BOOL bMeetVideoFlag = FALSE;
    BOOL bReleaseVideoFrameFlag = FALSE;    //if need release video frame.
    RecSinkPacket   *pEntry;
    list_for_each_entry(pEntry, &manager->mUsingPacketList, mList)
    {
        if(CODEC_TYPE_VIDEO == pEntry->mStreamType)
        {
            bMeetVideoFlag = TRUE;
            if(!(pEntry->mFlags & AVPACKET_FLAG_KEYFRAME))
            {
                alogv("first video packet is not keyFrame, need release!");
                bReleaseVideoFrameFlag = TRUE;
            }
            break;
        }
    }
    if(FALSE == bMeetVideoFlag)
    {
        list_for_each_entry(pEntry, &manager->mReadyPacketList, mList)
        {
            if(CODEC_TYPE_VIDEO == pEntry->mStreamType)
            {
                bMeetVideoFlag = TRUE;
                if(!(pEntry->mFlags & AVPACKET_FLAG_KEYFRAME))
                {
                    alogv("first video packet is not keyFrame, need release!");
                    bReleaseVideoFrameFlag = TRUE;
                }
                break;
            }
        }
    }
    if(bReleaseVideoFrameFlag)
    {
        //continue release frame until meet a keyframe, or other special case.
        BOOL bKeyFrameFlag = FALSE; //if meet key frame
        BOOL bStopFlag = FALSE;
        int cnt = 0;
    	if (!list_empty(&manager->mUsingPacketList))
    	{
            RecSinkPacket   *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &manager->mUsingPacketList, mList)
            {
                if((CODEC_TYPE_VIDEO == pEntry->mStreamType) && (manager->mRefStreamId == pEntry->mStreamId) && (pEntry->mFlags & AVPACKET_FLAG_KEYFRAME))
                {
                    alogv("continue release cache packet, meet refVideoStream[%d] keyFrame, stop!", manager->mRefStreamId);
                    bKeyFrameFlag = TRUE;
                    bStopFlag = TRUE;
                    break;
                }
                if(pEntry->mRefCnt>1)
                {
                    alogv("continue release cache packet, meet refCnt[%d]>1, stop!", pEntry->mRefCnt);
                    bStopFlag = TRUE;
                    break;
                }
                if(pEntry->mRefCnt<1)
                {
                    aloge("fatal error! continue release cache packet, meet refCnt[%d]<1, check code!", pEntry->mRefCnt);
                    bStopFlag = TRUE;
                    eError = ERR_MUX_NOT_PERM;
                    break;
                }
                eError = RPCMReleasePacket_l(hComponent, pEntry->mId);
                if(SUCCESS!=eError)
                {
                    aloge("fatal error! continue release cache packet fail[0x%x]", eError);
                    bStopFlag = TRUE;
                    break;
                }
                cnt++;
            }
    	}
        if(FALSE == bStopFlag)
        {
            RecSinkPacket   *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &manager->mReadyPacketList, mList)
            {
                if((CODEC_TYPE_VIDEO == pEntry->mStreamType) && (manager->mRefStreamId == pEntry->mStreamId) && (pEntry->mFlags & AVPACKET_FLAG_KEYFRAME))
                {
                    alogv("continue release cache ready packet, meet refVideoStream[%d] keyFrame, stop!", manager->mRefStreamId);
                    bKeyFrameFlag = TRUE;
                    bStopFlag = TRUE;
                    break;
                }
                if(pEntry->mRefCnt!=0)
                {
                    aloge("fatal error! why ready packet ref[%d]!=0?", pEntry->mRefCnt);
                }
                eError = RPCMReleaseReadyPacket_l(hComponent, pEntry->mId);
                if(SUCCESS!=eError)
                {
                    aloge("fatal error! continue release cache packet fail[0x%x]", eError);
                    bStopFlag = TRUE;
                    break;
                }
                cnt++;
            }
        }
        RPCMGetDuration_l(hComponent, &sRPCMDuration);
        alogv("continue release cache packet: num[%d], meetKeyFrame[%d], totalDuration[%lld]ms, usingPacketDuration[%lld]ms", 
            cnt, bKeyFrameFlag, sRPCMDuration.mTotalDuration, sRPCMDuration.mUsingPacketDuration);
    }
    pthread_mutex_unlock(&manager->mPacketListLock);
    return eError;
}

ERRORTYPE RPCMLockPacketList(PARAM_IN COMP_HANDLETYPE hComponent)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    pthread_mutex_lock(&manager->mPacketListLock);
    return SUCCESS;
}
ERRORTYPE RPCMUnlockPacketList(PARAM_IN COMP_HANDLETYPE hComponent)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    pthread_mutex_unlock(&manager->mPacketListLock);
    return SUCCESS;
}
ERRORTYPE RPCMGetUsingPacketList(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT struct list_head **ppUsingPacketList)
{
    RsPacketCacheManager *manager = (RsPacketCacheManager*)hComponent;
    *ppUsingPacketList = &manager->mUsingPacketList;
    return SUCCESS;
}
ERRORTYPE RsPacketCacheManagerInit(RsPacketCacheManager *pThiz, int64_t nCacheTime)
{
    int ret;
    DynamicBuffer   *pDataBufEntry;
    DynamicBuffer   *pEntry, *pTmp;
	if (nCacheTime <= 0)
	{
		return ERR_MUX_ILLEGAL_PARAM;
	}
    memset(pThiz, 0, sizeof(*pThiz));
    INIT_LIST_HEAD(&pThiz->mDataBufList);
    pThiz->mpWritePos = NULL;
    pThiz->mpReadPos = NULL;
    pThiz->mUsedBufSize = 0;
    pThiz->mBufSize = 0;
    pThiz->mPacketCount = 0;
    pThiz->mCacheTime = nCacheTime;
    pThiz->mFirstPts = -1;
    pThiz->mIsFull = FALSE;
    pThiz->mWaitReleaseUsingPacketFlag = FALSE;
    pThiz->mWaitReleaseUsingPacketId = -1;
    if(0 != pthread_mutex_init(&pThiz->mPacketListLock, NULL))
    {
        aloge("pthread mutex init fail!");
        return ERR_MUX_SYS_NOTREADY;
    }
    pthread_condattr_t  condAttr;
    pthread_condattr_init(&condAttr);
//#if defined(__LP64__)
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
//#endif
    ret = pthread_cond_init(&pThiz->mCondReleaseUsingPacket, &condAttr);
    if(ret!=0)
    {
        aloge("[%s]fatal error! pthread cond init fail", strrchr(__FILE__, '/')+1);
        goto _err1;
    }
    
    //INIT_LIST_HEAD(&pThiz->mRSPacketBufList);
    INIT_LIST_HEAD(&pThiz->mIdlePacketList);
    INIT_LIST_HEAD(&pThiz->mReadyPacketList);
    INIT_LIST_HEAD(&pThiz->mUsingPacketList);
    pThiz->mPacketIdCounter = 0;

    pThiz->SetRefStream = RPCMSetRefStream;
    pThiz->PushPacket = RPCMPushPacket;
    pThiz->GetPacket = RPCMGetPacket;
    pThiz->RefPacket = RPCMRefPacket;
    pThiz->ReleasePacket = RPCMReleasePacket;
    pThiz->IsReady = RPCMIsReady;
    pThiz->GetReadyPacketCount = RPCMGetReadyPacketCount;
    pThiz->ControlCacheLevel = RPCMControlCacheLevel;
    pThiz->QueryCacheState = RPCMQueryCacheState;
    pThiz->LockPacketList = RPCMLockPacketList;
    pThiz->UnlockPacketList = RPCMUnlockPacketList;
    pThiz->GetUsingPacketList = RPCMGetUsingPacketList;
    //malloc fisrt data buffer here.
    if(SUCCESS!=RPCMIncreaseDataBufList(pThiz, AVPACKET_CACHE_SIZE*sizeof(char)))
    {
        goto _err2;
    }
    //malloc fisrt RecSinkPacket struct buffer and init mIdlePacketList here.
    if(SUCCESS!=RPCMIncreaseIdlePacketList(pThiz))
    {
        goto _err3;
    }
    //init mpWritePos, mpReadPos
    pDataBufEntry = list_first_entry(&pThiz->mDataBufList, DynamicBuffer, mList);
    pThiz->mpWritePos = pThiz->mpReadPos = pDataBufEntry->mpBuffer;
	return SUCCESS;

_err3:
    list_for_each_entry_safe(pEntry, pTmp, &pThiz->mDataBufList, mList)
    {
        if(pEntry->mpBuffer)
        {
            free(pEntry->mpBuffer);
            pEntry->mpBuffer = NULL;
        }
        pEntry->mSize = 0;
        list_del(&pEntry->mList);
        free(pEntry);
    }
_err2:
    pthread_cond_destroy(&pThiz->mCondReleaseUsingPacket);
_err1:
//_err0:
    pthread_mutex_destroy(&pThiz->mPacketListLock);
    return ERR_MUX_NOT_PERM;
}

ERRORTYPE RsPacketCacheManagerDestroy(RsPacketCacheManager *pThiz)
{
    alogv("RsPacketCacheManager Destroy");
    if (NULL == pThiz)
    {
        return SUCCESS;
    }
    pthread_mutex_lock(&pThiz->mPacketListLock);
    if(!list_empty(&pThiz->mReadyPacketList))
    {
        aloge("fatal error! ready packet list is not empty!");
    }
    RecSinkPacket *pPktEntry, *pPktTmp;
    int cnt = 0;
    if(!list_empty(&pThiz->mUsingPacketList))
    {
        list_for_each_entry_safe(pPktEntry, pPktTmp, &pThiz->mUsingPacketList, mList)
        {
            if(pPktEntry->mRefCnt != 1)
            {
                aloge("fatal error! using packet refCnt[%d]!=1!", pPktEntry->mRefCnt);
            }
            RPCMReleasePacket_l(pThiz, pPktEntry->mId);
            cnt++;
        }
        alogd("release [%d]using packet!", cnt);
    }
    list_for_each_entry_safe(pPktEntry, pPktTmp, &pThiz->mIdlePacketList, mList)
    {
        list_del(&pPktEntry->mList);
        free(pPktEntry);
    }
    DynamicBuffer *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pThiz->mDataBufList, mList)
    {
        if(pEntry->mpBuffer)
        {
            free(pEntry->mpBuffer);
            pEntry->mpBuffer = NULL;
        }
        pEntry->mSize = 0;
        list_del(&pEntry->mList);
        free(pEntry);
    }
    INIT_LIST_HEAD(&pThiz->mIdlePacketList);
    INIT_LIST_HEAD(&pThiz->mReadyPacketList);
    INIT_LIST_HEAD(&pThiz->mUsingPacketList);
    pthread_mutex_unlock(&pThiz->mPacketListLock);
    pthread_cond_destroy(&pThiz->mCondReleaseUsingPacket);
    pthread_mutex_destroy(&pThiz->mPacketListLock);
	return SUCCESS;
}

RsPacketCacheManager *RsPacketCacheManagerConstruct(int64_t nCacheTime)
{
    RsPacketCacheManager *manager;

    alogd("RsPacketCache Construct, cacheTime[%lld]",nCacheTime);

    if (nCacheTime <= 0)
    {
        aloge("Failed to alloc AVPACKET_CACHE_MANAGER(%s)!", strerror(errno));
        return NULL;
    }
    manager = (RsPacketCacheManager*)malloc(sizeof(RsPacketCacheManager));
    if (manager == NULL)
    {
        aloge("Failed to alloc AVPACKET_CACHE_MANAGER(%s)!", strerror(errno));
        return NULL;
    }
    if(SUCCESS!=RsPacketCacheManagerInit(manager, nCacheTime))
    {
        free(manager);
        return NULL;
    }
    return manager;
}

void RsPacketCacheManagerDestruct(RsPacketCacheManager *manager)
{
	alogv("RsPacketCacheManager Destruct");
    if(manager)
    {
        RsPacketCacheManagerDestroy(manager);
    	free(manager);
    }
}

