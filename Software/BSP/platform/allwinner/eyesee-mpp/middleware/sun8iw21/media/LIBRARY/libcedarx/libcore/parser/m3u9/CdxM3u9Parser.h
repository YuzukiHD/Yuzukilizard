/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxM3u9Parser.h
 * Description : Part of m3u9 parser.
 * History :
 *
 */

#ifndef M3U9_PARSER_H
#define M3U9_PARSER_H
#include "M3U9Parser.h"
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>

#define _SAVE_VIDEO_STREAM (0)
#define _SAVE_AUDIO_STREAM (0)

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

typedef struct CdxM3u9Parser
{
    CdxParserT base;
    enum CdxParserStatus status;
    int mErrno;
    cdx_uint32 flags;/*使能标志*/
    
    int forceStop;          /* for forceStop()*/
    pthread_mutex_t statusLock;
    pthread_cond_t cond;
    
    CdxStreamT *cdxStream;/*对应于生成该CdxHlsParser的m3u8文件*/
    CdxDataSourceT cdxDataSource;/*用途1:playlist的更新，用途2:child打开*/
    char *m3u9Url;/*用于playlist的更新*/
    char *m3u9Buf;/*用于m3u8文件的下载*/
    cdx_int64 m3u9BufSize;
    
    Playlist* mPlaylist;
    int curSeqNum;
    cdx_int64 baseTimeUs;

    CdxParserT *child;/*为子parser，下一级的媒体文件parser，如flv parser*/
    CdxStreamT *childStream;

    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;

#if _SAVE_VIDEO_STREAM
    FILE* fpVideoStream;
#endif
#if _SAVE_AUDIO_STREAM
    FILE* fpAudioStream;
#endif
#if _SAVE_AUDIO_STREAM || _SAVE_VIDEO_STREAM
    char url[256];
#endif
}CdxM3u9Parser;

#endif
