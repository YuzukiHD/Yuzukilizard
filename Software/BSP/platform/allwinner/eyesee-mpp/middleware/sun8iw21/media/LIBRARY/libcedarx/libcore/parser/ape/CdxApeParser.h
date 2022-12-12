/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxApeParser.h
* Description :
* History :
*   Author  : Wenju Lin <linwenju@allwinnertech.com>
*   Date    : 2014/08/08
*/

#ifndef _CDX_APE_PARSER_H_
#define _CDX_APE_PARSER_H_

#define ENABLE_INFO_DEBUG       1
#define ENABLE_FILE_DEBUG       0
#define AV_TIME_BASE            1000000

/* The earliest and latest file formats supported by this library */
#define AW_APE_MIN_VERSION 3950
#define AW_APE_MAX_VERSION 3990

// is 8-bit [OBSOLETE]
#define APE_MAC_FORMAT_FLAG_8_BIT                 1
// uses the new CRC32 error detection [OBSOLETE]
#define APE_MAC_FORMAT_FLAG_CRC                   2
// uint32 nPeakLevel after the header [OBSOLETE]
#define APE_MAC_FORMAT_FLAG_HAS_PEAK_LEVEL        4
// is 24-bit [OBSOLETE]
#define APE_MAC_FORMAT_FLAG_24_BIT                8
// has the number of seek elements after the peak level
#define APE_MAC_FORMAT_FLAG_HAS_SEEK_ELEMENTS    16
// create the wave header on decompression (not stored)
#define APE_MAC_FORMAT_FLAG_CREATE_WAV_HEADER    32

#define APE_MAC_SUBFRAME_SIZE 4608

#define APE_EXTRADATA_SIZE 16

/* APE tags */
#define APE_TAG_VERSION               2000
#define APE_TAG_FOOTER_BYTES          32
#define APE_TAG_FLAG_CONTAINS_HEADER  (1 << 31)
#define APE_TAG_FLAG_IS_HEADER        (1 << 29)

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))

typedef struct {
    cdx_int64   pos;
    int         nblocks;
    int         size;
    int         skip;
    cdx_int64   pts;
} APEFrame;

typedef struct APEParserImpl
{
    // Cdx Struct
    CdxParserT      base;
    CdxStreamT      *stream;

    int             mStatus;
    int             mErrno;    // errno
    int             exitFlag;

    pthread_cond_t  cond;
    /* Derived fields */
    cdx_uint32    junklength;
    cdx_uint32    firstframe;
    cdx_uint32    totalsamples;
    cdx_uint32    currentframe;
    cdx_uint32    seek_flag;
    APEFrame    *frames;

    /* Info from Descriptor Block */
    char          magic[4];
    cdx_int16     fileversion;
    cdx_int16     padding1;
    cdx_uint32    descriptorlength;
    cdx_uint32    headerlength;
    cdx_uint32    seektablelength;
    cdx_uint32    wavheaderlength;
    cdx_uint32    audiodatalength;
    cdx_uint32    audiodatalength_high;
    cdx_uint32    wavtaillength;
    cdx_uint8       md5[16];

    /* Info from Header Block */
    cdx_uint16    compressiontype;
    cdx_uint16    formatflags;
    cdx_uint32    blocksperframe;
    cdx_uint32    finalframeblocks;
    cdx_uint32    totalframes;
    cdx_uint16    bps;
    cdx_uint16    channels;
    cdx_uint32    samplerate;

    /* Seektable */
    cdx_uint32    *seektable;

    /*Duration*/
    cdx_uint64  duration;
    cdx_int32   totalblock;
    cdx_int32   bitrate;
    cdx_int32   nBlockAlign;

    cdx_int32   nseeksession;
    cdx_int32   nheadframe;
    int         teeFd;

    int         extrasize;
    cdx_uint8  *extradata;
} APEParserImpl;

#endif
