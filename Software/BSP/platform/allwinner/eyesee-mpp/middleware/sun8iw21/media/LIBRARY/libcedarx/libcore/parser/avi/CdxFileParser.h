/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxFileParser.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _FILE_PARSER_H_
#define _FILE_PARSER_H_

#include<CdxTypes.h>

#define FRAME_RATE_BASE 1000

typedef struct __AVI_CHUNK_INFO
{
    cdx_uint32       chk_type;       //chunk type, audio type, video type, lyric type
    cdx_int32       t_size;         //total size of the chunk, based on byte
    cdx_int32       r_size;         //data size has been read out, based on byte
} __avi_chunck_info_t;


typedef struct CdxAviParserImplS  //AVI_FILE_IN
{
    CdxParserT base;
    CdxStreamT *stream;
    CdxPacketT pkt;
    
    void    *privData;  //store private data , AVI_FILE_IN
    cdx_int32   exitFlag;//for forcestop
    cdx_int32   mErrno;
    cdx_int32   mStatus;
    cdx_int32   flags;

    //cdx_uint8    status;     //file parser status
    cdx_int8    hasIndex;     //index table flag(we create it from index table of AVI file)
                              //=0:no index table; !=0:has index table 0 1 FFRRKeyframeTable?????
                              // FFRRKeyframeTable

    cdx_int8    hasVideo;            //video flag, =0:no video bitstream; >0:video bitstream count
    cdx_int8    hasAudio;            //audio flag, =0:no audio bitstream; >0:audio bitstream count
    cdx_int8    hasSubTitle;         //lyric flag, =0:no lyric bitstream; >0:lyric bitstream count

    cdx_uint8   videoStreamIndex;       //video bitstream index video stream
    cdx_uint8   audioStreamIndex;       //audio bitstream index audio stream
    cdx_uint8   subTitleStreamIndex;    //lyric stream index,   AVI
    cdx_uint8   videoStreamIndexArray[2];

    cdx_uint8    curAudStreamNum;        //AudioStreamIndexArray[] AuddioStreamIndex
    cdx_uint8    nextCurAudStreamNum;    //CurAudStreamNum, GetNextChunkInfo()SetProcMode()
    cdx_uint8    audioStreamIndexArray[MAX_AUDIO_STREAM];// AudioStreamIndex

    cdx_uint8    subtitleStreamIndexArray[MAX_SUBTITLE_STREAM];

    cdx_uint32   totalFrames;          //total frame count
    cdx_uint32   pictureNum;           //picture number
    cdx_int32    unkownChunkNum;       //unknown chunk type

    cdx_uint32   nPreFRSpeed;      //previous ff/rr speed, for dynamic adjust
    cdx_uint32   nFRSpeed;         //fast forward and fast backward speed, multiple of normal speed
    cdx_uint32   nFRPicShowTime;   //picture show time under fast forward and backward
    cdx_uint32   nFRPicCnt;        //picture count under ff/rr, for check if need delay

    cdx_uint32   nVidPtsOffset;       //video time offset
    cdx_uint32   nAudPtsOffsetArray[MAX_AUDIO_STREAM]; //audio time offset, unit:ms
    cdx_uint32   nSubPtsOffset;       //subtitle time offset

    cdx_int8    hasSyncVideo;         //flag, mark that if has sync video
    cdx_int8    hasSyncAudio;         //flag, mark that if has sync audio
    
    cdx_int8    bDiscardAud;    //1:discard, 0:transport
    cdx_int8    bAbortFlag;
    pthread_t   nOpenPid;
    
    pthread_mutex_t lock;
    pthread_cond_t  cond;

    AudioStreamInfo             aFormatArray[MAX_AUDIO_STREAM];
    AviVideoStreamInfo          aviFormat;
    SubtitleStreamInfo          tFormatArray[MAX_AUDIO_STREAM];

    VideoStreamInfo             vTempFormat;
}CdxAviParserImplT;

extern cdx_int16 AviCtrl(CdxAviParserImplT *p, cdx_uint32 uCmd, cdx_uint32 uParam);
extern cdx_int16 AviRead(CdxAviParserImplT *p);
extern cdx_int32 CalcAviChunkAudioFrameNum(AudioStreamInfoT *pAudStrmInfo, cdx_int32 nChunkSize);
extern cdx_int32 ConvertAudStreamidToArrayNum(CdxAviParserImplT *p, cdx_int32 streamId);
extern cdx_int32 ConvertSubStreamidToArrayNum(CdxAviParserImplT *p, cdx_int32 streamId);
extern cdx_int16 AVIBuildIdx(CdxAviParserImplT *p);
extern cdx_int16 AviOpen(CdxAviParserImplT *impl);
extern CdxAviParserImplT *AviInit(cdx_int32 *ret);
extern cdx_int32 ReconfigAviReadContext(CdxAviParserImplT *p, cdx_uint32 vidTime,
                    cdx_int32 searchDirection, cdx_int32 searchMode);
extern cdx_int16 AviClose(CdxAviParserImplT *p);
extern cdx_int16 AviExit(CdxAviParserImplT *p);

//extern cdx_int32 AviInitial(struct FILE_PARSER * p)(struct FILE_PARSER *p);
//extern cdx_int32 AVI_deinitial(struct FILE_PARSER *p);
//extern cdx_int32 AVI_DEPACK_initial(FILE_DEPACK *pAviDepack);
//extern CDX_S16 AVI_open(struct FILE_PARSER *p, CedarXDataSourceDesc *datasrc_desc);
//extern CDX_S16 AVI_close(struct FILE_PARSER *p);
//extern CDX_S16 AVI_read(struct FILE_PARSER *p);
//extern CDX_S16 AVI_IoCtrl(AVI_DEPACK *pAviDepack,cdx_uint32 uCmd, cdx_uint32 uParam);
//extern CDX_S16 AVI_build_idx(struct FILE_PARSER *p, CedarXDataSourceDesc *datasrc_desc);

#endif  //_FILE_PARSER_H_

