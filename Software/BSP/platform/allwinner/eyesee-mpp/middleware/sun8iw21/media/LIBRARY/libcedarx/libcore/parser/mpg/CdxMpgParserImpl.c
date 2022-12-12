/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxMpgPrserImpl.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment :
*
*
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxMpgParseImpl"
#include <cdx_log.h>

#include "CdxMpgParser.h"
#include "CdxMpgParserImpl.h"
#include "dvdTitleIfo.h"
#include "mpgFunc.h"

void DvdInfoExit (CdxMpgParserT *MpgParser)
{
    DvdIfoT *pDvdIfoT = NULL;
    pDvdIfoT = (DvdIfoT *) MpgParser->pDvdInfo;

    if(pDvdIfoT->titleIfoFlag)
    {
        free(pDvdIfoT->vtsBuffer);
        pDvdIfoT->vtsBuffer = NULL;
    }
    if(pDvdIfoT->inputPath)
    {
      free(pDvdIfoT->inputPath);
      pDvdIfoT->inputPath = NULL;
    }
}


void JudgeFileType(CdxMpgParserT *MpgParser, CdxStreamT *stream)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint8* pCheckBufPtr = NULL;
    cdx_uint8* pEndCheckBuf = NULL;
    cdx_int32 nReadLen     = 0;
    cdx_uint32 nStartCode     = 0;
    cdx_uint32 nFindPackNum = 0;
    cdx_char  *pUri         = NULL;
    cdx_char  *extension;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    MpgParser->bIsVOB = 0;
    MpgParser->bForbidContinuePlayFlag = 1;
    mMpgParserCxt->pStreamT = stream;

    CdxStreamGetMetaData(stream, "uri", (void **)&pUri);

    if(mMpgParserCxt->bIsNetworkStream == 0 && pUri != NULL)
        extension = strrchr(pUri,'.');
    else
        extension = NULL;

    if(mMpgParserCxt->bIsNetworkStream == 1)
    {
        return;
    }
    if(extension != NULL && (!strncasecmp(extension, ".m1v", 4)||
        !strncasecmp(extension, ".m2v", 4)))
    {
        return;
    }

    pCheckBufPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
    nReadLen     = CdxStreamRead(mMpgParserCxt->pStreamT,pCheckBufPtr,MAX_CHUNK_BUF_SIZE);
    if(nReadLen < 256)
    {
       CdxStreamClose(mMpgParserCxt->pStreamT);
       return;
    }
    pEndCheckBuf = mMpgParserCxt->mDataChunkT.pStartAddr+nReadLen-256;
    while(pCheckBufPtr <pEndCheckBuf)
    {
       pCheckBufPtr = MpgOpenSearchStartCode(pCheckBufPtr,
           pEndCheckBuf - pCheckBufPtr, &nStartCode);
       if(pCheckBufPtr == NULL)
       {
           MpgParser->bIsVOB = 0;
           MpgParser->bForbidContinuePlayFlag = 1;
           CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_SET);
           return;
       }
       else if(nStartCode == 0x000001BA)
       {
           break;
       }
       else if(nStartCode == 0x01B3)
       {
           MpgParser->bIsVOB = 0;
           MpgParser->bForbidContinuePlayFlag = 1;
           //destory_stream_handle(mMpgParserCxt->pStreamT);
           CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_SET);
           return;
       }
       pCheckBufPtr += 1;
    }

    while(pCheckBufPtr < (mMpgParserCxt->mDataChunkT.pStartAddr+nReadLen-0x800))
   {
        pCheckBufPtr += 0x800;
        nStartCode = (pCheckBufPtr[0]<<24)|(pCheckBufPtr[1]<<16)|
            (pCheckBufPtr[2]<<8)|(pCheckBufPtr[3]);
        if(nStartCode == 0x000001BA)
        {
            nFindPackNum += 1;
        }
        else if(nStartCode != 0x00000000)
        {
            nFindPackNum = 0;
            break;
        }
        if(nFindPackNum > 8)
        {
            break;
        }
    }
    if(nFindPackNum > 1)
    {
        MpgParser->bIsVOB = 1;
        MpgParser->bForbidContinuePlayFlag = 1;
    }
    else
    {
        MpgParser->bIsVOB = 0;
        MpgParser->bForbidContinuePlayFlag = 1;
    }
    CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_SET);
    return;
}

//**************************************************************************************//
//**************************************************************************************//
void InitSendVidFrmParam(CdxMpgParserT *MpgParser)
{
    cdx_uint8 i =0 ;
    MpgParser->mMpgVidFrmInfT.nVidFrmValidItemNums = 0;
    MpgParser->mMpgVidFrmInfT.nVidFrmRcdItemIdx    = 0;
    MpgParser->mMpgVidFrmInfT.nVidFrmSendItemIdx   = 0;

    if(MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf == NULL)
        MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf = (cdx_uint8*)malloc(sizeof(cdx_uint8)*1024*1024);

    if(MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf  == NULL)
    {
        CDX_LOGV("*********malloc memory for MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf failed.\n");
    }
    MpgParser->mMpgVidFrmInfT.pCurVidFrmDataPtr = MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf;
    MpgParser->mMpgVidFrmInfT.pVidFrmDataBufEnd =
        MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf+1024*1024;
    for(i=0; i<6; i++)
    {
        MpgParser->mMpgVidFrmInfT.mVidFrmItemT[i].nParsePicCodeMode = MPG_PARSE_PIC_CODE_NONE;
        MpgParser->mMpgVidFrmInfT.mVidFrmItemT[i].bBufRepeatFlag = 0;
    }
}
//**************************************************************************************//
//**************************************************************************************//

cdx_int16 MpgOpen(CdxMpgParserT *MpgParser, CdxStreamT *stream)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    DvdIfoT   *pDvdIfoT = NULL;
    cdx_int16  nRet     = -1;
    cdx_uint8  nIndex   = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
    pDvdIfoT = (DvdIfoT *) MpgParser->pDvdInfo;

    mMpgParserCxt->bSeekDisableFlag      = !CdxStreamSeekAble(stream);
    mMpgParserCxt->bIsNetworkStream  = CdxStreamIsNetStream(stream);
    mMpgParserCxt->nBaseSCR          = 0;
    mMpgParserCxt->nPreSCR           = 0;
    mMpgParserCxt->nBaseTime         = 0;
    mMpgParserCxt->nFileTitleNs       = 0;
    mMpgParserCxt->bRecordSeqInfFlag      = 0;
    pDvdIfoT->titleIfoFlag = CDX_FALSE;


    if(MpgParser->bForceReturnFlag == 1)
        return FILE_PARSER_OPEN_FILE_FAIL;

    if(mMpgParserCxt->bIsNetworkStream == 1 && mMpgParserCxt->bSeekDisableFlag==1)
    {
        MpgParser->bIsVOB = 0;
        MpgParser->bForbidContinuePlayFlag = 1;
        mMpgParserCxt->pStreamT = stream;
        if(!mMpgParserCxt->pStreamT) {
            return FILE_PARSER_OPEN_FILE_FAIL;
        }
        /*
        mMpgParserCxt->pStreamT = create_stream_handle(stream);
        if(mMpgParserCxt->pStreamT == NULL)
        {
            destory_stream_handle(mMpgParserCxt->pStreamT);
            return FILE_PARSER_OPEN_FILE_FAIL;
        }
        */
    }
    else
    {
        JudgeFileType(MpgParser, stream);
        if(!mMpgParserCxt->pStreamT) {
            CDX_LOGE("FILE_PARSER_OPEN_FILE_FAIL");
            return FILE_PARSER_OPEN_FILE_FAIL;
        }


        if(MpgParser->bIsVOB == 1)
        {
            cdx_char *pUrl = NULL;
            CdxStreamGetMetaData(stream, "uri", (void **)&pUrl);
            if(pUrl)
            {
                nRet = DvdParseTitleInfo(MpgParser, pUrl);
                if(nRet >= 0)
                {
                    if(MpgParser->bForceReturnFlag == 1)
                        return FILE_PARSER_OPEN_FILE_FAIL;
                    nRet = DvdOpenTitleFile(MpgParser, pUrl);
                    if(nRet < 0)
                    {
                        CDX_LOGE("DvdOpenTitleFile fail");
                        return nRet;
                    }
                }
            }
        }
    }

    if(MpgParser->bForceReturnFlag == 1)
        return FILE_PARSER_OPEN_FILE_FAIL;

    if((MpgParser->bIsVOB == 0)|| (nRet < 0))
    {
        MpgParser->nhasAudioNum = 0;
        nRet = MpgOpenReaderOpenFile(MpgParser, stream);
        if(nRet < 0)
        {
            CDX_LOGE("MpgOpenReaderOpenFile fail, nRet(%d)", nRet);
            return nRet;
        }
         nIndex = 0;
         if(MpgParser->nhasAudioNum > 0)
         {
            MpgParser->bHasAudioFlag = 1;
            mMpgParserCxt->nAudioId = mMpgParserCxt->nAudioIdArray[nIndex];
            mMpgParserCxt->nAudioStreamId = mMpgParserCxt->bAudioStreamIdCode[nIndex];
            MpgParser->mAudioFormatT.eCodecFormat =
                MpgParser->mAudioFormatTArry[nIndex].eCodecFormat;

            if(MpgParser->mAudioFormatT.eCodecFormat == AUDIO_CODEC_FORMAT_PCM)
            {
                MpgParser->mAudioFormatT.eSubCodecFormat = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_BIG;
                MpgParser->mAudioFormatT.nBitsPerSample =
                    MpgParser->mAudioFormatTArry[nIndex].nBitsPerSample;
                MpgParser->mAudioFormatT.nSampleRate =
                    MpgParser->mAudioFormatTArry[nIndex].nSampleRate;
                MpgParser->mAudioFormatT.nChannelNum =
                    MpgParser->mAudioFormatTArry[nIndex].nChannelNum;
            }
         }
         if(MpgParser->mVideoFormatT.eCodecFormat != 0)
         {
            MpgParser->bHasVideoFlag = 1;
            MpgParser->nhasVideoNum = 1;
         }
         if(MpgParser->mSubFormatT.eCodecFormat != SUBTITLE_CODEC_UNKNOWN)
         {
            MpgParser->bHasSubTitleFlag = 1;
            MpgParser->nhasSubTitleNum = 1;
            MpgParser->bSubStreamSyncFlag = 0;
            MpgParser->mSubFormatT.eTextFormat = SUBTITLE_TEXT_FORMAT_UNKNOWN;
         }
    }
    //MpgParser->eStatus = CDX_MPG_STATUS_IDLE;
    mMpgParserCxt->nCheckStatus = MPG_CHECK_NEXT_PACK;
    mMpgParserCxt->nFrmStep = (90000000UL/MpgParser->mVideoFormatT.nFrameRate)>>1;
    mMpgParserCxt->nPreVideoMaxPts = 0;
    mMpgParserCxt->bSwitchScrOverFlag = CDX_FALSE;
    MpgParser->bReadFileEndFlag = 0;
#if  MPG_SEND_VID_FRAME
    InitSendVidFrmParam(MpgParser);
#endif

    //init varialbe. the code from MPG_IoCtrl-->CDX_MPG_STATUS_PLAY
    MpgParser->bHasChangedStatus = CDX_TRUE;
       mMpgParserCxt->bGetRightPtsFlag = CDX_TRUE;
    mMpgParserCxt->nFindPtsNum = 0;
    MpgParser->bChangeAudioFlag = 0;
    mMpgParserCxt->bFindNvpackFlag = CDX_TRUE;

    mMpgParserCxt->bFindFstPackFlag = CDX_FALSE;
    mMpgParserCxt->bFstAudDataFlag = 0;
    mMpgParserCxt->bFstVidDataFlag = 0;
    mMpgParserCxt->bFstAudPtsFlag  = 0;
    mMpgParserCxt->bFstVidPtsFlag  = 0;
    mMpgParserCxt->nFstAudioPts    = 0;
    mMpgParserCxt->nFstVideoPts    = 0;
    if(mMpgParserCxt->bSecondPlayFlag)
    {
        mMpgParserCxt->bGetRightPtsFlag = CDX_FALSE;
        if(mMpgParserCxt->bHasNvpackFlag==CDX_TRUE)
        {
           MpgStatusInitStatusTitleChange(MpgParser);
        }
        else
        {
            MpgStatusInitStatusChange(MpgParser);
        }
        MpgParser->bHasSyncVideoFlag = 0;
        MpgParser->bHasSyncAudioFlag = 0;
        mMpgParserCxt->bSecondPlayFlag = CDX_FALSE;
        mMpgParserCxt->nPreVideoMaxPts = 0;
    }
    MpgParser->bFfrrStatusFlag = CDX_FALSE;
    mMpgParserCxt->mDataChunkT.nChunkNum    = 0;
    mMpgParserCxt->bSeamlessAudioSwitchFlag = 1;
    //set bSeamlessAudioSwitchFlag to support multi audio
    return nRet;
}

cdx_int16 MpgClose(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt;

    if(!MpgParser)  return -1;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if(!mMpgParserCxt)      return -1;

    if (mMpgParserCxt->pStreamT)
    {
        CdxStreamClose(mMpgParserCxt->pStreamT);
        mMpgParserCxt->pStreamT = NULL;
    }

    MpgParser->eStatus = CDX_PSR_IDLE;

    return 0;
}

cdx_int16 MpgSeekTo(CdxMpgParserT *MpgParser, cdx_uint32  timeMs )
{
    cdx_uint32         nSeekTime     = 0;
    MpgParserContextT *mMpgParserCxt = NULL;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    /*
    if((MpgParser->eStatus!=CDX_MPG_STATUS_IDLE)
    && (MpgParser->eStatus!=CDX_MPG_STATUS_STOP)
    && (MpgParser->eStatus!=CDX_MPG_STATUS_PLAY))
        return -1;
    */

    nSeekTime = timeMs;

    if(nSeekTime >= mMpgParserCxt->nFileTimeLength)
    {
        MpgParser->nError = PSR_EOS;
        return 0;
    }

    MpgParser->bHasChangedStatus = CDX_TRUE;
       mMpgParserCxt->bGetRightPtsFlag = CDX_TRUE;
    mMpgParserCxt->nFindPtsNum = 0;
    MpgParser->bChangeAudioFlag = 0;
    mMpgParserCxt->bFindNvpackFlag = CDX_TRUE;

    mMpgParserCxt->bFstAudDataFlag = 0;
    mMpgParserCxt->bFstVidDataFlag = 0;
    mMpgParserCxt->bFstAudPtsFlag  = 0;
    mMpgParserCxt->bFstVidPtsFlag  = 0;
    mMpgParserCxt->nFstAudioPts    = 0;
    mMpgParserCxt->nFstVideoPts    = 0;

    mMpgParserCxt->nDisplayTime = nSeekTime;
    mMpgParserCxt->bGetRightPtsFlag = CDX_FALSE;
    mMpgParserCxt->nStartFpPos = mMpgParserCxt->nFileLength/mMpgParserCxt->nFileTimeLength;
    mMpgParserCxt->nStartFpPos *= mMpgParserCxt->nDisplayTime;
    mMpgParserCxt->bPreviewModeFlag = 0;
    mMpgParserCxt->mDataChunkT.bHasPtsFlag         = 0;
    mMpgParserCxt->mDataChunkT.nSegmentNum        = 0;
    mMpgParserCxt->mDataChunkT.nId             = 0xff;
    mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_FALSE;
    mMpgParserCxt->mDataChunkT.nUpdateSize    = 0;

    mMpgParserCxt->mMpgCheckNulT.bFirstNulFlag      = 0;
    mMpgParserCxt->mMpgCheckNulT.bSecodNulFlag      = 0;
    mMpgParserCxt->mMpgCheckNulT.pStartReadAddr     = NULL;
    mMpgParserCxt->mMpgCheckNulT.pEndReadAddr       = NULL;
    mMpgParserCxt->mMpgCheckNulT.pStartJudgeNulAddr = mMpgParserCxt->mMpgCheckNulT.pDataBuf;
    mMpgParserCxt->mMpgCheckNulT.pWriteDataAddr     = mMpgParserCxt->mMpgCheckNulT.pDataBuf;

    if(mMpgParserCxt->bHasNvpackFlag == CDX_TRUE)
        MpgStatusInitStatusTitleChange(MpgParser);
    else
    {
        if(mMpgParserCxt->nFileTimeLength >= 10000)
        {
            CdxStreamSeek(mMpgParserCxt->pStreamT, mMpgParserCxt->nStartFpPos, STREAM_SEEK_SET);
        }
        else
        {
            CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_SET);
        }
    }

    MpgParser->bHasSyncVideoFlag = 0;
    MpgParser->bHasSyncAudioFlag = 0;
    mMpgParserCxt->nPreVideoMaxPts = 0;
    mMpgParserCxt->bSecondPlayFlag = CDX_FALSE;
    MpgParser->bFfrrStatusFlag = CDX_FALSE;
    mMpgParserCxt->mDataChunkT.nChunkNum = 0;
    mMpgParserCxt->nCheckStatus = MPG_CHECK_NEXT_PACK;
    mMpgParserCxt->bFindFstPackFlag = CDX_FALSE;
    //SetVideoClkSpeed(1);
    //MpgParser->eStatus = CDX_MPG_STATUS_PLAY;
    MpgParser->bReadFileEndFlag = 0;
    #if  MPG_SEND_VID_FRAME
    InitSendVidFrmParam(MpgParser);
    #endif
    return 0;
}

/**************************************************
MpgRead()
Functionality : Get the next data block
Return value  :
MPG_PSR_RESULT_EOF,         pEndPtr of file reached
MPG_PSR_RESULT_ERRFMT,      bad format
MPG_PSR_RESULT_OK,          all right
MPG_PSR_RESULT_WAITWRITE,   waiting update
**************************************************/

cdx_int16 MpgRead(CdxMpgParserT *MpgParser)
{
     MpgParserContextT *mMpgParserCxt = NULL;

     cdx_uint8 *pBuf1           = NULL;
     cdx_uint8 *pCurPtr          = NULL;
     cdx_uint8 *pCurPacketPtr = NULL;

     cdx_bool  bHasPtsFlag             = CDX_FALSE;
     cdx_bool  bHasScrFlag             = CDX_FALSE;
     cdx_bool  bDecodeOneSubPicFlag = CDX_FALSE;
     cdx_bool flag1, flag2, flag3, flag4;

     cdx_uint32 nCurScrLow        =0;
     cdx_uint32 nCurScrHigh    =0;
     cdx_uint32 nCurPtsLow        =0;
     cdx_uint32 nCurPtsHigh    =0;
     cdx_uint32 nNext32bits       =0;
     cdx_uint32 nCurId            =0;
     cdx_int32 nSize            = 0;
     cdx_uint32 nCheckPackSize = 0;
     cdx_int64  nPacketLength  = 0;
     cdx_int64  nOrgPacketLen  = 0;
     cdx_int64  nRemainDataLen = 0;

     cdx_uint8  nSubStreamId   = 0;
     cdx_int16  nRet           = 0;

     mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;


             if(mMpgParserCxt->bIsPsFlag)
             {
                 if(MpgParser->bHasChangedStatus)
                 {
                     MpgStatusInitParamsProcess(MpgParser);
                     if(mMpgParserCxt->bHasNvpackFlag== CDX_TRUE)
                        MpgParser->bForbidSwitchScr = CDX_TRUE;
                     else
                     MpgParser->bForbidSwitchScr = CDX_FALSE;
                     MpgParser->bHasChangedStatus = CDX_FALSE;
                 }

parser_read_data:
                 if(mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag)
                     return MPG_PSR_RESULT_WAITWRITE;

                 mMpgParserCxt->mDataChunkT.nUpdateSize = 0;
                 mMpgParserCxt->mDataChunkT.nSegmentNum = 0;
                 mMpgParserCxt->mDataChunkT.bHasPtsFlag = CDX_FALSE;

                 if(MpgReadParserReadData(MpgParser) == 0)
                     return MPG_PSR_RESULT_EOF;

                 nRet = MpgReadJudgeNextDataType(MpgParser);
                  if(nRet == 1)
                  {
                    //if(MpgParser->eStatus == CDX_MPG_STATUS_PLAY)
                    //{
                      nCheckPackSize += MAX_CHUNK_BUF_SIZE;
                      if(nCheckPackSize >= 2*1024*1024)
                       return  MPG_PSR_RESULT_EOF;
                    //}
                    goto parser_read_data;
                  }
                  else
                  {
                    nCheckPackSize = 0;
                    pBuf1 = pCurPtr = mMpgParserCxt->mDataChunkT.pReadPtr;
                    if(mMpgParserCxt->nCheckStatus == MPG_CHECK_NEXT_PACKET)
                        goto parsing_next_packet;
                    else
                        goto not_find_pack;

                }

search_next_pack:
                 pBuf1 = MpgOpenFindPackStart(mMpgParserCxt->mDataChunkT.pReadPtr,
                     mMpgParserCxt->mDataChunkT.pEndPtr);

not_find_pack:
                 if(!pBuf1)
                 {
                     MpgReadNotFindPackStart(MpgParser);
                     if(mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag == CDX_TRUE)
                     {
                        return MPG_PSR_RESULT_OK;
                     }
                     else
                     {
                        goto parser_read_data;
                     }
                 }

                 if(mMpgParserCxt->bIsISO11172Flag)
                 {
                     MpgTimeMpeg1ReadPackSCR(pBuf1+4, &nCurScrLow,&nCurScrHigh);
                     pCurPtr = pBuf1 + 12;
                     bHasScrFlag = CDX_TRUE;
                 }
                 else
                 {
                     MpgTimeMpeg2ReadPackSCR(pBuf1+4, &nCurScrLow,&nCurScrHigh,NULL);
                     pCurPtr = pBuf1 + 13;
                     bHasScrFlag = CDX_TRUE;

                     //skip stuffing byte
                     nSize = pCurPtr[0]&7;
                     pCurPtr += nSize+1;
                 }

parsing_next_packet:
                 if(MpgOpenShowDword(pCurPtr) == MPG_SYSTEM_HEADER_START_CODE)
                 {
                     if((mMpgParserCxt->bHasNvpackFlag==CDX_TRUE)&&
                         (mMpgParserCxt->bFindNvpackFlag == CDX_FALSE)&&
                         (mMpgParserCxt->bGetRightPtsFlag == CDX_TRUE))
                     {
                        nRemainDataLen = mMpgParserCxt->mDataChunkT.pEndPtr - pBuf1;
                        if(nRemainDataLen < VOB_VIDEO_LB_LEN)
                            goto remain_data_not_enough;
                        VobCheckUseInfo(MpgParser,pBuf1);
                        mMpgParserCxt->bFindNvpackFlag = CDX_TRUE;
                        mMpgParserCxt->nLastNvPackPos =
                            CdxStreamTell(mMpgParserCxt->pStreamT) - nRemainDataLen;
                     }
                     if((MpgParser->bIsVOB == 1) &&(mMpgParserCxt->bHasNvpackFlag==CDX_TRUE))
                     {
                        pCurPtr = pBuf1+ VOB_VIDEO_LB_LEN;
                        mMpgParserCxt->mDataChunkT.pReadPtr = pCurPtr;
                        goto search_next_pack;
                     }
                     else
                     {
                        nSize = (pCurPtr[4]<<8) | pCurPtr[5];
                        pCurPtr += nSize + 6;
                     }
                 }

                 nNext32bits = MpgOpenShowDword(pCurPtr);

                 pBuf1 = MpgReadJudgePacket(MpgParser, pCurPtr,nNext32bits, &nRet);
                 if(nRet == 0)
                 {
                     pCurPtr = pBuf1;
                     goto not_find_pack;
                 }
                 else if(nRet == 1)
                 {
                     pCurPtr = pBuf1;
                     goto parsing_next_packet;
                 }


                 if((pCurPtr+6) > mMpgParserCxt->mDataChunkT.pEndPtr)
                 {
       remain_data_not_enough:
                     nRet = MpgReadNotEnoughData(MpgParser);
                     if(nRet == 1)
                         return MPG_PSR_RESULT_OK;
                     else if(nRet == -1)
                        return MPG_PSR_RESULT_EOF;
                     else
                     {
                         pBuf1 = pCurPtr = mMpgParserCxt->mDataChunkT.pReadPtr;
                         if(mMpgParserCxt->nCheckStatus == MPG_CHECK_NEXT_PACKET)
                             goto parsing_next_packet;
                         else
                             goto search_next_pack;
                     }
                 }

                 if(!mMpgParserCxt->mDataChunkT.nUpdateSize)
                     nCurId = nNext32bits;    //right now we should not update chunk_id
                 else if(mMpgParserCxt->mDataChunkT.nId != nNext32bits)
                 {
                     mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
                     return MPG_PSR_RESULT_OK;
                 }

                 pCurPtr += 4;//skip stream_id
                 bHasPtsFlag = CDX_FALSE;
                 nOrgPacketLen  = (pCurPtr[0]<<8) | pCurPtr[1];

                 if(mMpgParserCxt->bIsISO11172Flag)
                 {
                     pCurPtr = MpgReadProcessIsISO11172(MpgParser,
                             pCurPtr, &nRet, &nPacketLength, &nCurPtsLow, &nCurPtsHigh);
                 }
                 else
                 {
                     pCurPtr = MpgReadProcessNonISO11172(MpgParser,
                             pCurPtr, &nRet, &nPacketLength, &nCurPtsLow, &nCurPtsHigh);
                 }

                 if(nRet == 1)
                     bHasPtsFlag = CDX_TRUE;

                 pCurPacketPtr = pCurPtr;
                 nSubStreamId = pCurPtr[0];

                 if(nPacketLength < 0)
                 {
                     goto new_set_rdptr;
                 }
                 else
                 {
                     pCurPtr += nPacketLength;
                 }

                 if(!MpgParser->bFfrrStatusFlag)
                 {
                     nSubStreamId =  pCurPacketPtr[0];
                     if((nNext32bits == 0x01BD) && (mMpgParserCxt->mDataChunkT.nId == 0x01BD))
                     {
                         flag1 = (nSubStreamId >= 0x20) && (nSubStreamId <= 0x3f);
                         flag2 = (mMpgParserCxt->mDataChunkT.nSubId >= 0x20) &&
                                 (mMpgParserCxt->mDataChunkT.nSubId <= 0x3f);
                         flag3 = (flag1 && !flag2);
                         flag4 = (!flag1 && flag2);

                         if((flag3 || flag4) && (mMpgParserCxt->mDataChunkT.nUpdateSize > 0))
                         {
                             mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
                             return MPG_PSR_RESULT_OK;
                         }
                     }
                }
                 if(pCurPtr > mMpgParserCxt->mDataChunkT.pEndPtr)
                 {
                     nRet = MpgReadNotEnoughData(MpgParser);
                     if(nRet == 1)
                         return MPG_PSR_RESULT_OK;
                     else if(nRet == -1)
                        return MPG_PSR_RESULT_EOF;
                     else
                     {
                         pBuf1 = pCurPtr = mMpgParserCxt->mDataChunkT.pReadPtr;
                         if(mMpgParserCxt->nCheckStatus == MPG_CHECK_NEXT_PACKET)
                             goto parsing_next_packet;
                         else
                             goto search_next_pack;
                     }
                 }

                 //processing audio packet
                 flag1 = (nCurId==mMpgParserCxt->nAudioId)&&(mMpgParserCxt->nAudioId == 0x01BD);
                 flag2 = ((nCurId & 0xe0) == MPG_AUDIO_ID);

                 if(flag1 || flag2)
                 {
                     if(MpgParser->bFfrrStatusFlag == 1)
                        goto new_set_rdptr;
                     else
                     {
                        if(MpgParser->bChangeAudioFlag == 1)
                            //cannot find the right audio packet
                        {
                            if((MpgParser->bDiscardAud==0) && bHasPtsFlag)
                            {
                                if((pCurPacketPtr[0]==mMpgParserCxt->nAudioStreamId) && flag1)
                                   MpgParser->bChangeAudioFlag = 0;
                                else if((nCurId==mMpgParserCxt->nAudioId) && flag2)
                                   MpgParser->bChangeAudioFlag = 0;
                            }
                            if(MpgParser->bChangeAudioFlag == 1)
                            {
                               goto new_set_rdptr;
                            }
                        }

                         pCurPacketPtr = MpgReadProcessAudioPacket(MpgParser,
                                 nCurId, pCurPacketPtr, &nPacketLength);
                         if(pCurPacketPtr == NULL)
                         {
                            goto new_set_rdptr;
                         }
                         else if(mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag == CDX_TRUE)
                         {
                             // send the other audio channel
                             return MPG_PSR_RESULT_OK;
                         }
                     }
                 }

                 if(mMpgParserCxt->mDataChunkT.nUpdateSize && bHasPtsFlag)
                 {
                     mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
                     return MPG_PSR_RESULT_OK;
                 }

                 //check the difference of PCR, audio PTS, video PTS
                 if(bHasScrFlag && mMpgParserCxt->bHasInitAVSFlag
                     && bHasPtsFlag && (!MpgParser->bForbidSwitchScr
                     && mMpgParserCxt->bGetRightPtsFlag == CDX_TRUE))
                 {
                     MpgReadCheckScrPts(MpgParser, nCurId, nCurScrLow,
                         nCurScrHigh, nCurPtsLow, nCurPtsHigh);
                 }

  check_pts_status:
                 if(bHasPtsFlag && mMpgParserCxt->bHasInitAVSFlag
                     && mMpgParserCxt->bSwitchScrOverFlag==CDX_FALSE)
                 {
                     mMpgParserCxt->mDataChunkT.bHasPtsFlag = CDX_TRUE;
                     mMpgParserCxt->mDataChunkT.nPts =
                         MpgTimePts2Ms((nCurPtsLow>>1)|(nCurPtsHigh<<31),
                         mMpgParserCxt->nBaseSCR,mMpgParserCxt->nBaseTime);

                     if(mMpgParserCxt->bFstValidPtsFlag == 1
                        && mMpgParserCxt->mDataChunkT.nPts < mMpgParserCxt->nBasePts)
                     {
                        mMpgParserCxt->nBasePts = mMpgParserCxt->mDataChunkT.nPts ;
                     }

                     mMpgParserCxt->bFstValidPtsFlag = 0;

                     if((mMpgParserCxt->bPreviewModeFlag==0)
                         &&(mMpgParserCxt->mDataChunkT.nPts<mMpgParserCxt->nBasePts))
                     {
                        mMpgParserCxt->mDataChunkT.bHasPtsFlag = CDX_FALSE;
                        bHasPtsFlag = CDX_FALSE;
                        goto check_pts_status;
                     }
                     if((mMpgParserCxt->bHasNvpackFlag==CDX_TRUE)
                         &&(MpgParser->bForbidSwitchScr==CDX_TRUE))
                     {
                        //if(MpgParser->eStatus==CDX_MPG_STATUS_PLAY)
                        //{
                            MpgParser->bForbidSwitchScr = CDX_FALSE;
                            mMpgParserCxt->nPreSCR = (nCurScrLow>>1) | (nCurScrHigh<<31);
                        //}
                     }
                    //  if(mMpgParserCxt->mDataChunkT.nPts < mMpgParserCxt->nBasePts)
                    //    mMpgParserCxt->nBasePts = mMpgParserCxt->mDataChunkT.nPts;
                     mMpgParserCxt->mDataChunkT.nPts -= mMpgParserCxt->nBasePts;
                     if((MpgParser->nTotalTimeLength!=0)
                         &&(mMpgParserCxt->mDataChunkT.nPts>=MpgParser->nTotalTimeLength))
                     {
                        mMpgParserCxt->mDataChunkT.nPts = -1;
                        mMpgParserCxt->mDataChunkT.bHasPtsFlag = CDX_FALSE;
                        if(mMpgParserCxt->bGetRightPtsFlag == CDX_FALSE)
                        {
                            goto new_set_rdptr;
                        }
                     }
                     if((mMpgParserCxt->bGetRightPtsFlag == CDX_FALSE)
                         && (mMpgParserCxt->nDisplayTime < mMpgParserCxt->nFileTimeLength))
                         // compare the nPts, when eStatus has changed
                     {
                        nRet = MpgStatusSelectSuitPts(MpgParser);
                         if(nRet == 1)
                             goto parser_read_data;
                         else if(nRet == 2)
                         {

                            return MPG_PSR_RESULT_EOF;
                         }
                         else
                         {
                             mMpgParserCxt->nPreSCR = (nCurScrLow>>1) | (nCurScrHigh<<31);
                         }
                     }
                     if(nCurId==mMpgParserCxt->nVideoId)
                     {
                         if(mMpgParserCxt->mDataChunkT.nPts>mMpgParserCxt->nPreVideoMaxPts)
                             mMpgParserCxt->nPreVideoMaxPts = mMpgParserCxt->mDataChunkT.nPts;
                     }
                 }
                 else if(mMpgParserCxt->bGetRightPtsFlag == CDX_FALSE &&
                     (mMpgParserCxt->nDisplayTime <mMpgParserCxt->nFileTimeLength))
                 {
                     goto new_set_rdptr;
                 }
                 #if MPG_SEND_VID_FRAME

                 if(nCurId==mMpgParserCxt->nVideoId)
                 {
                    MpgReadProcessVideoPacket(MpgParser, pCurPacketPtr,
                            nPacketLength, mMpgParserCxt->mDataChunkT.bHasPtsFlag,
                            mMpgParserCxt->mDataChunkT.nPts);
                    if(MpgParser->mMpgVidFrmInfT.nVidFrmValidItemNums > 0)
                    {
                        mMpgParserCxt->mDataChunkT.pReadPtr = pCurPtr;
                        return MPG_PSR_RESULT_OK;
                    }
                    goto new_set_rdptr;
                 }

                 #endif
                 nRet = MpgReadAddPacketIntoArray(MpgParser, nCurId,nSubStreamId,
                         pCurPacketPtr, nPacketLength, nOrgPacketLen,bHasPtsFlag);
                 if(nRet == -1)
                 {
                     if(bHasPtsFlag && (mMpgParserCxt->mDataChunkT.nUpdateSize > 0))
                     {
                         mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
                         return MPG_PSR_RESULT_OK;
                    }
                 }
                 else if(nRet == 0)
                     bDecodeOneSubPicFlag = CDX_TRUE;
                 else if(nRet == 1)
                     bDecodeOneSubPicFlag = CDX_FALSE;
new_set_rdptr:
                 mMpgParserCxt->mDataChunkT.pReadPtr = pCurPtr;

                 //if the accumulate nSize is over threshold, read operation is OK
                 if((mMpgParserCxt->mDataChunkT.nUpdateSize >= UPDATE_SIZE_TH)
                         || (mMpgParserCxt->mDataChunkT.nSegmentNum>=MAX_DATA_SEG)
                         || (bDecodeOneSubPicFlag == CDX_TRUE))
                 {
                     mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
                     bDecodeOneSubPicFlag = CDX_FALSE;
                     return MPG_PSR_RESULT_OK;
                 }
                 nRemainDataLen = mMpgParserCxt->mDataChunkT.pEndPtr - pCurPtr;
                 if(nRemainDataLen < VOB_VIDEO_LB_LEN)
                 {
                    goto remain_data_not_enough;
                 }
                 pBuf1 = MpgReadFindStartCode(MpgParser, &nRet);
                 pCurPtr = pBuf1;
                 if(nRet == 0)
                 {
                     goto not_find_pack;
                 }
                 else if(nRet == 1)
                 {
                     goto parsing_next_packet;
                 }
            }
            else
            {//only video ES
#if MPG_SEND_VID_FRAME
new_read_es_data:
#endif
                nSize = CdxStreamRead(mMpgParserCxt->pStreamT,
                        mMpgParserCxt->mDataChunkT.pStartAddr,UPDATE_SIZE_TH);

                if(nSize <= 0)
                    return MPG_PSR_RESULT_EOF; //pEndPtr
                #if MPG_SEND_VID_FRAME
                MpgReadProcessVideoPacket(MpgParser,
                        mMpgParserCxt->mDataChunkT.pStartAddr, nSize, 0, 0);
                if(MpgParser->mMpgVidFrmInfT.nVidFrmValidItemNums > 0)
                {
                    return MPG_PSR_RESULT_OK;
                }
                goto new_read_es_data;
                #else
                mMpgParserCxt->mDataChunkT.nUpdateSize = nSize;
                mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_TRUE;
                #endif
            }
    return MPG_PSR_RESULT_OK;
}


//**************************************************//
//*************************************************//

CdxMpgParserT* MpgInit(cdx_int32 *nRet)
{
    CdxMpgParserT     *MpgParser     = NULL;
    MpgParserContextT *mMpgParserCxt = NULL;
    DvdIfoT            *pDvdIfoT      = NULL;

    *nRet = 0;
    MpgParser = (CdxMpgParserT *)malloc(sizeof(CdxMpgParserT));
    if(!MpgParser)
    {
        *nRet = -1;
        return NULL;
    }
    memset(MpgParser, 0, sizeof(CdxMpgParserT));


    MpgParser->pMpgParserContext = NULL;
    mMpgParserCxt = (MpgParserContextT *)malloc(sizeof(MpgParserContextT));
    if(mMpgParserCxt)
    {
        memset(mMpgParserCxt, 0, sizeof(MpgParserContextT));
        mMpgParserCxt->bFstValidPtsFlag = 1;
        MpgParser->pMpgParserContext = (void *)mMpgParserCxt;

        mMpgParserCxt->mDataChunkT.pStartAddr = NULL;
        mMpgParserCxt->mDataChunkT.pStartAddr = (cdx_uint8 *)malloc(MAX_CHUNK_BUF_SIZE);
        if(mMpgParserCxt->mDataChunkT.pStartAddr)
        {
            memset(mMpgParserCxt->mDataChunkT.pStartAddr,0,MAX_CHUNK_BUF_SIZE);
            mMpgParserCxt->mDataChunkT.pEndAddr =
                    mMpgParserCxt->mDataChunkT.pStartAddr + MAX_CHUNK_BUF_SIZE;
        }
        else
            *nRet = -1;

        mMpgParserCxt->mMpgCheckNulT.pDataBuf = (cdx_uint8 *)malloc(MPG_H264_CHECK_NUL_BUF_SIZE);
        if(mMpgParserCxt->mMpgCheckNulT.pDataBuf)
        {
            mMpgParserCxt->mMpgCheckNulT.bFirstNulFlag      = 0;
            mMpgParserCxt->mMpgCheckNulT.bSecodNulFlag      = 0;
            mMpgParserCxt->mMpgCheckNulT.pStartReadAddr     = NULL;
            mMpgParserCxt->mMpgCheckNulT.pEndReadAddr       = NULL;
            mMpgParserCxt->mMpgCheckNulT.pStartJudgeNulAddr =
                    mMpgParserCxt->mMpgCheckNulT.pDataBuf;
            mMpgParserCxt->mMpgCheckNulT.pWriteDataAddr     =
                    mMpgParserCxt->mMpgCheckNulT.pDataBuf;
            mMpgParserCxt->mMpgCheckNulT.pEndAddr           =
                    mMpgParserCxt->mMpgCheckNulT.pDataBuf + MPG_H264_CHECK_NUL_BUF_SIZE;
            memset(mMpgParserCxt->mMpgCheckNulT.pDataBuf,0,MPG_H264_CHECK_NUL_BUF_SIZE);
        }
        else
            *nRet = -1;

        mMpgParserCxt->pPsmEsType = (unsigned char *)malloc(256);
        if(mMpgParserCxt->pPsmEsType)
            memset(mMpgParserCxt->pPsmEsType,0,256);
        else
            *nRet = -1;
    }
    else
        *nRet = -1;

    MpgParser->pDvdInfo = NULL;
    pDvdIfoT = (DvdIfoT *) malloc (sizeof(DvdIfoT));
    if(pDvdIfoT)
    {
        memset(pDvdIfoT, 0, sizeof(DvdIfoT));
        MpgParser->pDvdInfo = (void *)pDvdIfoT;
    }

    //initial function pointers
    MpgParser->open = MpgOpen;
    MpgParser->close = MpgClose;
    MpgParser->read = MpgRead;
    //MpgParser->IoCtrl = MPG_IoCtrl;
    MpgParser->seekTo = MpgSeekTo;

    return MpgParser;
}


cdx_int16 MpgExit(CdxMpgParserT *MpgParser)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    DvdIfoT           *pDvdIfoT      = NULL;

    if(!MpgParser)
      return -1;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
    pDvdIfoT = (DvdIfoT *) MpgParser->pDvdInfo;

    if(pDvdIfoT)
    {
      DvdInfoExit(MpgParser);
      free(MpgParser->pDvdInfo);
      MpgParser->pDvdInfo = NULL;
    }

    if(mMpgParserCxt)
    {
        if(mMpgParserCxt->pPsmEsType)
        {
            free(mMpgParserCxt->pPsmEsType);
            mMpgParserCxt->pPsmEsType = NULL;
        }
        if(mMpgParserCxt->mDataChunkT.pStartAddr)
        {
            free(mMpgParserCxt->mDataChunkT.pStartAddr);
            mMpgParserCxt->mDataChunkT.pStartAddr = NULL;
        }
        if(mMpgParserCxt->mMpgCheckNulT.pDataBuf)
        {
            free(mMpgParserCxt->mMpgCheckNulT.pDataBuf);
            mMpgParserCxt->mMpgCheckNulT.pDataBuf = NULL;
        }
        free(MpgParser->pMpgParserContext);
        MpgParser->pMpgParserContext = NULL;
    }
 #if MPG_SEND_VID_FRAME
    if(MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf != NULL)
    {
        free(MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf);
        MpgParser->mMpgVidFrmInfT.pVidFrmDataBuf = NULL;
    }
#endif
    free(MpgParser);

    return 0;
}


