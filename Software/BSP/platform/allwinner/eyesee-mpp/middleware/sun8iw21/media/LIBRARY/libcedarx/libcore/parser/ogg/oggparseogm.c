/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : oggparseogm.c
 * Description : oggparseogm
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "ParseOgm"
#include "CdxOggParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>

static AVCodecTag ff_codec_bmp_tags[] = {
    { CDX_CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
    { CDX_CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
    { CDX_CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
    { CDX_CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
    { CDX_CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
    { CDX_CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
    { CDX_CODEC_ID_H263,         MKTAG('H', '2', '6', '3') },
    { CDX_CODEC_ID_H263,         MKTAG('X', '2', '6', '3') },
    { CDX_CODEC_ID_H263,         MKTAG('T', '2', '6', '3') },
    { CDX_CODEC_ID_H263,         MKTAG('L', '2', '6', '3') },
    { CDX_CODEC_ID_H263,         MKTAG('V', 'X', '1', 'K') },
    { CDX_CODEC_ID_H263,         MKTAG('Z', 'y', 'G', 'o') },
    { CDX_CODEC_ID_H263P,        MKTAG('H', '2', '6', '3') },
    { CDX_CODEC_ID_H263I,        MKTAG('I', '2', '6', '3') }, /* intel h263 */
    { CDX_CODEC_ID_H261,         MKTAG('H', '2', '6', '1') },
    { CDX_CODEC_ID_H263P,        MKTAG('U', '2', '6', '3') },
    { CDX_CODEC_ID_H263P,        MKTAG('v', 'i', 'v', '1') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', 'X') },
    { CDX_CODEC_ID_DIVX5,        MKTAG('D', 'X', '5', '0') },
    { CDX_CODEC_ID_XVID,         MKTAG('X', 'V', 'I', 'D') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
    { CDX_CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  0 ) }, /* some broken avi use this */
    { CDX_CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('3', 'I', 'V', '2') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('F', 'F', 'D', 'S') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('F', 'V', 'F', 'W') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('D', 'C', 'O', 'D') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('M', 'V', 'X', 'M') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('P', 'M', '4', 'V') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('S', 'M', 'P', '4') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('D', 'X', 'G', 'M') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('V', 'I', 'D', 'M') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('M', '4', 'T', '3') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'X') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('H', 'D', 'X', '4') }, /* flipped video */
    { CDX_CODEC_ID_MPEG4,        MKTAG('D', 'M', 'K', '2') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('D', 'I', 'G', 'I') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('I', 'N', 'M', 'C') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('E', 'P', 'H', 'V') }, /* Ephv MPEG-4 */
    { CDX_CODEC_ID_MPEG4,        MKTAG('E', 'M', '4', 'A') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('M', '4', 'C', 'C') }, /* Divio MPEG-4 */
    { CDX_CODEC_ID_MPEG4,        MKTAG('S', 'N', '4', '0') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('V', 'S', 'P', 'X') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('U', 'L', 'D', 'X') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'V') },
    { CDX_CODEC_ID_MPEG4,        MKTAG('S', 'I', 'P', 'P') }, /* Samsung SHR-6040 */
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '3') }, /* default signature
                                                                 when using MSMPEG4 */
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', '4', '3') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', 'G', '3') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '5') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '6') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '4') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('D', 'V', 'X', '3') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('A', 'P', '4', '1') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '1') },
    { CDX_CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '0') },
    { CDX_CODEC_ID_MSMPEG4V2,    MKTAG('M', 'P', '4', '2') },
    { CDX_CODEC_ID_MSMPEG4V2,    MKTAG('D', 'I', 'V', '2') },
    { CDX_CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', 'G', '4') },
    { CDX_CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', '4', '1') },
    { CDX_CODEC_ID_WMV1,         MKTAG('W', 'M', 'V', '1') },
    { CDX_CODEC_ID_WMV2,         MKTAG('W', 'M', 'V', '2') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'd') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', 'd') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'l') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '2', '5') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '5', '0') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('c', 'd', 'v', 'c') }, /* Canopus DV */
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('C', 'D', 'V', 'H') }, /* Canopus DV */
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', ' ') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', 's') },
    { CDX_CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
    { CDX_CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '1') },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('m', 'p', 'g', '2') },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'P', 'E', 'G') },
    { CDX_CODEC_ID_MPEG1VIDEO,   MKTAG('P', 'I', 'M', '1') },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('P', 'I', 'M', '2') },
    { CDX_CODEC_ID_MPEG1VIDEO,   MKTAG('V', 'C', 'R', '2') },
    { CDX_CODEC_ID_MPEG1VIDEO,   MKTAG( 1 ,  0 ,  0 ,  16) },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG( 2 ,  0 ,  0 ,  16) },
    { CDX_CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  16) },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('D', 'V', 'R', ' ') },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'M', 'E', 'S') },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('L', 'M', 'P', '2') }, /* Lead MPEG2 in avi */
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('s', 'l', 'i', 'f') },
    { CDX_CODEC_ID_MPEG2VIDEO,   MKTAG('E', 'M', '2', 'V') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('d', 'm', 'b', '1') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('m', 'j', 'p', 'a') },
    { CDX_CODEC_ID_LJPEG,        MKTAG('L', 'J', 'P', 'G') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') }, /* Pegasus lossless JPEG */
    { CDX_CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC
                                                                 for avi - encoder */
    { CDX_CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC
                                                                 for avi - decoder */
    { CDX_CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('A', 'V', 'R', 'n') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('A', 'C', 'D', 'V') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('Q', 'I', 'V', 'G') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('S', 'L', 'M', 'J') }, /* SL M-JPEG */
    { CDX_CODEC_ID_MJPEG,        MKTAG('C', 'J', 'P', 'G') }, /* Creative Webcam JPEG */
    { CDX_CODEC_ID_MJPEG,        MKTAG('I', 'J', 'L', 'V') }, /* Intel JPEG Library Video Codec */
    { CDX_CODEC_ID_MJPEG,        MKTAG('M', 'V', 'J', 'P') }, /* Midvid JPEG Video Codec */
    { CDX_CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '1') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '2') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('M', 'T', 'S', 'J') },
    { CDX_CODEC_ID_MJPEG,        MKTAG('Z', 'J', 'P', 'G') }, /* Paradigm Matrix M-JPEG Codec */
    { CDX_CODEC_ID_HUFFYUV,      MKTAG('H', 'F', 'Y', 'U') },
    { CDX_CODEC_ID_FFVHUFF,      MKTAG('F', 'F', 'V', 'H') },
    { CDX_CODEC_ID_CYUV,         MKTAG('C', 'Y', 'U', 'V') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG( 0 ,  0 ,  0 ,  0 ) },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG( 3 ,  0 ,  0 ,  0 ) },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('I', '4', '2', '0') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'Y', '2') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', '2') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('V', '4', '2', '2') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'N', 'V') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'V') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'Y') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('u', 'y', 'v', '1') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('2', 'V', 'u', '1') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('2', 'v', 'u', 'y') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('P', '4', '2', '2') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', '1', '2') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'V', 'Y') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('V', 'Y', 'U', 'Y') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('I', 'Y', 'U', 'V') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('Y', '8', '0', '0') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('H', 'D', 'Y', 'C') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', 'U', '9') },
    { CDX_CODEC_ID_RAWVIDEO,     MKTAG('V', 'D', 'T', 'Z') }, /* SoftLab-NSK VideoTizer */
    //{ CDX_CODEC_ID_FRWU,         MKTAG('F', 'R', 'W', 'U') },
    //{ CDX_CODEC_ID_R210,         MKTAG('r', '2', '1', '0') },
    //{ CDX_CODEC_ID_V210,         MKTAG('v', '2', '1', '0') },
    { CDX_CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '1') },
    { CDX_CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '2') },
    { CDX_CODEC_ID_INDEO4,       MKTAG('I', 'V', '4', '1') },
    { CDX_CODEC_ID_INDEO5,       MKTAG('I', 'V', '5', '0') },
    { CDX_CODEC_ID_VP3,          MKTAG('V', 'P', '3', '1') },
    { CDX_CODEC_ID_VP3,          MKTAG('V', 'P', '3', '0') },
    { CDX_CODEC_ID_VP5,          MKTAG('V', 'P', '5', '0') },
    { CDX_CODEC_ID_VP6,          MKTAG('V', 'P', '6', '0') },
    { CDX_CODEC_ID_VP6,          MKTAG('V', 'P', '6', '1') },
    { CDX_CODEC_ID_VP6,          MKTAG('V', 'P', '6', '2') },
    { CDX_CODEC_ID_VP6F,         MKTAG('V', 'P', '6', 'F') },
    { CDX_CODEC_ID_VP6F,         MKTAG('F', 'L', 'V', '4') },
    { CDX_CODEC_ID_VP8,          MKTAG('V', 'P', '8', '0') },
    { CDX_CODEC_ID_ASV1,         MKTAG('A', 'S', 'V', '1') },
    { CDX_CODEC_ID_ASV2,         MKTAG('A', 'S', 'V', '2') },
    { CDX_CODEC_ID_VCR1,         MKTAG('V', 'C', 'R', '1') },
    { CDX_CODEC_ID_FFV1,         MKTAG('F', 'F', 'V', '1') },
    { CDX_CODEC_ID_XAN_WC4,      MKTAG('X', 'x', 'a', 'n') },
    { CDX_CODEC_ID_MIMIC,        MKTAG('L', 'M', '2', '0') },
    { CDX_CODEC_ID_MSRLE,        MKTAG('m', 'r', 'l', 'e') },
    { CDX_CODEC_ID_MSRLE,        MKTAG( 1 ,  0 ,  0 ,  0 ) },
    { CDX_CODEC_ID_MSRLE,        MKTAG( 2 ,  0 ,  0 ,  0 ) },
    { CDX_CODEC_ID_MSVIDEO1,     MKTAG('M', 'S', 'V', 'C') },
    { CDX_CODEC_ID_MSVIDEO1,     MKTAG('m', 's', 'v', 'c') },
    { CDX_CODEC_ID_MSVIDEO1,     MKTAG('C', 'R', 'A', 'M') },
    { CDX_CODEC_ID_MSVIDEO1,     MKTAG('c', 'r', 'a', 'm') },
    { CDX_CODEC_ID_MSVIDEO1,     MKTAG('W', 'H', 'A', 'M') },
    { CDX_CODEC_ID_MSVIDEO1,     MKTAG('w', 'h', 'a', 'm') },
    { CDX_CODEC_ID_CINEPAK,      MKTAG('c', 'v', 'i', 'd') },
    { CDX_CODEC_ID_TRUEMOTION1,  MKTAG('D', 'U', 'C', 'K') },
    { CDX_CODEC_ID_TRUEMOTION1,  MKTAG('P', 'V', 'E', 'Z') },
    { CDX_CODEC_ID_MSZH,         MKTAG('M', 'S', 'Z', 'H') },
    { CDX_CODEC_ID_ZLIB,         MKTAG('Z', 'L', 'I', 'B') },
    { CDX_CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
    { CDX_CODEC_ID_4XM,          MKTAG('4', 'X', 'M', 'V') },
    { CDX_CODEC_ID_FLV1,         MKTAG('F', 'L', 'V', '1') },
    { CDX_CODEC_ID_FLASHSV,      MKTAG('F', 'S', 'V', '1') },
    { CDX_CODEC_ID_SVQ1,         MKTAG('s', 'v', 'q', '1') },
    { CDX_CODEC_ID_TSCC,         MKTAG('t', 's', 'c', 'c') },
    { CDX_CODEC_ID_ULTI,         MKTAG('U', 'L', 'T', 'I') },
    { CDX_CODEC_ID_VIXL,         MKTAG('V', 'I', 'X', 'L') },
    { CDX_CODEC_ID_QPEG,         MKTAG('Q', 'P', 'E', 'G') },
    { CDX_CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '0') },
    { CDX_CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '1') },
    { CDX_CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', '3') },
    { CDX_CODEC_ID_VC1,          MKTAG('W', 'V', 'C', '1') },
    { CDX_CODEC_ID_VC1,          MKTAG('W', 'M', 'V', 'A') },
    { CDX_CODEC_ID_LOCO,         MKTAG('L', 'O', 'C', 'O') },
    { CDX_CODEC_ID_WNV1,         MKTAG('W', 'N', 'V', '1') },
    { CDX_CODEC_ID_AASC,         MKTAG('A', 'A', 'S', 'C') },
    { CDX_CODEC_ID_INDEO2,       MKTAG('R', 'T', '2', '1') },
    { CDX_CODEC_ID_FRAPS,        MKTAG('F', 'P', 'S', '1') },
    { CDX_CODEC_ID_THEORA,       MKTAG('t', 'h', 'e', 'o') },
    { CDX_CODEC_ID_TRUEMOTION2,  MKTAG('T', 'M', '2', '0') },
    { CDX_CODEC_ID_CSCD,         MKTAG('C', 'S', 'C', 'D') },
    { CDX_CODEC_ID_ZMBV,         MKTAG('Z', 'M', 'B', 'V') },
    { CDX_CODEC_ID_KMVC,         MKTAG('K', 'M', 'V', 'C') },
    { CDX_CODEC_ID_CAVS,         MKTAG('C', 'A', 'V', 'S') },
    { CDX_CODEC_ID_JPEG2000,     MKTAG('M', 'J', '2', 'C') },
    { CDX_CODEC_ID_VMNC,         MKTAG('V', 'M', 'n', 'c') },
    { CDX_CODEC_ID_TARGA,        MKTAG('t', 'g', 'a', ' ') },
    { CDX_CODEC_ID_PNG,          MKTAG('M', 'P', 'N', 'G') },
    { CDX_CODEC_ID_PNG,          MKTAG('P', 'N', 'G', '1') },
    { CDX_CODEC_ID_CLJR,         MKTAG('c', 'l', 'j', 'r') },
    { CDX_CODEC_ID_DIRAC,        MKTAG('d', 'r', 'a', 'c') },
    { CDX_CODEC_ID_RPZA,         MKTAG('a', 'z', 'p', 'r') },
    { CDX_CODEC_ID_RPZA,         MKTAG('R', 'P', 'Z', 'A') },
    { CDX_CODEC_ID_RPZA,         MKTAG('r', 'p', 'z', 'a') },
    { CDX_CODEC_ID_SP5X,         MKTAG('S', 'P', '5', '4') },
    //{ CDX_CODEC_ID_AURA,         MKTAG('A', 'U', 'R', 'A') },
    //{ CDX_CODEC_ID_AURA2,        MKTAG('A', 'U', 'R', '2') },
    //{ CDX_CODEC_ID_DPX,          MKTAG('d', 'p', 'x', ' ') },
    //{ CDX_CODEC_ID_KGV1,         MKTAG('K', 'G', 'V', '1') },
    { CDX_CODEC_ID_NONE,         0 }
};

int cdx_vorbis_comment(CdxOggParser *ogg, AVMetadata **m, const cdx_uint8 *buf, int size);

static cdx_uint32 bytestream_get_buffer(const cdx_uint8 **b, cdx_uint8 *dst, cdx_uint32 size)
{
    memcpy(dst, *b, size);
    (*b) += size;
    return size;
}

static int
ogm_header(CdxOggParser *ogg, int idx)
{
    struct ogg_stream *os = ogg->streams + idx;
    AVStream *st = os->stream;
    const cdx_uint8 *p = os->buf + os->pstart;
    uint64_t time_unit;
    uint64_t spu;
    uint32_t size;

    if(!(*p & 1))
        return 0;
    if(*p == 1) {
        p++;

        if(*p == 'v'){
            int tag_os;
            st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
            p += 8;
            tag_os = GetLE32Bits(p);//TODO
            st->codec->codec_id = cdx_codec_get_id(ff_codec_bmp_tags, tag_os);
            st->codec->codec_tag = tag_os;
            CDX_LOGD("codec_tag = 0x%x, codec_id = %d", tag_os, st->codec->codec_id);
            ogg->hasVideo =1;
        } else if (*p == 't') {
            st->codec->codec_id = CDX_CODEC_ID_TEXT;//TODO
            st->codec->codec_type = AVMEDIA_TYPE_SUBTITLE;
            p += 12;
            ogg->hasSubTitle =1;
        } else {
            uint8_t acid[5];
            int cid_os;
            st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
            p += 8;
            bytestream_get_buffer(&p, acid, 4);
            acid[4] = 0;
            cid_os = strtol((const char *)acid, NULL, 16);
            st->codec->codec_id = cdx_codec_get_id(ff_codec_wav_tags, cid_os);
            // our parser completely breaks AAC in Ogg
            if (st->codec->codec_id != CDX_CODEC_ID_AAC)
                st->need_parsing = AVSTREAM_PARSE_OS_FULL;

            ogg->hasAudio =1;
        }

        size        = GetLE32Bits(p);
        size        = FFMIN(size, os->psize);
        time_unit   = GetLE64Bits(p);
        spu         = GetLE64Bits(p);
        p += 4;    /* default_len */
        p += 8;    /* buffersize + bits_per_sample */

        if(AVMEDIA_TYPE_VIDEO == st->codec->codec_type){
            st->codec->width = GetLE32Bits(p);
            st->codec->height = GetLE32Bits(p);
            st->codec->time_base.num = time_unit;
            st->codec->time_base.den = spu * 10000000;
            st->codec->nFrameDuration =
                st->codec->time_base.num * 1000000LL/ st->codec->time_base.den;
            st->time_base = st->codec->time_base;
        } else {
            st->codec->channels = GetLE16Bits(p);
            p += 2;                 /* block_align */
            st->codec->bit_rate = GetLE32Bits(p) * 8;
            st->codec->sample_rate = time_unit ? spu * 10000000 / time_unit : 0;
            st->time_base.num = 1;
            st->time_base.den = st->codec->sample_rate;
        }
    } else if (*p == 3) {
        if (os->psize > 8)
            cdx_vorbis_comment(ogg, &st->metadata, p+7, (os->psize-8));
    }

    return 1;
}

static int
ogm_packet(CdxOggParser *ogg, int idx_os)
{
    struct ogg_stream *os = ogg->streams + idx_os;
    uint8_t *p = os->buf + os->pstart;
    int lb_os;

    if(*p & 8)
        os->pflags |= KEY_FRAME;

    lb_os = ((*p & 2) << 1) | ((*p >> 6) & 3);
    os->pstart += lb_os + 1;
    os->psize -= lb_os + 1;

    while (lb_os--)
        os->pduration += p[lb_os+1] << (lb_os*8);

    return 0;
}

const struct ogg_codec cdx_ff_ogm_video_codec = {
    .magic = "\001video",
    .magicsize = 6,
    .packet = ogm_packet,
    .header = ogm_header,
    .granule_is_start = 1,
    .nb_header = 2,
};

const struct ogg_codec cdx_ff_ogm_audio_codec = {
    .magic = "\001audio",
    .magicsize = 6,
    .packet = ogm_packet,
    .header = ogm_header,
    .granule_is_start = 1,
    .nb_header = 2,
};

const struct ogg_codec cdx_ff_ogm_text_codec = {
    .magic = "\001text",
    .magicsize = 5,
    .packet = ogm_packet,
    .header = ogm_header,
    .granule_is_start = 1,
    .nb_header = 2,
};

