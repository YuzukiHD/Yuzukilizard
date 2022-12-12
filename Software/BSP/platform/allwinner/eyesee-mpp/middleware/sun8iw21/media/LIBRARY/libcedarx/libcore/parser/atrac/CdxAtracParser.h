/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxAtracParser.h
* Description :
* History :
*   Author  : lszhang <lszhang@allwinnertech.com>
*   Date    : 2014/08/08
*/

#ifndef _CDX_ATRAC_PARSER_H_
#define _CDX_ATRAC_PARSER_H_

#define ENABLE_INFO_DEBUG   1
#define ENABLE_FILE_DEBUG   0
#define ID3v2_HEADER_SIZE   10
#define EA3_HEADER_SIZE     96
#define ID3v2_EA3_MAGIC     "ea3"
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))

enum {
    OMA_CODECID_ATRAC3  = 0,
    OMA_CODECID_ATRAC3P = 1,
};

typedef struct ATRACParserImpl
{
    // Cdx Struct
    CdxParserT      base;
    CdxStreamT      *stream;

    int             mStatus;
    int             mErrno;    // errno
    int             exitFlag;

    cdx_int32       mHeadSize;
    cdx_int32       mTaglen;
    cdx_int32       mChannels;
    cdx_int32       mSampleRate;
    cdx_int32       mBitrate;
    cdx_int32       mFrameSize;
    cdx_bool        mFirstFrame;
    cdx_int64       mFileSize;
    cdx_int64       mDuration;
    cdx_int64       mSeekTime;
    cdx_bool        mSeeking;

    pthread_cond_t  cond;
    int             teeFd;
} ATRACParserImpl;

#endif
