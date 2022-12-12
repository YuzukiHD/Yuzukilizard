/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : AwSstrParser.h
* Description :
* History :
*   Author  : gvc-al3 <gvc-al3@allwinnertech.com>
*   Date    : sometime
*/

#ifndef _AW_SSTR_PARSER_H_
#define _AW_SSTR_PARSER_H_

#include <stdint.h>
#include <CdxParser.h>
#include <string.h>

#define SSTR_UINT32_MAX    0xffffffff

#define SAVE_VIDEO_STREAM    (0)

enum SSTR_TYPE
{
    SSTR_UNKNOWN = 0x00,
    SSTR_VIDEO   = 0x01,
    SSTR_AUDIO   = 0x02,
    SSTR_TEXT     = 0x03,
    SSTR_NAV     = 0x04,
};

enum SSTR_PROTECT_SYSTEM
{
    SSTR_PROTECT_UNKNOWN,
    SSTR_PROTECT_PLAYREADY,
};

#define SSTR_FOURCC( a, b, c, d ) \
        (((cdx_uint32)a) | (((cdx_uint32)b) << 8)| \
        (((cdx_uint32)c) << 16 ) | (((cdx_uint32)d) << 24))

typedef struct SmsArrayS
{
    int    iCount;
    void **ppElems;
} SmsArrayT;

typedef struct itemS
{
    cdx_uint64 value;
    struct itemS *next;

} ItemT;

typedef struct SmsQueueS
{
    int length;
    ItemT *first;
}SmsQueueT;

typedef struct ChunkS
{
    cdx_int64     duration;   /* chunk duration (seconds / TimeScale) */
    cdx_int64     startTime;  /* PTS (seconds / TimeScale) */
    cdx_int32     size;       /* chunk size in bytes */
    cdx_uint32    sequence;   /* unique sequence number */
    cdx_uint64    offset;     /* offset in the media */
    cdx_int32     readPos;    /* position in the chunk */
    cdx_int32     type;       /* video, audio, or subtitles */

    cdx_uint8     *data;
} ChunkT;

typedef struct QualityLevelS
{
    cdx_int32         Index;
    cdx_uint32        FourCC;
    cdx_uint32        Bitrate;
    cdx_uint32        MaxWidth;
    cdx_uint32        MaxHeight;
    cdx_uint32        SamplingRate;
    cdx_uint32        Channels;
    cdx_uint32        BitsPerSample;
    cdx_uint32        cbSize;
    cdx_uint32        AudioTag;
    cdx_uint32        nBlockAlign;
    cdx_uint32        id;
    cdx_char          *CodecPrivateData; /* hex encoded string */
    cdx_uint16        WfTag;
} QualityLevelT;

typedef struct SmsStreamS
{
    SmsArrayT     *qlevels;       /* list of available Quality Levels */
    SmsArrayT     *chunks;        /* list of chunks */
    cdx_uint32    defaultFourCC;
    cdx_uint32    vodChunksNb;   /* total num of chunks of the VOD stream */
    cdx_uint64    timeScale;
    cdx_uint32    qlevelNb;      /* number of quality levels */
    cdx_uint32    id;             /* track id, > 0 */
    cdx_char      *name;
    cdx_char      *urlTemplate;
    cdx_int32     type;
    cdx_uint32    downloadQlvl;  /* current quality level ID for Download() */
} SmsStreamT;

typedef struct AwIsmcS
{
    cdx_int32  hasVideo;
    cdx_int32  hasAudio;
    cdx_int32  hasSubtitle;
    //cdx_uint64 duration;
    cdx_uint64 timeScale;

    char         *baseUrl;    /* URL common part for chunks */
//    vlc_thread_t thread;       /* SMS chunk download thread */

    SmsArrayT  *smsStreams; /* available streams */
    SmsArrayT  *selectedSt; /* selected streams */
    SmsArrayT  *initChunks;
    cdx_int32  iTracks;     /* Total number of tracks in the Manifest */
    SmsQueueT   *bws;         /* Measured bandwidths of the N last chunks */
    cdx_uint64  vodDuration; /* total duration of the VOD media */
    cdx_int64   timePos;

    /* Download */
    struct sms_download_s
    {
        cdx_uint64     lead[3];     /* how much audio/video/text data is available
                                     (downloaded), in seconds / TimeScale */

        cdx_uint32     ckIndex[3]; /* current chunk for download */

        cdx_uint64     nextChunkOffset;
        SmsArrayT  *chunks;     /* chunks that have been downloaded */
       // vlc_mutex_t  lock_wait;   /* protect chunk download counter. */
      //  vlc_cond_t   wait;        /* some condition to wait on */
    } download;

    /* Playback */
    struct sms_playback_s
    {
        uint64_t    boffset;     /* current byte offset in media */
        uint64_t    toffset;     /* current time offset in media */
        unsigned    index;       /* current chunk for playback */
    } playback;

    /* state */
    cdx_bool        bCache;     /* can cache files */
    cdx_bool        bLive;      /* live stream? or vod? */
    cdx_bool        bError;     /* parsing error */
    cdx_bool        bClose;     /* set by Close() */
    cdx_bool        bTseek;     /* time seeking */
    cdx_int32       protectSystem; /* protect system: defined in enum SSTR_PROTECT_SYSTEM */
}AwIsmcT;

typedef struct SstrMediaS
{
    CdxDataSourceT *datasource;
    cdx_int32 flag;
    CdxParserT *parser;
    CdxStreamT *stream;
    cdx_int32  eos;
    cdx_uint64 baseTime; //* for seek

}SstrMediaT;

typedef struct AwSstrParserImplS
{
    CdxParserT      base;
    CdxStreamT      *stream;

    cdx_int32       exitFlag;
    cdx_int32       mErrno;
    cdx_int32       mStatus;
    pthread_mutex_t statusMutex;
    pthread_cond_t  cond;

    char            *ismcUrl;
    AwIsmcT         *ismc;
    cdx_char        *pIsmcBuffer; //* store ismc data.
    cdx_int32       ismcSize;     //* size of manifest response.
    pthread_t       nOpenPid;

    cdx_int32       prefetchType;
    cdx_int64       streamPts[3]; //* in unit of us.
    SstrMediaT      mediaStream[3];
    CdxPacketT      pkt;
    cdx_uint8       *codecSpecDataV; //* from xml
    cdx_int32       codecSpecDataVLen;
    cdx_uint8       *codecSpecDataA;
    cdx_int32       bAddExtradata;
    CdxMediaInfoT   mediaInfo;
    cdx_uint8       pExtraData[1024]; //* constructed extradata
    cdx_int32       nExtraDataLen;

    cdx_int32       infoVersionV; //* if switch bandwidth, infoVersion increases 1.
    cdx_int32       infoVersionA;
    cdx_int32       infoVersionS;
    struct VideoInfo vInfo;
    struct AudioInfo aInfo;
    struct SubtitleInfo sInfo;

    ParserCallback callback;
    void *pUserData;

#if SAVE_VIDEO_STREAM
    FILE* fpVideoStream;
#endif
}AwSstrParserImplT;

SmsQueueT *SmsQueueInit(const int length);

#endif
