/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : mpgRead.c
* Description :read mpg audio and videod ata
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment : read mpg audio and videod ata
*
*
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "mpgRead"
#include <cdx_log.h>

#include "CdxMpgParser.h"
#include "CdxMpgParserImpl.h"
#include "mpgFunc.h"

//***********************************************************//
//***********************************************************//

cdx_uint8 ProcessVideoData(CdxMpgParserT *MpgParser,  MpgVideoFrmItemT* mVidFrmItemT,
                            cdx_uint8*curPtr, cdx_uint32 packetLen)
{
    cdx_uint32 copySize1 = 0;
    cdx_uint32 copySize2 = 0;

    if((MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr+packetLen)
            >= MpgParser->mMpgVidFrmInfT.pVidFrmDataBufEnd)
    {
        copySize1 = MpgParser->mMpgVidFrmInfT.pVidFrmDataBufEnd
                    - MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr;
        copySize2 = packetLen - copySize1;
        memcpy(MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr, curPtr, copySize1);
        curPtr += copySize1;
        MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr =  MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf;
        memcpy(MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr, curPtr, copySize2);
        mVidFrmItemT->nDataSize   += packetLen;
        MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr += copySize2;
        mVidFrmItemT->bBufRepeatFlag = 1;
    }
    else
    {
        memcpy(MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr, curPtr, packetLen);
        mVidFrmItemT->nDataSize   += packetLen;
        MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr += packetLen;
    }
    return 0;
}

cdx_uint8 MpgReadJudgeNextDataType(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32         nNext32bits   = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    while(mMpgParserCxt->mDataChunkT.pReadPtr < (mMpgParserCxt->mDataChunkT.pEndPtr-4))
    {
        nNext32bits = MpgOpenShowDword(mMpgParserCxt->mDataChunkT.pReadPtr);

        if(nNext32bits == 0x01BA)
        {
            mMpgParserCxt->nCheckStatus = MPG_CHECK_NEXT_PACK;
            mMpgParserCxt->bFindFstPackFlag = CDX_TRUE;
            return 0;
        }
        else if(mMpgParserCxt->bFindFstPackFlag == CDX_TRUE)
        {
            if((nNext32bits == mMpgParserCxt->nAudioId) || (nNext32bits == mMpgParserCxt->nVideoId)
                    || (nNext32bits == 0x01BD) || ((nNext32bits & 0xe0) == MPG_AUDIO_ID))
            {
                mMpgParserCxt->nCheckStatus = MPG_CHECK_NEXT_PACKET;
                return 0;
            }
        }
        mMpgParserCxt->mDataChunkT.pReadPtr++;
   }

   return 1;
}

cdx_uint8 MpgReadParserReadData(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_int32  nSize = 0;
    cdx_uint32 want  = 0;
    cdx_int32 size1 = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(mMpgParserCxt->mDataChunkT.pReadPtr == mMpgParserCxt->mDataChunkT.pEndPtr)
    {
        nSize = CdxStreamRead(mMpgParserCxt->pStreamT,
                mMpgParserCxt->mDataChunkT.pStartAddr,MAX_CHUNK_BUF_SIZE);
        if(nSize <=4)
        {

            CDX_LOGV("here1:file pEndPtr flag is true.nSize=%x\n", nSize);
            return 0;
        }

        mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
        mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr + nSize;
    }
    else if((mMpgParserCxt->mDataChunkT.pEndPtr - mMpgParserCxt->mDataChunkT.pReadPtr)<16*1024)
    {
        nSize = mMpgParserCxt->mDataChunkT.pEndPtr - mMpgParserCxt->mDataChunkT.pReadPtr;
        memcpy(mMpgParserCxt->mDataChunkT.pStartAddr,mMpgParserCxt->mDataChunkT.pReadPtr,nSize);
        mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr + nSize;
        mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
        want = mMpgParserCxt->mDataChunkT.pEndAddr - mMpgParserCxt->mDataChunkT.pEndPtr;
        size1 = CdxStreamRead(mMpgParserCxt->pStreamT,mMpgParserCxt->mDataChunkT.pEndPtr,want);
        if((size1 + nSize)<=4)
        {
            CDX_LOGV("here2:file pEndPtr flag is true.\n");
            return 0;
        }
        mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pEndPtr + size1;
    }
    return 1;
}

/******************************************************************************/
/******************************************************************************/

void MpgReadNotFindPackStart(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(mMpgParserCxt->mDataChunkT.nUpdateSize)
        mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
    mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
    mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
}

/******************************************************************************/
/******************************************************************************/

cdx_int16 MpgReadNotEnoughData(CdxMpgParserT *MpgParser)
{
   MpgParserContextT *mMpgParserCxt = NULL;
   cdx_uint32 nSize = 0;
   cdx_uint32 want  = 0;
   cdx_uint32 size1 = 0;

   mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

   if(mMpgParserCxt->mDataChunkT.nUpdateSize)
   {
       mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
       return 1;
   }
   else
   {
       nSize = mMpgParserCxt->mDataChunkT.pEndPtr - mMpgParserCxt->mDataChunkT.pReadPtr;
       memcpy(mMpgParserCxt->mDataChunkT.pStartAddr,mMpgParserCxt->mDataChunkT.pReadPtr,nSize);
       mMpgParserCxt->mDataChunkT.pEndPtr = mMpgParserCxt->mDataChunkT.pStartAddr + nSize;
       mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
       want = mMpgParserCxt->mDataChunkT.pEndAddr - mMpgParserCxt->mDataChunkT.pEndPtr;

       size1 = CdxStreamRead(mMpgParserCxt->pStreamT,mMpgParserCxt->mDataChunkT.pEndPtr,want);
       if(size1 == 0)
       {
           return -1;
       }
       mMpgParserCxt->mDataChunkT.pEndPtr += size1;
       mMpgParserCxt->bNoEnoughDataFlag = CDX_FALSE;
   }
   return 0;
}


cdx_uint8 *MpgReadProcessAudioPacket(CdxMpgParserT *MpgParser, cdx_uint32 cur_id,
                     cdx_uint8 *cur_packet_ptr,  cdx_int64 *packetLen)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    DvdIfoT*           dvdInfo       = NULL;
    cdx_uint8 i     = 0;
    cdx_uint8 code  = 0;
    cdx_uint8 flag1 = 0;
    cdx_uint8 flag2 = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
    dvdInfo = (DvdIfoT*)MpgParser->pDvdInfo;

    if(cur_id == 0x01bd && (*cur_packet_ptr >= 0x20 && *cur_packet_ptr <= 0x3f))
    {
        if(*cur_packet_ptr != mMpgParserCxt->nSubpicStreamId)
        {
            return NULL;
        }
        else
        {
            cur_packet_ptr++;
            *packetLen -= 1;
        }
        return cur_packet_ptr;
    }
    else if(mMpgParserCxt->bSeamlessAudioSwitchFlag == 0)
    {
        code = *cur_packet_ptr;
        if((cur_id & 0xe0) == MPG_AUDIO_ID)
        {
            if(cur_id != mMpgParserCxt->nAudioId)
            {
                return NULL;
            }
        }
        else if(code != mMpgParserCxt->nAudioStreamId)
        {
            return NULL;
        }
    }
    else if(mMpgParserCxt->mDataChunkT.nUpdateSize == 0)
    {
        code = *cur_packet_ptr;
        if(dvdInfo->titleIfoFlag == 1)
        {
            for(i=0; i<MpgParser->nhasAudioNum; i++)
            {
                flag1 = (cur_id!=MPG_PRIVATE_STREAM_1)&&(cur_id==dvdInfo->audioIfo.audioPackId[i]);
                flag2 = (cur_id==MPG_PRIVATE_STREAM_1)&&(code==dvdInfo->audioIfo.audioStreamId[i]);
                if((flag1==1) ||(flag2==1))
                {
                    mMpgParserCxt->nSendAudioIndex = i;
                    break;
                }
            }
        }
        else
        {
            for(i=0; i<MpgParser->nhasAudioNum; i++)
            {
                flag1 = (cur_id!=MPG_PRIVATE_STREAM_1)&&(cur_id==mMpgParserCxt->nAudioIdArray[i]);
                flag2 = (cur_id==MPG_PRIVATE_STREAM_1)
                        &&(code==mMpgParserCxt->bAudioStreamIdCode[i]);
                if((flag1==1) ||(flag2==1))
                {
                    mMpgParserCxt->nSendAudioIndex = i;
                    break;
                }
            }
        }
        //CDX_LOGV("**************i=%d,code=%x, cur_id=%x,mMpgParserCxt->nSendAudioIndex=%d\n",
        //            i, code, cur_id,mMpgParserCxt->nSendAudioIndex);
        if(i == MpgParser->nhasAudioNum)
        {
            return NULL;
        }
    }
    else
    {
        code = *cur_packet_ptr;
        flag1 = (cur_id==MPG_PRIVATE_STREAM_1)&&(code != mMpgParserCxt->mDataChunkT.nSubId);
        flag2 = (cur_id!=MPG_PRIVATE_STREAM_1)&&(cur_id != mMpgParserCxt->mDataChunkT.nId);
        // CDX_LOGV("*************here3:cur_id=%x, code=%x,
        // mMpgParserCxt->mDataChunkT.nSubId=%x, flag1=%x, flag2=%x\n",
        //        cur_id, code,mMpgParserCxt->mDataChunkT.nSubId, flag1, flag2);

        if(flag1==1 || flag2==1)
        {
            mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = 1;
            return cur_packet_ptr;
        }
    }

    if(cur_id != MPG_PRIVATE_STREAM_1)
    {
        return cur_packet_ptr;
    }

    if(*cur_packet_ptr>=0xb0 && *cur_packet_ptr<=0xbf)
    {
        cur_packet_ptr += 5;
        *packetLen -= 5;
    }
    else if((*cur_packet_ptr>=0x80 && *cur_packet_ptr<=0x8f)
            || (*cur_packet_ptr>=0xc0 && *cur_packet_ptr<=0xcf))
    {
        cur_packet_ptr += 4;
        *packetLen -= 4;
    }
    else if(*cur_packet_ptr>=0xa0 && *cur_packet_ptr<=0xaf)
    {
        cur_packet_ptr += 7;
        *packetLen-= 7;
    }
    return cur_packet_ptr;
}

/******************************************************************************/
/******************************************************************************/
cdx_uint8 *MpgReadFindStartCode(CdxMpgParserT *MpgParser, cdx_int16 *result)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8         *curBuf        = NULL;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    curBuf = MpgOpenFindNextStartCode(MpgParser,mMpgParserCxt->mDataChunkT.pReadPtr,
            mMpgParserCxt->mDataChunkT.pEndPtr);
    if(!curBuf || (curBuf && curBuf[3]==0xba))
    {
        mMpgParserCxt->nCheckStatus = MPG_CHECK_NEXT_PACK;
        if(curBuf)
            mMpgParserCxt->mDataChunkT.pReadPtr = curBuf;
        *result = 0;
    }
    else
    {
        mMpgParserCxt->mDataChunkT.pReadPtr = curBuf;
        mMpgParserCxt->nCheckStatus = MPG_CHECK_NEXT_PACKET;
        *result = 1;
    }
    return curBuf;
}


 /******************************************************************************/
 /******************************************************************************/

cdx_uint8 *MpgReadJudgePacket(CdxMpgParserT *MpgParser, cdx_uint8 *cur_ptr,
                cdx_uint32 code, cdx_int16 *result)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32 packet_length;
    cdx_bool   flag1;
    cdx_bool   flag2;
    cdx_bool   flag3;
    cdx_uint8 *curBuf;
    cdx_int16  nRet;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    flag1 = (code != mMpgParserCxt->nVideoId);
    flag2 = ((code != mMpgParserCxt->nAudioId) && (mMpgParserCxt->nAudioId == 0x1bd));
    flag3 = (((code & 0xe0) != MPG_AUDIO_ID) && (mMpgParserCxt->nAudioId != 0x1bd));

    curBuf = cur_ptr;
    *result = -1;

    if(flag1 && (flag2 || flag3))
    {
        packet_length = (cur_ptr[4]<<8) | cur_ptr[5];
        mMpgParserCxt->mDataChunkT.pReadPtr = cur_ptr + 6 +packet_length;
        if(mMpgParserCxt->mDataChunkT.pReadPtr > mMpgParserCxt->mDataChunkT.pEndPtr)
            mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pEndPtr;

        if(packet_length == 0xffff)         //data length is bigger than 2k
        {
            curBuf = MpgOpenFindNextStartCode(MpgParser, cur_ptr+6,
                        mMpgParserCxt->mDataChunkT.pEndPtr);
            if(curBuf && (curBuf < mMpgParserCxt->mDataChunkT.pReadPtr))
                mMpgParserCxt->mDataChunkT.pReadPtr = curBuf;
        }

        curBuf = MpgReadFindStartCode(MpgParser, &nRet);
        *result = nRet;
    }
    return curBuf;
}


/******************************************************************************/
/******************************************************************************/


cdx_uint8 *MpgReadProcessIsISO11172(CdxMpgParserT *MpgParser, cdx_uint8 *curPtr, cdx_int16 *result,
                cdx_int64 *packetLen, cdx_uint32 *ptsLow, cdx_uint32 *ptsHigh)
{
    (void)MpgParser;
    *result = 0;
    *packetLen = (curPtr[0]<<8) | curPtr[1];
    curPtr += 2;
    while(*curPtr == 0xff)
    {
        curPtr ++;
        (*packetLen) -= 1;
    }
    if(((*curPtr)&0xc0)==0x40)
    {
        curPtr += 2;
        (*packetLen) -= 2;
    }
    if(((*curPtr)&0xf0)==0x20)
    {
        MpgTimeMpeg1ReadPackSCR(curPtr, ptsLow, ptsHigh);
        curPtr += 5;
        (*packetLen) -= 5;
        *result = 1;
    }
    else if(((*curPtr)&0xf0)==0x30)
    {
        MpgTimeMpeg1ReadPackSCR(curPtr, ptsLow, ptsHigh);
        curPtr += 10;
        (*packetLen) -= 10;
        *result = 1;
    }
    else if(*curPtr == 0x0f)
    {
        curPtr ++;
        (*packetLen) -= 1;
    }
    return curPtr;
 }


/******************************************************************************/
/******************************************************************************/
cdx_uint8 *MpgReadProcessNonISO11172(CdxMpgParserT *MpgParser, cdx_uint8 *curPtr,
            cdx_int16 *result, cdx_int64 *packetLen, cdx_uint32 *ptsLow, cdx_uint32 *ptsHigh)
{
    cdx_uint32 pes_extension_flag;
    cdx_uint32 pack_field_length;
    cdx_uint32 pes_extension_field_length;
    cdx_int32  pes_header_length;
    cdx_uint32 TS_flag;
    (void)MpgParser;

    *result = 0;
    *packetLen = (curPtr[0]<<8) | curPtr[1];
    curPtr += 3;
    TS_flag = (cdx_uint32)curPtr[0];
    pes_header_length = (cdx_uint32)curPtr[1];
    curPtr += 2;
    *packetLen -= 3;

    if((TS_flag&0xc0)==0x80)
    {
        MpgTimeMpeg1ReadPackSCR(curPtr, ptsLow, ptsHigh);
        curPtr += 5;
        *packetLen -= 5;
        pes_header_length -= 5;
        *result = 1;
    }
    else if((TS_flag&0xc0)==0xc0)
    {
        MpgTimeMpeg1ReadPackSCR(curPtr, ptsLow, ptsHigh);
        curPtr += 10;
        *packetLen -= 10;
        pes_header_length -= 10;
        *result = 1;
    }

    if(TS_flag&0x20)//ESCR_flag
    {
        curPtr += 6;
        *packetLen -= 6;
        pes_header_length -= 6;
    }
    if(TS_flag&0x10)//ES_rate_flag
    {
        curPtr += 3;
        *packetLen -= 3;
        pes_header_length -= 3;
    }
    if(TS_flag&0x08)//DSM_trick_mode_flag
    {
        curPtr += 1;
        *packetLen -= 1;
        pes_header_length -= 1;
    }
    if(TS_flag&0x04)//adational_copy_info_flag
    {
        curPtr += 1;
        *packetLen -= 1;
        pes_header_length -= 1;
    }
    if(TS_flag&0x02)//PES_CRC_flag
    {
        curPtr += 2;
        *packetLen -= 2;
        pes_header_length -= 2;
    }
    if(TS_flag&0x01)//PES_extension_flag
    {
        pes_extension_flag = (cdx_uint32)curPtr[0];
        curPtr += 1;
        *packetLen -= 1;
        pes_header_length -= 1;
        if(pes_extension_flag & 0x80)
        {
            curPtr += 16;
            *packetLen -= 16;
            pes_header_length -= 16;
        }
        if(pes_extension_flag & 0x40)
        {
            pack_field_length = curPtr[0];
            curPtr += 1+pack_field_length;
            *packetLen -= 1+pack_field_length;
            pes_header_length -= 1+pack_field_length;
        }
        if(pes_extension_flag & 0x20)
        {
            curPtr += 2;
            *packetLen -= 2;
            pes_header_length -= 2;
        }
        if(pes_extension_flag & 0x10)
        {
            curPtr += 2;
            *packetLen -= 2;
            pes_header_length -= 2;
        }
        if(pes_extension_flag & 0x01)
        {
            pes_extension_field_length = curPtr[0]&0x7f;
            curPtr += 1+pes_extension_field_length;
            *packetLen -= 1+pes_extension_field_length;
            pes_header_length -= 1+pes_extension_field_length;
        }
    }
    if(pes_header_length<0)
    {
        ;//error
    }
    else if(pes_header_length)
    {
        curPtr += pes_header_length;
        *packetLen -= pes_header_length;
    }
    return curPtr;
}


/******************************************************************************/
/******************************************************************************/


cdx_int16 MpgReadAddPacketIntoArray(CdxMpgParserT *MpgParser, cdx_uint32 cur_id,
                                cdx_uint8 sub_stream_id, cdx_uint8 *curPtr,
                                cdx_int64 packetLen, cdx_int64 orgPacketLen, cdx_bool bHasPtsFlag)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    MpgCheckH264NulT  *pMpgCheckNulT = NULL;
    cdx_int16  nRet;
    cdx_uint32 nCpyDataLen = 0;


    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
    pMpgCheckNulT = &(mMpgParserCxt->mMpgCheckNulT);

    nRet = 2;
    if(!mMpgParserCxt->mDataChunkT.nUpdateSize)
    {
        mMpgParserCxt->mDataChunkT.nId = cur_id;
        mMpgParserCxt->mDataChunkT.nSubId = sub_stream_id;
    }
    if((sub_stream_id == mMpgParserCxt->nSubpicStreamId) && (cur_id == 0x01BD))  //subtitle data
    {
        if((mMpgParserCxt->mDataChunkT.nUpdateSize>0) && bHasPtsFlag)
        {
            return -1;
        }
        if(orgPacketLen < 0x7EC)
            nRet = 0;
        else
            nRet = 1;
    }
    mMpgParserCxt->mDataChunkT.pSegmentAddr[mMpgParserCxt->mDataChunkT.nSegmentNum] = curPtr;
    mMpgParserCxt->mDataChunkT.nSegmentLength[mMpgParserCxt->mDataChunkT.nSegmentNum] = packetLen;
    mMpgParserCxt->mDataChunkT.nSegmentNum ++ ;
    mMpgParserCxt->mDataChunkT.nUpdateSize += packetLen;

    //* this is for h264 , cpy data to another buffer
    if(mMpgParserCxt->mDataChunkT.nId == mMpgParserCxt->nVideoId
       && (MpgParser->mVideoFormatT.eCodecFormat == VIDEO_CODEC_FORMAT_H264 ||
           MpgParser->mVideoFormatT.eCodecFormat == VIDEO_CODEC_FORMAT_H265))
    {
       int nDataLen = 0;
       if(pMpgCheckNulT->pWriteDataAddr > pMpgCheckNulT->pStartReadAddr)
       {
           nDataLen = pMpgCheckNulT->pWriteDataAddr - pMpgCheckNulT->pStartReadAddr;
       }
       else if(pMpgCheckNulT->pWriteDataAddr!=NULL&&pMpgCheckNulT->pStartReadAddr!=NULL)
       {
           nDataLen += pMpgCheckNulT->pEndAddr - pMpgCheckNulT->pStartReadAddr;
           nDataLen += pMpgCheckNulT->pWriteDataAddr - pMpgCheckNulT->pDataBuf;
       }

       if((nDataLen+packetLen) > MPG_H264_CHECK_NUL_BUF_SIZE)
       {
           loge("***error : the check-nul buf size(%d) is too small! nDataLen:%d, packetLen: %lld",
            MPG_H264_CHECK_NUL_BUF_SIZE, nDataLen, packetLen);
           abort();
       }

       if(pMpgCheckNulT->pStartJudgeNulAddr == NULL)
       {
           pMpgCheckNulT->pStartJudgeNulAddr = pMpgCheckNulT->pWriteDataAddr;
       }

       if((pMpgCheckNulT->pEndAddr - pMpgCheckNulT->pWriteDataAddr)<packetLen)
       {
           nCpyDataLen = pMpgCheckNulT->pEndAddr - pMpgCheckNulT->pWriteDataAddr;
           memcpy(pMpgCheckNulT->pWriteDataAddr,curPtr,nCpyDataLen);
           memcpy(pMpgCheckNulT->pDataBuf,curPtr+nCpyDataLen,packetLen - nCpyDataLen);
           pMpgCheckNulT->pWriteDataAddr = pMpgCheckNulT->pDataBuf + (packetLen - nCpyDataLen);
       }
       else
       {
           memcpy(pMpgCheckNulT->pWriteDataAddr,curPtr,packetLen);
           pMpgCheckNulT->pWriteDataAddr += packetLen;
       }
    }
    return nRet;
}

 /******************************************************************************/
 /******************************************************************************/

void MpgReadCheckScrPts(CdxMpgParserT *MpgParser, cdx_uint32 cur_id, cdx_uint32 scrLow,
                        cdx_uint32 scrHigh, cdx_uint32 ptsLow, cdx_uint32 ptsHigh)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32 uTemp;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    uTemp = (scrLow>>1) | (scrHigh<<31);

    if(uTemp > mMpgParserCxt->nPreSCR)
        uTemp -= mMpgParserCxt->nPreSCR;
    else
        uTemp = mMpgParserCxt->nPreSCR - uTemp;

    if(uTemp > (mMpgParserCxt->nFrmStep*100))
    {
        mMpgParserCxt->bSwitchScrOverFlag = CDX_TRUE;
    }
    if(cur_id==mMpgParserCxt->nVideoId && mMpgParserCxt->bSwitchScrOverFlag==CDX_TRUE)
    {
        mMpgParserCxt->nBaseSCR = (ptsLow>>1) | (ptsHigh<<31);
        mMpgParserCxt->nBaseTime = mMpgParserCxt->nPreVideoMaxPts +
                ((1000000UL + (MpgParser->mVideoFormatT.nFrameRate>>1))
                /MpgParser->mVideoFormatT.nFrameRate);
        mMpgParserCxt->nBaseTime += mMpgParserCxt->nBasePts;
        mMpgParserCxt->bSwitchScrOverFlag = CDX_FALSE;
    }
    mMpgParserCxt->nPreSCR = (scrLow>>1) | (scrHigh<<31);
}

cdx_int16 MpgReadProcessVideoPacket(CdxMpgParserT *MpgParser, cdx_uint8* curPtr,
                cdx_uint32 packetLen, cdx_uint8 hasPts, cdx_uint32 nPtsValue)
{
    cdx_uint32 i = 0;
    cdx_uint32 nextWord = 0xFFFFFFFF;
    cdx_uint8  picEndDataBuf[4]={0x00, 0x00, 0x01, 0xB7};
    cdx_uint8  *tmpCurPtr = curPtr;
    cdx_uint32 index = 0;

    MpgVideoFrmItemT* mVidFrmItemT = NULL;

search_pic_start_code:

    while(i < packetLen)
    {
        nextWord <<= 8;
        nextWord |= tmpCurPtr[i];
        if((nextWord==0x00000100)||(nextWord==0x000001B7))
        {
            break;
        }
        i++;
    }

    index =  MpgParser->mMpgVidFrmInfT.nVidFrmRcdItemIdx;
    mVidFrmItemT = &(MpgParser->mMpgVidFrmInfT.mVidFrmItemT[index]);
    if(i == packetLen)
    {
        // cannot find the picture startcode
        if(mVidFrmItemT->nParsePicCodeMode == MPG_PARSE_PIC_START_CODE)
        {
            ProcessVideoData(MpgParser, mVidFrmItemT, tmpCurPtr, packetLen);
        }
        return 0;
    }
    else
    {
        if(mVidFrmItemT->nParsePicCodeMode == MPG_PARSE_PIC_CODE_NONE)
        {
            if(nextWord == 0x000001B7)
            {
                 i += 1;
                 tmpCurPtr += i;
                 packetLen -= i;
            }
            else
            {
                mVidFrmItemT->nParsePicCodeMode = MPG_PARSE_PIC_START_CODE;
                mVidFrmItemT->pStartAddr = MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr;
                mVidFrmItemT->nDataSize  = 0;
                mVidFrmItemT->bBufRepeatFlag = 0;
                mVidFrmItemT->nPtsValid = hasPts;
                mVidFrmItemT->nPtsValue   = nPtsValue;
                i+= 1;
                ProcessVideoData(MpgParser,mVidFrmItemT,tmpCurPtr,i);
                tmpCurPtr += i;
                packetLen -= i;
            }
        }
        else if(mVidFrmItemT->nParsePicCodeMode == MPG_PARSE_PIC_START_CODE)
        {
            mVidFrmItemT->nParsePicCodeMode = MPG_PARSE_PIC_CODE_NONE;
            MpgParser->mMpgVidFrmInfT.nVidFrmRcdItemIdx += 1;
            if(MpgParser->mMpgVidFrmInfT.nVidFrmRcdItemIdx >= MPG_MAX_FRM_NUM)
            {
                MpgParser->mMpgVidFrmInfT.nVidFrmRcdItemIdx = 0;
            }
            MpgParser->mMpgVidFrmInfT.nVidFrmValidItemNums += 1;
            if(nextWord == 0x00000100)
            {
                i -= 3;
                ProcessVideoData(MpgParser,mVidFrmItemT,tmpCurPtr,i);
                tmpCurPtr += i;
                packetLen -= i;
                ProcessVideoData(MpgParser,mVidFrmItemT,picEndDataBuf,4);
            }
            else
            {
                i+= 1;
                ProcessVideoData(MpgParser,mVidFrmItemT,tmpCurPtr,i);
                tmpCurPtr += i;
                packetLen -= i;
            }
        }
        i = 0;
        nextWord = 0xFFFFFFFF;
        goto search_pic_start_code;
    }

    return 0;
}


