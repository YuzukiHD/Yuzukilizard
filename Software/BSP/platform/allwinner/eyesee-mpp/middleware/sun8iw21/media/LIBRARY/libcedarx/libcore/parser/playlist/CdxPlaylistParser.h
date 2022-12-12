/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxPlaylistParser.h
 * Description : Part of play list Parser.
 * History :
 *
 */

#ifndef PLAYLIST_PARSER_H
#define PLAYLIST_PARSER_H
#include "PlaylistParser.h"
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

typedef struct CdxPlaylistParser
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
    char *playlistUrl;/*用于playlist的更新*/
    char *playlistBuf;/*用于m3u8文件的下载*/
    cdx_int64 playlistBufSize;

    Playlist* mPlaylist;
    int curSeqNum;
    cdx_int64 baseTimeUs;

    CdxParserT *child;/*为子parser，下一级的媒体文件parser，如flv parser*/
    CdxStreamT *childStream;
    CdxParserT *tmpChild;
    CdxStreamT *tmpChildStream;

    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;
}CdxPlaylistParser;

#endif
