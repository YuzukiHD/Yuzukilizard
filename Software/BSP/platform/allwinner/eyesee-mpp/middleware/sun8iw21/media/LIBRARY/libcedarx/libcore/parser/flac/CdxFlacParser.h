/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxFlacParser.h
 * Description : Flac Parser.
 * History :
 *
 */

#ifndef CDX_FLAC_PARSER_H
#define CDX_FLAC_PARSER_H

#include <CdxTypes.h>

#define     READFILELEN 4096  //2048
#define  LEFTFILELEN 100
#define  INPUT_BUFFER_PADDING_SIZE 2
#define  INPUT_BUFFER_GUARD_SIZE   8

#define AV_RB16(p) ((*(p)) << 8 | (*((p) + 1)))

typedef enum {
    FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT = 0, /**< independent channels */
    FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE   = 1, /**< left+side stereo */
    FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE  = 2, /**< right+side stereo */
    FLAC__CHANNEL_ASSIGNMENT_MID_SIDE    = 3 /**< mid+side stereo */
} FLAC__ChannelAssignment;

typedef struct FlacParserImplS
{
    //audio common
    CdxParserT base;
    CdxStreamT *stream;
    cdx_int64  ulDuration;//ms
    pthread_cond_t cond;
    cdx_int64  fileSize;//total file length
    cdx_int64  file_offset; //now read location
    cdx_int64  data_offset;
    cdx_int32  mErrno; //Parser Status
    cdx_uint32 flags; //cmd
    //flac base
    cdx_int32 maxBlocksize;//reserved
    cdx_int32 minBlocksize;//reserved
    cdx_int32 maxFramesize;
    cdx_int32 minFramesize;
    cdx_int32 ulSampleRate ;
    cdx_int32 ulChannels ;
    cdx_int32 ulBitsSample ;
    cdx_int32 ulBitRate;
    cdx_int32 totalsamples;
    cdx_int32 nowsamples;
    cdx_int32 orgSr;
    cdx_int32 orgSampsPerFrm;
    cdx_int32 SampsPerFrm;
    cdx_int32 orgChans;
    cdx_int32 orgSampbit;
    cdx_int32 isFirstFrame;
    FLAC__ChannelAssignment orgChanAsign;
    cdx_uint8 *pktbuf;
    cdx_uint8 *extradata;
    cdx_int32 extradata_size;
    //meta data
    //not imply yet
}FlacParserImplS;

#endif
