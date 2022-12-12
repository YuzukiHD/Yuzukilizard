
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
#include "cdc_log.h"

#define SBM_FRAME_FIFO_SIZE (2048)  //* store 2048 frames of bitstream data at maximum.
#define MAX_INVALID_STREAM_DATA_SIZE (1*1024*1024) //* 1 MB
#define MAX_NALU_NUM_IN_FRAME (1024)
#define min(x, y)   ((x) <= (y) ? (x) : (y));
#define ADD_ONE_FRAME   (0)

extern VideoStreamDataInfo *requestStream(SbmFrame *pSbm);
extern int flushStream(SbmFrame *pSbm, VideoStreamDataInfo *pDataInfo, int bFlush);
extern int returnStream(SbmFrame* pSbm , VideoStreamDataInfo *pDataInfo);
extern int addFramePic(SbmFrame* pSbm, FramePicInfo* pFramePic); //* addFramePic
extern FramePicInfo* requestEmptyFramePic(SbmFrame* pSbm);

#define IsFrameNalu(eNaluType) (eNaluType <= 5 || eNaluType == 20 || eNaluType == 14)

#if ADD_ONE_FRAME
static inline u8 getbits8(char* buffer, u32 start, u8 len)
{
    u32 i = 0;
    u8  n = 0;
    u8  w = 0;
    u8  k = 0;
    u8  ret = 0;
    n = start % 8;
    i = start / 8;
    w = 8 - n;
    k = (len > w ? len - w : 0);

    ret = (buffer[i] << n);
    if(8 > len)
        ret  >>= (8-len);
    if(k)
        ret |= (buffer[i+1] >> (8-k));

    return  ret;
}

static inline u32 readGolomb(char* buffer, u32* init)
{
    u32 y, w = 0, w2 = 0, k, len = 0, m = *init;

    while(getbits8(buffer, m++, 1) == 0)
        len++;

    y = len + m;
    while(m < y)
    {
        k = min(y - m, 8);
        w |= getbits8(buffer, m, k);
        m += k;
        if(y - m > 8)
            w <<= 8;
    }

    w2 = 1;
    for(m = 0; m < len; m++)
        w2 <<= 1;
    w2 = (w2 - 1) + w;

    //fprintf(stderr, "READ_GOLOMB(%u), V=2^%u + %u-1 = %u\n", *init, len, w, w2);
    *init = y;
    return w2;
}
#endif

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

static s32 checkTmpBufferIsEnough(SbmFrame* pSbm, int nSize)
{
    if(pSbm->nTmpBufferSize < nSize)
    {
        pSbm->nTmpBufferSize += TMP_BUFFER_SIZE;

        logw("** tmp buffer is not enough, remalloc to : %d MB",pSbm->nTmpBufferSize/1024/1024);

        free(pSbm->pTmpBuffer);

        pSbm->pTmpBuffer = (char*)malloc(pSbm->nTmpBufferSize);
        if(pSbm->pTmpBuffer == NULL)
        {
            loge("*** malloc for tmp buffer failed");
            abort();
        }

        pSbm->pDetectBufStart = pSbm->pTmpBuffer;
        pSbm->pDetectBufEnd   = pSbm->pTmpBuffer + pSbm->nTmpBufferSize - 1;
    }
    return 0;
}

static s32 selectCheckBuffer(SbmFrame* pSbm,VideoStreamDataInfo *pStream, char** ppBuf)
{
    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        checkTmpBufferIsEnough(pSbm, pStream->nLength);

        int nRingBufSize = pSbm->pStreamBufferEnd - pStream->pData + 1;

        if(pStream->nLength <= nRingBufSize)
        {
            CdcMemRead(pSbm->mConfig.memops, pSbm->pTmpBuffer, pStream->pData, pStream->nLength);
        }
        else
        {
            logw("** buffer ring, %d, %d",pStream->nLength, nRingBufSize);
            CdcMemRead(pSbm->mConfig.memops, pSbm->pTmpBuffer, pStream->pData, nRingBufSize);
            CdcMemRead(pSbm->mConfig.memops, pSbm->pTmpBuffer + nRingBufSize,
                       pSbm->pStreamBuffer, pStream->nLength - nRingBufSize);
        }
        *ppBuf   = pSbm->pTmpBuffer;
    }
    else
    {
        *ppBuf   = pStream->pData;
    }
    return 0;
}

static s32 checkBitStreamTypeWithStartCode(SbmFrame* pSbm,
                                           VideoStreamDataInfo *pStream)
{
    char *pBuf = NULL;
    char tmpBuf[4] = {0};
    const s32 nTsStreamType       = 0x000001;
    char* pStart = pSbm->pDetectBufStart;
    char* pEnd   = pSbm->pDetectBufEnd;
    s32 nHadCheckBytesLen = 0;
    s32 nCheck4BitsValue = -1;
    s32 nBufSize = pStream->nLength;
    //*1. process sbm-cycle-buffer case
    selectCheckBuffer(pSbm, pStream, &pBuf);

    while((nHadCheckBytesLen + 4) < nBufSize)
    {

        tmpBuf[0] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 0);
        tmpBuf[1] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 1);
        tmpBuf[2] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 2);
        tmpBuf[3] = readByteIdx(pBuf, pStart, pEnd, nHadCheckBytesLen + 3);

        nCheck4BitsValue = (tmpBuf[0] << 24) | (tmpBuf[1] << 16) | (tmpBuf[2] << 8) | tmpBuf[3];
        if(nCheck4BitsValue == 0) //*compatible for the case: 00 00 00 00 00 00 00 01
        {
            nHadCheckBytesLen++;
            continue;
        }

        if(nCheck4BitsValue == nTsStreamType)
        {
            pSbm->bStreamWithStartCode = 1;
            return 0;
        }
        else if((nCheck4BitsValue >> 8) == nTsStreamType)
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

    return -1;
}

static s32 checkBitStreamTypeWithoutStartCode(SbmFrame* pSbm,
                                           VideoStreamDataInfo *pStream)
{
    char *pBuf = NULL;
    char tmpBuf[4] = {0};
    s32 nDataSize   = 0;
    s32 nRemainSize = -1;
    s32 nRet = -1;
    char* pStart = pSbm->pDetectBufStart;
    char* pEnd   = pSbm->pDetectBufEnd;

    s32 nHadProcessLen = 0;
	int i = 0;
    selectCheckBuffer(pSbm, pStream, &pBuf);
    while(nHadProcessLen < pStream->nLength)
    {
       nDataSize = 0;
       nRemainSize = pStream->nLength-nHadProcessLen;
       logv("***** nRemainSize=%d\n", nRemainSize);
       logv("***** pSbm.mConfig.nNaluLength=%d\n", pSbm->mConfig.nNaluLength);
       for(i = 0; i < pSbm->mConfig.nNaluLength; i++)
       {
           tmpBuf[i] = readByteIdx(pBuf, pStart, pEnd, i);
		   logv("***** tmpBuf[%d] =%d \n", i, tmpBuf[i]);
		   nDataSize <<= 8;
		   nDataSize |= tmpBuf[i];
           logv("***** in the end nDataSize=%d \n", nDataSize);
       }
	   logv("***** over nDataSize=%d \n", nDataSize);
	   logv("***** start judge the return value *******");
       if(nDataSize > (nRemainSize -  pSbm->mConfig.nNaluLength)|| nDataSize < 0)
       {
            nRet = -1;
            break;
       }
       logv("*** nDataSize = %d, nRemainSize = %d, proceLen = %d, totalLen = %d",
          nDataSize, nRemainSize,nHadProcessLen,pStream->nLength);
       if(nDataSize == (nRemainSize -  pSbm->mConfig.nNaluLength) && nDataSize != 0)
       {
            nRet = 0;
            break;
       }

       nHadProcessLen += nDataSize + pSbm->mConfig.nNaluLength;
       pBuf += nDataSize + pSbm->mConfig.nNaluLength;
    }

    return nRet;
}

s32 AvcSbmFrameCheckBitStreamType(SbmFrame* pSbm)
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

        logd("result: bStreamWithStartCode[%d], with[%d], whitout[%d]",
              pSbm->bStreamWithStartCode,
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
    char* pStart = pSbm->pDetectBufStart;
    char* pEnd   = pSbm->pDetectBufEnd;

    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    char *pBuf = pDetectInfo->pCurStreamDataptr;
    s32 nSize = pDetectInfo->nCurStreamDataSize - 3;

    if(pDetectInfo->nCurStreamRebackFlag)
    {
        logv("bHasTwoDataTrunk pSbmBuf: %p, pSbmBufEnd: %p, curr: %p, diff: %d ",
                pStart, pEnd, pBuf, (u32)(pEnd - pBuf));
        while(nSize > 0)
        {
            char tmpBuf[3];
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
        //SemTimedWait(&pSbm->streamDataSem, 20);
        return -1;
    }

    if(pStream->nLength <= 0)
    {
        flushStream(pSbm, pStream, 0);
        return -1;
    }
    pDetectInfo->pCurStream = pStream;
    pDetectInfo->nCurStreamDataSize = pDetectInfo->pCurStream->nLength;

    selectCheckBuffer(pSbm, pStream, &pDetectInfo->pCurStreamDataptr);

    pDetectInfo->nCurStreamRebackFlag = 0;
    if(pSbm->mConfig.bSecureVideoFlag == 0)
    {
       int nRingBufSize = pSbm->pStreamBufferEnd - pDetectInfo->pCurStream->pData + 1;
       if(pDetectInfo->pCurStream->nLength > nRingBufSize)
           pDetectInfo->nCurStreamRebackFlag = 1;
    }
    return 0;

}

static void disposeInvalidStreamData(SbmFrame* pSbm)
{
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    int bNeedAddFramePic = 0;
    char* pStartBuf = NULL;

    if(pSbm->mConfig.bSecureVideoFlag == 1)
    {
        pStartBuf = pSbm->pTmpBuffer;
    }
    else
    {
        pStartBuf = pDetectInfo->pCurStream->pData;
    }

    logd("**1 pCurFramePic->nlength = %d, flag = %d",pDetectInfo->pCurFramePic->nlength,
         (pDetectInfo->pCurStreamDataptr == pStartBuf));

    if(pDetectInfo->pCurStreamDataptr == pStartBuf
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

    logd("disposeInvalidStreamData: bNeedAddFramePic = %d",bNeedAddFramePic);
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
    char* pStart = pSbm->pDetectBufStart;
    char* pEnd   = pSbm->pDetectBufEnd;
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    pDetectInfo->pCurStreamDataptr += nSkipSize;
    pDetectInfo->nCurStreamDataSize -= nSkipSize;
    if(pDetectInfo->pCurStreamDataptr > pEnd)
    {
        pDetectInfo->pCurStreamDataptr = pStart + (pDetectInfo->pCurStreamDataptr - pEnd - 1);
    }
    pDetectInfo->pCurFramePic->nlength += nSkipSize;

}

static inline void storeNaluInfo(SbmFrame* pSbm, int nNaluType,
                                    int nNalRefIdc, int nNaluSize, char* pNaluBuf)
{
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;

    int nNaluIdx = pDetectInfo->pCurFramePic->nCurNaluIdx;
    NaluInfo* pNaluInfo = &pDetectInfo->pCurFramePic->pNaluInfoList[nNaluIdx];
    logv("*** nNaluIdx = %d, pts = %lld, naluSize = %d, nalutype = %d",
          nNaluIdx, pDetectInfo->pCurStream->nPts, nNaluSize, nNaluType);
    if(nNaluIdx < MAX_FRAME_PTS_LIST_NUM)
        pDetectInfo->nCurFramePtsList[nNaluIdx] = pDetectInfo->pCurStream->nPts;

    pNaluInfo->nNalRefIdc = nNalRefIdc;
    pNaluInfo->nType = nNaluType;
    pNaluInfo->pDataBuf = pNaluBuf;
    pNaluInfo->nDataSize = nNaluSize;
    pDetectInfo->pCurFramePic->nCurNaluIdx++;
    if(pDetectInfo->pCurFramePic->nCurNaluIdx >= pDetectInfo->pCurFramePic->nMaxNaluNum)
    {
        expandNaluList(pDetectInfo->pCurFramePic);
    }
    if(pSbm->mDetectInfo.pCurStream->bVideoInfoFlag == 1)
    {
        pDetectInfo->pCurFramePic->bVideoInfoFlag = 1;
        pSbm->mDetectInfo.pCurStream->bVideoInfoFlag = 0;
    }
}

static char* checkCurStreamDataAddr(SbmFrame* pSbm)
{
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;
    char* pCurStreamDataAddr = NULL;

    int nSize = pDetectInfo->pCurStreamDataptr - pSbm->pDetectBufStart;
    int nRingBufSize = pSbm->pStreamBufferEnd - pDetectInfo->pCurStream->pData + 1;

    if(nSize >= nRingBufSize)
    {
        logd("*** buffer ring ");
        pCurStreamDataAddr = pSbm->pStreamBuffer + (nSize - nRingBufSize);
    }
    else
        pCurStreamDataAddr = pDetectInfo->pCurStream->pData + nSize;

    return pCurStreamDataAddr;
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
    char* pStart = pSbm->pDetectBufStart;
    char* pEnd   = pSbm->pDetectBufEnd;
#if ADD_ONE_FRAME
    int   bFirstSliceSegment = 0;
#endif
    char* pAfterStartCodeBuf = NULL;
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
        int   nRet = -1;

        //*1. request bit stream
        if(pDetectInfo->nCurStreamDataSize < 5 || pDetectInfo->pCurStreamDataptr == NULL)
        {
            if(supplyStreamData(pSbm) != 0)
            {
                if((pDetectInfo->bCurFrameStartCodeFound == 1) || (pSbm->nEosFlag == 1))
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

        logv("*** new new pSbm->mConfig.bSecureVideoFlag = %d",pSbm->mConfig.bSecureVideoFlag);
        if(pDetectInfo->pCurFramePic->pDataStartAddr == NULL)
        {
           if(pSbm->mConfig.bSecureVideoFlag == 1)
           {
                pDetectInfo->pCurFramePic->pDataStartAddr = checkCurStreamDataAddr(pSbm);
           }
           else
           {
                pDetectInfo->pCurFramePic->pDataStartAddr = pDetectInfo->pCurStreamDataptr;
           }
           pDetectInfo->pCurFramePic->pVideoInfo = pDetectInfo->pCurStream->pVideoInfo;
        }

        //*2. find startCode
        int nAfterStartCodeIdx = 0;
        nRet = searchStartCode(pSbm,&nAfterStartCodeIdx);
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

        pAfterStartCodeBuf = pDetectInfo->pCurStreamDataptr + nAfterStartCodeIdx;
        tmpBuf[0] = readByteIdx(pAfterStartCodeBuf ,pStart, pEnd, 0);

        int nNalRefIdc = (tmpBuf[0]&0x60)?1:0;
        int nNaluType = tmpBuf[0] & 0x1f;
        char* pSliceStartPlusOne = pAfterStartCodeBuf + 1;

        if(pSliceStartPlusOne > pEnd)
            pSliceStartPlusOne -= (pEnd - pStart + 1);

#if ADD_ONE_FRAME
        if((nNaluType >= 6 && nNaluType <= 13) || (nNaluType >= 15 && nNaluType < 20)
             || (nNaluType >= 21 && nNaluType <= 31))

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
            u32 n = 0;
            if(nNaluType == 20 || nNaluType == 14)
                n = 24;
            bFirstSliceSegment = readGolomb(pSliceStartPlusOne, &n);

            logv("***bFirstSliceSegment = %d", bFirstSliceSegment);

            if(!bFirstSliceSegment)//=0
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
#else
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
#endif

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

        logv("gqy***nNaluType =%d, store nalu size = %d, nNalRefIdc = %d",
               nNaluType, nNaluSize, nNalRefIdc);
        //*6. store  nalu info
        char* pNaluBuf = NULL;
        if(pSbm->mConfig.bSecureVideoFlag == 1)
        {
            pNaluBuf = checkCurStreamDataAddr(pSbm);
        }
        else
            pNaluBuf = pDetectInfo->pCurStreamDataptr;

        storeNaluInfo(pSbm, nNaluType, nNalRefIdc, nNaluSize, pNaluBuf);

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
static void  detectWithoutStartCode(SbmFrame* pSbm)
{
    int i = 0;
    char tmpBuf[6] = {0};
    char* pStart = pSbm->pDetectBufStart;
    char* pEnd   = pSbm->pDetectBufEnd;
    char* pAfterStartCodeBuf = NULL;

#if ADD_ONE_FRAME
    unsigned int bFirstSliceSegment = 0;
#endif
    //const int nPrefixBytes = 4; // indicate data length
    DetectFramePicInfo* pDetectInfo = &pSbm->mDetectInfo;
    if(pDetectInfo->pCurFramePic == NULL)
    {
       pDetectInfo->pCurFramePic = requestEmptyFramePic(pSbm);
       if(pDetectInfo->pCurFramePic == NULL)
       {
            SemTimedWait(&pSbm->emptyFramePicSem, 20);
            return ;
       }
       //usleep(10*1000);
       initFramePicInfo(pDetectInfo);
    }

    while(1)
    {
        int   nNaluSize = 0;
        unsigned int nNalRefIdc = 0;
        unsigned int nNaluType = 0;

        //*1. request bit stream
        if(pDetectInfo->nCurStreamDataSize < 5 || pDetectInfo->pCurStreamDataptr == NULL)
        {
            if(supplyStreamData(pSbm) != 0)
            {
                if((pDetectInfo->bCurFrameStartCodeFound == 1) || (pSbm->nEosFlag == 1))
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
           if(pSbm->mConfig.bSecureVideoFlag == 1)
           {
                pDetectInfo->pCurFramePic->pDataStartAddr = checkCurStreamDataAddr(pSbm);
           }
           else
           {
                pDetectInfo->pCurFramePic->pDataStartAddr = pDetectInfo->pCurStreamDataptr;
           }
           pDetectInfo->pCurFramePic->pVideoInfo     = pDetectInfo->pCurStream->pVideoInfo;
        }

        //*2. read nalu size
		for(i = 0; i < pSbm->mConfig.nNaluLength; i++)
		{
			tmpBuf[i] = readByteIdx(pDetectInfo->pCurStreamDataptr, pStart, pEnd, i);
			nNaluSize <<= 8;
			nNaluSize |= tmpBuf[i];
		}
		logv("***** nNaluSize=%d \n", nNaluSize);
		logv("***** pDetectInfo->pCurFramePic->nCurNaluIdx = %d \n",
           pDetectInfo->pCurFramePic->nCurNaluIdx);
        if(nNaluSize > (pDetectInfo->nCurStreamDataSize - pSbm->mConfig.nNaluLength)//nPrefixBytes replace with pSbm->mConfig.nNaluLength
           || nNaluSize <= 0
           || pDetectInfo->pCurFramePic->nCurNaluIdx > MAX_NALU_NUM_IN_FRAME)
        {
            logw(" error: nNaluSize[%u] > nCurStreamDataSize[%d], curNaluIdx = %d, max = %d",
                   nNaluSize, pDetectInfo->nCurStreamDataSize,
                   pDetectInfo->pCurFramePic->nCurNaluIdx, MAX_NALU_NUM_IN_FRAME);
            disposeInvalidStreamData(pSbm);
            return ;
        }

        //*3. read the naluType and bFirstSliceSegment

        pAfterStartCodeBuf = pDetectInfo->pCurStreamDataptr + pSbm->mConfig.nNaluLength;
        tmpBuf[0] = readByteIdx(pAfterStartCodeBuf ,pStart, pEnd, 0);

        nNalRefIdc = (tmpBuf[0]&0x60)?1:0;
        nNaluType = tmpBuf[0] & 0x1f;
        char* pSliceStartPlusOne = pAfterStartCodeBuf + 1;

        if(pSliceStartPlusOne > pEnd)
        {
            pSliceStartPlusOne -= (pEnd - pStart + 1);
        }


        logv("*** nNaluType = %d, nNalRefIdc = %d",nNaluType, nNalRefIdc);

#if ADD_ONE_FRAME
        if((nNaluType >= 6 && nNaluType <= 13) || (nNaluType >= 15 && nNaluType < 20)
             || (nNaluType >= 21 && nNaluType <= 31))
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
            u32 n = 0;
            if(nNaluType == 20 || nNaluType == 14)
                n = 24;
            bFirstSliceSegment = readGolomb(pSliceStartPlusOne, &n);

            if(!bFirstSliceSegment)//=0
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
#else
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
#endif

        //*4. skip 4 bytes
        skipCurStreamDataBytes(pSbm, pSbm->mConfig.nNaluLength);
        logv("gqy***nNaluType =%d, store nalu size = %d, nNalRefIdc = %d",
             nNaluType, nNaluSize, nNalRefIdc);

        //*5. store  nalu info
        char* pNaluBuf = NULL;
        if(pSbm->mConfig.bSecureVideoFlag == 1)
        {
            pNaluBuf = checkCurStreamDataAddr(pSbm);
        }
        else
            pNaluBuf = pDetectInfo->pCurStreamDataptr;

        storeNaluInfo(pSbm, nNaluType, nNalRefIdc, nNaluSize, pNaluBuf);

        //*6. skip naluSize bytes
        skipCurStreamDataBytes(pSbm, nNaluSize);

    }
    return ;
}

void AvcSbmFrameDetectOneFramePic(SbmFrame* pSbm)
{
    if(pSbm->bStreamWithStartCode == 1)
    {
        detectWithStartCode(pSbm);
    }
    else
    {
        detectWithoutStartCode(pSbm);
    }

}
