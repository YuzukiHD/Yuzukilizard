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
#define LOG_TAG "mpgStatusChange"
#include <cdx_log.h>

#include "CdxMpgParser.h"
#include "CdxMpgParserImpl.h"
#include "dvdTitleIfo.h"
#include "mpgFunc.h"

/**************************************************/
/**************************************************/
cdx_uint8  CalculatePosIndex(cdx_uint32 value)
{
    cdx_uint8 posIndex = 0;

    value/=500;
    if(value < 15)
    {
        posIndex = 19 - value;
    }
    else
    {
        if(value >= 240)
           posIndex = 0;
        else if(value >= 120)
           posIndex = 1;
        else if(value >= 60)
           posIndex = 2;
        else if(value >= 20)
           posIndex = 3;
        else
           posIndex = 4;
    }

    return posIndex;
}


cdx_int16 VobCheckTitleEdge(CdxMpgParserT *MpgParser, cdx_int64 *curFpPos)
{
    cdx_uint32  nTitleNum = 0;
    MpgParserContextT *mMpgParserCxt = NULL;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if((mMpgParserCxt->bNextVobuPosFlag==CDX_TRUE) &&
        ((*curFpPos)>=mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[mMpgParserCxt->nCurTitleNum]))
    {
        mMpgParserCxt->nCurTitleNum ++;
        if(mMpgParserCxt->nCurTitleNum == mMpgParserCxt->nFileTitleNs)
        {
            return 0;
        }
        else
        {
            nTitleNum = mMpgParserCxt->nCurTitleNum;
            *curFpPos = mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[nTitleNum];
            mMpgParserCxt->nBaseTime = mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[nTitleNum];
            return 1;
        }
    }
    else if((mMpgParserCxt->bPrevVobuPosFlag==CDX_TRUE) &&
        ((*curFpPos)<=mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[mMpgParserCxt->nCurTitleNum]))
    {
        if(mMpgParserCxt->nCurTitleNum == 0)
        {
            return 0;
        }
        else
        {
            mMpgParserCxt->nCurTitleNum --;
            nTitleNum = mMpgParserCxt->nCurTitleNum;
            *curFpPos = mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[nTitleNum];
            mMpgParserCxt->nBaseTime = mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[nTitleNum];
            return 1;
        }
    }

    return -1;
}

cdx_uint8 VobJudgePosValid(CdxMpgParserT *MpgParser, cdx_int64 *startFpPos, cdx_uint32 absDiffTime)
{
    cdx_int64 curFpPos = 0;
    cdx_uint8 posIndex = 0;
    cdx_int16 nRet     = 0;
    cdx_uint8 i        = 0;

    MpgParserContextT *mMpgParserCxt = NULL;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
    curFpPos = *startFpPos;

    nRet = VobCheckTitleEdge(MpgParser, &curFpPos);
    if(nRet != -1)
    {
        *startFpPos = curFpPos;
        return nRet;
    }
    else
    {
        posIndex = CalculatePosIndex(absDiffTime);
        i = posIndex;
        if((mMpgParserCxt->nRecordVobuPosArray[posIndex]==0xffffff)
            ||(mMpgParserCxt->nRecordVobuPosArray[posIndex]==0))
        {
            i = posIndex + 1;
            while(i<20)
            {
                if((mMpgParserCxt->nRecordVobuPosArray[i]!=0xffffff)
                    &&(mMpgParserCxt->nRecordVobuPosArray[i]!=0))
                    break;
                i++;
            }
        }

        if(i < 20)
        {
            if(mMpgParserCxt->bNextVobuPosFlag==CDX_TRUE)
            {
                curFpPos += mMpgParserCxt->nRecordVobuPosArray[i] * VOB_VIDEO_LB_LEN;
            }
            else if(mMpgParserCxt->bPrevVobuPosFlag==CDX_TRUE)
            {
                curFpPos -= mMpgParserCxt->nRecordVobuPosArray[i] * VOB_VIDEO_LB_LEN;
            }
        }

        nRet = VobCheckTitleEdge(MpgParser, &curFpPos);
        *startFpPos = curFpPos;
        if(nRet != 0)
          nRet = 1;
        return nRet;
    }
}


cdx_int16 VobSelectRightPts(CdxMpgParserT *MpgParser, cdx_int64 *startFpPos)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32 diffTime1      = 0;
    cdx_uint32 diffTime2      = 0;
    cdx_uint32 absDiffTime    = 0;
    cdx_int64  curPts           = 0;
    cdx_int64  curFpPos       = 0;
    cdx_int64  startPTM       = 0;
    cdx_int64  minAbsDiffTime = 0;
    cdx_int64  minDiffFpPos   = 0;
    cdx_uint8  nRet           = 0;
    cdx_uint8  findPosNum     = 0;
    //__s64 base_scr_time = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    diffTime1 = mMpgParserCxt->nDisplayTime -
            mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[mMpgParserCxt->nCurTitleNum];
    diffTime2 = mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[mMpgParserCxt->nCurTitleNum+1]
            - mMpgParserCxt->nDisplayTime;

    if(diffTime1 <= diffTime2)
    {
        curFpPos = mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[mMpgParserCxt->nCurTitleNum];
        minAbsDiffTime = diffTime1;
        minDiffFpPos   = curFpPos;
    }
    else
    {
        curFpPos = mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[mMpgParserCxt->nCurTitleNum];
        minAbsDiffTime = diffTime2;
        minDiffFpPos   = curFpPos;
    }

    while(1)
    {
        findPosNum++;
        if(findPosNum >= 100)
        {
            *startFpPos = minDiffFpPos;
            break;
        }
        startPTM = VobCheckStartPts(MpgParser,curFpPos+VOB_VIDEO_LB_LEN);
        curPts = startPTM + mMpgParserCxt->nBaseTime - mMpgParserCxt->nBasePts;

        if(curPts >= mMpgParserCxt->nDisplayTime)
        {
            absDiffTime = curPts - mMpgParserCxt->nDisplayTime;
            mMpgParserCxt->bNextVobuPosFlag = CDX_FALSE;
            mMpgParserCxt->bPrevVobuPosFlag = CDX_TRUE;
        }
        else
        {
            absDiffTime = mMpgParserCxt->nDisplayTime - curPts;
            mMpgParserCxt->bNextVobuPosFlag = CDX_TRUE;
            mMpgParserCxt->bPrevVobuPosFlag = CDX_FALSE;
        }

        if(absDiffTime <= VOB_JUMP_TIME_THRESH)
        {
            *startFpPos = curFpPos;
            break;
        }

        if(absDiffTime < minAbsDiffTime)
        {
            minAbsDiffTime = absDiffTime;
            minDiffFpPos   = curFpPos;
        }

        CdxStreamSeek(mMpgParserCxt->pStreamT,curFpPos,STREAM_SEEK_SET);
        if(CdxStreamRead(mMpgParserCxt->pStreamT,mMpgParserCxt->mDataChunkT.pStartAddr,
                    VOB_VIDEO_LB_LEN) < VOB_VIDEO_LB_LEN)
        {
            return -1;
        }
        VobCheckUseInfo(MpgParser,mMpgParserCxt->mDataChunkT.pStartAddr);
        nRet = VobJudgePosValid(MpgParser, &curFpPos, absDiffTime);
        if(nRet == 0)
        {
          *startFpPos = curFpPos;
          break;
        }
    }
    return 0;
}

cdx_int16 VobSelectRightPos(CdxMpgParserT *MpgParser, cdx_uint32 dispTime)
{
    cdx_uint32 timeInterval = 0;
    cdx_int64  curFpPos     = 0;
    cdx_int16  nRet         = 0;

    MpgParserContextT *mMpgParserCxt = NULL;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

     if(mMpgParserCxt->nLastNvTime != 0)
         timeInterval = mMpgParserCxt->nLastNvTime;
     else
         timeInterval =  mMpgParserCxt->nDisplayTime;
     mMpgParserCxt->nDisplayTime = dispTime;

     /*
     if(MpgParser->eStatus==CDX_MPG_STATUS_FORWARD)
     {
        if(timeInterval >= mMpgParserCxt->nDisplayTime)
        {
            timeInterval = 0;
        }
        else
          timeInterval =  (mMpgParserCxt->nDisplayTime - timeInterval);
     }
     else if(MpgParser->eStatus==CDX_MPG_STATUS_BACKWARD)
     {
        if(timeInterval <= mMpgParserCxt->nDisplayTime)
        {
            timeInterval = 0;
        }
        else
          timeInterval =  (timeInterval-mMpgParserCxt->nDisplayTime);
     }
     */

     curFpPos = mMpgParserCxt->nLastNvPackPos;

     nRet = VobJudgePosValid(MpgParser, &curFpPos, timeInterval);
     if(nRet== 0)       //µ½´ï±ß½ç
     {
        mMpgParserCxt->mDataChunkT.bFileEndFlag = CDX_TRUE;
     }
     CdxStreamSeek(mMpgParserCxt->pStreamT,curFpPos,STREAM_SEEK_SET);
     return 0;
}

void MpgStatusInitStatusTitleChange(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_int64  startFpPos = 0;
    cdx_uint32 time1      = 0;
    cdx_uint32 time2      = 0;
    cdx_uint8  j          = 0;
    cdx_uint32 nTitleNum  = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(mMpgParserCxt->nDisplayTime <= mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[0])
    {
        mMpgParserCxt->nCurTitleNum = 0;
        startFpPos = mMpgParserCxt->mFileTitleIfoT.nVobStartPosArray[mMpgParserCxt->nCurTitleNum];
    }
    else if(mMpgParserCxt->nDisplayTime
            >= mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[mMpgParserCxt->nFileTitleNs])
    {
        mMpgParserCxt->nCurTitleNum = mMpgParserCxt->nFileTitleNs-1;
        startFpPos = mMpgParserCxt->mFileTitleIfoT.nVobEndPosArray[mMpgParserCxt->nCurTitleNum ];
    }
    else
    {
        while(j<mMpgParserCxt->nFileTitleNs)
        {
            time1 = mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[j];
            time2 = mMpgParserCxt->mFileTitleIfoT.nVobTimeLenArray[j+1];
            if((mMpgParserCxt->nDisplayTime >= time1) && (mMpgParserCxt->nDisplayTime <= time2))
            {
                mMpgParserCxt->nCurTitleNum = j;
                mMpgParserCxt->nBaseTime = mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[j];
                break;
            }
            j++;
        }
        VobSelectRightPts(MpgParser, &startFpPos);
    }

    mMpgParserCxt->nBaseSCR = 0;
    nTitleNum  = mMpgParserCxt->nCurTitleNum;
    mMpgParserCxt->nPreSCR = mMpgParserCxt->mFileTitleIfoT.bVobPreScrArray[nTitleNum];
    mMpgParserCxt->nBaseTime = mMpgParserCxt->mFileTitleIfoT.nVobBaseTimeArray[nTitleNum];
    mMpgParserCxt->bGetRightPtsFlag = CDX_TRUE;
    CdxStreamSeek(mMpgParserCxt->pStreamT,startFpPos, STREAM_SEEK_SET);
}


void MpgStatusInitStatusChange(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(mMpgParserCxt->nFileTimeLength!=0)
    {
        if(mMpgParserCxt->nDisplayTime <= mMpgParserCxt->nFileTimeLength)
           mMpgParserCxt->nStartFpPos = (mMpgParserCxt->nFileLength /mMpgParserCxt->nFileTimeLength)
                                        *mMpgParserCxt->nDisplayTime;
        else
           mMpgParserCxt->nStartFpPos = CdxStreamTell(mMpgParserCxt->pStreamT) - UPDATE_SIZE_TH;
    }
    else
    {
        mMpgParserCxt->nStartFpPos = CdxStreamTell(mMpgParserCxt->pStreamT) - UPDATE_SIZE_TH;
    }

    CdxStreamSeek(mMpgParserCxt->pStreamT, mMpgParserCxt->nStartFpPos, STREAM_SEEK_SET);
}


void MpgStatusInitParamsProcess(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_FALSE;
    mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
    mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
    mMpgParserCxt->mDataChunkT.nSegmentNum = 0;
    mMpgParserCxt->mDataChunkT.nId = 0;
    mMpgParserCxt->nPreVideoMaxPts  = 0;
    MpgParser->bForbidSwitchScr = 0;
    mMpgParserCxt->bSwitchScrOverFlag = 0;
}

cdx_uint8 MpgStatusSelectSuitPts(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;

    cdx_int32  diff_pts     = 0;
    cdx_uint32 abs_diff_pts = 0;
    cdx_int64  seek_pos     = 0;
    cdx_uint8  result;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    diff_pts = mMpgParserCxt->mDataChunkT.nPts - mMpgParserCxt->nDisplayTime;
    abs_diff_pts = (diff_pts >= 0)? diff_pts : - diff_pts;

    if(mMpgParserCxt->nFindPtsNum >= 30)          // cannot find the right nPts
    {
        mMpgParserCxt->bGetRightPtsFlag = CDX_TRUE;
        result = 0;
        return result;
    }
    if(abs_diff_pts <= 1000)
    {
        mMpgParserCxt->bGetRightPtsFlag = CDX_TRUE;
        result = 0;
    }
    else
    {
        if(mMpgParserCxt->nFileTimeLength)
            seek_pos = (mMpgParserCxt->nFileLength/mMpgParserCxt->nFileTimeLength)*abs_diff_pts;
        else
            seek_pos = UPDATE_SIZE_TH;

        if(diff_pts > 0)
        {
            mMpgParserCxt->nLargestFpPos = mMpgParserCxt->nStartFpPos;
            if(mMpgParserCxt->mDataChunkT.nPts == mMpgParserCxt->nFirstPts)
            {
                mMpgParserCxt->bGetRightPtsFlag = CDX_TRUE;
                return 0;
            }
            if(mMpgParserCxt->nStartFpPos <= seek_pos)
                mMpgParserCxt->nStartFpPos /= 2;
            else
                mMpgParserCxt->nStartFpPos -= seek_pos;
        }
        else
        {
            mMpgParserCxt->nSmallestFpPos = mMpgParserCxt->nStartFpPos;
            if(mMpgParserCxt->mDataChunkT.nPts == mMpgParserCxt->nEndPts)
            {
                mMpgParserCxt->bGetRightPtsFlag = CDX_TRUE;
                return 0;
            }
            if((mMpgParserCxt->nStartFpPos +seek_pos) >= mMpgParserCxt->nFileLength)
                mMpgParserCxt->nStartFpPos +=
                            (mMpgParserCxt->nFileLength-mMpgParserCxt->nStartFpPos)/2;
            else
                mMpgParserCxt->nStartFpPos += seek_pos;
        }

        if(mMpgParserCxt->nStartFpPos<=0 || mMpgParserCxt->nStartFpPos>= mMpgParserCxt->nFileLength)
        {
            result =2;
        }
        else if((mMpgParserCxt->nLastDiffPts>0 && diff_pts <0)
                ||(mMpgParserCxt->nLastDiffPts<0 && diff_pts >0))
        {
            if(diff_pts < 0)
                mMpgParserCxt->nSmallestFpPos = mMpgParserCxt->nStartFpPos;
            else
                mMpgParserCxt->nLargestFpPos = mMpgParserCxt->nStartFpPos;
            mMpgParserCxt->nStartFpPos = (mMpgParserCxt->nSmallestFpPos
                                + mMpgParserCxt->nLargestFpPos)/2;
            CdxStreamSeek(mMpgParserCxt->pStreamT, mMpgParserCxt->nStartFpPos, STREAM_SEEK_SET);
            MpgStatusInitParamsProcess(MpgParser);
            mMpgParserCxt->nLastDiffPts = diff_pts;
            result = 1;
            mMpgParserCxt->nFindPtsNum ++;
        }
        else
        {
            CdxStreamSeek(mMpgParserCxt->pStreamT, mMpgParserCxt->nStartFpPos, STREAM_SEEK_SET);
            MpgStatusInitParamsProcess(MpgParser);
            mMpgParserCxt->nLastDiffPts = diff_pts;
            result = 1;
            mMpgParserCxt->nFindPtsNum ++;
        }
    }
    return result;
}

