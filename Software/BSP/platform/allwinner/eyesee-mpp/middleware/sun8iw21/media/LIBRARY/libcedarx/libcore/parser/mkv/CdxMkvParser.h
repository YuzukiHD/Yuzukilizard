/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMkvParser.h
 * Description : MkvParser
 * History :
 *
 */

#ifndef CDX__mkv_parser_H
#define CDX__mkv_parser_H

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <errno.h>
#include <CdxAtomic.h>

#define MAX_AUDIO_STREAM_NUM    (16)
#define MAX_VIDEO_STREAM_NUM    (1)
#define MAX_SUBTITLE_STREAM_NUM (64)//yf
#define MAX_SUBTITLE_LANG_SIZE     (64)

#define OGG_HEADER (0)

// EBML version supported
#define EBML_VERSION 1

// top-level master-IDs
#define CDX_EBML_ID_HEADER             0x1A45DFA3

// IDs in the HEADER master
#define CDX_EBML_ID_EBMLVERSION        0x4286
#define CDX_EBML_ID_EBMLREADVERSION    0x42F7
#define CDX_EBML_ID_EBMLMAXIDLENGTH    0x42F2
#define CDX_EBML_ID_EBMLMAXSIZELENGTH  0x42F3
#define CDX_EBML_ID_DOCTYPE            0x4282
#define CDX_EBML_ID_DOCTYPEVERSION     0x4287
#define CDX_EBML_ID_DOCTYPEREADVERSION 0x4285

// general EBML types
#define CDX_EBML_ID_VOID               0xEC

//  Matroska element IDs. max. 32-bit.

// toplevel segment
#define CDX_MATROSKA_ID_SEGMENT    0x18538067

// matroska top-level master IDs
#define CDX_MATROSKA_ID_INFO       0x1549A966
#define CDX_MATROSKA_ID_TRACKS     0x1654AE6B
#define CDX_MATROSKA_ID_CUES       0x1C53BB6B
#define CDX_MATROSKA_ID_TAGS       0x1254C367
#define CDX_MATROSKA_ID_SEEKHEAD   0x114D9B74
#define CDX_MATROSKA_ID_ATTACHMENTS 0x1941A469
#define CDX_MATROSKA_ID_CLUSTER    0x1F43B675
#define CDX_MATROSKA_ID_CHAPTERS   0x1043A770

// IDs in the info master
#define CDX_MATROSKA_ID_TIMECODESCALE 0x2AD7B1
#define CDX_MATROSKA_ID_DURATION   0x4489
#define CDX_MATROSKA_ID_TITLE      0x7BA9
#define CDX_MATROSKA_ID_WRITINGAPP 0x5741
#define CDX_MATROSKA_ID_MUXINGAPP  0x4D80
#define CDX_MATROSKA_ID_DATEUTC    0x4461
#define CDX_MATROSKA_ID_SEGMENTUID 0x73A4

// ID in the tracks master
#define CDX_MATROSKA_ID_TRACKENTRY 0xAE

// IDs in the trackentry master
#define CDX_MATROSKA_ID_TRACKNUMBER 0xD7
#define CDX_MATROSKA_ID_TRACKUID   0x73C5
#define CDX_MATROSKA_ID_TRACKTYPE  0x83
#define CDX_MATROSKA_ID_TRACKAUDIO 0xE1
#define CDX_MATROSKA_ID_TRACKVIDEO 0xE0
#define CDX_MATROSKA_ID_CODECID    0x86
#define CDX_MATROSKA_ID_CODECPRIVATE 0x63A2
#define CDX_MATROSKA_ID_CODECNAME  0x258688
#define CDX_MATROSKA_ID_CODECINFOURL 0x3B4040
#define CDX_MATROSKA_ID_CODECDOWNLOADURL 0x26B240
#define CDX_MATROSKA_ID_CODECDECODEALL 0xAA
#define CDX_MATROSKA_ID_TRACKNAME  0x536E
#define CDX_MATROSKA_ID_TRACKLANGUAGE 0x22B59C
#define CDX_MATROSKA_ID_TRACKFLAGENABLED 0xB9
#define CDX_MATROSKA_ID_TRACKFLAGDEFAULT 0x88
#define CDX_MATROSKA_ID_TRACKFLAGFORCED 0x55AA
#define CDX_MATROSKA_ID_TRACKFLAGLACING 0x9C
#define CDX_MATROSKA_ID_TRACKMINCACHE 0x6DE7
#define CDX_MATROSKA_ID_TRACKMAXCACHE 0x6DF8
#define CDX_MATROSKA_ID_TRACKDEFAULTDURATION 0x23E383
#define CDX_MATROSKA_ID_TRACKCONTENTENCODINGS 0x6D80
#define CDX_MATROSKA_ID_TRACKCONTENTENCODING 0x6240
#define CDX_MATROSKA_ID_TRACKTIMECODESCALE 0x23314F

// IDs in the trackvideo master
#define CDX_MATROSKA_ID_VIDEOFRAMERATE 0x2383E3
#define CDX_MATROSKA_ID_VIDEODISPLAYWIDTH 0x54B0
#define CDX_MATROSKA_ID_VIDEODISPLAYHEIGHT 0x54BA
#define CDX_MATROSKA_ID_VIDEOPIXELWIDTH 0xB0
#define CDX_MATROSKA_ID_VIDEOPIXELHEIGHT 0xBA
#define CDX_MATROSKA_ID_VIDEOFLAGINTERLACED 0x9A
#define CDX_MATROSKA_ID_VIDEOSTEREOMODE 0x53B9
#define CDX_MATROSKA_ID_VIDEOASPECTRATIO 0x54B3
#define CDX_MATROSKA_ID_VIDEOCOLORSPACE 0x2EB524

// IDs in the trackaudio master
#define CDX_MATROSKA_ID_AUDIOSAMPLINGFREQ 0xB5
#define CDX_MATROSKA_ID_AUDIOOUTSAMPLINGFREQ 0x78B5

#define CDX_MATROSKA_ID_AUDIOBITDEPTH 0x6264
#define CDX_MATROSKA_ID_AUDIOCHANNELS 0x9F

// IDs in the content encoding master
#define CDX_MATROSKA_ID_ENCODINGSCOPE 0x5032
#define CDX_MATROSKA_ID_ENCODINGTYPE 0x5033
#define CDX_MATROSKA_ID_ENCODINGCOMPRESSION 0x5034
#define CDX_MATROSKA_ID_ENCODINGCOMPALGO 0x4254
#define CDX_MATROSKA_ID_ENCODINGCOMPSETTINGS 0x4255

// ID in the cues master
#define CDX_MATROSKA_ID_POINTENTRY 0xBB

// IDs in the pointentry master
#define CDX_MATROSKA_ID_CUETIME    0xB3
#define CDX_MATROSKA_ID_CUETRACKPOSITION 0xB7

// IDs in the cuetrackposition master
#define CDX_MATROSKA_ID_CUETRACK   0xF7
#define CDX_MATROSKA_ID_CUECLUSTERPOSITION 0xF1

// IDs in the tags master

// IDs in the seekhead master
#define CDX_MATROSKA_ID_SEEKENTRY  0x4DBB

// IDs in the seekpoint master
#define CDX_MATROSKA_ID_SEEKID     0x53AB
#define CDX_MATROSKA_ID_SEEKPOSITION 0x53AC

// IDs in the cluster master
#define CDX_MATROSKA_ID_CLUSTERTIMECODE 0xE7
#define CDX_MATROSKA_ID_BLOCKGROUP 0xA0
#define CDX_MATROSKA_ID_SIMPLEBLOCK 0xA3

// IDs in the blockgroup master
#define CDX_MATROSKA_ID_BLOCK      0xA1
#define CDX_MATROSKA_ID_BLOCKDURATION 0x9B
#define CDX_MATROSKA_ID_BLOCKREFERENCE 0xFB

// IDs in the attachments master
#define CDX_MATROSKA_ID_ATTACHEDFILE        0x61A7
#define CDX_MATROSKA_ID_FILENAME            0x466E
#define CDX_MATROSKA_ID_FILEMIMETYPE        0x4660
#define CDX_MATROSKA_ID_FILEDATA            0x465C
#define CDX_MATROSKA_ID_FILEUID             0x46AE

// IDs in the chapters master
#define CDX_MATROSKA_ID_EDITIONENTRY        0x45B9
#define CDX_MATROSKA_ID_CHAPTERATOM         0xB6
#define CDX_MATROSKA_ID_CHAPTERTIMESTART    0x91
#define CDX_MATROSKA_ID_CHAPTERTIMEEND      0x92
#define CDX_MATROSKA_ID_CHAPTERDISPLAY      0x80
#define CDX_MATROSKA_ID_CHAPSTRING          0x85
#define CDX_MATROSKA_ID_EDITIONUID          0x45BC
#define CDX_MATROSKA_ID_EDITIONFLAGHIDDEN   0x45BD
#define CDX_MATROSKA_ID_EDITIONFLAGDEFAULT  0x45DB
#define CDX_MATROSKA_ID_CHAPTERUID          0x73C4
#define CDX_MATROSKA_ID_CHAPTERFLAGHIDDEN   0x98

#define AV_NOPTS_VALUE          ((cdx_uint64)1)<<63 //INT64_C(0x8000000000000000)
#define AV_TIME_BASE            1000000

#define PKT_FLAG_KEY   0x0001

#define AV_DISPOSITION_DEFAULT   0x0001
#define AV_DISPOSITION_DUB       0x0002
#define AV_DISPOSITION_ORIGINAL  0x0004
#define AV_DISPOSITION_COMMENT   0x0008
#define AV_DISPOSITION_LYRICS    0x0010
#define AV_DISPOSITION_KARAOKE   0x0020

/**
 * Identifies the syntax and semantics of the bitstream.
 * The principle is roughly:
 * Two decoders with the same ID can decode the same streams.
 * Two encoders with the same ID can encode compatible streams.
 * There may be slight deviations from the principle due to implementation
 * details.
 *
 * If you add a codec ID to this list, add it so that
 * 1. no value of a existing codec ID changes (that would break ABI),
 * 2. it is as close as possible to similar codecs.
 */
enum CodecID {
    CODEC_ID_NONE,

    // video codecs
    CODEC_ID_MPEG1VIDEO,
    CODEC_ID_MPEG2VIDEO, ///< preferred ID for MPEG-1/2 video decoding
    CODEC_ID_MPEG2VIDEO_XVMC,
    CODEC_ID_H261,
    CODEC_ID_H263,
    CODEC_ID_RV10,
    CODEC_ID_RV20,
    CODEC_ID_MJPEG,
    CODEC_ID_MJPEGB,
    CODEC_ID_LJPEG,
    CODEC_ID_SP5X,
    CODEC_ID_JPEGLS,
    CODEC_ID_MPEG4,
    CODEC_ID_RAWVIDEO,
    CODEC_ID_MSMPEG4V1,
    CODEC_ID_MSMPEG4V2,
    CODEC_ID_MSMPEG4V3,
    CODEC_ID_WMV1,
    CODEC_ID_WMV2,
    CODEC_ID_H263P,
    CODEC_ID_H263I,
    CODEC_ID_FLV1,
    CODEC_ID_SVQ1,
    CODEC_ID_SVQ3,
    CODEC_ID_DVVIDEO,
    CODEC_ID_HUFFYUV,
    CODEC_ID_CYUV,
    CODEC_ID_H264,
    CODEC_ID_H265,
    CODEC_ID_INDEO3,
    CODEC_ID_VP3,
    CODEC_ID_THEORA,
    CODEC_ID_ASV1,
    CODEC_ID_ASV2,
    CODEC_ID_FFV1,
    CODEC_ID_4XM,
    CODEC_ID_VCR1,
    CODEC_ID_CLJR,
    CODEC_ID_MDEC,
    CODEC_ID_ROQ,
    CODEC_ID_INTERPLAY_VIDEO,
    CODEC_ID_XAN_WC3,
    CODEC_ID_XAN_WC4,
    CODEC_ID_RPZA,
    CODEC_ID_CINEPAK,
    CODEC_ID_WS_VQA,
    CODEC_ID_MSRLE,
    CODEC_ID_MSVIDEO1,
    CODEC_ID_IDCIN,
    CODEC_ID_8BPS,
    CODEC_ID_SMC,
    CODEC_ID_FLIC,
    CODEC_ID_TRUEMOTION1,
    CODEC_ID_VMDVIDEO,
    CODEC_ID_MSZH,
    CODEC_ID_ZLIB,
    CODEC_ID_QTRLE,
    CODEC_ID_SNOW,
    CODEC_ID_TSCC,
    CODEC_ID_ULTI,
    CODEC_ID_QDRAW,
    CODEC_ID_VIXL,
    CODEC_ID_QPEG,
    CODEC_ID_XVID,
    CODEC_ID_PNG,
    CODEC_ID_PPM,
    CODEC_ID_PBM,
    CODEC_ID_PGM,
    CODEC_ID_PGMYUV,
    CODEC_ID_PAM,
    CODEC_ID_FFVHUFF,
    CODEC_ID_RV30,
    CODEC_ID_RV40,
    CODEC_ID_VC1,
    CODEC_ID_WMV3,
    CODEC_ID_LOCO,
    CODEC_ID_WNV1,
    CODEC_ID_AASC,
    CODEC_ID_INDEO2,
    CODEC_ID_FRAPS,
    CODEC_ID_TRUEMOTION2,
    CODEC_ID_BMP,
    CODEC_ID_CSCD,
    CODEC_ID_MMVIDEO,
    CODEC_ID_ZMBV,
    CODEC_ID_AVS,
    CODEC_ID_SMACKVIDEO,
    CODEC_ID_NUV,
    CODEC_ID_KMVC,
    CODEC_ID_FLASHSV,
    CODEC_ID_CAVS,
    CODEC_ID_JPEG2000,
    CODEC_ID_VMNC,
    CODEC_ID_VP5,
    CODEC_ID_VP6,
    CODEC_ID_VP8,
    CODEC_ID_VP9,
    CODEC_ID_VP6F,
    CODEC_ID_TARGA,
    CODEC_ID_DSICINVIDEO,
    CODEC_ID_TIERTEXSEQVIDEO,
    CODEC_ID_TIFF,
    CODEC_ID_GIF,
    CODEC_ID_FFH264,
    CODEC_ID_DXA,
    CODEC_ID_DNXHD,
    CODEC_ID_THP,
    CODEC_ID_SGI,
    CODEC_ID_C93,
    CODEC_ID_BETHSOFTVID,
    CODEC_ID_PTX,
    CODEC_ID_TXD,
    CODEC_ID_VP6A,
    CODEC_ID_AMV,
    CODEC_ID_VB,
    CODEC_ID_PCX,
    CODEC_ID_SUNRAST,
    CODEC_ID_INDEO4,
    CODEC_ID_INDEO5,
    CODEC_ID_MIMIC,
    CODEC_ID_RL2,
    CODEC_ID_8SVX_EXP,
    CODEC_ID_8SVX_FIB,
    CODEC_ID_ESCAPE124,
    CODEC_ID_DIRAC,
    CODEC_ID_BFI,
    CODEC_ID_DIVX3,
    CODEC_ID_DIVX4,
    CODEC_ID_DIVX5,
    CODEC_ID_V_UNKNOWN,

    // various PCM "codecs"
    CODEC_ID_PCM_S16LE= 0x10000,
    CODEC_ID_PCM_S16BE,
    CODEC_ID_PCM_U16LE,
    CODEC_ID_PCM_U16BE,
    CODEC_ID_PCM_S8,
    CODEC_ID_PCM_U8,
    CODEC_ID_PCM_MULAW,
    CODEC_ID_PCM_ALAW,
    CODEC_ID_PCM_S32LE,
    CODEC_ID_PCM_S32BE,
    CODEC_ID_PCM_U32LE,
    CODEC_ID_PCM_U32BE,
    CODEC_ID_PCM_S24LE,
    CODEC_ID_PCM_S24BE,
    CODEC_ID_PCM_U24LE,
    CODEC_ID_PCM_U24BE,
    CODEC_ID_PCM_S24DAUD,
    CODEC_ID_PCM_ZORK,
    CODEC_ID_PCM_S16LE_PLANAR,
    CODEC_ID_PCM_DVD,

    // various ADPCM codecs
    CODEC_ID_ADPCM_IMA_QT= 0x11000,
    CODEC_ID_ADPCM_IMA_WAV,
    CODEC_ID_ADPCM_IMA_DK3,
    CODEC_ID_ADPCM_IMA_DK4,
    CODEC_ID_ADPCM_IMA_WS,
    CODEC_ID_ADPCM_IMA_SMJPEG,
    CODEC_ID_ADPCM_MS,
    CODEC_ID_ADPCM_4XM,
    CODEC_ID_ADPCM_XA,
    CODEC_ID_ADPCM_ADX,
    CODEC_ID_ADPCM_EA,
    CODEC_ID_ADPCM_G726,
    CODEC_ID_ADPCM_CT,
    CODEC_ID_ADPCM_SWF,
    CODEC_ID_ADPCM_YAMAHA,
    CODEC_ID_ADPCM_SBPRO_4,
    CODEC_ID_ADPCM_SBPRO_3,
    CODEC_ID_ADPCM_SBPRO_2,
    CODEC_ID_ADPCM_THP,
    CODEC_ID_ADPCM_IMA_AMV,
    CODEC_ID_ADPCM_EA_R1,
    CODEC_ID_ADPCM_EA_R3,
    CODEC_ID_ADPCM_EA_R2,
    CODEC_ID_ADPCM_IMA_EA_SEAD,
    CODEC_ID_ADPCM_IMA_EA_EACS,
    CODEC_ID_ADPCM_EA_XAS,
    CODEC_ID_ADPCM_EA_MAXIS_XA,

    // AMR
    CODEC_ID_AMR_NB= 0x12000,
    CODEC_ID_AMR_WB,

    // RealAudio codecs
    CODEC_ID_RA_144= 0x13000,
    CODEC_ID_RA_288,

    // various DPCM codecs
    CODEC_ID_ROQ_DPCM= 0x14000,
    CODEC_ID_INTERPLAY_DPCM,
    CODEC_ID_XAN_DPCM,
    CODEC_ID_SOL_DPCM,

    // audio codecs
    CODEC_ID_MP2= 0x15000,
    CODEC_ID_MP3, ///< preferred ID for decoding MPEG audio layer 1, 2 or 3
    CODEC_ID_AAC,
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
    CODEC_ID_MPEG4AAC,
#endif
    CODEC_ID_AC3,
    CODEC_ID_DTS,
    CODEC_ID_VORBIS,
    CODEC_ID_DVAUDIO,
    CODEC_ID_WMAV1,
    CODEC_ID_WMAV2,
    CODEC_ID_MACE3,
    CODEC_ID_MACE6,
    CODEC_ID_VMDAUDIO,
    CODEC_ID_SONIC,
    CODEC_ID_SONIC_LS,
    CODEC_ID_FLAC,
    CODEC_ID_MP3ADU,
    CODEC_ID_MP3ON4,
    CODEC_ID_SHORTEN,
    CODEC_ID_ALAC,
    CODEC_ID_WESTWOOD_SND1,
    CODEC_ID_GSM, ///< as in Berlin toast format
    CODEC_ID_QDM2,
    CODEC_ID_COOK,
    CODEC_ID_SIPRO,
    CODEC_ID_TRUESPEECH,
    CODEC_ID_TTA,
    CODEC_ID_SMACKAUDIO,
    CODEC_ID_QCELP,
    CODEC_ID_WAVPACK,
    CODEC_ID_DSICINAUDIO,
    CODEC_ID_IMC,
    CODEC_ID_MUSEPACK7,
    CODEC_ID_MLP,
    CODEC_ID_GSM_MS, // as found in WAV
    CODEC_ID_ATRAC3,
    CODEC_ID_VOXWARE,
    CODEC_ID_APE,
    CODEC_ID_NELLYMOSER,
    CODEC_ID_MUSEPACK8,
    CODEC_ID_SPEEX,
    CODEC_ID_WMAVOICE,
    CODEC_ID_WMAPRO,
    CODEC_ID_WMALOSSLESS,
    CODEC_ID_ATRAC3P,
    CODEC_ID_OPUS,
    CODEC_ID_A_UNKNOWN,

    // subtitle codecs
    CODEC_ID_DVD_SUBTITLE= 0x17000,
    CODEC_ID_DVB_SUBTITLE,
    CODEC_ID_TEXT,  /// raw UTF-8 text
    CODEC_ID_XSUB,
    CODEC_ID_SSA,
    CODEC_ID_MOV_TEXT,
    CODEC_ID_PGS,

    // other specific kind of codecs (generally used for attachments)
    CODEC_ID_TTF= 0x18000,

    CODEC_ID_MPEG2TS= 0x20000, //_FAKE_ codec to indicate a raw MPEG-2 TS stream
                               //(only used by libavformat)
};

enum CodecType {
    CODEC_TYPE_UNKNOWN = -1,
    CODEC_TYPE_VIDEO,
    CODEC_TYPE_AUDIO,
    CODEC_TYPE_DATA,
    CODEC_TYPE_SUBTITLE,
    CODEC_TYPE_ATTACHMENT,
    CODEC_TYPE_NB
};

typedef enum {
  MATROSKA_TRACK_TYPE_NONE     = 0x0,
  MATROSKA_TRACK_TYPE_VIDEO    = 0x1,
  MATROSKA_TRACK_TYPE_AUDIO    = 0x2,
  MATROSKA_TRACK_TYPE_COMPLEX  = 0x3,
  MATROSKA_TRACK_TYPE_LOGO     = 0x10,
  MATROSKA_TRACK_TYPE_SUBTITLE = 0x11,
  MATROSKA_TRACK_TYPE_CONTROL  = 0x20,
} MatroskaTrackType;

typedef enum {
  MATROSKA_TRACK_ENCODING_COMP_ZLIB        = 0,
  MATROSKA_TRACK_ENCODING_COMP_BZLIB       = 1,
  MATROSKA_TRACK_ENCODING_COMP_LZO         = 2,
  MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP = 3,
} MatroskaTrackEncodingCompAlgo;

typedef enum {
  MATROSKA_TRACK_ENABLED = (1<<0),
  MATROSKA_TRACK_DEFAULT = (1<<1),
  MATROSKA_TRACK_LACING  = (1<<2),
  MATROSKA_TRACK_SHIFT   = (1<<16)
} MatroskaTrackFlags;

typedef enum {
  MATROSKA_VIDEOTRACK_INTERLACED = (MATROSKA_TRACK_SHIFT<<0)
} MatroskaVideoTrackFlags;

//#define EINVAL  1
//#define EIO     2
//#define EDOM    3
//#define ENOMEM  5
//#define EILSEQ  6
//#define ENOSYS  7
//#define ENOENT  8
#define AVERROR_EOS 1

// Returns a negative error code from a POSIX error code, to return from library functions.
#define AVERROR(e) (-(e))

// Returns a POSIX error code from a library function error return value.
#define AVUNERROR(e) (-(e))
#define AVERROR_UNKNOWN     AVERROR(EINVAL)  // unknown error
#define AVERROR_IO          AVERROR(EIO)     // I/O error
#define AVERROR_NUMEXPECTED AVERROR(EDOM)    // Number syntax expected in filename.
#define AVERROR_INVALIDDATA AVERROR(EINVAL)  // invalid data found
#define AVERROR_NOMEM       AVERROR(ENOMEM)  // not enough memory
#define AVERROR_NOFMT       AVERROR(EILSEQ)  // unknown format
#define AVERROR_NOTSUPP     AVERROR(ENOSYS)  // Operation not supported.
#define AVERROR_NOENT       AVERROR(ENOENT)  // No such file or directory.

typedef struct AVCodecTag {
    cdx_int32 id;
    unsigned int tag;
} AVCodecTag;

#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#define MKBETAG(a,b,c,d) (d | (c << 8) | (b << 16) | (a << 24))

// Matroska Codec IDs. Strings.

typedef struct CodecTags{
    char str[20];
    enum CodecID id;
}CodecTags;

// max. depth in the EBML tree structure
#define EBML_MAX_DEPTH 16

typedef struct AVIndexEntry {
    cdx_int64        pos;
    cdx_int64        timestamp;
} AVIndexEntry;

typedef struct AVStream {
    cdx_int32            index;     // stream index in AVFormatContext
    cdx_int32            id;        // format specific stream id

    // the average bitrate. 0 or some bitrate if this info is available in the stream.
    cdx_int32            bit_rate;

    /* some codecs need / can use extradata like Huffman tables.
     * mjpeg: Huffman tables
     * rv10: additional flags
     * mpeg4: global headers (they can be in the bitstream or here)
     * The allocated memory should be FF_INPUT_BUFFER_PADDING_SIZE bytes larger
     * than extradata_size to avoid prolems if it is read with the bitstream reader.
     * The bytewise contents of extradata must not depend on the architecture or CPU endianness.
     * - decoding: Set/allocated/freed by user.*/
    char            *extradata;
    cdx_int32            extradata_size;

     /* This is the fundamental unit of time (in seconds) in terms
      * of which frame timestamps are represented. For fixed-fps content,
      * timebase should be 1/framerate and timestamp increments should be
      * identically 1.*/
    cdx_uint64            default_duration;

    // Note: For compatibility it is possible to set this
    //       instead of coded_width/height before decoding.
    cdx_int32            width, height;

    // audio only
    cdx_int32            sample_rate; // samples per second
    cdx_int32            channels;    // number of audio channels

    // The following data should not be initialized.

    enum CodecType    codec_type;
    enum CodecID    codec_id;

    //number of bytes per packet if constant and known or 0 Used by some WAV based audio codecs.
    cdx_int32            block_align;

    unsigned int            sample_aspect_ratio;    //sample aspect ratio (0 if unknown)

    // FF_PROFILE_UNKNOWN -99; FF_PROFILE_AAC_MAIN 0;
    // FF_PROFILE_AAC_LOW  1; FF_PROFILE_AAC_SSR  2; FF_PROFILE_AAC_LTP  3
    cdx_int32            profile;

    cdx_int32            level;   // FF_LEVEL_UNKNOWN -99

    unsigned int            time_scale;

    cdx_int64            pts;
    cdx_int64           last_pts;

    // ISO 639 3-letter language code (empty string if undefined)
    char            language[MAX_SUBTITLE_LANG_SIZE];

    AVIndexEntry    *index_entries; // only used if the format does not support seeking natively
    cdx_int32            nb_index_entries;

    cdx_int32            disposition;              // AV_DISPOSITION_* bitfield
} AVStream;

#define MAX_STREAMS 64

typedef struct Track {
    MatroskaTrackType    type;

    // for seamless audio/subtitle switch, the stream number of all the audio/subtitle streams
    int                 nStreamIdx;      // it is not same with stream_index

    cdx_int32            encoding_scope;

    // Unique track number and track ID.
    // stream_index is the index that the calling app uses for this track.
    unsigned int        num;

    // number of all the streams( including video, audio,subtitle)
    cdx_int32            stream_index;

    char                language[MAX_SUBTITLE_LANG_SIZE];   // should 64 bytes

    enum CodecID        codec_id;
    unsigned int        codec_id_addtion;

    unsigned char        *codec_priv;
    cdx_int32            codec_priv_size;

    unsigned char        content_comp;
    unsigned char        *comp_settings;
    unsigned char        comp_settings_len;

    double                time_scale;
    cdx_uint64            default_duration;
    MatroskaTrackFlags    flags;

    cdx_int32            block_size_type;
} MatroskaTrack;

typedef struct MatroskaVideoTrack {
    MatroskaTrack    track;

    cdx_int32            pixel_width;
    cdx_int32            pixel_height;
    cdx_int32            display_width;
    cdx_int32            display_height;
    cdx_uint16            frame_rate;

    unsigned int            fourcc;
} MatroskaVideoTrack;

typedef struct MatroskaAudioTrack {
    MatroskaTrack    track;

    cdx_int32           tag; // for wma
    cdx_int32            channels;
    cdx_int32            bitdepth;
    cdx_int32            internal_samplerate;
    cdx_int32            samplerate;
    cdx_int32            block_align;
    cdx_int32           bitrate;

    // real audio header
    cdx_int32            coded_framesize;
    cdx_int32            sub_packet_h;
    cdx_int32            frame_size;
    cdx_int32            sub_packet_size;
} MatroskaAudioTrack;

#define MAX_TRACK_SIZE (MKVMAX(sizeof(MatroskaVideoTrack), sizeof(MatroskaAudioTrack)))

typedef struct MatroskaLevel {
    cdx_uint64            start;
    cdx_uint64            length;
} MatroskaLevel;

typedef struct MatroskaDemuxIndex {
    cdx_uint64            pos;   /* of the corresponding *cluster*! */
    cdx_uint16            track; /* reference to 'num' */
    cdx_uint64            time;  /* in nanoseconds */
} MatroskaDemuxIndex;

#define MAX_SUBTITLE_NUM           MAX_SUBTITLE_STREAM_NUM  // SUBTITLE_MAX_NUM
#define MAX_STREAM_NUM    10
typedef struct MatroskaDemuxContext {
    CdxStreamT            *fp;

    cdx_int64           fileEndPst;

    cdx_int64            vDataPos;
    cdx_int64            FilePos;

    unsigned int        nb_streams;
    AVStream           *streams[MAX_STREAMS];

    // ebml stuff
    cdx_int32            num_levels;
    MatroskaLevel        levels[EBML_MAX_DEPTH];
    cdx_int32            level_up;
    cdx_int32            num_levels_bak;

    cdx_int64            time_scale;        // timescale in the file

    // decoding: duration of the the segment, in AV_TIME_BASE fractional seconds.
    double                segment_duration_f;
    // decoding: duration of the the segment, in AV_TIME_BASE fractional seconds.
    cdx_int64            segment_duration;

    // num_streams is the number of streams
    cdx_int32            num_tracks;
    cdx_int32            num_streams;
    MatroskaTrack        *tracks[MAX_STREAMS];

    unsigned int        peek_id;            // cache for ID peeking

    cdx_uint64            segment_start;        // byte position of the segment inside the stream

    unsigned int        vfw_fourcc;

    cdx_int32            index_parsed;        // index has been parsed
    cdx_int32            done;                // file end
    cdx_int32            data_rdy;            // 1: audio or video data is ready for read
    cdx_int32            parsing_cluster;    // 1: cluster is parsing

    cdx_int32            num_indexes;        // the number of index table
    MatroskaDemuxIndex  *index;            // index table
    cdx_int32            index_ptr;            //the point if indexs

    // What to skip before effectively reading a packet.
    // we find a cluster for seek, and then skip the data until a keyframe
    cdx_int32            skip_to_keyframe;
    AVStream            *skip_to_stream;
    cdx_int64            frame_num_between_kframe;

    unsigned char        *data;
    cdx_int32            data_size;            // the size of *data
    cdx_int32            sector_align;

    // copy the video data from file directly, not from matroska->pkt_data
    unsigned char        video_direct_copy;

    unsigned char        *data_parse;        // the parsing data poiter
    cdx_int32            size_parse;            // the data has left

    cdx_uint64            cluster_time;        // the timestamp of cluster

    // the track No. of the data in current block , it is the prefetch type
    cdx_int32            track;

    cdx_int32            stream_index;        // the stream No. of the data in current block
    cdx_int32            is_keyframe;        // the data in current block is keyframe?
    cdx_uint64            blk_duration;        // duration for block

    unsigned int        *lace_size;
    cdx_int32            laces;                // the valid lace num
    cdx_int32            max_laces;            // the *lace_size size
    cdx_int32            lace_cnt;            // the parsing lace No.

    cdx_int32            slice_len;
    cdx_int32            slice_len_t;
    unsigned char        *pkt_data;

    //cdx_int32           prefetch_track_num;
    // ( 1, 2, 3, ..., we should use prefetch_track_num-1 to get the track )
    unsigned int        chk_type;           //chunk type, audio type, video type, lyric type
    cdx_int32            t_size;             //total size of the chunk, based on byte
    cdx_int32            r_size;             //data size has been read out, based on byte
    cdx_int64            time_stamp;         //time stamp for current video chunk
    cdx_int64           time_stamp0;
    cdx_int64            time_stamp_error;

    //time stamp offset for error case(if some error occur, it will skip to next keyframe))
    cdx_int64            time_stamp_offset;

    cdx_int16            VideoTrackIndex;
    cdx_int16            AudioTrackIndex;
    cdx_int16            SubTitleTrackIndex;

    cdx_int16            VideoStreamIndex;
    cdx_int16            AudioStreamIndex;
    cdx_int16            SubTitleStreamIndex;
    char                SubTitleStream[MAX_SUBTITLE_NUM];
    unsigned int        SubTitleCodecType[MAX_SUBTITLE_NUM];
    unsigned int        SubTitledata_encode_type[MAX_SUBTITLE_NUM];
    char                SubTitleTrack[MAX_SUBTITLE_NUM];
    char                language[MAX_SUBTITLE_NUM][MAX_SUBTITLE_LANG_SIZE];

    //use for rmvb
    unsigned char       *pVbsInf;
    unsigned char       slice_num;
    unsigned char       slice_cnt;
    unsigned int        slice_offset;

    //use for audio
    unsigned char          *a_Header;
    cdx_int32           a_Header_size;

    //use for ogg
    cdx_uint64           ogg_granulepos;
    cdx_int32           ogg_pageno;
    unsigned char       ogg_uflag;

    //use for COOK RA ATRAC
    cdx_int32            sub_packet_cnt;
    unsigned char       *buf;
    cdx_int32           buf_size;

    unsigned char        frm_num;

    unsigned char        hasAudio;
    unsigned char        hasVideo;
    unsigned char       seamlessAudioSwitch;
    unsigned char       pkt_dataIndex;

    int                 nVideoStream;   // video stream number
    int                 nAudioStream;
    int                 nSubtitleStream;

    //use for seek key frame
    unsigned char*    tmpbuf;
    unsigned int    tmpbufsize;

    // get extradata for XVID, when do not have CodecPriv
    int   priDataLen;
    char* priData;

    // estinete bitrate, when the keyframe index is not exist. the unit is byte/ms
    cdx_uint64 bitrate;
} MatroskaDemuxContext;

//use for RMVB
#define HX_RVTRVIDEO_ID   0x52565452  /* 'RVTR' (for rv20 codec) */
#define HX_RVTR_RV30_ID   0x52565432  /* 'RVT2' (for rv30 codec) */

#define HX_RV10VIDEO_ID   0x52563130  /* 'RV10' */
#define HX_RV20VIDEO_ID   0x52563230  /* 'RV20' */
#define HX_RVG2VIDEO_ID   0x52564732  /* 'RVG2' (raw TCK format) */
#define HX_RV30VIDEO_ID   0x52563330  /* 'RV30' */
#define HX_RV40VIDEO_ID   0x52563430  /* 'RV40' */
#define HX_RV89COMBO_ID   0x54524F4D  /* 'TROM' (raw TCK format) */

#define RAW_BITSTREAM_MINOR_VERSION             128
#define RV20_MAJOR_BITSTREAM_VERSION            2
#define RV30_MAJOR_BITSTREAM_VERSION            3
#define RV40_SPO_BITS_NUMRESAMPLE_IMAGES        0x00070000  /* max of 8 RPR images sizes */
#define RV40_SPO_BITS_NUMRESAMPLE_IMAGES_SHIFT  16

#define ILVC_MSG_SPHI_Unrestricted_Motion_Vectors   0x1
#define ILVC_MSG_SPHI_Advanced_Prediction           0x4
#define ILVC_MSG_SPHI_Advanced_Intra_Coding         0x8
#define ILVC_MSG_SPHI_Deblocking_Filter             0x10
#define ILVC_MSG_SPHI_Slice_Structured_Mode         0x20
#define ILVC_MSG_SPHI_Slice_Shape                   0x40
#define ILVC_MSG_SPHI_Slice_Order                   0x80
#define ILVC_MSG_SPHI_Reference_Picture_Selection   0x100
#define ILVC_MSG_SPHI_Independent_Segment_Decoding  0x200
#define ILVC_MSG_SPHI_Alternate_Inter_VLC           0x400
#define ILVC_MSG_SPHI_Modified_Quantization         0x800
#define ILVC_MSG_SPHI_Deblocking_Strength           0xe000
#define ILVC_MSG_SPHI_Deblocking_Shift              13

typedef enum {
    FORBIDDEN=0,
    SQCIF = 1,
    QCIF = 2,
    CIF = 3,
    fCIF = 4,
    ssCIF = 5,
    CUSTOM  = 6,
    EPTYPE = 7,
}FRAMESIZE;

typedef enum {
    PIA_FID_UNDEFINED=0,
    PIA_FID_RGB4,
    PIA_FID_CLUT8,
    PIA_FID_XRGB16_1555,
    PIA_FID_RGB16_565,
    PIA_FID_RGB16_655,
    PIA_FID_RGB16_664,
    PIA_FID_RGB24,
    PIA_FID_XRGB32,
    PIA_FID_MPEG2V,
    PIA_FID_YVU9,
    PIA_FID_YUV12,
    PIA_FID_IYUV,
    PIA_FID_YV12,
    PIA_FID_YUY2,
    PIA_FID_BITMAP16,
    PIA_FID_H261,
    PIA_FID_H263,
    PIA_FID_H263PLUS,
    PIA_FID_TROMSO,
    PIA_FID_ILVR,
    PIA_FID_REALVIDEO21,
    PIA_FID_REALVIDEO22,
    PIA_FID_REALVIDEO30,
}SubIDVersion;

typedef struct __RV_FORMAT_INFO
{
    unsigned int   ulLength;
    unsigned int   ulMOFTag;
    unsigned int   ulSubMOFTag;
    cdx_uint16   usWidth;
    cdx_uint16   usHeight;
    cdx_uint16   usBitCount;
    cdx_uint16   usPadWidth;
    cdx_uint16   usPadHeight;
    unsigned int   ufFramesPerSecond;
    unsigned int   ulOpaqueDataSize;
    unsigned char*   pOpaqueData;
} __rv_format_info;

typedef struct __VDEC_RXG2_INIT_INFO
{
    cdx_int32        bUMV;
    cdx_int32        bAP;
    cdx_int32        bAIC;
    cdx_int32        bDeblocking;
    cdx_int32        bSliceStructured;
    cdx_int32        bRPS;
    cdx_int32        bISD;
    cdx_int32        bAIV;
    cdx_int32        bMQ;
    cdx_int32        iRvSubIdVersion;
    cdx_uint16        uNum_RPR_Sizes;
    cdx_uint16        FormatPlus;
    unsigned int        RmCodecID;

}__vdec_rxg2_init_info;

/*
typedef struct __OGG_HEADER
{
    unsigned int       uOggXFcc;       // four character code, "OggS" =0x5367674F
    unsigned char        uVesion;         //must =0;
    unsigned char        uflag;
          //&0x1=continued，即(uflag第0位）如果上一个page的最后一个ubadybytes[lacing_fill-1]＝0xff,
          //continued=1，说明上一个page的最后一个badybytes数据不完整，
          //本page的开始数据是上一个badybytes的数据。否则=0;
          //&0x2=bos;即(uflag第1位）bos为第一个page标志bos＝1，其余为0。
          //&0x4=eos;即(uflag第2位）eos为最后一个page标志eos=1,其余为0。如果不确定是最后一个page，必须=0
    cdx_uint64       granulepos;      //到本page结束一共有多少个采样值
    unsigned int       serialno;        //序列号，每个page必须相同
    unsigned int       pageno;          //当前为第几个page，第一个为0、跟着为1、2.....
    unsigned int       chksum;          //CRC校验值，如果没有=0。
    unsigned char        lacing_fill;     //本page含有多少个badybytes。

     //顺序填放badys的字节数，如果某个bady的字节数大于0xff，badybytes=n*0xff + y
    unsigned char        ubadybytes[lacing_fill];
} __ogg_header_t;*/

/*
// 这个结构体需要放在数据流的开始处传到decode去，decode将根据这个结构体提取解码信息。
typedef struct ra_format_info_struct
{
    unsigned int ulSampleRate;
    unsigned int ulActualRate;
    cdx_uint16 usBitsPerSample;
    cdx_uint16 usNumChannels;
    cdx_uint16 usAudioQuality;        //default = 100;
    cdx_uint16 usFlavorIndex;        //default = 0;for sipr。aac needn‘t it

    //default=0x100;=ulFrameSize;一帧的大小（bytes），对于aac 1block含1个frame
    unsigned int ulBitsPerFrame;

    //default=0x100;ulFramesPerBlock*ulFrameSize传过来block的大小，一个block包含1个或者多个frames
    unsigned int ulGranularity;
    unsigned int ulOpaqueDataSize;
    unsigned char  *pOpaqueData;
} ra_format_info;
*/

// use for sipr
// Must regenerate table if these are changed!
#define RA50_NCODEBITS          148             // bits per 5.0kbps sl frame
#define RA65_NCODEBITS          116             // bits per 6.5kbps sl frame
#define RA85_NCODEBITS          152             // bits per 8.5kbps sl frame
#define RA160_NCODEBITS         160             // bits per 16.0kbps sl frame

#define RASL_MAXCODEBYTES       20              // max bytes per sl frame
#define RASL_NFRAMES            16              // frames per block - must match codec
#define RASL_NBLOCKS            6               // blocks to interleave across

struct CdxMkvParser
{
    CdxParserT   parserinfo;
    CdxStreamT  *stream;
    CdxPacketT   packet;

    int          flag;    // the flag of open

    cdx_atomic_t ref;

    int          mStatus;
    int          mErrno;    // errno
    int          exitFlag;

    void        *priv_data;

    char         hasIndex;      //index table flag, =0:no index table; !=0:has index table

    char         hasVideo;      //video flag, =0:no video bitstream; >0:video bitstream count
    char         hasAudio;      //audio flag, =0:no audio bitstream; >0:audio bitstream count
    char         hasSubTitle;   //lyric flag, =0:no lyric bitstream; >0:lyric bitstream count

    int          nhasVideo;     //video flag, =0:no video bitstream; >0:video bitstream count
    int          nhasAudio;     //audio flag, =0:no audio bitstream; >0:audio bitstream count
    int          nhasSubTitle;  //lyric flag, =0:no lyric bitstream; >0:lyric bitstream count

    char         bDiscardAud;        //  1:discard, 0:transport

    int          VideoStreamIndex;       //video bitstream index

    //audio bitstream index ( index of all the tracks(video, audio, subtitle)  )
    int          AudioStreamIndex;

    int          SubTitleStreamIndex;    //lyric stream index
    int          UserSelSubStreamIdx;    //the lyric stream index which user select.

    //When user change subtitle,psr need sync.when sync over, set to 0.
    char         SubStreamSyncFlg;

    unsigned int   total_frames;           //total frame count
    unsigned int   picture_num;            //picture number

    unsigned int   nPreFRSpeed;            //previous ff/rr speed, for dynamic adjust

    //fast forward and fast backward speed, multiple of normal speed
    unsigned int   nFRSpeed;

    unsigned int   nFRPicShowTime;         //picture show time under fast forward and backward
    unsigned int   nFRPicCnt;              //picture count under ff/rr, for check if need delay

    unsigned int   nVidPtsOffset;          //video time offset
    unsigned int   nAudPtsOffset;          //audio time offset
    unsigned int   nSubPtsOffset;          //subtitle time offset

    char           seamlessAudioSwitch;    //seamlessAudioSwitch Flag

    cdx_int64        pos;

    cdx_int16        AudioTrackIndex[MAX_AUDIO_STREAM_NUM];//audio index to all of streams, a2st
    AudioStreamInfo aFormat_arry[MAX_AUDIO_STREAM_NUM];    //audio format

    VideoStreamInfo      vFormat;    //video format
    AudioStreamInfo      aFormat;    //audio format
    SubtitleStreamInfo   tFormat;    //audio format

    void                 *pVopData;          //vop header pointer
    cdx_int32            nVopDataSize;       //vop header left size
};

int CdxMatroskaOpen(struct CdxMkvParser *p);
int CdxMatroskaExit(struct CdxMkvParser *p);
struct CdxMkvParser* CdxMatroskaInit(int *ret);
cdx_int16 CdxMatroskaClose(struct CdxMkvParser *p);
cdx_int16 CdxMatroskaRead(struct CdxMkvParser *p);
int CdxMatroskaSeek(struct CdxMkvParser *p, cdx_int64  timeUs);

#endif
