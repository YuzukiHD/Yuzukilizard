/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : mpgFunc.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment :
*
*
*/

#ifndef _MPG_FUNC_H_
#define _MPG_FUNC_H_

#include "CdxMpgParser.h"
#include "dvdTitleIfo.h"

// function interfaces of statusChange.c
void     MpgStatusInitStatusTitleChange(CdxMpgParserT *MpgParser);
void     MpgStatusInitStatusChange(CdxMpgParserT *MpgParser);
void     MpgStatusInitParamsProcess(CdxMpgParserT *MpgParser);
cdx_uint8   MpgStatusSelectSuitPts(CdxMpgParserT *MpgParser);


// function interfaces of mpgTime.c
cdx_int16  MpgTimeMpeg1ReadPackSCR(cdx_uint8 *buf, cdx_uint32 *scr_low,cdx_uint32 *scr_high);
cdx_int16  MpgTimeMpeg2ReadPackSCR(cdx_uint8 *buf, cdx_uint32 *scr_base_low,
            cdx_uint32 *scr_base_high,cdx_uint32 *scr_ext);
cdx_uint32  MpgTimePts2Ms(cdx_uint32 PTS,cdx_uint32 nBaseSCR,cdx_uint32 nBaseTime);
cdx_int16   MpgTimeCheckPsPts(CdxMpgParserT *MpgParser);
cdx_int16 MpgTimeCheckPsNetWorkStreamPts(CdxMpgParserT *MpgParser, u32 startPos);

cdx_int64  VobCheckStartPts(CdxMpgParserT *MpgParser, cdx_int64 curFpPos);
cdx_int16   VobCheckPsPts(CdxMpgParserT *MpgParser,cdx_int64 fstSysPos);
cdx_int16  VobCheckUseInfo(CdxMpgParserT *MpgParser, cdx_uint8 *buf);

// function interfaces of mpgOpen.c
cdx_uint8  *MpgOpenFindPackStartReverse(cdx_uint8 *buf_end, cdx_uint8 *buf_begin);
cdx_uint8  *MpgOpenFindPackStart(cdx_uint8 *buf, cdx_uint8 *buf_end);
cdx_uint8  *MpgOpenFindNextStartCode(CdxMpgParserT *MpgParser,cdx_uint8 *buf, cdx_uint8 *buf_end);
cdx_uint8  *MpgOpenJudgePacketType(CdxMpgParserT *MpgParser, cdx_uint8 *cur_ptr, cdx_int16 *nRet);
cdx_uint8  *MpgOpenSearchStartCode(cdx_uint8 *startPtr,cdx_uint32 dataLen, cdx_uint32 *startCode);
cdx_int16   MpgOpenReaderOpenFile(CdxMpgParserT *MpgParser, CdxStreamT  *stream);
cdx_uint32  MpgOpenShowDword(cdx_uint8 *buf);

// function interfaces of mpgRead.c
cdx_uint8   MpgReadJudgeNextDataType(CdxMpgParserT *MpgParser);
cdx_uint8   MpgReadParserReadData(CdxMpgParserT *MpgParser);
void        MpgReadNotFindPackStart(CdxMpgParserT *MpgParser);
cdx_int16   MpgReadNotEnoughData(CdxMpgParserT *MpgParser);
cdx_uint8  *MpgReadProcessAudioPacket(CdxMpgParserT *MpgParser,
            cdx_uint32 cur_id, cdx_uint8 *cur_packet_ptr,  cdx_int64 *packetLen);
cdx_uint8  *MpgReadFindStartCode(CdxMpgParserT *MpgParser, cdx_int16 *result);
cdx_uint8  *MpgReadJudgePacket(CdxMpgParserT *MpgParser, cdx_uint8 *cur_ptr,
           cdx_uint32 code, cdx_int16 *result);
cdx_uint8  *MpgReadProcessIsISO11172(CdxMpgParserT *MpgParser, cdx_uint8 *curPtr,
           cdx_int16 *result, cdx_int64 *packetLen, cdx_uint32 *ptsLow, cdx_uint32 *ptsHigh);
cdx_uint8  *MpgReadProcessNonISO11172(CdxMpgParserT *MpgParser, cdx_uint8 *curPtr,
           cdx_int16 *result, cdx_int64 *packetLen, cdx_uint32 *ptsLow, cdx_uint32 *ptsHigh);
cdx_int16   MpgReadAddPacketIntoArray(CdxMpgParserT *MpgParser, cdx_uint32 cur_id,
           cdx_uint8 sub_stream_id,cdx_uint8 *curPtr,cdx_int64 packetLen,
           cdx_int64 orgPacketLen, cdx_bool bHasPtsFlag);
void        MpgReadCheckScrPts(CdxMpgParserT *MpgParser, cdx_uint32 cur_id,
           cdx_uint32 scrLow, cdx_uint32 scrHigh,cdx_uint32 ptsLow, cdx_uint32 ptsHigh);
cdx_int16   MpgReadProcessVideoPacket(CdxMpgParserT *MpgParser, cdx_uint8* curPtr,
           cdx_uint32 packetLen, cdx_uint8 hasPts, cdx_uint32 nPtsValue);

#endif /* _MPG_FUNC_H_ */
