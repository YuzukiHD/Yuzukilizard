/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMkvParserImpl.c
 * Description : MkvParserImpl
 * History :
 *
 */

#include <ctype.h>
#include <math.h>
#include "CdxMkvParser.h"
#include <CdxBinary.h>
#include <errno.h>
#include <CdxISOLang.h>

#define PHY_MALLOC(a,b) malloc(a)
#define PHY_FREE free

#define DATA_OFFSET 4096

#define ABS_EDIAN_FLAG_MASK         ((unsigned int)(1<<16))
#define ABS_EDIAN_FLAG_LITTLE       ((unsigned int)(0<<16))
#define ABS_EDIAN_FLAG_BIG          ((unsigned int)(1<<16))

#define MKVMAX(a,b) ((a) > (b) ? (a) : (b))
#define MKVMAX3(a,b,c) MKVMAX(MKVMAX(a,b),c)
#define MKVMIN(a,b) ((a) > (b) ? (b) : (a))
#define MKVMIN3(a,b,c) MKVMIN(MKVMIN(a,b),c)

#define MKVSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)

#define CDX_AV_RB16(x)  ((((const cdx_uint8*)(x))[0] << 8) | ((const cdx_uint8*)(x))[1])
#define CDX_AV_WB16(p, d) do { \
                    ((cdx_uint8*)(p))[1] = (d); \
                    ((cdx_uint8*)(p))[0] = (d)>>8; } while(0)

#define CDX_AV_RL16(x)  ((((const cdx_uint8*)(x))[1] << 8) | \
                      ((const cdx_uint8*)(x))[0])
#define CDX_AV_WL16(p, d) do { \
                    ((cdx_uint8*)(p))[0] = (d); \
                    ((cdx_uint8*)(p))[1] = (d)>>8; } while(0)

#define CDX_AV_RB32(x)  ((((const cdx_uint8*)(x))[0] << 24) | \
                     (((const cdx_uint8*)(x))[1] << 16) | \
                     (((const cdx_uint8*)(x))[2] <<  8) | \
                      ((const cdx_uint8*)(x))[3])
#define CDX_AV_WB32(p, d) do { \
                    ((cdx_uint8*)(p))[3] = (d); \
                    ((cdx_uint8*)(p))[2] = (d)>>8; \
                    ((cdx_uint8*)(p))[1] = (d)>>16; \
                    ((cdx_uint8*)(p))[0] = (d)>>24; } while(0)

#define CDX_AV_RL32(x) ((((const cdx_uint8*)(x))[3] << 24) | \
                    (((const cdx_uint8*)(x))[2] << 16) | \
                    (((const cdx_uint8*)(x))[1] <<  8) | \
                     ((const cdx_uint8*)(x))[0])
#define CDX_AV_WL32(p, d) do { \
                    ((cdx_uint8*)(p))[0] = (d); \
                    ((cdx_uint8*)(p))[1] = (d)>>8; \
                    ((cdx_uint8*)(p))[2] = (d)>>16; \
                    ((cdx_uint8*)(p))[3] = (d)>>24; } while(0)

/* Note: when encoding, the first matching tag is used, so order is
   important if multiple tags possible for a given codec. */
const AVCodecTag codec_bmp_tags[] = {
    { CODEC_ID_H265,         MKTAG('H', '2', '6', '5') },
    { CODEC_ID_H265,         MKTAG('h', '2', '6', '5') },
    { CODEC_ID_H265,         MKTAG('X', '2', '6', '5') },
    { CODEC_ID_H265,         MKTAG('x', '2', '6', '5') },
    { CODEC_ID_H265,         MKTAG('h', 'e', 'v', 'c') },
    { CODEC_ID_H265,         MKTAG('H', 'E', 'V', 'C') },
    { CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
    { CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
    { CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
    { CODEC_ID_H263,         MKTAG('H', '2', '6', '3') },
    { CODEC_ID_H263P,        MKTAG('H', '2', '6', '3') },
    { CODEC_ID_H263I,        MKTAG('I', '2', '6', '3') }, // intel h263
    { CODEC_ID_H261,         MKTAG('H', '2', '6', '1') },
    { CODEC_ID_H263P,        MKTAG('U', '2', '6', '3') },
    { CODEC_ID_H263P,        MKTAG('v', 'i', 'v', '1') },
    { CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
    { CODEC_ID_DIVX4,         MKTAG('D', 'I', 'V', 'X') },
    { CODEC_ID_DIVX5,         MKTAG('D', 'X', '5', '0') },
    { CODEC_ID_MPEG4,        MKTAG('X', 'V', 'I', 'D') },
    { CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
    { CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
    { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  0 ) }, // some broken avi use this
    { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
    { CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
    { CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
    { CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
    { CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
    { CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
    { CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '3') }, // default signature when using MSMPEG4
    { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', '4', '3') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', 'G', '3') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '5') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '6') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '4') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('A', 'P', '4', '1') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '1') },
    { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '0') },
    { CODEC_ID_MSMPEG4V2,    MKTAG('M', 'P', '4', '2') },
    { CODEC_ID_MSMPEG4V2,    MKTAG('D', 'I', 'V', '2') },
    { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', 'G', '4') },
    { CODEC_ID_WMV1,         MKTAG('W', 'M', 'V', '1') },
    { CODEC_ID_WMV2,         MKTAG('W', 'M', 'V', '2') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'd') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', 'd') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'l') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '2', '5') },
    { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '5', '0') },
    { CODEC_ID_DVVIDEO,      MKTAG('c', 'd', 'v', 'c') }, // Canopus DV
    { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '1') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('m', 'p', 'g', '2') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'P', 'E', 'G') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG('P', 'I', 'M', '1') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG('V', 'C', 'R', '2') },
    { CODEC_ID_MPEG1VIDEO,   MKTAG( 1 ,  0 ,  0 ,  16) },
    { CODEC_ID_MPEG2VIDEO,   MKTAG( 2 ,  0 ,  0 ,  16) },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('D', 'V', 'R', ' ') },
    { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'M', 'E', 'S') },
    { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
    { CODEC_ID_LJPEG,        MKTAG('L', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') }, // Pegasus lossless JPEG
    { CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'L', 'S') }, // JPEG-LS custom FOURCC for avi - encoder
    { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') }, // JPEG-LS custom FOURCC for avi - decoder
    { CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
    { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },
    { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'R', 'n') },
    { CODEC_ID_HUFFYUV,      MKTAG('H', 'F', 'Y', 'U') },
    { CODEC_ID_FFVHUFF,      MKTAG('F', 'F', 'V', 'H') },
    { CODEC_ID_CYUV,         MKTAG('C', 'Y', 'U', 'V') },
    { CODEC_ID_RAWVIDEO,     MKTAG( 0 ,  0 ,  0 ,  0 ) },
    { CODEC_ID_RAWVIDEO,     MKTAG('I', '4', '2', '0') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'Y', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', '1', '2') },
    { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'V', 'Y') },
    { CODEC_ID_RAWVIDEO,     MKTAG('I', 'Y', 'U', 'V') },
    { CODEC_ID_RAWVIDEO,     MKTAG('Y', '8', '0', '0') },
    { CODEC_ID_RAWVIDEO,     MKTAG('H', 'D', 'Y', 'C') },
    { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '1') },
    { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '2') },
    { CODEC_ID_INDEO4,       MKTAG('I', 'V', '4', '1') },
    { CODEC_ID_INDEO5,       MKTAG('I', 'V', '5', '0') },
    { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '1') },
    { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '0') },
    { CODEC_ID_VP5,          MKTAG('V', 'P', '5', '0') },
    { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '0') },
    { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '1') },
    { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '2') },
    { CODEC_ID_VP8,          MKTAG('V', 'P', '8', '0') },
    { CODEC_ID_VP8,          MKTAG('V', 'P', '8', '1') },
    { CODEC_ID_VP8,          MKTAG('V', 'P', '8', '2') },
    { CODEC_ID_VP9,          MKTAG('V', 'P', '9', '0') },
    { CODEC_ID_VP9,          MKTAG('V', 'P', '9', '1') },
    { CODEC_ID_VP9,          MKTAG('V', 'P', '9', '2') },
    { CODEC_ID_ASV1,         MKTAG('A', 'S', 'V', '1') },
    { CODEC_ID_ASV2,         MKTAG('A', 'S', 'V', '2') },
    { CODEC_ID_VCR1,         MKTAG('V', 'C', 'R', '1') },
    { CODEC_ID_FFV1,         MKTAG('F', 'F', 'V', '1') },
    { CODEC_ID_XAN_WC4,      MKTAG('X', 'x', 'a', 'n') },
    { CODEC_ID_MSRLE,        MKTAG('m', 'r', 'l', 'e') },
    { CODEC_ID_MSRLE,        MKTAG( 1 ,  0 ,  0 ,  0 ) },
    { CODEC_ID_MSVIDEO1,     MKTAG('M', 'S', 'V', 'C') },
    { CODEC_ID_MSVIDEO1,     MKTAG('m', 's', 'v', 'c') },
    { CODEC_ID_MSVIDEO1,     MKTAG('C', 'R', 'A', 'M') },
    { CODEC_ID_MSVIDEO1,     MKTAG('c', 'r', 'a', 'm') },
    { CODEC_ID_MSVIDEO1,     MKTAG('W', 'H', 'A', 'M') },
    { CODEC_ID_MSVIDEO1,     MKTAG('w', 'h', 'a', 'm') },
    { CODEC_ID_CINEPAK,      MKTAG('c', 'v', 'i', 'd') },
    { CODEC_ID_TRUEMOTION1,  MKTAG('D', 'U', 'C', 'K') },
    { CODEC_ID_MSZH,         MKTAG('M', 'S', 'Z', 'H') },
    { CODEC_ID_ZLIB,         MKTAG('Z', 'L', 'I', 'B') },
    { CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
    { CODEC_ID_4XM,          MKTAG('4', 'X', 'M', 'V') },
    { CODEC_ID_FLV1,         MKTAG('F', 'L', 'V', '1') },
    { CODEC_ID_FLASHSV,      MKTAG('F', 'S', 'V', '1') },
    { CODEC_ID_VP6F,         MKTAG('V', 'P', '6', 'F') },
    { CODEC_ID_SVQ1,         MKTAG('s', 'v', 'q', '1') },
    { CODEC_ID_TSCC,         MKTAG('t', 's', 'c', 'c') },
    { CODEC_ID_ULTI,         MKTAG('U', 'L', 'T', 'I') },
    { CODEC_ID_VIXL,         MKTAG('V', 'I', 'X', 'L') },
    { CODEC_ID_QPEG,         MKTAG('Q', 'P', 'E', 'G') },
    { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '0') },
    { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '1') },
    { CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', '3') },
    { CODEC_ID_VC1,          MKTAG('W', 'V', 'C', '1') },
    { CODEC_ID_VC1,          MKTAG('W', 'M', 'V', 'A') },
    { CODEC_ID_LOCO,         MKTAG('L', 'O', 'C', 'O') },
    { CODEC_ID_WNV1,         MKTAG('W', 'N', 'V', '1') },
    { CODEC_ID_AASC,         MKTAG('A', 'A', 'S', 'C') },
    { CODEC_ID_INDEO2,       MKTAG('R', 'T', '2', '1') },
    { CODEC_ID_FRAPS,        MKTAG('F', 'P', 'S', '1') },
    { CODEC_ID_THEORA,       MKTAG('t', 'h', 'e', 'o') },
    { CODEC_ID_TRUEMOTION2,  MKTAG('T', 'M', '2', '0') },
    { CODEC_ID_CSCD,         MKTAG('C', 'S', 'C', 'D') },
    { CODEC_ID_ZMBV,         MKTAG('Z', 'M', 'B', 'V') },
    { CODEC_ID_KMVC,         MKTAG('K', 'M', 'V', 'C') },
    { CODEC_ID_CAVS,         MKTAG('C', 'A', 'V', 'S') },
    { CODEC_ID_JPEG2000,     MKTAG('M', 'J', '2', 'C') },
    { CODEC_ID_VMNC,         MKTAG('V', 'M', 'n', 'c') },
    { CODEC_ID_TARGA,        MKTAG('t', 'g', 'a', ' ') },
    { CODEC_ID_CLJR,         MKTAG('c', 'l', 'j', 'r') },
    { CODEC_ID_NONE,         0 }
};

const AVCodecTag codec_wav_tags[] = {
    { CODEC_ID_PCM_S16LE,       0x0001 },
    { CODEC_ID_PCM_U8,          0x0001 }, // must come after s16le in this list
    { CODEC_ID_PCM_S24LE,       0x0001 },
    { CODEC_ID_PCM_S32LE,       0x0001 },
    { CODEC_ID_ADPCM_MS,        0x0002 },
    { CODEC_ID_PCM_ALAW,        0x0006 },
    { CODEC_ID_PCM_MULAW,       0x0007 },
    { CODEC_ID_WMAVOICE,        0x000A },
    { CODEC_ID_ADPCM_IMA_WAV,   0x0011 },
    { CODEC_ID_ADPCM_YAMAHA,    0x0020 },
    { CODEC_ID_TRUESPEECH,      0x0022 },
    { CODEC_ID_GSM_MS,          0x0031 },
    { CODEC_ID_ADPCM_G726,      0x0045 },
    { CODEC_ID_MP2,             0x0050 },
    { CODEC_ID_MP3,             0x0055 },
    { CODEC_ID_ADPCM_IMA_DK4,   0x0061 },  // rogue format number
    { CODEC_ID_ADPCM_IMA_DK3,   0x0062 },  // rogue format number
    { CODEC_ID_VOXWARE,         0x0075 },
    { CODEC_ID_AAC,             0x00ff },
    { CODEC_ID_WMAV1,           0x0160 },
    { CODEC_ID_WMAV2,           0x0161 },
    { CODEC_ID_WMAPRO,          0x0162 },
    { CODEC_ID_WMALOSSLESS,     0x0163 },
    { CODEC_ID_ADPCM_CT,        0x0200 },
    { CODEC_ID_ATRAC3,          0x0270 },
    { CODEC_ID_IMC,             0x0401 },
    { CODEC_ID_AC3,             0x2000 },
    { CODEC_ID_DTS,             0x2001 },
    { CODEC_ID_SONIC,           0x2048 },
    { CODEC_ID_SONIC_LS,        0x2048 },
    { CODEC_ID_AAC,             0x706d },
    { CODEC_ID_FLAC,            0xF1AC },
    { CODEC_ID_ADPCM_SWF,       ('S'<<8)+'F' },
    { CODEC_ID_VORBIS,          ('V'<<8)+'o' },
    //HACK/FIXME, does vorbis in WAV/AVI have an (in)official id?

    // FIXME: All of the IDs below are not 16 bit and thus illegal.
    // for NuppelVideo (nuv.c)
    { CODEC_ID_PCM_S16LE, MKTAG('R', 'A', 'W', 'A') },
    { CODEC_ID_MP3,       MKTAG('L', 'A', 'M', 'E') },
    { CODEC_ID_MP3,       MKTAG('M', 'P', '3', ' ') },
    { 0, 0 },
};

const CodecTags ff_mkv_codec_tags[]={
    {"V_MS/VFW/FOURCC"  , CODEC_ID_V_UNKNOWN},
    {"V_UNCOMPRESSED"   , CODEC_ID_RAWVIDEO},
    {"V_MPEG4/ISO/ASP"  , CODEC_ID_MPEG4},
    {"V_MPEG4/ISO/SP"   , CODEC_ID_MPEG4},
    {"V_MPEG4/ISO/AP"   , CODEC_ID_MPEG4},
    {"V_MPEG4/ISO/AVC"  , CODEC_ID_H264},
    {"V_MPEGH/ISO/HEVC" , CODEC_ID_H265},
    {"V_MPEG4/MS/V3"    , CODEC_ID_MSMPEG4V3},
    {"V_MPEG1"          , CODEC_ID_MPEG1VIDEO},
    {"V_MPEG2"          , CODEC_ID_MPEG2VIDEO},
    {"V_MJPEG"          , CODEC_ID_MJPEG},
    {"V_REAL/RV10"      , CODEC_ID_RV10},
    {"V_REAL/RV20"      , CODEC_ID_RV20},
    {"V_REAL/RV30"      , CODEC_ID_RV30},
    {"V_REAL/RV40"      , CODEC_ID_RV40},
    {"V_THEORA"         , CODEC_ID_THEORA},
    {"V_SNOW"           , CODEC_ID_SNOW},
    {"V_VP8"            , CODEC_ID_VP8},
    {"V_VP9"            , CODEC_ID_VP9},
// TODO: Real/Quicktime

    {"A_MS/ACM"         , CODEC_ID_A_UNKNOWN},
    {"A_MPEG/L3"        , CODEC_ID_MP3},
    {"A_MPEG/L2"        , CODEC_ID_MP2},
    {"A_MPEG/L1"        , CODEC_ID_MP2},
    {"A_PCM/INT/BIG"    , CODEC_ID_PCM_U16BE},
    {"A_PCM/INT/LIT"    , CODEC_ID_PCM_U16LE},
//    {"A_PCM/FLOAT/IEEE" , CODEC_ID_NONE},
    {"A_AC3"            , CODEC_ID_AC3},
    {"A_EAC3"           , CODEC_ID_AC3},
    {"A_TRUEHD"         , CODEC_ID_AC3},
    {"A_DTS"            , CODEC_ID_DTS},
    {"A_VORBIS"         , CODEC_ID_VORBIS},
    {"A_AAC"            , CODEC_ID_AAC},
    {"A_FLAC"           , CODEC_ID_FLAC},
    {"A_WAVPACK4"       , CODEC_ID_WAVPACK},
    {"A_TTA1"           , CODEC_ID_TTA},
    {"A_REAL/14_4"      , CODEC_ID_RA_144},
    {"A_REAL/28_8"      , CODEC_ID_RA_288},
    {"A_REAL/ATRC"      , CODEC_ID_ATRAC3},
    {"A_REAL/COOK"      , CODEC_ID_COOK},
    {"A_REAL/SIPR"      , CODEC_ID_SIPRO},
    {"A_OPUS"           , CODEC_ID_OPUS},

    {"S_TEXT/UTF8"      , CODEC_ID_TEXT},
    {"S_TEXT/ASCII"     , CODEC_ID_TEXT},
    {"S_TEXT/ASS"       , CODEC_ID_SSA},
    {"S_TEXT/SSA"       , CODEC_ID_SSA},
    {"S_ASS"            , CODEC_ID_SSA},
    {"S_SSA"            , CODEC_ID_SSA},
    {"S_VOBSUB"         , CODEC_ID_DVD_SUBTITLE},
    {"S_HDMV/PGS"        , CODEC_ID_PGS},

    {""                 , CODEC_ID_NONE}
// TODO: AC3-9/10 (?), Real, Musepack, Quicktime
};

const cdx_int32 ff_mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static int MkvResyncWithoutSeekBack(struct CdxMkvParser *p);

enum CodecID codec_get_id(const AVCodecTag *tags, cdx_uint32 findTag)
{
    cdx_int32 i;
    for(i=0; tags[i].id != CODEC_ID_NONE;i++) {
        if(findTag == tags[i].tag)
            return (enum CodecID)tags[i].id;
    }
    for(i=0; tags[i].id != CODEC_ID_NONE; i++) {
        if(   toupper((tags[i].tag >> 0)&0xFF) == toupper((findTag >> 0)&0xFF)
           && toupper((tags[i].tag >> 8)&0xFF) == toupper((findTag >> 8)&0xFF)
           && toupper((tags[i].tag >>16)&0xFF) == toupper((findTag >>16)&0xFF)
           && toupper((tags[i].tag >>24)&0xFF) == toupper((findTag >>24)&0xFF))
            return (enum CodecID)tags[i].id;
    }
    return CODEC_ID_NONE;
}

static cdx_int32 FEOF(MatroskaDemuxContext *matroska)
{
    cdx_int64    tmpPst;
    CdxStreamT *fp = matroska->fp;

    tmpPst = CdxStreamTell(fp);
    if(tmpPst >= matroska->fileEndPst)
    {
        //reach the file end
        return -1;
    }

    return 0;
}

/*******************************************************************
 * Read: an "EBML number", which is defined as a variable-length
 * array of bytes. The first byte indicates the length by giving a
 * number of 0-bits followed by a one. The position of the first
 * "one" bit inside the first byte indicates the length of this
 * number.
 * Returns: num. of bytes read. < 0 on error.
 *******************************************************************/
cdx_int32 ebml_read_num (MatroskaDemuxContext *matroska, cdx_int32 max_size, cdx_uint64 *number)
{
    CdxStreamT *pb = matroska->fp;
    cdx_int32 len_mask = 0x80, read = 1, n = 1;
    cdx_int64 total = 0;
    cdx_uint8  t[16];

    /* the first byte tells us the length in bytes - get_byte() can normally
     * return 0, but since that's not a valid first ebmlID byte, we can
     * use it safely here to catch EOS. */
    if(CdxStreamRead(pb, t, 1) < 1)
    {
        CDX_LOGE(" read error at pos(%llx)", CdxStreamTell(pb));
        return AVERROR(EIO); // EOS or actual I/O error
    }
    total = t[0];
    if (!total)
    {
        // we might encounter EOS here
        if (!FEOF(matroska))
        {
            cdx_uint64 pos = CdxStreamTell(pb);
            CDX_LOGW("Read error at pos %lld(0x%llx)", pos, pos);
            return AVERROR_INVALIDDATA;
        }
        return AVERROR(EIO); // EOS or actual I/O error
    }

    // get the length of the EBML number
    while (read <= max_size && !(total & len_mask))
    {
        read++;
        len_mask >>= 1;
    }
    if (read > max_size)
    {
        cdx_uint64 pos = CdxStreamTell(pb) - 1;
        CDX_LOGE("Invalid EBML number size tag 0x%llx at pos(0x%llx)", total, pos);
        return AVERROR_INVALIDDATA;
    }

    // read out length
    total &= ~len_mask;
    if(read-n>0)
    {
        if(CdxStreamRead(pb, &t[n],read-n) < (read-n))
        {
            return AVERROR(EIO); // EOS or actual I/O error
        }
    }
    while (n++ < read)
        total = (total << 8) | t[n-1];

    *number = total;

    return read;
}

/*************************************************************************************
 * Read: the element content data ID.
 * Return: the number of bytes read or < 0 on error.
 *************************************************************************************/
cdx_int32 ebml_read_element_id (MatroskaDemuxContext *matroska, cdx_uint32 *id, cdx_int32 *level_up)
{
    cdx_int32 read;
    cdx_uint64 total;

    // if we re-call this, use our cached ID
    if (matroska->peek_id != 0)
    {
        if (level_up)
            *level_up = 0;
        *id = matroska->peek_id;
        return 0;
    }

    // read out the "EBML number", include tag in ID
    if ((read = ebml_read_num(matroska, 4, &total)) < 0)
        return read;
    *id = matroska->peek_id  = (cdx_uint32)(total | (1 << (read * 7)));

    //the amount of levels in the hierarchy that the
    //current element lies higher than the previous one.
    if (level_up)
    {
        CdxStreamT *pb = matroska->fp;
        cdx_uint64 pos = CdxStreamTell(pb);
        *level_up = 0;

        while (matroska->num_levels > 0)
        {
            MatroskaLevel *currentLevel = &matroska->levels[matroska->num_levels - 1];

            if (pos >= currentLevel->start + currentLevel->length)
            {
                matroska->num_levels--;
                (*level_up)++;
            }
            else
            {
                break;
            }
        }
    }

    return read;
}

/**********************************************************************************
 * Read: element content length.
 * Return: the number of bytes read or < 0 on error.
 **********************************************************************************/
cdx_int32 ebml_read_element_length (MatroskaDemuxContext *matroska, cdx_uint64 *length)
{
    // clear cache since we're now beyond that data point
    matroska->peek_id = 0;

    // read out the "EBML number", include tag in ID
    return ebml_read_num(matroska, 8, length);
}

/*****************************************************************
 * Return: the ID of the next element, or 0 on error.
 * Level_up contains the amount of levels that this
 * next element lies higher than the previous one.
 *****************************************************************/
cdx_uint32 ebml_peek_id (MatroskaDemuxContext *matroska, cdx_int32 *level_up)
{
    cdx_uint32 id = 0;

    if (ebml_read_element_id(matroska, &id, level_up) < 0)
    {
        CDX_LOGE("Invalid id(%d) at pos (0x%llx)", id, CdxStreamTell(matroska->fp));
        return 0;
    }

    return id;
}

/****************************************************************
 * Seek to a given offset.
 * 0 is success, -1 is failure.
 ****************************************************************/
cdx_int32 ebml_read_seek (MatroskaDemuxContext *matroska,  cdx_uint64 offset)
{
    CdxStreamT *pb = matroska->fp;
    int ret;

    // clear ID cache, if any
    matroska->peek_id = 0;

    if(CdxStreamSeekAble(pb))
    {
        ret = CdxStreamSeek(pb, offset, SEEK_SET);
    }
    else
    {
        int64_t tell = CdxStreamTell(pb);
        if((cdx_uint64)tell > offset)
        {
            CDX_LOGE("cannot seek");
            return -1;
        }
        else
        {
            ret = CdxStreamSkip(pb, offset-tell);
        }
    }
    return ret;
}

/******************************************************************
 * Skip the next element.
 * 0 is success, -1 is failure.
 ******************************************************************/
cdx_int32 ebml_read_skip (MatroskaDemuxContext *matroska)
{
    CdxStreamT *pb = matroska->fp;
    cdx_uint32 id;
    cdx_uint64 length;
    cdx_int32 res;

    if ((res = ebml_read_element_id(matroska, &id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &length)) < 0)
        return res;

    if(CdxStreamSeekAble(pb))
    {
        CdxStreamSeek(pb, length, SEEK_CUR);
    }
    else
    {
        CdxStreamSkip(pb, length);
    }

    return 0;
}

/*************************************************************************
 * Read the next element as an unsigned int.
 * 0 is success, < 0 is failure.
 ************************************************************************/
cdx_int32 ebml_read_uint (MatroskaDemuxContext *matroska, cdx_uint32 *id, cdx_uint64 *num)
{
    CdxStreamT *pb = matroska->fp;
    cdx_int32 n = 0, size, res;
    cdx_uint64 rlength;
    cdx_uint8  t[16];

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &rlength)) < 0)
        return res;

    size = (cdx_int32)rlength;
    if (size < 1 || size > 8)
    {
        //cdx_uint64 pos = CdxStreamTell(pb);
        return AVERROR_INVALIDDATA;
    }

    // big-endian ordening; build up number
    *num = 0;
    if(size-n>0)
    {
        if(CdxStreamRead(pb, &t[n], size-n) < (size-n))
        {
            return AVERROR(EIO); // EOS or actual I/O error
        }
    }

    while (n++ < size)
        *num = (*num << 8) | t[n-1];

    return 0;
}

/*************************************************************
 * Read the next element as a float.
 * 0 is success, < 0 is failure.
 ************************************************************/
#ifdef __OS_EPOS
cdx_int32 ebml_read_float (MatroskaDemuxContext *matroska, cdx_uint32 *id, double *num)
{
    CdxStreamT *pb = matroska->fp;
    cdx_int32 size, res;
    cdx_uint64 rlength;

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &rlength)) < 0)
        return res;
    size = (cdx_int32)rlength;

    if (size == 4)
    {
        cdx_int32 v = CdxStreamGetBE32(pb);
        float *v_f = (float *)&v;
        *num = *v_f;
    }
    else if(size==8)
    {
        cdx_int64 v = CdxStreamGetBE64(pb);
        double *v_b = (double *)&v;
        *num = *v_b;
    }
    else
    {
        cdx_uint64 pos = CdxStreamTell(pb);
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

#else

static double av_int2dbl(cdx_uint64 v)
{
    if(v+v > 0xFFEULL<<52)
        return 0.0/0.0;
    return ldexp(((v&((1LL<<52)-1)) + (1LL<<52)) * (v>>63|1), (v>>52&0x7FF)-1075);
}

static float av_int2flt(cdx_uint32 v){
    if(v+v > 0xFF000000U)
        return 0.0/0.0;
    return ldexp(((v&0x7FFFFF) + (1<<23)) * (v>>31|1), (v>>23&0xFF)-150);
}

cdx_int32 ebml_read_float (MatroskaDemuxContext *matroska, cdx_uint32 *id, double *num)
{
    CdxStreamT *pb = matroska->fp;
    cdx_int32 size, res;
    cdx_uint64 rlength;

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &rlength)) < 0)
        return res;
    size = (cdx_int32)rlength;

    if (size == 4)
    {
        *num = av_int2flt(CdxStreamGetBE32(pb));
    }
    else if(size==8)
    {
        *num= av_int2dbl(CdxStreamGetBE64(pb));
    }
    else
    {
        //cdx_uint64 pos = CdxStreamTell(pb);
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

#endif

/*******************************************************************
 * Read the next element as an ASCII string.
 * 0 is success, < 0 is failure.
 *******************************************************************/
cdx_int32 ebml_read_ascii (MatroskaDemuxContext *matroska, cdx_uint32 *id, char **str)
{
    CdxStreamT *pb = matroska->fp;
    cdx_int32 size, res;
    cdx_uint64 rlength;

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &rlength)) < 0)
        return res;
    size = (cdx_int32)rlength;

    // ebml strings are usually not 0-terminated, so we allocate one byte more,
    //read the string and NULL-terminate it ourselves.
    if (size < 0)
    {
        return AVERROR(ENOMEM);
    }

    if (*str != NULL)
    {
        loge("may be there is memleak");
    }

    *str = malloc(size + 1);
    if(!(*str))
        return AVERROR(ENOMEM);

    if (CdxStreamRead(pb, (*str), (size)) != (cdx_int32)size)
    {
        //cdx_uint64 pos = CdxStreamTell(pb);
        free(*str);
        *str = NULL;
        return AVERROR(EIO);
    }
    (*str)[size] = '\0';

    return 0;
}

/*****************************************************************
 * Read the next element, but only the header. The contents
 * are supposed to be sub-elements which can be read separately.
 * 0 is success, < 0 is failure.
 ****************************************************************/
cdx_int32 ebml_read_master (MatroskaDemuxContext *matroska, cdx_uint32 *id)
{
    CdxStreamT *pb = matroska->fp;
    cdx_uint64 length;
    MatroskaLevel *currentLevel;
    cdx_int32 res;

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &length)) < 0)
        return res;

    // protect... (Heaven forbids that the '>' is true)
    if (matroska->num_levels >= EBML_MAX_DEPTH)
    {
        return AVERROR(ENOSYS);
    }

    // remember level
    currentLevel = &matroska->levels[matroska->num_levels++];
    currentLevel->start = CdxStreamTell(pb);
    currentLevel->length = length;

    return 0;
}

/*********************************************************
 * Read the next element as binary data.
 * 0 is success, < 0 is failure.
 *********************************************************/
cdx_int32 ebml_read_binary (MatroskaDemuxContext *matroska,
                            cdx_uint32 *id, cdx_uint8 **binary, cdx_int32 *size, cdx_int32 offset)
{
    CdxStreamT *pb = matroska->fp;
    cdx_uint64 rlength;
    cdx_int32 res;

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &rlength)) < 0)
        return res;

    if(*binary==NULL)
    {
        *binary = PHY_MALLOC((cdx_uint32)(rlength+offset), 0);
        if (!(*binary)) {
            return AVERROR(ENOMEM);
        }
    }
    else
    {
        if((int)(rlength+offset)>*size)
        {
            PHY_FREE(*binary);
            *binary = PHY_MALLOC((cdx_uint32)(rlength+offset), 0);
            if (!(*binary)) {
                return AVERROR(ENOMEM);
            }
        }
    }
    *size = (cdx_int32)rlength;

    if (CdxStreamRead(pb, (*binary+offset), (*size)) != (cdx_int32)(*size))
    {
        //cdx_uint64 pos = CdxStreamTell(pb);
        return AVERROR(EIO);
    }

    return 0;
}

/****************************************************************************
 * Read an EBML header.
 * 0 is success, < 0 is failure.
 *
 * version indicate the minimum version number a DocType
 parser must be compliant with to read the file, which is in DocTypeReadVersion (ID: 0x4285)
 ****************************************************************************/
cdx_int32 ebml_read_header (MatroskaDemuxContext *matroska, cdx_int32 *version)
{
    cdx_uint32 id;
    cdx_int32 level_up, res = 0;

    // default init
    if (version)
        *version = 1;

    id = ebml_peek_id(matroska, &level_up);
    if (!id || level_up != 0 || id != CDX_EBML_ID_HEADER)
    {
        return AVERROR_INVALIDDATA;
    }
    res = ebml_read_master(matroska, &id);
    if (res < 0)
        return res;

    while (res == 0)
    {
        //**< get the EMBL id
        id = ebml_peek_id(matroska, &level_up);
        if (!id)
            return AVERROR(EIO);

        // end-of-header
        if (level_up)
            break;

        switch (id)
        {
            // is our read version uptodate?
            case CDX_EBML_ID_EBMLREADVERSION:
            {
                cdx_uint64 num;

                if ((res = ebml_read_uint(matroska, &id, &num)) < 0)
                    return res;
                if (num > EBML_VERSION)
                {
                    return AVERROR_INVALIDDATA;
                }
                break;
            }

            // we only handle 8 byte lengths at max
            // EMBL max number
            case CDX_EBML_ID_EBMLMAXSIZELENGTH:
            {
                cdx_uint64 num;

                if ((res = ebml_read_uint(matroska, &id, &num)) < 0)
                    return res;
                if (num > sizeof(cdx_uint64))
                {
                    CDX_LOGW("EMBL max size length(%lld) is large than default size(8)", num);
                    return AVERROR_INVALIDDATA;
                }
                break;
            }

            // we handle 4 byte IDs at max
            case CDX_EBML_ID_EBMLMAXIDLENGTH:
            {
                cdx_uint64 num;

                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    return res;
                if (num > sizeof(cdx_uint32))
                {
                    CDX_LOGW("EMBL ID size(%lld) is large than default size(4)", num);
                    return AVERROR_INVALIDDATA;
                }
                break;
            }

            case CDX_EBML_ID_DOCTYPE:
            {
                char *doctype = NULL;

                res = ebml_read_ascii(matroska, &id, &doctype);
                if (res < 0)
                {
                    if(doctype)
                    {
                        free(doctype);
                    }
                    return res;
                }
                if (strcmp(doctype, "matroska") &&
                    strcmp(doctype, "webm"))
                {
                    free(doctype);
                    return AVERROR_NOFMT;
                }
                free(doctype);
                break;
            }

            case CDX_EBML_ID_DOCTYPEREADVERSION:
            {
                cdx_uint64 num;

                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    return res;
                if (version)
                    *version = (cdx_int32)num;
                break;
            }

            default:
                res = ebml_read_skip (matroska);
                break;
        }
    }

    return 0;
}

/******************************************************************
 * From here on, it's all XML-style DTD stuff... Needs no comments.
 * including duration, timescale, segmentUID
 ******************************************************************/
cdx_int32 matroska_parse_info (MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    cdx_uint32 id;

    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            res = AVERROR(EIO);
            break;
        }
        else if (matroska->level_up)
        {
            matroska->level_up--;
            break;
        }

        switch (id)
        {
            // cluster timecode
            case CDX_MATROSKA_ID_TIMECODESCALE:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                matroska->time_scale = num;
                break;
            }

            case CDX_MATROSKA_ID_DURATION:
            {
                res = ebml_read_float(matroska, &id, &(matroska->segment_duration_f));
                if (res < 0)
                    break;
                break;
            }

            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up)
        {
            matroska->level_up--;
            break;
        }
    }

    matroska->segment_duration =
            (cdx_int64)(matroska->segment_duration_f * matroska->time_scale / AV_TIME_BASE);

    return res;
}

cdx_int32 matroska_aac_profile (char *codec_id)
{
    const char *aac_profiles[] = { "MAIN", "LC", "SSR" };
    size_t profile;

    for (profile=0; profile<ARRAY_SIZE(aac_profiles); profile++)
        if (strstr(codec_id, aac_profiles[profile]))
            break;
    return profile + 1;
}

cdx_int32 matroska_aac_sri (cdx_int32 samplerate)
{
    cdx_uint32 sri;

    for (sri=0; sri<ARRAY_SIZE(ff_mpeg4audio_sample_rates); sri++)
        if (ff_mpeg4audio_sample_rates[sri] == samplerate)
            break;
    return sri;
}

/**********************************************************************************
* child tracks of track entry :
*       1. trackNumber,
*       2. trackUID( unique id of track in this file)
*       3. tracktype (video, audio, subtitle)
*       4. Video, audio,
***********************************************************************************/
cdx_int32 matroska_add_stream (MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    cdx_uint32 id;
    MatroskaTrack *track;
    cdx_int32 j;
    char *track_name;

    track_name = NULL;
    // start with the master
    if ((res = ebml_read_master(matroska, &id)) < 0)
        return res;

    // Allocate a generic track.
    track = malloc(MAX_TRACK_SIZE);
    if(!track)
    {
        CDX_LOGE("no memory, errno = %d", errno);
        return -1;
    }
    memset(track, 0, MAX_TRACK_SIZE);
    track->time_scale = 1.0;
    strcpy((char *)track->language, "English");

    // try reading the trackentry headers
    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            res = AVERROR(EIO);
            break;
        }
        else if (matroska->level_up > 0)
        {
            matroska->level_up--;
            break;
        }

        switch (id)
        {
            // track number (unique stream ID)
            case CDX_MATROSKA_ID_TRACKNUMBER:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                track->num = (cdx_uint32)num;
                break;
            }

            // track type (video, audio, combined, subtitle, etc.)
            case CDX_MATROSKA_ID_TRACKTYPE:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                if (track->type && track->type != num)
                {
                    break;
                }
                track->type = (MatroskaTrackType)num;

                switch (track->type)
                {
                    case MATROSKA_TRACK_TYPE_VIDEO:
                        track->nStreamIdx = matroska->nVideoStream;
                        matroska->nVideoStream++;
                        break;

                    case MATROSKA_TRACK_TYPE_AUDIO:
                        track->nStreamIdx = matroska->nAudioStream;
                        matroska->nAudioStream++;
                        break;

                    case MATROSKA_TRACK_TYPE_SUBTITLE:
                        track->nStreamIdx = matroska->nSubtitleStream;
                        matroska->nSubtitleStream++;
                        break;

                    default:
                        track->type = MATROSKA_TRACK_TYPE_NONE;
                        break;
                }
                break;
            }

            // tracktype specific stuff for video
            case CDX_MATROSKA_ID_TRACKVIDEO:
            {
                MatroskaVideoTrack *videotrack;
                if (!track->type)
                    track->type = MATROSKA_TRACK_TYPE_VIDEO;
                if (track->type != MATROSKA_TRACK_TYPE_VIDEO)
                {
                    res = AVERROR_INVALIDDATA;
                    break;
                }
                else if ((res = ebml_read_master(matroska, &id)) < 0)
                    break;
                videotrack = (MatroskaVideoTrack *)track;

                while (res == 0)
                {
                    id = ebml_peek_id(matroska, &matroska->level_up);
                    if (!id)
                    {
                        res = AVERROR(EIO);
                        break;
                    }
                    else if (matroska->level_up > 0)
                    {
                        matroska->level_up--;
                        break;
                    }

                    switch (id)
                    {
                        case CDX_MATROSKA_ID_TRACKDEFAULTDURATION:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint (matroska, &id, &num);
                            if (res < 0)
                                break;
                            track->default_duration = num;
                            break;
                        }

                        // video framerate
                        case CDX_MATROSKA_ID_VIDEOFRAMERATE:
                        {
                            double num;
                            res = ebml_read_float(matroska, &id, &num);
                            if (res < 0)
                                break;
                            videotrack->frame_rate = (cdx_uint16)(num*1000);
                            CDX_LOGD("--- video framerate = %d", videotrack->frame_rate);
                            if (!track->default_duration&&num)
                                track->default_duration = (cdx_uint64)(1000000000/num);
                            break;
                        }

                        // width of the size to display the video at
                        case CDX_MATROSKA_ID_VIDEODISPLAYWIDTH:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            videotrack->display_width = (cdx_int32)num;
                            break;
                        }

                        // height of the size to display the video at
                        case CDX_MATROSKA_ID_VIDEODISPLAYHEIGHT:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            videotrack->display_height = (cdx_int32)num;
                            break;
                        }

                        // width of the video in the file
                        case CDX_MATROSKA_ID_VIDEOPIXELWIDTH:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            videotrack->pixel_width = (cdx_int32)num;
                            break;
                        }

                        // height of the video in the file
                        case CDX_MATROSKA_ID_VIDEOPIXELHEIGHT:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            videotrack->pixel_height = (cdx_int32)num;
                            break;
                        }

                        // whether the video is interlaced
                        case CDX_MATROSKA_ID_VIDEOFLAGINTERLACED:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            if (num)
                                track->flags |=  MATROSKA_VIDEOTRACK_INTERLACED;
                            else
                                track->flags &=  ~MATROSKA_VIDEOTRACK_INTERLACED;
                            break;
                        }

                        // colorspace (only matters for raw video)  fourcc
                        case CDX_MATROSKA_ID_VIDEOCOLORSPACE:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            videotrack->fourcc = (cdx_uint32)num;
                            break;
                        }

                        default:
                            CDX_LOGW("cannot support this atom: %x", id);
                            res = ebml_read_skip(matroska);
                            break;
                    }

                    if (matroska->level_up) {
                        matroska->level_up--;
                        break;
                    }
                }
                break;
            }

            // tracktype specific stuff for audio
            case CDX_MATROSKA_ID_TRACKAUDIO:
            {
                MatroskaAudioTrack *audiotrack;
                if (!track->type)
                    track->type = MATROSKA_TRACK_TYPE_AUDIO;
                if (track->type != MATROSKA_TRACK_TYPE_AUDIO) {
                    res = AVERROR_INVALIDDATA;
                    break;
                } else if ((res = ebml_read_master(matroska, &id)) < 0)
                    break;
                audiotrack = (MatroskaAudioTrack *)track;
                audiotrack->channels = 1;
                audiotrack->samplerate = 8000;

                while (res == 0)
                {
                    id = ebml_peek_id(matroska, &matroska->level_up);
                    if (!id)
                    {
                        res = AVERROR(EIO);
                        break;
                    }
                    else if (matroska->level_up > 0)
                    {
                        matroska->level_up--;
                        break;
                    }

                    switch (id)
                    {
                        // samplerate
                        case CDX_MATROSKA_ID_AUDIOSAMPLINGFREQ:
                        {
                            double num;
                            res = ebml_read_float(matroska, &id, &num);
                            if (res < 0)
                                break;
                            audiotrack->internal_samplerate =
                                    audiotrack->samplerate = (cdx_int32)num;
                            break;
                        }

                        case CDX_MATROSKA_ID_AUDIOOUTSAMPLINGFREQ:
                        {
                            double num;
                            res = ebml_read_float(matroska, &id, &num);
                            if (res < 0)
                                break;
                            audiotrack->samplerate = (cdx_int32)num;
                            break;
                        }

                        // bitdepth  (bit pre sample)
                        case CDX_MATROSKA_ID_AUDIOBITDEPTH:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            audiotrack->bitdepth = (cdx_int32)num;
                            break;
                        }

                        // nChannelNum
                        case CDX_MATROSKA_ID_AUDIOCHANNELS:
                        {
                            cdx_uint64 num;
                            res = ebml_read_uint(matroska, &id, &num);
                            if (res < 0)
                                break;
                            audiotrack->channels = (cdx_int32)num;
                            break;
                        }

                        default:
                            CDX_LOGW("cannot support this id: %x", id);
                            res = ebml_read_skip(matroska);
                            break;
                    }

                    if (matroska->level_up) {
                        matroska->level_up--;
                        break;
                    }
                }
                break;
            }

            // codec identifier
            case CDX_MATROSKA_ID_CODECID:
            {
                char *text=NULL;
                res = ebml_read_ascii(matroska, &id, &text);
                if (res < 0)
                {
                    track->codec_id = CODEC_ID_NONE;
                    break;
                }

                for(j=0; ff_mkv_codec_tags[j].id != CODEC_ID_NONE; j++)
                {
                    if(!strncmp(ff_mkv_codec_tags[j].str,
                                text,
                                strlen(ff_mkv_codec_tags[j].str)))
                    {
                        track->codec_id = ff_mkv_codec_tags[j].id;
                        break;
                    }
                }

                if(track->codec_id == CODEC_ID_AAC)
                {
                    track->codec_id_addtion = matroska_aac_profile(text);
                    if(strstr((char *)text, (char *)"SBR"))
                    {
                        track->codec_id_addtion += 0x100;
                    }
                }

                free(text);
                break;
            }

            // codec private data (extradata)
            case CDX_MATROSKA_ID_CODECPRIVATE:
            {
                cdx_uint8 *data = NULL;
                cdx_int32 size;
                res = ebml_read_binary(matroska, &id, &data, &size, 0);
                if (res < 0)
                    break;
                track->codec_priv = data;
                track->codec_priv_size = size;
                break;
            }

            // name of this track
            case CDX_MATROSKA_ID_TRACKNAME:
            {
                res = ebml_read_ascii(matroska, &id, &track_name);
                break;
            }

            // language (matters for audio/subtitles, mostly)
            case CDX_MATROSKA_ID_TRACKLANGUAGE:
            {
                char *text = NULL;
                char *end = NULL;
                res = ebml_read_ascii(matroska, &id, &text);
                if (res < 0)
                    break;
                end = strchr(text, '-');
                if (end)
                    *end = '\0';

                int i;
                for (i = 0; CDX_ISO_639_LANGS[i].eng_name != NULL; ++i)
                {
                    if (!strcmp(text, CDX_ISO_639_LANGS[i].iso639_2B) ||
                            !strcmp(text, CDX_ISO_639_LANGS[i].iso639_2T))
                    {
                        strcpy(track->language, CDX_ISO_639_LANGS[i].eng_name);
                        break;
                    }
                }

                if (CDX_ISO_639_LANGS[i].eng_name == NULL)
                {
                    // Todo: cannot find any standard/specification about
                    // "cht", add a reference or remove the code
                    if (!strcmp(text, "cht"))
                    {
                        strcpy(track->language, "Chinese-Traditional");
                    }
                    else
                    {
                        if (strcmp(text, "und"))
                            CDX_LOGD("unrecognized language: %s", text);
                        strcpy(track->language, "und");
                    }
                }

                free(text);
                break;
            }

            // whether this is actually used
            case CDX_MATROSKA_ID_TRACKFLAGENABLED:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                if (num)
                    track->flags |= MATROSKA_TRACK_ENABLED;
                else
                    track->flags &= ~MATROSKA_TRACK_ENABLED;
                break;
            }

            // When FLAGDEFAULT is 1, the track should be selected by
            // the player by default.
            case CDX_MATROSKA_ID_TRACKFLAGDEFAULT:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                if (num)
                    track->flags |= MATROSKA_TRACK_DEFAULT;
                else
                    track->flags &= ~MATROSKA_TRACK_DEFAULT;
                break;
            }

            //lacing (like MPEG, where blocks don't end/start on frame boundaries)
            case CDX_MATROSKA_ID_TRACKFLAGLACING:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                if (num)
                    track->flags |= MATROSKA_TRACK_LACING;
                else
                    track->flags &= ~MATROSKA_TRACK_LACING;
                break;
            }

            // default length (in time) of one data block in this track
            case CDX_MATROSKA_ID_TRACKDEFAULTDURATION:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                track->default_duration = num;
                break;
            }

            case CDX_MATROSKA_ID_TRACKTIMECODESCALE:
            {
                double num;
                res = ebml_read_float(matroska, &id, &num);
                if (res < 0)
                    break;
                track->time_scale = num;
                break;
            }

            // CONTENTENCODINGS contains information about (lossless)
            // compression or encryption of the track
            case CDX_MATROSKA_ID_TRACKCONTENTENCODINGS:
            {
                cdx_uint32 unsupported = 0;
                if ((res = ebml_read_master(matroska, &id)) < 0)
                    break;

                while (res == 0)
                {
                    id = ebml_peek_id(matroska, &matroska->level_up);
                    if (!id) {
                        res = AVERROR(EIO);
                        break;
                    } else if (matroska->level_up > 0) {
                        matroska->level_up--;
                        break;
                    }

                    switch (id)
                    {
                    case CDX_MATROSKA_ID_TRACKCONTENTENCODING:
                    {
                        track->encoding_scope = 1;
                        if ((res = ebml_read_master(matroska, &id)) < 0)
                            break;

                        while (res == 0)
                        {
                            id = ebml_peek_id(matroska, &matroska->level_up);
                            if (!id) {
                                res = AVERROR(EIO);
                                break;
                            } else if (matroska->level_up > 0) {
                                matroska->level_up--;
                                break;
                            }

                            switch (id)
                            {
                            case CDX_MATROSKA_ID_ENCODINGSCOPE: {
                                cdx_uint64 num;
                                if ((res = ebml_read_uint(matroska, &id, &num)) < 0)
                                    break;
                                track->encoding_scope = num;
                                break;
                            }

                            case CDX_MATROSKA_ID_ENCODINGTYPE: {
                                cdx_uint64 num;
                                if ((res = ebml_read_uint(matroska, &id, &num)) < 0)
                                    break;
                                if (num)
                                    unsupported = 1;
                                break;
                            }

                            case CDX_MATROSKA_ID_ENCODINGCOMPRESSION: {
                                if ((res = ebml_read_master(matroska, &id)) < 0)
                                    break;

                                while (res == 0) {
                                    id = ebml_peek_id(matroska, &matroska->level_up);
                                    if (!id) {
                                        res = AVERROR(EIO);
                                        break;
                                    } else if (matroska->level_up > 0) {
                                        matroska->level_up--;
                                        break;
                                    }

                                    switch (id) {
                                        case CDX_MATROSKA_ID_ENCODINGCOMPALGO: {
                                            cdx_uint64 num;
                                            if ((res = ebml_read_uint(matroska, &id, &num)) < 0)
                                                break;

                                            //
                                            if (num != MATROSKA_TRACK_ENCODING_COMP_ZLIB)
                                                unsupported = 1;
                                            track->content_comp = num;
                                            break;
                                        }
                                        case CDX_MATROSKA_ID_ENCODINGCOMPSETTINGS:
                                        {
                                            cdx_uint8 *data = NULL;
                                            cdx_int32 size;
                                            res = ebml_read_binary(matroska, &id, &data, &size, 0);
                                            if (res < 0)
                                                break;
                                            track->comp_settings = data;
                                            track->comp_settings_len = size;
                                            break;
                                        }

                                        default:
                                        case CDX_EBML_ID_VOID:
                                            res = ebml_read_skip(matroska);
                                        break;
                                    }

                                    if (matroska->level_up) {
                                        matroska->level_up--;
                                        break;
                                    }
                                }
                                break;
                            }

                            default:
                            case CDX_EBML_ID_VOID:
                                res = ebml_read_skip(matroska);
                                break;
                            }

                            if (matroska->level_up) {
                                matroska->level_up--;
                                break;
                            }
                        }
                        break;
                    }

                    default:
                    case CDX_EBML_ID_VOID:
                        res = ebml_read_skip(matroska);
                        break;
                }

                if (matroska->level_up) {
                    matroska->level_up--;
                    break;
                }
                if(unsupported)
                    track->encoding_scope = 0;;
                }
                break;
            }

            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up) {
            matroska->level_up--;
            break;
        }
    }

       //  we do not need the track name add to language
    if(track_name)
    {
        cdx_int32 i;
        cdx_int32 name_len = strlen((char *)track_name) + 2;
        if((strcmp((const char *)track->language, "und"))==0)
        {
            memcpy(track->language, track_name, (ADECODER_MAX_LANG_CHAR_SIZE-2));
        }
        else if(name_len<=ADECODER_MAX_LANG_CHAR_SIZE)
        {
            for(i=0; i<ADECODER_MAX_LANG_CHAR_SIZE-name_len; i++)
            {
                track->language[ADECODER_MAX_LANG_CHAR_SIZE-2-i] =
                        track->language[ADECODER_MAX_LANG_CHAR_SIZE-name_len-1-i];
            }

            for(i=0; i<name_len-2; i++)
            {
                track->language[i] = track_name[i];
            }
            track->language[name_len-2] = 0x2d;
            //track->language[name_len-1] = 0x2d;
        }
        else
        {
            memcpy(track->language, track_name, (ADECODER_MAX_LANG_CHAR_SIZE-2));
        }
        CDX_LOGD("-- name_len = %d, language = %s", name_len, track->language);

        free(track_name);
        track_name = NULL;
    }

    if (track->type && matroska->num_tracks < MAX_STREAMS) {
        matroska->tracks[matroska->num_tracks++] = track;
    } else {
        free(track);
    }
    return res;
}

/*************************************************************************
* The TRACKS element contains information about the tracks
* that are stored in the SEGMENT, like track type (audio, video, subtitles),
* the used codec, resolution and sample rate.
**************************************************************************/
cdx_int32 matroska_parse_tracks (MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    cdx_uint32 id;

    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            res = AVERROR(EIO);
            break;
        }
        else if (matroska->level_up)
        {
            matroska->level_up--;
            break;
        }

        switch (id)
        {
            // one track within the "all-tracks" header
            case CDX_MATROSKA_ID_TRACKENTRY:
                res = matroska_add_stream(matroska);
                break;

            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up)
        {
            matroska->level_up--;
            break;
        }
    }

    return res;
}

/***************************************************************************
 * The CUES element contains a timestamp-wise index to
 * CLUSTERs, thus it is helpful (but not necessary)for easy and quick seeking
******************************************************************************/
cdx_int32 matroska_parse_index (MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    cdx_uint32 id;
    MatroskaDemuxIndex idx;
    cdx_uint64 pre_index_time = 0;

    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id) {
            res = AVERROR(EIO);
            break;
        } else if (matroska->level_up) {
            matroska->level_up--;
            break;
        }

        if(matroska->index==NULL)
        {
            matroska->index = PHY_MALLOC((1024*64) * sizeof(MatroskaDemuxIndex), 0);
        }

        switch (id)
        {
            // one single index entry ('point')
            case CDX_MATROSKA_ID_POINTENTRY:
                res = ebml_read_master(matroska, &id);
                if (res < 0)
                    break;

                // in the end, we hope to fill one entry with a timestamp,
                // a file position and a tracknum
                idx.pos   = (cdx_uint64) -1;
                idx.time  = (cdx_uint64) -1;
                idx.track = (cdx_uint16) -1;

                while (res == 0)
                {
                    id = ebml_peek_id(matroska, &matroska->level_up);
                    if (!id) {
                        res = AVERROR(EIO);
                        break;
                    } else if (matroska->level_up) {
                        matroska->level_up--;
                        break;
                    }

                    switch (id)
                    {
                        // one single index entry ('point')
                        case CDX_MATROSKA_ID_CUETIME: {
                            cdx_uint64 time;
                            res = ebml_read_uint(matroska, &id, &time);
                            if (res < 0)
                                break;
                            idx.time = time;
                            break;
                        }

                        // position in the file + track to which it belongs
                        case CDX_MATROSKA_ID_CUETRACKPOSITION:
                            res = ebml_read_master(matroska, &id);
                            if (res < 0)
                                break;

                            while (res == 0) {
                                id = ebml_peek_id (matroska, &matroska->level_up);
                                if (!id) {
                                    res = AVERROR(EIO);
                                    break;
                                } else if (matroska->level_up) {
                                    matroska->level_up--;
                                    break;
                                }

                                switch (id) {
                                    // track number
                                    case CDX_MATROSKA_ID_CUETRACK: {
                                        cdx_uint64 num;
                                        res = ebml_read_uint(matroska, &id, &num);
                                        if (res < 0)
                                            break;
                                        idx.track = (cdx_uint16)num;
                                        break;
                                    }

                                    // position in file
                                    case CDX_MATROSKA_ID_CUECLUSTERPOSITION: {
                                        cdx_uint64 num;
                                        res = ebml_read_uint(matroska, &id, &num);
                                        if (res < 0)
                                            break;
                                        idx.pos = num+matroska->segment_start;
                                        break;
                                    }

                                    default:
                                        res = ebml_read_skip(matroska);
                                        break;
                                }

                                if (matroska->level_up) {
                                    matroska->level_up--;
                                    break;
                                }
                            }

                            break;

                        default:
                            res = ebml_read_skip(matroska);
                            break;
                    }

                    if (matroska->level_up) {
                        matroska->level_up--;
                        break;
                    }
                }

                // so let's see if we got what we wanted
                if(matroska->index && matroska->num_indexes<1024*64)
                {
                    if (idx.pos != (cdx_uint64) -1 &&
                        idx.time != (cdx_uint64) -1 &&
                        idx.track != (cdx_uint16) -1)
                    {
                        if((idx.time == 0) || ((idx.time-pre_index_time) > 500))
                        {
                            pre_index_time = idx.time;
                            matroska->index[matroska->num_indexes] = idx;
                            matroska->num_indexes++;
                        }
                    }
                }
                break;

            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up) {
            matroska->level_up--;
            break;
        }
    }

    return res;
}

cdx_int32 matroska_index_init (struct CdxMkvParser *p)
{
    MatroskaDemuxContext *matroska = p->priv_data;
    AVIndexEntry *entries;
    AVStream *st;
    cdx_int32 i;
    cdx_int32 distance = 1;
    cdx_int32 distance_cnt;
    cdx_int32 num_indexes = 0;
    cdx_uint32 TrackNum = matroska->tracks[matroska->VideoTrackIndex]->num;

    for (i=0; i<matroska->num_indexes; i++) {
        if(matroska->index[i].track == TrackNum)
        {
            num_indexes ++;
        }
    }

    if(num_indexes == 0)
    {
        return -1;
    }

    while( num_indexes > (1024*16*distance) )
        distance++;
    entries = PHY_MALLOC(((num_indexes/distance) + 1) * sizeof(AVIndexEntry), 0);
    if(!entries)
        return -1;

    p->hasIndex = 1;
    st = matroska->streams[matroska->VideoStreamIndex];
    st->index_entries= entries;

    distance_cnt = distance;
    for (i=0; i<matroska->num_indexes; i++)
    {
        if(matroska->index[i].track == TrackNum)
        {
            if(distance_cnt==distance)
            {
                AVIndexEntry *ie;
                distance_cnt = 1;
                ie= &entries[st->nb_index_entries++];
                ie->pos = matroska->index[i].pos;
                ie->timestamp = matroska->index[i].time * matroska->time_scale/AV_TIME_BASE;
            }
            else
            {
                distance_cnt++;
            }
        }
    }
    return 0;
}

/**********************************************************************************
* The SEEKHEAD element contains a list of positions of Level 1 elements
* in the SEGMENT.
*************************************************************************************/
cdx_int32 matroska_parse_seekhead (MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    cdx_uint32 id;

    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id) {
            res = AVERROR(EIO);
            break;
        } else if (matroska->level_up) {
            matroska->level_up--;
            break;
        }

        switch (id)
        {
            case CDX_MATROSKA_ID_SEEKENTRY:
            {
                cdx_uint32 seek_id = 0, peek_id_cache = 0;
                cdx_uint64 seek_pos = (cdx_uint64) -1, t;

                if ((res = ebml_read_master(matroska, &id)) < 0)
                    break;

                while (res == 0)
                {
                    id = ebml_peek_id(matroska, &matroska->level_up);
                    if (!id) {
                        res = AVERROR(EIO);
                        break;
                    } else if (matroska->level_up) {
                        matroska->level_up--;
                        break;
                    }

                    switch (id)
                    {
                        case CDX_MATROSKA_ID_SEEKID:
                            res = ebml_read_uint(matroska, &id, &t);
                            seek_id = (cdx_uint32)t;
                            break;

                        case CDX_MATROSKA_ID_SEEKPOSITION:
                            res = ebml_read_uint(matroska, &id, &seek_pos);
                            break;

                        default:
                            res = ebml_read_skip(matroska);
                            break;
                    }

                    if (matroska->level_up)
                    {
                        matroska->level_up--;
                        break;
                    }
                }

                if (!seek_id || seek_pos == (cdx_uint64) -1)
                {
                    break;
                }

                switch (seek_id)
                {
                    case CDX_MATROSKA_ID_CUES:
                    case CDX_MATROSKA_ID_TAGS:
                    {
                        cdx_uint32 level_up = matroska->level_up;
                        cdx_uint64 before_pos;
                        cdx_uint64 length;
                        MatroskaLevel level;

                        /* remember the peeked ID and the current position */
                        peek_id_cache = matroska->peek_id;
                        before_pos = CdxStreamTell(matroska->fp);

                        // seek
                        res = ebml_read_seek(matroska, seek_pos + matroska->segment_start);
                        if ( res < 0)
                            goto finish;

                        // we don't want to lose our seekhead level, so we add a dummy.
                        // This is a crude hack.
                        if (matroska->num_levels == EBML_MAX_DEPTH) {
                            return AVERROR_UNKNOWN;
                        }

                        level.start = 0;
                        level.length = (cdx_uint64)-1;
                        matroska->levels[matroska->num_levels] = level;
                        matroska->num_levels++;

                        // check ID
                        id = ebml_peek_id (matroska, &matroska->level_up);
                        if (!id)
                            goto finish_dummy_level;
                        if (id != seek_id) {
                            goto finish_dummy_level;
                        }

                        // read master + parse
                        if ((res = ebml_read_master(matroska, &id)) < 0)
                            goto finish_dummy_level;
                        switch (id)
                        {
                            case CDX_MATROSKA_ID_CUES:
                                res = matroska_parse_index(matroska);
                                if (matroska->num_indexes || !res || FEOF(matroska)) {
                                    matroska->index_parsed = 1;
                                    res = 0;
                                }
                                break;
                            case CDX_MATROSKA_ID_TAGS:
                                res = ebml_read_skip(matroska);
                                if (!res || FEOF(matroska)) {
                                    res = 0;
                                }
                                break;
                        }

                        // remove dummy level
                    finish_dummy_level:
                        while (matroska->num_levels) {
                            matroska->num_levels--;
                            length = matroska->levels[matroska->num_levels].length;
                            if (length == (cdx_uint64)-1)
                               break;
                        }

                    finish:
                        // seek back
                        if ((res = ebml_read_seek(matroska, before_pos)) < 0)
                            return res;
                        matroska->peek_id = peek_id_cache;
                        matroska->level_up = level_up;
                        break;
                    }

                    default:
                        break;
                }
                break;
            }

            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up) {
            matroska->level_up--;
            break;
        }
    }

    return res;
}

#if OGG_HEADER
static int split_xiph_header(unsigned char *codec_priv, int codec_priv_size,
                        unsigned char **extradata, int *extradata_size)
{
    #define AV_RN16(a) (*((unsigned short *) (a)))

    unsigned char  *header_start[3];
    unsigned int header_len[3];
    int i;
    int overall_len;
    unsigned char  *OggHeader;
    unsigned char  lacing_fill, lacing_fill_0, lacing_fill_1, lacing_fill_2;

    if (codec_priv_size >= 6 && AV_RN16(codec_priv) == 30) {

        overall_len = 6;
        for (i=0; i<3; i++) {
            header_len[i] = AV_RN16(codec_priv);
            codec_priv += 2;
            header_start[i] = codec_priv;
            codec_priv += header_len[i];
            if (overall_len > codec_priv_size - (int)header_len[i])
                return -1;
            overall_len += header_len[i];
        }
    } else if (codec_priv_size >= 3 && codec_priv[0] == 2) {
        overall_len = 3;
        codec_priv++;
        for (i=0; i<2; i++, codec_priv++) {
            header_len[i] = 0;
            for (; overall_len < codec_priv_size && *codec_priv==0xff; codec_priv++) {
                header_len[i] += 0xff;
                overall_len   += 0xff + 1;
            }
            header_len[i] += *codec_priv;
            overall_len   += *codec_priv;
            if (overall_len > codec_priv_size)
                return -1;
        }
        header_len[2] = codec_priv_size - overall_len;
        header_start[0] = codec_priv;
        header_start[1] = header_start[0] + header_len[0];
        header_start[2] = header_start[1] + header_len[1];
    } else {
        return -1;
    }

    for(lacing_fill_0=0; lacing_fill_0*0xff<header_len[0]; lacing_fill_0++);
    for(lacing_fill_1=0; lacing_fill_1*0xff<header_len[1]; lacing_fill_1++);
    for(lacing_fill_2=0; lacing_fill_2*0xff<header_len[2]; lacing_fill_2++);

    *extradata_size =  27 + lacing_fill_0 + header_len[0]
                       +  27 + lacing_fill_1 + header_len[1]
                          + lacing_fill_2 + header_len[2];
    OggHeader = *extradata = PHY_MALLOC(*extradata_size, 0);
    if(OggHeader == NULL)
        return AVERROR(ENOMEM);
    for(i=0; i<26; i++)
    {
       OggHeader[i] = 0;
    }
    OggHeader[0] = 0x4f; OggHeader[1] = 0x67;  OggHeader[2] = 0x67;  OggHeader[3] = 0x53;
    OggHeader[5] = 0x2;
    lacing_fill = lacing_fill_0;
    OggHeader[26] = lacing_fill;
    OggHeader += 27;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[0] - (lacing_fill-1)*0xff;
        }
    }
    OggHeader += lacing_fill;

    memcpy(OggHeader, header_start[0], header_len[0]);
    OggHeader += header_len[0];

    for(i=0; i<26; i++)
    {
       OggHeader[i] = 0;
    }
    OggHeader[0] = 0x4f; OggHeader[1] = 0x67;  OggHeader[2] = 0x67;  OggHeader[3] = 0x53;
    OggHeader[18] = 0x1;
    OggHeader[26] = lacing_fill_1 + lacing_fill_2;
    OggHeader += 27;
    lacing_fill = lacing_fill_1;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[1] - (lacing_fill-1)*0xff;
        }
    }
    OggHeader += lacing_fill;

    lacing_fill = lacing_fill_2;
    for(i=0; i<lacing_fill; i++)
    {
        if(i!=(lacing_fill-1))
        {
            OggHeader[i] = 0xff;
        }
        else
        {
            OggHeader[i] = header_len[2] - lacing_fill*0xff - 1;
        }
    }
    OggHeader += lacing_fill;

    memcpy(OggHeader, header_start[1], header_len[1]);
    OggHeader += header_len[1];
    memcpy(OggHeader, header_start[2], header_len[2]);
    OggHeader += header_len[2];

    return 0;
}
#endif

static int mkvParseOggPrivData(unsigned char *codec_priv, int codec_priv_size,
                        unsigned char **extradata, int *extradata_size)
{
    if(codec_priv[0] != 0x02)
    {
        CDX_LOGW("--- ogg priv data error!");
        return -1;
    }

    // codecInfo starts with two lengths, len1 (0x01 packet) and len2 (0x03 packet), that are
    // "Xiph-style-lacing encoded"...
    int offset = 0;
    int len1 = codec_priv[1];  // 0x01 packet length
    int len2;

    if(codec_priv[2] != 0xff)
        len2 = codec_priv[2];  // 0x03 packet length
    else
    {
        len2 = codec_priv[2] + codec_priv[3];  // 0x03 packet length
        offset ++;
    }

    int len3 = codec_priv_size-len1-len2-3 -offset;
    CDX_LOGD("len1 = %x, len2:%x", len1, len2);

    // we only need 0x01 and 0x05 packet in etradata
    CDX_CHECK(codec_priv_size > 3+len1+len2);
    *extradata_size = codec_priv_size-3-len2 -offset;
    *extradata = malloc(*extradata_size);
    if(*extradata == NULL)
    {
        return -1;
    }

    if(codec_priv[3+offset] != 0x01)
    {
        CDX_LOGW("--- ogg priv data error! first packet is not start with 0x01");
        return -1;
    }

    if(codec_priv[3 + len1 +offset] != 0x03)
    {
        CDX_LOGW("--- second packet in priv data is not started with 0x03");
        return -1;
    }

    if(codec_priv[3 + len1 + len2 + offset] != 0x05)
    {
        CDX_LOGW("--- third packet in priv data is not started with 0x05");
        return -1;
    }
    memcpy(*extradata, &codec_priv[3 + offset], len1);
    memcpy(*extradata+len1, &codec_priv[3+len1+len2 + offset], len3);

    return 0;
}
/**************************************
 * read EMBL and segment
 *****************************************/
cdx_int32 matroska_read_header (struct CdxMkvParser *p)
{
    MatroskaDemuxContext *matroska = p->priv_data;
    cdx_int32 version, last_level, res = 0;
    cdx_uint32 id;
    int segment_loop_flag = 0;
    //int v=0, a=0, s=0;

    // First read the EBML header.
    if ((res = ebml_read_header(matroska, &version)) < 0)
        return res;

    if(res == AVERROR_NOFMT)
    {
        return AVERROR_NOFMT;
    }
    if (version > 2)
    {
        return AVERROR_NOFMT;
    }

    // The next thing is a segment.
    while (1)
    {
        id = ebml_peek_id(matroska, &last_level);
        if (!id)
            return AVERROR(EIO);
        if (id == CDX_MATROSKA_ID_SEGMENT)
            break;

        if ((res = ebml_read_skip(matroska)) < 0)
            return res;
    }

    // We now have a Matroska segment. Seeks are from the beginning of the segment,
    // after the segment ID/length.
    if ((res = ebml_read_master(matroska, &id)) < 0)
        return res;
    matroska->segment_start = CdxStreamTell(matroska->fp);
    CDX_LOGD("segment strat pos = %lld", matroska->segment_start);

    matroska->time_scale = 1000000;
    // we've found our segment, start reading the different contents in here
    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            CDX_LOGW("--- error, id=%d", id);
            res = AVERROR(EIO);
            break;
        }
        else if (matroska->level_up)
        {
            matroska->level_up--;
            //can't break when the segment information has not started parsing
            if(segment_loop_flag != 0)
            {
                break;
            }
            else
            {
                segment_loop_flag++;
            }
        }

        switch (id)
        {
            // stream info
            case CDX_MATROSKA_ID_INFO:
            {
                CDX_LOGD("segment info");
                res = ebml_read_master(matroska, &id);
                if (res < 0)
                    break;
                res = matroska_parse_info(matroska);
                break;
            }

            // track info headers
            case CDX_MATROSKA_ID_TRACKS:
            {
                CDX_LOGD("track");
                res = ebml_read_master(matroska, &id);
                if (res < 0)
                    break;
                res = matroska_parse_tracks(matroska);
                break;
            }

            // stream index, used for seeking
            case CDX_MATROSKA_ID_CUES:
            {
                if (!matroska->index_parsed) {
                    res = ebml_read_master(matroska, &id);
                    if (res < 0)
                        break;
                    res = matroska_parse_index(matroska);
                } else
                    res = ebml_read_skip(matroska);
                break;
            }

            // file index (if seekable, seek to Cues/Tags to parse it)
            case CDX_MATROSKA_ID_SEEKHEAD:
            {
                res = ebml_read_master(matroska, &id);
                if (res < 0)
                    break;
                res = matroska_parse_seekhead(matroska);
                break;
            }

            case CDX_MATROSKA_ID_CLUSTER: {
                // Do not read the master -
                // this will be done in the next call to matroska_read_packet.
                res = 1;
                break;
            }

            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up) {
            matroska->level_up--;
            break;
        }
    }

    // Have we found a cluster?
    if (ebml_peek_id(matroska, NULL) != CDX_MATROSKA_ID_CLUSTER)
    {
        int tmp = MkvResyncWithoutSeekBack(p);
        if(tmp < 0)
        {
            if(tmp == -2)
            {
                CDX_LOGE("resync io error");
                return AVERROR_IO;
            }

            if(FEOF(matroska))
            {
                return AVERROR_EOS;
            }
            else
            {
                CDX_LOGE("resync io error");
                return AVERROR_IO;
            }
        }
    }

    {
        cdx_int32 i, j;
        MatroskaTrack *track;
        AVStream *st;
#define max_vs_search 20
#define max_as_search 24
        enum CodecID v_codec_id[max_vs_search] = {
            CODEC_ID_H265,
            CODEC_ID_H264,
            CODEC_ID_MPEG4,
            CODEC_ID_DIVX4,
            CODEC_ID_DIVX5,
            CODEC_ID_MPEG2VIDEO,
            CODEC_ID_MPEG1VIDEO,
            CODEC_ID_MSMPEG4V3,
            CODEC_ID_RV10,
            CODEC_ID_RV20,
            CODEC_ID_RV30,
            CODEC_ID_RV40,
            CODEC_ID_WMV3,
            CODEC_ID_WMV2,
            CODEC_ID_WMV1,
            CODEC_ID_VC1,
            CODEC_ID_VP8,
            CODEC_ID_VP9,
            CODEC_ID_MSMPEG4V2,
            CODEC_ID_MJPEG
        };
        enum CodecID a_codec_id[max_as_search] = {
            CODEC_ID_MP3,
            CODEC_ID_MP2,
            CODEC_ID_AAC,
            CODEC_ID_DTS,
            CODEC_ID_AC3,
            CODEC_ID_WMAV1,
            CODEC_ID_WMAV2,
            CODEC_ID_WMAPRO,
            CODEC_ID_FLAC,
            CODEC_ID_APE,
            CODEC_ID_COOK,
            CODEC_ID_ATRAC3,
            CODEC_ID_VORBIS,
            CODEC_ID_SIPRO,
            //CODEC_ID_RA_288,
            CODEC_ID_PCM_S32LE,
            CODEC_ID_PCM_S24LE,
            CODEC_ID_PCM_S16LE,
            CODEC_ID_PCM_MULAW,
            CODEC_ID_PCM_ALAW,
            CODEC_ID_ADPCM_MS,
            CODEC_ID_ADPCM_IMA_WAV,
            CODEC_ID_PCM_U16LE,
            CODEC_ID_ALAC,
            CODEC_ID_OPUS,
        };

        for (i = 0; i < matroska->num_tracks; i++)
        {
            enum CodecID codec_id = CODEC_ID_NONE;
            cdx_uint8 *extradata = NULL;
            cdx_int32 extradata_size = 0;
            cdx_int32 extradata_offset = 0;
            cdx_uint32 vfw_fourcc;
            track = matroska->tracks[i];
            track->stream_index = -1;

            codec_id = track->codec_id;
            vfw_fourcc = 0;

            // Set the FourCC from the CodecID.
            //This is the MS compatibility mode which stores a BITMAPINFOHEADER in the CodecPrivate.
            if ((codec_id == CODEC_ID_V_UNKNOWN) &&
                (track->codec_priv_size >= 40) &&
                (track->codec_priv != NULL))
            {
                //if(track->codec_priv_size < 40)
                //{
                //    CDX_LOGW("-- invalid BITMAPINFOHEADER");
                //}
                //else
                {
                    /*********************************************************
                            // V_MS/VFW/FOURCC, the real extradata is after the structure below
                            // and the biCompression is the real codec ID
                            typedef struct
                    ATTR_PACKED
                    {
                        uint32_t   biSize;
                        uint32_t   biWidth;
                        uint32_t   biHeight;
                        uint16_t   biPlanes;
                        uint16_t   biBitCount;
                        uint32_t   biCompression;
                        uint32_t   biSizeImage;
                        uint32_t   biXPelsPerMeter;
                        uint32_t   biYPelsPerMeter;
                        uint32_t   biClrUsed;
                        uint32_t   biClrImportant;
                    } BITMAPINFOHEADER;
                            ***********************************************************/
                    MatroskaVideoTrack *vtrack = (MatroskaVideoTrack *) track;

                    // Offset of biCompression. Stored in LE.
                    vtrack->fourcc = CDX_AV_RL32(track->codec_priv + 16);
                    CDX_LOGD("--- fourcc=%x", vtrack->fourcc);
                    track->codec_id = codec_id = codec_get_id(codec_bmp_tags, vtrack->fourcc);
                    matroska->vfw_fourcc = 1;
                    vfw_fourcc = 1;

                    extradata_offset = 40;
                    track->codec_priv_size -= extradata_offset;
                    extradata = PHY_MALLOC(track->codec_priv_size, 0);
                    if(extradata == NULL)
                        return AVERROR(ENOMEM);
                    extradata_size = track->codec_priv_size;
                    memcpy(extradata, track->codec_priv+extradata_offset, track->codec_priv_size);
                }
            }

            // This is the MS compatibility mode which stores a WAVEFORMATEX in the CodecPrivate.
            else if ((codec_id == CODEC_ID_A_UNKNOWN) &&
                     (track->codec_priv_size >= 18) &&
                     (track->codec_priv != NULL))
            {
                cdx_uint16 tag;
                //int ret;

                /************************************
                typedef struct
                {
                  WORD    wFormatTag;
                  WORD    nChannels;
                  DWORD  nSamplesPerSec;
                  DWORD  nAvgBytesPerSec;
                  WORD    nBlockAlign;
                  WORD    wBitsPerSample;
                  WORD    cbSize;
                } WAVEFORMATEX; *PWAVEFORMATEX;
                *************************************/
                // Offset of wFormatTag. Stored in LE.
                tag = CDX_AV_RL16(track->codec_priv);
                track->codec_id = codec_id = codec_get_id(codec_wav_tags, tag);

                MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *) track;

                audiotrack->tag         = tag;
                audiotrack->channels    = CDX_AV_RL16(track->codec_priv+2);
                audiotrack->samplerate  = CDX_AV_RL32(track->codec_priv+4);
                // it is bytes per seconds, so we should x 8
                audiotrack->bitrate     = (CDX_AV_RL32(track->codec_priv+8))*8;
                audiotrack->block_align = CDX_AV_RL16(track->codec_priv+12);
                audiotrack->bitdepth    = CDX_AV_RL16(track->codec_priv+14);
                int cbSize = CDX_AV_RL16(track->codec_priv+16);
                if(cbSize >= 22 && tag == 0xfffe)
                {
                    //* TODO
                    CDX_LOGW("-- we should deal it");
                }

                int size = track->codec_priv_size - 18;
                track->codec_priv_size = (size<cbSize) ? size: cbSize;

                extradata_offset = 18;
                extradata = PHY_MALLOC(track->codec_priv_size, 0);
                if(extradata == NULL)
                    return AVERROR(ENOMEM);
                extradata_size = track->codec_priv_size;
                memcpy(extradata, track->codec_priv+extradata_offset, track->codec_priv_size);

            }

            else if (codec_id == CODEC_ID_AAC && !track->codec_priv_size)
            {
                MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *) track;
                cdx_int32 profile = track->codec_id_addtion & 0xff;
                cdx_int32 sri = matroska_aac_sri(audiotrack->internal_samplerate);
                extradata = PHY_MALLOC(5, 0);
                if (extradata == NULL)
                    return AVERROR(ENOMEM);
                extradata[0] = (profile << 3) | ((sri&0x0E) >> 1);
                extradata[1] = ((sri&0x01) << 7) | (audiotrack->channels<<3);
                if (track->codec_id_addtion & 0x100)
                {
                    sri = matroska_aac_sri(audiotrack->samplerate);
                    extradata[2] = 0x56;
                    extradata[3] = 0xE5;
                    extradata[4] = 0x80 | (sri<<3);
                    extradata_size = 5;
                }
                else
                {
                    extradata_size = 2;
                }
            }

            else if (codec_id == CODEC_ID_TTA)
            {
                cdx_int32 pcm_cnt;
                MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *) track;
                extradata_size = 30;
                extradata = PHY_MALLOC(extradata_size, 0);
                if (extradata == NULL)
                    return AVERROR(ENOMEM);
                memset(extradata, 0, extradata_size);

                extradata[0] = 'T'; extradata[1] = 'T'; extradata[2] = 'A'; extradata[3] = '1';
                extradata[4] = 1; extradata[5] = 0;
                extradata[6] = audiotrack->channels & 0xff;
                extradata[7] = (audiotrack->channels>>8) & 0xff;
                extradata[8] = audiotrack->bitdepth & 0xff;
                extradata[9] = (audiotrack->channels>>8) & 0xff;
                extradata[10] = audiotrack->samplerate & 0xff;
                extradata[11] = (audiotrack->samplerate>>8) & 0xff;
                extradata[12] = (audiotrack->samplerate>>16) & 0xff;
                extradata[13] = (audiotrack->samplerate>>24) & 0xff;
                pcm_cnt = (cdx_int32)(matroska->segment_duration * audiotrack->samplerate);
                extradata[14] = pcm_cnt & 0xff; extradata[15] = (pcm_cnt>>8) & 0xff;
                extradata[16] = (pcm_cnt>>16) & 0xff; extradata[17] = (pcm_cnt>>24) & 0xff;
            }

            else if (codec_id == CODEC_ID_RA_144)
            {
                MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *)track;
                audiotrack->samplerate = 8000;
                audiotrack->channels = 1;
            }

            else if (codec_id == CODEC_ID_RA_288 ||
                     codec_id == CODEC_ID_COOK ||
                     codec_id == CODEC_ID_ATRAC3 ||
                     codec_id == CODEC_ID_SIPRO)
            {
                MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *)track;

                audiotrack->coded_framesize = (track->codec_priv[24]<<24) |
                                              (track->codec_priv[25]<<16) |
                                              (track->codec_priv[26]<<8) |
                                              (track->codec_priv[27]);

                audiotrack->sub_packet_h    = (track->codec_priv[40]<<8) | (track->codec_priv[41]);
                audiotrack->frame_size      = (track->codec_priv[42]<<8) | (track->codec_priv[43]);
                if(audiotrack->frame_size==0)
                {
                    return AVERROR(EIO);
                }
                audiotrack->sub_packet_size = (track->codec_priv[44]<<8) | (track->codec_priv[45]);
                if(audiotrack->sub_packet_size==0)
                {
                    return AVERROR(EIO);
                }

                if (codec_id == CODEC_ID_SIPRO) {
                    audiotrack->block_align = 0;
                    track->codec_priv_size = 0;
                    if(audiotrack->sub_packet_h != 6)
                    {
                        continue;
                    }
                }
                else if (codec_id == CODEC_ID_RA_288)
                {
                    audiotrack->block_align = audiotrack->coded_framesize;
                    track->codec_priv_size = 0;
                }
                else
                {
                    audiotrack->block_align = audiotrack->sub_packet_size;
                    extradata_offset = 78;
                    track->codec_priv_size -= extradata_offset;
                }

                extradata = PHY_MALLOC(track->codec_priv_size+32, 0);
                if(extradata == NULL)
                    return AVERROR(ENOMEM);
                extradata_size = track->codec_priv_size+32;

                extradata[0] = 0xff;
                extradata[1] = 0xff;
                extradata[2] = 0xff;
                extradata[3] = 0xff;
                extradata[4] = (audiotrack->samplerate>> 0) & 0xff;
                extradata[5] = (audiotrack->samplerate>> 8) & 0xff;
                extradata[6] = (audiotrack->samplerate>>16) & 0xff;
                extradata[7] = (audiotrack->samplerate>>24) & 0xff;
                if(codec_id == CODEC_ID_SIPRO)
                {
                    extradata[8] = extradata[9] = extradata[10] = extradata[11] = 0;
                }
                else
                {
                    extradata[8] = (audiotrack->samplerate>> 0) & 0xff;
                    extradata[9] = (audiotrack->samplerate>> 8) & 0xff;
                    extradata[10] = (audiotrack->samplerate>>16) & 0xff;
                    extradata[11] = (audiotrack->samplerate>>24) & 0xff;
                }
                extradata[12] = (audiotrack->bitdepth>>0) & 0xff;
                extradata[13] = (audiotrack->bitdepth>>8) & 0xff;
                extradata[14] = (audiotrack->channels>>0) & 0xff;
                extradata[15] = (audiotrack->channels>>8) & 0xff;
                extradata[16] = 100;
                extradata[17] = 0;
                extradata[18] = track->codec_priv[23];
                extradata[19] = track->codec_priv[22];
                extradata[20] = (audiotrack->block_align>> 0) & 0xff;
                extradata[21] = (audiotrack->block_align>> 8) & 0xff;
                extradata[22] = (audiotrack->block_align>>16) & 0xff;
                extradata[23] = (audiotrack->block_align>>24) & 0xff;
                extradata[24] = (audiotrack->frame_size>> 0) & 0xff;
                extradata[25] = (audiotrack->frame_size>> 8) & 0xff;
                extradata[26] = (audiotrack->frame_size>>16) & 0xff;
                extradata[27] = (audiotrack->frame_size>>24) & 0xff;
                extradata[28] = (track->codec_priv_size>> 0) & 0xff;
                extradata[29] = (track->codec_priv_size>> 8) & 0xff;
                extradata[30] = (track->codec_priv_size>>16) & 0xff;
                extradata[31] = (track->codec_priv_size>>24) & 0xff;

                if(track->codec_priv_size)
                    memcpy(extradata+32, track->codec_priv+extradata_offset,
                           track->codec_priv_size);
            }

            if(codec_id == CODEC_ID_VORBIS)
            {
                #if OGG_HEADER
                // this code got the ogg header in CDX1.0, not uesd now
                if(split_xiph_header(track->codec_priv, track->codec_priv_size,
                    &extradata, &extradata_size)==-1)
                {
                    codec_id = CODEC_ID_NONE;
                    continue;
                }
                #endif

                if(mkvParseOggPrivData(track->codec_priv, track->codec_priv_size,
                       &extradata, &extradata_size) < 0)
                {
                    codec_id = CODEC_ID_NONE;
                    continue;
                }

                matroska->ogg_pageno = 2;
                matroska->ogg_uflag = 0;
            }

            if(codec_id == CODEC_ID_WMV3 || codec_id == CODEC_ID_WMV2)
            {
                extradata = PHY_MALLOC(4, 0);
                if(extradata == NULL)
                    return AVERROR(ENOMEM);
                extradata_size = 4;
                memcpy(extradata, track->codec_priv+0x28, 4);
            }

            if(codec_id == CODEC_ID_VC1)
            {
                if(track->codec_priv_size > 0x28)
                {
                    extradata = PHY_MALLOC(track->codec_priv_size-0x28, 0);
                    if(extradata == NULL)
                        return AVERROR(ENOMEM);
                    extradata_size = track->codec_priv_size-0x28;
                    memcpy(extradata, track->codec_priv+0x28, track->codec_priv_size-0x28);
                }
            }

            if(codec_id == CODEC_ID_WMAV1 || codec_id == CODEC_ID_WMAV2)
            {
                CDX_LOGW("---****** care, it wma *************");
            }
#if 0
            if(codec_id == CODEC_ID_WMAV1 || codec_id == CODEC_ID_WMAV2)
            {
                extradata = PHY_MALLOC(28, 0);
                if(extradata == NULL)
                   return AVERROR(ENOMEM);
                extradata_size = 28;
                memcpy(extradata, track->codec_priv, 28);
            }

            if(codec_id == CODEC_ID_WMAPRO)
            {
                extradata = PHY_MALLOC(28, 0);
                if(extradata == NULL)
                   return AVERROR(ENOMEM);
                extradata_size = 28;
                memcpy(extradata, track->codec_priv, 28);
                *(extradata+22) = *(track->codec_priv+32);
                *(extradata+23) = *(track->codec_priv+33);
            }
#endif
            // Apply some sanity checks.
            if (codec_id == CODEC_ID_NONE)
            {
                continue;
            }

            track->stream_index = matroska->num_streams;

            matroska->num_streams++;

            if (matroska->nb_streams >= MAX_STREAMS)
            {
                if(extradata)
                {
                    free(extradata);
                }
                return AVERROR(ENOMEM);
            }

            st = malloc(sizeof(AVStream));
            if(st==NULL) return AVERROR(ENOMEM);
            memset(st, 0, sizeof(AVStream));

            st->index = matroska->nb_streams;
            st->id = track->stream_index;

            // 64 bit pts in ns
            st->time_scale = (cdx_int32)(matroska->time_scale*track->time_scale);
            CDX_LOGV("--------------matroska->time_scale:%lld track->time_scale:%f\n",
                     matroska->time_scale,track->time_scale);

            if (strcmp((const char *)track->language, "und")){
                strncpy((char *)st->language, (const char *)track->language,31);
                st->language[31] = '\0';
            }

            if (track->flags & MATROSKA_TRACK_DEFAULT)
                st->disposition |= AV_DISPOSITION_DEFAULT;

            matroska->streams[matroska->nb_streams++] = st;

            st->sample_aspect_ratio = 0;
            st->bit_rate = 0;
            st->codec_id = codec_id;
            st->default_duration = track->default_duration;

            if(extradata)
            {
                st->extradata = (char*)extradata;
                st->extradata_size = extradata_size;
            }
            else if(vfw_fourcc == 0 && track->codec_priv && track->codec_priv_size > 0)
            {
                st->extradata = PHY_MALLOC(track->codec_priv_size, 0);
                if(st->extradata == NULL)
                    return AVERROR(ENOMEM);
                st->extradata_size = track->codec_priv_size;
                memcpy(st->extradata, track->codec_priv, track->codec_priv_size);
            }

            if (track->type == MATROSKA_TRACK_TYPE_VIDEO)
            {
                MatroskaVideoTrack *videotrack = (MatroskaVideoTrack *)track;

                st->codec_type = CODEC_TYPE_VIDEO;
                st->width = videotrack->pixel_width;
                st->height = videotrack->pixel_height;
                if (videotrack->display_width == 0)
                    videotrack->display_width= videotrack->pixel_width;
                if (videotrack->display_height == 0)
                    videotrack->display_height= videotrack->pixel_height;
                st->sample_aspect_ratio = (st->height * videotrack->display_width)*1000 /
                                          (st->width * videotrack->display_height);

                if(track->codec_priv)
                    track->block_size_type = (track->codec_priv[4]&0x3)+1;
                matroska->hasVideo = 1;

                //matroska->v2st[v] = i;
                //v++;
            }
            else if (track->type == MATROSKA_TRACK_TYPE_AUDIO)
            {
                MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *)track;

                st->codec_type = CODEC_TYPE_AUDIO;
                st->sample_rate = audiotrack->samplerate;
                st->channels = audiotrack->channels;
                st->block_align = audiotrack->block_align;
                matroska->hasAudio = 1;

                //matroska->a2st[a] = i;
                //a++;
            }
            else if (track->type == MATROSKA_TRACK_TYPE_SUBTITLE)
            {
                st->codec_type = CODEC_TYPE_SUBTITLE;
                //matroska->s2st[s] = i;
                //s++;
            }
        }
        res = 0;

        int hasVideo_flag = 0;
        // find the display video stream
        for (j = 0; j<max_vs_search; j++)
        {
            for (i = 0; i < matroska->num_tracks; i++)
            {
                track = matroska->tracks[i];

                if (track->type == MATROSKA_TRACK_TYPE_VIDEO &&
                    track->codec_id == v_codec_id[j])
                {
                    if(!hasVideo_flag)
                    {
                        p->hasVideo = 1;
                        p->VideoStreamIndex = track->stream_index;
                        matroska->VideoStreamIndex = track->stream_index;
                        matroska->VideoTrackIndex = i;
                        hasVideo_flag++;
                    }
                    else
                    {
                        p->hasVideo++;
                        break;
                    }
                }
            }
            if(p->hasVideo)
                break;
        }

        for (i = 0; i < matroska->num_tracks; i++) {
            track = matroska->tracks[i];
            for (j = 0; j<max_as_search; j++)
            {
                if (track->type == MATROSKA_TRACK_TYPE_AUDIO &&
                    track->codec_id == a_codec_id[j])
                {
                    matroska->AudioTrackIndex = i;
                    p->hasAudio = 1;
                    break;
                }
            }
            if(p->hasAudio)
                break;
        }

        for (i = 0; i < matroska->num_tracks; i++)
        {
            track = matroska->tracks[i];

            if (track->type == MATROSKA_TRACK_TYPE_SUBTITLE &&
                (track->codec_id == CODEC_ID_TEXT || track->codec_id == CODEC_ID_SSA ||
                 track->codec_id == CODEC_ID_DVD_SUBTITLE || track->codec_id == CODEC_ID_PGS))
            {
                matroska->SubTitleStream[p->nhasSubTitle] = track->stream_index;
                matroska->SubTitleTrack[p->nhasSubTitle] = i;

                if(track->codec_id == CODEC_ID_TEXT )
                {
                    matroska->SubTitleCodecType[p->nhasSubTitle] = SUBTITLE_CODEC_TXT;
                    matroska->SubTitledata_encode_type[p->nhasSubTitle] = SUBTITLE_TEXT_FORMAT_UTF8;
                }
                else if(track->codec_id == CODEC_ID_SSA)
                {
                    cdx_uint8 *ssa_header =
                                     (cdx_uint8 *)matroska->streams[track->stream_index]->extradata;
                    matroska->SubTitleCodecType[p->nhasSubTitle] = SUBTITLE_CODEC_SSA;
                    matroska->SubTitledata_encode_type[p->nhasSubTitle] = SUBTITLE_TEXT_FORMAT_UTF8;

                    if(ssa_header != 0)
                    {
                        if(ssa_header[0] == 0xfe && ssa_header[1] == 0xff)
                        {
                            matroska->SubTitledata_encode_type[p->nhasSubTitle] =
                                                                      SUBTITLE_TEXT_FORMAT_UTF16BE;
                        }
                        else if(ssa_header[0] == 0xff && ssa_header[1] == 0xfe)
                        {
                            matroska->SubTitledata_encode_type[p->nhasSubTitle] =
                                                                      SUBTITLE_TEXT_FORMAT_UTF16LE;
                        }
                        else if(ssa_header[0] == 0xef &&
                                ssa_header[1] == 0xbb &&
                                ssa_header[2] == 0xbf)
                        {
                            matroska->SubTitledata_encode_type[p->nhasSubTitle] =
                                                                        SUBTITLE_TEXT_FORMAT_UTF8;
                        }
                    }
                }
                //add by yaosen
                else if(track->codec_id == CODEC_ID_DVD_SUBTITLE)
                {
                    matroska->SubTitleStream[p->nhasSubTitle] = track->stream_index;
                    matroska->SubTitleTrack[p->nhasSubTitle] = i;
                    matroska->SubTitleCodecType[p->nhasSubTitle] = SUBTITLE_CODEC_DVDSUB;
                    matroska->SubTitledata_encode_type[p->nhasSubTitle] =
                            SUBTITLE_TEXT_FORMAT_UNKNOWN;
                }
                else if(track->codec_id == CODEC_ID_PGS)
                {
                    matroska->SubTitleCodecType[p->nhasSubTitle] = SUBTITLE_CODEC_PGS;
                    matroska->SubTitledata_encode_type[p->nhasSubTitle] =
                            SUBTITLE_TEXT_FORMAT_UNKNOWN;
                }

                if (strlen((char *)track->language) <= MAX_SUBTITLE_LANG_SIZE)
                {
                    strcpy((char *)matroska->language[p->nhasSubTitle],
                            (const char *)track->language);
                }
                else
                {
                    strncpy((char *)matroska->language[p->nhasSubTitle],
                            (const char *)track->language, MAX_SUBTITLE_LANG_SIZE - 1);
                    matroska->language[p->nhasSubTitle][MAX_SUBTITLE_LANG_SIZE - 1] = '\0';
                }
                p->hasSubTitle = 1;
                p->nhasSubTitle ++;
                if(p->nhasSubTitle>SUBTITLE_STREAM_LIMIT)
                {
                    break;
                }
            }
        }

        if(p->hasSubTitle == 0)
        {
            for (i = 0; i < matroska->num_tracks; i++) {
                track = matroska->tracks[i];

                if (track->type == MATROSKA_TRACK_TYPE_SUBTITLE &&
                    track->codec_id == CODEC_ID_DVD_SUBTITLE)
                {
                    matroska->SubTitleStream[p->nhasSubTitle] = track->stream_index;
                    matroska->SubTitleTrack[p->nhasSubTitle] = i;

                    matroska->SubTitleCodecType[p->nhasSubTitle] = SUBTITLE_CODEC_DVDSUB;
                    matroska->SubTitledata_encode_type[p->nhasSubTitle] =
                                                                   SUBTITLE_TEXT_FORMAT_UNKNOWN;

                    if (strlen((char *)track->language) <= MAX_SUBTITLE_LANG_SIZE)
                    {
                        strcpy((char *)matroska->language[p->nhasSubTitle],
                               (const char *)track->language);
                    }
                    p->hasSubTitle = 1;
                    p->nhasSubTitle ++;
                    if(p->nhasSubTitle>SUBTITLE_STREAM_LIMIT)
                    {
                        break;
                    }
                }
            }
        }
        p->UserSelSubStreamIdx = p->SubTitleStreamIndex
            = matroska->SubTitleStream[0];
        matroska->SubTitleStreamIndex = matroska->SubTitleStream[0];
        matroska->SubTitleTrackIndex = matroska->SubTitleTrack[0];
    }

    if (matroska->index_parsed)
    {
        matroska_index_init (p);
    }
    if(matroska->index)
    {
        PHY_FREE(matroska->index);
        matroska->index = NULL;
    }

    matroska->num_levels_bak = matroska->num_levels;

    return res;
}

cdx_int32 matroska_find_track_by_num (MatroskaDemuxContext *matroska, cdx_int32 num)
{
    cdx_int32 i;

    for (i = 0; i < matroska->num_tracks; i++)
        if (matroska->tracks[i]->num == (cdx_uint32)num)
            return i;
    CDX_LOGE("Invalid track number:%d", num);
    return -1;
}

/*********************************************************
 * Read the next element as binary data.
 * 0 is success, < 0 is failure.
 *********************************************************/
cdx_int32 ebml_read_binary_part (MatroskaDemuxContext *matroska,
                                 cdx_uint32 *id,
                                 cdx_uint8 **binary,
                                 cdx_int32 *size,
                                 cdx_int32 offset)
{
    CdxStreamT *pb = matroska->fp;
    cdx_uint64 rlength;
    cdx_int32 res;
    cdx_uint8  track_num;
    cdx_uint8  flags;

    if(*binary==NULL)
    {
        *binary = PHY_MALLOC((cdx_uint32)(256*1024), 0);
        if (!(*binary)) {
            return AVERROR(ENOMEM);
        }
        matroska->data_size = 256*1024;
    }

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &rlength)) < 0)
        return res;

    *size = (cdx_int32)rlength;

    matroska->sector_align  = CdxStreamTell(pb) & 0x1ff;
    matroska->sector_align += (offset - 0x200);

    if(CdxStreamRead(pb, (*binary+matroska->sector_align), 4) != 4)
    {
        //cdx_uint64 pos = CdxStreamTell(pb);
        return AVERROR(EIO);
    }

    //**< trackNum (1 byte); TimeCode (2 bytes); flags(1 byte)
    track_num = *(*binary+matroska->sector_align);
    flags = (*(*binary+matroska->sector_align+3) & 0x6);

    //**< trackNum is stored in EBML, and it is 1 byte, so it starts with 0x8X
    if(track_num & 0x80)
    {
        track_num &= 0x3f;
        if(matroska->tracks[matroska->VideoTrackIndex]->num == track_num && flags!=0)
        {
            matroska->video_direct_copy = 0;
        }
    }
    else
    {
        matroska->video_direct_copy = 0;
    }

    /* //we can't limit it's illegal when track_num>num_tracks,
       //because some mkv/mka file take a bigger track_num than num_tracks
    if(track_num > matroska->num_tracks)
    {
        CDX_LOGW("Invalid track number:%d", track_num);
        matroska->vDataPos = CdxStreamTell(matroska->fp);

        if(CdxStreamSeekAble(pb))
        {
            CdxStreamSeek(matroska->fp, (*size)-4, SEEK_CUR);
        }
        else
        {
            CdxStreamSkip(pb, (*size)-4);
        }
        return 0;
    }*/

    if(matroska->video_direct_copy &&
       matroska->tracks[matroska->VideoTrackIndex]->num == track_num)
    {
        matroska->vDataPos = CdxStreamTell(matroska->fp);

        if(CdxStreamSeekAble(pb))
        {
            CdxStreamSeek(matroska->fp, (cdx_uint32)(*size)-4, SEEK_CUR);
        }
        else
        {
            CdxStreamSkip(pb, (cdx_uint32)(*size)-4);
        }
    }
    else if(matroska->tracks[matroska->VideoTrackIndex]->num == track_num ||
            matroska->tracks[track_num-1]->type == MATROSKA_TRACK_TYPE_AUDIO ||
           matroska->tracks[track_num-1]->type == MATROSKA_TRACK_TYPE_SUBTITLE )
    {
        if((rlength+offset)> (cdx_uint64)matroska->data_size)
        {
            PHY_FREE(*binary);
            *binary = PHY_MALLOC((cdx_uint32)(rlength+offset), 0);
            if (!(*binary)) {
                return AVERROR(ENOMEM);
            }
            matroska->data_size = rlength+offset;

            if(CdxStreamSeek(matroska->fp, -4, SEEK_CUR) < 0)
            {
                CDX_LOGE("seek failed");
                return -1;
            }
            if(CdxStreamRead(pb, (*binary+matroska->sector_align), 4) != 4)
            {
                //cdx_uint64 pos = CdxStreamTell(pb);
                return AVERROR(EIO);
            }
        }
        if (CdxStreamRead(pb, (cdx_uint8*)(*binary+matroska->sector_align+4),
                          (cdx_uint32)(*size)-4) != (cdx_int32)(*size)-4)
        {
            //cdx_uint64 pos = CdxStreamTell(pb);
            return AVERROR(EIO);
        }
    }
    else
    {
        matroska->vDataPos = CdxStreamTell(matroska->fp);
        if(CdxStreamSeekAble(pb))
        {
            CdxStreamSeek(matroska->fp, (cdx_uint32)(*size)-4, SEEK_CUR);
        }
        else
        {
            CdxStreamSkip(pb, (cdx_uint32)(*size)-4);
        }
    }
    return 0;
}

/*******************************************************
 * Read signed/unsigned "EBML" numbers.
 * Return: number of bytes processed, < 0 on error.
 * XXX: use ebml_read_num().
 ********************************************************/
cdx_int32 matroska_ebmlnum_uint (cdx_uint8  *data,  cdx_uint32  size, cdx_uint64 *num)
{
    cdx_int32 len_mask = 0x80, read = 1, n = 1, num_ffs = 0;
    cdx_uint64 total;

    if ((cdx_int32)(size) <= 0)
        return AVERROR_INVALIDDATA;

    total = data[0];
    while (read <= 8 && !(total & len_mask)) {
        read++;
        len_mask >>= 1;
    }
    if (read > 8)
        return AVERROR_INVALIDDATA;

    if ((total &= (len_mask - 1)) == (cdx_uint64)(len_mask - 1))
        num_ffs++;
    if ((cdx_int32)(size) < read)
        return AVERROR_INVALIDDATA;
    while (n < read) {
        if (data[n] == 0xff)
            num_ffs++;
        total = (total << 8) | data[n];
        n++;
    }

    if (read == num_ffs)
        *num = (cdx_uint64)-1;
    else
        *num = total;

    return read;
}

/*******************************************************
 * Same as above, but signed.
 *******************************************************/
cdx_int32 matroska_ebmlnum_sint (cdx_uint8  *data, cdx_uint32 size, cdx_int64  *num)
{
    cdx_uint64 unum;
    cdx_int32 res;

    // read as unsigned number first
    if ((res = matroska_ebmlnum_uint(data, size, &unum)) < 0)
        return res;

    // make signed (weird way)
    if (unum == (cdx_uint64)-1)
        *num = INT64_MAX;
    else
    {
        cdx_uint64 tmp = 1;
        tmp <<= ((7 * res) - 1);
        *num = unum - (tmp - 1);
    }

    return res;
}

cdx_int32 matroska_parse_block_header(MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    AVStream *st;
    cdx_int16 block_time = 0;
    cdx_uint32 *lace_size = matroska->lace_size;
    cdx_int32 n, flags, laces = 0;
    cdx_uint64 num;
    cdx_int32 stream_index;

    // first byte(s): tracknum
    if ((n = matroska_ebmlnum_uint(matroska->data_parse, matroska->size_parse, &num)) < 0) {
        return res;
    }
    (matroska->data_parse) += n;
    matroska->size_parse -= n;

    // fetch track from num
    matroska->track = matroska_find_track_by_num(matroska, (cdx_int32)num);
    if (matroska->size_parse <= 3 || matroska->track < 0 || matroska->track >= matroska->num_tracks)
    {
        return res;
    }

    matroska->chk_type = matroska->tracks[matroska->track]->type;
    stream_index       = matroska->tracks[matroska->track]->stream_index;

    st = matroska->streams[stream_index];
    if (matroska->blk_duration == AV_NOPTS_VALUE)
    {
        matroska->blk_duration = st->default_duration;
    }
    else
    {
        matroska->blk_duration *= st->time_scale;
    }

    // block_time (relative to cluster time)
    block_time = CDX_AV_RB16(matroska->data_parse);

    matroska->data_parse += 2;
    flags = *(matroska->data_parse)++;
    matroska->size_parse -= 3;
    if (matroska->is_keyframe == -1)
        matroska->is_keyframe = flags & 0x80 ? PKT_FLAG_KEY : 0;

    {
        if(matroska->chk_type == MATROSKA_TRACK_TYPE_AUDIO)
        {
            if((matroska->cluster_time != INT64_MAX) &&
               ((cdx_int64)(matroska->cluster_time + block_time)>=0))
            {
                st->pts = (matroska->cluster_time + block_time)*st->time_scale;
            }
            else
            {
                st->pts = matroska->streams[matroska->VideoStreamIndex]->pts;
            }

            matroska->pkt_dataIndex = stream_index;
        }
        else if(matroska->chk_type == MATROSKA_TRACK_TYPE_VIDEO)
        {
            if(stream_index != matroska->VideoStreamIndex)
            {
                CDX_LOGW("++++ it is a file with multi video streams, do not support");
            }

            if(matroska->tracks[matroska->track]->codec_id == CODEC_ID_RV10 ||
               matroska->tracks[matroska->track]->codec_id == CODEC_ID_RV20 ||
               matroska->tracks[matroska->track]->codec_id == CODEC_ID_RV30 ||
               matroska->tracks[matroska->track]->codec_id == CODEC_ID_RV40)
            {
                if((matroska->cluster_time != INT64_MAX) &&
                   (((cdx_int64)matroska->cluster_time + block_time)>=0))
                {
                    st->pts = (matroska->cluster_time + block_time)*st->time_scale;
                }
                else if((st->pts + block_time*st->time_scale)>=0)
                {
                    st->pts += block_time*st->time_scale;
                }
            }
            else
            {
                if(matroska->is_keyframe == PKT_FLAG_KEY)
                {
                    if((matroska->cluster_time != INT64_MAX) &&
                       (((cdx_int64)matroska->cluster_time + block_time)>=0))
                    {
                        cdx_int64 delta_pts =
                                (matroska->cluster_time + block_time)*st->time_scale - st->pts -
                                matroska->blk_duration;
                        if(matroska->frame_num_between_kframe > 5)
                        {
                            if(delta_pts>0)
                            {
                                matroska->blk_duration +=
                                        ((cdx_int32)delta_pts)/matroska->frame_num_between_kframe;
                            }
                            else if(delta_pts<0)
                            {
                                matroska->blk_duration -=
                                        ((cdx_int32)-delta_pts)/matroska->frame_num_between_kframe;
                            }
                            if(matroska->blk_duration > 16000000&&matroska->blk_duration < 80000000)
                            {
                                st->default_duration = matroska->blk_duration;
                            }
                        }
                        st->pts = (matroska->cluster_time + block_time)*st->time_scale;
                    }
                    else if((st->pts + block_time*st->time_scale)>=0)
                    {
                        st->pts += block_time*st->time_scale;
                    }
                    matroska->frame_num_between_kframe = 1;
                }
                else
                {
                    // if the blocktime is right, we add it to pts directly
                    if(1 || matroska->tracks[matroska->track]->codec_id == CODEC_ID_VP8 ||
                        matroska->tracks[matroska->track]->codec_id == CODEC_ID_VP9    )
                    {
                        if(((cdx_int64)matroska->cluster_time + block_time)>=0)
                        {
                            st->pts = (matroska->cluster_time + block_time)*st->time_scale;
                        }
                        else
                        {
                            st->pts += matroska->blk_duration;
                        }
                    }
                    else
                    {
                        // get the dts, not used now
                        matroska->frame_num_between_kframe++;
                        if(matroska->blk_duration > 16000000&&matroska->blk_duration < 60000000)
                        {
                            st->pts += matroska->blk_duration;
                        }
                        else
                        {
                            st->pts = (matroska->cluster_time + block_time)*st->time_scale;
                            if(st->last_pts > st->pts)
                            {
                                if((st->last_pts - st->pts) > 16000000 &&
                                   (st->last_pts - st->pts) < 60000000)
                                {
                                    st->default_duration = st->last_pts - st->pts;
                                }
                                st->pts = st->last_pts;
                            }
                        }
                    }
                }
            }
            st->last_pts = st->pts;
        }
        else if ( matroska->chk_type == MATROSKA_TRACK_TYPE_SUBTITLE )
        {
            if((matroska->cluster_time != INT64_MAX) &&
               (((cdx_int64)matroska->cluster_time + block_time)>=0))
            {
                st->pts = (matroska->cluster_time + block_time)*st->time_scale;
            }
            else
            {
                st->pts = matroska->streams[matroska->VideoStreamIndex]->pts;
            }
        }
        else
        {
            return res;
        }
    }

    matroska->time_stamp = st->pts/1000;

    if (matroska->skip_to_keyframe)
    {
        if (!matroska->is_keyframe || st != matroska->skip_to_stream) {
            return res;
        }
        matroska->skip_to_keyframe = 0;
    }

    if(matroska->time_stamp_error)
    {
        matroska->time_stamp_offset += (st->pts/1000) - 50000;
        matroska->time_stamp_error = 0;
    }

    //**< flags( 0x80:keyframe, 0x60:lace type,  0x08:invisible)
    switch ((flags & 0x06) >> 1)
    {
        case 0x0: // no lacing
            laces = 1;
            if(!lace_size)
            {
                lace_size = malloc(sizeof(cdx_int32));
                matroska->max_laces = 1;
            }
            memset(lace_size, 0, sizeof(cdx_int32));
            lace_size[0] = matroska->size_parse;
            break;

        case 0x1: // xiph lacing
        case 0x2: // fixed-size lacing
        case 0x3: // EBML lacing
            laces = (*(matroska->data_parse)) + 1;
            matroska->data_parse += 1;
            matroska->size_parse -= 1;
            if(!lace_size)
            {
                lace_size = malloc(laces * sizeof(cdx_int32));
                matroska->max_laces = laces;
            }
            else if(matroska->max_laces < laces)
            {
                free(lace_size);
                lace_size = malloc(laces * sizeof(cdx_int32));
                matroska->max_laces = laces;
            }
            memset(lace_size, 0, laces * sizeof(cdx_int32));

            switch ((flags & 0x06) >> 1)
            {
                case 0x1:  // xiph lacing
                {
                    cdx_uint8 temp;
                    cdx_uint32 total = 0;
                    for (n = 0; res == 0 && n < laces - 1; n++)
                    {
                        while (1)
                        {
                            if (matroska->size_parse == 0) {
                                res = -1;
                                break;
                            }
                            temp = *(matroska->data_parse);
                            lace_size[n] += temp;
                            matroska->data_parse += 1;
                            matroska->size_parse -= 1;
                            if (temp != 0xff)
                                break;
                        }
                        total += lace_size[n];
                    }
                    lace_size[n] = matroska->size_parse - total;
                    break;
                }

                case 0x2: // fixed-size lacing
                    for (n = 0; n < laces; n++)
                        lace_size[n] = matroska->size_parse / laces;
                    break;

                case 0x3:
                {   // EBML lacing
                    cdx_uint32 total;
                    n = matroska_ebmlnum_uint(matroska->data_parse, matroska->size_parse, &num);
                    if (n < 0)
                    {
                        break;
                    }
                    matroska->data_parse += n;
                    matroska->size_parse -= n;
                    total = lace_size[0] = (cdx_uint32)num;
                    for (n = 1; res == 0 && n < laces - 1; n++) {
                        cdx_int64 snum;
                        cdx_int32 r;
                        r = matroska_ebmlnum_sint (matroska->data_parse,
                                                   matroska->size_parse,
                                                   &snum);
                        if (r < 0)
                        {
                            break;
                        }
                        matroska->data_parse += r;
                        matroska->size_parse -= r;
                        lace_size[n] = lace_size[n - 1] + (cdx_uint32)snum;
                        total += lace_size[n];
                    }
                    lace_size[n] = matroska->size_parse - total;
                    break;
                }
            }
            break;
    }

    matroska->lace_size = lace_size;
    matroska->laces = laces;

    matroska->data_rdy = 1;
    matroska->lace_cnt = 0;

    return res;
}

cdx_int32 matroska_parse_blockgroup (MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    cdx_uint32 id;
    cdx_int32 size = 0;

    matroska->is_keyframe = PKT_FLAG_KEY;
    matroska->blk_duration = AV_NOPTS_VALUE;

    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            res = AVERROR(EIO);
            break;
        }
        else if (matroska->level_up)
        {
            matroska->level_up--;
            break;
        }

        switch (id)
        {
            case CDX_MATROSKA_ID_BLOCK:
            {
                size = matroska->data_size;
                res = ebml_read_binary_part(matroska, &id, &matroska->data, &size, DATA_OFFSET);
                break;
            }

            case CDX_MATROSKA_ID_BLOCKDURATION:
            {
                if ((res = ebml_read_uint(matroska, &id, &matroska->blk_duration)) < 0)
                    break;
                break;
            }

            // We've found a reference, so not even the first frame in the lace is a key frame.
            case CDX_MATROSKA_ID_BLOCKREFERENCE:
                matroska->is_keyframe = 0;
            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up)
        {
            matroska->level_up--;
            break;
        }
    }

    if (res)
        return res;

    if (size > 0)
    {
        matroska->data_parse = matroska->data+matroska->sector_align;
        matroska->size_parse = size;
        res = matroska_parse_block_header(matroska);
    }

    return res;
}

/***********************************************************************************
* A CLUSTER contains multimedia data and usually spans over
* a range of a few seconds.
***********************************************************************************/
cdx_int32 matroska_parse_cluster (MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    cdx_uint32 id;
    cdx_int32 size;

    while (res == 0)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            matroska->parsing_cluster = 0;
            res = AVERROR(EIO);
            break;
        }
        else if (matroska->level_up)
        {
            matroska->parsing_cluster = 0;
            matroska->level_up--;
            break;
        }

        switch (id)
        {
            // cluster timecode
            case CDX_MATROSKA_ID_CLUSTERTIMECODE:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                matroska->cluster_time = num;
                if((cdx_int64)matroska->cluster_time * matroska->time_scale >
                   (matroska->segment_duration * AV_TIME_BASE))
                {
                    //matroska->cluster_time = INT64_MAX;
                    CDX_LOGD("care! the cluster time is not start from 0");
                }
                break;
            }

            // a group of blocks inside a cluster
            case CDX_MATROSKA_ID_BLOCKGROUP:
                res = ebml_read_master(matroska, &id);
                if (res < 0)
                    break;
                res = matroska_parse_blockgroup(matroska);
                break;

            case CDX_MATROSKA_ID_SIMPLEBLOCK:
                size = matroska->data_size;
                res = ebml_read_binary_part(matroska, &id, &matroska->data, &size, DATA_OFFSET);
                if (res == 0)
                {
                    matroska->data_parse = matroska->data+matroska->sector_align;
                    matroska->size_parse = size;
                    matroska->is_keyframe = -1;
                    matroska->blk_duration = AV_NOPTS_VALUE;
                    res = matroska_parse_block_header(matroska);
                }
                break;

            default:
                res = ebml_read_skip(matroska);
                break;
        }

        if (matroska->level_up)
        {
            matroska->level_up--;
            matroska->parsing_cluster = 0;
            break;
        }

        if(matroska->data_rdy == 1)
        {
            break;
        }
    }

    return res;
}

/*
 * The interleave table was created with the following properties:
 * -equal contributions from each block
 * -symmetric (if table[i] = j, then table[j] = i), allows in-place interleave
 * -random spacing between lost frames, except
 * -no double losses
 * Solution was generated by a random-walk optimization loop.
 *
 */

/* Loss pattern for a six-block solution
....X.X....X..........X........X....X..X....X........X....X....X....X....X.......X....X...X.....
.X.....X..........X....X...X..X...X...........X...X...X..X...........X.X...X............X.....X.
..X.......X..X......X...X............X...X.....X........X..X.....X....X...X..........X.X....X...
X........X.....X.X........X..X..........X....X......X.......X.X.............X..X.........X...X.X
............X.X.X....X......X...X..X.......X....X............X..X.X.....X.....X.X..X............
...X.X..X..........X.....X.......X....X...X......X.X...X...........X.........X....X.X......X....
*/
static const cdx_int32 RASL_InterleaveTable[RASL_NFRAMES * RASL_NBLOCKS] = {
    63, 22, 44, 90, 4, 81, 6, 31, 86, 58, 36, 11, 68, 39, 73, 53,
    69, 57, 18, 88, 34, 71, 1, 23, 46, 94, 54, 27, 75, 50, 30, 7,
    70, 92, 20, 74, 10, 37, 85, 13, 56, 41, 87, 65, 2, 59, 24, 47,
    79, 93, 29, 89, 52, 15, 26, 95, 40, 17, 9, 45, 60, 76, 62, 0,
    64, 43, 66, 83, 12, 16, 32, 21, 72, 14, 35, 28, 61, 80, 78, 48,
    77, 5, 82, 67, 84, 38, 8, 42, 19, 51, 3, 91, 33, 49, 25, 55
};

static void ra_bitcopy(cdx_uint8 *toPtr, cdx_uint32  ulToBufSize, cdx_uint8  *fromPtr,
                       cdx_int32  bitOffsetTo, cdx_int32 bitOffsetFrom, cdx_int32 numBits)
{
    cdx_uint8 *pToLimit = toPtr + ulToBufSize;
    cdx_int32 offsetFromMod, offsetToMod, numMod, offsetToModLeft, offsetFromModLeft, j, jMax;
    cdx_uint8 rightWord, leftWord, *offsetFrom, *offsetTo,
                  alignWord, endWord;
    // Flawfinder: ignore
    cdx_uint8 leftMask[9] = {0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
    // Flawfinder: ignore
    cdx_uint8 rightMask[9] = {0, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

    cdx_int32 alignFrom, alignTo, alignIndex=30; // special case variables
    cdx_uint8 mask[2] = {0x0f, 0xf0}; // Flawfinder: ignore

    offsetFromMod = bitOffsetFrom & 0x07;  // same as %8
    offsetToMod = bitOffsetTo & 0x07;
    numMod = numBits & 0x07;
    offsetFromModLeft = 8 - offsetFromMod; // don't want these evaluated every loop
    offsetToModLeft = 8 - offsetToMod;
    offsetFrom = fromPtr + (bitOffsetFrom >> 3);
    offsetTo = toPtr + (bitOffsetTo >> 3);
    jMax = (numBits>>3) - 1; // last output byte not handled inside a loop

    if (numBits>>3 == 0) // quick and easy if we have fewer than 8 bits to align
    {
        leftWord = *(offsetFrom++);
        rightWord = *(offsetFrom);
        alignWord = (leftWord >> offsetFromMod) + (rightWord << (offsetFromModLeft));
        alignWord &= rightMask[numMod];

        // have more extra input bits than free space in current output byte
        if (numMod >= offsetToModLeft)
        {
            *(offsetTo) &= rightMask[offsetToMod];
            *(offsetTo++) += (alignWord << offsetToMod);
            *(offsetTo) = ((*offsetTo) & leftMask[8-(numMod-offsetToModLeft)])
                + (alignWord >> offsetToModLeft);
        }
        // have fewer input bits than free space in current output byte
        // be careful not to overwrite extra bits already in output byte
        else
        {
            endWord = *(offsetTo) & leftMask[8-(numMod+offsetToMod)];
            *(offsetTo) &= rightMask[offsetToMod];
            *(offsetTo) += ((alignWord << offsetToMod) + endWord);
        }
        return; // finished, return to calling function
      }

    // byte-packing done here is optimized for the common case of nibble-alignment
    if (bitOffsetFrom%4 == 0 && bitOffsetTo%4 == 0)
    {
        alignFrom = (bitOffsetFrom & 0x04)>>2;  // 0 implies whole-byte alignment
        alignTo = (bitOffsetTo & 0x04)>>2;      // 1 implies half-byte alignment

        if (alignFrom == alignTo) // either src and dest both byte-aligned or both half byte-aligned
            alignIndex = alignFrom == 0 ? 0 : 3;

        if (alignFrom != alignTo)
            alignIndex = alignFrom == 0 ? 1 : 2; // src aligned, dest half aligned

        switch (alignIndex)
        {
        case 0:
            for (j=0; j<jMax; j++)
                *offsetTo++ = *offsetFrom++; // copy byte-by-byte directly
            break;

        case 1:
            for (j=0; j<jMax; j++)  // move two nibbles from src to dest each loop
            {                       // shift bits as necessary
                *offsetTo = (*offsetTo & mask[0]) +
                    ((*offsetFrom & mask[0])<<4);
                *++offsetTo = ((*offsetFrom++ & mask[1])>>4);
            }
            break;

        case 2:
            for (j=0; j<jMax; j++)  // same as case 1, but shift other direction
            {
                *offsetTo = ((*offsetFrom & mask[1])>>4);
                *offsetTo++ += ((*++offsetFrom & mask[0])<<4);
            }
            break;

        case 3:
            *offsetTo &= mask[0];  // align first nibble, thereafter this is
            *offsetTo += (*offsetFrom & mask[1]);  // just like case 0
            for (j=0; j<jMax; j++)
                *++offsetTo = *++offsetFrom; // copy byte-by-byte directly
            break;
        }
    }
    else // this code can handle all source and destination buffer offsets
    {
        // take the first 8 desired bits from the input buffer, store them
        // in alignWord, then break up alignWord into two pieces to
        // fit in the free space in two consecutive output buffer bytes

        for (j=0; j<jMax; j++)
        {
            leftWord = *(offsetFrom++);
            rightWord = *(offsetFrom);
            alignWord = (leftWord >> offsetFromMod) + (rightWord << (offsetFromModLeft));
            *(offsetTo) = (*(offsetTo) & rightMask[offsetToMod]) +
                (alignWord << (offsetToMod));
            *(++offsetTo) = alignWord >> (offsetToModLeft);
        }
    }

    // special section to set last byte in fromBuf correctly
    // even if byte packing was done with the code optimized for nibble-alignment,
    // the tricky job of setting the last output byte is still done here

    leftWord = *(offsetFrom++);
    rightWord = *(offsetFrom);
    alignWord = (leftWord >> offsetFromMod) + (rightWord << (offsetFromModLeft));
    *(offsetTo) = (*(offsetTo) & rightMask[offsetToMod]) + (alignWord << (offsetToMod));

    if (numMod >= offsetToModLeft)
    {
        *(++offsetTo) = alignWord >> (offsetToModLeft);

        leftWord = *(offsetFrom++);
        rightWord = *(offsetFrom);
        alignWord = (leftWord >> offsetFromMod) + (rightWord << (offsetFromModLeft));
        alignWord &= rightMask[numMod];
        *(offsetTo++) += (alignWord << offsetToMod);
        if (offsetTo >= toPtr && offsetTo < pToLimit)
        {
            *(offsetTo) = ((*offsetTo) & leftMask[8-(numMod-offsetToModLeft)])
                             + (alignWord >> offsetToModLeft);
        }
    }
    else
    {
        endWord = *(++offsetTo) & leftMask[8-(numMod+offsetToMod)];
        *(offsetTo) = alignWord >> (offsetToModLeft);
        leftWord = *(offsetFrom++);
        rightWord = *(offsetFrom);
        alignWord = (leftWord >> offsetFromMod) + (rightWord << (offsetFromModLeft));
        alignWord &= rightMask[numMod];
        *(offsetTo) += ((alignWord << offsetToMod) + endWord);
    }
}

/******************************************************************
 function RASL_DeInterleave
 input: *buf          :rawdata pointer;
         ulBufSize     :ulSuperBlockSize;
         type          :pInfo->usFlavorIndex
 ******************************************************************/
static void RASL_DeInterleave(cdx_uint8 *buf, cdx_uint32 ulBufSize, cdx_int32 type)
{
    cdx_uint8 tmp[RASL_MAXCODEBYTES];     // space for swapping
    cdx_int32 codeBytes, codeBits=0;
    cdx_int32 nCodeBits_tab[4] = {RA65_NCODEBITS, RA85_NCODEBITS, RA50_NCODEBITS, RA160_NCODEBITS};
    cdx_int32 fIn, fOut;                    // frame in/out

    if(type < 4)
    {
        codeBits = nCodeBits_tab[type];
    }

    if(codeBits%8 == 0)
    {
        codeBytes=codeBits>>3;
        for (fIn = 0; fIn < RASL_NFRAMES * RASL_NBLOCKS; fIn++)
        {
            fOut = RASL_InterleaveTable[fIn];  // frame to swap with
            //Note that when (fOut == fIn), the frame doesn't move, and if (fOut < fIn),
            //we have swapped it already.
            if (fOut > fIn) // do the swap if needed
            {
                 memcpy(tmp, buf + fOut * codeBytes, codeBytes);
                 memcpy(buf + fOut * codeBytes, buf + fIn * codeBytes, codeBytes);
                 memcpy(buf + fIn * codeBytes, tmp, codeBytes);
            }
        }
    }
    else
    {
        for (fIn = 0; fIn < RASL_NFRAMES * RASL_NBLOCKS; fIn++)
        {
            fOut = RASL_InterleaveTable[fIn];  // frame to swap with
            //Note that when (fOut == fIn), the frame doesn't move, and if (fOut < fIn),
            //we have swapped it already.
            if (fOut > fIn) // do the swap if needed
            {
                ra_bitcopy(tmp, RASL_MAXCODEBYTES, buf,0,fOut*codeBits, codeBits);
                ra_bitcopy(buf, ulBufSize, buf, fOut*codeBits, fIn*codeBits, codeBits);
                ra_bitcopy(buf, ulBufSize, tmp, fIn*codeBits, 0, codeBits);
            }
        }
    }
}

cdx_int32 matroska_parse_block_info(MatroskaDemuxContext *matroska)
{
    cdx_int32 res = 0;
    AVStream *st;
    cdx_int32 stream_index;

    stream_index = matroska->tracks[matroska->track]->stream_index;
    st = matroska->streams[stream_index];

    if(matroska->lace_cnt)
    {
        st->pts += matroska->blk_duration;
        matroska->time_stamp = st->pts/1000;
    }

    if (st->codec_id == CODEC_ID_RA_288 ||
             st->codec_id == CODEC_ID_COOK ||
             st->codec_id == CODEC_ID_ATRAC3)
    {
        MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *)matroska->tracks[matroska->track];
        cdx_int32 sps     = audiotrack->sub_packet_size;
        cdx_int32 cfs     = audiotrack->coded_framesize;
        cdx_int32 h     = audiotrack->sub_packet_h;
        cdx_int32 w     = audiotrack->frame_size;
        cdx_int32 y     = matroska->sub_packet_cnt;
        cdx_int32 x;

        if(matroska->buf==NULL)
        {
            matroska->buf = PHY_MALLOC(w*h, 0);
            if (matroska->buf == NULL)
                return AVERROR(ENOMEM);
            matroska->buf_size = w*h;
        }
        else if(matroska->buf_size < w*h)
        {
            PHY_FREE(matroska->buf);
            matroska->buf = PHY_MALLOC(w*h, 0);
            if (matroska->buf == NULL)
                return AVERROR(ENOMEM);
            matroska->buf_size = w*h;
        }

        if(matroska->is_keyframe)
        {
            matroska->sub_packet_cnt = 0;
        }

        if (matroska->sub_packet_cnt == 0)
        {
            matroska->time_stamp0 = matroska->time_stamp;
        }

        if (st->codec_id == CODEC_ID_RA_288)
            for (x=0; x<h/2; x++)
                memcpy(matroska->buf+x*2*w+y*cfs, matroska->data_parse+x*cfs, cfs);
        else
            for (x=0; x<w/sps; x++)
                memcpy(matroska->buf+sps*(h*x+((h+1)/2)*(y&1)+(y>>1)),
                       matroska->data_parse+x*sps, sps);

        if (++matroska->sub_packet_cnt >= h) {
            matroska->sub_packet_cnt = 0;
            matroska->t_size = h*w;
            matroska->time_stamp = matroska->time_stamp0;
            matroska->pkt_data = matroska->buf;
        }
        else
        {
              matroska->t_size = 0;
        }
        matroska->r_size = 0;
        matroska->lace_cnt = matroska->laces;
    }
    else if (st->codec_id == CODEC_ID_SIPRO)
    {
        MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *)matroska->tracks[matroska->track];
        cdx_int32 h     = audiotrack->sub_packet_h;
        cdx_int32 w     = audiotrack->frame_size;

        if(matroska->buf==NULL)
        {
            matroska->buf = PHY_MALLOC(w*h, 0);
            if (matroska->buf == NULL)
                return AVERROR(ENOMEM);
            matroska->buf_size = w*h;
        }
        else if(matroska->buf_size < w*h)
        {
            PHY_FREE(matroska->buf);
            matroska->buf = PHY_MALLOC(w*h, 0);
            if (matroska->buf == NULL)
                return AVERROR(ENOMEM);
            matroska->buf_size = w*h;
        }

        if(matroska->is_keyframe)
        {
            matroska->sub_packet_cnt = 0;
        }

        if (matroska->sub_packet_cnt == 0)
        {
            matroska->time_stamp0 = matroska->time_stamp;
        }

        memcpy(matroska->buf+matroska->sub_packet_cnt*w, matroska->data_parse, w);

        if (++matroska->sub_packet_cnt >= h) {
            RASL_DeInterleave(matroska->buf, h*w, st->extradata[18]);
            matroska->sub_packet_cnt = 0;
            matroska->t_size = h*w;
            matroska->time_stamp = matroska->time_stamp0;
            matroska->pkt_data = matroska->buf;
        }
        else
        {
              matroska->t_size = 0;
        }
        matroska->r_size = 0;
        matroska->lace_cnt = matroska->laces;
    }

#if OGG_HEADER
    else if(st->codec_id == CODEC_ID_VORBIS)
    {
           cdx_int32 lacing_fill;
           cdx_int32 pading_len;
           cdx_int32 pading_cnt;

        matroska->r_size = 0;
        matroska->t_size = 0;
        matroska->pkt_data = matroska->data_parse;
        matroska->data_parse += matroska->t_size;
        matroska->lace_cnt++;
           pading_len = 27;
           matroska->pkt_data -= 512;
           for(pading_cnt=0; pading_cnt<26; pading_cnt++)
        {
            matroska->pkt_data[pading_cnt] = 0;
        }
        pading_cnt = 27;
           matroska->pkt_data[0] = 0x4f;
        matroska->pkt_data[1] = 0x67;
        matroska->pkt_data[2] = 0x67;
        matroska->pkt_data[3] = 0x53;
        matroska->pkt_data[5] = matroska->ogg_uflag;
        matroska->pkt_data[0x12] = (matroska->ogg_pageno >>  0) & 0xff;
        matroska->pkt_data[0x13] = (matroska->ogg_pageno >>  8) & 0xff;
        matroska->pkt_data[0x14] = (matroska->ogg_pageno >> 16) & 0xff;
        matroska->pkt_data[0x15] = (matroska->ogg_pageno >> 24) & 0xff;
        matroska->ogg_pageno++;
           for(matroska->lace_cnt=0; matroska->lace_cnt < matroska->laces; matroska->lace_cnt++)
           {
               matroska->t_size += matroska->lace_size[matroska->lace_cnt];
               for(lacing_fill=0;
                   lacing_fill*0xff<(cdx_int32)matroska->lace_size[matroska->lace_cnt];
                   lacing_fill++);
               pading_len += lacing_fill;
               if(pading_len>=(255+27))
               {
                   matroska->t_size = 0;
                   matroska->data_rdy = 0;
                   matroska->lace_cnt = matroska->laces;
                   return -1;
               }

            for(; pading_cnt<pading_len; pading_cnt++)
            {
                if(pading_cnt!=(pading_len-1))
                {
                    matroska->pkt_data[pading_cnt] = 0xff;
                }
                else
                {
                    matroska->pkt_data[pading_cnt] =
                            matroska->lace_size[matroska->lace_cnt] - (lacing_fill-1)*0xff;
                }
            }
        }
        if(matroska->pkt_data[pading_cnt-1]==0xff)
        {
            matroska->ogg_uflag = 1;
        }
        else
        {
            matroska->ogg_uflag = 0;
        }
        matroska->pkt_data[26] = pading_len - 27;
        //if(matroska->ogg_uflag == 0)
        //{
        //    matroska->ogg_granulepos += 1024;
        //    matroska->pkt_data[0x6] = (matroska->ogg_granulepos >>  0) & 0xff;
        //    matroska->pkt_data[0x7] = (matroska->ogg_granulepos >>  8) & 0xff;
        //    matroska->pkt_data[0x8] = (matroska->ogg_granulepos >> 16) & 0xff;
        //    matroska->pkt_data[0x9] = (matroska->ogg_granulepos >> 24) & 0xff;
        //    matroska->pkt_data[0xa] = (matroska->ogg_granulepos >> 32) & 0xff;
        //    matroska->pkt_data[0xb] = (matroska->ogg_granulepos >> 40) & 0xff;
        //    matroska->pkt_data[0xc] = (matroska->ogg_granulepos >> 48) & 0xff;
        //    matroska->pkt_data[0xd] = (matroska->ogg_granulepos >> 56) & 0xff;
        //}
        matroska->t_size += pading_len;
        matroska->pkt_data += (512 - pading_len);
        for(pading_cnt=0; pading_cnt<pading_len; pading_cnt++)
        {
            matroska->pkt_data[pading_cnt] = matroska->pkt_data[pading_len + pading_cnt - 512];
        }
    }
    #endif

    else
    {
        matroska->slice_num     = matroska->data_parse[0] + 1;   //for rmvb
        matroska->slice_cnt     = 0;                             //for rmvb
        matroska->slice_len     = 0;                             //for rmvb and 264
        matroska->slice_offset     = 1+(matroska->slice_num<<3);    //for rmvb
        matroska->r_size = 0;
        matroska->t_size = matroska->lace_size[matroska->lace_cnt];
        matroska->pkt_data = matroska->data_parse;
        matroska->data_parse += matroska->t_size;
        matroska->lace_cnt++;
    }

    if(matroska->t_size<0 || matroska->t_size>4*1024*1024)
    {
        return AVERROR(EIO);
    }

    return res;
}

cdx_int32 matroska_read_packet (struct CdxMkvParser *p)
{
    MatroskaDemuxContext *matroska = p->priv_data;
    cdx_int32 res = 0;
    cdx_uint32 id;

    while (matroska->data_rdy == 0)
    {
        if(matroska->parsing_cluster)
        {
            res = matroska_parse_cluster(matroska);
            if(res  < 0)
            {
                CDX_LOGE(" parse cluster error: %d", res);
                return AVERROR(EIO);
            }
        }
        else
        {
            while (res == 0)
            {
                id = ebml_peek_id(matroska, &matroska->level_up);
                if (!id)
                {
                    return AVERROR(EIO);
                }
                else if (matroska->level_up)
                {
                    matroska->level_up--;
                    break;
                }

                switch (id)
                {
                    case CDX_MATROSKA_ID_CLUSTER:
                        if ((res = ebml_read_master(matroska, &id)) < 0)
                            break;
                        matroska->parsing_cluster = 1;
                        matroska->cluster_time = INT64_MAX;
                        res = matroska_parse_cluster(matroska);
                        break;

                    default:
                        res = ebml_read_skip(matroska);
                        break;
                }

                if (matroska->level_up)
                {
                    matroska->level_up--;
                    break;
                }

                if (res < 0)
                {
                    matroska->done = 1;
                    return res;
                }
                if(matroska->data_rdy == 1)
                {
                    break;
                }
            }
        }
    }
    matroska_parse_block_info(matroska);

    return 0;
}

/************************************************************************/
/*  Find from current pos's nearest keyframe index                      */
/************************************************************************/
cdx_int32 matroska_find_keyframe(struct CdxMkvParser *p,
                                 cdx_int32 direction,
                                 cdx_int32 next_index,
                                 cdx_int32 ms)
{
    MatroskaDemuxContext *matroska = p->priv_data;
    AVStream *st = matroska->streams[matroska->VideoStreamIndex];
    cdx_int32 index;

    if(!st->index_entries)
    {
        CDX_LOGE("--- error, the index_entries is NULL");
        return -1;
    }
    if(next_index == 0)
    {
        if(!direction)
        {
            for(index=0; index<st->nb_index_entries-1; index++)
            {
                if(st->index_entries[index].timestamp >= ms)
                    break;
            }
        }
        else
        {
            for(index=st->nb_index_entries-1; index>=0; index--)
            {
                if(st->index_entries[index].timestamp <= ms)
                    break;
            }
        }

        // find index entry
        if (index < 0 || index>=st->nb_index_entries)
        {
            CDX_LOGD("index = %d, st->nb_index_entries = %d", index, st->nb_index_entries);
            return -1;
        }

        matroska->index_ptr = index;
    }
    else if(next_index == 1)
    {
        if(!direction)
        {
            index = matroska->index_ptr+1;
        }
        else
        {
            index = matroska->index_ptr-1;
        }

        // find index entry
        if (index < 0 || index>=st->nb_index_entries)
            return -1;

        matroska->index_ptr = index;
    }
    else
    {
        p->pos = CdxStreamTell(matroska->fp);
        if(p->pos < 0)
        {
            return -1;
        }
        for(index=0; index<st->nb_index_entries; index++)
        {
            if(st->index_entries[index].pos > p->pos)
                break;
            if((st->index_entries[index].timestamp-1000)*AV_TIME_BASE > st->pts)
                break;
        }
        if(index>=st->nb_index_entries)
        {
            return AVERROR_EOS;
        }
    }

    // do the seek
    matroska->num_levels = matroska->num_levels_bak;
    matroska->peek_id = 0;
    p->pos = st->index_entries[index].pos;
    matroska->frame_num_between_kframe = 1;

    if (ms != 0)
        matroska->skip_to_keyframe = 1;

    matroska->skip_to_stream = st;
    matroska->peek_id = 0;
    matroska->laces = 0;
    matroska->lace_cnt = 0;
    matroska->data_rdy = 0;
    matroska->parsing_cluster=0;
    st->pts = matroska->streams[p->AudioStreamIndex]->pts =
              st->index_entries[index].timestamp*AV_TIME_BASE;
    return 0;
}

static int find_next_pts(unsigned char* data, int data_size, int *tmp)
{
    unsigned char* pkt;
    unsigned char* dataEnd;
    int               frameFound =0;

    dataEnd  = data + data_size;
    *tmp         = 0;

    for(pkt = data; pkt <= dataEnd-9; pkt ++)
    {
        if(*pkt == 0xA3) // simple block in cluster
        {
            //LOGD("...0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
            //*(pkt+1), *(pkt+2), *(pkt+3), *(pkt+4),
            //*(pkt+5), *(pkt+6), *(pkt+7), *(pkt+8), *(pkt+9));
            if(((*(pkt+1) >= 0x80 && *(pkt+2) == 0x81 && *(pkt+6) == 0x00)
            || (*(pkt+1) >= 0x40 && *(pkt+1) < 0x80 && *(pkt+3) == 0x81 && *(pkt+7) == 0x00)
            || (*(pkt+1) >= 0x20 && *(pkt+1) < 0x40 && *(pkt+4) == 0x81 && *(pkt+8) == 0x00)
            || (*(pkt+1) >= 0x10 && *(pkt+1) < 0x20 && *(pkt+5) == 0x81))&& *(pkt+9) == 0x00)
            {
                CDX_LOGD("------- get one key frame!!");
                frameFound = 1;
                break;
            }
        }
        else if(*pkt == 0xA0 && *(pkt+3)==0xA1) // blockgroup in cluster
        {

        }
    }

    if (frameFound ==1)
    {
        *tmp = pkt - data;
        return 1;
    }
    else
    {
        return 0;
    }
}
/*
**************************************************************************************************
*                           SEEK KEYFRAME WHEN KEYFRAME INDEX NOT EXIST!
*
*Description: seek when keyframe not exist!!
*
*Arguments  : ret   seek result;
*
*Author: Fuqiang from ogg parser
**************************************************************************************************
*/

static int mkv_seek_keyframe(struct CdxMkvParser *p, int timeInterval)
{
    MatroskaDemuxContext *matroska = p->priv_data;
    AVStream *st = matroska->streams[matroska->VideoStreamIndex];
    unsigned int    cnt;
    cdx_uint64        jump_file_pos=0;
    cdx_uint64        pts_pos=0;
    int        tmp;
    int     ret = 0;
    //int     pos_find = 0;
    CDX_LOGV("%d ms , %lld ms , FileEndPst %lld",
            timeInterval, matroska->segment_duration, matroska->fileEndPst);
    jump_file_pos = (matroska->fileEndPst/matroska->segment_duration)*timeInterval
                    + ((matroska->fileEndPst%matroska->segment_duration)*timeInterval)/
                       matroska->segment_duration;
    CDX_LOGV("jump_file_pos %lld", jump_file_pos);

    tmp = 0;
    cnt = 0;

reseek:
    CdxStreamSeek(matroska->fp, jump_file_pos, SEEK_SET);
    if(CdxStreamRead(matroska->fp, matroska->tmpbuf, matroska->tmpbufsize) <
       (int)matroska->tmpbufsize)
    {
        CDX_LOGW("seek failed, read the end of file!!!");
        return -1;
    }
    ret = find_next_pts(matroska->tmpbuf, matroska->tmpbufsize, &tmp);
    if(ret == 0)
    {
        jump_file_pos += (matroska->tmpbufsize-9);
        cnt ++;
        if(cnt < 11)
        {
            goto reseek;
        }
        else
        {
            CDX_LOGW("seek failed, try too many times!!!");
            return -1;
        }
    }
    pts_pos = jump_file_pos + tmp;
    CDX_LOGV("tmp %d   pts_pos %lld", tmp, pts_pos);
    p->pos = pts_pos;
    matroska->num_levels = matroska->num_levels_bak;
    matroska->peek_id = 0;
    matroska->frame_num_between_kframe = 1;
    matroska->skip_to_keyframe = 1;
    matroska->skip_to_stream = st;
    matroska->peek_id = 0;
    matroska->laces = 0;
    matroska->lace_cnt = 0;
    matroska->data_rdy = 0;
    matroska->parsing_cluster=0;
    return 0;
}

cdx_int32 matroska_read_close (struct CdxMkvParser *p)
{
    MatroskaDemuxContext *matroska = p->priv_data;
    AVStream *st;
    cdx_int32 n = 0;
    cdx_uint32 stream;

    for(stream=0; stream<matroska->nb_streams; stream++)
    {
        st = matroska->streams[stream];
        if(st)
        {
            if(st->extradata)
            {
                PHY_FREE(st->extradata);
                st->extradata = NULL;
            }
            if(st->index_entries)
            {
                PHY_FREE(st->index_entries);
                st->index_entries = NULL;
            }
            free(st);
            matroska->streams[stream] = 0;
        }
    }

    if(matroska->data)
    {
        free(matroska->data);
        matroska->data = NULL;
    }

    if(matroska->buf)
    {
        free(matroska->buf);
        matroska->buf = NULL;
    }
    if(matroska->priData)
    {
        free(matroska->priData);
        matroska->priData = NULL;
    }

    if(matroska->index)
    {
        free(matroska->index);
        matroska->index = NULL;
    }

    if(matroska->lace_size)
    {
        free(matroska->lace_size);
        matroska->lace_size = NULL;
    }

    for (n = 0; n < matroska->num_tracks; n++) {
        MatroskaTrack *track = matroska->tracks[n];
        if(track->codec_priv)
        {
            free(track->codec_priv);
            track->codec_priv = NULL;
        }
        if(track->comp_settings)
        {
            free(track->comp_settings);
            track->comp_settings = NULL;
        }

        free(matroska->tracks[n]);
        matroska->tracks[n] = NULL;
    }

    if(matroska->fp)
    {
        CdxStreamClose(matroska->fp);
        matroska->fp = 0;
    }

    if(matroska->pVbsInf)
    {
        free(matroska->pVbsInf);
    }

    //added by fuqiang for seek key frame
    if(matroska->tmpbuf)
        free(matroska->tmpbuf);

    return 0;
}

static int MkvGetClusterTimeCode(struct CdxMkvParser *p, cdx_uint64* num)
{
    MatroskaDemuxContext    *matroska;
    if(!p || !p->stream)
    {
        return -1;
    }

    matroska = (MatroskaDemuxContext *)p->priv_data;
    unsigned int id;
    cdx_uint64 length;
    int res = 0;
    cdx_uint64 total;
    cdx_uint32 read;
    cdx_uint64 rlength;
    CDX_LOGD("--- offset: %llx", CdxStreamTell(matroska->fp));

    while (res == 0)
    {
        read = ebml_read_num(matroska, 4, &total);
        id = (cdx_uint32)(total | (1 << (read * 7)));
        CDX_LOGD("--- id = %x, total = %llx, read = %d, pos = %llx",
                 id, total, read, CdxStreamTell(matroska->fp));
        if (res < 0)
        {
            CDX_LOGE("--- id = %x", id);
            return -1;
        }

        switch (id)
        {
             // cluster timecode
            case CDX_MATROSKA_ID_CLUSTERTIMECODE:
            {
                CDX_LOGD("--- TimeCode offset = %llx", CdxStreamTell(matroska->fp));
                int n=0;
                cdx_uint8  t[16];
                if (( ebml_read_num(matroska, 8, &rlength)) < 0)
                    return -1;

                int size = (cdx_int32)rlength;
                if (size < 1 || size > 8)
                {
                    cdx_uint64 pos = CdxStreamTell(matroska->fp);
                    return AVERROR_INVALIDDATA;
                }

                // big-endian ordening; build up number
                *num = 0;
                if(size-n>0)
                {
                    if(CdxStreamRead(matroska->fp, &t[n], size-n) < (size-n))
                    {
                        return AVERROR(EIO); // EOS or actual I/O error
                    }
                }

                while (n++ < size)
                    *num = (*num << 8) | t[n-1];

                CDX_LOGD("--- timecode length = %lld, offset = %llx",
                        rlength, CdxStreamTell(matroska->fp));
                return 0;
            }

            default:
                if (( ebml_read_num(matroska, 8, &rlength)) < 0)
                    return -1;

                CdxStreamSeek(matroska->fp, rlength, SEEK_CUR);
                break;
        }
    }

    return res;
}

int stringSearch(unsigned char *s,unsigned char *p1,int s_len,int p_len,
    int *number_0,struct CdxMkvParser *p)
{
    int i = 0,j = 0;
    while(i < s_len && j < p_len)
    {
        if(p->exitFlag == 1)
        {
            return -2;
        }
        if(j == -1 || s[i] == p1[j])
        {
            i++;
            j++;
        }
        else
        {
            if(s[i] == 0)
            {
               (*number_0)++;
            }
            else
            {
                *number_0 = 0;
            }
            i = i - j + 1;
            j = 0;
        }
    }

    if(j == p_len)
        return i - j;
    else
        return -1;
}

#define SEEK_CLUSTER_LEN 2048
#define CLUSTER_LEN 4
int findCluster(struct CdxMkvParser *p,unsigned int id)
{
    MatroskaDemuxContext *matroska;
    int pos = -1,io_state = -1,read_len = -1;
    int flag = 0;
    int number_0 = 0;
    unsigned char data[SEEK_CLUSTER_LEN];
    unsigned char buf[6];
    unsigned char c[CLUSTER_LEN] = {0x1f,0x43,0xb6,0x75};

    if(!p)
    {
        return -1;
    }
    matroska = (MatroskaDemuxContext *)p->priv_data;
    if(!matroska)
    {
        return -1;
    }

    if(id == CDX_MATROSKA_ID_CLUSTER)
    {
        CDX_LOGD("find the cluster!\n");
        return 0;
    }

    CdxStreamSeek(matroska->fp, -4, SEEK_CUR);

    while(pos == -1)
    {
        if(p->exitFlag == 1)
        {
            return -1;
        }

        if(number_0 > 10000)
        {
            CDX_LOGE("the file has 10000 bytes 0x00,maybe file mailfald");
            number_0 = 0;
            return 0;
        }
        memset(data,0,SEEK_CLUSTER_LEN);
        read_len = CdxStreamRead(matroska->fp, data, SEEK_CLUSTER_LEN);
        io_state = CdxStreamGetIoState(matroska->fp);
        if(io_state == CDX_IO_STATE_INVALID ||
            io_state == CDX_IO_STATE_ERROR)
        {
            return -1;
        }
        else if(read_len < SEEK_CLUSTER_LEN)
        {
            pos = stringSearch(data,c,SEEK_CLUSTER_LEN,CLUSTER_LEN,
                    &number_0,p);
            if(pos > -1)
            {
                flag = 1;
            }
            else if(pos == -2)
            {
                return -1;
            }
            break;
        }
        else
        {
            pos = stringSearch(data,c,SEEK_CLUSTER_LEN,CLUSTER_LEN,
                    &number_0,p);
            if(pos == -1)
            {
                CdxStreamSeek(matroska->fp, -3, SEEK_CUR);
                memset(buf,0,6);
                read_len = CdxStreamRead(matroska->fp, buf, 6);
                io_state = CdxStreamGetIoState(matroska->fp);
                if(io_state == CDX_IO_STATE_INVALID ||
                    io_state == CDX_IO_STATE_ERROR)
                {
                    return -1;
                }
                else if(read_len < 6)
                {
                    pos = stringSearch(buf,c,6,CLUSTER_LEN,&number_0,p);
                    if(pos > -1)
                    {
                        flag = 1;
                    }
                    else if(pos == -2)
                    {
                        return -1;
                    }
                    break;
                }
                else
                {
                    pos = stringSearch(buf,c,6,CLUSTER_LEN,&number_0,p);
                    if(pos == -1)
                    {
                        CdxStreamSeek(matroska->fp, -3, SEEK_CUR);
                    }
                    else if(pos == -2)
                    {
                        return -1;
                    }
                    else
                    {
                        flag = 2;
                    }
                }
            }
            else if(pos == -2)
            {
                return -1;
            }
        }
    }
    if(flag == 0)
    {
        pos = 0 - (SEEK_CLUSTER_LEN - pos - CLUSTER_LEN);
        CdxStreamSeek(matroska->fp, pos, SEEK_CUR);
    }
    else if(flag == 1)
    {
        pos = 0 - (read_len - pos - CLUSTER_LEN);
        CdxStreamSeek(matroska->fp, pos, SEEK_CUR);
    }
    else if(flag == 2)
    {
        pos = 0 - (6 - pos - CLUSTER_LEN);
        CdxStreamSeek(matroska->fp, pos, SEEK_CUR);
    }

    return 0;
}

cdx_int32 matroska_parse_seek_cluster (MatroskaDemuxContext *matroska,cdx_int64  *timeUs,
    cdx_int64 cluster_len)
{
    cdx_int32 res = 0;
    cdx_uint32 id;
    cdx_int64 tell = 0;
    cdx_int64 r_end_tell = 0;
    tell = CdxStreamTell(matroska->fp);

    while (r_end_tell - tell < cluster_len)
    {
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            res = AVERROR(EIO);
            break;
        }

        switch (id)
        {
            // cluster timecode
            case CDX_MATROSKA_ID_CLUSTERTIMECODE:
            {
                cdx_uint64 num;
                res = ebml_read_uint(matroska, &id, &num);
                if (res < 0)
                    break;
                matroska->cluster_time = num;
                if((cdx_int64)matroska->cluster_time * matroska->time_scale >
                   (matroska->segment_duration * AV_TIME_BASE))
                {
                    //matroska->cluster_time = INT64_MAX;
                    CDX_LOGD("care! the cluster time is not start from 0");
                }

                break;
            }

            default:
                res = ebml_read_skip(matroska);
                break;
        }
        r_end_tell = CdxStreamTell(matroska->fp);
        *timeUs = (cdx_int64)matroska->cluster_time * matroska->time_scale / AV_TIME_BASE;
    }

    return res;
}

cdx_int32 ebml_read_cluster (MatroskaDemuxContext *matroska, cdx_uint32 *id,cdx_int64 *cluster_len)
{
    CdxStreamT *pb = matroska->fp;
    cdx_uint64 length;
    MatroskaLevel *currentLevel;
    cdx_int32 res;

    if ((res = ebml_read_element_id(matroska, id, NULL)) < 0 ||
        (res = ebml_read_element_length(matroska, &length)) < 0)
        return res;

    // protect... (Heaven forbids that the '>' is true)
    if (matroska->num_levels >= EBML_MAX_DEPTH)
    {
        return AVERROR(ENOSYS);
    }

    // remember level
    currentLevel = &matroska->levels[matroska->num_levels++];
    currentLevel->start = CdxStreamTell(pb);
    currentLevel->length = length;
    *cluster_len = currentLevel->length;

    return 0;
}

#define MAX_FILE_LENGTH 3221225472LL
cdx_int32 matroska_parse_cluster_time(MatroskaDemuxContext *matroska,cdx_int64  timeUs)
{
    cdx_int32 res = 0,flag_0 = 0,flag_1 = 0;
    cdx_uint32 id;
    cdx_int64 cur_seek_time = 0,diff_time = 0;
    cdx_int64 cluster_len = 0;

    while (res == 0)
    {
        /*Ensure that the time error of seek is within 4s,
        *if the time error can not be guaranteed within 4s,
        *then break out of the cycle.
        */
        diff_time = cur_seek_time- timeUs;
        if(diff_time < 0 && diff_time > -4000)
        {
            break;
        }
        else if(diff_time > 0 && diff_time < 4000)
        {
            break;
        }
        else if(diff_time < -4000)
        {
            flag_0++;
        }
        else if(diff_time > 4000)
        {
            flag_1++;
        }
        if(flag_0 != 0 && flag_1 != 0)
        {
            break;
        }
        id = ebml_peek_id(matroska, &matroska->level_up);
        if (!id)
        {
            return AVERROR(EIO);
        }

        switch (id)
        {
            case CDX_MATROSKA_ID_CLUSTER:
            {
                if ((res = ebml_read_cluster(matroska, &id , &cluster_len)) < 0)
                    break;
                matroska->cluster_time = INT64_MAX;
                res = matroska_parse_seek_cluster(matroska,&cur_seek_time,cluster_len);
                break;
            }

            default:
            {
                res = ebml_read_skip(matroska);
                break;
            }
        }

        if (res < 0)
        {
            return res;
        }
    }

    return 0;
}

static int MkvFindCluster(struct CdxMkvParser *p, cdx_uint64* num)
{
    int ret;
    MatroskaDemuxContext    *matroska;

    if(!p || !p->stream)
    {
        return -1;
    }

    matroska = (MatroskaDemuxContext *)p->priv_data;

    unsigned int id;
    id = CdxStreamGetBE32(matroska->fp);

    //find the Cluster
    ret = findCluster(p,id);
    if(ret < 0)
    {
        return ret;
    }
    CDX_LOGD("***** find cluster %llx", CdxStreamTell(matroska->fp));

    cdx_uint64 length;
    // read the length of cluster
    ebml_read_num(matroska, 8, num);
#if 0
    int n=0;
    cdx_uint8  t[16];

    int size = (cdx_int32)length;
    if (size < 1 || size > 8)
    {
        cdx_uint64 pos = CdxStreamTell(matroska->fp);
        return -1;
    }

    // big-endian ordening; build up number
    *num = 0;
    if(size-n>0)
    {
        if(CdxStreamRead(matroska->fp, &t[n], size-n) < (size-n))
        {
            return -1;
        }
    }

    while (n++ < size)
        *num = (*num << 8) | t[n-1];
 #endif
    CDX_LOGD("cluster length = %llx", *num);

    return 0;
}

static int MkvEstimateBR(struct CdxMkvParser *p)
{
    cdx_int64 offset, offset2=0;
    int ret;
    cdx_uint64 clusterTime, clusterTime2=0;
    cdx_uint64 timeTmp, offsetTmp;
    MatroskaDemuxContext    *matroska;
    int i;

    if(!p || !p->stream)
    {
        return -1;
    }

    matroska = (MatroskaDemuxContext *)p->priv_data;
    offset = CdxStreamTell(matroska->fp);

    CdxStreamSeek(matroska->fp, matroska->segment_start, SEEK_SET);
    cdx_uint64 clusterLength;
    if(MkvFindCluster(p, &clusterLength) < 0)
    {
        matroska->bitrate = 0;
        return -1;
    }

    ret = MkvGetClusterTimeCode(p, &clusterTime);
    if(ret < 0)
    {
        return ret;
    }

    offsetTmp = offset;
    for(i=0; i<20 && CdxStreamTell(matroska->fp)<matroska->fileEndPst; i++)
    {
        CdxStreamSeek(matroska->fp,
                      offsetTmp+clusterLength-CdxStreamTell(matroska->fp)-7,
                      SEEK_CUR);

        if(MkvFindCluster(p, &clusterLength) < 0)
        {
            matroska->bitrate = 0;
            return -1;
        }

        if(p->exitFlag == 1)
        {
            return -1;
        }

        ret = MkvGetClusterTimeCode(p, &timeTmp);
        if(ret < 0)
        {
            break;
        }

        clusterTime2 = timeTmp;
        offset2 = CdxStreamTell(matroska->fp);
        offsetTmp = offset2;
    }

    if(offset2== 0 || clusterTime2 == 0)
    {
        CDX_LOGE("cannot get the bitrate, ");
        matroska->bitrate = 0;
    }
    else
    {
        matroska->bitrate = (offset2-offset)/(clusterTime2-clusterTime)*
                            AV_TIME_BASE / matroska->time_scale;
    }
    CDX_LOGD("-- bitrate = %lld", matroska->bitrate);
    return 0;
}

static int MkvResync(struct CdxMkvParser *p)
{
    MatroskaDemuxContext    *matroska;
    if(!p || !p->stream)
    {
        return -1;
    }

    matroska = (MatroskaDemuxContext *)p->priv_data;
    AVStream *st = matroska->streams[matroska->VideoStreamIndex];

    unsigned int id;
    id = CdxStreamGetBE32(matroska->fp);
    int ret;

    //find the Cluster
    ret = findCluster(p,id);
    if(ret < 0)
    {
        return ret;
    }

    if(CdxStreamTell(matroska->fp) == matroska->fileEndPst)
    {
        CDX_LOGE("cannot find Cluster id, eos in resync");
        return -1;
    }

    CdxStreamSeek(matroska->fp, -4, SEEK_CUR);
    cdx_uint64 tell = CdxStreamTell(matroska->fp);
    CDX_LOGD("id = %x, pos = %lld(%llx)", id, tell, tell);

    // do the seek
    matroska->num_levels = matroska->num_levels_bak;
    matroska->peek_id = 0;
    matroska->frame_num_between_kframe = 1;
    matroska->skip_to_keyframe = 1;
    matroska->skip_to_stream = st;
    matroska->peek_id = 0;
    matroska->laces = 0;
    matroska->lace_cnt = 0;
    matroska->data_rdy = 0;
    matroska->parsing_cluster=0;

    return 0;
}

static int MkvResyncWithoutSeekBack(struct CdxMkvParser *p)
{
    MatroskaDemuxContext    *matroska;
    if(!p || !p->stream)
    {
        return -1;
    }

    matroska = (MatroskaDemuxContext *)p->priv_data;
    AVStream *st = matroska->streams[matroska->VideoStreamIndex];

    unsigned int id;
    int ret;
    id = CdxStreamGetBE32(matroska->fp);

    //find the Cluster
    ret = findCluster(p,id);
    if(ret < 0)
    {
        return ret;
    }

    if(CdxStreamTell(matroska->fp) == matroska->fileEndPst)
    {
        CDX_LOGE("cannot find Cluster id, eos in resync");
        return -1;
    }

    return 0;
}

/*
************************************************************************************************
*                       OPEN MKV struct cdx_stream_info READER
*
*Description: mkv file reader depack media file structure.
*
*Arguments  : p     mkv reader handle;
*             stream    mkv file path;
*
*Return     : depack mkv file structure result;
*               =   0   depack successed;
*               <   0   depack failed;
*
*Note       : This function open the mkv file with the file path, create the mkv file information
*             handle, and get the video, audio and subtitle bitstream information from the file
*             to set in the mkv file information handle. Then, set the mkv file information handle
*             to the pAviInfo for return.
*************************************************************************************************
*/
int CdxMatroskaOpen(struct CdxMkvParser *p)
{
    int ret;
    MatroskaDemuxContext    *matroska;

    if(!p || !p->stream)
    {
        return -1;
    }

    matroska = (MatroskaDemuxContext *)p->priv_data;
    // open the media file
    matroska->fp = p->stream;
    if(matroska->fp == NULL)
    {
        return -1;
    }

    //seek file end to get file size
    matroska->fileEndPst = CdxStreamSize(matroska->fp);
    if(matroska->fileEndPst == 0)
    {
        CDX_LOGD("can not get the stream size ");
        matroska->fileEndPst = 0x7fffffffffffff;
    }
    CDX_LOGD("file size = %lld", matroska->fileEndPst);

#if 0
    //seek back to file header
    if(CdxStreamSeek(matroska->fp, 0, SEEK_SET))
    {
        CDX_LOGW("Seek to file header failed!\n");
        return -1;
    }
#endif
    ret = matroska_read_header(p);
    if(ret < 0)
    {
        CDX_LOGW("read header failed: ret = %d", ret);
        return ret;
    }

    if(p->hasAudio == 0 && matroska->hasAudio != 0)
    {
        p->hasAudio = 0;
    }

    if(p->hasVideo == 0 && matroska->hasVideo != 0)
    {
        p->hasVideo = 0;
    }

    p->nhasAudio     = 0;
    p->nhasVideo    = p->hasVideo;

    p->nVidPtsOffset = 0;
    p->nAudPtsOffset = 0;

    if(p->hasVideo)
    {
        MatroskaTrack *track = matroska->tracks[matroska->VideoTrackIndex];
        MatroskaVideoTrack *videotrack = (MatroskaVideoTrack *)track;
        p->vFormat.nWidth             = (cdx_uint16)videotrack->pixel_width;
        p->vFormat.nHeight             = (cdx_uint16)videotrack->pixel_height;
        p->vFormat.nFrameRate         = videotrack->frame_rate;
        if(!p->vFormat.nFrameRate)
        {
            if( track->default_duration)
                p->vFormat.nFrameRate = 1000000*1000/(track->default_duration/1000);
        }
        p->total_frames = matroska->segment_duration;

        if(CdxStreamIsNetStream(matroska->fp))
        {
            matroska->video_direct_copy = 0;
        }
        else
        {
            matroska->video_direct_copy = 1;
        }

        switch (track->codec_id)
        {
            case CODEC_ID_MPEG4:
            {
                CDX_LOGV("video bitstream type: XVID\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_XVID;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                break;
            }

            case CODEC_ID_MSMPEG4V2:
            {
                CDX_LOGV("video bitstream type: XVID\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MSMPEG4V2;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                break;
            }

            case CODEC_ID_MSMPEG4V3:
            {
                CDX_LOGV("video bitstream type: XVID\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX3;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                break;
            }

            case CODEC_ID_DIVX4:
            {
                CDX_LOGV("video bitstream type: XVID\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX4;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                break;
            }

            case CODEC_ID_DIVX5:
            {
                CDX_LOGV("video bitstream type: XVID\n");
                //set video extradata size
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_DIVX5;
                break;
            }

            case CODEC_ID_H264:
            {
                CDX_LOGV("video bitstream type: H264\n");
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
                break;
            }

            case CODEC_ID_H265:
            {
                CDX_LOGD("video bitstream type: H265\n");
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_H265;
                break;
            }

            case CODEC_ID_MPEG2VIDEO:
            {
                CDX_LOGV("video bitstream type: XVID\n");
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG2;
                break;
            }

            case CODEC_ID_MPEG1VIDEO:
            {
                CDX_LOGD("video bitstream type: MPEG1\n");
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MPEG1;
                break;
            }

            case CODEC_ID_RV10:
            case CODEC_ID_RV20:
            {
                CDX_LOGV("video bitstream type: RXG2\n");
                matroska->video_direct_copy = 0;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_RXG2;
                break;
            }

            case CODEC_ID_RV30:
            case CODEC_ID_RV40:
            {
                CDX_LOGD("video bitstream type: RMVB89\n");
                matroska->video_direct_copy = 0;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_RX;
                break;
            }

            case CODEC_ID_WMV3:
            {
                CDX_LOGV("video bitstream type: wmv3\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV3;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                break;
            }

            case CODEC_ID_WMV1:
            {
                CDX_LOGD("video bitstream type: wmv1\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV1;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                break;
            }

            case CODEC_ID_VC1:
            {
                CDX_LOGV("video bitstream type: vc1\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV3;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                break;
            }

            case CODEC_ID_WMV2:
            {
                CDX_LOGV("video bitstream type: wmv2\n");
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_WMV2;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                break;
            }
            case CODEC_ID_VP8:
            {
                CDX_LOGV("video bitstream type: VP8\n");
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_VP8;
                break;
            }
            case CODEC_ID_VP9:
            {
                CDX_LOGV("video bitstream type: VP9\n");
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_VP9;
                break;
            }
            case CODEC_ID_MJPEG:
            {
                CDX_LOGD("video bitstream type: MJPEG\n");
                p->vFormat.pCodecSpecificData =
                        (void *)matroska->streams[p->VideoStreamIndex]->extradata;
                p->vFormat.nCodecSpecificDataLen =
                        matroska->streams[p->VideoStreamIndex]->extradata_size;
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;
                break;
            }

            default:
            {
                p->vFormat.eCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
                p->hasVideo = 0;
                break;
            }
        }
    }

    if(p->hasAudio)
    {
        cdx_int32 i;
        for (i = 0; i < matroska->num_tracks; i++)
        {
            MatroskaTrack *track = matroska->tracks[i];
            if (track->type == MATROSKA_TRACK_TYPE_AUDIO)
            {
                MatroskaAudioTrack *audiotrack = (MatroskaAudioTrack *)track;
                cdx_int32 stream_index = track->stream_index;
                AudioStreamInfo *aFormat=&p->aFormat_arry[p->nhasAudio];

                p->AudioTrackIndex[p->nhasAudio] = i;
                aFormat->nChannelNum         = audiotrack->channels;
                aFormat->nBitsPerSample         = audiotrack->bitdepth;
                aFormat->nAvgBitrate         = audiotrack->bitrate;
                aFormat->nMaxBitRate         = audiotrack->bitrate;
                aFormat->nSampleRate            = audiotrack->samplerate;
                aFormat->nBlockAlign        = audiotrack->block_align;
                //aFormat->eSubCodecFormat = track->codec_id;

                switch(track->codec_id)
                {
                    case CODEC_ID_MP2:
                    case CODEC_ID_MP3:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_MP3!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_MP3;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen     = 0;
                        aFormat->pCodecSpecificData     = 0;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_WMAV1:
                    case CODEC_ID_WMAV2:
                        CDX_LOGV("audio bitstream type: AUDIO_WMA!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_WMA_STANDARD;
                        aFormat->eSubCodecFormat = audiotrack->tag;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData     =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_WMAPRO:
                        CDX_LOGV("audio bitstream type: AUDIO_WMA!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_WMA_STANDARD;
                        aFormat->eSubCodecFormat = audiotrack->tag;
                        CDX_LOGD("---- CODEC_ID_WMAPRO codec_tag = %d", audiotrack->tag);
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_AC3:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_AC3!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_AC3;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen     = 0;
                        aFormat->pCodecSpecificData     = 0;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_DTS:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_DTS!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_DTS;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen     = 0;
                        aFormat->pCodecSpecificData     = 0;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_AAC:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_MPEG_AAC_LC!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_COOK:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_COOK!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_COOK;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_VORBIS:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_DTS!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_OGG;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_ATRAC3:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_ATRC!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_ATRC;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_FLAC:
                          CDX_LOGV("audio bitstream type: CDX_AUDIO_FLAC!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_FLAC;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_OPUS:
                        CDX_LOGD("audio bitstream type: CODEC_ID_OPUS!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_OPUS;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_SIPRO:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_SIPR!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_SIPR;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                          break;
                    /*case CODEC_ID_RA_288:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_RA!\n");
                        aFormat->eCodecFormat     = CDX_AUDIO_RA;
                        aFormat->eSubCodecFormat = 0;
                        aFormat->eAudioBitstreamSource     = CDX_MEDIA_FILE_FMT_MKV;
                        aFormat->nCodecSpecificDataLen     =
                                 matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData    =
                                 matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;*/
                    case CODEC_ID_PCM_S32LE: //32bit signed, little ending
                    case CODEC_ID_PCM_S24LE: //24bit signed, little ending
                    case CODEC_ID_PCM_S16LE:
                    case CODEC_ID_PCM_U16LE:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_PCM!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_PCM;
                        aFormat->eSubCodecFormat = WAVE_FORMAT_PCM | ABS_EDIAN_FLAG_LITTLE;
                        aFormat->nCodecSpecificDataLen     = 0;
                        aFormat->pCodecSpecificData     = 0;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_PCM_MULAW:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_PCM!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_PCM;
                        aFormat->eSubCodecFormat = WAVE_FORMAT_MULAW | ABS_EDIAN_FLAG_LITTLE;
                        aFormat->nCodecSpecificDataLen     = 0;
                        aFormat->pCodecSpecificData     = 0;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_PCM_ALAW:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_PCM!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_PCM;
                        aFormat->eSubCodecFormat = WAVE_FORMAT_ALAW | ABS_EDIAN_FLAG_LITTLE;
                        aFormat->nCodecSpecificDataLen     = 0;
                        aFormat->pCodecSpecificData     = 0;
                        p->nhasAudio ++;
                        break;
                    case CODEC_ID_ADPCM_MS:
                    case CODEC_ID_ADPCM_IMA_WAV:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_ADPCM!\n");
                        aFormat->eCodecFormat     = AUDIO_CODEC_FORMAT_PCM;
                        aFormat->eSubCodecFormat = WAVE_FORMAT_DVI_ADPCM | ABS_EDIAN_FLAG_LITTLE;
                        aFormat->nCodecSpecificDataLen =
                                matroska->streams[stream_index]->extradata_size;
                        aFormat->pCodecSpecificData =
                                matroska->streams[stream_index]->extradata;
                        p->nhasAudio ++;
                        break;
                    default:
                        CDX_LOGV("audio bitstream type: CDX_AUDIO_UNKNOWN!\n");
                        break;
                }
                if(p->nhasAudio >= MAX_AUDIO_STREAM_NUM)
                {
                    break;
                }
            }
        }

        if(p->nhasAudio == 0)
        {
            p->hasAudio = 0;
        }
        else
        {
            cdx_int32   i;
            //matroska->AudioTrackIndex = 2;
            for(i=0; i<p->nhasAudio; i++)
            {
                if(matroska->AudioTrackIndex == p->AudioTrackIndex[i])
                    break;
            }

            p->AudioStreamIndex = matroska->AudioStreamIndex
                = matroska->tracks[matroska->AudioTrackIndex]->stream_index;
            p->aFormat.nChannelNum           = p->aFormat_arry[i].nChannelNum;
            p->aFormat.nBitsPerSample        = p->aFormat_arry[i].nBitsPerSample;
            p->aFormat.nAvgBitrate           = p->aFormat_arry[i].nAvgBitrate;
            p->aFormat.nMaxBitRate              = p->aFormat_arry[i].nAvgBitrate;
            p->aFormat.nSampleRate           = p->aFormat_arry[i].nSampleRate;
            p->aFormat.eCodecFormat          = p->aFormat_arry[i].eCodecFormat;
            p->aFormat.eSubCodecFormat       = p->aFormat_arry[i].eSubCodecFormat;
            p->aFormat.eAudioBitstreamSource = p->aFormat_arry[i].eAudioBitstreamSource;
            p->aFormat.nCodecSpecificDataLen = p->aFormat_arry[i].nCodecSpecificDataLen;
            p->aFormat.pCodecSpecificData      = p->aFormat_arry[i].pCodecSpecificData;
            matroska->a_Header_size              = p->aFormat.nCodecSpecificDataLen;
            matroska->a_Header                  = (unsigned char*)p->aFormat.pCodecSpecificData;
        }
    }

    //if(p->hasSubTitle)
    //{
    //    p->tFormat.eCodecFormat = matroska->SubTitleCodecType[0];
    //    p->tFormat.eTextFormat = matroska->SubTitledata_encode_type[0];
    //}

    // check if the media file contain valid audio and video
    if(!p->hasVideo && !p->hasAudio)
    {
        CDX_LOGW("No valid audio or video bitstream, p->hasVideo = %d, p->hasAudio = %d!\n",
                p->hasVideo, p->hasAudio);
        return -1;
    }

#if 0
    // for aliyun 720p.mkv, whose video extradata is NULL,
    // so we must get the first video frame as extradata
    if((p->vFormat.eCodecFormat == VIDEO_CODEC_FORMAT_XVID) &&
       (p->vFormat.nCodecSpecificDataLen == 0))
    {

        int64_t offset = CdxStreamTell(matroska->fp);
        CdxStreamSeek(matroska->fp, 0, SEEK_SET);
        CDX_LOGD("------ here, offset = %llx", offset);

        unsigned int id;
        id = CdxStreamGetBE32(matroska->fp);

        //find the Cluster
        while(CdxStreamTell(matroska->fp) < 4096)
        {
            if(id == CDX_MATROSKA_ID_CLUSTER)
            {
                break;
            }

            id = (id<<8) | CdxStreamGetU8(matroska->fp);
        }

        if(id != CDX_MATROSKA_ID_CLUSTER)
        {
            goto seek_back;
        }

        cdx_uint64 length;
        int res = 0;
        cdx_uint64 total;
        cdx_uint32 read;
        cdx_uint64 rlength;
        unsigned int trackNum;
        char* buf = NULL;
        CDX_LOGD("--- offset: %llx", CdxStreamTell(matroska->fp));

        // read the length of cluster
        ebml_read_num(matroska, 8, &length);
        CDX_LOGD("cluster length = %llx", length);

        while (res == 0)
        {
            read = ebml_read_num(matroska, 4, &total);
            id = (cdx_uint32)(total | (1 << (read * 7)));
            CDX_LOGD("--- id = %x, total = %llx, read = %d, pos = %llx",
                     id, total, read, CdxStreamTell(matroska->fp));
            if (res < 0)
            {
                CDX_LOGE("--- id = %x", id);
                goto seek_back;
            }

            switch (id)
            {
                 // cluster timecode
                case CDX_MATROSKA_ID_CLUSTERTIMECODE:
                {
                    CDX_LOGD("--- TimeCode offset = %llx", CdxStreamTell(matroska->fp));
                    cdx_uint64 num;
                    if (( ebml_read_num(matroska, 8, &rlength)) < 0)
                        goto seek_back;

                    CdxStreamSeek(matroska->fp, rlength, SEEK_CUR);
                    CDX_LOGD("--- timecode length = %lld, offset = %llx",
                             rlength, CdxStreamTell(matroska->fp));
                    break;
                }

                case CDX_MATROSKA_ID_SIMPLEBLOCK:
                CDX_LOGD("################ simple block");
                    if ((res = ebml_read_num(matroska, 8, &rlength)) < 0)
                        goto seek_back;

                    buf = malloc(rlength);
                    CdxStreamRead(matroska->fp, buf, rlength);
                    trackNum = buf[0];
                    if(trackNum & 0x80)
                    {
                        trackNum &= 0x3f;
                        if(matroska->tracks[matroska->VideoTrackIndex]->num == trackNum)
                        {
                            matroska->priData = malloc(rlength - 4);
                            if(!matroska->priData)
                            {
                                CDX_LOGE("malloc failed");
                                goto seek_back;
                            }
                            matroska->priDataLen = rlength-4;
                            memcpy(matroska->priData, buf+4, rlength-4);
                            p->vFormat.nCodecSpecificDataLen = rlength-4;
                            p->vFormat.pCodecSpecificData = matroska->priData;

                            //CDX_BUF_DUMP(p->vFormat.pCodecSpecificData,
                            //p->vFormat.nCodecSpecificDataLen);
                            free(buf);
                            goto seek_back;
                        }
                    }
                    free(buf);
                    break;

                default:
                    res = ebml_read_skip(matroska);
                    break;
            }
        }

seek_back:
        CdxStreamSeek(matroska->fp, offset, SEEK_SET);
        CDX_LOGD("--- offset = %llx", offset);
    }
    #endif

    if(CdxStreamSeekAble(matroska->fp) && matroska->num_indexes == 0)
    {
        int64_t offset = CdxStreamTell(matroska->fp);
        MkvEstimateBR(p);
        CdxStreamSeek(matroska->fp, offset, SEEK_SET);
    }

    return 0;
}

/*
************************************************************************************************
*                           CREATE MKV struct cdx_stream_info READER
*
*Description: create mkv file reader.
*
*Arguments  : ret   create mkv file reader result;
*
*Return     : mkv file reader handle;
************************************************************************************************
*/
struct CdxMkvParser* CdxMatroskaInit(int *ret)
{
    MatroskaDemuxContext *matroska;
    struct CdxMkvParser* p;
    *ret = 0;
    p = (struct CdxMkvParser*)malloc(sizeof(struct CdxMkvParser));
    if(!p)
    {
        return NULL;
    }
    memset(p, 0, sizeof(struct CdxMkvParser));

    p->priv_data = NULL;
    matroska = (MatroskaDemuxContext *)malloc(sizeof(MatroskaDemuxContext));
    if(!matroska)
    {
        *ret = -1;
        return p;
    }
    memset(matroska, 0, sizeof(MatroskaDemuxContext));
    p->priv_data = matroska;
    p->mErrno = PSR_INVALID;
    memset(&p->packet, 0, sizeof(CdxPacketT));

    //use for seek keyframe added by fuqiang.
    matroska->tmpbufsize = 512*1024;
    matroska->tmpbuf = (unsigned char*)malloc (matroska->tmpbufsize);
    memset(matroska->tmpbuf,0,matroska->tmpbufsize);

    return p;
}

/*
**************************************************************************************************
*                           DESTROY MKV struct cdx_stream_info READER
*
*Description: destroy mkv file reader.
*
*Arguments  : p     mkv file reader handle;
*
*Return     : 0;
**************************************************************************************************
*/
int CdxMatroskaExit(struct CdxMkvParser *p)
{
    MatroskaDemuxContext *matroska;

    if(p)
    {
        matroska = (MatroskaDemuxContext *)p->priv_data;
        if(matroska)
        {
            free(p->priv_data);
            p->priv_data = NULL;
        }

        free(p);
    }

    return 0;
}

/*
**************************************************************************************************
*                       COLSE MKV struct cdx_stream_info READER
*
*Description: free system resource used by mkv file reader.
*
*Arguments  : p     mkv reader handle;
*
*Return     : 0;
**************************************************************************************************
*/
cdx_int16 CdxMatroskaClose(struct CdxMkvParser *p)
{
    if(p)
    {
        matroska_read_close(p);
    }
    return 0;
}

/*
*************************************************************************************************
*                       MKV FILE READER READ CHUNK INFORMATION
*
*Description: get chunk head information.
*
*Arguments  : p     mkv reader handle;
*
*Return     : 0;
*************************************************************************************************
*/
cdx_int16 CdxMatroskaRead(struct CdxMkvParser *p)
{
    MatroskaDemuxContext *matroska;
    //cdx_uint32           tmpLastKeyFrmPts;
    //cdx_uint32           tmpCurKeyFrmPts;
    //cdx_uint32           tmpDeltaTime;
    //cdx_uint32           tmpVideoTime;
    cdx_int32            ret = 0;
    int                 tmp;
    cdx_uint64           previous_pos = 0;
    cdx_int32            pos_flag = 0;

    if(!p)
    {
        return -1;
    }

    matroska = (MatroskaDemuxContext *)p->priv_data;
    if(!matroska)
    {
        return -1;
    }

    do
    {
        if(ret!=0)
        {
            if(p->hasIndex)
            {
                matroska->time_stamp_offset -=
                        matroska->streams[matroska->VideoStreamIndex]->pts/1000;
                ret = matroska_find_keyframe(p, 0, 2, 0);
                if(ret==0)
                {
                    if(CdxStreamSeek(matroska->fp, p->pos, SEEK_SET) < 0)
                        return -1;
                }
                else if(ret == 1)
                {
                    CDX_LOGD("&&&& eos");
                    return AVERROR_EOS;
                }
                matroska->time_stamp_error = 1;
            }
        }
        //if(ret!=0)
        //    return ret;
        p->nFRPicCnt = 0;

        //read cluster
        ret = matroska_read_packet(p);
        if(ret < 0)
        {
            if(FEOF(matroska))
            {
                return AVERROR_EOS;
            }

            if(p->exitFlag == 1)
            {
                return AVERROR_IO;
            }
            CDX_LOGD("-- resync");
            tmp = MkvResync(p);
            cdx_uint64 cur_pos = CdxStreamTell(matroska->fp);
            if(pos_flag == 0)
            {
                previous_pos = CdxStreamTell(matroska->fp);
                pos_flag = 1;
            }
            else
            {
                if(cur_pos == previous_pos)
                {
                    return AVERROR_EOS;
                }
                else
                {
                    pos_flag = 0;
                }
            }
            if(tmp < 0)
            {
                if(tmp == -2)
                {
                    CDX_LOGE("resync io error");
                    return AVERROR_IO;
                }

                if(FEOF(matroska))
                {
                    return AVERROR_EOS;
                }
                else
                {
                    CDX_LOGE("resync io error");
                    return AVERROR_IO;
                }
            }

            //check it again, because  maybe eos in resync
            if(FEOF(matroska))
            {
                return AVERROR_EOS;
            }
        }
    }while(ret != 0);

    return ret;
}

int CdxMatroskaSeek(struct CdxMkvParser *p, cdx_int64  timeUs)
{
    MatroskaDemuxContext *matroska;
    cdx_int32 result;

    if(!p)
    {
        return -1;
    }
    matroska = (MatroskaDemuxContext *)p->priv_data;
    if(!matroska)
    {
        return -1;
    }

    timeUs /= 1000;

    CDX_LOGD("============= seek to: %lldms  total: %lld\n", timeUs, matroska->segment_duration);
    if(timeUs < 0 || timeUs >= matroska->segment_duration)
    {
        CDX_LOGW("The parameter for jump play is invalid, timeUs<%lld ms>!\n", timeUs);
        return -1;
    }

    if(matroska->num_indexes > 0)
    {
        result = matroska_find_keyframe(p, 0, 0, timeUs);

        if(result < 0)
        {
            CDX_LOGW("look for key frame failed! try another way to get key frame");
            result = mkv_seek_keyframe(p, timeUs);
            if(result < 0)
            {
                CDX_LOGW("look for key frame failed!\n");
                return -1;
            }
        }
        CdxStreamSeek(matroska->fp, p->pos, SEEK_SET);
    }
    else
    {
        cdx_uint64        jump_file_pos=0;
        AVStream *st = matroska->streams[matroska->VideoStreamIndex];

        CDX_LOGD("%lld ms , %lld ms , FileEndPst %lld",
                timeUs, matroska->segment_duration, matroska->fileEndPst);
        jump_file_pos = matroska->fileEndPst * timeUs / matroska->segment_duration;
        CDX_LOGD("jump_file_pos %lld(0x%llx)", jump_file_pos, jump_file_pos);

        CdxStreamSeek(matroska->fp, jump_file_pos, SEEK_SET);

        unsigned int id;
        int      ret;
        id = CdxStreamGetBE32(matroska->fp);
        //find the Cluster
        ret = findCluster(p,id);
        if(ret < 0)
        {
            return ret;
        }

        CdxStreamSeek(matroska->fp, -4, SEEK_CUR);
        cdx_uint64 tell = CdxStreamTell(matroska->fp);
        CDX_LOGD("id = %x, pos = %lld(%llx)", id, tell, tell);
        if(matroska->fileEndPst <= MAX_FILE_LENGTH)
        {
            matroska_parse_cluster_time(matroska,timeUs);
        }

        // do the seek
        matroska->num_levels = matroska->num_levels_bak;
        matroska->peek_id = 0;
        matroska->frame_num_between_kframe = 1;

        if (timeUs != 0)
            matroska->skip_to_keyframe = 1;

        matroska->skip_to_stream = st;
        matroska->peek_id = 0;
        matroska->laces = 0;
        matroska->lace_cnt = 0;
        matroska->data_rdy = 0;
        matroska->parsing_cluster=0;
    }

    matroska->sub_packet_cnt = 0;
    matroska->time_stamp_error = 0;
    matroska->time_stamp_offset = 0;

    return 0;
}

