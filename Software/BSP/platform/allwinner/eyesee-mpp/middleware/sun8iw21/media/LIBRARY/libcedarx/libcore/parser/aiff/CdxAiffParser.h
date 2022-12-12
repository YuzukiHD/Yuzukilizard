/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxAiffParser.h
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2016/07/07  The Ghost Gate Welcome
*/
#ifndef CDX_AIFF_PARSER_H
#define CDX_AIFF_PARSER_H

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))

#define AIFF                    0
#define AIFF_C_VERSION1         0xA2805140

#define MAX_SIZE 4096

#define AV_RB32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[0] << 24) |    \
               (((const uint8_t*)(x))[1] << 16) |    \
               (((const uint8_t*)(x))[2] <<  8) |    \
               ((const uint8_t*)(x))[3])

enum CDXAVMediaType {
    AVMEDIA_TYPE_UNKNOWN = -1,
    AVMEDIA_TYPE_VIDEO,
    AVMEDIA_TYPE_AUDIO,
    AVMEDIA_TYPE_DATA,
    AVMEDIA_TYPE_SUBTITLE,
    AVMEDIA_TYPE_ATTACHMENT,
    AVMEDIA_TYPE_NB
};

typedef enum _AiffCodecID
{
    AIFF_CODEC_ID_PCM_S16BE,
    AIFF_CODEC_ID_PCM_S8,
    AIFF_CODEC_ID_PCM_U8,
    AIFF_CODEC_ID_PCM_F32BE,
    AIFF_CODEC_ID_PCM_F64BE,
    AIFF_CODEC_ID_PCM_ALAW,
    AIFF_CODEC_ID_PCM_MULAW,
    AIFF_CODEC_ID_PCM_S24BE,
    AIFF_CODEC_ID_PCM_S32BE,
    AIFF_CODEC_ID_MACE3,
    AIFF_CODEC_ID_MACE6,
    AIFF_CODEC_ID_GSM,
    AIFF_CODEC_ID_ADPCM_G722,
    AIFF_CODEC_ID_ADPCM_G726LE,
    AIFF_CODEC_ID_PCM_S16LE,
    AIFF_CODEC_ID_ADPCM_IMA_QT,
    AIFF_CODEC_ID_QDM2,
    AIFF_CODEC_ID_QCELP,
    AIFF_CODEC_ID_NONE
}AiffCodecID;

typedef struct _CDXAVCodecTag {
    AiffCodecID id;
    unsigned int tag;
} CDXAVCodecTag;

typedef struct _AIFFInputContext {
    int64_t data_end;
    int block_duration;
} AIFFInputContext;

typedef struct _AVCodecContext {
    cdx_int32   block_align;
    cdx_int32   codec_id;
    cdx_int32   codec_type;
    cdx_uint32  codec_tag;
    cdx_uint8   *extradata;
    cdx_int32   extradata_size;
    cdx_int32   bit_rate;
    cdx_int32   bits_per_coded_sample;
    cdx_int32   sample_rate; ///< samples per second
    cdx_int32   channels;    ///< number of audio channels
    cdx_int32   frame_size;
    cdx_uint64  channel_layout;
}AVCodecContext;

typedef struct AiffParserImplS
{
    //audio common
    CdxParserT  base;
    CdxStreamT  *stream;
    pthread_cond_t  cond;

    cdx_int64   ulDuration;//ms
    cdx_int64   Framelen;//ms
    cdx_int64   dFileSize;//total file length
    cdx_int64   dFileOffset; //now read location
    cdx_int64   dSndataOffset; //now read location
    cdx_int32   mErrno; //Parser Status
    cdx_uint32  nFlags; //cmd
    cdx_uint32  nFrames;
    //aiff base
    AIFFInputContext  aiff;
    AVCodecContext    avctx;
    cdx_int64   pts;
    cdx_int32   nb_frames;
}AiffParserImplS;

#endif
