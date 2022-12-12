/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmsParser.c
 * Description : parser for mms stream
 * History :
 *
 */

#include <CdxMmsParser.h>

extern CdxParserCreatorT asfParserCtor;

enum CdxMmsStatusE
{
    CDX_MMS_INITIALIZED, /*control , getMediaInfo, not prefetch or read, seekTo,.*/
    CDX_MMS_IDLE,
    CDX_MMS_PREFETCHING,
    CDX_MMS_PREFETCHED,
    CDX_MMS_READING,
    CDX_MMS_SEEKING,
    CDX_MMS_EOS,
};

static int mmsPlaylistClear(struct CdxMmsParser* impl)
{
    int status;
    CdxMediaInfoT mediaInfo;
    if(!impl)
    {
        return -1;
    }

    if(impl->asfParser)
    {
        CdxParserClose(impl->asfParser);
        impl->asfParser = NULL;
    }
    impl->mErrno = PSR_INVALID;
    impl->mStatus = CDX_MMS_INITIALIZED;

    //CdxStream
    CDX_LOGD("+++ streamTell: %llx", CdxStreamTell(impl->stream));
    CdxStreamControl(impl->stream, STREAM_CMD_RESET_STREAM, NULL);
    CDX_LOGD("+++ streamTell: %llx", CdxStreamTell(impl->stream));
    impl->asfParser = asfParserCtor.create(impl->stream, NO_NEED_CLOSE_STREAM);
    if(!impl->asfParser)
    {
        CDX_LOGE("open asf parser failed!");
        return -1;
    }

    while ((status = CdxParserGetStatus(impl->asfParser)) != PSR_OK && status != PSR_OPEN_FAIL)
    {
        CDX_LOGI("parser preparing...");
        usleep(100000);
        if (impl->exitFlag)
        {
            return -1;
        }
        CDX_LOGD("xxx CdxParserGetStatus(parser) != PSR_OK");
    }

    impl->mErrno = PSR_OK;
    impl->mStatus = CDX_MMS_IDLE;
    CdxParserGetMediaInfo(impl->asfParser, &mediaInfo);

    if((impl->videoCodecFormat != mediaInfo.program[0].video[0].eCodecFormat)
        || (impl->audioCodecFormat != mediaInfo.program[0].audio[0].eCodecFormat))
    {
        CDX_LOGW("---- videoCodecFormat not the same: <%d, %d>",
                impl->videoCodecFormat, mediaInfo.program[0].video[0].eCodecFormat);
    }

    return 0;
}

static void MmsParserClose(struct CdxMmsParser* impl)
{
    if (impl)
    {
        if(impl->asfParser) CdxParserClose(impl->asfParser);
        if(impl->stream) CdxStreamClose(impl->stream);
        free(impl);
    }
}

static cdx_int32 __CdxMmsParserClose(CdxParserT *parser)
{
    struct CdxMmsParser* impl = (struct CdxMmsParser*)parser;
    impl->exitFlag = 1;
    //pthread_join(impl->thread, NULL);
    MmsParserClose(impl);
    return 0;
}

static cdx_int32 __CdxMmsParserPrefetch(CdxParserT *parser, CdxPacketT * pkt)
{
    struct CdxMmsParser* impl = (struct CdxMmsParser*)parser;
    int ret;
    int ioStatus;
    impl->mStatus = CDX_MMS_PREFETCHING;
    //CDX_LOGD("--- mms parser prefetch");
    ret = CdxParserPrefetch(impl->asfParser, pkt);
    //CDX_LOGD("--- mms parser prefetch ret = %d", ret);
    if(ret < 0)
    {
        ioStatus = CdxStreamGetIoState(impl->stream);
        CDX_LOGD("--- ioStatus = %d", ioStatus);
        if(ioStatus == CDX_IO_STATE_CLEAR)
        {
            CDX_LOGD("--- mms playlist");
            ret = mmsPlaylistClear(impl);
            if(ret < 0)
            {
                CDX_LOGW("--- mmsPlaylistClear error");
                impl->mStatus = CDX_MMS_IDLE;
                return -1;
            }
            impl->mStatus = CDX_MMS_IDLE;
            return __CdxMmsParserPrefetch(parser, pkt);
        }
        else
        {
            CDX_LOGW("-- prefecth error, maybe eos");
            impl->mStatus = CDX_MMS_IDLE;
        }
    }

    impl->mStatus = CDX_MMS_PREFETCHED;
    return ret;
}

static cdx_int32 __CdxMmsParserRead(CdxParserT *parser, CdxPacketT *pkt)
{
    int ret;
    int ioStatus;
    struct CdxMmsParser* impl = (struct CdxMmsParser*)parser;
    if(impl->mStatus != CDX_MMS_PREFETCHED)
    {
        impl->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }

    ret = CdxParserRead(impl->asfParser, pkt);
    if(ret < 0)
    {
        ioStatus = CdxStreamGetIoState(impl->stream);
        CDX_LOGD("--- read ioStatus = %d", ioStatus);
        if(ioStatus == CDX_IO_STATE_CLEAR)
        {
            CDX_LOGD("--- mms playlist");
            ret = mmsPlaylistClear(impl);
            if(ret < 0)
            {
                CDX_LOGW("--- mmsPlaylistClear error");
                impl->mStatus = CDX_MMS_IDLE;
                return -1;
            }
            impl->mStatus = CDX_MMS_IDLE;
            ret = CdxParserPrefetch(impl->asfParser, pkt);
            if(ret < 0)
            {
                return -1;
            }
            ret = CdxParserRead(impl->asfParser, pkt);
            if(ret < 0)
            {
                return -1;
            }
            // __CdxMmsParserPrefetch(parser, pkt);
        }
        else
        {
            CDX_LOGW("-- read error, maybe eos");
            impl->mStatus = CDX_MMS_IDLE;
        }
    }

    impl->mStatus = CDX_MMS_IDLE;
    return ret;
}

static cdx_int32 __CdxMmsParserGetMediaInfo(CdxParserT *parser, CdxMediaInfoT * mediaInfo)
{
    struct CdxMmsParser* impl = (struct CdxMmsParser*)parser;
    int ret = CdxParserGetMediaInfo(impl->asfParser, mediaInfo);
    mediaInfo->program[0].duration = -1;
    impl->videoCodecFormat = mediaInfo->program[0].video[0].eCodecFormat;
    impl->audioCodecFormat = mediaInfo->program[0].audio[0].eCodecFormat;
    return ret;
}

cdx_int32 __CdxMmsParserForceStop(CdxParserT *parser)
{
    struct CdxMmsParser* impl;
    int ret;

    impl = (struct CdxMmsParser*)parser;
    if(!impl)
    {
        CDX_LOGE("mms file parser has not been initiated!");
        return -1;
    }
    impl->exitFlag = 1;

    ret = CdxParserForceStop(impl->asfParser);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        return -1;
    }
    #if 0 // stream force stop have done in  asf parser , so we do not need in mms parser
    ret = CdxStreamForceStop(impl->stream);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        return -1;
    }
    #endif

    while((impl->mStatus != CDX_MMS_IDLE) && (impl->mStatus != CDX_MMS_PREFETCHED))
    {
        usleep(2000);
    }

    impl->mStatus = CDX_MMS_IDLE;
    impl->mErrno = PSR_USER_CANCEL;
    return 0;
}

cdx_int32 __CdxMmsParserClrForceStop(CdxParserT *parser)
{
    struct CdxMmsParser* impl;
    int ret;

    impl = (struct CdxMmsParser*)parser;
    if(!impl)
    {
        CDX_LOGE("mms file parser has not been initiated!");
        return -1;
    }
    impl->exitFlag = 0;
    ret = CdxParserClrForceStop(impl->asfParser);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        return -1;
    }
    ret = CdxStreamClrForceStop(impl->stream);
    if(ret < 0)
    {
        CDX_LOGE("mms parser force stop error!");
        return -1;
    }

    impl->mErrno = PSR_OK;
    return 0;
}

static int __CdxMmsParserControl(CdxParserT *parser, int cmd, void *param)
{
    struct CdxMmsParser* impl = (struct CdxMmsParser*)parser;

    switch(cmd)
    {
        case CDX_PSR_CMD_SET_FORCESTOP:
            return __CdxMmsParserForceStop(parser);
        case CDX_PSR_CMD_CLR_FORCESTOP:
            return __CdxMmsParserClrForceStop(parser);

        case CDX_PSR_CMD_GET_CACHESTATE:
        {
            struct ParserCacheStateS* parserCS = (struct ParserCacheStateS*)param;
            struct StreamCacheStateS streamCS;
            if (CdxStreamControl(impl->stream, STREAM_CMD_GET_CACHESTATE, &streamCS) < 0)
            {
                CDX_LOGE("STREAM_CMD_GET_CACHESTATE fail");
                return -1;
            }
            parserCS->nCacheCapacity = streamCS.nCacheCapacity;
            parserCS->nCacheSize = streamCS.nCacheSize;
            parserCS->nBandwidthKbps = streamCS.nBandwidthKbps;
            parserCS->nPercentage = streamCS.nPercentage;
            break;
        }
        default:
            CDX_LOGD("unkown cmd");
            break;
    }

    return 0;
}

cdx_int32 __CdxMmsParserSeekTo(CdxParserT *parser, cdx_int64  timeUs, SeekModeType seekModeType)
{
    struct CdxMmsParser* impl = (struct CdxMmsParser*)parser;
    if(impl->exitFlag == 1)
    {
        return -1;
    }
    return CdxParserSeekTo(impl->asfParser, timeUs, seekModeType);
}

cdx_uint32 __CdxMmsParserAttribute(CdxParserT *parser) /*return falgs define as open's falgs*/
{
    struct CdxMmsParser* tmpMmsPsr = (struct CdxMmsParser*)parser;
    CDX_UNUSE(tmpMmsPsr);
    return -1;
}

cdx_int32 __CdxMmsParserGetStatus(CdxParserT *parser)
{
    struct CdxMmsParser* tmpMmsPsr = (struct CdxMmsParser*)parser;
    return tmpMmsPsr->mErrno;
}

static int __CdxMmsParserInit(CdxParserT *parser)
{
    struct CdxMmsParser* impl = (struct CdxMmsParser*)parser;
    CdxStreamT* stream = impl->stream;
    //char buf[13];
    int status;
    int ret;

    ret = CdxParserInit(impl->asfParser);
    if(ret < 0)
    {
        CDX_LOGE("asf parser init failed");
        return -1;
    }

    int ioStatus = CdxStreamGetIoState(impl->stream);
    if(ioStatus == CDX_IO_STATE_CLEAR)
    {
        CDX_LOGD("--- mms playlist  clear");
        ret = mmsPlaylistClear(impl);
        if(ret < 0)
        {
            CDX_LOGW("--- mmsPlaylistClear error");
            impl->mErrno = PSR_OPEN_FAIL;
            return -1;
        }
    }

    impl->mErrno = PSR_OK;
    impl->mStatus = CDX_MMS_IDLE;
    return 0;

error:
    impl->mErrno = PSR_OPEN_FAIL;
    return -1;
}

static struct CdxParserOpsS mmsParserOps =
{
    .init           = __CdxMmsParserInit,
    .control         = __CdxMmsParserControl,
    .prefetch         = __CdxMmsParserPrefetch,
    .read             = __CdxMmsParserRead,
    .getMediaInfo     = __CdxMmsParserGetMediaInfo,
    .close             = __CdxMmsParserClose,
    .seekTo             = __CdxMmsParserSeekTo,
    .attribute        = __CdxMmsParserAttribute,
    .getStatus        = __CdxMmsParserGetStatus
};

static void* mmsOpenThread(void* p_arg)
{
    struct CdxMmsParser* impl = (struct CdxMmsParser*)p_arg;
    CdxStreamT* stream = impl->stream;
    //char buf[13];
    int status;
    int ret;

    impl->asfParser = asfParserCtor.create(stream, NO_NEED_CLOSE_STREAM);
    if(!impl->asfParser)
    {
        CDX_LOGE("open asf parser failed!");
        goto error;
    }

    while ((status = CdxParserGetStatus(impl->asfParser)) != PSR_OK && status != PSR_OPEN_FAIL)
    {
        CDX_LOGI("parser preparing...");
        usleep(100000);
        if (impl->exitFlag)
        {
            goto error;
        }
        CDX_LOGD("xxx CdxParserGetStatus(parser) != PSR_OK");
    }

    int ioStatus = CdxStreamGetIoState(impl->stream);
    if(ioStatus == CDX_IO_STATE_CLEAR)
    {
        CDX_LOGD("--- mms playlist  clear");
        ret = mmsPlaylistClear(impl);
        if(ret < 0)
        {
            CDX_LOGW("--- mmsPlaylistClear error");
            impl->mErrno = PSR_OPEN_FAIL;
            return NULL;
        }
    }

    impl->mErrno = PSR_OK;
    impl->mStatus = CDX_MMS_IDLE;
    return NULL;

error:
    impl->mErrno = PSR_OPEN_FAIL;
    return NULL;
}

static CdxParserT *__CdxMmsParserOpen(CdxStreamT *stream, cdx_uint32 flag)
{
    cdx_int32            ret;
    struct CdxMmsParser *impl = NULL;
    CDX_UNUSE(flag);

    impl = CdxMalloc(sizeof(struct CdxMmsParser));
    if(impl == NULL) goto error;
    memset(impl, 0x00, sizeof(*impl));
    impl->parserinfo.ops = &mmsParserOps;
    impl->stream = stream;
    impl->mErrno = PSR_INVALID;
    impl->mStatus = CDX_MMS_INITIALIZED;

    impl->asfParser = asfParserCtor.create(stream, NO_NEED_CLOSE_STREAM);
    if(!impl->asfParser)
    {
        CDX_LOGE("open asf parser failed!");
        goto error;
    }

    //ret = pthread_create(&impl->thread, NULL, mmsOpenThread, (void*)impl) ;
    //if(ret != 0)
    //{
    //    CDX_LOGE("cannot create probedata thread");
    //    impl->thread = (pthread_t)0;
    //    goto error;
    //}

    return &impl->parserinfo;

error:
    if(impl)
    {
        MmsParserClose(impl);
        impl = NULL;
    }
    return NULL;
}

static cdx_uint32 __CdxMmsParserProbe(CdxStreamProbeDataT *probeData)
{
    if(!memcmp(probeData->buf, "mms-allwinner", 13))
    {
        CDX_LOGD("--- it is mms parser");
        return 100;
    }
    return 0;
}

CdxParserCreatorT mmsParserCtor =
{
    .create = __CdxMmsParserOpen,
    .probe     = __CdxMmsParserProbe
};
