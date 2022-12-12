/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxPlaylistParser.c
 * Description : Play list parser implementation.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxPlaylistParser"
#include "CdxPlaylistParser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <string.h>

static inline void SetDataSouceForSegment(PlaylistItem *item, CdxDataSourceT *cdxDataSource)
{
    cdxDataSource->uri = item->mURI;
}

/*return -1:error*/
static int DownloadParsePlaylist(CdxPlaylistParser *playlistParser)
{
    memset(playlistParser->playlistBuf, 0, playlistParser->playlistBufSize);

    int ret, readSize = 0;
    while(1)
    {
        if(playlistParser->forceStop == 1)
        {
            return -1;
        }
        ret = CdxStreamRead(playlistParser->cdxStream,
            playlistParser->playlistBuf + readSize, 1024);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            return -1;
        }
        if(ret == 0)
        {
            break;
        }
        readSize += ret;
    }

    if(readSize == 0)
    {
        CDX_LOGE("download playlist fail");
        return -1;
    }

    Playlist *playlist = NULL;
    status_t err;
    if ((err = PlaylistParse(playlistParser->playlistBuf, readSize, &playlist,
        playlistParser->playlistUrl)) != OK)
    {
        CDX_LOGE("creation playlist fail, err=%d", err);
        return -1;
    }
    playlistParser->mPlaylist = playlist;
    return 0;
}

/*return -1:error*/
/*return 0:playlistParser PSR_EOS*/
/*return 1:选择到了新的Segment*/
static int SelectNextSegment(CdxPlaylistParser *playlistParser)
{
    Playlist *playlist = playlistParser->mPlaylist;
    if(playlistParser->curSeqNum > playlist->lastSeqNum)
    {
        return 0;
    }
    PlaylistItem *item = findItemBySeqNumForPlaylist(playlist, playlistParser->curSeqNum);
    if(!item)
    {
        CDX_LOGE("findItemBySeqNum fail");
        return -1;
    }
    SetDataSouceForSegment(item, &playlistParser->cdxDataSource);
    playlistParser->baseTimeUs = item->baseTimeUs;
    return 1;

}

static cdx_int32 PlaylistParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser*)parser;
    if(playlistParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGE("status < CDX_PSR_IDLE, PlaylistParserGetMediaInfo invaild");
        playlistParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &playlistParser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}

static cdx_int32 PlaylistParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser*)parser;
    if(playlistParser->status != CDX_PSR_IDLE && playlistParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_IDLE && status != CDX_PSR_PREFETCHED,"
            " PlaylistParserPrefetch invaild");
        playlistParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(playlistParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(playlistParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &playlistParser->cdxPkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&playlistParser->statusLock);
    if(playlistParser->forceStop)
    {
        pthread_mutex_unlock(&playlistParser->statusLock);
        return -1;
    }
    playlistParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&playlistParser->statusLock);

    memset(pkt, 0, sizeof(CdxPacketT));
    int ret, mErrno;
    ret = CdxParserPrefetch(playlistParser->child, pkt);
    if(ret < 0)
    {
        mErrno = CdxParserGetStatus(playlistParser->child);
        if(mErrno == PSR_EOS)
        {
            pthread_mutex_lock(&playlistParser->statusLock);
            if(playlistParser->forceStop)
            {
                playlistParser->mErrno = PSR_USER_CANCEL;
                ret = -1;
                pthread_mutex_unlock(&playlistParser->statusLock);
                goto _exit;
            }
            ret = CdxParserClose(playlistParser->child);
            CDX_CHECK(ret == 0);
            playlistParser->child = NULL;
            playlistParser->childStream = NULL;
            pthread_mutex_unlock(&playlistParser->statusLock);

            playlistParser->curSeqNum++;
            CDX_LOGI("curSeqNum = %d", playlistParser->curSeqNum);
            ret = SelectNextSegment(playlistParser);
            if(ret < 0)
            {
                CDX_LOGE("SelectNextSegment fail");
                playlistParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
            else if(ret == 0)
            {
                CDX_LOGD("playlistParser->status = PSR_EOS");
                playlistParser->mErrno = PSR_EOS;
                ret = -1;
                goto _exit;
            }
            CDX_LOGD("replace parser");
            ret = CdxParserPrepare(&playlistParser->cdxDataSource, NO_NEED_DURATION,
                &playlistParser->statusLock, &playlistParser->forceStop,
                &playlistParser->child, &playlistParser->childStream, NULL, NULL);
            if(ret < 0)
            {
                playlistParser->curSeqNum = -1;
                if(!playlistParser->forceStop)
                {
                    CDX_LOGE("CdxParserPrepare fail");
                    playlistParser->mErrno = PSR_UNKNOWN_ERR;
                }
                goto _exit;
            }

            CdxMediaInfoT mediaInfo;
            ret = CdxParserGetMediaInfo(playlistParser->child, &mediaInfo);
            if(ret < 0)
            {
                CDX_LOGE(" CdxParserGetMediaInfo fail. ret(%d)", ret);
                playlistParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }

            ret = CdxParserPrefetch(playlistParser->child, pkt);
            if(ret < 0)
            {
                if(!playlistParser->forceStop)
                {
                    CDX_LOGE(" prefetch error! ret(%d)", ret);
                    playlistParser->mErrno = PSR_UNKNOWN_ERR;
                }
                goto _exit;
            }
        }
        else
        {
            playlistParser->mErrno = mErrno;
            CDX_LOGE("CdxParserPrefetch mErrno = %d", mErrno);
            goto _exit;
        }
    }

    pkt->pts += playlistParser->baseTimeUs;
    memcpy(&playlistParser->cdxPkt, pkt, sizeof(CdxPacketT));
    pthread_mutex_lock(&playlistParser->statusLock);
    playlistParser->status = CDX_PSR_PREFETCHED;
    pthread_mutex_unlock(&playlistParser->statusLock);
    pthread_cond_signal(&playlistParser->cond);
    CDX_LOGV("CdxParserPrefetch pkt->pts=%lld, pkt->type=%d, pkt->length=%d",
        pkt->pts, pkt->type, pkt->length);
    return 0;
_exit:
    pthread_mutex_lock(&playlistParser->statusLock);
    playlistParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&playlistParser->statusLock);
    pthread_cond_signal(&playlistParser->cond);
    return ret;
}

static cdx_int32 PlaylistParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser*)parser;
    if(playlistParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        playlistParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    pthread_mutex_lock(&playlistParser->statusLock);
    if(playlistParser->forceStop)
    {
        pthread_mutex_unlock(&playlistParser->statusLock);
        return -1;
    }
    playlistParser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&playlistParser->statusLock);

    int ret = CdxParserRead(playlistParser->child, pkt);
    pthread_mutex_lock(&playlistParser->statusLock);
    playlistParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&playlistParser->statusLock);
    pthread_cond_signal(&playlistParser->cond);
    return ret;
}

cdx_int32 PlaylistParserForceStop(CdxParserT *parser)
{
    CDX_LOGI("PlaylistParserForceStop start");
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser*)parser;
    int ret;
    pthread_mutex_lock(&playlistParser->statusLock);
    playlistParser->forceStop = 1;
    if(playlistParser->child)
    {
        ret = CdxParserForceStop(playlistParser->child);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserForceStop fail, ret(%d)", ret);
            //playlistParser->mErrno = CdxParserGetStatus(playlistParser->child);
            //return -1;
        }
    }
    else if(playlistParser->childStream)
    {
        ret = CdxStreamForceStop(playlistParser->childStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
        }
    }

    if(playlistParser->tmpChild)
    {
        ret = CdxParserForceStop(playlistParser->tmpChild);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserForceStop fail, ret(%d)", ret);
            //playlistParser->mErrno = CdxParserGetStatus(playlistParser->child);
            //return -1;
        }
    }
    else if(playlistParser->tmpChildStream)
    {
        ret = CdxStreamForceStop(playlistParser->tmpChildStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
        }
    }
    while(playlistParser->status != CDX_PSR_IDLE && playlistParser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&playlistParser->cond, &playlistParser->statusLock);
    }
    pthread_mutex_unlock(&playlistParser->statusLock);
    playlistParser->mErrno = PSR_USER_CANCEL;
    playlistParser->status = CDX_PSR_IDLE;
    CDX_LOGD("PlaylistParserForceStop end");
    return 0;

}

cdx_int32 PlaylistParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("PlaylistParserClrForceStop start");
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser*)parser;
    if(playlistParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("status != CDX_PSR_IDLE");
        playlistParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    playlistParser->forceStop = 0;
    if(playlistParser->child)
    {
        int ret = CdxParserClrForceStop(playlistParser->child);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserClrForceStop fail, ret(%d)", ret);
            return ret;
        }
    }
    CDX_LOGI("PlaylistParserClrForceStop end");
    return 0;

}

static int PlaylistParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser*)parser;
    (void)playlistParser;
    (void)param;

    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" playlist parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return PlaylistParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return PlaylistParserClrForceStop(parser);
        default:
            break;
    }
    return 0;
}

cdx_int32 PlaylistParserGetStatus(CdxParserT *parser)
{
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser *)parser;
    return playlistParser->mErrno;
}
cdx_int32 PlaylistParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_LOGI("PlaylistParserSeekTo start, timeUs = %lld", timeUs);
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser *)parser;
    playlistParser->mErrno = PSR_OK;
    if(timeUs < 0)
    {
        CDX_LOGE("timeUs invalid");
        playlistParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if(timeUs >= playlistParser->mPlaylist->durationUs)
    {
        CDX_LOGI("PSR_EOS");
        playlistParser->mErrno = PSR_EOS;
        return 0;
    }

    pthread_mutex_lock(&playlistParser->statusLock);
    if(playlistParser->forceStop)
    {
        playlistParser->mErrno = PSR_USER_CANCEL;
        CDX_LOGE("PSR_USER_CANCEL");
        pthread_mutex_unlock(&playlistParser->statusLock);
        return -1;
    }
    playlistParser->status = CDX_PSR_SEEKING;
    pthread_mutex_unlock(&playlistParser->statusLock);

    int ret = 0;
    Playlist *playlist = playlistParser->mPlaylist;
    int64_t mDurationUs = 0;
    PlaylistItem *item = playlist->mItems;
    while(item)
    {
        mDurationUs += item->durationUs;
        if (mDurationUs > timeUs)
            break;
        item = item->next;
    }
    if(!item)
    {
        CDX_LOGE("unknown error");
        playlistParser->mErrno = PSR_UNKNOWN_ERR;
        ret = -1;
        goto _exit;
    }
    mDurationUs -= item->durationUs;
    if(item->seqNum != playlistParser->curSeqNum)
    {
        pthread_mutex_lock(&playlistParser->statusLock);
        if(playlistParser->forceStop)
        {
            playlistParser->mErrno = PSR_USER_CANCEL;
            ret = -1;
            pthread_mutex_unlock(&playlistParser->statusLock);
            goto _exit;
        }
        if(playlistParser->child)
        {
            ret = CdxParserClose(playlistParser->child);
            CDX_CHECK(ret == 0);
            playlistParser->child = NULL;
            playlistParser->childStream = NULL;
        }
        pthread_mutex_unlock(&playlistParser->statusLock);

        SetDataSouceForSegment(item, &playlistParser->cdxDataSource);
        ret = CdxParserPrepare(&playlistParser->cdxDataSource, NO_NEED_DURATION,
            &playlistParser->statusLock, &playlistParser->forceStop,
            &playlistParser->child, &playlistParser->childStream, NULL, NULL);
        if(ret < 0)
        {
            playlistParser->curSeqNum = -1;
            if(!playlistParser->forceStop)
            {
                CDX_LOGE("CdxParserPrepare fail");
                playlistParser->mErrno = PSR_UNKNOWN_ERR;
            }
            ret = -1;
            goto _exit;
        }
        CdxMediaInfoT mediaInfo;
        ret = CdxParserGetMediaInfo(playlistParser->child, &mediaInfo);
        if(ret < 0)
        {
            CDX_LOGE(" CdxParserGetMediaInfo fail. ret(%d)", ret);
            playlistParser->mErrno = PSR_UNKNOWN_ERR;
            goto _exit;
        }
        playlistParser->baseTimeUs = item->baseTimeUs;
        playlistParser->curSeqNum = item->seqNum;
    }
    CDX_LOGI("mDurationUs = %lld", mDurationUs);
#if 1
    ret = CdxParserSeekTo(playlistParser->child, timeUs - mDurationUs, seekModeType);
#endif
_exit:

    pthread_mutex_lock(&playlistParser->statusLock);
    playlistParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&playlistParser->statusLock);
    pthread_cond_signal(&playlistParser->cond);
    CDX_LOGI("PlaylistParserSeekTo end, ret = %d", ret);
    return ret;
}

static cdx_int32 PlaylistParserClose(CdxParserT *parser)
{
    CDX_LOGI("PlaylistParserClose start");
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser *)parser;
    int ret = PlaylistParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("HlsParserForceStop fail");
    }

    if(playlistParser->child)
    {
        ret = CdxParserClose(playlistParser->child);
        CDX_CHECK(ret >= 0);
    }
    else if(playlistParser->childStream)
    {
        ret = CdxStreamClose(playlistParser->childStream);
    }

    if(playlistParser->tmpChild)
    {
        ret = CdxParserClose(playlistParser->tmpChild);
        CDX_CHECK(ret >= 0);
    }
    else if(playlistParser->tmpChildStream)
    {
        ret = CdxStreamClose(playlistParser->tmpChildStream);
    }

    ret = CdxStreamClose(playlistParser->cdxStream);
    CDX_CHECK(ret >= 0);
    if(playlistParser->playlistUrl)
    {
        free(playlistParser->playlistUrl);
    }
    if(playlistParser->playlistBuf)
    {
        free(playlistParser->playlistBuf);
    }
    if(playlistParser->mPlaylist)
    {
        destoryPlaylist_P(playlistParser->mPlaylist);
    }
    pthread_mutex_destroy(&playlistParser->statusLock);
    pthread_cond_destroy(&playlistParser->cond);
    free(playlistParser);
    CDX_LOGI("PlaylistParserClose end");
    return 0;
}

cdx_uint32 PlaylistParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser *)parser;
    return playlistParser->flags;
}

int PlaylistParserInit(CdxParserT *parser)
{
    CDX_LOGI("CdxPlaylistOpenThread start");
    CdxPlaylistParser *playlistParser = (CdxPlaylistParser*)parser;

    if(DownloadParsePlaylist(playlistParser) < 0)
    {
        CDX_LOGE("DownloadParsePlaylist fail");
        goto _exit;
    }

    Playlist *playlist = playlistParser->mPlaylist;
    //playlistParser->curSeqNum = playlist->mItems->seqNum;

    PlaylistItem *item = playlist->mItems;
    int ret;
    cdx_uint32 i;
    CdxMediaInfoT *pMediaInfo;
    CdxMediaInfoT mediaInfo;
    for(i = 0; i < playlist->mNumItems; i++)
    {
        SetDataSouceForSegment(item, &playlistParser->cdxDataSource);
        //playlistParser->cdxDataSource->uri = item->mURI;

        if(!i)
        {
            ret = CdxParserPrepare(&playlistParser->cdxDataSource, NO_NEED_DURATION,
                &playlistParser->statusLock, &playlistParser->forceStop,
                &playlistParser->child, &playlistParser->childStream, NULL, NULL);
            if(ret < 0)
            {
                CDX_LOGE("CreatMediaParser fail");
                goto _exit;
            }
            pMediaInfo = &playlistParser->mediaInfo;
            ret = CdxParserGetMediaInfo(playlistParser->child, pMediaInfo);
            if(ret < 0)
            {
                CDX_LOGE("playlistParser->child getMediaInfo error!");
                goto _exit;
            }
        }
        else
        {
            ret = CdxParserPrepare(&playlistParser->cdxDataSource, NO_NEED_DURATION,
                &playlistParser->statusLock, &playlistParser->forceStop,
                &playlistParser->tmpChild, &playlistParser->tmpChildStream, NULL, NULL);
            if(ret < 0)
            {
                CDX_LOGE("CreatMediaParser fail");
                goto _exit;
            }
            pMediaInfo = &mediaInfo;
            ret = CdxParserGetMediaInfo(playlistParser->tmpChild, pMediaInfo);
            if(ret < 0)
            {
                CDX_LOGE("playlistParser->child getMediaInfo error!");
            }
            ret = CdxParserClose(playlistParser->tmpChild);
            playlistParser->tmpChild = NULL;
            playlistParser->tmpChildStream = NULL;
            if(ret < 0)
            {
                CDX_LOGE("CdxParserClose fail!");
                goto _exit;
            }
        }
        item->baseTimeUs = playlist->durationUs;
        item->durationUs = (cdx_int64)(pMediaInfo->program[0].duration * 1000);
        playlist->durationUs += item->durationUs;
        item = item->next;

    }

    pMediaInfo = &playlistParser->mediaInfo;
    pMediaInfo->fileSize = -1;
    pMediaInfo->bSeekable = 1;
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->program[0].duration = playlist->durationUs / 1000; //duration ms

    //PrintMediaInfo(pMediaInfo);
    playlistParser->mErrno = PSR_OK;
    pthread_mutex_lock(&playlistParser->statusLock);
    playlistParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&playlistParser->statusLock);
    pthread_cond_signal(&playlistParser->cond);
    CDX_LOGI("CdxPlaylistOpenThread success");
    return 0;

_exit:
    playlistParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&playlistParser->statusLock);
    playlistParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&playlistParser->statusLock);
    pthread_cond_signal(&playlistParser->cond);
    CDX_LOGE("CdxPlaylistOpenThread fail");
    return -1;
}
static struct CdxParserOpsS playlistParserOps =
{
    .control         = PlaylistParserControl,
    .prefetch         = PlaylistParserPrefetch,
    .read             = PlaylistParserRead,
    .getMediaInfo     = PlaylistParserGetMediaInfo,
    .close             = PlaylistParserClose,
    .seekTo            = PlaylistParserSeekTo,
    .attribute        = PlaylistParserAttribute,
    .getStatus        = PlaylistParserGetStatus,
    .init            = PlaylistParserInit
};

CdxParserT *PlaylistParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxPlaylistParser *playlistParser = CdxMalloc(sizeof(CdxPlaylistParser));
    if(!playlistParser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    memset(playlistParser, 0x00, sizeof(CdxPlaylistParser));
    (void)flags;

    int ret = pthread_cond_init(&playlistParser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&playlistParser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);

    char *tmpUrl;
    ret = CdxStreamGetMetaData(stream, "uri", (void **)&tmpUrl);
    if(ret < 0)
    {
        CDX_LOGE("get the uri of the stream error!");
        goto open_error;
    }
    playlistParser->cdxStream = stream;
    int urlLen = strlen(tmpUrl) + 1;
    playlistParser->playlistUrl = malloc(urlLen);
    if(!playlistParser->playlistUrl)
    {
        CDX_LOGE("malloc fail!");
        goto open_error;
    }
    memcpy(playlistParser->playlistUrl, tmpUrl, urlLen);

    playlistParser->playlistBufSize = 120*1024;
    playlistParser->playlistBuf = malloc(playlistParser->playlistBufSize);
    if (!playlistParser->playlistBuf)
    {
        CDX_LOGE("allocate memory fail for m3u8 file");
        goto open_error;
    }
    //memset(playlistParser->playlistBuf, 0, playlistParser->playlistBufSize);
    //由每次下载m3u9文件时重置

    playlistParser->base.ops = &playlistParserOps;
    playlistParser->base.type = CDX_PARSER_PLAYLIST;
    playlistParser->mErrno = PSR_INVALID;
    playlistParser->status = CDX_PSR_INITIALIZED;
    return &playlistParser->base;

open_error:
    CdxStreamClose(stream);
    if(playlistParser->playlistBuf)
    {
        free(playlistParser->playlistBuf);
    }
    if(playlistParser->playlistUrl)
    {
        free(playlistParser->playlistUrl);
    }
    pthread_mutex_destroy(&playlistParser->statusLock);
    pthread_cond_destroy(&playlistParser->cond);
    free(playlistParser);
    return NULL;
}

cdx_uint32 PlaylistParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_LOGD("PlaylistParserProbe");

    if(probeData->len < 7)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }
    int ret = PlaylistProbe((const char *)probeData->buf, probeData->len);
    CDX_LOGD("PlaylistParserProbe = %d", ret);
    return ret*100;

}

CdxParserCreatorT playlistParserCtor =
{
    .create    = PlaylistParserOpen,
    .probe     = PlaylistParserProbe
};

