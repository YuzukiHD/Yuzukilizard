/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxMpgParserImpl.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment :
*
*
*/

#ifndef _CDX_MPG_PARSER_IMPL_H_
#define _CDX_MPG_PARSER_IMPL_H_

#include "CdxMpgParser.h"

#define MPG_SEQUENCE_HEADER_CODE        ((unsigned int)0x000001b3)
#define MPG_PACK_START_CODE             ((unsigned int)0x000001ba)
#define MPG_SYSTEM_HEADER_START_CODE    ((unsigned int)0x000001bb)
#define MPG_SEQUENCE_END_CODE           ((unsigned int)0x000001b7)
#define MPG_PACKET_START_CODE_MASK      ((unsigned int)0xffffff00)
#define MPG_PACKET_START_CODE_PREFIX    ((unsigned int)0x00000100)
#define MPG_ISO_11172_END_CODE          ((unsigned int)0x000001b9)
#define MPG_NAVIGATION_PACK_START_CODE  ((unsigned int)0x000001bf)

/* mpeg2 */
#define MPG_PROGRAM_STREAM_MAP          0x1bc
#define MPG_PRIVATE_STREAM_1            0x1bd
#define MPG_PADDING_STREAM              0x1be
#define MPG_PRIVATE_STREAM_2            0x1bf


#define MPG_AUDIO_ID                    0xc0
#define MPG_VIDEO_ID                    0xe0
#define MPG_AC3_ID                      0x80
#define MPG_DTS_ID                      0x8a
#define MPG_LPCM_ID                     0xa0
#define MPG_SUB_ID                      0x20
#define MPG_PCI_ID                      0x03d4
#define MPG_DSI_ID                      0x03fa

#define CDX_STREAM_TYPE_VIDEO_MPEG1                 0x01
#define CDX_STREAM_TYPE_VIDEO_MPEG2                 0x02
#define CDX_STREAM_TYPE_AUDIO_MPEG1                 0x03
#define CDX_STREAM_TYPE_AUDIO_MPEG2                 0x04
#define CDX_STREAM_TYPE_PRIVATE_SECTION             0x05
#define CDX_STREAM_TYPE_PRIVATE_DATA                0x06
#define CDX_STREAM_TYPE_AUDIO_AAC                   0x0f
#define CDX_STREAM_TYPE_VIDEO_MPEG4                 0x10
#define CDX_STREAM_TYPE_VIDEO_CODEC_FORMAT_H264     0x1b
#define CDX_STREAM_TYPE_CDX_AUDIO_AC3               0x81
#define CDX_STREAM_TYPE_AUDIO_CODEC_FORMAT_DTS      0x8a
#define CDX_STREAM_TYPE_VIDEO_CODEC_FORMAT_H265     0x24


#define UPDATE_SIZE_TH              (1024 * 32)
#define MAX_DATA_SEG                32

#define MPG_CHECK_NEXT_PACK         0
#define MPG_CHECK_NEXT_PACKET       1
#define MPG_CHECK_NEXT_START_CODE   2

#define AV_RB16(x)                  ((((cdx_uint8*)(x))[0] << 8) | ((cdx_uint8*)(x))[1])

#define MAX_VOB_NS          1024
#define AAC_NUM_PROFILES    3
#define NUM_SAMPLE_RATES    12
#define NUM_DEF_CHAN_MAPS    8

#define   NOT_FIND_NV_PACK     -1
#define   NOT_FIND_PREV_PACK   -2
#define   MPG_SEQUENCE_LEN     240

typedef enum MPG_PACK{mpgPack, sysPack, vidPack, audPack,
        subPack, vidSeq,vc1VidPack, avcVidPack}mpg_pack_type;

typedef struct MpgChunkS
{
    cdx_uint32        nId;
    cdx_uint8         nSubId;
    cdx_uint32        nStcId;
    cdx_uint8        *pStartAddr;
    cdx_uint8        *pEndAddr;
    cdx_uint32        nUpdateSize;
    cdx_uint8        *pReadPtr;
    cdx_uint8        *pEndPtr;
    cdx_uint32        nSegmentNum;
    cdx_uint8        *pSegmentAddr[MAX_DATA_SEG];
    cdx_uint32        nSegmentLength[MAX_DATA_SEG];
    cdx_bool       bFileEndFlag;
    cdx_bool       bWaitingUpdateFlag;
    cdx_bool       bHasPtsFlag;
    cdx_uint8         nChunkNum;
    cdx_uint32        nPts;

} MpgChunkT;

typedef struct MpgCheckH264NulS
{
    cdx_uint8         bFirstNulFlag;
    cdx_uint8         bSecodNulFlag;
    cdx_uint8        *pStartReadAddr;
    cdx_uint8        *pEndReadAddr;
    cdx_uint8        *pStartJudgeNulAddr;
    cdx_uint8        *pWriteDataAddr;
    cdx_uint8        *pDataBuf;
    cdx_uint8        *pEndAddr;
} MpgCheckH264NulT;


typedef struct FileTitleIfoS
{
    cdx_int64 nVobStartPosArray[MAX_VOB_NS];
    cdx_int64 nVobEndPosArray[MAX_VOB_NS];
    cdx_uint32 bVobPreScrArray[MAX_VOB_NS];
    cdx_uint32 nVobBaseTimeArray[MAX_VOB_NS];
    cdx_uint32 nVobTimeLenArray[MAX_VOB_NS];
    cdx_uint8  bVobScrChangedFlagArray[MAX_VOB_NS];
}FileTitleIfoT;

typedef struct MpgParserContextS
{
    MpgChunkT        mDataChunkT;
    MpgCheckH264NulT mMpgCheckNulT;
    FileTitleIfoT    mFileTitleIfoT;
    cdx_uint32       nAudioIdArray[AUDIO_STREAM_LIMIT];
    cdx_uint8        bAudioStreamIdCode[AUDIO_STREAM_LIMIT];

    CdxStreamT     *pStreamT;
    cdx_int64       nFileLength;
    cdx_int64       nStartFpPos;
    cdx_int64       nLastFpPos;
    cdx_int64       nLargestFpPos;
    cdx_int64       nSmallestFpPos;
    cdx_int64       nLastNvPackPos;
    cdx_int32       nLastDiffPts;

    cdx_uint8        bIsPsFlag;
    cdx_uint32       nVideoId;
    cdx_uint32       nAudioId;
    cdx_uint32       nSubpicStreamId;
    cdx_uint32       nAudioStreamId;
    cdx_uint32       nFileTimeLength;
    cdx_uint32       nStartScrLow,nStartScrHigh;
    cdx_uint32       nEndScrLow,nEndScrHigh;
    cdx_uint32       nLastValidVideoPts;
    cdx_uint32       nCurStcId;
    cdx_uint32       nFrmStep;
    cdx_uint32       nCheckStatus;
    cdx_uint32          nPreVideoMaxPts;
    cdx_uint32          nBaseSCR;
    cdx_uint32          nPreSCR;
    cdx_uint32          nBaseTime;
    cdx_uint32       nDisplayTime;
    cdx_uint32       nBasePts;
    cdx_uint32       nEndPts;
    cdx_uint32       nFirstPts;
    cdx_uint32       nRecordVobuPosArray[20];
    cdx_uint32       nLastNvTime;

    cdx_uint8       *pPsmEsType;
    cdx_uint8        nAudioPackNum;
    cdx_uint8        nFindPtsNum;
    cdx_uint8        nFileTitleNs;
    cdx_uint8        nCurTitleNum;

    cdx_bool      bIsISO11172Flag;
    cdx_bool      bHasInitAVSFlag;
    cdx_bool      bSwitchScrOverFlag;
    cdx_bool      bSecondPlayFlag;
    cdx_bool      bNoEnoughDataFlag;
    cdx_bool      bFindFstPackFlag;
    cdx_bool      bReleaseAudioFlag;
    cdx_bool      bAudioIdFlag;
    cdx_bool      bGetRightPtsFlag;
    cdx_bool      bHasNvpackFlag;
    cdx_bool      bFindNvpackFlag;
    cdx_bool      bNextVobuPosFlag;
    cdx_bool      bPrevVobuPosFlag;
    cdx_bool      bFstAudDataFlag;
    cdx_bool      bFstVidDataFlag;
    cdx_bool      bFstAudPtsFlag;
    cdx_bool      bFstVidPtsFlag;
    cdx_int64       nFstAudioPts;
    cdx_int64       nFstVideoPts;
    cdx_uint8        nSendAudioIndex;
    cdx_uint8        bSeamlessAudioSwitchFlag;
    cdx_uint8        bPreviewModeFlag;
    cdx_uint8        bRecordSeqInfFlag;
    cdx_uint8        nSequenceBuf[MPG_SEQUENCE_LEN];
    cdx_uint8        bSeekDisableFlag;
    cdx_uint8        bIsNetworkStream;
    cdx_uint8        bFstValidPtsFlag;
} MpgParserContextT;


typedef enum MpgParserResultE
{
    MPG_PSR_RESULT_EOF = -1,
    MPG_PSR_RESULT_ERRFMT = -2,

    MPG_PSR_RESULT_OK = 0,
    MPG_PSR_RESULT_WAITWRITE = 1,

} MpgParserResultT;


CdxMpgParserT* MpgInit(cdx_int32 *nRet);
cdx_int16 MpgExit(CdxMpgParserT *MpgParser);
/*
cdx_int16 MpgOpen(CdxMpgParserT *MpgParser, CdxStreamT *stream);
cdx_int16 MpgClose(CdxMpgParserT *MpgParser);
cdx_int16 MpgRead(CdxMpgParserT *MpgParser);
cdx_int16 MPG_IoCtrl(CdxMpgParserT *MpgParser, cdx_uint32 uCmd, cdx_uint32 uParam);
cdx_int16 MpgSeekTo(CdxMpgParserT *P, cdx_uint32  timeMs );
*/
#endif


