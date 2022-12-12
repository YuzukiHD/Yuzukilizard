/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxM3u9Parser.c
 * Description : m3u9 parser implementation.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxM3u9Parser"
#include "CdxM3u9Parser.h"
#include <cdx_log.h>
#include <CdxMemory.h>
#include <errno.h>
#include <string.h>


static inline void SetDataSouceForSegment(PlaylistItem *item, CdxDataSourceT *cdxDataSource)
{
    cdxDataSource->uri = item->mURI;
}


/*return -1:error*/
static int DownloadParseM3u9(CdxM3u9Parser *m3u9Parser)
{
    memset(m3u9Parser->m3u9Buf, 0, m3u9Parser->m3u9BufSize);
    
    int ret, m3u9ReadSize = 0;
    while(1)
    {
        if(m3u9Parser->forceStop == 1)
        {
            return -1;
        }
        ret = CdxStreamRead(m3u9Parser->cdxStream, m3u9Parser->m3u9Buf + m3u9ReadSize, 1024);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail, ret(%d)", ret);
            return -1;
        }
        if(ret == 0)
        {
            break;
        }
        m3u9ReadSize += ret;
    }

    if(m3u9ReadSize == 0)
    {
        CDX_LOGE("download m3u9 mPlaylist fail");
        return -1;
    }

    Playlist *playlist = NULL;
    status_t err;
    if ((err = M3u9Parse(m3u9Parser->m3u9Buf, m3u9ReadSize, &playlist, m3u9Parser->m3u9Url)) != OK)
    {
        CDX_LOGE("creation playlist fail, err=%d", err);
        return -1;
    }
    m3u9Parser->mPlaylist = playlist;
    return 0;
}

/*return -1:error*/
/*return 0:m3u9Parser PSR_EOS*/
/*return 1:选择到了新的Segment*/
static int SelectNextSegment(CdxM3u9Parser *m3u9Parser)
{
    Playlist *playlist = m3u9Parser->mPlaylist;
    if(m3u9Parser->curSeqNum > playlist->lastSeqNum)
    {
        return 0;
    }
    PlaylistItem *item = findItemBySeqNumForM3u9(playlist, m3u9Parser->curSeqNum);
    if(!item)
    {
        CDX_LOGE("findItemBySeqNum fail");
        return -1;
    }
    SetDataSouceForSegment(item, &m3u9Parser->cdxDataSource);
    m3u9Parser->baseTimeUs = item->baseTimeUs;
    return 1;

}

static cdx_int32 M3u9ParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pMediaInfo)
{
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser*)parser;
    if(m3u9Parser->status < CDX_PSR_IDLE)
    {
        CDX_LOGW("m3u9Parser->status < CDX_PSR_IDLE, can not GetMediaInfo");
        m3u9Parser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    memcpy(pMediaInfo, &m3u9Parser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}

static cdx_int32 M3u9ParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser*)parser;
    if(m3u9Parser->status != CDX_PSR_IDLE && m3u9Parser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_IDLE && status != "
            "CDX_PSR_PREFETCHED, M3u9ParserPrefetch invaild");
        m3u9Parser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(m3u9Parser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(m3u9Parser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(pkt, &m3u9Parser->cdxPkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&m3u9Parser->statusLock);
    if(m3u9Parser->forceStop)
    {
        pthread_mutex_unlock(&m3u9Parser->statusLock);
        return -1;
    }
    m3u9Parser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    
    memset(pkt, 0, sizeof(CdxPacketT));
    int ret, mErrno;
    ret = CdxParserPrefetch(m3u9Parser->child, pkt);
    if(ret < 0)
    {
        mErrno = CdxParserGetStatus(m3u9Parser->child);
        if(mErrno == PSR_EOS)
        {
#if _SAVE_VIDEO_STREAM
            if (m3u9Parser->fpVideoStream)
            {
                fclose(m3u9Parser->fpVideoStream);
            }
#endif
#if _SAVE_AUDIO_STREAM
            if (m3u9Parser->fpAudioStream)
            {
                fclose(m3u9Parser->fpAudioStream);
            }
#endif

            pthread_mutex_lock(&m3u9Parser->statusLock);
            if(m3u9Parser->forceStop)
            {
                m3u9Parser->mErrno = PSR_USER_CANCEL;
                ret = -1;
                pthread_mutex_unlock(&m3u9Parser->statusLock);
                goto _exit;
            }
            ret = CdxParserClose(m3u9Parser->child);
            CDX_CHECK(ret == 0);
            m3u9Parser->child = NULL;
            m3u9Parser->childStream = NULL;
            pthread_mutex_unlock(&m3u9Parser->statusLock);

            m3u9Parser->curSeqNum++;
            CDX_LOGI("curSeqNum = %d", m3u9Parser->curSeqNum);
            ret = SelectNextSegment(m3u9Parser);
            if(ret < 0)
            {
                CDX_LOGE("SelectNextSegment fail");
                m3u9Parser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
            else if(ret == 0)
            {
                CDX_LOGD("m3u9Parser->status = PSR_EOS");
                m3u9Parser->mErrno = PSR_EOS;
                ret = -1;
                goto _exit;
            }
            CDX_LOGD("replace parser");
            int ret = CdxParserPrepare(&m3u9Parser->cdxDataSource, NO_NEED_DURATION,
                &m3u9Parser->statusLock, &m3u9Parser->forceStop,
                &m3u9Parser->child, &m3u9Parser->childStream, NULL, NULL);
            if(ret < 0)
            {
                m3u9Parser->curSeqNum = -1;
                if(!m3u9Parser->forceStop)
                {
                    CDX_LOGE("CdxParserPrepare fail");
                    m3u9Parser->mErrno = PSR_UNKNOWN_ERR;
                }
                goto _exit;
            }
            
            CdxMediaInfoT mediaInfo;
            ret = CdxParserGetMediaInfo(m3u9Parser->child, &mediaInfo);
            if(ret < 0)
            {
                CDX_LOGE(" CdxParserGetMediaInfo fail. ret(%d)", ret);
                m3u9Parser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
#if _SAVE_VIDEO_STREAM
            sprintf(m3u9Parser->url, "/mnt/sdcard/vstream_%d.es", m3u9Parser->curSeqNum);
            m3u9Parser->fpVideoStream = fopen(m3u9Parser->url, "wb");
            if (!m3u9Parser->fpVideoStream)
            {
                CDX_LOGE("open video stream debug file failure errno(%d)", errno);
            }
#endif
#if _SAVE_AUDIO_STREAM
            sprintf(m3u9Parser->url, "/mnt/sdcard/astream_%d.es", m3u9Parser->curSeqNum);
            m3u9Parser->fpAudioStream = fopen(m3u9Parser->url, "wb");
            if (!m3u9Parser->fpAudioStream)
            {
                CDX_LOGE("open audio stream debug file failure errno(%d)", errno);
            }
#endif
            ret = CdxParserPrefetch(m3u9Parser->child, pkt);
            if(ret < 0)
            {
                if(!m3u9Parser->forceStop)
                {
                    CDX_LOGE(" prefetch error! ret(%d)", ret);
                    m3u9Parser->mErrno = PSR_UNKNOWN_ERR;
                }
                goto _exit;
            }
        }
        else
        {
            m3u9Parser->mErrno = mErrno;
            CDX_LOGE("CdxParserPrefetch mErrno = %d", mErrno);
            goto _exit;
        }
    }
    
    pkt->pts += m3u9Parser->baseTimeUs;
    memcpy(&m3u9Parser->cdxPkt, pkt, sizeof(CdxPacketT));
    pthread_mutex_lock(&m3u9Parser->statusLock);
    m3u9Parser->status = CDX_PSR_PREFETCHED;
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    pthread_cond_signal(&m3u9Parser->cond);
    //CDX_LOGD("CdxParserPrefetch pkt->pts=%lld, pkt->type=%d, "
    //"pkt->length=%d", pkt->pts, pkt->type, pkt->length);
    return 0;
_exit:
    pthread_mutex_lock(&m3u9Parser->statusLock);
    m3u9Parser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    pthread_cond_signal(&m3u9Parser->cond);
    return ret;
}

static cdx_int32 M3u9ParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser*)parser;
    if(m3u9Parser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGE("status != CDX_PSR_PREFETCHED, we can not read!");
        m3u9Parser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    pthread_mutex_lock(&m3u9Parser->statusLock);
    if(m3u9Parser->forceStop)
    {
        pthread_mutex_unlock(&m3u9Parser->statusLock);
        return -1;
    }
    m3u9Parser->status = CDX_PSR_READING;
    pthread_mutex_unlock(&m3u9Parser->statusLock);

    int ret = CdxParserRead(m3u9Parser->child, pkt);
    if(ret > 0)
    {
#if _SAVE_VIDEO_STREAM
        if(pkt->type == CDX_MEDIA_VIDEO)
        {
            if(m3u9Parser->fpVideoStream)
            {
                fwrite(pkt->buf, 1, pkt->buflen, m3u9Parser->fpVideoStream);
                if(pkt->length > pkt->buflen)
                {
                    fwrite(pkt->ringBuf, 1, pkt->length - pkt->buflen, m3u9Parser->fpVideoStream);
                }
                sync();
            }
            else
            {
                CDX_LOGE("m3u9Parser->fpVideoStream == NULL");
            }
        }
#endif
#if _SAVE_AUDIO_STREAM
        if(pkt->type == CDX_MEDIA_AUDIO)
        {
            if(m3u9Parser->fpAudioStream)
            {
                fwrite(pkt->buf, 1, pkt->buflen, m3u9Parser->fpAudioStream);
                if(pkt->length > pkt->buflen)
                {
                    fwrite(pkt->ringBuf, 1, pkt->length - pkt->buflen, m3u9Parser->fpAudioStream);
                }
                sync();
            }
            else
            {
                CDX_LOGE("m3u9Parser->fpAudioStream == NULL");
            }
        }
#endif
    }
    pthread_mutex_lock(&m3u9Parser->statusLock);
    m3u9Parser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    pthread_cond_signal(&m3u9Parser->cond);
    return ret;
}
cdx_int32 M3u9ParserForceStop(CdxParserT *parser)
{
    CDX_LOGI("M3u9ParserForceStop start");
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser*)parser;
    int ret;

    pthread_mutex_lock(&m3u9Parser->statusLock);
    m3u9Parser->forceStop = 1;
    if(m3u9Parser->child)
    {
        ret = CdxParserForceStop(m3u9Parser->child);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserForceStop fail, ret(%d)", ret);
            //m3u9Parser->mErrno = CdxParserGetStatus(m3u9Parser->child);
            //return -1;
        }
    }
    else if(m3u9Parser->childStream)
    {
        ret = CdxStreamForceStop(m3u9Parser->childStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
        }
    }
    while(m3u9Parser->status != CDX_PSR_IDLE && m3u9Parser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&m3u9Parser->cond, &m3u9Parser->statusLock);
    }
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    m3u9Parser->mErrno = PSR_USER_CANCEL;
    m3u9Parser->status = CDX_PSR_IDLE;
    CDX_LOGI("M3u9ParserForceStop end");
    return 0;
    
}

cdx_int32 M3u9ParserClrForceStop(CdxParserT *parser)
{
    CDX_LOGI("M3u9ParserClrForceStop start");
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser*)parser;
    if(m3u9Parser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("m3u9Parser->status != CDX_PSR_IDLE");
        m3u9Parser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    m3u9Parser->forceStop = 0;
    if(m3u9Parser->child)
    {
        int ret = CdxParserClrForceStop(m3u9Parser->child);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserClrForceStop fail, ret(%d)", ret);
            return ret;
        }
    }
    else if(m3u9Parser->childStream)
    {
        int ret = CdxStreamClrForceStop(m3u9Parser->childStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamClrForceStop fail");
            //hlsParser->mErrno = PSR_UNKNOWN_ERR;
        }
    }
    CDX_LOGI("M3u9ParserClrForceStop end");
    return 0;
    
}

static int M3u9ParserControl(CdxParserT *parser, int cmd, void *param)
{
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser*)parser;
    (void)m3u9Parser;
    (void)param;
    
    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" m3u9 parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            return M3u9ParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return M3u9ParserClrForceStop(parser);
        default:
            break;
    }
    return 0;
}


cdx_int32 M3u9ParserGetStatus(CdxParserT *parser)
{
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser *)parser;
    return m3u9Parser->mErrno;
}
cdx_int32 M3u9ParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_LOGI("M3u9ParserSeekTo start, timeUs = %lld", timeUs);
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser *)parser;
    m3u9Parser->mErrno = PSR_OK;
    if(timeUs < 0)
    {
        CDX_LOGE("timeUs invalid");
        m3u9Parser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    else if(timeUs >= m3u9Parser->mPlaylist->durationUs)
    {
        CDX_LOGI("PSR_EOS");
        m3u9Parser->mErrno = PSR_EOS;
        return 0;
    }
    
    pthread_mutex_lock(&m3u9Parser->statusLock);
    if(m3u9Parser->forceStop)
    {
        m3u9Parser->mErrno = PSR_USER_CANCEL;
        CDX_LOGE("PSR_USER_CANCEL");
        pthread_mutex_unlock(&m3u9Parser->statusLock);
        return -1;
    }
    m3u9Parser->status = CDX_PSR_SEEKING;
    pthread_mutex_unlock(&m3u9Parser->statusLock);

    int ret = 0;
    Playlist *playlist = m3u9Parser->mPlaylist;
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
        m3u9Parser->mErrno = PSR_UNKNOWN_ERR;
        ret = -1;
        goto _exit;
    }
    mDurationUs -= item->durationUs;
    if(item->seqNum != m3u9Parser->curSeqNum)
    {
        pthread_mutex_lock(&m3u9Parser->statusLock);
        if(m3u9Parser->forceStop)
        {
            m3u9Parser->mErrno = PSR_USER_CANCEL;
            ret = -1;
            pthread_mutex_unlock(&m3u9Parser->statusLock);
            goto _exit;
        }
        if(m3u9Parser->child)
        {
            ret = CdxParserClose(m3u9Parser->child);
            CDX_CHECK(ret == 0);
            m3u9Parser->child = NULL;
            m3u9Parser->childStream = NULL;
        }
        pthread_mutex_unlock(&m3u9Parser->statusLock);

        SetDataSouceForSegment(item, &m3u9Parser->cdxDataSource);
        
        ret = CdxParserPrepare(&m3u9Parser->cdxDataSource, NO_NEED_DURATION,
            &m3u9Parser->statusLock, &m3u9Parser->forceStop,
            &m3u9Parser->child, &m3u9Parser->childStream, NULL, NULL);
        if(ret < 0)
        {
            m3u9Parser->curSeqNum = -1;
            if(!m3u9Parser->forceStop)
            {
                CDX_LOGE("CdxParserPrepare fail");
                m3u9Parser->mErrno = PSR_UNKNOWN_ERR;
            }
            goto _exit;
        }
        CdxMediaInfoT mediaInfo;
        ret = CdxParserGetMediaInfo(m3u9Parser->child, &mediaInfo);
        if(ret < 0)
        {
            CDX_LOGE(" CdxParserGetMediaInfo fail. ret(%d)", ret);
            m3u9Parser->mErrno = PSR_UNKNOWN_ERR;
            goto _exit;
        }
        m3u9Parser->baseTimeUs = item->baseTimeUs;
        m3u9Parser->curSeqNum = item->seqNum;
    }
    CDX_LOGI("mDurationUs = %lld", mDurationUs);
#if 1
    ret = CdxParserSeekTo(m3u9Parser->child, timeUs - mDurationUs, seekModeType);
#endif
_exit:

    pthread_mutex_lock(&m3u9Parser->statusLock);
    m3u9Parser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    pthread_cond_signal(&m3u9Parser->cond);
    CDX_LOGI("M3u9ParserSeekTo end, ret = %d", ret);
    return ret;
}


static cdx_int32 M3u9ParserClose(CdxParserT *parser)
{
    CDX_LOGI("M3u9ParserClose start");
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser *)parser;
    int ret = M3u9ParserForceStop(parser);
    if(ret < 0)
    {
        CDX_LOGW("HlsParserForceStop fail");
    }
    if(m3u9Parser->child)
    {
        ret = CdxParserClose(m3u9Parser->child);
        CDX_CHECK(ret >= 0);
    }
    else if(m3u9Parser->childStream)
    {
        ret = CdxStreamClose(m3u9Parser->childStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamForceStop fail");
        }
    }
    ret = CdxStreamClose(m3u9Parser->cdxStream);
    CDX_CHECK(ret >= 0);
    if(m3u9Parser->m3u9Url)
    {
        free(m3u9Parser->m3u9Url);
    }
    if(m3u9Parser->m3u9Buf)
    {
        free(m3u9Parser->m3u9Buf);
    }
    if(m3u9Parser->mPlaylist)
    {
        destoryM3u9Playlist(m3u9Parser->mPlaylist);
    }
    pthread_mutex_destroy(&m3u9Parser->statusLock);
    pthread_cond_destroy(&m3u9Parser->cond);
    free(m3u9Parser);
    CDX_LOGI("M3u9ParserClose end");
    return 0;
}

cdx_uint32 M3u9ParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser *)parser;
    return m3u9Parser->flags;
}

int M3u9ParserInit(CdxParserT *parser)
{
    CDX_LOGI("PmpParserInit start");
    CdxM3u9Parser *m3u9Parser = (CdxM3u9Parser*)parser;

    if(DownloadParseM3u9(m3u9Parser) < 0)
    {
        CDX_LOGE("downloadParseM3u9 fail");
        goto _exit;
    }

    Playlist *playlist = m3u9Parser->mPlaylist;
    m3u9Parser->curSeqNum = playlist->mItems->seqNum;
    PlaylistItem *item = findItemBySeqNumForM3u9(playlist, m3u9Parser->curSeqNum);
    if(!item)
    {
        CDX_LOGE("findItemBySeqNum fail");
        goto _exit;
    }
    SetDataSouceForSegment(item, &m3u9Parser->cdxDataSource);
    int ret = CdxParserPrepare(&m3u9Parser->cdxDataSource, NO_NEED_DURATION,
        &m3u9Parser->statusLock, &m3u9Parser->forceStop,
        &m3u9Parser->child, &m3u9Parser->childStream, NULL, NULL);
    if(ret < 0)
    {
        CDX_LOGE("CdxParserPrepare fail");
        goto _exit;
    }

    CdxMediaInfoT *pMediaInfo = &m3u9Parser->mediaInfo;
    ret = CdxParserGetMediaInfo(m3u9Parser->child, pMediaInfo);
    if(ret < 0)
    {
        CDX_LOGE("m3u9Parser->child getMediaInfo error!");
        goto _exit;
    }
#if _SAVE_VIDEO_STREAM || _SAVE_AUDIO_STREAM
    memset(m3u9Parser->url, 0, sizeof(m3u9Parser->url));
#endif
#if _SAVE_VIDEO_STREAM
    sprintf(m3u9Parser->url, "/mnt/sdcard/vstream_%d.es", m3u9Parser->curSeqNum);
    m3u9Parser->fpVideoStream = fopen(m3u9Parser->url, "wb");
    if (!m3u9Parser->fpVideoStream)
    {
        CDX_LOGE("open video stream debug file failure errno(%d)", errno);
    }
#endif
#if _SAVE_AUDIO_STREAM
    sprintf(m3u9Parser->url, "/mnt/sdcard/astream_%d.es", m3u9Parser->curSeqNum);
    m3u9Parser->fpAudioStream = fopen(m3u9Parser->url, "wb");
    if (!m3u9Parser->fpAudioStream)
    {
        CDX_LOGE("open audio stream debug file failure errno(%d)", errno);
    }
#endif
    pMediaInfo->fileSize = -1;
    pMediaInfo->bSeekable = 1;
    pMediaInfo->programNum = 1;
    pMediaInfo->programIndex = 0;
    pMediaInfo->program[0].duration = playlist->durationUs / 1000; //duration ms

    //PrintMediaInfo(pMediaInfo);
    m3u9Parser->baseTimeUs = item->baseTimeUs;
    m3u9Parser->mErrno = PSR_OK;
    pthread_mutex_lock(&m3u9Parser->statusLock);
    m3u9Parser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    pthread_cond_signal(&m3u9Parser->cond);
    CDX_LOGI("CdxM3u9OpenThread success");
    return 0;
    
_exit:
    m3u9Parser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&m3u9Parser->statusLock);
    m3u9Parser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&m3u9Parser->statusLock);
    pthread_cond_signal(&m3u9Parser->cond);
    CDX_LOGE("CdxM3u9OpenThread fail");
    return -1;
}
static struct CdxParserOpsS m3u9ParserOps =
{
    .control         = M3u9ParserControl,
    .prefetch         = M3u9ParserPrefetch,
    .read             = M3u9ParserRead,
    .getMediaInfo     = M3u9ParserGetMediaInfo,
    .close             = M3u9ParserClose,
    .seekTo            = M3u9ParserSeekTo,
    .attribute        = M3u9ParserAttribute,
    .getStatus        = M3u9ParserGetStatus,
    .init            = M3u9ParserInit
};

CdxParserT *M3u9ParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    CdxM3u9Parser *m3u9Parser = CdxMalloc(sizeof(CdxM3u9Parser));
    if(!m3u9Parser)
    {
        CDX_LOGE("malloc fail!");
        CdxStreamClose(stream);
        return NULL;
    }
    (void)flags;
    memset(m3u9Parser, 0x00, sizeof(CdxM3u9Parser));

    int ret = pthread_cond_init(&m3u9Parser->cond, NULL);
    CDX_FORCE_CHECK(ret == 0);
    ret = pthread_mutex_init(&m3u9Parser->statusLock, NULL);
    CDX_FORCE_CHECK(ret == 0);

    char *tmpUrl;
    ret = CdxStreamGetMetaData(stream, "uri", (void **)&tmpUrl);
    if(ret < 0)
    {
        CDX_LOGE("get the uri of the stream error!");
        goto open_error;
    }
    m3u9Parser->cdxStream = stream;
    int urlLen = strlen(tmpUrl) + 1;
    m3u9Parser->m3u9Url = malloc(urlLen);
    if(!m3u9Parser->m3u9Url)
    {
        CDX_LOGE("malloc fail!");
        goto open_error;
    }
    memcpy(m3u9Parser->m3u9Url, tmpUrl, urlLen);

    m3u9Parser->m3u9BufSize = 120*1024;
    m3u9Parser->m3u9Buf = malloc(m3u9Parser->m3u9BufSize);
    if (!m3u9Parser->m3u9Buf)
    {
        CDX_LOGE("allocate memory fail for m3u8 file");
        goto open_error;
    }
    //memset(m3u9Parser->m3u9Buf, 0, m3u9Parser->m3u9BufSize);//由每次下载m3u9文件时重置

    m3u9Parser->base.ops = &m3u9ParserOps;
    m3u9Parser->base.type = CDX_PARSER_M3U9;
    m3u9Parser->mErrno = PSR_INVALID;
    m3u9Parser->status = CDX_PSR_INITIALIZED;
    return &m3u9Parser->base;

open_error:
    CdxStreamClose(stream);
    if(m3u9Parser->m3u9Buf)
    {
        free(m3u9Parser->m3u9Buf);
    }
    if(m3u9Parser->m3u9Url)
    {
        free(m3u9Parser->m3u9Url);
    }
    pthread_mutex_destroy(&m3u9Parser->statusLock);
    pthread_cond_destroy(&m3u9Parser->cond);
    free(m3u9Parser);
    return NULL;
}


cdx_uint32 M3u9ParserProbe(CdxStreamProbeDataT *probeData)
{
    CDX_LOGD("M3u9ParserProbe");
    if(probeData->len < 9)
    {
        CDX_LOGE("Probe data is not enough.");
        return 0;
    }
    int ret = M3u9Probe((const char *)probeData->buf, probeData->len);
    CDX_LOGD("M3u9Probe = %d", ret);
    return ret*100;
    
}

CdxParserCreatorT m3u9ParserCtor =
{
    .create    = M3u9ParserOpen,
    .probe     = M3u9ParserProbe
};

