
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : mpgPrser.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/05/05
*   Comment :
*
*
*/

#ifndef _CDX_MPG_PARSER_H
#define _CDX_MPG_PARSER_H

#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>

//For mpeg4 128K is enough
//for motion jpeg, maybe 256k is needed
#define MAX_CHUNK_BUF_SIZE      (1024 * 256)    //256k
#define MPG_SEND_VID_FRAME        0
#define MPG_PARSE_PIC_CODE_NONE    0
#define MPG_PARSE_PIC_START_CODE   1
#define MPG_PARSE_PIC_END_CODE     2
#define MPG_MAX_FRM_NUM            64

#define MPG_H264_CHECK_NUL_BUF_SIZE (1024 * 512)

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_PREPARED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    //PSR_EOS,
};

typedef enum __FILE_PARSER_RETURN_VAL {
    FILE_PARSER_RETURN_ERROR = -1,
    FILE_PARSER_RETURN_OK = 0,

    FILE_PARSER_READ_END,
    FILE_PARSER_BUF_LACK,
    FILE_PARSER_GET_NONE,
    FILE_PARSER_ERROR_IGNORE,
    FILE_PARSER_PAUSE_GET_DATA,
    FILE_PARSER_ERR_VIDEO_BUF_FULL,
    FILE_PARSER_ERR_AUDIO_BUF_FULL,

    FILE_PARSER_PARA_ERR            = -2,
    FILE_PARSER_OPEN_FILE_FAIL        = -3,
    FILE_PARSER_READ_FILE_FAIL        = -4,
    FILE_PARSER_FILE_FMT_ERR        = -5,
    FILE_PARSER_NO_AV                = -6,
    FILE_PARSER_END_OF_MOVI            = -7,
    FILE_PARSER_REQMEM_FAIL            = -8,
    FILE_PARSER_EXCEPTION            = -9,
    FILE_PARSER_,
} __file_parser_return_val_t;

typedef struct MpgChunkInfoS
{
    cdx_uint32  nChunkType;       //chunk type, audio type, video type, lyric type
    cdx_int32   nTsize;         //total nSize of the chunk, based on byte
    cdx_int32   nRsize;         //data nSize has been read out, based on byte
    cdx_uint32  nTimeStamp;     //time stamp for current video chunk
} MpgChunkInfoT;

typedef struct MPG_VIDEO_FRM_ITEM
{
    cdx_int16   nParsePicCodeMode;
    cdx_uint8*  pStartAddr;
    cdx_uint32  nDataSize;
    cdx_uint8   nPtsValid;
    cdx_uint32  nPtsValue;
    cdx_uint8   bBufRepeatFlag;
}MpgVideoFrmItemT;

typedef struct MPG_VIDEO_FRM_INF
{
    cdx_int32        nVidFrmValidItemNums;
    cdx_int32        nVidFrmRcdItemIdx;
    cdx_int32        nVidFrmSendItemIdx;
    cdx_uint8*       pVidFrmDataBuf;
    cdx_uint8*       pCurVidFrmDataPtr;
    cdx_uint8*       pVidFrmDataBufEnd;
    MpgVideoFrmItemT mVidFrmItemT[MPG_MAX_FRM_NUM];
}MpgVideoFrmInfT;

typedef struct CdxMpgParserS
{
    CdxParserT   base;
    void        *pMpgParserContext; //MpgParserContextT
    void        *pDvdInfo; //DvdIfoT, DvdIfoT

    cdx_int32    eStatus;
    cdx_uint8    bIsVOB;
    cdx_int16    bDiscardAud;
    cdx_uint8    bChangeAudioFlag;

    cdx_bool     bFfrrStatusFlag;
    cdx_bool     bHasChangedStatus;
    cdx_bool     bFindFirstPts;
    cdx_bool     bForbidSwitchScr;
    cdx_bool     bHasVideoFlag;
    cdx_bool     bHasAudioFlag;
    cdx_bool     bHasSubTitleFlag;

    cdx_int16    nhasVideoNum;
    //video flag, =0:no video bitstream; >0:video bitstream count
    cdx_int16    nhasAudioNum;
    //audio flag, =0:no audio bitstream; >0:audio bitstream count
    cdx_int16    nhasSubTitleNum;
    //lyric flag, =0:no lyric bitstream; >0:lyric bitstream count

    cdx_int16    nVideoStreamIndex;
    cdx_int16    nAudioStreamIndex;
    cdx_int16    nSubTitleStreamIndex;
    //lyric stream nIndex
    cdx_int16    nUserSelSubStreamIdx;
    //the lyric stream nIndex which user select.
    cdx_int16    bSubStreamSyncFlag;
    //When user change subtitle,psr need sync.when sync over, set to 0.

    cdx_uint32   nTotalFrames;
    cdx_uint32   nPictureNum;

    cdx_bool     bJumpPointChangeFlag;

    cdx_uint32   nPreFRSpeed;
    //previous ff/rr speed, for dynamic adjust
    cdx_uint32   nFRSpeed;
    //fast forward and fast backward speed, multiple of normal speed
    cdx_uint32   nPreFRPicShowTime;
    cdx_uint32   nFRPicShowTime;
    //picture show time under fast forward and backward
    cdx_uint32   nFRPicCnt;
    //picture count under ff/rr, for check if need delay

    cdx_uint32   nVidPtsOffset;
    //video time offset
    cdx_uint32   nAudPtsOffset;
    //audio time offset
    cdx_uint32   nSubPtsOffset;
    //subtitle time offset

    cdx_int16    bHasSyncVideoFlag;
    //flag, mark that if has sync video
    cdx_int16    bHasSyncAudioFlag;
    //flag, mark that if has sync audio
    cdx_uint32   nCurDispTime;
    cdx_uint32   nTotalTimeLength;
    cdx_bool     bAudioHasAc3Flag;
    cdx_uint8    eCurStatus;
    cdx_uint8    ePreStatus;
    cdx_uint32   nFstSeqAddr;
    cdx_uint8    bForbidContinuePlayFlag;
    cdx_uint8    bForceReturnFlag;

    VideoStreamInfo      mVideoFormatT;
    AudioStreamInfo      mAudioFormatT;
    AudioStreamInfo      mAudioFormatTArry[AUDIO_STREAM_LIMIT];
    //audio format, AUDIO_STREAM_LIMIT
    SubtitleStreamInfo   mSubFormatT;
    //subtitle format

    MpgChunkInfoT        mCurChunkInfT;
    cdx_int32            bFirstVideoFlag;
    cdx_int32            bFirstAudioFlag;

    MpgVideoFrmInfT      mMpgVidFrmInfT;
    cdx_int16            bReadFileEndFlag;
    cdx_int32            nError;
    cdx_uint32           nAttribute;
    CdxPacketT           mCurPacketT;

    cdx_int16   (*open)(struct CdxMpgParserS *MpgParser, CdxStreamT *stream);
    cdx_int16   (*close)(struct CdxMpgParserS *MpgParser);
    cdx_int16   (*read)(struct CdxMpgParserS *MpgParser);
    cdx_int16   (*seekTo)(struct CdxMpgParserS *MpgParser, cdx_uint32  timeMs );

    cdx_uint32 nSeekTime;
}CdxMpgParserT;


#endif // _CDX_MPG_PARSER_H
