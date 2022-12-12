
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : sbm.c
* Description :This is the stream buffer module. The SBM provides
*              methods for managing the stream data before decode.
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#include <stdlib.h>
#include<string.h>
#include <pthread.h>
#include "sbm.h"
#include "CdcMessageQueue.h"
#include "cdc_log.h"

extern VideoStreamDataInfo *requestStream(SbmFrame *pSbm);
extern int flushStream(SbmFrame *pSbm, VideoStreamDataInfo *pDataInfo, int bFlush);
extern int returnStream(SbmFrame* pSbm , VideoStreamDataInfo *pDataInfo);
extern int addFramePic(SbmFrame* pSbm, FramePicInfo* pFramePic); //* addFramePic
extern FramePicInfo* requestEmptyFramePic(SbmFrame* pSbm);



typedef enum SbmHevcNaluType
{
    SBM_HEVC_NAL_TRAIL_N    = 0,
    SBM_HEVC_NAL_TRAIL_R    = 1,
    SBM_HEVC_NAL_TSA_N      = 2,
    SBM_HEVC_NAL_TSA_R      = 3,
    SBM_HEVC_NAL_STSA_N     = 4,
    SBM_HEVC_NAL_STSA_R     = 5,
    SBM_HEVC_NAL_RADL_N     = 6,
    SBM_HEVC_NAL_RADL_R     = 7,
    SBM_HEVC_NAL_RASL_N     = 8,
    SBM_HEVC_NAL_RASL_R     = 9,
    SBM_HEVC_NAL_BLA_W_LP   = 16,
    SBM_HEVC_NAL_BLA_W_RADL = 17,
    SBM_HEVC_NAL_BLA_N_LP   = 18,
    SBM_HEVC_NAL_IDR_W_RADL = 19,
    SBM_HEVC_NAL_IDR_N_LP   = 20,
    SBM_HEVC_NAL_CRA_NUT    = 21,
    SBM_HEVC_NAL_VPS        = 32,
    SBM_HEVC_NAL_SPS        = 33,
    SBM_HEVC_NAL_PPS        = 34,
    SBM_HEVC_NAL_AUD        = 35,
    SBM_HEVC_NAL_EOS_NUT    = 36,
    SBM_HEVC_NAL_EOB_NUT    = 37,
    SBM_HEVC_NAL_FD_NUT     = 38,
    SBM_HEVC_NAL_SEI_PREFIX = 39,
    SBM_HEVC_NAL_SEI_SUFFIX = 40,
    SBM_HEVC_UNSPEC63          = 63
}SbmHevcNaluType;

#define IsFrameNalu(eNaluType) (eNaluType <= SBM_HEVC_NAL_CRA_NUT)


static inline char readByteIdx(char *p, char *pStart, char *pEnd, s32 i)
{
    logv("p = %p, start = %p, end = %p, i = %d",p,pStart, pEnd, i);
    char c = 0x0;
    if((p+i) <= pEnd)
        c = p[i];
    else
    {
        s32 d = (s32)(pEnd - p) + 1;
        c = pStart[i - d];
    }
    return c;
}

static inline void ptrPlusOne(char **p, char *pStart, char *pEnd)
{
    if((*p) == pEnd)
        (*p) = pStart;
    else
        (*p) += 1;
}

static s32 checkBitStreamTypeWithStartCode(SbmFrame* pSbm,
                                           VideoStreamDataInfo *pStream)
{
    char *pBuf = NULL;
    char tmpBuf[6] = {0};
    const s32 nTsStreamType       = 0x000001;
    const s32 nForbiddenBitValue  = 0;
    const s32 nTemporalIdMinValue = 1;
    char* pStart = pSbm->pStreamBuffer;
    char* pEnd   = pSbm->pStreamBufferEnd;
    s32 nHadCheckBytesLen = 0;
    s32 nCheck4BitsValue = -1;
    s32 nTemporalId      = -1;
    s32 nForbiddenBit    = -1;

    //*1. process sbm-cycle-buffer case
    //logd("the_111,bSecureVideoFlag=%d",pSbm->mConfig.bSecureVideoFlag);

    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        pBuf = (char *)malloc(pStream->nLength+32);
        if(pBuf == NULL)
        {
            loge("*** malloc pBuf failed, size = %d",pStream->nLength);
            return -2;
        }
        if(pStream->pData + pStream->nLength <= pEnd)
        {
            //logd("the_111");
            CdcMemRead(pSbm->mConfig.memops, pBuf, pStream->pData, pStream->nLength);
        }
        else
        {
            //logd("the_111");
            s32 nFirsrPart;
            nFirsrPart = pEnd-pStream->pData+1;
            CdcMemRead(pSbm->mConfig.memops, pBuf, pStream->pData, nFirsrPart);
            CdcMemRead(pSbm->mConfig.memops, pBuf+nFirsrPart,
                       pStart, pStream->nLength-nFirsrPart);

        }
        pStart = pBuf;
        pEnd   = pBuf + pStream->nLength;
        //logd("the_121,pStart=%p,pEnd=%p\n",pStart,pEnd);
    }
    else
    {
        //logd("the_222");
        pBuf = pStream->pData;
    }

    while((nHadCheckBytesLen + 6) < pStream->nLength)
    {

        tmpBuf[0] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 0);
        tmpBuf[1] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 1);
        tmpBuf[2] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 2);
        tmpBuf[3] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 3);
        tmpBuf[4] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 4);
        tmpBuf[5] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 5);
        //logd("%d,%d,%d,%d,%d,%d",tmpBuf[0],tmpBuf[1],tmpBuf[2],tmpBuf[3],tmpBuf[4],tmpBuf[5]);
        nCheck4BitsValue = (tmpBuf[0] << 24) | (tmpBuf[1] << 16) | (tmpBuf[2] << 8) | tmpBuf[3];
        if(nCheck4BitsValue == 0) //*compatible for the case: 00 00 00 00 00 00 00 01
        {
            nHadCheckBytesLen++;
            continue;
        }

        if(nCheck4BitsValue == nTsStreamType)
        {
            nForbiddenBit = tmpBuf[4] >> 7; //* read 1 bits
            nTemporalId   = tmpBuf[5] & 0x7;//* read 3 bits
            if(nTemporalId >= nTemporalIdMinValue && nForbiddenBit == nForbiddenBitValue)
            {
                pSbm->bStreamWithStartCode = 1;
                return 0;
            }
            else
            {
                nHadCheckBytesLen += 4;
                continue;
            }
        }
        else if((nCheck4BitsValue >> 8) == nTsStreamType)
        {
            nForbiddenBit = tmpBuf[3] >> 7; //* read 1 bits
            nTemporalId   = tmpBuf[4] & 0x7;//* read 3 bits
            if(nTemporalId >= nTemporalIdMinValue && nForbiddenBit == nForbiddenBitValue)
            {
                pSbm->bStreamWithStartCode = 1;
                return 0;
            }
            else
            {
                nHadCheckBytesLen += 3;
                continue;
            }

        }
        else
        {
            nHadCheckBytesLen += 4;
            continue;
        }
    }
    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        free(pBuf);
        pBuf = NULL;
    }
    return -1;
}

static s32 checkBitStreamTypeWithoutStartCode(SbmFrame* pSbm,
                                           VideoStreamDataInfo *pStream)
{
    const s32 nForbiddenBitValue  = 0;
    const s32 nTemporalIdMinValue = 1;
    char *pBuf = NULL;
    char tmpBuf[6] = {0};
    s32 nTemporalId      = -1;
    s32 nForbiddenBit    = -1;
    s32 nDataSize   = -1;
    s32 nRemainSize = -1;
    s32 nRet = -1;
    char* pStart = pSbm->pStreamBuffer;
    char* pEnd   = pSbm->pStreamBufferEnd;

    s32 nHadProcessLen = 0;

    logd("the_111,bSecureVideoFlag=%d",pSbm->mConfig.bSecureVideoFlag);

    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        pBuf = (char *)malloc(pStream->nLength+32);
        if(pBuf == NULL)
        {
            loge("*** malloc pBuf failed, size = %d",pStream->nLength);
            return -2;
        }
        if(pStream->pData + pStream->nLength <= pEnd)
        {
            logv("the_111");
            CdcMemRead(pSbm->mConfig.memops, pBuf, pStream->pData, pStream->nLength);
        }
        else
        {
            logv("the_112");
            s32 nFirsrPart;
            nFirsrPart = pEnd - pStream->pData +1;
            CdcMemRead(pSbm->mConfig.memops, pBuf, pStream->pData, nFirsrPart);
            CdcMemRead(pSbm->mConfig.memops, pBuf+nFirsrPart,
                       pStart, pStream->nLength-nFirsrPart);

        }
        pStart = pBuf;
        pEnd   = pBuf + pStream->nLength;
        logv("the_121,pStart=%p,pEnd=%p\n",pStart,pEnd);
    }
    else
    {
        logv("the_222");
        pBuf = pStream->pData;
    }
    while(nHadProcessLen < pStream->nLength)
    {
        logv("the_222,pBuf=%p,pStart=%p,pEnd=%p\n",pBuf,pStart,pEnd);
        nRemainSize = pStream->nLength-nHadProcessLen;
        tmpBuf[0] = readByteIdx(pBuf, pStart, pEnd, 0);
        tmpBuf[1] = readByteIdx(pBuf, pStart, pEnd, 1);
        tmpBuf[2] = readByteIdx(pBuf, pStart, pEnd, 2);
        tmpBuf[3] = readByteIdx(pBuf, pStart, pEnd, 3);
        tmpBuf[4] = readByteIdx(pBuf, pStart, pEnd, 4);
        tmpBuf[5] = readByteIdx(pBuf, pStart, pEnd, 5);
        logv("%d,%d,%d,%d,%d,%d",tmpBuf[0],tmpBuf[1],tmpBuf[2],tmpBuf[3],tmpBuf[4],tmpBuf[5]);
        nDataSize = (tmpBuf[0] << 24) | (tmpBuf[1] << 16) | (tmpBuf[2] << 8) | tmpBuf[3];
        nForbiddenBit = tmpBuf[4] >> 7; //* read 1 bits
        nTemporalId   = tmpBuf[5] & 0x7;//* read 3 bits
        logv("the_333,nRemainSize=%d,nTemporalIdMinValue=%d,nForbiddenBitValue=%d",
            nRemainSize,nTemporalIdMinValue,nForbiddenBitValue);
        if(nDataSize > (nRemainSize - 4)
           || nDataSize < 0
           || nTemporalId < nTemporalIdMinValue
           || nForbiddenBit != nForbiddenBitValue)
        {
            logv("check stream type fail: nDataSize[%d], streamSize[%d], nTempId[%d], fobBit[%d]",
                 nDataSize, (pStream->nLength-nHadProcessLen),nTemporalId,nForbiddenBit);
            nRet = -1;
            break;
        }
        logv("*** nDataSize = %d, nRemainSize = %d, proceLen = %d, totalLen = %d",
            nDataSize, nRemainSize,
            nHadProcessLen,pStream->nLength);

        if(nDataSize == (nRemainSize - 4) && nDataSize != 0)
        {
            nRet = 0;
            break;
        }

        nHadProcessLen += nDataSize + 4;
        if(pSbm->mConfig.bSecureVideoFlag == 1)
        {
            pBuf = pStart + nHadProcessLen;
        }
        else
        {
            pBuf = pStream->pData + nHadProcessLen;
            if(pBuf - pSbm->pStreamBufferEnd > 0)
            {
                pBuf = pSbm->pStreamBuffer + (pBuf - pSbm->pStreamBufferEnd);
            }
        }
    }

    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        free(pBuf);
        pBuf = NULL;
    }
    return nRet;
}

s32 HevcSbmFrameCheckBitStreamType(SbmFrame* pSbm)
{
    const s32 nUpLimitCount       = 50;
    s32 nReqeustCounter  = 0;
    s32 nRet = VDECODE_RESULT_NO_BITSTREAM;
    s32 bStartCode_with = 0;
    s32 bStartCode_without = 0;

    while(nReqeustCounter < nUpLimitCount)
    {
        VideoStreamDataInfo *pStream = NULL;
        nReqeustCounter++;
        pStream = requestStream(pSbm);
        if(pStream == NULL)
        {
            nRet = VDECODE_RESULT_NO_BITSTREAM;
            break;
        }
        if(pStream->nLength == 0 || pStream->pData == NULL)
        {
            flushStream(pSbm, pStream, 1);
            pStream = NULL;
            continue;
        }

        if(checkBitStreamTypeWithStartCode(pSbm, pStream) == 0)
        {
            bStartCode_with = 1;
        }
        else
        {
            bStartCode_with = 0;
        }

        if(checkBitStreamTypeWithoutStartCode(pSbm, pStream) == 0)
        {
            bStartCode_without = 1;
        }
        else
        {
            bStartCode_without = 0;
        }

        if(bStartCode_with == 1 && bStartCode_without == 1)
        {
            pSbm->bStreamWithStartCode = 0;
        }
        else if(bStartCode_with == 1 && bStartCode_without == 0)
        {
            pSbm->bStreamWithStartCode = 1;
        }
        else if(bStartCode_with == 0 && bStartCode_without == 1)
        {
            pSbm->bStreamWithStartCode = 0;
        }
        else
        {
           pSbm->bStreamWithStartCode = -1;
        }

        logd("result: bStreamWithStartCode[%d], with[%d], whitout[%d]",pSbm->bStreamWithStartCode,
              bStartCode_with, bStartCode_without);

        //*continue reqeust stream from sbm when if judge the stream type
        if(pSbm->bStreamWithStartCode == -1)
        {
            flushStream(pSbm, pStream, 1);
            continue;
        }
        else
        {
            //* judge stream type successfully, return.
            returnStream(pSbm, pStream);
            nRet = 0;
            break;
        }
    }

    return nRet;
}

static void expandNaluList(FramePicInfo* pFramePic)
{
    logd("nalu num for one frame is not enought, expand it: %d, %d",
          pFramePic->nMaxNaluNum, pFramePic->nMaxNaluNum + DEFAULT_NALU_NUM);

    pFramePic->nMaxNaluNum += DEFAULT_NALU_NUM;
    pFramePic->pNaluInfoList = realloc(pFramePic->pNaluInfoList,
                                       pFramePic->nMaxNaluNum*sizeof(NaluInfo));

}

static void chooseFramePts(DetectFramePicInfo* pDetectInfo)
{
    int i;
    pDetectInfo->pCurFramePic->nPts = -1;
    for(i=0; i < MAX_FRAME_PTS_LIST_NUM; i++)
    {
        logv("*** choose pts: %lld, i = %d",pDetectInfo->nCurFramePtsList[i], i);
        if(pDetectInfo->nCurFramePtsList[i] != -1)
        {
            pDetectInfo->pCurFramePic->nPts = pDetectInfo->nCurFramePtsList[i];
            break;
        }
    }
}

static void initFramePicInfo(DetectFramePicInfo* pDetectInfo)
{
    FramePicInfo* pFramePic = pDetectInfo->pCurFramePic;
    pFramePic->bValidFlag = 1;
    pFramePic->nlength = 0;
    pFramePic->pDataStartAddr = NULL;
    pFramePic->nPts = -1;
    pFramePic->nPcr = -1;
    pFramePic->nCurNaluIdx = 0;

    int i;
    for(i = 0; i < MAX_FRAME_PTS_LIST_NUM; i++)
        pDetectInfo->nCurFramePtsList[i] = -1;

    if(pFramePic->nMaxNaluNum > DEFAULT_NALU_NUM)
    {
        pFramePic->nMaxNaluNum   = DEFAULT_NALU_NUM;
        pFramePic->pNaluInfoList = realloc(pFramePic->pNaluInfoList,
                                           pFramePic->nMaxNaluNum*sizeof(NaluInfo));
    }

    memset(pFramePic->pNaluInfoList, 0, pFramePic->nMaxNaluNum*sizeof(NaluInfo));

}

static int searchStartCode(SbmFrame* pSbm, int* pAfterStartCodeIdx)
{
    char* pStart = pSbm->pStreamBuffer;
    char* pEnd   = pSbm->pStreamBufferEnd;
    //char* pBuf = NULL;

    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;


    char* pBuf = pDetectInfo->pCurStreamDataptr;
    s32 nSize = pDetectInfo->nCurStreamDataSize - 3;

    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        pBuf = (char *)malloc(nSize+32);
        if(pBuf == NULL)
        {
            loge("*** malloc pBuf failed, size = %d",pDetectInfo->nCurStreamDataSize);
            return -2;
        }
        if(pDetectInfo->pCurStreamDataptr + pDetectInfo->nCurStreamDataSize <= pEnd)
        {
            CdcMemRead(pSbm->mConfig.memops, pBuf,
                pDetectInfo->pCurStreamDataptr, pDetectInfo->nCurStreamDataSize);
        }
        else
        {
            s32 nFirsrPart;
            nFirsrPart = pEnd - pDetectInfo->pCurStreamDataptr +1;
            CdcMemRead(pSbm->mConfig.memops, pBuf, pDetectInfo->pCurStreamDataptr, nFirsrPart);
            CdcMemRead(pSbm->mConfig.memops, pBuf+nFirsrPart,
                       pStart, pDetectInfo->nCurStreamDataSize - nFirsrPart);

        }
        pStart = pBuf;
        pEnd   = pBuf + pDetectInfo->nCurStreamDataSize;
    }
    else
    {
        pBuf = pDetectInfo->pCurStreamDataptr;
    }

    if(pDetectInfo->nCurStreamRebackFlag)
    {
        logv("bHasTwoDataTrunk pSbmBuf: %p, pSbmBufEnd: %p, curr: %p, diff: %d ",
                pStart, pEnd, pBuf, (u32)(pEnd - pBuf));
        char tmpBuf[3];
        while(nSize > 0)
        {
            tmpBuf[0] = readByteIdx(pBuf , pStart, pEnd, 0);
            tmpBuf[1] = readByteIdx(pBuf , pStart, pEnd, 1);
            tmpBuf[2] = readByteIdx(pBuf , pStart, pEnd, 2);
            if(tmpBuf[0] == 0 && tmpBuf[1] == 0 && tmpBuf[2] == 1)
            {
                (*pAfterStartCodeIdx) += 3; //so that buf[0] is the actual data, not start code
                return 0;
            }
            ptrPlusOne(&pBuf, pStart, pEnd);
            ++(*pAfterStartCodeIdx);
            --nSize;
        }
    }
    else
    {
        while(nSize > 0)
        {
            if(pBuf[0] == 0 && pBuf[1] == 0 && pBuf[2] == 1)
            {
                (*pAfterStartCodeIdx) += 3; //so that buf[0] is the actual data, not start code
                return 0;
            }
            ++pBuf;
            ++(*pAfterStartCodeIdx);
            --nSize;
        }
    }
    return -1;

}

static inline int supplyStreamData(SbmFrame* pSbm)
{
    char* pEnd   = pSbm->pStreamBufferEnd;
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    if(pDetectInfo->pCurStream)
    {
        flushStream(pSbm, pDetectInfo->pCurStream, 0);
        pDetectInfo->pCurStream = NULL;
        if(pDetectInfo->pCurFramePic)
        {
           pDetectInfo->pCurFramePic->nlength += pDetectInfo->nCurStreamDataSize;
        }
        pDetectInfo->nCurStreamDataSize = 0;
        pDetectInfo->pCurStreamDataptr  = NULL;
    }

    VideoStreamDataInfo *pStream = requestStream(pSbm);
    if(pStream == NULL)
    {
        logv("no bit stream");
        //SemTimedWait(&pSbm->streamDataSem, 20);
        return -1;
    }

    if(pStream->nLength <= 0)
    {
        flushStream(pSbm, pStream, 0);
        return -1;
    }

    pDetectInfo->pCurStream = pStream;
    pDetectInfo->pCurStreamDataptr  = pDetectInfo->pCurStream->pData;
    pDetectInfo->nCurStreamDataSize = pDetectInfo->pCurStream->nLength;
    pDetectInfo->nCurStreamRebackFlag = 0;
    if((pDetectInfo->pCurStream->pData + pDetectInfo->pCurStream->nLength) > pEnd)
    {
        pDetectInfo->nCurStreamRebackFlag = 1;
    }
    return 0;

}

static void disposeInvalidStreamData(SbmFrame* pSbm)
{
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    int bNeedAddFramePic = 0;
    logd("**1 pCurFramePic->nlength = %d, flag = %d",pDetectInfo->pCurFramePic->nlength,
         (pDetectInfo->pCurStreamDataptr == pDetectInfo->pCurStream->pData));
    if(pDetectInfo->pCurStreamDataptr == pDetectInfo->pCurStream->pData
       && pDetectInfo->pCurFramePic->nlength == 0)
    {
        pDetectInfo->pCurFramePic->pDataStartAddr = pDetectInfo->pCurStream->pData;
        pDetectInfo->pCurFramePic->nlength = pDetectInfo->pCurStream->nLength;
        pDetectInfo->pCurFramePic->bValidFlag = 0;
        bNeedAddFramePic = 1;
    }
    else
    {
        pDetectInfo->pCurFramePic->nlength += pDetectInfo->nCurStreamDataSize;
        logd("**2, pCurFramePic->nlength = %d, diff = %d",pDetectInfo->pCurFramePic->nlength,
             pDetectInfo->pCurFramePic->nlength - MAX_INVALID_STREAM_DATA_SIZE);

        if(pDetectInfo->pCurFramePic->nlength > MAX_INVALID_STREAM_DATA_SIZE)
        {
            pDetectInfo->pCurFramePic->bValidFlag = 0;
            bNeedAddFramePic = 1;
        }
    }

    logd("bNeedAddFramePic = %d",bNeedAddFramePic );
    flushStream(pSbm, pDetectInfo->pCurStream, 0);
    pDetectInfo->pCurStream = NULL;
    pDetectInfo->pCurStreamDataptr = NULL;
    pDetectInfo->nCurStreamDataSize = 0;

    if(bNeedAddFramePic)
    {

        addFramePic(pSbm, pDetectInfo->pCurFramePic);
        pDetectInfo->pCurFramePic = NULL;
    }

}

static inline void skipCurStreamDataBytes(SbmFrame* pSbm, int nSkipSize)
{
    char* pStart = pSbm->pStreamBuffer;
    char* pEnd   = pSbm->pStreamBufferEnd;
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    pDetectInfo->pCurStreamDataptr += nSkipSize;
    pDetectInfo->nCurStreamDataSize -= nSkipSize;
    if(pDetectInfo->pCurStreamDataptr > pEnd)
    {
        pDetectInfo->pCurStreamDataptr = pStart + (pDetectInfo->pCurStreamDataptr - pEnd - 1);
    }
    pDetectInfo->pCurFramePic->nlength += nSkipSize;

}

static inline void storeNaluInfo(SbmFrame* pSbm, int nNaluType, int nNaluSize, char* pNaluBuf)
{
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    int nNaluIdx = pDetectInfo->pCurFramePic->nCurNaluIdx;
    NaluInfo* pNaluInfo = &pDetectInfo->pCurFramePic->pNaluInfoList[nNaluIdx];
    logv("*** nNaluIdx = %d, pts = %lld",nNaluIdx, pDetectInfo->pCurStream->nPts);
    if(nNaluIdx < MAX_FRAME_PTS_LIST_NUM)
        pDetectInfo->nCurFramePtsList[nNaluIdx] = pDetectInfo->pCurStream->nPts;

    pNaluInfo->nType = nNaluType;
    pNaluInfo->pDataBuf = pNaluBuf;
    pNaluInfo->nDataSize = nNaluSize;
    pDetectInfo->pCurFramePic->nCurNaluIdx++;
    if(pDetectInfo->pCurFramePic->nCurNaluIdx >= pDetectInfo->pCurFramePic->nMaxNaluNum)
    {
        expandNaluList(pDetectInfo->pCurFramePic);
    }

}

/*
detect step:
    1. request bit stream
    2. find startCode
    3.  read the naluType and bFirstSliceSegment
    4. skip nAfterStartCodeIdx
    5. find the next startCode to determine size of cur nalu
    6. store  nalu info
    7. skip naluSize bytes
*/
static void detectWithStartCode(SbmFrame* pSbm)
{
    char tmpBuf[6] = {0};
    char* pStart = pSbm->pStreamBuffer;
    char* pEnd   = pSbm->pStreamBufferEnd;
    int   bFirstSliceSegment = 0;

    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    if(pDetectInfo->pCurFramePic == NULL)
    {
       pDetectInfo->pCurFramePic = requestEmptyFramePic(pSbm);
       if(pDetectInfo->pCurFramePic == NULL)
       {
            SemTimedWait(&pSbm->emptyFramePicSem, 20);
            return ;
       }
       initFramePicInfo(pDetectInfo);
    }

    while(1)
    {

        //*1. request bit stream
        if(pDetectInfo->nCurStreamDataSize < 5 || pDetectInfo->pCurStreamDataptr == NULL)
        {
            if(supplyStreamData(pSbm) != 0)
            {
                if(/*pDetectInfo->bCurFrameStartCodeFound == 1 &&*/ pSbm->nEosFlag == 1)
                {
                    if(pDetectInfo->pCurFramePic->nlength != 0)
                    {
                        pDetectInfo->bCurFrameStartCodeFound = 0;
                        chooseFramePts(pDetectInfo);
                        addFramePic(pSbm, pDetectInfo->pCurFramePic);
                        logv("find eos, flush last stream frame, pts = %lld",
                          (long long int)pDetectInfo->pCurFramePic->nPts);
                        pDetectInfo->pCurFramePic = NULL;
                     }
                }
                SemTimedWait(&pSbm->streamDataSem, 20);
                return ;
            }
        }

        if(pDetectInfo->pCurFramePic->pDataStartAddr == NULL)
        {
           pDetectInfo->pCurFramePic->pDataStartAddr = pDetectInfo->pCurStreamDataptr;
           pDetectInfo->pCurFramePic->pVideoInfo     = pDetectInfo->pCurStream->pVideoInfo;
        }

        //*2. find startCode
        int nAfterStartCodeIdx = 0;
        int nRet = searchStartCode(pSbm,&nAfterStartCodeIdx);
        if(nRet != 0 //*  can not find startCode
           || pDetectInfo->pCurFramePic->nCurNaluIdx > MAX_NALU_NUM_IN_FRAME)
        {
            logw("can not find startCode, curNaluIdx = %d, max = %d",
                  pDetectInfo->pCurFramePic->nCurNaluIdx, MAX_NALU_NUM_IN_FRAME);
            disposeInvalidStreamData(pSbm);
            return ;
        }

        //* now had find the startCode
        //*3.  read the naluType and bFirstSliceSegment
        char* pAfterStartCodeBuf = pDetectInfo->pCurStreamDataptr + nAfterStartCodeIdx;

        if(pSbm->mConfig.bSecureVideoFlag == 1)
        {
            if((pAfterStartCodeBuf) <= pEnd)
            {
                CdcMemRead(pSbm->mConfig.memops, &tmpBuf[0], pAfterStartCodeBuf, 1);
            }
            else
            {
                s32 d = (s32)(pAfterStartCodeBuf-pEnd) - 1;
                CdcMemRead(pSbm->mConfig.memops, &tmpBuf[0], pStart+d, 1);
            }
        }
        else
        {
            tmpBuf[0] = readByteIdx(pAfterStartCodeBuf ,pStart, pEnd, 0);
        }
        int nNaluType = (tmpBuf[0] & 0x7e) >> 1;

        logv("*** nNaluType = %d",nNaluType);
        if((nNaluType >= SBM_HEVC_NAL_VPS && nNaluType <= SBM_HEVC_NAL_AUD) ||
            nNaluType == SBM_HEVC_NAL_SEI_PREFIX)
        {
            /* Begining of access unit, needn't bFirstSliceSegment */
            if(pDetectInfo->bCurFrameStartCodeFound == 1)
            {
                pDetectInfo->bCurFrameStartCodeFound = 0;
                chooseFramePts(pDetectInfo);
                addFramePic(pSbm, pDetectInfo->pCurFramePic);
                pDetectInfo->pCurFramePic = NULL;
                return ;
            }
        }

        if(IsFrameNalu(nNaluType))
        {
            if(pSbm->mConfig.bSecureVideoFlag == 1)
            {
                if((pAfterStartCodeBuf+2) <= pEnd)
                {
                    CdcMemRead(pSbm->mConfig.memops, &tmpBuf[2], pAfterStartCodeBuf+2, 1);
                }
                else
                {
                    s32 d = (s32)(pAfterStartCodeBuf+2-pEnd) - 1;
                    CdcMemRead(pSbm->mConfig.memops, &tmpBuf[2], pStart+d, 1);
                }
            }
            else
            {
                tmpBuf[2] = readByteIdx(pAfterStartCodeBuf ,pStart, pEnd, 2);
            }
            bFirstSliceSegment = (tmpBuf[2] >> 7);
            logv("***bFirstSliceSegment = %d", bFirstSliceSegment);
            if(bFirstSliceSegment == 1)
            {
                if(pDetectInfo->bCurFrameStartCodeFound == 0)
                {
                    logv("pCurFramePic = %p, pCurStream = %p",
                         pDetectInfo->pCurFramePic, pDetectInfo->pCurStream);
                    pDetectInfo->bCurFrameStartCodeFound = 1;
                    pDetectInfo->pCurFramePic->nFrameNaluType = nNaluType;
                }
                else
                {
                    logv("**** have found one frame pic ****");
                    pDetectInfo->bCurFrameStartCodeFound = 0;
                    chooseFramePts(pDetectInfo);
                    addFramePic(pSbm, pDetectInfo->pCurFramePic);
                    pDetectInfo->pCurFramePic = NULL;
                    return ;
                }
            }
        }

        //*if code run here, it means that this is the normal nalu of new frame, we should store it
        //*4. skip nAfterStartCodeIdx
        skipCurStreamDataBytes(pSbm, nAfterStartCodeIdx);

        //*5. find the next startCode to determine size of cur nalu
        int nNaluSize = 0;
        nAfterStartCodeIdx = 0;
        nRet = searchStartCode(pSbm,&nAfterStartCodeIdx);
        if(nRet != 0)//* can not find next startCode
        {
            nNaluSize = pDetectInfo->nCurStreamDataSize;
        }
        else
        {
            nNaluSize = nAfterStartCodeIdx - 3; //* 3 is the length of startCode
        }

        //*6. store  nalu info
        storeNaluInfo(pSbm, nNaluType, nNaluSize, pDetectInfo->pCurStreamDataptr);

        //*7. skip naluSize bytes
        skipCurStreamDataBytes(pSbm, nNaluSize);
    }

    return ;
}

/*
detect step:
    1. request bit stream
    2. read nalu size
    3. read the naluType and bFirstSliceSegment
    4. skip 4 bytes
    5. store  nalu info
    6. skip naluSize bytes
*/
static void detectWithoutStartCode(SbmFrame* pSbm)
{
    char tmpBuf[6] = {0};
    char pBuf[6] = {0};
    char* pStart1;
    char* pEnd1;
    char* pStart = pSbm->pStreamBuffer;
    char* pEnd   = pSbm->pStreamBufferEnd;
    unsigned int bFirstSliceSegment=0;
    const int nPrefixBytes = 4; // indicate data length
    //int i = 0;
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    if(pDetectInfo->pCurFramePic == NULL)
    {
       pDetectInfo->pCurFramePic = requestEmptyFramePic(pSbm);
       if(pDetectInfo->pCurFramePic == NULL)
       {
            SemTimedWait(&pSbm->emptyFramePicSem, 20);
            return ;
       }
       initFramePicInfo(pDetectInfo);
    }

    while(1)
    {
        //*1. request bit stream
        if(pDetectInfo->nCurStreamDataSize < 5 || pDetectInfo->pCurStreamDataptr == NULL)
        {
            if(supplyStreamData(pSbm) != 0)
            {
                if((pDetectInfo->bCurFrameStartCodeFound == 1) ||(pSbm->nEosFlag == 1))
                {
                    if(pDetectInfo->pCurFramePic->nlength != 0)
                    {
                        pDetectInfo->bCurFrameStartCodeFound = 0;
                        chooseFramePts(pDetectInfo);
                        addFramePic(pSbm, pDetectInfo->pCurFramePic);
                        pDetectInfo->pCurFramePic = NULL;
                    }
                }
                SemTimedWait(&pSbm->streamDataSem, 20);
                return ;
            }
        }

        if(pDetectInfo->pCurFramePic->pDataStartAddr == NULL)
        {
           pDetectInfo->pCurFramePic->pDataStartAddr = pDetectInfo->pCurStreamDataptr;
           pDetectInfo->pCurFramePic->pVideoInfo     = pDetectInfo->pCurStream->pVideoInfo;
        }

        //*2. read nalu size
        if(pSbm->mConfig.bSecureVideoFlag == 1)
        {
            if((pDetectInfo->pCurStreamDataptr + 4) <= pEnd)
            {
                CdcMemRead(pSbm->mConfig.memops, pBuf, pDetectInfo->pCurStreamDataptr, 4);
            }
            else
            {
                s32 nLastPart = (s32)(pEnd - pDetectInfo->pCurStreamDataptr) + 1;
                CdcMemRead(pSbm->mConfig.memops, pBuf, pDetectInfo->pCurStreamDataptr, nLastPart);
                CdcMemRead(pSbm->mConfig.memops, pBuf+nLastPart, pStart, 4-nLastPart);
            }
            pStart1 = pBuf;
            pEnd1 = pBuf +3;
        }

        if(pSbm->mConfig.bSecureVideoFlag == 1)
        {
            tmpBuf[0] = readByteIdx(pBuf ,pStart1, pEnd1, 0);
            tmpBuf[1] = readByteIdx(pBuf ,pStart1, pEnd1, 1);
            tmpBuf[2] = readByteIdx(pBuf ,pStart1, pEnd1, 2);
            tmpBuf[3] = readByteIdx(pBuf ,pStart1, pEnd1, 3);
        }
        else
        {
            tmpBuf[0] = readByteIdx(pDetectInfo->pCurStreamDataptr ,pStart,pEnd, 0);
            tmpBuf[1] = readByteIdx(pDetectInfo->pCurStreamDataptr ,pStart,pEnd, 1);
            tmpBuf[2] = readByteIdx(pDetectInfo->pCurStreamDataptr ,pStart,pEnd, 2);
            tmpBuf[3] = readByteIdx(pDetectInfo->pCurStreamDataptr ,pStart,pEnd, 3);
        }
        unsigned int nNaluSize = 0;
        nNaluSize = (tmpBuf[0] << 24) | (tmpBuf[1] << 16) | (tmpBuf[2] << 8) | tmpBuf[3];
        logv("*** read nalu size = %u, ",nNaluSize);
        if(nNaluSize > (unsigned int)(pDetectInfo->nCurStreamDataSize - nPrefixBytes)
           || nNaluSize == 0
           || pDetectInfo->pCurFramePic->nCurNaluIdx > MAX_NALU_NUM_IN_FRAME)
        {
            logw(" error: nNaluSize[%u] > nCurStreamDataSize[%d], curNaluIdx = %d, max = %d",
                   nNaluSize, pDetectInfo->nCurStreamDataSize,
                   pDetectInfo->pCurFramePic->nCurNaluIdx, MAX_NALU_NUM_IN_FRAME);
            disposeInvalidStreamData(pSbm);
            return ;
        }

        //*3. read the naluType and bFirstSliceSegment
        char* pAfterStartCodePtr = NULL;
        unsigned int nNaluType=0;
        pAfterStartCodePtr = pDetectInfo->pCurStreamDataptr + nPrefixBytes;

        if(pSbm->mConfig.bSecureVideoFlag == 1)
        {
            if((pAfterStartCodePtr) <= pEnd)
            {
                CdcMemRead(pSbm->mConfig.memops, &tmpBuf[0], pAfterStartCodePtr, 1);
            }
            else
            {
                s32 d = (s32)(pAfterStartCodePtr-pEnd) - 1;
                CdcMemRead(pSbm->mConfig.memops, &tmpBuf[0], pStart+d, 1);
            }
        }
        else
        {
            tmpBuf[0] = readByteIdx(pAfterStartCodePtr ,pStart, pEnd, 0);
        }

        nNaluType = (tmpBuf[0] & 0x7e) >> 1;
        logv("*** nNaluType = %d",nNaluType);
        if((nNaluType >= SBM_HEVC_NAL_VPS && nNaluType <= SBM_HEVC_NAL_AUD) ||
            nNaluType == SBM_HEVC_NAL_SEI_PREFIX)
        {
            /* Begining of access unit, needn't bFirstSliceSegment */
            if(pDetectInfo->bCurFrameStartCodeFound == 1)
            {
                pDetectInfo->bCurFrameStartCodeFound = 0;
                chooseFramePts(pDetectInfo);
                addFramePic(pSbm, pDetectInfo->pCurFramePic);
                pDetectInfo->pCurFramePic = NULL;
                return ;
            }
        }

        if(IsFrameNalu(nNaluType))
        {
            if(pSbm->mConfig.bSecureVideoFlag == 1)
            {
                if((pAfterStartCodePtr+2) <= pEnd)
                {
                    CdcMemRead(pSbm->mConfig.memops, &tmpBuf[2], pAfterStartCodePtr+2, 1);
                }
                else
                {
                    s32 d = (s32)(pAfterStartCodePtr+2-pEnd) - 1;
                    CdcMemRead(pSbm->mConfig.memops, &tmpBuf[2], pStart+d, 1);
                }
            }
            else
            {
                tmpBuf[2] = readByteIdx(pAfterStartCodePtr ,pStart, pEnd, 2);
            }

            bFirstSliceSegment = (tmpBuf[2] >> 7);
            logv("***bFirstSliceSegment = %d", bFirstSliceSegment);

            if(bFirstSliceSegment == 1)
            {
                if(pDetectInfo->bCurFrameStartCodeFound == 0)
                {
                    pDetectInfo->bCurFrameStartCodeFound = 1;
                    pDetectInfo->pCurFramePic->nFrameNaluType = nNaluType;
                }
                else
                {
                    logv("**** have found one frame pic ****");
                    pDetectInfo->bCurFrameStartCodeFound = 0;
                    chooseFramePts(pDetectInfo);
                    addFramePic(pSbm, pDetectInfo->pCurFramePic);
                    pDetectInfo->pCurFramePic = NULL;
                    return ;
                }
            }
        }

        //*4. skip 4 bytes
        skipCurStreamDataBytes(pSbm, nPrefixBytes);

        //*5. store  nalu info
        storeNaluInfo(pSbm, nNaluType, nNaluSize, pDetectInfo->pCurStreamDataptr);

        //*6. skip naluSize bytes
        skipCurStreamDataBytes(pSbm, nNaluSize);
    }
    return ;
}

void HevcSbmFrameDetectOneFramePic(SbmFrame* pSbm)
{
    logv("pSbm->bStreamWithStartCode = %d",pSbm->bStreamWithStartCode);
    if(pSbm->bStreamWithStartCode == 1)
    {
        detectWithStartCode(pSbm);
    }
    else
    {
        detectWithoutStartCode(pSbm);
    }

}



