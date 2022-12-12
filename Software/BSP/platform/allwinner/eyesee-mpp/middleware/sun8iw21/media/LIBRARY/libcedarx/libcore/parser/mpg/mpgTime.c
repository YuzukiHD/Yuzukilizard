/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : statusChange.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment :
*
*
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "mpgTime"
#include <cdx_log.h>


#include "CdxMpgParser.h"
#include "CdxMpgParserImpl.h"
#include "mpgFunc.h"

void CalculateBaseSCR(CdxMpgParserT *MpgParser, cdx_uint8 *buf, cdx_uint32 *baseSCR)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32 startScrLow  = 0;
    cdx_uint32 startScrHigh = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(mMpgParserCxt->bIsISO11172Flag)
    {
        if(MpgTimeMpeg1ReadPackSCR(buf, &startScrLow,&startScrHigh) == 0)
            *baseSCR = (startScrLow>>1) | (startScrHigh<<31);
    }
    else
    {
        if(MpgTimeMpeg2ReadPackSCR(buf, &startScrLow,&startScrHigh, NULL) == 0)
            *baseSCR = (startScrLow>>1) | (startScrHigh<<31);
    }
}

cdx_bool ComputeFilePts(CdxMpgParserT *MpgParser, cdx_uint8 *cur_buf, cdx_uint32 *pts_val)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32 TS_flag        = 0;
    cdx_uint32 cur_pts_low    = 0;
    cdx_uint32 cur_pts_high   = 0;
    cdx_bool   find_pts_flag  = CDX_FALSE;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    cur_buf += 4;
    if(mMpgParserCxt->bIsISO11172Flag)
    {
        cur_buf += 2;
        while(*cur_buf==0xff)
        {
            cur_buf++;
        }
        if(((*cur_buf)&0xc0)==0x40)
            cur_buf += 2;
        if((((*cur_buf)&0xf0)==0x20) && (cur_buf < mMpgParserCxt->mDataChunkT.pEndPtr))
        {
            MpgTimeMpeg1ReadPackSCR(cur_buf,&cur_pts_low,&cur_pts_high);
            find_pts_flag = CDX_TRUE;
        }
        else if((((*cur_buf)&0xf0)==0x30)&& (cur_buf < mMpgParserCxt->mDataChunkT.pEndPtr))
        {
            MpgTimeMpeg1ReadPackSCR(cur_buf,&cur_pts_low,&cur_pts_high);
            find_pts_flag = CDX_TRUE;
        }
    }
    else
    {
        cur_buf += 3;
        TS_flag = cur_buf[0];
        cur_buf += 2;
        if(((TS_flag&0xc0)==0x80) && (cur_buf < mMpgParserCxt->mDataChunkT.pEndPtr))
        {
            MpgTimeMpeg1ReadPackSCR(cur_buf,&cur_pts_low,&cur_pts_high);
            find_pts_flag = CDX_TRUE;
        }
        else if(((TS_flag&0xc0)==0xc0) && (cur_buf < mMpgParserCxt->mDataChunkT.pEndPtr))
        {
            MpgTimeMpeg1ReadPackSCR(cur_buf,&cur_pts_low,&cur_pts_high);
            find_pts_flag = CDX_TRUE;
        }
    }
    //*pts_val = MpgTimePts2Ms((cur_pts_low>>1)|(cur_pts_high<<31),0,0);
    *pts_val = (cur_pts_low>>1)|(cur_pts_high<<31);
    return find_pts_flag;
}

/********************************
read 11172-1 format SCR
return: 0---success
-1----fail
********************************/

cdx_uint32 MpgTimePts2Ms(cdx_uint32 PTS,cdx_uint32 nBaseSCR,cdx_uint32 nBaseTime)
{
    cdx_uint32 cur_ms;

    if(PTS > nBaseSCR)
    {
        cur_ms = PTS - nBaseSCR;
        cur_ms /= 45; //unit is ms
        cur_ms += nBaseTime;
    }
    else //base_scr >= PTS
    {
        cur_ms = nBaseSCR - PTS;
        cur_ms /= 45; //unit is ms
        if(nBaseTime > cur_ms)
            cur_ms = nBaseTime - cur_ms;
        else
            cur_ms = 0;
    }

    return cur_ms;
}

cdx_int16 MpgTimeMpeg1ReadPackSCR(cdx_uint8 *buf, cdx_uint32 *scr_low,cdx_uint32 *scr_high)
{

    if(scr_low == NULL || scr_high==NULL)
        return -1;
    *scr_high = (cdx_uint32)((buf[0]>>3)&0x01);

    *scr_low = (buf[0]>>1)&0x03;
    *scr_low <<= 8;
    *scr_low |= buf[1];
    *scr_low <<= 7;
    *scr_low |= (buf[2]>>1)&0x7f;
    *scr_low <<= 8;
    *scr_low |= buf[3];
    *scr_low <<= 7;
    *scr_low |= (buf[4]>>1)&0x7f;
    return 0;
}

cdx_int16 MpgTimeMpeg2ReadPackSCR(cdx_uint8 *buf, cdx_uint32 *scr_base_low,
        cdx_uint32 *scr_base_high,cdx_uint32 *scr_ext)
{

    if(scr_base_low == NULL || scr_base_high==NULL)
        return -1;

    *scr_base_high = (cdx_uint32)((buf[0]>>5)&0x01);        //bit 32

    *scr_base_low = (buf[0]&0x38)>>1;// 0011 1000
    *scr_base_low |= (buf[0]&0x03);// 0000 0011
    *scr_base_low <<= 8;
    *scr_base_low |= buf[1];
    *scr_base_low <<= 7;
    *scr_base_low |= ((buf[2]&0xf8)>>1); // 1111 1000
    *scr_base_low |= (buf[2]&0x03);// 0000 0011
    *scr_base_low <<= 8;
    *scr_base_low |= buf[3];
    *scr_base_low <<=5;
    *scr_base_low |= ((buf[4]&0xf8)>>3); // 1111 1000
    if(scr_ext)
    {
        *scr_ext = (buf[4]&0x03);
        *scr_ext <<= 7;
        *scr_ext |= (buf[5]>>1);
    }

    return 0;
}


cdx_int16 MpgTimeCheckPsPts(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt  = NULL;
    cdx_uint8         *buf               = NULL;
    cdx_uint8         *curPtr           = NULL;
    cdx_uint8         *checkStartAddr = NULL;
    cdx_int16  nRet        = 0;
    cdx_int32 readLen     = 0;
    cdx_int32 readDataLen = 0;
    cdx_uint32 scrLow        = 0;
    cdx_uint32 scrHigh     = 0;
    cdx_uint32 nPtsValue   = 0;
    cdx_uint32 endVidPts   = 0;
    cdx_uint32 endAudPts   = 0;
    cdx_uint32 fstVidPts   = 0;
    cdx_uint32 fstAudPts   = 0;
    cdx_uint32 fstScrVal   = 0;
    cdx_uint32 endScrVal   = 0;
    cdx_int64  seekPos     = 0;

    cdx_bool ptsFlag        = CDX_FALSE;
    cdx_bool fstScrFlag     = CDX_FALSE;
    cdx_bool endScrFlag     = CDX_FALSE;
    cdx_bool bFstVidPtsFlag = CDX_FALSE;
    cdx_bool bFstAudPtsFlag = CDX_FALSE;
    cdx_bool endVidPtsFlag  = CDX_FALSE;
    cdx_bool endAudPtsFlag  = CDX_FALSE;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    //step-2  check the first video nPts and the audio nPts
    readDataLen = 0;
    seekPos = 0;
    while(seekPos < mMpgParserCxt->nFileLength)
    {
         if(MpgParser->bForceReturnFlag == 1)
            return FILE_PARSER_OPEN_FILE_FAIL;
        CdxStreamSeek(mMpgParserCxt->pStreamT, seekPos, STREAM_SEEK_SET);
        seekPos += MAX_CHUNK_BUF_SIZE-4;
        readLen = CdxStreamRead(mMpgParserCxt->pStreamT,
            mMpgParserCxt->mDataChunkT.pStartAddr,MAX_CHUNK_BUF_SIZE);
        readDataLen += readLen;
        mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr + readLen;
        checkStartAddr = mMpgParserCxt->mDataChunkT.pStartAddr;
find_pack_start_code1:
        if(MpgParser->bForceReturnFlag == 1)
         return FILE_PARSER_OPEN_FILE_FAIL;

        buf = MpgOpenFindPackStart(checkStartAddr,mMpgParserCxt->mDataChunkT.pEndPtr);
        if((buf == NULL) &&((seekPos==mMpgParserCxt->nFileLength)||(readDataLen >= 0xF00000)))
        {
            if((bFstVidPtsFlag==CDX_FALSE) &&(bFstAudPtsFlag==CDX_FALSE) &&(fstScrFlag==CDX_FALSE))
            {
                mMpgParserCxt->nFileTimeLength = 0;
                return 0;
            }
            break;
        }
        else if(buf != NULL)
        {
            curPtr = MpgOpenJudgePacketType(MpgParser, buf, &nRet);
            if(nRet > 0)
            {
                if(fstScrFlag == CDX_FALSE)
                {
                    if(mMpgParserCxt->bIsISO11172Flag)
                        MpgTimeMpeg1ReadPackSCR(buf+4, &scrLow,&scrHigh);
                    else
                        MpgTimeMpeg2ReadPackSCR(buf+4, &scrLow,&scrHigh,NULL);
                    fstScrVal = (scrLow>>1) | (scrHigh<<31);
                    fstScrFlag = CDX_TRUE;
                }
                ptsFlag = ComputeFilePts(MpgParser, curPtr, &nPtsValue);
                if((nRet == 1) &&(bFstVidPtsFlag == CDX_FALSE)&&(ptsFlag==CDX_TRUE))
                {
                    fstVidPts = nPtsValue;
                    bFstVidPtsFlag = CDX_TRUE;
                    if(bFstAudPtsFlag == CDX_TRUE)
                        break;
                }
                else if((nRet == 2) &&(bFstAudPtsFlag == CDX_FALSE)&&(ptsFlag==CDX_TRUE))
                {
                    fstAudPts = nPtsValue;
                    bFstAudPtsFlag = CDX_TRUE;
                    if(bFstVidPtsFlag == CDX_TRUE)
                        break;
                }
            }
            checkStartAddr = buf+4;
            goto find_pack_start_code1;
        }
    }

    //step -1 check the last video nPts and the audio nPts
    seekPos = mMpgParserCxt->nFileLength;
    readDataLen = 0;
    while(seekPos > 0)
    {
        seekPos = (seekPos > MAX_CHUNK_BUF_SIZE)? (seekPos-MAX_CHUNK_BUF_SIZE) : 0;

        CdxStreamSeek(mMpgParserCxt->pStreamT, seekPos, STREAM_SEEK_SET);
        readLen = CdxStreamRead(mMpgParserCxt->pStreamT,
            mMpgParserCxt->mDataChunkT.pStartAddr,MAX_CHUNK_BUF_SIZE);
        readDataLen += readLen;
        mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr + readLen;
find_pack_start_code0:
        if(MpgParser->bForceReturnFlag == 1)
           return FILE_PARSER_OPEN_FILE_FAIL;
        buf = MpgOpenFindPackStartReverse(mMpgParserCxt->mDataChunkT.pEndPtr,
            mMpgParserCxt->mDataChunkT.pStartAddr);
        if((buf==NULL) &&((seekPos == 0)||(readDataLen >= 0xF00000)))
        {
            break;
        }
        else if(buf != NULL)
        {
            curPtr = MpgOpenJudgePacketType(MpgParser, buf-3, &nRet);
            if(nRet > 0)
            {
                if(endScrFlag == CDX_FALSE)
                {
                    if(mMpgParserCxt->bIsISO11172Flag)
                        MpgTimeMpeg1ReadPackSCR(buf+1, &scrLow,&scrHigh);
                    else
                        MpgTimeMpeg2ReadPackSCR(buf+1, &scrLow,&scrHigh,NULL);
                    endScrVal = (scrLow>>1) | (scrHigh<<31);
                    endScrFlag = CDX_TRUE;
                }
                ptsFlag = ComputeFilePts(MpgParser, curPtr, &nPtsValue);
                if((nRet == 1) &&(endVidPtsFlag == CDX_FALSE) &&(ptsFlag == CDX_TRUE))
                {
                    endVidPts = nPtsValue;
                    endVidPtsFlag = CDX_TRUE;
                    if(endAudPtsFlag == CDX_TRUE)
                        break;
                }
                else if((nRet == 2) &&(endAudPtsFlag == CDX_FALSE)&&(ptsFlag == CDX_TRUE))
                {
                    endAudPts = nPtsValue;
                    endAudPtsFlag = CDX_TRUE;
                    if(endVidPtsFlag == CDX_TRUE)
                        break;
                }
            }
            mMpgParserCxt->mDataChunkT.pEndPtr  = buf-3;
            goto find_pack_start_code0;
        }
        seekPos += 3;
    }

    if((bFstAudPtsFlag == CDX_TRUE) &&(bFstVidPtsFlag==CDX_TRUE))
        mMpgParserCxt->nFirstPts = (fstAudPts < fstVidPts)? fstAudPts: fstVidPts;
    else if(bFstAudPtsFlag == CDX_TRUE)
        mMpgParserCxt->nFirstPts = fstAudPts;
    else if(bFstVidPtsFlag == CDX_TRUE)
        mMpgParserCxt->nFirstPts = fstVidPts;
    else if(fstScrFlag == CDX_TRUE)
        mMpgParserCxt->nFirstPts = fstScrVal;
    else
        mMpgParserCxt->nFirstPts = 0;

    if((endAudPtsFlag == CDX_TRUE) &&(endVidPtsFlag==CDX_TRUE))
        mMpgParserCxt->nEndPts = (endAudPts > endVidPts)? endAudPts: endVidPts;
    else if(endAudPtsFlag == CDX_TRUE)
        mMpgParserCxt->nEndPts = endAudPts;
    else if(endVidPtsFlag == CDX_TRUE)
        mMpgParserCxt->nEndPts = endVidPts;
    else if(endScrFlag == CDX_TRUE)
        mMpgParserCxt->nEndPts = endScrVal;
    else
        mMpgParserCxt->nEndPts = 0;

    mMpgParserCxt->nEndPts = MpgTimePts2Ms(mMpgParserCxt->nEndPts ,0,0);
    mMpgParserCxt->nFirstPts = MpgTimePts2Ms(mMpgParserCxt->nFirstPts ,0,0);
    mMpgParserCxt->nBasePts  = mMpgParserCxt->nFirstPts;

    if(fstScrFlag == CDX_TRUE)
    {
        mMpgParserCxt->nPreSCR   = MpgTimePts2Ms(fstScrVal ,0,0);
        mMpgParserCxt->bHasInitAVSFlag = CDX_TRUE;
    }

    if(mMpgParserCxt->nEndPts > mMpgParserCxt->nFirstPts)
    {
        mMpgParserCxt->nEndPts -= mMpgParserCxt->nFirstPts;
        mMpgParserCxt->nFileTimeLength = mMpgParserCxt->nEndPts;
    }
    else
    {
        mMpgParserCxt->nFileTimeLength  = mMpgParserCxt->nEndPts;
    }
    return 0;
}

cdx_int16 MpgTimeCheckPsNetWorkStreamPts(CdxMpgParserT *MpgParser, u32 startPos)
{
    MpgParserContextT *mMpgParserCxt  = NULL;
    u8 videoIndex = 0;
    u8 audioIndex = 0;
    u32 readDataLen = 0;
    cdx_int64 seekPos = 0;
    u32 validBufferSize = 0;
    u32 readLen = 0;
    s32 nReadLen = 0;
    u8 bFstVidPtsFlag = 0;
    u8 bFstAudPtsFlag = 0;
    u8 fstScrFlag = 0;
    u8 ptsFlag = 0;
    u8 endScrFlag = 0;
    u8 endVidPtsFlag = 0;
    u8 endAudPtsFlag = 0;
    u8 i = 0;
    s16 nRet        = 0;
    cdx_uint32 scrLow        = 0;
    cdx_uint32 scrHigh     = 0;
    cdx_uint32 nPtsValue   = 0;
    cdx_uint32 endVidPts   = 0;
    cdx_uint32 endAudPts   = 0;
    cdx_uint32 fstVidPts   = 0;
    cdx_uint32 fstAudPts   = 0;
    cdx_uint32 fstScrVal   = 0;
    cdx_uint32 endScrVal   = 0;
    cdx_uint8* startBufAddr = NULL;
    cdx_uint8* checkStartAddr = NULL;
    cdx_uint8* buf = NULL;
    cdx_uint8  *curPtr = NULL;
    cdx_uint32 endVidPtsArry[24] = {0};
    cdx_uint32 endAudPtsArry[24] = {0};

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    //step-2  check the first video nPts and the audio nPts
    readDataLen = 0;
    seekPos = (cdx_int64)startPos;
    CdxStreamSeek(mMpgParserCxt->pStreamT, seekPos, STREAM_SEEK_SET);
    validBufferSize = MAX_CHUNK_BUF_SIZE;
    readLen = 0;
    startBufAddr = mMpgParserCxt->mDataChunkT.pStartAddr;
    while(1)
    {
         if(MpgParser->bForceReturnFlag == 1)
         {
             return FILE_PARSER_OPEN_FILE_FAIL;
         }

        //readLen += CdxStreamRead(mMpgParserCxt->pStreamT,startBufAddr,validBufferSize);
         nReadLen = CdxStreamRead(mMpgParserCxt->pStreamT,startBufAddr,validBufferSize);
         if(nReadLen <= 0)
         {
             break;
         }
        readDataLen += readLen+nReadLen;
        mMpgParserCxt->mDataChunkT.pEndPtr =
            mMpgParserCxt->mDataChunkT.pStartAddr + readLen+nReadLen;
        checkStartAddr = mMpgParserCxt->mDataChunkT.pStartAddr;
find_pack_start_code1:
        if(MpgParser->bForceReturnFlag == 1)
        {
            return FILE_PARSER_OPEN_FILE_FAIL;
        }

        buf = MpgOpenFindPackStart(checkStartAddr,mMpgParserCxt->mDataChunkT.pEndPtr);

        if((buf == NULL) &&((seekPos==mMpgParserCxt->nFileLength)||(readDataLen >= 0xF00000)))
        {
            if((bFstVidPtsFlag==CDX_FALSE) &&(bFstAudPtsFlag==CDX_FALSE) &&(fstScrFlag==CDX_FALSE))
            {
                mMpgParserCxt->nFileTimeLength = 0;
                return 0;
            }
            break;
        }
        else if(buf != NULL)
        {
            curPtr = MpgOpenJudgePacketType(MpgParser, buf, &nRet);
            if(nRet > 0)
            {
                if(fstScrFlag == CDX_FALSE)
                {
                    if(mMpgParserCxt->bIsISO11172Flag)
                        MpgTimeMpeg1ReadPackSCR(buf+4, &scrLow,&scrHigh);
                    else
                        MpgTimeMpeg2ReadPackSCR(buf+4, &scrLow,&scrHigh,NULL);
                    fstScrVal = (scrLow>>1) | (scrHigh<<31);
                    fstScrFlag = CDX_TRUE;
                }
                ptsFlag = ComputeFilePts(MpgParser, curPtr, &nPtsValue);
                if((nRet == 1) &&(bFstVidPtsFlag == CDX_FALSE)&&(ptsFlag==CDX_TRUE))
                {
                    fstVidPts = nPtsValue;
                    bFstVidPtsFlag = CDX_TRUE;
                    if(bFstAudPtsFlag == CDX_TRUE)
                        break;
                }
                else if((nRet == 2) &&(bFstAudPtsFlag == CDX_FALSE)&&(ptsFlag==CDX_TRUE))
                {
                    fstAudPts = nPtsValue;
                    bFstAudPtsFlag = CDX_TRUE;
                    if(bFstVidPtsFlag == CDX_TRUE)
                        break;
                }
            }
            checkStartAddr = buf+4;
            goto find_pack_start_code1;
        }
        validBufferSize = MAX_CHUNK_BUF_SIZE-4;
        memcpy(mMpgParserCxt->mDataChunkT.pStartAddr,
            mMpgParserCxt->mDataChunkT.pStartAddr+MAX_CHUNK_BUF_SIZE-4, 4);
        startBufAddr = mMpgParserCxt->mDataChunkT.pStartAddr+4;
        readLen = 4;
    }

    //step -1 check the last video nPts and the audio nPts
    seekPos = mMpgParserCxt->nFileLength;
    if(seekPos > 4*1024*1024)
    {
        seekPos -= 2*1024*1024;
    }
    else
    {
        seekPos /= 2;
    }
    readDataLen = 0;

    seekPos = (seekPos > MAX_CHUNK_BUF_SIZE)? (seekPos-MAX_CHUNK_BUF_SIZE) : 0;
    CdxStreamSeek(mMpgParserCxt->pStreamT, seekPos, STREAM_SEEK_SET);

    while(1)
    {
 check_next_pts:
        endAudPtsFlag = 0;
        endVidPtsFlag = 0;
        readLen = CdxStreamRead(mMpgParserCxt->pStreamT,
            mMpgParserCxt->mDataChunkT.pStartAddr,MAX_CHUNK_BUF_SIZE);
        if(readLen == 0)
        {
            if(mMpgParserCxt->bIsNetworkStream ==1)
            {
                break;
            }
        }
        readDataLen += readLen;
        mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr + readLen;
find_pack_start_code0:
        if(MpgParser->bForceReturnFlag == 1)
           return FILE_PARSER_OPEN_FILE_FAIL;
        buf = MpgOpenFindPackStartReverse(mMpgParserCxt->mDataChunkT.pEndPtr,
            mMpgParserCxt->mDataChunkT.pStartAddr);
        if((buf==NULL)&&(readLen<MAX_CHUNK_BUF_SIZE))
        {
            break;
        }
        else if(buf != NULL)
        {
            curPtr = MpgOpenJudgePacketType(MpgParser, buf-3, &nRet);
            if(nRet > 0)
            {
                if(endScrFlag == CDX_FALSE)
                {
                    if(mMpgParserCxt->bIsISO11172Flag)
                        MpgTimeMpeg1ReadPackSCR(buf+1, &scrLow,&scrHigh);
                    else
                        MpgTimeMpeg2ReadPackSCR(buf+1, &scrLow,&scrHigh,NULL);
                    endScrVal = (scrLow>>1) | (scrHigh<<31);
                    endScrFlag = CDX_TRUE;
                }
                ptsFlag = ComputeFilePts(MpgParser, curPtr, &nPtsValue);
                if((nRet == 1) &&(endVidPtsFlag == CDX_FALSE) &&(ptsFlag == CDX_TRUE))
                {
                    endVidPtsArry[videoIndex++] = nPtsValue;
                    endVidPtsFlag = CDX_TRUE;
                    if(endAudPtsFlag == CDX_TRUE)
                        goto check_next_pts;
                }
                else if((nRet == 2) &&(endAudPtsFlag == CDX_FALSE)&&(ptsFlag == CDX_TRUE))
                {
                    endAudPtsArry[audioIndex++] = nPtsValue;
                    endAudPtsFlag = CDX_TRUE;
                    if(endVidPtsFlag == CDX_TRUE)
                        goto check_next_pts;
                }
            }
            mMpgParserCxt->mDataChunkT.pEndPtr  = buf-3;
            goto find_pack_start_code0;
        }
    }

    endVidPts = 0;
    for(i=0; i<videoIndex; i++)
    {
        if(endVidPts <= endVidPtsArry[i])
        {
            endVidPtsFlag = CDX_TRUE;
            endVidPts =  endVidPtsArry[i];
        }
    }

    endAudPts = 0;
    for(i=0; i<audioIndex; i++)
    {
        if(endAudPts <= endAudPtsArry[i])
        {
            endAudPtsFlag = CDX_TRUE;
            endAudPts =  endAudPtsArry[i];
        }
    }

    if((bFstAudPtsFlag == CDX_TRUE) &&(bFstVidPtsFlag==CDX_TRUE))
        mMpgParserCxt->nFirstPts = (fstAudPts < fstVidPts)? fstAudPts: fstVidPts;
    else if(bFstAudPtsFlag == CDX_TRUE)
        mMpgParserCxt->nFirstPts = fstAudPts;
    else if(bFstVidPtsFlag == CDX_TRUE)
        mMpgParserCxt->nFirstPts = fstVidPts;
    else if(fstScrFlag == CDX_TRUE)
        mMpgParserCxt->nFirstPts = fstScrVal;
    else
        mMpgParserCxt->nFirstPts = 0;

    if((endAudPtsFlag == CDX_TRUE) &&(endVidPtsFlag==CDX_TRUE))
        mMpgParserCxt->nEndPts = (endAudPts > endVidPts)? endAudPts: endVidPts;
    else if(endAudPtsFlag == CDX_TRUE)
        mMpgParserCxt->nEndPts = endAudPts;
    else if(endVidPtsFlag == CDX_TRUE)
        mMpgParserCxt->nEndPts = endVidPts;
    else if(endScrFlag == CDX_TRUE)
        mMpgParserCxt->nEndPts = endScrVal;
    else
        mMpgParserCxt->nEndPts = 0;

    mMpgParserCxt->nEndPts = MpgTimePts2Ms(mMpgParserCxt->nEndPts ,0,0);
    mMpgParserCxt->nFirstPts = MpgTimePts2Ms(mMpgParserCxt->nFirstPts ,0,0);
    mMpgParserCxt->nBasePts  = mMpgParserCxt->nFirstPts;

    if(fstScrFlag == CDX_TRUE)
    {
        mMpgParserCxt->nPreSCR   = MpgTimePts2Ms(fstScrVal ,0,0);
        mMpgParserCxt->bHasInitAVSFlag = CDX_TRUE;
    }

    if(mMpgParserCxt->nEndPts > mMpgParserCxt->nFirstPts)
    {
        mMpgParserCxt->nEndPts -= mMpgParserCxt->nFirstPts;
        mMpgParserCxt->nFileTimeLength = mMpgParserCxt->nEndPts;
    }
    else
    {
        mMpgParserCxt->nFileTimeLength  = mMpgParserCxt->nEndPts;
    }
    return 0;
}


cdx_int16 VobSearchNextNvPack(CdxMpgParserT *MpgParser, cdx_int64 *startFpPos,
    cdx_uint8 *scrChangedFlag)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8 *buf          = NULL;
    cdx_uint8 *curPtr      = NULL;
    cdx_int32 redLen      = 0;
    cdx_uint32 curBaseSCR  = 0;
    cdx_uint32 nNext32bits = 0;
    cdx_int64 curFpPos     = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    curFpPos = *startFpPos+VOB_VIDEO_LB_LEN;

    while(curFpPos < mMpgParserCxt->nFileLength)
    {
        CdxStreamSeek(mMpgParserCxt->pStreamT,curFpPos, STREAM_SEEK_SET);
        redLen = CdxStreamRead(mMpgParserCxt->pStreamT,
            mMpgParserCxt->mDataChunkT.pStartAddr, MAX_CHUNK_BUF_SIZE);
        mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr + redLen;
        buf = mMpgParserCxt->mDataChunkT.pStartAddr;

        while(buf < mMpgParserCxt->mDataChunkT.pEndPtr)
        {
           CalculateBaseSCR(MpgParser, buf+4, &curBaseSCR);
           curPtr = buf+4;
           if(mMpgParserCxt->bIsISO11172Flag)
               curPtr += 8;
           else
           {
               curPtr += 9;
                curPtr += (curPtr[0]&7)+1;
            }
           nNext32bits = (curPtr[0]<<24) | (curPtr[1]<<16) | (curPtr[2]<<8) | curPtr[3];
           if(nNext32bits != MPG_SYSTEM_HEADER_START_CODE)
           {
               *startFpPos = curFpPos + buf - mMpgParserCxt->mDataChunkT.pStartAddr;
                buf += VOB_VIDEO_LB_LEN;
           }
           else
           {
               curFpPos += buf - mMpgParserCxt->mDataChunkT.pStartAddr;
               *startFpPos = curFpPos;
                if(curBaseSCR == 0)
                   *scrChangedFlag = 1;
                else
                   *scrChangedFlag = 0;
                return FIND_NEW_VOBID;
            }
         }
         curFpPos += MAX_CHUNK_BUF_SIZE;
    }
    return NOT_FIND_VOBID;
}


cdx_int16 VobCheckEndPts(CdxMpgParserT *MpgParser, cdx_int64 curFpPos,
    cdx_int64 endFpPos, cdx_int64 *orgEndPTM, cdx_uint8 calLastPtsFlag)
{
    MpgParserContextT *mMpgParserCxt=NULL;
    cdx_uint8 *cur_ptr        = NULL;
    cdx_uint8  flag1          = 0;
    cdx_uint8  flag2          = 0;
    cdx_uint8  isVideoPack    = 0;
    cdx_uint8  isMpegAudPack  = 0;
    cdx_int16  nRet           = 0;
    cdx_int16  i              = 0;
    cdx_uint32 nextCode       = 0;
    cdx_uint32 lastVobuPos    = 0;
    cdx_uint32 ptsLow         = 0;
    cdx_uint32 ptsHigh        = 0;
    cdx_int64 nPtsValue       = 0;
    cdx_int64 packtLen        = 0;
    cdx_int64 newFpPos        = 0;
    cdx_int32 redLen          = 0;


    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    CdxStreamSeek(mMpgParserCxt->pStreamT, curFpPos, STREAM_SEEK_SET);
    redLen  = CdxStreamRead(mMpgParserCxt->pStreamT,
        mMpgParserCxt->mDataChunkT.pStartAddr, VOB_VIDEO_LB_LEN);
    if(redLen < VOB_VIDEO_LB_LEN)
    {
        return 0;
    }
    mMpgParserCxt->bNextVobuPosFlag = CDX_FALSE;
    mMpgParserCxt->bPrevVobuPosFlag = CDX_TRUE;
    nRet = VobCheckUseInfo(MpgParser, mMpgParserCxt->mDataChunkT.pStartAddr);
    if(nRet == NOT_FIND_NV_PACK)
    {
        return NOT_FIND_NV_PACK;
    }
    else if(nRet == NOT_FIND_PREV_PACK)
    {
        return 0;
    }
    mMpgParserCxt->bNextVobuPosFlag = CDX_TRUE;
    mMpgParserCxt->bPrevVobuPosFlag = CDX_FALSE;

    i  = 19;
    while(i >= 0)
    {
        flag1 = (cdx_uint8)((mMpgParserCxt->nRecordVobuPosArray[i]&0x00ffffff)==0x00ffffff);
        flag2 = (cdx_uint8)((mMpgParserCxt->nRecordVobuPosArray[i]&0x00ffffff)==0x00000000);
        if((flag1==0)&&(flag2==0)&&((mMpgParserCxt->nRecordVobuPosArray[i]-lastVobuPos)>1))
        {
            lastVobuPos = mMpgParserCxt->nRecordVobuPosArray[i];
            break;
        }

        i--;
    }
    if(i == -1)
    {
        if(calLastPtsFlag == 0)
        {
            return 0;
        }
        else
        {
            newFpPos = endFpPos;
        }
    }
    else
    {
        newFpPos = curFpPos-mMpgParserCxt->nRecordVobuPosArray[i]*VOB_VIDEO_LB_LEN;
        if((newFpPos == endFpPos) &&(calLastPtsFlag==0))
        {
            return 0;
        }
    }

    CdxStreamSeek(mMpgParserCxt->pStreamT, newFpPos+VOB_VIDEO_LB_LEN, STREAM_SEEK_SET);
    redLen = CdxStreamRead(mMpgParserCxt->pStreamT,
        mMpgParserCxt->mDataChunkT.pStartAddr,VOB_VIDEO_LB_LEN);
    if(redLen < VOB_VIDEO_LB_LEN)
    {
        return 0;
    }

    if(mMpgParserCxt->bIsISO11172Flag)
    {
        //cur_ptr = mMpgParserCxt->mDataChunkT.pStartAddr + 16;
        cur_ptr = mMpgParserCxt->mDataChunkT.pStartAddr + 12;
        nextCode = (cur_ptr[0]<<24)|(cur_ptr[1]<<16)|(cur_ptr[2]<<8)| cur_ptr[3];
        isVideoPack   = ((nextCode&0xf0) == MPG_VIDEO_ID);
        isMpegAudPack = ((nextCode>=0x01c0) && (nextCode<=0x01df));
        if((isVideoPack==1)||(isMpegAudPack==1)||(nextCode==MPG_PRIVATE_STREAM_1))
        {
            cur_ptr += 4;
            MpgReadProcessIsISO11172(MpgParser, cur_ptr,&nRet,&packtLen, &ptsLow, &ptsHigh);
        }
    }
    else
    {
       cur_ptr = mMpgParserCxt->mDataChunkT.pStartAddr + 13;
       // cur_ptr += (cur_ptr[0]&7) + 5;
       cur_ptr += ((cur_ptr[0]&7)+1);
       nextCode = (cur_ptr[0]<<24)|(cur_ptr[1]<<16)|(cur_ptr[2]<<8)| cur_ptr[3];
       isVideoPack   = ((nextCode&0xf0) == MPG_VIDEO_ID);
       isMpegAudPack = ((nextCode>=0x01c0) && (nextCode<=0x01df));
       if((isVideoPack==1)||(isMpegAudPack==1)||(nextCode==MPG_PRIVATE_STREAM_1))
       {
          cur_ptr += 4;
          MpgReadProcessNonISO11172(MpgParser, cur_ptr,&nRet,&packtLen, &ptsLow, &ptsHigh);
       }
     }
     if(nRet == 1)
      {
        nPtsValue = MpgTimePts2Ms((ptsLow>>1)|(ptsHigh<<31),0,0);
        if(nPtsValue >= (*orgEndPTM))
         {
            //return nPtsValue;
            *orgEndPTM = nPtsValue;
         }
      }
    return 0;
}

cdx_int64 VobCheckStartPts(CdxMpgParserT *MpgParser, cdx_int64 curFpPos)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8 *cur_ptr   = NULL;
    cdx_uint32 ptsLow    = 0;
    cdx_uint32 ptsHigh   = 0;
    cdx_int64  nPtsValue = 0;
    cdx_int16  result    = 0;
    cdx_int64  packetLen = 0;
    cdx_uint8  searchNum = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    CdxStreamSeek(mMpgParserCxt->pStreamT, curFpPos, STREAM_SEEK_SET);

re_reada_data:
    if(CdxStreamRead(mMpgParserCxt->pStreamT,
        mMpgParserCxt->mDataChunkT.pStartAddr, VOB_VIDEO_LB_LEN) < VOB_VIDEO_LB_LEN)
    {
        return 0;
    }

    if(mMpgParserCxt->bIsISO11172Flag)
    {
      cur_ptr = mMpgParserCxt->mDataChunkT.pStartAddr + 16;
      MpgReadProcessIsISO11172(MpgParser, cur_ptr, &result, &packetLen, &ptsLow, &ptsHigh);
    }
    else
    {
      cur_ptr = mMpgParserCxt->mDataChunkT.pStartAddr + 13;
      cur_ptr += (cur_ptr[0]&7)+5;
      MpgReadProcessNonISO11172(MpgParser, cur_ptr, &result, &packetLen, &ptsLow, &ptsHigh);

    }
    nPtsValue = MpgTimePts2Ms((ptsLow>>1)|(ptsHigh<<31),0,0);
    if(nPtsValue  == 0)
    {
        if(searchNum < 2)
        {

            goto re_reada_data;
        }
        searchNum++;
    }
    return nPtsValue;
}


cdx_int16  VobCheckPsPts(CdxMpgParserT *MpgParser, cdx_int64 fstSysPos)
{
   MpgParserContextT *mMpgParserCxt = NULL;
   cdx_int16  i = 0;
   cdx_int16  j = 0;
   cdx_uint8  curVobId     = 0;
   cdx_uint8  scrChangFlag = 0;
   cdx_int32 redLen       = 0;
   cdx_uint32 fstBaseScr   = 0;
   cdx_uint32 calPreFrmNum = 0;
   cdx_uint32 next32code   = 0;
   cdx_uint32 lastBaseTime = 0;
   cdx_int16  nRet         = 0;
   cdx_int64  curFpPos     = 0;
   cdx_int64  lastFpPos    = 0;
   cdx_int64  newFpPos     = 0;
   cdx_int64  startPTM     = 0;
   cdx_int64  endPTM       = 0;
   cdx_int64  curPts       = 0;

   cdx_bool   rcdBasePtsFlag   = CDX_FALSE;
   cdx_bool   calStartPtmFlag  = CDX_FALSE;
   cdx_bool   recordFstSCR     = CDX_FALSE;
   cdx_bool   recordFstPts     = CDX_FALSE;
   cdx_bool   calPreFrmNumFlag = CDX_TRUE;
   cdx_uint8  basePtsValidFlag = CDX_TRUE;
   cdx_uint32 recordBasePts    = 0;

   cdx_uint32 vobfileTimeLen[MAX_VOB_NS] = {0};

   mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

   fstSysPos -= 4;
   curFpPos = fstSysPos;
   mMpgParserCxt->bNextVobuPosFlag = CDX_TRUE;
   mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[curVobId]= fstSysPos;
   mMpgParserCxt->mFileTitleIfoT.bVobScrChangedFlagArray[curVobId] = 0;
   mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[curVobId]= 0;
   vobfileTimeLen[curVobId]= 0;

   lastFpPos = fstSysPos;

  while(curFpPos < mMpgParserCxt->nFileLength)
  {
        if(MpgParser->bForceReturnFlag == 1)
            return FILE_PARSER_OPEN_FILE_FAIL;
        if(recordFstPts == CDX_TRUE)
        {
            if(VobCheckEndPts(MpgParser,curFpPos, lastFpPos, &endPTM, 0) == NOT_FIND_NV_PACK)
            {
                curFpPos = lastFpPos;
                i++;
                goto __cal_new_fp_pos;
            }
        }
        lastFpPos = curFpPos;
        CdxStreamSeek(mMpgParserCxt->pStreamT, curFpPos, STREAM_SEEK_SET);
        redLen = CdxStreamRead(mMpgParserCxt->pStreamT,
            mMpgParserCxt->mDataChunkT.pStartAddr,VOB_VIDEO_LB_LEN);
        if(redLen < VOB_VIDEO_LB_LEN)
        {
            break;
        }
        else
        {
            next32code = ((mMpgParserCxt->mDataChunkT.pStartAddr[0]<<24)|
                (mMpgParserCxt->mDataChunkT.pStartAddr[1]<<16)|
                (mMpgParserCxt->mDataChunkT.pStartAddr[2]<<8) |
                mMpgParserCxt->mDataChunkT.pStartAddr[3]);
            if(next32code != 0x01BA)
              break;
        }

        if(recordFstSCR == CDX_FALSE)
        {
            CalculateBaseSCR(MpgParser, mMpgParserCxt->mDataChunkT.pStartAddr+4, &fstBaseScr);
            recordFstSCR = CDX_TRUE;
            if((fstBaseScr!=0) && (rcdBasePtsFlag == CDX_TRUE))
                calStartPtmFlag = CDX_TRUE;
        }

        i =  VobCheckUseInfo(MpgParser, mMpgParserCxt->mDataChunkT.pStartAddr);
        if(i == -1)
        {
            mMpgParserCxt->bHasNvpackFlag = CDX_FALSE;
            return 0;
        }

        curPts = VobCheckStartPts(MpgParser,curFpPos+VOB_VIDEO_LB_LEN);
        if(calStartPtmFlag == CDX_FALSE)
        {
            startPTM = curPts;
            endPTM   = curPts;
            recordFstPts = CDX_TRUE;
            calStartPtmFlag = CDX_TRUE;
        }
        if(curVobId == 0)
        {
            if(mMpgParserCxt->nBasePts == 0)
            {
                mMpgParserCxt->nBasePts = curPts;
                rcdBasePtsFlag = CDX_TRUE;
            }
            else
            {
                if(mMpgParserCxt->nBasePts == curPts)
                {
                    recordBasePts = curPts;
                    basePtsValidFlag = CDX_FALSE;
                }
                if(basePtsValidFlag == CDX_FALSE)
                {
                    if((recordBasePts !=0) && (recordBasePts != curPts))
                    {
                        mMpgParserCxt->nBasePts = curPts;
                        startPTM = curPts;
                        basePtsValidFlag = CDX_TRUE;
                    }
                    else if((recordBasePts==0)&&(mMpgParserCxt->nBasePts != curPts))
                    {
                        basePtsValidFlag = CDX_TRUE;
                    }
                }
            }
        }
__cal_new_fp_pos:

    if(i < 20)
    {
        curFpPos += (cdx_int64)(mMpgParserCxt->nRecordVobuPosArray[i] * VOB_VIDEO_LB_LEN);
        if(curFpPos >= mMpgParserCxt->nFileLength)
        {
            curFpPos = lastFpPos;
            for(j=i+1; j<20; j++)
            {
               if((mMpgParserCxt->nRecordVobuPosArray[j]!=0x00ffffff)
                   &&(mMpgParserCxt->nRecordVobuPosArray[j]!=0))
               {
                  newFpPos = lastFpPos +mMpgParserCxt->nRecordVobuPosArray[j] * VOB_VIDEO_LB_LEN;
                  if((newFpPos < mMpgParserCxt->nFileLength) &&(newFpPos > curFpPos))
                  {
                     curFpPos = newFpPos;
                     break;
                  }
               }
            }
            if((j==20) || (mMpgParserCxt->nRecordVobuPosArray[j] ==
                mMpgParserCxt->nRecordVobuPosArray[19]))
            {
                if(curFpPos != lastFpPos)
                {
                   //endPTM = VobCheckEndPts(MpgParser,curFpPos, lastFpPos, endPTM);
                   if(VobCheckEndPts(MpgParser,curFpPos,
                       lastFpPos, &endPTM, 0) == NOT_FIND_NV_PACK)
                   {
                        curFpPos = lastFpPos;
                        i++;
                        goto __cal_new_fp_pos;
                   }
                }
                break;
            }
        }
        if(curFpPos == lastFpPos)
        {
           break;
        }
    }
    else
    {

       nRet = VobSearchNextNvPack(MpgParser, &curFpPos,&scrChangFlag);

       if(nRet == NOT_FIND_VOBID)
       {
            VobCheckEndPts(MpgParser,curFpPos, lastFpPos, &endPTM, 1);
            mMpgParserCxt->mFileTitleIfoT.bVobScrChangedFlagArray[curVobId] = 1;
            break;
       }
       else if(nRet == FIND_NEW_VOBID)
       {
     #if 0
            if(lastFpPos == mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[curVobId])
            {
               mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[curVobId] = curFpPos;
               if(calPreFrmNumFlag == CDX_TRUE)
               {

                  calPreFrmNum    ++;
                  recordFstPts    = CDX_FALSE;
                  recordFstSCR    = CDX_FALSE;
                  calStartPtmFlag = CDX_FALSE;
               }
            }
            else
    #endif
         {
            VobCheckEndPts(MpgParser,curFpPos, lastFpPos, &endPTM, 1);
            mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[curVobId] = lastFpPos;
            mMpgParserCxt->mFileTitleIfoT.bVobPreScrArray[curVobId] = fstBaseScr;
            mMpgParserCxt->mFileTitleIfoT.bVobScrChangedFlagArray[curVobId] = scrChangFlag;

            vobfileTimeLen[curVobId] = endPTM - startPTM;
            recordFstSCR = CDX_FALSE;
            calPreFrmNumFlag = CDX_FALSE;
            calStartPtmFlag   = CDX_FALSE;
            curVobId ++;
            mMpgParserCxt->mFileTitleIfoT.bVobScrChangedFlagArray[curVobId] = 0;
            mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[curVobId] = curFpPos;
         }
       }
    }
  }

    if(MpgParser->bForceReturnFlag == 1)
         return FILE_PARSER_OPEN_FILE_FAIL;


   if(lastFpPos == mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[curVobId])
   {
     if(curVobId > 0)
         curVobId -= 1;
     else
     {
         vobfileTimeLen[curVobId] = endPTM - startPTM;
         mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[curVobId] = lastFpPos;
         mMpgParserCxt->mFileTitleIfoT.bVobPreScrArray[curVobId] = fstBaseScr;
     }
   }
   else
   {
       vobfileTimeLen[curVobId] = endPTM - startPTM;
       mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[curVobId] = lastFpPos;
       mMpgParserCxt->mFileTitleIfoT.bVobPreScrArray[curVobId] = fstBaseScr;
   }

   if((curVobId == 0) &&
       (mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[curVobId]==
       mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[curVobId]))
   {
      mMpgParserCxt->bHasNvpackFlag = CDX_FALSE;
      mMpgParserCxt->nFileTimeLength = 0;
      CDX_LOGW("there is noly one nv pack.\n");
      return 0;
   }
   else
   {
      mMpgParserCxt->nFileTitleNs  = curVobId + 1;
      mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[0] = 0;
      mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[0] = 0;

      if(calPreFrmNum != 0)
      {
         if(mMpgParserCxt->nBasePts >= calPreFrmNum * MpgParser->mVideoFormatT.nFrameDuration)
             mMpgParserCxt->nBasePts -= calPreFrmNum * MpgParser->mVideoFormatT.nFrameDuration;
         else
            mMpgParserCxt->nBasePts = 0;
      }

      mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[0] = 0;
      mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[0] = 0;
      mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[1] = vobfileTimeLen[0];

      for(curVobId=2; curVobId<=mMpgParserCxt->nFileTitleNs; curVobId++)
      {
            if(mMpgParserCxt->mFileTitleIfoT.bVobScrChangedFlagArray[curVobId-2]==1)
            {
                lastBaseTime = mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[curVobId-1];
                mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[curVobId-1]=
                    lastBaseTime + mMpgParserCxt->nBasePts;
            }
            else
            {
                lastBaseTime = mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[curVobId-2];
                mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[curVobId-1] = lastBaseTime;
                if(lastBaseTime >= mMpgParserCxt->nBasePts)
                   lastBaseTime -= mMpgParserCxt->nBasePts;
                else
                    lastBaseTime = 0;
            }
            mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[curVobId]=
                lastBaseTime + vobfileTimeLen[curVobId-1];
      }

      mMpgParserCxt->nFileTimeLength =
          mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[mMpgParserCxt->nFileTitleNs];
      mMpgParserCxt->bHasNvpackFlag = CDX_TRUE;
   }


    mMpgParserCxt->nBaseSCR = 0;
    mMpgParserCxt->nBaseTime = 0;
    mMpgParserCxt->nPreSCR = mMpgParserCxt->mFileTitleIfoT.bVobPreScrArray[0];
    mMpgParserCxt->bHasInitAVSFlag = CDX_TRUE;
    return 1;
}

/******************************************************************************/
/******************************************************************************/
cdx_int16 VobCheckUseInfo(CdxMpgParserT *MpgParser, cdx_uint8 *buf)

{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8 *curPtr      = NULL;
    cdx_uint32 nNext32bits = 0;
    cdx_uint16 next16bits  = 0;
    cdx_uint16 nSize       = 0;
    cdx_int16  i = 0;
    cdx_int16  j = 20;
    cdx_int16  k = 0;
    
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
    
    if(mMpgParserCxt->bIsISO11172Flag)
    {
       buf += 12;
    }
    else
    {
       buf += 13;
       nSize = buf[0]&7;
       buf += nSize+1;
    }
    nSize = (buf[4]<<8) | buf[5];
    curPtr = buf +nSize+6;
    nNext32bits = ((curPtr[0]<<24) | (curPtr[1]<<16) | (curPtr[2]<<8) | curPtr[3]);
    next16bits = (curPtr[4]<<8) | curPtr[5];
    if((nNext32bits != 0x01BF) ||(next16bits !=0x03D4))
        return -1;
    curPtr += 0x03DA;
    nNext32bits = ((curPtr[0]<<24) | (curPtr[1]<<16) | (curPtr[2]<<8) | curPtr[3]);
    next16bits = (curPtr[4]<<8) | curPtr[5];
    if((nNext32bits != 0x01BF) ||(next16bits !=0x03FA))
        return NOT_FIND_NV_PACK;

    buf += nSize+25;


    buf += 1212;

    if(mMpgParserCxt->bNextVobuPosFlag == CDX_TRUE)
    {
       for(i = 0; i<20; i++)
       {
          nNext32bits = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
          nNext32bits &= 0x0ffffff;
          mMpgParserCxt->nRecordVobuPosArray[i] = nNext32bits;
          buf += 4;
          if(nNext32bits == 0)
            k++;
          if((j==20)&& (nNext32bits!= 0xffffff) &&(nNext32bits != 0))
              j = i;
        }
     }
    else if(mMpgParserCxt->bPrevVobuPosFlag == CDX_TRUE)
    {
       buf += 80;
       for(i = 0; i<20; i++)
       {
          nNext32bits = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
          nNext32bits &= 0x0ffffff;
          mMpgParserCxt->nRecordVobuPosArray[19-i] = nNext32bits;
          buf += 4;
        }
    }
    if(k==20)
    {
      return  NOT_FIND_PREV_PACK;
    }
    return j;
}

