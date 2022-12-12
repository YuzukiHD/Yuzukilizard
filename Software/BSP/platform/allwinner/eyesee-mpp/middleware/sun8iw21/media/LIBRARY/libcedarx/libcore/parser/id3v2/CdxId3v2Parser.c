/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxId3Parser.c
* Description :
* History :
*   Author  : Khan <chengkan@allwinnertech.com>
*   Date    : 2014/12/08
*   Comment : 创建初始版本，实现 ID3_tag 的解析功能
*/

#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxMemory.h>
#include <CdxId3v2Parser.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "_id3v2"
#define ID3V2TAG "ID3"
#define METEDATAKEY    "uri"

#define PROBE_PREOTECTION (8*1024)

static cdx_int32 Sync2FirstFrame(CdxStreamT* in)
{
    CdxStreamProbeDataT *probeData = CdxStreamGetProbeData(in);
    CdxStreamSeek(in,probeData->sync_pos,SEEK_CUR);
    return probeData->sync_pos;
}

static int Id3v2Init(CdxParserT *id3_impl)
{
    cdx_int32 ret = 0;
    cdx_int32 offset=0;
    cdx_int32 tmpFd = 0;

    struct Id3v2ParserImplS *impl = NULL;
    impl = (Id3v2ParserImplS*)id3_impl;

    offset = Sync2FirstFrame(impl->stream);
    impl->id3v2 = GenerateId3(impl->stream, NULL, 0, ktrue);
    if(!impl->id3v2)
    {
        CDX_LOGW("get id3 handle fail!");
        goto OPENFAILURE;
    }

    if(impl->id3v2)
        offset += impl->id3v2->mRawSize;

    if(impl->id3v2->mIsValid)
    {
        CDX_LOGD("Parser id3 success");
    }
    else
    {
        CDX_LOGD("Id3_v2 parser get fail? Joking me....");
        goto OPENFAILURE;
    }

    CdxStreamProbeDataT *probeData = CdxStreamGetProbeData(impl->stream);

    CDX_LOGD("From(%lld) skip to post id3 location(%lld), totally skip (%lld + %d)...",
impl->cur_id3_offset, impl->cur_id3_offset + offset, probeData->sync_pos, impl->id3v2->mRawSize);
    CDX_LOGD("Now actually offset %lld", CdxStreamTell(impl->stream));

    impl->cur_id3_offset += offset;

    //reopen parser
    memset(&impl->cdxDataSource, 0x00, sizeof(impl->cdxDataSource));
    CdxStreamGetMetaData(impl->stream,METEDATAKEY,(void **)&impl->keyinfo);
    if(strncmp(impl->keyinfo, "file://", 7) == 0 || strncmp(impl->keyinfo, "fd://", 5) == 0)
    {
        ret = sscanf(impl->keyinfo, "fd://%d?offset=%lld&length=%lld", &tmpFd, &impl->fdoffset,
                     &impl->file_size);
        ret = sprintf(impl->newurl, "fd://%d?offset=%lld&length=%lld", tmpFd,
                      impl->fdoffset+impl->cur_id3_offset, impl->file_size - impl->cur_id3_offset);
    }
    else if(strncmp(impl->keyinfo, "http://", 7) == 0 || strncmp(impl->keyinfo, "https://", 8)== 0)
    {
        strcpy(impl->newurl, impl->keyinfo);
        impl->cdxDataSource.offset = impl->cur_id3_offset;
    }

    logd("impl->newurl(%s), impl->file_offset(%lld)", impl->newurl, impl->cur_id3_offset);
    impl->cdxDataSource.uri = impl->newurl;

    if (probeData->len > offset + PROBE_PREOTECTION)
    {
        probeData->len -= offset;
        CDX_LOGD("Id3 probe data is enough at absolutive pos(%lld), reuse(sync_pos(%lld), skip(%d),\
 left(%d))", impl->fdoffset, probeData->sync_pos, offset, probeData->len);
        probeData->sync_pos = 0;
        memmove(probeData->buf, probeData->buf + offset, probeData->len);
        impl->id3Attributes |= SUCCESS_STREAM_TO_CHILD;
        ret = CdxParserOpen(impl->stream, NO_NEED_DURATION|DO_NOT_EXTRACT_ID3_METADATA,
            &impl->lock, &impl->forceStop, &impl->child, NULL);
        if (ret == 0)
        {
            impl->mErrno = PSR_OK;
            return 0;
        }
        CDX_LOGE("Reuse the father's probe data, but open child parser fail...");
        goto OPENFAILURE;
    }
    CDX_LOGD("Id3 probe data is not enough at absolutive pos(%lld), probe has(%d), need skip(%d)\
 + %d bytes protection, reopen stream and fill probe data...",
            impl->fdoffset, probeData->len, offset, PROBE_PREOTECTION);
    ret = CdxParserPrepare(&impl->cdxDataSource, NO_NEED_DURATION|DO_NOT_EXTRACT_ID3_METADATA,
        &impl->lock, &impl->forceStop, &impl->child, &impl->childStream, NULL, NULL);
    if(ret < 0)
    {
        CDX_LOGE("CdxParserPrepare fail");
        goto OPENFAILURE;
    }

    impl->mErrno = PSR_OK;
    return 0;
OPENFAILURE:
    CDX_LOGE("Id3OpenThread fail!!!");
    EraseId3(&impl->id3v2);
    impl->mErrno = PSR_OPEN_FAIL;
    return -1;
}

static cdx_int32 __Id3v2ParserControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    struct Id3v2ParserImplS *impl = NULL;
    impl = (Id3v2ParserImplS*)parser;
    (void)param;
    if(!impl->child)
        return CDX_SUCCESS;
    switch (cmd)
    {
        case CDX_PSR_CMD_DISABLE_AUDIO:
        case CDX_PSR_CMD_DISABLE_VIDEO:
        case CDX_PSR_CMD_SWITCH_AUDIO:
            break;
        case CDX_PSR_CMD_SET_FORCESTOP:
            CdxParserForceStop(impl->child);
          break;
        case CDX_PSR_CMD_CLR_FORCESTOP:
            CdxParserClrForceStop(impl->child);
            break;
        default :
            CDX_LOGD("Default success to child processing...");
            CdxParserControl(impl->child, cmd, param);
            break;
    }
    impl->flags = cmd;
    return CDX_SUCCESS;
}

static cdx_int32 __Id3v2ParserPrefetch(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret = CDX_FAILURE;
    struct Id3v2ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);
    if(!impl->child)
        return ret;
    ret = CdxParserPrefetch(impl->child,pkt);
    impl->mErrno = CdxParserGetStatus(impl->child);
    return ret;
}

static cdx_int32 __Id3v2ParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    cdx_int32 ret = CDX_FAILURE;
    struct Id3v2ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);
    if(!impl->child)
        return ret;
    ret = CdxParserRead(impl->child,pkt);
    impl->mErrno = CdxParserGetStatus(impl->child);
    return ret;
}

static cdx_int32 __Id3v2ParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *mediaInfo)
{
    cdx_int32 ret = CDX_FAILURE;
    struct Id3v2ParserImplS *impl = NULL;
    Iterator* it = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);

    if(!(impl->id3Attributes & DO_NOT_EXTRACT_ID3_METADATA))
    {
        if(impl->id3v2 && impl->id3v2->mIsValid)
        {
            CDX_LOGD("id3v2 has vaild parsed...");
            mediaInfo->id3v2HadParsed = 1;
            Id3BaseGetMetaData(mediaInfo, impl->id3v2);
            Id3BaseExtraAlbumPic(mediaInfo, impl->id3v2);
        }
    }
    if(!impl->child)
        return ret;
    ret = CdxParserGetMediaInfo(impl->child,mediaInfo);
    impl->mErrno = CdxParserGetStatus(impl->child);
    return ret;
}

static cdx_int32 __Id3v2ParserSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    cdx_int32 ret = CDX_FAILURE;
    struct Id3v2ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);
    if(!impl->child)
        return ret;
    ret = CdxParserSeekTo(impl->child,timeUs,seekModeType);
    impl->mErrno = CdxParserGetStatus(impl->child);
    return ret;
}

static cdx_uint32 __Id3v2ParserAttribute(CdxParserT *parser)
{
    struct Id3v2ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);
    return CdxParserAttribute(impl->child);
}
#if 0
static cdx_int32 __Id3ParserForceStop(CdxParserT *parser)
{
    struct Id3v2ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);
    return CdxParserForceStop(impl->child);
}
#endif
static cdx_int32 __Id3v2ParserGetStatus(CdxParserT *parser)
{
    struct Id3v2ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);

    return impl->child?CdxParserGetStatus(impl->child):impl->mErrno;
}

static cdx_int32 __Id3v2ParserClose(CdxParserT *parser)
{
    struct Id3v2ParserImplS *impl = NULL;
    impl = CdxContainerOf(parser, struct Id3v2ParserImplS, base);
#if 0
    struct Id3Pic* thiz = NULL,*tmp = NULL;
    thiz = impl->pAlbumArt;
    impl->pAlbumArt = NULL;
    while(thiz != NULL)
    {
        if(thiz->addr!=NULL)
        {
            free(thiz->addr);
            CDX_LOGE("FREE PIC");
            thiz->addr = NULL;
        }
        tmp = thiz;
        thiz = thiz->father;
        if(tmp!=NULL)
        {
            free(tmp);
            impl->pAlbumArtid--;
            CDX_LOGE("FREE PIC COMPLETE impl->pAlbumArtid:%d",impl->pAlbumArtid);
            tmp = NULL;
        }
    }
#endif
    if(impl->stream && !(impl->id3Attributes & SUCCESS_STREAM_TO_CHILD)){
        CDX_LOGD("id3v2 close stream it self...");
        CdxStreamClose(impl->stream);
        impl->stream = NULL;
    }
    else
    {
        CDX_LOGD("id3v2 success stream to its child, child close stream!");
    }

    if(impl->child)
    {
        CdxParserClose(impl->child);
        impl->child = NULL;
    }

    pthread_mutex_destroy(&impl->lock);
    EraseId3(&impl->id3v2);
    CdxFree(impl);
    return CDX_SUCCESS;
}

static struct CdxParserOpsS id3v2ParserOps =
{
    .control = __Id3v2ParserControl,
    .prefetch = __Id3v2ParserPrefetch,
    .read = __Id3v2ParserRead,
    .getMediaInfo = __Id3v2ParserGetMediaInfo,
    .seekTo = __Id3v2ParserSeekTo,
    .attribute = __Id3v2ParserAttribute,
    .getStatus = __Id3v2ParserGetStatus,
    .close = __Id3v2ParserClose,
    .init = Id3v2Init
};

static cdx_uint32 __Id3v2ParserProbe(CdxStreamProbeDataT *probeData)
{
    cdx_int32 probe_max_len = 4*1024 > probeData->len?probeData->len:4*1024;
    cdx_char  *ptr = probeData->buf;
    cdx_int32 i, score = 100;
    CDX_CHECK(probeData);
    if(probe_max_len < 10)
    {
        CDX_LOGE("Probe ID3_header data is not enough.");
        return 0;
    }

    for(i = 0; i < probe_max_len - 10; i++, ptr++)
    {
        if(memcmp(ptr,ID3V2TAG,3))
        {
            continue;
        }

        if(ptr[3] == 0xff || ptr[4] == 0xff)
        {
            continue;
        }
        break;
    }

    if(i != 0)
    {
        score -= 1;
        if(i >= probe_max_len - 10)
        {
            CDX_LOGE("Probe ID3_header loss sync...");
            return 0;
        }
    }
    CDX_LOGD("Id3v2 probe score : %d, offset : %d", score, i);
    probeData->sync_pos = i;
    return score;
}

static CdxParserT *__Id3v2ParserOpen(CdxStreamT *stream, cdx_uint32 flags)
{
    cdx_int32 ret = 0;
    struct Id3v2ParserImplS *impl;
    impl = CdxMalloc(sizeof(*impl));

    memset(impl, 0x00, sizeof(*impl));
    ret = pthread_mutex_init(&impl->lock, NULL);
    CDX_FORCE_CHECK(ret == 0);
    impl->stream = stream;
    impl->base.ops = &id3v2ParserOps;
    impl->id3Attributes |= flags;
    //ret = pthread_create(&impl->openTid, NULL, Id3OpenThread, (void*)impl);
    //CDX_FORCE_CHECK(!ret);
    impl->mErrno = PSR_INVALID;

    return &impl->base;
}

struct CdxParserCreatorS id3v2ParserCtor =
{
    .probe = __Id3v2ParserProbe,
    .create = __Id3v2ParserOpen
};
