/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : mpgPrser.c
* Description : parse mpg file
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment : parse mpg file
*
*
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxMpgParser"

#include <CdxParser.h>
#include "CdxMpgParser.h"
#include "CdxMpgParserImpl.h"
#include "dvdTitleIfo.h"

static char *const mpgSuffix[] = {".vob", ".mpg", ".mpeg",".dat", ".m1v", ".m2v", ".tp"};

cdx_int16 SendVidFrmData(CdxMpgParserT  *pCdxMpgParserT, CdxPacketT *pkt)
{
    MpgVideoFrmItemT  *mVidFrmItemT       = NULL;
    MpgParserContextT *pMpgParserContextT = NULL;
    cdx_uint32 nVidFrmSendItemIdx = 0;

    pMpgParserContextT = (MpgParserContextT *)pCdxMpgParserT->pMpgParserContext;
    if(pCdxMpgParserT->mMpgVidFrmInfT.nVidFrmValidItemNums > 0)
    {
        pkt->type = CDX_MEDIA_VIDEO;
        //pkt->video_type = VIDEO_TYPE_MAJOR;
        nVidFrmSendItemIdx = pCdxMpgParserT->mMpgVidFrmInfT.nVidFrmSendItemIdx;
        mVidFrmItemT = &(pCdxMpgParserT->mMpgVidFrmInfT.mVidFrmItemT[nVidFrmSendItemIdx]);
        mVidFrmItemT->nParsePicCodeMode = MPG_PARSE_PIC_CODE_NONE;
        pMpgParserContextT->mDataChunkT.nPts = mVidFrmItemT->nPtsValue;
        pMpgParserContextT->mDataChunkT.bHasPtsFlag = mVidFrmItemT->nPtsValid;
        pMpgParserContextT->mDataChunkT.nUpdateSize = mVidFrmItemT->nDataSize;
        if(mVidFrmItemT->bBufRepeatFlag == 0)
        {
            pMpgParserContextT->mDataChunkT.nSegmentNum = 1;
            pMpgParserContextT->mDataChunkT.nSegmentLength[0] = mVidFrmItemT->nDataSize;
            pMpgParserContextT->mDataChunkT.pSegmentAddr[0] = mVidFrmItemT->pStartAddr;
        }
        else
        {
            pMpgParserContextT->mDataChunkT.nSegmentNum = 2;
            pMpgParserContextT->mDataChunkT.nSegmentLength[0]
                    = pCdxMpgParserT->mMpgVidFrmInfT.pVidFrmDataBufEnd-mVidFrmItemT->pStartAddr;
            pMpgParserContextT->mDataChunkT.pSegmentAddr[0] = mVidFrmItemT->pStartAddr;
            pMpgParserContextT->mDataChunkT.nSegmentLength[1]
                    = mVidFrmItemT->nDataSize-pMpgParserContextT->mDataChunkT.nSegmentLength[0];
            pMpgParserContextT->mDataChunkT.pSegmentAddr[1]
                    = pCdxMpgParserT->mMpgVidFrmInfT.pVidFrmDataBuf;
        }
        pCdxMpgParserT->mMpgVidFrmInfT.nVidFrmValidItemNums -= 1;
        pCdxMpgParserT->mMpgVidFrmInfT.nVidFrmSendItemIdx += 1;
        if(pCdxMpgParserT->mMpgVidFrmInfT.nVidFrmSendItemIdx == MPG_MAX_FRM_NUM)
        {
            pCdxMpgParserT->mMpgVidFrmInfT.nVidFrmSendItemIdx = 0;
        }
        pMpgParserContextT->mDataChunkT.bWaitingUpdateFlag = 1;
        pMpgParserContextT->mDataChunkT.nId = pMpgParserContextT->nVideoId;
        return 0;
    }
    return -1;
}

static cdx_int32 CdxMpgParserClose(CdxParserT *parser)
{
    CdxMpgParserT          *pCdxMpgParserT;

    pCdxMpgParserT = (CdxMpgParserT *)parser;

    if(!pCdxMpgParserT)
    {
        CDX_LOGE("Close mov file parser module error, there is no file information!");
        return -1;
    }
    pCdxMpgParserT->close(pCdxMpgParserT);
    MpgExit(pCdxMpgParserT);
    //*parser = (void *)0;

    return CDX_SUCCESS;
}

static cdx_int32 CdxMpgParserPrefetch(CdxParserT *parser, CdxPacketT * pkt)
{
    CdxMpgParserT          *pCdxMpgParserT = NULL;
    MpgParserContextT       *mMpgParserCxt  = NULL;
    MpgCheckH264NulT       *pMpgCheckNulT  = NULL;
    int                        nResult;

    pCdxMpgParserT = (CdxMpgParserT *)parser;
    mMpgParserCxt = (MpgParserContextT*)pCdxMpgParserT->pMpgParserContext;
    pMpgCheckNulT = &(mMpgParserCxt->mMpgCheckNulT);
    //CDX_LOGD("prefetch");

    if(pCdxMpgParserT->nError == PSR_EOS)
    {
        CDX_LOGD("---eos");
        return -1;
    }

    if(pCdxMpgParserT->eStatus != CDX_PSR_IDLE && pCdxMpgParserT->eStatus != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("error operation:: not in the status of IDLE or PREFETCHED when call prefetch()");
        pCdxMpgParserT->nError = PSR_INVALID_OPERATION;
    }
    if(pCdxMpgParserT->eStatus == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &pCdxMpgParserT->mCurPacketT, sizeof(CdxPacketT));
        return CDX_SUCCESS;
    }
#if MPG_SEND_VID_FRAME
    SendVidFrmData(pCdxMpgParserT,&pCdxMpgParserT->mCurPacketT);
#endif

prefetchData:

    if(mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag == 0)
    {
        if(pCdxMpgParserT->bReadFileEndFlag == 1)
        {
            return CDX_FAILURE;
        }

        pCdxMpgParserT->eStatus = CDX_PSR_PREFETCHING;
        nResult = pCdxMpgParserT->read(pCdxMpgParserT);

        if(nResult==MPG_PSR_RESULT_EOF)
        {
            pCdxMpgParserT->bReadFileEndFlag = 1;
            pCdxMpgParserT->nError = PSR_EOS;
            #if MPG_SEND_VID_FRAME
            nResult = SendVidFrmData(pCdxMpgParserT,&pCdxMpgParserT->mCurPacketT);
            #endif
            if(nResult == -1)
            {
                CDX_LOGV("Try to read sample failed!");
                return CDX_FAILURE;
            }
        }
        else if(nResult == MPG_PSR_RESULT_ERRFMT)
        {
            pCdxMpgParserT->nError = PSR_IO_ERR;
        }

        #if MPG_SEND_VID_FRAME
        else if(mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag == 0)
        {
            SendVidFrmData(pCdxMpgParserT,&pCdxMpgParserT->mCurPacketT);
        }
        #endif
    }

    pCdxMpgParserT->mCurPacketT.length= mMpgParserCxt->mDataChunkT.nUpdateSize;

    //pkt->pkt_offset = 0;

    if(mMpgParserCxt->bIsPsFlag == 0)
    {
        pCdxMpgParserT->mCurPacketT.type = CDX_MEDIA_VIDEO;
        //pkt->video_type = VIDEO_TYPE_MAJOR;
    }
    else if(mMpgParserCxt->mDataChunkT.nId == mMpgParserCxt->nVideoId)
    {
        pCdxMpgParserT->mCurPacketT.type = CDX_MEDIA_VIDEO;

        //* deal with h264
        //  becase h264 decoder decode data based on nuls
        //  so we should search the beginning of nuls (0x00000001)
        if(pCdxMpgParserT->mVideoFormatT.eCodecFormat==VIDEO_CODEC_FORMAT_H264 ||
            pCdxMpgParserT->mVideoFormatT.eCodecFormat==VIDEO_CODEC_FORMAT_H265)
        {
            cdx_uint32 nJudeValues = 0xffffffff;
            cdx_int32  nJudgeLen = 0;

            if((pMpgCheckNulT->pWriteDataAddr-pMpgCheckNulT->pStartJudgeNulAddr)>0)
            {
                nJudgeLen = (pMpgCheckNulT->pWriteDataAddr - pMpgCheckNulT->pStartJudgeNulAddr);
            }
            else
            {
                nJudgeLen += pMpgCheckNulT->pEndAddr - pMpgCheckNulT->pStartJudgeNulAddr;
                nJudgeLen += pMpgCheckNulT->pWriteDataAddr - pMpgCheckNulT->pDataBuf;
            }

            while(nJudgeLen>0)
            {
                if(pMpgCheckNulT->pStartJudgeNulAddr >= pMpgCheckNulT->pEndAddr)
                {
                    pMpgCheckNulT->pStartJudgeNulAddr = pMpgCheckNulT->pDataBuf;
                }
                nJudeValues = nJudeValues << 8 ;
                nJudeValues = nJudeValues | (*pMpgCheckNulT->pStartJudgeNulAddr);
                if(nJudeValues==0x00000001)
                {
                    if(pMpgCheckNulT->bFirstNulFlag==0)
                    {
                        pMpgCheckNulT->bFirstNulFlag  = 1;
                        pMpgCheckNulT->pStartReadAddr = pMpgCheckNulT->pStartJudgeNulAddr - 3;
                        if(pMpgCheckNulT->pStartReadAddr<pMpgCheckNulT->pDataBuf)
                        {
                            pMpgCheckNulT->pStartReadAddr = pMpgCheckNulT->pEndAddr -
                                    (pMpgCheckNulT->pDataBuf-pMpgCheckNulT->pStartReadAddr-1);
                        }
                    }
                    else
                    {
                        pMpgCheckNulT->bSecodNulFlag = 1;
                        pMpgCheckNulT->pEndReadAddr = pMpgCheckNulT->pStartJudgeNulAddr - 3;
                        if(pMpgCheckNulT->pEndReadAddr<pMpgCheckNulT->pDataBuf)
                        {
                            pMpgCheckNulT->pEndReadAddr = pMpgCheckNulT->pEndAddr -
                                    (pMpgCheckNulT->pDataBuf-pMpgCheckNulT->pEndReadAddr-1);
                        }

                    }

                    break;
                }
                pMpgCheckNulT->pStartJudgeNulAddr += 1;
                nJudgeLen -= 1;
            }

            pMpgCheckNulT->pStartJudgeNulAddr = NULL;
            if(pMpgCheckNulT->bFirstNulFlag==1&&pMpgCheckNulT->bSecodNulFlag==1)
            {
                if(pMpgCheckNulT->pEndReadAddr>pMpgCheckNulT->pStartReadAddr)
                {
                    pCdxMpgParserT->mCurPacketT.length
                        = pMpgCheckNulT->pEndReadAddr - pMpgCheckNulT->pStartReadAddr;
                }
                else
                {
                    pCdxMpgParserT->mCurPacketT.length = 0;
                    pCdxMpgParserT->mCurPacketT.length
                        += pMpgCheckNulT->pEndAddr - pMpgCheckNulT->pStartReadAddr;
                    pCdxMpgParserT->mCurPacketT.length
                        += pMpgCheckNulT->pEndReadAddr - pMpgCheckNulT->pDataBuf;
                }
            }
            else
            {
               pCdxMpgParserT->mCurPacketT.length = 0;
            }

            //* if not search 0x00000001 , prefetch again
            if(pCdxMpgParserT->mCurPacketT.length==0)
            {
                mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_FALSE;
                mMpgParserCxt->mDataChunkT.nUpdateSize = 0;
                mMpgParserCxt->mDataChunkT.nSegmentNum = 0;
                goto prefetchData;
            }
        }

        //* deal with the pts
        if(mMpgParserCxt->bFstVidDataFlag == 0)
        {
            if(mMpgParserCxt->mDataChunkT.bHasPtsFlag==0)
            {
                mMpgParserCxt->nFstVideoPts = mMpgParserCxt->nFstAudioPts;
                mMpgParserCxt->mDataChunkT.bHasPtsFlag  = 1;
                mMpgParserCxt->mDataChunkT.nPts = mMpgParserCxt->nFstVideoPts;
            }
            else
            {
                mMpgParserCxt->nFstVideoPts = mMpgParserCxt->mDataChunkT.nPts;
                mMpgParserCxt->bFstVidPtsFlag = 1;
            }
            mMpgParserCxt->bFstVidDataFlag = 1;
        }
        else if(mMpgParserCxt->mDataChunkT.bHasPtsFlag == 1)
        {
            if((mMpgParserCxt->nFstVideoPts==mMpgParserCxt->mDataChunkT.nPts)
                    ||(mMpgParserCxt->mDataChunkT.nPts==0))
            {
               mMpgParserCxt->mDataChunkT.bHasPtsFlag = 0;
            }
            else
            {
                mMpgParserCxt->nFstVideoPts = mMpgParserCxt->mDataChunkT.nPts;
            }
        }

    }
    else if((mMpgParserCxt->mDataChunkT.nId==0x01BD)&&
            (mMpgParserCxt->mDataChunkT.nSubId==mMpgParserCxt->nSubpicStreamId)&&
            (pCdxMpgParserT->bIsVOB==1))
    {
        //*just for one subtitle track, we should motify here when surpport multi subtitle tracks
        pCdxMpgParserT->mCurPacketT.streamIndex = 0;
        pCdxMpgParserT->mCurPacketT.type = CDX_MEDIA_SUBTITLE;
        //pkt->video_type = VIDEO_TYPE_NOT_VIDEO;
    }
    else if((mMpgParserCxt->mDataChunkT.nId==mMpgParserCxt->nAudioId)
            ||((mMpgParserCxt->mDataChunkT.nId&0xe0)==MPG_AUDIO_ID))
    {
        pCdxMpgParserT->mCurPacketT.streamIndex = mMpgParserCxt->nSendAudioIndex;
        // send the audio pStartAddr nIndex
        pCdxMpgParserT->mCurPacketT.type = CDX_MEDIA_AUDIO;
        //pkt->video_type = VIDEO_TYPE_NOT_VIDEO;
        if(mMpgParserCxt->bFstAudDataFlag == 0)
        {
            if(mMpgParserCxt->mDataChunkT.bHasPtsFlag==0)
            {
                mMpgParserCxt->nFstAudioPts = mMpgParserCxt->nFstVideoPts;
                mMpgParserCxt->mDataChunkT.bHasPtsFlag  = 1;
                mMpgParserCxt->mDataChunkT.nPts = mMpgParserCxt->nFstAudioPts;
            }
            else
            {
                mMpgParserCxt->nFstAudioPts = mMpgParserCxt->mDataChunkT.nPts;
                mMpgParserCxt->bFstAudPtsFlag = 1;
            }
            mMpgParserCxt->bFstAudDataFlag = 1;
        }
        else if(mMpgParserCxt->mDataChunkT.bHasPtsFlag == 1)
        {
            mMpgParserCxt->nFstAudioPts = mMpgParserCxt->mDataChunkT.nPts;
        }
    }
    else
    {
        pCdxMpgParserT->mCurPacketT.type = CDX_MEDIA_UNKNOWN;
        //pkt->video_type = VIDEO_TYPE_NOT_VIDEO;
    }
    pCdxMpgParserT->mCurPacketT.pts  = -1;
    if(mMpgParserCxt->mDataChunkT.bHasPtsFlag)
    {
        pCdxMpgParserT->mCurPacketT.pts = mMpgParserCxt->mDataChunkT.nPts;
        pCdxMpgParserT->mCurPacketT.pts *= 1000;
    }
    //CDX_LOGD("prefetch end, pts=%lld, type=%d, streamIndex=%d, cdx_pkt->length=%d",
    //pkt->pts, pkt->type, pkt->streamIndex, pkt->length);
    memcpy(pkt, &pCdxMpgParserT->mCurPacketT, sizeof(CdxPacketT));
    pCdxMpgParserT->eStatus = CDX_PSR_PREFETCHED;
    return CDX_SUCCESS;
}


static cdx_int32 CdxMpgParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxMpgParserT          *pCdxMpgParserT = NULL;
    MpgParserContextT      *mMpgParserCxt  = NULL;
    MpgCheckH264NulT       *pMpgCheckNulT  = NULL;
    cdx_uint8              *pBufPtr        = NULL;
    cdx_uint8              *pDataBuf1       = NULL;
    cdx_uint8              *pDataBuf2      = NULL;
    cdx_uint32              nRemainSize       = 0;
    cdx_uint8               i = 0;
    pCdxMpgParserT = (CdxMpgParserT *)parser;
    mMpgParserCxt = (MpgParserContextT*)pCdxMpgParserT->pMpgParserContext;
    pMpgCheckNulT = &(mMpgParserCxt->mMpgCheckNulT);

    if((mMpgParserCxt->bIsPsFlag == 0)
        &&(mMpgParserCxt->mDataChunkT.nSegmentNum==0))// only has video stream
    {
         if(pkt->length <= pkt->buflen)
         {
            memcpy(pkt->buf,mMpgParserCxt->mDataChunkT.pStartAddr, pkt->length);
         }
         else
         {
            pDataBuf1 = mMpgParserCxt->mDataChunkT.pStartAddr;
            pDataBuf2 = pDataBuf1 + pkt->buflen;
            nRemainSize = pkt->length - pkt->buflen;
            memcpy(pkt->buf,pDataBuf1, pkt->buflen);
            memcpy(pkt->ringBuf,pDataBuf2, nRemainSize);
         }
         pkt->pts   = -1;
         pkt->flags |= (FIRST_PART|LAST_PART);
         if(pCdxMpgParserT->nSeekTime != 0)
         {
             pkt->pts  = pCdxMpgParserT->nSeekTime*1000;
             pCdxMpgParserT->nSeekTime = 0;
         }
    }
    else
    {
        //* if it is h264 cpy data to decoder from another buffer
        if(pkt->type==CDX_MEDIA_VIDEO
           && (pCdxMpgParserT->mVideoFormatT.eCodecFormat==VIDEO_CODEC_FORMAT_H264 ||
               pCdxMpgParserT->mVideoFormatT.eCodecFormat==VIDEO_CODEC_FORMAT_H265))
        {
            int nCpyLen = 0;
            nRemainSize = pkt->buflen;
            pBufPtr     = pkt->buf;
            if(pkt->length>0)
            {
                if(pMpgCheckNulT->bFirstNulFlag && pMpgCheckNulT->bSecodNulFlag)
                {
                    if(pkt->buflen==pkt->length)
                    {
                        if(pMpgCheckNulT->pEndReadAddr > pMpgCheckNulT->pStartReadAddr)
                        {
                            memcpy(pBufPtr,pMpgCheckNulT->pStartReadAddr,nRemainSize);
                        }
                        else
                        {
                            nCpyLen = pMpgCheckNulT->pEndAddr - pMpgCheckNulT->pStartReadAddr;
                            memcpy(pBufPtr,pMpgCheckNulT->pStartReadAddr,nCpyLen);
                            pBufPtr += nCpyLen;
                            nCpyLen = pMpgCheckNulT->pEndReadAddr - pMpgCheckNulT->pDataBuf;
                            memcpy(pBufPtr,pMpgCheckNulT->pDataBuf,nCpyLen);
                        }
                    }
                    else
                    {
                        if(pMpgCheckNulT->pEndReadAddr > pMpgCheckNulT->pStartReadAddr)
                        {
                            nCpyLen = pkt->buflen;
                            pBufPtr = pkt->buf;
                            memcpy(pBufPtr,pMpgCheckNulT->pStartReadAddr,pkt->buflen);
                            pBufPtr = pkt->ringBuf;
                            memcpy(pBufPtr,pMpgCheckNulT->pStartReadAddr+pkt->buflen,
                                    pkt->ringBufLen);
                        }
                        else
                        {
                            int srcLen1 = pMpgCheckNulT->pEndAddr - pMpgCheckNulT->pStartReadAddr;
                            int srcLen2 = pMpgCheckNulT->pEndReadAddr - pMpgCheckNulT->pDataBuf;
                            int dstLen1 = pkt->buflen;
                            int dstLen2 = pkt->ringBufLen;
                            if(srcLen1 > dstLen1)
                            {
                                pBufPtr = pkt->buf;
                                memcpy(pBufPtr,pMpgCheckNulT->pStartReadAddr,dstLen1);
                                pBufPtr = pkt->ringBuf;
                                memcpy(pBufPtr,pMpgCheckNulT->pStartReadAddr + dstLen1,
                                        srcLen1 - dstLen1);
                                memcpy(pBufPtr+(srcLen1-dstLen1),pMpgCheckNulT->pDataBuf,srcLen2);
                            }
                            else
                            {
                                pBufPtr = pkt->buf;
                                memcpy(pBufPtr,pMpgCheckNulT->pStartReadAddr,srcLen1);
                                memcpy(pBufPtr+srcLen1,pMpgCheckNulT->pDataBuf,dstLen1-srcLen1);
                                pBufPtr = pkt->ringBuf;
                                memcpy(pBufPtr,pMpgCheckNulT->pDataBuf+(dstLen1-srcLen1),dstLen2);
                            }
                        }
                    }

                    pMpgCheckNulT->bFirstNulFlag = 1;
                    pMpgCheckNulT->bSecodNulFlag = 0;
                    pMpgCheckNulT->pStartReadAddr = pMpgCheckNulT->pEndReadAddr;
                }
            }
        }
        else
        {
            nRemainSize = pkt->buflen;
            pBufPtr     = pkt->buf;
            for(i=0; i<mMpgParserCxt->mDataChunkT.nSegmentNum; i++)
            {
                if(mMpgParserCxt->mDataChunkT.nSegmentLength[i] <= nRemainSize)
                {
                    memcpy(pBufPtr,mMpgParserCxt->mDataChunkT.pSegmentAddr[i],
                            mMpgParserCxt->mDataChunkT.nSegmentLength[i]);
                    nRemainSize -= mMpgParserCxt->mDataChunkT.nSegmentLength[i];
                    pBufPtr += mMpgParserCxt->mDataChunkT.nSegmentLength[i];
                }
                else
                {
                    memcpy(pBufPtr,mMpgParserCxt->mDataChunkT.pSegmentAddr[i],nRemainSize);
                    pBufPtr = pkt->ringBuf;
                    memcpy(pBufPtr, mMpgParserCxt->mDataChunkT.pSegmentAddr[i]+nRemainSize,
                            mMpgParserCxt->mDataChunkT.nSegmentLength[i]-nRemainSize);
                    pBufPtr += mMpgParserCxt->mDataChunkT.nSegmentLength[i]-nRemainSize;
                    nRemainSize = pkt->length-pkt->buflen -
                        (mMpgParserCxt->mDataChunkT.nSegmentLength[i]-nRemainSize);
                }
            }
        }

        pkt->flags |= (FIRST_PART|LAST_PART);
    }

    mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_FALSE;
    mMpgParserCxt->mDataChunkT.nUpdateSize = 0;
    mMpgParserCxt->mDataChunkT.nSegmentNum = 0;
    pCdxMpgParserT->eStatus = CDX_PSR_IDLE;
    return CDX_SUCCESS;
}

static cdx_int32 CdxMpgParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT * pMediaInfo)
{
    CdxMpgParserT         *pCdxMpgParserT = NULL;
    DvdIfoT     *pDvdIfoS       = NULL;
    MpgParserContextT     *mMpgParserCxt  = NULL;
    struct CdxProgramS    *pProgramS      = NULL;
    cdx_uint8              i = 0;
    cdx_uint8                j = 0;

    pCdxMpgParserT = (CdxMpgParserT *)parser;
    if(!pCdxMpgParserT)
    {
        CDX_LOGE("mpg file parser lib has not been initiated!");
        return -1;
    }
    mMpgParserCxt = (MpgParserContextT *)pCdxMpgParserT->pMpgParserContext;
    pDvdIfoS = (DvdIfoT *)pCdxMpgParserT->pDvdInfo;

    pMediaInfo->fileSize = mMpgParserCxt->nFileLength;
    pMediaInfo->bSeekable = 0;
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pProgramS = &pMediaInfo->program[0];
    memset(pProgramS, 0, sizeof(struct CdxProgramS));

    //1.set if the media file contain audio, video or subtitle
    pProgramS->audioNum= pCdxMpgParserT->nhasAudioNum;
    pProgramS->subtitleNum= pCdxMpgParserT->nhasSubTitleNum;
    pProgramS->videoNum= pCdxMpgParserT->nhasVideoNum;
    pProgramS->flags= 0;
    //if((mMpgParserCxt->bIsPsFlag==1)&&(mMpgParserCxt->nFileTimeLength!=0))
    {
        pMediaInfo->bSeekable = 1;
    }

    //2.set video info
    pProgramS->video[0].eCodecFormat = pCdxMpgParserT->mVideoFormatT.eCodecFormat;
    pProgramS->video[0].nWidth = pCdxMpgParserT->mVideoFormatT.nWidth;
    pProgramS->video[0].nHeight = pCdxMpgParserT->mVideoFormatT.nHeight;
    pProgramS->video[0].nFrameRate = pCdxMpgParserT->mVideoFormatT.nFrameRate;
    pProgramS->video[0].bIs3DStream = 0;

    if(mMpgParserCxt->bRecordSeqInfFlag == 1)
    {
        pProgramS->video[0].nCodecSpecificDataLen = MPG_SEQUENCE_LEN;
        pProgramS->video[0].pCodecSpecificData    = (char*)mMpgParserCxt->nSequenceBuf;
    }
    else
    {
        pProgramS->video[0].nCodecSpecificDataLen = 0;
        pProgramS->video[0].pCodecSpecificData    = NULL;
    }

    //3.set subtitle infomation
    if(pProgramS->subtitleNum!=0)
    {
        for(i=0; i<pProgramS->subtitleNum; i++)
        {
            pProgramS->subtitle[i].nReferenceVideoFrameRate = pProgramS->video[0].nFrameRate;
            pProgramS->subtitle[i].nReferenceVideoWidth     = pProgramS->video[0].nWidth;
            pProgramS->subtitle[i].nReferenceVideoHeight    = pProgramS->video[0].nHeight;

            pProgramS->subtitle[i].eCodecFormat = pCdxMpgParserT->mSubFormatT.eCodecFormat;
            pProgramS->subtitle[i].eTextFormat = pCdxMpgParserT->mSubFormatT.eTextFormat;

            if(pDvdIfoS->titleIfoFlag == CDX_TRUE)
            {
                pProgramS->subtitle[i].nStreamIndex =  pDvdIfoS->subpicIfo.subpicId[i];
                pProgramS->subtitle[i].sPaletteInfo.bValid = 1;
                pProgramS->subtitle[i].sPaletteInfo.nEntryCount = 16;
                for(j=0; j<16; j++)
                {
                   pProgramS->subtitle[i].sPaletteInfo.entry[j] = pDvdIfoS->subpicIfo.palette[j];
                }

                memcpy(pProgramS->subtitle[i].strLang,
                        pDvdIfoS->subpicIfo.subpicLang[i], MAX_LANG_LEN);
            }
            else
            {
                pProgramS->subtitle[i].sPaletteInfo.bValid = 0;
                //memcpy(pMediaInfo->SubtitleStrmList[i].strLang, "unknown language",
                //sizeof("unknown language"));
                strcpy((char*)pProgramS->subtitle[i].strLang, "Unknown subtitle");
            }
        }
    }

    //4.set audio bitstream information
    if(pDvdIfoS->titleIfoFlag == CDX_FALSE)
    {
        for(i=0; i<pProgramS->audioNum; i++)
        {
            pProgramS->audio[i].eCodecFormat = pCdxMpgParserT->mAudioFormatTArry[i].eCodecFormat;
            if(pProgramS->audio[i].eCodecFormat == AUDIO_CODEC_FORMAT_PCM)
            {
                pProgramS->audio[i].eSubCodecFormat = WAVE_FORMAT_DVD_LPCM | ABS_EDIAN_FLAG_BIG;
            }
            pProgramS->audio[i].nChannelNum = pCdxMpgParserT->mAudioFormatTArry[i].nChannelNum;
            pProgramS->audio[i].nAvgBitrate = 0;
            pProgramS->audio[i].nMaxBitRate = 0;
            pProgramS->audio[i].nSampleRate = pCdxMpgParserT->mAudioFormatTArry[i].nSampleRate;
            pProgramS->audio[i].nBitsPerSample =
                    pCdxMpgParserT->mAudioFormatTArry[i].nBitsPerSample;
            //memcpy(pProgramS->audio[i].strLang, "unknown language", sizeof("unknown language"));
            strcpy((char*)pProgramS->audio[i].strLang, "Unknown language");
            pProgramS->audio[i].eDataEncodeType = SUBTITLE_TEXT_FORMAT_UTF8;
        }
    }
    else
    {
        j = 0;
        for(i=0; i<pProgramS->audioNum; i++)
        {
            for(; j<pDvdIfoS->audioIfo.audioNum; j++)
            {
                if(pDvdIfoS->audioIfo.audioCheckFlag[j]==1)
                {
                    break;
                }
           }
           memcpy(pProgramS->audio[i].strLang,
                   pDvdIfoS->audioIfo.audioLang[j], MAX_LANG_LEN);
           pProgramS->audio[i].eDataEncodeType = SUBTITLE_TEXT_FORMAT_UTF8;
           pProgramS->audio[i].eCodecFormat =  pDvdIfoS->audioIfo.audioCodeMode[j];
           pProgramS->audio[i].nChannelNum      = pDvdIfoS->audioIfo.nChannelNum[j];
           pProgramS->audio[i].nBitsPerSample = pDvdIfoS->audioIfo.bitsPerSample[j];
           pProgramS->audio[i].nSampleRate = pDvdIfoS->audioIfo.samplePerSecond[j];
           pProgramS->audio[i].nAvgBitrate = 0;
           pProgramS->audio[i].nMaxBitRate  = 0;
           if(pProgramS->audio[i].eCodecFormat == AUDIO_CODEC_FORMAT_PCM)
           {
                pProgramS->audio[i].eSubCodecFormat = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_BIG;
           }
           j++;
        }
    }

    //5.set total time
    pProgramS->duration= pCdxMpgParserT->nTotalTimeLength;

    return 0;

}

static int CdxMpgParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxMpgParserT          *pCdxMpgParserT     = NULL;
    MpgParserContextT      *pMpgParserContextT = NULL;
    DvdIfoT                *pDvdInfoT          = NULL;
    cdx_int32                 nAudIdx = 0;
    cdx_int32                 nSubIdx = 0;
    cdx_uint32                 i = 0;

    pCdxMpgParserT = (CdxMpgParserT *)parser;

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        {
            nAudIdx = *(int*)param;

            if(nAudIdx >= pCdxMpgParserT->nhasAudioNum || nAudIdx < 0)
            {
                return CDX_SUCCESS;
            }
            else
            {
                pMpgParserContextT = (MpgParserContextT *)pCdxMpgParserT->pMpgParserContext;
                pDvdInfoT = (DvdIfoT*)pCdxMpgParserT->pDvdInfo;
                if(pMpgParserContextT->bSeamlessAudioSwitchFlag == 0)
                {
                    if(pDvdInfoT->titleIfoFlag == 1)
                    {
                        pMpgParserContextT->nAudioId = pDvdInfoT->audioIfo.audioPackId[nAudIdx];
                        pMpgParserContextT->nAudioStreamId
                            = pDvdInfoT->audioIfo.audioStreamId[nAudIdx];
                    }
                    else
                    {
                        pMpgParserContextT->nAudioId = pMpgParserContextT->nAudioIdArray[nAudIdx];
                        pMpgParserContextT->nAudioStreamId
                            = pMpgParserContextT->bAudioStreamIdCode[nAudIdx];
                    }
                    pCdxMpgParserT->bChangeAudioFlag = 1;
                 }
                 else
                 {
                     pCdxMpgParserT->bChangeAudioFlag = 0;
                 }
                 pCdxMpgParserT->bHasSyncAudioFlag = 0;
                 pCdxMpgParserT->bDiscardAud = 0;
                 pMpgParserContextT->bFstAudDataFlag = 0;
            }
            return CDX_SUCCESS;
        }
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
        {
               nSubIdx = *(int*)param;
               pMpgParserContextT = (MpgParserContextT *)pCdxMpgParserT->pMpgParserContext;
               pDvdInfoT = (DvdIfoT*)pCdxMpgParserT->pDvdInfo;

               if((pDvdInfoT->titleIfoFlag==0)||
                    (nSubIdx>=pCdxMpgParserT->nhasSubTitleNum)||(nSubIdx<0))
               {
                    return CDX_SUCCESS;
               }
               else
               {
                    pCdxMpgParserT->nUserSelSubStreamIdx = pDvdInfoT->subpicIfo.subpicId[nSubIdx];
                    if(pCdxMpgParserT->nUserSelSubStreamIdx == pCdxMpgParserT->nSubTitleStreamIndex)
                    {
                        return CDX_SUCCESS;
                    }
                    for(i=0; i<pDvdInfoT->subpicIfo.nHasSubpic; i++)
                    {
                        if(pDvdInfoT->subpicIfo.subpicId[i]== pCdxMpgParserT->nUserSelSubStreamIdx)
                        {
                            pCdxMpgParserT->nSubTitleStreamIndex
                                    = pCdxMpgParserT->nUserSelSubStreamIdx;
                            pMpgParserContextT->nSubpicStreamId =
                                    pCdxMpgParserT->nUserSelSubStreamIdx;
                            pCdxMpgParserT->bSubStreamSyncFlag = 1;
                            break;
                        }
                    }
                    return CDX_SUCCESS;
               }
        }

        case CDX_PSR_CMD_DISABLE_AUDIO:
             //TODO
             break;
        case CDX_PSR_CMD_DISABLE_VIDEO:
             //TODO
             break;
        case CDX_PSR_CMD_DISABLE_SUBTITLE:
             //TODO
             break;
        case CDX_PSR_CMD_SET_DURATION:
             //TODO
             break;
        case CDX_PSR_CMD_REPLACE_STREAM:
             //TODO
             break;
        default:
            break;
    }

    return CDX_SUCCESS;

}

cdx_int32 CdxMpgParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    CdxMpgParserT          *pCdxMpgParserT = NULL;
    MpgParserContextT      *mMpgParserCxt  = NULL;
    pCdxMpgParserT = (CdxMpgParserT *)parser;
    mMpgParserCxt = pCdxMpgParserT->pMpgParserContext;

    if(mMpgParserCxt->bSeekDisableFlag == 1)
    {
        CDX_LOGE("error the stream can not seek !");
        return -1;
    }
    pCdxMpgParserT->eStatus = CDX_PSR_SEEKING;
    pCdxMpgParserT->nError = PSR_OK;
    pCdxMpgParserT->seekTo(pCdxMpgParserT,(cdx_uint32)(timeUs/1000));
    pCdxMpgParserT->eStatus = CDX_PSR_IDLE;
    pCdxMpgParserT->nSeekTime = (cdx_uint32)(timeUs/1000);
    return 0;
}

cdx_uint32 CdxMpgParserAttribute(CdxParserT *parser)
    /*return falgs define as open'mMpgParserCxt falgs*/
{
    CdxMpgParserT      *pCdxMpgParserT;
    pCdxMpgParserT = (CdxMpgParserT*)parser;
    return pCdxMpgParserT->nAttribute;
}

cdx_int32 CdxMpgParserForceStop(CdxParserT *parser)
{
    CdxMpgParserT      *pCdxMpgParserT;
    MpgParserContextT  *pMpgParserContextT = NULL;
    int nRet;

    pCdxMpgParserT = (CdxMpgParserT*)parser;
    if(!pCdxMpgParserT || !pCdxMpgParserT->pMpgParserContext)
    {
        CDX_LOGE("mpg file parser lib has not been initiated!");
        return -1;
    }
    pMpgParserContextT = (MpgParserContextT*)pCdxMpgParserT->pMpgParserContext;

    pCdxMpgParserT->nError = PSR_USER_CANCEL;
    CdxStreamT* pStreamT = pMpgParserContextT->pStreamT;
    nRet = CdxStreamForceStop(pStreamT);
    while(pCdxMpgParserT->eStatus != CDX_PSR_IDLE
            && pCdxMpgParserT->eStatus != CDX_PSR_PREFETCHED)
    {
        usleep(2000);
    }

    pCdxMpgParserT->nError = PSR_OK;
    return 0;
}

cdx_int32 CdxMpgParserGetStatus(CdxParserT *parser)
{
    CdxMpgParserT      *pCdxMpgParserT;
    pCdxMpgParserT = (CdxMpgParserT*)parser;
    return pCdxMpgParserT->nError;
}
cdx_int32 CdxMpgParserInit(CdxParserT *parser)
{
    //*we do nothing here
    CDX_UNUSE(parser);
    return 0;
}

static struct CdxParserOpsS mpgParserOps =
{
    .control         = CdxMpgParserControl,
    .prefetch         = CdxMpgParserPrefetch,
    .read             = CdxMpgParserRead,
    .getMediaInfo     = CdxMpgParserGetMediaInfo,
    .close             = CdxMpgParserClose,
    .seekTo         = CdxMpgParserSeekTo,
    .attribute        = CdxMpgParserAttribute,
    .getStatus        = CdxMpgParserGetStatus,
    .init           = CdxMpgParserInit
};

static CdxParserT *CdxMpgParserOpen(CdxStreamT *stream, cdx_uint32 flag)
{
    int                   nResult;
    CdxMpgParserT        *pCdxMpgParserT = NULL;
    CDX_UNUSE(flag);

    //init mpg parser lib module
    pCdxMpgParserT = MpgInit(&nResult);
    if(!pCdxMpgParserT)
    {
        CDX_LOGE("Initiate mpg file parser lib module failed!");
        return NULL;
    }
    if(nResult < 0)
    {
        CDX_LOGE("Initiate mpg file parser lib module error!");
        return NULL;
    }
    pCdxMpgParserT->eStatus = CDX_PSR_INITIALIZED;

    //open media file to parse file information
    nResult = pCdxMpgParserT->open(pCdxMpgParserT, stream);
    if(nResult < 0)
    {
        CDX_LOGE("open fail !");
        pCdxMpgParserT->nError = PSR_OPEN_FAIL;
        return NULL;
    }

    //initial some global parameter
    pCdxMpgParserT->nVidPtsOffset = 0;
    pCdxMpgParserT->nAudPtsOffset = 0;
    pCdxMpgParserT->nSubPtsOffset = 0;
    pCdxMpgParserT->base.ops = &mpgParserOps;
    pCdxMpgParserT->base.type = CDX_PARSER_MPG;

    pCdxMpgParserT->eStatus = CDX_PSR_IDLE;
    pCdxMpgParserT->nError  = PSR_OK;

    return &pCdxMpgParserT->base;
}

static int FdUriToFilepath(char* uri,char* path)
{
    int ret;
    int fd;
    cdx_int64 offset;
    cdx_int64 size;
    char fdInProc[1024] = {0};

    ret = sscanf(uri, "fd://%d?offset=%lld&length=%lld", &fd, &offset, &size);
    if (ret != 3)
    {
        CDX_LOGE("sscanf failure...(%s)", uri);
        return -1;
    }

    if (fd < 0)
    {
        CDX_LOGE("invalid fd(%d)", fd);
        return -1;
    }

    snprintf(fdInProc, sizeof(fdInProc), "/proc/self/fd/%d", fd);

    ret = readlink(fdInProc, path, 1024 - 1);
    if (ret == -1)
    {
        CDX_LOGE("readlink failure, errno(%d)", errno);
        return -1;
    }

    return 0;
}

static cdx_uint32 CdxMpgParserProbe(CdxStreamProbeDataT *probeData)
{
    cdx_char             *pBufEnd   = NULL;
    cdx_char             *pBuf      = NULL;
    unsigned int        nextCode = 0xffffffff;
    int                 nSysPackNum = 0;
    int                 nMpgPackNum = 0;
    int                 nStreamMapNum = 0;
    int                 nStreamPrivateNum = 0;
    int                 nVidPackNum = 0;
    int                 nAudPackNum = 0;
    int                 nSeqNum = 0;
    int                 num = 0;
    int                 score = 0;
    int                 i = 0;
    int                 nRealVideoPackNum = 0;
    char                *suffix = NULL;
    char                filePath[1024] = {0};

    if(probeData->len < 4)
    {
        CDX_LOGE("error: Probe data is not enough.");
        return 0;
    }

    //detect pack_header_startCode 0x000001BA
    pBufEnd = probeData->buf + probeData->len;
    pBuf    = probeData->buf;

    nextCode= 0xffffffff;

    score = 0;
    while(pBuf < pBufEnd)
    {
        nextCode <<= 8;
        nextCode |= pBuf[0];

        if(i==3 && nextCode==0x2E524D46)   //it is rx
        {
            return 0;
        }
        i++;

        if((nextCode&0xffffff00) == 0x00000100)
        {
            if(nextCode == MPG_SYSTEM_HEADER_START_CODE)           // 0x000001BB
            {
                nSysPackNum++;
            }
            else if(nextCode == MPG_PACK_START_CODE)               // 0x000001BA
            {
                nMpgPackNum++;
            }
            else if(nextCode == MPG_PROGRAM_STREAM_MAP)            // 0x000001BC
            {
                nStreamMapNum++;
            }
            else if((nextCode == MPG_PADDING_STREAM) || (nextCode == MPG_PRIVATE_STREAM_2)
                    || (nextCode == MPG_PRIVATE_STREAM_1))       // 0x000001BE 0x000001BF
            {
                nStreamPrivateNum++;
            }
            else if((nextCode&0xf0) == MPG_VIDEO_ID)
            {
                nVidPackNum++;
                if(nextCode==0x000001e0)
                {
                    nRealVideoPackNum++;
                }
            }
            else if((nextCode&0xff) == 0xfd)
            {
                nVidPackNum++;
            }
            else if((nextCode>=0x01c0) && (nextCode<=0x01df))
            {
                nAudPackNum++;
            }
            else if(nextCode == MPG_SEQUENCE_HEADER_CODE)
            {
                nSeqNum++;
                num = nSysPackNum+nMpgPackNum+nStreamMapNum+
                    nStreamPrivateNum+nVidPackNum+nAudPackNum;
                if(num == 0)
                {
                    score = 100;
                    break;
                }
            }
        }
        pBuf++;
    }

    if(score == 0)
    {
        num = nSysPackNum+nMpgPackNum+nStreamMapNum+nStreamPrivateNum+
                nVidPackNum+nAudPackNum+nSeqNum;
        if(num == 0)
        {
            score = 0;
        }
        else if((nRealVideoPackNum==0)||(nMpgPackNum ==0)||
            ((nStreamPrivateNum+nVidPackNum+nAudPackNum)==0))
        {
            score = 0;
        }
        else if(num < 5)
        {
            score = 0;
        }
        else
        {
            score = 100;
        }
    }

    if (!probeData->uri[0])
        return score;

    if (strncmp("fd://", probeData->uri[0],5) == 0)
    {
        if (FdUriToFilepath(probeData->uri[0],filePath) == 0)
            suffix = strrchr(filePath, '.');
        else
            return score;
    }

    if (!suffix)
        suffix = strrchr(probeData->uri[0], '.');

    if (suffix)
    {
        int mpgSuffixNum = sizeof(mpgSuffix) / sizeof(mpgSuffix[0]);
        for (i = 0; i < mpgSuffixNum; i++)
        {
            if (strcasecmp(mpgSuffix[i], suffix) == 0)
                return score; // suffix match, I'm sure that it's a mpg.
        }
    }

    logd("uri suffix not match for mpg score need cut half");
    return score/2;

}

CdxParserCreatorT mpgParserCtor =
{
    .create    = CdxMpgParserOpen,
    .probe     = CdxMpgParserProbe
};

