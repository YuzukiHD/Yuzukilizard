/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : dvdTitleInfo.c
* Description : parse dvdinfo file
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment : parse dvdinfo file
*
*
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "dvdTitleIfo"
#include <cdx_log.h>


#include <CdxParser.h>
#include "CdxMpgParser.h"
#include "CdxMpgParserImpl.h"
#include "dvdTitleIfo.h"
#include "mpgFunc.h"

const  char VTS_IFO_IDENTIFIER[] = "DVDVIDEO-VTS";
cdx_uint32 sizex[4] = {720, 704, 352, 352};
cdx_uint32 sizey[2][4] = {{480, 480, 480, 240}, {576, 576, 576, 288}};
const cdx_uint32 frame_rate[16] =
{
    00000,23976,24000,25000,
    29970,30000,50000,59940,
    60000,00000,00000,00000,
    00000,00000,00000,00000
};

//function-A1
cdx_uint32 BSWAP32(cdx_uint8 *dataPtr)
{
    cdx_uint32 realVal = 0;

    realVal = ((dataPtr[0] << 24) | (dataPtr[1] << 16) | (dataPtr[2] << 8) | dataPtr[3]);
    return realVal;
}

//function-A2
cdx_uint16 BSWAP16(cdx_uint8 *dataPtr)
{
    cdx_uint16 realVal = 0;

    realVal = ((dataPtr[0] << 8) | dataPtr[1]);
    return realVal;
}

//function-A3
cdx_uint8 *SetPgcMemory(struct PGCIT *curPgcit, cdx_uint8 *curBufferPtr)
{
    cdx_uint8 i,j;

   struct PgcIfo *curPgc;

    for(i=0; i<PGCIPTR_NS; i++)
    {
       curPgcit->pgciPtr[i] = (struct PgciPtr *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
       curBufferPtr += sizeof(struct PgciPtr) + 3;
    }
     for(i=0; i<PGCIPTR_NS; i++)
    {
       curPgcit->pgciPtr[i]->pgcIfo = (struct PgcIfo *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
       curBufferPtr += sizeof(struct PgcIfo) + 3;
    }
     for(i=0; i<PGCIPTR_NS; i++)
    {
       curPgc = curPgcit->pgciPtr[i]->pgcIfo;
       curPgc->cmdTable = (struct PgcCmdTable *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
       curBufferPtr += sizeof(struct PgcCmdTable) + 3;
    }
     for(i=0; i<PGCIPTR_NS; i++)
     {
        curPgc = curPgcit->pgciPtr[i]->pgcIfo;

        for(j=0; j<CMD_NS; j++)
        {
            curPgc->cmdTable->preCmd[j] = (struct Command *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
            curBufferPtr += sizeof(struct Command) + 3;
        }
        for(j=0; j<CMD_NS; j++)
        {
            curPgc->cmdTable->postCmd[j] = (struct Command *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
            curBufferPtr += sizeof(struct Command) + 3;
        }
        for(j=0; j<CMD_NS; j++)
        {
             curPgc->cmdTable->cellCmd[j] =
                     (struct Command *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
             curBufferPtr += sizeof(struct Command) + 3;
        }
     }
     for(i=0; i<PGCIPTR_NS; i++)
    {
       curPgc = curPgcit->pgciPtr[i]->pgcIfo;
       for(j=0; j<PGMAP_NS; j++)
        {
           curPgc->programMap[j] = (struct PgcPgMap *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
           curBufferPtr += sizeof(struct PgcPgMap) + 3;
         }

    }
     for(i=0; i<PGCIPTR_NS; i++)
    {
       curPgc = curPgcit->pgciPtr[i]->pgcIfo;
       for(j=0; j<CELLPB_NS; j++)
        {
            curPgc->cellPlayback[j] = (struct CellPlayback *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
            curBufferPtr+= sizeof(struct CellPlayback) + 3;
         }
    }
    for(i=0; i<PGCIPTR_NS; i++)
    {
       curPgc = curPgcit->pgciPtr[i]->pgcIfo;
       for(j=0; j<CELLPOS_NS; j++)
        {
           curPgc->cellPosition[j] = (struct CellPosition *)(((uintptr_t)curBufferPtr + 3) & ~0x3);
           curBufferPtr += sizeof(struct CellPosition) + 3;
        }
    }

    return curBufferPtr;
}

//function-A4
void SetVtsMemory(CdxMpgParserT *MpgParser)
{
    DvdIfoT   *dvdIfo       = NULL;
    cdx_uint8 *dvdVtsBuffer = NULL;
    cdx_uint32 redSize      = 0;

    dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;

    redSize = sizeof(struct VTSI) + sizeof(struct MPGCIUT)+ sizeof(struct PgciLangUnit)
            +sizeof(struct PgcIfo) + sizeof(struct PgcCmdTable) + 3*sizeof(struct PGCIT)+
            +3*PGCIPTR_NS * (sizeof(struct PgciPtr)+ 3*CMD_NS* sizeof(struct Command)
            + PGMAP_NS * sizeof(struct PgcPgMap) + CELLPB_NS *sizeof(struct CellPlayback)
            + CELLPOS_NS *sizeof(struct CellPosition)) + CELL_ENTRY_NS *sizeof(struct CellAdEntry);


    redSize += 3*(7+ 3*PGCIPTR_NS+3*CMD_NS+PGMAP_NS+CELLPB_NS+CELLPOS_NS+ TITLE_ENTRY_NS);

    dvdIfo->vtsBuffer = (cdx_uint8 *) malloc(sizeof(cdx_uint8) * redSize);
    dvdVtsBuffer = dvdIfo->vtsBuffer;

    dvdIfo->vtsIfo = (struct VTSI *)(((uintptr_t)dvdVtsBuffer + 3) & ~0x3);
    dvdVtsBuffer += sizeof(struct VTSI) + 3;
    dvdIfo->titlePgcit = (struct PGCIT *)(((uintptr_t)dvdVtsBuffer + 3) & ~0x3);
    dvdVtsBuffer += sizeof(struct PGCIT) + 3;
    dvdVtsBuffer = SetPgcMemory(dvdIfo->titlePgcit, dvdVtsBuffer);
}

//function-A5
void ParseVideoTitleSetInfo(MpgParserContextT *mMpgParserCxt, struct VTSI *vtsIfo)
{
 memset(vtsIfo, 0, sizeof(struct VTSI));
 memcpy(vtsIfo, mMpgParserCxt->mDataChunkT.pReadPtr,sizeof(struct VTSI));
}

//function-A6
cdx_int16 ParsePgciTable(  CdxMpgParserT *MpgParser, struct PGCIT *pgciTab,
        cdx_uint32 startOffset, cdx_uint8 pgcType)
{
    struct PgcIfo     *curPgc        = NULL;
    DvdIfoT           *dvdIfo        = NULL;
    MpgParserContextT *mMpgParserCxt = NULL;

    cdx_uint32 curPgcSa = 0;
    cdx_uint32 readSize = 0;
    cdx_uint16 j;

    dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr + startOffset;
    readSize = sizeof(struct PGCIT) - PGCIPTR_NS * sizeof(struct PgciPtr*);
    memcpy(pgciTab,mMpgParserCxt->mDataChunkT.pReadPtr,readSize);
    mMpgParserCxt->mDataChunkT.pReadPtr += readSize;

    dvdIfo->pgciPtrNum[pgcType] = BSWAP16(pgciTab->pgciNs);
    //type=0 : language; type = 1: title
    if(dvdIfo->pgciPtrNum[pgcType] >  PGCIPTR_NS)
    {
        CDX_LOGW("the pgc nChunkNum is too large.\n");
        return -1;
    }

    for(j = 0; j < dvdIfo->pgciPtrNum[pgcType]; j++)
    {
        readSize = sizeof(struct PgciPtr) - sizeof(struct PgcIfo *);
        memcpy(pgciTab->pgciPtr[j], mMpgParserCxt->mDataChunkT.pReadPtr,
            readSize);// pgc_menu
    }

    readSize = sizeof(struct PgcIfo) - sizeof(struct PgcCmdTable*)
            - PGMAP_NS * sizeof(struct PgcPgMap*)
            - CELLPB_NS * sizeof(struct CellPlayback*)
            -CELLPOS_NS * sizeof(struct CellPosition*);

    for(j=0; j<dvdIfo->pgciPtrNum[pgcType]; j++)
    {
        curPgc = pgciTab->pgciPtr[j]->pgcIfo;
        curPgcSa = startOffset + BSWAP32(pgciTab->pgciPtr[j]->pgciSa);
        mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr + curPgcSa;
        memcpy(curPgc, mMpgParserCxt->mDataChunkT.pReadPtr,readSize);
        mMpgParserCxt->mDataChunkT.pReadPtr += readSize;
    }
    return 0;
}


//function-B1
void JudgeLanguage(cdx_uint8 *str,cdx_uint8 data1, cdx_uint8 data2)
{

    memset(str, 0, MAX_LANG_LEN);

    if((data1 == 0) && (data2==0))
    {
         //memcpy(str, "NoDefined", MAX_LANG_LEN);          // no defined
         strcpy((char *)str, "NoDefined");                          // no defined
    }
    else if((data1 == 'e') && (data2=='n'))
    {
         //memcpy(str, "English", MAX_LANG_LEN);             //Ӣ��
         strcpy((char *)str, "English");                             //Ӣ��
    }
    else if((data1 == 'z') && (data2=='h'))
    {
         //memcpy(str, "Chinese", MAX_LANG_LEN);             //����
         strcpy((char *)str, "Chinese");                             //����
    }
    else if((data1 == 'j') && (data2=='a'))
    {
        //memcpy(str, "Japanese", MAX_LANG_LEN);              //����
        strcpy((char *)str, "Japanese");                              //����
    }
    else if((data1 == 'k') && (data2=='o'))
    {
        //memcpy(str, "Korean", MAX_LANG_LEN);               //����
        strcpy((char *)str, "Korean");                               //����
    }
    else if((data1 == 't') && (data2=='h'))
    {
        //memcpy(str, "Thai", MAX_LANG_LEN);                 //̩��
        strcpy((char *)str, "Thai");                                    //̩��
    }
    else if((data1 == 'f') && (data2=='r'))
    {
        //memcpy(str, "France", MAX_LANG_LEN);               //����
        strcpy((char *)str, "France");                               //����
    }
    else if((data1 == 'd') && (data2=='e'))
    {
        //memcpy(str, "Deutsch", MAX_LANG_LEN);              //����
        strcpy((char *)str, "Deutsch");                              //����
    }
    else if((data1 == 'v') && (data2=='i'))
    {
        //memcpy(str, "Vietnamese", MAX_LANG_LEN);           //Խ����
        strcpy((char *)str, "Vietnamese");                           //Խ����
    }
    else if((data1 == 'r') && (data2=='u'))
    {
        //memcpy(str, "Russian", MAX_LANG_LEN);              //����
        strcpy((char *)str, "Russian");                              //����
    }
    else
    {
        //memcpy(str, "NoDefined", MAX_LANG_LEN);
        strcpy((char *)str, "NoDefined");
    }

}
//*************************************************************//
//************************************************************//
//function-B2
void ParseAudioCodingMode(DvdIfoT *dvdIfo , struct PgcIfo *curPgcIfo,
        cdx_uint8 codingMode, cdx_uint8 indexI,cdx_uint8 indexJ)
{
    switch(codingMode)
    {
    case 0:
        dvdIfo->audioIfo.audioCodeMode[indexJ] = AUDIO_CODEC_FORMAT_AC3;
        dvdIfo->audioIfo.audioPackId[indexJ] = 0x01BD;
        dvdIfo->audioIfo.audioStreamId[indexJ] = 0x80 + (curPgcIfo->astCtl[indexI][0] & 0x03);
        break;
    case 2:
        dvdIfo->audioIfo.audioCodeMode[indexJ] = AUDIO_CODEC_FORMAT_MP1;
        dvdIfo->audioIfo.audioPackId[indexJ] = 0x01C0 + (curPgcIfo->astCtl[indexI][0] & 0x03);
        break;
    case 3:
        dvdIfo->audioIfo.audioCodeMode[indexJ] = AUDIO_CODEC_FORMAT_MP2;
        dvdIfo->audioIfo.audioPackId[indexJ] = 0x01D0 + (curPgcIfo->astCtl[indexI][0] & 0x03);
        break;
    case 4:
        dvdIfo->audioIfo.audioCodeMode[indexJ] = AUDIO_CODEC_FORMAT_PCM;
        dvdIfo->audioIfo.audioPackId[indexJ] = 0x01BD;
        dvdIfo->audioIfo.audioStreamId[indexJ] = 0xA0 + (curPgcIfo->astCtl[indexI][0] & 0x03);
        break;
    case 6:
        dvdIfo->audioIfo.audioCodeMode[indexJ] = AUDIO_CODEC_FORMAT_DTS;
        dvdIfo->audioIfo.audioPackId[indexJ] = 0x01BD;
        dvdIfo->audioIfo.audioStreamId[indexJ] = 0x88 + (curPgcIfo->astCtl[indexI][0] & 0x03);
        break;
    case 7:
    default:
        dvdIfo->audioIfo.audioCodeMode[indexJ] = AUDIO_CODEC_FORMAT_UNKNOWN;
        break;
    }

}

//function-B3
cdx_int16 ParseAudioInfo(CdxMpgParserT *MpgParser, cdx_uint16 audioNum)
{
    struct PgcIfo  *curPgcIfo    = NULL;
    DvdIfoT        *dvdIfo       = NULL;
    struct tAuAttr *curAudioAttr = NULL;

    cdx_uint8  quant_word_len[3]         = {16, 20, 24};
    cdx_uint32 audio_sample_frequency[2] = {48000, 96000};

    cdx_uint8   codingMode = 0;
    cdx_uint16  i = 0;
    cdx_uint16  j = 0;

    dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;
    dvdIfo->audioIfo.audioNum = 0;

    curPgcIfo = dvdIfo->titlePgcit->pgciPtr[0]->pgcIfo;

    for(i=0; i<audioNum; i++)
    {
       if(curPgcIfo->astCtl[i][0] >> 7)
       {
         curAudioAttr = &(dvdIfo->vtsIfo->titleAudioAttr[j]);
         dvdIfo->audioIfo.audioNum += 1;
         codingMode = (curAudioAttr->data[0] >> 5);

         JudgeLanguage(dvdIfo->audioIfo.audioLang[j], curAudioAttr->data[2], curAudioAttr->data[3]);
         dvdIfo->audioIfo.nChannelNum[j] = (curAudioAttr->data[1] & 0x07) + 1;
         dvdIfo->audioIfo.bitsPerSample[j] = quant_word_len[curAudioAttr->data[1] >>6];
         dvdIfo->audioIfo.samplePerSecond[j] =
                 audio_sample_frequency[(curAudioAttr->data[1] & 0x30) >> 4];
         ParseAudioCodingMode(dvdIfo, curPgcIfo,codingMode, i, j);
         j++;
      }
    }
    return 0;
}

//*************************************************************//
//************************************************************//
//function-B4
static cdx_int32 Yuv2Rgb(cdx_uint8 yuv_y, cdx_uint8 yuv_u, cdx_uint8 yuv_v)
{
    cdx_int32   u, v, rdif, invgdif, bdif, r, g, b, rgb;
    u = yuv_u - 128;
    v = yuv_v - 128;

    rdif = v + ((v * 103) >> 8);
    invgdif = ((u * 88) >> 8) +((v * 183) >> 8);
    bdif = u +( (u*198) >> 8);

    r = yuv_y + rdif;
    g = yuv_y - invgdif;
    b = yuv_y + bdif;

    r = r>255?255:r;
    r = r<0?0:r;
    g = g>255?255:g;
    g = g<0?0:g;
    b = b>255?255:b;
    b = b<0?0:b;

    rgb = ((r&0xFF)<<16) + ((g&0xFF)<<8) + (b&0xFF);
    return rgb;

}

//function-B5
void ParseSubPicId(DvdIfoT *dvdIfo , struct PgcIfo *curPgcIfo,cdx_uint8 indexI, cdx_uint8 indexJ)
{
    cdx_uint8 videoAspect43        = 0;
    cdx_uint8 videoAspetWide       = 0;
    cdx_uint8 videoAspectLetterbox = 0;
    cdx_uint8 videoAspectPanScan   = 0;

    videoAspect43        = curPgcIfo->spstCtl[indexI][0] & 0x1f;
    videoAspetWide       = curPgcIfo->spstCtl[indexI][1] & 0x1f;
    videoAspectLetterbox = curPgcIfo->spstCtl[indexI][2] & 0x1f;
    videoAspectPanScan   = curPgcIfo->spstCtl[indexI][3] & 0x1f;

    if(videoAspect43 > 0)
    {
        dvdIfo->subpicIfo.subpicId[indexJ] = 0x20 + videoAspect43;
    }
    else if(videoAspetWide > 0)
    {
        dvdIfo->subpicIfo.subpicId[indexJ] = 0x20 + videoAspetWide;
    }
    else if(videoAspectLetterbox > 0)
    {
        dvdIfo->subpicIfo.subpicId[indexJ] = 0x20 + videoAspectLetterbox;
    }
    else if(videoAspectPanScan > 0)
    {
        dvdIfo->subpicIfo.subpicId[indexJ] = 0x20 + videoAspectPanScan;
    }
    else
    {
        dvdIfo->subpicIfo.subpicId[indexJ] = 0x20;
    }

}

//function-B6
cdx_int16 ParseSubPicInfo(CdxMpgParserT *MpgParser, cdx_uint16 subpicNum)
{
    struct PgcIfo      *curPgcIfo     = NULL;
    DvdIfoT            *dvdIfo           = NULL;
    struct tSubpicAttr *curSubpicAttr = NULL;

    cdx_uint16  i = 0;
    cdx_uint16  j = 0;

    dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;

    dvdIfo->subpicIfo.nHasSubpic = 0;
    curPgcIfo = dvdIfo->titlePgcit->pgciPtr[0]->pgcIfo;

    for(i=0; i<subpicNum; i++)
    {
      if(curPgcIfo->spstCtl[i][0] >> 7)
      {
        curSubpicAttr = &(dvdIfo->vtsIfo->titleSubpicAttr[j]);
        dvdIfo->subpicIfo.nHasSubpic += 1;
        JudgeLanguage(dvdIfo->subpicIfo.subpicLang[j], curSubpicAttr->data[2],
                curSubpicAttr->data[3]);
        ParseSubPicId(dvdIfo,curPgcIfo, i,j);
       j++;
      }
    }

    for(i=0; i<16; i++)
    {
       dvdIfo->subpicIfo.palette[i] = (cdx_uint32)Yuv2Rgb(curPgcIfo->subpicPlt[i][1],
            curPgcIfo->subpicPlt[i][3], curPgcIfo->subpicPlt[i][2]);
    }
     return 0;
}

//************************************************************/
//************************************************************/
//function-B7
void CheckAudioStreamId(CdxMpgParserT *MpgParser, cdx_uint8* curptr,cdx_uint32 startCode)
{
    DvdIfoT   *dvdIfo     = NULL;
    cdx_uint8 *curDataBuf = NULL;
    cdx_int16  nRet       = 0;
    cdx_int64  packetLen  = 0;
    cdx_uint32 curPtsLow  = 0;
    cdx_uint32 curPtsHigh = 0;
    cdx_uint8  i = 0;
    cdx_uint8  j = 0;

    dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;

    curDataBuf = MpgReadProcessNonISO11172(MpgParser, curptr+4,
        &nRet, &packetLen, &curPtsLow, &curPtsHigh);

    if(startCode == MPG_PRIVATE_STREAM_1)
    {
        while(i < packetLen)
        {
            if(curDataBuf[i]!= 0xff)
            {
                curDataBuf += i;
                break;
            }
            i++;
        }
        if(i == packetLen)
            return;

        if(curDataBuf[0] >= 0x80)
        {
            for(i=0; i<dvdIfo->audioIfo.audioNum;i++)
            {
               if(curDataBuf[0] == dvdIfo->audioIfo.audioStreamId[i])
               {
                   if(dvdIfo->audioIfo.audioCheckFlag[i] == 0)
                   {
                      dvdIfo->audioIfo.audioCheckFlag[i] = 1;
                      dvdIfo->audioIfo.audioRightNum++;
                   }
                   break;
                }
            }
            if(i == dvdIfo->audioIfo.audioNum)
            {
                for(j=0; j<dvdIfo->audioIfo.audioWrongNum; j++)
                {
                    if(curDataBuf[0] == dvdIfo->audioIfo.audioStreamId[j])
                    {
                      break;
                    }
                }
               if(j == dvdIfo->audioIfo.audioWrongNum)
               {
                   dvdIfo->audioIfo.audioErrorPackId[dvdIfo->audioIfo.audioWrongNum] = 0x01BD;
                   dvdIfo->audioIfo.audioErrorStrmId[dvdIfo->audioIfo.audioWrongNum]
                           = curDataBuf[0];
                   dvdIfo->audioIfo.audioWrongNum++;
               }
            }
        }
    }
    else if((startCode>=0x01c0)&&(startCode<=0x01df))
    {
        for(i=0; i<dvdIfo->audioIfo.audioNum;i++)
        {
           if(startCode==dvdIfo->audioIfo.audioPackId[i])
           {
              if(dvdIfo->audioIfo.audioCheckFlag[i]==0)
              {
                 dvdIfo->audioIfo.audioCheckFlag[i] = 1;
                 dvdIfo->audioIfo.audioRightNum++;
              }
               break;
            }
        }
        if(i == dvdIfo->audioIfo.audioNum)
        {
            for(j=0; j<dvdIfo->audioIfo.audioWrongNum; j++)
            {
               if(startCode == dvdIfo->audioIfo.audioPackId[j])
               {
                 break;
               }
            }
            if(j == dvdIfo->audioIfo.audioWrongNum)
            {
                dvdIfo->audioIfo.audioErrorPackId[dvdIfo->audioIfo.audioWrongNum] = startCode;
                dvdIfo->audioIfo.audioWrongNum++;
            }
         }
    }
}

//function-B8
void FindSequenceHeader(CdxMpgParserT *MpgParser, cdx_uint8* curptr, cdx_uint8* findSeqFlag)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    cdx_uint32 nextCode = 0;
    cdx_uint8  i = 0;

    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    curptr += 4;

    for(i=0; i<22; i++)
    {
        nextCode = (nextCode<<8) | curptr[0];
        if(nextCode == 0x000001B3)
        {
            mMpgParserCxt->bRecordSeqInfFlag = 1;
            memcpy(mMpgParserCxt->nSequenceBuf,curptr-3, MPG_SEQUENCE_LEN);
            curptr++;
            *findSeqFlag = 1;
            break;
        }
        curptr++;
    }

    if(*findSeqFlag == 1)
    {
        MpgParser->mVideoFormatT.nFrameRate = frame_rate[curptr[3]&0x0f];
        if(MpgParser->mVideoFormatT.nFrameRate > 0)
           MpgParser->mVideoFormatT.nFrameDuration =
           1000 * 1000 / MpgParser->mVideoFormatT.nFrameRate;
    }
}

//function-B9
void CorrectAudioStreamId(CdxMpgParserT *MpgParser, cdx_uint8 audIdx)
{
     DvdIfoT  *dvdIfo     = NULL;
     cdx_uint8 flagAc3    = 0;
     cdx_uint8 flagPcm    = 0;
     cdx_uint8 flagDts    = 0;
     cdx_uint8 newflagAc3 = 0;
     cdx_uint8 newflagPcm = 0;
     cdx_uint8 newflagDts = 0;
     cdx_uint8 i = 0;

     dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;

     flagAc3 = (dvdIfo->audioIfo.audioStreamId[audIdx]>=0x80)
        && (dvdIfo->audioIfo.audioStreamId[audIdx]<=0x83);
     flagPcm = (dvdIfo->audioIfo.audioStreamId[audIdx]>=0xa0)
        && (dvdIfo->audioIfo.audioStreamId[audIdx]<=0xa3);
     flagDts = (dvdIfo->audioIfo.audioStreamId[audIdx]>=0x88)
        && (dvdIfo->audioIfo.audioStreamId[audIdx]<=0x8b);

    for(i=0; i<dvdIfo->audioIfo.audioWrongNum; i++)
    {
        if(dvdIfo->audioIfo.audioErrorPackId[i]==MPG_PRIVATE_STREAM_1)
        {
            newflagAc3 = (dvdIfo->audioIfo.audioErrorStrmId[i]>=0x80)
              && (dvdIfo->audioIfo.audioErrorStrmId[i]<=0x83);
            newflagPcm = (dvdIfo->audioIfo.audioErrorStrmId[i]>=0xa0)
              && (dvdIfo->audioIfo.audioErrorStrmId[i]<=0xa3);
            newflagDts = (dvdIfo->audioIfo.audioErrorStrmId[i]>=0x88)
              && (dvdIfo->audioIfo.audioErrorStrmId[i]<=0x8b);

            if((newflagAc3==flagAc3)||(newflagPcm == flagPcm)||(newflagDts == flagDts))
            {
                dvdIfo->audioIfo.audioStreamId[audIdx] =
                    dvdIfo->audioIfo.audioErrorStrmId[i];
                dvdIfo->audioIfo.audioErrorStrmId[i] = 0;
                dvdIfo->audioIfo.audioErrorPackId[i] = 0;
                dvdIfo->audioIfo.audioRightNum++;
                dvdIfo->audioIfo.audioCheckFlag[audIdx] = 1;
                break;
            }
       }
    }
}

//function-B10
void CorrectAudioPackId(CdxMpgParserT *MpgParser, cdx_uint8 audIdx)
{
     cdx_uint8  flagMp1    = 0;
     cdx_uint8  flagMp2    = 0;
     cdx_uint8  newflagMp1 = 0;
     cdx_uint8  newflagMp2 = 0;
     cdx_uint8  i = 0;

     DvdIfoT  *dvdIfo = NULL;
     dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;

     flagMp1 = ((dvdIfo->audioIfo.audioPackId[audIdx]&0x01e0)==0x01c0);
     flagMp2 = ((dvdIfo->audioIfo.audioPackId[audIdx]&0x01e0)==0x01d0);

     for(i=0; i<dvdIfo->audioIfo.audioWrongNum; i++)
     {
       if(dvdIfo->audioIfo.audioErrorPackId[i]==MPG_PRIVATE_STREAM_2)
       {
          newflagMp1 = ((dvdIfo->audioIfo.audioErrorPackId[i]&0x01e0)==0x01c0);
          newflagMp2 = ((dvdIfo->audioIfo.audioErrorStrmId[i]&0x01e0)==0x01d0);

          if((newflagMp1==flagMp1)||(newflagMp2 == flagMp2))
          {
            dvdIfo->audioIfo.audioPackId[audIdx] = dvdIfo->audioIfo.audioErrorPackId[i];
            dvdIfo->audioIfo.audioErrorStrmId[i] = 0;
            dvdIfo->audioIfo.audioErrorPackId[i] = 0;
            dvdIfo->audioIfo.audioRightNum++;
            dvdIfo->audioIfo.audioCheckFlag[audIdx] = 1;
            break;
          }
       }
    }
}

//function-B11
void CheckVideoAudioInfo(CdxMpgParserT *MpgParser, cdx_uint32 *fstSysPos)
{
   MpgParserContextT *mMpgParserCxt = NULL;
   DvdIfoT   *dvdIfo      = NULL;
   cdx_int32 redLen      = 0;
   cdx_int32 newRedLen   = 0;
   cdx_uint32 nextByte    = 0;
   cdx_uint8* curDataBuf  = NULL;
   cdx_uint8* redPtr      = NULL;
   cdx_uint8  fileEndFlag = 0;
   cdx_uint8  findSeqFlag = 0;
   cdx_uint8  flag1       = 0;
   cdx_uint8  flag2       = 0;
   cdx_uint8  flag3       = 0;
   cdx_uint8  i = 0;

   mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;
   dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;

   CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_SET);
   redLen = CdxStreamRead(mMpgParserCxt->pStreamT,
           mMpgParserCxt->mDataChunkT.pStartAddr, MAX_CHUNK_BUF_SIZE);

new_parse_data:
   curDataBuf = mMpgParserCxt->mDataChunkT.pStartAddr;

   do
   {
        while(redLen > 0)
        {
            nextByte = (nextByte<<8)|curDataBuf[0];
            if(nextByte == 0x000001BA)
            {
                curDataBuf++;
                redLen --;
                break;
            }
            curDataBuf++;
            redLen--;
        }
        if(redLen < (0x800-4))
        {
            if(fileEndFlag == 1)
            {
                break;
            }
            memcpy(mMpgParserCxt->mDataChunkT.pStartAddr, curDataBuf-4, redLen+4);
            newRedLen = (redLen+4);
            newRedLen += CdxStreamRead( mMpgParserCxt->pStreamT,
                mMpgParserCxt->mDataChunkT.pStartAddr+redLen+4, MAX_CHUNK_BUF_SIZE-(redLen+4));
            redLen = newRedLen;
            if(redLen < MAX_CHUNK_BUF_SIZE)
               fileEndFlag = 1;
            goto new_parse_data;
        }
        else
        {
            redPtr = curDataBuf+9;
            redPtr += (redPtr[0]&0x07)+1;
            nextByte = (redPtr[0]<<24)|(redPtr[1]>>16)|(redPtr[2]<<8)|redPtr[3];
            if((nextByte == MPG_SYSTEM_HEADER_START_CODE)&&(*fstSysPos==0))
            {
                *fstSysPos = CdxStreamTell(mMpgParserCxt->pStreamT) - redLen;
            }
            else if((nextByte==0x01e0)&&(findSeqFlag==0))
            {
              FindSequenceHeader(MpgParser, redPtr,&findSeqFlag);
            }
            else if((nextByte==MPG_PRIVATE_STREAM_1)||((nextByte>=0x01c0)&&(nextByte<=0x01df)))
            {
                CheckAudioStreamId(MpgParser, redPtr, nextByte);
            }
            curDataBuf += (0x800-4);
            redLen -=(0x800-4);
     }  // pEndPtr else

     flag1 = ((*fstSysPos) != 0);
     flag2 = (findSeqFlag == 1);
     flag3 = ((dvdIfo->audioIfo.audioWrongNum+dvdIfo->audioIfo.audioRightNum)>=
        dvdIfo->audioIfo.audioNum);

   } while(!(flag1&&flag2&&flag3));

   for(i=0; i<dvdIfo->audioIfo.audioNum; i++)
   {
      if(dvdIfo->audioIfo.audioCheckFlag[i] == 0)
      {
          if(dvdIfo->audioIfo.audioPackId[i] == MPG_PRIVATE_STREAM_1)
          {
             CorrectAudioStreamId(MpgParser, i);
          }
          else if((dvdIfo->audioIfo.audioPackId[i]&0x01e0) == 0x000001c0)
          {
              CorrectAudioPackId(MpgParser,i);
          }
      }
   }
}

cdx_int16 DvdParseTitleInfo(CdxMpgParserT *MpgParser, cdx_char *pUrl)
{
    MpgParserContextT *mMpgParserCxt = NULL;
    DvdIfoT    *dvdIfo        = NULL;
    FILE       *pStreamT      = NULL;
    cdx_uint8   strBuffer[13] = {0};
    cdx_uint32  titlePgcitSa  = 0;
    cdx_uint32  nSize         = 0;
    cdx_uint16  fileNameLen   = 0;
    cdx_int16   k = 0;


    dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;
    mMpgParserCxt = (MpgParserContextT *)MpgParser->pMpgParserContext;

    if (pUrl == NULL)
    {
        return -1;
    }

    fileNameLen = strlen((const char*)pUrl);
    for(k=0; k<13; k++)
    {
        strBuffer[k] = pUrl[fileNameLen-12+k];
    }
   // memcpy(strBuffer, &stream[fileNameLen-12], 13);

    if((strncmp((const char *)strBuffer, "video_ts.vob", 12) == 0) ||
        (strncmp((const char *)strBuffer, "VIDEO_TS.VOB", 12) == 0))
     {
        return -1;
     }
     else
      {
         strBuffer[4] = '0';
         strBuffer[5] = '0';

        if((strncmp((const char *)strBuffer, "vts_00_0.vob", 12) == 0)||
            (strncmp((const char *)strBuffer, "VTS_00_0.VOB", 12) == 0))
        return -1;

        dvdIfo->inputPath = (cdx_uint8 *) malloc (sizeof(cdx_uint8) *(fileNameLen + 1));
        memset(dvdIfo->inputPath, 0, fileNameLen + 1);
       // memcpy(dvdIfo->inputPath, stream, fileNameLen-5);

        for(k=0; k<(fileNameLen-5); k++)
        {
            dvdIfo->inputPath[k] = pUrl[k];
        }
        dvdIfo->inputPath[fileNameLen-5] = '\0';
        strcat((char*)dvdIfo->inputPath, "0.IFO");
        pStreamT = fopen((const char *)dvdIfo->inputPath, "rb");

        if(pStreamT == NULL)        // cannot find the file VTS_0X_Y.IFO
        {
          memset(dvdIfo->inputPath, 0, sizeof(cdx_uint8) *(fileNameLen + 1));
         // memcpy(dvdIfo->inputPath, stream, fileNameLen-5);
          for(k=0; k<(fileNameLen-5); k++)
          {
            dvdIfo->inputPath[k] = pUrl[k];
          }
          dvdIfo->inputPath[fileNameLen-5] = '\0';
          strcat((char*)dvdIfo->inputPath, "0.ifo");
          pStreamT = fopen((char*)dvdIfo->inputPath, "rb");
          if(pStreamT == NULL)
          {
            dvdIfo->titleIfoFlag = CDX_FALSE;
            free(dvdIfo->inputPath);
            dvdIfo->inputPath = NULL;
            return -1;
          }
        }
     }

    strBuffer[12] = '\0';
    nSize = fread(mMpgParserCxt->mDataChunkT.pStartAddr, 1, MAX_CHUNK_BUF_SIZE, pStreamT);
    if(nSize > MAX_CHUNK_BUF_SIZE)
       CDX_LOGW("The length of the info is oo larger.\n");
    mMpgParserCxt->mDataChunkT.pReadPtr = mMpgParserCxt->mDataChunkT.pStartAddr;
    memcpy(strBuffer,mMpgParserCxt->mDataChunkT.pReadPtr, 12);

    if(strncmp((const char *)strBuffer, VTS_IFO_IDENTIFIER, 12) != 0)
     {
        dvdIfo->titleIfoFlag = CDX_FALSE;
        free(dvdIfo->inputPath);
        dvdIfo->inputPath = NULL;
        fclose(pStreamT);
        return -1;
     }
     else
     {
         if(dvdIfo->titleIfoFlag == CDX_FALSE)
         {
            SetVtsMemory(MpgParser);
         }
         ParseVideoTitleSetInfo(mMpgParserCxt, dvdIfo->vtsIfo);
         titlePgcitSa = BSWAP32(dvdIfo->vtsIfo->titlePgcitSa);
         if(titlePgcitSa > 0)
         {
            if(ParsePgciTable(MpgParser, dvdIfo->titlePgcit,
                titlePgcitSa * VOB_VIDEO_LB_LEN,TITLE_PGC) < 0)
            {
                dvdIfo->titleIfoFlag = CDX_FALSE;
                free(dvdIfo->inputPath);
                dvdIfo->inputPath = NULL;
                free(dvdIfo->vtsBuffer);
                fclose(pStreamT);
                return -1;
            }
         }
      }

    dvdIfo->titleIfoFlag = CDX_TRUE;
    fclose(pStreamT);
    return 0;
}


/**********************************************************************************/
/**************open the VTS_xx_y.VOB file *****************************************/

cdx_int16 DvdOpenTitleFile(CdxMpgParserT *MpgParser, cdx_char *stream)
{
    MpgParserContextT *mMpgParserCxt      = NULL;
    DvdIfoT    *dvdIfo    = NULL;
    cdx_uint32  fstSysPos = 0;
    cdx_uint16  audioNum  = 0;
    cdx_uint16  subpicNum = 0;
    cdx_uint8   srcPicId  = 0;
    cdx_uint8   tvSystem  = 0;

    (void)stream;
    mMpgParserCxt      = (MpgParserContextT *)MpgParser->pMpgParserContext;
    dvdIfo = (DvdIfoT *) MpgParser->pDvdInfo;

    //mMpgParserCxt->pStreamT = (struct cdx_stream_info *)create_stream_handle(stream);

    if(mMpgParserCxt->pStreamT == NULL)
    {
        return FILE_PARSER_OPEN_FILE_FAIL;
    }

    CdxStreamSeek(mMpgParserCxt->pStreamT, 0, STREAM_SEEK_END);
    mMpgParserCxt->nFileLength = CdxStreamTell(mMpgParserCxt->pStreamT);

    mMpgParserCxt->bIsPsFlag = 1;

    //video information
    mMpgParserCxt->nVideoId = 0x01E0;
    MpgParser->bHasVideoFlag = CDX_TRUE;
    MpgParser->nhasVideoNum = 1;


    //video infomation
    tvSystem = (dvdIfo->vtsIfo->titleVideoAttr[0] & 0x30) >> 4;  //0: 525/60     1: 625/60
    srcPicId = (dvdIfo->vtsIfo->titleVideoAttr[1] & 0x38) >> 3;
    audioNum = BSWAP16(dvdIfo->vtsIfo->titleAudioStreamNs);
    subpicNum = BSWAP16(dvdIfo->vtsIfo->titleSubpicStreamNs);
    if(audioNum != 0)
       ParseAudioInfo(MpgParser, audioNum);
    if(subpicNum != 0)
       ParseSubPicInfo(MpgParser, subpicNum);
    CheckVideoAudioInfo(MpgParser, &fstSysPos);

    MpgParser->mVideoFormatT.nWidth = sizex[srcPicId];
    MpgParser->mVideoFormatT.nHeight = sizey[tvSystem][srcPicId];

    if(MpgParser->mVideoFormatT.nFrameRate == 0)
    {
       MpgParser->mVideoFormatT.nFrameRate = 25000;
       MpgParser->mVideoFormatT.nFrameDuration = 1000 * 1000 / 25000;
    }
    MpgParser->mVideoFormatT.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
    if((MpgParser->mVideoFormatT.eCodecFormat != VIDEO_CODEC_FORMAT_MPEG1)
        && (MpgParser->mVideoFormatT.eCodecFormat != VIDEO_CODEC_FORMAT_MPEG2))
    {
        return FILE_PARSER_NO_AV;
    }
    mMpgParserCxt->bIsISO11172Flag  = 0;

    // audio information
    if(audioNum == 0)
    {
       MpgParser->bHasAudioFlag = CDX_FALSE;
       MpgParser->nhasAudioNum = 0;
    }
    else
    {
       MpgParser->nhasAudioNum =
           (dvdIfo->audioIfo.audioNum>=dvdIfo->audioIfo.audioRightNum)?
           dvdIfo->audioIfo.audioRightNum:dvdIfo->audioIfo.audioNum;
       MpgParser->bHasAudioFlag = CDX_TRUE;
       mMpgParserCxt->bAudioIdFlag = CDX_TRUE;
       mMpgParserCxt->nAudioId = dvdIfo->audioIfo.audioPackId[0];
       mMpgParserCxt->nAudioStreamId = dvdIfo->audioIfo.audioStreamId[0];
       //mMpgParserCxt->audio_id_num = mMpgParserCxt->nAudioStreamId;
       MpgParser->mAudioFormatT.eCodecFormat = dvdIfo->audioIfo.audioCodeMode[0];

       if(MpgParser->mAudioFormatT.eCodecFormat == AUDIO_CODEC_FORMAT_PCM)
       {
          MpgParser->mAudioFormatT.eSubCodecFormat = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_BIG;
       }
        MpgParser->mAudioFormatT.nBitsPerSample = dvdIfo->audioIfo.bitsPerSample[0];
        MpgParser->mAudioFormatT.nSampleRate = dvdIfo->audioIfo.samplePerSecond[0];
        MpgParser->mAudioFormatT.nChannelNum = dvdIfo->audioIfo.nChannelNum[0];
    }


    // subPic information
    if(subpicNum == 0)
    {
        MpgParser->nhasSubTitleNum = 0;
        MpgParser->bHasSubTitleFlag = CDX_FALSE;
    }
    else
    {
        //ParseSubPicInfo(MpgParser, beginTitle, subpicNum);
        MpgParser->nhasSubTitleNum = dvdIfo->subpicIfo.nHasSubpic;
        MpgParser->bHasSubTitleFlag = CDX_TRUE;
        mMpgParserCxt->nSubpicStreamId = dvdIfo->subpicIfo.subpicId[0];

        MpgParser->nUserSelSubStreamIdx = mMpgParserCxt->nSubpicStreamId;
        MpgParser->nSubTitleStreamIndex = mMpgParserCxt->nSubpicStreamId;
        MpgParser->mSubFormatT.eCodecFormat = SUBTITLE_CODEC_DVDSUB;
        MpgParser->bSubStreamSyncFlag = 0;
        MpgParser->mSubFormatT.eTextFormat = SUBTITLE_TEXT_FORMAT_UNKNOWN;
    }

    // calculate the file time length
    VobCheckPsPts(MpgParser,fstSysPos);
    if(fstSysPos == 0)
    {
        return FILE_PARSER_NO_AV;
    }
    MpgParser->nTotalTimeLength = mMpgParserCxt->nFileTimeLength;
    CdxStreamSeek(mMpgParserCxt->pStreamT,fstSysPos-4,STREAM_SEEK_SET);
    //shift pointer to the file header
    mMpgParserCxt->mDataChunkT.pReadPtr =
        mMpgParserCxt->mDataChunkT.pEndPtr  =
        mMpgParserCxt->mDataChunkT.pStartAddr;
    mMpgParserCxt->mDataChunkT.nUpdateSize =
        mMpgParserCxt->mDataChunkT.nSegmentNum = 0;
    mMpgParserCxt->mDataChunkT.bWaitingUpdateFlag = CDX_FALSE;
    return 0;
}

