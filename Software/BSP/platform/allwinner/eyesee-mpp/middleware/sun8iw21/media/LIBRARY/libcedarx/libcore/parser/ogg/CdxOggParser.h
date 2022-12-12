/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxOggParser.h
 * Description : OggParser
 * History :
 *
 */

#ifndef OGG_PARSER_H
#define OGG_PARSER_H
#include <stdint.h>
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <ctype.h>

#define OGG_SAVE_VIDEO_STREAM    (0)
#define OGG_SAVE_AUDIO_STREAM    (0)

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define Get8Bits(p)   (*p++)
#define GetLE16Bits(p)  ({\
    cdx_uint32 myTmp = Get8Bits(p);\
    myTmp | Get8Bits(p) << 8;})
#define GetLE32Bits(p)  ({\
    cdx_uint32 myTmp = GetLE16Bits(p);\
    myTmp | GetLE16Bits(p) << 16;})
#define GetLE64Bits(p) ({\
        cdx_uint64 myTmp = GetLE32Bits(p);\
        myTmp | (cdx_uint64)GetLE32Bits(p) << 32;})

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))

#define MAX_PAGE_SIZE 65307
#define DECODER_BUFFER_SIZE MAX_PAGE_SIZE
#define CDX_AV_NOPTS_VALUE          ((cdx_int64)-1)
#define OGG_NOGRANULE_VALUE (-1ull)
#define INT_MAX      0x7fffffff

#define AVERROR_EOF (-1)
#define AVERROR_INVALIDDATA (-2)
#define AVERROR_ENOMEM (-3)

#define MaxProbePacketNum (1024)
#define SIZE_OF_VIDEO_PROVB_DATA (2*1024*1024)

//#define AV_PKT_FLAG_KEY     0x0001 ///< The packet contains a keyframe
//#define AV_PKT_FLAG_CORRUPT 0x0002 ///< The packet content is corrupted

#define CDX_AV_METADATA_MATCH_CASE      1
#define CDX_AV_METADATA_IGNORE_SUFFIX   2
#define CDX_AV_METADATA_DONT_STRDUP_KEY 4
#define CDX_AV_METADATA_DONT_STRDUP_VAL 8
#define CDX_AV_METADATA_DONT_OVERWRITE 16
#define CDX_INPUT_BUFFER_PADDING_SIZE 32


#define OGG_FLAG_CONT 1
#define OGG_FLAG_BOS  2
#define OGG_FLAG_EOS  4

enum CDXAVMediaType {
    AVMEDIA_TYPE_UNKNOWN = -1,
    AVMEDIA_TYPE_VIDEO,
    AVMEDIA_TYPE_AUDIO,
    AVMEDIA_TYPE_DATA,
    AVMEDIA_TYPE_SUBTITLE,
    AVMEDIA_TYPE_ATTACHMENT,
    AVMEDIA_TYPE_NB
};

enum CodecID {
    CDX_CODEC_ID_NONE,

    // video codecs
    CDX_CODEC_ID_MPEG1VIDEO,
    CDX_CODEC_ID_MPEG2VIDEO, ///< preferred ID for MPEG-1/2 video decoding
    CDX_CODEC_ID_MPEG2VIDEO_XVMC,
    CDX_CODEC_ID_H261,
    CDX_CODEC_ID_H263,
    CDX_CODEC_ID_RV10,
    CDX_CODEC_ID_RV20,
    CDX_CODEC_ID_MJPEG,
    CDX_CODEC_ID_MJPEGB,
    CDX_CODEC_ID_LJPEG,
    CDX_CODEC_ID_SP5X,
    CDX_CODEC_ID_JPEGLS,
    CDX_CODEC_ID_MPEG4,
    CDX_CODEC_ID_RAWVIDEO,
    CDX_CODEC_ID_MSMPEG4V1,
    CDX_CODEC_ID_MSMPEG4V2,
    CDX_CODEC_ID_MSMPEG4V3,
    CDX_CODEC_ID_WMV1,
    CDX_CODEC_ID_WMV2,
    CDX_CODEC_ID_H263P,
    CDX_CODEC_ID_H263I,
    CDX_CODEC_ID_FLV1,
    CDX_CODEC_ID_SVQ1,
    CDX_CODEC_ID_SVQ3,
    CDX_CODEC_ID_DVVIDEO,
    CDX_CODEC_ID_HUFFYUV,
    CDX_CODEC_ID_CYUV,
    CDX_CODEC_ID_H264,
    CDX_CODEC_ID_INDEO3,
    CDX_CODEC_ID_VP3,
    CDX_CODEC_ID_THEORA,
    CDX_CODEC_ID_ASV1,
    CDX_CODEC_ID_ASV2,
    CDX_CODEC_ID_FFV1,
    CDX_CODEC_ID_4XM,
    CDX_CODEC_ID_VCR1,
    CDX_CODEC_ID_CLJR,
    CDX_CODEC_ID_MDEC,
    CDX_CODEC_ID_ROQ,
    CDX_CODEC_ID_INTERPLAY_VIDEO,
    CDX_CODEC_ID_XAN_WC3,
    CDX_CODEC_ID_XAN_WC4,
    CDX_CODEC_ID_RPZA,
    CDX_CODEC_ID_CINEPAK,
    CDX_CODEC_ID_WS_VQA,
    CDX_CODEC_ID_MSRLE,
    CDX_CODEC_ID_MSVIDEO1,
    CDX_CODEC_ID_IDCIN,
    CDX_CODEC_ID_8BPS,
    CDX_CODEC_ID_SMC,
    CDX_CODEC_ID_FLIC,
    CDX_CODEC_ID_TRUEMOTION1,
    CDX_CODEC_ID_VMDVIDEO,
    CDX_CODEC_ID_MSZH,
    CDX_CODEC_ID_ZLIB,
    CDX_CODEC_ID_QTRLE,
    CDX_CODEC_ID_SNOW,
    CDX_CODEC_ID_TSCC,
    CDX_CODEC_ID_ULTI,
    CDX_CODEC_ID_QDRAW,
    CDX_CODEC_ID_VIXL,
    CDX_CODEC_ID_QPEG,
    CDX_CODEC_ID_XVID,
    CDX_CODEC_ID_PNG,
    CDX_CODEC_ID_PPM,
    CDX_CODEC_ID_PBM,
    CDX_CODEC_ID_PGM,
    CDX_CODEC_ID_PGMYUV,
    CDX_CODEC_ID_PAM,
    CDX_CODEC_ID_FFVHUFF,
    CDX_CODEC_ID_RV30,
    CDX_CODEC_ID_RV40,
    CDX_CODEC_ID_VC1,
    CDX_CODEC_ID_WMV3,
    CDX_CODEC_ID_LOCO,
    CDX_CODEC_ID_WNV1,
    CDX_CODEC_ID_AASC,
    CDX_CODEC_ID_INDEO2,
    CDX_CODEC_ID_FRAPS,
    CDX_CODEC_ID_TRUEMOTION2,
    CDX_CODEC_ID_BMP,
    CDX_CODEC_ID_CSCD,
    CDX_CODEC_ID_MMVIDEO,
    CDX_CODEC_ID_ZMBV,
    CDX_CODEC_ID_AVS,
    CDX_CODEC_ID_SMACKVIDEO,
    CDX_CODEC_ID_NUV,
    CDX_CODEC_ID_KMVC,
    CDX_CODEC_ID_FLASHSV,
    CDX_CODEC_ID_CAVS,
    CDX_CODEC_ID_JPEG2000,
    CDX_CODEC_ID_VMNC,
    CDX_CODEC_ID_VP5,
    CDX_CODEC_ID_VP6,
    CDX_CODEC_ID_VP8,
    CDX_CODEC_ID_VP6F,
    CDX_CODEC_ID_TARGA,
    CDX_CODEC_ID_DSICINVIDEO,
    CDX_CODEC_ID_TIERTEXSEQVIDEO,
    CDX_CODEC_ID_TIFF,
    CDX_CODEC_ID_GIF,
    CDX_CODEC_ID_FFH264,
    CDX_CODEC_ID_DXA,
    CDX_CODEC_ID_DNXHD,
    CDX_CODEC_ID_THP,
    CDX_CODEC_ID_SGI,
    CDX_CODEC_ID_C93,
    CDX_CODEC_ID_BETHSOFTVID,
    CDX_CODEC_ID_PTX,
    CDX_CODEC_ID_TXD,
    CDX_CODEC_ID_VP6A,
    CDX_CODEC_ID_AMV,
    CDX_CODEC_ID_VB,
    CDX_CODEC_ID_PCX,
    CDX_CODEC_ID_SUNRAST,
    CDX_CODEC_ID_INDEO4,
    CDX_CODEC_ID_INDEO5,
    CDX_CODEC_ID_MIMIC,
    CDX_CODEC_ID_RL2,
    CDX_CODEC_ID_8SVX_EXP,
    CDX_CODEC_ID_8SVX_FIB,
    CDX_CODEC_ID_ESCAPE124,
    CDX_CODEC_ID_DIRAC,
    CDX_CODEC_ID_BFI,
    CDX_CODEC_ID_DIVX3,
    CDX_CODEC_ID_DIVX4,
    CDX_CODEC_ID_DIVX5,
    CDX_CODEC_ID_V_UNKNOWN,

    // various PCM "codecs"
    CDX_CODEC_ID_PCM_S16LE= 0x10000,
    CDX_CODEC_ID_PCM_S16BE,
    CDX_CODEC_ID_PCM_U16LE,
    CDX_CODEC_ID_PCM_U16BE,
    CDX_CODEC_ID_PCM_S8,
    CDX_CODEC_ID_PCM_U8,
    CDX_CODEC_ID_PCM_MULAW,
    CDX_CODEC_ID_PCM_ALAW,
    CDX_CODEC_ID_PCM_S32LE,
    CDX_CODEC_ID_PCM_S32BE,
    CDX_CODEC_ID_PCM_U32LE,
    CDX_CODEC_ID_PCM_U32BE,
    CDX_CODEC_ID_PCM_S24LE,
    CDX_CODEC_ID_PCM_S24BE,
    CDX_CODEC_ID_PCM_U24LE,
    CDX_CODEC_ID_PCM_U24BE,
    CDX_CODEC_ID_PCM_S24DAUD,
    CDX_CODEC_ID_PCM_ZORK,
    CDX_CODEC_ID_PCM_S16LE_PLANAR,
    CDX_CODEC_ID_PCM_DVD,

    // various ADPCM codecs
    CDX_CODEC_ID_ADPCM_IMA_QT= 0x11000,
    CDX_CODEC_ID_ADPCM_IMA_WAV,
    CDX_CODEC_ID_ADPCM_IMA_DK3,
    CDX_CODEC_ID_ADPCM_IMA_DK4,
    CDX_CODEC_ID_ADPCM_IMA_WS,
    CDX_CODEC_ID_ADPCM_IMA_SMJPEG,
    CDX_CODEC_ID_ADPCM_MS,
    CDX_CODEC_ID_ADPCM_4XM,
    CDX_CODEC_ID_ADPCM_XA,
    CDX_CODEC_ID_ADPCM_ADX,
    CDX_CODEC_ID_ADPCM_EA,
    CDX_CODEC_ID_ADPCM_G726,
    CDX_CODEC_ID_ADPCM_CT,
    CDX_CODEC_ID_ADPCM_SWF,
    CDX_CODEC_ID_ADPCM_YAMAHA,
    CDX_CODEC_ID_ADPCM_SBPRO_4,
    CDX_CODEC_ID_ADPCM_SBPRO_3,
    CDX_CODEC_ID_ADPCM_SBPRO_2,
    CDX_CODEC_ID_ADPCM_THP,
    CDX_CODEC_ID_ADPCM_IMA_AMV,
    CDX_CODEC_ID_ADPCM_EA_R1,
    CDX_CODEC_ID_ADPCM_EA_R3,
    CDX_CODEC_ID_ADPCM_EA_R2,
    CDX_CODEC_ID_ADPCM_IMA_EA_SEAD,
    CDX_CODEC_ID_ADPCM_IMA_EA_EACS,
    CDX_CODEC_ID_ADPCM_EA_XAS,
    CDX_CODEC_ID_ADPCM_EA_MAXIS_XA,

    // AMR
    CDX_CODEC_ID_AMR_NB= 0x12000,
    CDX_CODEC_ID_AMR_WB,

    // RealAudio codecs
    CDX_CODEC_ID_RA_144= 0x13000,
    CDX_CODEC_ID_RA_288,

    // various DPCM codecs
    CDX_CODEC_ID_ROQ_DPCM= 0x14000,
    CDX_CODEC_ID_INTERPLAY_DPCM,
    CDX_CODEC_ID_XAN_DPCM,
    CDX_CODEC_ID_SOL_DPCM,

    // audio codecs
    CDX_CODEC_ID_MP2= 0x15000,
    CDX_CODEC_ID_MP3, ///< preferred ID for decoding MPEG audio layer 1, 2 or 3
    CDX_CODEC_ID_AAC,
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
    CDX_CODEC_ID_MPEG4AAC,
#endif
    CDX_CODEC_ID_AC3,
    CDX_CODEC_ID_DTS,
    CDX_CODEC_ID_VORBIS,
    CDX_CODEC_ID_DVAUDIO,
    CDX_CODEC_ID_WMAV1,
    CDX_CODEC_ID_WMAV2,
    CDX_CODEC_ID_MACE3,
    CDX_CODEC_ID_MACE6,
    CDX_CODEC_ID_VMDAUDIO,
    CDX_CODEC_ID_SONIC,
    CDX_CODEC_ID_SONIC_LS,
    CDX_CODEC_ID_FLAC,
    CDX_CODEC_ID_MP3ADU,
    CDX_CODEC_ID_MP3ON4,
    CDX_CODEC_ID_SHORTEN,
    CDX_CODEC_ID_ALAC,
    CDX_CODEC_ID_WESTWOOD_SND1,
    CDX_CODEC_ID_GSM, ///< as in Berlin toast format
    CDX_CODEC_ID_QDM2,
    CDX_CODEC_ID_COOK,
    CDX_CODEC_ID_SIPRO,
    CDX_CODEC_ID_TRUESPEECH,
    CDX_CODEC_ID_TTA,
    CDX_CODEC_ID_SMACKAUDIO,
    CDX_CODEC_ID_QCELP,
    CDX_CODEC_ID_WAVPACK,
    CDX_CODEC_ID_DSICINAUDIO,
    CDX_CODEC_ID_IMC,
    CDX_CODEC_ID_MUSEPACK7,
    CDX_CODEC_ID_MLP,
    CDX_CODEC_ID_GSM_MS, // as found in WAV
    CDX_CODEC_ID_ATRAC3,
    CDX_CODEC_ID_VOXWARE,
    CDX_CODEC_ID_APE,
    CDX_CODEC_ID_NELLYMOSER,
    CDX_CODEC_ID_MUSEPACK8,
    CDX_CODEC_ID_SPEEX,
    CDX_CODEC_ID_WMAVOICE,
    CDX_CODEC_ID_WMAPRO,
    CDX_CODEC_ID_WMALOSSLESS,
    CDX_CODEC_ID_ATRAC3P,
    CDX_CODEC_ID_OPUS,
    CDX_CODEC_ID_A_UNKNOWN,

    // subtitle codecs
    CDX_CODEC_ID_DVD_SUBTITLE= 0x17000,
    CDX_CODEC_ID_DVB_SUBTITLE,
    CDX_CODEC_ID_TEXT,  /// raw UTF-8 text
    CDX_CODEC_ID_XSUB,
    CDX_CODEC_ID_SSA,
    CDX_CODEC_ID_MOV_TEXT,

    // other specific kind of codecs (generally used for attachments)
    CDX_CODEC_ID_TTF= 0x18000,

    CDX_CODEC_ID_MPEG2TS= 0x20000, //_FAKE_ codec to indicate a
                                   //raw MPEG-2 TS stream (only used by libavformat)
};

typedef struct AVCodecTag {
    int id;
    unsigned int tag;
} AVCodecTag;

static const AVCodecTag ff_codec_wav_tags[] = {
    { CDX_CODEC_ID_PCM_S16LE,       0x0001 },
    { CDX_CODEC_ID_PCM_U8,          0x0001 }, /* must come after s16le in this list */
    { CDX_CODEC_ID_PCM_S24LE,       0x0001 },
    { CDX_CODEC_ID_PCM_S32LE,       0x0001 },
    { CDX_CODEC_ID_ADPCM_MS,        0x0002 },
    //{ CDX_CODEC_ID_PCM_F32LE,       0x0003 },
    //{ CDX_CODEC_ID_PCM_F64LE,       0x0003 }, /* must come after f32le in this list */
    { CDX_CODEC_ID_PCM_ALAW,        0x0006 },
    { CDX_CODEC_ID_PCM_MULAW,       0x0007 },
    { CDX_CODEC_ID_WMAVOICE,        0x000A },
    { CDX_CODEC_ID_ADPCM_IMA_WAV,   0x0011 },
    { CDX_CODEC_ID_PCM_ZORK,        0x0011 }, /* must come after adpcm_ima_wav in this list */
    { CDX_CODEC_ID_ADPCM_YAMAHA,    0x0020 },
    { CDX_CODEC_ID_TRUESPEECH,      0x0022 },
    { CDX_CODEC_ID_GSM_MS,          0x0031 },
    { CDX_CODEC_ID_ADPCM_G726,      0x0045 },
    { CDX_CODEC_ID_MP2,             0x0050 },
    { CDX_CODEC_ID_MP3,             0x0055 },
    { CDX_CODEC_ID_AMR_NB,          0x0057 },
    { CDX_CODEC_ID_AMR_WB,          0x0058 },
    { CDX_CODEC_ID_ADPCM_IMA_DK4,   0x0061 },  /* rogue format number */
    { CDX_CODEC_ID_ADPCM_IMA_DK3,   0x0062 },  /* rogue format number */
    { CDX_CODEC_ID_ADPCM_IMA_WAV,   0x0069 },
    { CDX_CODEC_ID_VOXWARE,         0x0075 },
    { CDX_CODEC_ID_AAC,             0x00ff },
    //{ CDX_CODEC_ID_SIPR,            0x0130 },
    { CDX_CODEC_ID_WMAV1,           0x0160 },
    { CDX_CODEC_ID_WMAV2,           0x0161 },
    { CDX_CODEC_ID_WMAPRO,          0x0162 },
    { CDX_CODEC_ID_WMALOSSLESS,     0x0163 },
    { CDX_CODEC_ID_ADPCM_CT,        0x0200 },
    { CDX_CODEC_ID_ATRAC3,          0x0270 },
    { CDX_CODEC_ID_IMC,             0x0401 },
    { CDX_CODEC_ID_GSM_MS,          0x1500 },
    { CDX_CODEC_ID_TRUESPEECH,      0x1501 },
    { CDX_CODEC_ID_AC3,             0x2000 },
    { CDX_CODEC_ID_DTS,             0x2001 },
    { CDX_CODEC_ID_SONIC,           0x2048 },
    { CDX_CODEC_ID_SONIC_LS,        0x2048 },
    { CDX_CODEC_ID_PCM_MULAW,       0x6c75 },
    { CDX_CODEC_ID_AAC,             0x706d },
    { CDX_CODEC_ID_AAC,             0x4143 },
    { CDX_CODEC_ID_FLAC,            0xF1AC },
    { CDX_CODEC_ID_ADPCM_SWF,       ('S'<<8)+'F' },
    { CDX_CODEC_ID_VORBIS,          ('V'<<8)+'o' }, //HACK/FIXME, does vorbis
                                                    //in WAV/AVI have an (in)official id?

    /* FIXME: All of the IDs below are not 16 bit and thus illegal. */
    // for NuppelVideo (nuv.c)
    { CDX_CODEC_ID_PCM_S16LE, MKTAG('R', 'A', 'W', 'A') },
    { CDX_CODEC_ID_MP3,       MKTAG('L', 'A', 'M', 'E') },
    { CDX_CODEC_ID_MP3,       MKTAG('M', 'P', '3', ' ') },
    { CDX_CODEC_ID_NONE,      0 },
};

enum AVStreamParseType {
    AVSTREAM_PARSE_OS_NONE,
    AVSTREAM_PARSE_OS_FULL,       /**< full parsing and repack */
    AVSTREAM_PARSE_OS_HEADERS,    /**< Only parse headers, do not repack. */
    AVSTREAM_PARSE_OS_TIMESTAMPS, /**< full parsing and interpolation of timestamps for
                                       frames not starting on a packet boundary */
};

struct oggvorbis_private {
    cdx_uint32 len[3];
    unsigned char *packet[3];
};

typedef struct {
    char *value;
    char *key;
}AVMetadataTag;

typedef struct {
    int count;
    AVMetadataTag *elems;
}AVMetadata;

typedef struct AVRational {
    int num; ///< numerator
    int den; ///< denominator
} AVRational;

typedef struct AVChapter {
    int id;                 ///< unique ID to identify the chapter
    AVRational time_base;   ///< time base in which the start/end timestamps are specified
    long long  start, end;     ///< chapter start/end time in time_base units
    AVMetadata *metadata;
} AVChapter;

typedef struct AVCodecContext {
    enum CDXAVMediaType codec_type; /* see AVMEDIA_TYPE_xxx */
    enum CodecID codec_id; /* see CODEC_ID_xxx */

    unsigned char *extradata;
    int extradata_size;

    int bit_rate;
    unsigned int codec_tag;

    int width, height;
    AVRational time_base;
    cdx_int64 nFrameDuration;
    int sample_rate;
    int channels;

    AVRational sample_aspect_ratio;

    long long reordered_opaque;

}AVCodecContext;

typedef struct AVFrac {
    long long val, num, den;
} AVFrac;

typedef struct AVStream {
    int index;    /**< stream index in AVFormatContext */
    int id;       /**< format-specific stream ID */
    AVCodecContext *codec; /**< codec context */

    /* internal data used in av_find_stream_info() */
    long long first_dts;
    /** encoding: pts generation when outputting stream */
    struct AVFrac pts;

    /**
     * This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * time base should be 1/framerate and timestamp increments should be 1.
     */
    AVRational time_base;
    int pts_wrap_bits; /**< number of bits in pts (used for wrapping control) */
    /**
     * Decoding: pts of the first frame of the stream, in stream time base.
     * Only set this if you are absolutely 100% sure that the value you set
     * it to really is the pts of the first frame.
     * This may be undefined (AV_NOPTS_VALUE).
     * @note The ASF header does NOT contain a correct start_time the ASF
     * demuxer must NOT set this.
     */
    long long start_time;
    /**
     * Decoding: duration of the stream, in stream time base.
     * If a source file does not specify a duration, but does specify
     * a bitrate, this value will be estimated from bitrate and file size.
     */
    long long duration;

    /* av_read_frame() support */
    enum AVStreamParseType need_parsing;
    long long cur_dts;
    int last_IP_duration;
    long long last_IP_pts;

/*
#define MAX_REORDER_DELAY 16
    long long pts_buffer[MAX_REORDER_DELAY+1];*/

    /**
     * sample aspect ratio (0 if unknown)
     * - encoding: Set by user.
     * - decoding: Set by libavformat.
     */
    AVRational sample_aspect_ratio;

    AVMetadata *metadata;

    // Timestamp generation support:
    /**
     * Timestamp corresponding to the last dts sync point.
     *
     * Initialized when AVCodecParserContext.dts_sync_point >= 0 and
     * a DTS is received from the underlying container. Otherwise set to
     * AV_NOPTS_VALUE by default.
     */
    long long reference_dts;

} AVStream;

typedef struct CdxOggParserS CdxOggParser;

struct ogg_codec {
    const char *magic;
    uint8_t magicsize;
    const char *name;
    /**
     * Attempt to process a packet as a header
     * @return 1 if the packet was a valid header,
     *         0 if the packet was not a header (was a data packet)
     *         -1 if an error occurred or for unsupported stream
     */
    int (*header)(CdxOggParser *, int);
    int (*packet)(CdxOggParser *, int);
    /**
     * Translate a granule into a timestamp.
     * Will set dts if non-null and known.
     * @return pts
     */
    uint64_t (*gptopts)(CdxOggParser *, int, uint64_t, int64_t *dts);
    /**
     * 1 if granule is the start time of the associated packet.
     * 0 if granule is the end time of the associated packet.
     */
    int granule_is_start;
    /**
     * Number of expected headers
     */
    int nb_header;
    void (*cleanup)(CdxOggParser *s, int idx);

};

struct ogg_stream {
    uint8_t *buf;
    unsigned int psize;
    unsigned int pstart;
    unsigned int bufsize;
    unsigned int bufpos;
    unsigned int pduration;
    unsigned int pflags;
    uint32_t serial;
    int64_t granule;
    uint64_t start_granule;
    int64_t lastdts;
    int64_t lastpts;
    int64_t page_pos;   ///< file offset of the current page
    int64_t sync_pos;   ///< file offset of the first page needed to reconstruct the current packet
    int flags;
    int header;
    const struct ogg_codec *codec;
    int nsegs, segp;
    uint8_t segments[255];
    int keyframe_seek;
    int incomplete; ///< whether we're expecting a continuation in the next page
    int page_end;   ///< current packet is the last one completed in the page
    int nb_header; ///< set to the number of parsed headers
    int got_start;
    int got_data;   ///< 1 if the stream got some data (non-initial packets), 0 otherwise
    //int end_trimming; ///< set the number of packets to drop from the end
    void *private;

    int firstPacket;

    char *extradata;
    int extradata_size;

    AVStream *stream;
    int invalid;
    int streamIndex;
};

enum CdxParserStatus {
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
};
typedef struct {
    cdx_uint8 *probeBuf;
    unsigned probeBufSize;
    unsigned probeDataSize;
}ProbeSpecificDataBuf;

struct CdxOggParserS {
    CdxParserT base;
    cdx_uint32 flags;/*使能标志*/

    enum CdxParserStatus status;
    pthread_mutex_t statusLock;
    pthread_cond_t cond;
    int mErrno;
    int forceStop;

    CdxStreamT *file;
    cdx_int64 fileSize;
    int seekable;
    cdx_uint8 *buf;
    int bufSize;
    int seekStreamIndex;
    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;

    char     hasVideo;               //video flag, =0:no video bitstream; >0:video bitstream count
    char     hasAudio;               //audio flag, =0:no audio bitstream; >0:audio bitstream count
    char     hasSubTitle;            //lyric flag, =0:no lyric bitstream; >0:lyric bitstream count
    long long       pos;//

    intptr_t nb_chapters;
    AVChapter **chapters;

    int nstreams;
    struct ogg_stream *streams;

    int headers;
    int curidx;//
    cdx_uint64 total_frame;
    int64_t data_offset; /**< offset of the first packet */

    int curStream;
    int pstart;

    int page_pos;

    int io_repositioned;

    cdx_uint64 durationUs;
    //int firstPacket;
    cdx_int64 seekTimeUs;

#if OGG_SAVE_VIDEO_STREAM
    FILE* fpVideoStream[2];
#endif
#if OGG_SAVE_AUDIO_STREAM
    FILE* fpAudioStream[AUDIO_STREAM_LIMIT];
    char url[256];
#endif

    CdxPacketT *packets[MaxProbePacketNum];
    cdx_uint32 packetNum;
    cdx_uint32 packetPos;
    ProbeSpecificDataBuf vProbeBuf;
    VideoStreamInfo tempVideoInfo;
};

void aw_freep(void *arg);
void *av_mallocz(cdx_uint32 size);
enum CodecID cdx_codec_get_id(const AVCodecTag *tags, unsigned int tag);

#endif
